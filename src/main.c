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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <luna-service2/lunaservice.h>

#include "diskmode.h"
#include "erase.h"
#include "signals.h"
#include "log.h"
#include "main.h"

/*
 * Notes on what storaged does
 *
 * Wake up on USB changes via udev rules resulting in luna-service calls
 *
 * Listens to udev changes related to "bus", which offers possiblity of Mass
 * Storage Mode.
 *
 * Send broadcast signals (not targeted messages) on transitions.  (See
 * signals.h for wrapper functions and the signals they send.)
 *
 * Support a query returning the same at any time. NOT DONE; do we need this?
 *
 * In response to the MSM-avail signal, Renchi will display alert asking user
 * whether to go into MSM mode.  He'll send an "enable" message (below) on
 * affirmative response.  User's other choice is to dismiss the dialog, and
 * the only way to get it back is to re-insert the USB cable.
 *
 * Support command "enable storage mode".  On receipt, send a signal meaning
 * "Trying to enter MSM."  If /media/internal is unmountable, send
 * "successfully entered".  Otherwise wait 3-4 seconds and try again, sending
 * "success" or "failure" at that point.  Log apps whose open files prevented
 * unmounting.  Apps that might have files open need to listen for the signal.
 *
 * On entry to MSM, send additional signal.
 *
 * Listen to messages of "usb umount" from Toshi.  Remount partition.
 * Send notice of transition out of MSM.
 */

static GMainLoop * g_mainloop = NULL;
static int sTimerEventSource = 0;


/***********************************************************************
 * LockFile
 ***********************************************************************/
typedef struct
{
    char path[ PATH_MAX ];
    int fd;
}
LockFile;


static LockFile	sProcessLock;

/**
 * @brief LockProcess
 *
 * Acquire the process lock (by getting an file lock on our pid file).
 *
 * @param component
 *
 * @return true on success, false if failed.
 */
bool LockProcess(const char* component)
{
#define LOCKS_DIR_PATH "/tmp/run"

    pid_t pid;
    int fd;
    int result;

    pid = getpid();

    LockFile * lock = &sProcessLock;

    // create the locks directory if necessary
    (void) mkdir(LOCKS_DIR_PATH, 0777);

    snprintf(lock->path, sizeof(lock->path), "%s/%s.pid", LOCKS_DIR_PATH, component);

    // open or create the lock file
    fd = open(lock->path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        g_error("Failed to open lock file (err %d, %s), exiting.", errno, strerror(errno));
        return false;
    }

    // use a POSIX advisory file lock as a mutex
    result = lockf(fd, F_TLOCK, 0);
    if (result < 0)
    {
        if ((errno == EDEADLK) || (errno == EAGAIN))
            g_error("Failed to acquire lock, exiting.");
        else
            g_error("Failed to acquire lock (errno %d, %s), exiting.", errno, strerror(errno));

        return false;
    }

    // remove the old pid number data
    result = ftruncate(fd, 0);
    if (result < 0)
        g_debug("Failed truncating lock file (err %d, %s).", errno, strerror(errno));

    // write the pid to the file to aid debugging
    {
        gchar *pid_str = g_strdup_printf("%d\n", pid);
        int pid_str_len = (int) strlen(pid_str);
        result = write(fd, pid_str, pid_str_len);
        if (result < pid_str_len)
            g_debug("Failed writing lock file (err %d, %s).", errno, strerror(errno));
        g_free(pid_str);
    }

    lock->fd = fd;
    return true;
}


/**
 * @brief UnlockProcess
 *
 * Release the lock on the pid file as previously acquired by
 * LockProcess.
 */
void UnlockProcess(void)
{
    LockFile* lock;

    lock = &sProcessLock;
    close(lock->fd);
    (void) unlink(lock->path);
}



void
term_handler(int signal)
{
    g_main_loop_quit(g_mainloop);
}

gboolean
timeout_handler(gpointer data)
{
    g_main_loop_quit(g_mainloop);
    return TRUE;
}

void
PrintUsage(const char* progname)
{
    printf("%s\n", progname);
    printf(" -h this help screen\n"
           " -c invert is-carrier test\n"
           " -d turn debug logging on\n"
           " -s logging via syslog\n");
}

#define DYNAMIC_LIFETIME_MS 10000

void
disable_lifetime_timer()
{
    g_debug("%s called", __func__);
    if (sTimerEventSource != 0)
        g_source_remove(sTimerEventSource);

    sTimerEventSource = 0;
}

//TODO: need to call this from erase API's aswell
void
reset_lifetime_timer()
{
    g_debug("%s called", __func__);
    disable_lifetime_timer();
    sTimerEventSource = g_timeout_add_full(G_PRIORITY_DEFAULT, DYNAMIC_LIFETIME_MS, timeout_handler, NULL, NULL);
    g_debug("%s: timeout set", __func__);
}

static nyx_device_handle_t nyxSystem = NULL;
static nyx_device_handle_t nyxMassStorageMode = NULL;

nyx_device_handle_t
GetNyxSystemDevice(void)
{
    return nyxSystem;
}

nyx_device_handle_t
GetNyxMassStorageModeDevice(void)
{
    return nyxMassStorageMode;
}


int
main(int argc, char **argv)
{
    bool retVal;
    int opt;
    bool invertCarrier = false;

    LSPalmService * lsps = NULL;

    while ((opt = getopt(argc, argv, "chdst")) != -1)
    {
        switch (opt) {
        case 'c':
            invertCarrier = true;
            break;
        case 'd':
            setLogLevel(G_LOG_LEVEL_DEBUG);
            break;
        case 's':
            setUseSyslog(true);
            break;
        case 'h':
        default:
            PrintUsage(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    // make sure we aren't already running.
    if (!LockProcess("storaged")) {
        g_error("%s: %s daemon is already running.\n", __func__, argv[0]);
        exit(EXIT_FAILURE);
    }

    g_log_set_default_handler(logFilter, NULL);
    g_debug( "entering %s in %s", __func__, __FILE__ );

    signal(SIGTERM, term_handler);

    g_mainloop = g_main_loop_new(NULL, FALSE);


    int ret = nyx_device_open(NYX_DEVICE_SYSTEM, "Main", &nyxSystem);
    if(ret != NYX_ERROR_NONE)
    {
        g_critical("Unable to open the nyx device system");
        abort();
    }
    else
        g_debug("Initialized nyx system device");

    ret = nyx_device_open(NYX_DEVICE_MASS_STORAGE_MODE, "Main", &nyxMassStorageMode);
    if(ret != NYX_ERROR_NONE)
    {
        g_critical("Unable to open the nyx mass storage mode device");
        abort();
    }
    else
        g_debug("Initialized nyx mass storage mode device");


    /**
     *  initialize the lunaservice and we want it before all the init
     *  stuff happening.
     */
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSRegisterPalmService("com.palm.storage", &lsps, &lserror);
    if (!retVal)
    {
        g_critical ("failed in function %s with erro %s", lserror.func, lserror.message);
        LSErrorFree(&lserror);
        return EXIT_FAILURE;
    }

    SignalsInit( lsps );

    LSHandle *lsh_priv = LSPalmServiceGetPrivateConnection(lsps);
    LSHandle *lsh_pub = LSPalmServiceGetPublicConnection(lsps);

    DiskModeInterfaceInit( g_mainloop, lsh_priv, lsh_pub, invertCarrier );
    EraseInit(g_mainloop, lsh_priv);

    retVal = LSGmainAttach( lsh_priv, g_mainloop, &lserror );
    if ( !retVal )
    {
        g_critical( "LSGmainAttach private returned %s", lserror.message );
        LSErrorFree(&lserror);
    }
    retVal = LSGmainAttach( lsh_pub, g_mainloop, &lserror );
    if ( !retVal )
    {
        g_critical( "LSGmainAttach public returned %s", lserror.message );
        LSErrorFree(&lserror);
    }
    g_main_loop_run(g_mainloop);
    g_main_loop_unref(g_mainloop);

    if (!LSUnregister( lsh_priv, &lserror)) {
        g_critical( "LSUnregister private returned %s", lserror.message );
    }
    if (!LSUnregister( lsh_pub, &lserror)) {
        g_critical( "LSUnregister public returned %s", lserror.message );
    }

    UnlockProcess();

    g_debug( "exiting %s in %s", __func__, __FILE__ );

    if (!retVal)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}
