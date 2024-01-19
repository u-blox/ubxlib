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
 * @brief Tests that should be run as a preamble in any suite
 * of tests to make sure that everything is in a good state.
 * This test suite can be made to run first by setting
 * U_RUNNER_PREAMBLE_STR to "preamble", which runner.c sets it to
 * by default anyway.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
//lint -efile(766, stdio.h)
#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"    // rand()
#include "time.h"      // mktime()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* In some cases mktime() and rand() */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#if defined(U_CFG_TEST_PIN_GNSS_RESET_N) && (U_CFG_TEST_PIN_GNSS_RESET_N >= 0)
#include "u_port_gpio.h"
#endif
#include "u_port_uart.h"
#include "u_port_i2c.h"

#include "u_test_util_resource_check.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
# include "u_cell_module_type.h"
# include "u_cell_test_preamble.h"
#endif

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
# include "u_short_range_module_type.h"
# include "u_short_range_test_preamble.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if defined(__NEWLIB__) && defined(_REENT_SMALL) && \
    !defined(_REENT_GLOBAL_STDIO_STREAMS) && !defined(_UNBUF_STREAM_OPT)
# define PRE_ALLOCATE_FILE_COUNT 16
#endif

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_PREAMBLE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** A defence against platform-related memory loss.
 * Some platform-related functions and library calls
 * (e.g. UART initialisation, rand(), printf()) allocate
 * memory from the heap when they are first called and never
 * free that memory again.  The heap accounting in our tests
 * can fail due to this loss, even though it is out of our control.
 * Hence this test is provided and positioned early in
 * the test suite to call those functions and hence move those
 * allocations out of the sums.
 */
U_PORT_TEST_FUNCTION("[preamble]", "preambleHeapDefence")
{
#if U_CFG_ENABLE_LOGGING
    int32_t heapPlatformLoss;
#endif
#if (U_CFG_TEST_UART_A >= 0) || (U_CFG_TEST_UART_B >= 0) || (U_CFG_APP_GNSS_I2C >= 0)
    int32_t handle;
#endif
    struct tm tmStruct = {0,  0, 0,  1, 0,  70,  0, 0, 0};
# if defined(U_CFG_TEST_PIN_GNSS_RESET_N) && (U_CFG_TEST_PIN_GNSS_RESET_N >= 0)
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;
# endif

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Print out the heap and stack usage before we've done
    // anything: useful information for RAM usage calculations
    U_TEST_PRINT_LINE("at start(ish) of day main task"
                      " stack had a minimum of %d byte(s) free.",
                      uPortTaskStackMinFree(NULL));
    U_TEST_PRINT_LINE("at start(ish) of day heap had a"
                      " minimum of %d byte(s) free.",
                      uPortGetHeapMinFree());

#if U_CFG_ENABLE_LOGGING
    heapPlatformLoss = uPortGetHeapFree();
#endif

    uPortInit();

    // Call the things that allocate memory
    U_TEST_PRINT_LINE("calling platform APIs that"
                      " might allocate memory when first called...");
    rand();
    mktime(&tmStruct);

#if (U_CFG_TEST_UART_A >= 0)
# ifdef U_CFG_TEST_UART_PREFIX
    U_PORT_TEST_ASSERT(uPortUartPrefix(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_UART_PREFIX)) == 0);
# endif
    handle = uPortUartOpen(U_CFG_TEST_UART_A, 115200,
                           NULL, U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                           U_CFG_TEST_PIN_UART_A_TXD,
                           U_CFG_TEST_PIN_UART_A_RXD,
                           U_CFG_TEST_PIN_UART_A_CTS,
                           U_CFG_TEST_PIN_UART_A_RTS);
    uPortUartClose(handle);
#endif

#if (U_CFG_TEST_UART_B >= 0)
    handle = uPortUartOpen(U_CFG_TEST_UART_B, 115200,
                           NULL, U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                           U_CFG_TEST_PIN_UART_B_TXD,
                           U_CFG_TEST_PIN_UART_B_RXD,
                           U_CFG_TEST_PIN_UART_B_CTS,
                           U_CFG_TEST_PIN_UART_B_RTS);
    uPortUartClose(handle);
#endif

#if (U_CFG_APP_GNSS_I2C >= 0)
    uPortI2cInit();
    handle = uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA,
                          U_CFG_APP_PIN_GNSS_SCL, true);
    uPortI2cClose(handle);
    uPortI2cDeinit();
#endif

#if defined(U_CFG_TEST_PIN_GNSS_RESET_N) && (U_CFG_TEST_PIN_GNSS_RESET_N >= 0)
    // If there is a GNSS module attached that has a RESET_N line
    // wired to it then pull that line low to reset the GNSS module,
    // nice and clean
    U_TEST_PRINT_LINE("resetting GNSS module by toggling pin %d (0x%0x) low.",
                      U_CFG_TEST_PIN_GNSS_RESET_N, U_CFG_TEST_PIN_GNSS_RESET_N);
    // Make the pin an open-drain output, and low
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_GNSS_RESET_N, 0) == 0);
    gpioConfig.pin = U_CFG_TEST_PIN_GNSS_RESET_N;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
    // Leave it low for half a second and release
    uPortTaskBlock(500);
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_GNSS_RESET_N, 1) == 0);
    // Let the chip recover
    uPortTaskBlock(2000);
#endif

    uPortEventQueueCleanUp();
    uPortDeinit();

#ifdef PRE_ALLOCATE_FILE_COUNT
    // This is a newlib-specific workaround
    // When newlib is built with _REENT_GLOBAL_STDIO_STREAMS *disabled*
    // a global dynamic pool will be used for FILE pointers.
    // The pool re-uses existing FILE pointers but if no FILE is currently
    // free for use, newlib will allocate a new one. Some of our tests check
    // heap usage directly, rather than through the resource counting mechanism,
    // and so this can result in "false" memory leak failurs.
    // To mitigate this problem we start with allocating a couple of FILE
    // pointers so that that newlib doesn't need to allocate any new ones
    // throughout the complete test suite.
    //
    static bool files_allocated = false;
    extern FILE *__sfp (struct _reent *);
    if (!files_allocated) {
        U_TEST_PRINT_LINE("pre-allocating FILE pointers.");
        FILE *f[PRE_ALLOCATE_FILE_COUNT];
        for (int i = 0; i < PRE_ALLOCATE_FILE_COUNT; i++) {
            f[i] = __sfp(_REENT);
        }
        for (int i = 0; i < PRE_ALLOCATE_FILE_COUNT; i++) {
            f[i]->_flags = 0;
        }
        files_allocated = true;
    }
#endif

#if U_CFG_ENABLE_LOGGING
    heapPlatformLoss -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to"
                      " the platform.",  heapPlatformLoss);
#endif
}

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
/** Set cellular straight.
 */
U_PORT_TEST_FUNCTION("[preamble]", "preambleCell")
{
    uCellTestPreamble(U_CFG_TEST_CELL_MODULE_TYPE);
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}
#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
/** Set short-range straight.
 */
U_PORT_TEST_FUNCTION("[preamble]", "preambleShortRange")
{
    uShortRangeTestPreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE);
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}
#endif // #ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[preamble]", "preambleCleanUp")
{
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

// End of file
