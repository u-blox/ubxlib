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
#include "string.h"    // memset()

#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"                 // For uCellAtClientHandleGet()
#include "u_cell_net.h"             // Required by u_cell_cfg.h
#include "u_cell_cfg.h"             // For uCellCfgGetGnssProfile()/uCellCfgSetGnssProfile()
#include "u_cell_loc.h"             // For uCellLocSetPinGnssPwr()/uCellLocSetPinGnssDataReady()
#include "u_cell_mux.h"             // For U_CELL_MUX_CHANNEL_ID_GNSS
#include "u_cell_pwr.h"             // For uCellPwrEnableUartSleep() etc.
#include "u_cell_ppp_shared.h"      // For uCellPppIsOpen()

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

// Set the GNSS network context pointer, used when the underlying
// device is a non-GNSS device.
static void setGnssNetworkContext(uDeviceHandle_t devHandle,
                                  uNetworkPrivateGnssContext_t *pNetworkContext,
                                  uGnssTransportHandle_t gnssTransportHandle)
{
    uDeviceInstance_t *pInstance = U_DEVICE_INSTANCE(devHandle);
    uNetworkPrivateGnssContext_t *pFoundNetworkContext = NULL;
    uDeviceSerial_t *pDeviceSerial;

    for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                        sizeof(pInstance->networkData[0])); x++) {
        if (pInstance->networkData[x].networkType == U_NETWORK_TYPE_GNSS) {
            pFoundNetworkContext = (uNetworkPrivateGnssContext_t *) pInstance->networkData[x].pContext;
            if ((pNetworkContext == NULL) && (pFoundNetworkContext != NULL)) {
                // Disabling a context that we already have, put things
                // back as they were
                if (pFoundNetworkContext->usingCmux) {
                    // Re-enable UART sleep if we had switched it off
                    if (pFoundNetworkContext->cellUartSleepWakeOnDataWasEnabled) {
                        uCellPwrEnableUartSleep(devHandle);
                    }
                    if (!pFoundNetworkContext->cellMuxGnssChannelAlreadyEnabled) {
                        // Remove the multiplexer channel if one was in use
                        // and it was us who started it
                        pDeviceSerial = (uDeviceSerial_t *) (gnssTransportHandle.pDeviceSerial);
                        uCellMuxRemoveChannel(devHandle, pDeviceSerial);
                    }
                    if (!pFoundNetworkContext->cellMuxAlreadyEnabled && !uCellPppIsOpen(devHandle)) {
                        // Disable the multiplexer if one was in use, it was us who
                        // started and PPP isn't using it
                        uCellMuxDisable(devHandle);
                    }
                }
                uPortFree(pFoundNetworkContext);
            }
            pInstance->networkData[x].pContext = pNetworkContext;
            break;
        }
    }
}

// Get the GNSS network context pointer from a device.
static uNetworkPrivateGnssContext_t *pGetGnssNetworkContext(uDeviceHandle_t devHandle)
{
    uNetworkPrivateGnssContext_t *pNetworkContext = NULL;
    uDeviceInstance_t *pInstance = U_DEVICE_INSTANCE(devHandle);

    for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                        sizeof(pInstance->networkData[0])); x++) {
        if (pInstance->networkData[x].networkType == U_NETWORK_TYPE_GNSS) {
            pNetworkContext = (uNetworkPrivateGnssContext_t *) pInstance->networkData[x].pContext;
            break;
        }
    }

    return pNetworkContext;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uNetworkPrivateGnssLink()
{
    //dummy
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
#ifndef U_NETWORK_GNSS_CFG_CELL_USE_AT_ONLY
    uDeviceSerial_t *pDeviceSerial = NULL;
#endif
    uGnssTransportHandle_t gnssTransportHandle = {0};
    uGnssTransportType_t gnssTransportType = U_GNSS_TRANSPORT_NONE;
    uNetworkPrivateGnssContext_t *pNetworkContext = NULL;
    uDeviceHandle_t gnssDeviceHandle = NULL;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_GNSS) &&
            (pDevInstance->pContext != NULL)) {

            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            deviceType = (uDeviceType_t) uDeviceGetDeviceType(devHandle);
            switch (deviceType) {
                case U_DEVICE_TYPE_GNSS:
                    // If the device was a GNSS device it will have been
                    // added and powered up by uDeviceOpen(), there's
                    // nothing else we need to do
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    break;
                case U_DEVICE_TYPE_CELL: {
                    // For the cellular device case we will need to carry
                    // around some context information; try to get the one
                    // that is already there and, if there is none, grab
                    // memory for it now
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pNetworkContext = pGetGnssNetworkContext(devHandle);
                    if (pNetworkContext == NULL) {
                        pNetworkContext = (uNetworkPrivateGnssContext_t *) pUPortMalloc(sizeof(
                                                                                            uNetworkPrivateGnssContext_t));
                        if (pNetworkContext != NULL) {
                            memset(pNetworkContext, 0, sizeof(*pNetworkContext));
                            // Determine and if "wake-up on UART data line" UART power saving is enabled on cellular
                            pNetworkContext->cellMuxAlreadyEnabled = uCellMuxIsEnabled(devHandle);
                            pNetworkContext->cellMuxGnssChannelAlreadyEnabled = (pUCellMuxChannelGetDeviceSerial(devHandle,
                                                                                                                 U_CELL_MUX_CHANNEL_ID_GNSS) != NULL);
                            pNetworkContext->cellUartSleepWakeOnDataWasEnabled = uCellPwrUartSleepIsEnabled(devHandle);
                            if (uCellPwrGetDtrPowerSavingPin(devHandle) >= 0) {
                                pNetworkContext->cellUartSleepWakeOnDataWasEnabled = false;
                            }
                        }
                    }
                    if (pNetworkContext != NULL) {
#ifndef U_NETWORK_GNSS_CFG_CELL_USE_AT_ONLY
                        // Determine if CMUX and the GNSS channel are already enabled,
                        pDeviceSerial = pUCellMuxChannelGetDeviceSerial(devHandle,
                                                                        U_CELL_MUX_CHANNEL_ID_GNSS);
                        if (pDeviceSerial != NULL) {
                            // All good, use what we have with discard on overflow set
                            pDeviceSerial->discardOnOverflow(pDeviceSerial, true);
                            gnssTransportHandle.pDeviceSerial = pDeviceSerial;
                            gnssTransportType = U_GNSS_TRANSPORT_VIRTUAL_SERIAL;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        } else {
                            if (upNotDown) {
                                // If we're going up, try to enable CMUX and the GNSS channel
                                // No need to worry about the flags above as all of these
                                // functions do nothing if the thing is already there
                                errorCode = uCellMuxEnable(devHandle);
                                if (errorCode == 0) {
                                    // It is, good: make sure a GNSS channel is opeend
                                    errorCode = uCellMuxAddChannel(devHandle,
                                                                   U_CELL_MUX_CHANNEL_ID_GNSS,
                                                                   &pDeviceSerial);
                                    if (errorCode == 0) {
                                        // Set discard on overflow so that we aren't
                                        // overwhelmed by a stream of position data
                                        pDeviceSerial->discardOnOverflow(pDeviceSerial, true);
                                        // If we're on wake-up-on-data UART power saving and CMUX, switch
                                        // UART power saving off the GNSS stuff has no concept of waking
                                        // stuff up in that way.
                                        errorCode = uCellPwrDisableUartSleep(devHandle);
                                        if (errorCode == 0) {
                                            pNetworkContext->usingCmux = true;
                                            gnssTransportHandle.pDeviceSerial = pDeviceSerial;
                                            gnssTransportType = U_GNSS_TRANSPORT_VIRTUAL_SERIAL;
                                        }
                                    }
                                }
                                if (errorCode < 0) {
                                    // Tidy up on error
                                    if (!pNetworkContext->cellMuxAlreadyEnabled && !uCellPppIsOpen(devHandle)) {
                                        uCellMuxDisable(devHandle);
                                    }
                                    if ((pDeviceSerial != NULL) &&
                                        !pNetworkContext->cellMuxGnssChannelAlreadyEnabled) {
                                        uCellMuxRemoveChannel(devHandle, pDeviceSerial);
                                    }
                                }
                            }
                        }
#endif
                        if (errorCode < 0) {
                            // Nothing doing with CMUX, get the AT handle so that we
                            // can add and configure GNSS through AT commands
                            errorCode = uCellAtClientHandleGet(devHandle,
                                                               //lint -e(181) Suppress unusual pointer cast
                                                               (uAtClientHandle_t *) &(gnssTransportHandle.pAt)); // *NOPAD*
                            if (errorCode == 0) {
                                gnssTransportType = U_GNSS_TRANSPORT_AT;
                            }
                        }
                    }
                }
                break;
                case U_DEVICE_TYPE_SHORT_RANGE:
                case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                default:
                    break;
            }

            if ((errorCode == 0) && (gnssTransportType != U_GNSS_TRANSPORT_NONE) && (pNetworkContext != NULL)) {
                // We're connected via an intermediate module
                // so we may need to do stuff if we're not
                // already in the requested state
                gnssDeviceHandle = uNetworkGetDeviceHandle(devHandle, U_NETWORK_TYPE_GNSS);
                if (upNotDown) {
                    if (gnssDeviceHandle == NULL) {
                        // No piggy-backed GNSS "device" already
                        // exists so create it
                        errorCode = uGnssAdd((uGnssModuleType_t) pCfg->moduleType,
                                             gnssTransportType,
                                             gnssTransportHandle,
                                             -1, false,
                                             &gnssDeviceHandle);
                    }
                    if (errorCode == 0) {
                        // Hook the GNSS "device" handle off the network
                        // data context
                        pNetworkContext->gnssDeviceHandle = gnssDeviceHandle;
                        setGnssNetworkContext(devHandle, pNetworkContext, gnssTransportHandle);
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
                        // Set printing of commands sent to the GNSS chip,
                        // which can be useful while debugging, but
                        // only if the C library doesn't leak.
                        uGnssSetUbxMessagePrint(devHandle, true);
                        if ((deviceType == U_DEVICE_TYPE_CELL) &&
                            (gnssTransportType == U_GNSS_TRANSPORT_VIRTUAL_SERIAL)) {
                            // Set the intermediate device in GNSS so that it knows
                            // what kind of power on/off to do etc.
                            //NOLINTNEXTLINE(readability-suspicious-call-argument)
                            errorCode = uGnssSetIntermediate(gnssDeviceHandle, devHandle);
                        }

                        if (errorCode == 0) {
                            // Power on the GNSS "device".
                            errorCode = uGnssPwrOn(devHandle);
                        }
                        if (errorCode < 0) {
                            // Clean up on error
                            uGnssRemove(devHandle);
                            setGnssNetworkContext(devHandle, NULL, gnssTransportHandle);
                        }
                    }
                } else {
                    if (devHandle != NULL) {
                        uGnssGetTransportHandle(devHandle,
                                                &gnssTransportType,
                                                &gnssTransportHandle);
                        // Power off the GNSS "device"
                        errorCode = uGnssPwrOff(devHandle);
                        uGnssRemove(devHandle);
                        setGnssNetworkContext(devHandle, NULL, gnssTransportHandle);
                    }
                }
            }
        }
    }

    return errorCode;
}

// End of file
