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
 * @brief Implementation of the geofence API for GNSS, only available
 * if U_CFG_GEOFENCE is defined.
 *
 * Geofencing is quite an expensive thing to do: expensive in that it
 * requires floating point support in the C library and maths functions
 * (sin(), cos(), atan(), sqrt() etc.), so it costs code size and
 * processing time.  Hence this code is not compiled unless required,
 * by applying the build flag U_CFG_GEOFENCE.
 *
 * Geofencing is made harder because the question to be answered is not
 * "is this point inside or outside a geofence" but "does this circle
 * cross a geofence": this is because a point always has a radius of
 * uncertainty and, if this is not taken into account, you'll get the
 * wrong answer.
 *
 * The shapes in a geofence can be circles or polygons (i.e. sets of
 * three or more vertices) but, as well as their coordinates, when a
 * shape is added the "square extent" latitude/longitude of the shape
 * is also stored: this is the smallest "square", on the surface
 * of the earth, that will fit around the shape, plus a margin. The
 * margin (100 m) is sized to be larger than the radius of uncertainty
 * of most GNSS fixes, which means that, once we have a sufficiently
 * good fix, a quick latitude/longitude check can be performed to
 * eliminate a shape in a geofence from concern, without having to do
 * the expensive "distance to some wacky shape from a circle" maths.
 *
 * For large shapes (> 1 km) it is necessary to take the curvature of
 * the earth into account (this is where the cos()'s, sin()'s and
 * atan()'s come in).  More than that, the earth is not a sphere,
 * so to maintain accuracy something called WGS84 coordinates must be
 * used: all latitude/longitude vertices on a map or delivered via
 * GNSS use this coordinate system, which accounts for the "oblate
 * spheroid" nature of the planet [you'll note the term "geodesic"
 * creeps in: a geodesic distance is the shortest distance between
 * two points _across_a_surface_, in this case the agreed WGS84 shape
 * of the earth, which actually changes over time as tectonic plates
 * move, kind of WGSXX I guess].  The calculations that require WGS84
 * coordinates are split-out by this code (uGeofenceWgs84Xxx) so
 * that an application may implement them in its own sweet way; this
 * code also offers an integration with
 * https://github.com/geographiclib/geographiclib, an extremely useful
 * C library written by the very helpful Charles A Karney of MIT,
 * which provides the necessary functions.
 *
 * By default, if the uGeofenceWgs84Xxx functions are not populated,
 * this code uses the harversine formula (look it up), also full of
 * sin(), cos() and atan(), which assumes a spherical earth and hence
 * [according to the internet] may be out by as much as 0.5%, which
 * is quite a lot when you are dealing with kilometres.
 *
 * But, to keep things quick, if a position falls within the "square
 * extent" of a shape, and our uncertainty of position is less than
 * 100 m, then, if the shape is smaller than 1 km in size, this code
 * assumes that the local space is flat: now we can do calculations
 * without most of the trigonometry (sin(), cos(), atan() etc.), only
 * cos() is required (because the number of metres per degree of
 * longitude changes with latitude) and good 'ole:
 *
 * ```
 *                                 ----------------------------------
 *  distance between a and b =   / (alon - blon)^2 + (alat - blat)^2
 *                              v
 * ```
 *
 * ...(with the longitude wrap at 180 handled). Floating point maths
 * is still required, since sqrt() is needed to work out distances,
 * but a minimum of trigonometry because everything is close enough
 * together not to need it; flat earthers r'us.
 *
 * There is one exception to this: if the shape or the radius of
 * position are close to a pole the calculations have to remain
 * spherical.  This is because the horizontal and vertical axes
 * (which are of course the lines of latitude and longitude), even
 * at the 1 km scale, can no longer be considered to be at
 * 90 degrees.  This is set to being within 10 degrees of the pole;
 * a complete guess.
 *
 * Of course, large shapes, or at least shapes with vertices far
 * apart, also cannot be handled in this way, but that's just life;
 * the planet is what it is.
 *
 * If the uncertainty of position is larger than 100 m then it may
 * still be possible to eliminate shapes based on the speed that
 * the device is travelling and the previous known distance from
 * a geofence.  For this reason, #uGeofenceCallback_t is given the
 * minimum distance from the edge of the geofence, but ONLY if it
 * was necessary to calculate that distance to handle the geofence;
 * performing a distance calculation is relatively expensive so
 * if you don't need to do it you don't.  The caller may remember
 * this and pass it to the next testPosition() in the optional
 * pDynamic parameter, along with the timestamp and the maximum
 * speed.
 *
 * Summarizing:
 * - if radius of position > 100 m and previous distance/speed
 *   from geofence is known, see if the whole geofence can be
 *   eliminated based on this,
 * - otherwise, if radius of position > 100 m, or square extent of
 *   shape under test > 1 km, or either is within 10 degrees of a
 *   pole, use expensive spherical maths all of the time (WGS84,
 *   or haversine if WGS84 not available),
 * - otherwise, first use simple lat/long test to eliminate a shape
 *   under test,
 * - if shape under test is not eliminated and square extent of
 *   shape <= 1 km and radius of position <= 100 m then use less
 *   expensive, almost-trigonometry-less, "flat" X/Y maths.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MIN, INT_MAX, LLONG_MIN, LLONG_MAX
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#ifdef U_CFG_GEOFENCE
# include "math.h"     // sqrt(), cos(), etc.
#endif

#include "u_compiler.h"    // For U_WEAK, U_INLINE

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_linked_list.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"
#include "u_geofence_geodesic.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The radius of a spherical earth in metres.
 */
#define U_GEOFENCE_RADIUS_AT_EQUATOR_METERS 6378100

/** Pi as a float.
 */
#define U_GEOFENCE_PI_FLOAT 3.14159265358

/** The number of metres per degree along the longitudinal
 * axis: Pi * d / 360.
 */
#define U_GEOFENCE_METRES_PER_DEGREE_LATITUDE 111319

/** Limiting latitude value in degrees times ten to the power nine.
 */
#define U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9 90000000000LL

/** Limiting longitude value in degrees time ten to the power nine.
 */
#define U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9 180000000000LL

/** The maximum half-diagonal of a square extent: bigger than
 * this and it wraps more than half the earth.  The pole-to-pole
 * circumference of the earth is 40,000 km.
 */
#define U_GEOFENCE_MAX_SQUARE_EXTENT_HALF_DIAGONAL_METRES 10000000LL

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

/** The possible types of shape.
 */
typedef enum {
    U_GEOFENCE_SHAPE_TYPE_CIRCLE,
    U_GEOFENCE_SHAPE_TYPE_POLYGON
} uGeofenceShapeType_t;

/** Structure to hold a coordinate in latitude/longitude terms.
 */
typedef struct {
    double latitude;
    double longitude;
} uGeofenceCoordinates_t;

/** Structure to hold the square extent of a shape.
 */
typedef struct {
    uGeofenceCoordinates_t max;
    uGeofenceCoordinates_t min;
} uGeofenceSquare_t;

/** Structure to hold a circle.
 */
typedef struct {
    uGeofenceCoordinates_t centre;
    double radiusMetres;
} uGeofenceCircle_t;

/** Structure to hold a shape.
 */
typedef struct {
    uGeofenceShapeType_t type;
    union {
        uGeofenceCircle_t *pCircle;
        uLinkedList_t *pPolygon; /**< a linked list containing uGeofenceCoordinates_t. */
    } u;
    uGeofenceSquare_t squareExtent; /**< the square extent of the shape. */
    bool wgs84Required; /**< true if the shape is so big as to require WGS84 handling. */
} uGeofenceShape_t;

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

/** Mutex to protect the uGeofence API.
 */
static uPortMutexHandle_t gMutex = NULL;

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Ensure that the uGeofence API is initialised.
static void init()
{
    if (gMutex == NULL) {
        uPortMutexCreate(&gMutex);
    }
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: FENCE RELATED
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Return zero (success) if a fence is NOT in use.
static int32_t fenceNotInUse(uGeofence_t *pFence)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pFence != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_BUSY;
        if (pFence->referenceCount == 0) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Clear the map data contained in a polygon.
static void fenceClearMapDataPolygon(uLinkedList_t **ppPolygon)
{
    uLinkedList_t *pList;
    uLinkedList_t *pListNext;
    void *pVertex;

    if (ppPolygon != NULL) {
        pList = *ppPolygon;
        while (pList != NULL) {
            pVertex = pList->p;
            pListNext = pList->pNext;
            uLinkedListRemove(ppPolygon, pVertex);
            uPortFree(pVertex);
            pList = pListNext;
        }
    }
}

// Clear the map data contained in a fence.
static void fenceClearMapData(uGeofence_t *pFence)
{
    uLinkedList_t *pList;
    uLinkedList_t *pListNext;
    uGeofenceShape_t *pShape;

    if (pFence != NULL) {
        // Clear the list of shapes
        pList = pFence->pShapes;
        while (pList != NULL) {
            pShape = (uGeofenceShape_t *) pList->p;
            if (pShape != NULL) {
                switch (pShape->type) {
                    case U_GEOFENCE_SHAPE_TYPE_CIRCLE:
                        uPortFree(pShape->u.pCircle);
                        break;
                    case U_GEOFENCE_SHAPE_TYPE_POLYGON:
                        fenceClearMapDataPolygon(&(pShape->u.pPolygon));
                        break;
                    default:
                        break;
                }
            }
            pListNext = pList->pNext;
            uLinkedListRemove(&(pFence->pShapes), pShape);
            uPortFree(pShape);
            pList = pListNext;
        }
        // Reset the altitude limits and the position state
        pFence->altitudeMillimetresMax = INT_MAX;
        pFence->altitudeMillimetresMin = INT_MIN;
        pFence->positionState = U_GEOFENCE_POSITION_STATE_NONE;
        pFence->distanceMinMillimetres = LLONG_MIN;
    }
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: TRIGONOMETRY
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Convert an angle in degrees to radians.
static double degreesToRadians(double degrees)
{
    return degrees * U_GEOFENCE_PI_FLOAT / 180;
}

// Convert an angle in radians to degrees.
static double radiansToDegrees(double radians)
{
    return radians * 180 / U_GEOFENCE_PI_FLOAT;
}

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

// As longitudeSubtract() but with values in radians.
static double longitudeSubtractRadians(double aLongitudeRadians,
                                       double bLongitudeRadians)
{
    double difference = aLongitudeRadians - bLongitudeRadians;

    if (difference <= -U_GEOFENCE_PI_FLOAT) {
        difference += 2 * U_GEOFENCE_PI_FLOAT;
    } else if (difference >= U_GEOFENCE_PI_FLOAT) {
        difference -= 2 * U_GEOFENCE_PI_FLOAT;
    }

    return difference;
}

// Return the number of metres per degree longitude
// at the given latitude.  Only works  within a space
// small enough not to require WGS84 coordinates.
static double longitudeMetresPerDegree(double latitude)
{
    // The number of metres per degree longitude at the equator
    // is Pi * d / 360, so at a given latitude we multiply by
    // cos of the latitude in radians
    return U_GEOFENCE_PI_FLOAT * U_GEOFENCE_RADIUS_AT_EQUATOR_METERS * 2 *
           cos(degreesToRadians(latitude)) / 360;
}

// Return the distance between two points on a spherical earth;
// from https://www.movable-type.co.uk/scripts/latlong.html
static double haversine(const uGeofenceCoordinates_t *pA, const uGeofenceCoordinates_t *pB)
{
    // EVERYTHING INSIDE HERE IS IN RADIANS

    double latitudeDeltaRadians = degreesToRadians(pB->latitude - pA->latitude);
    double longitudeDeltaRadians = degreesToRadians(longitudeSubtract(pB->longitude, pA->longitude));
    double aLatitudeRadians = degreesToRadians(pA->latitude);
    double bLatitudeRadians = degreesToRadians(pB->latitude);

    double sinHalfLatitudeDelta = sin(latitudeDeltaRadians / 2);
    double sinHalfLongitudeDelta = sin(longitudeDeltaRadians / 2);
    double squareHalfChord = (sinHalfLatitudeDelta * sinHalfLatitudeDelta) +
                             cos(aLatitudeRadians) * cos(bLatitudeRadians) *
                             sinHalfLongitudeDelta * sinHalfLongitudeDelta;
    double angularDistanceRadians = 2 * atan2(sqrt(squareHalfChord), sqrt(1 - squareHalfChord));
    if (angularDistanceRadians < 0) {
        angularDistanceRadians = - angularDistanceRadians;
    }

    return angularDistanceRadians * U_GEOFENCE_RADIUS_AT_EQUATOR_METERS;
}

// Calculate the coordinates of a point at a given distance and azimuth from
// another point on a spherical earth, from
// https://www.movable-type.co.uk/scripts/latlong.html
static void reverseHaversine(double latitude, double longitude,
                             double azimuthDegrees, double lengthMetres,
                             double *pLatitude, double *pLongitude)
{
    // EVERYTHING INSIDE HERE IS IN RADIANS

    double startLatitudeRadians = degreesToRadians(latitude);
    double azimuthRadians = degreesToRadians(azimuthDegrees);

    double lengthOverR = lengthMetres / U_GEOFENCE_RADIUS_AT_EQUATOR_METERS;
    double sinLatitude = sin(startLatitudeRadians);
    double cosLatitude = cos(startLatitudeRadians);
    double sinLengthOverR = sin(lengthOverR);
    double cosLengthOverR = cos(lengthOverR);
    double sinAzimuth = sin(azimuthRadians);
    double cosAzimuth = cos(azimuthRadians);

    double endLatitudeRadians = asin((sinLatitude * cosLengthOverR) + (cosLatitude * sinLengthOverR *
                                                                       cosAzimuth));
    if (pLatitude != NULL) {
        *pLatitude = radiansToDegrees(endLatitudeRadians);
    }
    if (pLongitude != NULL) {
        double longitudeRadians = atan2(sinAzimuth * sinLengthOverR * cosLatitude,
                                        cosLengthOverR - (sinLatitude * sin(endLatitudeRadians)));
        // Do this via longitudeSubtract() to handle the wrap
        *pLongitude = longitudeSubtract(longitude, -radiansToDegrees(longitudeRadians));
    }
}

// The intersection calculation here is derived from the equation for
// the intersection of two great circles.  The original is "Intersection of
// two paths" at https://www.movable-type.co.uk/scripts/latlong.html.
// IMPORTANT: this function doesn't always behave (for narrow angles or
// meridian/equatorial lines).  Should it detect that this is the case
// it will still give an answer but will also return false.
// Implementation note: it would be possible, of course, to have
// sub-functions to obtain bearing etc. but then it wouldn't be possible
// to re-use the cosine/sine values across this function.
static bool latitudeOfIntersectionSpherical(const uGeofenceCoordinates_t *pA,
                                            const uGeofenceCoordinates_t *pB,
                                            double longitude,
                                            double *pIntersectLatitude)
{
    bool success = true;
    // Return nan by default
    double intersectLatitudeRadians = NAN;

    // EVERYTHING INSIDE HERE IS IN RADIANS

    // Throw out the simple cases first, otherwise these can cause
    // infinities to appear in the calculation below
    if (longitude == pA->longitude) {
        intersectLatitudeRadians = degreesToRadians(pA->latitude);
    } else if (longitude == pB->longitude) {
        intersectLatitudeRadians = degreesToRadians(pB->latitude);
    } else {
        // First need to get the azimuth of our first great circle from the two
        // points we've been given.
        double aToBDeltaLongitudeRadians = degreesToRadians(longitudeSubtract(pB->longitude,
                                                                              pA->longitude));
        double bLatitudeRadians = degreesToRadians(pB->latitude);
        // This is just latitudeRadians, not aLatitudeRadians or oneLatitudeRadians,
        // for reasons that will become clear below
        double latitudeRadians = degreesToRadians(pA->latitude);
        double oneLongitudeRadians = degreesToRadians(pA->longitude);
        double cosBLatitude = cos(bLatitudeRadians);

        // Calculate the bearing from A to B
        double oneAzimuthRadians = atan2(sin(aToBDeltaLongitudeRadians) * cosBLatitude,
                                         (cos(latitudeRadians) * sin(bLatitudeRadians)) -
                                         (sin(latitudeRadians) * cosBLatitude * cos(aToBDeltaLongitudeRadians)));
        if (oneAzimuthRadians < 0) {
            oneAzimuthRadians = (U_GEOFENCE_PI_FLOAT * 2) + oneAzimuthRadians;
        }

        // That's circle one created, with its reference point, p1, being pA

        // Circle two is our other "line", the reference point, p2, of which
        // we give the same latitude as the reference point of circle one;
        // since circle two is a meridian the latitude is arbitrary and making
        // the latitude the same as for the reference point of circle one
        // simplifies the calculations, as can be seen below, effectively
        // giving us a right-angle triangle to work with.  The longitude of p2
        // is that passed in.  The azimuth is 0, north, if pB is above pA,
        // or south if pB is below pA, which comes out in the setting of
        // oneAngle below
        double twoLongitudeRadians = degreesToRadians(longitude);

        // These values are used multiple times below, so derive them once here
        double cosLatitude = cos(latitudeRadians);
        double oneTwoDeltaLongitude = longitudeSubtractRadians(twoLongitudeRadians, oneLongitudeRadians);
        double sinHalfOneTwoDeltaLongitude = sin(oneTwoDeltaLongitude / 2);
        // For the generic calculation we would also derive oneTwoDeltaLatitude
        // and sin() of it, but since the latitudes of p1 and p2 are the
        // same, these both come out to be zero.

        // Next, work out the "angular distance" between p1 and p2, our reference
        // points on circles one and two.  This is the angle between the two points
        // subtended at the centre of the earth, so the chord across that angle,
        // at the radius of the earth, is the distance between the two points,
        // hence the term angular distance.
        //
        // The calculation was originally:
        //
        // 2 * asin(sqrt((sinHalfOneTwoDeltaLatitude *
        //                sinHalfOneTwoDeltaLatitude) +
        //               (cosOneLatitude * cosTwoLatitude *
        //                sinHalfOneTwoDeltaLongitude * sinHalfOneTwoDeltaLongitude)));
        //
        // However, since we've chosen the latitudes of the reference points
        // to be the same, sinHalfOneTwoDeltaLatitude is zero and cosTwoLatitude
        // is the same as cosOneLatitude, so we can lose the square root, just
        // need to make sure any sign disappears
        double thingToASin = cosLatitude  * sinHalfOneTwoDeltaLongitude;
        if (thingToASin < 0) {
            thingToASin = -thingToASin;
        }
        double oneTwoAngularDistance = 2 * asin(thingToASin);

        // The generic calculation would now work out the bearing from
        // p1 to p2, and back from p2 to p1, with:
        //
        // acos((sinTwoLatitude - (sinOneLatitude * cos(oneTwoAngularDistance))) /
        //      (sin(oneTwoAngularDistance) * cosOneLatitude))
        //
        // ...for p1 to p2, and:
        //
        // acos((sinOneLatitude - (sinTwoLatitude * cosOneTwoAngularDistance)) /
        //      (sinOneTwoAngularDistance * cosTwoLatitude))
        //
        // ... for p2 to p1.
        //
        // However, for our simplified case, with the latitudes of the two
        // reference points being the same, it is a horizontal line, and we
        // only need the p1 to p2 direction.
        double oneTwoBearing = U_GEOFENCE_PI_FLOAT / 2;
        if (oneTwoDeltaLongitude < 0) {
            oneTwoBearing =  U_GEOFENCE_PI_FLOAT / 2 * 3;
        }

        // The intersection point will be on both great circles, a point
        // we can think of as p3.  Now that we have the bearing between
        // p1 and p2, we can work out all the angles of the "triangle on
        // a sphere" they form, which will be:
        //
        //            .   .
        //              . .
        //                x  p3, the point of intersection of the circles
        //                . .
        //                .    .
        //                .       .
        //                p2         . p1 (pA) on circle one
        //           on circle two
        //
        // ...or:
        //
        //                p2         . p1 (pA) on circle one
        //                .       .
        //                .    .
        //                . .
        //                x  p3
        //              . .
        //            .   .
        //
        //... or the mirror image of both, noting that, in our
        // simplified case, the angle at p2 will always be 90 degrees,
        // so sinTwo will be either 1 or -1 and cosTwo will always
        // be zero.
        //
        // Angle p2–p1–p3
        double oneAngle = oneAzimuthRadians - oneTwoBearing;
        if (oneTwoDeltaLongitude > 0) {
            oneAngle = -oneAngle;
        }
        double sinOne = sin(oneAngle);
        // Angle p1–p2–p3
        int32_t sinTwo = 1;
        if (oneAngle < 0) {
            sinTwo = -1;
        }

        if ((sinOne == 1) || (sinOne == -1)) {
            // The circles are on top of one another so there are
            // an infinite number of solutions: return NAN
            // This _shouldn't_ occur, because of the check
            // at the top, but it is kept just in case of rounding
            // errors
            success = false;
            // Note: the original calculation had a check for
            // sinOne and sinTwo being of opposites signs
            // but in our simplified case that can never occur
        } else {
            double cosOne = cos(oneAngle);
            // Angle p2–p3–p1, where the first term disappears
            // because cosTwo is zero
            double threeAngle = acos(/*(-cosOne * cosTwo) + */ sinOne * sinTwo * cos(oneTwoAngularDistance));

            // Now work out the angular distance from point one to point three
            // (in which cosTwo disappears)
            double oneThreeAngularDistance = atan2(sin(oneTwoAngularDistance) * sinOne * sinTwo,
                                                   /* cosTwo + */ cosOne * cos(threeAngle));

            // Now, finally, we can work out the latitude of point three
            intersectLatitudeRadians = asin((sin(latitudeRadians) * cos(oneThreeAngularDistance)) +
                                            (cos(latitudeRadians) * sin(oneThreeAngularDistance) *
                                             cos(oneAzimuthRadians)));
        }
    }

    if (pIntersectLatitude != NULL) {
        *pIntersectLatitude = radiansToDegrees(intersectLatitudeRadians);
    }

    return success;
}

// Return the distance in metres between a point and the line between two
// other points: from the great advice "Cross-track distance" at
// https://www.movable-type.co.uk/scripts/latlong.html, but also taking
// into account finite line length.
// Implementation note: it would be possible, of course, to have sub-functions
// to obtain bearing etc. but then it wouldn't be possible to re-use the
// cosine/sine values across this function.
static double distanceToSegmentSpherical(const uGeofenceCoordinates_t *pA,
                                         const uGeofenceCoordinates_t *pB,
                                         const uGeofenceCoordinates_t *pPoint)
{
    // EVERYTHING INSIDE HERE IS IN RADIANS

    double angularDistanceRadians = 0;
    double aLatitudeRadians = degreesToRadians(pA->latitude);
    double bLatitudeRadians = degreesToRadians(pB->latitude);
    double pointLatitudeRadians = degreesToRadians(pPoint->latitude);
    double aToPointDeltaLatitudeRadians = degreesToRadians(pPoint->latitude - pA->latitude);
    double aToPointDeltaLongitudeRadians = degreesToRadians(longitudeSubtract(pPoint->longitude,
                                                                              pA->longitude));
    double aToBDeltaLongitudeRadians = degreesToRadians(longitudeSubtract(pB->longitude,
                                                                          pA->longitude));
    // These values are used multiple times below, so derive them once here
    double cosPointLatitude = cos(pointLatitudeRadians);
    double cosALatitude = cos(aLatitudeRadians);
    double cosBLatitude = cos(bLatitudeRadians);
    double sinALatitude = sin(aLatitudeRadians);
    double sinHalfAToPointDeltaLatitude = sin(aToPointDeltaLatitudeRadians / 2);
    double sinHalfAToPointDeltaLongitude = sin(aToPointDeltaLongitudeRadians / 2);

    // Calculate the angular distance from A to our point
    double aToPointSquareHalfChord = (sinHalfAToPointDeltaLatitude * sinHalfAToPointDeltaLatitude) +
                                     cosALatitude * cosPointLatitude *
                                     sinHalfAToPointDeltaLongitude * sinHalfAToPointDeltaLongitude;
    double aToPointAngularDistance = 2 * atan2(sqrt(aToPointSquareHalfChord),
                                               sqrt(1 - aToPointSquareHalfChord));

    // Calculate the bearing from A to our point,
    // azimuth being clockwise from north with anticlockwise
    // being negative
    double aToPointAzimuthRadians = atan2(sin(aToPointDeltaLongitudeRadians) * cosPointLatitude,
                                          (cosALatitude * sin(pointLatitudeRadians)) -
                                          (sinALatitude * cosPointLatitude * cos(aToPointDeltaLongitudeRadians)));

    // Calculate the bearing from A to B
    double aToBAzimuthRadians = atan2(sin(aToBDeltaLongitudeRadians) * cosBLatitude,
                                      (cosALatitude * sin(bLatitudeRadians)) -
                                      (sinALatitude * cosBLatitude * cos(aToBDeltaLongitudeRadians)));

    // If the difference in the bearings is greater than 90 degrees
    // then there isn't a normal from the great circle to our point,
    // the distance is just that from our point to point A
    double azimuthDeltaRadians = aToPointAzimuthRadians - aToBAzimuthRadians;
    if (azimuthDeltaRadians < 0) {
        azimuthDeltaRadians = -azimuthDeltaRadians;
    }
    // Always need the smallest angle
    if (azimuthDeltaRadians > U_GEOFENCE_PI_FLOAT) {
        azimuthDeltaRadians = (U_GEOFENCE_PI_FLOAT * 2) - azimuthDeltaRadians;
    }
    if (azimuthDeltaRadians > U_GEOFENCE_PI_FLOAT / 2) {
        angularDistanceRadians = aToPointAngularDistance;
    } else {
        // The distance _might_ be to a point along the great circle, work it out
        angularDistanceRadians = asin(sin(aToPointAngularDistance) *
                                      sin(aToPointAzimuthRadians - aToBAzimuthRadians));
        // Need to abs() angularDistanceRadians here 'cos it is used in a comparison below
        if (angularDistanceRadians < 0) {
            angularDistanceRadians = -angularDistanceRadians;
        }
        // Now check if that is beyond the end of the segment
        double aToBDeltaLatitudeRadians =  degreesToRadians(pB->latitude - pA->latitude);
        double sinHalfAToBDeltaLatitude = sin(aToBDeltaLatitudeRadians / 2);
        double sinHalfAToBDeltaLongitude = sin(aToBDeltaLongitudeRadians / 2);
        double aToBSquareHalfChord = (sinHalfAToBDeltaLatitude * sinHalfAToBDeltaLatitude) +
                                     cosALatitude * cosBLatitude *
                                     sinHalfAToBDeltaLongitude * sinHalfAToBDeltaLongitude;
        double aToBAngularDistanceRadians = 2 * atan2(sqrt(aToBSquareHalfChord),
                                                      sqrt(1 - aToBSquareHalfChord));
        if (aToBAngularDistanceRadians < 0) {
            aToBAngularDistanceRadians = -aToBAngularDistanceRadians;
        }
        if (aToBAngularDistanceRadians < angularDistanceRadians) {
            // The distance is beyond the end of the segment, so the one
            // we want is actually that from our point to point B.  TODO:
            // there might be a shorter way to do this, given all we have above
            double bToPointDeltaLatitudeRadians = degreesToRadians(pPoint->latitude - pB->latitude);
            double bToPointDeltaLongitudeRadians = degreesToRadians(longitudeSubtract(pPoint->longitude,
                                                                                      pB->longitude));
            double sinHalfBToPointDeltaLatitude = sin(bToPointDeltaLatitudeRadians / 2);
            double sinHalfBToPointDeltaLongitude = sin(bToPointDeltaLongitudeRadians / 2);
            double bToPointSquareHalfChord = (sinHalfBToPointDeltaLatitude * sinHalfBToPointDeltaLatitude) +
                                             cosPointLatitude * cosBLatitude *
                                             sinHalfBToPointDeltaLongitude * sinHalfBToPointDeltaLongitude;
            angularDistanceRadians = 2 * atan2(sqrt(bToPointSquareHalfChord),
                                               sqrt(1 - bToPointSquareHalfChord));
        }
    }
    if (angularDistanceRadians < 0) {
        angularDistanceRadians = -angularDistanceRadians;
    }

    return angularDistanceRadians * U_GEOFENCE_RADIUS_AT_EQUATOR_METERS;
}

// Return the distance between two points on a flat plane.
static double distanceXY(const uGeofenceCoordinates_t *pA,
                         const uGeofenceCoordinates_t *pB,
                         double metresPerDegreeLongitude)
{
    double x = longitudeSubtract(pA->longitude, pB->longitude) * metresPerDegreeLongitude;
    double y = (pA->latitude - pB->latitude) * U_GEOFENCE_METRES_PER_DEGREE_LATITUDE;
    return sqrt((x * x) + (y * y));
}

// The distance between two points in metres; WGS84, spherical or XY,
// calling the above as appropriate.
static double distanceBetweenPoints(const uGeofenceCoordinates_t *pA,
                                    const uGeofenceCoordinates_t *pB,
                                    double metresPerDegreeLongitude,
                                    bool wgs84Required)
{
    double distanceMetres = NAN;
    bool success;

    if (wgs84Required) {
        // Need to take into account the true shape of the earth, if possible
        success = (uGeofenceWgs84GeodInverse(pA->latitude, pA->longitude,
                                             pB->latitude, pB->longitude,
                                             &distanceMetres, NULL, NULL) == 0);
        if (!success) {
            // Don't have a WGS84 answer, do it spherically
            distanceMetres = haversine(pA, pB);
        }
    } else {
        // The earth is flat
        distanceMetres = distanceXY(pA, pB, metresPerDegreeLongitude);
    }

    return distanceMetres;
}

// Given a line between two points, populate pLatitude with the
// latitude at which the given line of longitude, at the given
// azimuth, cuts it; WGS84, spherical or XY, as appropriate.
static bool latitudeOfIntersection(const uGeofenceCoordinates_t *pA,
                                   const uGeofenceCoordinates_t *pB,
                                   double longitude,
                                   bool wgs84Required,
                                   double *pLatitude)
{
    double intersectLatitude = NAN;
    bool success = false;

    if (wgs84Required) {
        // Need to take into account the true shape of the earth, if
        // possible.
        success = (uGeofenceWgs84LatitudeOfIntersection(pA->latitude,
                                                        pA->longitude,
                                                        pB->latitude,
                                                        pB->longitude,
                                                        longitude,
                                                        &intersectLatitude) == 0);
        if (!success) {
            // Don't have a WGS84 answer, do it spherically
            success = latitudeOfIntersectionSpherical(pA, pB,
                                                      longitude,
                                                      &intersectLatitude);
        }
        success = success && (intersectLatitude == intersectLatitude); // nan test
    } else {
        success = true;
        // Cut latitude (y) = start latitude (yA) + difference in longitude (xADelta) * slope (yAB/xAB)
        // Note: seems a bit strange to use the aLatitude/aLongitude local variables
        // below but if you don't MSVC somehow gets the contents confused
        // during the calculation
        double aLatitude = pA->latitude;
        double aLongitude = pA->longitude;
        // codechecker_suppress [readability-suspicious-call-argument]
        double longitudeDelta = longitudeSubtract(longitude, aLongitude);
        double slope = (pB->latitude - aLatitude) / longitudeSubtract(pB->longitude, aLongitude);
        intersectLatitude = aLatitude + (longitudeDelta * slope);
    }

    if (pLatitude != NULL) {
        *pLatitude = intersectLatitude;
    }

    return success;
}

// The shortest distance from a point to a line segment in metres;
// WGS84, spherical or XY, calling the above as appropriate.
static double distanceToSegment(const uGeofenceCoordinates_t *pA,
                                const uGeofenceCoordinates_t *pB,
                                const uGeofenceCoordinates_t *pPoint,
                                double metresPerDegreeLongitude,
                                bool wgs84Required)
{
    double distanceMetres = NAN;
    bool success;

    if (wgs84Required) {
        success = (uGeofenceWgs84DistanceToSegment(pA->latitude,
                                                   pA->longitude,
                                                   pB->latitude,
                                                   pB->longitude,
                                                   pPoint->latitude,
                                                   pPoint->longitude,
                                                   &distanceMetres) == 0);
        if (!success) {
            // Don't have a WGS84 answer, have to do it spherically
            distanceMetres = distanceToSegmentSpherical(pA, pB, pPoint);
        }
    } else {
        // Note: there is an implementation of this, using pure X/Y,
        // over in u_geofence_geodesic.cpp in
        // uGeofenceWgs84DistanceToSegment()
        double xDeltaPoint =  longitudeSubtract(pPoint->longitude,
                                                pA->longitude) * metresPerDegreeLongitude;
        double yDeltaPoint = (pPoint->latitude - pA->latitude) * U_GEOFENCE_METRES_PER_DEGREE_LATITUDE;
        double xDeltaLine = longitudeSubtract(pB->longitude, pA->longitude) * metresPerDegreeLongitude;
        double yDeltaLine = (pB->latitude - pA->latitude) * U_GEOFENCE_METRES_PER_DEGREE_LATITUDE;
        // dot represents the proportion of the distance along the line
        // that the "normal" projection of our point lands
        double dot = (xDeltaPoint * xDeltaLine) + (yDeltaPoint * yDeltaLine);
        double lineLengthSquared = (xDeltaLine * xDeltaLine) + (yDeltaLine * yDeltaLine);
        // param is a normalised version of dot, range 0 to 1
        double param = dot / lineLengthSquared;

        double longitude;
        double latitude;
        if (param < 0) {
            // Param is out of range, with A beyond our point, so use A
            longitude = pA->longitude;
            latitude = pA->latitude;
        } else if (param > 1) {
            // Param is out of range, with B beyond our point, so use B
            longitude = pB->longitude;
            latitude = pB->latitude;
        } else {
            // In range, just grab the coordinates of where the normal
            // from the line is
            longitude = pA->longitude + (param * xDeltaLine / metresPerDegreeLongitude);
            latitude = pA->latitude + (param * yDeltaLine / U_GEOFENCE_METRES_PER_DEGREE_LATITUDE);
        }
        double xDelta = longitudeSubtract(pPoint->longitude, longitude) * metresPerDegreeLongitude;
        double yDelta = (pPoint->latitude - latitude) * U_GEOFENCE_METRES_PER_DEGREE_LATITUDE;
        distanceMetres = sqrt((xDelta * xDelta) + (yDelta * yDelta));
    }

    return distanceMetres;
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SHAPE RELATED
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Return true if the latitude of a vertex puts it close enough to
// the pole that we cannot use X/Y maths.
static bool atAPole(double latitude, double radiusMetres)
{
    if (latitude < 0) {
        latitude = -latitude;
    }
    if (radiusMetres > 0) {
        latitude += radiusMetres / U_GEOFENCE_METRES_PER_DEGREE_LATITUDE;
    }

    return (latitude > 90 - U_GEOFENCE_WGS84_THRESHOLD_POLE_DEGREES_FLOAT);
}

// Update the square extent and the wgs84Required flag of a shape.
static void updateSquareExtentAndWgs84(uGeofenceShape_t *pShape)
{
    uGeofenceSquare_t *pSquareExtent = NULL;
    bool success = false;
    uGeofenceSquare_t squareExtent = {0};

    if (pShape != NULL) {
        pSquareExtent = &(pShape->squareExtent);
        switch (pShape->type) {
            case U_GEOFENCE_SHAPE_TYPE_CIRCLE: {
                squareExtent = *pSquareExtent;
                // For the circle we need to convert the centre plus a distance
                // (the radius plus the square extent uncertainty margin) into
                // latitude/longitude, which may require WGS84 coordinates if the
                // circle is big enough
                uGeofenceCircle_t *pCircle = pShape->u.pCircle;
                double radiusMetres = pCircle->radiusMetres;
                double latitude = pCircle->centre.latitude;
                double longitude = pCircle->centre.longitude;
                // Check the diameter and the proximity-with-a-pole for deciding
                // to work in WGS84 coordinates
                if (((radiusMetres * 2) > U_GEOFENCE_WGS84_THRESHOLD_METRES) ||
                    atAPole(latitude, pCircle->radiusMetres)) {
                    pShape->wgs84Required = true;
                }
                // Extend the radius to reach the corner point including the uncertainty
                radiusMetres += U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES;
                radiusMetres *= 1.4142;
                if (radiusMetres > U_GEOFENCE_MAX_SQUARE_EXTENT_HALF_DIAGONAL_METRES) {
                    // Too big, can't do a square extent check
                    squareExtent.max.latitude = NAN;
                    squareExtent.max.longitude = NAN;
                    squareExtent.min.latitude = NAN;
                    squareExtent.min.longitude = NAN;
                } else {
                    if (pShape->wgs84Required) {
                        // Try to get the corner points in WGS84-speak
                        uGeofenceCoordinates_t max = squareExtent.max;
                        uGeofenceCoordinates_t min = squareExtent.min;
                        success = (uGeofenceWgs84GeodDirect(latitude, longitude,
                                                            45, radiusMetres,
                                                            &max.latitude,
                                                            &max.longitude,
                                                            NULL) == 0) &&
                                  (uGeofenceWgs84GeodDirect(latitude, longitude,
                                                            225, radiusMetres,
                                                            &min.latitude,
                                                            &min.longitude,
                                                            NULL) == 0);
                        // Do a NAN check and, if anything has not been calculated,
                        // set success to false so that we fall back to spherical
                        // for this case; it should be good enough for a square-extent
                        // check
                        if (success &&
                            ((max.latitude != max.latitude) ||
                             (max.longitude != max.longitude) ||
                             (min.latitude != min.latitude) ||
                             (min.longitude != min.longitude))) {
                            success = false;
                        }
                        if (success) {
                            squareExtent.max = max;
                            squareExtent.min = min;
                        }
                    }
                    if (!success) {
                        // Either WGS didn't work or we don't need it, the earth is a sphere
                        reverseHaversine(latitude, longitude, 45, radiusMetres, &squareExtent.max.latitude,
                                         &squareExtent.max.longitude);
                        reverseHaversine(latitude, longitude, 225, radiusMetres, &squareExtent.min.latitude,
                                         &squareExtent.min.longitude);
                    }
                }
            }
            break;
            case U_GEOFENCE_SHAPE_TYPE_POLYGON: {
                uLinkedList_t *pList = pShape->u.pPolygon;
                // Note: on the face of it, we could only work with the
                // last vertex here, since all of the other vertices could
                // already have been taken into account. However we need
                // to add the uncertainty margin on top of the square
                // extent and, if we did that incrementally each time,
                // we would end up adding it multiple times.  Hence we
                // recalculate the square extent entirely when a vertex
                // is added.  It is not a huge overhead to do this when
                // first adding a shape, much better than doing it on
                // each position calculation
                if (pList != NULL) {
                    uGeofenceCoordinates_t *pVertex = (uGeofenceCoordinates_t *) pList->p;
                    squareExtent.max = *pVertex;
                    squareExtent.min = *pVertex;
                    pList = pList->pNext;
                    while (pList != NULL) {
                        pVertex = (uGeofenceCoordinates_t *) pList->p;
                        if (pVertex->latitude > squareExtent.max.latitude) {
                            squareExtent.max.latitude = pVertex->latitude;
                        } else if (pVertex->latitude < squareExtent.min.latitude) {
                            squareExtent.min.latitude = pVertex->latitude;
                        }
                        if (longitudeSubtract(pVertex->longitude, squareExtent.max.longitude) > 0) {
                            squareExtent.max.longitude = pVertex->longitude;
                        } else if (longitudeSubtract(squareExtent.min.longitude, pVertex->longitude) > 0) {
                            squareExtent.min.longitude = pVertex->longitude;
                        }
                        pList = pList->pNext;
                    }
                }
                // Having done all that, work out the diagonal and decide if it is big enough
                // to need WGS84 coordinates, not resetting the flag if it was already
                // set due to a previous square extent check of the polygon; or of course
                // if it is near a pole
                double diagonal = haversine(&squareExtent.max, &squareExtent.min);
                if (!pShape->wgs84Required) {
                    pShape->wgs84Required = (diagonal > U_GEOFENCE_WGS84_THRESHOLD_METRES) ||
                                            atAPole(squareExtent.max.latitude, 0) ||
                                            atAPole(squareExtent.min.latitude, 0);
                }
                if (diagonal > U_GEOFENCE_MAX_SQUARE_EXTENT_HALF_DIAGONAL_METRES) {
                    // Too big, can't do a square extent check
                    squareExtent.max.latitude = NAN;
                    squareExtent.max.longitude = NAN;
                    squareExtent.min.latitude = NAN;
                    squareExtent.min.longitude = NAN;
                } else {
                    // Now we can add the uncertainty margin on top for the square extent;
                    // spherical is fine for this, in fact linear would be fine, but the
                    // reverseHaversine handles any wraps
                    reverseHaversine(squareExtent.max.latitude, squareExtent.max.longitude, 45,
                                     U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1.4142,
                                     &squareExtent.max.latitude, &squareExtent.max.longitude);
                    reverseHaversine(squareExtent.min.latitude, squareExtent.min.longitude, 225,
                                     U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1.4142,
                                     &squareExtent.min.latitude, &squareExtent.min.longitude);
                }
            }
            break;
            default:
                break;
        }
        *pSquareExtent = squareExtent;
    }
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: TEST RELATED
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Take account of an uncertain outcome in any preceding test; this
// should be called after ANY test that populates uncertainty.
static uGeofencePositionState_t testAccountForUncertainty(uGeofenceTestType_t testType,
                                                          bool pessimisticNotOptimistic,
                                                          uGeofencePositionState_t positionState,
                                                          uGeofencePositionState_t previousPositionState)
{
    // Uncertainty means that the current position state would be
    // reversed if the radius of position or uncertainty in altitude
    // was taken into account; we may need to modify the current
    // position state depending on whether we take a pessimistic
    // or optimistic view
    if (positionState != U_GEOFENCE_POSITION_STATE_NONE) {
        // Only need to do this if we have an inside or an outside state
        if ((testType == U_GEOFENCE_TEST_TYPE_INSIDE) ||
            ((testType == U_GEOFENCE_TEST_TYPE_TRANSIT) &&
             (previousPositionState == U_GEOFENCE_POSITION_STATE_INSIDE))) {
            if (positionState == U_GEOFENCE_POSITION_STATE_INSIDE) {
                // Want to be on the inside and seem to be on the inside but
                // there is uncertainty; the pessimist changes their conclusion
                if (pessimisticNotOptimistic) {
                    positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                }
            } else {
                // Want to be on the inside but seem to be on the outside; however
                // there is uncertainty and so the optimist changes their conclusion
                if (!pessimisticNotOptimistic) {
                    positionState = U_GEOFENCE_POSITION_STATE_INSIDE;
                }
            }
        } else if ((testType == U_GEOFENCE_TEST_TYPE_OUTSIDE) ||
                   ((testType == U_GEOFENCE_TEST_TYPE_TRANSIT) &&
                    (previousPositionState == U_GEOFENCE_POSITION_STATE_OUTSIDE))) {
            if (positionState == U_GEOFENCE_POSITION_STATE_OUTSIDE) {
                // Want to be on the outside and seem to be on the outside but
                // there is uncertainty; the pessimist changes their conclusion
                if (pessimisticNotOptimistic) {
                    positionState = U_GEOFENCE_POSITION_STATE_INSIDE;
                }
            } else {
                // Want to be on the outside but seem to be on the inside; however
                // there is uncertainty and so the optimist changes their conclusion
                if (!pessimisticNotOptimistic) {
                    positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                }
            }
        }
    }

    return positionState;
}

// Test the state of a position with respect to altitude; all this
// can do is return OUTSIDE if not within the altitude range, taking
// pessimism and the uncertainty into account.
static uGeofencePositionState_t testAltitude(const uGeofence_t *pFence,
                                             int32_t altitudeMillimetres,
                                             uGeofenceTestType_t testType,
                                             bool pessimisticNotOptimistic,
                                             uGeofencePositionState_t previousPositionState,
                                             int32_t uncertaintyMillimetres)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;
    bool couldBeIn = false;
    bool couldBeOut = false;

    if (altitudeMillimetres != INT_MIN) {
        if ((altitudeMillimetres > pFence->altitudeMillimetresMax) ||
            (altitudeMillimetres < pFence->altitudeMillimetresMin)) {
            // The outcome ignoring uncertainty
            positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
        }
        if (uncertaintyMillimetres > 0) {
            if (altitudeMillimetres > pFence->altitudeMillimetresMax) {
                if (altitudeMillimetres - uncertaintyMillimetres <= pFence->altitudeMillimetresMax) {
                    // Uncertainty could bring the altitude down into range
                    couldBeIn = true;
                }
            } else {
                if (altitudeMillimetres >= pFence->altitudeMillimetresMin) {
                    if ((altitudeMillimetres - uncertaintyMillimetres < pFence->altitudeMillimetresMin) ||
                        (altitudeMillimetres + uncertaintyMillimetres > pFence->altitudeMillimetresMax)) {
                        // Uncertainty could send the altitude either up or down out of range
                        couldBeOut = true;
                    }
                } else {
                    if (altitudeMillimetres + uncertaintyMillimetres >= pFence->altitudeMillimetresMin) {
                        // Uncertainty could bring the altitude up into range
                        couldBeIn = true;
                    }
                }
            }
            switch (testType) {
                case U_GEOFENCE_TEST_TYPE_INSIDE:
                    if (couldBeIn && !pessimisticNotOptimistic) {
                        // We are checking for inside; if we _could_ be inside and
                        // are an optimist then don't eliminate the position yet
                        positionState = U_GEOFENCE_POSITION_STATE_NONE;
                    } else if (couldBeOut && pessimisticNotOptimistic) {
                        // We are checking for inside but we _could_ be outside
                        // so a pessimist would eliminate the position
                        positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                    }
                    break;
                case U_GEOFENCE_TEST_TYPE_OUTSIDE:
                    if (couldBeIn && pessimisticNotOptimistic) {
                        // We are checking for outside; if we _could_ be
                        // inside and are a pessimist then don't eliminate
                        // the position yet
                        positionState = U_GEOFENCE_POSITION_STATE_NONE;
                    } else if (couldBeOut && !pessimisticNotOptimistic) {
                        // We are checking for outside; if we _could_ be
                        // outside then an optimist would eliminate the position
                        positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                    }
                    break;
                case U_GEOFENCE_TEST_TYPE_TRANSIT:
                    switch (previousPositionState) {
                        case U_GEOFENCE_POSITION_STATE_OUTSIDE:
                            if (couldBeIn && pessimisticNotOptimistic) {
                                // If the previous position state was outside and
                                // we _could_ be inside then a pessimist would
                                // think there had been a transition, so carry
                                // on checking if we really are inside
                                positionState = U_GEOFENCE_POSITION_STATE_NONE;
                            } else if (couldBeOut && !pessimisticNotOptimistic) {
                                // If the previous position state was outside and
                                // we _could_ be outside then an optimist would
                                // happy with that, nothing more to do
                                positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                            }
                            break;
                        case U_GEOFENCE_POSITION_STATE_INSIDE:
                            if (couldBeIn && !pessimisticNotOptimistic) {
                                // If the previous position state was inside and we
                                // _could_ be inside then an optimist would be happy
                                // with that, carry on checking if we really are
                                // inside
                                positionState = U_GEOFENCE_POSITION_STATE_NONE;
                            } else if (couldBeOut && pessimisticNotOptimistic) {
                                // If the previous position state was inside and
                                // we _could_ be outside then a pessimist would
                                // take that, nothing more to do
                                positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return positionState;
}

// Test a position against the square extent of a shape.
static uGeofencePositionState_t testSquareExtent(const uGeofenceSquare_t *pSquareExtent,
                                                 const uGeofenceCoordinates_t *pCoordinates)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;

    if ((pSquareExtent->max.latitude == pSquareExtent->max.latitude) &&
        // NAN check, only need to do one
        ((pCoordinates->latitude > pSquareExtent->max.latitude) ||
         (pCoordinates->latitude < pSquareExtent->min.latitude) ||
         (longitudeSubtract(pCoordinates->longitude, pSquareExtent->max.longitude) > 0) ||
         (longitudeSubtract(pCoordinates->longitude, pSquareExtent->min.longitude) < 0))) {
        positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
    }

    return positionState;
}

// Test if the previous distance and maximum speed of the position eliminates it.
static uGeofencePositionState_t testSpeed(const uGeofenceDynamic_t *pPreviousDistance)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;
    int32_t timeNowMs;
    int64_t distanceTravelledMillimetres;

    if ((pPreviousDistance->lastStatus.distanceMillimetres != LLONG_MIN) &&
        (pPreviousDistance->maxHorizontalSpeedMillimetresPerSecond >= 0)) {
        // Work out how far we can have travelled in the time
        timeNowMs = uPortGetTickTimeMs();
        // Guard against wrap
        if (timeNowMs > pPreviousDistance->lastStatus.timeMs) {
            // Divide by 1000 below to get per second
            distanceTravelledMillimetres = ((int64_t) (timeNowMs - pPreviousDistance->lastStatus.timeMs)) *
                                           pPreviousDistance->maxHorizontalSpeedMillimetresPerSecond / 1000;
            if (distanceTravelledMillimetres < pPreviousDistance->lastStatus.distanceMillimetres) {
                positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
            }
        }
    }

    return positionState;
}

// Test the state of a position with respect to a circle.
static uGeofencePositionState_t testCircle(const uGeofenceCircle_t *pCircle,
                                           bool wgs84Required,
                                           double metresPerDegreeLongitude,
                                           const uGeofenceCoordinates_t *pCoordinates,
                                           int32_t uncertaintyMillimetres,
                                           double *pDistanceMetres,
                                           bool *pUncertain)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;
    double distanceMetres;

    *pDistanceMetres = NAN;
    *pUncertain = false;
    distanceMetres = distanceBetweenPoints(&pCircle->centre, pCoordinates,
                                           metresPerDegreeLongitude, wgs84Required);
    if (distanceMetres == distanceMetres) { // NAN test
        distanceMetres -= pCircle->radiusMetres;

        // Have the distance from our point to the edge of the circle,
        // which will be negative if we are inside it.
        positionState = U_GEOFENCE_POSITION_STATE_INSIDE;
        if (distanceMetres > 0) {
            positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
        }

        // Check if the uncertainty changes the outcome
        if (distanceMetres < 0) {
            distanceMetres = -distanceMetres;
        }
        *pDistanceMetres = distanceMetres;
        *pUncertain = (uncertaintyMillimetres >= distanceMetres * 1000);
    }

    return positionState;
}

// Test the state of a position with respect to a polygon.
//
// The solution here is the "point in polygon" ray-casting
// method, which takes advantage of the observation that if you
// draw a line in an arbitrary direction from a point that
// is inside a polygon then the line will cross the perimeter
// of the polygon an odd number of times, whereas if the same
// point is outside the polygon the line will cross the
// perimeter an even number of times.
//
// But it is not a simple as that, since what we have is
// not a point but a point with uncertainty.  So while
// we are doing the "point in polygon" check with each side
// of the polygon we also check if the shortest distance
// to a side is greater than our radius of uncertainty
// (in which case the point could be beyond the side)
// and report that as well.
//
// ...and, it is not quite as simple as that either, since
// we don't actually have a flat plane, we are wrapped
// around a globe, so our ray, rather than fading off
// into the distance, loops around to the opposite side
// and comes back again.
//
// The algorithm below handles these problems, plus some
// corner cases which occur when you happen to draw a line
// that hits a vertex or hits a side, side-on.
//
// 0:  Start with:
//     0.0) A set of vertices, in order, e.g.:
//                                          0
//                               2        / |
//                                \ \    /  |
//                                 \ \  /   |
//                                  \  3    |
//                                   \      |
//                                    \     |
//                                     \    |
//                                      \   |
//                                       \  |
//                                        \ |
//                                          1
//    0.1) a point you want to test.
//    0.2) a flag, "IS INSIDE", set to false, the state
//         of which is inverted by a FLIP.
//    0.3) a flag, "IS UNCERTAIN", set to false.
//
// 1: IF the point is outside the square formed by the
//    vertices, it is NOT inside, stop.  Where it is possible
//    to do such a check, this check will already have been
//    performed by testSquareExtent().
//
// 2: ELSE check if the latitude/longitude of our point is
//    THE SAME as any of our vertices: if so it is inside,
//    FLIP and stop.
//    2.1) AND IF radius of position > 0 set "IS UNCERTAIN"
//         to true.
//
// 3: ELSE, test each segment, as follows, with our ray chosen
//    to be a line of longitude heading NORTH of our point:
//    3.0) Do the simple check: IF both vertices of a segment
//         are to the left OR both are to the right of our point,
//         OR if BOTH are below our point, then do nothing
//         (no intersection).
//    3.1) ELSE, IF our ray passes throught a vertex then IF the other
//         vertex of the segment is to the LEFT of our point, FLIP;
//         if it is EQUAL or to the RIGHT of our point then do
//         nothing (no intersection).
//    3.2) ELSE, if the difference between the longitude of our point to
//         the longitudes of both vertices of the segment, when added
//         together, are more than 180 degrees then do nothing; the
//         segment is on the other side of the planet.
//    3.3) ELSE, calculate the latitude of the intersection
//         of our ray with the segment: if that latitude is
//         ABOVE OR EQUAL to the latitude of our point then FLIP
//         (i.e. there is an intersection, this is the "obvious" case).
//    3.4) ALSO, IF "IS UNCERTAIN"  is false, check if the shortest
//         distance from our point to the segment is less than the
//         radius of position: if so, set "IS UNCERTAIN" to true.
//
// 4: When all segments have been tested or skipped the states of
//    "IS INSIDE" and "IS UNCERTAIN" are correct.
//
static uGeofencePositionState_t testPolygon(const uLinkedList_t *pPolygon,
                                            bool wgs84Required,
                                            double metresPerDegreeLongitude,
                                            const uGeofenceCoordinates_t *pCoordinates,
                                            int32_t uncertaintyMillimetres,
                                            double *pDistanceMetres,
                                            bool *pUncertain)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;
    int32_t vertexCount = 0;
    bool isInside = false;
    bool exitNow = false;
    bool calculationFailure = false;
    const uLinkedList_t *pTmp = pPolygon;
    uGeofenceCoordinates_t *pSide[2] = {0};
    double cutLatitude = NAN;
    double distanceMetres;
    double distanceMinMetres = NAN;

    *pDistanceMetres = NAN;
    *pUncertain = false;

    // First count the number of vertices
    while ((pTmp != NULL) && (pTmp->p != NULL)) {
        vertexCount++;
        pTmp = pTmp->pNext;
    }
    if (vertexCount >= 3) {
        // Check all sides making sure to check the final
        // side which links back to the first vertex
        while ((vertexCount >= 0) && !exitNow) {
            if (pTmp == NULL) {
                // This sets us up at the beginning and also
                // at the end, to pick up the first vertexCount
                // that ends the last side
                pTmp = pPolygon;
            }
            pSide[0] = (uGeofenceCoordinates_t *) pTmp->p;
            if (pSide[0] != NULL) {
                // Now have a side which starts at pSide[1] and ends at pSide[0]
                if ((pSide[0]->latitude == pCoordinates->latitude) &&
                    (pSide[0]->longitude == pCoordinates->longitude)) {
                    // Check 2 has been met, we're in
                    isInside = true;
                    if (uncertaintyMillimetres > 0) {
                        // ...uncertainly
                        *pUncertain = true;
                    }
                    exitNow = true;
                } else {
                    if (pSide[1] != NULL) {
                        // These things are used multiple times below so set them out here
                        double longitude1Delta = longitudeSubtract(pCoordinates->longitude, pSide[1]->longitude);
                        double longitude0Delta = longitudeSubtract(pCoordinates->longitude, pSide[0]->longitude);
                        bool sideIsBelow = (pSide[1]->latitude < pCoordinates->latitude) &&
                                           (pSide[0]->latitude < pCoordinates->latitude);
                        // Check 3.0
                        if ((((longitude1Delta > 0) && (longitude0Delta > 0)) ||
                             ((longitude1Delta < 0) && (longitude0Delta < 0))) || sideIsBelow) {
                            // No intersection
                        } else {
                            // Check 3.1
                            bool vertex1Intersection = (pSide[1]->longitude == pCoordinates->longitude) &&
                                                       (pSide[1]->latitude >= pCoordinates->latitude);
                            bool vertex0Intersection = (pSide[0]->longitude == pCoordinates->longitude) &&
                                                       (pSide[0]->latitude >= pCoordinates->latitude);
                            if (vertex1Intersection || vertex0Intersection) {
                                if ((vertex1Intersection && (longitude0Delta > 0)) ||
                                    (vertex0Intersection && (longitude1Delta > 0))) {
                                    // Flip
                                    isInside = !isInside;
                                }
                            } else {
                                // Check 3.2
                                double longitude1DeltaAbs = longitude1Delta;
                                if (longitude1DeltaAbs < 0) {
                                    longitude1DeltaAbs = -longitude1DeltaAbs;
                                }
                                double longitude0DeltaAbs = longitude0Delta;
                                if (longitude0DeltaAbs < 0) {
                                    longitude0DeltaAbs = -longitude0DeltaAbs;
                                }
                                if ((longitude1DeltaAbs + longitude0DeltaAbs <= 180)) {
                                    // Check 3.3: need to do some calculations
                                    calculationFailure = !latitudeOfIntersection(pSide[1], pSide[0],
                                                                                 pCoordinates->longitude,
                                                                                 wgs84Required,
                                                                                 &cutLatitude);
                                    if (calculationFailure) {
                                        exitNow = true;
                                    } else {
                                        if (cutLatitude >= pCoordinates->latitude) {
                                            // Flip
                                            isInside = !isInside;
                                        }
                                    }
                                }
                            }
                        }
                        // Check 3.4
                        if (!*pUncertain && (uncertaintyMillimetres > 0)) {
                            // Check if the shortest distance between the side
                            // and our point is less than the uncertainty
                            distanceMetres = distanceToSegment(pSide[1], pSide[0], pCoordinates,
                                                               metresPerDegreeLongitude, wgs84Required);
                            calculationFailure = (distanceMetres != distanceMetres);  // NAN test
                            if (calculationFailure) {
                                exitNow = true;
                            } else {
                                if ((distanceMinMetres != distanceMinMetres) || // NAN test
                                    (distanceMetres < distanceMinMetres)) {
                                    distanceMinMetres = distanceMetres;
                                }
                                *pUncertain = (uncertaintyMillimetres > distanceMetres * 1000);
                            }
                        }
                    }
                    pSide[1] = pSide[0];
                }
            }
            vertexCount--;
            pTmp = pTmp->pNext;
        }

        if (!calculationFailure) {
            *pDistanceMetres = distanceMinMetres;
            positionState = U_GEOFENCE_POSITION_STATE_OUTSIDE;
            if (isInside) {
                positionState = U_GEOFENCE_POSITION_STATE_INSIDE;
            }
        }
    }

    return positionState;
}

// Check whether we need to carry on testing the next shape.
static bool testKeepGoing(uGeofencePositionState_t positionState)
{
    // Inside any shape means we're done
    return !(positionState == U_GEOFENCE_POSITION_STATE_INSIDE);
}

// Test a single position against a fence.
bool testPosition(const uGeofence_t *pFence,
                  uGeofenceTestType_t testType,
                  bool pessimisticNotOptimistic,
                  uGeofencePositionState_t *pPositionState,
                  uGeofenceDynamic_t *pDynamic,
                  int64_t latitudeX1e9,
                  int64_t longitudeX1e9,
                  int32_t altitudeMillimetres,
                  int32_t radiusMillimetres,
                  int32_t altitudeUncertaintyMillimetres)
{
    bool testIsMet = false;
    uGeofencePositionState_t positionState;
    uGeofencePositionState_t previousPositionState = U_GEOFENCE_POSITION_STATE_NONE;
    uLinkedList_t *pList;
    bool uncertain;
    uGeofenceCoordinates_t coordinates;
    uGeofenceShape_t *pShape;
    bool wgs84Required;
    double metresPerDegreeLongitude;
    double distanceMetres;
    double distanceMinMetres = NAN;

    if ((pFence != NULL) && (latitudeX1e9 < U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
        (latitudeX1e9 > -U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
        (longitudeX1e9 < U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9) &&
        (longitudeX1e9 >  -U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9) &&
        (radiusMillimetres >= 0)) {
        if (pPositionState != NULL) {
            previousPositionState = *pPositionState;
        }
        // First check the position against the altitude limits of the fence
        positionState = testAltitude(pFence, altitudeMillimetres, testType,
                                     pessimisticNotOptimistic, previousPositionState,
                                     altitudeUncertaintyMillimetres);
        // Only continue if we're not outside on altitude (since it is global, not shape-related)
        if (positionState != U_GEOFENCE_POSITION_STATE_OUTSIDE) {
            coordinates.latitude = ((double) latitudeX1e9) / 1000000000ULL;
            coordinates.longitude = ((double) longitudeX1e9) / 1000000000ULL;
            // Test if the position is too uncertain or is within the polar danger zone,
            // in which case we need WGS84 calculations all-round
            wgs84Required = (radiusMillimetres > U_GEOFENCE_WGS84_THRESHOLD_METRES * 1000) ||
                            atAPole(coordinates.latitude,
                                    // codechecker_suppress [bugprone-integer-division]
                                    (double) (radiusMillimetres / 1000) + 1); // +1 to round up;
            // Need this for the non-WGS84 world
            metresPerDegreeLongitude = longitudeMetresPerDegree(coordinates.latitude);
            // Then check the position against all of the shapes in the fence
            pList = pFence->pShapes;
            while (testKeepGoing(positionState) && (pList != NULL)) {
                pShape = (uGeofenceShape_t *) pList->p;
                if (pShape != NULL) {
                    positionState = U_GEOFENCE_POSITION_STATE_NONE;
                    // Before we bother checking a shape in detail, see if
                    // we can eliminate it based on square extent or speed
                    if (radiusMillimetres < U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000) {
                        positionState = testSquareExtent(&(pShape->squareExtent), &coordinates);
                    }
                    if ((positionState != U_GEOFENCE_POSITION_STATE_OUTSIDE) && (pDynamic != NULL)) {
                        positionState = testSpeed(pDynamic);
                    }
                    if (positionState != U_GEOFENCE_POSITION_STATE_OUTSIDE) {
                        uncertain = false;
                        distanceMetres = NAN;
                        switch (pShape->type) {
                            case U_GEOFENCE_SHAPE_TYPE_CIRCLE:
                                positionState = testCircle(pShape->u.pCircle,
                                                           wgs84Required || pShape->wgs84Required,
                                                           metresPerDegreeLongitude,
                                                           &coordinates,
                                                           radiusMillimetres,
                                                           &distanceMetres,
                                                           &uncertain);
                                break;
                            case U_GEOFENCE_SHAPE_TYPE_POLYGON:
                                positionState = testPolygon(pShape->u.pPolygon,
                                                            wgs84Required || pShape->wgs84Required,
                                                            metresPerDegreeLongitude,
                                                            &coordinates,
                                                            radiusMillimetres,
                                                            &distanceMetres,
                                                            &uncertain);
                                break;
                            default:
                                break;
                        }
                        if ((distanceMetres == distanceMetres) && // NAN test
                            ((distanceMinMetres != distanceMinMetres) || // NAN test
                             (distanceMetres < distanceMinMetres))) {
                            distanceMinMetres = distanceMetres;
                            if (distanceMinMetres < 0) {
                                distanceMinMetres = 0;
                            }
                        }
                        if (uncertain) {
                            // Take account of any uncertainty in the outcome
                            positionState = testAccountForUncertainty(testType,
                                                                      pessimisticNotOptimistic,
                                                                      positionState,
                                                                      previousPositionState);
                        }
                    }
                }
                pList = pList->pNext;
            }
            if (pDynamic != NULL) {
                pDynamic->lastStatus.distanceMillimetres = LLONG_MIN;
                if (positionState == U_GEOFENCE_POSITION_STATE_INSIDE) {
                    pDynamic->lastStatus.distanceMillimetres = 0;
                } else {
                    if (distanceMinMetres == distanceMinMetres) { // NAN test
                        pDynamic->lastStatus.distanceMillimetres = (int64_t) (distanceMinMetres * 1000);
                        pDynamic->lastStatus.timeMs = uPortGetTickTimeMs();
                    }
                }
            }
        }
        testIsMet = ((testType == U_GEOFENCE_TEST_TYPE_INSIDE) &&
                     (positionState == U_GEOFENCE_POSITION_STATE_INSIDE)) ||
                    ((testType == U_GEOFENCE_TEST_TYPE_OUTSIDE) &&
                     (positionState == U_GEOFENCE_POSITION_STATE_OUTSIDE)) ||
                    ((testType == U_GEOFENCE_TEST_TYPE_TRANSIT) &&
                     (previousPositionState != U_GEOFENCE_POSITION_STATE_NONE) &&
                     (positionState != U_GEOFENCE_POSITION_STATE_NONE) &&
                     (positionState != previousPositionState));
        if (pPositionState != NULL) {
            *pPositionState = positionState;
        }
    }

    return testIsMet;
}

#endif // U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE SHARED ONLY WITHIN UBXLIB, REQUIRED IFDEF U_CFG_GEOFENCE
 * -------------------------------------------------------------- */

#ifdef U_CFG_GEOFENCE

// Ensure that a geofence context exists, creating it if necessary.
int32_t uGeofenceContextEnsure(uGeofenceContext_t **ppFenceContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (ppFenceContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (*ppFenceContext == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            *ppFenceContext = (uGeofenceContext_t *) pUPortMalloc(sizeof(**ppFenceContext));
            if (*ppFenceContext != NULL) {
                memset(*ppFenceContext, 0, sizeof(**ppFenceContext));
                (*ppFenceContext)->dynamic.lastStatus.distanceMillimetres = LLONG_MIN;
                (*ppFenceContext)->dynamic.lastStatus.timeMs = uPortGetTickTimeMs();
                (*ppFenceContext)->dynamic.maxHorizontalSpeedMillimetresPerSecond = -1;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Apply a geofence to a geofence context.
int32_t uGeofenceApply(uGeofenceContext_t **ppFenceContext,
                       uGeofence_t *pFence)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((ppFenceContext != NULL) && (pFence != NULL)) {
        errorCode = uGeofenceContextEnsure(ppFenceContext);
        if ((*ppFenceContext != NULL) &&
            uLinkedListAdd(&((*ppFenceContext)->pFences), (void *) pFence)) {
            pFence->referenceCount++;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else {
            // Clean up on error
            uPortFree(*ppFenceContext);
        }
    }

    return errorCode;
}

// Remove the given geofence(s) from the given geofence context.
int32_t uGeofenceRemove(uGeofenceContext_t **ppFenceContext,
                        uGeofence_t *pFence)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uLinkedList_t *pList;
    uLinkedList_t *pListNext;

    if (ppFenceContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if ((*ppFenceContext != NULL) && ((*ppFenceContext)->pFences != NULL)) {
            if (pFence == NULL) {
                // Remove all the fences from the instance
                pList = (*ppFenceContext)->pFences;
                while (pList != NULL) {
                    pFence = (uGeofence_t *) pList->p;
                    if (pFence->referenceCount > 0) {
                        pFence->referenceCount--;
                    }
                    pListNext = pList->pNext;
                    uLinkedListRemove(&((*ppFenceContext)->pFences), pList->p);
                    pList = pListNext;
                }
            } else {
                // Just the one
                uLinkedListRemove(&((*ppFenceContext)->pFences), (void *) pFence);
                if (pFence->referenceCount > 0) {
                    pFence->referenceCount--;
                }
            }
        }
    }

    return errorCode;
}

// Apply a callback to the given geofence context.
int32_t uGeofenceSetCallback(uGeofenceContext_t **ppFenceContext,
                             uGeofenceTestType_t testType,
                             bool pessimisticNotOptimistic,
                             uGeofenceCallback_t *pCallback,
                             void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (ppFenceContext != NULL) {
        errorCode = uGeofenceContextEnsure(ppFenceContext);
        if (*ppFenceContext != NULL) {
            (*ppFenceContext)->testType = testType;
            (*ppFenceContext)->pessimisticNotOptimistic = false;
            (*ppFenceContext)->pCallback = NULL;
            (*ppFenceContext)->pCallbackParam = NULL;
            if (testType != U_GEOFENCE_TEST_TYPE_NONE) {
                (*ppFenceContext)->pessimisticNotOptimistic = pessimisticNotOptimistic;
                (*ppFenceContext)->pCallback = pCallback;
                (*ppFenceContext)->pCallbackParam = pCallbackParam;
            }
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Reset the memory of a fence.
void uGeofenceTestResetMemory(uGeofence_t *pFence)
{
    if (pFence != NULL) {
        pFence->positionState = U_GEOFENCE_POSITION_STATE_NONE;
    }
}

// Get last position state of a fence.
uGeofencePositionState_t uGeofenceTestGetPositionState(const uGeofence_t *pFence)
{
    uGeofencePositionState_t positionState = U_GEOFENCE_POSITION_STATE_NONE;

    if (pFence != NULL) {
        positionState = pFence->positionState;
    }

    return positionState;
}

// Get the last distance calculated by testPosition().
int64_t uGeofenceTestGetDistanceMin(const uGeofence_t *pFence)
{
    int64_t distanceMinMillimetres = LLONG_MIN;

    if (pFence != NULL) {
        distanceMinMillimetres = pFence->distanceMinMillimetres;
    }

    return distanceMinMillimetres;
}

#endif   // #ifdef U_CFG_GEOFENCE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE SHARED ONLY WITHIN UBXLIB, REQUIRED ALWAYS
 * -------------------------------------------------------------- */

// Test a position against the fences pointed-to by a geofence context.
uGeofencePositionState_t uGeofenceContextTest(uDeviceHandle_t devHandle,
                                              uGeofenceContext_t *pFenceContext,
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
    uGeofencePositionState_t fencePositionState;
    const uGeofence_t *pFence;
    uLinkedList_t *pList;
    uGeofenceDynamic_t dynamic;
    uGeofenceDynamic_t dynamicsMinDistance;
    uGeofenceTestType_t _testType;
    bool _pessimisticNotOptimistic;

    if ((pFenceContext != NULL) && (pFenceContext->pFences != NULL)) {
        pList = pFenceContext->pFences;
        _testType = pFenceContext->testType;
        _pessimisticNotOptimistic = pFenceContext->pessimisticNotOptimistic;
        if (testType != U_GEOFENCE_TEST_TYPE_NONE) {
            _testType = testType;
            _pessimisticNotOptimistic = pessimisticNotOptimistic;
        }
        dynamicsMinDistance = pFenceContext->dynamic;
        while (pList != NULL) {
            // Test against each fence and call the callback each
            // time, so that the callback gets to know whether the
            // position has met the test against each fence
            pFence = (const uGeofence_t *) pList->p;
            fencePositionState = pFenceContext->positionState;
            if (pFence != NULL) {
                dynamic = dynamicsMinDistance;
                testPosition(pFence, _testType,
                             _pessimisticNotOptimistic,
                             &fencePositionState,
                             &dynamic,
                             latitudeX1e9, longitudeX1e9,
                             altitudeMillimetres,
                             radiusMillimetres,
                             altitudeUncertaintyMillimetres);
                if (pFenceContext->positionState == U_GEOFENCE_POSITION_STATE_NONE) {
                    // If we've never updated the instance position state, do it now
                    pFenceContext->positionState = fencePositionState;
                } else {
                    // Otherwise, if the instance is inside any fence then its
                    // over all position state should remain "inside"; in other
                    // words "inside" should be sticky
                    if (fencePositionState == U_GEOFENCE_POSITION_STATE_INSIDE) {
                        pFenceContext->positionState = fencePositionState;
                    }
                }
                if (positionState == U_GEOFENCE_POSITION_STATE_NONE) {
                    // If we've never updated the over all instance position state,
                    // do it now
                    positionState = fencePositionState;
                } else {
                    // Again, for the over all instance position state,
                    // inside should be sticky
                    if (fencePositionState == U_GEOFENCE_POSITION_STATE_INSIDE) {
                        positionState = fencePositionState;
                    }
                }

                if ((dynamic.lastStatus.distanceMillimetres != LLONG_MIN) &&
                    (dynamic.lastStatus.distanceMillimetres < dynamicsMinDistance.lastStatus.distanceMillimetres)) {
                    dynamicsMinDistance.lastStatus.distanceMillimetres = dynamic.lastStatus.distanceMillimetres;
                    dynamicsMinDistance.lastStatus.distanceMillimetres = uPortGetTickTimeMs();
                }
                if ((pFenceContext->pCallback != NULL) && (devHandle != NULL)) {
                    pFenceContext->pCallback(devHandle, pFence, pFence->pNameStr,
                                             fencePositionState,
                                             latitudeX1e9, longitudeX1e9,
                                             altitudeMillimetres,
                                             radiusMillimetres,
                                             altitudeUncertaintyMillimetres,
                                             dynamic.lastStatus.distanceMillimetres,
                                             pFenceContext->pCallbackParam);
                }
            }
            pList = pList->pNext;
        }
        // Set the new over all position state of the instance
        // and the dynamic
        pFenceContext->positionState = positionState;
        pFenceContext->dynamic = dynamicsMinDistance;
    }
#else
    (void) devHandle;
    (void) pFenceContext;
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

// Clear out any geofences from a GNSS instance and free the context
// held by the instance.
void uGeofenceContextFree(uGeofenceContext_t **ppFenceContext)
{
#ifdef U_CFG_GEOFENCE
    uLinkedList_t *pList;
    uLinkedList_t *pListNext;

    if ((ppFenceContext != NULL) && (*ppFenceContext != NULL)) {
        pList = (*ppFenceContext)->pFences;
        while (pList != NULL) {
            pListNext = pList->pNext;
            uLinkedListRemove(&((*ppFenceContext)->pFences), pList->p);
            pList = pListNext;
        }
        uPortFree(*ppFenceContext);
        *ppFenceContext = NULL;
    }
#else
    (void) ppFenceContext;
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CREATING FENCES
 * -------------------------------------------------------------- */

// Create a new geofence.
uGeofence_t *pUGeofenceCreate(const char *pNameStr)
{
    uGeofence_t *pFence = NULL;

#ifdef U_CFG_GEOFENCE
    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pFence = (uGeofence_t *) pUPortMalloc(sizeof(*pFence));
        if (pFence != NULL) {
            memset(pFence, 0, sizeof(*pFence));
            // This will set up the defaults that are non-zero
            fenceClearMapData(pFence);
            pFence->pNameStr = pNameStr;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    (void) pNameStr;
#endif

    return pFence;
}

// Free a geofence.
int32_t uGeofenceFree(uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = fenceNotInUse(pFence);
        if (errorCode == 0) {
            // This will free memory occupied by the shapes in the geofence
            fenceClearMapData(pFence);
            uPortFree(pFence);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
#endif

    return errorCode;
}

// Add a circle to a geofence.
int32_t uGeofenceAddCircle(uGeofence_t *pFence,
                           int64_t latitudeX1e9, int64_t longitudeX1e9,
                           int64_t radiusMillimetres)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uGeofenceShape_t *pShape = NULL;
    uGeofenceCircle_t *pCircle = NULL;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((latitudeX1e9 < U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
            (latitudeX1e9 > -U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
            (longitudeX1e9 < U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9) &&
            (longitudeX1e9 > -U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9) &&
            (radiusMillimetres > 0)) {
            errorCode = fenceNotInUse(pFence);
            if (errorCode == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Get memory for the shape (don't populate it yet)
                pShape = (uGeofenceShape_t *) pUPortMalloc(sizeof(*pShape));
                if (pShape != NULL) {
                    memset(pShape, 0, sizeof(*pShape));
                    // Get storage for the circle and populate it
                    pCircle = pUPortMalloc(sizeof(uGeofenceCircle_t));
                    if (pCircle != NULL) {
                        pCircle->radiusMetres = ((double) radiusMillimetres) / 1000;
                        pCircle->centre.latitude = ((double) latitudeX1e9) / 1000000000ULL;
                        pCircle->centre.longitude = ((double) longitudeX1e9) / 1000000000ULL;
                    } else {
                        // Clean up on error
                        uPortFree(pShape);
                        pShape = NULL;
                    }
                }
                if (pShape != NULL) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    // If, after all that, we have a shape, populate it
                    pShape->type = U_GEOFENCE_SHAPE_TYPE_CIRCLE;
                    pShape->u.pCircle = pCircle;
                    // Update the square extent and wgs84Required
                    updateSquareExtentAndWgs84(pShape);
                    // Finally, add it to the list
                    if (!uLinkedListAdd(&(pFence->pShapes), pShape)) {
                        // Clean up on error
                        uPortFree(pCircle);
                        uPortFree(pShape);
                        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
    (void) latitudeX1e9;
    (void) longitudeX1e9;
    (void) radiusMillimetres;
#endif

    return errorCode;
}

// Add a vertex to a geofence.
int32_t uGeofenceAddVertex(uGeofence_t *pFence,
                           int64_t latitudeX1e9, int64_t longitudeX1e9,
                           bool newPolygon)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    uLinkedList_t *pList;
    uGeofenceShape_t *pShape = NULL;
    uGeofenceCoordinates_t *pVertex = NULL;
    uLinkedList_t **ppPolygon = NULL;

    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((latitudeX1e9 < U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
            (latitudeX1e9 > -U_GEOFENCE_LIMIT_LATITUDE_DEGREES_X1E9) &&
            (longitudeX1e9 < U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9) &&
            (longitudeX1e9 > -U_GEOFENCE_LIMIT_LONGITUDE_DEGREES_X1E9)) {
            errorCode = fenceNotInUse(pFence);
            if (errorCode == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Try to pick up the current shape, if it is a polygon
                if (pFence->pShapes != NULL) {
                    pList = pFence->pShapes;
                    while (pList->pNext != NULL) {
                        pList = pList->pNext;
                    }
                    pShape = (uGeofenceShape_t *) pList->p;
                    if ((pShape != NULL) && (pShape->type == U_GEOFENCE_SHAPE_TYPE_POLYGON)) {
                        ppPolygon = &pShape->u.pPolygon;
                    }
                }
                if ((ppPolygon == NULL) || newPolygon) {
                    newPolygon = true;
                    // Need a new shape: allocate one (don't populate it yet)
                    pShape = (uGeofenceShape_t *) pUPortMalloc(sizeof(*pShape));
                    if (pShape != NULL) {
                        memset(pShape, 0, sizeof(*pShape));
                        pShape->type = U_GEOFENCE_SHAPE_TYPE_POLYGON;
                        ppPolygon = &(pShape->u.pPolygon);
                    }
                }
                if (ppPolygon != NULL) {
                    // Allocate and populate the vertex
                    pVertex = pUPortMalloc(sizeof(uGeofenceCoordinates_t));
                    if (pVertex != NULL) {
                        pVertex->latitude = ((double) latitudeX1e9) / 1000000000ULL;
                        pVertex->longitude = ((double) longitudeX1e9) / 1000000000ULL;
                        // Add it to the list
                        if (uLinkedListAdd(ppPolygon, pVertex)) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            // Update the square extent and set wgs84Required
                            updateSquareExtentAndWgs84(pShape);
                            if (newPolygon) {
                                // If this is a new shape, add it to the list
                                if (!uLinkedListAdd(&(pFence->pShapes), pShape)) {
                                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                                    // Clean up on error
                                    uLinkedListRemove(ppPolygon, pVertex);
                                    uPortFree(pVertex);
                                    uPortFree(pShape);
                                }
                            }
                        } else {
                            // Clean up on error
                            uPortFree(pVertex);
                            if (newPolygon) {
                                uPortFree(pShape);
                            }
                        }
                    } else {
                        // Clean up on error
                        if (newPolygon) {
                            uPortFree(pShape);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
    (void) latitudeX1e9;
    (void) longitudeX1e9;
    (void) newPolygon;
#endif

    return errorCode;
}

// Set the maximum altitude of a geofence.
int32_t uGeofenceSetAltitudeMax(uGeofence_t *pFence,
                                int32_t altitudeMillimetres)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = fenceNotInUse(pFence);
        if (errorCode == 0) {
            pFence->altitudeMillimetresMax = altitudeMillimetres;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
    (void) altitudeMillimetres;
#endif

    return errorCode;
}

// Set the minimum altitude of a geofence.
int32_t uGeofenceSetAltitudeMin(uGeofence_t *pFence,
                                int32_t altitudeMillimetres)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = fenceNotInUse(pFence);
        if (errorCode == 0) {
            pFence->altitudeMillimetresMin = altitudeMillimetres;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
    (void) altitudeMillimetres;
#endif

    return errorCode;
}

// Clear all geofence data.
int32_t uGeofenceClearMap(uGeofence_t *pFence)
{
    int32_t errorCode;

#ifdef U_CFG_GEOFENCE
    errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Make sure that we are initialised
    init();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = fenceNotInUse(pFence);
        if (errorCode == 0) {
            fenceClearMapData(pFence);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    errorCode = (int32_t) U_ERROR_COMMON_NOT_COMPILED;
    (void) pFence;
#endif

    return errorCode;
}

// Test a position against a geofence.
bool uGeofenceTest(uGeofence_t *pFence, uGeofenceTestType_t testType,
                   bool pessimisticNotOptimistic,
                   int64_t latitudeX1e9,
                   int64_t longitudeX1e9,
                   int32_t altitudeMillimetres,
                   int32_t radiusMillimetres,
                   int32_t altitudeUncertaintyMillimetres)
{
    bool testIsMet = false;

#ifdef U_CFG_GEOFENCE
    uGeofencePositionState_t positionState;
    uGeofenceDynamic_t dynamic = {0};
    // Make sure that we are initialised
    init();

    if ((gMutex != NULL) && (pFence != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex);

        dynamic.lastStatus.distanceMillimetres = LLONG_MIN;
        dynamic.maxHorizontalSpeedMillimetresPerSecond = -1;
        positionState = pFence->positionState;
        testIsMet = testPosition(pFence, testType,
                                 pessimisticNotOptimistic,
                                 &positionState,
                                 &dynamic,
                                 latitudeX1e9, longitudeX1e9,
                                 altitudeMillimetres,
                                 radiusMillimetres,
                                 altitudeUncertaintyMillimetres);
        if (positionState != U_GEOFENCE_POSITION_STATE_NONE) {
            pFence->positionState = positionState;
            pFence->distanceMinMillimetres = dynamic.lastStatus.distanceMillimetres;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
#else
    (void) pFence;
    (void) testType;
    (void) pessimisticNotOptimistic;
    (void) latitudeX1e9;
    (void) longitudeX1e9;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) altitudeUncertaintyMillimetres;
#endif

    return testIsMet;
}

// Free gMutex.
void uGeofenceCleanUp()
{
#ifdef U_CFG_GEOFENCE
    if (gMutex != NULL) {
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
#endif
}

// End of file
