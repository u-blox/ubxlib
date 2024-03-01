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
 * @brief Tests for the timeout API, should run without any HW.
 *
 * IMPORTANT: this test only makes sense with #U_CFG_TEST_TIMEOUT_SPEED_UP
 * set to 18 or more.  To run thie test in an acceptable time-frame (around
 * a minute) please set #U_CFG_TEST_TIMEOUT_SPEED_UP to 18 when compiling
 * ubxlib. The aim here is to have the tick timer wrap during testing
 * and no tests to get "stuck" as a result.  With a 1 ms tick, a 32-bit
 * counter would wrap in 2^32 - 1 (4,294,967,295) seconds, so a
 * speed-up of 18 means a wrap every 16 seconds.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"
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

#include "u_test_util_resource_check.h"

#include "u_timeout.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_TIMEOUT_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_TIMEOUT_NUMBER_OF_WRAPS
/** How many times we would like to go around the
 * clock during testing.
 */
# define U_TIMEOUT_NUMBER_OF_WRAPS 2
#endif

#ifndef U_TIMEOUT_TEST_ITERATIONS
/** How many iterations we would like in total.
 */
# define U_TIMEOUT_TEST_ITERATIONS (U_TIMEOUT_NUMBER_OF_WRAPS * 5)
#endif

#ifndef U_TIMEOUT_DURATION_MS
/** How long each timeout should be.
 */
# define U_TIMEOUT_DURATION_MS ((UINT32_MAX * U_TIMEOUT_NUMBER_OF_WRAPS) / U_TIMEOUT_TEST_ITERATIONS)
#endif

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

#if (U_CFG_TEST_TIMEOUT_SPEED_UP >= 18)
// A basic test that the timeout functions do not get stuck at a wrap.
U_PORT_TEST_FUNCTION("[timeout]", "timeoutWrap")
{
    int32_t resourceCount;
    uTimeoutStart_t timeoutStart;
    int32_t startTickMs;
    int32_t stopTickMs;
    uint32_t acceleratedElapsedTimeMs;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    uPortInit();

    U_TEST_PRINT_LINE("testing timeout API at 2^%d real time"
                      ", so one day passes in %d milliseconds"
                      " and a 32-bit counter will wrap every"
                      " %d seconds; there will be %d iterations"
                      " each of length %d days.",
                      U_CFG_TEST_TIMEOUT_SPEED_UP,
                      86400000 >> U_CFG_TEST_TIMEOUT_SPEED_UP,
                      ((UINT32_MAX >> U_CFG_TEST_TIMEOUT_SPEED_UP) + 1) / 1000,
                      U_TIMEOUT_TEST_ITERATIONS,
                      (U_TIMEOUT_DURATION_MS << U_CFG_TEST_TIMEOUT_SPEED_UP) / 86400000);

    for (uint32_t x = 0; x < U_TIMEOUT_TEST_ITERATIONS; x++) {
        U_TEST_PRINT_LINE("timeout %d: %d day(s)...", x,
                          (U_TIMEOUT_DURATION_MS << U_CFG_TEST_TIMEOUT_SPEED_UP) / 86400000);
        timeoutStart = uTimeoutStart();
        startTickMs = uPortGetTickTimeMs();
        U_PORT_TEST_ASSERT(uTimeoutElapsedMs(timeoutStart) << U_CFG_TEST_TIMEOUT_SPEED_UP <
                           U_TIMEOUT_DURATION_MS);
        while (!uTimeoutExpiredMs(timeoutStart, U_TIMEOUT_DURATION_MS)) {
            uPortTaskBlock(10);
        }
        U_PORT_TEST_ASSERT(uTimeoutElapsedMs(timeoutStart) << U_CFG_TEST_TIMEOUT_SPEED_UP >=
                           U_TIMEOUT_DURATION_MS);
        stopTickMs = uPortGetTickTimeMs();
        acceleratedElapsedTimeMs = (uint32_t) (stopTickMs - startTickMs) << U_CFG_TEST_TIMEOUT_SPEED_UP;
        U_TEST_PRINT_LINE("...took %u day(s) to elapse%s.",
                          acceleratedElapsedTimeMs / 86400000,
                          ((uint32_t) (stopTickMs) << U_CFG_TEST_TIMEOUT_SPEED_UP) <
                          ((uint32_t) (startTickMs) << U_CFG_TEST_TIMEOUT_SPEED_UP) ? " and the underlying tick wrapped" :
                          "");
        // If a timer has taken longer than one half of a loop around
        // uPortGetTickTimeMs(), when scaled by U_CFG_TEST_TIMEOUT_SPEED_UP
        // then it has got stuck
        U_PORT_TEST_ASSERT(acceleratedElapsedTimeMs < UINT32_MAX / 2);
    }

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#endif

// End of file