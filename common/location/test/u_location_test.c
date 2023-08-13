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
 * @brief Test for the location API: these should pass on all platforms
 * that have a u-blox module connected to them.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifndef U_LOCATION_TEST_DISABLE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // LONG_MIN, INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // memset(), strlen(), strncmp()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_i2c.h"
#include "u_port_spi.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_location.h"
#include "u_location_test_shared_cfg.h"

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
// Needed only to effect a reset of a short-range module
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#endif

// These are needed to test HTTP/LOC simultaneity on Wi-Fi
#include "u_security.h"
#include "u_security_tls.h"
#include "u_http_client.h"
#include "u_http_client_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_LOCATION_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_LOCATION_TEST_CONTINUOUS_STOP_TIMEOUT_SECONDS
/** How long to wait after stopping a continuous location test for
 * any "in-transit" request to resolve itself.
 */
# define U_LOCATION_TEST_CONTINUOUS_STOP_TIMEOUT_SECONDS 10
#endif

#ifndef U_LOCATION_TEST_HTTP_TIMEOUT_SECONDS
/** How long to wait for the HTTP response in the case where
 * we are running one in parallel with location (the WiFi case).
 */
# define U_LOCATION_TEST_HTTP_TIMEOUT_SECONDS 5
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs = 0;

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

/** Keep track of the number of times the callback is called
 * for the continuous case.
 */
static volatile int32_t gCount;

/** Place to put the HTTP context used when testing the
 * simultaneity of HTTP and location for Wi-Fi.
 */
static uHttpClientContext_t *gpHttpContext = NULL;

/** A string to POST over HTTP.
 */
static const char gHttpString[] = "HTTP and LOC work at the same time";

/** A buffer to get the POST'ed string back in: must be
 * big enough to store gHttpString.
 */
static char gHttpBufferIn[40];

/** A place to store the amount of stuff put into gHttpBufferIn
 * by HTTP.
 */
static size_t gSizeHttpBufferIn = 0;

/** A place to store the content type of whatever HTTP GETs.
 */
static char gHttpContentTypeBuffer[U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES];

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
    bool shoResetDone = false;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    // Don't check these for success as not all platforms support I2C or SPI
    uPortI2cInit();
    uPortSpiInit();
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get all of the networks
    pList = pUNetworkTestListAlloc(NULL);
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
#if defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE) && defined(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS) && \
    (U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS >= 0)
        if (!shoResetDone &&
            ((pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_SHORT_RANGE) ||
             (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU))) {
            // We test HTTP and Location simultaneously on Wifi but if,
            // for some reasone, the module has an HTTP connection left
            // open it won't be be able to do both (since it has a
            // maximum of two at once), hence reset the module if we
            // can to avoid that.
            uShortRangeResetToDefaultSettings(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS);
            shoResetDone = true;
        }
#else
        // Avoid compiler warnings
        (void) shoResetDone;
#endif
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

// HTTP callback, only used for checking of Wi-Fi location/HTTP simultaneity.
static void httpCallback(uDeviceHandle_t devHandle,
                         int32_t statusCodeOrError,
                         size_t responseSize,
                         void *pResponseCallbackParam)
{
    int32_t *pStatusCode = (int32_t *) pResponseCallbackParam;

    (void) devHandle;
    (void) responseSize;

    if (pStatusCode != NULL) {
        *pStatusCode = statusCodeOrError;
    }
}

// Do an [asynchronous] HTTP POST request.
static int32_t httpPostRequest(uLocationType_t locationType,
                               uHttpClientContext_t *pHttpContext,
                               const char *pSerialNumber)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    char pathBuffer[32];

    if ((pHttpContext != NULL) &&
        ((locationType == U_LOCATION_TYPE_CLOUD_GOOGLE) ||
         (locationType == U_LOCATION_TYPE_CLOUD_SKYHOOK) ||
         (locationType == U_LOCATION_TYPE_CLOUD_HERE))) {
        gSizeHttpBufferIn = sizeof(gHttpBufferIn);
        memset(gHttpBufferIn, 0, sizeof(gHttpBufferIn));
        memset(gHttpContentTypeBuffer, 0, sizeof(gHttpContentTypeBuffer));
        // Create a path
        snprintf(pathBuffer, sizeof(pathBuffer), "/%.16s_%d.html", pSerialNumber, locationType);
        errorCode = uHttpClientPostRequest(pHttpContext, pathBuffer,
                                           gHttpString, strlen(gHttpString),
                                           "application/text", gHttpBufferIn,
                                           &gSizeHttpBufferIn,
                                           gHttpContentTypeBuffer);
    }

    return errorCode;
}

// Check the outcome of an HTTP POST request.
static bool httpPostCheck(uLocationType_t locationType,
                          uHttpClientContext_t *pHttpContext,
                          volatile int32_t *pHttpStatusCode)
{
    bool success = true;
    int32_t startTimeMs;

    if ((pHttpContext != NULL) &&
        ((locationType == U_LOCATION_TYPE_CLOUD_GOOGLE) ||
         (locationType == U_LOCATION_TYPE_CLOUD_SKYHOOK) ||
         (locationType == U_LOCATION_TYPE_CLOUD_HERE))) {
        success = false;
        startTimeMs = uPortGetTickTimeMs();
        while ((*pHttpStatusCode != 200) &&
               (uPortGetTickTimeMs() - startTimeMs < U_LOCATION_TEST_HTTP_TIMEOUT_SECONDS * 1000)) {
            uPortTaskBlock(100);
        }
        if (*pHttpStatusCode != 200) {
            U_TEST_PRINT_LINE("HTTP POST request returned status code %d (expected 200).",
                              *pHttpStatusCode);
        } else if (gSizeHttpBufferIn != strlen(gHttpString)) {
            U_TEST_PRINT_LINE("expected HTTP response of length %d but got length %d.",
                              strlen(gHttpString), gSizeHttpBufferIn);
        } else if (strncmp(gHttpBufferIn, gHttpString, sizeof(gHttpString) - 1) != 0) {
            U_TEST_PRINT_LINE("expected HTTP response \"%s\" but got \"%s\".",
                              gHttpString, gHttpBufferIn);
        } else if (strlen(gHttpContentTypeBuffer) == 0) {
            // Can't really tell what the content type string will turn
            // out to be so just check for non-empty
            U_TEST_PRINT_LINE("HTTP response content type was empty.");
        } else {
            success = true;
        }
        // Reset for next time
        *pHttpStatusCode = 0;
    }

    return success;
}

// Test the blocking location API.
static void testBlocking(uDeviceHandle_t devHandle,
                         uNetworkType_t networkType,
                         uLocationType_t locationType,
                         const uLocationTestCfg_t *pLocationCfg)
{
    uLocation_t location;
    int32_t startTimeMs = 0;
    int32_t timeoutMs = U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    int32_t y;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    if (networkType == U_NETWORK_TYPE_WIFI) {
        timeoutMs = U_LOCATION_TEST_CFG_WIFI_TIMEOUT_SECONDS * 1000;
    }

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
    uLocationTestResetLocation(&location);
    if (pLocationCfg != NULL) {
        U_TEST_PRINT_LINE("blocking API.");
        // Try this a few times as obtaining position using Here over
        // WiFi can sometimes fail
        y = -1;
        for (int32_t x = 0; (x < 3) && (y != 0); x++) {
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs + timeoutMs;
            y = uLocationGet(devHandle, locationType,
                             pLocationAssist,
                             pAuthenticationTokenStr,
                             &location,
                             keepGoingCallback);
        }
        // The location type is supported (a GNSS network always
        // supports location, irrespective of the location type) so it
        // should work
        U_TEST_PRINT_LINE("uLocationGet() returned %d.", y);
        U_PORT_TEST_ASSERT(y >= 0);
        if (networkType != U_NETWORK_TYPE_WIFI) {
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            if (y != 0) {
                // The cloud services used for Wifi-based location can sometimes
                // be unable to determine position, which they indicate through
                // a positive, non-200, HTTP status code
                U_TEST_PRINT_LINE("*** WARNING *** cloud service was unable to determine"
                                  " position (HTTP status code %d).", y);
            }
        }
        U_TEST_PRINT_LINE("location establishment took %d second(s).",
                          (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
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
                U_PORT_TEST_ASSERT(location.svs >= 0);
            } else {
                U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond == INT_MIN);
                U_PORT_TEST_ASSERT(location.svs == -1);
            }
        }
        if (networkType != U_NETWORK_TYPE_WIFI) {
            // Only Wifi doesn't return the time
            U_TEST_PRINT_LINE("able to get time (%d).", (int32_t) location.timeUtc);
            U_PORT_TEST_ASSERT(location.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
        } else {
            U_PORT_TEST_ASSERT(location.timeUtc == -1);
        }
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

// Callback function for the non-blocking APIs.
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
    if (errorCode == 0) {
        gCount++;
    }
}

// Test the one-shot location API.
static void testOneShot(uDeviceHandle_t devHandle,
                        uNetworkType_t networkType,
                        uLocationType_t locationType,
                        const uLocationTestCfg_t *pLocationCfg)
{
    int32_t startTimeMs;
    int32_t y;
    int32_t timeoutMs = U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    if (networkType == U_NETWORK_TYPE_WIFI) {
        timeoutMs = U_LOCATION_TEST_CFG_WIFI_TIMEOUT_SECONDS * 1000;
    }

    if (pLocationCfg != NULL) {
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
        pLocationAssist = pLocationCfg->pLocationAssist;
    }
    startTimeMs = uPortGetTickTimeMs();

    uLocationTestResetLocation(&gLocation);
    if (pLocationCfg != NULL) {
        gDevHandle = NULL;
        gErrorCode = INT_MIN;
        // Try this a few times as the Cell Locate AT command can sometimes
        // (e.g. on SARA-R412M-02B) return "generic error" if asked to establish
        // location again quickly after returning an answer
        U_TEST_PRINT_LINE("one-shot API.");
        for (int32_t x = 3; (x > 0) && (gErrorCode != 0); x--) {
            uLocationTestResetLocation(&gLocation);
            y = uLocationGetStart(devHandle, locationType,
                                  pLocationAssist,
                                  pAuthenticationTokenStr,
                                  locationCallback);
            U_TEST_PRINT_LINE("uLocationGetStart() returned %d.", y);
            if (y == 0) {
                U_TEST_PRINT_LINE("waiting up to %d second(s) for results from"
                                  " one-shot API...",
                                  timeoutMs);
                while ((gErrorCode == INT_MIN) &&
                       (uPortGetTickTimeMs() - startTimeMs < timeoutMs)) {
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
                                      (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
                    // If we are running on a cellular test network we might not
                    // get position but we should always get time
                    U_PORT_TEST_ASSERT(gDevHandle == devHandle);
                    if ((gLocation.radiusMillimetres > 0) &&
                        (gLocation.radiusMillimetres <= U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES)) {
                        uLocationTestPrintLocation(&gLocation);
                        U_PORT_TEST_ASSERT(gLocation.latitudeX1e7 > INT_MIN);
                        U_PORT_TEST_ASSERT(gLocation.longitudeX1e7 > INT_MIN);
                        // Don't check altitude as we might only have a 2D fix
                        U_PORT_TEST_ASSERT(gLocation.radiusMillimetres > INT_MIN);
                        if (locationType == U_LOCATION_TYPE_GNSS) {
                            // Only get these for GNSS
                            U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond > INT_MIN);
                            U_PORT_TEST_ASSERT(gLocation.svs >= 0);
                        } else {
                            U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond == INT_MIN);
                            U_PORT_TEST_ASSERT(gLocation.svs == -1);
                        }
                    }
                    if (networkType != U_NETWORK_TYPE_WIFI) {
                        // Only Wifi doesn't return the time
                        U_TEST_PRINT_LINE("able to get time (%d).", (int32_t) gLocation.timeUtc);
                        U_PORT_TEST_ASSERT(gLocation.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
                    } else {
                        U_PORT_TEST_ASSERT(gLocation.timeUtc == -1);
                    }
                }
                if ((gErrorCode != 0) && (x >= 1)) {
                    U_TEST_PRINT_LINE("failed to get an answer, will retry in 30 seconds...");
                    uPortTaskBlock(30000);
                    gErrorCode = 0;
                }
                uLocationGetStop(devHandle);
            } else {
                if (locationType == U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE) {
                    // Cloud locate is not currently supported for async position
                    U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
                    gErrorCode = 0;
                } else {
                    U_PORT_TEST_ASSERT(false);
                }
            }
        }
        U_PORT_TEST_ASSERT(gErrorCode >= 0);
        if (networkType != U_NETWORK_TYPE_WIFI) {
            U_PORT_TEST_ASSERT(gErrorCode == 0);
        } else {
            if (gErrorCode != 0) {
                // The cloud services used for Wifi-based location can sometimes
                // be unable to determine position, which they indicate through
                // a positive, non-200, HTTP status code
                U_TEST_PRINT_LINE("*** WARNING *** cloud service was unable to determine"
                                  " position (HTTP status code %d).", gErrorCode);
            }
        }
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

// Test the continuous location API.
static void testContinuous(uDeviceHandle_t devHandle,
                           uNetworkType_t networkType,
                           uLocationType_t locationType,
                           const uLocationTestCfg_t *pLocationCfg)
{
    int32_t startTimeMs;
    int32_t timeoutMs = U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
    int32_t y;
    const uLocationAssist_t *pLocationAssist = NULL;
    const char *pAuthenticationTokenStr = NULL;

    if (networkType == U_NETWORK_TYPE_WIFI) {
        timeoutMs = U_LOCATION_TEST_CFG_WIFI_TIMEOUT_SECONDS * 1000;
    }

    if (pLocationCfg != NULL) {
        pAuthenticationTokenStr = pLocationCfg->pAuthenticationTokenStr;
        pLocationAssist = pLocationCfg->pLocationAssist;
    }
    startTimeMs = uPortGetTickTimeMs();

    uLocationTestResetLocation(&gLocation);
    if (pLocationCfg != NULL) {
        U_TEST_PRINT_LINE("continuous API.");
        gDevHandle = NULL;
        gErrorCode = INT_MIN;
        gCount = 0;
        uLocationTestResetLocation(&gLocation);
        y =  uLocationGetContinuousStart(devHandle,
                                         U_LOCATION_TEST_CFG_CONTINUOUS_RATE_MS,
                                         locationType,
                                         pLocationAssist,
                                         pAuthenticationTokenStr,
                                         locationCallback);
        U_TEST_PRINT_LINE("uLocationGetContinuousStart() returned %d.", y);
        if (y == 0) {
            U_TEST_PRINT_LINE("waiting up to %d second(s) to get at least %d"
                              " results from continuous API...",
                              timeoutMs * U_LOCATION_TEST_CFG_CONTINUOUS_COUNT,
                              U_LOCATION_TEST_CFG_CONTINUOUS_COUNT);
            while ((gCount < U_LOCATION_TEST_CFG_CONTINUOUS_COUNT) &&
                   (uPortGetTickTimeMs() - startTimeMs < timeoutMs)) {
                // Location establishment status is only supported for cell locate
                y = uLocationGetStatus(devHandle);
                if (locationType == U_LOCATION_TYPE_CLOUD_CELL_LOCATE) {
                    U_PORT_TEST_ASSERT(y >= 0);
                } else {
                    U_PORT_TEST_ASSERT(y <= (int32_t) U_LOCATION_STATUS_UNKNOWN);
                }
                uPortTaskBlock(1000);
            }

            // There has been something off going on with this test on Windows,
            // so print a little extra diagnostic information here so that we
            // can watch it
            U_TEST_PRINT_LINE("startTimeMs is %d, ticks now is %d, gCount is %d.",
                              startTimeMs, uPortGetTickTimeMs(), gCount);
            if (gCount >= U_LOCATION_TEST_CFG_CONTINUOUS_COUNT) {
                U_TEST_PRINT_LINE("took %d second(s) to get location %d time(s).",
                                  (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000,
                                  gCount);
                // If we are running on a cellular test network we might not
                // get position but we should always get time
                U_PORT_TEST_ASSERT(gDevHandle == devHandle);
                if ((gLocation.radiusMillimetres > 0) &&
                    (gLocation.radiusMillimetres <= U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES)) {
                    uLocationTestPrintLocation(&gLocation);
                    U_PORT_TEST_ASSERT(gLocation.latitudeX1e7 > INT_MIN);
                    U_PORT_TEST_ASSERT(gLocation.longitudeX1e7 > INT_MIN);
                    // Don't check altitude as we might only have a 2D fix
                    U_PORT_TEST_ASSERT(gLocation.radiusMillimetres > INT_MIN);
                    if (locationType == U_LOCATION_TYPE_GNSS) {
                        // Only get these for GNSS
                        U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond > INT_MIN);
                        U_PORT_TEST_ASSERT(gLocation.svs >= 0);
                    } else {
                        U_PORT_TEST_ASSERT(gLocation.speedMillimetresPerSecond == INT_MIN);
                        U_PORT_TEST_ASSERT(gLocation.svs == -1);
                    }
                }
                if (networkType != U_NETWORK_TYPE_WIFI) {
                    // Only Wifi doesn't return the time
                    U_TEST_PRINT_LINE("able to get time (%d).", (int32_t) gLocation.timeUtc);
                    U_PORT_TEST_ASSERT(gLocation.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
                } else {
                    U_PORT_TEST_ASSERT(gLocation.timeUtc == -1);
                }
            }
            uLocationGetStop(devHandle);
            U_PORT_TEST_ASSERT(gErrorCode >= 0);
            if (networkType != U_NETWORK_TYPE_WIFI) {
                U_PORT_TEST_ASSERT(gErrorCode == 0);
            } else {
                if (gErrorCode != 0) {
                    // The cloud services used for Wifi-based location can sometimes
                    // be unable to determine position, which they indicate through
                    // a positive, non-200, HTTP status code
                    U_TEST_PRINT_LINE("*** WARNING *** cloud service was unable to determine"
                                      " position (HTTP status code %d).", gErrorCode);
                }
            }
            // When a continuous location is stopped we only generally stop listening
            // for results, any request "in transit" may still be completed on the
            // module side, so we need to wait here to give it a chance to turn up
            // and be discarded otherwise there may be confusion
            uPortTaskBlock(U_LOCATION_TEST_CONTINUOUS_STOP_TIMEOUT_SECONDS * 1000);
        } else {
            if (locationType == U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE) {
                // Cloud locate is not currently supported for async position
                U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
            } else {
#ifdef U_NETWORK_GNSS_CFG_CELL_USE_AT_ONLY
                // Continuous GNSS location is only supported were we have a streaming
                // transport, so if U_NETWORK_GNSS_CFG_CELL_USE_AT_ONLY is defined then
                // uLocationGetContinuousStart() is allowed to return "not supported"
                U_PORT_TEST_ASSERT(y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
#else
                U_TEST_PRINT_LINE("uLocationGetContinuousStart() returned %d, expecting 0.", y);
                U_PORT_TEST_ASSERT(false);
#endif
            }
        }
    } else {
        if (!U_NETWORK_TEST_TYPE_HAS_LOCATION(networkType)) {
            gDevHandle = NULL;
            gErrorCode = INT_MIN;
            gCount = 0;
            uLocationTestResetLocation(&gLocation);
            U_PORT_TEST_ASSERT(uLocationGetContinuousStart(devHandle,
                                                           U_LOCATION_TEST_CFG_CONTINUOUS_RATE_MS,
                                                           locationType,
                                                           pLocationAssist, pAuthenticationTokenStr,
                                                           locationCallback) < 0);
            U_PORT_TEST_ASSERT(gDevHandle == NULL);
            U_PORT_TEST_ASSERT(gErrorCode == INT_MIN);
            U_PORT_TEST_ASSERT(gCount == 0);
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
    uDeviceType_t devType;
    int32_t locationType;
    const uLocationTestCfgList_t *pLocationCfgList;
    uHttpClientConnection_t connection = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    char urlBuffer[64];
    char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    volatile int32_t httpStatusCode = 0;
    int32_t heapUsed;
    int32_t heapHttpInitLoss = 0;
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
        devType = pTmp->pDeviceCfg->deviceType;
        U_TEST_PRINT_LINE("testing %s network...",
                          gpUNetworkTestTypeName[pTmp->networkType]);

        // Do this for all location types
        for (locationType = (int32_t) U_LOCATION_TYPE_GNSS;
             locationType < (int32_t) U_LOCATION_TYPE_MAX_NUM;
             locationType++) {

            // Check the location types supported by this network type
            U_TEST_PRINT_LINE("testing location type %s on %s.",
                              gpULocationTestTypeStr[locationType],
                              gpUNetworkTestDeviceTypeName[devType]);
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
                if ((pTmp->networkType == U_NETWORK_TYPE_WIFI) && (gpHttpContext == NULL)) {
                    // For Wifi, since the same URC form is used to return
                    // HTTP responses and location, we test that both succeed
                    U_PORT_TEST_ASSERT(uSecurityGetSerialNumber(devHandle, serialNumber) > 0);
                    // Create a complete URL from the domain name and port number
                    snprintf(urlBuffer, sizeof(urlBuffer), "%s:%d",
                             U_HTTP_CLIENT_TEST_SERVER_DOMAIN_NAME,
                             U_HTTP_CLIENT_TEST_SERVER_PORT);
                    // Configure the connection
                    connection.pServerName = urlBuffer;
                    connection.pResponseCallback = httpCallback;
                    connection.pResponseCallbackParam = (void *) &httpStatusCode;
                    // Opening an HTTP client for the first time creates a mutex
                    // that is not destroyed for thread-safety reasons; take
                    // account of that here
                    heapHttpInitLoss += uPortGetHeapFree();
                    gpHttpContext = pUHttpClientOpen(devHandle, &connection, NULL);
                    U_PORT_TEST_ASSERT(gpHttpContext != NULL);
                    heapHttpInitLoss -= uPortGetHeapFree();
                }
            } else {
                U_TEST_PRINT_LINE("%s is not supported on a %s network.",
                                  gpULocationTestTypeStr[locationType],
                                  gpUNetworkTestTypeName[pTmp->networkType]);
            }

            U_PORT_TEST_ASSERT(httpPostRequest((uLocationType_t) locationType,
                                               gpHttpContext, serialNumber) == 0);
            // Test the blocking location API (supported and non-supported cases)
            testBlocking(devHandle, pTmp->networkType,
                         (uLocationType_t) locationType, gpLocationCfg);
            U_PORT_TEST_ASSERT(httpPostCheck((uLocationType_t) locationType, gpHttpContext, &httpStatusCode));

            U_PORT_TEST_ASSERT(httpPostRequest((uLocationType_t) locationType,
                                               gpHttpContext, serialNumber) == 0);
            // Test the one-shot location API (supported and non-supported cases)
            testOneShot(devHandle, pTmp->networkType,
                        (uLocationType_t) locationType, gpLocationCfg);
            U_PORT_TEST_ASSERT(httpPostCheck((uLocationType_t) locationType, gpHttpContext, &httpStatusCode));

            U_PORT_TEST_ASSERT(httpPostRequest((uLocationType_t) locationType,
                                               gpHttpContext, serialNumber) == 0);
            // Test the continuous location API (supported and non-supported cases)
            testContinuous(devHandle, pTmp->networkType,
                           (uLocationType_t) locationType, gpLocationCfg);
            U_PORT_TEST_ASSERT(httpPostCheck((uLocationType_t) locationType, gpHttpContext, &httpStatusCode));

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

    if (gpHttpContext != NULL) {
        uHttpClientClose(gpHttpContext);
        gpHttpContext = NULL;
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
                      " to initialisation.", heapUsed - (heapLoss + heapHttpInitLoss),
                      heapLoss + heapHttpInitLoss);
    // heapUsed <= heapLoss for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= heapLoss + heapHttpInitLoss);

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

    if (gpHttpContext != NULL) {
        uHttpClientClose(gpHttpContext);
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
    uPortSpiDeinit();
    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifndef U_LOCATION_TEST_DISABLE

// End of file
