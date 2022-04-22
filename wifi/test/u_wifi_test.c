/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** UART handle for one AT client.
 */
static uWifiTestPrivate_t gHandles = {-1, -1, NULL, NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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

    uPortLog("U_WIFI: calling uShortRangeOpenUart with same arg twice,"
             " should fail...\n");
    uDeviceHandle_t dummyHandle;
    U_PORT_TEST_ASSERT(uShortRangeOpenUart((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                           &uart, true, &dummyHandle) < 0);

    uWifiTestPrivatePostamble(&gHandles);

    uPortLog("U_WIFI: calling uShortRangeOpenUart with NULL uart arg,"
             " should fail...\n");
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                NULL,
                                                &gHandles) < 0);
    uPortLog("U_WIFI: calling uShortRangeOpenUart with wrong module type,"
             " should fail...\n");
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_SHORT_RANGE_MODULE_TYPE_INTERNAL,
                                                &uart,
                                                &gHandles) < 0);
    uart.uartPort = -1;
    uPortLog("U_WIFI: calling uShortRangeOpenUart with invalid uart arg,"
             " should fail...\n");
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
    uPortLog("U_WIFI_TEST: we have leaked %d byte(s).\n", heapUsed);
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
U_PORT_TEST_FUNCTION("[wifi]", "wifiCleanUp")
{
    uWifiTestPrivateCleanup(&gHandles);
}

#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
