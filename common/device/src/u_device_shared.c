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
 * @brief Functions for initializing a u-blox device (chip or module),
 * that do not form part of the device API but are shared internally
 * for use within ubxlib.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // for malloc()/free()
#include "string.h"    // for memset()

#include "u_cfg_sw.h"
#include "u_compiler.h" // for U_INLINE
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_device_shared.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Used for validation of the device structure.
 */
#define U_DEVICE_MAGIC_NUMBER 0x0EA7BEEF

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the device (and network) APIs.  Also used
 * for a non-NULL check that we're initialised.
 */
uPortMutexHandle_t gMutex = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uDeviceMutexCreate()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

void uDeviceMutexDestroy()
{
    if (gMutex != NULL) {
        // Make sure the mutex isn't locked before
        // we delete it
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

uDeviceInstance_t *pUDeviceCreateInstance(uDeviceType_t type)
{
    uDeviceInstance_t *pInstance;
    pInstance = (uDeviceInstance_t *) malloc(sizeof(uDeviceInstance_t));

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
        uPortLog("U_DEVICE: Warning: trying to destroy an already"
                 " destroyed instance.\n");
    }
}

int32_t uDeviceLock()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {
        errorCode = uPortMutexLock(gMutex);
    }

    return errorCode;
}

int32_t uDeviceUnlock()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {
        errorCode = uPortMutexUnlock(gMutex);
    }

    return errorCode;
}

U_INLINE void uDeviceInitInstance(uDeviceInstance_t *pInstance,
                                  uDeviceType_t type)
{
    memset(pInstance, 0, sizeof(uDeviceInstance_t));
    pInstance->magic = U_DEVICE_MAGIC_NUMBER;
    pInstance->deviceType = type;
}

U_INLINE bool uDeviceIsValidInstance(const uDeviceInstance_t *pInstance)
{
    return (pInstance != NULL) && (pInstance->magic == U_DEVICE_MAGIC_NUMBER);
}

U_INLINE int32_t uDeviceGetInstance(uDeviceHandle_t devHandle,
                                    uDeviceInstance_t **ppInstance)
{
    bool isValid = false;

    if (devHandle != NULL) {
        *ppInstance = U_DEVICE_INSTANCE(devHandle);
        isValid = uDeviceIsValidInstance(*ppInstance);
    }

    return isValid ? (int32_t) U_ERROR_COMMON_SUCCESS : (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
}

U_INLINE int32_t uDeviceGetDeviceType(uDeviceHandle_t devHandle)
{
    uDeviceInstance_t *pInstance;
    int32_t errorCodeOrType = uDeviceGetInstance(devHandle, &pInstance);

    if (errorCodeOrType == (int32_t) U_ERROR_COMMON_SUCCESS) {
        errorCodeOrType = (int32_t) pInstance->deviceType;
    }

    return errorCodeOrType;
}

// End of file
