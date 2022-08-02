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
#include "u_port_i2c.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
# include "u_cell_module_type.h"
# include "u_cell_test_cfg.h" // For the cellular test macros
# ifdef U_CFG_TEST_NET_STATUS_CELL
#  include "u_cell_net.h"
# endif
#endif

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)
# include "u_ble_sps.h"
# include "u_error_common.h"
#endif

#include "u_network.h"
#ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
# include "u_wifi.h"
# include "u_network_config_wifi.h"
#endif
#include "u_network_test_shared_cfg.h"

#include "u_sock.h"                  // In order to prove that we can do something
#include "u_sock_test_shared_cfg.h"  // with an "up" network that can support sockets

#include "u_location.h"                  // In order to prove that we can do something
#include "u_location_test_shared_cfg.h"  // with an "up" network that supports location

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_NETWORK_TEST"

/** The string to put at the start of all prints from this test
 * that do not require an iteration on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A type to hold all of the parameters passed to the network
 * status callback.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    bool isUp;
    uNetworkStatus_t status;
} uNetworkStatusCallbackParameters_t;

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
# ifdef U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL
static const char gRemoteSpsAddress[] =
    U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL);
# endif
#endif

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)
# define U_BLE_TEST_TEST_DATA_LOOPS 2
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
static const int32_t gTotalBytes = (sizeof gTestData - 1) * U_BLE_TEST_TEST_DATA_LOOPS;
static volatile int32_t gBytesSent;
static volatile int32_t gChannel;
static volatile uDeviceHandle_t gBleHandle;
static uPortSemaphoreHandle_t gBleConnectionSem = NULL;
#endif

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Keep track of the current network handle so that the
 * keepGoingCallback() can check it.
 */
//lint -esym(844, gDevHandle)
static uDeviceHandle_t gDevHandle = NULL;

#ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
/** A network configuration for a Wifi network we can control
 * via U_CFG_TEST_NET_STATUS_SHORT_RANGE
 * (U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL should be set to
 * the MAC address of a BLE device on the same short-range
 * module).
 */
static uNetworkCfgWifi_t gNetworkCfgWifiNetStatus = {
    .type = U_NETWORK_TYPE_WIFI,
    .pSsid = "disconnect_test_peer",
    .authentication = 1, // open
    .pPassPhrase = NULL
};
#endif

#if defined (U_CFG_TEST_NET_STATUS_SHORT_RANGE) || defined (U_CFG_TEST_NET_STATUS_CELL)
/** Array to hold the parameters passed to a network status
 * callback, big enough for one of each network type.
 */
static uNetworkStatusCallbackParameters_t gNetworkStatusCallbackParameters[U_NETWORK_TYPE_MAX_NUM];
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

/** Print text from a buffer and wrap when end is reached
 *
 * @param pBuffer         the buffer to print from.
 * @param bufLength       the length of the buffer to print from.
 * @param startIndex      the starting index in the buffer to print.
 * @param printLength     the number of characters to print from the buffer.
 */
static void wrapPrint(const char *pBuffer, size_t bufLength,
                      uint32_t startIndex, uint32_t printLength)
{
    for (size_t x = 0; x < printLength; x++) {
        char c = pBuffer[(startIndex + x) % bufLength];
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

static void sendBleSps(uDeviceHandle_t devHandle)
{
    uint32_t tries = 0;
    int32_t testDataOffset = 0;
    U_TEST_PRINT_LINE("sending data on channel %d...", gChannel);
    while ((tries++ < 15) && (gBytesSent < gTotalBytes)) {
        // -1 to omit gTestData string terminator
        int32_t bytesSentNow =
            uBleSpsSend(devHandle, gChannel, gTestData + testDataOffset, sizeof gTestData - 1 - testDataOffset);

        if (bytesSentNow >= 0) {
            gBytesSent += bytesSentNow;
            testDataOffset += bytesSentNow;
            if (testDataOffset >= sizeof gTestData - 1) {
                testDataOffset -= sizeof gTestData - 1;
            }
        } else {
            U_TEST_PRINT_LINE("error sending data!!!");
        }
        U_TEST_PRINT_LINE("%d byte(s) sent.", gBytesSent);

        // Make room for context switch letting receive event process
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }
    if (gBytesSent < gTotalBytes) {
        U_TEST_PRINT_LINE("%d byte(s) were not sent.", gTotalBytes - gBytesSent);
    }
}

//lint -e{818} Suppress 'pData' could be declared as const:
// need to follow function signature
static void bleSpsCallback(int32_t channel, void *pParameters)
{
    char buffer[100];
    int32_t length;
# if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    int32_t heapClibLoss = uPortGetHeapFree();
# endif
    (void)pParameters;

    U_PORT_TEST_ASSERT(channel == gChannel);

    do {
        length = uBleSpsReceive(gBleHandle, channel, buffer, sizeof(buffer));
        if (length > 0) {
            int32_t previousBytesReceived = gBytesReceived;
            int32_t errorStartByte = -1;

            // Compare the data with the expected data
            for (int32_t x = 0; (x < length); x++) {
                int32_t index = gBytesReceived % (sizeof(gTestData) - 1);
                if (gTestData[index] != buffer[x]) {
                    if (errorStartByte < 0) {
                        errorStartByte = x;
                    }
                    gErrors++;
                }
                gBytesReceived++;
            }

            U_TEST_PRINT_LINE("received %d bytes (total %d with %d errors).",
                              length, gBytesReceived, gErrors);
            if (errorStartByte >= 0) {
                U_TEST_PRINT_LINE("expected:");
                wrapPrint(gTestData, sizeof(gTestData) - 1, previousBytesReceived, errorStartByte + 1);
                U_TEST_PRINT_LINE("got:");
                wrapPrint(buffer, sizeof(buffer), 0, errorStartByte + 1);
            }
        }
    } while (length > 0);
# if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
# endif
}

static void connectionCallback(int32_t connHandle, char *address, int32_t status,
                               int32_t channel, int32_t mtu, void *pParameters)
{
# if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;
# endif

    (void)address;
    (void)mtu;
    (void)pParameters;

# if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
# endif

    if (status == (int32_t) U_BLE_SPS_CONNECTED) {
        gConnHandle = connHandle;
        gChannel = channel;
        U_TEST_PRINT_LINE("connected %s handle %d (channel %d).", address, connHandle, channel);
    } else if (status == (int32_t) U_BLE_SPS_DISCONNECTED) {
        gConnHandle = -1;
        if (connHandle != U_BLE_SPS_INVALID_HANDLE) {
            U_TEST_PRINT_LINE("disconnected connection handle %d.", connHandle);
        } else {
            U_TEST_PRINT_LINE("connection attempt failed.");
        }
    }
    if (gBleConnectionSem) {
        uPortSemaphoreGive(gBleConnectionSem);
    }

# if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
# endif
}
#endif // #if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

// Callback function for location establishment process.
static bool keepGoingCallback(uDeviceHandle_t devHandle)
{
    bool keepGoing = true;

    U_PORT_TEST_ASSERT(devHandle == gDevHandle);
    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

#if defined(U_CFG_TEST_NET_STATUS_SHORT_RANGE) || defined(U_CFG_TEST_NET_STATUS_CELL)
static void networkStatusCallback(uDeviceHandle_t devHandle,
                                  uNetworkType_t netType,
                                  bool isUp,
                                  uNetworkStatus_t *pStatus,
                                  void *pParameter)
{
    U_PORT_TEST_ASSERT(pParameter == (void *) gNetworkStatusCallbackParameters);

    U_PORT_TEST_ASSERT(netType < sizeof(gNetworkStatusCallbackParameters) /
                       sizeof(gNetworkStatusCallbackParameters[0]));

#if !U_CFG_OS_CLIB_LEAKS
    // Only print stuff if the C library isn't going to leak
    U_TEST_PRINT_LINE("network status callback called for %s.",
                      gpUNetworkTestTypeName[netType]);
#endif

    gNetworkStatusCallbackParameters[netType].devHandle = devHandle;
    gNetworkStatusCallbackParameters[netType].isUp = isUp;

    U_PORT_TEST_ASSERT(pStatus != NULL);
    switch (netType) {
        case U_NETWORK_TYPE_BLE:
            gNetworkStatusCallbackParameters[netType].status.ble = pStatus->ble;
            gConnHandle = -1;
            if (pStatus->ble.status == (int32_t) U_BLE_SPS_CONNECTED) {
                gConnHandle = pStatus->ble.connHandle;
            }
            break;
        case U_NETWORK_TYPE_CELL:
            gNetworkStatusCallbackParameters[netType].status.cell = pStatus->cell;
            break;
        case U_NETWORK_TYPE_WIFI:
            gNetworkStatusCallbackParameters[netType].status.wifi = pStatus->wifi;
            break;
        case U_NETWORK_TYPE_GNSS:
        default:
            U_PORT_TEST_ASSERT(false);
            break;
    }
}
#endif

// Open a socket and use it.
static int32_t openSocketAndUseIt(uDeviceHandle_t devHandle,
                                  uNetworkType_t netType,
                                  int32_t *pHeapXxxSockInitLoss)
{
    int32_t errorCodeOrSize;
    uSockDescriptor_t descriptor;
    uSockAddress_t address;
    char buffer[32];

    U_TEST_PRINT_LINE("looking up echo server \"%s\"...",
                      U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
    // Look up the address of the server we use for UDP echo
    // The first call to a sockets API needs to
    // initialise the underlying sockets layer; take
    // account of that initialisation heap cost here.
    *pHeapXxxSockInitLoss += uPortGetHeapFree();
    // Look up the address of the server we use for UDP echo
    errorCodeOrSize = uSockGetHostByName(devHandle,
                                         U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                         &(address.ipAddress));
    *pHeapXxxSockInitLoss -= uPortGetHeapFree();

    if (errorCodeOrSize == 0) {
        // Add the port number we will use
        address.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

        // Create a UDP socket
        descriptor = uSockCreate(devHandle, U_SOCK_TYPE_DGRAM,
                                 U_SOCK_PROTOCOL_UDP);
        if (descriptor >= 0) {
            // Send and wait for the UDP echo data, trying a few
            // times to reduce the chance of internet loss getting
            // in the way
            U_TEST_PRINT_LINE("sending %d byte(s) to %s:%d over"
                              " %s...", sizeof(gTestString) - 1,
                              U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                              U_SOCK_TEST_ECHO_UDP_SERVER_PORT,
                              gpUNetworkTestTypeName[netType]);
            errorCodeOrSize = 0;
            memset(buffer, 0, sizeof(buffer));
            for (size_t x = 0; (x < 3) &&
                 (errorCodeOrSize != sizeof(gTestString) - 1); x++) {
                errorCodeOrSize = uSockSendTo(descriptor, &address, gTestString,
                                              sizeof(gTestString) - 1);
                if (errorCodeOrSize == sizeof(gTestString) - 1) {
                    // Wait for the answer
                    errorCodeOrSize = uSockReceiveFrom(descriptor, NULL,
                                                       buffer, sizeof(buffer));
                    if (errorCodeOrSize != sizeof(gTestString) - 1) {
                        U_TEST_PRINT_LINE("failed to receive UDP echo on try %d.", x + 1);
                        uPortTaskBlock(1000);
                    }
                } else {
                    U_TEST_PRINT_LINE("failed to send UDP data on try %d.", x + 1);
                }
            }
            U_TEST_PRINT_LINE("%d byte(s) echoed over UDP on %s.", errorCodeOrSize,
                              gpUNetworkTestTypeName[netType]);
            if ((errorCodeOrSize == sizeof(gTestString) - 1) &&
                (strncmp(buffer, gTestString, sizeof(buffer)) == 0)) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
            }

            // Close the socket
            uSockClose(descriptor);
        }
    }

    // Clean up to ensure no memory leaks
    uSockCleanUp();

    return errorCodeOrSize;
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
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;

    // Make sure we start fresh for this test case
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of things that support sockets
    pList = pUNetworkTestListAlloc(uNetworkTestHasSock);
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
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
        // Bring up each network configuration
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            devHandle = *pTmp->pDevHandle;

            U_TEST_PRINT_LINE("bringing up %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);
            // Do the thing
            U_PORT_TEST_ASSERT(openSocketAndUseIt(devHandle, pTmp->networkType,
                                                  &heapSockInitLoss) == 0);
        }

        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
        }
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test, %d byte(s) were lost to"
                      " sockets initialisation and we have leaked"
                      " %d byte(s).", gSystemHeapLost, heapSockInitLoss,
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
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t timeoutCount;
    uBleSpsHandles_t spsHandles;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of things that support BLE
    pList = pUNetworkTestListAlloc(uNetworkTestIsBle);
    if (pList == NULL) {
        U_TEST_PRINT_LINE("*** WARNING *** nothing to do.");
    }
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Do this twice to prove that we can go from down
    // back to up again
    for (size_t a = 0; a < 2; a++) {
        // Bring up the BLE network
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            devHandle = *pTmp->pDevHandle;

            U_TEST_PRINT_LINE("bringing up %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);

            memset(&spsHandles, 0x00, sizeof spsHandles);

            gConnHandle = -1;
            gBytesSent = 0;
            gBytesReceived = 0;
            U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gBleConnectionSem, 0, 1) == 0);

            uBleSpsSetCallbackConnectionStatus(devHandle,
                                               connectionCallback,
                                               &devHandle);
            uBleSpsSetDataAvailableCallback(devHandle, bleSpsCallback,
                                            &devHandle);
            gBleHandle = devHandle;

            for (int32_t i = 0; i < 3; i++) {
                if (i > 0) {
                    if (uBleSpsPresetSpsServerHandles(devHandle, &spsHandles) ==
                        U_ERROR_COMMON_NOT_IMPLEMENTED) {
                        continue;
                    }
                }
                if (i > 1) {
                    if (uBleSpsDisableFlowCtrlOnNext(devHandle) ==
                        U_ERROR_COMMON_NOT_IMPLEMENTED) {
                        continue;
                    }
                }
                for (size_t tries = 0; tries < 3; tries++) {
                    int32_t result;
                    // Use first testrun(up/down) to test default connection parameters
                    // and the second for using non-default.
                    if (a == 0) {
                        U_TEST_PRINT_LINE("connecting SPS: %s.", gRemoteSpsAddress);
                        result = uBleSpsConnectSps(devHandle,
                                                   gRemoteSpsAddress,
                                                   NULL);
                    } else {
                        uBleSpsConnParams_t connParams;
                        connParams.scanInterval = 64;
                        connParams.scanWindow = 64;
                        connParams.createConnectionTmo = 5000;
                        connParams.connIntervalMin = 28;
                        connParams.connIntervalMax = 34;
                        connParams.connLatency = 0;
                        connParams.linkLossTimeout = 2000;
                        U_TEST_PRINT_LINE("connecting SPS with conn params: %s.", gRemoteSpsAddress);
                        result = uBleSpsConnectSps(devHandle,
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
                    U_TEST_PRINT_LINE("all SPS connection attempts failed!");
                    U_PORT_TEST_ASSERT(false);
                }
                if (i == 0) {
                    uBleSpsGetSpsServerHandles(devHandle, gChannel, &spsHandles);
                }

                uBleSpsSetSendTimeout(devHandle, gChannel, 100);
                uPortTaskBlock(100);
                timeoutCount = 0;
                sendBleSps(devHandle);
                while (gBytesReceived < gBytesSent) {
                    uPortTaskBlock(100);
                    if (timeoutCount++ > 100) {
                        break;
                    }
                }
                U_PORT_TEST_ASSERT(gBytesSent == gTotalBytes);
                U_PORT_TEST_ASSERT(gBytesSent == gBytesReceived);
                U_PORT_TEST_ASSERT(gErrors == 0);
                // Disconnect
                U_PORT_TEST_ASSERT(uBleSpsDisconnect(devHandle, gConnHandle) == 0);
                for (int32_t i = 0; (i < 40) && (gConnHandle != -1); i++) {
                    uPortTaskBlock(100);
                }
                gBytesSent = 0;
                gBytesReceived = 0;
                U_PORT_TEST_ASSERT(gConnHandle == -1);
            }

            uBleSpsSetDataAvailableCallback(devHandle, NULL, NULL);
            uBleSpsSetCallbackConnectionStatus(devHandle, NULL, NULL);

            U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gBleConnectionSem) == 0);
            gBleConnectionSem = NULL;
        }

        // Remove the BLE network
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
        }
    }

    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test, %d byte(s) were lost to"
                      " sockets initialisation and we have leaked %d"
                      " byte(s).", gSystemHeapLost, heapSockInitLoss,
                      heapUsed - (gSystemHeapLost + heapSockInitLoss ));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= (int32_t) (gSystemHeapLost +
                                               heapSockInitLoss)));
# else
    (void) gSystemHeapLost;
    (void) heapSockInitLoss;
    (void) heapUsed;
# endif
}
#endif // #if defined(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL) || defined(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL)

/** Test networks that support location.
 */
U_PORT_TEST_FUNCTION("[network]", "networkLoc")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    const uLocationTestCfg_t *pLocationCfg;
    int32_t y;
    uLocation_t location;
    int64_t startTime;
    int32_t heapUsed;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    // Don't check this for success as not all platforms support I2C
    uPortI2cInit();
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of all things
    pList = pUNetworkTestListAlloc(NULL);
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Do this twice to prove that we can go from down
    // back to up again
    for (size_t a = 0; a < 2; a++) {
        // Bring up each network type
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            devHandle = *pTmp->pDevHandle;

            U_TEST_PRINT_LINE("bringing up %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);
            if (gpULocationTestCfg[pTmp->networkType]->numEntries > 0) {
                // Just take the first one, we don't care which as this
                // is a network test not a location test
                pLocationCfg = gpULocationTestCfg[pTmp->networkType]->pCfgData[0];
                startTime = uPortGetTickTimeMs();
                gStopTimeMs = startTime + U_LOCATION_TEST_CFG_TIMEOUT_SECONDS * 1000;
                uLocationTestResetLocation(&location);
                U_TEST_PRINT_LINE("getting location using %s.",
                                  gpULocationTestTypeStr[pLocationCfg->locationType]);
                gDevHandle = devHandle;
                y = uLocationGet(devHandle, pLocationCfg->locationType,
                                 pLocationCfg->pLocationAssist,
                                 pLocationCfg->pAuthenticationTokenStr,
                                 &location, keepGoingCallback);
                if (y == 0) {
                    U_TEST_PRINT_LINE("location establishment took %d second(s).",
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
                    U_TEST_PRINT_LINE("only able to get time (%d).", (int32_t) location.timeUtc);
                }
                U_PORT_TEST_ASSERT(location.timeUtc > U_LOCATION_TEST_MIN_UTC_TIME);
            } else {
                U_TEST_PRINT_LINE("not testing %s for location as we have"
                                  " no location configuration information for it.",
                                  gpUNetworkTestTypeName[pTmp->networkType]);
            }
        }

        // Remove each network type
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
        }
    }

    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortI2cDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
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

/** Test BLE and Wifi one after the other on a single device.
 */
U_PORT_TEST_FUNCTION("[network]", "networkShortRange")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t heapUsed;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of configurations that are short-range devices
    pList = pUNetworkTestListAlloc(uNetworkTestIsDeviceShortRange);
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    for (size_t a = 0; a < 2; a++) {
        // Bring up and down each short-range network type
        // in turn
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            devHandle = *pTmp->pDevHandle;

            U_TEST_PRINT_LINE("bringing up %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);

            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(devHandle,
                                                     pTmp->networkType) == 0);
        }
    }

    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
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

#if defined(U_CFG_TEST_NET_STATUS_SHORT_RANGE) || defined(U_CFG_TEST_NET_STATUS_CELL)
/** Test network outages.
 */
U_PORT_TEST_FUNCTION("[network]", "networkOutage")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t y;
    uNetworkStatusCallbackParameters_t *pCallbackParameters;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

# ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
    // Prepare for BLE connection stuff
    if (gBleConnectionSem != NULL) {
        U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gBleConnectionSem) == 0);
    }
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gBleConnectionSem, 0, 1) == 0);
# endif

    // Get all of the configurations that support the network
    // status callback
# if defined(U_CFG_TEST_NET_STATUS_SHORT_RANGE) && defined(U_CFG_TEST_NET_STATUS_CELL)
    pList = pUNetworkTestListAlloc(uNetworkTestHasStatusCallback);
# elif defined(U_CFG_TEST_NET_STATUS_SHORT_RANGE)
    pList = pUNetworkTestListAlloc(uNetworkTestIsDeviceShortRange);
# elif defined(U_CFG_TEST_NET_STATUS_CELL)
    pList = pUNetworkTestListAlloc(uNetworkTestIsDeviceCell);
# endif
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
        if (pTmp->networkType == U_NETWORK_TYPE_WIFI) {
            // Replace the wifi network in the list with one we have control over
            pTmp->pNetworkCfg = (const void *) &gNetworkCfgWifiNetStatus;
        }
    }

    // It is possible for socket closure in an
    // underlying layer to have failed in a previous
    // test, leaving sockets hanging, so just in case,
    // clear them up here
    uSockDeinit();

    // Tell the test script that is monitoring progress
    // to switch all the switches on to begin with
# ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
    uPortLog("AUTOMATION_SET_SWITCH SHORT_RANGE 1\n");
# endif
# ifdef U_CFG_TEST_NET_STATUS_CELL
    uPortLog("AUTOMATION_SET_SWITCH CELL 1\n");
# endif
    uPortTaskBlock(1000);

    // Do this twice, 'cos.
    for (size_t a = 0; a < 2; a++) {
        // Bring up each network type
        uPortLog(U_TEST_PREFIX_X "\n", a);
        U_TEST_PRINT_LINE_X("########## SECONDS AWAY... ROUND %d ##########", a, a + 1);
        uPortLog(U_TEST_PREFIX_X "\n", a);
        // Fill gNetworkStatusCallbackParameters with rubbish
        memset(gNetworkStatusCallbackParameters, 0xFF, sizeof(gNetworkStatusCallbackParameters));
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            devHandle = *pTmp->pDevHandle;

            U_TEST_PRINT_LINE_X("bringing up %s...", a,
                                gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);
            U_TEST_PRINT_LINE_X("adding network status callback for %s...", a,
                                gpUNetworkTestTypeName[pTmp->networkType]);
            // networkStatusCallback is given the address of
            // gNetworkStatusCallbackParameters as a parameter so that
            // it can fill it in with the stuff it receives.
            U_PORT_TEST_ASSERT(uNetworkSetStatusCallback(devHandle,
                                                         pTmp->networkType,
                                                         networkStatusCallback,
                                                         gNetworkStatusCallbackParameters) == 0);
            switch (pTmp->networkType) {
                case U_NETWORK_TYPE_BLE:
                    // For BLE, make a connection with our test peer
                    U_TEST_PRINT_LINE_X("connecting SPS: %s.", a, gRemoteSpsAddress);
                    gConnHandle = -1;
                    for (size_t tries = 0; (tries < 3) && (gConnHandle < 0); tries++) {
                        if (uBleSpsConnectSps(devHandle, gRemoteSpsAddress, NULL) == 0) {
                            // Wait for connection
                            uPortSemaphoreTryTake(gBleConnectionSem, 10000);
                        } else {
                            // Wait a bit and try again...
                            uPortTaskBlock(5000);
                        }
                    }
                    U_PORT_TEST_ASSERT(gConnHandle >= 0);
                    break;
                case U_NETWORK_TYPE_CELL:
                    // For cellular, we have network access, so we
                    // should be able to perform a sockets operation
                    U_PORT_TEST_ASSERT(openSocketAndUseIt(devHandle, pTmp->networkType,
                                                          &heapSockInitLoss) == 0);
                    break;
                case U_NETWORK_TYPE_WIFI:
                    // Nothing to do for Wi-Fi, connecting to the AP
                    // is enough; it is a local one that we can control
                    // and so does not have internet access
                    break;
                default:
                    break;
            }
        }

        // Tell the test script that is monitoring progress
        // to set the switches to 0/off
# ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
        uPortLog("AUTOMATION_SET_SWITCH SHORT_RANGE 0\n");
# endif
# ifdef U_CFG_TEST_NET_STATUS_CELL
        uPortLog("AUTOMATION_SET_SWITCH CELL 0\n");
# endif

        U_TEST_PRINT_LINE_X("waiting for all network types to drop...", a);
        // Note: BLE/Wi-Fi will drop within a few seconds but cellular is much
        // more difficult to shake since it works down to near -140dBm these days;
        // a screened box with high quality RF cables is barely enough
        uPortTaskBlock(30000);

        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            pCallbackParameters = &(gNetworkStatusCallbackParameters[pTmp->networkType]);
            U_TEST_PRINT_LINE_X("checking that the callback has been called for"
                                " the \"network down\" case for network type %s...",
                                a, gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(pCallbackParameters->devHandle == *pTmp->pDevHandle);
            U_PORT_TEST_ASSERT(!pCallbackParameters->isUp);
            switch (pTmp->networkType) {
                case U_NETWORK_TYPE_BLE:
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.pAddress == NULL);
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.status == U_BLE_SPS_DISCONNECTED);
                    break;
                case U_NETWORK_TYPE_CELL:
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.cell.domain == (int32_t) U_CELL_NET_REG_DOMAIN_PS);
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.cell.status == (int32_t)
                                       U_CELL_NET_STATUS_OUT_OF_COVERAGE);
                    break;
                case U_NETWORK_TYPE_WIFI:
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.pBssid == NULL);
                    U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.disconnectReason == U_WIFI_REASON_OUT_OF_RANGE);
                    break;
                case U_NETWORK_TYPE_GNSS:
                default:
                    break;
            }
        }
        // Fill gNetworkStatusCallbackParameters with rubbish again
        memset(gNetworkStatusCallbackParameters, 0xFF, sizeof(gNetworkStatusCallbackParameters));

        // Do this twice: once to prove that a connection can fail,
        // since the peer is not there, and a second time to
        // reconnect, recovering from the outage
        for (size_t x = 0; x < 2; x++) {
            for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
                devHandle = *pTmp->pDevHandle;
                switch (pTmp->networkType) {
                    case U_NETWORK_TYPE_BLE:
                        // For BLE, the network will stay dropped so
                        // we need to re-establish it and reconnect
                        U_TEST_PRINT_LINE_X("re-bringing up %s...", a,
                                            gpUNetworkTestTypeName[pTmp->networkType]);
                        y = uNetworkInterfaceUp(devHandle, pTmp->networkType,
                                                pTmp->pNetworkCfg);
                        U_TEST_PRINT_LINE_X("uNetworkInterfaceUp() returned %d.", a, y);
                        U_PORT_TEST_ASSERT(y == 0);
                        U_TEST_PRINT_LINE_X("re-connecting SPS: %s.", a, gRemoteSpsAddress);
                        gConnHandle = -1;
                        for (size_t tries = 0; (tries < 3) && (gConnHandle < 0); tries++) {
                            y = uBleSpsConnectSps(devHandle, gRemoteSpsAddress, NULL);
                            U_TEST_PRINT_LINE_X("uBleSpsConnectSps() returned %d.", a, y);
                            if (y == 0) {
                                // Wait for connection
                                uPortSemaphoreTryTake(gBleConnectionSem, 10000);
                            } else {
                                // Wait a bit and try again...
                                uPortTaskBlock(5000);
                            }
                        }
                        U_TEST_PRINT_LINE_X("at the end of that gConnHandle was %d.", a,
                                            gConnHandle);
                        if (x == 0) {
                            U_PORT_TEST_ASSERT(gConnHandle < 0);
                        } else {
                            U_PORT_TEST_ASSERT(gConnHandle >= 0);
                        }
                        break;
                    case U_NETWORK_TYPE_CELL:
                        // For cellular, the network should have re-established
                        // itself, and hence we should be able to perform a
                        // sockets operation straight away, no need to do an "up"
                        y = openSocketAndUseIt(devHandle, pTmp->networkType,
                                               &heapSockInitLoss);
                        if (x == 0) {
                            U_PORT_TEST_ASSERT(y < 0);
                        } else {
                            U_PORT_TEST_ASSERT(y == 0);
                        }
                        break;
                    case U_NETWORK_TYPE_WIFI:
                        // For Wi-Fi, the network will stay dropped;
                        // we need to re-establish it
                        U_TEST_PRINT_LINE_X("re-bringing up %s...", a,
                                            gpUNetworkTestTypeName[pTmp->networkType]);
                        y = uNetworkInterfaceUp(devHandle, pTmp->networkType,
                                                pTmp->pNetworkCfg);
                        U_TEST_PRINT_LINE_X("uNetworkInterfaceUp() returned %d.", a, y);
                        if (x == 0) {
                            U_PORT_TEST_ASSERT(y < 0);
                        } else {
                            U_PORT_TEST_ASSERT(y == 0);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (x == 0) {
                // Tell the test script that is monitoring progress
                // to set the switches to 1/on
# ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
                uPortLog("AUTOMATION_SET_SWITCH SHORT_RANGE 1\n");
# endif
# ifdef U_CFG_TEST_NET_STATUS_CELL
                uPortLog("AUTOMATION_SET_SWITCH CELL 1\n");
# endif
                U_TEST_PRINT_LINE_X("waiting for all network types to come back up...", a);
                uPortTaskBlock(15000);
            } else {
                for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
                    pCallbackParameters = &(gNetworkStatusCallbackParameters[pTmp->networkType]);
                    U_TEST_PRINT_LINE_X("checking that the callback has been called for the"
                                        " \"network up\" case for network type %s...", a,
                                        gpUNetworkTestTypeName[pTmp->networkType]);
                    U_PORT_TEST_ASSERT(pCallbackParameters->devHandle == *pTmp->pDevHandle);
                    U_PORT_TEST_ASSERT(pCallbackParameters->isUp);
                    switch (pTmp->networkType) {
                        case U_NETWORK_TYPE_BLE:
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.pAddress != NULL);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.status == (int32_t) U_BLE_SPS_CONNECTED);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.channel >= 0);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.ble.mtu > 0);
                            break;
                        case U_NETWORK_TYPE_CELL:
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.cell.domain == (int32_t) U_CELL_NET_REG_DOMAIN_PS);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.cell.status == (int32_t)
                                               U_CELL_NET_STATUS_REGISTERED_HOME);
                            break;
                        case U_NETWORK_TYPE_WIFI:
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.connId >= 0);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.status == (int32_t)
                                               U_WIFI_CON_STATUS_CONNECTED);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.channel >= 0);
                            U_PORT_TEST_ASSERT(pCallbackParameters->status.wifi.pBssid != NULL);
                            break;
                        case U_NETWORK_TYPE_GNSS:
                        default:
                            break;
                    }
                }
            }
        }

        // Remove each network type
        for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
            U_TEST_PRINT_LINE_X("taking down %s...", a,
                                gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
        }
    }

    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    // Clean up
# ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gBleConnectionSem) == 0);
    gBleConnectionSem = NULL;
# endif
    uDeviceDeinit();
    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'defed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test, %d byte(s) were lost to"
                      " sockets initialisation and we have leaked %d"
                      " byte(s).", gSystemHeapLost, heapSockInitLoss,
                      heapUsed - (gSystemHeapLost + heapSockInitLoss ));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= (int32_t) (gSystemHeapLost +
                                               heapSockInitLoss)));
# else
    (void) gSystemHeapLost;
    (void) heapSockInitLoss;
    (void) heapUsed;
# endif
}

#endif // #if defined(U_CFG_TEST_NET_STATUS_SHORT_RANGE) || defined (U_CFG_TEST_NET_STATUS_CELL)

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[network]", "networkCleanUp")
{
    int32_t y;

    // Make sure that the switches haven't been left in the
    // "off" position
#ifdef U_CFG_TEST_NET_STATUS_SHORT_RANGE
    uPortLog("AUTOMATION_SET_SWITCH SHORT_RANGE 1\n");
#endif
#ifdef U_CFG_TEST_NET_STATUS_CELL
    uPortLog("AUTOMATION_SET_SWITCH CELL 1\n");
    uPortTaskBlock(1000);
#endif

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    uDeviceDeinit();

    y = uPortTaskStackMinFree(NULL);
    if (y != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortI2cDeinit();
    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
