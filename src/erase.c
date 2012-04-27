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

#include <cjson/json.h>
#include <glib/gstdio.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <lunaservice.h>
#include "util.h"
#include "erase.h"
#include "main.h"

typedef enum EraseType
{
    kEraseVar,
    kEraseAll,
    kEraseMedia,
    kWipe,
} EraseType_t;

static nyx_device_handle_t nyxSystem = NULL;

/** 
 * @brief Erase
 *
 * Set the run level, which executes the reset scripts.  These
 * scripts bring the system down cleanly, then reboot into the
 * mountall script that erases /var or both /var and the user
 * partition.
 * 
 * @param pHandle 
 * @param pMessage 
 * @param type 
 */
static void
Erase(LSHandle* pHandle, LSMessage* pMessage, EraseType_t type)
{
    LSError lserror;
    char *error_text=NULL;
    char* return_msg = NULL;
    nyx_system_erase_type_t nyx_type;

    // write flag file used by mountall.sh
    switch (type)
    {
        case kEraseVar:
        	nyx_type = NYX_SYSTEM_ERASE_VAR;
            break;
            
        case kEraseAll:
        	nyx_type = NYX_SYSTEM_ERASE_ALL;
            break;
            
        case kEraseMedia:
        	nyx_type = NYX_SYSTEM_ERASE_MEDIA;
            break;
            
        case kWipe:
        	nyx_type = NYX_SYSTEM_WIPE;
        	break;

        default:
            error_text = g_strdup_printf("Invalid type %d", type);
            goto err;
    }

    nyx_error_t ret = 0;
    ret = nyx_system_erase_partition(nyxSystem,nyx_type,error_text);
    if(ret != NYX_ERROR_NONE) {
    	g_debug("Failed to execute nyx_system_erase_partition, ret : %d",ret);
    	error_text = g_strdup_printf("Failed to execute NYX erase API");
    }

    
err:
    if (error_text) {
        g_warning("%s: %s", __func__, error_text);
        return_msg = g_strdup_printf("{\"returnValue\":false, \"errorText\":\"%s\"}", error_text);
        g_free(error_text);
    } else {
        return_msg = g_strdup_printf("{\"returnValue\":true}");
    }


    LSErrorInit(&lserror);        
    if (!LSMessageReply(pHandle, pMessage, return_msg, &lserror)) 
        LSREPORT( lserror );
    g_free(return_msg);
    LSErrorFree(&lserror);
}

/** 
 * @brief handle_erase_var
 * 
 * @param pHandle 
 * @param pMessage 
 * @param pUserData 
 * 
 * @return 
 */
static bool
handle_erase_var(LSHandle* pHandle, LSMessage* pMessage, void* pUserData)
{
    LSTRACE_LSMESSAGE(pMessage);
    Erase(pHandle, pMessage, kEraseVar);
    
    return true;
}

/** 
 * @brief handle_erase_all
 * 
 * @param pHandle 
 * @param pMessage 
 * @param pUserData 
 * 
 * @return 
 */
static bool
handle_erase_all(LSHandle* pHandle, LSMessage* pMessage, void* pUserData)
{
    LSTRACE_LSMESSAGE(pMessage);
    Erase(pHandle, pMessage, kEraseAll);
    
    return true;
}

/** 
 * @brief handle_erase_media
 * 
 * @param pHandle 
 * @param pMessage 
 * @param pUserData 
 * 
 * @return 
 */
static bool
handle_erase_media(LSHandle* pHandle, LSMessage* pMessage, void* pUserData)
{
    LSTRACE_LSMESSAGE(pMessage);
    Erase(pHandle, pMessage, kEraseMedia);
    
    return true;
}

/** 
 * @brief handle_secure_wipe
 * 
 * @param pHandle 
 * @param pMessage 
 * @param pUserData 
 * 
 * @return 
 */
static bool
handle_secure_wipe(LSHandle* pHandle, LSMessage* pMessage, void* pUserData)
{
    LSTRACE_LSMESSAGE(pMessage);
    Erase(pHandle, pMessage, kWipe);
    
    return true;
}

static LSMethod erase_mthds[] = {
    { "EraseVar", handle_erase_var },
    { "EraseAll", handle_erase_all },
    { "EraseMedia", handle_erase_media },
    { "Wipe", handle_secure_wipe },
    { },
};


/** 
 * EraseInit
 *
 * @brief Register storaged with luna-service as implementer of several
 * methods.
 */
int
EraseInit(GMainLoop *loop, LSHandle* handle)
{
    LSError lserror;
    LSErrorInit(&lserror);

    if ( !LSRegisterCategory ( handle, "/erase", erase_mthds, 
                NULL, NULL, &lserror) ) 
    {
        LSREPORT( lserror );
    }
    LSErrorFree( &lserror );
    nyxSystem = GetNyxSystemDevice();

    return 0;
}

