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
 * @brief Tests for the cellular info API: these should pass on all
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
#include "string.h"    // memset()

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

#include "u_cell.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_info.h"

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
static int64_t gStopTimeMs;

/** Handles.
 */
static uCellTestPrivate_t gHandles = {-1};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test all the info functions that read static data.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoImeiEtc")
{
    int32_t cellHandle;
    char buffer[64];
    int32_t bytesRead;

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    uPortLog("U_CELL_INFO_TEST: getting and checking IMEI...\n");
    memset(buffer, 0, sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < U_CELL_INFO_IMEI_SIZE) {
            U_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            U_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    uPortLog("U_CELL_INFO_TEST: getting and checking manufacturer string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetManufacturerStr(cellHandle, buffer, 1);
    U_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        U_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetManufacturerStr(cellHandle, buffer, sizeof(buffer));
    U_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                       (bytesRead == strlen(buffer)));
    uPortLog("U_CELL_INFO_TEST: getting and checking model string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetModelStr(cellHandle, buffer, 1);
    U_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        U_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetModelStr(cellHandle, buffer, sizeof(buffer));
    U_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                       (bytesRead == strlen(buffer)));
    uPortLog("U_CELL_INFO_TEST: getting and checking firmware version string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetFirmwareVersionStr(cellHandle, buffer, 1);
    U_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        U_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetFirmwareVersionStr(cellHandle, buffer, sizeof(buffer));
    U_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                       (bytesRead == strlen(buffer)));

    uPortLog("U_CELL_INFO_TEST: getting and checking IMSI...\n");
    memset(buffer, 0, sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellInfoGetImsi(cellHandle, buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < U_CELL_INFO_IMEI_SIZE) {
            U_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            U_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    uPortLog("U_CELL_INFO_TEST: getting and checking ICCID...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    memset(buffer, 0, sizeof(buffer));
    bytesRead = uCellInfoGetIccidStr(cellHandle, buffer, 1);
    U_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        U_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    memset(buffer, 0, sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellInfoGetIccidStr(cellHandle, buffer,
                                            sizeof(buffer)) >= 0);
    U_PORT_TEST_ASSERT(strlen(buffer) <= U_CELL_INFO_ICCID_BUFFER_SIZE);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);
}

/** Test all the radio parameters functions.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoRadioParameters")
{
    int32_t cellHandle;
    int32_t x;
    int32_t snrDb;
    size_t count;

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    uPortLog("U_CELL_INFO_TEST: checking values before a refresh (should"
             " return errors)...\n");
    U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle, &snrDb) != 0);
    U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) == -1);
    U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) == -1);

    uPortLog("U_CELL_INFO_TEST: checking values after a refresh but before"
             " network registration (should return errors)...\n");
    U_PORT_TEST_ASSERT(uCellInfoRefreshRadioParameters(cellHandle) != 0);
    U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0);
    U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle, &snrDb) != 0);
    U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) == -1);
    U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) == -1);

    uPortLog("U_CELL_INFO_TEST: checking values after registration...\n");
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_PRIVATE_CONNECT_TIMEOUT_SECONDS * 1000);
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) == 0);
    U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));

    // Attempt this a number of times as it can return a temporary
    // "operation not allowed" error
    for (count = 10; (uCellInfoRefreshRadioParameters(cellHandle) != 0) &&
         (count > 0); count--) {
        uPortTaskBlock(1000);
    }
    U_PORT_TEST_ASSERT(count > 0);
    // Should now have everything
    if (U_CELL_PRIVATE_RAT_IS_EUTRAN(uCellNetGetActiveRat(cellHandle))) {
        // Only get these with AT+UCGED on EUTRAN
        U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) < 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) < 0);
        U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) >= 0);
        U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) >= 0);
    }
    // ...however RSSI can take a long time to
    // get so keep trying if it has not arrived
    for (count = 10; (uCellInfoGetRssiDbm(cellHandle) == 0) &&
         (count > 0); count--) {
        uCellInfoRefreshRadioParameters(cellHandle);
        uPortTaskBlock(5000);
    }
    U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) < 0);
    if (U_CELL_PRIVATE_RAT_IS_EUTRAN(uCellNetGetActiveRat(cellHandle))) {
        // Only get this if we have RSRP a well
        x = uCellInfoGetSnrDb(cellHandle, &snrDb);
        if (x == 0) {
            uPortLog("U_CELL_INFO_TEST: SNR is %d dB.\n", snrDb);
        }
        U_PORT_TEST_ASSERT((x == 0) || (x == U_CELL_ERROR_VALUE_OUT_OF_RANGE));
    }

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoCleanUp")
{
    uCellTestPrivateCleanup(&gHandles);
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
