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

#include "u_device_shared.h"

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

// TODO: WILL BE REMOVED.
int32_t uNetworkInitBle(void)
{
    uBleInit();

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// TODO: WILL BE REMOVED.
void uNetworkDeinitBle(void)
{
    uBleDeinit();
}

// TODO: WILL BE REMOVED: into uDevicePrivateAddShortRange() in
// u_device_private_short_range.c.
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration,
                       uDeviceHandle_t *pDevHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((uBleModuleType_t)pConfiguration->module == U_BLE_MODULE_TYPE_INTERNAL) {
        errorCode = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        *pDevHandle =
            (uDeviceHandle_t)pUDeviceCreateInstance(U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU);
        if (*pDevHandle != NULL) {
            errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: into uDevicePrivateRemoveShortRange() in
// u_device_private_short_range.c.
int32_t uNetworkRemoveBle(uDeviceHandle_t devHandle)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    uDeviceDestroyInstance(U_DEVICE_INSTANCE(devHandle));
    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

// TODO: WILL BE REMOVED: functionality will be in the "up" part
// of uNetworkChangeStateBle().
int32_t uNetworkUpBle(uDeviceHandle_t devHandle,
                      const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode;
    uBleCfg_t cfg;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    cfg.role = (uBleCfgRole_t) pConfiguration->role;
    cfg.spsServer = pConfiguration->spsServer;
    uPortLog("call uBleCfgConfigure\n");
    errorCode = uBleCfgConfigure(devHandle, &cfg);
    if (errorCode >= 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }
    uPortLog("uNetworkUpBle complete\n");

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality will be in the "down" part
// of uNetworkChangeStateBle().
int32_t uNetworkDownBle(uDeviceHandle_t devHandle,
                        const uNetworkConfigurationBle_t *pConfiguration)
{
    int32_t errorCode;
    uBleCfg_t cfg;
    (void)pConfiguration;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    cfg.role = U_BLE_CFG_ROLE_DISABLED;
    cfg.spsServer = false;
    errorCode = uBleCfgConfigure(devHandle, &cfg);
    if (errorCode >= 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Bring a BLE interface up or take it down.
int32_t uNetworkPrivateChangeStateBle(uDeviceHandle_t devHandle,
                                      uNetworkCfgBle_t *pCfg,
                                      bool upNotDown)
{
    (void) devHandle;
    (void) pCfg;
    (void) upNotDown;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

#endif

// End of file
