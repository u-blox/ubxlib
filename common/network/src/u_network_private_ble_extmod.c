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
 * @brief Implementation of the BLE  portion of the
 * network API. The contents of this file aren't any more
 * "private" than the other sources files but the associated header
 * file should be private and this is simply named to match.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_cfg.h"

#include "u_network.h"
#include "u_network_private_short_range.h"
#include "u_network_config_ble.h"
#include "u_network_private_ble.h"

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

// TODO since we're changing things, rename this to
//  uNetworkPrivateInitBle() for consistency?
// Initialise the network API for BLE.
int32_t uNetworkInitBle(void)
{
    int32_t errorCode = uNetworkInitShortRange();
    if (errorCode >= 0) {
        errorCode = uBleInit();
    }

    return errorCode;
}

// TODO since we're changing things, rename this to
// uNetworkPrivateDeinitBle() for consistency?
// Deinitialise the sho network API.
void uNetworkDeinitBle(void)
{
    uBleDeinit();
    uNetworkDeinitShortRange();
}

// TODO: WILL BE REMOVED: into uDevicePrivateAddShortRange() in
// u_device_private_short_range.c.
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration,
                       uDeviceHandle_t *pDevHandle)
{
    int32_t errorCode;
    const uShortRangeConfig_t shoConfig = {
        .module = pConfiguration->module,
        .uart = pConfiguration->uart,
        .pinTxd = pConfiguration->pinTxd,
        .pinRxd = pConfiguration->pinRxd,
        .pinCts = pConfiguration->pinCts,
        .pinRts = pConfiguration->pinRts
    };

    // Check that the module supports BLE
    const uShortRangeModuleInfo_t *pModuleInfo;
    pModuleInfo = uShortRangeGetModuleInfo(pConfiguration->module);
    if (!pModuleInfo) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (!pModuleInfo->supportsBle) {
        return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    }

    errorCode = uNetworkAddShortRange(U_NETWORK_TYPE_BLE, &shoConfig, pDevHandle);

    return errorCode;
}

// TODO: WILL BE REMOVED: into uDevicePrivateRemoveShortRange() in
// u_device_private_short_range.c.
int32_t uNetworkRemoveBle(uDeviceHandle_t devHandle)
{
    return uNetworkRemoveShortRange(devHandle);
}

// TODO: WILL BE REMOVED: functionality will be in the "up" part
// of uNetworkChangeStateBle().
int32_t uNetworkUpBle(uDeviceHandle_t devHandle,
                      const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode;
    uBleCfg_t cfg;
    cfg.role = (uBleCfgRole_t) pConfiguration->role;
    cfg.spsServer = pConfiguration->spsServer;
    errorCode = uBleCfgConfigure(devHandle, &cfg);
    if (errorCode >= 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality will be in the "down" part
// of uNetworkChangeStateBle().
int32_t uNetworkDownBle(uDeviceHandle_t devHandle,
                        const uNetworkConfigurationBle_t *pConfiguration)
{
    // Up and down is the same function as the pConfiguration variable determines
    // if ble and/or sps is enabled or disabled. So we trust the user to set the
    // correct values here.
    return uNetworkUpBle(devHandle, pConfiguration);
}

// TODO rename this to uNetworkPrivateChangeStateBle() for consistency?
// Bring a BLE interface up or take it down.
int32_t uNetworkChangeStateBle(uDeviceHandle_t devHandle,
                               uDeviceNetworkCfgBle_t *pCfg, bool up)
{
    (void) devHandle;
    (void) pCfg;
    (void) up;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

#endif
// End of file
