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
 * @brief Implementation of the "general" API for ble.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

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

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_private.h"

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

uBleModuleType_t shortRangeToBleModule(uShortRangeModuleType_t module)
{
    const uShortRangeModuleInfo_t *pModuleInfo;
    pModuleInfo = uShortRangeGetModuleInfo(module);
    //lint -e(568) Suppress value never being negative
    if (!pModuleInfo) {
        return U_BLE_MODULE_TYPE_INVALID;
    }
    if (!pModuleInfo->supportsBle) {
        return U_BLE_MODULE_TYPE_UNSUPPORTED;
    }
    return (uBleModuleType_t)module;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the ble driver.
int32_t uBleInit()
{
    uBleDataPrivateInit();
    return uShortRangeInit();
}

// Shut-down the ble driver.
void uBleDeinit()
{
    uBleDataPrivateDeinit();
    uShortRangeDeinit();
}

// Add a ble instance.
int32_t uBleAdd(uBleModuleType_t moduleType,
                uAtClientHandle_t atHandle)
{
    int32_t errorCode;
    // First make sure the moduleType value is really valid
    // If not shortRangeToBleModule() will return a negative value
    // that uShortRangeAdd() will reject
    moduleType = shortRangeToBleModule((uShortRangeModuleType_t)moduleType);

    errorCode = uShortRangeLock();

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        errorCode = uShortRangeAdd((uShortRangeModuleType_t) moduleType, atHandle);
        uShortRangeUnlock();
    }
    if (errorCode >= 0) {
        // If we successfully added the module we convert the sho handle to a BLE handle
        errorCode = uShoToBleHandle(errorCode);
    }

    return errorCode;
}

// Remove a ble instance.
void uBleRemove(int32_t bleHandle)
{
    int32_t errorCode;
    int32_t shoHandle = uBleToShoHandle(bleHandle);
    errorCode = uShortRangeLock();

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uShortRangeRemove(shoHandle);
        uShortRangeUnlock();
    }
}

// Get the handle of the AT client.
int32_t uBleAtClientHandleGet(int32_t bleHandle,
                              uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode;
    int32_t shoHandle = uBleToShoHandle(bleHandle);
    errorCode = uShortRangeLock();

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        errorCode = uShortRangeAtClientHandleGet(shoHandle, pAtHandle);
        uShortRangeUnlock();
    }

    return errorCode;
}

uBleModuleType_t uBleDetectModule(int32_t bleHandle)
{
    int32_t errorCode;
    int32_t shoHandle = uBleToShoHandle(bleHandle);
    uBleModuleType_t bleModule = U_BLE_MODULE_TYPE_INVALID;
    errorCode = uShortRangeLock();

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uShortRangeModuleType_t shortRangeModule = uShortRangeDetectModule(shoHandle);
        bleModule = shortRangeToBleModule(shortRangeModule);
        uShortRangeUnlock();
    }

    return bleModule;
}

#endif

// End of file
