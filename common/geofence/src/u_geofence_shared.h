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

#ifndef _U_GEOFENCE_SHARED_H_
#define _U_GEOFENCE_SHARED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines the functions of Geofence
 * that may be needed by the GNSS, cellular or Wi-Fi APIs,
 * i.e. for use only internally within ubxlib.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold the distance and timestamp part of the
 * dynamic, kept separately as some underlying layers need to
 * cache it.
 */
typedef struct {
    int64_t distanceMillimetres; /**< use LLONG_MIN to mean "not known". */
    int32_t timeMs; /**< populated from uPortGetTickTimeMs(). */
} uGeofenceDynamicStatus_t;

/** Structure to hold the maximum speed that a device will travel
 * at and its last known distance from the fence.  This may be
 * populated in #uGeofenceContext_t in order to allow distant
 * fences to be discarded from checking quickly.
 */
typedef struct {
    int32_t maxHorizontalSpeedMillimetresPerSecond; /**< -1 if not known. */
    uGeofenceDynamicStatus_t lastStatus;
} uGeofenceDynamic_t;

/** Context for a geofence, may be associated with a device.
 */
typedef struct {
    uLinkedList_t *pFences; /**< a linked list containing #uGeofence_t. */
    uGeofencePositionState_t positionState;
    uGeofenceCallback_t *pCallback;
    void *pCallbackParam;
    uGeofenceTestType_t testType;
    bool pessimisticNotOptimistic;
    uGeofenceDynamic_t dynamic;
} uGeofenceContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Ensure that a geofence context exists, creaing it with defaults
 * applied if it does not.  This may be called by the uXxxGeofence
 * APIs.
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] ppFenceContext  a pointer to the pointer to where
 *                            the geofence context should be; cannot
 *                            be NULL.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uGeofenceContextEnsure(uGeofenceContext_t **ppFenceContext);

/** Unlink all geofences from a geofence context and then free the
 * context; does NOT free the geofences, just unlinks them.  This
 * may be called by the uXxxGeofence APIs.
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] ppFenceContext  a pointer to the pointer to the geofence
 *                            context that is to be free'd.  This
 *                            pointer will be set to NULL when done.
 */
void uGeofenceContextFree(uGeofenceContext_t **ppFenceContext);

/** Apply the given geofence to the given geofence context; this
 * may be called by uGnssGeofenceApply(), uCellGeofenceApply() or
 * uWifiGeofenceApply().
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] ppFenceContext  a pointer to the pointer to the
 *                            geofence context where the fence is
 *                            to be applied; cannot be NULL.
 * @param[in] pFence          a pointer to the fence to apply;
 *                            cannot be NULL.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uGeofenceApply(uGeofenceContext_t **ppFenceContext,
                       uGeofence_t *pFence);

/** Remove the given geofence(s) from the given geofence context;
 * this may be called by uGnssGeofenceRemove(), uCellGeofenceRemove()
 * or uWifiGeofenceRemove().
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] ppFenceContext  a pointer to the pointer to the
 *                            geofence context that the fence is
 *                            to be removed from; cannot be NULL.
 * @param[in] pFence          a pointer to the geofence to be removed;
 *                            use NULL to remove all geofences from
 *                            the given context.
 * @return                    zero on success else negative error code.
 */
int32_t uGeofenceRemove(uGeofenceContext_t **ppFenceContext,
                        uGeofence_t *pFence);

/** Apply a callback to the given geofence context; this may be called
 * by uGnssGeofenceSetCallback(), uCellGeofenceSetCallback() or
 * uWifiGeofenceSetCallback().  When uGeofenceContextTest() is called,
 * this callback will be called once for each geofence attached to the
 * context.
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] ppFenceContext       a pointer to the pointer to the
 *                                 geofence context that the callback
 *                                 is to be applied to.
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
 *                                 if testType is
 *                                 #U_GEOFENCE_TEST_TYPE_NONE,
 *                                 otherwise cannot be NULL.
 * @param[in,out] pCallbackParam   parameter that will be passed to pCallback
 *                                 as its last parameter; may be NULL.
 * @return                         zero on success else negative error code.
 */
int32_t uGeofenceSetCallback(uGeofenceContext_t **ppFenceContext,
                             uGeofenceTestType_t testType,
                             bool pessimisticNotOptimistic,
                             uGeofenceCallback_t *pCallback,
                             void *pCallbackParam);

/** Test a position against the geofences of a context; may be called by
 * the uXxxGeofence APIs.
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param devHandle                      the device handle, required if
 *                                       the callback is to be called;
 *                                       use NULL if you do not want the
 *                                       callback to be called.
 * @param[in] pFenceContext              a pointer to the geofence context
 *                                       containing the fences to test
 *                                       against; cannot be NULL.
 * @param testType                       the type of test to perform;
 *                                       set this to
 *                                       #U_GEOFENCE_TEST_TYPE_NONE
 *                                       to just let any callbacks do their
 *                                       thing according to what you set
 *                                       for them, or set to a specific value
 *                                       to override the setting associated
 *                                       with the callbacks.
 * @param pessimisticNotOptimistic       if true then the test is optimistic
 *                                       with respect to radiusMillimetres
 *                                       and altitudeUncertaintyMillimetres,
 *                                       else it is pessimistic; see the
 *                                       description of this parameter to
 *                                       uGnssGeofenceSetCallback() for more
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
 * @param radiusMillimetres              the radius of the position to be
 *                                       checked in millimetres; -1 if the
 *                                       radius of position is unknown.
 * @param altitudeUncertaintyMillimetres like radiusMillimetres but vertically;
 *                                       -1 if the altitude uncertainty is
 *                                       unknown.
 * @return                               the outcome of the test, taking into
 *                                       account the radius of position and the
 *                                       altitude uncertainty.
 */
uGeofencePositionState_t uGeofenceContextTest(uDeviceHandle_t devHandle,
                                              uGeofenceContext_t *pFenceContext,
                                              uGeofenceTestType_t testType,
                                              bool pessimisticNotOptimistic,
                                              int64_t latitudeX1e9,
                                              int64_t longitudeX1e9,
                                              int32_t altitudeMillimetres,
                                              int32_t radiusMillimetres,
                                              int32_t altitudeUncertaintyMillimetres);

/** Used only when testing: resets the previous position remembered by
 * a geofence; use this when testing transitions to get everything back
 * to a starting state.
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] pFence  a pointer to the geofence to reset the memory of;
 *                    cannot be NULL.
 */
void uGeofenceTestResetMemory(uGeofence_t *pFence);

/** Used only when testing: the last position state of the geofence,
 * the last outcome of uGeofenceContextTest().
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] pFence  a pointer to the geofence; cannot be NULL.
 * @return            the position state.
 */
uGeofencePositionState_t uGeofenceTestGetPositionState(const uGeofence_t *pFence);

/** Used only when testing: get the last minimum distance to the
 * geofence that was calculated by uGeofenceContextTest().
 *
 * Note: the relevant API mutex, e.g. gMutex if called from within
 * the Geofence API, gUGnssPrivateMutex if called from within the
 * GNSS API, etc., must be locked before this is called.
 *
 * @param[in] pFence  a pointer to the geofence; cannot be NULL.
 * @return            the distance in millimetres, LLONG_MIN if
 *                    the distance was not calculated.
 */
int64_t uGeofenceTestGetDistanceMin(const uGeofence_t *pFence);

#ifdef __cplusplus
}
#endif

#endif // _U_GEOFENCE_SHARED_H_

// End of file
