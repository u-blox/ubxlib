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

/** @file
 * @brief Stub of the cellular portion of the device API.
 * Build this instead of u_device_private_cell.c if you want to
 * avoid linking cellular features into your application.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"
#include "u_device.h"
#include "u_device_shared.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uDevicePrivateCellInit()
{
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

void uDevicePrivateCellDeinit()
{
}

int32_t uDevicePrivateCellAdd(const uDeviceCfg_t *pDevCfg,
                              uDeviceHandle_t *pDeviceHandle)
{
    (void) pDevCfg;
    (void) pDeviceHandle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

int32_t uDevicePrivateCellRemove(uDeviceHandle_t devHandle,
                                 bool powerOff)
{
    (void) devHandle;
    (void) powerOff;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

// End of file
