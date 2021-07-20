/*
 * Copyright 2020 u-blox Ltd
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
 * @brief Private wifi functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "string.h"
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_wifi_private.h"

#include "u_network_handle.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uWifiToShoHandle(int32_t wifiHandle)
{
    if ((wifiHandle < (int32_t)U_NETWORK_HANDLE_WIFI_MIN) ||
        (wifiHandle > (int32_t)U_NETWORK_HANDLE_WIFI_MAX)) {
        return -1;
    }
    return wifiHandle - (int32_t)U_NETWORK_HANDLE_WIFI_MIN;
}

int32_t uShoToWifiHandle(int32_t shortRangeHandle)
{
    if ((shortRangeHandle < 0) ||
        (shortRangeHandle >= (int32_t)U_NETWORK_HANDLE_RANGE)) {
        return -1;
    }
    return shortRangeHandle + (int32_t)U_NETWORK_HANDLE_WIFI_MIN;
}

// End of file
