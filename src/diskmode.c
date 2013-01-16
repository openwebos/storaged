/* @@@LICENSE
*
*      Copyright (c) 2002-2013 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/usb/ch9.h>
#include <asm/byteorder.h>
#include <mntent.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cjson/json.h>

#include "log.h"

#include <luna-service2/lunaservice.h>

#include "signals.h"
#include "util.h"
#include "main.h"

static guint sUmountTimerId = 0;   /* real ids always > 0 */
static bool inMSM = false, unmount = false;


#define SYSTEM_SERVICE "com.palm.systemservice"
#define TIMEOUT_SECONDS 10  /* how many seconds of inactivity before quitting */
#define MSM_WAIT_SECONDS 3  /* how long to wait for open file owners to quit */
#define MEDIA_INTERNAL "/media/internal"

#define ETC_FSTAB    "/etc/fstab"
#define MAX_FSCK_RETRIES 3 // number of times to try running fsck before giving up


#define DISKMODE_ERROR diskmode_error_quark ()

GQuark
diskmode_error_quark (void)
{
    return g_quark_from_static_string ("storaged-diskmode");
}

enum {
    DISKMODE_ERROR_NO_SYS_FILE,
    DISKMODE_ERROR_SPAWN,
    DISKMODE_ERROR_FSCK_ON_MOUNTED_PARTITION
};

static void finish_mass_storage_mode_transition( LSHandle* lsh );
static void abort_mass_storage_mode_transition( LSHandle* lsh );

static nyx_device_handle_t nyxMassStorageMode = NULL;

/**
 * @brief timer proc that, when fired by a GTimer, attempts to make the disk
 * mountable by the remote host and if unsuccessful aborts the transition to
 * Mass Storage Mode.
 */
static gboolean 
umount_timer_proc( gpointer data )
{
    g_debug( "%s()", __func__ );
    LSHandle* lsh = (LSHandle*)data;

    nyx_mass_storage_mode_return_code_t ret_status;

    bool ret = nyx_mass_storage_mode_set_mode(nyxMassStorageMode,NYX_MASS_STORAGE_MODE_ENABLE, &ret_status);

    if( ret == NYX_ERROR_NONE) {
        finish_mass_storage_mode_transition( lsh );
    } else {
        g_message("Aborting Mass Storage Mode due to return code : %d",ret_status);
        abort_mass_storage_mode_transition( lsh );
    }
    sUmountTimerId = 0;

    return false;               /* we never try again; user is waiting.... */
}

/**
 * @brief sets 
 */
static void
set_umount_timer( LSHandle* lsh )
{
    g_debug( "%s()", __func__ );
    if ( 0 == sUmountTimerId ) {
        sUmountTimerId = g_timeout_add_seconds( MSM_WAIT_SECONDS, umount_timer_proc, lsh );
    } else {
        g_debug( "%s: timer exists; not creating", __func__ );
    }
}

/*
 * TODO: get rid of this dependency on customization
 *
 */
static void
launch_customization(LSHandle* lsh)
{
    LSError lserror;
    LSErrorInit(&lserror);
    char* payload = "{\"subdir\":\"/" MEDIA_INTERNAL "\"}";
    const char* uri = "luna://com.palm.customization/copyBinaries";
    if (!LSCallOneReply(lsh, uri, payload, NULL, NULL, NULL, &lserror)) {
        LSREPORT( lserror );
    }
    LSErrorFree( &lserror );
}

void handle_mass_storage_mode_exit(nyx_mass_storage_mode_return_code_t ret_status, LSHandle* lsh)
{
    /* Let's just ignore this message if we can't get into Mass Storage Mode at all. */
    if (ret_status >= NYX_MASS_STORAGE_MODE_PARTITION_REFORMATTED)
    {
        g_critical("Drive reformatted due to unmount failures");
        if(ret_status != NYX_MASS_STORAGE_MODE_MOUNT_FAILURE_AFTER_REFORMAT)
            launch_customization(lsh);
    }

    if (unmount) {
        bool reformatted = (ret_status >= NYX_MASS_STORAGE_MODE_PARTITION_REFORMATTED);
        bool fsck_found_problem = (ret_status == NYX_MASS_STORAGE_MODE_FSCK_PROBLEM) || (ret_status == NYX_MASS_STORAGE_MODE_PARTITION_REFORMATTED_FSCK_PROBLEM);
        SignalPartitionAvail( lsh, MEDIA_INTERNAL, true, reformatted, fsck_found_problem );
        unmount = false;
    }
}

static void
handle_cable( LSHandle* lsh, bool plugIn) {

    g_debug("%s: called with plugin=%d", __func__, plugIn);
    GError *error = NULL;
    bool still_exported = true; 
    bool know_export_state = true;
    int mass_storage_mode_state = 0;

    know_export_state = nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);
    still_exported = mass_storage_mode_state & NYX_MASS_STORAGE_MODE_MODE_ON;

    SHOW_ERROR(error);

    if ( plugIn ) {
        if (know_export_state && still_exported) {
            g_warning("%s: media is exported, but got a cable insert!! wtf? ignoring",__func__);
        } else {
            /* Tell the world */
            SignalMSMAvailChange( lsh, true );
        }

        // must not shut down
        disable_lifetime_timer();
    } else {
        /* We've just lost a connection we had: cable was unplugged */
        if (sUmountTimerId != 0) {
            g_debug("%s: UmountTimer existed after cable pull. removing source", __func__);
            g_source_remove(sUmountTimerId);
            sUmountTimerId = 0;
        }
        if(still_exported)
            SignalMSMFscking(lsh);

        nyx_mass_storage_mode_return_code_t ret_status;
        nyx_mass_storage_mode_set_mode(nyxMassStorageMode, NYX_MASS_STORAGE_MODE_DISABLE_AFTER_FSCK, &ret_status);

        handle_mass_storage_mode_exit(ret_status,lsh);

        SignalMSMAvailChange( lsh, false );

        // can now shut down
        reset_lifetime_timer();
    }
}


/** handle_cableLS: called on notification from udev that cable plugged in
*/
/**
 * @brief called when cable [un]plugged
 */
static bool
handle_cableLS( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);
    LSError lserror;
    bool result;
    gchar* answer = "";
    bool plugIn;

    int mass_storage_mode_state = 0;
    nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);

    /* Let's just ignore this message if we can't get into Mass Storage Mode at all. */
    if (!(mass_storage_mode_state & NYX_MASS_STORAGE_MODE_DRIVER_AVAILABLE))
    {
        g_debug( "%s: Mass Storage Mode driver unavailable", __func__ );
        answer = "{\"returnValue\":false,\"errorText\":\"Mass Storage Mode driver unavailable\"}";
        SignalMSMAvailChange( lsh, false );
        goto send;
    }

    const char *payload = LSMessageGetPayload(message);
    struct json_object *object = json_tokener_parse(payload);
    if (is_error(object)) {
        answer = "{\"returnValue\":false,\"errorText\":\"param 'connected' missing or invalid\"}";
        goto send;
    }
    plugIn = json_object_get_boolean(
               json_object_object_get(object, "connected"));

    handle_cable( lsh, plugIn);
    answer = "{\"returnValue\": true}";

send:
    LSErrorInit(&lserror);
    result = LSMessageReply( lsh, message, answer, &lserror);
    if (!result)
    {
        LSREPORT( lserror );
    }

    if (!is_error(object)) json_object_put(object);

    return true;
} /* handle_cableLS */

void
handle_mount_on_host(LSHandle *lsh, bool mount) 
{
    g_debug("%s: called with mount=%d", __func__, mount);

    /* Tell the world we're an externally mounted drive or not.  Do this even
       if we're going to fail to mount, as the device needs to be in phone
       mode to hand error messages to the user. */
    SignalMSMModeChange( lsh, mount );

    if ( !mount ) {
        nyx_mass_storage_mode_return_code_t ret_status;

    nyx_mass_storage_mode_set_mode(nyxMassStorageMode, NYX_MASS_STORAGE_MODE_DISABLE, &ret_status);

    if(ret_status == NYX_MASS_STORAGE_MODE_MOUNT_FAILURE) {
        SignalMSMFscking(lsh);
        nyx_mass_storage_mode_set_mode(nyxMassStorageMode, NYX_MASS_STORAGE_MODE_DISABLE_AFTER_FSCK, &ret_status);
        }

        handle_mass_storage_mode_exit(ret_status, lsh);

        inMSM = false;
        SignalMSMStatus ( lsh, false);
    }
}

/** @brief called when the media is actually mounted or ejected on the host
 * side.  This is the final stage, and happens (for connected == true) after
 * we've set disk mode or media mode (confirm for media mode case)
 *
 * for media mode case this is called after each sync
 */
static bool
handle_mount_on_hostLS( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);
    LSError lserror;
    char* answer;
    bool connected;

    /* IIRC, we can't have allowed mount or eject without the driver being
       involved.
       We can get into this case if passthru mode is enabled, so ignore this event */
    int mass_storage_mode_state = 0;
    nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);

    if (!(mass_storage_mode_state & NYX_MASS_STORAGE_MODE_DRIVER_AVAILABLE))
    {
        g_debug( "%s: Mass Storage Mode driver unavailable", __func__ );
        answer = "{\"returnValue\":false,\"errorText\":\"Mass Storage Mode driver unavailable\"}";
        goto send;
    }

    const char *payload = LSMessageGetPayload(message);
    struct json_object *object = json_tokener_parse(payload);
    if (is_error(object)) {
    	answer = "{\"returnValue\":false,\"errorText\":\"param 'connected' missing or invalid\"}";
    	goto send;
    }

    connected = json_object_get_boolean(
                json_object_object_get(object, "connected"));

    handle_mount_on_host(lsh, connected);

    answer = "{\"returnValue\": true}";
send:
    LSErrorInit(&lserror);
    if ( !LSMessageReply( lsh, message, answer, &lserror) )
    {
        LSREPORT( lserror );
    }
    LSErrorFree( &lserror );

    if (!is_error(object)) json_object_put(object);

    return true;
} /* handle_mount_on_host */

static bool
handle_host_connected_query( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);

    LSError lserror;
    LSErrorInit( &lserror );

    int mass_storage_mode_state = 0;
    nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);
    bool connected = mass_storage_mode_state & NYX_MASS_STORAGE_MODE_HOST_CONNECTED;

    char reply[128];
    snprintf( reply, sizeof(reply), "{\"result\": true, \"hostIsConnected\": %s}",
            connected? "true" : "false");
    if ( !LSMessageReply( lsh, message, reply, &lserror ) )
    {
        LSREPORT( lserror );
    }

    LSErrorFree( &lserror );
    return true;
} /* handle_host_connected_query */

/** @brief
 * 
 * We need to unmount /media/internal before we can put ourselves in Mass Storage Mode.
 * We may succeed right away, in which case there's little to do here.  But if
 * we fail, we assume it's because other processes are holding files open
 * there.  They must stop, and if well-behaved are listening on our signal.
 * We set a timer and try again, reporting the results via a signal and, if we
 * failed, logging whose fault it was.
 */
static void
begin_mass_storage_mode_transition( LSHandle* lsh )
{
    g_debug( "%s()", __func__ );
    inMSM = true;
    SignalMSMStatus ( lsh, true);
    SignalMSMProgress( lsh, MSM_MODE_CHANGE_ATTEMPTING, false );

    set_umount_timer( lsh );
}

/**
 * @brief 
 */
static void
finish_mass_storage_mode_transition( LSHandle* lsh)
{
    SignalMSMProgress( lsh, MSM_MODE_CHANGE_SUCCEEDED, false );
    unmount = true;
}

/**
 * @brief 
 */
static void
abort_mass_storage_mode_transition( LSHandle* lsh )
{
    g_warning("%s: called", __func__);
    inMSM = false;
    SignalMSMStatus ( lsh, false);
    SignalMSMProgress( lsh, MSM_MODE_CHANGE_FAILED, false );
}

static bool
handle_bus_suspended( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);
    LSError lserror;
    char* answer = "{\"returnValue\": true}";
#if 0
    // until further notice.. suspend suport is removed

    bool is_suspend_notification;
    json_t* payload = json_parse_document(LSMessageGetPayload( message ));
    if ( !json_get_bool( payload, "suspended", &is_suspend_notification ) ) {
        answer = "{\"returnValue\":false,\"errorText\":\"param 'suspended' missing or invalid\"}";
        goto send;
    }

    if (is_suspend_notification) {
        handle_mount_on_host(lsh, false);
    } else {
        g_debug("%s: resume.. nothing to do", __func__);
    }

    if (payload)
        json_free_value(&payload);


send:
#endif

    g_debug("%s: about to send answer %s", __func__, answer);
    LSErrorInit(&lserror);
    if ( !LSMessageReply( lsh, message, answer, &lserror) )
    {
        LSREPORT( lserror );
    }
    LSErrorFree( &lserror );
    g_debug("%s: returning", __func__);
    return true;
}

/**
 * @brief 
 */
static bool
handle_enter_mass_storage_mode( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);

    LSError lserror;
    LSErrorInit( &lserror );

    char* errStr = "parameter user-confirmed missing";

    bool confirmed = false;

    const char *payload = LSMessageGetPayload(message);
    struct json_object *object = json_tokener_parse(payload);
    if (is_error(object)) {
        errStr = "{\"returnValue\":false,\"errorText\":\"param 'user-confirmed' missing or invalid\"}";
        goto err;
    }

    confirmed = json_object_get_boolean(json_object_object_get(object, "connected"));

    errStr = NULL;
    if ( confirmed )
    {
        int mass_storage_mode_state = 0;
        nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);
        bool connected = mass_storage_mode_state & NYX_MASS_STORAGE_MODE_HOST_CONNECTED;

        if ( !connected ) {
            g_debug( "not entering brick mode because no usb connection" );
        } else {
            begin_mass_storage_mode_transition( lsh );
        }
    }

    if ( !LSMessageReply( lsh, message, "{\"result\": true}", &lserror ) )
    {
        LSREPORT( lserror );
    }

err:
    if ( NULL != errStr ) {
        char* msg = g_strdup_printf( "{\"result\": false; \"errorText\":\"%s\"}",
                errStr );
        if ( !LSMessageReply( lsh, message, msg, &lserror ) ) {
            LSREPORT( lserror );
        }
        g_free( msg );
    }

    LSErrorFree( &lserror );

    if (!is_error(object))
        json_object_put(object);

    g_debug( "%s() exiting", __func__ );
    return true;
} /* handle_enter_mass_storage_mode */

static bool
handle_mass_storage_mode_status_query( LSHandle* lsh, LSMessage* message, void* user_data )
{
    LSTRACE_LSMESSAGE(message);

    LSError lserror;
    LSErrorInit( &lserror );

    char reply[128];
    snprintf( reply, sizeof(reply), "{\"result\": true, \"inMSM\": %s}",
            inMSM? "true" : "false");

    if ( !LSMessageReply( lsh, message, reply, &lserror ) )
    {
        LSREPORT( lserror );
    }

    LSErrorFree( &lserror );
    return true;
} /* handle_mass_storage_mode_status_query */


static LSMethod diskModePrivMethods[] = {
    { "changed", handle_cableLS },   /* notification from udev: cable plugged in */
    { "avail", handle_mount_on_hostLS },       /* kernel set up for disk to be mounted */
    { "busSuspended", handle_bus_suspended },       /* suspend notifications */
    { "enterMSM", handle_enter_mass_storage_mode }, /* command/notice of user confirmation to enter Mass Storage Mode */
    { "hostIsConnected", handle_host_connected_query },       /* support questions about state of USB */
    { "queryMSMStatus", handle_mass_storage_mode_status_query },   /* query if device is in Mass Storage Mode */
    { },
};

static LSMethod diskModePubMethods[] = {
    { "queryMSMStatus", handle_mass_storage_mode_status_query },   /* query if device is in Mass Storage Mode */
    {},
};

static void mass_storage_mode_state_changed(nyx_device_handle_t handle, nyx_callback_status_t status, void* data)
{
    g_debug("%s : Mass Storage Mode state changed", __FUNCTION__);
}

/** DiskModeInterfaceInit
 *
 * @brief Register storaged with luna-service as implementer of several
 * methods.
 */
int
DiskModeInterfaceInit(GMainLoop *loop, LSHandle* priv_handle, LSHandle* pub_handle,
        bool invertCarrier )
{
    LSError lserror;
    LSErrorInit(&lserror);

    if ( !LSRegisterCategory ( priv_handle, "/diskmode", diskModePrivMethods,
            NULL, NULL, &lserror) )
    {
        LSREPORT( lserror );
    }
    if ( !LSRegisterCategory ( pub_handle, "/diskmode", diskModePubMethods,
        NULL, NULL, &lserror) )
    {
        LSREPORT( lserror );
    }
    LSErrorFree( &lserror );

    g_debug("%s: starting up", __func__);

    nyxMassStorageMode = GetNyxMassStorageModeDevice();

    int mass_storage_mode_state = 0;
    nyx_mass_storage_mode_get_state(nyxMassStorageMode, &mass_storage_mode_state);
    bool connected = mass_storage_mode_state & NYX_MASS_STORAGE_MODE_HOST_CONNECTED;

    //register for Mass Storage Mode state changes
    nyx_mass_storage_mode_register_change_callback(nyxMassStorageMode, mass_storage_mode_state_changed, NULL);

    // behave like the cable just got plugged in (or unplugged)
    // This ensures we start at the correct state
    handle_cable( priv_handle, connected);
    //TODO: handle case where media is exported, but cable is not plugged in.. or media is not exported, but in media mode.
    return 0;
}

