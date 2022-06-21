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
 * @brief Stub of the short-range portion of the device API.
 * Build this instead of u_device_private_short_range.c if you want to
 * avoid linking short-range features into your application.
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

int32_t uDevicePrivateShortRangeInit()
{
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

void uDevicePrivateShortRangeDeinit()
{
}

int32_t uDevicePrivateShortRangeAdd(const uDeviceCfg_t *pDevCfg,
                                    uDeviceHandle_t *pDeviceHandle)
{
    (void) pDevCfg;
    (void) pDeviceHandle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

int32_t uDevicePrivateShortRangeOpenCpuAdd(const uDeviceCfg_t *pDevCfg,
                                           uDeviceHandle_t *pDeviceHandle)
{
    (void) pDevCfg;
    (void) pDeviceHandle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

int32_t uDevicePrivateShortRangeRemove(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

int32_t uDevicePrivateShortRangeOpenCpuRemove(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;;
}

// End of file
