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

/** @file
 * @brief Functions associated with a cellular device.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // for memset()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_uart.h"

#include "u_device.h"
#include "u_device_shared.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_pwr.h"

#include "u_device_shared_cell.h"
#include "u_device_private_cell.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_DEVICE_PRIVATE_CELL_POWER_ON_GUARD_TIME_SECONDS
/** How long the cellular module is allowed to power-on.
 */
# define U_DEVICE_PRIVATE_CELL_POWER_ON_GUARD_TIME_SECONDS 60
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Call-back for power-up timeout.
static bool keepGoingCallback(uDeviceHandle_t devHandle)
{
    uDeviceCellContext_t *pContext;
    uDeviceInstance_t *pInstance = NULL;
    bool keepGoing = false;

    if (uDeviceGetInstance(devHandle, &pInstance) == 0) {
        pContext = (uDeviceCellContext_t *) pInstance->pContext;
        if ((pContext == NULL) ||
            (uPortGetTickTimeMs() < pContext->stopTimeMs)) {
            keepGoing = true;
        }
    }

    return keepGoing;
}

// Do all the leg-work to remove a cellular device.
static int32_t removeDevice(uDeviceHandle_t devHandle, bool powerOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceCellContext_t *pContext = (uDeviceCellContext_t *) U_DEVICE_INSTANCE(devHandle)->pContext;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (powerOff) {
            if (pContext->pinPwrOn >= 0) {
                // Power off only if we have a pin that will let us power on again
                errorCode = uCellPwrOff(devHandle, NULL);
                if (errorCode != 0) {
                    // If that didn't do it, try the hard way
                    errorCode = uCellPwrOffHard(devHandle, false, NULL);
                    if (errorCode != 0) {
                        // If that didn't do it, try the truly hard way
                        errorCode = uCellPwrOffHard(devHandle, true, NULL);
                    }
                }
            }
        }
        if (errorCode == 0) {
            // This will destroy the instance
            uCellRemove(devHandle);
            uAtClientRemove(pContext->at);
            uPortUartClose(pContext->uart);
            uPortFree(pContext);
        }
    }

    return errorCode;
}

// Do all the leg-work to add a cellular device.
static int32_t addDevice(const uDeviceCfgUart_t *pCfgUart,
                         const uDeviceCfgCell_t *pCfgCell,
                         uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uDeviceCellContext_t *pContext;

    pContext = (uDeviceCellContext_t *) pUPortMalloc(sizeof(uDeviceCellContext_t));
    if (pContext != NULL) {
        // Open a UART with the recommended buffer length
        // and default baud rate.
        errorCode = uPortUartOpen(pCfgUart->uart,
                                  pCfgUart->baudRate, NULL,
                                  U_CELL_UART_BUFFER_LENGTH_BYTES,
                                  pCfgUart->pinTxd,
                                  pCfgUart->pinRxd,
                                  pCfgUart->pinCts,
                                  pCfgUart->pinRts);
        if (errorCode >= 0) {
            pContext->uart = errorCode;
            // Add an AT client on the UART with the recommended
            // default buffer size.
            errorCode = (int32_t) U_CELL_ERROR_AT;
            pContext->at = uAtClientAdd(pContext->uart,
                                        U_AT_CLIENT_STREAM_TYPE_UART,
                                        NULL,
                                        U_CELL_AT_BUFFER_LENGTH_BYTES);
            if (pContext->at != NULL) {
                // Set printing of AT commands by the cellular driver,
                // which can be useful while debugging.
                uAtClientPrintAtSet(pContext->at, true);

                // Add a cellular instance, which actually
                // creates the device instance for us in pDeviceHandle
                errorCode = uCellAdd((uCellModuleType_t) pCfgCell->moduleType,
                                     pContext->at,
                                     pCfgCell->pinEnablePower,
                                     pCfgCell->pinPwrOn,
                                     pCfgCell->pinVInt, false,
                                     pDeviceHandle);
                if (errorCode == 0) {
                    // Set the timeout
                    pContext->stopTimeMs = uPortGetTickTimeMs() +
                                           (U_DEVICE_PRIVATE_CELL_POWER_ON_GUARD_TIME_SECONDS * 1000);
                    // Remember the PWR_ON pin 'cos we need it during power down
                    pContext->pinPwrOn = pCfgCell->pinPwrOn;
                    // Hook our context data off the device handle
                    U_DEVICE_INSTANCE(*pDeviceHandle)->pContext = (void *) pContext;
                    if (pCfgCell->pinDtrPowerSaving >= 0) {
                        errorCode = uCellPwrSetDtrPowerSavingPin(*pDeviceHandle,
                                                                 pCfgCell->pinDtrPowerSaving);
                    }
                    if (errorCode == 0) {
                        // Power on
                        errorCode = uCellPwrOn(*pDeviceHandle, pCfgCell->pSimPinCode,
                                               keepGoingCallback);
                    }
                    if (errorCode != 0) {
                        // If we failed to power on, clean up
                        removeDevice(*pDeviceHandle, false);
                    }
                } else {
                    // Failed to add cellular, clean up
                    uAtClientRemove(pContext->at);
                    uPortUartClose(pContext->uart);
                    uPortFree(pContext);
                }
            } else {
                // Failed to add AT client, clean up
                uPortUartClose(pContext->uart);
                uPortFree(pContext);
            }
        } else {
            // Failed to add UART, clean up
            uPortFree(pContext);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise cellular.
int32_t uDevicePrivateCellInit()
{
    int32_t errorCode = uAtClientInit();
    if (errorCode == 0) {
        errorCode = uCellInit();
    }

    return errorCode;
}

// Deinitialise cellular.
void uDevicePrivateCellDeinit()
{
    uCellDeinit();
    uAtClientDeinit();
}

// Power up a cellular device, making it available for configuration.
int32_t uDevicePrivateCellAdd(const uDeviceCfg_t *pDevCfg,
                              uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uDeviceCfgUart_t *pCfgUart;
    const uDeviceCfgCell_t *pCfgCell;

    if ((pDevCfg != NULL) &&
        (pDevCfg->transportType == U_DEVICE_TRANSPORT_TYPE_UART) &&
        (pDeviceHandle != NULL)) {
        pCfgUart = &(pDevCfg->transportCfg.cfgUart);
        pCfgCell = &(pDevCfg->deviceCfg.cfgCell);
        if (pCfgCell->version == 0) {
            errorCode = addDevice(pCfgUart, pCfgCell, pDeviceHandle);
        }
    }

    return errorCode;
}

// Remove a cellular device.
int32_t uDevicePrivateCellRemove(uDeviceHandle_t devHandle,
                                 bool powerOff)
{
    return removeDevice(devHandle, powerOff);
}

// End of file
