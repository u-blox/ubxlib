/*
 * Copyright 2019-2023 u-blox
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
 * @brief Stubs to allow the device API to be compiled without GNSS; if
 * you call a GNSS API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when GNSS is not included in the
 * build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_device.h"
#include "u_device_shared.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uDevicePrivateGnssInit()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uDevicePrivateGnssDeinit()
{
}

U_WEAK int32_t uDevicePrivateGnssAdd(const uDeviceCfg_t *pDevCfg,
                                     uDeviceHandle_t *pDeviceHandle)
{
    (void) pDevCfg;
    (void) pDeviceHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uDevicePrivateGnssRemove(uDeviceHandle_t devHandle,
                                        bool powerOff)
{
    (void) devHandle;
    (void) powerOff;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
