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
 * @brief Stub of the cellular portion of the network API.
 * Include this if the cellular network is not used in the application.
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_private_cell.h"

int32_t uNetworkInitCell(void)
{
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

void uNetworkDeinitCell(void)
{

}

int32_t uNetworkAddCell(const uNetworkConfigurationCell_t *pConfiguration)
{
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uNetworkRemoveCell(int32_t handle)
{
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uNetworkUpCell(int32_t handle,
                       const uNetworkConfigurationCell_t *pConfiguration)
{
    (void) handle;
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uNetworkDownCell(int32_t handle,
                         const uNetworkConfigurationCell_t *pConfiguration)
{
    (void) handle;
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// End of file
