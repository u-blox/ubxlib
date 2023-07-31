/*
 * Copyright 2023 u-blox
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
 * @brief Tests for the BLE NUS API: these should pass on all
 * platforms where one UART is available. Testing the NUS API implicitly tests
 * both the GAP and GATT APIs as well
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */
#ifdef U_CFG_OVERRIDE
#include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stdint.h"  // int32_t etc.

// Must always be included before u_short_range_test_selector.h
// lint -efile(766, u_ble_module_type.h)
#include "u_ble_module_type.h"
#include "u_short_range_test_selector.h"

#if U_SHORT_RANGE_TEST_BLE() && defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE)
#include "stddef.h"
#include "stdbool.h"
#include "string.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"
#include "u_ble.h"

#include "u_ble_cfg.h"

#include "u_network.h"
#include "u_network_config_ble.h"

#include "u_ble_cfg.h"
#include "u_ble.h"
#include "u_ble_gap.h"
#include "u_ble_gatt.h"
#include "u_ble_nus.h"


/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_BLE_NUS_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#define EXT_SERVER_NAME "UbxExtNusServer"
#define EXT_SERVER_COMMAND "Hello"

#define INT_SERVER_NAME "UbxDutNusServer"
#define INT_SERVER_COMMAND "Hello from DUT"

#define HAS_RESPONSE (gPeerResponse[0] != 0)
#define SERVER_FOUND (gPeerMac[0] != 0)
// Connection wait time in seconds. The external server and client may be busy
#define PEER_WAIT_TIME_S 100

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uDeviceHandle_t gDeviceHandle;
static uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
        }
    },
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

static uNetworkCfgBle_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_BLE,
    .spsServer = false,
};

static uBleGapAdvConfig_t gAdvCfg = {
    .minIntervalMs = 200,
    .maxIntervalMs = 200,
    .connectable = true,
    .maxClients = 1,
    .pAdvData = NULL,
    .advDataLength = 0
};


static char gPeerMac[U_SHORT_RANGE_BT_ADDRESS_SIZE] = {0};
static char gPeerResponse[100] = {0};

static int32_t gHeapStartSize;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static bool scanResponse(uBleScanResult_t *pScanResult)
{
    if (strstr(pScanResult->name, EXT_SERVER_NAME)) {
        strncpy(gPeerMac, pScanResult->address, sizeof(gPeerMac));
        return false;
    }
    return true;
}

static void peerIncoming(uint8_t *pValue, uint8_t valueSize)
{
    uint8_t maxSize = sizeof(gPeerResponse) - 1;
    if (valueSize > maxSize) {
        valueSize = maxSize;
    }
    memcpy(gPeerResponse, pValue, valueSize);
    gPeerResponse[valueSize] = 0;
}

static void preamble(int32_t role)
{
    uPortDeinit();
    gHeapStartSize = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);
    U_TEST_PRINT_LINE("initiating the module");
    U_PORT_TEST_ASSERT(uDeviceOpen(&gDeviceCfg, &gDeviceHandle) == 0);
    U_TEST_PRINT_LINE("initiating BLE");
    gNetworkCfg.role = role;
    U_PORT_TEST_ASSERT(uNetworkInterfaceUp(gDeviceHandle, U_NETWORK_TYPE_BLE, &gNetworkCfg) == 0);
    gPeerResponse[0] = 0;
}

static void postamble()
{
    U_TEST_PRINT_LINE("closing down the module");
    U_PORT_TEST_ASSERT(uBleNusDeInit() == 0);
    U_PORT_TEST_ASSERT(uBleGapReset(gDeviceHandle) == 0);
    U_PORT_TEST_ASSERT(uDeviceClose(gDeviceHandle, false) == 0);
    uDeviceDeinit();
    uPortDeinit();
#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    int32_t heapUsed = gHeapStartSize - uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */


/** BLE NUS client test.
 */
U_PORT_TEST_FUNCTION("[bleNus]", "bleNusClient")
{
    preamble(U_BLE_CFG_ROLE_CENTRAL);
    U_TEST_PRINT_LINE("scanning for server");
    gPeerMac[0] = 0;
    for (uint32_t i = 0; !SERVER_FOUND && i < PEER_WAIT_TIME_S / 10; i++) {
        U_TEST_PRINT_LINE("try #%d", i + 1);
        U_PORT_TEST_ASSERT(uBleGapScan(gDeviceHandle,
                                       U_BLE_GAP_SCAN_DISCOVER_ALL_ONCE,
                                       true, 10000,
                                       scanResponse) == 0);
    }
    U_PORT_TEST_ASSERT(SERVER_FOUND);
    // BLE connection may fail so do multiple tries if that happens
    bool ok = false;
    for (uint32_t i = 0; !ok && i < 3; i++) {
        U_TEST_PRINT_LINE("connecting to: %s, try #%d", gPeerMac, i + 1);
        if (uBleNusInit(gDeviceHandle, gPeerMac, peerIncoming) == 0) {
            ok = true;
        } else {
            U_TEST_PRINT_LINE("failed to initiate NUS server connection");
            uBleNusDeInit();
            uPortTaskBlock(2000);
        }
    }
    U_PORT_TEST_ASSERT(ok);
    uPortTaskBlock(2000);
    U_TEST_PRINT_LINE("sending command: %s", EXT_SERVER_COMMAND);
    U_PORT_TEST_ASSERT(uBleNusWrite(EXT_SERVER_COMMAND,
                                    (uint8_t)(strlen(EXT_SERVER_COMMAND) + 1)) == 0);
    U_TEST_PRINT_LINE("waiting for server response");
    uPortTaskBlock(2000);
    if (HAS_RESPONSE) {
        U_TEST_PRINT_LINE("server response: %s", gPeerResponse);
    } else {
        U_TEST_PRINT_LINE("No server response before timeout");
    }
    U_PORT_TEST_ASSERT(HAS_RESPONSE);
    postamble();
}

/** BLE NUS server test.
 */
U_PORT_TEST_FUNCTION("[bleNus]", "bleNusServer")
{
    int32_t x;
    preamble(U_BLE_CFG_ROLE_PERIPHERAL);
    U_TEST_PRINT_LINE("init NUS Service");
    x = uBleNusInit(gDeviceHandle, NULL, peerIncoming);
    U_PORT_TEST_ASSERT((x == 0) || (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
    if (x == 0) {
        U_TEST_PRINT_LINE("init advertising data");
        uint8_t manufData[] = {1, 2, 3, 4};
        uint8_t advData[32];
        uint8_t respData[32];
        gAdvCfg.pRespData = respData;
        gAdvCfg.respDataLength = uBleNusSetAdvData(respData, sizeof(respData));
        gAdvCfg.pAdvData = advData;
        gAdvCfg.advDataLength = uBleGapSetAdvData(INT_SERVER_NAME,
                                                  manufData, sizeof(manufData),
                                                  advData, sizeof(advData));
        U_PORT_TEST_ASSERT(gAdvCfg.respDataLength > 0 && gAdvCfg.advDataLength > 0);
        U_TEST_PRINT_LINE("start advertising");
        U_PORT_TEST_ASSERT(uBleGapAdvertiseStart(gDeviceHandle, &gAdvCfg) == 0);
        U_TEST_PRINT_LINE("waiting for client connection");
        uint32_t waitCnt = 0;
        while (!HAS_RESPONSE && waitCnt++ < PEER_WAIT_TIME_S) {
            uPortTaskBlock(1000);
        }
        if (HAS_RESPONSE) {
            U_TEST_PRINT_LINE("client sent: %s", gPeerResponse);
            U_TEST_PRINT_LINE("sending response: %s", INT_SERVER_COMMAND);
            U_PORT_TEST_ASSERT(uBleNusWrite(INT_SERVER_COMMAND,
                                            (uint8_t)(strlen(INT_SERVER_COMMAND) + 1)) == 0);
            // Wait for client disconnect
            uPortTaskBlock(3000);
        } else {
            U_TEST_PRINT_LINE("No client response before timeout");
        }
    } else {
        U_TEST_PRINT_LINE("module does not support NUS server, not testing it");
    }
    postamble();
    U_PORT_TEST_ASSERT((x == (int32_t) (U_ERROR_COMMON_NOT_SUPPORTED)) || HAS_RESPONSE);
}

#endif

// End of file
