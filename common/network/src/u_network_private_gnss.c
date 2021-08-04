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

#include "u_cfg_os_platform_specific.h"

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

#include "u_network_handle.h"
#include "u_network.h"
#include "u_network_config_gnss.h"
#include "u_network_private_gnss.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_GNSS_MAX_NUM
/** The maximum number of instances of GNSS that can be
 * active at any one time.
 */
# define U_NETWORK_PRIVATE_GNSS_MAX_NUM 3
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The things we need to remember per instance.
 */
typedef struct {
    uGnssTransportType_t transportType;
    uGnssTransportHandle_t transportHandle;
    int32_t gnss;
} uNetworkPrivateGnssInstance_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to keep track of the instances.
 */
static uNetworkPrivateGnssInstance_t gInstance[U_NETWORK_PRIVATE_GNSS_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a free place in the list.
static uNetworkPrivateGnssInstance_t *pGetFree()
{
    uNetworkPrivateGnssInstance_t *pFree = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pFree == NULL); x++) {
        if ((gInstance[x].gnss < 0) &&
            (gInstance[x].transportType == U_GNSS_TRANSPORT_NONE)) {
            pFree = &(gInstance[x]);
        }
    }

    return pFree;
}

// Find the given instance in the list.
static uNetworkPrivateGnssInstance_t *pGetInstance(int32_t gnssHandle)
{
    uNetworkPrivateGnssInstance_t *pInstance = NULL;

    // Find the handle in the list
    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pInstance == NULL); x++) {
        if (gInstance[x].gnss == gnssHandle) {
            pInstance = &(gInstance[x]);
        }
    }

    return pInstance;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for GNSS.
int32_t uNetworkInitGnss()
{
    uGnssInit();

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        gInstance[x].transportType = U_GNSS_TRANSPORT_NONE;
        gInstance[x].gnss = -1;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the GNSS network API.
void uNetworkDeinitGnss()
{
    uGnssDeinit();
}

// Add a GNSS network instance.
int32_t uNetworkAddGnss(const uNetworkConfigurationGnss_t *pConfiguration)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateGnssInstance_t *pInstance;
    int32_t x;

    pInstance = pGetFree();
    if (pInstance != NULL) {
        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
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
                // Get the AT handle from the network instance we are
                // attached through
                if (U_NETWORK_HANDLE_IS_BLE(pConfiguration->networkHandleAt) ||
                    U_NETWORK_HANDLE_IS_WIFI(pConfiguration->networkHandleAt)) {
                    errorCodeOrHandle = uShortRangeAtClientHandleGet(pConfiguration->networkHandleAt,
                                                                     //lint -e(181) Suppress unusual pointer cast
                                                                     (uAtClientHandle_t *) & (pInstance->transportHandle.pAt));
                } else if (U_NETWORK_HANDLE_IS_CELL(pConfiguration->networkHandleAt)) {
                    errorCodeOrHandle = uCellAtClientHandleGet(pConfiguration->networkHandleAt,
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
                                         pConfiguration->pinGnssEnablePower, false);
            if (errorCodeOrHandle >= 0) {
                pInstance->gnss = errorCodeOrHandle;
                if (pConfiguration->transportType == (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                    // If specified, and if the GNSS chip is not inside the
                    // intervening module, set the pins of the AT module that
                    // control power to and see Data Ready from the GNSS chip
                    // Note: if we put GNSS chips inside non-cellular modules
                    // then this will need to be extended
                    if (U_NETWORK_HANDLE_IS_CELL(pConfiguration->networkHandleAt) &&
                        !uCellLocGnssInsideCell(pConfiguration->networkHandleAt)) {
                        if (pConfiguration->gnssAtPinPwr >= 0) {
                            uGnssSetAtPinPwr(pInstance->gnss,
                                             pConfiguration->gnssAtPinPwr);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssPwr(pConfiguration->networkHandleAt,
                                                  pConfiguration->gnssAtPinPwr);
                        }
                        if (pConfiguration->gnssAtPinDataReady >= 0) {
                            uGnssSetAtPinDataReady(pInstance->gnss,
                                                   pConfiguration->gnssAtPinDataReady);
                            // Do it for the Cell Locate API as well in case the
                            // user wants to use that
                            uCellLocSetPinGnssDataReady(pConfiguration->networkHandleAt,
                                                        pConfiguration->gnssAtPinDataReady);
                        }
                    }

                }
#if !U_CFG_OS_CLIB_LEAKS
                // Set printing of commands sent to the GNSS chip,
                // which can be useful while debugging, but
                // only if the C library doesn't leak.
                uGnssSetUbxMessagePrint(pInstance->gnss, true);
#endif
                // Power on the GNSS chip
                x = uGnssPwrOn(errorCodeOrHandle);
                if (x != 0) {
                    // If we failed to power on, clean up
                    uNetworkRemoveGnss(errorCodeOrHandle);
                    errorCodeOrHandle = x;
                }
            }
        }
    }

    return errorCodeOrHandle;
}

// Remove a GNSS network instance.
int32_t uNetworkRemoveGnss(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateGnssInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        uGnssRemove(pInstance->gnss);
        pInstance->gnss = -1;
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

// Bring up the given GNSS network instance.
int32_t uNetworkUpGnss(int32_t handle,
                       const uNetworkConfigurationGnss_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateGnssInstance_t *pInstance;

    (void) pConfiguration;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        // Power on, in case we weren't on before
        errorCode = uGnssPwrOn(handle);
    }

    return errorCode;
}

// Take down the given GNSS network instance.
int32_t uNetworkDownGnss(int32_t handle,
                         const uNetworkConfigurationGnss_t *pConfiguration)
{
    (void) pConfiguration;
    // Power off
    return uGnssPwrOff(handle);
}

// End of file
