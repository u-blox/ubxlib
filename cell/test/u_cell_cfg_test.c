/*
 * Copyright 2019-2023 u-blox
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
 * @brief Tests for the cellular cfg API: these should pass on all
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

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_cfg.h"
#include "u_cell_info.h"    // For uCellInfoTime()
#ifdef U_CELL_TEST_MUX_ALWAYS
# include "u_cell_mux.h"
#endif

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_CFG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_CFG_TEST_GREETING_STR
/** The greeting message to use during testing.
 */
# define U_CELL_CFG_TEST_GREETING_STR "beeble"
#endif

#ifndef U_CELL_CFG_TEST_GREETING_CALLBACK_INVALID_STR
/** An invalid length of string for a greeting-with-callback
 * i.e. 65 characters.
 */
# define U_CELL_CFG_TEST_GREETING_CALLBACK_INVALID_STR "01234567890123456789012345678901234567890123456789012345678901234"
#endif

#ifndef U_CELL_CFG_TEST_GNSS_IP_STR
/** The server string to use when testing forwarding of GNSS messages
 * with AT+UGPRF.
 */
# define U_CELL_CFG_TEST_GNSS_IP_STR "myserver:1234"
#endif

#ifndef U_CELL_CFG_TEST_TIME_OFFSET_SECONDS
/** How far ahead to adjust the time when testing.
 */
# define U_CELL_CFG_TEST_TIME_OFFSET_SECONDS 75
#endif

#ifndef U_CELL_CFG_TEST_TIME_MARGIN_SECONDS
/** The permitted margin between reading time several times during
 * testing, in seconds.
 */
# define U_CELL_CFG_TEST_TIME_MARGIN_SECONDS 10
#endif

#ifndef U_CELL_CFG_TEST_FIXED_TIME
/** A time value to use if the module doesn't have one: should be no
 * less than #U_CELL_INFO_TEST_MIN_TIME (i.e. 21 July 2021 13:40:36)
 * plus any timezone offset.
 */
# define U_CELL_CFG_TEST_FIXED_TIME (1626874836 + (3600 * 24))
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** The GNSS profile bit map.
 */
static int32_t gGnssProfileBitMapOriginal = -1;

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Read, change and check band mask for the given RAT
static void testBandMask(uDeviceHandle_t cellHandle,
                         uCellNetRat_t rat,
                         const char *pRatString,
                         uint32_t supportedRatsBitmap,
                         uCellModuleType_t moduleType)
{
    int32_t errorCode;
    uint64_t originalBandMask1 = 0;
    uint64_t originalBandMask2 = 0;
    uint64_t bandMask1;
    uint64_t bandMask2;

    U_TEST_PRINT_LINE("getting band masks for %s...", pRatString);
    errorCode = uCellCfgGetBandMask(cellHandle, rat,
                                    &originalBandMask1, &originalBandMask2);
    // For SARA-R4 and LARA-R6 the module reports the band mask for
    // all of the RATs it supports, while SARA-R5 only reports
    // the band masks for the RAT that is enabled, which in the
    // case of these tests is only one, the one at rank 0
    if (((moduleType != U_CELL_MODULE_TYPE_SARA_R5) ||
         (uCellCfgGetRatRank(cellHandle, rat) == 0)) &&
        (supportedRatsBitmap & (1UL << (int32_t) rat))) {
        U_PORT_TEST_ASSERT(errorCode == 0);
        U_TEST_PRINT_LINE("band mask for %s is 0x%08x%08x %08x%08x...",
                          pRatString,
                          (uint32_t) (originalBandMask2 >> 32), (uint32_t) originalBandMask2,
                          (uint32_t) (originalBandMask1 >> 32), (uint32_t) originalBandMask1);
    } else {
        U_PORT_TEST_ASSERT(errorCode != 0);
    }

    // Take the existing values and mask off every other bit
    U_TEST_PRINT_LINE("setting band mask for %s to"
                      " 0x%08x%08x %08x%08x...", pRatString,
                      (uint32_t) (U_CELL_TEST_CFG_ALT_BANDMASK2 >> 32),
                      (uint32_t) (U_CELL_TEST_CFG_ALT_BANDMASK2),
                      (uint32_t) (U_CELL_TEST_CFG_ALT_BANDMASK1 >> 32),
                      (uint32_t) (U_CELL_TEST_CFG_ALT_BANDMASK1));

    U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));

    errorCode = uCellCfgSetBandMask(cellHandle, rat,
                                    U_CELL_TEST_CFG_ALT_BANDMASK1,
                                    U_CELL_TEST_CFG_ALT_BANDMASK2);
    if (supportedRatsBitmap & (1UL << (int32_t) rat)) {
        U_PORT_TEST_ASSERT(errorCode == 0);
        U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));
        // Re-boot for the change to take effect
        U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
        U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));
        // For SARA-R5 we can only read it back if it is the current RAT
        if ((moduleType != U_CELL_MODULE_TYPE_SARA_R5) ||
            (uCellCfgGetRatRank(cellHandle, rat) == 0)) {
            U_TEST_PRINT_LINE("reading new band mask for %s...",
                              pRatString);
            U_PORT_TEST_ASSERT(uCellCfgGetBandMask(cellHandle, rat,
                                                   &bandMask1, &bandMask2) == 0);
            U_TEST_PRINT_LINE("new %s band mask is 0x%08x%08x %08x%08x...",
                              pRatString, (uint32_t) (bandMask2 >> 32), (uint32_t) bandMask2,
                              (uint32_t) (bandMask1 >> 32), (uint32_t) bandMask1);
            U_PORT_TEST_ASSERT(bandMask1 == U_CELL_TEST_CFG_ALT_BANDMASK1);
            U_PORT_TEST_ASSERT(bandMask2 == U_CELL_TEST_CFG_ALT_BANDMASK2);
            U_TEST_PRINT_LINE("putting original band masks back...");
            U_PORT_TEST_ASSERT(uCellCfgSetBandMask(cellHandle, rat,
                                                   originalBandMask1,
                                                   originalBandMask2) == 0);
            // Re-boot for the change to take effect
            U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
            U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
        }
    } else {
        U_PORT_TEST_ASSERT(errorCode != 0);
    }
}

// Callback for the greeting configuration test.
static void greetingCalback(uDeviceHandle_t cellHandle, void *pParameter)
{
    int32_t *pParam = (int32_t *) pParameter;

    (void) cellHandle;
    (*pParam)++;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/* Note: we don't test the uCellCfgFactoryReset() here since
 * it is a relatively simple function and performing a factory
 * reset before each test run on the modules in our test farm
 * probably isn't good use of their flash wear reserves.
 */

/** Test band masks.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgBandMask")
{
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(gHandles.cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Test cat-M1
    testBandMask(gHandles.cellHandle, U_CELL_NET_RAT_CATM1, "cat-M1",
                 pModule->supportedRatsBitmap, pModule->moduleType);

    // Test NB1
    testBandMask(gHandles.cellHandle, U_CELL_NET_RAT_NB1, "NB1",
                 pModule->supportedRatsBitmap, pModule->moduleType);

    // Test LTE
    testBandMask(gHandles.cellHandle, U_CELL_NET_RAT_LTE, "LTE",
                 pModule->supportedRatsBitmap, pModule->moduleType);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test getting/setting RAT.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgGetSetRat")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    size_t numSupportedRats = 0;
    uCellNetRat_t supportedRats[U_CELL_NET_RAT_MAX_NUM];
    int32_t heapUsed;

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

    // Fill the array up with the supported cellular RATs and
    // unused values
    for (size_t x = 0 ; x < sizeof(supportedRats) / sizeof(supportedRats[0]); x++) {
        supportedRats[x] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }
    for (size_t x = 0; x < sizeof(supportedRats) / sizeof(supportedRats[0]); x++) {
        if (pModule->supportedRatsBitmap & (1 << x)) {
            supportedRats[numSupportedRats] = (uCellNetRat_t) x;
            numSupportedRats++;
        }
    }

    uPortLog(U_TEST_PREFIX "%d RAT(s) supported by this module: ", numSupportedRats);
    for (size_t x = 0; x < numSupportedRats; x++) {
        if (x < numSupportedRats - 1) {
            uPortLog("%d, ", supportedRats[x]);
        } else {
            uPortLog("%d.\n", supportedRats[x]);
        }
    }

    // Set each one of them
    for (size_t x = 0; x < numSupportedRats; x++) {
        U_TEST_PRINT_LINE("setting sole RAT to %d...", supportedRats[x]);
        U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));
        U_PORT_TEST_ASSERT(uCellCfgSetRat(cellHandle, supportedRats[x]) == 0);
        U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));
        U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
        U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));

        for (size_t rank = 0; rank < pModule->maxNumSimultaneousRats; rank++) {
            if (rank == 0) {
                U_TEST_PRINT_LINE("checking that the RAT at rank 0 is %d...", supportedRats[x]);
                U_PORT_TEST_ASSERT(uCellCfgGetRat(cellHandle, rank) == supportedRats[x]);
            } else {
                U_TEST_PRINT_LINE("checking that there is no RAT at rank %d...", rank);
                U_PORT_TEST_ASSERT(uCellCfgGetRat(cellHandle, rank) == U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED);
            }
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test getting/setting RAT at a rank.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgSetGetRatRank")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    size_t numSupportedRats = 0;
    uCellNetRat_t setRats[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    uCellNetRat_t supportedRats[U_CELL_NET_RAT_MAX_NUM];
    uCellNetRat_t rat;
    uCellNetRat_t ratTmp;
    size_t count;
    size_t rank;
    int32_t found;
    int32_t numRats;
    int32_t repeats;
    int32_t y;
    int32_t readRank;
    int32_t heapUsed;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

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

    // Fill the array up with the supported cellular RATs and
    // unused values
    for (size_t x = 0 ; x < sizeof(supportedRats) / sizeof(supportedRats[0]); x++) {
        supportedRats[x] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }
    for (size_t x = 0; x < sizeof(supportedRats) / sizeof(supportedRats[0]); x++) {
        if (pModule->supportedRatsBitmap & (1 << x)) {
            supportedRats[numSupportedRats] = (uCellNetRat_t) x;
            numSupportedRats++;
        }
    }

    uPortLog(U_TEST_PREFIX "%d RAT(s) supported by this module: ", numSupportedRats);
    for (size_t x = 0; x < numSupportedRats; x++) {
        if (x < numSupportedRats - 1) {
            uPortLog("%d, ", supportedRats[x]);
        } else {
            uPortLog("%d.\n", supportedRats[x]);
        }
    }

    //  Note the code below deliberately checks an out of range value
    for (rank = 0; (rank <= numSupportedRats) &&
         (rank <= pModule->maxNumSimultaneousRats); rank++) {
        rat = uCellCfgGetRat(cellHandle, rank);
        if (rank == 0) {
            U_TEST_PRINT_LINE("RAT at rank %d is expected to be %d and "
                              "is %d.", rank,
                              uCellTestPrivateInitRatGet(pModule->supportedRatsBitmap), rat);
            U_PORT_TEST_ASSERT(rat == uCellTestPrivateInitRatGet(pModule->supportedRatsBitmap));
        } else {
            if (rank < pModule->maxNumSimultaneousRats) {
                U_TEST_PRINT_LINE("RAT at rank %d is expected to be %d and is %d.",
                                  rank, U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, rat);
                U_PORT_TEST_ASSERT(rat == U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED);
            } else {
                U_TEST_PRINT_LINE("asking for the RAT at rank %d is "
                                  "expected to fail and is %d.", rank, rat);
                U_PORT_TEST_ASSERT(rat < 0);
            }
        }
    }

    // Now set up the maximum number of supported RATs
    // deliberately checking out of range values
    U_TEST_PRINT_LINE("now set a RAT at all %d possible ranks.",
                      pModule->maxNumSimultaneousRats);
    for (rank = 0; rank <= pModule->maxNumSimultaneousRats; rank++) {
        if (rank < pModule->maxNumSimultaneousRats) {
            U_TEST_PRINT_LINE("setting RAT at rank %d to %d.",
                              rank, supportedRats[rank]);
            U_PORT_TEST_ASSERT(uCellCfgSetRatRank(cellHandle,
                                                  supportedRats[rank],
                                                  rank) == 0);
        } else {
            U_TEST_PRINT_LINE("try to set RAT at rank %d to %d, "
                              "should fail.", rank, supportedRats[0]);
            U_PORT_TEST_ASSERT(uCellCfgSetRatRank(cellHandle,
                                                  supportedRats[0],
                                                  rank) < 0);
        }
    }

    U_TEST_PRINT_LINE("expected RAT list is now:");
    for (rank = 0; rank < pModule->maxNumSimultaneousRats; rank++) {
        uPortLog("  rank %d: %d.\n", rank, supportedRats[rank]);
    }
    U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));
    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
    U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
    U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));

    // Check that worked and remember what was set
    for (rank = 0; rank <= pModule->maxNumSimultaneousRats; rank++) {
        y = uCellCfgGetRatRank(cellHandle, supportedRats[rank]);
        if (rank < pModule->maxNumSimultaneousRats) {
            U_TEST_PRINT_LINE("rank of RAT %d is expected to be "
                              "%d and is %d.", supportedRats[rank], rank, y);
            U_PORT_TEST_ASSERT(y == (int32_t) rank);
            setRats[rank] = supportedRats[rank];
        } else {
            U_TEST_PRINT_LINE("asking for the RAT at rank %d is "
                              "expected to fail and is %d.", rank, y);
            U_PORT_TEST_ASSERT(y < 0);
        }
    }

    U_TEST_PRINT_LINE("RAT list read back was:");
    for (rank = 0; rank < pModule->maxNumSimultaneousRats; rank++) {
        uPortLog("  rank %d: %d.\n", rank, supportedRats[rank]);
    }

    // Now randomly pick a rank to change and check, in each case,
    // that only the RAT at that rank has changed, and do this
    // enough times given the number of possible simultaneous RATs
    if (pModule->maxNumSimultaneousRats > 1) {
        repeats = 1UL << pModule->maxNumSimultaneousRats;
        U_TEST_PRINT_LINE("randomly removing RATs at ranks.");
        y = 0;
        while (y < repeats) {
            // Find a rat to change that leaves us with a non-zero number of RATs
            numRats = 0;
            while (numRats == 0) {
                rank = rand() % pModule->maxNumSimultaneousRats;
                // Find a RAT that isn't the one that is already set at this rank
                // ('cos that would be a pointless test)
                do {
                    rat = supportedRats[rand() % (sizeof(supportedRats) / sizeof(supportedRats[0]))];
                } while (rat == setRats[rank]);

                // Count the number of RATs left
                for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                    ratTmp = setRats[x];
                    if (x == rank) {
                        ratTmp = rat;
                    }
                    if (ratTmp != U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
                        numRats++;
                    }
                }
            }
            setRats[rank] = rat;

            y++;
            U_TEST_PRINT_LINE("changing RAT at rank %d to %d.", rank, setRats[rank]);
            // Do the setting
            U_PORT_TEST_ASSERT(uCellCfgSetRatRank(cellHandle, setRats[rank], rank) == 0);
            U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));
            U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
            U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
            U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));
            // Remove duplicates from the set RAT list
            for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                for (size_t z = x + 1; z < pModule->maxNumSimultaneousRats; z++) {
                    if ((setRats[x] > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) && (setRats[x] == setRats[z])) {
                        setRats[z] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
                    }
                }
            }
            // Sort empty values to the end as the driver does
            count = 0;
            for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                if (setRats[x] != U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
                    setRats[count] = setRats[x];
                    count++;
                }
            }
            for (; count < pModule->maxNumSimultaneousRats; count++) {
                setRats[count] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
            }
            U_TEST_PRINT_LINE("new expected RAT list is:");
            for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                uPortLog("  rank %d: %d.\n", x, setRats[x]);
            }
            // Check that the RATs are as expected
            U_TEST_PRINT_LINE("checking that the module agrees...");
            for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                rat = uCellCfgGetRat(cellHandle, x);
                uPortLog("  RAT at rank %d is expected to be %d and is %d.\n",
                         x, setRats[x], rat);
                U_PORT_TEST_ASSERT(rat == setRats[x]);
            }
            for (size_t x = 0 ; x < numSupportedRats; x++) {
                if (supportedRats[x] != U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
                    found = -1;
                    for (size_t z = 0; (found < 0) &&
                         (z < pModule->maxNumSimultaneousRats); z++) {
                        if (setRats[z] == supportedRats[x]) {
                            found = z;
                        }
                    }
                    readRank = uCellCfgGetRatRank(cellHandle, supportedRats[x]);
                    if (found < 0) {
                        if (readRank >= 0) {
                            uPortLog("  RAT %d is expected to be not ranked but is ranked at %d.\n",
                                     supportedRats[x], readRank);
                            U_PORT_TEST_ASSERT(false);
                        }
                    } else {
                        uPortLog("  rank of RAT %d is expected to be %d and is %d.\n",
                                 supportedRats[x], found, readRank);
                        U_PORT_TEST_ASSERT(found == readRank);
                    }
                }
            }
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test getting/setting MNO profile.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgGetSetMnoProfile")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t readMnoProfile;
    int32_t mnoProfile;
    int32_t heapUsed;

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

    U_TEST_PRINT_LINE("getting MNO profile...");
    readMnoProfile = uCellCfgGetMnoProfile(cellHandle);
    U_TEST_PRINT_LINE("MNO profile was %d.", readMnoProfile);

    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_MNO_PROFILE)) {
        U_PORT_TEST_ASSERT(readMnoProfile >= 0);
    } else {
        U_PORT_TEST_ASSERT(readMnoProfile < 0);
    }
    // Need to be careful here as changing the
    // MNO profile changes the RAT and the BAND
    // as well.  0 is usually the default one and 100
    // is Europe.
    if (readMnoProfile != 100) {
        if (pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6) {
            // LARA-R6 doesn't support 100 (Europe) so use
            // 201 (GCF-PTCRB) instead
            mnoProfile = 201;
        } else {
            mnoProfile = 100;
        }
    } else {
        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) {
            // SARA-R422 doesn't support setting MNO profile 0
            // so in this case use 90 (global)
            mnoProfile = 90;
        } else {
            mnoProfile = 0;
        }
    }

    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_MNO_PROFILE)) {
        U_TEST_PRINT_LINE("trying to set MNO profile while  connected...");
        gStopTimeMs = uPortGetTickTimeMs() +
                      (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
        U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL,
                                            keepGoingCallback) == 0);
        U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));
        U_PORT_TEST_ASSERT(uCellCfgSetMnoProfile(cellHandle,
                                                 mnoProfile) < 0);
        U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));

        U_TEST_PRINT_LINE("disconnecting to really set MNO profile...");
        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
        U_PORT_TEST_ASSERT(!uCellNetIsRegistered(cellHandle));
        U_PORT_TEST_ASSERT(uCellCfgSetMnoProfile(cellHandle,
                                                 mnoProfile) == 0);
        U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));
        U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
#ifdef U_CELL_TEST_MUX_ALWAYS
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
#endif
        U_PORT_TEST_ASSERT(!uCellPwrRebootIsRequired(cellHandle));
        readMnoProfile = uCellCfgGetMnoProfile(cellHandle);
        U_TEST_PRINT_LINE("MNO profile is now %d.", readMnoProfile);
        U_PORT_TEST_ASSERT(readMnoProfile == mnoProfile);
    } else {
        U_PORT_TEST_ASSERT(uCellCfgSetMnoProfile(cellHandle,
                                                 mnoProfile) < 0);
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test UDCONF.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgUdconf")
{
    uDeviceHandle_t cellHandle;
    int32_t udconfOriginal;
    int32_t x;
    int32_t setUdconf = 0;
    int32_t heapUsed;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // All modules support AT+UDCONF=1 so we can test that safely
    U_TEST_PRINT_LINE("getting UDCONF=1...");
    udconfOriginal = uCellCfgGetUdconf(cellHandle, 1, -1);
    U_TEST_PRINT_LINE("UDCONF=1 is %d.", udconfOriginal);
    U_PORT_TEST_ASSERT((udconfOriginal == 0) || (udconfOriginal == 1));

    if (udconfOriginal == 0) {
        setUdconf = 1;
    }

    U_TEST_PRINT_LINE("setting UDCONF=1,%d...", setUdconf);
    U_PORT_TEST_ASSERT(uCellCfgSetUdconf(cellHandle, 1, setUdconf, -1) == 0);
    x = uCellCfgGetUdconf(cellHandle, 1, -1);
    U_TEST_PRINT_LINE("UDCONF=1 is now %d.", x);
    U_PORT_TEST_ASSERT(x == setUdconf);
    U_PORT_TEST_ASSERT(uCellPwrRebootIsRequired(cellHandle));

    U_TEST_PRINT_LINE("putting UDCONF=1 back to what it was...");
    U_PORT_TEST_ASSERT(uCellCfgSetUdconf(cellHandle, 1, udconfOriginal, -1) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test setting auto-bauding off and on.
 * IMPORTANT: this test leaves auto-bauding OFF afterwards.
 * This is because that way, during automated testing, we will
 * get the greeting message as soon as the module has booted
 * rather than only when we send the first AT command to the
 * module.  This is deliberately NOT done as part of the preamble
 * run before the suite of tests since that preamble would be
 * run if the user were just running the examples and it is
 * better not to fix the baud rate of the cellular module
 * to the value we happen to chose just as a consequence of
 * the user running the examples.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgAutoBaud")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t x;
    int32_t heapUsed;

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

    U_TEST_PRINT_LINE("setting auto-bauding on...");
    x = uCellCfgSetAutoBaudOn(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)) {
        U_PORT_TEST_ASSERT(x == 0);
        U_PORT_TEST_ASSERT(uCellCfgAutoBaudIsOn(cellHandle));
    } else {
        U_PORT_TEST_ASSERT(x < 0);
        U_PORT_TEST_ASSERT(!uCellCfgAutoBaudIsOn(cellHandle));
    }

    U_TEST_PRINT_LINE("setting auto-bauding off...");
    U_PORT_TEST_ASSERT(uCellCfgSetAutoBaudOff(cellHandle) == 0);
    U_PORT_TEST_ASSERT(!uCellCfgAutoBaudIsOn(cellHandle));
    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)) {
        U_TEST_PRINT_LINE("IMPORTANT the baud rate of the cellular"
                          " module is now fixed at %d, if you want the module"
                          " to auto-baud your application must connect to the"
                          " module at %d and then call uCellCfgSetAutoBaudOff().",
                          U_CELL_UART_BAUD_RATE, U_CELL_UART_BAUD_RATE);
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test greeting message.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgGreeting")
{
    uDeviceHandle_t cellHandle;
    char bufferOriginal[64];
    char buffer[64];
    int32_t x;
    int32_t y;
    int32_t heapUsed;
    int32_t param = 0;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    U_TEST_PRINT_LINE("getting greeting...");
    x = uCellCfgGetGreeting(cellHandle, bufferOriginal, sizeof(bufferOriginal));
    U_TEST_PRINT_LINE("greeting is \"%s\".", bufferOriginal);

    U_TEST_PRINT_LINE("setting greeting to \"%s\"...", U_CELL_CFG_TEST_GREETING_STR);
    U_PORT_TEST_ASSERT(uCellCfgSetGreeting(cellHandle,
                                           U_CELL_CFG_TEST_GREETING_STR) == 0);

    y = uCellCfgGetGreeting(cellHandle, buffer, sizeof(buffer));
    U_TEST_PRINT_LINE("greeting is now \"%s\".", buffer);
    U_PORT_TEST_ASSERT(strcmp(buffer, U_CELL_CFG_TEST_GREETING_STR) == 0);
    U_PORT_TEST_ASSERT(y == strlen(buffer));

    // Set a greeting with callback with invalid parameters
    U_PORT_TEST_ASSERT(uCellCfgSetGreetingCallback(cellHandle,
                                                   NULL,
                                                   greetingCalback,
                                                   &param) < 0);
    U_PORT_TEST_ASSERT(uCellCfgSetGreetingCallback(cellHandle,
                                                   U_CELL_CFG_TEST_GREETING_CALLBACK_INVALID_STR,
                                                   greetingCalback,
                                                   &param) < 0);

    U_TEST_PRINT_LINE("setting greeting with callback to \"%s\"...",
                      U_CELL_CFG_GREETING);
    U_PORT_TEST_ASSERT(uCellCfgSetGreetingCallback(cellHandle,
                                                   U_CELL_CFG_GREETING,
                                                   greetingCalback,
                                                   &param) == 0);
    y = uCellCfgGetGreeting(cellHandle, buffer, sizeof(buffer));
    U_TEST_PRINT_LINE("greeting is now \"%s\".", buffer);
    U_PORT_TEST_ASSERT(strcmp(buffer, U_CELL_CFG_GREETING) == 0);
    U_PORT_TEST_ASSERT(y == strlen(buffer));

#if !defined(U_CFG_TEST_DISABLE_GREETING_CALLBACK) && (!defined(U_CFG_APP_PIN_CELL_DTR) || (U_CFG_APP_PIN_CELL_DTR < 0))
    // The greeting message is not emitted if DTR is used and we don't
    // test this on some instances
    U_TEST_PRINT_LINE("rebooting...");
    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
    U_PORT_TEST_ASSERT(param == 1);
#endif

    U_TEST_PRINT_LINE("removing greeting with callback...");
    U_PORT_TEST_ASSERT(uCellCfgSetGreetingCallback(cellHandle,
                                                   NULL, NULL, NULL) == 0);
    y = uCellCfgGetGreeting(cellHandle, buffer, sizeof(buffer));
    U_TEST_PRINT_LINE("greeting is now \"%s\".", buffer);
    U_PORT_TEST_ASSERT(y == 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == 0);

    U_TEST_PRINT_LINE("setting greeting with callback to \"%s\" again...",
                      U_CELL_CFG_GREETING);
    U_PORT_TEST_ASSERT(uCellCfgSetGreetingCallback(cellHandle,
                                                   U_CELL_CFG_GREETING,
                                                   greetingCalback,
                                                   &param) == 0);
    y = uCellCfgGetGreeting(cellHandle, buffer, sizeof(buffer));
    U_TEST_PRINT_LINE("greeting is now \"%s\".", buffer);
    U_PORT_TEST_ASSERT(strcmp(buffer, U_CELL_CFG_GREETING) == 0);
    U_PORT_TEST_ASSERT(y == strlen(buffer));

    U_TEST_PRINT_LINE("setting greeting to non-callback \"%s\"...",
                      U_CELL_CFG_GREETING);
    U_PORT_TEST_ASSERT(uCellCfgSetGreeting(cellHandle,
                                           U_CELL_CFG_GREETING) == 0);

    y = uCellCfgGetGreeting(cellHandle, buffer, sizeof(buffer));
    U_TEST_PRINT_LINE("greeting is now \"%s\".", buffer);
    U_PORT_TEST_ASSERT(strcmp(buffer, U_CELL_CFG_GREETING) == 0);
    U_PORT_TEST_ASSERT(y == strlen(buffer));

#if !defined(U_CFG_TEST_DISABLE_GREETING_CALLBACK) && (!defined(U_CFG_APP_PIN_CELL_DTR) || (U_CFG_APP_PIN_CELL_DTR < 0))
    // The greeting message is not emitted if DTR is used and we don't
    // test this on some instances
    U_TEST_PRINT_LINE("rebooting to make sure we don't know...");
    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
    U_PORT_TEST_ASSERT(param == 1);
#endif

    U_TEST_PRINT_LINE("putting greeting back to what it was...");
    if (x > 0) {
        U_PORT_TEST_ASSERT(uCellCfgSetGreeting(cellHandle, bufferOriginal) == 0);
    } else {
        U_PORT_TEST_ASSERT(uCellCfgSetGreeting(cellHandle, NULL) == 0);
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test setting GNSS profile.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgGnssProfile")
{
    uDeviceHandle_t cellHandle;
    char *pServerNameOriginal;
    char *pServerName;
    int32_t x;
    int32_t heapUsed;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Malloc space to hold the original GNSS profile server string and the
    // on we will read back during testing
    pServerNameOriginal = (char *) pUPortMalloc(U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
    U_PORT_TEST_ASSERT(pServerNameOriginal != NULL);
    pServerName = (char *) pUPortMalloc(U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
    U_PORT_TEST_ASSERT(pServerName != NULL);
    memset(pServerName, 0xFF, U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);

    U_TEST_PRINT_LINE("getting GNSS profile...");
    gGnssProfileBitMapOriginal = uCellCfgGetGnssProfile(cellHandle, pServerNameOriginal,
                                                        U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
    U_TEST_PRINT_LINE("GNSS profile is 0x%02x, \"%s\".", gGnssProfileBitMapOriginal,
                      pServerNameOriginal);

    U_TEST_PRINT_LINE("setting GNSS profile to MUX plus IP at \"%s\"...", U_CELL_CFG_TEST_GNSS_IP_STR);
    // We only check U_CELL_CFG_GNSS_PROFILE_IP plus one other (MUX)
    // since all modules support those
    U_PORT_TEST_ASSERT(uCellCfgSetGnssProfile(cellHandle,
                                              U_CELL_CFG_GNSS_PROFILE_IP | U_CELL_CFG_GNSS_PROFILE_MUX,
                                              U_CELL_CFG_TEST_GNSS_IP_STR) == 0);

    U_TEST_PRINT_LINE("checking GNSS profile...");
    x = uCellCfgGetGnssProfile(cellHandle, pServerName, U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
    U_TEST_PRINT_LINE("GNSS profile is now 0x%02x, \"%s\".", x, pServerName);
    U_PORT_TEST_ASSERT(x == (U_CELL_CFG_GNSS_PROFILE_IP | U_CELL_CFG_GNSS_PROFILE_MUX));
    U_PORT_TEST_ASSERT(strncmp(pServerName, U_CELL_CFG_TEST_GNSS_IP_STR,
                               sizeof(U_CELL_CFG_TEST_GNSS_IP_STR) - 1) == 0);

    // Make sure that the value that ends up in the profile does NOT include a server
    // name as that causes confusion inside the module
    U_TEST_PRINT_LINE("putting GNSS profile back to what it was without server...");
    U_PORT_TEST_ASSERT(uCellCfgSetGnssProfile(cellHandle,
                                              gGnssProfileBitMapOriginal & ~U_CELL_CFG_GNSS_PROFILE_IP,
                                              NULL) == 0);

    // Free memory
    uPortFree(pServerNameOriginal);
    uPortFree(pServerName);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test setting time.
 */
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgTime")
{
    uDeviceHandle_t cellHandle;
    int64_t timeLocal;
    int32_t timeZoneOffsetOriginalSeconds;
    int32_t timeZoneOffsetSeconds;
    int64_t x;
    int32_t heapUsed;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

#ifndef U_CELL_CFG_TEST_USE_FIXED_TIME_SECONDS
    // Get the time
    timeLocal = uCellInfoGetTime(cellHandle, &timeZoneOffsetOriginalSeconds);
    U_TEST_PRINT_LINE("local time is %d, timezone offset %d seconds.",
                      (int32_t) timeLocal, timeZoneOffsetOriginalSeconds);
#else
    timeLocal = U_CELL_CFG_TEST_FIXED_TIME;
    timeZoneOffsetOriginalSeconds = 3600;
    U_TEST_PRINT_LINE("using fixed local time %d, timezone offset %d seconds.",
                      (int32_t) timeLocal, timeZoneOffsetOriginalSeconds);
#endif
    // Set the time forward
    U_TEST_PRINT_LINE("setting time forward %d second(s)...", U_CELL_CFG_TEST_TIME_OFFSET_SECONDS);
    U_PORT_TEST_ASSERT(uCellCfgSetTime(cellHandle, timeLocal + U_CELL_CFG_TEST_TIME_OFFSET_SECONDS,
                                       timeZoneOffsetOriginalSeconds) == 0);
    x = uCellInfoGetTime(cellHandle, &timeZoneOffsetSeconds);
    U_TEST_PRINT_LINE("local time is now %d, timezone offset %d seconds.",
                      (int32_t) x, timeZoneOffsetSeconds);
    U_PORT_TEST_ASSERT(x - timeLocal >= U_CELL_CFG_TEST_TIME_OFFSET_SECONDS);
    U_PORT_TEST_ASSERT((x - timeLocal) - U_CELL_CFG_TEST_TIME_OFFSET_SECONDS <
                       U_CELL_CFG_TEST_TIME_MARGIN_SECONDS);
    // Set the timezone forward
    U_TEST_PRINT_LINE("setting timezone forward a quarter of an hour...");
    U_PORT_TEST_ASSERT(uCellCfgSetTime(cellHandle, x, timeZoneOffsetOriginalSeconds + (15 * 60)) == 0);
    x = uCellInfoGetTime(cellHandle, &timeZoneOffsetSeconds);
    U_TEST_PRINT_LINE("local time is now %d, timezone offset is now %d seconds.",
                      (int32_t) x, timeZoneOffsetSeconds);
    U_PORT_TEST_ASSERT(timeZoneOffsetSeconds - timeZoneOffsetOriginalSeconds == 15 * 60);
    U_PORT_TEST_ASSERT(x - timeLocal >= U_CELL_CFG_TEST_TIME_OFFSET_SECONDS);
    U_PORT_TEST_ASSERT((x - timeLocal) - U_CELL_CFG_TEST_TIME_OFFSET_SECONDS <
                       U_CELL_CFG_TEST_TIME_MARGIN_SECONDS);
    // Set the timezone backward
    U_TEST_PRINT_LINE("setting timezone to minus what it was...");
    U_PORT_TEST_ASSERT(uCellCfgSetTime(cellHandle, x, -timeZoneOffsetOriginalSeconds) == 0);
    x = uCellInfoGetTime(cellHandle, &timeZoneOffsetSeconds);
    U_TEST_PRINT_LINE("local time is now %d, timezone offset is now %d seconds.",
                      (int32_t) x, timeZoneOffsetSeconds);
    U_PORT_TEST_ASSERT(timeZoneOffsetSeconds + timeZoneOffsetOriginalSeconds == 0);
    U_PORT_TEST_ASSERT(x - timeLocal >= U_CELL_CFG_TEST_TIME_OFFSET_SECONDS);
    U_PORT_TEST_ASSERT((x - timeLocal) - U_CELL_CFG_TEST_TIME_OFFSET_SECONDS <
                       U_CELL_CFG_TEST_TIME_MARGIN_SECONDS);
    // Put everything back as it was
    U_TEST_PRINT_LINE("setting time back %d second(s) again and putting the timezone offset back to %d seconds.",
                      U_CELL_CFG_TEST_TIME_OFFSET_SECONDS, timeZoneOffsetOriginalSeconds);
    x = uCellInfoGetTime(cellHandle, &timeZoneOffsetSeconds);
    U_PORT_TEST_ASSERT(uCellCfgSetTime(cellHandle, x - U_CELL_CFG_TEST_TIME_OFFSET_SECONDS,
                                       timeZoneOffsetOriginalSeconds) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

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
U_PORT_TEST_FUNCTION("[cellCfg]", "cellCfgCleanUp")
{
    int32_t x;

    if ((gHandles.cellHandle != NULL) && (gGnssProfileBitMapOriginal >= 0)) {
        // Make sure that the value that ends up in the GNSS profile
        // does NOT include a server name as that causes confusion
        // inside the module
        uCellCfgSetGnssProfile(gHandles.cellHandle,
                               gGnssProfileBitMapOriginal & ~U_CELL_CFG_GNSS_PROFILE_IP,
                               NULL);
    }

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
