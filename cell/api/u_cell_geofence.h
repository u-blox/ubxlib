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

#ifndef _U_CELL_GEOFENCE_H_
#define _U_CELL_GEOFENCE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the functions that apply a
 * geofence, created using the common uGeofence API, to a cellular
 * device.
 *
 * This API is ONLY available if U_CFG_GEOFENCE is defined; this
 * is because it uses floating point and maths functions that would
 * otherwise cause unnecessary bloat from the C library.
 *
 * Here you will find only the functions to apply, remove and test
 * a geofence.  All of the functions which manipulate the geofence
 * can be found in /common/geofence/api.
 *
 * -----------------------------------------------------------------
 *
 * IMPORTANT: if the shapes in your geofence are less than 1 km in size
 * then a flat surface can be assumed.  For shapes larger than that,
 * if you do nothing, this code will assume a spherical earth.
 * However this can be out by, worst case, 0.5%, hence to get
 * accurate results please see u_geofence_geodesic.h in the common
 * Geofence API for the functions which must be provided to take account
 * of the non-spherial nature of the earth.
 *
 * -----------------------------------------------------------------
 *
 * To use a geofence, create one or more geofences with pUGeofenceCreate()
 * and then call uGeofenceAddCircle() and uGeofenceAddVertex() as
 * required to form the 2D perimeters of your geofence; at least one
 * circle or at least three vertices are required to form a valid
 * geofence.  You may also call uGeofenceSetAltitudeMax() and/or
 * uGeofenceSetAltitudeMin() if that is important to you.
 *
 * With the geofence set up, call uCellGeofenceSetCallback() to be informed
 * as to the state of a cellular device with respect to any geofences that
 * are applied to it, then call uCellGeofenceApply() to apply the geofence to
 * the cellular instance: from that point onwards, if a position arrives as
 * a result of any of the uCellLocXxx APIs for that instance it will be
 * evaluated against the geofence and your callback(s) may be called.
 *
 * You may also call uCellGeofencePosition() to supply a position for
 * evaluation against the geofence "manually".
 *
 * When done, call uCellGeofenceRemove() to remove the geofence from the
 * cellular instance(s) and then call uGeofenceFree() to free the memory
 * that held the geofence; there is no automatic clean-up, it is up to
 * the application to do this.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Set the maximum horzontal speed that the cellular instance can be
 * assumed to travel at in MILLIMETRES per second; if not called then
 * #U_GEOFENCE_HORIZONTAL_SPEED_MILLIMETRES_PER_SECOND_MAX will
 * be assumed.  Setting this appropriately can help to reduce
 * calculation overhead.
 *
 * @param cellHandle                    the handle of the cellular
 *                                      instance.
 * @param maxSpeedMillimetresPerSecond  the maximum horizontal speed
 *                                      in millimetres per second.
 * @return                              zero on success else negative
 *                                      error code.
 */
int32_t uCellGeofenceSetMaxSpeed(uDeviceHandle_t cellHandle,
                                 int64_t maxSpeedMillimetresPerSecond);

/** Apply the given geofence to the given cellular instance; this
 * must be called to make use of a geofence after it has been set
 * up to your liking with calls to uGeofenceAddVertex() and/or
 * uGeofenceAddCircle() etc.As many geofences as you like may
 * be applied and the same geofence may be applied to many instances.
 * You will probably also want to call uCellGeofenceSetCallback().
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[in] pFence  a pointer to the geofence to be applied;
 *                    cannot be NULL.
 * @return            zero on success else negative error code.
 */
int32_t uCellGeofenceApply(uDeviceHandle_t cellHandle,
                           uGeofence_t *pFence);

/** Remove the given geofence(s) from the given cellular instance(s).
 *
 * @param cellHandle  the handle of the cellular instance; use NULL
 *                    to remove the geofence from all cellular
 *                    instances.
 * @param[in] pFence  a pointer to the geofence to be removed; use
 *                    NULL to remove all geofences from the given
 *                    cellular instance(s).
 * @return            zero on success else negative error code.
 */
int32_t uCellGeofenceRemove(uDeviceHandle_t cellHandle,
                            uGeofence_t *pFence);

/** Set a callback to be called if a position reading arrives from
 * the cellular device that affects any geofences that have been applied.
 * There is only one callback per cellular instance, setting a new one
 * will replace the previous.  ANY position reading received because
 * of any of the uCellLocXxx APIs, or through uCellGeofencePosition(),
 * may trigger this callback; the callback will be called once for
 * each geofence attached to the cellular instance.
 *
 * IMPORTANT: don't do much in your callback!  There may be many,
 * many, calls and they should not be blocked. Also, DEFINITELY don't
 * call into the cellular API from your callback as the API may be
 * locked, you will get stuck.
 *
 * @param cellHandle               the handle of the cellular instance.
 * @param testType                 the type of callback; use
 *                                 #U_GEOFENCE_TEST_TYPE_NONE
 *                                 to remove an existing callback.
 * @param pessimisticNotOptimistic if true then the radius of position and
 *                                 uncertainty of altitude, where present,
 *                                 are taken into account pessimistically.
 *                                 For #U_GEOFENCE_TEST_TYPE_INSIDE
 *                                 this means that if the radius is
 *                                 such that the position might _not_
 *                                 be inside the geofence(s) then the
 *                                 callback will be called with
 *                                 #U_GEOFENCE_POSITION_STATE_OUTSIDE;
 *                                 for #U_GEOFENCE_TEST_TYPE_OUTSIDE
 *                                 this means that if the radius is
 *                                 such that the position might _not_
 *                                 be outside the geofence(s) then the
 *                                 callback will be called with
 *                                 #U_GEOFENCE_POSITION_STATE_INSIDE;
 *                                 for #U_GEOFENCE_TEST_TYPE_TRANSIT, if
 *                                 the radius is such that the position
 *                                 _might_ cause a transit then the
 *                                 callback will be called with the
 *                                 opposite position state to what
 *                                 went before.  Putting it another way,
 *                                 the pessimist expects the worst.
 * @param[in] pCallback            the function to be called; ignored
 *                                 if testType is #U_GEOFENCE_TEST_TYPE_NONE,
 *                                 otherwise cannot be NULL.
 * @param[in,out] pCallbackParam   parameter that will be passed to pCallback
 *                                 as its last parameter; may be NULL.
 * @return                         zero on success else negative error code.
 */
int32_t uCellGeofenceSetCallback(uDeviceHandle_t cellHandle,
                                 uGeofenceTestType_t testType,
                                 bool pessimisticNotOptimistic,
                                 uGeofenceCallback_t *pCallback,
                                 void *pCallbackParam);

/** Manually provide a position to be evaluated against the geofences
 * applied to a cellular instance; if set, the callback may be called once
 * per fence.  If you want to test a geofence with a position before
 * applying it to a cellular instance, use uGeofenceTest().
 *
 * IMPORTANT: the latitude/longitude parameters are multiplied by
 * ten to the power NINE (1e9), i.e. for a latitude of 52.1234567 you
 * would pass in the value 52,123,456,700, rather than the usual
 * ten to the power seven (1e7).
 *
 * @param cellHandle                     the handle of the cellular instance;
 *                                       NULL to send the position to
 *                                       all instances.
 * @param testType                       the type of test to perform;
 *                                       set this to
 *                                       #U_GEOFENCE_TEST_TYPE_NONE
 *                                       to just let any callbacks do their
 *                                       thing according to what you set
 *                                       for them, or set to a specific value
 *                                       to override the setting associated
 *                                       with the callbacks.
 * @param pessimisticNotOptimistic       if true then the test is pessimistic
 *                                       with respect to radiusMillimetres
 *                                       and altitudeUncertaintyMillimetres,
 *                                       else it is optimistic; see the
 *                                       description of this parameter to
 *                                       uCellGeofenceSetCallback() for more
 *                                       information; ignored if testType is
 *                                       #U_GEOFENCE_TEST_TYPE_NONE.
 * @param latitudeX1e9                   the latitude of the position to be
 *                                       checked in degrees times ten to the
 *                                       power nine.
 * @param longitudeX1e9                  the longitude of the position to be
 *                                       checked in degrees times ten to the
 *                                       power nine.
 * @param altitudeMillimetres            the altitude of the position to be
 *                                       checked in millimetres; use INT_MIN
 *                                       to express a 2D position.
 * @param radiusMillimetres              the horizontal radius of the position
 *                                       to be checked  in millimetres; -1 if
 *                                       the horizontal radius of position is
 *                                       unknown.
 * @param altitudeUncertaintyMillimetres like radiusMillimetres but vertically;
 *                                       -1 if the altitude uncertainty is
 *                                       unknown, ignored if altitudeMillimetres
 *                                       is INT_MIN.
 * @return                               the outcome of the evaluation; where
 *                                       there are multiple geofences inside ANY
 *                                       geofence will result in an "inside"
 *                                       outcome.
 */
uGeofencePositionState_t uCellGeofencePosition(uDeviceHandle_t cellHandle,
                                               uGeofenceTestType_t testType,
                                               bool pessimisticNotOptimistic,
                                               int64_t latitudeX1e9,
                                               int64_t longitudeX1e9,
                                               int32_t altitudeMillimetres,
                                               int32_t radiusMillimetres,
                                               int32_t altitudeUncertaintyMillimetres);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_GEOFENCE_H_

// End of file
