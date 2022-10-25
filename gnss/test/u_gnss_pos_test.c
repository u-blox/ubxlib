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
 * @brief Tests for the GNSS position API: these should pass on all
 * platforms that have a GNSS module connected to them.  They
 * are only compiled if U_CFG_TEST_GNSS_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

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
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"
#include "u_gnss_pwr.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_pos.h"
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_POS_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POS_TEST_TIMEOUT_SECONDS
/** The timeout on position establishment.
 */
#define U_GNSS_POS_TEST_TIMEOUT_SECONDS 180
#endif

#ifndef U_GNSS_POS_RRLP_SIZE_BYTES
/** The number of bytes of buffer to allow for storing the RRLP
 * information.
 */
#define U_GNSS_POS_RRLP_SIZE_BYTES 1024
#endif

#ifndef U_GNSS_POS_TEST_RRLP_SVS_THRESHOLD
/** Minimum number of space vehicles for RRLP testing.
 */
#define U_GNSS_POS_TEST_RRLP_SVS_THRESHOLD 3
#endif

#ifndef U_GNSS_POS_TEST_RRLP_CNO_THRESHOLD
/** Minimum carrier to noise ratio for RRLP testing.
 */
#define U_GNSS_POS_TEST_RRLP_CNO_THRESHOLD 10
#endif

#ifndef U_GNSS_POS_TEST_RRLP_MULTIPATH_INDEX_LIMIT
/** Multipath limit for RRLP testing; we don't care
 * about this when testing this SW, provided it has
 * a value we're good.
 */
#define U_GNSS_POS_TEST_RRLP_MULTIPATH_INDEX_LIMIT 3
#endif

#ifndef U_GNSS_POS_TEST_RRLP_PSEUDORANGE_RMS_ERROR_INDEX_LIMIT
/** Pseudo-range RMS error limit for RRLP testing; we don't care
 * about this when testing this SW, provided it has a value we're
 * good.
 */
#define U_GNSS_POS_TEST_RRLP_PSEUDORANGE_RMS_ERROR_INDEX_LIMIT 63
#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** GNSS handle as seen by posCallback().
 */
static uDeviceHandle_t gGnssHandle = NULL;

/** Error code as seen by posCallback().
 */
static volatile int32_t gErrorCode;

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

/** Number of satellites as seen by posCallback().
 */
static int32_t gSvs = 0;

/** Time as seen by posCallback().
 */
static int64_t gTimeUtc = LONG_MIN;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the position establishment process.
static bool keepGoingCallback(uDeviceHandle_t gnssHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT(gnssHandle == gHandles.gnssHandle);
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback function for non-blocking API.
static void posCallback(uDeviceHandle_t gnssHandle,
                        int32_t errorCode,
                        int32_t latitudeX1e7,
                        int32_t longitudeX1e7,
                        int32_t altitudeMillimetres,
                        int32_t radiusMillimetres,
                        int32_t speedMillimetresPerSecond,
                        int32_t svs,
                        int64_t timeUtc)
{
    gGnssHandle = gnssHandle;
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test GNSS position establishment.
 */
U_PORT_TEST_FUNCTION("[gnssPos]", "gnssPosPos")
{
    uDeviceHandle_t gnssHandle;
    int32_t latitudeX1e7 = INT_MIN;
    int32_t longitudeX1e7 = INT_MIN;
    int32_t altitudeMillimetres = INT_MIN;
    int32_t radiusMillimetres = INT_MIN;
    int32_t speedMillimetresPerSecond = INT_MIN;
    int32_t svs = 0;
    int64_t timeUtc = LONG_MIN;
    int32_t y;
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];
    int64_t startTime;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing position establishment on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        // Do the standard preamble
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssHandle, true);

        // Make sure we have a 3D fix to get altitude as well
        U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, U_GNSS_FIX_MODE_3D) == 0);

        U_TEST_PRINT_LINE("using synchronous API.");

        startTime = uPortGetTickTimeMs();
        gStopTimeMs = startTime + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        y = uGnssPosGet(gnssHandle,
                        &latitudeX1e7, &longitudeX1e7,
                        &altitudeMillimetres,
                        &radiusMillimetres,
                        &speedMillimetresPerSecond,
                        &svs, &timeUtc,
                        keepGoingCallback);

        U_PORT_TEST_ASSERT(y == 0);

        U_TEST_PRINT_LINE("position establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
        prefix[0] = latLongToBits(latitudeX1e7, &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits(longitudeX1e7, &(whole[1]), &(fraction[1]));
        U_TEST_PRINT_LINE("location %c%d.%07d/%c%d.%07d (radius %d metre(s)), %d metre(s) high,"
                          " moving at %d metre(s)/second, %d satellite(s) visible, time %d.",
                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                          radiusMillimetres / 1000, altitudeMillimetres / 1000,
                          speedMillimetresPerSecond / 1000, svs, (int32_t) timeUtc);
        U_TEST_PRINT_LINE("paste this into a browser https://maps.google.com/?q=%c%d.%07d,%c%d.%07d",
                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);

        U_PORT_TEST_ASSERT(latitudeX1e7 > INT_MIN);
        U_PORT_TEST_ASSERT(longitudeX1e7 > INT_MIN);
        // Don't test altitude as we may only have a 2D fix
        U_PORT_TEST_ASSERT(radiusMillimetres > INT_MIN);
        U_PORT_TEST_ASSERT(speedMillimetresPerSecond > INT_MIN);
        // Inertial fixes will be reported with no satellites, hence >= 0
        U_PORT_TEST_ASSERT(svs >= 0);
        U_PORT_TEST_ASSERT(timeUtc > 0);

#if U_CFG_OS_CLIB_LEAKS
        // Switch off printing for the asynchronous API if the
        // platform has a leaky C-lib since we will be printing
        // stuff from a new task
        uGnssSetUbxMessagePrint(gnssHandle, false);
#endif

        U_TEST_PRINT_LINE("switching GNSS off then starting and stopping the asynchronous API.");
        U_PORT_TEST_ASSERT(uGnssPwrOff(gnssHandle) == 0);
        gErrorCode = 0;
        U_PORT_TEST_ASSERT(uGnssPosGetStart(gnssHandle, posCallback) == 0);
        uGnssPosGetStop(gnssHandle);
        U_PORT_TEST_ASSERT(gErrorCode < 0);
        U_PORT_TEST_ASSERT(gTimeUtc < 0);

        U_TEST_PRINT_LINE("switching GNSS on and using the asynchronous API properly.");
        U_PORT_TEST_ASSERT(uGnssPwrOn(gnssHandle) == 0);
        gErrorCode = 0xFFFFFFFF;
        gStopTimeMs = startTime + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        startTime = uPortGetTickTimeMs();
        U_PORT_TEST_ASSERT(uGnssPosGetStart(gnssHandle, posCallback) == 0);
        U_TEST_PRINT_LINE("waiting up to %d second(s) for results from asynchonous API...",
                          U_GNSS_POS_TEST_TIMEOUT_SECONDS);
        while ((gErrorCode == 0xFFFFFFFF) && (uPortGetTickTimeMs() < gStopTimeMs)) {
            uPortTaskBlock(1000);
        }

        // See what we're doing again now
        uGnssSetUbxMessagePrint(gnssHandle, true);

        U_PORT_TEST_ASSERT(gGnssHandle == gnssHandle);
        U_TEST_PRINT_LINE("asynchonous API received error code %d.", gErrorCode);
        U_PORT_TEST_ASSERT(gErrorCode == 0);
        U_TEST_PRINT_LINE("position establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);

        prefix[0] = latLongToBits(gLatitudeX1e7, &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits(gLongitudeX1e7, &(whole[1]), &(fraction[1]));
        U_TEST_PRINT_LINE("location %c%d.%07d/%c%d.%07d (radius %d metre(s)), %d metre(s) high,"
                          " moving at %d metre(s)/second, %d satellite(s) visible, time %d.",
                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                          gRadiusMillimetres / 1000, gAltitudeMillimetres / 1000,
                          gSpeedMillimetresPerSecond / 1000, gSvs, (int32_t) gTimeUtc);
        U_TEST_PRINT_LINE("paste this into a browser https://maps.google.com/?q=%c%d.%07d,%c%d.%07d",
                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);

        U_PORT_TEST_ASSERT(gLatitudeX1e7 > INT_MIN);
        U_PORT_TEST_ASSERT(gLongitudeX1e7 > INT_MIN);
        // Don't test altitude as we may only have a 2D fix
        U_PORT_TEST_ASSERT(gRadiusMillimetres > INT_MIN);
        U_PORT_TEST_ASSERT(gSpeedMillimetresPerSecond > INT_MIN);
        // Inertial fixes will be reported with no satellites, hence >= 0
        U_PORT_TEST_ASSERT(gSvs >= 0);
        U_PORT_TEST_ASSERT(gTimeUtc > 0);

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test retrieving RRLP information.
 */
U_PORT_TEST_FUNCTION("[gnssPos]", "gnssPosRrlp")
{
    uDeviceHandle_t gnssHandle;
    int32_t y;
    char *pBuffer;
    int64_t startTime;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Malloc memory to put the RRLP information in
    pBuffer = (char *) pUPortMalloc(U_GNSS_POS_RRLP_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);
    //lint -esym(613, pBuffer) Suppress possible use of NULL pointer
    // for pBuffer from now on

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing RRLP retrieval on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        // Do the standard preamble
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssHandle, true);

        U_TEST_PRINT_LINE("asking for RRLP information with no thresholds...");
        y = uGnssPosGetRrlp(gnssHandle, pBuffer, U_GNSS_POS_RRLP_SIZE_BYTES,
                            -1, -1, -1, -1, NULL);
        U_TEST_PRINT_LINE("%d byte(s) of RRLP information was returned.", y);
        // Must contain at least 6 bytes for the header
        U_PORT_TEST_ASSERT(y >= 6);
        U_PORT_TEST_ASSERT(y <= U_GNSS_POS_RRLP_SIZE_BYTES);

        startTime = uPortGetTickTimeMs();
        gStopTimeMs = startTime + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        U_TEST_PRINT_LINE("asking for RRLP information with thresholds...");
        y = uGnssPosGetRrlp(gnssHandle, pBuffer, U_GNSS_POS_RRLP_SIZE_BYTES,
                            U_GNSS_POS_TEST_RRLP_SVS_THRESHOLD,
                            U_GNSS_POS_TEST_RRLP_CNO_THRESHOLD,
                            U_GNSS_POS_TEST_RRLP_MULTIPATH_INDEX_LIMIT,
                            U_GNSS_POS_TEST_RRLP_PSEUDORANGE_RMS_ERROR_INDEX_LIMIT,
                            keepGoingCallback);
        U_TEST_PRINT_LINE("RRLP took %d second(s) to arrive.",
                          (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
        U_TEST_PRINT_LINE("%d byte(s) of RRLP information was returned.", y);
        // Must contain at least 6 bytes for the header
        U_PORT_TEST_ASSERT(y >= 6);
        U_PORT_TEST_ASSERT(y <= U_GNSS_POS_RRLP_SIZE_BYTES);

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Free memory
    uPortFree(pBuffer);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssPos]", "gnssPosCleanUp")
{
    int32_t x;

    uGnssTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
