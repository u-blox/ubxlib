/*
 * Copyright 2019-2024 u-blox
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
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the location APIs for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strtok_r()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_YIELD_MS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_location.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_wifi_module_type.h"
#include "u_wifi_loc.h"
#include "u_wifi_loc_private.h"
#include "u_wifi_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiLocPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO WIFI LOC
 * -------------------------------------------------------------- */

// Process a URC containing a LOC response.
void uWifiLocPrivateUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    (void)atHandle;
    (void)pParameter;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// ##### NOT supported in uCx yet! #####

int32_t uWifiLocGet(uDeviceHandle_t wifiHandle,
                    uLocationType_t type, const char *pApiKey,
                    int32_t accessPointsFilter,
                    int32_t rssiDbmFilter,
                    uLocation_t *pLocation,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void)wifiHandle;
    (void)type;
    (void)pApiKey;
    (void)accessPointsFilter;
    (void)rssiDbmFilter;
    (void)pLocation;
    (void)pKeepGoingCallback;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// Get the current location, non-blocking version.
int32_t uWifiLocGetStart(uDeviceHandle_t wifiHandle,
                         uLocationType_t type, const char *pApiKey,
                         int32_t accessPointsFilter,
                         int32_t rssiDbmFilter,
                         uWifiLocCallback_t *pCallback)
{
    (void)wifiHandle;
    (void)type;
    (void)pApiKey;
    (void)accessPointsFilter;
    (void)rssiDbmFilter;
    (void)pCallback;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// Cancel a uWifiLocGetStart().
void uWifiLocGetStop(uDeviceHandle_t wifiHandle)
{
    (void)wifiHandle;
}

// Free the mutex that is protecting the data passed around by uWifiLoc.
void uWifiLocFree(uDeviceHandle_t wifiHandle)
{
    (void)wifiHandle;
}

// End of file
