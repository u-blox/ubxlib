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

/** @file
 * @brief The application entry point for the NRF52 platform.  Starts
 * the platform and calls Unity to run the selected examples/tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_debug_utils.h"

#include "nrf_delay.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** When running under automation the target is reset and then
 * logging begins, which should be fine, except that the NRF logging
 * system over RTT doesn't seem to like reaching a blocking state
 * for any length of time, which it will do if there's a long string
 * printed (e.g. a lot of tests in the test menu) _before_ an RTT
 * logging thing is connected to the MCU.  Hence this delay is added
 * to keep it happy.
 */
#ifndef U_CFG_STARTUP_DELAY_SECONDS
# define U_CFG_STARTUP_DELAY_SECONDS 0
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// This is intentionally a bit hidden and comes from u_port_debug.c
extern volatile int32_t gStdoutCounter;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// The task within which th examples and tests run.
static void appTask(void *pParam)
{
    (void) pParam;

#if U_CFG_TEST_ENABLE_INACTIVITY_DETECTOR
    uDebugUtilsInitInactivityDetector(&gStdoutCounter);
#endif

#ifdef U_CFG_MUTEX_DEBUG
    uMutexDebugInit();
    uMutexDebugWatchdog(uMutexDebugPrint, NULL,
                        U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS);
#endif

    uPortInit();

    uPortLog("\n\nU_APP: application task started.\n");

    // Optional delay for Segger RTT autodetection
    uPortTaskBlock(U_CFG_STARTUP_DELAY_SECONDS * 1000);

    UNITY_BEGIN();

    uPortLog("U_APP: functions available:\n\n");
    uRunnerPrintAll("U_APP: ");
    // Give some slack for RTT here so that the RTT buffer is empty when we
    // start the tests.
    uPortTaskBlock(100);
#ifdef U_CFG_APP_FILTER
    uPortLog("U_APP: running functions that begin with \"%s\".\n",
             U_PORT_STRINGIFY_QUOTED(U_CFG_APP_FILTER));
    uRunnerRunFiltered(U_PORT_STRINGIFY_QUOTED(U_CFG_APP_FILTER),
                       "U_APP: ");
#else
    uPortLog("U_APP: running all functions.\n");
    uRunnerRunAll("U_APP: ");
#endif

    // The things that we have run may have
    // called deinit so call init again here.
    uPortInit();

    UNITY_END();

    uPortLog("\n\nU_APP: application task ended.\n");
    uPortDeinit();

    while (1) {}
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Unity setUp() function.
void setUp(void)
{
    // Nothing to do
}

// Unity tearDown() function.
void tearDown(void)
{
    // Add a small delay between test to make sure the
    // host have some time to read out RTT buffer
    nrf_delay_ms(10);
}

void testFail(void)
{
    // Nothing to do
}

// Entry point
int main(void)
{
    // Start the platform to run the tests
    uPortPlatformStart(appTask, NULL,
                       U_CFG_OS_APP_TASK_STACK_SIZE_BYTES,
                       U_CFG_OS_APP_TASK_PRIORITY);

    // Should never get here
    U_ASSERT(false);

    return 0;
}

// End of file
