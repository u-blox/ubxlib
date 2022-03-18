/*
 * Copyright 2020 u-blox
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

/** @file
 * @brief Internal high-level API for initializing an u-blox device (chip or module).
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include <stddef.h>    // NULL, size_t etc.
#include <stdint.h>    // int32_t etc.
#include <stdbool.h>   // bool.
#include <stdlib.h>
#include <string.h>

#include "u_cfg_sw.h"
#include "u_compiler.h" // for U_INLINE
#include "u_error_common.h"

#include "u_port_debug.h"

#include "u_device_internal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_DEVICE_MAGIC_NUMBER   0x0EA7BEEF

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

uDeviceInstance_t *uDeviceCreateInstance(uDeviceType_t type)
{
    uDeviceInstance_t *pInstance;
    pInstance = (uDeviceInstance_t *)malloc(sizeof(uDeviceInstance_t));
    if (pInstance) {
        uDeviceInitInstance(pInstance, type);
    }
    return pInstance;
}

void uDeviceDestroyInstance(uDeviceInstance_t *pInstance)
{
    if (uDeviceIsValidInstance(pInstance)) {
        // Invalidate the instance
        pInstance->magic = 0;
        free(pInstance);
    } else {
        uPortLog("U_DEVICE: Warning: Trying to destroy an already destroyed instance");
    }
}


U_INLINE void uDeviceInitInstance(uDeviceInstance_t *pInstance,
                                  uDeviceType_t type)
{
    //lint -esym(429, pInstance) Suppress "custodial pointer not been freed or returned"
    //                           Why should it be freed or returned??
    memset(pInstance, 0, sizeof(uDeviceInstance_t));
    pInstance->magic = U_DEVICE_MAGIC_NUMBER;
    pInstance->type = type;
}

U_INLINE bool uDeviceIsValidInstance(const uDeviceInstance_t *pInstance)
{
    return pInstance && (pInstance->magic == U_DEVICE_MAGIC_NUMBER);
}

U_INLINE int32_t uDeviceGetInstance(uDeviceHandle_t devHandle,
                                    uDeviceInstance_t **ppInstance)
{
    *ppInstance = U_DEVICE_INSTANCE(devHandle);
    bool isValid = uDeviceIsValidInstance(*ppInstance);
    return isValid ? (int32_t) U_ERROR_COMMON_SUCCESS :
           (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
}

U_INLINE int32_t uDeviceGetDeviceType(uDeviceHandle_t devHandle)
{
    uDeviceInstance_t *pInstance;
    int32_t returnCode = uDeviceGetInstance(devHandle, &pInstance);
    if (returnCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
        returnCode = (int32_t)pInstance->type;
    }
    return returnCode;
}


// End of file
