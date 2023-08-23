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

#include "u_at_client.h" // Required by u_gnss_private.h

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_val_key.h"
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

#ifndef U_GNSS_POS_TEST_STREAMED_RATE_MS
/** The rate at which to ask for streamed position in milliseconds.
 */
# define U_GNSS_POS_TEST_STREAMED_RATE_MS 250
#endif

#ifndef U_GNSS_POS_TEST_STREAMED_RATE_MARGIN_PERCENT
/** You would normally expect us to receive streamed position at
 * a rate of #U_GNSS_POS_TEST_STREAMED_RATE_MS but there is some
 * rounding etc. involved so allow some margin. i.e. we should
 * get within this percentage amount of that rate (e.g. every 110
 * ms versus every 100 ms would be within 90 percent).
 */
# define U_GNSS_POS_TEST_STREAMED_RATE_MARGIN_PERCENT 90
#endif

#ifndef U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS
/** It takes a little while for a requested rate change in the GNSS
 * chip to filter through to us; e.g. this lon.
 */
# define U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS 5
#endif

#ifndef U_GNSS_POS_TEST_STREAMED_SECONDS
/** How long to run streamed position for, once it has started
 * returning good results.
 */
# define U_GNSS_POS_TEST_STREAMED_SECONDS 10
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

/** The number of times by posCallback() has been called.
 */
static volatile size_t gGoodPosCount;

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

/** The initial measurement rate (for streamed position).
 */
static int32_t gMeasurementRate = -1;

/** The initial measurement period (for streamed position).
 */
static int32_t gMeasurementPeriodMs = -1;

/** The initial navigation count (for streamed position).
 */
static int32_t gNavigationCount = -1;

/** The initial time system (for streamed position).
 */
static uGnssTimeSystem_t gTimeSystem = U_GNSS_TIME_SYSTEM_NONE;

/** The initial message rate (for streamed position).
 */
static int32_t gMsgRate = -1;

/** The initial protocol bit-map (for streamed position).
 */
static int32_t gProtocolBitMap = -1;

/** The message ID of the UBX-NAV-PVT message (for streamed position).
 */
static uGnssMessageId_t gUbxNavPvtMessageId = {U_GNSS_PROTOCOL_UBX, {0x0107}};

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
    if (gErrorCode == 0) {
        gGoodPosCount++;
    }
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
    int64_t startTimeMs;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
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

        startTimeMs = uPortGetTickTimeMs();
        gStopTimeMs = startTimeMs + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        y = uGnssPosGet(gnssHandle,
                        &latitudeX1e7, &longitudeX1e7,
                        &altitudeMillimetres,
                        &radiusMillimetres,
                        &speedMillimetresPerSecond,
                        &svs, &timeUtc,
                        keepGoingCallback);

        U_PORT_TEST_ASSERT(y == 0);

        U_TEST_PRINT_LINE("position establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
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

        gErrorCode = 0xFFFFFFFF;
        gGoodPosCount = 0;
        startTimeMs = uPortGetTickTimeMs();
        gStopTimeMs = startTimeMs + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        U_PORT_TEST_ASSERT(uGnssPosGetStart(gnssHandle, posCallback) == 0);
        U_TEST_PRINT_LINE("waiting up to %d second(s) for results from asynchronous API...",
                          U_GNSS_POS_TEST_TIMEOUT_SECONDS);
        while ((gErrorCode == 0xFFFFFFFF) && (uPortGetTickTimeMs() < gStopTimeMs)) {
            uPortTaskBlock(1000);
        }

        // See what we're doing again now
        uGnssSetUbxMessagePrint(gnssHandle, true);

        U_PORT_TEST_ASSERT(gGnssHandle == gnssHandle);
        U_TEST_PRINT_LINE("asynchonous API received error code %d.", gErrorCode);
        U_PORT_TEST_ASSERT(gErrorCode == 0);
        U_PORT_TEST_ASSERT(gGoodPosCount == 1);
        U_TEST_PRINT_LINE("position establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);

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
    int64_t startTimeMs;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];

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
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
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

        // Check the RRLP mode we are running
        y = uGnssPosGetRrlpMode(gnssHandle);
        U_PORT_TEST_ASSERT(y == (int32_t) U_GNSS_RRLP_MODE_MEASX);

        U_TEST_PRINT_LINE("asking for RRLP information with no thresholds...");
        y = uGnssPosGetRrlp(gnssHandle, pBuffer, U_GNSS_POS_RRLP_SIZE_BYTES,
                            -1, -1, -1, -1, NULL);
        U_TEST_PRINT_LINE("%d byte(s) of RRLP information was returned.", y);
        // Must contain at least 6 bytes for the header
        U_PORT_TEST_ASSERT(y >= 6);
        U_PORT_TEST_ASSERT(y <= U_GNSS_POS_RRLP_SIZE_BYTES);

        startTimeMs = uPortGetTickTimeMs();
        gStopTimeMs = startTimeMs + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
        U_TEST_PRINT_LINE("asking for RRLP information with thresholds...");
        y = uGnssPosGetRrlp(gnssHandle, pBuffer, U_GNSS_POS_RRLP_SIZE_BYTES,
                            U_GNSS_POS_TEST_RRLP_SVS_THRESHOLD,
                            U_GNSS_POS_TEST_RRLP_CNO_THRESHOLD,
                            U_GNSS_POS_TEST_RRLP_MULTIPATH_INDEX_LIMIT,
                            U_GNSS_POS_TEST_RRLP_PSEUDORANGE_RMS_ERROR_INDEX_LIMIT,
                            keepGoingCallback);
        U_TEST_PRINT_LINE("RRLP took %d second(s) to arrive.",
                          (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
        U_TEST_PRINT_LINE("%d byte(s) of RRLP information was returned.", y);
        // Must contain at least 6 bytes for the header
        U_PORT_TEST_ASSERT(y >= 6);
        U_PORT_TEST_ASSERT(y <= U_GNSS_POS_RRLP_SIZE_BYTES);

        // Set/get all the other modes: for M10 modules or later they should
        // be supported
        y = uGnssPosSetRrlpMode(gnssHandle, U_GNSS_RRLP_MODE_MEAS50);
        if (y == 0) {
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEAS50);
        } else {
            U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASX);
        }
        y = uGnssPosSetRrlpMode(gnssHandle, U_GNSS_RRLP_MODE_MEAS20);
        if (y == 0) {
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEAS20);
        } else {
            U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASX);
        }
        y = uGnssPosSetRrlpMode(gnssHandle, U_GNSS_RRLP_MODE_MEASD12);
        if (y == 0) {
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASD12);
        } else {
            U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASX);
        }

        y = uGnssPosSetRrlpMode(gnssHandle, U_GNSS_RRLP_MODE_MEASC12);
        if (y == 0) {
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASC12);
        } else {
            U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
            U_PORT_TEST_ASSERT(uGnssPosGetRrlpMode(gnssHandle) == (int32_t) U_GNSS_RRLP_MODE_MEASX);
        }

        if (y == 0) {
            // Do an RRLP get of the 12C compact mode with whacky thresholds, since
            // they should be ignored
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
            U_TEST_PRINT_LINE("asking for compact RRLP information 12C...");
            // length should be the UBX protocol overhead plus 12 bytes of data
            y = uGnssPosGetRrlp(gnssHandle, pBuffer, 8 + 12,
                                INT_MAX, INT_MAX, INT_MAX, INT_MAX,
                                keepGoingCallback);
            U_TEST_PRINT_LINE("RRLP took %d second(s) to arrive.",
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
            U_TEST_PRINT_LINE("%d byte(s) of RRLP information was returned.", y);
            // Must contain at least 6 bytes for the header
            U_PORT_TEST_ASSERT(y >= 6);
            U_PORT_TEST_ASSERT(y <= 8 + 12);
        }

        // Put the RRLP mode back to the default again (should always work)
        U_PORT_TEST_ASSERT(uGnssPosSetRrlpMode(gnssHandle, U_GNSS_RRLP_MODE_MEASX) == 0);

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

/** Test streamed GNSS position establishment.
 */
U_PORT_TEST_FUNCTION("[gnssPos]", "gnssPosStreamed")
{
    uDeviceHandle_t gnssHandle;
    int32_t y;
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];
    int32_t startTimeMs;
    int32_t posTimeMs = -1;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    int32_t a = -1;
    int32_t b = -1;
    uGnssTimeSystem_t t = U_GNSS_TIME_SYSTEM_NONE;

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing streamed position establishment on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        // Do the standard preamble
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        if (transportTypes[x] == U_GNSS_TRANSPORT_AT) {
            // Streamed position not supported on an AT transport
            U_PORT_TEST_ASSERT(uGnssPosGetStreamedStart(gnssHandle,
                                                        U_GNSS_POS_TEST_STREAMED_RATE_MS,
                                                        posCallback) < 0);
        } else {
            // So that we can see what we're doing
            uGnssSetUbxMessagePrint(gnssHandle, true);

            // Get the initial protocol bit-map, then switch off NMEA messages
            // so that we get max speed of UBX messages
            U_TEST_PRINT_LINE("switching off NMEA messages as we want to receive large"
                              " UBX-NAV-PVT messages every %d milliseconds.",
                              U_GNSS_POS_TEST_STREAMED_RATE_MS);
            gProtocolBitMap = uGnssCfgGetProtocolOut(gnssHandle);
            uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, false);

            // Make sure we have a 3D fix to get altitude as well
            U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, U_GNSS_FIX_MODE_3D) == 0);

            // Get the initial measurement rate
            gMeasurementRate = uGnssCfgGetRate(gnssHandle,
                                               &gMeasurementPeriodMs,
                                               &gNavigationCount,
                                               &gTimeSystem);
            U_PORT_TEST_ASSERT(gMeasurementRate >= 0);
            U_TEST_PRINT_LINE("initial measurement rate was %d milliseconds"
                              " (measurement period %d milliseconds, navigation"
                              " count %d, time system %d).",
                              gMeasurementRate, gMeasurementPeriodMs,
                              gNavigationCount, gTimeSystem);

            // Get the initial message rate for UBX-NAV-PVT
            gMsgRate = uGnssCfgGetMsgRate(gnssHandle, &gUbxNavPvtMessageId);
            if (gMsgRate < 0) {
                gMsgRate = 0;
                if (uGnssCfgValGet(gnssHandle,
                                   U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1,
                                   (void *) &gMsgRate, sizeof(gMsgRate),
                                   U_GNSS_CFG_VAL_LAYER_RAM) != 0) {
                    gMsgRate = -1;
                    U_PORT_TEST_ASSERT(false);
                }
            }
            U_TEST_PRINT_LINE("initial message rate for UBX-NAV-PVT was %d.", gMsgRate);

            // Switch off message printing as we can't afford the time
            uGnssSetUbxMessagePrint(gnssHandle, false);

            gErrorCode = 0xFFFFFFFF;
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs + U_GNSS_POS_TEST_TIMEOUT_SECONDS * 1000;
            U_PORT_TEST_ASSERT(uGnssPosGetStreamedStart(gnssHandle,
                                                        U_GNSS_POS_TEST_STREAMED_RATE_MS,
                                                        posCallback) == 0);
            U_TEST_PRINT_LINE("waiting up to %d second(s) for first valid result from streamed API...",
                              U_GNSS_POS_TEST_TIMEOUT_SECONDS);
            while ((gErrorCode != 0) && (uPortGetTickTimeMs() < gStopTimeMs)) {
                uPortTaskBlock(1000);
            }

            if (gErrorCode == 0) {
                posTimeMs = uPortGetTickTimeMs();
                U_TEST_PRINT_LINE("waiting %d second(s) for rate change to take effect...",
                                  U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS);
                uPortTaskBlock(1000 * U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS);
                // gGoodPosCount should now be building up
                gGoodPosCount = 0;
                U_TEST_PRINT_LINE("waiting %d second(s) for streamed position calls to accumulate...",
                                  U_GNSS_POS_TEST_STREAMED_SECONDS);
                uPortTaskBlock(1000 * U_GNSS_POS_TEST_STREAMED_SECONDS);
            }
            uGnssPosGetStreamedStop(gnssHandle);

            // See what we're doing again now
            uGnssSetUbxMessagePrint(gnssHandle, true);

            U_PORT_TEST_ASSERT(gGnssHandle == gnssHandle);
            U_TEST_PRINT_LINE("streamed position callback received error code %d.", gErrorCode);
            U_PORT_TEST_ASSERT(gErrorCode == 0);
            if (gGoodPosCount > 0) {
                U_TEST_PRINT_LINE("position establishment took %d second(s).", (posTimeMs - startTimeMs) / 1000);
                U_TEST_PRINT_LINE("the streamed position callback was called with a good position %d time(s)"
                                  " in %d second(s), average every %d millisecond(s) (expected every"
                                  " %d milliseconds).",
                                  gGoodPosCount, U_GNSS_POS_TEST_STREAMED_SECONDS,
                                  (U_GNSS_POS_TEST_STREAMED_SECONDS * 1000) / gGoodPosCount,
                                  U_GNSS_POS_TEST_STREAMED_RATE_MS);
                U_PORT_TEST_ASSERT(gGoodPosCount >= (((U_GNSS_POS_TEST_STREAMED_SECONDS * 1000) /
                                                      U_GNSS_POS_TEST_STREAMED_RATE_MS) *
                                                     U_GNSS_POS_TEST_STREAMED_RATE_MARGIN_PERCENT) / 100);
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
            }

            U_TEST_PRINT_LINE("waiting %d second(s) for things to calm down and then flushing...",
                              U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS);
            uPortTaskBlock(1000 * U_GNSS_POS_TEST_STREAMED_WAIT_SECONDS);
            // Flush any remaining messages out of the system before
            // we continue, or the replies to the messages below
            // can get stuck behind them
            uGnssMsgReceiveFlush(gnssHandle, true);

            // Check that the rates are back as they were
            y = uGnssCfgGetRate(gnssHandle, &a, &b, &t);
            U_TEST_PRINT_LINE("final measurement rate is %d milliseconds"
                              " (measurement period %d milliseconds, navigation"
                              " count %d, time system %d).",
                              y, a, b, t);
            U_PORT_TEST_ASSERT(y == gMeasurementRate);
            gMeasurementRate = -1;
            U_PORT_TEST_ASSERT(a == gMeasurementPeriodMs);
            gMeasurementPeriodMs = -1;
            U_PORT_TEST_ASSERT(b == gNavigationCount);
            gNavigationCount = -1;
            U_PORT_TEST_ASSERT(t == gTimeSystem);
            gTimeSystem = U_GNSS_TIME_SYSTEM_NONE;
            y = uGnssCfgGetMsgRate(gnssHandle, &gUbxNavPvtMessageId);
            if (y < 0) {
                y = 0;
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle,
                                                  U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1,
                                                  (void *) &y, sizeof(y),
                                                  U_GNSS_CFG_VAL_LAYER_RAM) == 0);
            }
            U_TEST_PRINT_LINE("final message rate for UBX-NAV-PVT is %d.", y);
            U_PORT_TEST_ASSERT(y == gMsgRate);
            gMsgRate = -1;

            // Put NMEA protocol output back if we switched it off
            if ((gProtocolBitMap >= 0) && (gProtocolBitMap & (1 << U_GNSS_PROTOCOL_NMEA))) {
                uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, true);
            }

            // Check that we haven't dropped any incoming data
            y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
            U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
            U_PORT_TEST_ASSERT(y == 0);
        }

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

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssPos]", "gnssPosCleanUp")
{
    int32_t x;

    if (gHandles.gnssHandle != NULL) {
        // Put the rate settings back (-1 will just not be set so no need to check)
        uGnssCfgSetRate(gHandles.gnssHandle, gMeasurementPeriodMs,
                        gNavigationCount, gTimeSystem);
    }

    // Put the message rate setting back
    if ((gMsgRate >= 0) && (gHandles.gnssHandle != NULL)) {
        uGnssCfgSetMsgRate(gHandles.gnssHandle, &gUbxNavPvtMessageId, gMsgRate);
    }

    // Put NMEA protocol output back if we switched it off
    if ((gProtocolBitMap >= 0) && (gHandles.gnssHandle != NULL) &&
        (gProtocolBitMap & (1 << U_GNSS_PROTOCOL_NMEA))) {
        uGnssCfgSetProtocolOut(gHandles.gnssHandle, U_GNSS_PROTOCOL_NMEA, true);
    }

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
