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

#include "u_short_range_module_type.h"
#include "u_short_range.h"          // For uShortRangeAtClientHandleGet()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"

#include "u_network.h"
#include "u_network_config_gnss.h"
#include "u_network_private_gnss.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_shared_gnss.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// TODO: WILL BE REMOVED
#ifndef U_NETWORK_PRIVATE_GNSS_MAX_NUM
/** The maximum number of instances of GNSS that can be
 * active at any one time.
 */
# define U_NETWORK_PRIVATE_GNSS_MAX_NUM 3
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// TODO: I guess this is replaced by a uDevicePrivateGnssInstance_t
// structure (with reduced contents) in u_device_private_gnss.h, as
// the structure will be hung off the uDevice structure?
/** The things we need to remember per instance.
 */
typedef struct {
    uGnssTransportType_t transportType;
    uGnssTransportHandle_t transportHandle;
    uDeviceHandle_t devHandle;
} uNetworkPrivateGnssInstance_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// TODO: WILL BE REMOVED
/** Array to keep track of the instances.
 */
static uNetworkPrivateGnssInstance_t gInstance[U_NETWORK_PRIVATE_GNSS_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// TODO: WILL BE REMOVED
// Find a free place in the list.
static uNetworkPrivateGnssInstance_t *pGetFree()
{
    uNetworkPrivateGnssInstance_t *pFree = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pFree == NULL); x++) {
        if ((gInstance[x].devHandle == NULL) &&
            (gInstance[x].transportType == U_GNSS_TRANSPORT_NONE)) {
            pFree = &(gInstance[x]);
        }
    }

    return pFree;
}

// TODO: WILL BE REMOVED
// Find the given instance in the list.
static uNetworkPrivateGnssInstance_t *pGetInstance(uDeviceHandle_t devHandle)
{
    uNetworkPrivateGnssInstance_t *pInstance = NULL;

    // Find the devHandle in the list
    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pInstance == NULL); x++) {
        if (gInstance[x].devHandle == devHandle) {
            pInstance = &(gInstance[x]);
        }
    }

    return pInstance;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// TODO: WILL BE REMOVED.
int32_t uNetworkInitGnss(void)
{
    uGnssInit();

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        gInstance[x].transportType = U_GNSS_TRANSPORT_NONE;
        gInstance[x].devHandle = NULL;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// TODO: WILL BE REMOVED.
void uNetworkDeinitGnss(void)
{
    uGnssDeinit();
}

// TODO: WILL BE REMOVED: functionality will be in a static function inside
// u_device_private_gnss.c.
int32_t uNetworkAddGnss(const uNetworkConfigurationGnss_t *pConfiguration,
                        uDeviceHandle_t *pDevHandle)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateGnssInstance_t *pInstance;
    int32_t x;
    int32_t atDevType = uDeviceGetDeviceType(pConfiguration->devHandleAt);

    pInstance = pGetFree();
    if (pInstance != NULL) {
        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance->transportHandle.uart = -1;
        switch (pConfiguration->transportType) {
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
                // Open a UART with the recommended buffer length
                // and default baud rate.
                errorCodeOrHandle = uPortUartOpen(pConfiguration->uart,
                                                  U_GNSS_UART_BAUD_RATE, NULL,
                                                  U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                                  pConfiguration->pinTxd,
                                                  pConfiguration->pinRxd,
                                                  pConfiguration->pinCts,
                                                  pConfiguration->pinRts);
                if (errorCodeOrHandle >= 0) {
                    pInstance->transportHandle.uart = errorCodeOrHandle;
                }
                break;
            case U_GNSS_TRANSPORT_UBX_AT:
                // Get the AT devHandle from the network instance we are
                // attached through
                if (atDevType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                    errorCodeOrHandle = uShortRangeAtClientHandleGet(pConfiguration->devHandleAt,
                                                                     //lint -e(181) Suppress unusual pointer cast
                                                                     (uAtClientHandle_t *) & (pInstance->transportHandle.pAt));
                } else if (atDevType == (int32_t) U_DEVICE_TYPE_CELL) {
                    errorCodeOrHandle = uCellAtClientHandleGet(pConfiguration->devHandleAt,
                                                               //lint -e(181) Suppress unusual pointer cast
                                                               (uAtClientHandle_t *) & (pInstance->transportHandle.pAt));
                }
                break;
            default:
                break;
        }

        if (errorCodeOrHandle >= 0) {
            pInstance->transportType = (uGnssTransportType_t) pConfiguration->transportType;
            // Add a GNSS instance
            errorCodeOrHandle = uGnssAdd((uGnssModuleType_t) pConfiguration->moduleType,
                                         (uGnssTransportType_t) pInstance->transportType,
                                         pInstance->transportHandle,
                                         pConfiguration->pinGnssEnablePower, false,
                                         pDevHandle);
            if (errorCodeOrHandle >= 0) {
                pInstance->devHandle = *pDevHandle;
                if (pConfiguration->transportType == (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                    // If specified, and if the GNSS chip is not inside the
                    // intervening module, set the pins of the AT module that
                    // control power to and see Data Ready from the GNSS chip
                    // Note: if we put GNSS chips inside non-cellular modules
                    // then this will need to be extended
                    if ((atDevType == (int32_t) U_DEVICE_TYPE_CELL) &&
                        !uCellLocGnssInsideCell(pConfiguration->devHandleAt)) {
                        if (pConfiguration->gnssAtPinPwr >= 0) {
                            uGnssSetAtPinPwr(pInstance->devHandle,
                                             pConfiguration->gnssAtPinPwr);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssPwr(pConfiguration->devHandleAt,
                                                  pConfiguration->gnssAtPinPwr);
                        }
                        if (pConfiguration->gnssAtPinDataReady >= 0) {
                            uGnssSetAtPinDataReady(pInstance->devHandle,
                                                   pConfiguration->gnssAtPinDataReady);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssDataReady(pConfiguration->devHandleAt,
                                                        pConfiguration->gnssAtPinDataReady);
                        }
                    }

                }
#if !U_CFG_OS_CLIB_LEAKS
                // Set printing of commands sent to the GNSS chip,
                // which can be useful while debugging, but
                // only if the C library doesn't leak.
                uGnssSetUbxMessagePrint(pInstance->devHandle, true);
#endif
                if (pConfiguration->transportType != (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                    // Power on the GNSS chip but only if it's not used
                    // over an AT interface: if it is used over an AT interface
                    // then we leave the GNSS chip powered off so that Cell Locate
                    // can use it.  If the GNSS chip is to be used directly then
                    // uNetworkUpGnss() will power it up and "claim" it.
                    x = uGnssPwrOn(pInstance->devHandle);
                    if (x != 0) {
                        // If we failed to power on, clean up
                        uNetworkRemoveGnss(pInstance->devHandle);
                        errorCodeOrHandle = x;
                    }
                }
            } else {
                if (pInstance->transportHandle.uart >= 0) {
                    // If we failed to add, close the UART again
                    uPortUartClose(pInstance->transportHandle.uart);
                }
            }
        }
    }

    return errorCodeOrHandle;
}

// TODO: WILL BE REMOVED: functionality will be in a static function
// inside u_device_private_gnss.c.
int32_t uNetworkRemoveGnss(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateGnssInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(devHandle);
    if (pInstance != NULL) {
        uGnssRemove(pInstance->devHandle);
        pInstance->devHandle = NULL;
        switch (pInstance->transportType) {
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
                if (pInstance->transportHandle.uart >= 0) {
                    uPortUartClose(pInstance->transportHandle.uart);
                }
                break;
            case U_GNSS_TRANSPORT_UBX_AT:
                // Nothing to do
                break;
            default:
                break;
        }
        pInstance->transportType = U_GNSS_TRANSPORT_NONE;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality is in the "up" part
// of uNetworkChangeStateGnss().
int32_t uNetworkUpGnss(uDeviceHandle_t devHandle,
                       const uNetworkConfigurationGnss_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateGnssInstance_t *pInstance;

    (void) pConfiguration;

    // Find the instance in the list
    pInstance = pGetInstance(devHandle);
    if (pInstance != NULL) {
        // Power on, in case we weren't on before
        errorCode = uGnssPwrOn(devHandle);
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality is in the "up" part
// of uNetworkChangeStateGnss().
int32_t uNetworkDownGnss(uDeviceHandle_t devHandle,
                         const uNetworkConfigurationGnss_t *pConfiguration)
{
    (void) pConfiguration;
    // Power off
    return uGnssPwrOff(devHandle);
}

// Bring a GNSS interface up or take it down.
int32_t uNetworkPrivateChangeStateGnss(uDeviceHandle_t devHandle,
                                       uNetworkCfgGnss_t *pCfg,
                                       bool upNotDown)
{
    uDeviceGnssInstance_t *pContext;
    uDeviceInstance_t *pDevInstance;
    int32_t errorCode = uDeviceGetInstance(devHandle, &pDevInstance);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (uDeviceGnssInstance_t *) pDevInstance->pContext;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_GNSS) && (pContext != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            // If the GNSS chip was directly connected to this MCU
            // then it would have been powered up by the device API.
            // If it is used over an AT interface then it will have
            // been left powered off so that Cell Locate could use it,
            // hence we need to manage the power here.
            if (pContext->transportType == (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                if (upNotDown) {
                    errorCode = uGnssPwrOn(devHandle);
                } else {
                    errorCode = uGnssPwrOff(devHandle);
                }
            }
        }
    }

    return errorCode;
}

// End of file
