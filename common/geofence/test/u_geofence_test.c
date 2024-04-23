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
 * @brief Tests for the Geofence API: if U_CFG_GEOFENCE is defined
 * defined these tests should pass on all platforms, the tests do
 * not need a module of any type to be connected.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#ifdef U_CFG_GEOFENCE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "limits.h"    // INT_MAX, INT_MIN, LLONG_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf(), fprintf()
#include "string.h"    // strlen(), memcpy()
#include "ctype.h"     // tolower(), isalnum(), isblank()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_timeout.h"

#include "u_linked_list.h"

#include "u_port_clib_platform_specific.h" /* must be included before the other
                                              port files if any print or scan
                                              function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_geofence_test_data.h"

#ifdef _WIN32
#include "u_geofence_test_kml_doc.h"
#include "windows.h"
#include "math.h"     // sqrt(), cos(), etc.
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_GEOFENCE_TEST"

/** The string to put at the start of all prints from this test
 * that do not require any iterations on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * kmlFile, no iteration(s) version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an identifier is required on the end.
 */
#define U_TEST_PREFIX_A U_TEST_PREFIX_BASE "_%c: "

/** Print a whole line, with terminator and an identifier on the end,
 * prefixed for this test kmlFile.
 */
#define U_TEST_PRINT_LINE_A(format, ...) uPortLog(U_TEST_PREFIX_A format "\n", ##__VA_ARGS__)

#ifndef U_GEOFENCE_TEST_STAR_RAYS
/** When plotting KML files, plot this many rays in the star emitted
 * from each test point.
 */
# define U_GEOFENCE_TEST_STAR_RAYS 16
#endif

#ifndef U_GEOFENCE_TEST_STAR_POINTS_PER_RAY
/** When plotting KML files, plot this many points on each ray
 * from a test point.
 */
# define U_GEOFENCE_TEST_STAR_POINTS_PER_RAY 16
#endif

#ifdef _WIN32
/** The radius of a spherical earth in metres.
 */
#define U_GEOFENCE_TEST_RADIUS_AT_EQUATOR_METERS 6378100

/** Pi as a float.
 */
# define U_GEOFENCE_TEST_PI_FLOAT 3.14159265358
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifdef _WIN32
/** Structure to hold a coordinate that we will plot in a KML file.
 */
typedef struct {
    const char *pStyleMap;
    uGeofenceTestVertex_t vertex;
    int32_t radiusMillimetres;
    int32_t altitudeMillimetres;
} uGeofenceTestKmlCoordinate_t;

/** Structure to hold a star with its rays and the coordinates
 * along each ray.
 */
typedef struct {
    uGeofenceTestKmlCoordinate_t star[U_GEOFENCE_TEST_STAR_RAYS][U_GEOFENCE_TEST_STAR_POINTS_PER_RAY];
} uGeofenceTestKmlStar_t;

/** Structure to hold a set of stars for a given test point, one
 * star for each combination of test parameters.
 */
typedef struct {
    const uGeofenceTestPoint_t *pTestPoint;
    uGeofenceTestKmlStar_t stars[U_GEOFENCE_TEST_PARAMETERS_MAX_NUM];
} uGeofenceTestKmlStarSet_t;
#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A geofence.
 */
static uGeofence_t *gpFence = NULL;

/** String to print for each test type.
 */
static const char *gpTestTypeString[] = {"none", "in", "out", "transit"};

/** String to print for each position state.
 */
static const char *gpPositionStateString[] = {"none", "inside", "outside"};

/** The parameter combinations to test for every point; order is
 * important, must match and have the same number of elements
 * as gTestType and gPessimisticNotOptimistic.
 */
static const uGeofenceTestParameters_t gTestParameters[] = {
    U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST,
    U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST,
    U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST,
    U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST,
    U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST,
    U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST
};

/** The value for testType to match gTestParameters.
 */
static const uGeofenceTestType_t gTestType[] = {
    U_GEOFENCE_TEST_TYPE_INSIDE, U_GEOFENCE_TEST_TYPE_INSIDE,
    U_GEOFENCE_TEST_TYPE_OUTSIDE, U_GEOFENCE_TEST_TYPE_OUTSIDE,
    U_GEOFENCE_TEST_TYPE_TRANSIT, U_GEOFENCE_TEST_TYPE_TRANSIT,
};

/** The value for pessimisticNotOptimistic to match gTestParameters.
 */
static const bool gPessimisticNotOptimistic[] = {
    true, false, true, false, true, false
};

#ifdef _WIN32
/** File handle, used for writing KML files on Windows.
 */
static HANDLE gKmlFile = INVALID_HANDLE_VALUE;

/** KML style stings for each position state.
 */
static const char *gpKmlStyleMap[] = {
    U_GEOFENCE_TEST_KML_STYLE_MAP_ID_NONE,
    U_GEOFENCE_TEST_KML_STYLE_MAP_ID_INSIDE,
    U_GEOFENCE_TEST_KML_STYLE_MAP_ID_OUTSIDE
};

/** All of the test data that we will plot to the KML file
 * for a given fence.
 */
static uGeofenceTestKmlStarSet_t *gpKmlStarSet[U_GEOFENCE_TEST_DATA_MAX_NUM_POINTS] = {0};

/** The number of entries of gpKmlStarSet populated.
 */
static size_t gKmlNumStarSets = 0;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert a lat/long into a whole number and a bit-after-the-decimal-point
// that can be printed by a version of printf() that does not support
// floating point operations or 64 bit numbers, returning the prefix
// (either "+" or "-") and the fractional part in two halves.  The result
// should be printed with printf() format specifiers %c%d.%06d%03d,
// e.g. something like:
//
// int32_t whole;
// int32_t fractionUpper;
// int32_t fractionLower;
//
// printf("%c%d.%06d%03d/%c%d.%06d%03d", latLongToBits(latitudeX1e9, &whole,
//                                                     &fractionUpper,
//                                                     &fractionLower),
//                               whole, fractionUpper, fractionLower,
//                               latLongToBits(longitudeX1e9, &whole,
//                                             &fractionUpper,
//                                             &fractionLower),
//                               whole, fractionUpper, fractionLower);
static char latLongToBits(int64_t thingX1e9,
                          int32_t *pWhole,
                          int32_t *pFractionUpper,
                          int32_t *pFractionLower)
{
    char prefix = '+';
    int64_t fraction;

    // Deal with the sign
    if (thingX1e9 < 0) {
        thingX1e9 = -thingX1e9;
        prefix = '-';
    }
    *pWhole = (int32_t) (thingX1e9 / 1000000000);
    fraction = thingX1e9 % 1000000000;
    *pFractionUpper = (int32_t) (fraction / 1000);
    *pFractionLower = (int32_t) (fraction % 1000);

    return prefix;
}

// Print out a test fence, at the top level anyway.
static void printTestFence(const char *pPrefix,
                           const uGeofenceTestFence_t *pTestFence)
{
    const char *pName = pTestFence->pName;
    char altitudeBuffer[42] = "2D";
    int32_t x = 0;

    if (pPrefix == NULL) {
        pPrefix = "";
    }
    if (pName == NULL) {
        pName = "<no name>";
    }
    if (pTestFence->altitudeMaxMillimetres != INT_MAX) {
        x = snprintf(altitudeBuffer, sizeof(altitudeBuffer), "%d.%03d m high",
                     (int) (pTestFence->altitudeMaxMillimetres / 1000),
                     (int) (pTestFence->altitudeMaxMillimetres % 1000));
        if ((pTestFence->altitudeMinMillimetres != INT_MIN) && (x < sizeof(altitudeBuffer) - 2)) {
            altitudeBuffer[x] = ',';
            x++;
            altitudeBuffer[x] = ' ';
            x++;
        }
    }
    if ((pTestFence->altitudeMinMillimetres != INT_MIN) && (x < sizeof(altitudeBuffer))) {
        snprintf(altitudeBuffer + x, sizeof(altitudeBuffer) - x, "%d.%03d m base",
                 (int) (pTestFence->altitudeMinMillimetres / 1000),
                 (int) (pTestFence->altitudeMinMillimetres % 1000));
    }
    uPortLog("%sfence \"%s\", %d circle(s), %d polygon(s), %s:\n", pPrefix,
             pName, pTestFence->numCircles, pTestFence->numPolygons,
             altitudeBuffer);
}

// Print out the latitude/longitude of a test vertex.
static void printTestVertex(const char *pPrefix,
                            const uGeofenceTestVertex_t *pTestVertex)
{
    char sign[2] = {0};
    int32_t whole[2] = {0};
    int32_t fractionUpper[2] = {0};
    int32_t fractionLower[2] = {0};

    sign[0] = latLongToBits(pTestVertex->latitudeX1e9,
                            &(whole[0]), &(fractionUpper[0]), &(fractionLower[0]));
    sign[1] = latLongToBits(pTestVertex->longitudeX1e9,
                            &(whole[1]), &(fractionUpper[1]), &(fractionLower[1]));

    if (pPrefix == NULL) {
        pPrefix = "";
    }
    uPortLog("%s%c%d.%06d%03d,%c%d.%06d%03d", pPrefix,
             sign[0], whole[0], fractionUpper[0], fractionLower[0],
             sign[1], whole[1], fractionUpper[1], fractionLower[1]);
}

// Print out the dimensions of a test circle.
static void printTestCircle(const char *pPrefix,
                            const uGeofenceTestCircle_t *pTestCircle)
{
    if (pPrefix == NULL) {
        pPrefix = "";
    }
    uPortLog("%scircle ", pPrefix);
    printTestVertex(NULL, pTestCircle->pCentre);
    uPortLog(" %d.%03d m\n", (int) (pTestCircle->radiusMillimetres / 1000),
             (int) (pTestCircle->radiusMillimetres % 1000));
}

// Print out the dimensions of a test polygon.
static void printTestPolygon(const char *pPrefix,
                             const uGeofenceTestPolygon_t *pTestPolygon)
{
    const uGeofenceTestVertex_t *pTestVertex;
    const uGeofenceTestVertex_t *pTestVertexPrevious = NULL;
    char prefixIndentBuffer[25];
    size_t y;

    if (pPrefix == NULL) {
        pPrefix = "";
    }
    y = strlen(pPrefix);
    if (y > sizeof(prefixIndentBuffer) - 3) { // -3 for our two character indent, plus a terminator
        y = sizeof(prefixIndentBuffer) - 3;
    }
    memcpy(prefixIndentBuffer, pPrefix, y);
    memcpy(prefixIndentBuffer + y, "  ", 3);
    uPortLog("%spolygon %d sides:\n", pPrefix, pTestPolygon->numVertices);
    for (size_t x = 0; x <= pTestPolygon->numVertices; x++) {
        pTestVertex = pTestPolygon->pVertex[x % pTestPolygon->numVertices];
        if ((pTestVertex != NULL) && (pTestVertexPrevious != NULL)) {
            printTestVertex(prefixIndentBuffer, pTestVertexPrevious);
            printTestVertex(" <-> ", pTestVertex);
            uPortLog("\n");
        }
        pTestVertexPrevious = pTestVertex;
    }
}

// Print out a test point.
static void printTestPoint(const char *pPrefix,
                           const uGeofenceTestPoint_t *pTestPoint,
                           size_t parametersIndex)
{
    const uGeofencePositionVariables_t *pTestPositionVariables = &(pTestPoint->positionVariables);
    char altitudeBuffer[32] = "2D";

    if (pPrefix == NULL) {
        pPrefix = "";
    }
    if (pTestPositionVariables->altitudeMillimetres >= 0) {
        snprintf(altitudeBuffer, sizeof(altitudeBuffer), "%d.%03d +/-%d.%03d m high",
                 (int) (pTestPositionVariables->altitudeMillimetres / 1000),
                 (int) (pTestPositionVariables->altitudeMillimetres % 1000),
                 (int) (pTestPositionVariables->altitudeUncertaintyMillimetres / 1000),
                 (int) (pTestPositionVariables->altitudeUncertaintyMillimetres % 1000));
    }
    uPortLog("%spoint ", pPrefix);
    printTestVertex(NULL, pTestPoint->pPosition);
    uPortLog(", radius %d.%03d m, %s -> expected %s",
             (int) (pTestPositionVariables->radiusMillimetres / 1000),
             (int) (pTestPositionVariables->radiusMillimetres % 1000), altitudeBuffer,
             pTestPoint->outcomeBitMap & (1U << gTestParameters[parametersIndex]) ? "true" : "false");
}

#ifdef _WIN32

// Write the given position into the given buffer.
static int32_t kmlPositionString(char *pBuffer, size_t bufferLength,
                                 const uGeofenceTestVertex_t *pVertex,
                                 int32_t radiusMillimetres)
{
    double latitude = ((double) pVertex->latitudeX1e9) / 1000000000LL;
    double longitude = ((double) pVertex->longitudeX1e9)  / 1000000000LL;
    double radiusMetres = ((double) radiusMillimetres)  / 1000;

    return snprintf(pBuffer, bufferLength, "%0.9f, %0.9f, %0.3f m radius",
                    latitude, longitude, radiusMetres);
}

// Write the given KML file format coordinates into the given buffer.
static int32_t kmlCoordinatesString(char *pBuffer, size_t bufferLength,
                                    const uGeofenceTestVertex_t *pVertex,
                                    int32_t altitudeMillimetres)
{
    double latitude = ((double) pVertex->latitudeX1e9) / 1000000000LL;
    double longitude = ((double) pVertex->longitudeX1e9)  / 1000000000LL;
    double altitudeMetres = ((double) altitudeMillimetres)  / 1000;

    // In a KML file it is long,lat NOT lat,long
    return snprintf(pBuffer, bufferLength, "%0.9f, %0.9f, %0.3f",
                    longitude, latitude, altitudeMetres);
}

// Open a KML kmlFile using the fence name as the basis for the kmlFile name.
static HANDLE kmlOpenFile(const char *pFenceName)
{
    char buffer[128];
    size_t x = 0;

    // -5 to leave room for ".kml"
    while ((x < sizeof(buffer) - 5) && (*pFenceName != 0)) {
        if (isblank((int) *pFenceName)) {
            buffer[x] = '_';
            x++;
        } else if (isalnum((int) *pFenceName)) {
            buffer[x] = (char) tolower((int) * pFenceName);
            x++;
        }
        pFenceName++;
    }
    memcpy(&(buffer[x]), ".kml", 5);
    U_TEST_PRINT_LINE("Creating KML file \"%s\"...", buffer);
    return CreateFile(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

// Write an indent to the KML file; indent is a logical one,
// so 0 for no indent, 1 for the first indent etc.
static bool kmlWriteIndent(HANDLE kmlFile, size_t indent)
{
    char buffer[16];
    size_t x;

    for (x = 0; x < indent; x++) {
        buffer[x] = '\t';
    }

    return WriteFile(kmlFile, buffer, x, NULL, NULL);
}

// Write a name into the KML kmlFile
static bool kmlWriteName(HANDLE kmlFile, size_t indent, const char *pName)
{
    return kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "<name>\"", 7, NULL, NULL) &&
           WriteFile(kmlFile, pName, strlen(pName), NULL, NULL) &&
           WriteFile(kmlFile, "\"</name>\n", 9, NULL, NULL);
}

// Write a visible flag into the KML kmlFile
static bool kmlWriteVisible(HANDLE kmlFile, size_t indent, bool isVisible)
{
    char visible[2] = {'0', 0};

    if (isVisible) {
        visible[0] = '1';
    }

    return kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "<visibility>", 12, NULL, NULL) &&
           WriteFile(kmlFile, visible, 1, NULL, NULL) &&
           WriteFile(kmlFile, "</visibility>\n", 14, NULL, NULL);
}

// Write the start of a folder with the given name to kmlFile.
static bool kmlWriteFolderStart(HANDLE kmlFile, size_t indent,
                                const char *pNameStr, bool isVisible)
{
    return kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "<Folder>\n", 9, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "<name>", 6, NULL, NULL) &&
           WriteFile(kmlFile, pNameStr, strlen(pNameStr), NULL, NULL) &&
           WriteFile(kmlFile, "</name>\n", 8, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "<open>0</open>\n", 15, NULL, NULL) &&
           kmlWriteVisible(kmlFile, indent + 1, isVisible);
}

// Write the start of a test point folder to kmlFile.
static bool kmlWriteFolderStartTestPoint(HANDLE kmlFile, size_t indent,
                                         const uGeofenceTestVertex_t *pVertex,
                                         int32_t radiusMillimetres,
                                         bool isVisible)
{
    char buffer[128];
    kmlPositionString(buffer, sizeof(buffer), pVertex, radiusMillimetres);
    return kmlWriteFolderStart(kmlFile, indent, buffer, isVisible);
}

// Write the end of a folder to kmlFile.
static bool kmlWriteFolderEnd(HANDLE kmlFile, size_t indent)
{
    return kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "</Folder>\n", 10, NULL, NULL);
}

// Write a point to kmlFile with the given style and
// optional name (otherwise the poosition will be used).
static bool kmlWritePoint(HANDLE kmlFile, size_t indent,
                          const char *pName,
                          const char *pStyleUrlStr,
                          const uGeofenceTestVertex_t *pVertex,
                          int32_t radiusMillimetres,
                          int32_t altitudeMillimetres)
{
    char bufferName[128];
    char bufferCoordinates[128];
    if (pName == NULL) {
        kmlPositionString(bufferName, sizeof(bufferName), pVertex, radiusMillimetres);
    }
    kmlCoordinatesString(bufferCoordinates, sizeof(bufferCoordinates), pVertex, altitudeMillimetres);
    return kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "<Placemark>\n", 12, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "<name>", 6, NULL, NULL) &&
           (pName ?
            WriteFile(kmlFile, pName, strlen(pName), NULL, NULL) :
            WriteFile(kmlFile, bufferName, strlen(bufferName), NULL, NULL)) &&
           WriteFile(kmlFile, "</name>\n", 8, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "<styleUrl>", 10, NULL, NULL) &&
           (pStyleUrlStr ? WriteFile(kmlFile, pStyleUrlStr, strlen(pStyleUrlStr), NULL, NULL) : true) &&
           WriteFile(kmlFile, "</styleUrl>\n", 12, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "<Point>\n", 8, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 2) &&
           WriteFile(kmlFile, "<coordinates>", 13, NULL, NULL) &&
           WriteFile(kmlFile, bufferCoordinates, strlen(bufferCoordinates), NULL, NULL) &&
           WriteFile(kmlFile, "</coordinates>\n", 15, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent + 1) &&
           WriteFile(kmlFile, "</Point>\n", 9, NULL, NULL) &&
           kmlWriteIndent(kmlFile, indent) &&
           WriteFile(kmlFile, "</Placemark>\n", 13, NULL, NULL);
}

// Convert degrees to radians.
static double kmlDegreesToRadians(double degrees)
{
    return degrees * U_GEOFENCE_TEST_PI_FLOAT / 180;
}

// Convert an angle in radians to degrees.
static double kmlRadiansToDegrees(double radians)
{
    return radians * 180 / U_GEOFENCE_TEST_PI_FLOAT;
}

// Calculate the coordinates of a point at a given distance and azimuth from
// another point on a spherical earth, from
// https://www.movable-type.co.uk/scripts/latlong.html
static void kmlReverseHaversine(double latitude, double longitude,
                                double azimuthDegrees, double lengthMetres,
                                double *pLatitude, double *pLongitude)
{
    double startLatitudeRadians = kmlDegreesToRadians(latitude);
    double startLongitudeRadians = kmlDegreesToRadians(longitude);
    double azimuthRadians = kmlDegreesToRadians(azimuthDegrees);

    double lengthOverR = lengthMetres / U_GEOFENCE_TEST_RADIUS_AT_EQUATOR_METERS;
    double sinLatitude = sin(startLatitudeRadians);
    double cosLatitude = cos(startLatitudeRadians);
    double sinLengthOverR = sin(lengthOverR);
    double cosLengthOverR = cos(lengthOverR);
    double sinAzimuth = sin(azimuthRadians);
    double cosAzimuth = cos(azimuthRadians);

    double latitudeRadians = asin((sinLatitude * cosLengthOverR) + (cosLatitude * sinLengthOverR *
                                                                    cosAzimuth));
    *pLatitude = kmlRadiansToDegrees(latitudeRadians);
    double longitudeRadians = startLongitudeRadians + atan2(sinAzimuth * sinLengthOverR * cosLatitude,
                                                            cosLengthOverR - (sinLatitude * sin(latitudeRadians)));
    // Handle the wrap at -180
    if (longitudeRadians <= -U_GEOFENCE_TEST_PI_FLOAT) {
        longitudeRadians += U_GEOFENCE_TEST_PI_FLOAT * 2;
    } else if (longitudeRadians >= U_GEOFENCE_TEST_PI_FLOAT) {
        longitudeRadians -= U_GEOFENCE_TEST_PI_FLOAT * 2;
    }
    *pLongitude = kmlRadiansToDegrees(longitudeRadians);
}

// Take a vertex, which is the centre of our star and produce a new
// point that is some percentage of starRadiusMillimetres from the
// centre and some proportion of 360 degrees from north.
static void kmlStarTransformVertex(const uGeofenceTestVertex_t *pVertex,
                                   int64_t starRadiusMillimetres,
                                   size_t rays, size_t points,
                                   uGeofenceTestVertex_t *pNewVertex)
{
    double latitude;
    double longitude;

    kmlReverseHaversine(((double) pVertex->latitudeX1e9) / 1000000000LL,
                        ((double) pVertex->longitudeX1e9) / 1000000000LL,
                        ((double) rays) * 360 / U_GEOFENCE_TEST_STAR_RAYS,
                        points * ((double) starRadiusMillimetres) / 1000 / U_GEOFENCE_TEST_STAR_POINTS_PER_RAY,
                        &latitude, &longitude);

    pNewVertex->latitudeX1e9 = (int64_t) (latitude * 1000000000LL);
    pNewVertex->longitudeX1e9 = (int64_t) (longitude * 1000000000LL);
}

#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test geofence things standalone, without a device.
 */
U_PORT_TEST_FUNCTION("[geofence]", "geofenceBasic")
{
    int32_t resourceCount;
    const uGeofenceTestData_t *pTestData;
    const uGeofenceTestFence_t *pTestFence;
    const uGeofenceTestCircle_t *pTestCircle;
    const uGeofenceTestPolygon_t *pTestPolygon;
    const uGeofenceTestVertex_t *pTestVertex;
    const uGeofenceTestPoint_t *pTestPoint;
    const uGeofencePositionVariables_t *pTestPositionVariables;
    bool testShouldBeTrue;
    bool testIsTrue;
    uGeofencePositionState_t positionState;
    bool newPolygon;
    char prefixBuffer[32];
    uTimeoutStart_t timeoutStart;
    size_t numEdges;
    size_t numShapes;
    size_t numFailedCalculations;
    int64_t distanceMinMillimetres;

    uPortDeinit();

    // Get the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Need to initialise only the port
    uPortInit();

    // Get us a fence
    gpFence = pUGeofenceCreate(U_GEOFENCE_TEST_FENCE_NAME);
    U_PORT_TEST_ASSERT(gpFence != NULL);
    // Free it again
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);

    // Do it again with no name
    gpFence = pUGeofenceCreate(NULL);
    U_PORT_TEST_ASSERT(gpFence != NULL);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);

    // Back to the named one: this time add a circle to it and empty it
    gpFence = pUGeofenceCreate(U_GEOFENCE_TEST_FENCE_NAME);
    U_PORT_TEST_ASSERT(gpFence != NULL);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, 0, 0, 1) == 0);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);

    // Same for a single vertex
    gpFence = pUGeofenceCreate(U_GEOFENCE_TEST_FENCE_NAME);
    U_PORT_TEST_ASSERT(gpFence != NULL);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, 0, 0, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);

    // Then limiting values
    gpFence = pUGeofenceCreate(U_GEOFENCE_TEST_FENCE_NAME);
    U_PORT_TEST_ASSERT(gpFence != NULL);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, 0, 0, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, U_GEOFENCE_TEST_LATITUDE_MAX_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9, 100000) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, U_GEOFENCE_TEST_LATITUDE_MAX_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9, 100000) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, U_GEOFENCE_TEST_LATITUDE_MIN_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9, 100000) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, U_GEOFENCE_TEST_LATITUDE_MIN_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9, 100000) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, U_GEOFENCE_TEST_LATITUDE_MAX_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, U_GEOFENCE_TEST_LATITUDE_MAX_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, U_GEOFENCE_TEST_LATITUDE_MIN_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, U_GEOFENCE_TEST_LATITUDE_MIN_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9, false) == 0);
    // Now invalid values
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(NULL, 0, 0, 1) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, 0, 0, 0) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence,
                                          U_GEOFENCE_TEST_LATITUDE_MAX_X1E9 + 1, 0, 1) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence,
                                          0, U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9 + 1, 1) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence,
                                          U_GEOFENCE_TEST_LATITUDE_MIN_X1E9 - 1, 0, 1) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence,
                                          0, U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9 - 1, 1) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(NULL, 0, 0, false) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence,
                                          U_GEOFENCE_TEST_LATITUDE_MAX_X1E9 + 1, 0, false) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence,
                                          0, U_GEOFENCE_TEST_LONGITUDE_MAX_X1E9 + 1, false) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence,
                                          U_GEOFENCE_TEST_LATITUDE_MIN_X1E9 - 1, 0, false) < 0);
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence,
                                          0, U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9 - 1, false) < 0);
    // Then a few more valid values
    U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence, -10, 19, false) == 0);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence, U_GEOFENCE_TEST_LATITUDE_MIN_X1E9,
                                          U_GEOFENCE_TEST_LONGITUDE_MIN_X1E9, 1000) == 0);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);

    // Now run through the test data
    for (size_t x = 0; x < gpUGeofenceTestDataSize; x++) {
        pTestData = gpUGeofenceTestData[x];
        snprintf(prefixBuffer, sizeof(prefixBuffer), U_TEST_PREFIX_A, (char) (x + 0x41));
        // Add the fence
        numShapes = 0;
        numEdges = 0;
        numFailedCalculations = 0;
        pTestFence = pTestData->pFence;
        printTestFence(prefixBuffer, pTestFence);
        snprintf(prefixBuffer, sizeof(prefixBuffer), U_TEST_PREFIX_A "  ", (char) (x + 0x41));
        // On even numbers create a new fence, odd numbers re-use the existing
        if (x % 2 == 0) {
            gpFence = pUGeofenceCreate(pTestFence->pName);
        }
        U_PORT_TEST_ASSERT(gpFence != NULL);
        if (pTestFence->altitudeMaxMillimetres != INT_MAX) {
            U_PORT_TEST_ASSERT(uGeofenceSetAltitudeMax(gpFence,
                                                       pTestFence->altitudeMaxMillimetres) == 0);
        }
        if (pTestFence->altitudeMinMillimetres != INT_MIN) {
            U_PORT_TEST_ASSERT(uGeofenceSetAltitudeMin(gpFence,
                                                       pTestFence->altitudeMinMillimetres) == 0);
        }
        for (size_t y = 0; y < pTestFence->numCircles; y++) {
            pTestCircle = pTestFence->pCircle[y];
            printTestCircle(prefixBuffer, pTestCircle);
            U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFence,
                                                  pTestCircle->pCentre->latitudeX1e9,
                                                  pTestCircle->pCentre->longitudeX1e9,
                                                  pTestCircle->radiusMillimetres) == 0);
            numShapes++;
            numEdges++;
        }
        newPolygon = false;
        for (size_t y = 0; y < pTestFence->numPolygons; y++) {
            pTestPolygon = pTestFence->pPolygon[y];
            printTestPolygon(prefixBuffer, pTestPolygon);
            for (size_t z = 0; z < pTestPolygon->numVertices; z++) {
                pTestVertex = pTestPolygon->pVertex[z];
                U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFence,
                                                      pTestVertex->latitudeX1e9,
                                                      pTestVertex->longitudeX1e9,
                                                      newPolygon) == 0);
                numEdges++;
            }
            numShapes++;
            newPolygon = true;
        }
        // Test the point(s) against the fence in all permutations of parameters,
        // do it twice, once with prints and then without to get an accurate timing
        for (size_t t = 0; t < 2; t++) {
            timeoutStart = uTimeoutStart();
            // We take all of the points and test one parameter combination, then
            // take all of the points and repeat for the next parameter combination,
            // etc., rather than doing all of the parameter combinations for one
            // point then moving to the next; this is because the transition test
            // has memory (of the previous position) and won't be tested properly
            // if it is mixed in with the other test types.  Think if it as taking
            // a fence through a journey (a set of points) with a set of test
            // parameters and then repeating the same journey for the next set of
            // test parameters.
            if (pTestData->numPoints > 0) {
                for (size_t z = 0; z < sizeof(gTestParameters) / sizeof(gTestParameters[0]); z++) {
                    if (t == 0) {
                        // Note: only print in the first iteration, the second iteration
                        // is just to get an idea of the calculation time, hence no asserts
                        // or prints there
                        U_TEST_PRINT_LINE_A("  %d test type \"%s %s\":", (char) (x + 0x41), (int) z + 1,
                                            gPessimisticNotOptimistic[z] ?  "pessimistic" : "optimistic",
                                            gpTestTypeString[gTestType[z]]);
                    }
                    snprintf(prefixBuffer, sizeof(prefixBuffer), U_TEST_PREFIX_A "   ", (char) (x + 0x41));
                    uGeofenceTestResetMemory(gpFence);
                    for (size_t y = 0; y < pTestData->numPoints; y++) {
                        pTestPoint = pTestData->pPoint[y];
                        pTestVertex = pTestPoint->pPosition;
                        pTestPositionVariables = &(pTestPoint->positionVariables);
                        if (t == 0) {
                            snprintf(prefixBuffer, sizeof(prefixBuffer), U_TEST_PREFIX_A "   %2d ",
                                     (char) (x + 0x41), (int) y + 1);
                            printTestPoint(prefixBuffer, pTestPoint, z);
                        }
                        testShouldBeTrue = ((pTestPoint->outcomeBitMap & (1U << gTestParameters[z])) != 0);
                        testIsTrue = uGeofenceTest(gpFence, gTestType[z],
                                                   gPessimisticNotOptimistic[z],
                                                   pTestVertex->latitudeX1e9,
                                                   pTestVertex->longitudeX1e9,
                                                   pTestPositionVariables->altitudeMillimetres,
                                                   pTestPositionVariables->radiusMillimetres,
                                                   pTestPositionVariables->altitudeUncertaintyMillimetres);
                        positionState = uGeofenceTestGetPositionState(gpFence);
                        if (positionState != U_GEOFENCE_POSITION_STATE_NONE) {
                            if (t == 0) {
                                if (testIsTrue) {
                                    if (testShouldBeTrue) {
                                        uPortLog(", is true");
                                    } else {
                                        uPortLog(", is TRUE");
                                    }
                                } else {
                                    if (!testShouldBeTrue) {
                                        uPortLog(", is false");
                                    } else {
                                        uPortLog(", is FALSE");
                                    }
                                }
                                uPortLog(" (%s", gpPositionStateString[positionState]);
                                if (positionState == U_GEOFENCE_POSITION_STATE_OUTSIDE) {
                                    distanceMinMillimetres = uGeofenceTestGetDistanceMin(gpFence);
                                    if (distanceMinMillimetres != LLONG_MIN) {
                                        uPortLog(", %d.%03d m).\n", (int) (distanceMinMillimetres / 1000),
                                                 (int) (distanceMinMillimetres % 1000));
                                    } else {
                                        uPortLog(" ---).\n");
                                    }
                                } else {
                                    uPortLog(").\n");
                                }
                                U_PORT_TEST_ASSERT(testIsTrue == testShouldBeTrue);
                            }
                        } else {
                            numFailedCalculations++;
                            if (t == 0) {
                                uPortLog(", but CALCULATION FAILED!\n");
                            }
                        }
                    }
                }
                if (t > 0) {
                    uPortLog(U_TEST_PREFIX_A "testing %d shape(s) (%d edge(s)) against %d point(s),"
                             " %d times each (print time excluded), averaged %u ms per point",
                             (char) (x + 0x41), numShapes, numEdges, pTestData->numPoints,
                             sizeof(gTestParameters) / sizeof(gTestParameters[0]),
                             (uTimeoutElapsedMs(timeoutStart) /
                              (pTestData->numPoints * sizeof(gTestParameters) / sizeof(gTestParameters[0]))));
                    if (numFailedCalculations > 0) {
                        uPortLog(" AND %d CALCULATION(S) FAILED.\n", numFailedCalculations);
                        U_PORT_TEST_ASSERT(false);
                    } else {
                        uPortLog(" and no calculations failed.\n");
                    }
                }
            }
        }
        // On even numbers clear the fence without freeing it,
        // so that it can be re-used above, on odd numbers free it
        // so that it must be recreated when we loop
        if (x % 2 == 0) {
            U_PORT_TEST_ASSERT(uGeofenceClearMap(gpFence) == 0);
        } else {
            U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);
            gpFence = NULL;
        }
    }

    if (gpFence != NULL) {
        // Make sure the fence is free'd now
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFence) == 0);
        gpFence = NULL;
    }

    // Free the mutex so that our memory sums add up
    uGeofenceCleanUp();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#ifdef _WIN32

/** Repeat run through the standalone test data but producing
 * a KML kmlFile on Windows that can be loaded into Google Earth
 * with stars at every point emitting travel in all directions
 * that can be checked against what is expecit by eye.
 * There are few asserts in here: we've done the testing in
 * the basic test already, this is just to create data
 * files that we can eyeball.
 *
 * Here's how to do that: open Google Earth and import one or
 * more of the KML files written by this test into it.  By
 * default the stars emanating from each test point with the
 * test type set to "pessimistic inside" will be displayed.
 * A dark green dot indicates outside and a light green dot
 * indicates inside (a red dot indicates a calculation error,
 * which you should not see any of).  You can visually check
 * that the stars illuminating the map reveal the underlying
 * shapes of the fence that is inside that set of test data.
 * When you're fiddling with a shape in a fence, this is a
 * good way to see the effect of your changes.
 */
U_PORT_TEST_FUNCTION("[geofence]", "geofenceStars")
{
    const uGeofenceTestData_t *pTestData;
    const uGeofenceTestFence_t *pTestFence;
    const uGeofenceTestCircle_t *pTestCircle;
    const uGeofenceTestPolygon_t *pTestPolygon;
    const uGeofenceTestVertex_t *pTestVertex;
    const uGeofenceTestPoint_t *pTestPoint;
    const uGeofenceTestPoint_t *pTestPointTmp;
    const uGeofencePositionVariables_t *pTestPositionVariables;
    uGeofencePositionState_t positionState;
    uGeofenceTestKmlStarSet_t *pStarSet;
    uGeofenceTestKmlCoordinate_t *pKmlCoordinate;
    bool plotStar;
    bool newPolygon;
    char buffer[128];
    size_t indent;
    bool folderVisible;

    // Only need to initialise the port
    uPortInit();

    // Run through the test data
    for (size_t x = 0; x < gpUGeofenceTestDataSize; x++) {
        pTestData = gpUGeofenceTestData[x];
        gKmlNumStarSets = 0;
        folderVisible = true;
        indent = 0;
        // Add the fence
        pTestFence = pTestData->pFence;
        gpFence = pUGeofenceCreate(pTestFence->pName);
        U_PORT_TEST_ASSERT(gpFence != NULL);
        // Open a KML kmlFile with the name of the fence and write the
        // initial blocks, including the styles
        gKmlFile = kmlOpenFile(pTestFence->pName);
        U_PORT_TEST_ASSERT(gKmlFile != INVALID_HANDLE_VALUE);
        U_PORT_TEST_ASSERT(WriteFile(gKmlFile, gUGeofenceTestKmlDocStartStr,
                                     gUGeofenceTestKmlDocStartStrlen, NULL, NULL));
        indent++;
        U_PORT_TEST_ASSERT(kmlWriteName(gKmlFile, indent, pTestFence->pName));
        U_PORT_TEST_ASSERT(WriteFile(gKmlFile, gUGeofenceTestKmlDocStylesStr,
                                     gUGeofenceTestKmlDocStylesStrlen, NULL, NULL));

        // Populate the fence
        if (pTestFence->altitudeMaxMillimetres != INT_MAX) {
            uGeofenceSetAltitudeMax(gpFence, pTestFence->altitudeMaxMillimetres);
        }
        if (pTestFence->altitudeMinMillimetres != INT_MIN) {
            uGeofenceSetAltitudeMin(gpFence, pTestFence->altitudeMinMillimetres);
        }
        for (size_t y = 0; y < pTestFence->numCircles; y++) {
            pTestCircle = pTestFence->pCircle[y];
            uGeofenceAddCircle(gpFence,
                               pTestCircle->pCentre->latitudeX1e9,
                               pTestCircle->pCentre->longitudeX1e9,
                               pTestCircle->radiusMillimetres);
        }
        newPolygon = false;
        for (size_t y = 0; y < pTestFence->numPolygons; y++) {
            pTestPolygon = pTestFence->pPolygon[y];
            for (size_t z = 0; z < pTestPolygon->numVertices; z++) {
                pTestVertex = pTestPolygon->pVertex[z];
                uGeofenceAddVertex(gpFence,
                                   pTestVertex->latitudeX1e9,
                                   pTestVertex->longitudeX1e9,
                                   newPolygon);
            }
            newPolygon = true;
        }
        // Run through all of the test points and, from each, plot
        // N rays of our star and, on each ray, plot M test points
        // for each test type, and write all of this into a huge KML
        // kmlFile that _should_ present a set of stars shining from the
        // test points, illuminating the interesting bits of each
        // map.
        // Collect all of the data for one test type first, write that
        // to the KML file, and repeat for the next test type: this
        // way the user can see the entire map for each test type with
        // one click, rather than having to go through all of the test
        // points selecting the same test type on each
        // First, make our list of test points, eliminating any
        // rhat are a repeat of an earlier position with a different
        // altitude (this test is only 2D).
        for (size_t y = 0; y < pTestData->numPoints; y++) {
            pTestPoint = pTestData->pPoint[y];
            pTestPositionVariables = &(pTestPoint->positionVariables);
            plotStar = false;
            if (((pTestFence->altitudeMinMillimetres == INT_MIN) &&
                 (pTestFence->altitudeMaxMillimetres == INT_MAX)) ||  // Map is 2D
                ((pTestPositionVariables->altitudeMillimetres == INT_MIN) || // Point is 2D
                 (pTestPositionVariables->altitudeMillimetres >= pTestFence->altitudeMinMillimetres) &&
                 (pTestPositionVariables->altitudeMillimetres <= pTestFence->altitudeMaxMillimetres))) { // In limits
                // Check that this is not a repeat of an earlier
                // position with a different altitude (this test
                // is only 2D)
                plotStar = true;
                for (size_t z = 0; (z < gKmlNumStarSets) && plotStar; z++) {
                    pTestPointTmp = gpKmlStarSet[z]->pTestPoint;
                    if ((pTestPointTmp->pPosition->latitudeX1e9 == pTestPoint->pPosition->latitudeX1e9) &&
                        (pTestPointTmp->pPosition->longitudeX1e9 == pTestPoint->pPosition->longitudeX1e9) &&
                        (pTestPointTmp->positionVariables.radiusMillimetres ==
                         pTestPoint->positionVariables.radiusMillimetres)) {
                        // We've plotted this test point before at a different altitude
                        plotStar = false;
                    }
                }
            }
            if (plotStar) {
                // Allocate a HUGE block of memory to contain all of the
                // coordinates that compries the set of stars for this
                // test point
                pStarSet = (uGeofenceTestKmlStarSet_t *) pUPortMalloc(sizeof(*pStarSet));
                U_PORT_TEST_ASSERT(pStarSet != NULL);
                memset(pStarSet, 0, sizeof(*pStarSet));
                pStarSet->pTestPoint = pTestPoint;
                gpKmlStarSet[gKmlNumStarSets] = pStarSet;
                gKmlNumStarSets++;
            }
        }
        // Now have gKmlNumStarSets test points set up in gKmlTestData,
        // populate all of the stars for each parameter combination
        for (size_t y = 0; y < gKmlNumStarSets; y++) {
            pStarSet = gpKmlStarSet[y];
            pTestPoint = pStarSet->pTestPoint;
            pTestVertex = pTestPoint->pPosition;
            pTestPositionVariables = &(pTestPoint->positionVariables);
            for (size_t z = 0; z < sizeof(gTestParameters) / sizeof(gTestParameters[0]); z++) {
                for (size_t rays = 0; rays < U_GEOFENCE_TEST_STAR_RAYS; rays++) {
                    uGeofenceTestResetMemory(gpFence);
                    for (size_t points = 0; points < U_GEOFENCE_TEST_STAR_POINTS_PER_RAY; points++) {
                        pKmlCoordinate = &(pStarSet->stars[z].star[rays][points]);
                        pKmlCoordinate->radiusMillimetres = pTestPositionVariables->radiusMillimetres;
                        pKmlCoordinate->altitudeMillimetres = pTestPositionVariables->altitudeMillimetres;
                        // Take the centre of the star and produce a new
                        // point that is some way around in azimuth from
                        // north and some portion of starRadiusMillimetres
                        // (taken from our test data) away.
                        // Start from point 1 in order to not keep repeating the point at the centre
                        kmlStarTransformVertex(pTestVertex, pTestData->starRadiusMillimetres,
                                               rays, points + 1, &(pKmlCoordinate->vertex));
                        uGeofenceTest(gpFence, gTestType[z],
                                      gPessimisticNotOptimistic[z],
                                      pKmlCoordinate->vertex.latitudeX1e9,
                                      pKmlCoordinate->vertex.longitudeX1e9,
                                      pKmlCoordinate->altitudeMillimetres,
                                      pKmlCoordinate->radiusMillimetres,
                                      pTestPositionVariables->altitudeUncertaintyMillimetres);
                        positionState = uGeofenceTestGetPositionState(gpFence);
                        pKmlCoordinate->pStyleMap = gpKmlStyleMap[positionState];
                    }
                }
            }
        }
        // The gKmlNumStarSets KML coordinates buried in gKmlTestData are now
        // populated: write them to the KML file with the test parameters
        // at the top of the folder structure
        for (size_t y = 0; y < sizeof(gTestParameters) / sizeof(gTestParameters[0]); y++) {
            snprintf(buffer, sizeof(buffer), "%s %s",
                     gPessimisticNotOptimistic[y] ?  "pessimistic" : "optimistic",
                     gpTestTypeString[gTestType[y]]);
            kmlWriteFolderStart(gKmlFile, indent, buffer, folderVisible);
            indent++;
            for (size_t z = 0; z < gKmlNumStarSets; z++) {
                pStarSet = gpKmlStarSet[z];
                pTestPoint = pStarSet->pTestPoint;
                pTestVertex = pTestPoint->pPosition;
                pTestPositionVariables = &(pTestPoint->positionVariables);
                kmlWriteFolderStartTestPoint(gKmlFile, indent, pTestVertex,
                                             pTestPositionVariables->radiusMillimetres,
                                             true);
                indent++;
                // Write the centre of the star into the KML kmlFile with no style
                // so that it appears as the default yellow pin
                kmlWritePoint(gKmlFile, indent, NULL, NULL, pTestVertex,
                              pTestPositionVariables->radiusMillimetres,
                              pTestPositionVariables->altitudeMillimetres);
                for (size_t rays = 0; rays < U_GEOFENCE_TEST_STAR_RAYS; rays++) {
                    uGeofenceTestResetMemory(gpFence);
                    for (size_t points = 0; points < U_GEOFENCE_TEST_STAR_POINTS_PER_RAY; points++) {
                        pKmlCoordinate = &(pStarSet->stars[y].star[rays][points]);
                        kmlWritePoint(gKmlFile, indent, NULL,
                                      pKmlCoordinate->pStyleMap,
                                      &(pKmlCoordinate->vertex),
                                      pKmlCoordinate->radiusMillimetres,
                                      pKmlCoordinate->altitudeMillimetres);
                    }
                }
                indent--;
                kmlWriteFolderEnd(gKmlFile, indent);
            }
            indent--;
            kmlWriteFolderEnd(gKmlFile, indent);

            // Only make the first folder of the parameters sets
            // visible by default, that's usually enough to get
            // a good idea that things are working
            folderVisible = false;
        }

        // Free the allocated memory
        for (size_t y = 0; y < gKmlNumStarSets; y++) {
            uPortFree(gpKmlStarSet[y]);
            gpKmlStarSet[y] = NULL;
        }

        // Free the fence and write the end of the KML kmlFile before closing it
        uGeofenceFree(gpFence) ;
        gpFence = NULL;
        U_PORT_TEST_ASSERT(WriteFile(gKmlFile, gUGeofenceTestKmlDocEndStr, gUGeofenceTestKmlDocEndStrlen,
                                     NULL, NULL));
        CloseHandle(gKmlFile);
    }

    gKmlFile = INVALID_HANDLE_VALUE;
    U_TEST_PRINT_LINE("KML kmlFile written.");

    // Free the mutex so that our memory sums add up
    uGeofenceCleanUp();
    uPortDeinit();
}

#endif // #ifdef _WIN32

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[geofence]", "geofenceCleanUp")
{
    // In case a fence was left hanging
    uGeofenceFree(gpFence);
    uGeofenceCleanUp();

#ifdef _WIN32
    // Free any memory that remains allocated
    for (size_t x = 0; x < gKmlNumStarSets; x++) {
        uPortFree(gpKmlStarSet[x]);
    }
    CloseHandle(gKmlFile);
#endif

    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #ifdef U_CFG_GEOFENCE

// End of file
