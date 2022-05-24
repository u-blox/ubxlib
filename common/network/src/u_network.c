/*
 * Copyright 2022 u-blox
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
 * @brief Implementation of the common portion of the network API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // malloc(), free()

#include "u_error_common.h"

#include "u_device_shared.h"

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
                                           bool upNotDown)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceInstance_t *pInstance;

    if (uDeviceGetInstance(devHandle, &pInstance) == 0) {
        switch (uDeviceGetDeviceType(devHandle)) {
            case U_DEVICE_TYPE_CELL: {
                uNetworkCfgCell_t *pDevCfgCell = (uNetworkCfgCell_t *)
                                                 pInstance->pNetworkCfg[U_NETWORK_TYPE_CELL];
                errorCode = uNetworkPrivateChangeStateCell(devHandle, pDevCfgCell, upNotDown);
            }
            break;
            case U_DEVICE_TYPE_GNSS: {
                uNetworkCfgGnss_t *pDevCfgGnss = (uNetworkCfgGnss_t *)
                                                 pInstance->pNetworkCfg[U_NETWORK_TYPE_GNSS];
                errorCode = uNetworkPrivateChangeStateGnss(devHandle, pDevCfgGnss, upNotDown);
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE: {
                if (netType == U_NETWORK_TYPE_WIFI) {
                    //lint -e(1773) Suppress complaints about passing the pointer as non-volatile
                    uNetworkCfgWifi_t *pDevCfgWifi = (uNetworkCfgWifi_t *)
                                                     pInstance->pNetworkCfg[U_NETWORK_TYPE_WIFI];
                    errorCode = uNetworkPrivateChangeStateWifi(devHandle, pDevCfgWifi, upNotDown);
                } else if (netType == U_NETWORK_TYPE_BLE) {
                    uNetworkCfgBle_t *pDevCfgBle = (uNetworkCfgBle_t *)
                                                   pInstance->pNetworkCfg[U_NETWORK_TYPE_BLE];
                    errorCode = uNetworkPrivateChangeStateBle(devHandle, pDevCfgBle, upNotDown);
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU: {
                if (netType == U_NETWORK_TYPE_BLE) {
                    uNetworkCfgBle_t *pDevCfgBle = (uNetworkCfgBle_t *)
                                                   pInstance->pNetworkCfg[U_NETWORK_TYPE_BLE];
                    errorCode = uNetworkPrivateChangeStateBle(devHandle, pDevCfgBle, upNotDown);
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
                            const void *pConfiguration)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();
    uDeviceInstance_t *pInstance;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uDeviceGetInstance(devHandle, &pInstance) == 0) &&
            (netType >= U_NETWORK_TYPE_NONE) &&
            (netType < U_NETWORK_TYPE_MAX_NUM)) {

            if (pConfiguration == NULL) {
                // Use possible last set configuration
                pConfiguration = pInstance->pNetworkCfg[netType];
            }
            if (pConfiguration != NULL) {
                pInstance->pNetworkCfg[netType] = pConfiguration;
                errorCode = networkInterfaceChangeState(devHandle, netType, true);
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

    if (errorCode == 0) {
        errorCode = networkInterfaceChangeState(devHandle, netType, false);
        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

// End of file
