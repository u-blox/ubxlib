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

/** @file
 * @brief Functions associated with a GNSS device.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // malloc()/free()
#include "string.h"    // for memset()

#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_port_uart.h"

#include "u_cell_module_type.h"
#include "u_cell.h"                 // For uCellAtClientHandleGet()
#include "u_cell_loc.h"             // For uCellLocSetPinGnssPwr()/uCellLocSetPinGnssDataReady()

#include "u_short_range_module_type.h"
#include "u_short_range.h"          // For uShortRangeAtClientHandleGet()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_shared_gnss.h"
#include "u_device_private_gnss.h"

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

// Do all the leg-work to remove a GNSS device.
static int32_t removeDevice(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceGnssInstance_t *pContext = (uDeviceGnssInstance_t *) U_DEVICE_INSTANCE(devHandle)->pContext;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (pContext->transportType != (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
            // If the GNSS chip is connected directly, power it off.
            errorCode = uGnssPwrOff(devHandle);
        }
        if (errorCode == 0) {
            // This will destroy the instance
            uGnssRemove(devHandle);
            switch (pContext->transportType) {
                case U_GNSS_TRANSPORT_UBX_UART:
                //lint -fallthrough
                case U_GNSS_TRANSPORT_NMEA_UART:
                    if (pContext->transportHandle.uart >= 0) {
                        uPortUartClose(pContext->transportHandle.uart);
                    }
                    break;
                case U_GNSS_TRANSPORT_UBX_AT:
                    // Nothing to do
                    break;
                default:
                    break;
            }
            free(pContext);
        }
    }

    return errorCode;
}

// Do all the leg-work to add a GNSS device.
static int32_t addDevice(const uDeviceCfgUart_t *pCfgUart,
                         const uDeviceCfgGnss_t *pCfgGnss,
                         uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uDeviceGnssInstance_t *pContext;
    int32_t atDevType = uDeviceGetDeviceType(pCfgGnss->devHandleAt);

    pContext = (uDeviceGnssInstance_t *) malloc(sizeof(uDeviceGnssInstance_t));
    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext->transportHandle.uart = -1;
        switch (pCfgGnss->transportType) {
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
                // Open a UART with the recommended buffer length
                // and default baud rate.
                errorCode = uPortUartOpen(pCfgUart->uart,
                                          pCfgUart->baudRate, NULL,
                                          U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                          pCfgUart->pinTxd,
                                          pCfgUart->pinRxd,
                                          pCfgUart->pinCts,
                                          pCfgUart->pinRts);
                if (errorCode >= 0) {
                    pContext->transportHandle.uart = errorCode;
                }
                break;
            case U_GNSS_TRANSPORT_UBX_AT:
                // Get the AT devHandle from the network instance we are
                // attached through
                if (atDevType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                    errorCode = uShortRangeAtClientHandleGet(pCfgGnss->devHandleAt,
                                                             //lint -e(181) Suppress unusual pointer cast
                                                             (uAtClientHandle_t *) &(pContext->transportHandle.pAt)); // *NOPAD*
                } else if (atDevType == (int32_t) U_DEVICE_TYPE_CELL) {
                    errorCode = uCellAtClientHandleGet(pCfgGnss->devHandleAt,
                                                       //lint -e(181) Suppress unusual pointer cast
                                                       (uAtClientHandle_t *) &(pContext->transportHandle.pAt)); // *NOPAD*
                }
                break;
            default:
                break;
        }

        if (errorCode >= 0) {
            pContext->transportType = (uGnssTransportType_t) pCfgGnss->transportType;
            // Add the GNSS instance, which actually creates pDeviceHandle
            errorCode = uGnssAdd((uGnssModuleType_t) pCfgGnss->moduleType,
                                 (uGnssTransportType_t) pContext->transportType,
                                 pContext->transportHandle,
                                 pCfgGnss->pinGnssEnablePower, false,
                                 pDeviceHandle);
            if (errorCode == 0) {
                if (pContext->transportType == (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                    // If specified, and if the GNSS chip is not inside the
                    // intervening module, set the pins of the AT module that
                    // control power to and see Data Ready from the GNSS chip
                    // Note: if we put GNSS chips inside non-cellular modules
                    // then this will need to be extended
                    if ((atDevType == (int32_t) U_DEVICE_TYPE_CELL) &&
                        !uCellLocGnssInsideCell(pCfgGnss->devHandleAt)) {
                        if (pCfgGnss->gnssAtPinPwr >= 0) {
                            uGnssSetAtPinPwr(*pDeviceHandle,
                                             pCfgGnss->gnssAtPinPwr);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssPwr(pCfgGnss->devHandleAt,
                                                  pCfgGnss->gnssAtPinPwr);
                        }
                        if (pCfgGnss->gnssAtPinDataReady >= 0) {
                            uGnssSetAtPinDataReady(*pDeviceHandle,
                                                   pCfgGnss->gnssAtPinDataReady);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssDataReady(pCfgGnss->devHandleAt,
                                                        pCfgGnss->gnssAtPinDataReady);
                        }
                    }
                }
#if !U_CFG_OS_CLIB_LEAKS
                // Set printing of commands sent to the GNSS chip,
                // which can be useful while debugging, but
                // only if the C library doesn't leak.
                uGnssSetUbxMessagePrint(*pDeviceHandle, true);
#endif
                // Attach the context
                U_DEVICE_INSTANCE(*pDeviceHandle)->pContext = pContext;
                if (pContext->transportType != (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                    // Power on the GNSS chip but only if it's not used
                    // over an AT interface: if it is used over an AT interface
                    // then we leave the GNSS chip powered off so that Cell Locate
                    // can use it.  If the GNSS chip is to be used directly then
                    // uNetworkInterfaceUpGnss() will power it up and "claim" it.
                    errorCode = uGnssPwrOn(*pDeviceHandle);
                    if (errorCode != 0) {
                        // If we failed to power on, clean up
                        removeDevice(*pDeviceHandle);
                    }
                }
            } else {
                if (pContext->transportHandle.uart >= 0) {
                    // If we failed to add, close the UART again
                    uPortUartClose(pContext->transportHandle.uart);
                }
                free(pContext);
            }
        } else {
            // Failed to setup transport, clear context
            free(pContext);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise GNSS.
int32_t uDevicePrivateGnssInit()
{
    return uGnssInit();
}

// Deinitialise GNSS.
void uDevicePrivateGnssDeinit()
{
    uGnssDeinit();
}

// Power up a GNSS device, making it available for configuration.
int32_t uDevicePrivateGnssAdd(const uDeviceCfg_t *pDevCfg,
                              uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uDeviceCfgUart_t *pCfgUart;
    const uDeviceCfgGnss_t *pCfgGnss;

    if ((pDevCfg != NULL) && (pDeviceHandle != NULL)) {
        pCfgUart = &(pDevCfg->transportCfg.cfgUart);
        pCfgGnss = &(pDevCfg->deviceCfg.cfgGnss);
        // Either a GNSS chip that is inside another [e.g. cellular]
        // module is being accessed over an AT interface, or we must
        // (currently) have a UART transport talking to the
        // directly-connected GNSS chip
        if ((pCfgGnss->version == 0) &&
            ((pCfgGnss->transportType == U_GNSS_TRANSPORT_UBX_AT) ||
             (pDevCfg->transportType == U_DEVICE_TRANSPORT_TYPE_UART))) {
            errorCode = addDevice(pCfgUart, pCfgGnss, pDeviceHandle);
        }
    }

    return errorCode;
}

// Remove a GNSS device, powering it down.
int32_t uDevicePrivateGnssRemove(uDeviceHandle_t devHandle)
{
    return removeDevice(devHandle);
}

// End of file
