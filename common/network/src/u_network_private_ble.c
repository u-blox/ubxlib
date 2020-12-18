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
 * @brief Implementation of the BLE  portion of the
 * network API. The contents of this file aren't any more
 * "private" than the other sources files but the associated header
 * file should be private and this is simply named to match.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_cfg.h"

#include "u_network.h"
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
    int32_t uartHandle; /**< The handle returned by uPortUartOpen(). */
    int32_t edmStreamHandle; /**< The handle returned by uPorrEdmStreamOpen(). */
    uAtClientHandle_t atClientHandle; /**< The handle returned by uAtClientAdd(). */
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
        if (gInstance[x].uartHandle < 0) {
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for BLE.
int32_t uNetworkInitBle()
{
    uShortRangeEdmStreamInit();
    uAtClientInit();
    uBleInit();

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        gInstance[x].uartHandle = -1;
        gInstance[x].atClientHandle = NULL;
        gInstance[x].edmStreamHandle = -1;
        gInstance[x].bleHandle = -1;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the sho network API.
void uNetworkDeinitBle()
{
    uAtClientDeinit();
    uShortRangeEdmStreamDeinit();
    uBleDeinit();
}

// Add a BLE network instance.
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateBleInstance_t *pInstance;

    pInstance = pGetFree();
    if (pInstance != NULL) {
        // Open a UART with the recommended buffer length
        // and default baud rate.
        errorCode = uPortUartOpen(pConfiguration->uart,
                                  U_SHORT_RANGE_UART_BAUD_RATE, NULL,
                                  U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES,
                                  pConfiguration->pinTxd,
                                  pConfiguration->pinRxd,
                                  pConfiguration->pinCts,
                                  pConfiguration->pinRts);
        if (errorCode >= 0) {
            pInstance->uartHandle = errorCode;
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_AT;

            pInstance->edmStreamHandle = uShortRangeEdmStreamOpen(pInstance->uartHandle);

            if (pInstance->edmStreamHandle >= 0) {
                // Add an AT client on the UART with the recommended
                // default buffer size.
                pInstance->atClientHandle = uAtClientAdd(pInstance->edmStreamHandle,
                                                         U_AT_CLIENT_STREAM_TYPE_EDM,
                                                         NULL,
                                                         U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
                if (pInstance->atClientHandle != NULL) {
                    uShortRangeEdmStreamSetAtHandle(pInstance->edmStreamHandle, pInstance->atClientHandle);

                    // Set printing of AT commands by the short range driver,
                    // which can be useful while debugging.
                    uAtClientPrintAtSet(pInstance->atClientHandle, true);

                    errorCode = uBleAdd((uBleModuleType_t) pConfiguration->module,
                                        pInstance->atClientHandle);

                    if (errorCode >= 0) {
                        pInstance->bleHandle = errorCode;
                        uBleModuleType_t module = uBleDetectModule(pInstance->bleHandle);

                        if (module == U_BLE_MODULE_TYPE_INVALID) {
                            errorCode = (int32_t)  U_SHORT_RANGE_ERROR_NOT_DETECTED;
                        } else if ((int32_t) module != pConfiguration->module) {
                            errorCode = (int32_t) U_SHORT_RANGE_ERROR_WRONG_TYPE;
                        }
                    }
                }
            }
        }

        if (errorCode < 0) {
            uBleRemove(pInstance->bleHandle);
            uAtClientRemove(pInstance->atClientHandle);
            uShortRangeEdmStreamClose(pInstance->edmStreamHandle);
            uPortUartClose(pInstance->uartHandle);
            pInstance->uartHandle = -1;
            pInstance->atClientHandle = NULL;
            pInstance->edmStreamHandle = -1;
            pInstance->bleHandle = -1;
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
        pInstance->bleHandle = -1;
        uAtClientRemove(pInstance->atClientHandle);
        pInstance->atClientHandle = NULL;
        uShortRangeEdmStreamClose(pInstance->edmStreamHandle);
        pInstance->edmStreamHandle = -1;
        uPortUartClose(pInstance->uartHandle);
        pInstance->uartHandle = -1;
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

// End of file
