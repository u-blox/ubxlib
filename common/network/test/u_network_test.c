/*
 * Copyright 2020 u-blox
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
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)
#include "u_ble_data.h"
#include "u_error_common.h"
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_sock.h"                  // In order to prove that we can do something
#include "u_sock_test_shared_cfg.h"  // with an "up" network that can support sockets

#include "u_location.h"                  // In order to prove that we can do something
#include "u_location_test_shared_cfg.h"  // with an "up" network that supports location

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

/** One of the macro U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL or U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL
 * should be set to the address of the BLE test peer WITHOUT quotation marks, e.g.
 * U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL=2462ABB6CC42p.  If none of the macros are
 * defined then no network test of BLE will be run.
 */
#ifdef U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL
static const char gRemoteSpsAddress[] =
    U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL);
#else
#ifdef U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL
static const char gRemoteSpsAddress[] =
    U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL);
#endif
#endif
#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)
#define U_BLE_TEST_TEST_DATA_LOOPS 2
static const char gTestData[] =  "_____0000:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0001:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0002:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0003:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0004:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0005:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0006:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0007:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0008:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____0009:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "abcdefg";
// Make sure the test data is not a multiple of the MTU
// so we test packets smaller than MTU as well as MTU sized packets


static volatile int32_t gConnHandle;
static volatile int32_t gBytesReceived;
static volatile int32_t gErrors = 0;
static volatile uint32_t gIndexInBlock;
static const int32_t gTotalBytes = (sizeof gTestData - 1) * U_BLE_TEST_TEST_DATA_LOOPS;
static volatile int32_t gBytesSent;
static volatile int32_t gChannel;
static volatile size_t gBleHandle;
static uPortSemaphoreHandle_t gBleConnectionSem = NULL;
#endif

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Keep track of the current network handle so that the
 * keepGoingCallback() can check it.
 */
static int32_t gNetworkHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

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
    uPortLog("\n");
}

static void sendBleData(int32_t handle)
{
    uint32_t tries = 0;
    int32_t testDataOffset = 0;
    uPortLog("U_NETWORK_TEST: Sending data on channel %d...\n", gChannel);
    while ((tries++ < 15) && (gBytesSent < gTotalBytes)) {
        // -1 to omit gTestData string terminator
        int32_t bytesSentNow =
            uBleDataSend(handle, gChannel, gTestData + testDataOffset, sizeof gTestData - 1 - testDataOffset);

        if (bytesSentNow >= 0) {
            gBytesSent += bytesSentNow;
            testDataOffset += bytesSentNow;
            if (testDataOffset >= sizeof gTestData - 1) {
                testDataOffset -= sizeof gTestData - 1;
            }
        } else {
            uPortLog("U_NETWORK_TEST: Error sending Data!!\n");
        }
        uPortLog("U_NETWORK_TEST: %d byte(s) sent.\n", gBytesSent);

        // Make room for context switch letting receive event process
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }
    if (gBytesSent < gTotalBytes) {
        uPortLog("U_NETWORK_TEST: %d byte(s) were not sent.\n", gTotalBytes - gBytesSent);
    }
}

//lint -e{818} Suppress 'pData' could be declared as const:
// need to follow function signature
static void bleDataCallback(int32_t channel, void *pParameters)
{
    char buffer[100];
    int32_t length;
#if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    int32_t heapClibLoss = uPortGetHeapFree();
#endif
    (void)pParameters;

    U_PORT_TEST_ASSERT(channel == gChannel);

    do {
        length = uBleDataReceive(gBleHandle, channel, buffer, sizeof buffer);
        if (length > 0) {
            int32_t errorOrigDataStartIndex = -1;
            int32_t errorRecDataStartIndex = -1;
            int32_t errorLength = -1;

            // Compare the data with the expected data
            for (int32_t x = 0; (x < length); x++) {
                gBytesReceived++;
                if (gTestData[gIndexInBlock] != buffer[x]) {
                    if (errorOrigDataStartIndex < 0) {
                        errorOrigDataStartIndex = gIndexInBlock;
                        errorRecDataStartIndex = x;
                        errorLength = 1;
                    } else {
                        errorLength++;
                    }
                    gErrors++;
                }
                if (++gIndexInBlock >= sizeof gTestData - 1) {
                    gIndexInBlock -= sizeof gTestData - 1;
                }
            }

            uPortLog("U_NETWORK_TEST: received %d bytes (total %d with %d errors).\n", length, gBytesReceived,
                     gErrors);
            if (errorOrigDataStartIndex >= 0) {
                uPortLog("U_NETWORK_TEST: expected:\n");
                print(gTestData + errorOrigDataStartIndex, errorLength);
                uPortLog("U_NETWORK_TEST: got:\n");
                print(buffer + errorRecDataStartIndex, errorLength);
            }
        }
    } while (length > 0);
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

    (void)address;
    (void)mtu;
    (void)pParameters;

#if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
#endif

    if (type == 0) {
        gConnHandle = connHandle;
        gChannel = channel;
        uPortLog("U_NETWORK_TEST: connected %s handle %d (channel %d).\n", address, connHandle, channel);
    } else if (type == 1) {
        gConnHandle = -1;
        if (connHandle != U_BLE_DATA_INVALID_HANDLE) {
            uPortLog("U_NETWORK_TEST: disconnected connection handle %d.\n", connHandle);
        } else {
            uPortLog("U_NETWORK_TEST: connection attempt failed\n");
        }
    }
    if (gBleConnectionSem) {
        uPortSemaphoreGive(gBleConnectionSem);
    }

#if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
#endif
}
#endif // #if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

// Callback function for location establishment process.
static bool keepGoingCallback(int32_t networkHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT(networkHandle == gNetworkHandle);
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test networks that support sockets.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[network]", "networkSock")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    int32_t networkHandle = -1;
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

    // Add the networks that support sockets
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
        if ((*((uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            uPortLog("U_NETWORK_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
#if (U_CFG_APP_GNSS_UART < 0)
            // If there is no GNSS UART then any GNSS chip must
            // be connected via the cellular module's AT interface
            // hence we capture the cellular network handle here and
            // modify the GNSS configuration to use it before we add
            // the GNSS network
            uNetworkTestGnssAtConfiguration(networkHandle,
                                            gUNetworkTestCfg[x].pConfiguration);
#endif
            gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                     gUNetworkTestCfg[x].pConfiguration);
            U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
            if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                networkHandle = gUNetworkTestCfg[x].handle;
            }
        }
    }

    // It is possible for socket closure in an
    // underlying layer to have failed in a previous
    // test, leaving sockets hanging, so just in case,
    // clear them up here
    uSockDeinit();

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
                U_PORT_TEST_ASSERT(descriptor >= 0);

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
            }
        }

        // Remove each network type, in reverse order so
        // that GNSS (which might be connected via a cellular
        // module) is taken down before cellular
        for (int32_t x = (int32_t) gUNetworkTestCfgSize - 1; x >= 0; x--) {
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

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)
/** Test BLE network.
 */
U_PORT_TEST_FUNCTION("[network]", "networkBle")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    int32_t networkHandle;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t timeoutCount;
    uBleDataSpsHandles_t spsHandles;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add a BLE network
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
        if ((*((uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) &&
            (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_BLE)) {
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
        // Bring up the BLE network
        for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
            if (gUNetworkTestCfg[x].handle >= 0) {
                pNetworkCfg = &(gUNetworkTestCfg[x]);
                networkHandle = pNetworkCfg->handle;

                uPortLog("U_NETWORK_TEST: bringing up %s...\n",
                         gpUNetworkTestTypeName[pNetworkCfg->type]);
                U_PORT_TEST_ASSERT(uNetworkUp(networkHandle) == 0);

                memset(&spsHandles, 0x00, sizeof spsHandles);

                gConnHandle = -1;
                gBytesSent = 0;
                gBytesReceived = 0;
                gIndexInBlock = 0;
                U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gBleConnectionSem, 0, 1) == 0);

                uBleDataSetCallbackConnectionStatus(gUNetworkTestCfg[x].handle,
                                                    connectionCallback,
                                                    &gUNetworkTestCfg[x].handle);
                uBleDataSetDataAvailableCallback(gUNetworkTestCfg[x].handle, bleDataCallback,
                                                 &gUNetworkTestCfg[x].handle);
                gBleHandle = gUNetworkTestCfg[x].handle;

                for (int32_t i = 0; i < 3; i++) {
                    if (i > 0) {
                        if (uBleDataPresetSpsServerHandles(gUNetworkTestCfg[x].handle, &spsHandles) ==
                            U_ERROR_COMMON_NOT_IMPLEMENTED) {
                            continue;
                        }
                    }
                    if (i > 1) {
                        if (uBleDataDisableFlowCtrlOnNext(gUNetworkTestCfg[x].handle) ==
                            U_ERROR_COMMON_NOT_IMPLEMENTED) {
                            continue;
                        }
                    }
                    for (size_t tries = 0; tries < 3; tries++) {
                        int32_t result;
                        // Use first testrun(up/down) to test default connection parameters
                        // and the second for using non-default.
                        if (a == 0) {
                            uPortLog("U_NETWORK_TEST: Connecting SPS: %s\n", gRemoteSpsAddress);
                            result = uBleDataConnectSps(gUNetworkTestCfg[x].handle,
                                                        gRemoteSpsAddress,
                                                        NULL);
                        } else {
                            uBleDataConnParams_t connParams;
                            connParams.scanInterval = 64;
                            connParams.scanWindow = 64;
                            connParams.createConnectionTmo = 5000;
                            connParams.connIntervalMin = 28;
                            connParams.connIntervalMax = 34;
                            connParams.connLatency = 0;
                            connParams.linkLossTimeout = 2000;
                            uPortLog("U_NETWORK_TEST: Connecting SPS with conn params: %s\n", gRemoteSpsAddress);
                            result = uBleDataConnectSps(gUNetworkTestCfg[x].handle,
                                                        gRemoteSpsAddress, &connParams);
                        }

                        if (result == 0) {
                            // Wait for connection
                            uPortSemaphoreTryTake(gBleConnectionSem, 10000);
                            if (gConnHandle != -1) {
                                break;
                            }
                        } else {
                            // Just wait a bit and try again...
                            uPortTaskBlock(5000);
                        }
                    }

                    if (gConnHandle == -1) {
                        uPortLog("U_NETWORK_TEST: All SPS connection attempts failed!\n");
                        U_PORT_TEST_ASSERT(false);
                    }
                    if (i == 0) {
                        uBleDataGetSpsServerHandles(gUNetworkTestCfg[x].handle, gChannel, &spsHandles);
                    }

                    uBleDataSetSendTimeout(gUNetworkTestCfg[x].handle, gChannel, 100);
                    uPortTaskBlock(100);
                    timeoutCount = 0;
                    sendBleData(gUNetworkTestCfg[x].handle);
                    while (gBytesReceived < gBytesSent) {
                        uPortTaskBlock(10);
                        if (timeoutCount++ > 100) {
                            break;
                        }
                    }
                    U_PORT_TEST_ASSERT(gBytesSent == gTotalBytes);
                    U_PORT_TEST_ASSERT(gBytesSent == gBytesReceived);
                    U_PORT_TEST_ASSERT(gErrors == 0);
                    // Disconnect
                    U_PORT_TEST_ASSERT(uBleDataDisconnect(gUNetworkTestCfg[x].handle, gConnHandle) == 0);
                    for (int32_t i = 0; (i < 40) && (gConnHandle != -1); i++) {
                        uPortTaskBlock(100);
                    }
                    gBytesSent = 0;
                    gBytesReceived = 0;
                    U_PORT_TEST_ASSERT(gConnHandle == -1);
                }

                uBleDataSetDataAvailableCallback(gUNetworkTestCfg[x].handle, NULL, NULL);
                uBleDataSetCallbackConnectionStatus(gUNetworkTestCfg[x].handle, NULL, NULL);

                U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gBleConnectionSem) == 0);
                gBleConnectionSem = NULL;
            }
        }

        // Remove the BLE network
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
#endif // #if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

/** Test networks that support location.
 */
U_PORT_TEST_FUNCTION("[network]", "networkLoc")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    int32_t networkHandle = -1;
    const uLocationTestCfg_t *pLocationCfg;
    int32_t y;
    uLocation_t location;
    int64_t startTime;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add the networks that support location
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
        if ((*((uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) &&
            U_NETWORK_TEST_TYPE_HAS_LOCATION(gUNetworkTestCfg[x].type)) {
            uPortLog("U_NETWORK_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
#if (U_CFG_APP_GNSS_UART < 0)
            // If there is no GNSS UART then any GNSS chip must
            // be connected via the cellular module's AT interface
            // hence we capture the cellular network handle here and
            // modify the GNSS configuration to use it before we add
            // the GNSS network
            uNetworkTestGnssAtConfiguration(networkHandle,
                                            gUNetworkTestCfg[x].pConfiguration);
#endif
            gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                     gUNetworkTestCfg[x].pConfiguration);
            U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
            if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                networkHandle = gUNetworkTestCfg[x].handle;
            }
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
                if (gpULocationTestCfg[gUNetworkTestCfg[x].type]->numEntries > 0) {
                    // Just take the first one, we don't care which as this
                    // is a network test not a location test
                    pLocationCfg = gpULocationTestCfg[gUNetworkTestCfg[x].type]->pCfgData[0];
                    startTime = uPortGetTickTimeMs();
                    gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
                    uLocationTestResetLocation(&location);
                    uPortLog("U_NETWORK_TEST: getting location using %s.\n",
                             gpULocationTestTypeStr[pLocationCfg->locationType]);
                    gNetworkHandle = networkHandle;
                    y = uLocationGet(networkHandle, pLocationCfg->locationType,
                                     pLocationCfg->pLocationAssist,
                                     pLocationCfg->pAuthenticationTokenStr,
                                     &location, keepGoingCallback);
                    if (y == 0) {
                        uPortLog("U_NETWORK_TEST: location establishment took %d second(s).\n",
                                 (int32_t) (uPortGetTickTimeMs() - startTime) / 1000);
                    }
                    // If we are running on a local cellular network we won't get position but
                    // we should always get time
                    if ((location.radiusMillimetres > 0) &&
                        (location.radiusMillimetres <= U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES)) {
                        uLocationTestPrintLocation(&location);
                        U_PORT_TEST_ASSERT(location.latitudeX1e7 > INT_MIN);
                        U_PORT_TEST_ASSERT(location.longitudeX1e7 > INT_MIN);
                        // Don't check altitude as we might only have a 2D fix
                        U_PORT_TEST_ASSERT(location.radiusMillimetres > INT_MIN);
                        if (pLocationCfg->locationType == U_LOCATION_TYPE_GNSS) {
                            // Only get these for GNSS
                            U_PORT_TEST_ASSERT(location.speedMillimetresPerSecond > INT_MIN);
                            U_PORT_TEST_ASSERT(location.svs > 0);
                        }
                    } else {
                        uPortLog("U_NETWORK_TEST: only able to get time (%d).\n",
                                 (int32_t) location.timeUtc);
                    }
                    U_PORT_TEST_ASSERT(location.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
                } else {
                    uPortLog("U_NETWORK_TEST: not testing %s for location as we have"
                             " no location configuration information for it.\n",
                             gpUNetworkTestTypeName[pNetworkCfg->type]);
                }
            }
        }

        // Remove each network type, in reverse order so
        // that GNSS (which might be connected via a cellular
        // module) is taken down before cellular
        for (int32_t x = (int32_t) gUNetworkTestCfgSize - 1; x >= 0; x--) {
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
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost, heapUsed - gSystemHeapLost);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= (int32_t) gSystemHeapLost));
#else
    (void) gSystemHeapLost;
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

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    if (y != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_NETWORK_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_NETWORK_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
