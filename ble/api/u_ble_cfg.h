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

#ifndef _U_BLE_CFG_H_
#define _U_BLE_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _BLE
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that configure BLE.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef enum {
    U_BLE_CFG_ROLE_DISABLED = 0, /**< BLE disabled. */
    U_BLE_CFG_ROLE_CENTRAL, /**< central only mode. */
    U_BLE_CFG_ROLE_PERIPHERAL, /**< peripheral only mode. */
    U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL, /**< simultaneous central and peripheral mode. */
} uBleCfgRole_t;

typedef struct {
    uBleCfgRole_t role;
    bool spsServer;
} uBleCfg_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure BLE for a short range module, may require module restarts
 *  so can take up to 500 ms before it returns.
 *
 * @param devHandle     the handle of the BLE instance.
 * @param[in] pCfg      pointer to the configuration data, must not be NULL.
 * @return              zero on success or negative error code
 *                      on failure.
 */
int32_t uBleCfgConfigure(uDeviceHandle_t devHandle,
                         const uBleCfg_t *pCfg);
#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_BLE_CFG_H_

// End of file
