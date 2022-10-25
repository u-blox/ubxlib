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
 * @brief Implementation of the cfg API for ble.
 */

#ifdef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_cfg_sw.h"
#include "u_port_gatt.h"

#include "u_ble_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */
extern const uPortGattService_t gSpsService;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int32_t uBleCfgConfigure(uDeviceHandle_t devHandle,
                         const uBleCfg_t *pCfg)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (uDeviceGetDeviceType(devHandle) != (int32_t) U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (pCfg != NULL) {
        bool startAdv;

        if (pCfg->role == U_BLE_CFG_ROLE_DISABLED) {
            uPortGattDown();
            errorCode = uPortGattRemoveAllServices();
        } else {
            if (pCfg->spsServer) {
                uPortGattAddPrimaryService(&gSpsService);
            }
            startAdv = ((pCfg->role == U_BLE_CFG_ROLE_PERIPHERAL) ||
                        (pCfg->role == U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL));
            errorCode = uPortGattUp(startAdv);
        }
    }

    return errorCode;
}

#endif

// End of file
