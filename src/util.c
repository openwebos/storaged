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

#include <stdbool.h>
#include <string.h>

#include "util.h"

static bool
isNumber( const gchar* name )
{
    const char* sptr = name;
    bool passes = true;

    for ( sptr = name; '\0' != *sptr; ++sptr )
    {
        if (!g_ascii_isdigit( *sptr) )
        {
            passes = false;
            break;
        }
    }

    return passes;
}

void
log_blame( const char* prefix )
{
    GDir* dir;
    const gchar* entry;

    dir = g_dir_open ("/proc", 0, NULL);

    if (NULL == dir)
    {
        g_warning ("failed scanning /proc");
        return ;
    }

    while ((entry = g_dir_read_name (dir)) != NULL)
    {
        if (isNumber (entry))
        {
            gchar* fdpath = g_build_path ("/", "/proc", entry, "fd", NULL);

            if (g_file_test(fdpath, G_FILE_TEST_IS_DIR))
            {
                GDir* fddir;
                fddir = g_dir_open (fdpath, 0, NULL);

                if (NULL != dir)
                {
                    const gchar *nentry;
                    gchar* exe = NULL;
                    while ((nentry = g_dir_read_name (fddir)) != NULL)
                    {
                        gchar *lnpath = g_build_path ("/", "/proc", entry, "fd", nentry, NULL);
                        if (g_file_test(lnpath, G_FILE_TEST_IS_SYMLINK))
                        {
                            gchar* link = g_file_read_link (lnpath, NULL);
                            if (g_ascii_strncasecmp (link, prefix, strlen(prefix)) == 0)
                            {
                                if (NULL == exe)
                                {
                                    gchar *epath = g_build_path ("/", "/proc", entry, "exe", NULL);
                                    if (g_file_test(epath, G_FILE_TEST_IS_SYMLINK))
                                    {
                                        exe = g_file_read_link (epath, NULL);
                                    }
                                    else
                                    {
                                       exe = g_strdup_printf ("(unknown PID=%s)", entry);
                                    }

                                    g_warning ("Application %s (%s) has the following files open:", exe, entry);
                                    g_free (epath);
                                }
                                g_warning ("file: (%s)", link);
                            }
                            g_free (link);
                        }
                        g_free (lnpath);
                    }
                    g_dir_close (fddir);
                    if (NULL != exe)
                        g_free (exe);
                }
            }
            g_free (fdpath);
        }
    }
    g_dir_close (dir);
} /* log_blame */
