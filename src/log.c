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

#include <stdio.h>
#include <glib.h>
#include <syslog.h>
#include <stdbool.h>

static int sLogLevel = G_LOG_LEVEL_MESSAGE;
static bool sUseSyslog = false;

void setLogLevel(int level)
{
    sLogLevel = level;
}

void
setUseSyslog( bool useit )
{
    sUseSyslog = useit;
}

void logFilter(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer unused_data)
{
    if (log_level > sLogLevel) return;

    if (sUseSyslog)
    {
        int priority;
        switch (log_level & G_LOG_LEVEL_MASK) {
            case G_LOG_LEVEL_ERROR:
                priority = LOG_CRIT;
                break;
            case G_LOG_LEVEL_CRITICAL:
                priority = LOG_ERR;
                break;
            case G_LOG_LEVEL_WARNING:
                priority = LOG_WARNING;
                break;
            case G_LOG_LEVEL_MESSAGE:
                priority = LOG_NOTICE;
                break;
            case G_LOG_LEVEL_DEBUG:
                priority = LOG_DEBUG;
                break;
            case G_LOG_LEVEL_INFO:
            default:
                priority = LOG_INFO;
                break;
        }
        syslog(priority, "%s", message);
    }
    else
    {
        g_log_default_handler(log_domain, log_level, message, unused_data);
    }
}

