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

#ifndef _U_GEOFENCE_H_
#define _U_GEOFENCE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup Geofence
 *  @{
 */

/** @file
 * @brief This header file defines the functions of the ubxlib
 * Geofence API; this provides a flexible geofence implementation
 * that runs on this MCU and can be used with GNSS, cellular or
 * short-range devices.
 *
 * This API is ONLY available if U_CFG_GEOFENCE is defined; this
 * is because it uses floating point and maths funcions that would
 * otherwise cause unnecessary bloat from the C library.
 *
 * -----------------------------------------------------------------
 *
 * IMPORTANT: if the shapes in your fence are less than 1 km in size
 * then a flat surface can be assumed.  For shapes larger than that,
 * if you do nothing, this code will assume a spherical earth.
 * However this can be out by, worst case, 0.5%, hence to get
 * accurate results please see u_geofence_geodesic.h for the
 * functions which must be provided to take account of the non-spherial
 * nature of the earth.
 *
 * -----------------------------------------------------------------
 *
 * To use a geofence, create one or more fences with pUGeofenceCreate()
 * and then call uGeofenceAddCircle() and uGeofenceAddVertex() as
 * required to form the 2D perimeters of your fence; at least one
 * circle or at least three vertices are required to form a valid
 * fence.  You may also call uGeofenceSetAltitudeMax() and/or
 * uGeofenceSetAltitudeMin() if that is important to you.
 *
 * With the fence set up, call uGnssGeofenceSetCallback(),
 * uCellGeofenceSetCallback() or uWifiGeofenceSetCallback() to be
 * informed as to the state of a GNSS, cellular or Wifi device with
 * respect to any geofences that are applied to it, then call
 * uGnssGeofenceApply(), uCellGeofenceApply()  or uWifiGeofenceApply()
 * to apply the fence to the device: from that point onwards, for a GNSS
 * device, if a position arrives as a result of any of the uGnssPosXxx
 * APIs for that device it will be evaluated against the geofence
 * and your callback(s) may be called; similarly, for a cellular device
 * if a position arrives as a result of a uCellLocXxx API it will be
 * evaluated against the geofence, or for a Wifi device if a position
 * arrives as a result of a uWifiPosXxx API it will be evaluated against
 * the geofence.
 *
 * You may also call uGnssGeofencePosition(), uCellGeofencePosition()
 * or uWifiGeofencePosition() to supply a position for evaluation against
 * the fence "manually".
 *
 * When done, call uGnssGeofenceRemove(), uCellGeofenceRemove() or
 * uWifiGeofenceRemove() to remove the fence from the device and then call
 * uGeofenceFree() to free the memory that held the geofence; there is
 * no automatic clean-up, it is up to the application to do this.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GEOFENCE_WGS84_THRESHOLD_METRES
/** The threshold at which an attempt will be made to use WGS84
 * coordinates, where available, in order to take into account the
 * true shape of the earth; below this size, calculations can
 * safely be performed in a flat X/Y space provided the shape
 * is more than #U_GEOFENCE_WGS84_THRESHOLD_POLE_DEGREES_FLOAT
 * from a pole.
 */
# define U_GEOFENCE_WGS84_THRESHOLD_METRES 1000
#endif

#ifndef U_GEOFENCE_WGS84_THRESHOLD_POLE_DEGREES_FLOAT
/** The distance from the pole, in degrees of longitude, under
 * which WGS84 coordinates _must_ be used, even for distances
 * #U_GEOFENCE_WGS84_THRESHOLD_METRES or less; flat X/Y just
 * does not work. 10 degrees at a complete guess.
 */
# define U_GEOFENCE_WGS84_THRESHOLD_POLE_DEGREES_FLOAT 10
#endif

#ifndef U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES
/** For each shape, a "square extent" is stored, to which this
 * is added as an uncertainty margin.  Provided the radius of
 * position (i.e. the uncertainty of the position) is less than
 * this amount, shapes can be discarded from a position test with
 * a _very_ quick latitude/longitude difference check.  If the
 * radius of position is larger then no such quick check can be
 * reliably performed.  Conversely, once a position is within this
 * distance of any shape in the geofence a full, computationally
 * intensive, check must be made, but provided the shape is less
 * than #U_GEOFENCE_WGS84_THRESHOLD_METRES in size, because the
 * two are so close, the calculations can be done in X/Y space,
 * removing the need for trigonometry.
 *
 * See also #U_GEOFENCE_WGS84_THRESHOLD_METRES and
 * #U_GEOFENCE_WGS84_THRESHOLD_POLE_DEGREES_FLOAT for additional
 * conditions.
 */
# define U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES 100
#endif

#ifndef U_GEOFENCE_HORIZONTAL_SPEED_MILLIMETRES_PER_SECOND_MAX
/** The maximum horizontal speed that anything is expected to
 * travel at in MILLIMETRES per second.
 */
# define U_GEOFENCE_HORIZONTAL_SPEED_MILLIMETRES_PER_SECOND_MAX 500000LL
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The type of test that may be requested for a geofence.
 */
typedef enum {
    U_GEOFENCE_TEST_TYPE_NONE,
    U_GEOFENCE_TEST_TYPE_INSIDE,  /**< the test is true if a position
                                       reading is inside a geofence.
                                       "inside" means inside a
                                       polygon formed by calls to
                                       uGeofenceAddVertex() or
                                       inside a circle added
                                       with uGeofenceAddCircle(),
                                       taking into account the
                                       uGeofenceSetAltitudeMin()
                                       and uGeofenceSetAltitudeMax()
                                       of the geofence. */
    U_GEOFENCE_TEST_TYPE_OUTSIDE, /**< the test is true if a position
                                       reading is outside a geofence.
                                       "outside" means outside all
                                       polygons formed by calls to
                                       uGeofenceAddVertex() and
                                       outside any circles added
                                       with uGeofenceAddCircle(),
                                       taking into account the
                                       uGeofenceSetAltitudeMin()
                                       and uGeofenceSetAltitudeMax()
                                       of the geofence. */
    U_GEOFENCE_TEST_TYPE_TRANSIT, /**< the test is true if a position
                                       has moved from being inside a
                                       geofence to being outside it or
                                       vice-versa.  There is no
                                       hysteresis; many calls to a
                                       callback may be made as a
                                       position transits a geofence if
                                       you have pessimism set.
                                       IMPORTANT: transit tests do not
                                       work if the shapes in your geofence
                                       overlap, since a transit will be
                                       detected at each shape edge and
                                       those edges now may be INSIDE
                                       another shape in your geofence. */
    U_GEOFENCE_TEST_TYPE_MAX_NUM
} uGeofenceTestType_t;

/** The state of a position with respect to the shapes in a geofence.
 *
 * You might think that the answer is Boolean, in or out.  However,
 * limitations in the accuracy of even double-precision variables,
 * and cos()/sin()/tan() etc. trigonometry with such variables, when
 * dealing with angles subtended at the centre of the earth, 6.4 thousand
 * km away and small distances apart (nine decimal digits past the decimal
 * point), can bring in rounding errors which mean that the calculations
 * fail (e.g. trying to take the cos() of a value just over 1).  Under
 * these circumstances #U_GEOFENCE_POSITION_STATE_NONE will be returned
 * and the result should be ignored, as if the position measurement had
 * not been made.
 */
typedef enum {
    U_GEOFENCE_POSITION_STATE_NONE = 0,
    U_GEOFENCE_POSITION_STATE_INSIDE,
    U_GEOFENCE_POSITION_STATE_OUTSIDE
} uGeofencePositionState_t;

/** Callback that may be called if a position is inside/outside/transiting
 * a geofence.
 *
 * @param devHandle                      the handle of the device.
 * @param[in] pFence                     the geofence that is being referred
 *                                       to, passed as a void * to avoid
 *                                       forward declaration shennanigans.
 * @param[in] pNameStr                   the name of the geofence that was
 *                                       checked against; NULL if the geofence
 *                                       was not named.
 * @param positionState                  the outcome of the check against
 *                                       the geofence, taking into acount radius
 *                                       of position and altitude uncertainty,
 *                                       where present; if
 *                                       #U_GEOFENCE_POSITION_STATE_NONE then
 *                                       a check has been made but limitations
 *                                       in the maths means that a clear
 *                                       determination could not be made
 *                                       and the result can be ignored.
 * @param latitudeX1e9                   the latitude of the position that
 *                                       caused the geofence event in
 *                                       degrees times ten to the power nine.
 * @param longitudeX1e9                  the longitude of the position that
 *                                       caused the geofence event in
 *                                       degrees times ten to the power nine.
 * @param altitudeMillimetres            the altitude of the position that
 *                                       caused the geofence event in
 *                                       millimetres; INT_MIN if only a 2D
 *                                       position.
 * @param radiusMillimetres              radius of position in millimetres,
 *                                       -1 if the radius of position as not
 *                                       known.
 * @param altitudeUncertaintyMillimetres like radiusMillimetres but vertically;
 *                                       -1 if the altitude uncertainty was
 *                                       not known, should be ignored if
 *                                       altitudeMillimetres as INT_MIN.
 * @param distanceMillimetres            the shortest horizontal distance from
 *                                       the position to the edge of the fence
 *                                       in millimetres, zero if the position
 *                                       is inside the fence.  Since deriving
 *                                       this is a computationally intensive
 *                                       operation the value is ONLY POPULATED
 *                                       if the check requires it; should it
 *                                       be possible to complete the check
 *                                       without calculating the distance
 *                                       this will be LLONG_MIN, which should
 *                                       be interpreted as meaning "not
 *                                       calculated".
 * @param[in,out] pCallbackParam         the pCallbackParam pointer that
 *                                       was passed to uGnssFenceSetCallback(),
 *                                       uCellFenceSetCallback() or
 *                                       uWifiFenceSetCallback().
 */
typedef void (uGeofenceCallback_t)(uDeviceHandle_t devHandle,
                                   const void *pFence,
                                   const char *pNameStr,
                                   uGeofencePositionState_t positionState,
                                   int64_t latitudeX1e9,
                                   int64_t longitudeX1e9,
                                   int32_t altitudeMillimetres,
                                   int32_t radiusMillimetres,
                                   int32_t altitudeUncertaintyMillimetres,
                                   int64_t distanceMillimetres,
                                   void *pCallbackParam);

/* ----------------------------------------------------------------
 * PRIVATE TYPES
 * -------------------------------------------------------------- */

/** A geofence: this type is used internally by this code to hold
 * a geofence and is exposed here only so that it can be handed around
 * by the caller.  The contents and, umm, structure of this structure
 * may be changed without notice and should not be relied upon
 * by the caller; please use the functions pUGeofenceCreate(),
 * uGeofenceAddVertex(), uGeofenceAddCircle() etc. to create and
 * populate your fence.
 */
typedef struct {
    const char *pNameStr;
    int32_t referenceCount;
    uLinkedList_t *pShapes;  /**< a linked-list containing uGeofenceShape_t. */
    int32_t altitudeMillimetresMax; /**< INT_MAX for not present. */
    int32_t altitudeMillimetresMin; /**< INT_MIN for not present. */
    uGeofencePositionState_t positionState; /**< purely to allow a
                                                 transit-type test
                                                 when this geofence is
                                                 not attached to a
                                                 GNSS device; cannot
                                                 be used to hold
                                                 device state since
                                                 a geofence can be
                                                 applied to more than
                                                 one device. */
    int64_t distanceMinMillimetres; /**< purely for use when testing. */
} uGeofence_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a new geofence.  Once created the geofence should be
 * populated through multiple calls to uGeofenceAddVertex(),
 * uGeofenceAddCircle(), and potentially uGeofenceSetAltitudeMax()
 * and uGeofenceSetAltitudeMin(), before calling one of
 * uGnssGeofenceApply(), uCellGeofenceApply() or uWifiGeofenceApply()
 * to apply it to a device.
 *
 * Note: it is up to the application to remove from use and then free
 * all geofences, they are not automatically cleared-up.
 *
 * @param[in] pNameStr  an optional null-terminated name for the
 *                      geofence; MUST be a true constant, the string
 *                      is NOT copied by this code.  May be NULL.
 * @return              a pointer to the geofence, NULL on error.
 */
uGeofence_t *pUGeofenceCreate(const char *pNameStr);

/** Free a geofence that was created by pUGeofenceCreate(),
 * releasing all memory it occupied.  If the geofence is currently
 * applied to a device an error will be returned; use one of
 * uGnssGeofenceRemove(), uCellGeofenceRemove() or
 * uWifiGeofenceRemove() to remove it from any devices first.
 *
 * @param[in] pFence a pointer to the fence to free.
 * @return           zero on success else negative error code.
 */
int32_t uGeofenceFree(uGeofence_t *pFence);

/** Add a circle to a geofence; any number of circles may be added,
 * subject only to heap memory constraints, though obviously the
 * more circles that have to be checked for a device, the more
 * processing time that will require and the more circles that
 * are larger than #U_GEOFENCE_WGS84_THRESHOLD_METRES the worse
 * the computational load will be.
 *
 * If the radius of the circle is larger than
 * #U_GEOFENCE_WGS84_THRESHOLD_METRES this code will try to use
 * implementations of the functions
 * uGeofenceWgs84GeodInverse() and uGeofenceWgs84GeodDirect(),
 * if available, so that the true shape of the earth is taken into
 * account.  Either follow the instructions in
 * u_geofence_geodesic.h to employ https://github.com/geographiclib,
 * or provide implementations of the function yourself.  If these
 * functions are not available a spherical earth will be assumed.
 *
 * The geofence does not become active until one of
 * uGnssGeofenceApply(), uCellGeofenceApply() or uWifiGeofenceApply()
 * is called; once a geofence is applied to one or more devices
 * uGeofenceAddCircle() cannot be called on it again until the
 * geofence is removed from those instances by a call to one of
 * uGnssGeofenceRemove(), uCellGeofenceRemove() or
 * uWifiGeofenceRemove().
 *
 * IMPORTANT: the latitude/longitude parameters are multiplied by
 * ten to the power NINE (1e9), i.e. for a latitude of 52.1234567 you
 * would pass in the value 52,123,456,700, rather than the usual
 * ten to the power seven (1e7); this is so that you have the option
 * of obtaining high precision position from a HPG GNSS device using
 * the uGnssMsg and uGnssDec APIs and checking that against the fences
 * at high precision with a call to uGnssGeofencePosition().  Otherwise,
 * within ubxlib, only standard precision is currently used.
 *
 * @param[in] pFence          a pointer to the geofence to add the circle
 *                            to; cannot be NULL.
 * @param latitudeX1e9        the latitude of the centre of the circle
 *                            in degrees times ten to the power nine.
 * @param longitudeX1e9       the longitude of the centre of the circle
 *                            in degrees times ten to the power nine.
 * @param radiusMillimetres   radius of the circle in millimetres.
 * @return                    zero on success else negative error code.
 */
int32_t uGeofenceAddCircle(uGeofence_t *pFence,
                           int64_t latitudeX1e9, int64_t longitudeX1e9,
                           int64_t radiusMillimetres);

/** Add a vertex to a geofence.  At least three vertices must be
 * added, with repeated calls to this function, to make a valid polygon;
 * if you call uGeofenceAddCircle() instead then the current polygon
 * will be assumed to be finished.  The number of vertices is limited
 * only by available heap memory, though obviously the more sides that
 * have to be checked for a device, the more processing time that will
 * require.  Polygons are considerably more computationally intensive
 * to check than circles and polygons with sides larger than
 * #U_GEOFENCE_WGS84_THRESHOLD_METRES are the most computationally
 * intensive to check of all.
 *
 * If a vertex is such that the square extent of the shape it is making
 * is more than #U_GEOFENCE_WGS84_THRESHOLD_METRES on a side then
 * this code will try to use implementations of the functions
 * uGeofenceWgs84LatitudeOfIntersection() and
 * uGeofenceWgs84DistanceToSegment() in order that the true shape of
 * the earth is taken into account.  Either follow the instructions
 * in u_geofence_geodesic.h to employ https://github.com/geographiclib,
 * or provide the implementations of the function yourself.  If these
 * functions are not available a spherical earth will be assumed.
 *
 * Vertices are added in the order this function is called, e.g. adding
 * five vertices, 0 to 4, in one polygon might result in this shape:
 *
 * ```
 *            0
 *  3       /   \
 *  \ \    /     \
 *   \ \  /       \
 *    \  4         \
 *     \            \
 *      2 ---------- 1
 * ```
 *
 * The geofence does not become active until one of
 * uGnssGeofenceApply(), uCellGeofenceApply() or uWifiGeofenceApply()
 * is called; once a geofence is applied to one or more devices
 * uGeofenceAddCircle() cannot be called on it again until the
 * geofence is removed from those instances by a call to
 * uGnssGeofenceRemove(), uCellGeofenceRemove() or
 * uWifiGeofenceRemove().
 *
 * IMPORTANT: the latitude/longitude parameters are multiplied by
 * ten to the power NINE (1e9), i.e. for a latitude of 52.1234567 you
 * would pass in the value 52,123,456,700, rather than the usual
 * ten to the power seven (1e7); this is so that you have the option
 * of obtaining high precision position from a HPG GNSS device using
 * the uGnssMsg and uGnssDec APIs and checking that against the
 * geofences at high precision with a call to uGnssGeofencePosition().
 * Otherwise, within ubxlib, only standard precision is currently
 * used.
 *
 * @param[in] pFence    a pointer to the geofence to add the vertex
 *                      to; cannot be NULL.
 * @param latitudeX1e9  the latitude of the vertex in degrees times
 *                      ten to the power nine.
 * @param longitudeX1e9 the longitude of the vertex in degrees times
 *                      ten to the power nine.
 * @param newPolygon    if true, this is the first vertex of a new
 *                      polygon, else this is the next vertex of an
 *                      existing polygon; ignored on the first call
 *                      to this function or the first call to this
 *                      function after uGeofenceClearMap().
 * @return              zero on success else negative error code.
 */
int32_t uGeofenceAddVertex(uGeofence_t *pFence,
                           int64_t latitudeX1e9, int64_t longitudeX1e9,
                           bool newPolygon);

/** Set the maximum altitude of a geofence; if this is not called there
 * is no maximum altitude.  If the geofence is currently applied to any
 * devices an error will be returned; call uGeofenceRemove() to remove
 * it from those devices first.
 *
 * IMPORTANT: if a maximum altitude is set but only 2D position is
 * achieved then that position will be IGNORED for this geofence; do
 * not set a maximum altitude if you want 2D positions to be checked
 * against your geofence.
 *
 * @param[in] pFence          a pointer to the geofence where the maximum
 *                            altitude is to apply; cannot be NULL.
 * @param altitudeMillimetres the maximum altitude of the geofence in
 *                            millimetres; use INT_MAX to remove
 *                            a previous maximum altitude.
 * @return                    zero on success else negative error code.
 */
int32_t uGeofenceSetAltitudeMax(uGeofence_t *pFence,
                                int32_t altitudeMillimetres);

/** Set the minimum altitude of a geofence; if this is not called there
 * is no minimum altitude.  If the geofence is currently applied to any
 * devices an error will be returned; call uGnssGeofenceRemove(),
 * uCellGeofenceRemove() or uWifiGeofenceRemove() to remove it from
 * those devices first.
 *
 * IMPORTANT: if a minimum altitude is set but only 2D position is
 * achieved then that position will be IGNORED for this geofence; do
 * not set a minimum altitude if you want 2D positions to be checked
 * against your geofence.
 *
 * @param[in] pFence          a pointer to the geofence where the minimum
 *                            altitude is to apply; cannot be NULL.
 * @param altitudeMillimetres the minimum altitude of the geofence in
 *                            millimetres; use INT_MIN to remove
 *                            a previous minumum altitude.
 * @return                    zero on success else negative error code.
 */
int32_t uGeofenceSetAltitudeMin(uGeofence_t *pFence,
                                int32_t altitudeMillimetres);

/** Clear all objects from a geofence: all vertices, all circles and
 * any minimum or maximum altitude will be cleared from the geofence;
 * you have a clean sheet.  This does NOT free the geofence, you must
 * do that with a call to uGeofenceFree() when you have finished with
 * it; you do not need to call uGeofenceClearMap() before calling
 * uGeofenceFree().  If the geofence is currently applied to a device
 * an error will be returned; call uGnssGeofenceRemove(),
 * uCellGeofenceRemove() or uWifiGeofenceRemove() to remove it from
 * those devices first.
 *
 * @param[in] pFence a pointer to the geofence to be cleared.
 * @return           zero on success else negative error code.
 */
int32_t uGeofenceClearMap(uGeofence_t *pFence);

/** Test a position against a geofence.  This will not cause
 * any callbacks to be called, it is simply a local test of the
 * geofence.
 *
 * Note: if a maximum or minimum altitude is set and altitudeMillimetres
 * is INT_MIN (i.e. not present) then false will be returned.
 *
 * IMPORTANT: the latitude/longitude parameters are multiplied by
 * ten to the power NINE (1e9), i.e. for a latitude of 52.1234567 you
 * would pass in the value 52,123,456,700, rather than the usual
 * ten to the power seven (1e7); this is so that you have the option
 * of obtaining high precision position from a HPG GNSS device using
 * the uGnssMsg and uGnssDec APIs and checking that against the
 * geofences at high precision.
 *
 * @param[in] pFence                     a pointer to the geofence to test;
 *                                       cannot be NULL.
 * @param testType                       the type of test to perform.
 * @param pessimisticNotOptimistic       if true then the test is pessimistic
 *                                       with respect to radiusMillimetres
 *                                       and altitudeUncertaintyMillimetres,
 *                                       else it is optimistic; see the
 *                                       description of this parameter to
 *                                       uGnssGeofenceSetCallback(),
 *                                       uCellGeofenceSetCallback() or
 *                                       uWifiGeofenceSetCallback() for more
 *                                       information.
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
 *                                       unknown, ignored if altitudeMillimetres
 *                                       is INT_MIN.
 * @return                               true if the test is met, false if it is
 *                                       not, in all cases taking into account
 *                                       the radius of position and the altitude
 *                                       uncertainty.
 */
bool uGeofenceTest(uGeofence_t *pFence, uGeofenceTestType_t testType,
                   bool pessimisticNotOptimistic,
                   int64_t latitudeX1e9,
                   int64_t longitudeX1e9,
                   int32_t altitudeMillimetres,
                   int32_t radiusMillimetres,
                   int32_t altitudeUncertaintyMillimetres);

/** When any function of the Geofence API is called it will ensure that
 * a mutex, used for thread-safety, has been created.  This mutex is
 * not intended to be free'd, ever.  However, if you are quite
 * finished with the Geofence API, no fence is in use etc. you may
 * call this function to free the mutex and get that memory back.
 * There is no harm in calling a Geofence API function again after
 * this, it will simply recreate the mutex.
 */
void uGeofenceCleanUp();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GEOFENCE_H_

// End of file
