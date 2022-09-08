/*
 * Copyright 2022 u-blox
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

/** @file
 * @brief An Arduino sketch used ONLY to run the ubxlib examples/tests,
 * this NOT an example of how to write Arduino code.
 */

#ifdef U_CFG_OVERRIDE
# include <u_cfg_override.h> // For a customer's configuration override
#endif

#include <ubxlib.h>
#include <ubxlib_test.h>

// Bring in the U_CFG_APP_xxx application settings; you ONLY need
// this for the U_CFG_APP_xxx values below, it is not required by
// the ubxlib code
#include <u_cfg_app_platform_specific.h>

#ifdef U_RUNNER_TOP_STR
#include "u_runner.h"
#endif

// Required, just for this example file, to bring in U_CFG_OS_xxx;
// you would not need this in your application.
#include <u_cfg_os_platform_specific.h>

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Do things via Unity.
static void appTask(void *pParam) {
    (void) pParam;

#ifdef U_RUNNER_TOP_STR
    // If U_RUNNER_TOP_STR is defined we must be running inside the
    // test automation system (since the definition is added by
    // u_run.py) so run the tests through u_runner as that allows us
    // to do filtering

    uPortInit();

    UNITY_BEGIN();

    uPortLog("U_APP: functions available:\n\n");
    uRunnerPrintAll("U_APP: ");
# ifdef U_CFG_APP_FILTER
    uPortLog("U_APP: running functions that begin with \"%s\".\n",
             U_PORT_STRINGIFY_QUOTED(U_CFG_APP_FILTER));
    uRunnerRunFiltered(U_PORT_STRINGIFY_QUOTED(U_CFG_APP_FILTER),
                       "U_APP: ");
# else
    uPortLog("U_APP: running all functions.\n");
    uRunnerRunAll("U_APP: ");
# endif

    // The things that we have run may have
    // called deinit so call init again here.
    uPortInit();

    UNITY_END();

    uPortDeinit();

    while (1) {
        // Can't just put a large block here since there
        // might be a dog watching
        uPortTaskBlock(1000);
    }
#else
    // If U_RUNNER_TOP_STR is not defined we must be running outside
    // the test automation environment so call the normal ESP32
    // menu system
    unity_run_menu();
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** In this application we launch a task directly from setup()
 * which runs the ubxlib tests and examples in their normal
 * [non-Arduino] fashion, hence setup() never returns and
 * loop() is not used.
 */
void setup() {
    uPortPlatformStart(appTask, NULL,
                       U_CFG_OS_APP_TASK_STACK_SIZE_BYTES,
                       U_CFG_OS_APP_TASK_PRIORITY);

    // Should never get here
    assert(false);
}

/** Not used.
 */
void loop() {
  // put your main code here, to run repeatedly:

}
