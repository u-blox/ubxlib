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
 * @brief Tests for the wifi "general" API: these should pass on all
 * platforms where one UART is available. No short range module is
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
#include "u_wifi_cfg.h"

#include "u_wifi_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_CFG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiCfg]", "wifiCfgConfigureModule")
{
    int32_t heapUsed;
    uWifiCfg_t cfg;
    uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                     .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                     .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                     .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                     .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                     .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                   };

    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &uart,
                                                &gHandles) == 0);
    cfg.notUsed = false;
    U_PORT_TEST_ASSERT(uWifiCfgConfigure(gHandles.devHandle, &cfg) == 0);


    uWifiTestPrivatePostamble(&gHandles);

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

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[wifiCfg]", "wifiCfgCleanUp")
{
    int32_t x;

    uWifiTestPrivateCleanup(&gHandles);


    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // U_SHORT_RANGE_TEST_WIFI()
// End of file
