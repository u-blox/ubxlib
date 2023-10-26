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

#ifndef _U_GEOFENCE_TEST_KML_DOC_H_
#define _U_GEOFENCE_TEST_KML_DOC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types related to populating
 * a KML document full of output data.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GEOFENCE_TEST_KML_STYLE_MAP_ID_INSIDE
/** The styleMap ID to use for "inside" data points when writing
 * the KML file (Windows only).  Should match an ID of a styleMap
 * in kml_document_start.txt.
 */
# define U_GEOFENCE_TEST_KML_STYLE_MAP_ID_INSIDE "#style_map_inside"
#endif

#ifndef U_GEOFENCE_TEST_KML_STYLE_MAP_ID_OUTSIDE
/** The styleMap ID to use for "outside" data points when writing
 * the KML file (Windows only).  Should match an ID of a styleMap
 * in kml_document_start.txt.
 */
# define U_GEOFENCE_TEST_KML_STYLE_MAP_ID_OUTSIDE "#style_map_outside"
#endif

#ifndef U_GEOFENCE_TEST_KML_STYLE_MAP_ID_NONE
/** The styleMap ID to use in the KML file (Windows only) when
 * "none" is returned by a test against the fence (which shouldn't
 * happen).  Should match an ID of a styleMap in
 * kml_document_start.txt.
 */
# define U_GEOFENCE_TEST_KML_STYLE_MAP_ID_NONE "#style_map_none"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The string that forms the start of a KML document.
 */
extern const char gUGeofenceTestKmlDocStartStr[];

/** The length of the string gpUGeofenceTestKmlDocStart.
 */
extern const size_t gUGeofenceTestKmlDocStartStrlen;

/** The string that forms the syles block of a KML document.
 */
extern const char gUGeofenceTestKmlDocStylesStr[];

/** The length of the string gUGeofenceTestKmlDocStylesStr.
 */
extern const size_t gUGeofenceTestKmlDocStylesStrlen;

/** The string that forms the end of a KML document.
 */
extern const char gUGeofenceTestKmlDocEndStr[];

/** The length of the string gpUGeofenceTestKmlDocStart.
 */
extern const size_t gUGeofenceTestKmlDocEndStrlen;

#ifdef __cplusplus
}
#endif

#endif // _U_GEOFENCE_TEST_KML_DOC_H_

// End of file
