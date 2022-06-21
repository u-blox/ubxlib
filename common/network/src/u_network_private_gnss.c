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
 * @brief Implementation of the GNSS portion of the network API. The
 * contents of this file aren't any more "private" than the other
 * sources files but the associated header file should be private and
 * this is simply named to match.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"                 // For uCellAtClientHandleGet()
#include "u_cell_loc.h"             // For uCellLocSetPinGnssPwr()/uCellLocSetPinGnssDataReady()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_shared_gnss.h"

#include "u_network.h"
#include "u_network_shared.h"
#include "u_network_config_gnss.h"
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

// Set the GNSS device handle that is actually the _network_ context
// pointer, used when the underlying device is a non-GNSS device.
static void setGnssDeviceHandle(uDeviceHandle_t devHandle,
                                uDeviceHandle_t gnssDeviceHandle)
{
    uDeviceInstance_t *pInstance = U_DEVICE_INSTANCE(devHandle);

    for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                        sizeof(pInstance->networkData[0])); x++) {
        if (pInstance->networkData[x].networkType == U_NETWORK_TYPE_GNSS) {
            pInstance->networkData[x].pContext = gnssDeviceHandle;
            break;
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Bring a GNSS interface up or take it down.
int32_t uNetworkPrivateChangeStateGnss(uDeviceHandle_t devHandle,
                                       const uNetworkCfgGnss_t *pCfg,
                                       bool upNotDown)
{
    uDeviceInstance_t *pDevInstance;
    int32_t errorCode = uDeviceGetInstance(devHandle, &pDevInstance);
    uDeviceType_t deviceType;
    uGnssTransportHandle_t gnssTransportHandle;
    uDeviceHandle_t gnssDeviceHandle = NULL;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_GNSS) &&
            (pDevInstance->pContext != NULL)) {

            gnssTransportHandle.pAt = NULL;
            deviceType = (uDeviceType_t) uDeviceGetDeviceType(devHandle);
            switch (deviceType) {
                case U_DEVICE_TYPE_GNSS:
                    // If the device was a GNSS device it will have been
                    // added and powered up by uDeviceOpen(), there's
                    // nothing else we need to do
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    break;
                case U_DEVICE_TYPE_CELL:
                    // Get the AT handle so that we can add and
                    // configure GNSS through AT commands via
                    // the cellular module
                    errorCode = uCellAtClientHandleGet(devHandle,
                                                       //lint -e(181) Suppress unusual pointer cast
                                                       (uAtClientHandle_t *) &(gnssTransportHandle.pAt)); // *NOPAD*);
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE:
                case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                default:
                    break;
            }

            if (gnssTransportHandle.pAt != NULL) {
                // We're connected via an intermediate module
                // so we may need to do stuff if we're not
                // already in the requested state
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                gnssDeviceHandle = uNetworkGetDeviceHandle(devHandle, U_NETWORK_TYPE_GNSS);
                if (upNotDown) {
                    if (gnssDeviceHandle == NULL) {
                        // No piggy-backed GNSS "device" already
                        // exists so create it
                        errorCode = uGnssAdd((uGnssModuleType_t) pCfg->moduleType,
                                             U_GNSS_TRANSPORT_UBX_AT,
                                             gnssTransportHandle,
                                             -1, false,
                                             &gnssDeviceHandle);
                    }
                    if (errorCode == 0) {
                        // Hook the GNSS "device" handle off the network
                        // data context
                        setGnssDeviceHandle(devHandle, gnssDeviceHandle);
                        // If specified, set the pins of the intermediate module
                        // that control power to and see Data Ready from the GNSS
                        // chip.  Note: if we put GNSS chips inside non-cellular
                        // modules then this will need to be extended
                        if ((deviceType == U_DEVICE_TYPE_CELL) &&
                            !uCellLocGnssInsideCell(devHandle)) {
                            if (pCfg->devicePinPwr >= 0) {
                                uGnssSetAtPinPwr(devHandle,
                                                 pCfg->devicePinPwr);
                                // Do it for the Cell Locate API as well in case the
                                // user wants to use that
                                uCellLocSetPinGnssPwr(devHandle,
                                                      pCfg->devicePinPwr);
                            }
                            if (pCfg->devicePinDataReady >= 0) {
                                uGnssSetAtPinDataReady(devHandle,
                                                       pCfg->devicePinDataReady);
                                // Do it for the Cell Locate API as well in case the
                                // user wants to use that
                                uCellLocSetPinGnssDataReady(devHandle,
                                                            pCfg->devicePinDataReady);
                            }
                        }
#if !U_CFG_OS_CLIB_LEAKS
                        // Set printing of commands sent to the GNSS chip,
                        // which can be useful while debugging, but
                        // only if the C library doesn't leak.
                        uGnssSetUbxMessagePrint(devHandle, true);
#endif
                        // Power on the GNSS "device".
                        errorCode = uGnssPwrOn(devHandle);
                        if (errorCode != 0) {
                            // Clean up on error
                            uGnssRemove(devHandle);
                            setGnssDeviceHandle(devHandle, NULL);
                        }
                    }
                } else {
                    if (devHandle != NULL) {
                        // Power off the GNSS "device"
                        errorCode = uGnssPwrOff(devHandle);
                        uGnssRemove(devHandle);
                        setGnssDeviceHandle(devHandle, NULL);
                    }
                }
            }
        }
    }

    return errorCode;
}

// End of file
