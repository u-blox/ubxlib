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

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_ble_module_type.h"
#include "u_ble.h"
#include "u_ble_cfg.h"

#include "u_network.h"
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

// Bring a BLE interface up or take it down.
int32_t uNetworkPrivateChangeStateBle(uDeviceHandle_t devHandle,
                                      const uNetworkCfgBle_t *pCfg,
                                      bool upNotDown)
{
    uDeviceInstance_t *pDevInstance;
    int32_t errorCode = uDeviceGetInstance(devHandle, &pDevInstance);
    uBleCfg_t bleCfg;

    (void) upNotDown;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_BLE)) {
            // Up and down is the same function as the pCfg variable
            // determines if ble and/or sps is enabled or disabled.
            // So we trust the user to set the correct values here.
            bleCfg.role = (uBleCfgRole_t) pCfg->role;
            bleCfg.spsServer = pCfg->spsServer;
            errorCode = uBleCfgConfigure(devHandle, &bleCfg);
            if (errorCode >= 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

#endif
// End of file
