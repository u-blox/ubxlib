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
 * @brief Tests for the Wifi location API: these should pass on all
 * platforms where one UART is available. No short range module is
 * actually used in this set of tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.

// Must always be included before u_short_range_test_selector.h
//lint -efile(766, u_wifi_module_type.h)
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

#if U_SHORT_RANGE_TEST_WIFI()

#include "stddef.h"    // NULL, size_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_location.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"
#include "u_wifi.h"
#include "u_wifi_loc.h"

#include "u_wifi_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_LOC_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_WIFI_LOC_TEST_AP_FILTER
/** The minimum number of Wifi access points required to cause
 * a position request to a cloud service: use the minimum (5).
 */
# define U_WIFI_LOC_TEST_AP_FILTER 5
#endif

#ifndef U_WIFI_LOC_TEST_RSSI_FILTER_DBM
/** The minimum RSSI to receive a Wifi access point at for it
 * to be used in a request to a cloud service: use the minimum (-100).
 */
# define U_WIFI_LOC_TEST_RSSI_FILTER_DBM -100
#endif

#ifndef U_WIFI_LOC_TEST_TIMEOUT_SECONDS
/** The timeout when waiting for position from a cloud service:
 * they don't generally take very long to respond.
 */
# define U_WIFI_LOC_TEST_TIMEOUT_SECONDS 30
#endif

#ifndef U_WIFI_LOC_TEST_TRIES
/** How may times to try location with each cloud service; they can
 * sometimes fail if not enough Wifi APs that they recognize are visible.
 */
# define U_WIFI_LOC_TEST_TRIES 3
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    const char *pName;
    const char *pApiKey;
    uLocationType_t type;
} uWifiLocTestLocType_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uShortRangeUartConfig_t gUart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                         .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                         .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                         .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                         .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                         .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
#ifdef U_CFG_APP_UART_PREFIX // Relevant for Linux only
                                         .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX)
#else
                                         .pPrefix = NULL
#endif
                                       };

static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

// The types of location to test, with their API keys and a name.
static uWifiLocTestLocType_t gLocType[] = {
    {"Google", U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GOOGLE_MAPS_API_KEY), U_LOCATION_TYPE_CLOUD_GOOGLE},
    {"Skyhook", U_PORT_STRINGIFY_QUOTED(U_CFG_APP_SKYHOOK_API_KEY), U_LOCATION_TYPE_CLOUD_SKYHOOK},
    {"Here", U_PORT_STRINGIFY_QUOTED(U_CFG_APP_HERE_API_KEY), U_LOCATION_TYPE_CLOUD_HERE}
};

// Test iteration count, global so that the callback() can find it.
static size_t gIteration;

// Stop time, global so that keepGoingCallback() can find it.
static int32_t gStopTimeMs;

// Global used for callback() to indicate what it received.
static int32_t gCallback;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// keepGoingCallback for blocking call.
static bool keepGoingCallback(uDeviceHandle_t param)
{
    bool keepGoing = true;

    (void) param;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

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

// Print the containts of uLocation_t nicely.
static void printLocation(const uLocation_t *pLocation)
{
#if U_CFG_ENABLE_LOGGING
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];

    prefix[0] = latLongToBits(pLocation->latitudeX1e7, &(whole[0]), &(fraction[0]));
    prefix[1] = latLongToBits(pLocation->longitudeX1e7, &(whole[1]), &(fraction[1]));
    U_TEST_PRINT_LINE("location %c%d.%07d/%c%d.%07d (radius %d metre(s)), %d metre(s) high.",
                      prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                      pLocation->radiusMillimetres / 1000, pLocation->altitudeMillimetres / 1000);
    U_TEST_PRINT_LINE("paste this into a browser https://maps.google.com/?q=%c%d.%07d,%c%d.%07d",
                      prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);
#else
    (void) pLocation;
#endif
}

// Set some initial values that are _different_ to the uLocation_t defaults.
static void locationSetDefaults(uLocation_t *pLocation)
{
    pLocation->type = U_LOCATION_TYPE_NONE;
    pLocation->latitudeX1e7 = INT_MIN;
    pLocation->longitudeX1e7 = INT_MIN;
    pLocation->altitudeMillimetres = INT_MIN;
    pLocation->radiusMillimetres = INT_MIN;
    pLocation->timeUtc = LONG_MIN;
    pLocation->speedMillimetresPerSecond = INT_MIN;
    pLocation->svs = INT_MIN;
}

// Async callback: gCallback should end up being zero if all is good.
static void callback(uDeviceHandle_t wifiHandle, int32_t errorCode,
                     const uLocation_t *pLocation)
{
    gCallback = errorCode;

    if (wifiHandle != gHandles.devHandle) {
        gCallback = -1000;
    }

    if (errorCode == 0) {
        if (pLocation == NULL) {
            gCallback = -1001;
        } else {
#if !U_CFG_OS_CLIB_LEAKS
            printLocation(pLocation);
#endif
            if (gIteration >= sizeof(gLocType) / sizeof(gLocType[0])) {
                gCallback = -1002;
            } else {
                if (pLocation->type != gLocType[gIteration].type) {
                    gCallback = -1300 - pLocation->type;
                }
                if (pLocation->latitudeX1e7 == INT_MIN) {
                    gCallback = -1004;
                }
                if (pLocation->longitudeX1e7 == INT_MIN) {
                    gCallback = -1005;
                }
                if (pLocation->radiusMillimetres < 0) {
                    gCallback = -1006;
                }
                if (pLocation->timeUtc != -1) {
                    gCallback = -1007;
                }
                if (pLocation->speedMillimetresPerSecond != INT_MIN) {
                    gCallback = -1008;
                }
                if (pLocation->svs != -1) {
                    gCallback = -1009;
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiLoc]", "wifiLocBasic")
{
    int32_t z;
    int32_t heapUsed;
    int32_t startTimeMs = 0;
    uLocation_t location;

    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);

    for (gIteration = 0; gIteration < sizeof(gLocType) / sizeof(gLocType[0]); gIteration++) {
        z = -1;
        U_TEST_PRINT_LINE("testing blocking Wifi location with %s.", gLocType[gIteration].pName);
        // It is possible for these cloud services to fail, so give them a few goes
        for (size_t y = 0; (y < U_WIFI_LOC_TEST_TRIES) && (z != 0); y++) {
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs + U_WIFI_LOC_TEST_TIMEOUT_SECONDS * 1000;
            locationSetDefaults(&location);
            z = uWifiLocGet(gHandles.devHandle, gLocType[gIteration].type,
                            gLocType[gIteration].pApiKey,
                            U_WIFI_LOC_TEST_AP_FILTER,
                            U_WIFI_LOC_TEST_RSSI_FILTER_DBM,
                            &location, keepGoingCallback);
            U_TEST_PRINT_LINE("uWifiLocGet() for %s returned %d in %d ms.",
                              gLocType[gIteration].pName, z, uPortGetTickTimeMs() - startTimeMs);
        }
        // Success or allow error code 206 on HERE since it often isn't able to establish position in our lab
        U_PORT_TEST_ASSERT((z == 0) || ((z == 206) &&
                                        (gLocType[gIteration].type == U_LOCATION_TYPE_CLOUD_HERE)));
        if (z == 0) {
            printLocation(&location);
            U_PORT_TEST_ASSERT(location.type == gLocType[gIteration].type);
            U_PORT_TEST_ASSERT(location.latitudeX1e7 > INT_MIN);
            U_PORT_TEST_ASSERT(location.longitudeX1e7 > INT_MIN);
            // Can't check altitude; only get 2D position from these services
            U_PORT_TEST_ASSERT(location.radiusMillimetres >= 0);
            U_PORT_TEST_ASSERT(location.timeUtc == -1);
            U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond == INT_MIN);
            U_PORT_TEST_ASSERT(location.svs == -1);
        } else {
            U_TEST_PRINT_LINE("*** WARNING *** %s cloud service was unable to determine position,"
                              " HTTP status code %d.", gLocType[gIteration].pName, z);
        }

        // Should do any harm to call this here
        uWifiLocGetStop(gHandles.devHandle);

        U_TEST_PRINT_LINE("testing non-blocking Wifi location with %s.", gLocType[gIteration].pName);
        // It is possible for these cloud services to fail, so give them a few goes
        gCallback = INT_MIN;
        for (size_t y = 0; (y < U_WIFI_LOC_TEST_TRIES) && (gCallback != 0); y++) {
            startTimeMs = uPortGetTickTimeMs();
            gCallback = INT_MIN;
            locationSetDefaults(&location);
            z = uWifiLocGetStart(gHandles.devHandle, gLocType[gIteration].type,
                                 gLocType[gIteration].pApiKey,
                                 U_WIFI_LOC_TEST_AP_FILTER,
                                 U_WIFI_LOC_TEST_RSSI_FILTER_DBM,
                                 callback);
            U_TEST_PRINT_LINE("uWifiLocGetStart() for %s returned %d.", gLocType[gIteration].pName, z);
            U_PORT_TEST_ASSERT(z == 0);
            U_TEST_PRINT_LINE("waiting %d second(s) for result...", U_WIFI_LOC_TEST_TIMEOUT_SECONDS);
            while ((gCallback == INT_MIN) &&
                   ((uPortGetTickTimeMs() - startTimeMs) < U_WIFI_LOC_TEST_TIMEOUT_SECONDS * 1000)) {
                uPortTaskBlock(250);
            }
            if (gCallback != 0) {
                U_TEST_PRINT_LINE("stopping async location on failure...");
                uWifiLocGetStop(gHandles.devHandle);
            }
        }
        uWifiLocGetStop(gHandles.devHandle);
        uWifiLocFree(gHandles.devHandle);
        U_TEST_PRINT_LINE("gCallback was %d after %d second(s).", gCallback,
                          (uPortGetTickTimeMs() - startTimeMs) / 1000);
        U_PORT_TEST_ASSERT(gCallback >= 0);
        if (gCallback != 0) {
            // Sometimes the cloud service (e.g. Here does this on occasion) is
            // unable to determine position
            U_TEST_PRINT_LINE("*** WARNING *** %s cloud service was unable to determine position,"
                              " HTTP status code %d.", gLocType[gIteration].pName,  gCallback);
        }
    }

    uWifiTestPrivatePostamble(&gHandles);

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[wifiLoc]", "wifiLocCleanUp")
{
    int32_t x;

    if (gHandles.devHandle != NULL) {
        uWifiLocGetStop(gHandles.devHandle);
    }
    uWifiTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
