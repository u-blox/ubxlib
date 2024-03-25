/*
 * Copyright 2024 u-blox
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
 * @brief Tests for the BLE bonding API: these should pass on all
 * platforms where two UARTs are available.
 * IMPORTANT: Please note that two modules are required for this test.
 * This means that it can only run by default on a host which have the feature
 * short_range_gen2 enabled. However it is also possible to run this test with
 * modules which have the older version of u-connectXpress but it requires some
 * trickery, se more below and in this case the test can only be executed on
 * Windows or Linux.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */
#ifdef U_CFG_OVERRIDE
#include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h" // int32_t etc.
#include "stddef.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#ifdef __linux__
# include "unistd.h"
#endif

// Must always be included before u_short_range_test_selector.h
// lint -efile(766, u_ble_module_type.h)
#include "u_ble_module_type.h"
#include "u_short_range_test_selector.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_test_util_resource_check.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_cfg.h"
#include "u_short_range_private.h"

#include "u_network.h"
#include "u_network_config_ble.h"

#include "u_ble_cfg.h"
#include "u_ble.h"
#include "u_ble_gap.h"
#include "u_port_named_pipe.h"

#if U_SHORT_RANGE_TEST_BLE() &&  \
    defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE) && \
    defined(U_CFG_APP_SHORT_RANGE_UART2)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_BLE_BOND_TEST"

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog("%s" format "\n", gTestPrefix, ##__VA_ARGS__)

/* Second module uart definitions */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD2
# define U_CFG_APP_PIN_SHORT_RANGE_TXD2 -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD2
# define U_CFG_APP_PIN_SHORT_RANGE_RXD2 -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS2
# define U_CFG_APP_PIN_SHORT_RANGE_CTS2 -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS2
# define U_CFG_APP_PIN_SHORT_RANGE_RTS2 -1
#endif

#ifndef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE2
# define U_CFG_TEST_SHORT_RANGE_MODULE_TYPE2 U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
#endif

#define PIPE_NAME "ubx_ble_bond_test"

// Command handling when running in separate program instances
#define CMD_SETPARAM_FORM "%d,%d,%d,%d"
#define CMD_SETPARAM   0
#define CMD_RESP_MAC   1
#define CMD_INIT_MAC   2
#define CMD_ENTER_PASS 3
#define CMD_REM_BOND   4
#define CMD_EXIT       5

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uDeviceHandle_t gInitiatorDeviceHandle = NULL;
static uDeviceCfg_t gInitiatorDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {.cfgSho = {.moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE}},
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        }
    }
};

static uDeviceHandle_t gResponderDeviceHandle = NULL;
static uDeviceCfg_t gResponderDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {.cfgSho = {.moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE2}},
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART2,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD2,
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD2,
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS2,
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS2,
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        }
    }
};

static uNetworkCfgBle_t gInitiatorNetworkCfg = {
    .type = U_NETWORK_TYPE_BLE,
    .role = U_BLE_CFG_ROLE_CENTRAL,
    .spsServer = false,
};

static uNetworkCfgBle_t gResponderNetworkCfg = {
    .type = U_NETWORK_TYPE_BLE,
    .role = U_BLE_CFG_ROLE_PERIPHERAL,
    .spsServer = false,
};

static uBleGapAdvConfig_t gAdvCfg = {.minIntervalMs = 200,
                                     .maxIntervalMs = 200,
                                     .connectable = true,
                                     .maxClients = 1,
                                     .pAdvData = NULL,
                                     .advDataLength = 0,
                                     .pRespData = NULL,
                                     .respDataLength = 0
                                    };

char gInitiatorMacAddr[U_SHORT_RANGE_BT_ADDRESS_SIZE];
char gResponderMacAddr[U_SHORT_RANGE_BT_ADDRESS_SIZE];
static uPortSemaphoreHandle_t gBondCompleteSemaphore = NULL;
static uPortSemaphoreHandle_t gSyncSemaphore = NULL;
static volatile int32_t gBondStatus;
static volatile int32_t gPasskey;

/** This test can be run in different ways.
 * It requires two separate modules.
 * The default option is to run both bonding initiator and responder
 * in the same program. This can only be used when both the two modules
 * have u-connectXpress second generation, i.e. short_range_gen2 is defined
 * in UBXLIB_FEATURES.
 * For testing earlier version of u-connectXpress this test has to be
 * run in two different application instances, one acting as the initiator
 * and one as the responder. This is handled here by spawning a new instance
 * but with proper environment variables to ensure that only the responder
 * variant of this test is run. The initiator will terminate the responder
 * when the test has completed. The output from the responder is merged with
 * that from the initiator.
 */

static int32_t gTestOption = 0;
static uPortNamePipeHandle_t gPipe = NULL;

static int32_t gResourceCountStart;
static char gTestPrefix[50];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Write integer to a temporary string.
static const char *pIntToStr(uint32_t i)
{
    static char result[10];
    snprintf(result, sizeof(result), "%d", i);
    return result;
}

static void preamble()
{
    uPortDeinit();
    gResourceCountStart = uTestUtilGetDynamicResourceCount();
    if (gBondCompleteSemaphore == NULL) {
        U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gBondCompleteSemaphore, 0, 1) == 0);
    }
    if (gSyncSemaphore == NULL) {
        U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSyncSemaphore, 0, 1) == 0);
    }
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    if (gTestOption != 0) {
        U_PORT_TEST_ASSERT(uPortNamedPipeCreate(&gPipe, PIPE_NAME, gTestOption == 1) == 0);
    }

    if (gTestOption < 2) {
        U_TEST_PRINT_LINE("initiating the bonding initiator module");
        U_PORT_TEST_ASSERT(uDeviceOpen(&gInitiatorDeviceCfg, &gInitiatorDeviceHandle) == 0);
        U_TEST_PRINT_LINE("initiating bonding initiator BLE");
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(gInitiatorDeviceHandle, U_NETWORK_TYPE_BLE,
                                               &gInitiatorNetworkCfg) == 0);
        U_PORT_TEST_ASSERT(uBleGapRemoveBond(gInitiatorDeviceHandle, NULL) == 0);
        uBleGapGetMac(gInitiatorDeviceHandle, gInitiatorMacAddr);
        U_PORT_TEST_ASSERT(uBleGapSetPairable(gInitiatorDeviceHandle, true) == 0);
    }

    if (gTestOption != 1) {
        U_TEST_PRINT_LINE("initiating the bonding responder module");
        U_PORT_TEST_ASSERT(uDeviceOpen(&gResponderDeviceCfg, &gResponderDeviceHandle) == 0);
        U_TEST_PRINT_LINE("initiating bonding responder BLE");
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(gResponderDeviceHandle, U_NETWORK_TYPE_BLE,
                                               &gResponderNetworkCfg) == 0);
        // U_PORT_TEST_ASSERT(uBleGapReset(gResponderDeviceHandle) == 0);
        U_PORT_TEST_ASSERT(uBleGapRemoveBond(gResponderDeviceHandle, NULL) == 0);
        uint8_t advData[32];
        gAdvCfg.pAdvData = advData;
        int32_t res = uBleGapSetAdvData("BondResp", NULL, 0, advData, sizeof(advData));
        U_PORT_TEST_ASSERT(res > 0);
        gAdvCfg.advDataLength = (uint8_t)res;
        U_PORT_TEST_ASSERT(uBleGapAdvertiseStart(gResponderDeviceHandle, &gAdvCfg) == 0);
        U_PORT_TEST_ASSERT(uBleGapSetPairable(gResponderDeviceHandle, true) == 0);
        uBleGapGetMac(gResponderDeviceHandle, gResponderMacAddr);
    } else {
        // Launch a new instance. By using popen the stdout of the initiator is inherited
        // to the responder and hence all output is merged.
        U_TEST_PRINT_LINE("launching responder application instance...");
        putenv("U_CFG_APP_FILTER=bleBond");
        putenv("U_CFG_TEST_BLE_BOND_OP=2");
        putenv("U_CFG_TEST_SPAWNED=1");
        FILE *procPipe = NULL;
#if defined(_WIN32)
        char *exePath;
        U_PORT_TEST_ASSERT(_get_pgmptr(&exePath) == 0);
        procPipe = _popen(exePath, "w");
        U_PORT_TEST_ASSERT(procPipe != NULL);
#elif defined(__linux__)
        char exePath[100];
        U_PORT_TEST_ASSERT(readlink("/proc/self/exe", exePath, sizeof(exePath)) > 0);
        procPipe = popen(exePath, "w");
#endif
        U_PORT_TEST_ASSERT(procPipe != NULL);
        putenv("U_CFG_TEST_SPAWNED=");

        // Wait for responder to start up.
        char buff[20];
        U_PORT_TEST_ASSERT(uPortNamedPipeReadStr(gPipe, buff, sizeof(buff)) > 0);

        // Exchange mac addresses.
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, pIntToStr(CMD_RESP_MAC)) == 0);
        U_PORT_TEST_ASSERT(uPortNamedPipeReadStr(gPipe, gResponderMacAddr, sizeof(gResponderMacAddr)) > 0);
        snprintf(buff, sizeof(buff), "%d,%s", CMD_INIT_MAC, gInitiatorMacAddr);
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, buff) == 0);
    }
}

static void postamble()
{
    U_TEST_PRINT_LINE("closing down the modules");
    if (gInitiatorDeviceHandle != NULL) {
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(gInitiatorDeviceHandle, U_NETWORK_TYPE_BLE) == 0);
        U_PORT_TEST_ASSERT(uDeviceClose(gInitiatorDeviceHandle, false) == 0);
    }

    if (gResponderDeviceHandle != NULL) {
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(gResponderDeviceHandle, U_NETWORK_TYPE_BLE) == 0);
        U_PORT_TEST_ASSERT(uDeviceClose(gResponderDeviceHandle, false) == 0);
    }

    if (gBondCompleteSemaphore != NULL) {
        uPortSemaphoreDelete(gBondCompleteSemaphore);
        gBondCompleteSemaphore = NULL;
    }
    if (gSyncSemaphore != NULL) {
        uPortSemaphoreDelete(gSyncSemaphore);
        gSyncSemaphore = NULL;
    }

    if (gTestOption == 1) {
        // Close down the spawned responder.
        char buff[10];
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, pIntToStr(CMD_EXIT)) == 0);
        // Wait for confirmation on close.
        U_PORT_TEST_ASSERT(uPortNamedPipeReadStr(gPipe, buff, sizeof(buff)) > 0);
        // Wait a while, initiator final printout should come last and we can't
        // sync as the named pipe must be removed before leakage check.
        U_TEST_PRINT_LINE("waiting for responder to close.");
        uPortTaskBlock(5000);
    } else if (gTestOption == 2) {
        // Send confirmation on close.
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, "Done") == 0);
    }

    if (gTestOption != 0) {
        U_PORT_TEST_ASSERT(uPortNamedPipeDelete(gPipe) == 0);
    }

    uDeviceDeinit();
    uPortDeinit();
    uTestUtilResourceCheck(gTestPrefix, NULL, true);
    int32_t resourceCount = gResourceCountStart - uTestUtilGetDynamicResourceCount();
    U_TEST_PRINT_LINE("we have leaked %d resource(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Different handling for signaling bonding complete has to be applied for old and
* new u-connectXpress. In the old version the module is disconnected after bonding
* and we must wait for that before signaling in order to be able to do subsequent
* bonding operations. In the newer version the connection remains so we signal
* complete directly on the status callback.
*/
static void bondResultCallback(const char *pAddress, int32_t status)
{
    gBondStatus = status;
#ifdef U_UCONNECT_GEN2
    uPortSemaphoreGive(gBondCompleteSemaphore);
#endif
}

static void connectCallback(int32_t connHandle, char *pAddress, bool connected)
{
    (void)connHandle;
    (void)pAddress;
#ifndef U_UCONNECT_GEN2
    if (!connected) {
        uPortSemaphoreGive(gBondCompleteSemaphore);
    }
#endif
}

static void doBond(bool expectSuccess)
{
    // Remove old bonds
    U_PORT_TEST_ASSERT(uBleGapRemoveBond(gInitiatorDeviceHandle, NULL) == 0);
    if (gTestOption == 0) {
        U_PORT_TEST_ASSERT(uBleGapRemoveBond(gResponderDeviceHandle, NULL) == 0);
    } else {
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, pIntToStr(CMD_REM_BOND)) == 0);
    }
    // Start and wait for result callback
    U_PORT_TEST_ASSERT(
        (uBleGapBond(gInitiatorDeviceHandle, gResponderMacAddr, bondResultCallback) == 0) ||
        !expectSuccess
    );
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gBondCompleteSemaphore, 10000) == 0);
    if (expectSuccess) {
        U_PORT_TEST_ASSERT(gBondStatus == U_BT_LE_BOND_ERR_SUCCESS);
    } else {
        U_PORT_TEST_ASSERT(gBondStatus != U_BT_LE_BOND_ERR_SUCCESS);
    }
}

static uDeviceHandle_t oppositeHandle(const char *pAddress)
{
    return (strstr(pAddress, gInitiatorMacAddr) != NULL) ?
           gResponderDeviceHandle :
           gInitiatorDeviceHandle;
}

static void confirmNumber(const char *pAddress, int32_t numericValue)
{
    // Confirm the specified number
    uBleGapBondConfirm(oppositeHandle(pAddress), true, pAddress);
}

static void passKeyEntryCallback(const char *pAddress, int32_t numericValue)
{
    // Incoming passkey
    gPasskey = numericValue;
    if (gTestOption == 0) {
        uPortSemaphoreGive(gSyncSemaphore);
    } else {
        // Send to remote
        char buff[20];
        snprintf(buff, sizeof(buff), "%d,%d", CMD_ENTER_PASS, gPasskey);
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, buff) == 0);
    }
}

static void passkeyRequestCallback(const char *pAddress)
{
    // Wait for and then send the passkey received from the counterpart
    if (gTestOption != 2) {
        U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSyncSemaphore, 10000) == 0);
        uBleGapBondEnterPasskey(oppositeHandle(pAddress), true, pAddress, gPasskey);
    }
}

static void setParams(uDeviceHandle_t handle, int32_t cap, int32_t sec, bool pairable)
{
    if (handle != NULL) {
        U_PORT_TEST_ASSERT(uBleSetBondParameters(
                               handle,
                               cap,
                               sec,
                               confirmNumber,
                               passkeyRequestCallback,
                               passKeyEntryCallback
                           ) == 0);
        U_PORT_TEST_ASSERT(uBleGapSetPairable(handle, pairable) == 0);
    } else if (gInitiatorDeviceHandle != NULL) {
        char buff[20];
        memset(buff, 0, sizeof(buff));
        snprintf(buff, sizeof(buff) - 1, CMD_SETPARAM_FORM, CMD_SETPARAM, cap, sec, pairable);
        U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, buff) == 0);
    }
}

static void runAsInitiator()
{
    uBleGapSetConnectCallback(gInitiatorDeviceHandle, connectCallback);
    // Test some of the combinations from the bonding matrix at:
    // https://www.bluetooth.com/blog/bluetooth-pairing-part-2-key-generation-methods/

    // -- No security --

    // Pairing

    U_TEST_PRINT_LINE("Pairing enabled only on one side, should fail");
    setParams(gInitiatorDeviceHandle, U_BT_LE_IO_NONE, U_BT_LE_BOND_NO_SEC, true);
    setParams(gResponderDeviceHandle, U_BT_LE_IO_NONE, U_BT_LE_BOND_NO_SEC, false);
    doBond(false);
    U_TEST_PRINT_LINE("Pairing enabled on both sides");
    setParams(gResponderDeviceHandle, U_BT_LE_IO_NONE, U_BT_LE_BOND_NO_SEC, true);
    doBond(true);

    U_TEST_PRINT_LINE("Just works");
    setParams(gInitiatorDeviceHandle, U_BT_LE_IO_NONE, U_BT_LE_BOND_UNAUTH, true);
    setParams(gResponderDeviceHandle, U_BT_LE_IO_NONE, U_BT_LE_BOND_UNAUTH, true);
    doBond(true);

    // -- Security --

    U_TEST_PRINT_LINE("Security, initiator DISP_ONLY, responder KEYB_ONLY");
    setParams(gInitiatorDeviceHandle, U_BT_LE_IO_DISP_ONLY, U_BT_LE_BOND_AUTH, true);
    setParams(gResponderDeviceHandle, U_BT_LE_IO_KEYB_ONLY, U_BT_LE_BOND_AUTH, true);
    doBond(true);

    U_TEST_PRINT_LINE("Security MITM protection, initiator YES_NO, responder YES_NO");
    setParams(gInitiatorDeviceHandle, U_BT_LE_IO_DISP_YES_NO, U_BT_LE_BOND_AUTH_ENCR, true);
    setParams(gResponderDeviceHandle, U_BT_LE_IO_DISP_YES_NO, U_BT_LE_BOND_AUTH_ENCR, true);
    doBond(true);
}

static void runAsResponder()
{
    char cmd[50];
    U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, "Ready") == 0);
    while (uPortNamedPipeReadStr(gPipe, cmd, sizeof(cmd)) >= 0) {
        if (strlen(cmd) > 0) {
            // Incoming command
            int32_t op;
            U_PORT_TEST_ASSERT(sscanf(cmd, "%d", &op) == 1);
            if (op == CMD_SETPARAM) {
                int32_t cap, sec, pairable;
                int cnt = sscanf(cmd, CMD_SETPARAM_FORM, &op, &cap, &sec, &pairable);
                U_PORT_TEST_ASSERT(cnt == 4);
                setParams(gResponderDeviceHandle, cap, sec, pairable == 1);
            } else if (op == CMD_RESP_MAC) {
                U_PORT_TEST_ASSERT(uPortNamedPipeWriteStr(gPipe, gResponderMacAddr) == 0);
            } else if (op == CMD_INIT_MAC) {
                U_PORT_TEST_ASSERT(sscanf(cmd, "%d,%s", &op, gInitiatorMacAddr) == 2);
            } else if (op == CMD_ENTER_PASS) {
                sscanf(cmd + 2, "%d", &gPasskey);
                uBleGapBondEnterPasskey(gResponderDeviceHandle, true, gInitiatorMacAddr, gPasskey);
            } else if (op == CMD_REM_BOND) {
                U_PORT_TEST_ASSERT(uBleGapRemoveBond(gResponderDeviceHandle, NULL) == 0);
            } else {
                // Exit or unknown command
                break;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** BLE bonding test.
 */
U_PORT_TEST_FUNCTION("[bleBond]", "bleBond")
{
    const char *envOpt = getenv("U_CFG_TEST_BLE_BOND_OP");
    if (envOpt != NULL) {
        U_PORT_TEST_ASSERT(sscanf(envOpt, "%d", &gTestOption) == 1);
    } else {
#ifndef U_UCONNECT_GEN2
        // Older versions of u-connectXpress can only run in separate instances
        gTestOption = 1;
#endif
    }
    if (gTestOption == 0) {
        snprintf(gTestPrefix, sizeof(gTestPrefix) - 1, "%s: ", U_TEST_PREFIX);
    } else {
        snprintf(gTestPrefix, sizeof(gTestPrefix) - 1, "%s(%d): ", U_TEST_PREFIX, gTestOption);
    }
    preamble();
    if (gInitiatorDeviceHandle != NULL) {
        runAsInitiator();
    } else {
        runAsResponder();
    }
    postamble();
}

#endif

// End of file
