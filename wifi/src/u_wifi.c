/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the "general" API for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"
#include "u_wifi_private.h"


/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

uWifiModuleType_t shortRangeToWifiModule(uShortRangeModuleType_t module)
{
    const uShortRangeModuleInfo_t *pModuleInfo;
    pModuleInfo = uShortRangeGetModuleInfo(module);
    if (!pModuleInfo) {
        return U_WIFI_MODULE_TYPE_INVALID;
    }
    if (!pModuleInfo->supportsWifi) {
        return U_WIFI_MODULE_TYPE_UNSUPPORTED;
    }
    return (uWifiModuleType_t)module;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the wifi driver.
int32_t uWifiInit()
{
    return uShortRangeInit();
}

// Shut-down the wifi driver.
void uWifiDeinit()
{
    uShortRangeDeinit();
}

// Add a wifi instance.
int32_t uWifiAdd(uWifiModuleType_t moduleType,
                 uAtClientHandle_t atHandle)
{
    int32_t handle;
    // First make sure the moduleType value is really valid
    // If not shortRangeToWifiModule() will return a negative value
    // that uShortRangeAdd() will reject
    moduleType = shortRangeToWifiModule((uShortRangeModuleType_t)moduleType);
    handle = uShortRangeAdd((uShortRangeModuleType_t)moduleType, atHandle);
    if (handle >= 0) {
        // If the module was added successfully we convert the handle to a wifi handle
        handle = uShoToWifiHandle(handle);
    }
    return handle;
}

// Remove a wifi instance.
void uWifiRemove(int32_t wifiHandle)
{
    int32_t shoHandle = uWifiToShoHandle(wifiHandle);
    uShortRangeRemove(shoHandle);
}

// Get the handle of the AT client.
int32_t uWifiAtClientHandleGet(int32_t wifiHandle,
                               uAtClientHandle_t *pAtHandle)
{
    int32_t shoHandle = uWifiToShoHandle(wifiHandle);
    return uShortRangeAtClientHandleGet(shoHandle, pAtHandle);
}

uWifiModuleType_t uWifiDetectModule(int32_t wifiHandle)
{
    int32_t errorCode;
    uWifiModuleType_t wifiModule = U_WIFI_MODULE_TYPE_INVALID;
    int32_t shoHandle = uWifiToShoHandle(wifiHandle);
    errorCode = uShortRangeLock();

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uShortRangeModuleType_t shortRangeModule = uShortRangeDetectModule(shoHandle);
        wifiModule = shortRangeToWifiModule(shortRangeModule);
        uShortRangeUnlock();
    }

    return wifiModule;
}

// End of file
