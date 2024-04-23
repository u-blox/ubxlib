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
 * @brief Tests for the cellular network API: these should pass on all
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
#include "string.h"    // memset(), strncpy()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
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

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_NET_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

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

/** The last status value passed to registerCallback.
 */
static uCellNetStatus_t gLastNetStatus = U_CELL_NET_STATUS_UNKNOWN;

/** Flag to show that connectCallback has been called.
 */
static bool gConnectCallbackCalled = false;

/** Whether gConnectCallbackCalled has been called with isConnected
 * true.
 */
static bool gHasBeenConnected = false;

/** A variable to track errors in the callbacks.
 */
static int32_t gCallbackErrorCode = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for certain cellular network processes.
static bool keepGoingCallback(uDeviceHandle_t cellHandle)
{
    bool keepGoing = true;

    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorCode = 1;
    }

    if (uTimeoutExpiredMs(gTimeoutStop.timeoutStart,
                          gTimeoutStop.durationMs)) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback for registration status.
static void registerCallback(uCellNetRegDomain_t domain,
                             uCellNetStatus_t status,
                             void *pParameter)
{
    // Note: not using asserts here as, when they go
    // off, the seem to cause stack overruns
    if (domain >= U_CELL_NET_REG_DOMAIN_MAX_NUM) {
        gCallbackErrorCode = 2;
    }
    if (status <= U_CELL_NET_STATUS_UNKNOWN) {
        gCallbackErrorCode = 3;
    }
    if (status >= U_CELL_NET_STATUS_MAX_NUM) {
        gCallbackErrorCode = 4;
    }

    if (pParameter == NULL) {
        gCallbackErrorCode = 5;
    } else {
        if (strcmp((char *) pParameter, "Boo!") != 0) {
            gCallbackErrorCode = 6;
        }
    }

    if (domain == U_CELL_NET_REG_DOMAIN_PS) {
        gLastNetStatus = status;
    }
}

// Callback for base station connection status.
static void connectCallback(bool isConnected, void *pParameter)
{
    // Note: not using asserts here as, when they go
    // off, the seem to cause stack overruns
    if (pParameter == NULL) {
        gCallbackErrorCode = 7;
    } else {
        if (strcmp((char *) pParameter, "Bah!") != 0) {
            gCallbackErrorCode = 8;
        }
    }

    gConnectCallbackCalled = true;
    if (isConnected) {
        gHasBeenConnected = true;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test connecting and disconnecting and most things in-between.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellNet]", "cellNetConnectDisconnectPlus")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    uCellNetStatus_t status;
    uCellNetRat_t rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    int32_t x;
    char buffer[U_CELL_NET_IP_ADDRESS_SIZE * 2];
    int32_t mcc = 0;
    int32_t mnc = 0;
    char parameter1[5]; // enough room for "Boo!"
    char parameter2[5]; // enough room for "Bah!"
    int32_t resourceCount;
    int32_t networkCause;

    strncpy(parameter1, "Boo!", sizeof(parameter1));
    strncpy(parameter2, "Bah!", sizeof(parameter2));

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Set a registration status calback
    U_PORT_TEST_ASSERT(uCellNetSetRegistrationStatusCallback(cellHandle,
                                                             registerCallback,
                                                             (void *) parameter1) == 0);

    // Set a connection status callback, if possible
    gCallbackErrorCode = 0;
    x = uCellNetSetBaseStationConnectionStatusCallback(cellHandle,
                                                       connectCallback,
                                                       (void *) parameter2);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CSCON)) {
        U_PORT_TEST_ASSERT(x == 0);
    } else {
        U_PORT_TEST_ASSERT(x < 0);
    }

    U_PORT_TEST_ASSERT(gLastNetStatus == U_CELL_NET_STATUS_UNKNOWN);

    // Read the authentication mode for PDP contexts
    x = uCellNetGetAuthenticationMode(cellHandle);
    U_PORT_TEST_ASSERT(x >= 0);
    U_PORT_TEST_ASSERT(x < U_CELL_NET_AUTHENTICATION_MODE_MAX_NUM);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_AUTHENTICATION_MODE_AUTOMATIC)) {
        U_PORT_TEST_ASSERT(x == U_CELL_NET_AUTHENTICATION_MODE_AUTOMATIC);
    } else {
        U_PORT_TEST_ASSERT(x == U_CELL_NET_AUTHENTICATION_MODE_NOT_SET);
    }

    // Try setting all of the permitted authentication modes
    U_PORT_TEST_ASSERT(uCellNetSetAuthenticationMode(cellHandle,
                                                     U_CELL_NET_AUTHENTICATION_MODE_PAP) == 0);
    U_PORT_TEST_ASSERT(uCellNetGetAuthenticationMode(cellHandle) == U_CELL_NET_AUTHENTICATION_MODE_PAP);
    U_PORT_TEST_ASSERT(uCellNetSetAuthenticationMode(cellHandle,
                                                     U_CELL_NET_AUTHENTICATION_MODE_CHAP) == 0);
    U_PORT_TEST_ASSERT(uCellNetGetAuthenticationMode(cellHandle) ==
                       U_CELL_NET_AUTHENTICATION_MODE_CHAP);
    x = uCellNetSetAuthenticationMode(cellHandle, U_CELL_NET_AUTHENTICATION_MODE_AUTOMATIC);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_AUTHENTICATION_MODE_AUTOMATIC)) {
        U_PORT_TEST_ASSERT(x == 0);
        U_PORT_TEST_ASSERT(uCellNetGetAuthenticationMode(cellHandle) ==
                           U_CELL_NET_AUTHENTICATION_MODE_AUTOMATIC);
    } else {
        U_PORT_TEST_ASSERT(x < 0);
    }

    // Get the network cause
    networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
    U_TEST_PRINT_LINE("network cause is %d.", networkCause);
    U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                       (networkCause == 0));

    // Connect with a very short time-out to show that aborts work
    U_TEST_PRINT_LINE("testing abort of connection attempt due to timeout.");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = 1000;
    x = uCellNetConnect(cellHandle, NULL,
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
    U_PORT_TEST_ASSERT(x < 0);

    // Now connect with a sensible timeout
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
    x = uCellNetConnect(cellHandle, NULL,
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
    U_PORT_TEST_ASSERT (x == 0);

    // Check that we're registered
    U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));

    // Check that the status is registered
    status = uCellNetGetNetworkStatus(cellHandle, U_CELL_NET_REG_DOMAIN_PS);
    U_TEST_PRINT_LINE("uCellNetGetNetworkStatus() returned %d.", status);
    U_PORT_TEST_ASSERT((status == U_CELL_NET_STATUS_REGISTERED_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_ROAMING) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING));
    U_TEST_PRINT_LINE("gLastNetStatus is %d.", gLastNetStatus);
    U_PORT_TEST_ASSERT(gLastNetStatus == status);

    // Check that the RAT we're registered on
    rat = uCellNetGetActiveRat(cellHandle);
    U_PORT_TEST_ASSERT((rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
                       (rat < U_CELL_NET_RAT_MAX_NUM));

    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CSCON)) {
        // Check that the connect status callback has been called.
        U_PORT_TEST_ASSERT(gConnectCallbackCalled);
        U_PORT_TEST_ASSERT(gHasBeenConnected);
        U_TEST_PRINT_LINE("gCallbackErrorCode is %d.", gCallbackErrorCode);
        U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);
    } else {
        U_PORT_TEST_ASSERT(!gConnectCallbackCalled);
        U_PORT_TEST_ASSERT(!gHasBeenConnected);
    }

    // Get the network cause
    networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
    U_TEST_PRINT_LINE("network cause is now %d.", networkCause);
    U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                       (networkCause == 0));

    // Check that we have an active RAT
    // Note: can't check that it's the right one for this module
    // as we only keep the configurable RATs which are a subset of the
    // available RATs (e.g. you can configure UTRAN but not HSUPA yet
    // you might be on a HSUPA capable network when configured for UTRAN).
    U_PORT_TEST_ASSERT(uCellNetGetActiveRat(cellHandle) > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED);

    // Get the operator string with a short buffer and check
    // for overrun
    memset(buffer, '|', sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellNetGetOperatorStr(cellHandle, buffer, 2) == 1);
    U_PORT_TEST_ASSERT(strlen(buffer) == 1);
    U_PORT_TEST_ASSERT(buffer[2] == '|');

    // Get the operator string into a proper buffer length
    memset(buffer, '|', sizeof(buffer));
    x = uCellNetGetOperatorStr(cellHandle, buffer, sizeof(buffer));
    U_PORT_TEST_ASSERT(x > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == x);

    // Get the MCC/MNC
    U_PORT_TEST_ASSERT(uCellNetGetMccMnc(cellHandle, &mcc, &mnc) == 0);
    U_PORT_TEST_ASSERT(mcc > 0);
    U_PORT_TEST_ASSERT(mnc > 0);

    // Get the IP address with a NULL buffer
    memset(buffer, '|', sizeof(buffer));
    U_PORT_TEST_ASSERT(uCellNetGetIpAddressStr(cellHandle, NULL) > 0);
    // Get the IP address with a proper buffer and check length
    x = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(x > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == x);

    if (pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
        // Get the DNS addresses with a NULL buffer
        memset(buffer, '|', sizeof(buffer));
        U_PORT_TEST_ASSERT(uCellNetGetDnsStr(cellHandle, false, NULL, NULL) == 0);
        // Get the DNS addresses with a proper buffer
        U_PORT_TEST_ASSERT(uCellNetGetDnsStr(cellHandle, false,
                                             buffer,
                                             buffer + U_CELL_NET_IP_ADDRESS_SIZE) == 0);
        x = strlen(buffer);
        U_PORT_TEST_ASSERT(x > 0);
        U_PORT_TEST_ASSERT(x < U_CELL_NET_IP_ADDRESS_SIZE);
        // There may not be a secondary IP address so can't check that

        // Get the APN with a short buffer and check for overrun
        memset(buffer, '|', sizeof(buffer));
        U_PORT_TEST_ASSERT(uCellNetGetApnStr(cellHandle, buffer, 2) == 1);
        U_PORT_TEST_ASSERT(strlen(buffer) == 1);
        U_PORT_TEST_ASSERT(buffer[2] == '|');

        // Get the APN with a proper buffer length
        memset(buffer, '|', sizeof(buffer));
        x = uCellNetGetApnStr(cellHandle, buffer, sizeof(buffer));
        U_PORT_TEST_ASSERT(x > 0);
        U_PORT_TEST_ASSERT(strlen(buffer) == x);
    } else {
        U_TEST_PRINT_LINE("reading DNS address and APN not supported, not testing them.");
    }

    // Check that we can connect again with the same APN,
    // should return pretty much immediately, unless this is
    // LENA-R8 which does not support reading the current APN
    // and hence can't tell if we're on the right one or not,
    // hence the timeout is larger than the 5 seconds it used to be
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = 60000;
    U_TEST_PRINT_LINE("connecting again with same APN...");
    x = uCellNetConnect(cellHandle, NULL,
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
    U_PORT_TEST_ASSERT(x == 0);

    // Get the IP address to check that we're still there
    memset(buffer, '|', sizeof(buffer));
    x = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(x > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == x);

#ifndef U_CELL_TEST_NO_INVALID_APN
    // The compilation switch is for live networks which may just ignore
    // invalid APNs and employ the correct default, resulting in successful
    // registration
    if (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_U201) {
        // Don't try using an invalid APN with SARA-U201 as it
        // upsets it too much
        U_TEST_PRINT_LINE("connecting with different (invalid) APN...");
        gTimeoutStop.timeoutStart = uTimeoutStart();
        gTimeoutStop.durationMs = 10000;
        x = uCellNetConnect(cellHandle, NULL, "flibble",
# ifdef U_CELL_TEST_CFG_USERNAME
                            U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
# else
                            NULL,
# endif
# ifdef U_CELL_TEST_CFG_PASSWORD
                            U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
# else
                            NULL,
# endif
                            keepGoingCallback);
        U_PORT_TEST_ASSERT(x < 0);
        // Get the IP address: should now have none since the above
        // will have deactivated what we had and been unable to
        // activate the new one
        U_PORT_TEST_ASSERT(uCellNetGetIpAddressStr(cellHandle, buffer) < 0);

        // Get the network cause
        networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
        U_TEST_PRINT_LINE("network cause with incorrect APN is %d.", networkCause);
        U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                           (networkCause > 0));
    }
#endif

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Make sure the registration status callback doesn't say we are
    // registered
    U_PORT_TEST_ASSERT((gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_HOME) &&
                       (gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_ROAMING) &&
                       (gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_HOME) &&
                       (gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_ROAMING) &&
                       (gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) &&
                       (gLastNetStatus != U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING));

    // Note: can't check that gHasBeenConnected is false here as the RRC
    // connection may not yet be closed.
    U_TEST_PRINT_LINE("gCallbackErrorCode is %d.", gCallbackErrorCode);
    U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test scanning and doing registration/activation/deactivation
 * separately.
 */
U_PORT_TEST_FUNCTION("[cellNet]", "cellNetScanRegActDeact")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    uCellNetStatus_t status;
    char buffer[U_CELL_NET_IP_ADDRESS_SIZE];
    char mccMnc[U_CELL_NET_MCC_MNC_LENGTH_BYTES];
    int32_t mcc = 0;
    int32_t mnc = 0;
    int32_t y = 0;
    uCellNetRat_t rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    int32_t resourceCount;
    int32_t networkCause;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Scan for networks properly
    // Have seen this fail on some occasions; sometimes the module
    // can be scanning already, internally, and won't respond to a
    // user request, so give it several goes
    for (size_t x = 5; (x > 0) && (y <= 0); x--) {
        U_TEST_PRINT_LINE("scanning for networks...");
        gTimeoutStop.timeoutStart = uTimeoutStart();
        gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
        memset(buffer, 0, sizeof(buffer));
        memset(mccMnc, 0, sizeof(mccMnc));
        for (int32_t z = uCellNetScanGetFirst(cellHandle, buffer,
                                              sizeof(buffer), mccMnc, &rat,
                                              keepGoingCallback);
             z >= 0;
             z = uCellNetScanGetNext(cellHandle, buffer, sizeof(buffer), mccMnc, &rat)) {
            U_PORT_TEST_ASSERT(strlen(mccMnc) > 0);
            // Might not be a network name (this is the case for 001/01)
            // so don't check content of buffer
            U_PORT_TEST_ASSERT((rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
                               (rat < U_CELL_NET_RAT_MAX_NUM));
            y++;
            U_TEST_PRINT_LINE("found \"%s\", MCC/MNC %s (%s).", buffer, mccMnc,
                              pUCellTestPrivateRatStr(rat));
            memset(buffer, 0, sizeof(buffer));
            memset(mccMnc, 0, sizeof(mccMnc));
            rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
        }
        if (y == 0) {
            // Give us something to search for in the log
            U_TEST_PRINT_LINE("*** WARNING *** RETRY SCAN.");
            uPortTaskBlock(5000);
        }
    }

    U_TEST_PRINT_LINE("%d network(s) found in total.", y);
    // Must be at least one, can't guarantee more than that
    U_PORT_TEST_ASSERT(y > 0);

    // Note: uCellNetDeepScan() is tested with the uCellTime API
    // since that is where the results can be used

    // Register with a very short time-out to show that aborts work
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = 1000;
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) < 0);

    // Now register with a sensible timeout
    U_TEST_PRINT_LINE("registering...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) == 0);

    // Check that we're registered
    U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));

    // Check that the status is registered
    status = uCellNetGetNetworkStatus(cellHandle, U_CELL_NET_REG_DOMAIN_PS);
    U_PORT_TEST_ASSERT((status == U_CELL_NET_STATUS_REGISTERED_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_ROAMING) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING));

    // Get the MCC/MNC and store it in the mccMnc buffer
    U_PORT_TEST_ASSERT(uCellNetGetMccMnc(cellHandle, &mcc, &mnc) == 0);
    U_PORT_TEST_ASSERT(mcc > 0);
    U_PORT_TEST_ASSERT(mnc > 0);
    snprintf(mccMnc, sizeof(mccMnc), "%03d%02d", (uint8_t) mcc, (uint8_t) mnc);

    // Register again: should come back with no error pretty much straight away
    U_TEST_PRINT_LINE("registering while already registered...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = 10000;
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) == 0);

    // Now activate a PDP context
    U_TEST_PRINT_LINE("activating context...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONTEXT_ACTIVATION_TIMEOUT_SECONDS * 1000;
    y = uCellNetActivate(cellHandle,
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

    // Get the IP address
    memset(buffer, '|', sizeof(buffer));
    y = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(y > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == y);

    // Get the network cause
    networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
    U_TEST_PRINT_LINE("network cause is %d.", networkCause);
    U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                       (networkCause == 0));

    // Deactivate the context
    rat = uCellNetGetActiveRat(cellHandle);
    U_TEST_PRINT_LINE("deactivating context...");
    U_PORT_TEST_ASSERT(uCellNetDeactivate(cellHandle, NULL) == 0);
    if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat) ||
        U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType)) {
        // If we were originally on LTE, or if this is a SARA-R4
        // we will now be deregistered, so register again
        gTimeoutStop.timeoutStart = uTimeoutStart();
        gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
        U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, NULL, keepGoingCallback) == 0);
    } else {
        // Get the IP address again, should be gone in the non-LTE/R4 case
        U_PORT_TEST_ASSERT(uCellNetGetIpAddressStr(cellHandle, buffer) < 0);
    }

    // Check that we can activate the PDP context again
    U_TEST_PRINT_LINE("activating context...");
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONTEXT_ACTIVATION_TIMEOUT_SECONDS * 1000;
    y = uCellNetActivate(cellHandle,
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

    // Get the IP address
    memset(buffer, '|', sizeof(buffer));
    y = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(y > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == y);

    // Check that we can activate the PDP context again with
    // the same APN
    U_TEST_PRINT_LINE("activating context again with same APN...");
    // This timer used to be 10 seconds but on LENA-R8 there is
    // no way to read the current APN and hence the check that
    // uCellNetActivate() performs to see if the current context
    // is fine will fail and so we will detach and reattach here,
    // which takes longer
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = 60000;
    y = uCellNetActivate(cellHandle,
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

    // Get the IP address
    memset(buffer, '|', sizeof(buffer));
    y = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(y > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == y);

#ifndef U_CELL_TEST_NO_INVALID_APN
    // The compilation switch is for live networks which may just ignore
    // invalid APNs and employ the correct default, resulting in successful
    // registration
    if (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_U201) {
        // Try to activate a PDP context with a different, invalid, APN
        // Don't do this for SARA-U201 as it upsets it rather a lot
        U_TEST_PRINT_LINE("activating context with different (invalid) APN...");
        rat = uCellNetGetActiveRat(cellHandle);
        gTimeoutStop.timeoutStart = uTimeoutStart();
        gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONTEXT_ACTIVATION_TIMEOUT_SECONDS * 1000;
        y = uCellNetActivate(cellHandle, "flibble",
# ifdef U_CELL_TEST_CFG_USERNAME
                             U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
# else
                             NULL,
# endif
# ifdef U_CELL_TEST_CFG_PASSWORD
                             U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
# else
                             NULL,
# endif
                             keepGoingCallback);
        U_PORT_TEST_ASSERT(y < 0);
        // Get the IP address.
        U_PORT_TEST_ASSERT(uCellNetGetIpAddressStr(cellHandle, buffer) < 0);

        // Get the network cause
        networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
        U_TEST_PRINT_LINE("network cause with incorrect APN is %d.", networkCause);
        U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                           (networkCause > 0));
    }
#endif

    // Disconnect
    U_TEST_PRINT_LINE("disconnecting...");
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Connect to the network using manual selection
    // Have seen this fail on rare occasions, give it two goes
    y = -1;
    for (size_t x = 2; (x > 0) && (y < 0); x--) {
        U_TEST_PRINT_LINE("connecting manually to network %s...", mccMnc);
        gTimeoutStop.timeoutStart = uTimeoutStart();
        gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
        y = uCellNetConnect(cellHandle, mccMnc,
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
        if (y < 0) {
            // Give us something to search for in the log
            U_TEST_PRINT_LINE("*** WARNING *** RETRY MANUAL.");
        }
    }
    U_PORT_TEST_ASSERT(y == 0);

    // Check that we're registered
    U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));

    // Check that the status is registered
    status = uCellNetGetNetworkStatus(cellHandle, U_CELL_NET_REG_DOMAIN_PS);
    U_PORT_TEST_ASSERT((status == U_CELL_NET_STATUS_REGISTERED_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_ROAMING) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING));

    // Get the IP address
    memset(buffer, '|', sizeof(buffer));
    y = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(y > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == y);

    // Get the network cause
    networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
    U_TEST_PRINT_LINE("network cause is now %d.", networkCause);
    U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                       (networkCause == 0));

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Check that we're not registered
    U_PORT_TEST_ASSERT(!uCellNetIsRegistered(cellHandle));

    // Get the IP address again
    U_PORT_TEST_ASSERT(uCellNetGetIpAddressStr(cellHandle, buffer) < 0);

    // Now register with manual network selection
    U_TEST_PRINT_LINE("registering manually on network %s...", mccMnc);
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
    U_PORT_TEST_ASSERT(uCellNetRegister(cellHandle, mccMnc,
                                        keepGoingCallback) == 0);

    // Check that we're registered
    U_PORT_TEST_ASSERT(uCellNetIsRegistered(cellHandle));

    // Check that the status is registered
    status = uCellNetGetNetworkStatus(cellHandle, U_CELL_NET_REG_DOMAIN_PS);
    U_PORT_TEST_ASSERT((status == U_CELL_NET_STATUS_REGISTERED_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_ROAMING) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) ||
                       (status == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING));

    // Now activate a PDP context
    gTimeoutStop.timeoutStart = uTimeoutStart();
    gTimeoutStop.durationMs = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000;
    y = uCellNetActivate(cellHandle,
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

    // Get the IP address
    memset(buffer, '|', sizeof(buffer));
    y = uCellNetGetIpAddressStr(cellHandle, buffer);
    U_PORT_TEST_ASSERT(y > 0);
    U_PORT_TEST_ASSERT(strlen(buffer) == y);

    // Get the network cause
    networkCause = uCellNetGetLastEmmRejectCause(cellHandle);
    U_TEST_PRINT_LINE("network cause is finally %d.", networkCause);
    U_PORT_TEST_ASSERT((networkCause == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) ||
                       (networkCause == 0));

    // Disconnect
    U_TEST_PRINT_LINE("disconnecting...");
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

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellNet]", "cellNetCleanUp")
{
    uCellTestPrivateCleanup(&gHandles);
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
