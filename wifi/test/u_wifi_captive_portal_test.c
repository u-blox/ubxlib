/*
 * Copyright 2023 u-blox
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
 * connects to the stated access point and provides WiFi credentials.
 */

#ifdef U_CFG_TEST_WIFI_CAPTIVE_PORTAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.

// Must always be included before u_short_range_test_selector.h
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_sock.h"

#include "u_at_client.h"

#include "u_short_range.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"

#include "u_wifi_captive_portal.h"

#define U_TEST_PREFIX "U_WIFI_CAPTIVE_PORTAL_TEST: "
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

static uDeviceHandle_t gDeviceHandle;
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
        }
    }
};

U_PORT_TEST_FUNCTION("[wifiCapPort]", "wifiCapPortTest")
{
    uPortDeinit();
    int32_t heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);
    U_TEST_PRINT_LINE("initiating the module");
    U_PORT_TEST_ASSERT(uDeviceOpen(&gDeviceCfg, &gDeviceHandle) == 0);
    U_TEST_PRINT_LINE("start");

    int32_t returnCode = uWifiCaptivePortal(gDeviceHandle, "UBXLIB_TEST_PORTAL", NULL, NULL);
    U_PORT_TEST_ASSERT(returnCode == 0);

    uDeviceDeinit();
    uPortDeinit();

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

#endif