/*
 * Copyright 2020 u-blox Ltd
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
 * @brief Tests for the ble "general" API: these should pass on all
 * platforms where one UART is available. No short range module is
 * actually used in this set of tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
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

#include "u_short_range.h"
#include "u_short_range_edm_stream.h"
#include "u_ble.h"

#include "u_ble_test_private.h"

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
static int32_t gUartHandle = -1;
static int32_t gEdmStreamHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then de-initialise ble.
 */
U_PORT_TEST_FUNCTION("[ble]", "bleInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeEdmStreamInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uBleInit() == 0);
    uBleDeinit();
    uAtClientDeinit();
    uShortRangeEdmStreamDeinit();
    uPortDeinit();
}

#if (U_CFG_TEST_SHORT_RANGE_UART >= 0)
/** Add a ble instance and remove it again.
 */
U_PORT_TEST_FUNCTION("[ble]", "bleAdd")
{
    int32_t bleHandle;
    uAtClientHandle_t atClientHandle;
    uAtClientHandle_t atClientHandleCheck = (uAtClientHandle_t) -1;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    gUartHandle = uPortUartOpen(U_CFG_TEST_SHORT_RANGE_UART,
                                U_CFG_TEST_BAUD_RATE,
                                NULL,
                                U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                                U_CFG_TEST_PIN_UART_B_TXD,
                                U_CFG_TEST_PIN_UART_B_RXD,
                                U_CFG_TEST_PIN_UART_B_CTS,
                                U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gUartHandle >= 0);

    U_PORT_TEST_ASSERT(uShortRangeEdmStreamInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uBleInit() == 0);

    gEdmStreamHandle = uShortRangeEdmStreamOpen(gUartHandle);
    U_PORT_TEST_ASSERT(gEdmStreamHandle >= 0);

    uPortLog("U_BLE_TEST: adding an AT client on UART %d...\n",
             U_CFG_TEST_SHORT_RANGE_UART);
    atClientHandle = uAtClientAdd(gEdmStreamHandle, U_AT_CLIENT_STREAM_TYPE_EDM,
                                  NULL, U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(atClientHandle != NULL);

    uPortLog("U_BLE_TEST: adding a ble instance on that AT client...\n");
    bleHandle = uBleAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B3, atClientHandle);
    U_PORT_TEST_ASSERT(bleHandle >= 0);
    U_PORT_TEST_ASSERT(uBleAtClientHandleGet(bleHandle,
                                             &atClientHandleCheck) == 0);
    U_PORT_TEST_ASSERT(atClientHandle == atClientHandleCheck);

    uPortLog("U_BLE_TEST: adding another instance on the same AT client,"
             " should fail...\n");
    U_PORT_TEST_ASSERT(uBleAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B3, atClientHandle));

    uPortLog("U_BLE_TEST: removing ble instance...\n");
    uBleRemove(bleHandle);

    uPortLog("U_BLE_TEST: adding it again...\n");
    bleHandle = uBleAdd(U_BLE_MODULE_TYPE_NINA_B3, atClientHandle);
    U_PORT_TEST_ASSERT(bleHandle >= 0);

    atClientHandleCheck = (uAtClientHandle_t) -1;
    U_PORT_TEST_ASSERT(uBleAtClientHandleGet(bleHandle,
                                             &atClientHandleCheck) == 0);
    U_PORT_TEST_ASSERT(atClientHandle == atClientHandleCheck);

    uPortLog("U_BLE_TEST: deinitialising ble API...\n");
    uBleDeinit();

    uPortLog("U_BLE_TEST: removing AT client...\n");
    uAtClientRemove(atClientHandle);

    uAtClientDeinit();

    uShortRangeEdmStreamClose(gEdmStreamHandle);
    gEdmStreamHandle = -1;

    uPortUartClose(gUartHandle);
    gUartHandle = -1;

    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_BLE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}
#if (U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED >= 0)

static uBleTestPrivate_t gHandles;

U_PORT_TEST_FUNCTION("[ble]", "bleDetect")
{
    int32_t heapUsed;
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uBleTestPrivatePreamble(U_BLE_MODULE_TYPE_NINA_B3,
                                               &gHandles) == 0);

    uBleTestPrivatePostamble(&gHandles);

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_BLE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}

#endif
#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[ble]", "bleCleanUp")
{
    int32_t x;

    uBleDeinit();
    uAtClientDeinit();
    if (gUartHandle >= 0) {
        uPortUartClose(gUartHandle);
    }
    if (gEdmStreamHandle >= 0) {
        uShortRangeEdmStreamClose(gEdmStreamHandle);
    }

    x = uPortTaskStackMinFree(NULL);
    uPortLog("U_BLE_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", x);
    U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_BLE_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
