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
 * @brief Provides a wrapper to CPP functions in
 * https://github.com/geographiclib/geographiclib, only built if
 * "geodesic" is included in your UBXLIB_FEATURES CMake variable.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_compiler.h"    // For U_WEAK

#include "u_error_common.h"

#include "u_geofence_geodesic.h"

#ifdef U_CFG_GEOFENCE_USE_GEODESIC
# include "Geodesic.hpp"
# include "Intersect.hpp"
# include "Gnomonic.hpp"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Return the maximum of two values.
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/** Return the minimum of two values.
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(U_CFG_GEOFENCE) && defined(U_CFG_GEOFENCE_USE_GEODESIC)
// Subtract two longitudes (A - B), specified in degrees,
// taking into account the wrap at 180.
static double longitudeSubtract(double aLongitudeDegrees,
                                double bLongitudeDegrees)
{
    double difference = aLongitudeDegrees - bLongitudeDegrees;

    if (difference <= -180) {
        difference += 360;
    } else if (difference >= 180) {
        difference -= 360;
    }

    return difference;
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

U_WEAK int32_t uGeofenceWgs84GeodDirect(double latitudeDegrees,
                                        double longitudeDegrees,
                                        double azimuthDegrees,
                                        double lengthMetres,
                                        double *pLatitudeDegrees,
                                        double *pLongitudeDegrees,
                                        double *pAzimuthDegrees)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TOO_BIG;

# ifdef U_CFG_GEOFENCE_USE_GEODESIC
    double bLatitudeDegrees = NAN;
    double bLongitudeDegrees = NAN;
    double bAzimuthDegrees = NAN;
    const GeographicLib::Geodesic &geod = GeographicLib::Geodesic::WGS84();

    if (pAzimuthDegrees == NULL) {
        geod.Direct(latitudeDegrees, longitudeDegrees,
                    azimuthDegrees, lengthMetres,
                    bLatitudeDegrees, bLongitudeDegrees);
    } else {
        geod.Direct(latitudeDegrees, longitudeDegrees,
                    azimuthDegrees, lengthMetres,
                    bLatitudeDegrees, bLongitudeDegrees,
                    bAzimuthDegrees);
    }
    if (pLatitudeDegrees != NULL) {
        *pLatitudeDegrees = bLatitudeDegrees;
    }
    if (pLongitudeDegrees != NULL) {
        *pLongitudeDegrees = bLongitudeDegrees;
    }
    if (pAzimuthDegrees != NULL) {
        *pAzimuthDegrees = bAzimuthDegrees;
    }
    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
# else
    (void) latitudeDegrees;
    (void) longitudeDegrees;
    (void) azimuthDegrees;
    (void) lengthMetres;
    (void) pLatitudeDegrees;
    (void) pLongitudeDegrees;
    (void) pAzimuthDegrees;
# endif

    return errorCode;
}

U_WEAK int32_t uGeofenceWgs84GeodInverse(double aLatitudeDegrees,
                                         double aLongitudeDegrees,
                                         double bLatitudeDegrees,
                                         double bLongitudeDegrees,
                                         double *pDistanceMetres,
                                         double *pAAzimuthDegrees,
                                         double *pBAzimuthDegrees)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TOO_BIG;

# ifdef U_CFG_GEOFENCE_USE_GEODESIC
    double distanceMetres = NAN;
    double aAzimuthDegrees = NAN;
    double bAzimuthDegrees = NAN;
    const GeographicLib::Geodesic &geod = GeographicLib::Geodesic::WGS84();

    if ((pAAzimuthDegrees == NULL) && (pBAzimuthDegrees == NULL)) {
        geod.Inverse(aLatitudeDegrees, aLongitudeDegrees,
                     bLatitudeDegrees, bLongitudeDegrees,
                     distanceMetres);
    } else if (pBAzimuthDegrees == NULL) {
        geod.Inverse(aLatitudeDegrees, aLongitudeDegrees,
                     bLatitudeDegrees, bLongitudeDegrees,
                     distanceMetres, aAzimuthDegrees);
    } else {
        geod.Inverse(aLatitudeDegrees, aLongitudeDegrees,
                     bLatitudeDegrees, bLongitudeDegrees,
                     distanceMetres, aAzimuthDegrees,
                     bAzimuthDegrees);
    }

    if (pDistanceMetres != NULL) {
        *pDistanceMetres = distanceMetres;
    }
    if (pAAzimuthDegrees != NULL) {
        *pAAzimuthDegrees = aAzimuthDegrees;
    }
    if (pBAzimuthDegrees != NULL) {
        *pBAzimuthDegrees = bAzimuthDegrees;
    }
    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
# else
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) pDistanceMetres;
    (void) pAAzimuthDegrees;
    (void) pBAzimuthDegrees;
# endif

    return errorCode;
}

U_WEAK int32_t uGeofenceWgs84LatitudeOfIntersection(double aLatitudeDegrees,
                                                    double aLongitudeDegrees,
                                                    double bLatitudeDegrees,
                                                    double bLongitudeDegrees,
                                                    double longitudeDegrees,
                                                    double *pLatitudeDegrees)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TOO_BIG;

# ifdef U_CFG_GEOFENCE_USE_GEODESIC
    double intersectLatitudeDegrees = NAN;
    double intersectLongitudeDegrees = NAN;
    const GeographicLib::Geodesic &geod = GeographicLib::Geodesic::WGS84();
    GeographicLib::Intersect intersect(geod);

    // Define the geodesic line between the points
    GeographicLib::GeodesicLine line = geod.InverseLine(aLatitudeDegrees,
                                                        aLongitudeDegrees,
                                                        bLatitudeDegrees,
                                                        bLongitudeDegrees);
    // Define the line of longitude
    GeographicLib::GeodesicLine meridian(geod, 0, longitudeDegrees, 0,
                                         GeographicLib::Intersect::LineCaps);
    // Find the intersection
    GeographicLib::Intersect::Point point = intersect.Closest(line, meridian);
    line.Position(point.first, intersectLatitudeDegrees, intersectLongitudeDegrees);
    if (pLatitudeDegrees != NULL) {
        *pLatitudeDegrees = intersectLatitudeDegrees;
    }
    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
# else
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) longitudeDegrees;
    (void) pLatitudeDegrees;
# endif

    return errorCode;
}

U_WEAK int32_t uGeofenceWgs84DistanceToSegment(double aLatitudeDegrees,
                                               double aLongitudeDegrees,
                                               double bLatitudeDegrees,
                                               double bLongitudeDegrees,
                                               double pointLatitudeDegrees,
                                               double pointLongitudeDegrees,
                                               double *pDistanceMetres)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TOO_BIG;

# ifdef U_CFG_GEOFENCE_USE_GEODESIC
    double distanceMetres = NAN;
    double maximum;
    double minimum;
    const GeographicLib::Geodesic &geod = GeographicLib::Geodesic::WGS84();
    GeographicLib::Gnomonic gnomonic(geod);

    // The way that Charles Karney recommands to do this is to convert
    // the three points we have into Gnomonic coordinates, a projection
    // in which geodesic lines become straight.  Then the point of
    // intersection can be performed in a flat X/Y space.

    // Need to find an origin for this space that keeps everything within range
    maximum = MAX(aLatitudeDegrees, bLatitudeDegrees);
    maximum = MAX(maximum, pointLatitudeDegrees);
    minimum = MIN(aLatitudeDegrees, bLatitudeDegrees);
    minimum = MIN(minimum, pointLatitudeDegrees);
    double originLatitudeDegrees = minimum + ((maximum - minimum) / 2);

    maximum = MAX(aLongitudeDegrees, bLongitudeDegrees);
    maximum = MAX(maximum, pointLongitudeDegrees);
    minimum = MIN(aLongitudeDegrees, bLongitudeDegrees);
    minimum = MIN(minimum, pointLongitudeDegrees);
    double originLongitudeDegrees = minimum + (longitudeSubtract(maximum, minimum) / 2);

    double ax;
    double ay;
    double bx;
    double by;
    double px;
    double py;
    gnomonic.Forward(originLatitudeDegrees, originLongitudeDegrees,
                     aLatitudeDegrees, aLongitudeDegrees, ax, ay);
    gnomonic.Forward(originLatitudeDegrees, originLongitudeDegrees,
                     bLatitudeDegrees, bLongitudeDegrees, bx, by);
    gnomonic.Forward(originLatitudeDegrees, originLongitudeDegrees,
                     pointLatitudeDegrees, pointLongitudeDegrees, px, py);

    // Note: there is an implementation of this which begins from
    // latitude/longitude coordinates and approximates over in
    // u_gnss_fence.c, distanceToSegment().
    double xDeltaPoint = px - ax;
    double yDeltaPoint = py - ay;
    double xDeltaLine = bx - ax;
    double yDeltaLine = by - ay;
    // dot represents the proportion of the distance along the line
    // that the "normal" projection of our point lands
    double dot = (xDeltaPoint * xDeltaLine) + (yDeltaPoint * yDeltaLine);
    double lineLengthSquared = (xDeltaLine * xDeltaLine) + (yDeltaLine * yDeltaLine);
    // param is a normalised version of dot, range 0 to 1
    double param = dot / lineLengthSquared;

    double x;
    double y;
    if (param < 0) {
        // Param is out of range, with A beyond our point, so use A
        x = ax;
        y = ay;
    } else if (param > 1) {
        // Param is out of range, with B beyond our point, so use B
        x = bx;
        y = by;
    } else {
        // In range, just grab the coordinates of where the normal
        // from the line is
        x = ax + (param * xDeltaLine);
        y = ay + (param * yDeltaLine);
    }

    // Now convert the coordinates x,y back into the real world
    double latitude;
    double longitude;
    gnomonic.Reverse(originLatitudeDegrees, originLongitudeDegrees,
                     x, y, latitude, longitude);

    // Finally, work out the distance between our point and x,y
    geod.Inverse(latitude, longitude,
                 pointLatitudeDegrees, pointLongitudeDegrees,
                 distanceMetres);

    if (pDistanceMetres != NULL) {
        *pDistanceMetres = distanceMetres;
    }

    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
# else
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) pointLatitudeDegrees;
    (void) pointLongitudeDegrees;
    (void) pDistanceMetres;
# endif

    return errorCode;
}

#endif // #ifdef U_CFG_GEOFENCE

// End of file
