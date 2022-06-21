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
 * @brief Functions for handling networks that do not form part of
 * the network API but are shared internally for use within ubxlib.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_device_shared.h"
#include "u_network_shared.h"

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
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

uDeviceHandle_t uNetworkGetDeviceHandle(uDeviceHandle_t devHandle,
                                        uNetworkType_t netType)
{
    uDeviceHandle_t returnedDevHandle = NULL;
    uDeviceInstance_t *pInstance;

    // This function does NOT lock the device API - if
    // it ends up being called from within the network API (e.g.
    // if a [GNSS] lower-level API function is called when bringing
    // up a network) then the device API will have already been
    // locked, if it ends up being called outside the network API
    // (e.g. directly be a lower-level API such as GNSS) then the
    // device API is not relevant in any case.

    if ((uDeviceGetInstance(devHandle, &pInstance) == 0) &&
        (netType >= U_NETWORK_TYPE_NONE) &&
        (netType < U_NETWORK_TYPE_MAX_NUM)) {
        switch (uDeviceGetDeviceType(devHandle)) {
            case U_DEVICE_TYPE_CELL: {
                if (netType == U_NETWORK_TYPE_CELL) {
                    returnedDevHandle = devHandle;
                } else if (netType == U_NETWORK_TYPE_GNSS) {
                    // For a GNSS network on a cellular device
                    // we go find the network data context pointer
                    // and _that_ is the GNSS "device" handle
                    for (size_t x = 0; (returnedDevHandle == NULL) &&
                         (x < sizeof(pInstance->networkData) /
                          sizeof(pInstance->networkData[0])); x++) {
                        if (pInstance->networkData[x].networkType == (int32_t) U_NETWORK_TYPE_GNSS) {
                            returnedDevHandle = (uDeviceHandle_t) pInstance->networkData[x].pContext;
                        }
                    }
                }
            }
            break;
            case U_DEVICE_TYPE_GNSS: {
                if (netType == U_NETWORK_TYPE_GNSS) {
                    returnedDevHandle = devHandle;
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE: {
                if ((netType == U_NETWORK_TYPE_WIFI) ||
                    (netType == U_NETWORK_TYPE_BLE)) {
                    returnedDevHandle = devHandle;
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU: {
                if (netType == U_NETWORK_TYPE_BLE) {
                    returnedDevHandle = devHandle;
                }
            }
            break;
            default:
                break;
        }
    }

    return returnedDevHandle;
}

// Get the network data for the given network type.
uDeviceNetworkData_t *pUNetworkGetNetworkData(uDeviceInstance_t *pInstance,
                                              uNetworkType_t netType)
{
    uDeviceNetworkData_t *pNetworkData = NULL;

    if (pInstance != NULL) {
        for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                            sizeof(pInstance->networkData[0])); x++) {
            if (pInstance->networkData[x].networkType == (int32_t) netType) {
                pNetworkData = &(pInstance->networkData[x]);
                break;
            }
        }
    }

    return pNetworkData;
}

// End of file
