/*
 * Copyright 2019-2022 u-blox
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
 * @brief Implementation of the common portion of the network API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_network_shared.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_location.h"
#include "u_location_shared.h"

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"
#include "u_network_config_gnss.h"
#include "u_network_private_ble.h"
#include "u_network_private_cell.h"
#include "u_network_private_wifi.h"
#include "u_network_private_gnss.h"

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

// Bring a network up or down.
// This must be called between uDeviceLock() and uDeviceUnlock().
static int32_t networkInterfaceChangeState(uDeviceHandle_t devHandle,
                                           uNetworkType_t netType,
                                           const void *pNetworkCfg,
                                           bool upNotDown)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceInstance_t *pInstance;

    if (uDeviceGetInstance(devHandle, &pInstance) == 0) {
        switch (uDeviceGetDeviceType(devHandle)) {
            case U_DEVICE_TYPE_CELL: {
                if (netType == U_NETWORK_TYPE_CELL) {
                    errorCode = uNetworkPrivateChangeStateCell(devHandle,
                                                               (const uNetworkCfgCell_t *) pNetworkCfg,
                                                               upNotDown);
                } else if (netType == U_NETWORK_TYPE_GNSS) {
                    errorCode = uNetworkPrivateChangeStateGnss(devHandle,
                                                               (const uNetworkCfgGnss_t *) pNetworkCfg,
                                                               upNotDown);
                }
            }
            break;
            case U_DEVICE_TYPE_GNSS: {
                if (netType == U_NETWORK_TYPE_GNSS) {
                    errorCode = uNetworkPrivateChangeStateGnss(devHandle,
                                                               (const uNetworkCfgGnss_t *) pNetworkCfg,
                                                               upNotDown);
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE: {
                if (netType == U_NETWORK_TYPE_WIFI) {
                    //lint -e(1773) Suppress complaints about passing the pointer as non-volatile
                    errorCode = uNetworkPrivateChangeStateWifi(devHandle,
                                                               (const uNetworkCfgWifi_t *) pNetworkCfg,
                                                               upNotDown);
                } else if (netType == U_NETWORK_TYPE_BLE) {
                    errorCode = uNetworkPrivateChangeStateBle(devHandle,
                                                              (const uNetworkCfgBle_t *) pNetworkCfg,
                                                              upNotDown);
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU: {
                if (netType == U_NETWORK_TYPE_BLE) {
                    errorCode = uNetworkPrivateChangeStateBle(devHandle,
                                                              (const uNetworkCfgBle_t *) pNetworkCfg,
                                                              upNotDown);
                }
            }
            break;
            default:
                break;
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uNetworkInterfaceUp(uDeviceHandle_t devHandle,
                            uNetworkType_t netType,
                            const void *pCfg)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();
    uDeviceInstance_t *pInstance;
    uDeviceNetworkData_t *pNetworkData;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uDeviceGetInstance(devHandle, &pInstance) == 0) &&
            (netType >= U_NETWORK_TYPE_NONE) &&
            (netType < U_NETWORK_TYPE_MAX_NUM)) {
            pNetworkData = pUNetworkGetNetworkData(pInstance, netType);
            if (pNetworkData == NULL) {
                // No network of this type has yet been brought up on
                // this device
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pNetworkData = pUNetworkGetNetworkData(pInstance, U_NETWORK_TYPE_NONE);
            }
            if (pNetworkData != NULL) {
                pNetworkData->networkType = (int32_t) netType;
                errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                if (pCfg == NULL) {
                    // Use possible last set configuration
                    pCfg = pNetworkData->pCfg;
                }
                if (pCfg != NULL) {
                    pNetworkData->pCfg = pCfg;
                    errorCode = networkInterfaceChangeState(devHandle, netType,
                                                            pNetworkData->pCfg,
                                                            true);
                }
            }
        }
        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

int32_t uNetworkInterfaceDown(uDeviceHandle_t devHandle, uNetworkType_t netType)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();
    uDeviceInstance_t *pInstance;
    uDeviceNetworkData_t *pNetworkData;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uDeviceGetInstance(devHandle, &pInstance) == 0) &&
            (netType >= U_NETWORK_TYPE_NONE) &&
            (netType < U_NETWORK_TYPE_MAX_NUM)) {
            // If pNetworkData is NULL then this network has never
            // been brought up, hence success
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pNetworkData = pUNetworkGetNetworkData(pInstance, netType);
            if (pNetworkData != NULL) {
                errorCode = networkInterfaceChangeState(devHandle, netType,
                                                        pNetworkData->pCfg,
                                                        false);
                uPortFree(pNetworkData->pStatusCallbackData);
                pNetworkData->pStatusCallbackData = NULL;
            }
        }
        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

// Set a network status callback.
int32_t uNetworkSetStatusCallback(uDeviceHandle_t devHandle,
                                  uNetworkType_t netType,
                                  uNetworkStatusCallback_t pCallback,
                                  void *pCallbackParameter)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();
    uDeviceInstance_t *pInstance;
    uDeviceNetworkData_t *pNetworkData;
    uNetworkStatusCallbackData_t *pStatusCallbackData;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uDeviceGetInstance(devHandle, &pInstance) == 0) &&
            (netType >= U_NETWORK_TYPE_NONE) &&
            (netType < U_NETWORK_TYPE_MAX_NUM)) {
            // If pNetworkData is NULL then this network has not
            // been brought up
            pNetworkData = pUNetworkGetNetworkData(pInstance, netType);
            if (pNetworkData != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Allocate space for the status callback data
                // and attach it to the network data block;
                // the various callback functions can then
                // obtain it from there with a call to
                // pUNetworkGetNetworkData()
                if (pNetworkData->pStatusCallbackData == NULL) {
                    pNetworkData->pStatusCallbackData = pUPortMalloc(sizeof(uNetworkStatusCallbackData_t));
                }
                pStatusCallbackData = (uNetworkStatusCallbackData_t *) pNetworkData->pStatusCallbackData;
                if (pStatusCallbackData != NULL) {
                    pStatusCallbackData->pCallback = pCallback;
                    pStatusCallbackData->pCallbackParameter = pCallbackParameter;
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                    switch (netType) {
                        case U_NETWORK_TYPE_BLE:
                            errorCode = uNetworkSetStatusCallbackBle(devHandle);
                            break;
                        case U_NETWORK_TYPE_CELL:
                            errorCode = uNetworkSetStatusCallbackCell(devHandle);
                            break;
                        case U_NETWORK_TYPE_WIFI:
                            errorCode = uNetworkSetStatusCallbackWifi(devHandle);
                            break;
                        case U_NETWORK_TYPE_GNSS:
                            // Not relevant to GNSS
                            break;
                        default:
                            break;
                    }
                    if (errorCode != 0) {
                        uPortFree(pNetworkData->pStatusCallbackData);
                        pNetworkData->pStatusCallbackData = NULL;
                    }
                }
            }
        }
        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

// End of file
