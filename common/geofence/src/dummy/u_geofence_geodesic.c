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
 * @brief Dummy C implementations of the geodesic functions, required
 * so that users of make (rather than CMake) can build geofence with
 * U_CFG_GEOFENCE on and use fences that are smaller than
 * 1 km in size, or larger ones but with only "spherical earth"
 * accuracy (up to 0.5% error).
 *
 * For the real-deal, see u_geofence_geodesic.cpp up in the src
 * directory.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_compiler.h"    // For U_WEAK

#include "u_error_common.h"

#include "u_geofence_geodesic.h"

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
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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
    (void) latitudeDegrees;
    (void) longitudeDegrees;
    (void) azimuthDegrees;
    (void) lengthMetres;
    (void) pLatitudeDegrees;
    (void) pLongitudeDegrees;
    (void) pAzimuthDegrees;

    return (int32_t) U_ERROR_COMMON_TOO_BIG;
}

U_WEAK int32_t uGeofenceWgs84GeodInverse(double aLatitudeDegrees,
                                         double aLongitudeDegrees,
                                         double bLatitudeDegrees,
                                         double bLongitudeDegrees,
                                         double *pDistanceMetres,
                                         double *pAAzimuthDegrees,
                                         double *pBAzimuthDegrees)
{
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) pDistanceMetres;
    (void) pAAzimuthDegrees;
    (void) pBAzimuthDegrees;

    return (int32_t) U_ERROR_COMMON_TOO_BIG;
}

U_WEAK int32_t uGeofenceWgs84LatitudeOfIntersection(double aLatitudeDegrees,
                                                    double aLongitudeDegrees,
                                                    double bLatitudeDegrees,
                                                    double bLongitudeDegrees,
                                                    double longitudeDegrees,
                                                    double *pLatitudeDegrees)
{
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) longitudeDegrees;
    (void) pLatitudeDegrees;

    return (int32_t) U_ERROR_COMMON_TOO_BIG;
}

U_WEAK int32_t uGeofenceWgs84DistanceToSegment(double aLatitudeDegrees,
                                               double aLongitudeDegrees,
                                               double bLatitudeDegrees,
                                               double bLongitudeDegrees,
                                               double pointLatitudeDegrees,
                                               double pointLongitudeDegrees,
                                               double *pDistanceMetres)
{
    (void) aLatitudeDegrees;
    (void) aLongitudeDegrees;
    (void) bLatitudeDegrees;
    (void) bLongitudeDegrees;
    (void) pointLatitudeDegrees;
    (void) pointLongitudeDegrees;
    (void) pDistanceMetres;

    return (int32_t) U_ERROR_COMMON_TOO_BIG;
}

#endif // #ifdef U_CFG_GEOFENCE

// End of file
