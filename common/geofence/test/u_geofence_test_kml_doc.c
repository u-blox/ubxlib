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
 * @brief The start and end required when writing a KML document
 * full of output data; Windows only.
 */

#if defined(U_CFG_GEOFENCE) && defined(_WIN32)

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "windows.h"
#include "u_geofence_test_kml_doc.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** String to put at the start of a KML document.
 */
const char gUGeofenceTestKmlDocStartStr[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
    "<Document>\n";

/** The length of the string gUGeofenceTestKmlDocStartStr.
 */
const size_t gUGeofenceTestKmlDocStartStrlen = sizeof(gUGeofenceTestKmlDocStartStr) - 1;

/** Styles section of a KML document.
 */
const char gUGeofenceTestKmlDocStylesStr[] =
    "\t<Style id=\"light_green_highlight\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=00fb0c&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"
    "\t<Style id=\"light_green_normal\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>0.5</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=00fb0c&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"

    "\t<Style id=\"dark_green_highlight\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=007000&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"
    "\t<Style id=\"dark_green_normal\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>0.5</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=007000&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"
    "\t<Style id=\"red_highlight\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=fb0000&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>1.0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"
    "\t<Style id=\"red_normal\">\n"
    "\t\t<IconStyle>\n"
    "\t\t\t<scale>0.5</scale>\n"
    "\t\t\t<Icon>\n"
    "\t\t\t\t<href>https://earth.google.com/earth/rpc/cc/icon?color=fb0000&amp;id=2000&amp;scale=4</href>\n"
    "\t\t\t</Icon>\n"
    "\t\t\t<hotSpot x=\"64\" y=\"128\" xunits=\"pixels\" yunits=\"insetPixels\"/>\n"
    "\t\t</IconStyle>\n"
    "\t\t<LabelStyle>\n"
    "\t\t\t<scale>0</scale>\n"
    "\t\t</LabelStyle>\n"
    "\t</Style>\n"
    "\n"
    "\t<StyleMap id=\"style_map_inside\">\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>normal</key>\n"
    "\t\t\t<styleUrl>#light_green_normal</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>highlight</key>\n"
    "\t\t\t<styleUrl>#light_green_highlight</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t</StyleMap>\n"
    "\t<StyleMap id=\"style_map_outside\">\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>normal</key>\n"
    "\t\t\t<styleUrl>#dark_green_normal</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>highlight</key>\n"
    "\t\t\t<styleUrl>#dark_green_highlight</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t</StyleMap>\n"
    "\t<StyleMap id=\"style_map_none\">\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>normal</key>\n"
    "\t\t\t<styleUrl>#red_normal</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t\t<Pair>\n"
    "\t\t\t<key>highlight</key>\n"
    "\t\t\t<styleUrl>#red_highlight</styleUrl>\n"
    "\t\t</Pair>\n"
    "\t</StyleMap>\n"
    "\n";

/** The length of the string gpUGeofenceTestKmlDocStyles.
 */
const size_t gUGeofenceTestKmlDocStylesStrlen = sizeof(gUGeofenceTestKmlDocStylesStr) - 1;

/** String to put at the end of a KML document.
 */
const char gUGeofenceTestKmlDocEndStr[] =
    "</Document>\n"
    "</kml>";

/** The length of the string gpUGeofenceTestKmlDocEnd.
 */
const size_t gUGeofenceTestKmlDocEndStrlen = sizeof(gUGeofenceTestKmlDocEndStr) - 1;

#endif // #if defined(U_CFG_GEOFENCE) && defined(_WIN32)

// End of file
