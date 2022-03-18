/*
 * Copyright 2020 u-blox
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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the cellular GPIO API: these should pass on all
 * platforms that have a cellular module connected to them.  They
 * are only compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

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
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_info.h"    // Required for uCellInfoIsxxxFlowControlEnabled()
#include "u_cell_gpio.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CFG_TEST_GPIO_NAME
/** The GPIO ID to use when testing.
 */
# define U_CFG_TEST_GPIO_NAME U_CELL_GPIO_NUMBER_TO_GPIO_ID(1)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test GPIOs.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellGpio]", "cellGpioBasic")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateInstance_t *pInstance;
    int32_t heapUsed;
    int32_t x;
    int32_t y;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module instance data as we need it for testing
    pInstance = pUCellPrivateGetInstance(cellHandle);
    U_PORT_TEST_ASSERT(pInstance != NULL);
    //lint -esym(613, pInstance) Suppress possible use of NULL pointer
    // for pInstance from now on

    uPortLog("U_CELL_GPIO_TEST: setting GPIO ID %d to an output and 1.\n",
             U_CFG_TEST_GPIO_NAME);
    U_PORT_TEST_ASSERT(uCellGpioConfig(cellHandle, U_CFG_TEST_GPIO_NAME,
                                       true, 1) == 0);
    x = uCellGpioGet(cellHandle, U_CFG_TEST_GPIO_NAME);
    uPortLog("U_CELL_GPIO_TEST: GPIO ID %d is %d.\n", U_CFG_TEST_GPIO_NAME, x);
    U_PORT_TEST_ASSERT(x == 1);
    uPortLog("U_CELL_GPIO_TEST: setting GPIO ID %d to 0.\n",
             U_CFG_TEST_GPIO_NAME);
    U_PORT_TEST_ASSERT(uCellGpioSet(cellHandle, U_CFG_TEST_GPIO_NAME, 0) == 0);
    x = uCellGpioGet(cellHandle, U_CFG_TEST_GPIO_NAME);
    uPortLog("U_CELL_GPIO_TEST: GPIO ID %d is %d.\n", U_CFG_TEST_GPIO_NAME, x);
    U_PORT_TEST_ASSERT(x == 0);

    // For toggling the CTS pin we need to know that it is not
    // already in use for flow control and this command is also not
    // supported on SARA-R4
    if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) &&
        !uCellInfoIsCtsFlowControlEnabled(cellHandle)) {
        uPortLog("U_CELL_GPIO_TEST: getting CTS...\n");
        x = uCellGpioGetCts(cellHandle);
        uPortLog("U_CELL_GPIO_TEST: CTS is %d.\n", x);
        U_PORT_TEST_ASSERT((x == 0) || (x == 1));
        uPortLog("U_CELL_GPIO_TEST: setting CTS to %d.\n", !((bool) x));
        U_PORT_TEST_ASSERT(uCellGpioSetCts(cellHandle, !x) == 0);
        y = uCellGpioGetCts(cellHandle);
        uPortLog("U_CELL_GPIO_TEST: CTS is now %d.\n", y);
        U_PORT_TEST_ASSERT(y == !((bool) x));
        uPortLog("U_CELL_GPIO_TEST: putting CTS back again...\n");
        U_PORT_TEST_ASSERT(uCellGpioSetCts(cellHandle, x) == 0);
    } else {
        uPortLog("U_CELL_GPIO_TEST: not testing setting of the CTS pin.\n");
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_GPIO_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellGpio]", "cellGpioCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_CELL_GPIO_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_CELL_GPIO_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
