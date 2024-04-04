/*
 * Copyright 2024 u-blox
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
 * @brief Tests for the WiFi captive portal function. Requires that a client
 * connects to the stated access point and provides WiFi credentials for
 * a visible access point.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.

// Must always be included before u_short_range_test_selector.h
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

// *** UCX MISSING FUNCTION ***
// Currently no support for access point ip-address in ucx, hence disabled
#ifndef U_UCONNECT_GEN2

#if U_SHORT_RANGE_TEST_WIFI()

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"

#include "u_test_util_resource_check.h"

#include "u_timeout.h"

#include "u_sock.h"

#include "u_at_client.h"

#include "u_network.h"

#include "u_short_range.h"

#include "u_network_config_wifi.h"
#include "u_wifi_test_cfg.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"

#include "u_wifi_captive_portal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_CAPTIVE_PORTAL_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** Timeout for captive portal operation in seconds.
 */
#define U_WIFI_CAPTIVE_PORTAL_TEST_TIMEOUT_SECONDS 30

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uDeviceHandle_t gDeviceHandle = NULL;
static uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
        }
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        }
    }
};

static uTimeoutStop_t gTimeoutStop;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static bool keepGoingCallback(uDeviceHandle_t devHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT(devHandle == gDeviceHandle);

    if ((gTimeoutStop.durationMs > 0) &&
        uTimeoutExpiredMs(gTimeoutStop.timeoutStart,
                          gTimeoutStop.durationMs)) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiCaptivePortal]", "wifiCaptivePortal")
{
    uPortDeinit();
    int32_t resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);
    U_TEST_PRINT_LINE("initiating the module");
    U_PORT_TEST_ASSERT(uDeviceOpen(&gDeviceCfg, &gDeviceHandle) == 0);
    U_TEST_PRINT_LINE("start");

    // uWifiCaptivePortal() makes calls into the sockets API and
    // the first call to a sockets API initialises the underlying
    // sockets layer, occupying heap which is not recovered for
    // thread-safety reasons; to take account of that, make a
    // sockets call here.
    uNetworkCfgWifi_t networkCfg = {
        .type = U_NETWORK_TYPE_WIFI,
        .pSsid = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
        .authentication = U_WIFI_TEST_CFG_AUTHENTICATION,
        .pPassPhrase = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE)
    };
    U_PORT_TEST_ASSERT(uNetworkInterfaceUp(gDeviceHandle, U_NETWORK_TYPE_WIFI, &networkCfg) == 0);
    uSockAddress_t remoteAddress;
    U_PORT_TEST_ASSERT(uSockGetHostByName(gDeviceHandle,
                                          "8.8.8.8",
                                          &(remoteAddress.ipAddress)) == 0);
    uNetworkInterfaceDown(gDeviceHandle, U_NETWORK_TYPE_WIFI);

    // Now do the actual test
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_WIFI_CAPTIVE_PORTAL_TEST_TIMEOUT_SECONDS * 1000;
    int32_t returnCode = uWifiCaptivePortal(gDeviceHandle, "UBXLIB_TEST_PORTAL", NULL,
                                            keepGoingCallback);
    U_TEST_PRINT_LINE("uWifiCaptivePortal() returned %d.", returnCode);
    U_PORT_TEST_ASSERT(returnCode == 0);

    // The network interface will have been brought up by
    // uWifiCaptivePortal(), we need to take it down again
    uNetworkInterfaceDown(gDeviceHandle, U_NETWORK_TYPE_WIFI);

    // Clean-up sockets so that heap checking will add up, or maybe minus down
    uSockCleanUp();

    uDeviceClose(gDeviceHandle, false);
    gDeviceHandle = NULL;

    uDeviceDeinit();
    uPortDeinit();
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
U_PORT_TEST_FUNCTION("[wifiCaptivePortal]", "wifiCaptivePortalCleanUp")
{
    U_TEST_PRINT_LINE("cleaning up any outstanding resources.\n");

    if (gDeviceHandle != NULL) {
        uSockCleanUp();
        uNetworkInterfaceDown(gDeviceHandle, U_NETWORK_TYPE_WIFI);
        uDeviceClose(gDeviceHandle, false);
    }

    uDeviceDeinit();
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // U_SHORT_RANGE_TEST_WIFI()

#endif // U_UCONNECT_GEN2

// End of file
