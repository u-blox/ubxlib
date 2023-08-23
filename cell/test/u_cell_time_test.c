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
 * @brief Tests for the cellular CellTime API: these should pass on all
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

#include "limits.h"    // INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), memcpy()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_loc.h"     // For uCellLocGnssInsideCell()
#include "u_cell_time.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_TIME_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_TIME_TEST_MIN_UTC_TIME
/** A minimum value for UTC time (21 July 2021 13:40:36).
 */
# define U_CELL_TIME_TEST_MIN_UTC_TIME 1626874836
#endif

#ifndef U_CELL_TIME_TEST_MAX_CELL_TIME
/** A maximum value for CellTime (in seconds).
 */
# define U_CELL_TIME_TEST_MAX_CELL_TIME 120ULL
#endif

#ifndef U_CELL_TIME_TEST_DEEP_SCAN_TIMEOUT_SECONDS
/** Guard time for deep scan.
 */
# define U_CELL_TIME_TEST_DEEP_SCAN_TIMEOUT_SECONDS 60
#endif

#ifndef U_CELL_TIME_TEST_GUARD_TIME_SECONDS
/** Guard time for CellTime operations.
 */
# define U_CELL_TIME_TEST_GUARD_TIME_SECONDS 30
#endif

#ifndef U_CELL_TIME_TEST_RETRIES
/** How many times to re-try CellTime if it fails to
 * synchronise the first time.
 */
# define U_CELL_TIME_TEST_RETRIES 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Linked list container for #uCellNetCellInfo_t.
 */
typedef struct uCellTimeTestCellInfoList_t {
    uCellNetCellInfo_t cell;
    struct uCellTimeTestCellInfoList_t *pNext;
} uCellTimeTestCellInfoList_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs = 0;

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** Flag to share with eventCallback.
 */
static int32_t gEventCallback;

/** Storage for event callback data.
 */
static uCellTimeEvent_t gEvent;

/** Flag to share with timeCallback.
 */
static int32_t gTimeCallback;

/** Storage for time callback data.
 */
static uCellTime_t gTime;

/** Place to hook a list of cell information.
 */
static uCellTimeTestCellInfoList_t *gpCellInfoList = NULL;

/** Flag to determine the success of cellInfoCallback().
 */
static int32_t gCellInfoCallback;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the scan and cellular connection processes.
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback for uCellTimeEnable() events.
static void eventCallback(uDeviceHandle_t cellHandle,
                          uCellTimeEvent_t *pEvent,
                          void *pParameter)
{
    int32_t *pEventCallback = (int32_t *) pParameter;

    gEventCallback = 0;

    if (pEventCallback != &gEventCallback) {
        gEventCallback = -1;
    }

    if (cellHandle != gHandles.cellHandle) {
        *pEventCallback = -2;
    }

    if (pEvent != NULL) {
        gEvent = *pEvent;
        if ((gEvent.result == U_CELL_TIME_RESULT_OFFSET_DETECTED) &&
            (gEvent.offsetNanoseconds == 0)) {
            *pEventCallback = -3;
        }
    } else {
        *pEventCallback = -4;
    }
}

// Callback for time.
static void timeCallback(uDeviceHandle_t cellHandle, uCellTime_t *pTime,
                         void *pParameter)
{
    int32_t *pTimeCallback = (int32_t *) pParameter;

    gTimeCallback = 0;

    if (pTimeCallback != &gTimeCallback) {
        gTimeCallback = -1;
    }

    if (cellHandle != gHandles.cellHandle) {
        *pTimeCallback = -2;
    }

    if (pTime != NULL) {
        gTime = *pTime;
    } else {
        *pTimeCallback = -3;
    }
}

// Clear a cell information list.
static void clearCellInfoList(uCellTimeTestCellInfoList_t **ppCellInfoList)
{
    uCellTimeTestCellInfoList_t *pTmp;

    if (ppCellInfoList != NULL) {
        while (*ppCellInfoList != NULL) {
            pTmp = (*ppCellInfoList)->pNext;
            uPortFree(*ppCellInfoList);
            *ppCellInfoList = pTmp;
        }
    }
}

// Callback for cell information.
static bool cellInfoCallback(uDeviceHandle_t cellHandle,
                             uCellNetCellInfo_t *pCell,
                             void *pParameter)
{
    bool keepGoing = true;
    uCellTimeTestCellInfoList_t **ppCellInfoList = (uCellTimeTestCellInfoList_t **) pParameter;
    uCellTimeTestCellInfoList_t *pTmp;

    gCellInfoCallback = 0;

    if (ppCellInfoList != &gpCellInfoList) {
        gCellInfoCallback = -1;
    }

    if (cellHandle != gHandles.cellHandle) {
        gCellInfoCallback = -2;
    }

    if (pCell != NULL) {
        // Make a copy of the cell information and add it to the list
        pTmp = *ppCellInfoList;
        *ppCellInfoList = (uCellTimeTestCellInfoList_t *) pUPortMalloc(sizeof(uCellTimeTestCellInfoList_t));
        if (*ppCellInfoList != NULL) {
            memcpy(&((*ppCellInfoList)->cell), pCell, sizeof((*ppCellInfoList)->cell));
            (*ppCellInfoList)->pNext = pTmp;
        } else {
            *ppCellInfoList = pTmp;
            gCellInfoCallback = -3;
        }
    }

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Print and check an event structure.
static void printAndCheckEvent(uCellTimeEvent_t *pEvent, bool cellTime)
{
    U_TEST_PRINT_LINE("  synchronised:     %s.", pEvent->synchronised ? "true" : "false");
    U_TEST_PRINT_LINE("  result:           %d.", pEvent->result);
    U_TEST_PRINT_LINE("  mode:             %d.", pEvent->mode);
    U_TEST_PRINT_LINE("  source:           %d.", pEvent->source);
    U_TEST_PRINT_LINE("  physical cell ID: %d.", pEvent->cellIdPhysical);
    U_TEST_PRINT_LINE("  cell time:        %s.", pEvent->cellTime ? "true" : "false");
    U_TEST_PRINT_LINE("  offset:           %d.%09d.",
                      (int32_t) (pEvent->offsetNanoseconds / 1000000000),
                      (int32_t) (pEvent->offsetNanoseconds % 1000000000));
    U_PORT_TEST_ASSERT(pEvent->result == 0);
    // Can't check mode - it seems to come back as "best-effort" sometimes,
    // despite us specifically requesting CellTime ONLY.
    if (cellTime) {
        U_PORT_TEST_ASSERT(pEvent->source == U_CELL_TIME_SOURCE_CELL);
    }
    if (pEvent->source == U_CELL_TIME_SOURCE_CELL) {
        if (pEvent->cellIdPhysical < 0) {
            // Can't assert on this as sometimes AT+CELLINFO returns 65535 for
            // the cell ID, even after CellTime says that it has successfully
            // synchronised to it
            U_TEST_PRINT_LINE("*** WARNING *** CELLINFO did not return a valid cell ID.");
        }
    } else {
        U_PORT_TEST_ASSERT(pEvent->cellIdPhysical == -1);
    }
    U_PORT_TEST_ASSERT(pEvent->cellTime);
    U_PORT_TEST_ASSERT(pEvent->offsetNanoseconds >= 0);
}

// Print and check a time structure.
static void printAndCheckTime(uCellTime_t *pTime)
{
    U_TEST_PRINT_LINE("  cell time: %s.", pTime->cellTime ? "true" : "false");
    U_TEST_PRINT_LINE("  time:      %d.%09d.", (int32_t) (pTime->timeNanoseconds / 1000000000),
                      (int32_t) (pTime->timeNanoseconds % 1000000000));
    U_TEST_PRINT_LINE("  accuracy:  %d.%09d.", (int32_t) (pTime->accuracyNanoseconds / 1000000000),
                      (int32_t) (pTime->accuracyNanoseconds % 1000000000));
    if (pTime->cellTime) {
        U_PORT_TEST_ASSERT(pTime->timeNanoseconds < U_CELL_TIME_TEST_MAX_CELL_TIME * 1000000000);
    } else {
        U_PORT_TEST_ASSERT(pTime->timeNanoseconds / 1000000000 >= U_CELL_TIME_TEST_MIN_UTC_TIME);
    }
    U_PORT_TEST_ASSERT(pTime->accuracyNanoseconds >= 0);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test all the functions of the CellTime API.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellTime]", "cellTimeBasic")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;
    int32_t y;
    int32_t startTimeMs;
    uCellTimeTestCellInfoList_t *pTmp;
    bool gnssIsInsideCell;
#ifndef U_CELL_CFG_SARA_R5_00B
    int32_t timingAdvance;
#endif

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

    gnssIsInsideCell = uCellLocGnssInsideCell(cellHandle);

    // Make a cellular connection so that we can test that sync works
    // despite that
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
    y = uCellNetConnect(cellHandle, NULL,
#ifdef U_CELL_TEST_CFG_APN
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
#else
                        NULL,
#endif
#ifdef U_CELL_TEST_CFG_USERNAME
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
#else
                        NULL,
#endif
#ifdef U_CELL_TEST_CFG_PASSWORD
                        U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
#else
                        NULL,
#endif
                        keepGoingCallback);
    U_PORT_TEST_ASSERT(y == 0);

    // In case a previous test failed and left CellTime switched on in the
    // module, disable it initially.  Don't check the outcome in case it
    // wasn't actually enabled.
    uCellTimeDisable(cellHandle);

    // Enable CellTime with an invalid mode
    U_PORT_TEST_ASSERT(uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_OFF, true,
                                       0, NULL, NULL) < 0);
    U_PORT_TEST_ASSERT(uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_BEST_EFFORT, true,
                                       0, NULL, NULL) < 0);

    // Now pulse mode, where "GPIO4" of the module should be toggled
    U_TEST_PRINT_LINE("testing CellTime pulse mode...");
    gEventCallback = INT_MIN;
    memset(&gEvent, 0xFF, sizeof(gEvent));
    gEvent.synchronised = false;
    startTimeMs = uPortGetTickTimeMs();
    y = 0;
    // Give this a few goes as sync can fail randomly
    for (size_t x = 0; (y == 0) && !gEvent.synchronised && (x < U_CELL_TIME_TEST_RETRIES + 1); x++) {
        y = uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_PULSE, true, 0,
                            eventCallback, &gEventCallback);
        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
            U_PORT_TEST_ASSERT(y == 0);
            while (!gEvent.synchronised &&
                   (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                uPortTaskBlock(100);
            }
            U_TEST_PRINT_LINE("gEventCallback is %d.", gEventCallback);
            if (gEvent.synchronised) {
                U_PORT_TEST_ASSERT(gEventCallback == 0);
                printAndCheckEvent(&gEvent, true);
#if defined (U_CFG_TEST_PIN_CELL_GPIO4) && (U_CFG_TEST_PIN_CELL_GPIO4 >= 0)
                U_TEST_PRINT_LINE("pin %d of this MCU must be connected to the \"GPIO4\" pin of SARA-R5.",
                                  U_CFG_TEST_PIN_CELL_GPIO4);
                // TODO test that toggling occurred
#endif
            }
            U_PORT_TEST_ASSERT(uCellTimeDisable(cellHandle) == 0);
        } else {
            U_TEST_PRINT_LINE("CellTime not supported, not testing uCellTimeEnable().");
            U_PORT_TEST_ASSERT(y < 0);
        }
    }
    U_PORT_TEST_ASSERT((y < 0) || (gEvent.synchronised));

    // Next one-shot mode, where "GPIO4" of the module should
    // be toggled once and we should get a timestamp URC
    // First run without the callback
    U_TEST_PRINT_LINE("testing CellTime one-shot pulse mode with no callback...");
    gEventCallback = INT_MIN;
    memset(&gEvent, 0xFF, sizeof(gEvent));
    gEvent.synchronised = false;
    startTimeMs = uPortGetTickTimeMs();
    y = 0;
    // Give this a few goes as sync can fail randomly
    for (size_t x = 0; (y == 0) && !gEvent.synchronised && (x < U_CELL_TIME_TEST_RETRIES + 1); x++) {
        y = uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_ONE_SHOT, true, 0,
                            eventCallback, &gEventCallback);
        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
            U_PORT_TEST_ASSERT(y == 0);
            while (!gEvent.synchronised &&
                   (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                uPortTaskBlock(100);
            }
            U_TEST_PRINT_LINE("gEventCallback is %d.", gEventCallback);
            if (gEvent.synchronised) {
                U_PORT_TEST_ASSERT(gEventCallback == 0);
                printAndCheckEvent(&gEvent, true);
#if defined (U_CFG_TEST_PIN_CELL_GPIO4) && (U_CFG_TEST_PIN_CELL_GPIO4 >= 0)
                U_TEST_PRINT_LINE("pin %d of this MCU must be connected to the \"GPIO4\" pin of SARA-R5.",
                                  U_CFG_TEST_PIN_CELL_GPIO4);
                // TODO test that toggling occurred
#endif
            }
            U_PORT_TEST_ASSERT(uCellTimeDisable(cellHandle) == 0);
        } else {
            U_PORT_TEST_ASSERT(y < 0);
        }
    }
    U_PORT_TEST_ASSERT((y < 0) || (gEvent.synchronised));

    // And again with a callback, also this time allowing non-cellular timing,
    // if GNSS is available inside the module of course
    U_TEST_PRINT_LINE("testing CellTime one-shot pulse mode with a callback...");
    gTimeCallback = INT_MIN;
    memset(&gTime, 0xFF, sizeof(gTime));
    y = 0;
    // Give this a few goes as sync can fail randomly
    for (size_t x = 0; (y == 0) && !gEvent.synchronised && (x < U_CELL_TIME_TEST_RETRIES + 1); x++) {
        y = uCellTimeSetCallback(cellHandle, timeCallback, &gTimeCallback);
        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
            U_PORT_TEST_ASSERT(y == 0);
            gEventCallback = INT_MIN;
            memset(&gEvent, 0xFF, sizeof(gEvent));
            gEvent.synchronised = false;
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_ONE_SHOT,
                                               !gnssIsInsideCell, 0,
                                               eventCallback, &gEventCallback) == 0);
            while (!gEvent.synchronised &&
                   (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                uPortTaskBlock(100);
            }
            U_TEST_PRINT_LINE("gEventCallback is %d.", gEventCallback);
            if (gEvent.synchronised) {
                U_PORT_TEST_ASSERT(gEventCallback == 0);
                printAndCheckEvent(&gEvent, !gnssIsInsideCell);
                startTimeMs = uPortGetTickTimeMs();
                while ((gTimeCallback == INT_MIN) &&
                       (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                    uPortTaskBlock(100);
                }
                U_TEST_PRINT_LINE("gTimeCallback is %d.", gTimeCallback);
                U_PORT_TEST_ASSERT(gTimeCallback == 0);
                printAndCheckTime(&gTime);
#if defined (U_CFG_TEST_PIN_CELL_GPIO4) && (U_CFG_TEST_PIN_CELL_GPIO4 >= 0)
                U_TEST_PRINT_LINE("pin %d of this MCU must be connected to the \"GPIO4\" pin of SARA-R5.",
                                  U_CFG_TEST_PIN_CELL_GPIO4);
                // TODO test that toggling occurred
#endif
            }
            U_PORT_TEST_ASSERT(uCellTimeDisable(cellHandle) == 0);
        } else {
            U_PORT_TEST_ASSERT(y < 0);
        }
    }
    U_PORT_TEST_ASSERT((y < 0) || (gEvent.synchronised));

    // Remove the time callback: should always work, even for non-SARA-R5 modules
    U_PORT_TEST_ASSERT(uCellTimeSetCallback(cellHandle, NULL, NULL) == 0);

#if defined(U_CFG_TEST_PIN_CELL_EXT_INT) && (U_CFG_TEST_PIN_CELL_EXT_INT >= 0)
    // Add the callback again and test the external-timestamping mode
    U_TEST_PRINT_LINE("testing CellTime external time-stamp mode...");
    U_TEST_PRINT_LINE("pin %d of this MCU must be connected to the EXT_INT pin of SARA-R5.",
                      U_CFG_TEST_PIN_CELL_EXT_INT);
    gTimeCallback = INT_MIN;
    memset(&gTime, 0xFF, sizeof(gTime));
    y = uCellTimeSetCallback(cellHandle, timeCallback, &gTimeCallback);
    if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        U_PORT_TEST_ASSERT(y == 0);
        gEventCallback = INT_MIN;
        memset(&gEvent, 0xFF, sizeof(gEvent));
        gEvent.synchronised = false;
        startTimeMs = uPortGetTickTimeMs();
        y = 0;
        // Give this a few goes as sync can fail randomly
        for (size_t x = 0; (y == 0) && !gEvent.synchronised && (x < U_CELL_TIME_TEST_RETRIES + 1); x++) {
            U_PORT_TEST_ASSERT(uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_EXT_INT_TIMESTAMP,
                                               !uCellLocGnssInsideCell(cellHandle), 0,
                                               eventCallback, &gEventCallback) == 0);
            while (!gEvent.synchronised &&
                   (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                uPortTaskBlock(100);
            }
            U_TEST_PRINT_LINE("gEventCallback is %d.", gEventCallback);
            if (gEvent.synchronised) {
                U_PORT_TEST_ASSERT(gEventCallback == 0);
                printAndCheckEvent(&gEvent, !gnssIsInsideCell);
                startTimeMs = uPortGetTickTimeMs();
                while ((gTimeCallback == INT_MIN) &&
                       (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                    uPortTaskBlock(100);
                }
                U_TEST_PRINT_LINE("gTimeCallback is %d.", gTimeCallback);
                U_PORT_TEST_ASSERT(gTimeCallback == 0);
                printAndCheckTime(&gTime);
            }
            // Don't disable this time: we do a uCellTimeEnable() in the same
            // mode below and that should work without needing to disable first
        } else {
            U_PORT_TEST_ASSERT(y < 0);
        }
    }
#endif
    U_PORT_TEST_ASSERT((y < 0) || (gEvent.synchronised));

    // Do a deep scan, first with no callback
    U_TEST_PRINT_LINE("performing a deep scan, no callback provided.");
    y = uCellNetDeepScan(cellHandle, NULL, NULL);
    if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        U_TEST_PRINT_LINE("%d cell(s) found.", y);
        U_PORT_TEST_ASSERT(y >= 0);
    } else {
        U_TEST_PRINT_LINE("...maybe not, this is not a SARA-R5.");
        U_PORT_TEST_ASSERT(y < 0);
    }

    if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        // ...and again with a callback, but abort immediately
        U_TEST_PRINT_LINE("adding a callback but aborting the deep scan.");
        gCellInfoCallback = INT_MIN;
        gStopTimeMs = 0;
        y = uCellNetDeepScan(cellHandle, cellInfoCallback, &gpCellInfoList);
        U_TEST_PRINT_LINE("aborted uCellNetDeepScan() returned %d.", y);
        U_PORT_TEST_ASSERT(y < 0);
        U_PORT_TEST_ASSERT(gCellInfoCallback == 0);
        clearCellInfoList(&gpCellInfoList);
    }

    if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        // Now do it properly
        U_TEST_PRINT_LINE("performing a deep scan, with a callback and no abort this time.");
        // Do this a few times as the module can sometimes find nothing
        gCellInfoCallback = INT_MIN;
        for (size_t x = 0; ((gCellInfoCallback == INT_MIN) || (gpCellInfoList == NULL)) && (x < 3); x++) {
            gStopTimeMs = uPortGetTickTimeMs() + (U_CELL_TIME_TEST_DEEP_SCAN_TIMEOUT_SECONDS * 1000);
            y = uCellNetDeepScan(cellHandle, cellInfoCallback, &gpCellInfoList);
            U_TEST_PRINT_LINE("%d cell(s) found on try %d.", y, x + 1);
            if (y > 0) {
                U_PORT_TEST_ASSERT(gCellInfoCallback == 0);
                U_PORT_TEST_ASSERT(gpCellInfoList != NULL);
                pTmp = gpCellInfoList;
                y = 0;
                while (pTmp != NULL) {
                    y++;
                    U_TEST_PRINT_LINE("%d  MCC/MNC          %03d/%03d.", y, pTmp->cell.mcc, pTmp->cell.mnc);
                    U_TEST_PRINT_LINE("%d  TAC              0x%x.", y, pTmp->cell.tac);
                    U_TEST_PRINT_LINE("%d  DL EARFCN        %d.", y, pTmp->cell.earfcnDownlink);
                    U_TEST_PRINT_LINE("%d  UL EARFCN        %d.", y, pTmp->cell.earfcnUplink);
                    U_TEST_PRINT_LINE("%d  logical cell ID  %d.", y, pTmp->cell.cellIdLogical);
                    U_TEST_PRINT_LINE("%d  physical cell ID %d.", y, pTmp->cell.cellIdPhysical);
                    U_TEST_PRINT_LINE("%d  RSRP             %d dBm.", y, pTmp->cell.rsrpDbm);
                    U_TEST_PRINT_LINE("%d  RSRQ             %d dB.", y, pTmp->cell.rsrqDb);
                    U_PORT_TEST_ASSERT(pTmp->cell.mcc >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.mnc >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.tac >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.earfcnDownlink >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.earfcnUplink >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.cellIdLogical >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.cellIdPhysical >= 0);
                    U_PORT_TEST_ASSERT(pTmp->cell.rsrpDbm < 0);
                    // Can't check RSRQ easily as it can be positive or negative
                    pTmp = pTmp->pNext;
                }

#ifndef U_CELL_CFG_SARA_R5_00B
                // Ask to fix to the first of the cells, first without a place
                // to put the timing advance
                U_TEST_PRINT_LINE("syncing to the first cell...");
                U_PORT_TEST_ASSERT(uCellTimeSyncCellEnable(cellHandle, &(gpCellInfoList->cell), NULL) == 0);
                // ..and again with a place to put the timing advance
                y = -1;
                U_PORT_TEST_ASSERT(uCellTimeSyncCellEnable(cellHandle, &(gpCellInfoList->cell), &y) == 0);
                U_TEST_PRINT_LINE("uCellTimeSyncCellEnable() returned timing advance %d.", y);
                // Would check the timing advance here but it doesn't seem to be returned reliably by the module
                if (y >= 0) {
                    // Disable sync, then sync again, this time with the timing advance added
                    U_PORT_TEST_ASSERT(uCellTimeSyncCellDisable(cellHandle) == 0);
                    timingAdvance = y;
                    U_TEST_PRINT_LINE("syncing to the first cell again with timing advance %d...", y);
                    U_PORT_TEST_ASSERT(uCellTimeSyncCellEnable(cellHandle, &(gpCellInfoList->cell), &y) == 0);
                    U_TEST_PRINT_LINE("uCellTimeSyncCellEnable() returned timing advance %d.", y);
                    U_PORT_TEST_ASSERT(y == timingAdvance);
                }

                U_TEST_PRINT_LINE("testing that CellTime works with this cell...");
                gEventCallback = INT_MIN;
                memset(&gEvent, 0xFF, sizeof(gEvent));
                gEvent.synchronised = false;
                startTimeMs = uPortGetTickTimeMs();
                U_PORT_TEST_ASSERT(uCellTimeEnable(cellHandle, U_CELL_TIME_MODE_EXT_INT_TIMESTAMP, true, 0,
                                                   eventCallback, &gEventCallback) == 0);
                while (!gEvent.synchronised &&
                       (uPortGetTickTimeMs() - startTimeMs < U_CELL_TIME_TEST_GUARD_TIME_SECONDS * 1000)) {
                    uPortTaskBlock(100);
                }
                U_TEST_PRINT_LINE("gEventCallback is %d.", gEventCallback);
                U_PORT_TEST_ASSERT(gEventCallback == 0);
                printAndCheckEvent(&gEvent, true);
                U_PORT_TEST_ASSERT(gEvent.cellIdPhysical == gpCellInfoList->cell.cellIdPhysical);
                // The time URC won't be emitted since this is one-shot mode and it has already "shot"
                // Don't disable the callback this time, allow closing to sort it out
#endif
            } else {
                // Free the cell information list, in case we were called
                // but then the module emitted a +CME ERROR
                clearCellInfoList(&gpCellInfoList);
            }
        }

        // Must have found _something_
        U_PORT_TEST_ASSERT(gpCellInfoList != NULL);
        // Free the cell information list
        clearCellInfoList(&gpCellInfoList);
    }

    // Disable cell sync: should always work, even for non-SARA-R5
    U_PORT_TEST_ASSERT(uCellTimeSyncCellDisable(cellHandle) == 0);

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
U_PORT_TEST_FUNCTION("[cellTime]", "cellTimeCleanUp")
{
    int32_t x;

    clearCellInfoList(&gpCellInfoList);

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
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at the"
                          " end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
