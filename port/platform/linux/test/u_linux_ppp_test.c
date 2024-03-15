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
 * @brief Tests of Linux sockets using pppd connected to a cellular
 * module via ubxlib.  These tests should pass on Linux when there is a
 * cellular module connected.  These tests use the network API and the
 * test configuration information from the network API and sockets API
 & to provide the communication path.
 *
 * The tests are only compiled if U_CFG_PPP_ENABLE is defined.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#ifdef U_CFG_PPP_ENABLE

#include "stddef.h"    // NULL, size_t etc.
#include "stdlib.h"    // rand(), snprintf()
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen()
#include "unistd.h"
#include "sys/socket.h"
#include "netdb.h"     // struct addrinfo
#include "arpa/inet.h"
#include "errno.h"
#include "fcntl.h"
#include "net/if.h" // struct ifreq

#include "u_compiler.h" // U_WEAK

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* struct timeval in some cases. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"

#include "u_test_util_resource_check.h"

#include "u_network.h"                  // In order to provide a comms
#include "u_network_test_shared_cfg.h"  // path for the socket

#include "u_sock_test_shared_cfg.h"

// These for uCellPrivateModule_t
#include "u_at_client.h"
#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_LINUX_SOCK_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_LINUX_PPP_TEST_RECEIVE_TASK_STACK_SIZE_BYTES
/** The stack size to use for the asynchronous receive task.
 */
# define U_LINUX_PPP_TEST_RECEIVE_TASK_STACK_SIZE_BYTES 2560
#endif

#ifndef U_LINUX_PPP_TEST_RECEIVE_TASK_PRIORITY
/** The priority to use for the asynchronous receive task.
 */
# define U_LINUX_PPP_TEST_RECEIVE_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 5)
#endif

#ifndef U_LINUX_PPP_TEST_RECEIVE_TASK_RELAX_MS
/** How long the receive task should relax for between receive
 * attempts.
 */
# define U_LINUX_PPP_TEST_RECEIVE_TASK_RELAX_MS 10
#endif

#ifndef U_LINUX_PPP_TEST_RECEIVE_TASK_EXIT_MS
/** How long to allow for the the receive task to exit;
 * should be quite a lot longer than
 * #U_LINUX_PPP_TEST_RECEIVE_TASK_EXIT_MS.
 */
# define U_LINUX_PPP_TEST_RECEIVE_TASK_EXIT_MS 100
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to pass to rxTask().
 */
typedef struct {
    int32_t sock;
    char *pBuffer;
    size_t bufferLength;
    size_t bytesToSend;
    size_t bytesReceived;
    size_t packetsReceived;
    uPortTaskHandle_t taskHandle;
    bool asyncExit;
} uLinuxPppSockTestConfig_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Some data to exchange with an echo server.
 */
static const char gSendData[] =  "_____0000:0123456789012345678901234567890123456789"
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
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1000:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1100:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1200:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1300:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1400:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1500:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1600:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1700:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1800:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____1900:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789"
                                 "_____2000:0123456789012345678901234567890123456789"
                                 "01234567890123456789012345678901234567890123456789";

/** Data structure passed around during aynchronous receive.
 */
//lint -esym(785, gTestConfig) Suppress too few initialisers
static uLinuxPppSockTestConfig_t gTestConfig = {.taskHandle = NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Do this before every test to ensure there is a usable network.
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Add the device for each network configuration
    // if not already added
    pList = pUNetworkTestListAlloc(uNetworkTestHasPpp);
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

    // Bring up each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(*pTmp->pDevHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);
    }

    return pList;
}

// Receive data echoed back to us over a socket.
static void rxTask(void *pParameter)
{
    int32_t sizeBytes;
    uLinuxPppSockTestConfig_t *pTestConfig = (uLinuxPppSockTestConfig_t *) pParameter;

    U_TEST_PRINT_LINE("rxTask receiving on socket %d.", pTestConfig->sock);
    // Read from the socket until there's nothing left to read
    //lint -e{776} Suppress possible truncation of addition
    do {
        sizeBytes = recv(pTestConfig->sock,
                         pTestConfig->pBuffer +
                         pTestConfig->bytesReceived,
                         pTestConfig->bytesToSend -
                         pTestConfig->bytesReceived, 0);
        if (sizeBytes > 0) {
            U_TEST_PRINT_LINE("received %d byte(s) of data @%d ms.",
                              sizeBytes, (int32_t) uPortGetTickTimeMs());
            pTestConfig->bytesReceived += sizeBytes;
            pTestConfig->packetsReceived++;
        } else {
            uPortTaskBlock(U_LINUX_PPP_TEST_RECEIVE_TASK_RELAX_MS);
        }
    } while ((pTestConfig->bytesReceived < pTestConfig->bytesToSend) &&
             !pTestConfig->asyncExit);

    U_TEST_PRINT_LINE("rxTask exiting.");

    uPortTaskDelete(NULL);
}

// Make sure that size is greater than 0 and no more than limit.
static size_t fix(size_t size, size_t limit)
{
    if (size == 0) {
        size = limit / 2; // better than 1
    } else if (size > limit) {
        size = limit;
    }

    return size;
}

// Send an entire TCP data buffer until done.
static size_t sendTcp(int32_t sock, const char *pData, size_t sizeBytes)
{
    int32_t x;
    size_t sentSizeBytes = 0;
    int32_t startTimeMs;

    U_TEST_PRINT_LINE("sending %d byte(s) of TCP data...", sizeBytes);
    startTimeMs = uPortGetTickTimeMs();
    while ((sentSizeBytes < sizeBytes) &&
           ((uPortGetTickTimeMs() - startTimeMs) < 10000)) {
        x = send(sock, (const void *) pData, sizeBytes - sentSizeBytes, 0);
        if (x > 0) {
            sentSizeBytes += x;
            pData += x;
            U_TEST_PRINT_LINE("sent %d byte(s) of TCP data @%d ms.",
                              sentSizeBytes, (int32_t) uPortGetTickTimeMs());
        }
    }

    return sentSizeBytes;
}

// Check a buffer of what was sent against what was echoed back and
// print out useful info if they differ.
static bool checkAgainstSentData(const char *pDataSent,
                                 size_t dataSentSizeBytes,
                                 const char *pDataReceived,
                                 size_t dataReceivedSizeBytes)
{
    bool success = true;
    int32_t x;
#if U_CFG_ENABLE_LOGGING
    int32_t y;
    int32_t z;
#endif

    if (dataReceivedSizeBytes == dataSentSizeBytes) {
        // Run through checking that the characters are the same
        for (x = 0; ((*(pDataReceived + x) == *(pDataSent + x))) &&
             (x < (int32_t) dataSentSizeBytes); x++) {
        }
        if (x != (int32_t) dataSentSizeBytes) {
#if U_CFG_ENABLE_LOGGING
            y = x - 5;
            if (y < 0) {
                y = 0;
            }
            z = 10;
            if (y + z > (int32_t) dataSentSizeBytes) {
                z = ((int32_t) dataSentSizeBytes) - y;
            }
            U_TEST_PRINT_LINE("difference at character %d (sent \"%*.*s\", received"
                              " \"%*.*s\").", x + 1, z, z, pDataSent + y, z, z,
                              pDataReceived + y);
#endif
            success = false;
        }
    } else {
        U_TEST_PRINT_LINE("%d byte(s) missing (%d byte(s) received when %d were"
                          " expected)).", dataSentSizeBytes - dataReceivedSizeBytes,
                          dataReceivedSizeBytes, dataSentSizeBytes);
        success = false;
    }

    return success;
}

// Release OS resources that may have been left hanging
// by a failed test
static void osCleanup()
{
    if (gTestConfig.taskHandle != NULL) {
        gTestConfig.asyncExit = true;
        uPortTaskBlock(U_LINUX_PPP_TEST_RECEIVE_TASK_EXIT_MS);
        gTestConfig.taskHandle = NULL;
    }
    uPortFree(gTestConfig.pBuffer);
    gTestConfig.pBuffer = NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WEAK IMPLEMENTATION OF CELL API FUNCTION
 * -------------------------------------------------------------- */

// This to allow the code here to at least compile if cellular is not included.
U_WEAK const uCellPrivateModule_t *pUCellPrivateGetModule(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic TCP test.
 *
 * Note: we used to name the tests here linuxBlah to match the
 * pattern of the platform tests under ESP-IDF and Zephyr.  However,
 * setting U_CFG_APP_FILTER to "linux" to run just these Linux tests
 * doesn't work: "linux" is implicitly defined by the toolchain to
 * be 1, so any time it appears as a conditional compilation flag
 * the compiler will replace it with 1.  These test names begin with
 * testLinux instead of just linux; setting U_CFG_APP_FILTER to
 * "testLinux" _will_ work.
 */
U_PORT_TEST_FUNCTION("[testLinuxSock]", "testLinuxSockTcp")
{
    const uCellPrivateModule_t *pModule;
    uNetworkTestList_t *pList;
    int32_t resourceCount;
    char hostIp[] = U_SOCK_TEST_ECHO_TCP_SERVER_IP_ADDRESS;
    struct sockaddr_in destinationAddress;
    int32_t sock;
    int32_t errorCode;
    int32_t x;
    size_t sizeBytes = 0;
    size_t offset;
    int32_t startTimeMs;
    struct ifreq interface = {0};

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial resource count
    uPortDeinit();

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble to make sure there is
    // a network underneath us
    pList = pStdPreamble();

    // Repeat for all bearers that have a supported PPP interface
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        pModule = NULL;
        if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) {
            pModule = pUCellPrivateGetModule(*pTmp->pDevHandle);
        }
        if ((pModule == NULL) ||
            U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_PPP)) {

            U_TEST_PRINT_LINE("doing async TCP test on %s.",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            osCleanup();

            inet_pton(AF_INET, hostIp, &destinationAddress.sin_addr);
            destinationAddress.sin_family = AF_INET;
            destinationAddress.sin_port = htons(U_SOCK_TEST_ECHO_TCP_SERVER_PORT);

            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            U_TEST_PRINT_LINE("opening socket() to %s:%d returned %d (errno %d).",
                              hostIp, U_SOCK_TEST_ECHO_TCP_SERVER_PORT, sock, errno);
            U_PORT_TEST_ASSERT(sock >= 0);

            // Set socket to be non-blocking for our asynchronous receive
            x = fcntl(sock, F_GETFL, 0);
            U_PORT_TEST_ASSERT(x >= 0);
            x &= ~O_NONBLOCK;
            U_PORT_TEST_ASSERT(fcntl(sock, F_SETFL, x) == 0);

            // Bind the socket to ppp0, the PPP interface, otherwise it will
            // likely send over the Ethernet port
            snprintf(interface.ifr_name, sizeof(interface.ifr_name), "ppp0");
            U_PORT_TEST_ASSERT(setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
                                          &interface, sizeof(interface)) == 0);

            errorCode = connect(sock, (struct sockaddr *) &destinationAddress, sizeof(destinationAddress));
            U_TEST_PRINT_LINE("connect() returned %d (errno %d).", errorCode, errno);
            U_PORT_TEST_ASSERT(errorCode == 0);

            memset(&gTestConfig, 0, sizeof(gTestConfig));
            gTestConfig.sock = sock;
            // We're sending all of gSendData except the
            // null terminator on the end
            gTestConfig.bytesToSend = sizeof(gSendData) - 1;

            // Malloc a buffer to receive TCP packets into
            // and put the fill value into it
            gTestConfig.bufferLength = gTestConfig.bytesToSend;
            gTestConfig.pBuffer = (char *) pUPortMalloc(gTestConfig.bufferLength);
            U_PORT_TEST_ASSERT(gTestConfig.pBuffer != NULL);

            // Create a task to receive data
            U_PORT_TEST_ASSERT(uPortTaskCreate(rxTask, "rxTask",
                                               U_LINUX_PPP_TEST_RECEIVE_TASK_STACK_SIZE_BYTES,
                                               (void *) &gTestConfig,
                                               U_LINUX_PPP_TEST_RECEIVE_TASK_PRIORITY,
                                               &gTestConfig.taskHandle) == 0);

            // Throw random sized segments up...
            offset = 0;
            x = 0;
            startTimeMs = uPortGetTickTimeMs();
            while (offset < gTestConfig.bytesToSend) {
                sizeBytes = (rand() % U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE) + 1;
                sizeBytes = fix(sizeBytes, U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE);
                if (sizeBytes < U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE) {
                    sizeBytes = U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE;
                }
                if (offset + sizeBytes > gTestConfig.bytesToSend) {
                    sizeBytes = gTestConfig.bytesToSend - offset;
                }
                U_TEST_PRINT_LINE("write number %d.", x + 1);
                U_PORT_TEST_ASSERT(sendTcp(gTestConfig.sock,
                                           gSendData + offset,
                                           sizeBytes) == sizeBytes);
                offset += sizeBytes;
                x++;
            }
            U_TEST_PRINT_LINE("a total of %d byte(s) sent in %d write(s).", offset, x);

            // Give the data time to come back
            for (x = 10; (x > 0) &&
                 (gTestConfig.bytesReceived < gTestConfig.bytesToSend); x--) {
                uPortTaskBlock(1000);
            }

            U_TEST_PRINT_LINE("TCP async receive task got %d segment(s)"
                              " totalling %d byte(s) and the send/receive"
                              " process took %d milliseconds.",
                              gTestConfig.packetsReceived,
                              gTestConfig.bytesReceived,
                              uPortGetTickTimeMs() - startTimeMs);

            // Check that we reassembled everything correctly
            U_PORT_TEST_ASSERT(checkAgainstSentData(gSendData,
                                                    gTestConfig.bytesToSend,
                                                    gTestConfig.pBuffer,
                                                    gTestConfig.bytesReceived));

            // Let the receive task close
            gTestConfig.asyncExit = true;
            uPortTaskBlock(U_LINUX_PPP_TEST_RECEIVE_TASK_EXIT_MS);
            gTestConfig.taskHandle = NULL;

            shutdown(sock, 0);
            close(sock);

            // Free memory
            uPortFree(gTestConfig.pBuffer);
            gTestConfig.pBuffer = NULL;

            // Free memory from event queues
            uPortEventQueueCleanUp();

        } else {
            U_TEST_PRINT_LINE("*** WARNING *** not testing PPP since device does not support it.");
        }
    }

    // Remove each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("taking down %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                 pTmp->networkType) == 0);
    }
    // Remove each device
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
U_PORT_TEST_FUNCTION("[testLinuxSock]", "testLinuxSockCleanUp")
{
    osCleanup();
    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    uDeviceDeinit();
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #ifdef U_CFG_PPP_ENABLE

// End of file
