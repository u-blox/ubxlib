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

#ifndef _U_GEOFENCE_GEODESIC_H_
#define _U_GEOFENCE_GEODESIC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup Geofence
 *  @{
 */

/** @file
 * @brief This header file defines the functions which should
 * be provided if shapes greater than 1 km in size are to
 * be used in a geofence.  If you do nothing, weakly-linked
 * implementations of the functions defined in this file
 * will return false and a spherical earth will be assumed,
 * resulting in calculation errors up to 0.5% for large shapes.
 *
 * IMPORTANT: computationally, a true earth model is, of course,
 * the most expensive; probably 10 timed more than the spherical
 * case, think 10 to 100 ms of calculation time per position
 * for a polygon > 1 km on an average MCU (e.g. ESP32) and about
 * 5 kbytes more task stack required in ANY TASK where the
 * uGnssFenceXxx() functions are called and ANY TASK where
 * position calculations may take place.
 *
 * The functions are uGeofenceWgs84GeodInverse() and
 * uGeofenceWgs84GeodDirect() for circles and, in addition,
 * uGeofenceWgs84LatitudeOfIntersection() and
 * uGeofenceWgs84DistanceToSegment() for polygons.
 *
 * You may provide these functions yourself, in your own way,
 * or alternatively ubxlib provides an integration with
 * https://github.com/geographiclib, which is included as a
 * sub-module of the geofence directory.  GeographicLib requires
 * a C++ (11) compiler with support for exceptions switched-on
 * and only works on platforms that employ CMake in their
 * build process.
 *
 * The GeographicLib functions used by this code are good for
 * shapes that fall within a sector of radius a few thousand
 * kilometres.
 *
 * Instructions for all platforms except ESP-IDF:
 *
 * To use this integration, add "geodesic" (without quotes)
 * to the existing UBXLIB_FEATURES CMake variable in your
 * CMakeLists.txt file; this will cause ubxlib.cmake to bring
 * in the necessary code to build a library whose name it will
 * export in the CMake variable UBXLIB_EXTRA_LIBS, which you
 * should pass to your link stage (e.g. add it to
 * target_link_libraries()).  FYI, ubxlib.cmake will also
 * populate a CMake variable UBXLIB_COMPILE_OPTIONS containing
 * some compilation flags that are required when ubxlib is
 * built, but the addition of those compilation options to
 * your build is _already_ done inside linux.cmake, windows.cmake
 * and the CMakeLists.txt file that is employed when ubxlib is
 * included as a Zephyr module, hence you do not need to worry
 * about it.
 *
 * Instructions for ESP-IDF:
 *
 * Since ESP-IDF has its own component system, the instructions
 * are slightly different.  In this case a component "geodesic"
 * is provided, in the same location as the "ubxlib" component.
 * You must still add "geodesic" (without quotes) to your
 * existing UBXLIB_FEATURES CMake variable in your CMakeLists.txt
 * file but you can ignore the UBXLIB_EXTRA_LIBS CMake variable
 * and the target_link_libraries() stage.  The only thing you
 * need to worry about is, if you are specifying a reduced list
 * of components in your build to save build time, please add
 * "geodesic" to that list of components.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * GEODESIC FUNCTIONS THAT YOU MUST PROVIDE IF YOU WANT TO USE LARGE GEOFENCES
 * -------------------------------------------------------------- */

/** Given a point and a bearing from that point in a given direction
 * for a given distance, work out the WGS84 coordinates at the end.
 *
 * If the function is unable to calculate any of the parameters
 * requested it should populate those variables with NAN and return
 * success.
 *
 * YOU MUST PROVIDE an implementation of this function if you wish to
 * use large circles or large polygons (e.g. more than a kilometre in
 * square extent) in your geofence with an accuracy better than 0.5%
 * worst case.
 *
 * @param latitudeDegrees         the latitude of the starting point in
 *                                degrees.
 * @param longitudeDegrees        the longitude of the starting point in
 *                                degrees.
 * @param azimuthDegrees          the bearing, in degrees clockwise from north.
 * @param distanceMetres          the distance in metres.
 * @param[out] pLatitudeDegrees   a pointer to a place to store the latitude
 *                                at the end, in degrees; may be NULL if the
 *                                latitude is not required.
 * @param[out] pLongitudeDegrees  a pointer to a place to store the longitude
 *                                at the end in degrees; may be NULL if the
 *                                longitude is not required.
 * @param[out] pAzimuthDegrees    a pointer to a place to put the azimuth
 *                                at the end; may be NULL if this is not
 *                                required.
 * @return                        zero on success, i.e. if pLatitudeDegrees,
 *                                pLongitudeDegrees and pAzimuthDegrees, where
 *                                non-NULL, have been populated, else negative
 *                                error code.
 */
int32_t uGeofenceWgs84GeodDirect(double latitudeDegrees,
                                 double longitudeDegrees,
                                 double azimuthDegrees,
                                 double distanceMetres,
                                 double *pLatitudeDegrees,
                                 double *pLongitudeDegrees,
                                 double *pAzimuthDegrees);

/** Work out the shortest distance between two points on the earth
 * in WGS84 coordinates.
 *
 * If the function is unable to calculate any of the parameters
 * requested it should populate those variables with NAN and return
 * success.
 *
 * YOU MUST PROVIDE an implementation of this function if you wish to
 * use large circles or large polygons (e.g. more than a kilometre in
 * square extent) in your geofence with an accuracy better than 0.5%
 * worst case.
 *
 * @param aLatitudeDegrees       the latitude of point (a) in degrees.
 * @param aLongitudeDegrees      the longitude of point (a) in degrees.
 * @param bLatitudeDegrees       the latitude of point (b) in degrees.
 * @param bLongitudeDegrees      the longitude of point (b) in degrees.
 * @param[out] pDistanceMetres   a pointer to a place to put the distance
 *                               between (a) and (b) in metres; may be
 *                               NULL if the distance is not required.
 * @param[out] pAAzimuthDegrees  a pointer to a place to put the azimuth
 *                               at point (a) in degrees from north; may
 *                               be NULL if the azimuth is not required.
 * @param[out] pBAzimuthDegrees  a pointer to a place to put the azimuth
 *                               of point (b) in degrees from north; may
 *                               be NULL if the azimuth is not required.
 * @return                       zero on success, i.e. if pDistanceMetres
 *                               pAAzimuthDegrees and pBAzimuthDegrees
 *                               have been populated, where non-NULL,
 *                               else negative error code.
 */
int32_t uGeofenceWgs84GeodInverse(double aLatitudeDegrees,
                                  double aLongitudeDegrees,
                                  double bLatitudeDegrees,
                                  double bLongitudeDegrees,
                                  double *pDistanceMetres,
                                  double *pAAzimuthDegrees,
                                  double *pBAzimuthDegrees);

/** Given two points on the surface of the earth, work out the
 * latitude at which a line between those two points is cut by a
 * line of longitude, in WGS84 coordinates.  The line betweeen the
 * two points can be considered to be a great circle, i.e. there
 * is no need to check which side of the start and end points the
 * cut falls.
 *
 * If the function is unable to calculate the intersection it
 * should populate pLatitudeDegrees with NAN and return success.
 *
 * YOU MUST PROVIDE an implementation of this function if you wish to
 * use large polygons (e.g. more than a kilometre in square extent)
 * in your geofence with an accuracy better than 0.5% worst case.
 *
 * @param aLatitudeDegrees      the latitude of one end of the line
 *                              in degrees.
 * @param aLongitudeDegrees     the longitude of one end of the line
 *                              in degrees.
 * @param bLatitudeDegrees      the latitude of the other end of the
 *                              line in degrees.
 * @param bLongitudeDegrees     the longitude of the other end of the
 *                              line in degrees.
 * @param longitudeDegrees      the longitude of the cut line in
 *                              degrees.
 * @param[out] pLatitudeDegrees a pointer to a place to put the latitude
 *                              of the intersection between (a)-(b) and
 *                              the line of longitude; will never be NULL.
 * @return                      zero on success, that is if there has
 *                              been an intersection and pLatitudeDegrees
 *                              has been populated, else negative error
 *                              code.
 */
int32_t uGeofenceWgs84LatitudeOfIntersection(double aLatitudeDegrees,
                                             double aLongitudeDegrees,
                                             double bLatitudeDegrees,
                                             double bLongitudeDegrees,
                                             double longitudeDegrees,
                                             double *pLatitudeDegrees);

/** Given two points on the surface of the earth, work out the shortest
 * distance from the shortest line between the two points to a third
 * point, in WGS84 coordinates.  The solution must take into account the
 * fact that the line is a segment, i.e. this not the distance to a
 * great circle, the line has finite length.
 *
 * If the function is unable to calculate the distance it
 * should populate pDistanceMetres with NAN and return success.
 *
 * YOU MUST PROVIDE an implementation of this function if you wish to
 * use large polygons (e.g. more than a kilometre in square extent)
 * in your geofence with an accuracy better than 0.5% worst case.
 *
 * @param aLatitudeDegrees      the latitude of one end of the line
 *                              in degrees.
 * @param aLongitudeDegrees     the longitude of one end of the line
 *                              in degrees.
 * @param bLatitudeDegrees      the latitude of the other end of the
 *                              line in degrees.
 * @param bLongitudeDegrees     the longitude of the other end of the
 *                              line in degrees.
 * @param pointLatitudeDegrees  the latitude of the point in degrees.
 * @param pointLongitudeDegrees the longitude of the point in degrees.
 * @param[out] pDistanceMetres  a pointer to a place to put the shortest
 *                              distance from the point to the line
 *                              between (a) and (b) in metres; will
 *                              never be NULL.
 * @return                      zero on success, that is if there has
 *                              been an intersection and pLatitudeDegrees
 *                              has been populated, else negative error
 *                              code.
 */
int32_t uGeofenceWgs84DistanceToSegment(double aLatitudeDegrees,
                                        double aLongitudeDegrees,
                                        double bLatitudeDegrees,
                                        double bLongitudeDegrees,
                                        double pointLatitudeDegrees,
                                        double pointLongitudeDegrees,
                                        double *pDistanceMetres);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GEOFENCE_GEODESIC_H_

// End of file
