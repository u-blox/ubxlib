/*
 * Copyright 2019-2024 u-blox
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
 * @brief Tests for the cellular "general" API: these should pass on all
 * platforms where one or preferably two UARTs are available.  No
 * cellular module is actually used in this set of tests.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
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

#include "u_test_util_resource_check.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_TEST: "

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
static int32_t gUartAHandle = -1;

/** UART handle for another AT client.
 */
static int32_t gUartBHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then de-initialise cellular.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cell]", "cellInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uCellInit() == 0);
    uCellDeinit();
    uAtClientDeinit();
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#if (U_CFG_TEST_UART_A >= 0)
/** Add a cellular instance and remove it again.
 */
U_PORT_TEST_FUNCTION("[cell]", "cellAdd")
{
    uAtClientStreamHandle_t stream;
    uAtClientHandle_t atClientHandleA;
    uDeviceHandle_t devHandleA;
# if (U_CFG_TEST_UART_B >= 0)
    uAtClientHandle_t atClientHandleB;
    uDeviceHandle_t devHandleB;
    int32_t y;
# endif
    uDeviceHandle_t dummyHandle;
    uAtClientHandle_t atClientHandle = NULL;
    int32_t resourceCount;
    int32_t errorCode;
    int32_t x;
    int32_t a[4];
    int32_t b[4];
    int32_t c[4];
    int32_t d[4];

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

#ifdef U_CFG_TEST_UART_PREFIX
    U_PORT_TEST_ASSERT(uPortUartPrefix(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_UART_PREFIX)) == 0);
#endif
    gUartAHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                                 U_CFG_TEST_BAUD_RATE,
                                 NULL,
                                 U_CELL_UART_BUFFER_LENGTH_BYTES,
                                 U_CFG_TEST_PIN_UART_A_TXD,
                                 U_CFG_TEST_PIN_UART_A_RXD,
                                 U_CFG_TEST_PIN_UART_A_CTS,
                                 U_CFG_TEST_PIN_UART_A_RTS);
    U_PORT_TEST_ASSERT(gUartAHandle >= 0);

    U_PORT_TEST_ASSERT(uAtClientInit() == 0);

    U_PORT_TEST_ASSERT(uCellInit() == 0);

    U_TEST_PRINT_LINE("adding an AT client on UART %d...", U_CFG_TEST_UART_A);
    stream.handle.int32 = gUartAHandle;
    stream.type = U_AT_CLIENT_STREAM_TYPE_UART;
    atClientHandleA = uAtClientAddExt(&stream, NULL, U_CELL_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(atClientHandleA != NULL);

    U_TEST_PRINT_LINE("adding a cellular instance on that AT client...");
    errorCode = uCellAdd(U_CELL_MODULE_TYPE_SARA_U201, atClientHandleA,
                         -1, -1, -1, false, &devHandleA);
    U_PORT_TEST_ASSERT(errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
    U_PORT_TEST_ASSERT(uCellAtClientHandleGet(devHandleA,
                                              &atClientHandle) == 0);
    U_PORT_TEST_ASSERT(atClientHandle == atClientHandleA);

    // Check that we can get and set the inter-AT command delay
    // of this cellular instance
    x = uCellAtCommandDelayGet(devHandleA);
    U_TEST_PRINT_LINE("inter AT-command delay is %d ms.", x);
    U_PORT_TEST_ASSERT(x >= 0);
    x++;
    errorCode = uCellAtCommandDelaySet(devHandleA, x);
    U_PORT_TEST_ASSERT(errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
    errorCode = uCellAtCommandDelayGet(devHandleA);
    U_TEST_PRINT_LINE("inter AT-command delay is now %d ms.", errorCode);
    U_PORT_TEST_ASSERT(errorCode == x);
    x--;
    U_PORT_TEST_ASSERT(uCellAtCommandDelaySet(devHandleA, x) == 0);

    // Check that we can get and set all of the AT timings
    // of this cellular instance
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, NULL, NULL, NULL, NULL) == 0);
    a[0] = -1;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[0]), NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(a[0] > 0);
    b[0] = -1;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, NULL, &(b[0]), NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(b[0] > 0);
    c[0] = -1;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, NULL, NULL, &(c[0]), NULL) == 0);
    U_PORT_TEST_ASSERT(c[0] > 0);
    d[0] = -1;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, NULL, NULL, NULL, &(d[0])) == 0);
    U_PORT_TEST_ASSERT(d[0] > 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingSet(devHandleA, -1, -1, -1, -1) == 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[1]), &(b[1]), &(c[1]), &(d[1])) == 0);
    U_PORT_TEST_ASSERT(a[1] == a[0]);
    U_PORT_TEST_ASSERT(b[1] == b[0]);
    U_PORT_TEST_ASSERT(c[1] == c[0]);
    U_PORT_TEST_ASSERT(d[1] == d[0]);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingSet(devHandleA, a[0] + 1, b[0] + 1, c[0] + 1,
                                               d[0] + 1) == 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[1]), &(b[1]), &(c[1]), &(d[1])) == 0);
    U_PORT_TEST_ASSERT(a[1] == a[0] + 1);
    U_PORT_TEST_ASSERT(b[1] == b[0] + 1);
    U_PORT_TEST_ASSERT(c[1] == c[0] + 1);
    U_PORT_TEST_ASSERT(d[1] == d[0] + 1);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingSetDefault(devHandleA) == 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[1]), &(b[1]), &(c[1]), &(d[1])) == 0);
    U_PORT_TEST_ASSERT(a[1] == a[0]);
    U_PORT_TEST_ASSERT(b[1] == b[0]);
    U_PORT_TEST_ASSERT(c[1] == c[0]);
    U_PORT_TEST_ASSERT(d[1] == d[0]);

    U_TEST_PRINT_LINE("adding another instance on the same AT client, should fail...");
    U_PORT_TEST_ASSERT(uCellAdd(U_CELL_MODULE_TYPE_SARA_U201, atClientHandleA,
                                -1, -1, -1, false, &dummyHandle) < 0);

# if (U_CFG_TEST_UART_B >= 0)
    // If we have a second UART port, add a second cellular API on it
    // and do it the old way for now
    gUartBHandle = uPortUartOpen(U_CFG_TEST_UART_B,
                                 U_CFG_TEST_BAUD_RATE,
                                 NULL,
                                 U_CELL_UART_BUFFER_LENGTH_BYTES,
                                 U_CFG_TEST_PIN_UART_B_TXD,
                                 U_CFG_TEST_PIN_UART_B_RXD,
                                 U_CFG_TEST_PIN_UART_B_CTS,
                                 U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gUartBHandle >= 0);

    U_TEST_PRINT_LINE("adding an AT client on UART %d...", U_CFG_TEST_UART_B);
    atClientHandleB = uAtClientAdd(gUartBHandle, U_AT_CLIENT_STREAM_TYPE_UART,
                                   NULL, U_CELL_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(atClientHandleB != NULL);

    U_TEST_PRINT_LINE("adding a cellular instance on that AT client...");
    errorCode = uCellAdd(U_CELL_MODULE_TYPE_SARA_R5, atClientHandleB,
                         -1, -1, -1, false, &devHandleB);
    U_PORT_TEST_ASSERT(errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
    atClientHandle = NULL;
    U_PORT_TEST_ASSERT(uCellAtClientHandleGet(devHandleB,
                                              &atClientHandle) == 0);
    U_PORT_TEST_ASSERT(atClientHandle == atClientHandleB);

    U_TEST_PRINT_LINE("adding another instance on the same AT client,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uCellAdd(U_CELL_MODULE_TYPE_SARA_R5, atClientHandleB,
                                -1, -1, -1, false, &dummyHandle) < 0);

    // Check that we can get and set the inter-AT command delay
    // of this cellular instance without affecting the other
    y = uCellAtCommandDelayGet(devHandleA);
    U_PORT_TEST_ASSERT(y >= 0);
    x = uCellAtCommandDelayGet(devHandleB);
    U_TEST_PRINT_LINE("inter AT-command delay is %d ms.", x);
    U_PORT_TEST_ASSERT(x >= 0);
    x++;
    errorCode = uCellAtCommandDelaySet(devHandleB, x);
    U_PORT_TEST_ASSERT(errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
    errorCode = uCellAtCommandDelayGet(devHandleB);
    U_TEST_PRINT_LINE("inter AT-command delay is now %d ms.", errorCode);
    U_PORT_TEST_ASSERT(errorCode == x);
    errorCode = uCellAtCommandDelayGet(devHandleA);
    U_PORT_TEST_ASSERT(errorCode == y);
    x--;
    U_PORT_TEST_ASSERT(uCellAtCommandDelaySet(devHandleB, x) == 0);

    // Check that we can get and set all of the AT timings
    // of this cellular instance without affecting the other
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[0]), &(b[0]), &(c[0]), &(d[0])) == 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleB, &(a[1]), &(b[1]), &(c[1]), &(d[1])) == 0);
    a[1]++;
    b[1]++;
    c[1]++;
    d[1]++;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingSet(devHandleB, a[1], b[1], c[1], d[1]) == 0);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleB, &(a[2]), &(b[2]), &(c[2]), &(d[2])) == 0);
    U_PORT_TEST_ASSERT(a[2] == a[1]);
    U_PORT_TEST_ASSERT(b[2] == b[1]);
    U_PORT_TEST_ASSERT(c[2] == c[1]);
    U_PORT_TEST_ASSERT(d[2] == d[1]);
    U_PORT_TEST_ASSERT(uCellAtCommandTimingGet(devHandleA, &(a[3]), &(b[3]), &(c[3]), &(d[3])) == 0);
    U_PORT_TEST_ASSERT(a[3] == a[0]);
    U_PORT_TEST_ASSERT(b[3] == b[0]);
    U_PORT_TEST_ASSERT(c[3] == c[0]);
    U_PORT_TEST_ASSERT(d[3] == d[0]);
    a[1]--;
    b[1]--;
    c[1]--;
    d[1]--;
    U_PORT_TEST_ASSERT(uCellAtCommandTimingSet(devHandleB, a[1], b[1], c[1], d[1]) == 0);

    // Don't remove this one, let uCellDeinit() do it
# endif

    U_TEST_PRINT_LINE("removing first cellular instance...");
    uCellRemove(devHandleA);

    U_TEST_PRINT_LINE("adding it again...");
    errorCode = uCellAdd(U_CELL_MODULE_TYPE_SARA_U201, atClientHandleA,
                         -1, -1, -1, false, &devHandleA);
    U_PORT_TEST_ASSERT(errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
    U_PORT_TEST_ASSERT(devHandleA != NULL);
    atClientHandle = NULL;
    U_PORT_TEST_ASSERT(uCellAtClientHandleGet(devHandleA,
                                              &atClientHandle) == 0);
    U_PORT_TEST_ASSERT(atClientHandle == atClientHandleA);

    U_TEST_PRINT_LINE("deinitialising cellular API...");
    uCellDeinit();

    U_TEST_PRINT_LINE("removing AT client...");
    uAtClientRemove(atClientHandleA);

    uAtClientDeinit();

    uPortUartClose(gUartAHandle);
    gUartAHandle = -1;

# if (U_CFG_TEST_UART_B >= 0)
    uPortUartClose(gUartBHandle);
    gUartBHandle = -1;
# endif

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cell]", "cellCleanUp")
{
    uCellDeinit();
    uAtClientDeinit();
    if (gUartAHandle >= 0) {
        uPortUartClose(gUartAHandle);
    }
    if (gUartBHandle >= 0) {
        uPortUartClose(gUartBHandle);
    }
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

// End of file
