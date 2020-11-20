/*
 * Copyright 2020 u-blox Ltd
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
 * @brief Tests for the cellular power API: these should pass on all
 * platforms that have a cellular module connected to them.  They
 * are only compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
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
#include "u_cell_pwr.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMS;

/** Handles.
 */
static uCellTestPrivate_t gHandles = {-1};

/** A variable to track errors in the callbacks.
 */
static int32_t gCallbackErrorCode = 0;

/** For tracking heap lost to allocations made
 * by the C library in new tasks: newlib does NOT
 * necessarily reclaim it on task deletion.
 */
static size_t gSystemHeapLost = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular power-down process
static bool keepGoingCallback(int32_t cellHandle)
{
    bool keepGoing = true;

    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorCode = 1;
    }

    if (uPortGetTickTimeMs() > gStopTimeMS) {
        keepGoing = false;
    }

    return keepGoing;
}

// Test power on/off and aliveness, parameterised by the VInt pin.
static void testPowerAliveVInt(uCellTestPrivate_t *pHandles,
                               int32_t pinVint)
{
    bool (*pKeepGoingCallback) (int32_t) = NULL;
    int32_t cellHandle;
    bool trulyHardPowerOff = false;
    const uCellPrivateModule_t *pModule;
# if U_CFG_APP_PIN_CELL_VINT < 0
    int64_t timeMs;
# endif

# if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
    //lint -e(838) Suppress previously assigned value has not been used
    trulyHardPowerOff = true;
# endif

    uPortLog("U_CELL_PWR_TEST: running power-on and alive tests");
    if (pinVint >= 0) {
        uPortLog(" with VInt on pin %d.\n", pinVint);
    } else {
        uPortLog(" without VInt.\n");
    }

    uPortLog("U_CELL_PWR_TEST: adding a cellular instance on the AT client...\n");
    pHandles->cellHandle = uCellAdd(U_CFG_TEST_CELL_MODULE_TYPE,
                                    pHandles->atClientHandle,
                                    U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                    U_CFG_APP_PIN_CELL_PWR_ON,
                                    pinVint, false);
    U_PORT_TEST_ASSERT(pHandles->cellHandle >= 0);
    cellHandle = pHandles->cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Let the module state settle in case it is on but still
    // booting
    uPortTaskBlock((pModule->bootWaitSeconds) * 1000);

    // If the module is on at the start, switch it off.
    if (uCellPwrIsAlive(cellHandle)) {
        uPortLog("U_CELL_PWR_TEST: powering off to begin test.\n");
        uCellPwrOff(cellHandle, NULL);
        uPortLog("U_CELL_PWR_TEST: power off completed.\n");
# if U_CFG_APP_PIN_CELL_VINT < 0
        uPortLog("U_CELL_PWR_TEST: waiting another %d second(s)"
                 " to be sure of a clean power off as there's"
                 " no VInt pin to tell us...\n",
                 pModule->powerDownWaitSeconds);
        uPortTaskBlock(pModule->powerDownWaitSeconds);
# endif
    }

    // Do this twice so as to check transiting from
    // a call to uCellPwrOff() back to a call to uCellPwrOn().
    for (size_t x = 0; x < 2; x++) {
        uPortLog("U_CELL_PWR_TEST: testing power-on and alive calls");
        if (x > 0) {
            uPortLog(" with a callback passed to uCellPwrOff(), and"
                     " a %d second power-off timer, iteration %d.\n",
                     pModule->powerDownWaitSeconds, x + 1);
        } else {
            uPortLog(" with cellPwrOff(NULL), iteration %d.\n", x + 1);
        }
        U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
# if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
        U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
# endif
        // TODO Note: only use a NULL PIN as we don't support anything
        // else at least that's the case on SARA-R4 when you want to
        // have power saving
        uPortLog("U_CELL_PWR_TEST: powering on...\n");
        U_PORT_TEST_ASSERT(uCellPwrOn(cellHandle, U_CELL_TEST_CFG_SIM_PIN,
                                      NULL) == 0);
        uPortLog("U_CELL_PWR_TEST: checking that module is alive...\n");
        U_PORT_TEST_ASSERT(uCellPwrIsAlive(cellHandle));
        // Test with and without a keep-going callback
        if (x > 0) {
            // Note: can't check if keepGoingCallback is being
            // called here as we've no control over how long the
            // module takes to power off.
            pKeepGoingCallback = keepGoingCallback;
            gStopTimeMS = uPortGetTickTimeMs() +
                          (((int64_t) pModule->powerDownWaitSeconds) * 1000);
        }
        // Give the module time to sort itself out
        uPortLog("U_CELL_PWR_TEST: waiting %d second(s) before powering off...\n",
                 pModule->minAwakeTimeSeconds);
        uPortTaskBlock(pModule->minAwakeTimeSeconds * 1000);
# if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs();
# endif
        uPortLog("U_CELL_PWR_TEST: powering off...\n");
        uCellPwrOff(cellHandle, pKeepGoingCallback);
        uPortLog("U_CELL_PWR_TEST: power off completed.\n");
# if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs() - timeMs;
        if (timeMs < pModule->powerDownWaitSeconds * 1000) {
            timeMs = (pModule->powerDownWaitSeconds * 1000) - timeMs;
            uPortLog("U_CELL_PWR_TEST: waiting another %d second(s) to be sure of a "
                     "clean power off as there's no VInt pin to tell us...\n",
                     (int32_t) ((timeMs / 1000) + 1));
            uPortTaskBlock(timeMs);
        }
# endif
    }

    // Do this twice so as to check transiting from
    // a call to uCellPwrOffHard() to a call to
    // uCellPwrOn().
    for (size_t x = 0; x < 2; x++) {
        uPortLog("U_CELL_PWR_TEST: testing power-on and alive calls with "
                 "uCellPwrOffHard()");
        if (trulyHardPowerOff) {
            uPortLog(" and truly hard power off");
        }
        uPortLog(", iteration %d.\n", x + 1);
        U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
# if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
        U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
# endif
        uPortLog("U_CELL_PWR_TEST: powering on...\n");
        U_PORT_TEST_ASSERT(uCellPwrOn(cellHandle, U_CELL_TEST_CFG_SIM_PIN,
                                      NULL) == 0);
        uPortLog("U_CELL_PWR_TEST: checking that module is alive...\n");
        U_PORT_TEST_ASSERT(uCellPwrIsAlive(cellHandle));
        // Let the module sort itself out
        uPortLog("U_CELL_PWR_TEST: waiting %d second(s) before powering off...\n",
                 pModule->minAwakeTimeSeconds);
        uPortTaskBlock(pModule->minAwakeTimeSeconds * 1000);
# if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs();
# endif
        uPortLog("U_CELL_PWR_TEST: hard powering off...\n");
        uCellPwrOffHard(cellHandle, trulyHardPowerOff, NULL);
        uPortLog("U_CELL_PWR_TEST: hard power off completed.\n");
# if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs() - timeMs;
        if (!trulyHardPowerOff && (timeMs < pModule->powerDownWaitSeconds * 1000)) {
            timeMs = (pModule->powerDownWaitSeconds * 1000) - timeMs;
            uPortLog("U_CELL_PWR_TEST: waiting another %d second(s) to be"
                     " sure of a clean power off as there's no VInt pin to"
                     " tell us...\n", (int32_t) ((timeMs / 1000) + 1));
            uPortTaskBlock(timeMs);
        }
# endif
    }

    uPortLog("U_CELL_PWR_TEST: testing power-on and alive calls after hard power off.\n");
    U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
# if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
    U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
# endif

    uPortLog("U_CELL_PWR_TEST: removing cellular instance...\n");
    uCellRemove(cellHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test all the power functions apart from reboot.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwr")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Note: not using the standard preamble here as
    // we need to fiddle with the parameters into
    // uCellInit().
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    gHandles.uartHandle = uPortUartOpen(U_CFG_APP_CELL_UART,
                                        115200, NULL,
                                        U_CELL_UART_BUFFER_LENGTH_BYTES,
                                        U_CFG_APP_PIN_CELL_TXD,
                                        U_CFG_APP_PIN_CELL_RXD,
                                        U_CFG_APP_PIN_CELL_CTS,
                                        U_CFG_APP_PIN_CELL_RTS);
    U_PORT_TEST_ASSERT(gHandles.uartHandle >= 0);

    U_PORT_TEST_ASSERT(uAtClientInit() == 0);

    uPortLog("U_CELL_PWR_TEST: adding an AT client on UART %d...\n",
             U_CFG_APP_CELL_UART);
    gHandles.atClientHandle = uAtClientAdd(gHandles.uartHandle,
                                           U_AT_CLIENT_STREAM_TYPE_UART,
                                           NULL, U_CELL_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gHandles.atClientHandle != NULL);

    // So that we can see what we're doing
    uAtClientPrintAtSet(gHandles.atClientHandle, true);

    U_PORT_TEST_ASSERT(uCellInit() == 0);

    // The main bit, which is done with and
    // without use of the VInt pin, even
    // if it is connected
    testPowerAliveVInt(&gHandles, -1);
# if U_CFG_APP_PIN_CELL_VINT >= 0
    testPowerAliveVInt(&gHandles, U_CFG_APP_PIN_CELL_VINT);
# endif

    U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_PWR_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Test reboot.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrReboot")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);

    // Not much of a test really, need to find some setting
    // that is ephemeral so that we know whether a reboot has
    // occurred.  Anyway, this will be tested in those tests that
    // change bandmask and RAT.
    uPortLog("U_CELL_PWR_TEST: rebooting cellular...\n");
    U_PORT_TEST_ASSERT(uCellPwrReboot(gHandles.cellHandle, NULL) == 0);

    U_PORT_TEST_ASSERT(uCellPwrIsAlive(gHandles.cellHandle));

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_PWR_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    uPortLog("U_CELL_PWR_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", x);
    U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_CELL_PWR_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
