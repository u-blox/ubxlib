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
 * @brief Implementation of the BLE  portion of the
 * network API. The contents of this file aren't any more
 * "private" than the other sources files but the associated header
 * file should be private and this is simply named to match.
 */

#ifdef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_cfg.h"

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_private_ble.h"

#include "u_port_debug.h"
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

// Initialise the network API for BLE.
int32_t uNetworkInitBle()
{
    uBleInit();

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the sho network API.
void uNetworkDeinitBle()
{
    uBleDeinit();
}

// Add a BLE network instance.
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration)
{
    return uBleAdd((uBleModuleType_t)pConfiguration->module, NULL);
}

// Remove a BLE network instance.
int32_t uNetworkRemoveBle(int32_t handle)
{
    if (handle != 0) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

// Bring up the given BLE network instance.
int32_t uNetworkUpBle(int32_t handle,
                      const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode;
    uBleCfg_t cfg;

    if (handle != 0) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    cfg.role = (uBleCfgRole_t) pConfiguration->role;
    cfg.spsServer = pConfiguration->spsServer;
    uPortLog("call uBleCfgConfigure\n");
    errorCode = uBleCfgConfigure(0, &cfg);
    if (errorCode >= 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }
    uPortLog("uNetworkUpBle complete\n");

    return errorCode;
}

// Take down the given BLE network instance.
int32_t uNetworkDownBle(int32_t handle,
                        const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode;
    uBleCfg_t cfg;
    (void)pConfiguration;

    if (handle != 0) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    cfg.role = U_BLE_CFG_ROLE_DISABLED;
    cfg.spsServer = false;
    errorCode = uBleCfgConfigure(0, &cfg);
    if (errorCode >= 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

#endif
// End of file
