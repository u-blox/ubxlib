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
 * @brief Tests for the short range "general" API: these should pass on all
 * platforms where one or preferably two UARTs are available.
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
#include "u_port_os.h"
#if (U_CFG_TEST_SHORT_RANGE_UART >= 0)
#include "u_port_uart.h"
#include "u_port_debug.h"
#include "u_short_range_edm_stream.h"
#endif
#include "u_at_client.h"

#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uShortRangeTestPrivate_t gHandles = {-1, -1, NULL, -1};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void resetGlobals()
{
    gHandles.uartHandle = -1;
    gHandles.edmStreamHandle = -1;
    gHandles.atClientHandle = NULL;
    gHandles.shortRangeHandle = -1;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialize and then de-initialize short range.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);
    uShortRangeDeinit();
    uAtClientDeinit();
    uPortDeinit();
    resetGlobals();
}

#if (U_CFG_TEST_SHORT_RANGE_UART >= 0)

/** Add a ShortRange instance and remove it again using an uart stream.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeAddUart")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    gHandles.uartHandle = uPortUartOpen(U_CFG_TEST_SHORT_RANGE_UART,
                                        U_CFG_TEST_BAUD_RATE,
                                        NULL,
                                        U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                                        U_CFG_TEST_PIN_UART_B_TXD,
                                        U_CFG_TEST_PIN_UART_B_RXD,
                                        U_CFG_TEST_PIN_UART_B_CTS,
                                        U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gHandles.uartHandle >= 0);

    U_PORT_TEST_ASSERT(uAtClientInit() == 0);

    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);

    uPortLog("U_SHORT_RANGE_TEST: adding an AT client on UART %d...\n",
             U_CFG_TEST_SHORT_RANGE_UART);
    gHandles.atClientHandle = uAtClientAdd(gHandles.uartHandle, U_AT_CLIENT_STREAM_TYPE_UART,
                                           NULL, U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle != NULL);

    uPortLog("U_SHORT_RANGE_TEST: adding a short range instance on that AT client...\n");
    gHandles.shortRangeHandle = uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1,
                                               gHandles.atClientHandle);
    U_PORT_TEST_ASSERT(gHandles.shortRangeHandle >= 0);

    uPortLog("U_SHORT_RANGE_TEST: adding another instance on the same AT client,"
             " should fail...\n");
    U_PORT_TEST_ASSERT(uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1, gHandles.atClientHandle));

    uPortLog("U_SHORT_RANGE_TEST: removing first short range instance...\n");
    uShortRangeRemove(gHandles.shortRangeHandle);

    uPortLog("U_SHORT_RANGE_TEST: adding it again...\n");
    gHandles.shortRangeHandle = uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1,
                                               gHandles.atClientHandle);
    U_PORT_TEST_ASSERT(gHandles.shortRangeHandle >= 0);

    uPortLog("U_SHORT_RANGE_TEST: deinitialising short range API...\n");
    uShortRangeDeinit();

    uPortLog("U_SHORT_RANGE_TEST: removing AT client...\n");
    uAtClientRemove(gHandles.atClientHandle);
    uAtClientDeinit();

    uPortUartClose(gHandles.uartHandle);
    gHandles.uartHandle = -1;

    uPortDeinit();
    resetGlobals();
}

/** Add a ShortRange instance and remove it again using an edm stream.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeAddEdm")
{
    uPortDeinit();
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    gHandles.uartHandle = uPortUartOpen(U_CFG_TEST_SHORT_RANGE_UART,
                                        U_CFG_TEST_BAUD_RATE,
                                        NULL,
                                        U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                                        U_CFG_TEST_PIN_UART_B_TXD,
                                        U_CFG_TEST_PIN_UART_B_RXD,
                                        U_CFG_TEST_PIN_UART_B_CTS,
                                        U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gHandles.uartHandle >= 0);

    U_PORT_TEST_ASSERT(uAtClientInit() == 0);

    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);

    U_PORT_TEST_ASSERT(uShortRangeEdmStreamInit() == 0);

    uPortLog("U_SHORT_RANGE_TEST: open edm stream...\n");
    gHandles.edmStreamHandle = uShortRangeEdmStreamOpen(gHandles.uartHandle);
    U_PORT_TEST_ASSERT(gHandles.edmStreamHandle >= 0);

    uPortLog("U_SHORT_RANGE_TEST: adding an AT client on edm stream...\n");
    gHandles.atClientHandle = uAtClientAdd(gHandles.edmStreamHandle, U_AT_CLIENT_STREAM_TYPE_EDM,
                                           NULL, U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle != NULL);

    uPortLog("U_SHORT_RANGE_TEST: adding a short range instance on that AT client...\n");
    gHandles.shortRangeHandle = uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1,
                                               gHandles.atClientHandle);
    U_PORT_TEST_ASSERT(gHandles.shortRangeHandle >= 0);

    uPortLog("U_SHORT_RANGE_TEST: adding another instance on the same AT client,"
             " should fail...\n");
    U_PORT_TEST_ASSERT(uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1, gHandles.atClientHandle));

    uPortLog("U_SHORT_RANGE_TEST: removing first short range instance...\n");
    uShortRangeRemove(gHandles.shortRangeHandle);

    uPortLog("U_SHORT_RANGE_TEST: adding it again...\n");
    gHandles.shortRangeHandle = uShortRangeAdd(U_SHORT_RANGE_MODULE_TYPE_NINA_B1,
                                               gHandles.atClientHandle);
    U_PORT_TEST_ASSERT(gHandles.shortRangeHandle >= 0);

    uPortLog("U_SHORT_RANGE_TEST: deinitialising short range API...\n");
    uShortRangeRemove(gHandles.shortRangeHandle);
    uShortRangeDeinit();

    uPortLog("U_SHORT_RANGE_TEST: removing AT client...\n");
    uAtClientRemove(gHandles.atClientHandle);
    uAtClientDeinit();

    uShortRangeEdmStreamClose(gHandles.edmStreamHandle);
    uShortRangeEdmStreamDeinit();

    uPortUartClose(gHandles.uartHandle);
    gHandles.uartHandle = -1;

    uPortDeinit();
    resetGlobals();
}

#if (U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED >= 0)

/** Short range edm stream add and sent attention command.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeAddAndDetect")
{
    uPortDeinit();
    // Do the standard preamble
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_EDM,
                                                      &gHandles) == 0);
    uShortRangeTestPrivatePostamble(&gHandles);
}


/** Short range edm stream add and sent attention command.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeAddAndDetect")
{
    uPortDeinit();
    // Do the standard preamble
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_UART,
                                                      &gHandles) == 0);
    uShortRangeTestPrivatePostamble(&gHandles);
    resetGlobals();
}

/** Short range mode change.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeModeChange")
{
    uPortDeinit();
    // Do the standard preamble
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_UART,
                                                      &gHandles) == 0);

    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.shortRangeHandle) == 0);
    U_PORT_TEST_ASSERT(uShortRangeDataMode(gHandles.shortRangeHandle) == 0);
    // should fail
    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.shortRangeHandle) != 0);
    U_PORT_TEST_ASSERT(uShortRangeCommandMode(gHandles.shortRangeHandle,
                                              &gHandles.atClientHandle) == 0);

    uShortRangeTestPrivatePostamble(&gHandles);
    resetGlobals();
}

/** Short range mode change.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeRecover")
{
    uPortDeinit();
    // Do the standard preamble
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_EDM,
                                                      &gHandles) == 0);
    uShortRangeTestPrivatePostamble(&gHandles);

    // Module in EDM, start up in command mode
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_UART,
                                                      &gHandles) == 0);

    uShortRangeDataMode(gHandles.shortRangeHandle);
    gHandles.atClientHandle = NULL;

    uShortRangeTestPrivatePostamble(&gHandles);

    // Module in data mode, start up in EDM mode
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_EDM,
                                                      &gHandles) == 0);

    uShortRangeTestPrivatePostamble(&gHandles);

    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_UART,
                                                      &gHandles) == 0);

    uShortRangeDataMode(gHandles.shortRangeHandle);
    gHandles.atClientHandle = NULL;

    uShortRangeTestPrivatePostamble(&gHandles);

    // Module in data mode, start up in command mode
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_CONNECTED,
                                                      U_AT_CLIENT_STREAM_TYPE_EDM,
                                                      &gHandles) == 0);

    uShortRangeTestPrivatePostamble(&gHandles);
    resetGlobals();
}

#endif
#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeCleanUp")
{
    uShortRangeTestPrivateCleanup(&gHandles);
}

// End of file
