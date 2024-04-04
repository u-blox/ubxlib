/*
 * Copyright 2019-2024 u-blox
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
 * @brief Tests for the cellular info API: these should pass on all
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

#include "u_test_util_resource_check.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_info.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_INFO_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_INFO_TEST_MIN_TIME
/** A minimum value for time to test against (21 July 2021 13:40:36).
 */
# define U_CELL_INFO_TEST_MIN_TIME 1626874836
#endif

#ifndef U_CELL_INFO_TEST_TIME_MARGIN_SECONDS
/** The permitted margin between reading time several times during
 * testing, in seconds.
 */
# define U_CELL_INFO_TEST_TIME_MARGIN_SECONDS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static uTimeoutStop_t gTimeoutStop;

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

    if (uTimeoutExpiredMs(gTimeoutStop.timeoutStart,
                          gTimeoutStop.durationMs)) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test all the info functions that read static data.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoImeiEtc")
{
    uDeviceHandle_t cellHandle;
    char buffer[64];
    int32_t bytesRead;
    int32_t resourceCount;
#if defined(U_CFG_APP_PIN_CELL_RTS_GET) || defined(U_CFG_APP_PIN_CELL_CTS_GET)
    bool isEnabled;
#endif

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    U_TEST_PRINT_LINE("getting and checking IMEI...");
    memset(buffer, 0, sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < U_CELL_INFO_IMEI_SIZE) {
            U_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            U_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    U_TEST_PRINT_LINE("getting and checking manufacturer string...");
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
    U_TEST_PRINT_LINE("getting and checking model string...");
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
    U_TEST_PRINT_LINE("getting and checking firmware version string...");
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

    U_TEST_PRINT_LINE("getting and checking IMSI...");
    memset(buffer, 0, sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellInfoGetImsi(cellHandle, buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < U_CELL_INFO_IMSI_SIZE) {
            U_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            U_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    U_TEST_PRINT_LINE("getting and checking ICCID...");
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

#ifdef U_CFG_APP_PIN_CELL_RTS_GET
    U_TEST_PRINT_LINE("checking RTS...");
    isEnabled = uCellInfoIsRtsFlowControlEnabled(cellHandle);
# ifdef U_CELL_TEST_MUX_ALWAYS
    // Flow control is always enabled for CMUX
    U_PORT_TEST_ASSERT(isEnabled);
# else
#  if U_CFG_APP_PIN_CELL_RTS_GET >= 0
    U_PORT_TEST_ASSERT(isEnabled);
#  else
    U_PORT_TEST_ASSERT(!isEnabled);
#  endif
# endif
#endif

#ifdef U_CFG_APP_PIN_CELL_CTS_GET
    U_TEST_PRINT_LINE("checking CTS...");
    isEnabled = uCellInfoIsCtsFlowControlEnabled(cellHandle);
# ifdef U_CELL_TEST_MUX_ALWAYS
    // Flow control is always enabled for CMUX
    U_PORT_TEST_ASSERT(isEnabled);
# else
#  if U_CFG_APP_PIN_CELL_CTS_GET >= 0
    U_PORT_TEST_ASSERT(isEnabled);
#  else
    U_PORT_TEST_ASSERT(!isEnabled);
#  endif
# endif
#endif

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test all the radio parameters functions.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoRadioParameters")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t x;
    int32_t snrDb;
    size_t count;
    int32_t resourceCount;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(gHandles.cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if (pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
        U_TEST_PRINT_LINE("checking values before a refresh (should return errors)...");
        U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0x7FFFFFFF);
        U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle, &snrDb) != 0);
        U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdLogical(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdPhysical(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) == -1);

        U_TEST_PRINT_LINE("checking values after a refresh but before"
                          " network registration (should return errors)...");
        U_PORT_TEST_ASSERT(uCellInfoRefreshRadioParameters(cellHandle) != 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0x7FFFFFFF);
        U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle, &snrDb) != 0);
        U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdLogical(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdPhysical(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) == -1);
    } else {
        U_TEST_PRINT_LINE("LENA-R8 only supports RSSI and logical cell ID, only testing them.");
        U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0x7FFFFFFF);
        U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle, &snrDb) == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
        U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) ==  -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdLogical(cellHandle) == -1);
        U_PORT_TEST_ASSERT(uCellInfoGetCellIdPhysical(cellHandle) ==  (int32_t)
                           U_ERROR_COMMON_NOT_SUPPORTED);
        U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) ==  (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
    }

    U_TEST_PRINT_LINE("checking values after registration...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
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
        if (pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
            // Only get these with AT+UCGED on EUTRAN and not at all with LENA-R8
            U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) < 0);
            U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) != 0x7FFFFFFF);
            U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) >= 0);
            U_PORT_TEST_ASSERT(uCellInfoGetCellIdPhysical(cellHandle) >= 0);
            U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) >= 0);
        } else {
            U_PORT_TEST_ASSERT(uCellInfoGetRsrpDbm(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellInfoGetRsrqDb(cellHandle) == 0x7FFFFFFF);
            U_PORT_TEST_ASSERT(uCellInfoGetCellId(cellHandle) >= 0);
            U_PORT_TEST_ASSERT(uCellInfoGetCellIdPhysical(cellHandle) ==  (int32_t)
                               U_ERROR_COMMON_NOT_SUPPORTED);
            U_PORT_TEST_ASSERT(uCellInfoGetEarfcn(cellHandle) == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
        }
    }
    // ...however RSSI can take a long time to
    // get so keep trying if it has not arrived
    for (count = 10; (uCellInfoGetRssiDbm(cellHandle) == 0) &&
         (count > 0); count--) {
        uCellInfoRefreshRadioParameters(cellHandle);
        uPortTaskBlock(5000);
    }
    U_PORT_TEST_ASSERT(uCellInfoGetRssiDbm(cellHandle) < 0);
    U_PORT_TEST_ASSERT(uCellInfoGetCellIdLogical(cellHandle) >= 0);
    if (pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
        if (U_CELL_PRIVATE_RAT_IS_EUTRAN(uCellNetGetActiveRat(cellHandle))) {
            // Only get this if we have RSRP as well
            x = uCellInfoGetSnrDb(cellHandle, &snrDb);
            if (x == 0) {
                U_TEST_PRINT_LINE("SNR is %d dB.", snrDb);
            }
            U_PORT_TEST_ASSERT((x == 0) || (x == (int32_t) U_CELL_ERROR_VALUE_OUT_OF_RANGE) ||
                               (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
        }
    } else {
        U_PORT_TEST_ASSERT(uCellInfoGetSnrDb(cellHandle,
                                             &snrDb) ==  (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
    }

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test fetching the time.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoTime")
{
    uDeviceHandle_t cellHandle;
    int64_t timeUtc;
    int64_t timeLocal;
    int32_t timeZoneOffsetSeconds = 0;
    int64_t x;
    int32_t resourceCount;
    char buffer[32] = {0};

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    U_TEST_PRINT_LINE("registering to check the time...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) == 0);

    U_TEST_PRINT_LINE("fetching the UTC time...");
    timeUtc = uCellInfoGetTimeUtc(cellHandle);
    U_TEST_PRINT_LINE("UTC time is %d.", (int32_t) timeUtc);
    U_PORT_TEST_ASSERT(timeUtc > U_CELL_INFO_TEST_MIN_TIME);

    U_TEST_PRINT_LINE("fetching the time string...");
    U_PORT_TEST_ASSERT(uCellInfoGetTimeUtcStr(cellHandle, buffer, sizeof(buffer)) >= 0);
    U_TEST_PRINT_LINE("UTC time: %s.", buffer);

    U_TEST_PRINT_LINE("fetching the local time without timezone...");
    x = uCellInfoGetTime(cellHandle, NULL);
    U_TEST_PRINT_LINE("local time is %d.", (int32_t) x);
    U_PORT_TEST_ASSERT(x > U_CELL_INFO_TEST_MIN_TIME);

    U_TEST_PRINT_LINE("...and again with timezone.");
    timeLocal = uCellInfoGetTime(cellHandle, &timeZoneOffsetSeconds);
    U_TEST_PRINT_LINE("local time is %d, timezone is %d, therefore UTC time is %d.",
                      (int32_t) timeLocal, timeZoneOffsetSeconds,
                      (int32_t) (timeLocal - timeZoneOffsetSeconds));
    U_PORT_TEST_ASSERT(timeLocal - x < U_CELL_INFO_TEST_TIME_MARGIN_SECONDS);
    U_PORT_TEST_ASSERT(timeLocal - timeZoneOffsetSeconds >= timeUtc);
    U_PORT_TEST_ASSERT((timeLocal - timeZoneOffsetSeconds) - timeUtc <
                       U_CELL_INFO_TEST_TIME_MARGIN_SECONDS);

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
    uPortTaskBlock(1000);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellInfo]", "cellInfoCleanUp")
{
    uCellTestPrivateCleanup(&gHandles);
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
