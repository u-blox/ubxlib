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
 * @brief Implementations of the functions to apply a geofence,
 * created using the common Geofence API, to a GNSS device.
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

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_private.h" // For uGnssCfgPrivateGetDynamic()
#include "u_gnss_geofence.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_GEOFENCE_PORTABLE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_PORTABLE in metres per second.
 */
# define U_GNSS_GEOFENCE_PORTABLE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 310
#endif

#ifndef U_GNSS_GEOFENCE_PORTABLE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_PORTABLE in metres per second.
 */
# define U_GNSS_GEOFENCE_PORTABLE_VERTICAL_SPEED_METRES_PER_SECOND_MAX 50
#endif

#ifndef U_GNSS_GEOFENCE_STATIONARY_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_STATIONARY in metres per second.
 */
# define U_GNSS_GEOFENCE_STATIONARY_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 10
#endif

#ifndef U_GNSS_GEOFENCE_STATIONARY_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_STATIONARY in metres per second.
 */
# define U_GNSS_GEOFENCE_STATIONARY_VERTICAL_SPEED_METRES_PER_SECOND_MAX 6
#endif

#ifndef U_GNSS_GEOFENCE_PEDESTRIAN_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_PEDESTRIAN in metres per second.
 */
# define U_GNSS_GEOFENCE_PEDESTRIAN_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 30
#endif

#ifndef U_GNSS_GEOFENCE_PEDESTRIAN_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_PEDESTRIAN in metres per second.
 */
# define U_GNSS_GEOFENCE_PEDESTRIAN_VERTICAL_SPEED_METRES_PER_SECOND_MAX 20
#endif

#ifndef U_GNSS_GEOFENCE_AUTOMOTIVE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_AUTOMOTIVE in metres per second.
 */
# define U_GNSS_GEOFENCE_AUTOMOTIVE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_AUTOMOTIVE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_AUTOMOTIVE in metres per second.
 */
# define U_GNSS_GEOFENCE_AUTOMOTIVE_VERTICAL_SPEED_METRES_PER_SECOND_MAX 15
#endif

#ifndef U_GNSS_GEOFENCE_SEA_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_SEA in metres per second.
 */
# define U_GNSS_GEOFENCE_SEA_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 25
#endif

#ifndef U_GNSS_GEOFENCE_SEA_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_SEA in metres per second.
 */
# define U_GNSS_GEOFENCE_SEA_VERTICAL_SPEED_METRES_PER_SECOND_MAX 5
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_1G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_1G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_1G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_1G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_1G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_1G_VERTICAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_2G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_2G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_2G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 250
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_2G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_2G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_2G_VERTICAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_4G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_4G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_4G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 500
#endif

#ifndef U_GNSS_GEOFENCE_AIRBORNE_4G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_AIRBORNE_4G in metres per second.
 */
# define U_GNSS_GEOFENCE_AIRBORNE_4G_VERTICAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_WRIST_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_WRIST in metres per second.
 */
# define U_GNSS_GEOFENCE_WRIST_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 30
#endif

#ifndef U_GNSS_GEOFENCE_WRIST_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_WRIST in metres per second.
 */
# define U_GNSS_GEOFENCE_WRIST_VERTICAL_SPEED_METRES_PER_SECOND_MAX 20
#endif

#ifndef U_GNSS_GEOFENCE_BIKE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_BIKE in metres per second.
 */
# define U_GNSS_GEOFENCE_BIKE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 100
#endif

#ifndef U_GNSS_GEOFENCE_BIKE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_BIKE in metres per second.
 */
# define U_GNSS_GEOFENCE_BIKE_VERTICAL_SPEED_METRES_PER_SECOND_MAX 15
#endif

#ifndef U_GNSS_GEOFENCE_MOWER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_MOWER in metres per second.
 */
# define U_GNSS_GEOFENCE_MOWER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 3
#endif

#ifndef U_GNSS_GEOFENCE_MOWER_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_MOWER in metres per second.
 */
# define U_GNSS_GEOFENCE_MOWER_VERTICAL_SPEED_METRES_PER_SECOND_MAX -1
#endif

#ifndef U_GNSS_GEOFENCE_ESCOOTER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum horizontal speed for dynamic model
 * #U_GNSS_DYNAMIC_ESCOOTER in metres per second.
 */
# define U_GNSS_GEOFENCE_ESCOOTER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX 50
#endif

#ifndef U_GNSS_GEOFENCE_ESCOOTER_VERTICAL_SPEED_METRES_PER_SECOND_MAX
/** The maximum vertical speed for dynamic model
 * #U_GNSS_DYNAMIC_ESCOOTER in metres per second.
 */
# define U_GNSS_GEOFENCE_ESCOOTER_VERTICAL_SPEED_METRES_PER_SECOND_MAX 15
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

/** Structure to hold the possible types of dynamic model.
 */
typedef struct {
    uGnssDynamic_t dynamicModel;
    int32_t horizontalSpeedMetresPerSecondMax;
    int32_t verticalSpeedMetresPerSecondMax;
} uGnssGeofenceDynamicModel_t;

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

/** The maximum horizontal and vertical speeds for each dynamic model.
 */
static const uGnssGeofenceDynamicModel_t gMaxSpeed[] = {
    {
        U_GNSS_DYNAMIC_PORTABLE, U_GNSS_GEOFENCE_PORTABLE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_PORTABLE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_STATIONARY, U_GNSS_GEOFENCE_STATIONARY_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_STATIONARY_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_PEDESTRIAN, U_GNSS_GEOFENCE_PEDESTRIAN_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_PEDESTRIAN_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_AUTOMOTIVE, U_GNSS_GEOFENCE_AUTOMOTIVE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_AUTOMOTIVE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_SEA, U_GNSS_GEOFENCE_SEA_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_SEA_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_AIRBORNE_1G, U_GNSS_GEOFENCE_AIRBORNE_1G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_AIRBORNE_1G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_AIRBORNE_2G, U_GNSS_GEOFENCE_AIRBORNE_2G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_AIRBORNE_2G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_AIRBORNE_4G, U_GNSS_GEOFENCE_AIRBORNE_4G_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_AIRBORNE_4G_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_WRIST, U_GNSS_GEOFENCE_WRIST_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_WRIST_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_BIKE, U_GNSS_GEOFENCE_BIKE_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_BIKE_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_MOWER, U_GNSS_GEOFENCE_MOWER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_MOWER_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    },
    {
        U_GNSS_DYNAMIC_ESCOOTER, U_GNSS_GEOFENCE_ESCOOTER_HORIZONTAL_SPEED_METRES_PER_SECOND_MAX,
        U_GNSS_GEOFENCE_ESCOOTER_VERTICAL_SPEED_METRES_PER_SECOND_MAX
    }
};

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Get the maximum horizontal speed that a GNSS instance is able
// to travel.
static int32_t getMaxHorizontalSpeed(uGnssPrivateInstance_t *pInstance)
{
    int32_t maxHorizontalSpeedMetresPerSecond = -1;
    int32_t dynamicModel = uGnssCfgPrivateGetDynamic(pInstance);

    if (dynamicModel >= 0) {
        for (size_t x = 0; (x < sizeof(gMaxSpeed) / sizeof(gMaxSpeed[0])) &&
             (maxHorizontalSpeedMetresPerSecond < 0); x++) {
            if (gMaxSpeed[x].dynamicModel == (uGnssDynamic_t) dynamicModel) {
                maxHorizontalSpeedMetresPerSecond = gMaxSpeed[x].horizontalSpeedMetresPerSecondMax;
            }
        }
    }

    return maxHorizontalSpeedMetresPerSecond;
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Apply a fence to a GNSS instance.
int32_t uGnssGeofenceApply(uDeviceHandle_t gnssHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uGnssPrivateInstance_t *pInstance;
    uGeofenceContext_t **ppFenceContext;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pFence != NULL) && (pInstance != NULL)) {
            ppFenceContext = (uGeofenceContext_t **) &pInstance->pFenceContext;
            errorCode = uGeofenceApply(ppFenceContext, pFence);
            if (*ppFenceContext != NULL) {
                (*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond = getMaxHorizontalSpeed(
                                                                                        pInstance) * 1000;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) gnssHandle;
    (void) pFence;
#endif

    return errorCode;
}

// Remove fence(s) from GNSS instance(s).
int32_t uGnssGeofenceRemove(uDeviceHandle_t gnssHandle, uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (gnssHandle != NULL) {
            pInstance = pUGnssPrivateGetInstance(gnssHandle);
        }
        if ((pInstance != NULL) || (gpUGnssPrivateInstanceList == NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            while (pInstance != NULL) {
                errorCode = uGeofenceRemove((uGeofenceContext_t **) &pInstance->pFenceContext,
                                            pFence);
                // Next instance
                pInstance = pInstance->pNext;
                if (gnssHandle != NULL) {
                    // Just doing the one, stop there
                    pInstance = NULL;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) gnssHandle;
    (void) pFence;
#endif

    return errorCode;
}

// Associate a callback with a device.
int32_t uGnssGeofenceSetCallback(uDeviceHandle_t gnssHandle,
                                 uGeofenceTestType_t testType,
                                 bool pessimisticNotOptimistic,
                                 uGeofenceCallback_t *pCallback,
                                 void *pCallbackParam)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uGnssPrivateInstance_t *pInstance;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGeofenceSetCallback((uGeofenceContext_t **) &pInstance->pFenceContext,
                                             testType,
                                             pessimisticNotOptimistic,
                                             pCallback,
                                             pCallbackParam);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) gnssHandle;
    (void) testType;
    (void) pessimisticNotOptimistic;
    (void) pCallback;
    (void) pCallbackParam;
#endif

    return errorCode;
}

// Manually provide a position to be evaluated against the fence.
uGeofencePositionState_t uGnssGeofencePosition(uDeviceHandle_t gnssHandle,
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
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;
    uGeofencePositionState_t instancePositionState;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        if (gnssHandle != NULL) {
            pInstance = pUGnssPrivateGetInstance(gnssHandle);
        }
        while (pInstance != NULL) {
            instancePositionState = uGeofenceContextTest(gnssHandle,
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
            if (gnssHandle != NULL) {
                // Just doing the one, stop there
                pInstance = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
#else
    (void) gnssHandle;
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
