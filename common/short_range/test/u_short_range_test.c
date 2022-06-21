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
 * @brief Tests for the short range "general" API: these should pass on all
 * platforms where one or preferably two UARTs are available.
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
#if defined U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
#include "u_port_os.h"
#endif
#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#if (U_CFG_TEST_UART_A >= 0) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE)
#include "u_port_debug.h"
#endif

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
#include "u_ble_sps.h"
#include "u_short_range_private.h"
#endif
#include "u_short_range_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SHORT_RANGE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
# ifndef U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS
/** The GPIO pin to use in the shortRangeResetToDefaultSettings()
 * test.
 */
#  define U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS -1
# endif
#endif
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

//lint -esym(551, gHandles) Suppress symbols not accessed
static uShortRangeTestPrivate_t gHandles = {-1, -1, NULL, NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void resetGlobals()
{
    gHandles.uartHandle = -1;
    gHandles.edmStreamHandle = -1;
    gHandles.atClientHandle = NULL;
    gHandles.devHandle = NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialize and then de-initialize short range.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
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

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE

/** Short range open UART test.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeOpenUart")
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
    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                      &uart,
                                                      &gHandles) == 0);

    U_PORT_TEST_ASSERT(uShortRangeGetUartHandle(gHandles.devHandle) == gHandles.uartHandle);
    U_PORT_TEST_ASSERT(uShortRangeGetEdmStreamHandle(gHandles.devHandle) ==
                       gHandles.edmStreamHandle);
    uShortRangeAtClientHandleGet(gHandles.devHandle, &atClient);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle == atClient);
    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.devHandle) == 0);

    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with same arg twice,"
                      " should fail...");
    uDeviceHandle_t devHandle;
    U_PORT_TEST_ASSERT(uShortRangeOpenUart(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE, &uart,
                                           true, &devHandle) < 0);

    uShortRangeTestPrivatePostamble(&gHandles);

    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with NULL uart arg,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                      NULL,
                                                      &gHandles) < 0);
    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with wrong module type,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_SHORT_RANGE_MODULE_TYPE_INTERNAL,
                                                      &uart,
                                                      &gHandles) < 0);
    uart.uartPort = -1;
    U_TEST_PRINT_LINE("calling uShortRangeOpenUart with invalid uart arg,"
                      " should fail...");
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                      &uart,
                                                      &gHandles) < 0);

    uShortRangeTestPrivateCleanup(&gHandles);
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

/** Short range set baudrate UART test.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeUartSetBaudrate")
{
    uAtClientHandle_t atClient = NULL;
    uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                     .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                     .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                     .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                     .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                     .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                   };

    int32_t testBaudrates[] = { 19200,
                                38400,
                                57600,
                                230400,
                                //460800, TODO: Enable this when instance 12 uses flow control
                                115200
                              };
    uPortDeinit();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                      &uart,
                                                      &gHandles) == 0);

    U_PORT_TEST_ASSERT(uShortRangeGetUartHandle(gHandles.devHandle) == gHandles.uartHandle);
    uShortRangeAtClientHandleGet(gHandles.devHandle, &atClient);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle == atClient);
    /* port is now opened at default speed */

    for (int32_t count = 0; count < (sizeof(testBaudrates) / sizeof(int32_t)); count++) {
        U_TEST_PRINT_LINE("setting baudrate %d.", testBaudrates[count]);
        uart.baudRate = testBaudrates[count];
        U_PORT_TEST_ASSERT(uShortRangeSetBaudrate(&gHandles.devHandle, &uart) == 0);
        // Must re-get the handles since uShortRangeSetBaudrate() will have
        // closed and re-opened them all
        gHandles.uartHandle = uShortRangeGetUartHandle(gHandles.devHandle);
        gHandles.edmStreamHandle = uShortRangeGetEdmStreamHandle(gHandles.devHandle);
        U_PORT_TEST_ASSERT(uShortRangeAtClientHandleGet(gHandles.devHandle,
                                                        &gHandles.atClientHandle) == 0);
        // This should receive a valid response
        U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.devHandle) == 0);
    }
    uShortRangeTestPrivateCleanup(&gHandles);
    U_TEST_PRINT_LINE("shortRangeUartSetBaudrate succeded.");
}

#if defined(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS) && (U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS >= 0)

/** Short range reset to default UART settings test.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeResetToDefaultSettings")
{
    uAtClientHandle_t atClient = NULL;
    uShortRangePrivateInstance_t *pInstance;
    uShortRangeModuleType_t moduleType;
    uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                     .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                     .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                     .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                     .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                     .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                   };
    uPortDeinit();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uAtClientInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeInit() == 0);
    U_PORT_TEST_ASSERT(uShortRangeTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                      &uart,
                                                      &gHandles) == 0);

    U_PORT_TEST_ASSERT(uShortRangeGetUartHandle(gHandles.devHandle) == gHandles.uartHandle);
    uShortRangeAtClientHandleGet(gHandles.devHandle, &atClient);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle == atClient);
    /* port is now opened at default speed, set other speed for test */

    uart.baudRate = 19200;
    U_TEST_PRINT_LINE("setting baudrate on host and target to %d.", uart.baudRate);
    U_PORT_TEST_ASSERT(uShortRangeSetBaudrate(&gHandles.devHandle, &uart) == 0); // set to 19200
    // Must re-get the handles since uShortRangeSetBaudrate() will have
    // closed and re-opened them all
    gHandles.uartHandle = uShortRangeGetUartHandle(gHandles.devHandle);
    gHandles.edmStreamHandle = uShortRangeGetEdmStreamHandle(gHandles.devHandle);
    U_PORT_TEST_ASSERT(uShortRangeAtClientHandleGet(gHandles.devHandle,
                                                    &gHandles.atClientHandle) == 0);
    // This should receive a valid response
    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.devHandle) == 0);

    U_TEST_PRINT_LINE("restoring to default settings via GPIO pin...");
    // restore to default 115200
    uShortRangeResetToDefaultSettings(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS);

    U_TEST_PRINT_LINE("comm. should now fail due to different baudrates.");
    U_PORT_TEST_ASSERT(uShortRangeAttention(gHandles.devHandle) != 0); // should not get valid respons

    pInstance = pUShortRangePrivateGetInstance(gHandles.devHandle);
    moduleType = pInstance->pModule->moduleType;
    uart.baudRate = 115200;
    uShortRangeClose(gHandles.devHandle);
    U_TEST_PRINT_LINE("setting baudrate on host to %d.", uart.baudRate);
    U_PORT_TEST_ASSERT(uShortRangeOpenUart(moduleType, &uart, false,
                                           &gHandles.devHandle) == 0); // target should already be at 115200 due to reset
    uShortRangeTestPrivateCleanup(&gHandles);

    U_TEST_PRINT_LINE("shortRangeResetToDefaultSettings() succeded.");
}

#endif // #if U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS >= 0

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[shortRange]", "shortRangeCleanUp")
{
    uShortRangeTestPrivateCleanup(&gHandles);
}

#endif

// End of file
