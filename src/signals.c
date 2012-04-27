/* @@@LICENSE
*
*      Copyright (c) 2002-2012 Hewlett-Packard Development Company, L.P.
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

#include <glib.h>
#include <string.h>

#include "signals.h"
#include "util.h"

/**
 * Functions implemented in this file are documented in signals.h.
 */

#define LUNA_STORAGED "luna://com.palm.storage"

static LSPalmService* lsps = NULL;

void
SignalMSMAvailChange( LSHandle* lsh, bool avail )
{
    LSError lserror;
    LSErrorInit( &lserror );

    char* payload = g_strdup_printf( "{\"mode-avail\":%s}",
                                     avail?"true":"false" );

    const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_AVAIL;
    g_debug( "%s: sending %s to %s", __func__, payload, uri );

    if ( !LSSignalSend( lsh, uri, payload, &lserror ) ) {
        LSREPORT(lserror);
    }

    g_free( payload );

    LSErrorFree( &lserror );
}

void
SignalMSMModeChange( LSHandle* lsh, bool entering )
{
    LSError lserror;
    LSErrorInit( &lserror );

    char* modeParam = NULL;
    if ( entering ) {
        modeParam = g_strdup_printf( ", \"enterIMasq\": false");
    }

    char* payload = g_strdup_printf( "{\"new-mode\":\"%s\"%s}", 
		    entering?"brick":"phone",
		    modeParam ? modeParam : "");

    const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_MODE;
    g_debug( "%s: sending %s to %s", __func__, payload, uri );

    if ( !LSSignalSend( lsh, uri, payload, &lserror ) ) {
        LSREPORT(lserror);
    }

    g_free( payload );
    g_free( modeParam );

    LSErrorFree( &lserror );
}

void
SignalMSMFscking( LSHandle* lsh )
{
    LSError lserror;
    LSErrorInit( &lserror );


    char* payload = "{}";

    const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_FSCKING;
    g_debug( "%s: sending %s to %s", __func__, payload, uri );

    if ( !LSSignalSend( lsh, uri, payload, &lserror ) ) {
        LSREPORT(lserror);
    }

    LSErrorFree( &lserror );
}

void
SignalMSMProgress( LSHandle* lsh, const char* stage, bool forceRequired )
{
    LSError lserror;
    LSErrorInit( &lserror );

    char* forceParam = NULL;
    char* modeParam = NULL;
    if ( !strcmp(MSM_MODE_CHANGE_SUCCEEDED, stage) ) {
        forceParam = g_strdup_printf( ", \"forceRequired\": %s", 
                                      forceRequired?"true":"false" );
    }

    modeParam = g_strdup_printf( ", \"enterIMasq\": false" );

    char* payload = g_strdup_printf( "{\"stage\":\"%s\"%s%s}", stage, 
                                     forceParam?forceParam:"",
                                     modeParam);

    const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_PROGRESS;
    g_debug( "%s: sending %s to %s", __func__, payload, uri );

    if ( !LSSignalSend( lsh, uri, payload, &lserror ) ) {
        LSREPORT(lserror);
    }

    g_free( payload );
    g_free( forceParam );
    g_free( modeParam );

    LSErrorFree( &lserror );
}

void
SignalPartitionAvail( LSHandle* lsh, const char* mountPoint, bool avail,
                      bool reformatted, bool fsck_found_problem )
{
    LSError lserror;
    LSErrorInit( &lserror );

    char* payload_private = g_strdup_printf( "{\"mount_point\":\"%s\", "
            "\"available\":%s%s%s}",
            mountPoint, 
            avail?"true":"false",
            reformatted?", \"reformatted\": true":"",
            fsck_found_problem?", \"fscked\": true":"");
    char* payload_public = g_strdup_printf( "{\"mount_point\":\"%s\", "
            "\"available\":%s}",
            mountPoint, 
            avail?"true":"false");

    const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_PARTAVAIL;
    g_debug( "%s: sending %s to private %s", __func__, payload_private, uri );

    if ( !LSSignalSend( LSPalmServiceGetPrivateConnection(lsps), uri, payload_private, &lserror ) ) {
        LSREPORT(lserror);
    }

    g_debug( "%s: sending %s to public %s", __func__, payload_public, uri );
    if ( !LSSignalSend( LSPalmServiceGetPublicConnection(lsps), uri, payload_public, &lserror ) ) {
        LSREPORT(lserror);
    }

    g_free( payload_private );
    g_free( payload_public );

    LSErrorFree( &lserror );
}

void
SignalMSMStatus( LSHandle* lsh, bool inMSM)
{
	LSError lserror;
	LSErrorInit( &lserror );

	char* payload = g_strdup_printf("{\"inMSM\": %s}",inMSM?"true":"false");

	const char* uri = LUNA_STORAGED MSM_CATEGORY "/" MSM_METHOD_STATUS;
	g_debug( "%s: sending %s to private %s", __func__, payload, uri );

	if ( !LSSignalSend( LSPalmServiceGetPrivateConnection(lsps), uri, payload, &lserror ) ) {
			LSREPORT(lserror);
	}

	g_debug( "%s: sending %s to public %s", __func__, payload, uri );
	if ( !LSSignalSend( LSPalmServiceGetPublicConnection(lsps), uri, payload, &lserror ) ) {
			LSREPORT(lserror);
	}

	g_free( payload);

	LSErrorFree( &lserror );
}


static LSSignal signals[] = {
    { MSM_METHOD_AVAIL, 0 },
    { MSM_METHOD_PROGRESS, 0 },
    { MSM_METHOD_PARTAVAIL, 0 },
    { MSM_METHOD_MODE, 0 },
    { MSM_METHOD_FSCKING, 0 },
    { MSM_METHOD_STATUS, 0},
    { }
};

void
SignalsInit( LSPalmService* lsps_ )
    
{
    LSError lserror;
    LSErrorInit( &lserror );

    lsps = lsps_;

    if ( !LSPalmServiceRegisterCategory( lsps, MSM_CATEGORY, 
                              NULL, NULL, signals, NULL, &lserror) ) 
    {
        LSREPORT(lserror);
    }

    LSErrorFree( &lserror );
}
