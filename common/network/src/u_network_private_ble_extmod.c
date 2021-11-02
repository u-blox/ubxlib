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
 * @brief Implementation of the BLE  portion of the
 * network API. The contents of this file aren't any more
 * "private" than the other sources files but the associated header
 * file should be private and this is simply named to match.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_cfg.h"

#include "u_network.h"
#include "u_network_private_short_range.h"
#include "u_network_config_ble.h"
#include "u_network_private_ble.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_BLE_MAX_NUM
# define U_NETWORK_PRIVATE_BLE_MAX_NUM 1
#endif
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    int32_t netShoHandle;  /**< The handle returned by uNetworkAddShortRange(). */
    int32_t bleHandle;  /**< The handle returned by uBleAdd(). */
} uNetworkPrivateBleInstance_t;



/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to keep track of the instances.
 */
static uNetworkPrivateBleInstance_t gInstance[U_NETWORK_PRIVATE_BLE_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uNetworkPrivateBleInstance_t *pGetFree()
{
    uNetworkPrivateBleInstance_t *pFree = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pFree == NULL); x++) {
        if (gInstance[x].netShoHandle < 0) {
            pFree = &(gInstance[x]);
        }
    }

    return pFree;
}

// Find the given instance in the list.
static uNetworkPrivateBleInstance_t *pGetInstance(int32_t bleHandle)
{
    uNetworkPrivateBleInstance_t *pInstance = NULL;

    // Find the handle in the list
    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pInstance == NULL); x++) {
        if (gInstance[x].bleHandle == bleHandle) {
            pInstance = &(gInstance[x]);
        }
    }

    return pInstance;
}

static void clearInstance(uNetworkPrivateBleInstance_t *pInstance)
{
    pInstance->bleHandle = -1;
    pInstance->netShoHandle = -1;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for BLE.
int32_t uNetworkInitBle(void)
{
    int32_t errorCode = uNetworkInitShortRange();
    if (errorCode >= 0) {
        errorCode = uBleInit();
    }

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        clearInstance(&gInstance[x]);
    }

    return errorCode;
}

// Deinitialise the sho network API.
void uNetworkDeinitBle(void)
{
    uBleDeinit();
    uNetworkDeinitShortRange();
}

// Add a BLE network instance.
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateBleInstance_t *pInstance;
    const uShortRangeConfig_t shoConfig = {
        .module = pConfiguration->module,
        .uart = pConfiguration->uart,
        .pinTxd = pConfiguration->pinTxd,
        .pinRxd = pConfiguration->pinRxd,
        .pinCts = pConfiguration->pinCts,
        .pinRts = pConfiguration->pinRts
    };

    // Check that the module supports BLE
    const uShortRangeModuleInfo_t *pModuleInfo;
    pModuleInfo = uShortRangeGetModuleInfo(pConfiguration->module);
    if (!pModuleInfo) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (!pModuleInfo->supportsBle) {
        return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    }

    pInstance = pGetFree();
    if (pInstance != NULL) {
        errorCode = uNetworkAddShortRange(&shoConfig);
        pInstance->netShoHandle = errorCode;

        if (errorCode >= 0) {
            uAtClientHandle_t atHandle = uNetworkGetAtClientShortRange(pInstance->netShoHandle);
            errorCode = uBleAdd((uBleModuleType_t) pConfiguration->module, atHandle);
            pInstance->bleHandle = errorCode;
        }

        if (errorCode >= 0) {
            errorCode = pInstance->bleHandle;
        } else {
            // Something went wrong - cleanup...
            if (pInstance->bleHandle >= 0) {
                uBleRemove(pInstance->bleHandle);
            }

            if (pInstance->netShoHandle >= 0) {
                uNetworkRemoveShortRange(pInstance->netShoHandle);
            }
            clearInstance(pInstance);
        }
    }

    return errorCode;
}

// Remove a BLE network instance.
int32_t uNetworkRemoveBle(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateBleInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        uBleRemove(pInstance->bleHandle);
        uNetworkRemoveShortRange(pInstance->netShoHandle);
        clearInstance(pInstance);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Bring up the given BLE network instance.
int32_t uNetworkUpBle(int32_t handle,
                      const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateBleInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL && pInstance->bleHandle >= 0) {
        uBleCfg_t cfg;
        cfg.role = (uBleCfgRole_t) pConfiguration->role;
        cfg.spsServer = pConfiguration->spsServer;
        errorCode = uBleCfgConfigure(pInstance->bleHandle, &cfg);
        if (errorCode >= 0) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Take down the given BLE network instance.
int32_t uNetworkDownBle(int32_t handle,
                        const uNetworkConfigurationBle_t *pConfiguration)
{
    // Up and down is the same function as the pConfiguration variable determines
    // if ble and/or sps is enabled or disabled. So we trust the user to set the
    // correct values here.
    return uNetworkUpBle(handle, pConfiguration);
}

#endif
// End of file
