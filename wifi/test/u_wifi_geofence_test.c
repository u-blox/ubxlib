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
 * @brief Tests for the wifi geofence API: if U_CFG_GEOFENCE is
 * defined, these tests should pass on all platforms that have a
 * short-range module that supports wifi connected to them.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#ifdef U_CFG_GEOFENCE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.

// Must always be included before u_short_range_test_selector.h
//lint -efile(766, u_wifi_module_type.h)
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

# if U_SHORT_RANGE_TEST_WIFI()

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

#include "u_test_util_resource_check.h"

#include "u_location.h"

#include "u_linked_list.h"
#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"
#include "u_wifi.h"
#include "u_wifi_loc.h"
#include "u_wifi_geofence.h"

#include "u_geofence_test_data.h"

#include "u_wifi_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
# define U_TEST_PREFIX "U_WIFI_GEOFENCE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
# define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_WIFI_GEOFENCE_TEST_AP_FILTER
/** The minimum number of Wifi access points required to cause
 * a position request to a cloud service: use the minimum (5).
 */
# define U_WIFI_GEOFENCE_TEST_AP_FILTER 5
#endif

#ifndef U_WIFI_GEOFENCE_TEST_RSSI_FILTER_DBM
/** The minimum RSSI to receive a Wifi access point at for it
 * to be used in a request to a cloud service: use the minimum (-100).
 */
# define U_WIFI_GEOFENCE_TEST_RSSI_FILTER_DBM -100
#endif

#ifndef U_WIFI_GEOFENCE_TEST_TIMEOUT_SECONDS
/** The timeout when waiting for position from a cloud service:
 * they don't generally take very long to respond.
 */
# define U_WIFI_GEOFENCE_TEST_TIMEOUT_SECONDS 30
#endif

#ifndef U_WIFI_GEOFENCE_TEST_RADIUS_METRES
/** The radius of position used in the "live" geofence tests.
  */
# define U_WIFI_GEOFENCE_TEST_RADIUS_METRES 500
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uShortRangeUartConfig_t gUart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                         .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                         .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                         .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                         .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                         .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
# ifdef U_CFG_APP_UART_PREFIX // Relevant for Linux only
                                         .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX)
# else
                                         .pPrefix = NULL
# endif
                                       };

static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

static int32_t gStopTimeMs;
static int32_t gErrorCode;

static uGeofence_t *gpFenceA = NULL;
static uGeofence_t *gpFenceB = NULL;

static uGeofencePositionState_t gPositionStateA;
static uGeofencePositionState_t gPositionStateB;

static const char *gpPositionStateString[] = {"none", "inside", "outside"};

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

// Stub position callback.
static void posCallback(uDeviceHandle_t wifiHandle, int32_t errorCode,
                        const uLocation_t *pLocation)
{
    (void) wifiHandle;
    (void) errorCode;
    (void) pLocation;
}

// Fence callback.
static void callback(uDeviceHandle_t wifiHandle,
                     const void *pFence, const char *pNameStr,
                     uGeofencePositionState_t positionState,
                     int64_t latitudeX1e9,
                     int64_t longitudeX1e9,
                     int32_t altitudeMillimetres,
                     int32_t radiusMillimetres,
                     int32_t altitudeUncertaintyMillimetres,
                     int64_t distanceMillimetres,
                     void *pCallbackParam)
{
    int32_t *pErrorCode = (int32_t *) pCallbackParam;

    (void) latitudeX1e9;
    (void) longitudeX1e9;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) altitudeUncertaintyMillimetres;
    (void) distanceMillimetres;

    if (pErrorCode != NULL) {
        (*pErrorCode)++;
        if (wifiHandle != gHandles.devHandle) {
            *pErrorCode = -100;
        }
        if (pFence == NULL) {
            *pErrorCode = -101;
        } else {
            if ((pFence != gpFenceA) && (pFence != gpFenceB)) {
                *pErrorCode = -102;
            } else {
                if (pFence == gpFenceA) {
                    if (pNameStr != gpFenceA->pNameStr) {
                        *pErrorCode = -103;
                    }
                    gPositionStateA = positionState;
                } else {
                    if (pNameStr != gpFenceB->pNameStr) {
                        *pErrorCode = -104;
                    }
                    gPositionStateB = positionState;
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiGeofence]", "wifiGeofenceBasic")
{
    int32_t resourceCount;
    uLocation_t location;
    int32_t startTimeMs;
    int32_t x;

    resourceCount = uTestUtilGetDynamicResourceCount();

    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);

    // Create two fences, one containing a circle centred
    // on the location of the test system, the other containing
    // a circle some distance away
    U_TEST_PRINT_LINE("fence A: %d m circle centred on the test system.",
                      U_WIFI_GEOFENCE_TEST_RADIUS_METRES);
    gpFenceA = pUGeofenceCreate("test system");
    U_PORT_TEST_ASSERT(gpFenceA != NULL);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA,
                                          U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                          U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9,
                                          U_WIFI_GEOFENCE_TEST_RADIUS_METRES *  1000) == 0);
    gpFenceB = pUGeofenceCreate("not the test system");
    U_PORT_TEST_ASSERT(gpFenceB != NULL);
    U_TEST_PRINT_LINE("fence B: %d m circle a bit to the right, not near the test system.",
                      U_WIFI_GEOFENCE_TEST_RADIUS_METRES);
    // Note: we used to have this just 0.1 degrees away but, for whatever reason, in our
    // location Google can sometimes return a result with a radius of uncertainty of 6 km,
    // hence we now make it 1 degree away
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceB,
                                          U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                          U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9  + 1000000000LL,
                                          U_WIFI_GEOFENCE_TEST_RADIUS_METRES * 1000) == 0);

    // Add a callback
    gErrorCode = 0;
    gPositionStateA = U_GEOFENCE_POSITION_STATE_NONE;
    gPositionStateB = U_GEOFENCE_POSITION_STATE_NONE;
    // Be optimistic about this, Wifi position can be a bit wacky sometimes
    U_PORT_TEST_ASSERT(uWifiGeofenceSetCallback(gHandles.devHandle,
                                                U_GEOFENCE_TEST_TYPE_INSIDE, false,
                                                callback, &gErrorCode) == 0);

    // Apply both fences to the Wifi instance
    U_PORT_TEST_ASSERT(uWifiGeofenceApply(gHandles.devHandle, gpFenceA) == 0);
    U_PORT_TEST_ASSERT(uWifiGeofenceApply(gHandles.devHandle, gpFenceB) == 0);

    U_TEST_PRINT_LINE("testing geofence with blocking Wifi location.");
    startTimeMs = uPortGetTickTimeMs();
    gStopTimeMs = startTimeMs + U_WIFI_GEOFENCE_TEST_TIMEOUT_SECONDS * 1000;
    // Choose Google to do this with as it seems generally the most reliable
    x = uWifiLocGet(gHandles.devHandle, U_LOCATION_TYPE_CLOUD_GOOGLE,
                    U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GOOGLE_MAPS_API_KEY),
                    U_WIFI_GEOFENCE_TEST_AP_FILTER,
                    U_WIFI_GEOFENCE_TEST_RSSI_FILTER_DBM,
                    &location, keepGoingCallback);
    U_TEST_PRINT_LINE("uWifiLocGet() returned %d in %d ms.",
                      x, uPortGetTickTimeMs() - startTimeMs);
    U_TEST_PRINT_LINE("%s fence A, %s fence B.",
                      gpPositionStateString[gPositionStateA],
                      gpPositionStateString[gPositionStateB]);
    // The callback should have been called twice, once for each fence
    U_PORT_TEST_ASSERT(gErrorCode == 2);
    U_PORT_TEST_ASSERT(gPositionStateA == U_GEOFENCE_POSITION_STATE_INSIDE);
    U_PORT_TEST_ASSERT(gPositionStateB == U_GEOFENCE_POSITION_STATE_OUTSIDE);
    U_PORT_TEST_ASSERT(x == 0);

    U_TEST_PRINT_LINE("testing geofence with non-blocking Wifi location.");
    gErrorCode = 0;
    gPositionStateA = U_GEOFENCE_POSITION_STATE_NONE;
    gPositionStateB = U_GEOFENCE_POSITION_STATE_NONE;
    startTimeMs = uPortGetTickTimeMs();
    x = uWifiLocGetStart(gHandles.devHandle, U_LOCATION_TYPE_CLOUD_GOOGLE,
                         U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GOOGLE_MAPS_API_KEY),
                         U_WIFI_GEOFENCE_TEST_AP_FILTER,
                         U_WIFI_GEOFENCE_TEST_RSSI_FILTER_DBM,
                         posCallback);
    U_TEST_PRINT_LINE("uWifiLocGetStart() returned %d.", x);
    U_PORT_TEST_ASSERT(x == 0);
    U_TEST_PRINT_LINE("waiting %d second(s) for result...", U_WIFI_GEOFENCE_TEST_TIMEOUT_SECONDS);
    while ((gErrorCode >= 0) && (gErrorCode < 2) &&
           ((uPortGetTickTimeMs() - startTimeMs) < U_WIFI_GEOFENCE_TEST_TIMEOUT_SECONDS * 1000)) {
        uPortTaskBlock(250);
    }
    // On really fast systems (e.g. Linux machines) it is possible
    // for the callback to have not quite exitted when we get here, so
    // give it a moment to do so
    uPortTaskBlock(250);
    uWifiLocGetStop(gHandles.devHandle);
    U_TEST_PRINT_LINE("gErrorCode was %d after %d second(s).", gErrorCode,
                      (uPortGetTickTimeMs() - startTimeMs) / 1000);
    U_TEST_PRINT_LINE("%s fence A, %s fence B.",
                      gpPositionStateString[gPositionStateA],
                      gpPositionStateString[gPositionStateB]);
    U_PORT_TEST_ASSERT(gErrorCode == 2);
    U_PORT_TEST_ASSERT(gPositionStateA == U_GEOFENCE_POSITION_STATE_INSIDE);
    U_PORT_TEST_ASSERT(gPositionStateB == U_GEOFENCE_POSITION_STATE_OUTSIDE);

    // Remove the fences and free them
    U_PORT_TEST_ASSERT(uWifiGeofenceRemove(gHandles.devHandle, NULL) == 0);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceA) == 0);
    gpFenceA = NULL;
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceB) == 0);
    gpFenceB = NULL;

    uWifiTestPrivatePostamble(&gHandles);

    // Free the mutex so that our memory sums add up
    uGeofenceCleanUp();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[wifiGeofence]", "wifiGeofenceCleanUp")
{
    if (gHandles.devHandle != NULL) {
        uWifiLocGetStop(gHandles.devHandle);
    }
    uWifiTestPrivateCleanup(&gHandles);

    // In case a fence was left hanging
    uWifiGeofenceRemove(NULL, NULL);
    uGeofenceFree(gpFenceA);
    uGeofenceFree(gpFenceB);
    uGeofenceCleanUp();

    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

# endif // U_SHORT_RANGE_TEST_WIFI()
#endif // U_CFG_GEOFENCE

// End of file
