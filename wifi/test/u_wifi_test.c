/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicawifi law or agreed to in writing, software
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
 * @brief Tests for the wifi "general" API: these should pass on all
 * platforms where one UART is availawifi. No short range module is
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
#include "string.h"
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_wifi.h"

#include "u_wifi_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** UART handle for one AT client.
 */
static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

static const uint32_t gWifiStatusMaskAllUp = U_WIFI_STATUS_MASK_IPV4_UP |
                                             U_WIFI_STATUS_MASK_IPV6_UP;

static volatile int32_t gWifiConnected = 0;
static volatile int32_t gWifiDisconnected = 0;
static volatile uint32_t gWifiStatusMask = 0;
static volatile int32_t gLookForDisconnectReasonBitMask = 0;
static volatile int32_t gDisconnectReasonFound = 0;
static uWifiScanResult_t gScanResult;
static uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                        .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                        .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                        .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                        .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                        .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                      };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void wifiConnectionCallback(uDeviceHandle_t devHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    (void)devHandle;
    (void)pBssid;
    (void)pCallbackParameter;
    (void)channel;
    (void)connId;
    if (status == U_WIFI_CON_STATUS_CONNECTED) {
#if !U_CFG_OS_CLIB_LEAKS
        U_TEST_PRINT_LINE("connected Wifi connId: %d, bssid: %s, channel: %d.",
                          connId, pBssid, channel);
#endif
        gWifiConnected = 1;
    } else {
#if defined(U_CFG_ENABLE_LOGGING) && !U_CFG_OS_CLIB_LEAKS
        //lint -esym(752, strDisconnectReason)
        static const char strDisconnectReason[6][20] = {
            "Unknown", "Remote Close", "Out of range",
            "Roaming", "Security problems", "Network disabled"
        };
        if ((disconnectReason < 0) || (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            disconnectReason = 0;
        }
        U_TEST_PRINT_LINE("wifi connection lost connId: %d, reason: %d (%s).",
                          connId, disconnectReason,
                          strDisconnectReason[disconnectReason]);
#endif
        gWifiConnected = 0;
        gWifiDisconnected = 1;
        if (((1ul << disconnectReason) & gLookForDisconnectReasonBitMask) > 0) {
            gDisconnectReasonFound = 1;
        }
    }
}

static void wifiNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)devHandle;
    (void)interfaceType;
    (void)statusMask;
    (void)pCallbackParameter;
#if !U_CFG_OS_CLIB_LEAKS
    U_TEST_PRINT_LINE("network status IPv4 %s, IPv6 %s.",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");
#endif

    gWifiStatusMask = statusMask;
}


static uWifiTestError_t runWifiTest(const char *pSsid, const char *pPassPhrase)
{
    uWifiTestError_t testError = U_WIFI_TEST_ERROR_NONE;
    uWifiTestError_t connectError = U_WIFI_TEST_ERROR_NONE;
    uWifiTestError_t disconnectError = U_WIFI_TEST_ERROR_NONE;
    int32_t waitCtr = 0;
    gWifiStatusMask = 0;
    gWifiConnected = 0;
    gWifiDisconnected = 0;

    // Do the standard preamble
    if (0 != uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                      &uart,
                                      &gHandles)) {
        testError = U_WIFI_TEST_ERROR_PREAMBLE;
    }

    if (testError == U_WIFI_TEST_ERROR_NONE) {
        // Add unsolicited response cb for connection status
        uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                         wifiConnectionCallback, NULL);
        // Add unsolicited response cb for IP status
        uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                      wifiNetworkStatusCallback, NULL);
        // Connect to wifi network
        int32_t res = uWifiStationConnect(gHandles.devHandle,
                                          pSsid,
                                          U_WIFI_AUTH_WPA_PSK,
                                          pPassPhrase);
        if (res == 0) {
            //Wait for connection and IP events.
            //There could be multiple IP events depending on network configuration.
            while (!connectError && (!gWifiConnected || (gWifiStatusMask != gWifiStatusMaskAllUp))) {
                if (waitCtr >= 15) {
                    if (!gWifiConnected) {
                        U_TEST_PRINT_LINE("unable to connect to WiFi network.");
                        connectError = U_WIFI_TEST_ERROR_CONNECTED;
                    } else {
                        U_TEST_PRINT_LINE("unable to retrieve IP address.");
                        connectError = U_WIFI_TEST_ERROR_IPRECV;
                    }
                    break;
                }

                uPortTaskBlock(1000);
                waitCtr++;
            }
        } else {
            connectError = U_WIFI_TEST_ERROR_CONNECT;
        }
    }

    if (testError == U_WIFI_TEST_ERROR_NONE) {
        // Disconnect from wifi network (regardless of previous connectError)
        if (uWifiStationDisconnect(gHandles.devHandle) == 0) {
            waitCtr = 0;
            while (!disconnectError && (!gWifiDisconnected || (gWifiStatusMask > 0))) {
                if (waitCtr >= 5) {
                    disconnectError = U_WIFI_TEST_ERROR_DISCONNECT;
                    if (!gWifiDisconnected) {
                        U_TEST_PRINT_LINE("unable to diconnect from wifi network.");
                    } else {
                        U_TEST_PRINT_LINE("network status is still up.");
                    }
                    break;
                }
                uPortTaskBlock(1000);
                waitCtr++;
            }
        } else {
            disconnectError = U_WIFI_TEST_ERROR_DISCONNECT;
        }
    }

    // Aggregate result
    if (testError == U_WIFI_TEST_ERROR_NONE) {
        if (connectError != U_WIFI_TEST_ERROR_NONE) {
            testError = connectError;
        } else {
            testError = disconnectError;
        }
    }

    // Cleanup
    uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                     NULL, NULL);
    uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                  NULL, NULL);
    uWifiTestPrivatePostamble(&gHandles);
    return testError;
}

static void uWifiScanResultCallback(uDeviceHandle_t devHandle, uWifiScanResult_t *pResult)
{
    (void)devHandle;
    if (strcmp(pResult->ssid, U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID)) == 0) {
        gScanResult = *pResult;
    }
}

static bool validateScanResult(uWifiScanResult_t *pResult)
{
    if ((pResult->channel <= 0) || (pResult->channel > 185)) {
        U_TEST_PRINT_LINE("invalid WiFi channel: %d.", pResult->channel);
        return false;
    }
    if (pResult->rssi > 0) {
        U_TEST_PRINT_LINE("invalid RSSI value: %d.", pResult->rssi);
        return false;
    }
    if ((pResult->opMode != U_WIFI_OP_MODE_INFRASTRUCTURE) &&
        (pResult->opMode != U_WIFI_OP_MODE_ADHOC)) {
        U_TEST_PRINT_LINE("invalid opMode value: %d.", pResult->rssi);
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then de-initialise wifi.
 */
U_PORT_TEST_FUNCTION("[wifi]", "wifiInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeEdmStreamInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uWifiInit() == 0);
    uWifiDeinit();
    uAtClientDeinit();
    uShortRangeEdmStreamDeinit();
    uPortDeinit();
}

/** Add a wifi instance and remove it again.
 */
U_PORT_TEST_FUNCTION("[wifi]", "wifiOpenUart")
{
    int32_t heapUsed;
    uAtClientHandle_t atClient = NULL;
    uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                     .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                     .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                     .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                     .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                     .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                   };
    uPortDeinit();

    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &uart,
                                                &gHandles) == 0);
    U_PORT_TEST_ASSERT(uShortRangeGetUartHandle(gHandles.devHandle) == gHandles.uartHandle);
    U_PORT_TEST_ASSERT(uShortRangeGetEdmStreamHandle(gHandles.devHandle) == gHandles.edmStreamHandle);
    uShortRangeAtClientHandleGet(gHandles.devHandle, &atClient);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle == atClient);
    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.devHandle) == 0);

    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with same arg twice,"
                      " should fail...");
    uDeviceHandle_t dummyHandle;
    U_PORT_TEST_ASSERT(uShortRangeOpenUart((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                           &uart, true, &dummyHandle) < 0);

    uWifiTestPrivatePostamble(&gHandles);

    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with NULL uart arg,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                NULL,
                                                &gHandles) < 0);
    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with wrong module type,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_SHORT_RANGE_MODULE_TYPE_INTERNAL,
                                                &uart,
                                                &gHandles) < 0);
    uart.uartPort = -1;
    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with invalid uart arg,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &uart,
                                                &gHandles) < 0);

    uWifiTestPrivateCleanup(&gHandles);
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

U_PORT_TEST_FUNCTION("[wifi]", "wifiNetworkInitialisation")
{
    int32_t waitCtr = 0;
    int32_t errorCode = 0;
    gWifiStatusMask = 0;
    gWifiConnected = 0;
    gWifiDisconnected = 0;
    uWifiTestError_t testError = U_WIFI_TEST_ERROR_NONE;

    // Previous test may have left wifi connected
    // For this reason we start with making sure the wifi gets disconnected here

    // Do the standard preamble
    if (0 != uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                      &uart,
                                      &gHandles)) {
        testError = U_WIFI_TEST_ERROR_PREAMBLE;
    }

    if (!testError) {
        // Add unsolicited response cb for connection status
        uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                         wifiConnectionCallback, NULL);
        // Add unsolicited response cb for IP status
        uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                      wifiNetworkStatusCallback, NULL);
    }

    if (!testError) {
        errorCode = uWifiStationDisconnect(gHandles.devHandle);
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            waitCtr = 0;
            while (!testError && (!gWifiDisconnected || (gWifiStatusMask > 0))) {
                if (waitCtr >= 5) {
                    break;
                }
                uPortTaskBlock(1000);
                waitCtr++;
            }
        } else if (errorCode != U_WIFI_ERROR_ALREADY_DISCONNECTED) {
            testError = U_WIFI_TEST_ERROR_DISCONNECT;
        }
    }

    // Cleanup
    uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                     NULL, NULL);
    uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                  NULL, NULL);
    uWifiTestPrivatePostamble(&gHandles);

    U_PORT_TEST_ASSERT(testError == U_WIFI_TEST_ERROR_NONE);
}

U_PORT_TEST_FUNCTION("[wifi]", "wifiStationConnect")
{
    uWifiTestError_t testError = runWifiTest(U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                             U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE));
    // Handle errors
    U_PORT_TEST_ASSERT(testError == U_WIFI_TEST_ERROR_NONE);
}

U_PORT_TEST_FUNCTION("[wifi]", "wifiStationConnectWrongSSID")
{
    gLookForDisconnectReasonBitMask = (1 << U_WIFI_REASON_OUT_OF_RANGE); // (cant find SSID)
    gDisconnectReasonFound = 0;
    uWifiTestError_t testError = runWifiTest("DUMMYSSID",
                                             U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE));

    // Handle errors
    U_PORT_TEST_ASSERT(testError == U_WIFI_TEST_ERROR_CONNECTED);
    U_PORT_TEST_ASSERT(gDisconnectReasonFound);
}

U_PORT_TEST_FUNCTION("[wifi]", "wifiStationConnectWrongPassphrase")
{
    // The expected disconnect reason is U_WIFI_REASON_SECURITY_PROBLEM.
    // However, for some APs we will only get U_WIFI_REASON_UNKNOWN.
    gLookForDisconnectReasonBitMask = (1 << U_WIFI_REASON_UNKNOWN) |
                                      (1 << U_WIFI_REASON_SECURITY_PROBLEM);
    gDisconnectReasonFound = 0;
    uWifiTestError_t testError = runWifiTest(U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                             "WRONGPASSWD");
    // Handle errors
    U_PORT_TEST_ASSERT(testError == U_WIFI_TEST_ERROR_CONNECTED);
    U_PORT_TEST_ASSERT(gDisconnectReasonFound);
}

U_PORT_TEST_FUNCTION("[wifi]", "wifiScan")
{
    int32_t result;
    uWifiTestError_t testError = U_WIFI_TEST_ERROR_NONE;

    result = uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                      &uart,
                                      &gHandles);
    U_PORT_TEST_ASSERT(result == 0);

    //----------------------------------------------------------
    // Scan for all networks
    //----------------------------------------------------------

    // If an AP is found with SSID matching U_WIFI_TEST_CFG_SSID the scan result entry
    // will be stored in gScanResult.
    memset(&gScanResult, 0, sizeof(gScanResult));

    // There is a risk that the AP is not included in the scan results even if it's present
    // For this reason we use a retry loop
    for (int32_t i = 0; i < 3; i++) {
        result = uWifiStationScan(gHandles.devHandle,
                                  NULL,
                                  uWifiScanResultCallback);
        U_PORT_TEST_ASSERT(result == 0);
        if (gScanResult.channel != 0) {
            // We found it
            break;
        }
    }
    // Make sure the AP was found
    U_PORT_TEST_ASSERT(gScanResult.channel != 0);
    // Basic validation of the result
    U_PORT_TEST_ASSERT(validateScanResult(&gScanResult));

    //----------------------------------------------------------
    // Scan specifically for U_WIFI_TEST_CFG_SSID
    //----------------------------------------------------------
    memset(&gScanResult, 0, sizeof(gScanResult));
    for (int32_t i = 0; i < 3; i++) {
        result = uWifiStationScan(gHandles.devHandle,
                                  U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                  uWifiScanResultCallback);
        U_PORT_TEST_ASSERT(result == 0);
        if (gScanResult.channel != 0) {
            // We found it
            break;
        }
    }
    // Make sure the AP was found
    U_PORT_TEST_ASSERT(gScanResult.channel != 0);
    // Basic validation of the result
    U_PORT_TEST_ASSERT(validateScanResult(&gScanResult));

    //----------------------------------------------------------
    // Scan for non existent SSID
    //----------------------------------------------------------
    memset(&gScanResult, 0, sizeof(gScanResult));
    for (int32_t i = 0; i < 3; i++) {
        result = uWifiStationScan(gHandles.devHandle,
                                  "DUMMYSSID",
                                  uWifiScanResultCallback);
        U_PORT_TEST_ASSERT(result == 0);
        if (gScanResult.channel != 0) {
            // We found it
            break;
        }
    }
    // Make sure the AP was NOT found
    U_PORT_TEST_ASSERT(gScanResult.channel == 0);

    uWifiTestPrivatePostamble(&gHandles);

    // Handle errors
    U_PORT_TEST_ASSERT(testError == U_WIFI_TEST_ERROR_NONE);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[wifi]", "wifiCleanUp")
{
    uWifiTestPrivateCleanup(&gHandles);
}

#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
