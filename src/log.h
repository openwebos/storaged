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

void setLogLevel(int level);
void setUseSyslog( bool useit );
void logFilter(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer unused_data);

#define BUG() { \
    *( (int*) NULL) = 0; \
}

#define LOG_FATAL(msg) { \
    do { \
        g_critical(msg); \
        BUG(); \
    } while (false); \
}
