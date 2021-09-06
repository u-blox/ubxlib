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
 * @brief Test for the location API: these should pass on all platforms
 * that have a u-blox module connected to them.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
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
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

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
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Keep track of the current network handle so that the
 * keepGoingCallback() can check it.
 */
static int32_t gNetworkHandle = -1;

/** A place to hook a writeable copy of a test location configuration.
 */
static uLocationTestCfg_t *gpLocationCfg = NULL;

/** Location structure for use in the callback for the asynchronous
 * case.
 */
static uLocation_t gLocation;

/** Keep track of the error code in the callback for the
 * asynchronous case.
 */
static int32_t gErrorCode;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for location establishment process.
static bool keepGoingCallback(int32_t networkHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT((gNetworkHandle < 0) || (networkHandle == gNetworkHandle));
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Standard preamble for the location test.
static void stdPreamble()
{
#if (U_CFG_APP_GNSS_UART < 0)
    int32_t networkHandle = -1;
#endif

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type if its not already been added
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle < 0) {
            if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
                uPortLog("U_LOCATION_TEST: adding %s network...\n",
                         gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
#if (U_CFG_APP_GNSS_UART < 0)
                // If there is no GNSS UART then any GNSS chip must
                // be connected via the cellular module's AT interface
                // hence we capture the cellular network handle here and
                // modify the GNSS configuration to use it before we add
                // the GNSS network
                uNetworkTestGnssAtConfiguration(networkHandle,
                                                gUNetworkTestCfg[x].pConfiguration);
#endif
                gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                         gUNetworkTestCfg[x].pConfiguration);
                U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
#if (U_CFG_APP_GNSS_UART < 0)
                if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                    networkHandle = gUNetworkTestCfg[x].handle;
                }
#endif
            }
        }
    }

    // Bring up each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle >= 0) {
            uPortLog("U_LOCATION_TEST: bringing up %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkUp(gUNetworkTestCfg[x].handle) == 0);
        }
    }
}

// Get the GNSS network handle from the set we have brought up.
static int32_t getGnssNetworkHandle()
{
    int32_t gnssNetworkHandle = -1;

    for (size_t x = 0; (x < gUNetworkTestCfgSize) && (gnssNetworkHandle < 0); x++) {
        if ((gUNetworkTestCfg[x].type == U_NETWORK_TYPE_GNSS) &&
            (gUNetworkTestCfg[x].handle >= 0)) {
            gnssNetworkHandle = gUNetworkTestCfg[x].handle;
        }
    }

    return gnssNetworkHandle;
}

// Test the blocking location API.
static void testBlocking(int32_t networkHandle,
                         uNetworkType_t networkType,
                         uLocationType_t locationType,
                         const uLocationTestCfg_t *pLocationCfg)
{
    uLocation_t location;
    int64_t startTime;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    gNetworkHandle = networkHandle;
    if (pLocationCfg != NULL) {
        pLocationAssist = pLocationCfg->pLocationAssist;
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
        if (pLocationCfg->pLocationAssist->networkHandleAssist >= 0) {
            // If an assistance network handle is in play we can't
            // check the network handle in the callback
            gNetworkHandle = -1;
        }
    }
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    uLocationTestResetLocation(&location);
    if (pLocationCfg != NULL) {
        uPortLog("U_LOCATION_TEST: blocking API.\n");
        // The location type is supported (a GNSS network always
        // supports location, irrespective of the location type) so it
        // should work
        U_PORT_TEST_ASSERT(uLocationGet(networkHandle, locationType,
                                        pLocationAssist,
                                        pAuthenticationTokenStr,
                                        &location,
                                        keepGoingCallback) == 0);
        uPortLog("U_LOCATION_TEST: location establishment took %d second(s).\n",
                 (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
        // If we are running on a local cellular network we won't get position but
        // we should always get time
        if ((location.radiusMillimetres > 0) &&
            (location.radiusMillimetres <= U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES)) {
            uLocationTestPrintLocation(&location);
            U_PORT_TEST_ASSERT(location.latitudeX1e7 > INT_MIN);
            U_PORT_TEST_ASSERT(location.longitudeX1e7 > INT_MIN);
            // Don't check altitude as we might only have a 2D fix
            U_PORT_TEST_ASSERT(location.radiusMillimetres > INT_MIN);
            if (locationType == U_LOCATION_TYPE_GNSS) {
                // Only get these for GNSS
                U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond > INT_MIN);
                U_PORT_TEST_ASSERT(location.svs > 0);
            }
        } else {
            uPortLog("U_LOCATION_TEST: only able to get time (%d).\n",
                     (int32_t) location.timeUtc);
        }
        U_PORT_TEST_ASSERT(location.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
    } else {
        if (!U_NETWORK_TEST_TYPE_HAS_LOCATION(networkType)) {
            U_PORT_TEST_ASSERT(uLocationGet(networkHandle, locationType,
                                            pLocationAssist, pAuthenticationTokenStr,
                                            &location,
                                            keepGoingCallback) < 0);
            U_PORT_TEST_ASSERT(location.latitudeX1e7 == INT_MIN);
            U_PORT_TEST_ASSERT(location.longitudeX1e7 == INT_MIN);
            U_PORT_TEST_ASSERT(location.altitudeMillimetres == INT_MIN);
            U_PORT_TEST_ASSERT(location.radiusMillimetres == INT_MIN);
            U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond == INT_MIN);
            U_PORT_TEST_ASSERT(location.svs == INT_MIN);
            U_PORT_TEST_ASSERT(location.timeUtc == LONG_MIN);
            U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond == INT_MIN);
            U_PORT_TEST_ASSERT(location.svs == INT_MIN);
        }
    }
}

// Callback function for the non-blocking API.
static void locationCallback(int32_t networkHandle,
                             int32_t errorCode,
                             const uLocation_t *pLocation)
{
    gNetworkHandle = networkHandle,
    gErrorCode = errorCode;
    if (pLocation != NULL) {
        gLocation.latitudeX1e7 = pLocation->latitudeX1e7;
        gLocation.longitudeX1e7 = pLocation->longitudeX1e7;
        gLocation.altitudeMillimetres = pLocation->altitudeMillimetres;
        gLocation.radiusMillimetres = pLocation->radiusMillimetres;
        gLocation.speedMillimetresPerSecond = pLocation->speedMillimetresPerSecond;
        gLocation.svs = pLocation->svs;
        gLocation.timeUtc = pLocation->timeUtc;
    }
}

// Test the non-blocking location API.
static void testNonBlocking(int32_t networkHandle,
                            uNetworkType_t networkType,
                            uLocationType_t locationType,
                            const uLocationTestCfg_t *pLocationCfg)
{
    int64_t startTime;
    int32_t y;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    if (pLocationCfg != NULL) {
        pLocationAssist = pLocationCfg->pLocationAssist;
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
    }
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    uLocationTestResetLocation(&gLocation);
    if (pLocationCfg != NULL) {
        // Try this a few times as the Cell Locate AT command can sometimes
        // (e.g. on SARA-R412M-02B) return "generic error" if asked to establish
        // location again quickly after returning an answer
        for (int32_t x = 3; (x > 0) && (gErrorCode != 0); x--) {
            uPortLog("U_LOCATION_TEST: non-blocking API.\n");
            gNetworkHandle = -1;
            gErrorCode = INT_MIN;
            uLocationTestResetLocation(&gLocation);
            U_PORT_TEST_ASSERT(uLocationGetStart(networkHandle, locationType,
                                                 pLocationAssist,
                                                 pAuthenticationTokenStr,
                                                 locationCallback) == 0);
            uPortLog("U_LOCATION_TEST: waiting up to %d second(s) for results from"
                     " non-blocking API...\n", U_LOCATION_TEST_CFG_TIMEOUT_SECONDS);
            while ((gErrorCode == INT_MIN) && (uPortGetTickTimeMs() < gStopTimeMs)) {
                // Location establishment status is only supported for cell locate
                y = uLocationGetStatus(networkHandle);
                if (locationType == U_LOCATION_TYPE_CLOUD_CELL_LOCATE) {
                    U_PORT_TEST_ASSERT(y >= 0);
                } else {
                    U_PORT_TEST_ASSERT(y <= (int32_t) U_LOCATION_STATUS_UNKNOWN);
                }
                uPortTaskBlock(1000);
            }

            if (gErrorCode == 0) {
                uPortLog("U_LOCATION_TEST: location establishment took %d second(s).\n",
                         (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
                // If we are running on a cellular test network we might not
                // get position but we should always get time
                U_PORT_TEST_ASSERT(gNetworkHandle == networkHandle);
                if ((gLocation.radiusMillimetres > 0) &&
                    (gLocation.radiusMillimetres <= U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES)) {
                    uLocationTestPrintLocation(&gLocation);
                    U_PORT_TEST_ASSERT(gLocation.latitudeX1e7 > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.longitudeX1e7 > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.altitudeMillimetres > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.radiusMillimetres > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.svs > INT_MIN);
                } else {
                    uPortLog("U_LOCATION_TEST: only able to get time (%d).\n",
                             (int32_t) gLocation.timeUtc);
                }
                U_PORT_TEST_ASSERT(gLocation.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
            }
            if ((gErrorCode != 0) && (x >= 1)) {
                uPortLog("U_LOCATION_TEST: failed to get an answer, will retry in 30 seconds...\n");
                uPortTaskBlock(30000);
            }
        }
        U_PORT_TEST_ASSERT(gErrorCode == 0);
    } else {
        if (!U_NETWORK_TEST_TYPE_HAS_LOCATION(networkType)) {
            gNetworkHandle = -1;
            gErrorCode = INT_MIN;
            uLocationTestResetLocation(&gLocation);
            U_PORT_TEST_ASSERT(uLocationGetStart(networkHandle, locationType,
                                                 pLocationAssist, pAuthenticationTokenStr,
                                                 locationCallback) < 0);
            U_PORT_TEST_ASSERT(gNetworkHandle == -1);
            U_PORT_TEST_ASSERT(gErrorCode == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.latitudeX1e7 == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.longitudeX1e7 == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.altitudeMillimetres == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.radiusMillimetres == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.timeUtc == LONG_MIN);
            U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond == INT_MIN);
            U_PORT_TEST_ASSERT(gLocation.svs == INT_MIN);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test the location API.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[location]", "locationBasic")
{
    int32_t networkHandle;
    int32_t locationType;
    const uLocationTestCfgList_t *pLocationCfgList;
    int32_t heapUsed;
    int32_t heapLossFirstCall[U_LOCATION_TYPE_MAX_NUM];
    int32_t heapLoss = 0;

    for (size_t x = 0; x < sizeof(heapLossFirstCall) / sizeof(heapLossFirstCall[0]); x++) {
        heapLossFirstCall[x] = INT_MIN;
    }

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Get the initialish heap
    heapUsed = uPortGetHeapFree();

    // Repeat for all network types
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            uPortLog("U_LOCATION_TEST: testing %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);

            // Do this for all location types
            for (locationType = (int32_t) U_LOCATION_TYPE_GNSS;
                 locationType < (int32_t) U_LOCATION_TYPE_MAX_NUM;
                 locationType++) {

                // Check the location types supported by this network type
                uPortLog("U_LOCATION_TEST: testing location type %s.\n",
                         gpULocationTestTypeStr[locationType]);
                pLocationCfgList = gpULocationTestCfg[gUNetworkTestCfg[x].type];
                gpLocationCfg = NULL;
                for (size_t y = 0;
                     (y < pLocationCfgList->numEntries) && (gpLocationCfg == NULL);
                     y++) {
                    if (locationType == (int32_t) (pLocationCfgList + y)->pCfgData->locationType) {
                        // The location type is supported, make a copy
                        // of the test configuration for it into something
                        // writeable
                        gpLocationCfg = pULocationTestCfgDeepCopyMalloc((pLocationCfgList + y)->pCfgData);
                    }
                }

                // The first time a given location type is called it may allocate
                // memory (e.g for mutexes) which are only released at deinitialisation
                // of the location API.  Track that this so as to take account of it in
                // the heap check calculation.
                if ((heapLossFirstCall[locationType] == INT_MIN) && (gpLocationCfg != NULL)) {
                    heapLoss = uPortGetHeapFree();
                }

                if (gpLocationCfg != NULL) {
                    if (gpLocationCfg->pLocationAssist->pClientIdStr != NULL) {
                        // If we have a Client ID then we will need to log into the
                        // MQTT broker first
                        gpLocationCfg->pLocationAssist->pMqttClientContext = pULocationTestMqttLogin(networkHandle,
                                                                                                     gpLocationCfg->pServerUrlStr,
                                                                                                     gpLocationCfg->pUserNameStr,
                                                                                                     gpLocationCfg->pPasswordStr,
                                                                                                     gpLocationCfg->pLocationAssist->pClientIdStr);
                        if (locationType == (int32_t) U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE) {
                            // Cloud Locate requires the GNSS network handle
                            // as its assistance network
                            gpLocationCfg->pLocationAssist->networkHandleAssist = getGnssNetworkHandle();
                        }
                    }
                } else {
                    uPortLog("U_LOCATION_TEST: %s is not supported on a %s network.\n",
                             gpULocationTestTypeStr[locationType],
                             gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                }

                // Test the blocking location API (supported and non-supported cases)
                testBlocking(networkHandle,
                             gUNetworkTestCfg[x].type,
                             (uLocationType_t) locationType, gpLocationCfg);

                // Test the non-blocking location API (supported and non-supported cases)
                testNonBlocking(networkHandle,
                                gUNetworkTestCfg[x].type,
                                (uLocationType_t) locationType, gpLocationCfg);

                if (gpLocationCfg != NULL) {
                    if (gpLocationCfg->pLocationAssist->pMqttClientContext != NULL) {
                        // Log out of the MQTT broker again
                        uLocationTestMqttLogout(gpLocationCfg->pLocationAssist->pMqttClientContext);
                        gpLocationCfg->pLocationAssist->pMqttClientContext = NULL;
                    }
                    // Free the memory from the location configuration copy
                    uLocationTestCfgDeepCopyFree(gpLocationCfg);
                    // Note: not NULLifying gpLocationCfg as we use it as a flag below
                }

                // Account for first-call heap usage
                if ((heapLossFirstCall[locationType] == INT_MIN) && (gpLocationCfg != NULL)) {
                    heapLossFirstCall[locationType] = heapLoss - uPortGetHeapFree();
                }
            }
        }
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    heapLoss = 0;
    for (size_t x = 0; x < sizeof(heapLossFirstCall) / sizeof(heapLossFirstCall[0]); x++) {
        if (heapLossFirstCall[x] > INT_MIN) {
            heapLoss += heapLossFirstCall[x];
        }
    }
    uPortLog("U_LOCATION_TEST: we have leaked %d byte(s) and lost %d byte(s)"
             " to initialisation.\n", heapUsed - heapLoss, heapLoss);
    // heapUsed <= heapLoss for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= heapLoss);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[location]", "locationCleanUp")
{
    int32_t x;

    if (gpLocationCfg != NULL) {
        // Free the memory from the location configuration copy
        uLocationTestCfgDeepCopyFree(gpLocationCfg);
        gpLocationCfg = NULL;
    }

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    for (size_t y = 0; y < gUNetworkTestCfgSize; y++) {
        gUNetworkTestCfg[y].handle = -1;
    }
    uNetworkDeinit();

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_LOCATION_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_LOCATION_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
