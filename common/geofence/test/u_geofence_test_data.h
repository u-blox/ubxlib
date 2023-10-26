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

#ifndef _U_GEOFENCE_TEST_DATA_H_
#define _U_GEOFENCE_TEST_DATA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types used when testing the
 * Geofence API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GEOFENCE_TEST_FENCE_NAME
/** The name to use for a fence.
 */
# define U_GEOFENCE_TEST_FENCE_NAME "test fence"
#endif

/** A maximum latitude value, in degrees times ten to the power nine.
 */
#define U_GEOFENCE_TEST_LATITUDE_MAX_X1E9 89999999999

/** A minimum latitude value, in degrees times ten to the power nine.
 */
#define U_GEOFENCE_TEST_LATITUDE_MIN_X1E9 -U_GEOFENCE_TEST_LATITUDE_MAX_X1E9

/** A maximum longitude value, in degrees times ten to the power nine.
 */
#define U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9 179999999999

/** A minimum longitude value, in degrees times ten to the power nine.
 */
#define U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9 -U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9

#ifndef U_GEOFENCE_TEST_DATA_MAX_NUM_CIRCLES
/** The maximum number of circles in a block of test data.
 */
# define U_GEOFENCE_TEST_DATA_MAX_NUM_CIRCLES 4
#endif

#ifndef U_GEOFENCE_TEST_DATA_MAX_NUM_POLYGONS
/** The maximum number of polygons in a block of test data.
 */
# define U_GEOFENCE_TEST_DATA_MAX_NUM_POLYGONS 2
#endif

#ifndef U_GEOFENCE_TEST_DATA_MAX_NUM_VERTICES
/** The maximum number of vertices in a polygon.
 */
# define U_GEOFENCE_TEST_DATA_MAX_NUM_VERTICES 22
#endif

#ifndef U_GEOFENCE_TEST_DATA_MAX_NUM_POINTS
/** The maximum number of points to be tested against each fence.
 */
# define U_GEOFENCE_TEST_DATA_MAX_NUM_POINTS 20
#endif

#ifndef U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9
/** The latitude of the location of the test system
 * in degrees times ten to the power nine; used for
 * live testing.
 */
# define U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9 52222565519LL
#endif

#ifndef U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9
/** The longitude of the location of the test system
 * in degrees times ten to the power nine; used for
 * live testing.
 */
# define U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9 -74404134LL
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a vertex.
 */
typedef struct {
    int64_t latitudeX1e9;
    int64_t longitudeX1e9;
} uGeofenceTestVertex_t;

/** Structure to hold a circle.
 */
typedef struct {
    const uGeofenceTestVertex_t *pCentre;
    int64_t radiusMillimetres;
} uGeofenceTestCircle_t;

/** Structure to hold a polygon.
 */
typedef struct {
    size_t numVertices;
    const uGeofenceTestVertex_t *pVertex[U_GEOFENCE_TEST_DATA_MAX_NUM_VERTICES];
} uGeofenceTestPolygon_t;

/** Structure to hold a fence.
 */
typedef struct {
    const char *pName;
    int32_t altitudeMaxMillimetres; /**< INT_MAX if not present. */
    int32_t altitudeMinMillimetres; /**< INT_MIN if not present. */
    size_t numCircles;
    const uGeofenceTestCircle_t *pCircle[U_GEOFENCE_TEST_DATA_MAX_NUM_CIRCLES];
    size_t numPolygons;
    const uGeofenceTestPolygon_t *pPolygon[U_GEOFENCE_TEST_DATA_MAX_NUM_POLYGONS];
} uGeofenceTestFence_t;

/** The height and uncertainty parameters associated with a test point.
 */
typedef struct {
    int32_t radiusMillimetres;
    int32_t altitudeMillimetres; /**< INT_MIN if not present. */
    int32_t altitudeUncertaintyMillimetres;
} uGeofencePositionVariables_t;

/** A test point, with position variables and bit-map that gives the
 * expected outcome of the point being tested for all parameter
 * combinations.
 */
typedef struct {
    const uGeofenceTestVertex_t *pPosition;
    uGeofencePositionVariables_t positionVariables;
    uint8_t outcomeBitMap; /**< bits from #uGeofenceTestParameters_t. */
} uGeofenceTestPoint_t;

/** Structure to hold a geofence and the data to test it.
 */
typedef struct {
    const uGeofenceTestFence_t *pFence;
    int64_t starRadiusMillimetres; /**< used when plotting KML file data only. */
    size_t numPoints;
    const uGeofenceTestPoint_t *pPoint[U_GEOFENCE_TEST_DATA_MAX_NUM_POINTS];
} uGeofenceTestData_t;

/** The possible permutations of test parameters, values used in
 * the outcomeBitMap of #uGeofenceTestPoint_t.
 */
typedef enum {
    U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST = 0,
    U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST = 1,
    U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST = 2,
    U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST = 3,
    U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST = 4,
    U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST = 5,
    U_GEOFENCE_TEST_PARAMETERS_MAX_NUM,
    U_GEOFENCE_TEST_PARAMETERS_CALCULATION_FAILURE
} uGeofenceTestParameters_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The geofence test data.
 */
extern const uGeofenceTestData_t *gpUGeofenceTestData[];

/** Number of items in the gpUGeofenceTestData array.
 */
extern const size_t gpUGeofenceTestDataSize;

#ifdef __cplusplus
}
#endif

#endif // _U_GEOFENCE_TEST_DATA_H_

// End of file
