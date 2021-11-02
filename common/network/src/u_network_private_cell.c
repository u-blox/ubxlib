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
 * @brief Implementation of the cellular portion of the network API. The
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

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_pwr.h"

#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_private_cell.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_CELL_MAX_NUM
/** The maximum number of instances of cellular that can be
 * active at any one time.
 */
# define U_NETWORK_PRIVATE_CELL_MAX_NUM 3
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The things we need to remember per instance.
 */
typedef struct {
    int32_t uart;
    uAtClientHandle_t at;
    int32_t cell;
    int64_t stopTimeMs;
} uNetworkPrivateCellInstance_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to keep track of the instances.
 */
static uNetworkPrivateCellInstance_t gInstance[U_NETWORK_PRIVATE_CELL_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a free place in the list.
static uNetworkPrivateCellInstance_t *pGetFree()
{
    uNetworkPrivateCellInstance_t *pFree = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pFree == NULL); x++) {
        if ((gInstance[x].uart < 0) && (gInstance[x].at == NULL) &&
            (gInstance[x].cell < 0)) {
            pFree = &(gInstance[x]);
        }
    }

    return pFree;
}

// Find the given instance in the list.
static uNetworkPrivateCellInstance_t *pGetInstance(int32_t cellHandle)
{
    uNetworkPrivateCellInstance_t *pInstance = NULL;

    // Find the handle in the list
    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pInstance == NULL); x++) {
        if (gInstance[x].cell == cellHandle) {
            pInstance = &(gInstance[x]);
        }
    }

    return pInstance;
}

// Call-back for connection timeout.
static bool keepGoingCallback(int32_t cellHandle)
{
    uNetworkPrivateCellInstance_t *pInstance;
    bool keepGoing = false;

    pInstance = pGetInstance(cellHandle);
    if ((pInstance != NULL) && (uPortGetTickTimeMs() < pInstance->stopTimeMs)) {
        keepGoing = true;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for cellular.
int32_t uNetworkInitCell(void)
{
    uAtClientInit();
    uCellInit();

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        gInstance[x].uart = -1;
        gInstance[x].at = NULL;
        gInstance[x].cell = -1;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the cellular network API.
void uNetworkDeinitCell(void)
{
    uAtClientDeinit();
    uCellDeinit();
}

// Add a cellular network instance.
int32_t uNetworkAddCell(const uNetworkConfigurationCell_t *pConfiguration)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateCellInstance_t *pInstance;
    int32_t x;

    pInstance = pGetFree();
    if (pInstance != NULL) {
        // Open a UART with the recommended buffer length
        // and default baud rate.
        errorCodeOrHandle = uPortUartOpen(pConfiguration->uart,
                                          U_CELL_UART_BAUD_RATE, NULL,
                                          U_CELL_UART_BUFFER_LENGTH_BYTES,
                                          pConfiguration->pinTxd,
                                          pConfiguration->pinRxd,
                                          pConfiguration->pinCts,
                                          pConfiguration->pinRts);
        if (errorCodeOrHandle >= 0) {
            pInstance->uart = errorCodeOrHandle;

            // Add an AT client on the UART with the recommended
            // default buffer size.
            errorCodeOrHandle = (int32_t) U_CELL_ERROR_AT;
            pInstance->at = uAtClientAdd(pInstance->uart,
                                         U_AT_CLIENT_STREAM_TYPE_UART,
                                         NULL,
                                         U_CELL_AT_BUFFER_LENGTH_BYTES);
            if (pInstance->at != NULL) {
                // Set printing of AT commands by the cellular driver,
                // which can be useful while debugging.
                uAtClientPrintAtSet(pInstance->at, true);

                // Add a cell instance
                errorCodeOrHandle = uCellAdd((uCellModuleType_t) pConfiguration->moduleType,
                                             pInstance->at,
                                             pConfiguration->pinEnablePower,
                                             pConfiguration->pinPwrOn,
                                             pConfiguration->pinVInt, false);
                if (errorCodeOrHandle >= 0) {
                    pInstance->cell = errorCodeOrHandle;
                    // Set the timeout
                    pInstance->stopTimeMs = uPortGetTickTimeMs() +
                                            (((int64_t) pConfiguration->timeoutSeconds) * 1000);
                    // Power on
                    x = uCellPwrOn(errorCodeOrHandle, pConfiguration->pPin,
                                   keepGoingCallback);
                    if (x != 0) {
                        // If we failed to power on, clean up
                        uNetworkRemoveCell(errorCodeOrHandle);
                        errorCodeOrHandle = x;
                    }
                }
            }
        }
    }

    return errorCodeOrHandle;
}

// Remove a cellular network instance.
int32_t uNetworkRemoveCell(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateCellInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        uCellRemove(pInstance->cell);
        pInstance->cell = -1;
        uAtClientRemove(pInstance->at);
        pInstance->at = NULL;
        uPortUartClose(pInstance->uart);
        pInstance->uart = -1;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Bring up the given cellular network instance.
int32_t uNetworkUpCell(int32_t handle,
                       const uNetworkConfigurationCell_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateCellInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        // Set the timeout
        pInstance->stopTimeMs = uPortGetTickTimeMs() +
                                (((int64_t) pConfiguration->timeoutSeconds) * 1000);
        // Power on, in case we weren't on before
        errorCode = uCellPwrOn(handle, pConfiguration->pPin,
                               keepGoingCallback);
        if (errorCode == 0) {
            // Connect using automatic selection,
            // default no user name or password for the APN
            errorCode = uCellNetConnect(handle, NULL,
                                        pConfiguration->pApn,
                                        NULL, NULL,
                                        keepGoingCallback);
        }
    }

    return errorCode;
}

// Take down the given cellular network instance.
int32_t uNetworkDownCell(int32_t handle,
                         const uNetworkConfigurationCell_t *pConfiguration)
{
    int32_t errorCode;

    (void) pConfiguration;

    // Disonnect with default timeout, ignoring
    // error code as we're going to power off anyway
    uCellNetDisconnect(handle, NULL);
    // Power off with default timeout
    errorCode = uCellPwrOff(handle, NULL);
    if (errorCode != 0) {
        // If that didn't do it, try the hard way
        errorCode = uCellPwrOffHard(handle, false, NULL);
        if (errorCode != 0) {
            // If that didn't do it, try the truly hard way
            errorCode = uCellPwrOffHard(handle, true, NULL);
        }
    }

    return errorCode;
}

// End of file
