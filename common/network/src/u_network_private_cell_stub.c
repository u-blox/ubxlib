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

// End of file
