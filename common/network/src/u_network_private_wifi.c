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
 * @brief Implementation of the Wifi portion of the network API.
 * The contents of this file aren't any more "private" than the
 * other sources files but the associated header file should be
 * private and this is simply named to match.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_network.h"
#include "u_network_config_wifi.h"
#include "u_network_private_wifi.h"

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for Wifi.
int32_t uNetworkInitWifi()
{
    // Nothing [yet] to do
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the sho network API.
void uNetworkDeinitWifi()
{
}

// Add a Wifi network instance.
int32_t uNetworkAddWifi(const uNetworkConfigurationWifi_t *pConfiguration)
{
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Remove a Wifi network instance.
int32_t uNetworkRemoveWifi(int32_t handle)
{
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Bring up the given Wifi network instance.
int32_t uNetworkUpWifi(int32_t handle,
                       const uNetworkConfigurationWifi_t *pConfiguration)
{
    (void) handle;
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Take down the given Wifi network instance.
int32_t uNetworkDownWifi(int32_t handle,
                         const uNetworkConfigurationWifi_t *pConfiguration)
{
    (void) handle;
    (void) pConfiguration;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// End of file
