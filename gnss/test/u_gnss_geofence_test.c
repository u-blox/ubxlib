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
 * @brief Tests for the GNSS geofence API: if U_CFG_GEOFENCE is
 * defined, these tests should pass on all platforms that have a
 * GNSS module connected to them.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#if defined(U_CFG_GEOFENCE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE)

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "limits.h"    // INT_MAX, INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strlen(), memcpy()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_at_client.h" // Required by u_gnss_private.h

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_port_clib_platform_specific.h" /* must be included before the other
                                              port files if any print or scan
                                              function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_i2c.h"
#include "u_port_spi.h"

#include "u_test_util_resource_check.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_geofence.h"
#include "u_gnss_pwr.h"     // U_GNSS_RESET_TIME_SECONDS
#include "u_gnss_msg.h"     // uGnssMsgReceiveStatStreamLoss(), uGnssMsgSend()
#include "u_gnss_info.h"    // uGnssInfoGetCommunicationStats()
#include "u_gnss_pos.h"
#include "u_gnss_geofence.h"
#include "u_gnss_private.h"

#include "u_geofence_test_data.h"
#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_GNSS_GEOFENCE_TEST"

/** The string to put at the start of all prints from this test
 * that do not require any iterations on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration(s) version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and an iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS
/** The timeout on position establishment.
 */
# define U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS 180
#endif

#ifndef U_GNSS_GEOFENCE_TEST_POS_RADIUS_METRES
/** The radius of position used in the "live" geofence tests:
 * leave plenty of room, don't want tests failing because of
 * poor GNSS results.
  */
# define U_GNSS_GEOFENCE_TEST_POS_RADIUS_METRES 1000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a vertex.
 */
typedef struct {
    int64_t latitudeX1e9;
    int64_t longitudeX1e9;
} uGnssGeofenceTestVertex_t;

/** Structure to hold a pointer to a fence and the expected
 * outcome for that fence.
 */
typedef struct {
    uGeofencePositionState_t positionState;
    const uGeofence_t *pFence;
} uGnssGeofenceTestCallbackOutcome_t;

/** Structure to hold the parameters received by a callback
 * that may change per position tested.
 */
typedef struct {
    uGeofencePositionState_t positionStateA;
    uGeofencePositionState_t positionStateB;
    uGnssGeofenceTestVertex_t position;
    int32_t altitudeMillimetres;
    int32_t radiusMillimetres;
    int32_t altitudeUncertaintyMillimetres;
    int64_t distanceMillimetres;
    int32_t statusCode;
    size_t called;
} uGnssGeofenceTestCallbackParams_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Variable to track the parameters received by a callback that
 * vary with the position being tested.
 */
static uGnssGeofenceTestCallbackParams_t gCallbackParameters;

/** A geofence.
 */
static uGeofence_t *gpFenceA = NULL;

/** A second geofence.
 */
static uGeofence_t *gpFenceB = NULL;

/** String to print for each test type.
 */
static const char *gpTestTypeString[] = {"none", "in", "out", "transit"};

/** String to print for each position state.
 */
static const char *gpPositionStateString[] = {"none", "inside", "outside"};

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

// Stub function for non-blocking position API.
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
    (void) gnssHandle;
    (void) errorCode;
    (void) latitudeX1e7;
    (void) longitudeX1e7;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) speedMillimetresPerSecond;
    (void) svs;
    (void) timeUtc;
}

// Convert a lat/long into a whole number and a bit-after-the-decimal-point
// that can be printed by a version of printf() that does not support
// floating point operations or 64 bit numbers, returning the prefix
// (either "+" or "-") and the fractional part in two halves.  The result
// should be printed with printf() format specifiers %c%d.%06d%03d,
// e.g. something like:
//
// int32_t whole;
// int32_t fractionUpper;
// int32_t fractionLower;
//
// printf("%c%d.%06d%03d/%c%d.%06d%03d", latLongToBits(latitudeX1e9, &whole,
//                                                     &fractionUpper,
//                                                     &fractionLower),
//                               whole, fractionUpper, fractionLower,
//                               latLongToBits(longitudeX1e9, &whole,
//                                             &fractionUpper,
//                                             &fractionLower),
//                               whole, fractionUpper, fractionLower);
static char latLongToBits(int64_t thingX1e9,
                          int32_t *pWhole,
                          int32_t *pFractionUpper,
                          int32_t *pFractionLower)
{
    char prefix = '+';
    int64_t fraction;

    // Deal with the sign
    if (thingX1e9 < 0) {
        thingX1e9 = -thingX1e9;
        prefix = '-';
    }
    *pWhole = (int32_t) (thingX1e9 / 1000000000);
    fraction = thingX1e9 % 1000000000;
    *pFractionUpper = (int32_t) (fraction / 1000);
    *pFractionLower = (int32_t) (fraction % 1000);

    return prefix;
}

// Print out the latitude/longitude of a test vertex.
static void printTestVertex(const char *pPrefix,
                            const uGnssGeofenceTestVertex_t *pTestVertex)
{
    char sign[2] = {0};
    int32_t whole[2] = {0};
    int32_t fractionUpper[2] = {0};
    int32_t fractionLower[2] = {0};

    sign[0] = latLongToBits(pTestVertex->latitudeX1e9,
                            &(whole[0]), &(fractionUpper[0]), &(fractionLower[0]));
    sign[1] = latLongToBits(pTestVertex->longitudeX1e9,
                            &(whole[1]), &(fractionUpper[1]), &(fractionLower[1]));

    if (pPrefix == NULL) {
        pPrefix = "";
    }
    uPortLog("%s%c%d.%06d%03d,%c%d.%06d%03d", pPrefix,
             sign[0], whole[0], fractionUpper[0], fractionLower[0],
             sign[1], whole[1], fractionUpper[1], fractionLower[1]);
}

// Return true if a fence is modifiable in all permitted ways,
// else false, noting that this will FREE the fence if it IS modifiable
static bool modifyAndFree(uGeofence_t **ppFence)
{
    bool modifiable = (uGeofenceAddCircle(*ppFence, 0, 0, 1000) == 0) &&
                      (uGeofenceAddVertex(*ppFence, 0, 0, false) == 0) &&
                      (uGeofenceSetAltitudeMax(*ppFence, INT_MAX) == 0) &&
                      (uGeofenceSetAltitudeMin(*ppFence, INT_MIN) == 0) &&
                      (uGeofenceClearMap(*ppFence) == 0) &&
                      (uGeofenceFree(*ppFence) == 0);
    if (modifiable) {
        // NULL the pointer if we successfully free'd the fence
        *ppFence = NULL;
    }

    return modifiable;
}

// Fence callback.
static void callback(uDeviceHandle_t gnssHandle,
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
    uGnssGeofenceTestCallbackParams_t *pCallbackParameters = (uGnssGeofenceTestCallbackParams_t *)
                                                             pCallbackParam;

    if (pCallbackParameters != NULL) {
        pCallbackParameters->called++;
        pCallbackParameters->statusCode = 0;
        if (gnssHandle != gHandles.gnssHandle) {
            pCallbackParameters->statusCode = 1;
        }
        if (pFence == NULL) {
            pCallbackParameters->statusCode = 2;
        } else {
            if ((pFence != gpFenceA) && (pFence != gpFenceB)) {
                pCallbackParameters->statusCode = 3;
            } else {
                if (pFence == gpFenceA) {
                    if (pNameStr != gpFenceA->pNameStr) {
                        pCallbackParameters->statusCode = 4;
                    }
                    pCallbackParameters->positionStateA = positionState;
                } else {
                    if (pNameStr != gpFenceB->pNameStr) {
                        pCallbackParameters->statusCode = 5;
                    }
                    pCallbackParameters->positionStateB = positionState;
                }
            }
        }
        pCallbackParameters->position.latitudeX1e9 = latitudeX1e9;
        pCallbackParameters->position.longitudeX1e9 = longitudeX1e9;
        pCallbackParameters->altitudeMillimetres = altitudeMillimetres;
        pCallbackParameters->radiusMillimetres = radiusMillimetres;
        pCallbackParameters->altitudeUncertaintyMillimetres = altitudeUncertaintyMillimetres;
        pCallbackParameters->distanceMillimetres = distanceMillimetres;
    }
}

// Set up a callback.
static int32_t setCallback(uDeviceHandle_t gnssHandle, uGeofenceTestType_t testType,
                           bool pessimisticNotOptimistic)
{
    U_TEST_PRINT_LINE("  callback type \"%s %s\"",
                      pessimisticNotOptimistic ? "pessimistic" : "optimistic",
                      gpTestTypeString[testType]);
    return uGnssGeofenceSetCallback(gnssHandle, testType,
                                    pessimisticNotOptimistic,
                                    callback, &gCallbackParameters);
}

// Set up a callback parameters structure.
static void setCallbackParams(uGnssGeofenceTestCallbackParams_t *pParams,
                              uGeofencePositionState_t positionStateA,
                              uGeofencePositionState_t positionStateB,
                              int64_t latitudeX1e9,
                              int64_t longitudeX1e9,
                              int32_t altitudeMillimetres,
                              int32_t radiusMillimetres,
                              int32_t altitudeUncertaintyMillimetres)
{
    pParams->positionStateA = positionStateA;
    pParams->positionStateB = positionStateB;
    pParams->position.latitudeX1e9 = latitudeX1e9;
    pParams->position.longitudeX1e9 = longitudeX1e9;
    pParams->altitudeMillimetres = altitudeMillimetres;
    pParams->radiusMillimetres = radiusMillimetres;
    pParams->altitudeUncertaintyMillimetres = altitudeUncertaintyMillimetres;
    pParams->distanceMillimetres = LLONG_MIN;
    pParams->statusCode = 0;
    pParams->called = 2;

    if ((pParams->position.latitudeX1e9 != LLONG_MIN) &&
        (pParams->position.longitudeX1e9 != LLONG_MIN)) {
        printTestVertex(U_TEST_PREFIX "  test position ", &(pParams->position));
        uPortLog(", radius %d.%03d m.\n", (int) (radiusMillimetres / 1000),
                 (int) (radiusMillimetres % 1000));
    }
}

// Test a position.
static uGeofencePositionState_t testPosition(uDeviceHandle_t gnssHandle,
                                             uGeofenceTestType_t testType,
                                             bool pessimisticNotOptimistic,
                                             uGnssGeofenceTestCallbackParams_t *pParams)
{
    uGeofencePositionState_t positionState;

    memset(&gCallbackParameters, 0, sizeof(gCallbackParameters));
    positionState = uGnssGeofencePosition(gnssHandle, testType,
                                          pessimisticNotOptimistic,
                                          pParams->position.latitudeX1e9,
                                          pParams->position.longitudeX1e9,
                                          pParams->altitudeMillimetres,
                                          pParams->radiusMillimetres,
                                          pParams->altitudeUncertaintyMillimetres);
    uPortLog(U_TEST_PREFIX "  uGnssGeofencePosition() ");
    if (testType != U_GEOFENCE_TEST_TYPE_NONE) {
        uPortLog("\"%s %s\" check ",
                 pessimisticNotOptimistic ? "pessimistic" : "optimistic",
                 gpTestTypeString[testType]);
    }
    uPortLog("returned %s.\n", gpPositionStateString[positionState]);

    return positionState;
}

// Check a set of expected callback parameters against what was received
static bool checkCallbackResult(uGnssGeofenceTestCallbackParams_t *pExpected,
                                uGnssGeofenceTestCallbackParams_t *pGot)
{
    bool success = true;

    if (pGot->called != pExpected->called) {
        success = false;
        U_TEST_PRINT_LINE("  callback was called %d time(s) not %d.",
                          pGot->called, pExpected->called);
    }
    if (pExpected->statusCode != pGot->statusCode) {
        success = false;
        U_TEST_PRINT_LINE("  expected status %d, got %d.",
                          pExpected->statusCode, pGot->statusCode);
    }
    if (pExpected->positionStateA != pGot->positionStateA) {
        success = false;
        U_TEST_PRINT_LINE("  fence A expected \"%s\", got \"%s\".",
                          gpPositionStateString[pExpected->positionStateA],
                          gpPositionStateString[pGot->positionStateA]);
    } else {
        U_TEST_PRINT_LINE("  %s fence A.", gpPositionStateString[pGot->positionStateA]);
    }
    if (pExpected->positionStateB != pGot->positionStateB) {
        success = false;
        U_TEST_PRINT_LINE("  fence B expected %s, got %s.",
                          gpPositionStateString[pExpected->positionStateB],
                          gpPositionStateString[pGot->positionStateB]);
    } else {
        U_TEST_PRINT_LINE("  %s fence B.", gpPositionStateString[pGot->positionStateB]);
    }

    if ((pExpected->position.latitudeX1e9 != LLONG_MIN) &&
        (pExpected->position.longitudeX1e9 != LLONG_MIN) &&
        ((pExpected->position.latitudeX1e9 != pGot->position.latitudeX1e9) ||
         (pExpected->position.longitudeX1e9 != pGot->position.longitudeX1e9))) {
        success = false;
        uPortLog(U_TEST_PREFIX);
        printTestVertex("  expected ", &pExpected->position);
        printTestVertex(", got ", &pGot->position);
        uPortLog(".\n");
    }
    if ((pExpected->altitudeMillimetres != INT_MIN) &&
        (pExpected->altitudeMillimetres != pGot->altitudeMillimetres)) {
        success = false;
        U_TEST_PRINT_LINE("  expected altitude %d.%03d m, got %d.%03d m.",
                          (int) (pExpected->altitudeMillimetres / 1000),
                          (int) (pExpected->altitudeMillimetres % 1000),
                          (int) (pGot->altitudeMillimetres / 1000),
                          (int) (pGot->altitudeMillimetres % 1000));
    }
    if ((pExpected->radiusMillimetres >= 0) &&
        (pExpected->radiusMillimetres != pGot->radiusMillimetres)) {
        success = false;
        U_TEST_PRINT_LINE("  expected radius %d.%03d m, got %d.%03d m.",
                          (int) (pExpected->radiusMillimetres / 1000),
                          (int) (pExpected->radiusMillimetres % 1000),
                          (int) (pGot->radiusMillimetres / 1000),
                          (int) (pGot->radiusMillimetres % 1000));
    }
    if ((pExpected->altitudeMillimetres != INT_MIN) &&
        (pGot->altitudeMillimetres != INT_MIN) &&
        (pExpected->altitudeUncertaintyMillimetres != pGot->altitudeUncertaintyMillimetres)) {
        success = false;
        U_TEST_PRINT_LINE("  expected altitude uncertainty %d.%03d m, got %d.%03d m.",
                          (int) (pExpected->altitudeUncertaintyMillimetres / 1000),
                          (int) (pExpected->altitudeUncertaintyMillimetres % 1000),
                          (int) (pGot->altitudeUncertaintyMillimetres / 1000),
                          (int) (pGot->altitudeUncertaintyMillimetres % 1000));
    }
    if ((pExpected->distanceMillimetres != LLONG_MIN) &&
        (pExpected->distanceMillimetres != pGot->distanceMillimetres)) {
        success = false;
        U_TEST_PRINT_LINE("  expected distance%d.%03d m, got %d.%03d m.",
                          (int) (pExpected->distanceMillimetres / 1000),
                          (int) (pExpected->distanceMillimetres % 1000),
                          (int) (pGot->distanceMillimetres / 1000),
                          (int) (pGot->distanceMillimetres % 1000));
    }

    return success;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test geofence things with a GNSS device, with "potted" positions.
 */
U_PORT_TEST_FUNCTION("[gnssGeofence]", "gnssGeofenceBasic")
{
    uDeviceHandle_t gnssDevHandle = NULL;
    int32_t resourceCount;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    uGeofencePositionState_t positionState;
    uGnssGeofenceTestCallbackParams_t callbackParams;

    // In case fence A was left hanging
    uGeofenceFree(gpFenceA);
    gpFenceA = NULL;
    uGeofenceCleanUp();

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Get the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssDevHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssDevHandle, true);

        // Apply a NULL fence: should fail
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, NULL) < 0);
        // Remove all fences: should pass
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(gnssDevHandle, NULL) == 0);
        // Remove all fences from all instances: should pass
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(NULL, NULL) == 0);
        // Create a fence and apply it
        gpFenceA = pUGeofenceCreate("test");
        U_PORT_TEST_ASSERT(gpFenceA != NULL);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceA) == 0);
        // Now try to do stuff to it while it is applied: should all fail
        // Note: do all of them "manually" first time, afterwards just use
        // modifyAndFree().
        U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA, 0, 0, 1000) < 0);
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceA, 0, 0, false) < 0);
        U_PORT_TEST_ASSERT(uGeofenceSetAltitudeMax(gpFenceA, INT_MAX) < 0);
        U_PORT_TEST_ASSERT(uGeofenceSetAltitudeMin(gpFenceA, INT_MIN) < 0);
        U_PORT_TEST_ASSERT(uGeofenceClearMap(gpFenceA) < 0);
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceA) < 0);
        // Create a second fence and apply it
        gpFenceB = pUGeofenceCreate(NULL);
        U_PORT_TEST_ASSERT(gpFenceB != NULL);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceB) == 0);
        // Check that it is also no longer modifiable
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceB));
        // Remove the first and check that we can modify it now
        // but still can't modify the second
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(gnssDevHandle, gpFenceA) == 0);
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceB));
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(gpFenceA == NULL);
        // Recreate it again (as modifyAndFree() will have free'd it)
        gpFenceA = pUGeofenceCreate("test");
        // Re-add the first and check that it is not modifiable again
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceA) == 0);
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceB));
        // Remove the lot
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(gnssDevHandle, NULL) == 0);
        // Check that both are modifiable
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceB));
        U_PORT_TEST_ASSERT(gpFenceB == NULL);
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceA));
        // Create both fences again and apply them
        U_PORT_TEST_ASSERT(gpFenceA == NULL);
        gpFenceA = pUGeofenceCreate("test");
        U_PORT_TEST_ASSERT(gpFenceA != NULL);
        gpFenceB = pUGeofenceCreate(NULL);
        U_PORT_TEST_ASSERT(gpFenceB != NULL);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceA) == 0);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceB) == 0);
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceB));
        // Now remove the second fence from all instances
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(NULL, gpFenceB) == 0);
        // Check that the first is still not modifiable but the second is
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceB));
        U_PORT_TEST_ASSERT(gpFenceB == NULL);
        // Recreate the second fence and apply it again
        gpFenceB = pUGeofenceCreate(NULL);
        U_PORT_TEST_ASSERT(gpFenceB != NULL);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceB) == 0);
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(!modifyAndFree(&gpFenceB));
        // Now remove both from all instances to take us back to square one
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(NULL, NULL) == 0);
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceA));
        U_PORT_TEST_ASSERT(gpFenceA == NULL);
        U_PORT_TEST_ASSERT(modifyAndFree(&gpFenceB));
        U_PORT_TEST_ASSERT(gpFenceB == NULL);

        // Now create two fences, one containing two circles
        // and the other two triangles, none of which overlap
        gpFenceA = pUGeofenceCreate("two circles");
        U_PORT_TEST_ASSERT(gpFenceA != NULL);
        // A circle, 10 metres in diameter, on the equator at
        // zero longitude
        U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA, 0, 0, 10000) == 0);
        // A circle, 10 metres in diameter, on the equator,
        // one degree west of the first
        U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA, 0, -1000000000, 10000) == 0);
        U_TEST_PRINT_LINE("fence A: two circles of radius 10 metres centred at"
                          " 0,0 and 0,-1.");

        gpFenceB = pUGeofenceCreate("two triangles");
        U_PORT_TEST_ASSERT(gpFenceB != NULL);
        // A triangle like this:
        //
        // 1.00001,0
        //     x
        //     . .
        //     .  .
        //     x...x
        //    1,0  1,0.00001
        //
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000000000, 0, false) == 0);
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000010000, 0, false) == 0);
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000000000, 10000, false) == 0);
        // And again, but one degree to the west
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000000000, -1000000000, true) == 0);
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000010000, -1000000000, false) == 0);
        U_PORT_TEST_ASSERT(uGeofenceAddVertex(gpFenceB, 1000000000, -1000010000, false) == 0);
        U_TEST_PRINT_LINE("fence B: two right-angle triangles facing north-east with the"
                          " right angles at 1,0 and 1,-1.");

        // Apply both fences to the GNSS instance
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceA) == 0);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceB) == 0);

        U_TEST_PRINT_LINE("testing without callback.");
        // Test a position with no callback set:
        // test for inside, pessimistically, with a 2D position,
        // radius 1 metre, at the origin; this should be inside
        // the fence (at the centre of the first circle)
        positionState = uGnssGeofencePosition(gnssDevHandle,
                                              U_GEOFENCE_TEST_TYPE_INSIDE, true,
                                              0, 0, INT_MIN, 1000, 0);
        U_TEST_PRINT_LINE("\"pessimistic in\" check at 0,0, 1 metre radius, returned %s.",
                          gpPositionStateString[positionState]);
        U_PORT_TEST_ASSERT(positionState == U_GEOFENCE_POSITION_STATE_INSIDE);
        // Increase the uncertainty to 15 metres, so that the position might be
        // outside the fence; the pessimist should change their mind
        positionState = uGnssGeofencePosition(gnssDevHandle,
                                              U_GEOFENCE_TEST_TYPE_INSIDE, true,
                                              0, 0, INT_MIN, 15000, 0);
        U_TEST_PRINT_LINE("\"pessimistic in\" check at 0,0, 15 metre radius, returned %s.",
                          gpPositionStateString[positionState]);
        U_PORT_TEST_ASSERT(positionState == U_GEOFENCE_POSITION_STATE_OUTSIDE);
        // Change the position to do an optimistic check (still for inside)
        positionState = uGnssGeofencePosition(gnssDevHandle,
                                              U_GEOFENCE_TEST_TYPE_INSIDE, false,
                                              0, 0, INT_MIN, 15000, 0);
        U_TEST_PRINT_LINE("\"optimistic in\" check at 0,0, 15 metre radius, returned %s.",
                          gpPositionStateString[positionState]);
        U_PORT_TEST_ASSERT(positionState == U_GEOFENCE_POSITION_STATE_INSIDE);
        // And finally make it an optimistic outside check
        positionState = uGnssGeofencePosition(gnssDevHandle,
                                              U_GEOFENCE_TEST_TYPE_OUTSIDE, false,
                                              0, 0, INT_MIN, 15000, 0);
        U_TEST_PRINT_LINE("\"optimistic out\" check at 0,0, 15 metre radius, returned %s.",
                          gpPositionStateString[positionState]);
        U_PORT_TEST_ASSERT(positionState == U_GEOFENCE_POSITION_STATE_OUTSIDE);

        U_TEST_PRINT_LINE("testing with callback.");
        // Now set a callback, a pessimistic "inside" one
        U_TEST_PRINT_LINE("test type \"pessimistic in\", should be inside fence A.");
        U_PORT_TEST_ASSERT(setCallback(gnssDevHandle, U_GEOFENCE_TEST_TYPE_INSIDE, true) == 0);
        // Test the same position as above, so inside fence A and outside
        // fence B, and give no test criteria: the callback should be
        // called based on its own criteria
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          0, 0, INT_MIN, 1000, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_NONE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_INSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Expand the radius of position again, so that the pessimist
        // changes their mind on fence A
        U_TEST_PRINT_LINE("expand radius of position, the pessimist changes their mind.");
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          0, 0, INT_MIN, 15000, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_NONE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_OUTSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Now switch to a transit callback, an optimistic one
        U_TEST_PRINT_LINE("test type \"optimistic transit\" and point inside fence B.");
        U_PORT_TEST_ASSERT(setCallback(gnssDevHandle, U_GEOFENCE_TEST_TYPE_TRANSIT, false) == 0);
        // Move to within the first triangle of fence B
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          1000000000, 0, INT_MIN, 0, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_NONE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_INSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Expand the uncertainty on the test point: nothing should
        // change due to the optimism of the callback
        U_TEST_PRINT_LINE("expand radius of position, the optimist sees no change.");
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          1000000000, 0, INT_MIN, 1000, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_NONE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_INSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Now switch the transit callback to a pessimistic one
        // and the pessimist will see a transit to outside
        U_TEST_PRINT_LINE("test type \"pessimistic transit\", the pessimist sees"
                          " a transit outside fence B.");
        U_PORT_TEST_ASSERT(setCallback(gnssDevHandle, U_GEOFENCE_TEST_TYPE_TRANSIT, true) == 0);
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          1000000000, 0, INT_MIN, 1000, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_NONE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_OUTSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Finally, leave the callback and position unchanged and
        // override the test type in the call to uGnssGeofencePosition()
        // to force an "optimistic in" check
        U_TEST_PRINT_LINE("force test type to \"optimistic in\", now inside fence B.");
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          1000000000, 0, INT_MIN, 1000, 0);
        U_PORT_TEST_ASSERT(testPosition(gnssDevHandle,
                                        U_GEOFENCE_TEST_TYPE_INSIDE, false,
                                        &callbackParams) == U_GEOFENCE_POSITION_STATE_INSIDE);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Remove the fences and free them
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(gnssDevHandle, NULL) == 0);
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceA) == 0);
        gpFenceA = NULL;
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceB) == 0);
        gpFenceB = NULL;

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Free the mutex so that our memory sums add up
    uGeofenceCleanUp();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test geofence things with a GNSS device using live position.
 */
U_PORT_TEST_FUNCTION("[gnssGeofence]", "gnssGeofenceLive")
{
    uDeviceHandle_t gnssDevHandle = NULL;
    int32_t resourceCount;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    int32_t y;
    uGnssGeofenceTestCallbackParams_t callbackParams;
    int32_t startTimeMs;

    // In case fence A was left hanging
    uGnssGeofenceRemove(NULL, NULL);
    uGeofenceFree(gpFenceA);
    gpFenceA = NULL;
    uGeofenceCleanUp();

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Get the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssDevHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssDevHandle, true);

        // Create two fences, one containing a circle centred
        // on the location of the test system (or at least, its
        // GNSS antenna), the other containing a circle some
        // distance away
        U_TEST_PRINT_LINE("fence A: 100 metre circle centred on the test system.");
        gpFenceA = pUGeofenceCreate("test system");
        U_PORT_TEST_ASSERT(gpFenceA != NULL);
        U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceA,
                                              U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                              U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9,
                                              U_GNSS_GEOFENCE_TEST_POS_RADIUS_METRES *  1000) == 0);
        gpFenceB = pUGeofenceCreate("not the test system");
        U_PORT_TEST_ASSERT(gpFenceB != NULL);
        U_TEST_PRINT_LINE("fence B: 100 metre circle a bit to the right, not near the test system.");
        U_PORT_TEST_ASSERT(uGeofenceAddCircle(gpFenceB,
                                              U_GEOFENCE_TEST_SYSTEM_LATITUDE_X1E9,
                                              U_GEOFENCE_TEST_SYSTEM_LONGITUDE_X1E9  + 100000000LL,
                                              U_GNSS_GEOFENCE_TEST_POS_RADIUS_METRES * 1000) == 0);

        // Add a callback
        memset(&gCallbackParameters, 0, sizeof(gCallbackParameters));
        U_PORT_TEST_ASSERT(uGnssGeofenceSetCallback(gnssDevHandle,
                                                    U_GEOFENCE_TEST_TYPE_INSIDE, true,
                                                    callback, &gCallbackParameters) == 0);

        // Apply both fences to the GNSS instance
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceA) == 0);
        U_PORT_TEST_ASSERT(uGnssGeofenceApply(gnssDevHandle, gpFenceB) == 0);

        // Test that we are flagged as inside Fence A and outside Fence B when
        // the synchronous position API is called
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          LLONG_MIN, LLONG_MIN, INT_MIN, INT_MIN, INT_MIN);
        startTimeMs = uPortGetTickTimeMs();
        gStopTimeMs = startTimeMs + U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS * 1000;
        y = uGnssPosGet(gnssDevHandle, NULL, NULL, NULL,
                        NULL, NULL, NULL, NULL, keepGoingCallback);
        U_TEST_PRINT_LINE("calling uGnssPosGet() returned %d.", y);
        U_PORT_TEST_ASSERT(y == 0);
        U_TEST_PRINT_LINE("position establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        // Repeat for the asynchronous position API
        memset(&gCallbackParameters, 0, sizeof(gCallbackParameters));
        setCallbackParams(&callbackParams,
                          U_GEOFENCE_POSITION_STATE_INSIDE,
                          U_GEOFENCE_POSITION_STATE_OUTSIDE,
                          LLONG_MIN, LLONG_MIN, INT_MIN, INT_MIN, INT_MIN);
        startTimeMs = uPortGetTickTimeMs();
        gStopTimeMs = startTimeMs + U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS * 1000;
        y = uGnssPosGetStart(gnssDevHandle, posCallback);
        U_TEST_PRINT_LINE("calling uGnssPosGetStart() returned %d.", y);
        U_PORT_TEST_ASSERT(y == 0);
        U_TEST_PRINT_LINE("waiting up to %d second(s) for results from asynchronous API...",
                          U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS);
        while ((gCallbackParameters.called < 2) && (uPortGetTickTimeMs() < gStopTimeMs)) {
            uPortTaskBlock(1000);
        }
        U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

        if (transportTypes[x] != U_GNSS_TRANSPORT_AT) {
            // And finally, the streamed position API, where supported
            memset(&gCallbackParameters, 0, sizeof(gCallbackParameters));
            setCallbackParams(&callbackParams,
                              U_GEOFENCE_POSITION_STATE_INSIDE,
                              U_GEOFENCE_POSITION_STATE_OUTSIDE,
                              LLONG_MIN, LLONG_MIN, INT_MIN, INT_MIN, INT_MIN);
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs + U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS * 1000;
            y = uGnssPosGetStreamedStart(gnssDevHandle, 1000, posCallback);;
            U_TEST_PRINT_LINE("calling uGnssPosGetStreamedStart() returned %d.", y);
            U_PORT_TEST_ASSERT(y == 0);
            U_TEST_PRINT_LINE("waiting up to %d second(s) for results from streamed API...",
                              U_GNSS_GEOFENCE_TEST_POS_TIMEOUT_SECONDS);
            while ((gCallbackParameters.called < 2) && (uPortGetTickTimeMs() < gStopTimeMs)) {
                uPortTaskBlock(1000);
            }
            // Stop the stream before potentially asserting
            uGnssPosGetStreamedStop(gnssDevHandle);
            U_PORT_TEST_ASSERT(checkCallbackResult(&callbackParams, &gCallbackParameters));

            U_TEST_PRINT_LINE("waiting for things to calm down and then flushing...");
            uPortTaskBlock(5000);
            // Flush any remaining messages out of the system before
            // we continue, to prevent them messing up later tests
            uGnssMsgReceiveFlush(gnssDevHandle, true);
        }

        // Remove the fences and free them
        U_PORT_TEST_ASSERT(uGnssGeofenceRemove(gnssDevHandle, NULL) == 0);
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceA) == 0);
        gpFenceA = NULL;
        U_PORT_TEST_ASSERT(uGeofenceFree(gpFenceB) == 0);
        gpFenceB = NULL;

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssDevHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

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
U_PORT_TEST_FUNCTION("[gnssGeofence]", "gnssGeofenceCleanUp")
{
    // In case a fence was left hanging
    uGnssGeofenceRemove(NULL, NULL);
    uGeofenceFree(gpFenceA);
    uGeofenceFree(gpFenceB);
    uGeofenceCleanUp();

    uGnssTestPrivateCleanup(&gHandles);

    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #if defined(U_CFG_GEOFENCE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE)

// End of file
