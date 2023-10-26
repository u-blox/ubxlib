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
 * @brief Tests for the cellular geofence API: these should pass on
 * all platforms that have a cellular module connected to them and
 * where a CellLocate subscription is available.  They are only
 * compiled if both U_CFG_GEOFENCE and U_CFG_TEST_CELL_MODULE_TYPE are
 * defined and only do anything useful if U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN
 * and U_CFG_TEST_CELL_GEOFENCE are defined.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#if defined(U_CFG_GEOFENCE) && defined(U_CFG_TEST_CELL_MODULE_TYPE)

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_location.h"

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_test_util_resource_check.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_loc.h"
#if U_CFG_APP_PIN_CELL_PWR_ON < 0
# include "u_cell_pwr.h"
#endif
#ifdef U_CELL_TEST_MUX_ALWAYS
# include "u_cell_mux.h"
#endif
#include "u_cell_geofence.h"

#include "u_geofence_test_data.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_GEOFENCE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS
/** The position establishment timeout to use during testing, in
 * seconds.
 */
# define U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS 180
#endif

#ifndef U_CELL_GEOFENCE_TEST_RADIUS_METRES
/** The radius of position used in the "live" geofence tests:
 * has to be relatively large for CellLocate.
  */
# define U_CELL_GEOFENCE_TEST_RADIUS_METRES 10000
#endif

#ifndef U_CELL_GEOFENCE_TEST_BAD_STATUS_LIMIT
/** The maximum number of fatal-type location status checks
 * to tolerate before giving up, as a back-stop for SARA-R4
 * not giving an answer.  Since we query the status once a
 * second, should be more than the time we ask Cell Locate
 * to respond in, which is by default
 * U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS.
 */
#define U_CELL_GEOFENCE_TEST_BAD_STATUS_LIMIT (U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS + 30)
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

/** A geofence.
 */
static uGeofence_t *gpFenceA = NULL;

/** A second geofence.
 */
static uGeofence_t *gpFenceB = NULL;

#if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Variable to track the errors in the callback.
 */
static int32_t gErrorCode;

/** Variable to track the position state of Fence A according to
 * the callback.
 */
static uGeofencePositionState_t gPositionStateA;

/** Variable to track the position state of Fence B according to
 * the callback.
 */
static uGeofencePositionState_t gPositionStateB;

/** String to print for each position state.
 */
static const char *gpPositionStateString[] = {"none", "inside", "outside"};
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)
// Callback function for the position establishment process.
static bool keepGoingCallback(uDeviceHandle_t cellHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT(cellHandle == gHandles.cellHandle);
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Stub position callback.
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
    (void) cellHandle;
    (void) errorCode;
    (void) latitudeX1e7;
    (void) longitudeX1e7;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) speedMillimetresPerSecond;
    (void) svs;
    (void) timeUtc;
}

// Fence callback.
static void callback(uDeviceHandle_t cellHandle,
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

    if (pErrorCode != NULL) {
        *pErrorCode = 0;
        if (cellHandle != gHandles.cellHandle) {
            *pErrorCode = 1;
        }
        if (pFence == NULL) {
            *pErrorCode = 2;
        } else {
            if ((pFence != gpFenceA) && (pFence != gpFenceB)) {
                *pErrorCode = 3;
            } else {
                if (pFence == gpFenceA) {
                    if (pNameStr != gpFenceA->pNameStr) {
                        *pErrorCode = 4;
                    }
                    gPositionStateA = positionState;
                } else {
                    if (pNameStr != gpFenceB->pNameStr) {
                        *pErrorCode = 5;
                    }
                    gPositionStateB = positionState;
                }
            }
        }
    }
}
#endif // #if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test geofencing with cellular.  This MUST be tested on the live
 * cellular network, otherwise it will not get position from
 * CellLocate.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellGeofence]", "cellGeofenceLive")
{
#if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)
    uDeviceHandle_t cellHandle;
    int32_t resourceCount;
    int32_t startTime;
    int32_t x;
    size_t badStatusCount;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Configure the module pins in case a GNSS chip is present
# if (U_CFG_APP_CELL_PIN_GNSS_POWER >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssPwr(cellHandle,
                                                 U_CFG_APP_CELL_PIN_GNSS_POWER) == 0);
    }
# endif

# if (U_CFG_APP_CELL_PIN_GNSS_DATA_READY >= 0)
    if (!uCellLocGnssInsideCell(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellLocSetPinGnssDataReady(cellHandle,
                                                       U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
    }
# endif

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
    gStopTimeMs = uPortGetTickTimeMs() + (U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS * 1000);
    x = uCellNetConnect(cellHandle, NULL,
# ifdef U_CELL_TEST_CFG_APN
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
# else
                        NULL,
# endif
# ifdef U_CELL_TEST_CFG_USERNAME
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
# else
                        NULL,
# endif
# ifdef U_CELL_TEST_CFG_PASSWORD
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
# else
                        NULL,
# endif
                        keepGoingCallback);
    U_PORT_TEST_ASSERT(x == 0);

    // Create two fences, one containing a circle centred
    // on the location of the test system, the other containing
    // a circle some distance away
    U_TEST_PRINT_LINE("fence A: %d m circle centred on the test system.",
                      U_CELL_GEOFENCE_TEST_RADIUS_METRES);
    gpFenceA = pUGeofenceCreate("test system");
    U_PORT_TEST_ASSERT(gpFenceA != NULL);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA,
                                          U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                          U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9,
                                          U_CELL_GEOFENCE_TEST_RADIUS_METRES *  1000) == 0);
    gpFenceB = pUGeofenceCreate("not the test system");
    U_PORT_TEST_ASSERT(gpFenceB != NULL);
    U_TEST_PRINT_LINE("fence B: %d m circle a bit to the right, not near the test system.",
                      U_CELL_GEOFENCE_TEST_RADIUS_METRES);
    U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceB,
                                          U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                          U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9 + 200000000LL,
                                          U_CELL_GEOFENCE_TEST_RADIUS_METRES * 1000) == 0);

    // Add a callback
    gErrorCode = 0xFFFFFFFF;
    gPositionStateA = U_GEOFENCE_POSITION_STATE_NONE;
    gPositionStateB = U_GEOFENCE_POSITION_STATE_NONE;
    U_PORT_TEST_ASSERT(uCellGeofenceSetCallback(cellHandle,
                                                U_GEOFENCE_TEST_TYPE_INSIDE, true,
                                                callback, &gErrorCode) == 0);

    // Apply both fences to the cellular instance
    U_PORT_TEST_ASSERT(uCellGeofenceApply(cellHandle, gpFenceA) == 0);
    U_PORT_TEST_ASSERT(uCellGeofenceApply(cellHandle, gpFenceB) == 0);

    // Get position, blocking version
    U_TEST_PRINT_LINE("cell locate, blocking version.");
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS * 1000;
    x = uCellLocGet(cellHandle, NULL, NULL, NULL, NULL,
                    NULL, NULL, NULL, keepGoingCallback);
    U_TEST_PRINT_LINE("result was %d, gErrorCode was %d.", x, gErrorCode);
    U_TEST_PRINT_LINE("%s fence A, %s fence B.",
                      gpPositionStateString[gPositionStateA],
                      gpPositionStateString[gPositionStateB]);
    if (x == 0) {
    U_TEST_PRINT_LINE("location establishment took %d second(s).",
                      (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
    }
    U_PORT_TEST_ASSERT(x == 0);

    U_PORT_TEST_ASSERT(gErrorCode == 0);
    U_PORT_TEST_ASSERT(gPositionStateA == U_GEOFENCE_POSITION_STATE_INSIDE);
    U_PORT_TEST_ASSERT(gPositionStateB == U_GEOFENCE_POSITION_STATE_OUTSIDE);

    // Get position, non-blocking version
    U_TEST_PRINT_LINE("location establishment, non-blocking version.");
    // Try this a few times as the Cell Locate AT command can sometimes
    // (e.g. on SARA-R412M-02B) return "generic error" if asked to establish
    // location again quickly after returning an answer
    gErrorCode = 0xFFFFFFFF;
    for (int32_t y = 3; (y > 0) && (gErrorCode != 0); y--) {
        gErrorCode = 0xFFFFFFFF;
        gPositionStateA = U_GEOFENCE_POSITION_STATE_NONE;
        gPositionStateB = U_GEOFENCE_POSITION_STATE_NONE;
        gStopTimeMs = startTime + U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS * 1000;
        startTime = uPortGetTickTimeMs();
        U_PORT_TEST_ASSERT(uCellLocGetStart(cellHandle, posCallback) == 0);
        U_TEST_PRINT_LINE("waiting up to %d second(s) for results from asynchonous API...",
                          U_CELL_GEOFENCE_TEST_TIMEOUT_SECONDS);
        badStatusCount = 0;
        while ((gErrorCode == 0xFFFFFFFF) && (uPortGetTickTimeMs() < gStopTimeMs) &&
               (badStatusCount < U_CELL_GEOFENCE_TEST_BAD_STATUS_LIMIT)) {
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
        if (gErrorCode == 0) {
            U_TEST_PRINT_LINE("location establishment took %d second(s).",
                              (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
            U_TEST_PRINT_LINE("result was %d, gErrorCode was %d.", x, gErrorCode);
            U_TEST_PRINT_LINE("%s fence A, %s fence B.",
                              gpPositionStateString[gPositionStateA],
                              gpPositionStateString[gPositionStateB]);
        }
        if ((gErrorCode != 0) && (y >= 1)) {
            uCellLocGetStop(cellHandle);
            U_TEST_PRINT_LINE("failed to get an answer, will retry in 30 seconds...");
            uPortTaskBlock(30000);
        }
    }
    U_PORT_TEST_ASSERT(gErrorCode == 0);
    U_PORT_TEST_ASSERT(gPositionStateA == U_GEOFENCE_POSITION_STATE_INSIDE);
    U_PORT_TEST_ASSERT(gPositionStateB == U_GEOFENCE_POSITION_STATE_OUTSIDE);

#if U_CFG_APP_PIN_CELL_PWR_ON < 0
    // The standard postamble would normally power the module off
    // but if there is no power-on pin it won't (for obvious reasons)
    // so instead reboot here to ensure a clean start
    uCellPwrReboot(cellHandle, NULL);
# ifdef U_CELL_TEST_MUX_ALWAYS
    uCellMuxEnable(cellHandle);
# endif
#endif

    // Remove the fences and free them
    U_PORT_TEST_ASSERT(uCellGeofenceRemove(cellHandle, NULL) == 0);
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceA) == 0);
    gpFenceA = NULL;
    U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceB) == 0);
    gpFenceB = NULL;

    // Do the standard postamble, and this time switch the module
    // off as I've seen some modules end up in a funny state after
    // this test, where they look fine and dandy until, in the
    // following test, the code sends AT+CFUN=4: after which they
    // (SARA-R5) can become unresponsive.
    uCellTestPrivatePostamble(&gHandles, true);

    // Free the mutex so that our memory sums add up
    uGeofenceCleanUp();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
#else // #if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)
    U_TEST_PRINT_LINE("*** WARNING *** U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN"
                      " is not defined, unable to run the Cell Geofence test.");
#endif // #if defined(U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_CELL_GEOFENCE)
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellGeofence]", "cellGeofenceCleanUp")
{
    uCellTestPrivateCleanup(&gHandles);

    // In case a fence was left hanging
    uCellGeofenceRemove(NULL, NULL);
    uGeofenceFree(gpFenceA);
    uGeofenceFree(gpFenceB);
    uGeofenceCleanUp();

    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #if defined(U_CFG_GEOFENCE) && defined(U_CFG_TEST_CELL_MODULE_TYPE)

// End of file
