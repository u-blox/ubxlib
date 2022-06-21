/*
 * Copyright 2019-2022 u-blox
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
 * @brief Tests for the Cell Locate API: these should pass on all
 * platforms that have a cellular module connected to them.  They
 * are only compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "limits.h"    // INT_MIN, LONG_MIN

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_location.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_loc.h"
#if U_CFG_APP_PIN_CELL_PWR_ON < 0
#include "u_cell_pwr.h"
#endif

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_LOC_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_LOC_TEST_TIMEOUT_SECONDS
/** The position establishment timeout to use during testing, in
 * seconds.
 */
# define U_CELL_LOC_TEST_TIMEOUT_SECONDS 180
#endif

#ifndef U_CELL_LOC_TEST_MIN_UTC_TIME
/** A minimum value for UTC time to test against (21 July 2021 13:40:36).
 */
# define U_CELL_LOC_TEST_MIN_UTC_TIME 1626874836
#endif

#ifndef U_CELL_LOC_TEST_MAX_RADIUS_MILLIMETRES
/** The maximum radius we consider valid.
 */
# define U_CELL_LOC_TEST_MAX_RADIUS_MILLIMETRES (10000 * 1000)
#endif

#ifndef U_CELL_LOC_TEST_BAD_STATUS_LIMIT
/** The maximum number of fatal-type location status checks
 * to tolerate before giving up, as a back-stop for SARA-R4
 * not giving an answer.  Since we query the status once a
 * second, should be more than the time we ask Cell Locate
 * to respond in, which is by default
 * U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS.
 */
#define U_CELL_LOC_TEST_BAD_STATUS_LIMIT (U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS + 30)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

#ifdef U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Cell handle as seen by posCallback().
 */
static uDeviceHandle_t gCellHandle = NULL;

/** Error code as seen by posCallback().
 */
static volatile int32_t gErrorCode = -1;

/** Latitude as seen by posCallback().
 */
static int32_t gLatitudeX1e7 = INT_MIN;

/** Longitude as seen by posCallback().
 */
static int32_t gLongitudeX1e7 = INT_MIN;

/** Altitude as seen by posCallback().
 */
static int32_t gAltitudeMillimetres = INT_MIN;

/** Radius as seen by posCallback().
 */
static int32_t gRadiusMillimetres = INT_MIN;

/** Speed as seen by posCallback().
 */
static int32_t gSpeedMillimetresPerSecond = INT_MIN;

/** Number of space vehicles as seen by posCallback().
 */
static int32_t gSvs = INT_MIN;

/** Time as seen by posCallback().
 */
static int64_t gTimeUtc = LONG_MIN;

#endif //U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
// Callback function for the cellular connection process
static bool keepGoingCallback(uDeviceHandle_t param)
{
    bool keepGoing = true;

    (void) param;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback function for non-blocking API.
static void posCallback(uDeviceHandle_t cellHandle,
                        int32_t errorCode,
                        int32_t latitudeX1e7,
                        int32_t longitudeX1e7,
                        int32_t altitudeMillimetres,
                        int32_t radiusMillimetres,
                        int32_t speedMillimetresPerSecond,
                        int32_t svs,
                        int64_t timeUtc)
{
    gCellHandle = cellHandle,
    gErrorCode = errorCode;
    gLatitudeX1e7 = latitudeX1e7;
    gLongitudeX1e7 = longitudeX1e7;
    gAltitudeMillimetres = altitudeMillimetres;
    gRadiusMillimetres = radiusMillimetres;
    gSpeedMillimetresPerSecond = speedMillimetresPerSecond;
    gSvs = svs;
    gTimeUtc = timeUtc;
}

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

#endif //U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the Cell Locate API configuration items.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellLoc]", "cellLocCfg")
{
    uDeviceHandle_t cellHandle;
    int32_t heapUsed;
    int32_t y;
    int32_t z;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Check desired accuracy
    y = uCellLocGetDesiredAccuracy(cellHandle);
    U_TEST_PRINT_LINE("desired accuracy is %d millimetres.", y);
    U_PORT_TEST_ASSERT(y > 0);
    z = y - 1;
    uCellLocSetDesiredAccuracy(cellHandle, z);
    z = uCellLocGetDesiredAccuracy(cellHandle);
    U_TEST_PRINT_LINE("desired accuracy is now %d millimetres.", z);
    U_PORT_TEST_ASSERT(z == y - 1);
    // Put it back as it was
    uCellLocSetDesiredAccuracy(cellHandle, y);
    U_TEST_PRINT_LINE("desired accuracy returned to.", y);

    // Check desired fix timeout
    y = uCellLocGetDesiredFixTimeout(cellHandle);
    U_TEST_PRINT_LINE("desired fix timeout is %d second(s).", y);
    U_PORT_TEST_ASSERT(y > 0);
    z = y - 1;
    uCellLocSetDesiredFixTimeout(cellHandle, z);
    z = uCellLocGetDesiredFixTimeout(cellHandle);
    U_TEST_PRINT_LINE("desired fix timeout is now %d second(s).", z);
    U_PORT_TEST_ASSERT(z == y - 1);
    // Put it back as it was
    uCellLocSetDesiredFixTimeout(cellHandle, y);
    U_TEST_PRINT_LINE("desired fix timeout returned to.", y);

    // Check whether GNSS is used or not
    y = (int32_t) uCellLocGetGnssEnable(cellHandle);
    U_TEST_PRINT_LINE("GNSS is %s.", y ? "enabled" : "disabled");
    z = ! (bool) y;
    uCellLocSetGnssEnable(cellHandle, (bool) z);
    z = (int32_t) uCellLocGetGnssEnable(cellHandle);
    U_TEST_PRINT_LINE("GNSS is now %s.", z ? "enabled" : "disabled");
    U_PORT_TEST_ASSERT((bool) z == ! (bool) y);
    // Put it back as it was
    uCellLocSetGnssEnable(cellHandle, (bool) y);
    U_TEST_PRINT_LINE("GNSS returned to %s.", y ? "enabled" : "disabled");

#if (U_CFG_APP_CELL_PIN_GNSS_POWER >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssPwr(cellHandle,
                                                 U_CFG_APP_CELL_PIN_GNSS_POWER) == 0);
    }
#endif

#if (U_CFG_APP_CELL_PIN_GNSS_DATA_READY >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssDataReady(cellHandle,
                                                       U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
    }
#endif

#if (U_CFG_APP_CELL_PIN_GNSS_POWER >= 0) || (U_CFG_APP_CELL_PIN_GNSS_DATA_READY >= 0)
    U_TEST_PRINT_LINE("checking if GNSS is present...");
    U_PORT_TEST_ASSERT(uCellLocIsGnssPresent(cellHandle));
#endif

#ifdef U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
    y = uCellLocSetServer(cellHandle,
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN),
# ifdef U_CFG_APP_CELL_LOC_PRIMARY_SERVER
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOC_PRIMARY_SERVER),
# else
                          NULL,
# endif
# ifdef U_CFG_APP_CELL_LOC_SECONDARY_SERVER
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOC_SECONDARY_SERVER)) == 0);
# else
                          NULL);
# endif
    U_PORT_TEST_ASSERT(y == 0);
#endif

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test getting position using Cell Locate.
 */
U_PORT_TEST_FUNCTION("[cellLoc]", "cellLocLoc")
{
#ifdef U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
    uDeviceHandle_t cellHandle;
    int32_t heapUsed;
    int64_t startTime;
    int32_t latitudeX1e7 = INT_MIN;
    int32_t longitudeX1e7 = INT_MIN;
    int32_t altitudeMillimetres = INT_MIN;
    int32_t radiusMillimetres = INT_MIN;
    int32_t speedMillimetresPerSecond = INT_MIN;
    int32_t svs = INT_MIN;
    int64_t timeUtc = LONG_MIN;
    int32_t x;
    size_t badStatusCount;
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Configure the module pins in case a GNSS chip is present
#if (U_CFG_APP_CELL_PIN_GNSS_POWER >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssPwr(cellHandle,
                                                 U_CFG_APP_CELL_PIN_GNSS_POWER) == 0);
    }
#endif

#if (U_CFG_APP_CELL_PIN_GNSS_DATA_READY >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssDataReady(cellHandle,
                                                       U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
    }
#endif

    // Set the authentication token
    x = uCellLocSetServer(cellHandle,
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN),
# ifdef U_CFG_APP_CELL_LOCATE_PRIMARY_SERVER
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOCATE_PRIMARY_SERVER),
# else
                          NULL,
# endif
# ifdef U_CFG_APP_CELL_LOCATE_SECONDARY_SERVER
                          U_PORT_STRINGIFY_QUOTED(U_CFG_APP_CELL_LOCATE_SECONDARY_SERVER)) == 0);
# else
                          NULL);
# endif
    U_PORT_TEST_ASSERT(x == 0);

    // Make sure we are connected to a network
    gStopTimeMs = uPortGetTickTimeMs() + (U_CELL_LOC_TEST_TIMEOUT_SECONDS * 1000);
    x = uCellNetConnect(cellHandle, NULL,
#ifdef U_CELL_TEST_CFG_APN
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
#else
                        NULL,
#endif
#ifdef U_CELL_TEST_CFG_USERNAME
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
#else
                        NULL,
#endif
#ifdef U_CELL_TEST_CFG_PASSWORD
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
#else
                        NULL,
#endif
                        keepGoingCallback);
    U_PORT_TEST_ASSERT(x == 0);

    // Get position, blocking version
    U_TEST_PRINT_LINE("location establishment, blocking version.");
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_CELL_LOC_TEST_TIMEOUT_SECONDS * 1000;
    x = uCellLocGet(cellHandle, &latitudeX1e7, &longitudeX1e7,
                    &altitudeMillimetres, &radiusMillimetres,
                    &speedMillimetresPerSecond, &svs,
                    &timeUtc, keepGoingCallback);
    U_TEST_PRINT_LINE("result was %d.", x);
    // If we are running on a cellular test network we won't get position but
    // we should always get time
    if (x == 0) {
    U_TEST_PRINT_LINE("location establishment took %d second(s).",
                      (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
        if ((radiusMillimetres > 0) &&
            (radiusMillimetres <= U_CELL_LOC_TEST_MAX_RADIUS_MILLIMETRES)) {
            prefix[0] = latLongToBits(latitudeX1e7, &(whole[0]), &(fraction[0]));
            prefix[1] = latLongToBits(longitudeX1e7, &(whole[1]), &(fraction[1]));
            uPortLog(U_TEST_PREFIX "location %c%d.%07d/%c%d.%07d, %d metre(s) high",
                     prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                     altitudeMillimetres / 1000);
            uPortLog(", radius %d metre(s)", radiusMillimetres / 1000);
            uPortLog(", speed %d metres/second, svs %d", speedMillimetresPerSecond / 1000, svs);
            uPortLog(", time %d.\n", (int32_t) timeUtc);
            U_TEST_PRINT_LINE("paste this into a browser"
                              " https://maps.google.com/?q=%c%d.%07d/%c%d.%07d",
                              prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);

            U_PORT_TEST_ASSERT(latitudeX1e7 > INT_MIN);
            U_PORT_TEST_ASSERT(longitudeX1e7 > INT_MIN);
            U_PORT_TEST_ASSERT(altitudeMillimetres > INT_MIN);
        } else {
            U_TEST_PRINT_LINE("only able to get time (%d).", (int32_t) timeUtc);
        }
    }
    U_PORT_TEST_ASSERT(x == 0);
    U_PORT_TEST_ASSERT(timeUtc > U_CELL_LOC_TEST_MIN_UTC_TIME);

    // Get position, non-blocking version
    U_TEST_PRINT_LINE("location establishment, non-blocking version.");
    // Try this a few times as the Cell Locate AT command can sometimes
    // (e.g. on SARA-R412M-02B) return "generic error" if asked to establish
    // location again quickly after returning an answer
    for (int32_t y = 3; (y > 0) && (gErrorCode != 0); y--) {
        gErrorCode = 0xFFFFFFFF;
        gStopTimeMs = startTime + U_CELL_LOC_TEST_TIMEOUT_SECONDS * 1000;
        startTime = uPortGetTickTimeMs();
        U_PORT_TEST_ASSERT(uCellLocGetStart(cellHandle, posCallback) == 0);
        U_TEST_PRINT_LINE("waiting up to %d second(s) for results from asynchonous API...",
                          U_CELL_LOC_TEST_TIMEOUT_SECONDS);
        badStatusCount = 0;
        while ((gErrorCode == 0xFFFFFFFF) && (uPortGetTickTimeMs() < gStopTimeMs) &&
               (badStatusCount < U_CELL_LOC_TEST_BAD_STATUS_LIMIT)) {
            x = uCellLocGetStatus(cellHandle);
            U_PORT_TEST_ASSERT((x >= U_LOCATION_STATUS_UNKNOWN) &&
                               (x < U_LOCATION_STATUS_MAX_NUM));
            // Cope with SARA-R4: it will sometimes return a +UULOCIND URC
            // indicating "generic error" and then (a) return a +UULOC with a URC
            // containing at least the time shortly afterwards or (b)
            // not return a +UULOC at all.  Hence we count the bad
            // status reports here and give up if there are too many
            if (x >= U_LOCATION_STATUS_FATAL_ERROR_HERE_AND_BEYOND) {
                badStatusCount++;
            }
            uPortTaskBlock(1000);
        }

        // If we are running on a cellular test network we won't get position but
        // we should always get time
        if (gErrorCode == 0) {
            U_TEST_PRINT_LINE("location establishment took %d second(s).",
                              (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
            U_PORT_TEST_ASSERT(gCellHandle == cellHandle);
            if ((radiusMillimetres > 0) &&
                (radiusMillimetres <= U_CELL_LOC_TEST_MAX_RADIUS_MILLIMETRES)) {
                x = uCellLocGetStatus(cellHandle);
                U_PORT_TEST_ASSERT((x >= U_LOCATION_STATUS_UNKNOWN) &&
                                   (x < U_LOCATION_STATUS_MAX_NUM));
                U_TEST_PRINT_LINE("location establishment took %d second(s).",
                                  (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
                U_PORT_TEST_ASSERT(gLatitudeX1e7 > INT_MIN);
                U_PORT_TEST_ASSERT(gLongitudeX1e7 > INT_MIN);
                U_PORT_TEST_ASSERT(gAltitudeMillimetres > INT_MIN);
                U_PORT_TEST_ASSERT(gRadiusMillimetres > 0);
                U_PORT_TEST_ASSERT(gSpeedMillimetresPerSecond >= 0);
                U_PORT_TEST_ASSERT(gSvs >= 0);

                prefix[0] = latLongToBits(gLatitudeX1e7, &(whole[0]), &(fraction[0]));
                prefix[1] = latLongToBits(gLongitudeX1e7, &(whole[1]), &(fraction[1]));
                uPortLog(U_TEST_PREFIX "location %c%d.%07d/%c%d.%07d, %d metre(s) high",
                         prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                         gAltitudeMillimetres / 1000);
                uPortLog(", radius %d metre(s)", gRadiusMillimetres / 1000);
                uPortLog(", speed %d metres/second, svs %d", gSpeedMillimetresPerSecond / 1000, gSvs);
                uPortLog(", time %d.\n", (int32_t) gTimeUtc);
                U_TEST_PRINT_LINE("paste this into a browser"
                                  " https://maps.google.com/?q=%c%d.%07d,%c%d.%07d",
                                  prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);
            } else {
                U_TEST_PRINT_LINE("only able to get time (%d).", (int32_t) gTimeUtc);
            }
        }
        if ((gErrorCode != 0) && (y >= 1)) {
            uCellLocGetStop(cellHandle);
            U_TEST_PRINT_LINE("failed to get an answer, will retry in 30 seconds...");
            uPortTaskBlock(30000);
        }
    }
    U_PORT_TEST_ASSERT(gErrorCode == 0);
    U_PORT_TEST_ASSERT(gTimeUtc > U_CELL_LOC_TEST_MIN_UTC_TIME);

#if U_CFG_APP_PIN_CELL_PWR_ON < 0
    // The standard postamble would normally power the module off
    // but if there is no power-on pin it won't (for obvious reasons)
    // so instead rebooot here to ensure a clean start
    uCellPwrReboot(cellHandle, NULL);
#endif

    // Do the standard postamble
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    U_TEST_PRINT_LINE("*** WARNING *** U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN"
                      " is not defined, unable to run the Cell Locate"
                      " location establishment test.");
#endif
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellLoc]", "cellLocCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
