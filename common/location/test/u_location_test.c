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
#include "u_port_i2c.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_location.h"
#include "u_location_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_LOCATION_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

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
//lint -esym(844, gDevHandle)
static uDeviceHandle_t gDevHandle = NULL;

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
static bool keepGoingCallback(uDeviceHandle_t devHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT((gDevHandle == NULL) || (devHandle == gDevHandle));
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Standard preamble for the location test.
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    // Don't check this for success as not all platforms support I2C
    uPortI2cInit();
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get all of the networks
    pList = pUNetworkTestListAlloc(NULL);
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Bring up each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(*pTmp->pDevHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);
    }

    return pList;
}

// Test the blocking location API.
static void testBlocking(uDeviceHandle_t devHandle,
                         uNetworkType_t networkType,
                         uLocationType_t locationType,
                         const uLocationTestCfg_t *pLocationCfg)
{
    uLocation_t location;
    int64_t startTime;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    gDevHandle = devHandle;
    if (pLocationCfg != NULL) {
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
        pLocationAssist = pLocationCfg->pLocationAssist;
        if ((pLocationAssist != NULL) &&
            (locationType == U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE)) {
            // If we're doing cloud locate then we can't
            // check the network handle in the callback
            gDevHandle = NULL;
        }
    }
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    uLocationTestResetLocation(&location);
    if (pLocationCfg != NULL) {
        U_TEST_PRINT_LINE("blocking API.");
        // The location type is supported (a GNSS network always
        // supports location, irrespective of the location type) so it
        // should work
        U_PORT_TEST_ASSERT(uLocationGet(devHandle, locationType,
                                        pLocationAssist,
                                        pAuthenticationTokenStr,
                                        &location,
                                        keepGoingCallback) == 0);
        U_TEST_PRINT_LINE("location establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
        // If we are running on a test cellular network we won't get position but
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
            U_TEST_PRINT_LINE("only able to get time (%d).", (int32_t) location.timeUtc);
        }
        U_PORT_TEST_ASSERT(location.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
    } else {
        if (!U_NETWORK_TEST_TYPE_HAS_LOCATION(networkType)) {
            U_PORT_TEST_ASSERT(uLocationGet(devHandle, locationType,
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
static void locationCallback(uDeviceHandle_t devHandle,
                             int32_t errorCode,
                             const uLocation_t *pLocation)
{
    gDevHandle = devHandle,
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
static void testNonBlocking(uDeviceHandle_t devHandle,
                            uNetworkType_t networkType,
                            uLocationType_t locationType,
                            const uLocationTestCfg_t *pLocationCfg)
{
    int64_t startTime;
    int32_t y;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    if (pLocationCfg != NULL) {
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
        pLocationAssist = pLocationCfg->pLocationAssist;
    }
    startTime = uPortGetTickTimeMs();
    gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;

    uLocationTestResetLocation(&gLocation);
    if (pLocationCfg != NULL) {
        // Try this a few times as the Cell Locate AT command can sometimes
        // (e.g. on SARA-R412M-02B) return "generic error" if asked to establish
        // location again quickly after returning an answer
        for (int32_t x = 3; (x > 0) && (gErrorCode != 0); x--) {
            U_TEST_PRINT_LINE("non-blocking API.");
            gDevHandle = NULL;
            gErrorCode = INT_MIN;
            uLocationTestResetLocation(&gLocation);
            U_PORT_TEST_ASSERT(uLocationGetStart(devHandle, locationType,
                                                 pLocationAssist,
                                                 pAuthenticationTokenStr,
                                                 locationCallback) == 0);
            U_TEST_PRINT_LINE("waiting up to %d second(s) for results from"
                              " non-blocking API...",
                              U_LOCATION_TEST_CFG_TIMEOUT_SECONDS);
            while ((gErrorCode == INT_MIN) && (uPortGetTickTimeMs() < gStopTimeMs)) {
                // Location establishment status is only supported for cell locate
                y = uLocationGetStatus(devHandle);
                if (locationType == U_LOCATION_TYPE_CLOUD_CELL_LOCATE) {
                    U_PORT_TEST_ASSERT(y >= 0);
                } else {
                    U_PORT_TEST_ASSERT(y <= (int32_t) U_LOCATION_STATUS_UNKNOWN);
                }
                uPortTaskBlock(1000);
            }

            if (gErrorCode == 0) {
                U_TEST_PRINT_LINE("location establishment took %d second(s).",
                                  (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
                // If we are running on a cellular test network we might not
                // get position but we should always get time
                U_PORT_TEST_ASSERT(gDevHandle == devHandle);
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
                    U_TEST_PRINT_LINE("only able to get time (%d).", (int32_t) gLocation.timeUtc);
                }
                U_PORT_TEST_ASSERT(gLocation.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
            }
            if ((gErrorCode != 0) && (x >= 1)) {
                U_TEST_PRINT_LINE("failed to get an answer, will retry in 30 seconds...");
                uPortTaskBlock(30000);
            }
        }
        U_PORT_TEST_ASSERT(gErrorCode == 0);
    } else {
        if (!U_NETWORK_TEST_TYPE_HAS_LOCATION(networkType)) {
            gDevHandle = NULL;
            gErrorCode = INT_MIN;
            uLocationTestResetLocation(&gLocation);
            U_PORT_TEST_ASSERT(uLocationGetStart(devHandle, locationType,
                                                 pLocationAssist, pAuthenticationTokenStr,
                                                 locationCallback) < 0);
            U_PORT_TEST_ASSERT(gDevHandle == NULL);
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
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t locationType;
    const uLocationTestCfgList_t *pLocationCfgList;
    int32_t heapUsed;
    int32_t heapLossFirstCall[U_LOCATION_TYPE_MAX_NUM];
    int32_t heapLoss = 0;

    for (size_t x = 0; x < sizeof(heapLossFirstCall) / sizeof(heapLossFirstCall[0]); x++) {
        heapLossFirstCall[x] = INT_MIN;
    }

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Do the standard preamble to make sure there is
    // a network underneath us
    pList = pStdPreamble();
    if (pList == NULL) {
        U_TEST_PRINT_LINE("*** WARNING *** nothing to do.");
    }

    // Get the initialish heap
    heapUsed = uPortGetHeapFree();

    // Repeat for all network types
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;
        U_TEST_PRINT_LINE("testing %s network...",
                          gpUNetworkTestTypeName[pTmp->networkType]);

        // Do this for all location types
        for (locationType = (int32_t) U_LOCATION_TYPE_GNSS;
             locationType < (int32_t) U_LOCATION_TYPE_MAX_NUM;
             locationType++) {

            // Check the location types supported by this network type
            U_TEST_PRINT_LINE("testing location type %s.",
                              gpULocationTestTypeStr[locationType]);
            pLocationCfgList = gpULocationTestCfg[pTmp->networkType];
            for (size_t y = 0;
                 (y < pLocationCfgList->numEntries) && (gpLocationCfg == NULL);
                 y++) {
                if (locationType == (int32_t) (pLocationCfgList->pCfgData[y]->locationType)) {
                    // The location type is supported, make a copy
                    // of the test configuration for it into something
                    // writeable
                    gpLocationCfg = pULocationTestCfgDeepCopyMalloc(pLocationCfgList->pCfgData[y]);
                }
            }

            if (gpLocationCfg != NULL) {
                // The first time a given location type is called it may allocate
                // memory (e.g for mutexes) which are only released at deinitialisation
                // of the location API.  Track this so as to take account of it in
                // the heap check calculation.
                if (heapLossFirstCall[locationType] == INT_MIN) {
                    heapLoss = uPortGetHeapFree();
                }
                if ((gpLocationCfg->pLocationAssist != NULL) &&
                    (gpLocationCfg->pLocationAssist->pClientIdStr != NULL)) {
                    // If we have a Client ID then we will need to log into the
                    // MQTT broker first
                    gpLocationCfg->pLocationAssist->pMqttClientContext = pULocationTestMqttLogin(devHandle,
                                                                                                 gpLocationCfg->pServerUrlStr,
                                                                                                 gpLocationCfg->pUserNameStr,
                                                                                                 gpLocationCfg->pPasswordStr,
                                                                                                 gpLocationCfg->pLocationAssist->pClientIdStr);
                }
            } else {
                U_TEST_PRINT_LINE("%s is not supported on a %s network.",
                                  gpULocationTestTypeStr[locationType],
                                  gpUNetworkTestTypeName[pTmp->networkType]);
            }

            // Test the blocking location API (supported and non-supported cases)
            testBlocking(devHandle, pTmp->networkType,
                         (uLocationType_t) locationType, gpLocationCfg);

            // Test the non-blocking location API (supported and non-supported cases)
            testNonBlocking(devHandle, pTmp->networkType,
                            (uLocationType_t) locationType, gpLocationCfg);

            if (gpLocationCfg != NULL) {
                if ((gpLocationCfg->pLocationAssist != NULL) &&
                    (gpLocationCfg->pLocationAssist->pMqttClientContext != NULL)) {
                    // Log out of the MQTT broker again
                    uLocationTestMqttLogout(gpLocationCfg->pLocationAssist->pMqttClientContext);
                    gpLocationCfg->pLocationAssist->pMqttClientContext = NULL;
                }
                // Account for first-call heap usage
                if (heapLossFirstCall[locationType] == INT_MIN) {
                    heapLossFirstCall[locationType] = heapLoss - uPortGetHeapFree();
                }
                // Free the memory from the location configuration copy
                uLocationTestCfgDeepCopyFree(gpLocationCfg);
                gpLocationCfg = NULL;
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
    U_TEST_PRINT_LINE("we have leaked %d byte(s) and lost %d byte(s)"
                      " to initialisation.", heapUsed - heapLoss, heapLoss);
    // heapUsed <= heapLoss for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= heapLoss);

    // Remove each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("taking down %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                 pTmp->networkType) == 0);
    }

    // Close the devices and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();
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
    uNetworkTestCleanUp();
    uDeviceDeinit();

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortI2cDeinit();
    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
