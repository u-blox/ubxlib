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
 * @brief Tests for the GNSS configuratoin API: these should pass on all
 * platforms that have a GNSS module connected to them.  They
 * are only compiled if U_CFG_TEST_GNSS_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_CFG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** The initial dynamic setting.
*/
static int32_t gDynamic = -1;

/** The initial fix mode.
*/
static int32_t gFixMode = -1;

/** The initial UTC standard.
*/
static int32_t gUtcStandard = -1;

/** Array of UTC standard values to check (ones that are supported by
 * all module types).
 */
static const uGnssUtcStandard_t gUtcStandardValues[] = {U_GNSS_UTC_STANDARD_AUTOMATIC,
                                                        U_GNSS_UTC_STANDARD_USNO,
                                                        U_GNSS_UTC_STANDARD_GALILEO,
                                                        U_GNSS_UTC_STANDARD_GLONASS,
                                                        U_GNSS_UTC_STANDARD_NTSC
                                                       };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the basic GNSS configuration functions.
 */
U_PORT_TEST_FUNCTION("[gnssCfg]", "gnssCfgBasic")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    size_t iterations;
    int32_t y;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssHandle, true);

        // Get the initial dynamic setting
        gDynamic = uGnssCfgGetDynamic(gnssHandle);
        U_TEST_PRINT_LINE("initial dynamic setting is %d.", gDynamic);
        U_PORT_TEST_ASSERT((gDynamic >= (int32_t) U_GNSS_DYNAMIC_PORTABLE) &&
                           (gDynamic <= (int32_t) U_GNSS_DYNAMIC_BIKE));

        // Get the initial fix mode
        gFixMode = uGnssCfgGetFixMode(gnssHandle);
        U_TEST_PRINT_LINE("initial fix mode is %d.", gFixMode);
        U_PORT_TEST_ASSERT((gFixMode >= (int32_t) U_GNSS_FIX_MODE_2D) &&
                           (gFixMode <= (int32_t) U_GNSS_FIX_MODE_AUTO));

        // Get the initial UTC standard
        gUtcStandard = uGnssCfgGetUtcStandard(gnssHandle);
        U_TEST_PRINT_LINE("initial UTC standard is %d.", gUtcStandard);
        U_PORT_TEST_ASSERT((gUtcStandard >= (int32_t) U_GNSS_UTC_STANDARD_AUTOMATIC) &&
                           (gUtcStandard <= (int32_t) U_GNSS_UTC_STANDARD_NPLI));

        // Set all the dynamic types except for U_GNSS_DYNAMIC_BIKE
        // since that is only supported on a specific protocol version
        // which might not be on the chip we're using
        for (int32_t z = (int32_t) U_GNSS_DYNAMIC_PORTABLE; z <= (int32_t) U_GNSS_DYNAMIC_WRIST; z++) {
            U_TEST_PRINT_LINE("setting dynamic %d.", z);
            U_PORT_TEST_ASSERT(uGnssCfgSetDynamic(gnssHandle, (uGnssDynamic_t) z) == 0);
            y = uGnssCfgGetDynamic(gnssHandle);
            U_TEST_PRINT_LINE("dynamic setting is now %d.", y);
            U_PORT_TEST_ASSERT(y == z);
            // Check that the fix mode and UTC standard haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetFixMode(gnssHandle) == gFixMode);
            U_PORT_TEST_ASSERT(uGnssCfgGetUtcStandard(gnssHandle) == gUtcStandard);
        }
        // Put the initial dynamic setting back
        U_PORT_TEST_ASSERT(uGnssCfgSetDynamic(gnssHandle, (uGnssDynamic_t) gDynamic) == 0);

        // Set all the fix modes
        for (int32_t z = (int32_t) U_GNSS_FIX_MODE_2D; z <= (int32_t) U_GNSS_FIX_MODE_AUTO; z++) {
            U_TEST_PRINT_LINE("setting fix mode %d.", z);
            U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, (uGnssFixMode_t) z) == 0);
            y = uGnssCfgGetFixMode(gnssHandle);
            U_TEST_PRINT_LINE("fix mode is now %d.", y);
            U_PORT_TEST_ASSERT(y == z);
            // Check that the dynamic setting and UTC standard haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetDynamic(gnssHandle) == gDynamic);
            U_PORT_TEST_ASSERT(uGnssCfgGetUtcStandard(gnssHandle) == gUtcStandard);
        }
        // Put the initial fix mode back
        U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, (uGnssFixMode_t) gFixMode) == 0);

        // Set all the UTC standards
        for (size_t z = 0; z < sizeof(gUtcStandardValues) / sizeof(gUtcStandardValues[0]); z++) {
            U_TEST_PRINT_LINE("setting UTC standard %d.", gUtcStandardValues[z]);
            U_PORT_TEST_ASSERT(uGnssCfgSetUtcStandard(gnssHandle, gUtcStandardValues[z]) == 0);
            y = uGnssCfgGetUtcStandard(gnssHandle);
            U_TEST_PRINT_LINE("UTC standard is now %d.", y);
            U_PORT_TEST_ASSERT(y == (int32_t) gUtcStandardValues[z]);
            // Check that the fix mode and dynamic setting haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetFixMode(gnssHandle) == gFixMode);
            U_PORT_TEST_ASSERT(uGnssCfgGetDynamic(gnssHandle) == gDynamic);
        }
        // Put the initial UTC standard back
        U_PORT_TEST_ASSERT(uGnssCfgSetUtcStandard(gnssHandle, (uGnssUtcStandard_t) gUtcStandard) == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssCfg]", "gnssCfgCleanUp")
{
    int32_t x;

    if ((gDynamic >= 0) && (gHandles.gnssHandle != NULL)) {
        // Put the initial dynamic setting back
        uGnssCfgSetDynamic(gHandles.gnssHandle, (uGnssDynamic_t) gDynamic);
    }

    if ((gFixMode >= 0) && (gHandles.gnssHandle != NULL)) {
        // Put the initial fix mode back
        uGnssCfgSetFixMode(gHandles.gnssHandle, (uGnssFixMode_t) gFixMode);
    }

    uGnssTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at the"
                          " end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
