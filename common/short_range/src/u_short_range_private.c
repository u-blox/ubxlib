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

#include "u_at_client.h"

#include "u_device_shared.h"
#include "u_error_common.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"

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
        U_SHORT_RANGE_MODULE_TYPE_ANNA_B1,
        (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_NINA_B1,
        (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_NINA_B2,
        (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_NINA_B3,
        (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_NINA_W13,
        ((1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER) |
         (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT)) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_NINA_W15,
        (1UL << (int32_t) U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT) /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
    },
    {
        U_SHORT_RANGE_MODULE_TYPE_ODIN_W2,
        0  /* features */,
        5 /* Boot wait */, 5 /* Min awake */,
        5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
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

static int32_t getServer(const uAtClientHandle_t atHandle, uShortRangeServerType_t type)
{
    int32_t errorOrId = (int32_t)U_ERROR_COMMON_NOT_FOUND;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UDSC");
    uAtClientCommandStop(atHandle);
    while (uAtClientResponseStart(atHandle, "+UDSC:") == 0) {
        int32_t id = uAtClientReadInt(atHandle);
        if (uAtClientReadInt(atHandle) == (int32_t)type) {
            errorOrId = id;
            break;
        }
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    return errorOrId;
}

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

int32_t uShortRangePrivateStartServer(const uAtClientHandle_t atHandle,
                                      uShortRangeServerType_t type,
                                      const char *pParam)
{
    int32_t errorOrId = getServer(atHandle, type);
    if (errorOrId >= 0) {
        // Only one server of the type can be active
        return errorOrId;
    }
    // Find first empty slot
    errorOrId = getServer(atHandle, U_SHORT_RANGE_SERVER_DISABLED);
    if (errorOrId >= 0) {
        int32_t id = errorOrId;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDSC=");
        uAtClientWriteInt(atHandle, id);
        uAtClientWriteInt(atHandle, (int32_t)type);
        if (pParam) {
            uAtClientWriteString(atHandle, pParam, false);
        }
        uAtClientCommandStopReadResponse(atHandle);
        errorOrId = uAtClientUnlock(atHandle);
        if (errorOrId == 0) {
            errorOrId = id;
        }
    }
    return errorOrId;
}

int32_t uShortRangePrivateStopServer(const uAtClientHandle_t atHandle, int32_t serverId)
{
    int32_t errorCode;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UDSC=");
    uAtClientWriteInt(atHandle, serverId);
    uAtClientWriteInt(atHandle, 0);
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    uPortTaskBlock(1000);
    return errorCode;
}

// End of file
