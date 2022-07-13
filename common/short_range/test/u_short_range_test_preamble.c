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
 * @brief Test that should be run before any other short range tests or
 * examples.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_port_gpio.h"

#include "u_short_range_module_type.h"

#include "u_short_range_test_preamble.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SHORT_RANGE_TEST_PREAMBLE: "

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
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// The preamble of any suite of short-range tests/examples.
int32_t uShortRangeTestPreamble(uShortRangeModuleType_t moduleType)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
#if defined(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS) && (U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS >= 0)
    uPortGpioConfig_t gpioConfig;

    U_TEST_PRINT_LINE("start.");

    // The only thing to do is, if a "reset to defaults" pin is
    // defined, then make sure that the pin is set to an output
    // and is asserted; the pin will be connected to the DSR pin
    // of a short-range module and that module won't work correctly
    // unless DSR is normally asserted
    U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
    gpioConfig.pin = U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    errorCode = uPortGpioConfig(&gpioConfig);
    uPortGpioSet(U_CFG_APP_PIN_SHORT_RANGE_RESET_TO_DEFAULTS, 0); //assert

    U_TEST_PRINT_LINE("complete.");
#endif

#ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
    // If there is a test script monitoring progress
    // which operates switches for us, make sure that the
    // switches are all on.
    uPortLog("AUTOMATION_SET_SWITCH SHORT_RANGE 1\n");
    uPortTaskBlock(1000);
#endif

    (void) moduleType;

    return errorCode;
}

// End of file
