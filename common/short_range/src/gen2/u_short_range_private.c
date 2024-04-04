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
 * @brief Implementation of functions that are private to short range.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_port_os.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_device_shared.h"
#include "u_error_common.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"

#include "u_cx_system.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES THAT ARE SHARED THROUGHOUT THE SHORT RANGE IMPLEMENTATION
 * -------------------------------------------------------------- */

/** Root for the linked list of instances.
 */
uShortRangePrivateInstance_t *gpUShortRangePrivateInstanceList = NULL;

const uShortRangePrivateModule_t gUShortRangePrivateModuleList[] = {
    {
        U_SHORT_RANGE_MODULE_TYPE_NORA_W36,
        ((1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) |
         (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT)) /* features */,
        5 /* Boot wait */,
        5 /* Min awake */,
        5 /* Pwr down wait */,
        5 /* Reboot wait */,
        5 /* AT timeout */,
    },
    // Add new module types here, before the U_SHORT_RANGE_MODULE_TYPE_ANY entry.
    {
        // The module attributes set here are such that they help in identifying
        // the actual module type.
        U_SHORT_RANGE_MODULE_TYPE_ANY,
        0  /* features */,
        5 /* Boot wait */,
        5 /* Min awake */,
        5 /* Pwr down wait */,
        5 /* Reboot wait */,
        10 /* AT timeout */,
    }
};

/** Mutex to protect the linked list.
 */
uPortMutexHandle_t gUShortRangePrivateMutex = NULL;

/** Number of items in the gUShortRangePrivateModuleList array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUShortRangePrivateModuleListSize = sizeof(gUShortRangePrivateModuleList) /
                                                 sizeof(gUShortRangePrivateModuleList[0]);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO SHORT RANGE
 * -------------------------------------------------------------- */

// Find a short range instance in the list by instance handle.
uShortRangePrivateInstance_t *pUShortRangePrivateGetInstance(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance = NULL;
    uDeviceInstance_t *pDevInstance = U_DEVICE_INSTANCE(devHandle);
    // Check that the handle is valid
    if (uDeviceIsValidInstance(pDevInstance)) {
        pInstance = (uShortRangePrivateInstance_t *)pDevInstance->pContext;
    }

    return pInstance;
}

// Get the module characteristics for a given instance.
const uShortRangePrivateModule_t *pUShortRangePrivateGetModule(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    const uShortRangePrivateModule_t *pModule = NULL;

    if (pInstance != NULL) {
        pModule = pInstance->pModule;
    }

    return pModule;
}

uCxHandle_t *pShortRangePrivateGetUcxHandle(uDeviceHandle_t devHandle)
{
    uCxHandle_t *pUcxHandle = NULL;
    uShortRangePrivateInstance_t *pDevInstance = pUShortRangePrivateGetInstance(devHandle);
    if (pDevInstance != NULL) {
        pUcxHandle = &(pDevInstance->pUcxContext->uCxHandle);
    }
    return pUcxHandle;
}

int32_t uShortrangePrivateRestartDevice(uDeviceHandle_t devHandle, bool storeConfig)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = 0;
        if (storeConfig) {
            errorCode = uCxSystemStoreConfiguration(pUcxHandle);
        }
        if (errorCode == 0) {
            errorCode = uCxSystemReboot(pUcxHandle);
        }
        if (errorCode == 0) {
            // Wait for module to become responsive
            uPortTaskBlock(5000);
            int32_t tries = 0;
            while (tries++ < 5) {
                errorCode = uShortRangeAttention(devHandle);
                if (errorCode == 0) {
                    break;
                }
            }
        }
    }
    return errorCode;
}

// The following routines are not used for uCx

int32_t uShortRangePrivateStartServer(const uAtClientHandle_t atHandle,
                                      uShortRangeServerType_t type,
                                      const char *pParam)
{
    (void)atHandle;
    (void)type;
    (void)pParam;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

int32_t uShortRangePrivateStopServer(const uAtClientHandle_t atHandle, int32_t serverId)
{
    (void)atHandle;
    (void)serverId;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// End of file
