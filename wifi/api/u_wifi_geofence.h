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

#ifndef _U_WIFI_GEOFENCE_H_
#define _U_WIFI_GEOFENCE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the functions that apply a
 * geofence, created using the common uGeofence API, to wifi.
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
 * With the geofence set up, call uWifiGeofenceSetCallback() to be informed
 * as to the state of a Wi-Fi device with respect to any geofences that
 * are applied to it, then call uWifiGeofenceApply() to apply the geofence
 * to Wifi: from that point onwards, if a position arrives as a result of
 * any of the uWifiLocXxx APIs for that instance it will be evaluated
 * against the geofence and your callback(s) may be called.
 *
 * You may also call uWifiGeofencePosition() to supply a position for
 * evaluation against the geofence "manually".
 *
 * When done, call uWifiGeofenceRemove() to remove the geofence from Wifi
 * and then call uGeofenceFree() to free the memory that held the geofence;
 * there is no automatic clean-up, it is up to the application to do this.
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

/** Set the maximum horzontal speed that the Wi-Fi instance can be
 * assumed to travel at in MILLIMETRES per second; if not called then
 * #U_GEOFENCE_HORIZONTAL_SPEED_MILLIMETRES_PER_SECOND_MAX will
 * be assumed.  Setting this appropriately can help to reduce
 * calculation overhead.
 *
 * @param wifiHandle                    the handle of the Wi-Fi
 *                                      instance.
 * @param maxSpeedMillimetresPerSecond  the maximum horizontal speed
 *                                      in millimetres per second.
 * @return                              zero on success else negative
 *                                      error code.
 */
int32_t uWifiGeofenceSetMaxSpeed(uDeviceHandle_t wifiHandle,
                                 int64_t maxSpeedMillimetresPerSecond);

/** Apply the given geofence to the given Wi-Fi instance; this
 * must be called to make use of a geofence after it has been set
 * up to your liking with calls to uGeofenceAddVertex() and/or
 * uGeofenceAddCircle() etc.  As many geofences as you like may
 * be applied and the same geofence may be applied to many instances.
 * You will probably also want to call uWifiGeofenceSetCallback().
 *
 * @param wifiHandle  the handle of the Wi-Fi instance.
 * @param[in] pFence  a pointer to the geofence to be applied;
 *                    cannot be NULL.
 * @return            zero on success else negative error code.
 */
int32_t uWifiGeofenceApply(uDeviceHandle_t wifiHandle,
                           uGeofence_t *pFence);

/** Remove the given geofence(s) from the given Wi-Fi instance(s).
 *
 * @param wifiHandle  the handle of the Wi-Fi instance; use NULL
 *                    to remove the geofence from all Wi-Fi
 *                    instances.
 * @param[in] pFence  a pointer to the geofence to be removed; use
 *                    NULL to remove all geofences from the given
 *                    Wi-Fi instance(s).
 * @return            zero on success else negative error code.
 */
int32_t uWifiGeofenceRemove(uDeviceHandle_t wifiHandle,
                            uGeofence_t *pFence);

/** Set a callback to be called if a position reading arrives from
 * the Wi-Fi device that affects any geofences that have been applied.
 * There is only one callback per Wi-Fi instance, setting a new one
 * will replace the previous.  ANY position reading received because
 * of any of the uWifiLocXxx APIs, or through uWifiGeofencePosition(),
 * may trigger this callback; the callback will be called once for
 * each geofence attached to the Wi-Fi instance.
 *
 * IMPORTANT: don't do much in your callback!  There may be many,
 * many, calls and they should not be blocked. Also, DEFINITELY don't
 * call into the Wi-Fi or short-range APIs from your callback as the
 * APIs may be locked, you will get stuck.
 *
 * @param wifiHandle               the handle of the Wi-Fi instance.
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
int32_t uWifiGeofenceSetCallback(uDeviceHandle_t wifiHandle,
                                 uGeofenceTestType_t testType,
                                 bool pessimisticNotOptimistic,
                                 uGeofenceCallback_t *pCallback,
                                 void *pCallbackParam);

/** Manually provide a position to be evaluated against the geofences
 * applied to a Wi-Fi instance; if set, the callback may be called once
 * per fence.  If you want to test a geofence with a position before
 * applying it to a Wi-Fi instance, use uGeofenceTest().
 *
 * IMPORTANT: the latitude/longitude parameters are multiplied by
 * ten to the power NINE (1e9), i.e. for a latitude of 52.1234567 you
 * would pass in the value 52,123,456,700, rather than the usual
 * ten to the power seven (1e7).
 *
 * @param wifiHandle                     the handle of the Wi-Fi instance;
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
 *                                       uWifiGeofenceSetCallback() for more
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
uGeofencePositionState_t uWifiGeofencePosition(uDeviceHandle_t wifiHandle,
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

#endif // _U_WIFI_GEOFENCE_H_

// End of file
