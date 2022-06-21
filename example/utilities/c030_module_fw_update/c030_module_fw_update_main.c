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

/** @brief This code can be used to set a C030-R5 or C030-R4xx board
 * into the correct state so that the FW of the cellular module on that
 * board can be updated.
 */

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_TEST_FUNCTION
# error if you are not using the unit test framework to run this code you must ensure that the platform clocks/RTOS are set up and either define U_PORT_TEST_FUNCTION yourself or replace it as necessary.
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
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleC030ModuleFwUpdate")
{
    // Do absolutely nothing: u_main.c has already done everything
    // that is required.

    uPortLog("This build sets the pins connected to a SARA-R4 or R5 module\n");
    uPortLog("on a C030 board to the correct state for firmware to be\n");
    uPortLog("downloaded to the module.\n\n");
    uPortLog("Note that a battery must be plugged into the board or, alternatively,\n");
    uPortLog("3.8V @ 5 Amps supplied to the VIn pin of the Arduino connector and the\n");
    uPortLog("jumper P1 moved to be nearest that pin (farthest from the module); USB\n");
    uPortLog("power alone is NOT sufficient.\n\n");
    uPortLog("When told to reset the module, simply lift P1 and put it back again.\n");
}

// End of file
