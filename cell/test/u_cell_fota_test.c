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
 * @brief Tests for the cellular FOTA API: these should pass on all
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

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_fota.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_FOTA_TEST: "

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
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// The FOTA status callback.
void fotaStatusCallback(uDeviceHandle_t cellHandle,
                        uCellFotaStatus_t *pStatus,
                        void *pParameter)
{
    // Deliberately empty

    (void) cellHandle;
    (void) pStatus;
    (void) pParameter;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test FOTA; there's very little here I'm afraid as it is not
 * currently possible to run a proper regression test of FOTA.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellFota]", "cellFotaVeryBasicIndeed")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;
    int32_t heapFotaInitLoss = 0;
    int32_t x;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if ((pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R412M_02B) &&
        (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R410M_02B)) {

        U_TEST_PRINT_LINE("setting FOTA call-back.");

        // The first call to FOTA will allocate a context which
        // is not deallocated until cellular is taken down, which
        // we don't do here to save time; take account of that
        // initialisation heap cost here.
        heapFotaInitLoss = uPortGetHeapFree();
        x = uCellFotaSetStatusCallback(cellHandle, -1, fotaStatusCallback, NULL);
        heapFotaInitLoss -= uPortGetHeapFree();
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FOTA)) {
            U_PORT_TEST_ASSERT(x == 0);
            x = uCellFotaSetStatusCallback(cellHandle, -1, NULL, NULL);
            U_PORT_TEST_ASSERT(x == 0);
        } else {
            U_PORT_TEST_ASSERT(x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
        }
    } else {
        // The SARA-R410M and SARA-R412M modules we have under regression
        // test are the 02B-02 varieties, not the 02B-03 varieties, which
        // are the ones that support FOTA
        U_TEST_PRINT_LINE("note testing FOTA on SARA-R410M-02B-02 or"
                          " SARA-R412M-02B-02.");
    }

    // That's all we can do I'm afraid

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("during this part of the test %d byte(s)"
                      " were lost to cell FOTA initialisation; we"
                      " have leaked %d byte(s).",
                      heapFotaInitLoss, heapUsed - heapFotaInitLoss);
    U_PORT_TEST_ASSERT(heapUsed <= heapFotaInitLoss);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellFota]", "cellFotaCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
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

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
