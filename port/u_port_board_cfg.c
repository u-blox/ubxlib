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

/** @file
 * @brief Default implementation of uPortDeviceCfg().
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_compiler.h" // U_WEAK

#include "u_error_common.h"

#include "u_port_board_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Default implementation; does nothing and returns success.
U_WEAK int32_t uPortBoardCfgDevice(void *pDeviceCfg)
{
    (void) pDeviceCfg;

    return U_ERROR_COMMON_SUCCESS;
}

// Default implementation; does nothing and returns success.
U_WEAK int32_t uPortBoardCfgNetwork(uDeviceHandle_t devHandle,
                                    uNetworkType_t networkType,
                                    void *pNetworkCfg)
{
    (void) devHandle;
    (void) networkType;
    (void) pNetworkCfg;

    return U_ERROR_COMMON_SUCCESS;
}

// End of file
