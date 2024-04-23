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
 * created using the common Geofence API, to a WiFi device.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"         // Order is important

#include "u_wifi_geofence.h"
#include "u_wifi_private.h"

#include "u_http_client.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_hex_bin_convert.h"

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

int32_t uWifiGeofenceSetMaxSpeed(uDeviceHandle_t wifiHandle,
                                 int64_t maxSpeedMillimetresPerSecond)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uShortRangePrivateInstance_t *pInstance;
    uGeofenceContext_t **ppFenceContext;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            ppFenceContext = (uGeofenceContext_t **) &pInstance->pFenceContext;
            errorCode = uGeofenceContextEnsure(ppFenceContext);
            if (*ppFenceContext != NULL) {
                (*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond = maxSpeedMillimetresPerSecond;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) wifiHandle;
    (void) maxSpeedMillimetresPerSecond;
#endif

    return errorCode;
}

int32_t uWifiGeofenceApply(uDeviceHandle_t wifiHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uShortRangePrivateInstance_t *pInstance;
    uGeofenceContext_t **ppFenceContext;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
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

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) wifiHandle;
    (void) pFence;
#endif

    return errorCode;
}

int32_t uWifiGeofenceRemove(uDeviceHandle_t wifiHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uShortRangePrivateInstance_t *pInstance = NULL;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (wifiHandle != NULL) {
            pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        }
        if ((pInstance != NULL) || (gpUShortRangePrivateInstanceList == NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            while (pInstance != NULL) {
                errorCode = uGeofenceRemove((uGeofenceContext_t **) &pInstance->pFenceContext,
                                            pFence);
                // Next instance
                pInstance = pInstance->pNext;
                if (wifiHandle != NULL) {
                    // Just doing the one, stop there
                    pInstance = NULL;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) wifiHandle;
    (void) pFence;
#endif

    return errorCode;
}

int32_t uWifiGeofenceSetCallback(uDeviceHandle_t wifiHandle,
                                 uGeofenceTestType_t testType,
                                 bool pessimisticNotOptimistic,
                                 uGeofenceCallback_t *pCallback,
                                 void *pCallbackParam)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uShortRangePrivateInstance_t *pInstance;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (wifiHandle != NULL) {
            pInstance = pUShortRangePrivateGetInstance(wifiHandle);
            if (pInstance != NULL) {
                errorCode = uGeofenceSetCallback((uGeofenceContext_t **) &pInstance->pFenceContext,
                                                 testType,
                                                 pessimisticNotOptimistic,
                                                 pCallback,
                                                 pCallbackParam);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) wifiHandle;
    (void) testType;
    (void) pessimisticNotOptimistic;
    (void) pCallback;
    (void) pCallbackParam;
#endif

    return errorCode;
}

uGeofencePositionState_t uWifiGeofencePosition(uDeviceHandle_t wifiHandle,
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
    uShortRangePrivateInstance_t *pInstance = gpUShortRangePrivateInstanceList;
    uGeofencePositionState_t instancePositionState;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        if (wifiHandle != NULL) {
            pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        }
        while (pInstance != NULL) {
            instancePositionState = uGeofenceContextTest(wifiHandle,
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
            if (wifiHandle != NULL) {
                // Just doing the one, stop there
                pInstance = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
#else
    (void) wifiHandle;
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
