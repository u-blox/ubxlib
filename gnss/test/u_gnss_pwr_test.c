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
 * @brief Tests for the GNSS power API: these should pass on all
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

#include "u_cell_module_type.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_PWR_TEST: "

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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Power up and down a GNSS chip.
 */
U_PORT_TEST_FUNCTION("[gnssPwr]", "gnssPwrBasic")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];
    int32_t y;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, false,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        U_TEST_PRINT_LINE("powering on GNSS...");
        U_PORT_TEST_ASSERT(uGnssPwrOn(gnssHandle) == 0);

        U_TEST_PRINT_LINE("checking that GNSS is alive...");
        U_PORT_TEST_ASSERT(uGnssPwrIsAlive(gnssHandle));

        U_TEST_PRINT_LINE("powering off GNSS...");
        U_PORT_TEST_ASSERT(uGnssPwrOff(gnssHandle) == 0);

        switch (transportTypes[x]) {
            case U_GNSS_TRANSPORT_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UBX_UART:
                // If we are communicating via UART we can also test the
                // power-off-to-back-up version
                U_TEST_PRINT_LINE("powering on GNSS...");
                U_PORT_TEST_ASSERT(uGnssPwrOn(gnssHandle) == 0);

                U_TEST_PRINT_LINE("powering off GNSS to back-up mode...");
                U_PORT_TEST_ASSERT(uGnssPwrOffBackup(gnssHandle) == 0);
                break;
            case U_GNSS_TRANSPORT_I2C:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UBX_I2C:
                U_TEST_PRINT_LINE("not testing uGnssPwrOffBackup() 'cos we're on I2C...");
                break;
            case U_GNSS_TRANSPORT_AT:
                U_PORT_TEST_ASSERT(uGnssPwrOffBackup(gnssHandle) == U_ERROR_COMMON_NOT_SUPPORTED);
                break;
            default:
                U_PORT_TEST_ASSERT(false);
                break;
        }

#if U_CFG_APP_PIN_GNSS_ENABLE_POWER >= 0
        U_TEST_PRINT_LINE("checking that GNSS is no longer alive...");
        U_PORT_TEST_ASSERT(!uGnssPwrIsAlive(gnssHandle));
#endif

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost from the message stream during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble
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
U_PORT_TEST_FUNCTION("[gnssPwr]", "gnssPwrCleanUp")
{
    int32_t x;

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
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
