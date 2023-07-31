/*
 * Copyright 2019-2023 u-blox
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
 * @brief Stubs to allow the Location API to be compiled without WiFi;
 * if you call a WiFi API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED for when WiFi is not included in the
 * build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_port_os.h"
#include "u_location.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_wifi_module_type.h"
#include "u_wifi_loc.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uWifiLocGet(uDeviceHandle_t wifiHandle,
                           uLocationType_t type, const char *pApiKey,
                           int32_t accessPointsFilter,
                           int32_t rssiDbmFilter,
                           uLocation_t *pLocation,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void) wifiHandle;
    (void) type;
    (void) pApiKey;
    (void) accessPointsFilter;
    (void) rssiDbmFilter;
    (void) pLocation;
    (void) pKeepGoingCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiLocGetStart(uDeviceHandle_t wifiHandle,
                                uLocationType_t type, const char *pApiKey,
                                int32_t accessPointsFilter,
                                int32_t rssiDbmFilter,
                                void (*pCallback) (uDeviceHandle_t wifiHandle,
                                                   int32_t errorCode,
                                                   const uLocation_t *pLocation))
{
    (void) wifiHandle;
    (void) type;
    (void) pApiKey;
    (void) accessPointsFilter;
    (void) rssiDbmFilter;
    (void) pCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uWifiLocGetStop(uDeviceHandle_t wifiHandle)
{
    (void) wifiHandle;
}

// End of file
