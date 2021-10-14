/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Types and location test configuration information shared
 * between testing of the location and network APIs.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // LONG_MIN, INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"

#include "u_port.h"
#if U_CFG_ENABLE_LOGGING
#include "u_port_debug.h"
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_location.h"
#include "u_location_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Location configuration for a network that does not support
 * location.
 */
static const uLocationTestCfgList_t gCfgListNone = {0};

#if defined U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
/** Location assist for Cloud locate.
 */
static const uLocationAssist_t gLocationAssistCloudLocate = {500000, // desiredAccuracyMillimetres
                                                             60,     // desiredTimeoutSeconds
                                                             true,   // disable GNSS so that a GNSS network can use it
                                                             -1
                                                             };    // networkHandleAssist

/** Location configuration for Cloud Locate.
 */
static const uLocationTestCfg_t gCfgCloudLocate = {U_LOCATION_TYPE_CLOUD_CELL_LOCATE,
                                                   &gLocationAssistCloudLocate,
                                                   U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN)
                                                  };
#endif

/** Location configuration list for a cellular network.
 */
#if defined U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
static const uLocationTestCfgList_t gCfgListCell = {1, &gCfgCloudLocate};
#else
static const uLocationTestCfgList_t gCfgListCell = {0};
#endif

/** Location configuration for a GNSS network.
 */
static const uLocationTestCfg_t gCfgGnss = {U_LOCATION_TYPE_GNSS, NULL, NULL};

/** Location configuration list for a GNSS network.
 */
static const uLocationTestCfgList_t gCfgListGnss = {1, &gCfgGnss};

/* ----------------------------------------------------------------
 * SHARED VARIABLES
 * -------------------------------------------------------------- */

/** Location configurations for each network type.
 * ORDER IS IMPORTANT: follows the order of uNetworkType_t.
 */
const uLocationTestCfgList_t *const gpULocationTestCfg[] = {&gCfgListNone,    // U_NETWORK_TYPE_NONE
                                                            &gCfgListNone,   // U_NETWORK_TYPE_BLE
                                                            &gCfgListCell,   // U_NETWORK_TYPE_CELL
                                                            &gCfgListNone,   // U_NETWORK_TYPE_WIFI
                                                            &gCfgListGnss
                                                           };  // U_NETWORK_TYPE_GNSS

/** Number of items in the gpULocationTestCfg array,
 * has to be done in this file and externed or GCC complains about
 * asking for the size of a partially defined type.
 */
const size_t gpULocationTestCfgSize = sizeof (gpULocationTestCfg) /
                                      sizeof (gpULocationTestCfg[0]);

/** So that we can print the name of the location type being tested.
 */
const char *const gpULocationTestTypeStr[] = {"none",        // U_LOCATION_TYPE_NONE
                                              "GNSS",        // U_LOCATION_TYPE_GNSS
                                              "Cell Locate", // U_LOCATION_TYPE_CLOUD_CELL_LOCATE
                                              "Google",      // U_LOCATION_TYPE_CLOUD_GOOGLE
                                              "Skyhook",     // U_LOCATION_TYPE_CLOUD_SKYHOOK
                                              "Here"         // U_LOCATION_TYPE_CLOUD_HERE
                                             };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if U_CFG_ENABLE_LOGGING
// Convert a lat/long into a whole number and a
// bit-after-the-decimal-point that can be printed
// without having to invoke floating point operations,
// returning the prefix (either "+" or "-").
// The result should be printed with printf() format
// specifiers %c%d.%07d, e.g. something like:
//
// int32_t whole;
// int32_t fraction;
//
// printf("%c%d.%07d/%c%d.%07d", latLongToBits(latitudeX1e7, &whole, &fraction),
//                               whole, fraction,
//                               latLongToBits(longitudeX1e7, &whole, &fraction),
//                               whole, fraction);
static char latLongToBits(int32_t thingX1e7,
                          int32_t *pWhole,
                          int32_t *pFraction)
{
    char prefix = '+';

    // Deal with the sign
    if (thingX1e7 < 0) {
        thingX1e7 = -thingX1e7;
        prefix = '-';
    }
    *pWhole = thingX1e7 / 10000000;
    *pFraction = thingX1e7 % 10000000;

    return prefix;
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Reset the location structure.
void uLocationTestResetLocation(uLocation_t *pLocation)
{
    pLocation->latitudeX1e7 = INT_MIN;
    pLocation->longitudeX1e7 = INT_MIN;
    pLocation->altitudeMillimetres = INT_MIN;
    pLocation->radiusMillimetres = INT_MIN;
    pLocation->speedMillimetresPerSecond = INT_MIN;
    pLocation->svs = INT_MIN;
    pLocation->timeUtc = LONG_MIN;
}

// Print a location structure.
void uLocationTestPrintLocation(const uLocation_t *pLocation)
{
#if U_CFG_ENABLE_LOGGING
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];

    prefix[0] = latLongToBits(pLocation->latitudeX1e7, &(whole[0]), &(fraction[0]));
    prefix[1] = latLongToBits(pLocation->longitudeX1e7, &(whole[1]), &(fraction[1]));
    uPortLog("U_LOCATION_TEST_SHARED: location %c%d.%07d/%c%d.%07d (radius %d metre(s)),"
             " %d metre(s) high, moving at %d metre(s)/second, %d satellite(s) visible,"
             " UTC time %d.\n", prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
             pLocation->radiusMillimetres / 1000, pLocation->altitudeMillimetres / 1000,
             pLocation->speedMillimetresPerSecond / 1000, pLocation->svs,
             (int32_t) pLocation->timeUtc);
    uPortLog("U_LOCATION_TEST_SHARED: paste this into a browser"
             " https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
             prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);
#else
    (void) pLocation;
#endif
}

// End of file
