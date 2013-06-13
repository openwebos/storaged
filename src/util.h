/* @@@LICENSE
*
*      Copyright (c) 2002-2013 LG Electronics, Inc.
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <glib.h>


void disable_lifetime_timer();

void reset_lifetime_timer();

#define LSREPORT(lse) g_critical( "in %s: %s => %s", __func__, \
                                  (lse).func, (lse).message )

#define SHOW_STDERR(standard_error) \
    if (standard_error != NULL) { \
        if (strlen(standard_error) > 0) { \
            g_critical( "%s: stderr: %s", __func__, standard_error ); \
        } \
        g_free(standard_error); \
        standard_error = NULL; \
    }

#define SHOW_ERROR(error) \
    if (error != NULL) { \
        g_critical( "%s: error=>%s", __func__, error->message ); \
        g_error_free(error); \
        error = NULL; \
    }


#define LSTRACE_LSMESSAGE(message) \
    do { \
        const char *payload = LSMessageGetPayload(message); \
        g_debug( "%s(%s)", __func__, (NULL == payload) ? "{}" : payload ); \
        struct json_object *object = json_tokener_parse(payload);	\
        if(is_error(object))	break;	\
        const char *errorCode; \
        const char *errorText; \
        errorCode =  json_object_get_string(json_object_object_get(object, "errorCode"));	\
        g_warning("%s: called with errorCode = %s\n", __FUNCTION__, errorCode); \
        errorText =  json_object_get_string(json_object_object_get(object, "errorText"));	\
        g_warning("%s: called with errorText = %s\n", __FUNCTION__, errorText); \
        json_object_put(object);	\
    } while (0)


/**
 *  write to syslog a list of apps that have open file descriptors inside some
 *  directory.  Typically this will be used to "blame" folks keeping files
 *  open in /media/internal that prevent us from cleanly unmounting the
 *  partition there when going into brick mode.
 *
 * @param dirPath        fully-qualified path describing directory to search
 *                       for files.
 */
void log_blame( const char* dirPath );


#endif
