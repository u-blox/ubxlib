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
 * @brief Stubs to allow the network API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when cellular is not included in the
 * build.
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_private_cell.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uNetworkPrivateChangeStateCell(uDeviceHandle_t devHandle,
                                              const uNetworkCfgCell_t *pCfg,
                                              bool upNotDown)
{
    (void) devHandle;
    (void) pCfg;
    (void) upNotDown;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uNetworkSetStatusCallbackCell(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellLocSetPinGnssPwr(uDeviceHandle_t cellHandle, int32_t pin)
{
    (void) cellHandle;
    (void) pin;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellLocSetPinGnssDataReady(uDeviceHandle_t cellHandle, int32_t pin)
{
    (void) cellHandle;
    (void) pin;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellLocGnssInsideCell(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellPwrGetDtrPowerSavingPin(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellPwrDisableUartSleep(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellPwrEnableUartSleep(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellPwrUartSleepIsEnabled(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellMuxEnable(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellMuxIsEnabled(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellMuxAddChannel(uDeviceHandle_t cellHandle,
                                  int32_t channel,
                                  uDeviceSerial_t **ppDeviceSerial)
{
    (void) cellHandle;
    (void) channel;
    (void) ppDeviceSerial;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK uDeviceSerial_t *pUCellMuxChannelGetDeviceSerial(uDeviceHandle_t cellHandle,
                                                        int32_t channel)
{
    (void) cellHandle;
    (void) channel;
    return NULL;
}

U_WEAK int32_t uCellMuxRemoveChannel(uDeviceHandle_t cellHandle,
                                     uDeviceSerial_t *pDeviceSerial)
{
    (void) cellHandle;
    (void) pDeviceSerial;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMuxDisable(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uCellMuxFree(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
}

U_WEAK bool uCellPppIsOpen(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellCfgSetGnssProfile(uDeviceHandle_t cellHandle,
                                      int32_t profileBitMap,
                                      const char *pServerName)
{
    (void) cellHandle;
    (void) profileBitMap;
    (void) pServerName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellCfgGetGnssProfile(uDeviceHandle_t cellHandle,
                                      char *pServerName,
                                      size_t sizeBytes)
{
    (void) cellHandle;
    (void) pServerName;
    (void) sizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file

// End of file
