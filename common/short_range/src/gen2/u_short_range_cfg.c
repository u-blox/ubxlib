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
 * @brief Implementation of the "general" API for short range modules.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strtol(), atoi()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // Required by u_at_client.h

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_short_range.h"
#include "u_short_range_private.h"

#include "u_cx_system.h"

#include "u_short_range_cfg.h"

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

int32_t uShortRangeCfgFactoryReset(uDeviceHandle_t devHandle)
{
    int32_t errorCode = uShortRangeLock();
    if (errorCode == 0) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
        if (pUcxHandle != NULL) {
            errorCode = uCxSystemFactoryReset(pUcxHandle);
            if (errorCode == 0) {
                errorCode = uShortrangePrivateRestartDevice(devHandle, false);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

// End of file