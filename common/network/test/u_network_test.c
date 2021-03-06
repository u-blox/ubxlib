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
 * @brief Test for the network API: these should pass on all platforms
 * that include the appropriate communications hardware, i.e. at least
 * one of cellular or short range.  These tests use the sockets API to
 * prove that communication is possible over the network that has
 * been brought into existence.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strcmp()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
//lint -efile(766, u_cfg_os_platform_specific.h)
// Suppress header file not used: one of the defines
// in it (U_CFG_OS_CLIB_LEAKS) is.
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#ifdef U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS
#include "u_ble_data.h"
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_sock.h"           // In order to prove that we can do
#include "u_sock_test_shared_cfg.h"  // something with an "up" network

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** An echo test string.
 */
static const char gTestString[] = "Hello from u-blox.";

//lint -esym(843, gSystemHeapLost) Suppress could be declared as const, which will be the
// case if U_CFG_OS_CLIB_LEAKS is not defined.
/** For tracking heap lost to memory  lost by the C library.
 */
static size_t gSystemHeapLost = 0;

#ifdef U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS
/** The macro U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS should be set to the
 * address of the BLE test peer WITHOUT quotation marks, e.g.
 * U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS=2462ABB6CC42p.  If the macro is
 * not defined then no network test of BLE will be run.
 */
static const char gRemoteSpsAddress[] = U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS);
static const char gTestData[] =  "_____0000:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0100:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0200:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0300:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0400:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0500:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0600:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0700:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0800:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0900:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789";

static volatile int32_t gConnHandle = -1;
static volatile int32_t gBytesReceived;
static volatile int32_t gErrors = 0;
static volatile uint32_t gIndexInBlock;
static const int32_t gTotalData = 400;
static volatile int32_t gBytesSent;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS

// Print out text.
static void print(const char *pStr, size_t length)
{
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        if (!isprint((int32_t) c)) {
            // Print the hex
            uPortLog("[%02x]", c);
        } else {
            // Print the ASCII character
            uPortLog("%c", c);
        }
    }
}

//lint -e{818} Suppress 'pData' could be declared as const:
// need to follow function signature
static void dataCallback(int32_t channel, size_t length,
                         char *pData, void *pParameters)
{
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;
#endif

    (void) channel;
    (void) pParameters;

#if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
#endif

    uPortLog("U_NETWORK_TEST: received %d byte(s):\n", length);
    print(pData, length);
    uPortLog("\n");
    uPortLog("U_NETWORK_TEST: expected:\n");
    print(gTestData + gIndexInBlock, length);
    uPortLog("\n");

    // Compare the data with the expected data
    for (uint32_t x = 0; (x < length); x++) {
        gBytesReceived++;
        if (gTestData[gIndexInBlock] == *(pData + x)) {
            gIndexInBlock++;
            if (gIndexInBlock >= sizeof(gTestData) - 1) {
                gIndexInBlock = 0;
            }
        } else {
            gErrors++;
        }
    }
    uPortLog("U_NETWORK_TEST: received %d byte(s) of %d, error %d.\n",
             length, gBytesReceived, gErrors);

#if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
#endif
}

static void connectionCallback(int32_t connHandle, char *address, int32_t type,
                               int32_t channel, int32_t mtu, void *pParameters)
{
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;
#endif

    (void) address;
    (void) mtu;
    int32_t bytesToSend;
    int32_t *pHandle = (int32_t *) pParameters;

#if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
#endif

    if (type == 0) {
        // Wait a moment for the other end may not be immediately ready
        uPortTaskBlock(100);
        gConnHandle = connHandle;
        while (gBytesSent < gTotalData) {
            // -1 to omit gTestData string terminator
            bytesToSend = sizeof(gTestData) - 1;
            if (bytesToSend > gTotalData - gBytesSent) {
                bytesToSend = gTotalData - gBytesSent;
            }
            uBleDataSend(*pHandle, channel, gTestData, bytesToSend);

            gBytesSent += bytesToSend;
            uPortLog("U_NETWORK_TEST: %d byte(s) sent.\n", gBytesSent);
        }
    } else if (type == 1) {
        gConnHandle = -1;
        uPortLog("U_NETWORK_TEST: disconnected connection handle %d.\n", connHandle);
    }

#if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
#endif
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test everything; there isn't much.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[network]", "networkTest")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    int32_t networkHandle;
    uSockDescriptor_t descriptor;
    uSockAddress_t address;
    char buffer[32];
    int32_t y;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
        if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
            uPortLog("U_NETWORK_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                     gUNetworkTestCfg[x].pConfiguration);
            U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
        }
    }

    // Do this twice to prove that we can go from down
    // back to up again
    for (size_t a = 0; a < 2; a++) {
        // Bring up each network type
        for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {

            if (gUNetworkTestCfg[x].handle >= 0) {
                pNetworkCfg = &(gUNetworkTestCfg[x]);
                networkHandle = pNetworkCfg->handle;

                uPortLog("U_NETWORK_TEST: bringing up %s...\n",
                         gpUNetworkTestTypeName[pNetworkCfg->type]);
                U_PORT_TEST_ASSERT(uNetworkUp(networkHandle) == 0);

                if (pNetworkCfg->type == U_NETWORK_TYPE_CELL ||
                    pNetworkCfg->type == U_NETWORK_TYPE_WIFI) {

                    uPortLog("U_NETWORK_TEST: looking up echo server \"%s\"...\n",
                             U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
                    // Look up the address of the server we use for UDP echo
                    // The first call to a sockets API needs to
                    // initialise the underlying sockets layer; take
                    // account of that initialisation heap cost here.
                    heapSockInitLoss += uPortGetHeapFree();
                    // Look up the address of the server we use for UDP echo
                    U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                          U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                          &(address.ipAddress)) == 0);
                    heapSockInitLoss -= uPortGetHeapFree();

                    // Add the port number we will use
                    address.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

                    // Create a UDP socket
                    descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_DGRAM,
                                             U_SOCK_PROTOCOL_UDP);

                    // Send and wait for the UDP echo data, trying a few
                    // times to reduce the chance of internet loss getting
                    // in the way
                    uPortLog("U_NETWORK_TEST: sending %d byte(s) to %s:%d over"
                             " %s...\n", sizeof(gTestString) - 1,
                             U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                             U_SOCK_TEST_ECHO_UDP_SERVER_PORT,
                             gpUNetworkTestTypeName[pNetworkCfg->type]);
                    y = 0;
                    memset(buffer, 0, sizeof(buffer));
                    for (size_t z = 0; (z < U_SOCK_TEST_UDP_RETRIES) &&
                         (y != sizeof(gTestString) - 1); z++) {
                        y = uSockSendTo(descriptor, &address, gTestString,
                                        sizeof(gTestString) - 1);
                        if (y == sizeof(gTestString) - 1) {
                            // Wait for the answer
                            y = 0;
                            for (size_t w = 10; (w > 0) &&
                                 (y != sizeof(gTestString) - 1); w--) {
                                y = uSockReceiveFrom(descriptor, NULL,
                                                     buffer, sizeof(buffer));
                                if (y <= 0) {
                                    uPortTaskBlock(1000);
                                }
                            }
                            if (y != sizeof(gTestString) - 1) {
                                uPortLog("U_NETWORK_TEST: failed to receive UDP echo"
                                         " on try %d.\n", z + 1);
                            }
                        } else {
                            uPortLog("U_NETWORK_TEST: failed to send UDP data on"
                                     " try %d.\n", z + 1);
                        }
                    }
                    uPortLog("U_NETWORK_TEST: %d byte(s) echoed over UDP on %s.\n",
                             y, gpUNetworkTestTypeName[pNetworkCfg->type]);
                    U_PORT_TEST_ASSERT(y == sizeof(gTestString) - 1);
                    U_PORT_TEST_ASSERT(strcmp(buffer, gTestString) == 0);

                    // Close the socket
                    U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);

                    // Clean up to ensure no memory leaks
                    uSockCleanUp();

#ifdef U_BLE_TEST_CFG_REMOTE_SPS_ADDRESS
                } else if (pNetworkCfg->type == U_NETWORK_TYPE_BLE) {

                    gBytesSent = 0;
                    gBytesReceived = 0;
                    gIndexInBlock = 0;

                    uBleDataSetCallbackConnectionStatus(gUNetworkTestCfg[x].handle,
                                                        connectionCallback,
                                                        &gUNetworkTestCfg[x].handle);
                    uBleDataSetCallbackData(gUNetworkTestCfg[x].handle, dataCallback,
                                            &gUNetworkTestCfg[x].handle);

                    U_PORT_TEST_ASSERT(uBleDataConnectSps(gUNetworkTestCfg[x].handle,
                                                          gRemoteSpsAddress) == 0);

                    y = 100;
                    while ((gBytesSent < gTotalData) && (y > 0)) {
                        uPortTaskBlock(100);
                        y--;
                    };
                    // All sent, give some time to finish receiving
                    uPortTaskBlock(2000);

                    U_PORT_TEST_ASSERT(gBytesSent == gTotalData);
                    U_PORT_TEST_ASSERT(gTotalData == gBytesReceived);
                    U_PORT_TEST_ASSERT(gErrors == 0);

                    // Disconnect
                    U_PORT_TEST_ASSERT(uBleDataDisconnect(gUNetworkTestCfg[x].handle, gConnHandle) == 0);
                    for (int32_t i = 0; (i < 40) && (gConnHandle != -1); i++) {
                        uPortTaskBlock(100);
                    };

                    uBleDataSetCallbackData(gUNetworkTestCfg[x].handle, NULL, NULL);
                    uBleDataSetCallbackConnectionStatus(gUNetworkTestCfg[x].handle, NULL, NULL);

                    U_PORT_TEST_ASSERT(gConnHandle == -1);
#endif
                }
            }
        }

        // Remove each network type
        for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
            if (gUNetworkTestCfg[x].handle >= 0) {
                uPortLog("U_NETWORK_TEST: taking down %s...\n",
                         gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                U_PORT_TEST_ASSERT(uNetworkDown(gUNetworkTestCfg[x].handle) == 0);
            }
        }
    }

    uNetworkDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_NETWORK_TEST: %d byte(s) of heap were lost to"
             " the C library during this test, %d byte(s) were"
             " lost to sockets initialisation and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost, heapSockInitLoss,
             heapUsed - (gSystemHeapLost + heapSockInitLoss ));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= (int32_t) (gSystemHeapLost +
                                               heapSockInitLoss)));
#else
    (void) gSystemHeapLost;
    (void) heapSockInitLoss;
    (void) heapUsed;
#endif
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[network]", "networkCleanUp")
{
    int32_t y;

    // The network test configuration is shared
    // between the network and sockets tests so
    // must reset the handles here in case the
    // sockets API tests are coming next.
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    uPortLog("U_NETWORK_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", y);
    U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_NETWORK_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
