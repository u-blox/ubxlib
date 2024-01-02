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
 * @brief Implementations of the functions to apply a geofence,
 * created using the common Geofence API, to a cellular device.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // LLONG_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Order is important here
#include "u_cell_private.h" // don't change it
#include "u_cell_geofence.h"

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
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the horzontal speed that the cellular instance can travel at.
int32_t uCellGeofenceSetMaxSpeed(uDeviceHandle_t cellHandle,
                                 int64_t maxSpeedMillimetresPerSecond)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uCellPrivateInstance_t *pInstance;
    uGeofenceContext_t **ppFenceContext;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            ppFenceContext = (uGeofenceContext_t **) &pInstance->pFenceContext;
            errorCode = uGeofenceContextEnsure(ppFenceContext);
            if (*ppFenceContext != NULL) {
                (*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond = maxSpeedMillimetresPerSecond;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) cellHandle;
    (void) maxSpeedMillimetresPerSecond;
#endif

    return errorCode;
}

// Apply a fence to a cellular instance.
int32_t uCellGeofenceApply(uDeviceHandle_t cellHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uCellPrivateInstance_t *pInstance;
    uGeofenceContext_t **ppFenceContext;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pFence != NULL) && (pInstance != NULL)) {
            ppFenceContext = (uGeofenceContext_t **) &pInstance->pFenceContext;
            errorCode = uGeofenceApply(ppFenceContext, pFence);
            if ((*ppFenceContext != NULL) &&
                ((*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond < 0)) {
                (*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond =
                    U_GEOFENCE_HORIZONTAL_SPEED_MILLIMETRES_PER_SECOND_MAX;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) cellHandle;
    (void) pFence;
#endif

    return errorCode;
}

// Remove fence(s) from cellular instance(s).
int32_t uCellGeofenceRemove(uDeviceHandle_t cellHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uCellPrivateInstance_t *pInstance = NULL;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (cellHandle != NULL) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
        }
        if ((pInstance != NULL) || (gpUCellPrivateInstanceList == NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            while (pInstance != NULL) {
                errorCode = uGeofenceRemove((uGeofenceContext_t **) &pInstance->pFenceContext,
                                            pFence);
                // Next instance
                pInstance = pInstance->pNext;
                if (cellHandle != NULL) {
                    // Just doing the one, stop there
                    pInstance = NULL;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) cellHandle;
    (void) pFence;
#endif

    return errorCode;
}

// Associate a callback with a device.
int32_t uCellGeofenceSetCallback(uDeviceHandle_t cellHandle,
                                 uGeofenceTestType_t testType,
                                 bool pessimisticNotOptimistic,
                                 uGeofenceCallback_t *pCallback,
                                 void *pCallbackParam)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uCellPrivateInstance_t *pInstance;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (cellHandle != NULL) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
            if (pInstance != NULL) {
                errorCode = uGeofenceSetCallback((uGeofenceContext_t **) &pInstance->pFenceContext,
                                                 testType,
                                                 pessimisticNotOptimistic,
                                                 pCallback,
                                                 pCallbackParam);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) cellHandle;
    (void) testType;
    (void) pessimisticNotOptimistic;
    (void) pCallback;
    (void) pCallbackParam;
#endif

    return errorCode;
}

// Manually provide a position to be evaluated against the fence.
uGeofencePositionState_t uCellGeofencePosition(uDeviceHandle_t cellHandle,
                                               uGeofenceTestType_t testType,
                                               bool pessimisticNotOptimistic,
                                               int64_t latitudeX1e9,
                                               int64_t longitudeX1e9,
                                               int32_t altitudeMillimetres,
                                               int32_t radiusMillimetres,
                                               int32_t altitudeUncertaintyMillimetres)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;

#ifdef U_CFG_GEOFENCE
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;
    uGeofencePositionState_t instancePositionState;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (cellHandle != NULL) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
        }
        while (pInstance != NULL) {
            instancePositionState = uGeofenceContextTest(cellHandle,
                                                         (uGeofenceContext_t *) pInstance->pFenceContext,
                                                         testType,
                                                         pessimisticNotOptimistic,
                                                         latitudeX1e9,
                                                         longitudeX1e9,
                                                         altitudeMillimetres,
                                                         radiusMillimetres,
                                                         altitudeUncertaintyMillimetres);
            if (positionState == U_GEOFENCE_POSITION_STATE_NONE) {
                // If we've never updated the over all position state, do it now
                positionState = instancePositionState;
            }
            if (instancePositionState == U_GEOFENCE_POSITION_STATE_INSIDE) {
                // For the over all state, any instance being inside a fence is
                // "inside", make it stick
                positionState = instancePositionState;
            }
            // Next instance
            pInstance = pInstance->pNext;
            if (cellHandle != NULL) {
                // Just doing the one, stop there
                pInstance = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
#else
    (void) cellHandle;
    (void) testType;
    (void) pessimisticNotOptimistic;
    (void) latitudeX1e9;
    (void) longitudeX1e9;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) altitudeUncertaintyMillimetres;
#endif

    return positionState;
}

// End of file
