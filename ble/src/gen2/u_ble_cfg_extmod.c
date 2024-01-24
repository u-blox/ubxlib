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
 * @brief Implementation of the cfg API for ble.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_cfg_sw.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_ble_cfg.h"

#include "u_cx_system.h"
#include "u_cx_bluetooth.h"
#include "u_cx_sps.h"

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
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO BLE EXTMOD
 * -------------------------------------------------------------- */

int32_t uBlePrivateGetRole(uDeviceHandle_t devHandle)
{
    int32_t mode = 0;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uCxBluetoothGetMode(pUcxHandle, (uBtMode_t *)&mode);
    }
    return mode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleCfgConfigure(uDeviceHandle_t devHandle,
                         const uBleCfg_t *pCfg)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uBtMode_t mode = (uBtMode_t)pCfg->role;
    if ((mode == U_BT_MODE_CENTRAL) && pCfg->spsServer) {
        mode = U_BT_MODE_CENTRAL_PERIPHERAL;
    }
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
        int32_t currMode = uBlePrivateGetRole(devHandle);
        if (currMode != mode) {
            errorCode = uCxBluetoothSetMode(pUcxHandle, mode);
            // Restart needed to change mode
            errorCode = uShortrangePrivateRestartDevice(devHandle, true);
        }
        bool isActive = false;
        if (errorCode == 0) {
            uSpsServiceOption_t opt;
            errorCode = uCxSpsGetServiceEnable(pUcxHandle, &opt);
            isActive = opt == U_SPS_SERVICE_OPTION_ENABLE_SPS_SERVICE;
        }
        if ((errorCode == 0) && (isActive != pCfg->spsServer)) {
            errorCode = uCxSpsSetServiceEnable(pUcxHandle,
                                               pCfg->spsServer ?
                                               U_SPS_SERVICE_OPTION_ENABLE_SPS_SERVICE :
                                               U_SPS_SERVICE_OPTION_DISABLE_SPS_SERVICE);
            // *** UCX WORKAROUND FIX ***
            // Need to restart
            errorCode = uShortrangePrivateRestartDevice(devHandle, true);
        }
    }
    return errorCode;
}

#endif

// End of file
