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
 * @brief Tests for the cellular power API: these should pass on all
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
#include "string.h"    // For memset()/memcmp()

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

#include "u_sock.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_info.h"  // In order to fetch the IMEI as a test command for power saving
#include "u_cell_sock.h"  // So that we can transfer some data during E-DRX tests

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

#include "u_sock_test_shared_cfg.h"   // For some of the test macros

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_PWR_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS
/** The active time to use during 3GPP power saving testing,
 * a value known to work with the Nutaq test network we use in our
 * test system.
 */
# define U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS 10
#endif

#ifndef U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS
/** The periodic wake-up to use during 3GPP power saving testing,
 * a value known to work with the Nutaq test network we use in our
 * test system.
 */
# define U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS 300
#endif

#ifndef U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS
/** How long to wait for the module to return to idle, 10 seconds for the
 * RRC connection to drop on the Nutaq box we use in testing, plus
 * a little bit of margin to be sure.
 */
#define U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS 12
#endif

#ifndef U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS
/** How much longer to wait than the active time for a module to
 * actually go to sleep after the RRC disconnect.
 */
#define U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS 10
#endif

#ifndef U_CELL_PWR_TEST_EDRX_SECONDS
/** The E-DRX time to use during testing in seconds.
 */
# define U_CELL_PWR_TEST_EDRX_SECONDS 10
#endif

#ifndef U_CELL_PWR_TEST_EDRX_MARGIN_SECONDS
/** How much longer to wait then the E-DRX timer for a
 * module to actually go to sleep.
 */
#define U_CELL_PWR_TEST_EDRX_MARGIN_SECONDS 2
#endif

#ifndef U_CELL_PWR_TEST_PAGING_WINDOW_SECONDS
/** The paging window to use when testing E-DRX in seconds.
 */
# define U_CELL_PWR_TEST_PAGING_WINDOW_SECONDS 1
#endif

#ifndef U_CELL_PWR_TEST_ECHO_STRING
/** String to send to the echo server during power saving testing.
 */
# define U_CELL_PWR_TEST_ECHO_STRING "Hello world!"
#endif

#ifndef U_CELL_PWR_TEST_ECHO_STRING_LENGTH_BYTES
/** The length of U_CELL_PWR_TEST_ECHO_STRING, not including
 * terminator, as strlen() would return.
 */
# define U_CELL_PWR_TEST_ECHO_STRING_LENGTH_BYTES 12
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold E-DRX values.
 */
typedef struct {
    int32_t eDrxSecondsRequested;
    int32_t eDrxSecondsExpected;
} uCellPwrTestEdrxValues_t;

/** Structure to hold all 3GPP power saving parameters, for use with
 * the calllback.
 */
typedef struct {
    bool onNotOff;
    int32_t activeTimeSeconds;
    int32_t periodicWakeupSeconds;
} uCellPwrTest3gppPowerSavingParameters_t;

/** Structure to hold all E-DRX parameters, for use with the calllback.
 */
typedef struct {
    uCellNetRat_t rat;
    bool onNotOff;
    int32_t eDrxSecondsRequested;
    int32_t eDrxSecondsAssigned;
    int32_t pagingWindowSecondsAssigned;
} uCellPwrTestEdrxParameters_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** For tracking heap lost to allocations made
 * by the C library in new tasks: newlib does NOT
 * necessarily reclaim it on task deletion.
 */
static size_t gSystemHeapLost = 0;

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** A variable to track errors in the callbacks.
 */
static int32_t gCallbackErrorCode = 0;

# ifndef U_CFG_CELL_DISABLE_UART_POWER_SAVING

/** TCP socket handle.
 */
static int32_t gSockHandle = -1;

/** Test values for requested and expected E-DRX on Cat-M1;
 * just a few spot-checks
 */
static uCellPwrTestEdrxValues_t gEDrxSecondsCatM1[] = {
    {   7,   10},
    { 103,  122},
    {2622, 2621}
};

/** Place to store the E-DRX parameters as received by the
 * E-DRX callback function.
 */
static uCellPwrTestEdrxParameters_t gEDrxParameters;

/** Place to store the 3GPP power sacing parameters as received
 * by the callback function.
 */
uCellPwrTest3gppPowerSavingParameters_t g3gppPowerSavingCallbackParameter = {0};

# endif // U_CFG_CELL_DISABLE_UART_POWER_SAVING

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular power-down process
static bool keepGoingCallback(uDeviceHandle_t cellHandle)
{
    bool keepGoing = true;

    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorCode = 1;
    }

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

# if U_CFG_APP_PIN_CELL_PWR_ON >= 0

// Test power on/off and aliveness, parameterised by the VInt pin.
static void testPowerAliveVInt(uCellTestPrivate_t *pHandles,
                               int32_t pinVint)
{
    bool (*pKeepGoingCallback) (uDeviceHandle_t) = NULL;
    uDeviceHandle_t cellHandle;
    int32_t returnCode;
    bool trulyHardPowerOff = false;
    const uCellPrivateModule_t *pModule;
#  if U_CFG_APP_PIN_CELL_VINT < 0
    int64_t timeMs;
#  endif

#  if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
    //lint -e(838) Suppress previously assigned value has not been used
    trulyHardPowerOff = true;
#  endif

    uPortLog(U_TEST_PREFIX "running power-on and alive tests");
    if (pinVint >= 0) {
        uPortLog(" with VInt on pin %d.\n", pinVint);
    } else {
        uPortLog(" without VInt.\n");
    }

    U_TEST_PRINT_LINE("adding a cellular instance on the AT client...");
    returnCode = uCellAdd(U_CFG_TEST_CELL_MODULE_TYPE,
                          pHandles->atClientHandle,
                          U_CFG_APP_PIN_CELL_ENABLE_POWER,
                          U_CFG_APP_PIN_CELL_PWR_ON,
                          pinVint, false,
                          &pHandles->cellHandle);
    U_PORT_TEST_ASSERT_EQUAL((int32_t) U_ERROR_COMMON_SUCCESS, returnCode);
    cellHandle = pHandles->cellHandle;

#if defined(U_CFG_APP_PIN_CELL_DTR) && (U_CFG_APP_PIN_CELL_DTR >= 0)
    uCellPwrSetDtrPowerSavingPin(cellHandle, U_CFG_APP_PIN_CELL_DTR);
#endif

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Let the module state settle in case it is on but still
    // booting
    uPortTaskBlock(pModule->bootWaitSeconds * 1000);

    // If the module is on at the start, switch it off.
    if (uCellPwrIsAlive(cellHandle)) {
        U_TEST_PRINT_LINE("powering off to begin test.");
        uCellPwrOff(cellHandle, NULL);
        U_TEST_PRINT_LINE("power off completed.");
#  if U_CFG_APP_PIN_CELL_VINT < 0
        U_TEST_PRINT_LINE("waiting another %d second(s) to be"
                          " sure of a clean power off as there's"
                          " no VInt pin to tell us...",
                          pModule->powerDownWaitSeconds);
        uPortTaskBlock(pModule->powerDownWaitSeconds * 1000);
#  endif
    }

    // Do this twice so as to check transiting from
    // a call to uCellPwrOff() back to a call to uCellPwrOn().
    for (size_t x = 0; x < 2; x++) {
        uPortLog(U_TEST_PREFIX "testing power-on and alive calls");
        if (x > 0) {
            uPortLog(" with a callback passed to uCellPwrOff(), and"
                     " a %d second power-off timer, iteration %d.\n",
                     pModule->powerDownWaitSeconds, x + 1);
        } else {
            uPortLog(" with cellPwrOff(NULL), iteration %d.\n", x + 1);
        }
        U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
#  if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
        U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
#  endif
        // TODO Note: only use a NULL PIN as we don't support anything
        // else at least that's the case on SARA-R4 when you want to
        // have power saving
        U_TEST_PRINT_LINE("powering on...");
        U_PORT_TEST_ASSERT(uCellPwrOn(cellHandle, U_CELL_TEST_CFG_SIM_PIN,
                                      NULL) == 0);
        U_TEST_PRINT_LINE("checking that module is alive...");
        U_PORT_TEST_ASSERT(uCellPwrIsAlive(cellHandle));
        // Give the module time to sort itself out
        U_TEST_PRINT_LINE("waiting %d second(s) before powering off...",
                          pModule->minAwakeTimeSeconds);
        uPortTaskBlock(pModule->minAwakeTimeSeconds * 1000);
        // Test with and without a keep-going callback
        if (x > 0) {
            // Note: can't check if keepGoingCallback is being
            // called here as we've no control over how long the
            // module takes to power off.
            pKeepGoingCallback = keepGoingCallback;
            gStopTimeMs = uPortGetTickTimeMs() +
                          (((int64_t) pModule->powerDownWaitSeconds) * 1000);
        }
#  if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs();
#  endif
        U_TEST_PRINT_LINE("powering off...");
        uCellPwrOff(cellHandle, pKeepGoingCallback);
        U_TEST_PRINT_LINE("power off completed.");
#  if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs() - timeMs;
        if (timeMs < pModule->powerDownWaitSeconds * 1000) {
            timeMs = (pModule->powerDownWaitSeconds * 1000) - timeMs;
            U_TEST_PRINT_LINE("waiting another %d second(s) to be sure of a "
                              "clean power off as there's no VInt pin to tell us...",
                              (int32_t) ((timeMs / 1000) + 1));
            uPortTaskBlock(timeMs);
        }
#  endif
    }

    // Do this twice so as to check transiting from
    // a call to uCellPwrOffHard() to a call to
    // uCellPwrOn().
    for (size_t x = 0; x < 2; x++) {
        uPortLog(U_TEST_PREFIX "testing power-on and alive calls with "
                 "uCellPwrOffHard()");
        if (trulyHardPowerOff) {
            uPortLog(" and truly hard power off");
        }
        uPortLog(", iteration %d.\n", x + 1);
        U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
#  if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
        U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
#  endif
        U_TEST_PRINT_LINE("powering on...");
        U_PORT_TEST_ASSERT(uCellPwrOn(cellHandle, U_CELL_TEST_CFG_SIM_PIN,
                                      NULL) == 0);
        U_TEST_PRINT_LINE("checking that module is alive...");
        U_PORT_TEST_ASSERT(uCellPwrIsAlive(cellHandle));
        // Let the module sort itself out
        U_TEST_PRINT_LINE("waiting %d second(s) before powering off...",
                          pModule->minAwakeTimeSeconds);
        uPortTaskBlock(pModule->minAwakeTimeSeconds * 1000);
#  if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs();
#  endif
        U_TEST_PRINT_LINE("hard powering off...");
        uCellPwrOffHard(cellHandle, trulyHardPowerOff, NULL);
        U_TEST_PRINT_LINE("hard power off completed.");
#  if U_CFG_APP_PIN_CELL_VINT < 0
        timeMs = uPortGetTickTimeMs() - timeMs;
        if (!trulyHardPowerOff && (timeMs < pModule->powerDownWaitSeconds * 1000)) {
            timeMs = (pModule->powerDownWaitSeconds * 1000) - timeMs;
            U_TEST_PRINT_LINE("waiting another %d second(s) to be sure of"
                              " a clean power off as there's no VInt pin to"
                              " tell us...", (int32_t) ((timeMs / 1000) + 1));
            uPortTaskBlock(timeMs);
        }
#  endif
    }

    U_TEST_PRINT_LINE("testing power-on and alive calls after hard power off.");
    U_PORT_TEST_ASSERT(!uCellPwrIsAlive(cellHandle));
#  if U_CFG_APP_PIN_CELL_ENABLE_POWER >= 0
    U_PORT_TEST_ASSERT(!uCellPwrIsPowered(cellHandle));
#  endif

    U_TEST_PRINT_LINE("removing cellular instance...");
    uCellRemove(cellHandle);
}

# if (U_CFG_APP_PIN_CELL_VINT >= 0) && !defined(U_CFG_CELL_DISABLE_UART_POWER_SAVING)
// Callback for when the 3GPP power saving parameters are indicated
// by the network
static void powerSaving3gppCallback(uDeviceHandle_t cellHandle, bool onNotOff,
                                    int32_t activeTimeSeconds,
                                    int32_t periodicWakeupSeconds,
                                    void *pParameter)
{
    if (cellHandle != *((uDeviceHandle_t *) pParameter)) {
        gCallbackErrorCode = 2;
    }

    g3gppPowerSavingCallbackParameter.onNotOff = onNotOff;
    g3gppPowerSavingCallbackParameter.activeTimeSeconds = activeTimeSeconds;
    g3gppPowerSavingCallbackParameter.periodicWakeupSeconds = periodicWakeupSeconds;
}

// 3GPP power saving wake-up callback where the second parameter is a pointer
// to an int32_t.
static void wakeCallback(uDeviceHandle_t cellHandle, void *pParam)
{
    U_PORT_TEST_ASSERT(cellHandle == gHandles.cellHandle);
    (*((int32_t *) pParam))++;

    // Re-disable that remarkably persistent LWM2M client for
    // modules which forget that it was disabled
    uCellTestPrivateLwm2mDisable(cellHandle);
}
#  endif // if (U_CFG_APP_PIN_CELL_VINT >= 0) && !defined(U_CFG_CELL_DISABLE_UART_POWER_SAVING)

# endif // if U_CFG_APP_PIN_CELL_PWR_ON >= 0

# ifndef U_CFG_CELL_DISABLE_UART_POWER_SAVING
// Connect to a cellular network.
static int32_t connectNetwork(uDeviceHandle_t cellHandle)
{
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);

    return uCellNetConnect(cellHandle, NULL,
# ifdef U_CELL_TEST_CFG_APN
                           U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
# else
                           NULL,
# endif
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
}

// Callback for base station connection status where the parameter is a
// pointer to an int32_t.
static void connectCallback(bool isConnected, void *pParameter)
{
    if (isConnected) {
        (*((int32_t *) pParameter))++;
    } else {
        (*((int32_t *) pParameter))--;
    }
}

// Connect to an echo server, so that we can exchange data during tests.
static int32_t connectToEchoServer(uDeviceHandle_t cellHandle, uSockAddress_t *pEchoServerAddress)
{
    int32_t sockHandle = -1;

    // Init cell sockets so that we can run a data transfer
    U_PORT_TEST_ASSERT(uCellSockInit() == 0);
    U_PORT_TEST_ASSERT(uCellSockInitInstance(cellHandle) == 0);

    // Look up the address of the server we use for TCP echo
    if (uCellSockGetHostByName(cellHandle, U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                               &(pEchoServerAddress->ipAddress)) == 0) {
        // Add the port number we will use
        pEchoServerAddress->port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

        U_TEST_PRINT_LINE("connecting to %s:%d...",
                          U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                          pEchoServerAddress->port);

        // Create a TCP socket
        sockHandle = uCellSockCreate(cellHandle, U_SOCK_TYPE_STREAM, U_SOCK_PROTOCOL_TCP);
        if (sockHandle >= 0) {
            // ...and connect it
            uCellSockConnect(cellHandle, sockHandle, pEchoServerAddress);
        }

        U_TEST_PRINT_LINE("socket connected is %d.", sockHandle);
    }

    return sockHandle;
}

// Exchange some data with the echo server
static int32_t echoData(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    int32_t y;
    int32_t z;
    size_t count;
    char buffer[U_CELL_PWR_TEST_ECHO_STRING_LENGTH_BYTES];

    U_TEST_PRINT_LINE("sending \"%s\" (%d byte(s)) on socket %d...",
                      U_CELL_PWR_TEST_ECHO_STRING, sizeof(buffer), sockHandle);

    y = 0;
    count = 0;
    while ((y < sizeof(buffer)) && (count < 100)) {
        z = uCellSockWrite(cellHandle, sockHandle,
                           &U_CELL_PWR_TEST_ECHO_STRING[y],
                           sizeof(buffer) - y);
        if (z > 0) {
            y += z;
        } else {
            uPortTaskBlock(500);
        }
        count++;
    }
    if (y == sizeof(buffer)) {
        U_TEST_PRINT_LINE("%d byte(s) sent.", y);
    }

    // Get the data back again
    U_TEST_PRINT_LINE("receiving echoed data back...");
    y = 0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
    while ((y < sizeof(buffer)) && (count < 100)) {
        z = uCellSockRead(cellHandle, sockHandle,
                          buffer + y, sizeof(buffer) - y);
        if (z > 0) {
            y += z;
        } else {
            uPortTaskBlock(500);
        }
        count++;
    }
    U_TEST_PRINT_LINE("%d byte(s) received back.", y);

    // Compare the data
    return memcmp(buffer, U_CELL_PWR_TEST_ECHO_STRING, sizeof(buffer));
}

// Disconnect the given socket
static void disconnectFromEchoServer(uDeviceHandle_t cellHandle,
                                     int32_t *pSockHandle)
{
    // Close the socket
    uCellSockClose(cellHandle, *pSockHandle, NULL);
    *pSockHandle = -1;
    // Deinit cell sockets
    uCellSockDeinit();
}

// Calllback for when E-DRX parameters are changed.
static void eDrxCallback(uDeviceHandle_t cellHandle, uCellNetRat_t rat,
                         bool onNotOff,
                         int32_t eDrxSecondsRequested,
                         int32_t eDrxSecondsAssigned,
                         int32_t pagingWindowSecondsAssigned,
                         void *pParameter)
{
    if (cellHandle != (uDeviceHandle_t)pParameter) {
        gCallbackErrorCode = 1;
    }

    gEDrxParameters.rat = rat;
    gEDrxParameters.onNotOff = onNotOff;
    gEDrxParameters.eDrxSecondsRequested = eDrxSecondsRequested;
    gEDrxParameters.eDrxSecondsAssigned = eDrxSecondsAssigned;
    gEDrxParameters.pagingWindowSecondsAssigned = pagingWindowSecondsAssigned;
}

// Set some E-DRX parameters and return what was actually assigned.
// On entry the values at the pointers should be set to the values
// that the requested values would resolve to: e.g. an E-DRX value
// of 7 seconds would be expected to end up as a requested
// value of 10 seconds, since that's the nearest coded value.
// On exit the values at the pointers will be the assigned values.
static bool setEdrx(uDeviceHandle_t cellHandle, int32_t *pSockHandle,
                    uSockAddress_t *pEchoServerAddress,
                    uCellNetRat_t rat, bool onNotOff,
                    int32_t eDrxSeconds, int32_t pagingWindowSeconds,
                    bool *pOnNotOff, int32_t *pEDrxSeconds,
                    int32_t *pPagingWindowSeconds)
{
    bool onNotOffExpected = *pOnNotOff;
    int32_t eDrxSecondsExpected = *pEDrxSeconds;
    int32_t pagingWindowSecondsExpected = *pPagingWindowSeconds;
    bool rebooted = false;

    *pOnNotOff = !onNotOff;
    *pEDrxSeconds = -1;
    *pPagingWindowSeconds = -1;

    memset(&gEDrxParameters, 0, sizeof(gEDrxParameters));
    gCallbackErrorCode = 0;

    U_TEST_PRINT_LINE("**REQUESTING** E-DRX %s, %d second(s), paging window"
                      " %d second(s).", onNotOff ? "on" : "off",
                      eDrxSeconds, pagingWindowSeconds);
    U_PORT_TEST_ASSERT(uCellPwrSetRequestedEDrx(cellHandle, rat,
                                                onNotOff,
                                                eDrxSeconds,
                                                pagingWindowSeconds) == 0);
    if (uCellPwrRebootIsRequired(cellHandle)) {
        U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
        // Re-make the cellular connection 'cos the request to get
        // the assigned E-DRX parameters won't work otherwise
        U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
        if ((pSockHandle != NULL) && (*pSockHandle >= 0) && (pEchoServerAddress != NULL)) {
            // And reconnect the socket
            *pSockHandle = connectToEchoServer(cellHandle, pEchoServerAddress);
        }
        // LWM2M activity can get in the way of 3GPP power saving and
        // some module types don't store the disabledness of the LWM2M
        // client in NVRAM, so we need to disable it again after a reboot
        uCellTestPrivateLwm2mDisable(cellHandle);
        rebooted = true;
    }

    // Wait for the callback to be called if we have an expected value to check
    if (eDrxSecondsExpected >= 0) {
        U_TEST_PRINT_LINE("waiting for the URC...");
        for (size_t x = 0; (x < 60) && (gEDrxParameters.rat != rat) &&
             (gEDrxParameters.onNotOff != onNotOff) &&
             (gEDrxParameters.eDrxSecondsRequested != eDrxSecondsExpected) &&
             (gEDrxParameters.eDrxSecondsAssigned != eDrxSecondsExpected) &&
             // Not all modules support setting or getting paging window so
             // need to allow it to be -1
             ((gEDrxParameters.pagingWindowSecondsAssigned != -1) ||
              (gEDrxParameters.pagingWindowSecondsAssigned != pagingWindowSecondsExpected)); x++) {
            uPortTaskBlock(1000);
        }
        U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

        // Now we get the requested E-DRX parameters and they should be correct
        U_PORT_TEST_ASSERT(uCellPwrGetRequestedEDrx(cellHandle, rat,
                                                    &onNotOff,
                                                    &eDrxSeconds,
                                                    &pagingWindowSeconds) == 0);
        U_TEST_PRINT_LINE("E-DRX set to %s, %d second(s), paging window"
                          " %d second(s).", onNotOff ? "on" : "off", eDrxSeconds,
                          pagingWindowSeconds);
        U_PORT_TEST_ASSERT(onNotOff == onNotOffExpected);
        U_PORT_TEST_ASSERT(eDrxSeconds == eDrxSecondsExpected);
        // Not all modules support setting or getting paging window so
        // it is not possible to check it
        U_PORT_TEST_ASSERT((pagingWindowSeconds == -1) ||
                           (pagingWindowSecondsExpected == -1) ||
                           (pagingWindowSeconds == pagingWindowSecondsExpected));

        // Finally get the assigned E-DRX parameters
        U_PORT_TEST_ASSERT(uCellPwrGetEDrx(cellHandle,
                                           uCellNetGetActiveRat(cellHandle),
                                           pOnNotOff, pEDrxSeconds,
                                           pPagingWindowSeconds) == 0);
        U_PORT_TEST_ASSERT(*pOnNotOff == onNotOffExpected);
        U_PORT_TEST_ASSERT(*pEDrxSeconds == eDrxSecondsExpected);
        // Not all modules support setting or getting paging window so
        // need to allow it to be -1
        U_PORT_TEST_ASSERT((*pPagingWindowSeconds == -1) ||
                           (pagingWindowSecondsExpected == -1) ||
                           (*pPagingWindowSeconds == pagingWindowSecondsExpected));
    }

    return rebooted;
}

# endif // U_CFG_CELL_DISABLE_UART_POWER_SAVING

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

# if U_CFG_APP_PIN_CELL_PWR_ON >= 0

/** Test all the power functions apart from reboot.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
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

    U_TEST_PRINT_LINE("adding an AT client on UART %d...",
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
#  if U_CFG_APP_PIN_CELL_VINT >= 0
    testPowerAliveVInt(&gHandles, U_CFG_APP_PIN_CELL_VINT);
#  endif

    U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

# endif // if U_CFG_APP_PIN_CELL_PWR_ON >= 0

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
    U_TEST_PRINT_LINE("rebooting cellular...");
    U_PORT_TEST_ASSERT(uCellPwrReboot(gHandles.cellHandle, NULL) == 0);

    U_PORT_TEST_ASSERT(uCellPwrIsAlive(gHandles.cellHandle));

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Test reset
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrReset")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
    int32_t x;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);

    U_TEST_PRINT_LINE("resetting cellular...");
    x = uCellPwrResetHard(gHandles.cellHandle, U_CFG_APP_PIN_CELL_RESET);
# if U_CFG_APP_PIN_CELL_RESET >= 0
    U_PORT_TEST_ASSERT(x == 0);
# else
    U_PORT_TEST_ASSERT(x < 0);
# endif

    U_PORT_TEST_ASSERT(uCellPwrIsAlive(gHandles.cellHandle));

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Test UART power saving.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrSavingUart")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
    uDeviceHandle_t cellHandle;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    if (uCellPwrUartSleepIsEnabled(cellHandle)) {
        // Check that enabling when already enabled is fine
        U_PORT_TEST_ASSERT(uCellPwrEnableUartSleep(cellHandle) == 0);
        // Now disable it and check that worked
        U_PORT_TEST_ASSERT(uCellPwrDisableUartSleep(cellHandle) == 0);
        U_PORT_TEST_ASSERT(!uCellPwrUartSleepIsEnabled(cellHandle));
        // Check that disabling when already disabled is fine
        U_PORT_TEST_ASSERT(uCellPwrDisableUartSleep(cellHandle) == 0);
        // Now enable it again and check that worked
        U_PORT_TEST_ASSERT(uCellPwrEnableUartSleep(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellPwrUartSleepIsEnabled(cellHandle));
    } else {
        // Nothing much to do here: if sleep is not enabled at the outset
        // then it is not supported so just show that disabling it is
        // fine and enabling it is not
        U_PORT_TEST_ASSERT(uCellPwrDisableUartSleep(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellPwrEnableUartSleep(cellHandle) < 0);
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

# ifndef U_CFG_CELL_DISABLE_UART_POWER_SAVING
#  if (U_CFG_APP_PIN_CELL_PWR_ON >= 0) && (U_CFG_APP_PIN_CELL_VINT >= 0)

/** Test 3GPP power saving.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrSaving3gpp")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
    const uCellPrivateModule_t *pModule;
    uDeviceHandle_t cellHandle;
    int32_t x;
    uCellNetRat_t rat;
    bool onNotOff3gppSleepSaved;
    int32_t activeTimeSecondsSaved;
    int32_t periodicWakeupSecondsSaved;
    bool onNotOffEDrxSaved = false;
    int32_t eDrxSecondsSaved;
    int32_t pagingWindowSecondsSaved;
    bool onNotOff = false;
    bool sleepActive = false;
    int32_t activeTimeSeconds = 0;
    int32_t periodicWakeupSeconds = 0;
    volatile int32_t wakeCallbackParam = 0;
    char buffer[U_CELL_INFO_IMEI_SIZE];
    uSockAddress_t echoServerAddress;
    volatile int32_t connectionCallbackParameter = -1;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(gHandles.cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Also in case a previous test failed
    if (gSockHandle >= 0) {
        disconnectFromEchoServer(cellHandle, &gSockHandle);
    }
    // Use a callback to track our connectivity state, if we can
    //lint -e(1773) Suppress complaints about
    // passing the pointer as non-volatile
    if (uCellNetSetBaseStationConnectionStatusCallback(cellHandle,
                                                       connectCallback,
                                                       (void *) &connectionCallbackParameter) == 0) {
        connectionCallbackParameter = 0;
    }

    // Make a cellular connection
    U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);

    // If we're on an EUTRAN RAT then 3GPP power saving will be supported
    rat = uCellNetGetActiveRat(cellHandle);
    if (((rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_CATM1) ||
         (rat == U_CELL_NET_RAT_NB1)) &&
        // ...except we currently check the support flag as 3GPP
        // sleep for SARA-R422 is temporarily disabled
        U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
        U_TEST_PRINT_LINE("testing 3GPP power saving...");

        // LWM2M activity can get in the way of 3GPP power saving and
        // some module types don't store the disabledness of the LWM2M
        // client in NVRAM, so we need to keep disabling it during
        // this test
        uCellTestPrivateLwm2mDisable(cellHandle);

        // Set a callback for when the 3GPP power saving parameters are
        // signalled by the network
        U_PORT_TEST_ASSERT(uCellPwrSet3gppPowerSavingCallback(cellHandle,
                                                              powerSaving3gppCallback,
                                                              (void *) &cellHandle) == 0);

        // Read out the original settings
        U_PORT_TEST_ASSERT(uCellPwrGetRequested3gppPowerSaving(cellHandle,
                                                               &onNotOff3gppSleepSaved,
                                                               &activeTimeSecondsSaved,
                                                               &periodicWakeupSecondsSaved) == 0);

        // Also read out the original E-DRX settings, as, if E-DRX is
        // active, 3GPP power saving might not
        U_PORT_TEST_ASSERT(uCellPwrGetEDrx(cellHandle, rat, &onNotOffEDrxSaved,
                                           &eDrxSecondsSaved, &pagingWindowSecondsSaved) == 0);
        // Make sure that E-DRX is off
        if (onNotOffEDrxSaved) {
            if (uCellPwrSetRequestedEDrx(cellHandle, rat,
                                         false, eDrxSecondsSaved,
                                         pagingWindowSecondsSaved) == U_CELL_ERROR_CONNECTED) {
                // Must be on one of them thar modules that doesn't
                // like setting E-DRX when connected, so disconnect
                // and try again
                U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
                U_PORT_TEST_ASSERT(uCellPwrSetRequestedEDrx(cellHandle, rat,
                                                            false, eDrxSecondsSaved,
                                                            pagingWindowSecondsSaved) == 0);
                if (uCellPwrRebootIsRequired(cellHandle)) {
                    // If necessary reboot
                    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
                }
                // Remake the cellular connection
                U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
            } else {
                if (uCellPwrRebootIsRequired(cellHandle)) {
                    // If necessary reboot and remake the cellular connection
                    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
                    U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
                }
            }
            // Check what we got
            U_PORT_TEST_ASSERT(uCellPwrGetEDrx(cellHandle, rat,
                                               &onNotOff, NULL, NULL) == 0);
            U_PORT_TEST_ASSERT(!onNotOff);
        }

        // Start with 3GPP power saving off
        U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                               false, -1, -1) == 0);
        if (uCellPwrRebootIsRequired(cellHandle)) {
            U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
            U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
            uCellTestPrivateLwm2mDisable(cellHandle);
        }
        U_PORT_TEST_ASSERT(uCellPwrGetRequested3gppPowerSaving(cellHandle, &onNotOff,
                                                               NULL, NULL) == 0);
        U_PORT_TEST_ASSERT(!onNotOff);
        if (uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0) {
            U_PORT_TEST_ASSERT(!sleepActive);
        }

        // Test getting the power saving parameters with all NULL variables
        U_PORT_TEST_ASSERT(uCellPwrGetRequested3gppPowerSaving(cellHandle, NULL,
                                                               NULL, NULL) == 0);

        // Now set some power saving parameters without switching power saving on
        U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                               false,
                                                               U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS,
                                                               U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS) == 0);
        U_PORT_TEST_ASSERT(uCellPwrGetRequested3gppPowerSaving(cellHandle, &onNotOff,
                                                               &activeTimeSeconds,
                                                               &periodicWakeupSeconds) == 0);
        U_TEST_PRINT_LINE("active time set to %d second(s), perodic wake-up"
                          " %d second(s) (power saving %s).",
                          activeTimeSeconds, periodicWakeupSeconds,
                          onNotOff ? "on" : "off");
        U_PORT_TEST_ASSERT(!onNotOff);
        U_PORT_TEST_ASSERT(activeTimeSeconds == U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS);
        U_PORT_TEST_ASSERT(periodicWakeupSeconds == U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS);

        // Set a wake-up callback
        //lint -e(1773) Suppress complaints about
        // passing the pointer as non-volatile
        U_PORT_TEST_ASSERT(uCellPwrSetDeepSleepWakeUpCallback(cellHandle, wakeCallback,
                                                              (void *) &wakeCallbackParam) == 0);

        // Now actually enable 3GPP power saving
        U_TEST_PRINT_LINE("**REQUESTING** 3GPP power saving on...");
        U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                               true,
                                                               U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS,
                                                               U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS) == 0);
        if (uCellPwrRebootIsRequired(cellHandle)) {
            U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
            U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
            uCellTestPrivateLwm2mDisable(cellHandle);
        }
        activeTimeSeconds = 0;
        periodicWakeupSeconds = 0;
        U_PORT_TEST_ASSERT(uCellPwrGetRequested3gppPowerSaving(cellHandle, &onNotOff,
                                                               &activeTimeSeconds,
                                                               &periodicWakeupSeconds) == 0);
        U_PORT_TEST_ASSERT(onNotOff);
        U_PORT_TEST_ASSERT(activeTimeSeconds == U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS);
        U_PORT_TEST_ASSERT(periodicWakeupSeconds == U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS);

        // Wait for us to return to idle
        U_TEST_PRINT_LINE("waiting up to %d seconds(s) for return to idle...",
                          U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS);
        if (connectionCallbackParameter >= 0) {
            for (x = 0; (connectionCallbackParameter > 0) &&
                 (x < U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS); x++) {
                uPortTaskBlock(1000);
            }
        } else {
            // No callback, just have to wait
            uPortTaskBlock(U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS);
        }

        // Get the assigned 3GPP power saving parameters; the new settings may
        // take a while to be propagated to the network so try this a few times
        U_TEST_PRINT_LINE("waiting for the network to agree...");
        onNotOff = false;
        activeTimeSeconds = 0;
        periodicWakeupSeconds = 0;
        for (x = 0; (x < 10) && ((onNotOff != true) ||
                                 (activeTimeSeconds != U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS) ||
                                 (periodicWakeupSeconds != U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS)); x++) {
            U_PORT_TEST_ASSERT(uCellPwrGet3gppPowerSaving(cellHandle, &onNotOff,
                                                          &activeTimeSeconds,
                                                          &periodicWakeupSeconds) == 0);
            uPortTaskBlock(1000);
        }
        U_PORT_TEST_ASSERT(onNotOff);
        U_PORT_TEST_ASSERT(activeTimeSeconds == U_CELL_PWR_TEST_ACTIVE_TIME_SECONDS);
        U_PORT_TEST_ASSERT(periodicWakeupSeconds == U_CELL_PWR_TEST_PERIODIC_WAKEUP_SECONDS);

        // Wait for the active time to expire, with some margin,
        // and check that the module is asleep
        U_TEST_PRINT_LINE("waiting up to %d second(s) for sleep...",
                          activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                          U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS);
        if (uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0) {
            // A sleep activity indication is supported so we can wait for that
            for (x = 0; !sleepActive && (x < activeTimeSeconds +
                                         U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                                         U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS); x++) {
                U_PORT_TEST_ASSERT(uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0);
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(sleepActive);
            U_TEST_PRINT_LINE("module has fallen asleep.");
        } else {
            // No indication is available, just have to block
            uPortTaskBlock((activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                            U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS) * 1000);
        }

        // Perform an operation that sends an AT command to the module:
        // this should work
        U_TEST_PRINT_LINE("requesting the IMEI when the module is asleep...");
        U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer) == 0);
        U_TEST_PRINT_LINE("wake-up callback has been called %d time(s).",
                          wakeCallbackParam);
        // Wait a moment for the wake-up callback to propagate
        uPortTaskBlock(1000);
        U_PORT_TEST_ASSERT(wakeCallbackParam == 1);

        // We should still be registered on an EUTRAN RAT
        rat = uCellNetGetActiveRat(cellHandle);
        U_PORT_TEST_ASSERT((rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_CATM1) ||
                           (rat == U_CELL_NET_RAT_NB1));

        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) {
            // SARA-R422 does not re-enter 3GPP power saving unless there has been
            // an RRC connection/disconnection, so do a DNS lookup to stimulate that
            U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                                      U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                      &echoServerAddress.ipAddress) == 0);
        }

        // Wait for the module to go to sleep again
        U_TEST_PRINT_LINE("waiting up to %d second(s) for sleep again...",
                          activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                          U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS);
        if (uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0) {
            for (x = 0; !sleepActive && (x < activeTimeSeconds +
                                         U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                                         U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS); x++) {
                U_PORT_TEST_ASSERT(uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0);
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(sleepActive);
            U_TEST_PRINT_LINE("module has fallen asleep again.");
        } else {
            // No indication is available, just have to block
            uPortTaskBlock((activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                            U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS) * 1000);
        }

        // Wake the module up using the pwr API call this time; the
        // wake-up callback should have been called
        U_TEST_PRINT_LINE("waking the module by calling the pwr API directly...");
        U_PORT_TEST_ASSERT(uCellPwrWakeUpFromDeepSleep(cellHandle, NULL) == 0);
        U_TEST_PRINT_LINE("wake-up callback has been called %d time(s).",
                          wakeCallbackParam);
        // Wait a moment for the wake-up callback to propagate
        uPortTaskBlock(1000);
        U_PORT_TEST_ASSERT(wakeCallbackParam == 2);

        // We should still be registered on an EUTRAN RAT
        rat = uCellNetGetActiveRat(cellHandle);
        U_PORT_TEST_ASSERT((rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_CATM1) ||
                           (rat == U_CELL_NET_RAT_NB1));

        if (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) {
            // SARA-R422 does not re-enter 3GPP power saving unless there has been
            // an RRC connection/disconnection, so do a DNS lookup to stimulate that
            U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                                      U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                      &echoServerAddress.ipAddress) == 0);
        }

        // Wait for the module to fall sleep again
        U_TEST_PRINT_LINE("waiting up to %d second(s) for sleep...",
                          activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                          U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS);

        if (uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0) {
            for (x = 0; !sleepActive && (x < activeTimeSeconds +
                                         U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                                         U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS); x++) {
                U_PORT_TEST_ASSERT(uCellPwrGetDeepSleepActive(cellHandle, &sleepActive) == 0);
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(sleepActive);
            U_TEST_PRINT_LINE("module has successfully gone to sleepy-byes.");
        } else {
            // No indication is available, just have to block
            uPortTaskBlock((activeTimeSeconds + U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS +
                            U_CELL_PWR_TEST_3GPP_POWER_SAVING_MARGIN_SECONDS) * 1000);
        }

        // We should still be registered on an EUTRAN RAT
        rat = uCellNetGetActiveRat(cellHandle);
        U_PORT_TEST_ASSERT((rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_CATM1) ||
                           (rat == U_CELL_NET_RAT_NB1));

        // Do a DNS look-up to check that we can still do radio-ey things.
        U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                                  U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                  &echoServerAddress.ipAddress) == 0);

        U_TEST_PRINT_LINE("wake-up callback has been called %d time(s).",
                          wakeCallbackParam);
        // It is possible for uCellSockGetHostByName() to take longer than the active time
        // and hence the wake-up callback may actually be called four times
        U_PORT_TEST_ASSERT((wakeCallbackParam == 3) || (wakeCallbackParam == 4));

        // Remove the deep sleep callback
        U_PORT_TEST_ASSERT(uCellPwrSetDeepSleepWakeUpCallback(cellHandle, NULL, NULL) == 0);

        // Disconnect and reconnect to the network so that a +CEREG is sent
        // and hence the powerSaving3gppCallback() should be called
        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
        U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);

        U_TEST_PRINT_LINE("3GPP power saving callback has power saving %s,"
                          " active time %d second(s), periodic wake-up %d second(s).",
                          g3gppPowerSavingCallbackParameter.onNotOff ? "on" : "off",
                          g3gppPowerSavingCallbackParameter.activeTimeSeconds,
                          g3gppPowerSavingCallbackParameter.periodicWakeupSeconds);
        U_PORT_TEST_ASSERT(g3gppPowerSavingCallbackParameter.onNotOff == onNotOff);
        U_PORT_TEST_ASSERT(g3gppPowerSavingCallbackParameter.activeTimeSeconds == activeTimeSeconds);
        // Some modules don't include the periodic wake-up in their CEREG so need to allow
        // that to be -1
        U_PORT_TEST_ASSERT((g3gppPowerSavingCallbackParameter.periodicWakeupSeconds ==
                            periodicWakeupSeconds) ||
                           (g3gppPowerSavingCallbackParameter.periodicWakeupSeconds == -1));

        // Put the original saved settings back again
        U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                               onNotOff3gppSleepSaved,
                                                               activeTimeSecondsSaved,
                                                               periodicWakeupSecondsSaved) == 0);
        if (onNotOffEDrxSaved) {
            // Disconnect the network before putting the E-DRX settings back as some
            // modules require that
            U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
            U_PORT_TEST_ASSERT(uCellPwrSetRequestedEDrx(cellHandle, rat,
                                                        onNotOffEDrxSaved, eDrxSecondsSaved,
                                                        pagingWindowSecondsSaved) == 0);
        }
        if (uCellPwrRebootIsRequired(cellHandle)) {
            U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
        }

    } else {
        U_TEST_PRINT_LINE("not on an EUTRAN RAT, or 3GPP power saving not"
                          " supported, 3GPP power saving cannot be tested.");
    }

    uCellNetSetBaseStationConnectionStatusCallback(cellHandle, NULL, NULL);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during this"
                      " test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

#  endif // if (U_CFG_APP_PIN_CELL_PWR_ON >= 0) && (U_CFG_APP_PIN_CELL_VINT >= 0)

/** Test E-DRX.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrSavingEDrx")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
    const uCellPrivateModule_t *pModule;
    uDeviceHandle_t cellHandle;
    int32_t x;
    uCellNetRat_t rat;
    bool onNotOffEDrxSaved;
    int32_t eDrxSecondsSaved;
    int32_t pagingWindowSecondsSaved;
    bool onNotOff3gppSleepSaved = false;
    int32_t activeTimeSecondsSaved;
    int32_t periodicWakeupSecondsSaved;
    bool onNotOff;
    int32_t eDrxSeconds;
    int32_t pagingWindowSeconds;
    volatile int32_t connectionCallbackParameter = -1;
    uSockAddress_t echoServerAddress;
    uCellPwrTestEdrxValues_t *pTestEdrx = NULL;
    size_t testEdrxLength = 0;
    bool rebooted = false;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(gHandles.cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_EDRX)) {
        U_TEST_PRINT_LINE("testing EDRX...");

        // Also in case a previous test failed
        if (gSockHandle >= 0) {
            disconnectFromEchoServer(cellHandle, &gSockHandle);
        }

        // Set a callback for when E-DRX parameters are changed
        U_PORT_TEST_ASSERT(uCellPwrSetEDrxCallback(cellHandle, eDrxCallback,
                                                   cellHandle) == 0);

        // Use a callback to track our connectivity state, if we can
        //lint -e(1773) Suppress complaints about
        // passing the pointer as non-volatile
        if (uCellNetSetBaseStationConnectionStatusCallback(cellHandle,
                                                           connectCallback,
                                                           (void *) &connectionCallbackParameter) == 0) {
            connectionCallbackParameter = 0;
        }

        // Make a cellular connection
        U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);

        // Now we can tell which RAT we're on
        rat = uCellNetGetActiveRat(cellHandle);

        // Connect to an echo server so that we can exchange data during the test
        gSockHandle = connectToEchoServer(cellHandle, &echoServerAddress);
        U_PORT_TEST_ASSERT(gSockHandle >= 0);

        // Read out the original E-DRX settings
        x = uCellPwrGetRequestedEDrx(cellHandle, rat,
                                     &onNotOffEDrxSaved,
                                     &eDrxSecondsSaved,
                                     &pagingWindowSecondsSaved);
        if (x == 0) {
            // Also read out the original 3GPP power saving settings and switch
            // if off, as if 3GPP power saving is active it will mess us up
            uCellPwrGetRequested3gppPowerSaving(cellHandle,
                                                &onNotOff3gppSleepSaved,
                                                &activeTimeSecondsSaved,
                                                &periodicWakeupSecondsSaved);
            if (onNotOff3gppSleepSaved) {
                U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                                       false, -1, -1) == 0);
                if (uCellPwrRebootIsRequired(cellHandle)) {
                    // If necessary reboot and remake the cellular connection
                    U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
                    U_PORT_TEST_ASSERT(connectNetwork(cellHandle) == 0);
                    gSockHandle = connectToEchoServer(cellHandle, &echoServerAddress);
                }
            }

            // First, try to set the E-DRX settings to what they are already
            // as a check to see if the module we're using permits E-DRX
            // to be set while it is connected
            x = uCellPwrSetRequestedEDrx(cellHandle, rat, onNotOffEDrxSaved,
                                         eDrxSecondsSaved, pagingWindowSecondsSaved);
            if (x == (int32_t) U_CELL_ERROR_CONNECTED) {
                // Setting E-DRX while connected is not supported, disconnect
                // from the network
                U_TEST_PRINT_LINE("setting E-DRX while connected to the"
                                  " network is not supported by this module.");
                U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
                if (connectionCallbackParameter >= 0) {
                    uCellNetSetBaseStationConnectionStatusCallback(cellHandle, NULL, NULL);
                    connectionCallbackParameter = -1;
                }
            } else {
                U_PORT_TEST_ASSERT(x == 0);
            }

            // Start with E-DRX off
            onNotOff = true;
            eDrxSeconds = U_CELL_PWR_TEST_EDRX_SECONDS;
            // Can't reliably set paging window as some modules have it fixed
            pagingWindowSeconds = -1;
            rebooted = setEdrx(cellHandle, &gSockHandle, &echoServerAddress,
                               rat, onNotOff, U_CELL_PWR_TEST_EDRX_SECONDS,
                               U_CELL_PWR_TEST_PAGING_WINDOW_SECONDS,
                               &onNotOff, &eDrxSeconds, &pagingWindowSeconds);

            if (gSockHandle >= 0) {
                // Send something to prove we're connected
                U_PORT_TEST_ASSERT(echoData(cellHandle, gSockHandle) == 0);
            }

            U_TEST_PRINT_LINE("waiting for idle...",
                              U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS);
            // Wait us to return to idle
            if (connectionCallbackParameter >= 0) {
                for (x = 0; (connectionCallbackParameter > 0) &&
                     (x < U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS); x++) {
                    uPortTaskBlock(1000);
                }
            } else {
                // No callback, just have to wait
                uPortTaskBlock(U_CELL_PWR_TEST_RRC_DISCONNECT_SECONDS * 1000);
            }
            U_TEST_PRINT_LINE("waiting up to %d second(s) so that we likely"
                              " enter E-DRX...", pagingWindowSeconds +
                              U_CELL_PWR_TEST_EDRX_MARGIN_SECONDS);
            uPortTaskBlock((pagingWindowSeconds + U_CELL_PWR_TEST_EDRX_MARGIN_SECONDS) * 1000);

            // Send something again to prove that we can still connect
            U_PORT_TEST_ASSERT(echoData(cellHandle, gSockHandle) == 0);

            // Test getting the E-DRX parameters with all NULL variables
            U_PORT_TEST_ASSERT(uCellPwrGetRequestedEDrx(cellHandle, rat,
                                                        NULL, NULL, NULL) == 0);

            if (!rebooted) {
                // Spot-check some E-DRX values, but only if we don't have to
                // reboot between each one as then the test takes ages
                // TODO NB1
                if (rat == U_CELL_NET_RAT_CATM1) {
                    pTestEdrx = gEDrxSecondsCatM1;
                    testEdrxLength = sizeof(gEDrxSecondsCatM1) / sizeof(gEDrxSecondsCatM1[0]);
                }
                if ((pTestEdrx != NULL) && (testEdrxLength > 0)) {
                    for (size_t y = 0; y < testEdrxLength; y++) {
                        onNotOff = true;
                        eDrxSeconds = (pTestEdrx + y)->eDrxSecondsExpected;
                        pagingWindowSeconds = -1;
                        setEdrx(cellHandle,  &gSockHandle, &echoServerAddress, rat,
                                onNotOff, (pTestEdrx + y)->eDrxSecondsRequested, -1,
                                &onNotOff, &eDrxSeconds, &pagingWindowSeconds);
                    }
                }
            }

            // Send something to prove we're still connected
            U_PORT_TEST_ASSERT(echoData(cellHandle, gSockHandle) == 0);
            // Disconnect from the echo server and then the network
            disconnectFromEchoServer(cellHandle, &gSockHandle);
            U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

            // Put the original saved settings back again
            U_PORT_TEST_ASSERT(uCellPwrSetRequestedEDrx(cellHandle, rat,
                                                        onNotOffEDrxSaved,
                                                        eDrxSecondsSaved,
                                                        pagingWindowSecondsSaved) == 0);
            if (onNotOff3gppSleepSaved) {
                U_PORT_TEST_ASSERT(uCellPwrSetRequested3gppPowerSaving(cellHandle, rat,
                                                                       onNotOff3gppSleepSaved,
                                                                       activeTimeSecondsSaved,
                                                                       periodicWakeupSecondsSaved) == 0);
            }
            if (uCellPwrRebootIsRequired(cellHandle)) {
                U_PORT_TEST_ASSERT(uCellPwrReboot(cellHandle, NULL) == 0);
            }
        } else {
            U_TEST_PRINT_LINE("looks like E-DRX is not supported"
                              " (uCellPwrGetRequestedEDrx() returned %d).", x);
            U_PORT_TEST_ASSERT(x == U_ERROR_COMMON_NOT_SUPPORTED);
        }

        // Don't remove the callbacks this time
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}
# endif //U_CFG_CELL_DISABLE_UART_POWER_SAVING

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellPwr]", "cellPwrCleanUp")
{
    int32_t x;
    bool onNotOff = false;

    // Make completely sure 3GPP power saving is off as it can mess us up
    if (gHandles.cellHandle != NULL) {
        uCellPwrGetRequested3gppPowerSaving(gHandles.cellHandle,
                                            &onNotOff,
                                            NULL, NULL);
        if (onNotOff) {
            uCellPwrSetRequested3gppPowerSaving(gHandles.cellHandle,
                                                uCellNetGetActiveRat(gHandles.cellHandle),
                                                false, -1, -1);
            if (uCellPwrRebootIsRequired(gHandles.cellHandle)) {
                uCellPwrReboot(gHandles.cellHandle, NULL);
            }
        }
    }

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d bytes"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
