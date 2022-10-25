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
 * @brief Tests for the ble "general" API: these should pass on all
 * platforms where one UART is available. No short range module is
 * actually used in this set of tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.

// Must always be included before u_short_range_test_selector.h
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

#if U_SHORT_RANGE_TEST_WIFI()

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_sock.h"

#include "u_at_client.h"

#include "u_short_range.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"
#include "u_wifi_sock.h"
#include "u_wifi_test_private.h"

#include "u_sock_test_shared_cfg.h"   // For some of the test macros

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_SOCK_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

//lint --emacro((774), TEST_CHECK_TRUE) suppress "Boolean within 'if' always evaluates to True"
#define TEST_CHECK_TRUE(x) \
    if (!(x)) { \
        if (gErrorLine == 0) { \
            gErrorLine = __LINE__; \
        } \
    }

//lint -esym(750, TEST_GET_ERROR_LINE) Suppress TEST_GET_ERROR_LINE not referenced
#define TEST_HAS_ERROR() (gErrorLine != 0)
#define TEST_CLEAR_ERROR() (gErrorLine = 0)
#define TEST_GET_ERROR_LINE() gErrorLine

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static long gErrorLine = 0;

static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };
/** UDP socket handle.
 */
static int32_t gSockHandleUdp = -1;

/** TCP socket handle.
 */
static int32_t gSockHandleTcp = -1;

/** Error indicator for call-backs: not using asserts
 * in call-backs as when they go off the seem to cause
 * stack overflows.
 */
static volatile int32_t gCallbackErrorNum = 0;

/** Flag to indicate that the UDP data
 * callback has been called.
 */
static volatile bool gDataCallbackCalledUdp = false;

/** Flag to indicate that the TCP data
 * callback has been called.
 */
static volatile bool gDataCallbackCalledTcp = false;

/** Flag to indicate that the UDP closed
 * callback has been called.
 */
static volatile bool gClosedCallbackCalledUdp = false;

/** Flag to indicate that the TCP closed
 * callback has been called.
 */
static volatile bool gClosedCallbackCalledTcp = false;

/** Flag to indicate that the TCP async closed
 * callback has been called.
 */
static volatile bool gAsyncClosedCallbackCalledTcp = false;

/** Flag to indicate that the UDP async closed
 * callback has been called.
 */
static volatile bool gAsyncClosedCallbackCalledUdp = false;

static const uint32_t gWifiStatusMaskAllUp = U_WIFI_STATUS_MASK_IPV4_UP |
                                             U_WIFI_STATUS_MASK_IPV6_UP;

static volatile int32_t gWifiConnected = 0;
static volatile uint32_t gWifiStatusMask = 0;

/** A string of all possible characters, including strings
 * that might appear as terminators in the AT interface.
 */
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n \r\nABORTED\r\n";

static uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                        .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                        .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                        .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                        .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                        .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                      };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback for data being available, UDP.

static void dataCallbackUdp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 1;
    } else if (sockHandle != gSockHandleUdp)  {
        gCallbackErrorNum = 2;
    }

    gDataCallbackCalledUdp = true;
}

// Callback for data being available, TCP.
static void dataCallbackTcp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 3;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 4;
    }
    gDataCallbackCalledTcp = true;
}

// Callback for socket closed, UDP.
static void closedCallbackUdp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 5;
    } else if (sockHandle != gSockHandleUdp)  {
        gCallbackErrorNum = 6;
    }

    gClosedCallbackCalledUdp = true;
}

// Callback for socket closed, TCP.
static void closedCallbackTcp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
#if !U_CFG_OS_CLIB_LEAKS
    U_TEST_PRINT_LINE("wifi socket closed devHandle: %d, sockHandle: %d.",
                      devHandle, sockHandle);
#endif
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 7;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 8;
    }

    gClosedCallbackCalledTcp = true;
}

// Callback for async socket closed, UDP
static void asyncClosedCallbackUdp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 9;
    } else if (sockHandle != gSockHandleUdp)  {
        gCallbackErrorNum = 10;
    }

    gAsyncClosedCallbackCalledUdp = true;
}

// Callback for async socket closed, TCP
static void asyncClosedCallbackTcp(uDeviceHandle_t devHandle, int32_t sockHandle)
{
    if (devHandle != gHandles.devHandle) {
        gCallbackErrorNum = 11;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 12;
    }

    gAsyncClosedCallbackCalledTcp = true;
}

static void wifiConnectionCallback(uDeviceHandle_t devHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    (void)devHandle;
    (void)connId;
    (void)channel;
    (void)pBssid;
    (void)disconnectReason;
    (void)pCallbackParameter;
    if (status == U_WIFI_CON_STATUS_CONNECTED) {
#if !U_CFG_OS_CLIB_LEAKS
        U_TEST_PRINT_LINE("connected Wifi connId: %d, bssid: %s, channel: %d.",
                          connId, pBssid, channel);
#endif
        gWifiConnected = 1;
    } else {
#if defined(U_CFG_ENABLE_LOGGING) && !U_CFG_OS_CLIB_LEAKS
        //lint -esym(752, strDisconnectReason)
        static const char strDisconnectReason[6][20] = {
            "Unknown", "Remote Close", "Out of range",
            "Roaming", "Security problems", "Network disabled"
        };
        if ((disconnectReason < 0) || (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            //lint -esym(438, disconnectReason)
            disconnectReason = 0;
        }
        U_TEST_PRINT_LINE("wifi connection lost connId: %d, reason: %d (%s).",
                          connId, disconnectReason,
                          strDisconnectReason[disconnectReason]);
#endif
        gWifiConnected = 0;
    }
}

static void wifiNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)devHandle;
    (void)interfaceType;
    (void)statusMask;
    (void)pCallbackParameter;
#if !U_CFG_OS_CLIB_LEAKS
    U_TEST_PRINT_LINE("network status IPv4 %s, IPv6 %s.",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");
#endif

    gWifiStatusMask = statusMask;
}

// Helper function to connect wifi
static void connectWifi()
{
    int32_t tmp;
    int32_t waitCtr = 0;

    // Add unsolicited response cb for connection status
    tmp = uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                           wifiConnectionCallback, NULL);
    TEST_CHECK_TRUE(tmp == 0);
    if (!TEST_HAS_ERROR()) {
        // Add unsolicited response cb for IP status
        tmp = uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                            wifiNetworkStatusCallback, NULL);
        TEST_CHECK_TRUE(tmp == 0);
    }
    if (!TEST_HAS_ERROR()) {
        // Connect to wifi network
        tmp = uWifiStationConnect(gHandles.devHandle,
                                  U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                  U_WIFI_AUTH_WPA_PSK,
                                  U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE));
        TEST_CHECK_TRUE(tmp == 0);
    }

    //Wait for connection and IP events.
    //There could be multiple IP events depending on network configuration.
    while (!TEST_HAS_ERROR() && (!gWifiConnected || (gWifiStatusMask != gWifiStatusMaskAllUp))) {
        if (waitCtr++ >= 15) {
            if (!gWifiConnected) {
                U_TEST_PRINT_LINE("unable to connect to WiFi network.");
                TEST_CHECK_TRUE(false);
            } else {
                U_TEST_PRINT_LINE("unable to retrieve IP address.");
                TEST_CHECK_TRUE(false);
            }
            break;
        }
        uPortTaskBlock(1000);
    }
}

// Helper function to disconnect wifi
static void disconnectWifi()
{
    int32_t tmp;
    int32_t waitCtr = 0;

    tmp = uWifiStationDisconnect(gHandles.devHandle);
    TEST_CHECK_TRUE(tmp == 0);

    while (!TEST_HAS_ERROR() && gWifiConnected) {
        if (waitCtr >= 5) {
            U_TEST_PRINT_LINE("unable to disconnect from WiFi network.");
            TEST_CHECK_TRUE(false);
            break;
        }
        uPortTaskBlock(1000);
        waitCtr++;
    }
    // Remove callbacks (regardless if there was an error)
    tmp = uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                           NULL, NULL);
    TEST_CHECK_TRUE(tmp == 0);

    tmp = uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                        NULL, NULL);
    TEST_CHECK_TRUE(tmp == 0);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiSock]", "wifiSockTCPTest")
{
    int32_t heapUsed;
    char *pBuffer;
    int32_t returnCode;

    TEST_CLEAR_ERROR();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    gWifiStatusMask = 0;
    gWifiConnected = 0;

    // Malloc a buffer to receive things into.
    pBuffer = (char *) pUPortMalloc(U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    // Do the standard preamble
    returnCode = uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                          &uart,
                                          &gHandles);
    TEST_CHECK_TRUE(returnCode == 0);

    // Connect to Wifi AP
    if (!TEST_HAS_ERROR()) {
        connectWifi();
    }

    // Init wifi sockets
    if (!TEST_HAS_ERROR() && (0 != uWifiSockInit())) {
        U_TEST_PRINT_LINE("unable to init socket.");
        TEST_CHECK_TRUE(false);
    }

    if (!TEST_HAS_ERROR() && (0 != uWifiSockInitInstance(gHandles.devHandle))) {
        U_TEST_PRINT_LINE("unable to init socket instance.");
        TEST_CHECK_TRUE(false);
    }

    // Create a TCP socket
    if (!TEST_HAS_ERROR()) {
        gSockHandleTcp = uWifiSockCreate(gHandles.devHandle, U_SOCK_TYPE_STREAM,
                                         U_SOCK_PROTOCOL_TCP);
        if (gSockHandleTcp < 0) {
            U_TEST_PRINT_LINE("unable to create socket, return code: %d.", gSockHandleTcp);
            TEST_CHECK_TRUE(false);
        }
    }
    if (!TEST_HAS_ERROR()) {
        uWifiSockRegisterCallbackData(gHandles.devHandle, gSockHandleTcp,
                                      dataCallbackTcp);
        uWifiSockRegisterCallbackClosed(gHandles.devHandle, gSockHandleTcp,
                                        closedCallbackTcp);
    }

    if (!TEST_HAS_ERROR()) {
        uSockAddress_t localAddress;
        returnCode = uWifiSockGetLocalAddress(gHandles.devHandle,
                                              gSockHandleTcp,
                                              &localAddress);
        TEST_CHECK_TRUE(returnCode == 0);
    }


    //lint -esym(645, remoteAddress) 'remoteAddress' may not have been initialized
    uSockAddress_t remoteAddress;
    // Lookup the IP address for the host name
    if (!TEST_HAS_ERROR()) {
        returnCode = uWifiSockGetHostByName(gHandles.devHandle,
                                            U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                            &remoteAddress.ipAddress);
        remoteAddress.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;
        TEST_CHECK_TRUE(returnCode == 0);
    }

    // Connect the TCP socket
    if (!TEST_HAS_ERROR()) {
        returnCode = uWifiSockConnect(gHandles.devHandle, gSockHandleTcp, &remoteAddress);
        if (returnCode != 0) {
            U_TEST_PRINT_LINE("unable to connect socket, return code: %d.", returnCode);
            TEST_CHECK_TRUE(false);
        }
    }

    if (!TEST_HAS_ERROR()) {
        U_TEST_PRINT_LINE("sending %d byte(s) to %s:%d in random sized chunks...",
                          sizeof(gAllChars), U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                          U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
        size_t bytesWritten = 0;
        size_t chunkCounter = 0;
        while ((bytesWritten < sizeof(gAllChars)) && (chunkCounter < 100) && !TEST_HAS_ERROR()) {
            size_t bytesToWrite = 1;
            if (sizeof(gAllChars) - bytesWritten > 1) {
                // Generate a random number between 1 and sizeof(gAllChars) - bytesWritten
                bytesToWrite = 1ul + ((uint32_t)rand() % ((sizeof(gAllChars) - bytesWritten) - 1ul));
            }
            chunkCounter++;
            returnCode = uWifiSockWrite(gHandles.devHandle, gSockHandleTcp,
                                        gAllChars + bytesWritten, bytesToWrite);
            if (returnCode > 0) {
                bytesWritten += returnCode;
            } else if (returnCode == 0) {
                uPortTaskBlock(500);
            } else {
                U_TEST_PRINT_LINE("uWifiSockWrite() returned: %d.", returnCode);
                TEST_CHECK_TRUE(false);
            }
        }
        U_TEST_PRINT_LINE("%d byte(s) sent in %d chunks.", bytesWritten, chunkCounter);
    }

    if (!TEST_HAS_ERROR()) {
        // Wait a little while to get a data callback
        // triggered by a URC
        for (size_t x = 100; (x > 0) && !gDataCallbackCalledTcp; x--) {
            uPortTaskBlock(100);
        }

        // Get the data back again
        U_TEST_PRINT_LINE("receiving TCP echo data back in random sized chunks...");
        size_t bytesRead = 0;
        size_t chunkCounter = 0;
        //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
        memset(pBuffer, 0, U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES);
        while ((bytesRead < sizeof(gAllChars)) && (chunkCounter < 100) && !TEST_HAS_ERROR()) {
            size_t bytesToRead = 1;
            if (sizeof(gAllChars) - bytesRead > 1) {
                // Generate a random number between 1 and sizeof(gAllChars) - bytesWritten
                bytesToRead = 1ul + ((uint32_t)rand() % ((sizeof(gAllChars) - bytesRead) - 1ul));
            }
            chunkCounter++;

            returnCode = uWifiSockRead(gHandles.devHandle, gSockHandleTcp,
                                       pBuffer + bytesRead, bytesToRead);
            if (returnCode > 0) {
                bytesRead += returnCode;
            } else if (returnCode == 0) {
                uPortTaskBlock(500);
            } else {
                U_TEST_PRINT_LINE("uWifiSockRead() returned: %d.", returnCode);
                TEST_CHECK_TRUE(false);
            }
        }
        U_TEST_PRINT_LINE("%d byte(s) echoed over TCP, received in %d receive call(s).",
                          bytesRead, chunkCounter);
        if (!gDataCallbackCalledTcp) {
            U_TEST_PRINT_LINE("*** WARNING *** the data callback was not called during"
                              " the test.  This can happen legimitately if all the reads"
                              " from the module happened to coincide with data receptions"
                              " and so the URC was not involved.  However if it happens"
                              " too often something may be wrong.");
        }

        // Compare data
        TEST_CHECK_TRUE(memcmp(pBuffer, gAllChars,
                               sizeof(gAllChars)) == 0);
    }

    if (!TEST_HAS_ERROR()) {
        // Socket should still be open
        TEST_CHECK_TRUE(!gClosedCallbackCalledTcp);
        TEST_CHECK_TRUE(!gAsyncClosedCallbackCalledTcp);
    }

    // Close TCP socket with asynchronous callback
    U_TEST_PRINT_LINE("closing sockets...");
    returnCode = uWifiSockClose(gHandles.devHandle, gSockHandleTcp,
                                &asyncClosedCallbackTcp);
    if (!TEST_HAS_ERROR()) {
        // Try to close the socket regardless if there are any previous errors.
        // But if is the first error we need to log it.
        if (returnCode != 0) {
            U_TEST_PRINT_LINE("unable to close socket, return code: %d.", returnCode);
            TEST_CHECK_TRUE(false);
        }
    }

    //Socket cleanup
    uWifiSockRegisterCallbackData(gHandles.devHandle, gSockHandleTcp,
                                  NULL);
    uWifiSockRegisterCallbackClosed(gHandles.devHandle, gSockHandleTcp,
                                    NULL);

    if (uWifiSockDeinitInstance(gHandles.devHandle) != 0) {
        U_TEST_PRINT_LINE("unable to deinit socket instance.");
        TEST_CHECK_TRUE(false);
    }
    // Deinit wifi sockets
    uWifiSockDeinit();

    // Cleanup
    disconnectWifi();
    uWifiTestPrivatePostamble(&gHandles);

    // Free memory
    uPortFree(pBuffer);

    // Now do all assert checking after cleanup

    if (TEST_HAS_ERROR()) {
        U_TEST_PRINT_LINE(__FILE__ ":%d:FAIL", TEST_GET_ERROR_LINE());
        U_PORT_TEST_ASSERT(false);
    }

    U_PORT_TEST_ASSERT_EQUAL(gCallbackErrorNum, 0);
    U_PORT_TEST_ASSERT(gClosedCallbackCalledTcp);

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}

U_PORT_TEST_FUNCTION("[wifiSock]", "wifiSockUDPTest")
{
    int32_t heapUsed;
    char *pBuffer;
    int32_t returnCode;

    TEST_CLEAR_ERROR();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    gWifiStatusMask = 0;
    gWifiConnected = 0;

    // Malloc a buffer to receive things into.
    pBuffer = (char *) pUPortMalloc(U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    // Do the standard preamble
    returnCode = uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                          &uart,
                                          &gHandles);
    TEST_CHECK_TRUE(returnCode == 0);

    // Connect to Wifi AP
    if (!TEST_HAS_ERROR()) {
        connectWifi();
    }

    // Init wifi sockets
    if (!TEST_HAS_ERROR() && (0 != uWifiSockInit())) {
        U_TEST_PRINT_LINE("unable to init socket.");
        TEST_CHECK_TRUE(false);
    }

    if (!TEST_HAS_ERROR() && (0 != uWifiSockInitInstance(gHandles.devHandle))) {
        U_TEST_PRINT_LINE("unable to init socket instance.");
        TEST_CHECK_TRUE(false);
    }

    // Create a UDP socket
    if (!TEST_HAS_ERROR()) {
        gSockHandleUdp = uWifiSockCreate(gHandles.devHandle, U_SOCK_TYPE_DGRAM,
                                         U_SOCK_PROTOCOL_UDP);
        if (gSockHandleUdp < 0) {
            U_TEST_PRINT_LINE("unable to create socket, return code: %d.", gSockHandleUdp);
            TEST_CHECK_TRUE(false);
        }
    }
    if (!TEST_HAS_ERROR()) {
        uWifiSockRegisterCallbackData(gHandles.devHandle, gSockHandleUdp,
                                      dataCallbackUdp);
        uWifiSockRegisterCallbackClosed(gHandles.devHandle, gSockHandleUdp,
                                        closedCallbackUdp);
    }

    //lint -esym(645, remoteAddress) 'remoteAddress' may not have been initialized
    uSockAddress_t remoteAddress;
    uSockAddress_t rxAddress;

    // Lookup the IP address for the host name
    if (!TEST_HAS_ERROR()) {
        returnCode = uWifiSockGetHostByName(gHandles.devHandle,
                                            U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                            &remoteAddress.ipAddress);
        remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;
        TEST_CHECK_TRUE(returnCode == 0);
    }

    if (!TEST_HAS_ERROR()) {
        // Send and wait for the UDP echo data, trying a few
        // times to reduce the chance of internet loss getting
        // in the way
        U_TEST_PRINT_LINE("sending %d byte(s) to %s:%d...",
                          sizeof(gAllChars),
                          U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                          U_SOCK_TEST_ECHO_UDP_SERVER_PORT);
        returnCode = 0;
        //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
        memset(pBuffer, 0, U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES);
        for (size_t x = 0; !TEST_HAS_ERROR() && (x < U_SOCK_TEST_UDP_RETRIES); x ++) {
            returnCode = uWifiSockSendTo(gHandles.devHandle, gSockHandleUdp,
                                         &remoteAddress,
                                         gAllChars, sizeof(gAllChars));
            if (returnCode != sizeof(gAllChars)) {
                U_TEST_PRINT_LINE("failed to send UDP data on try %d.", x + 1);
                continue;
            }

            // Wait a little while to get a data callback
            // triggered by a URC
            for (size_t a = 100; (a > 0) && !gDataCallbackCalledUdp; a--) {
                uPortTaskBlock(100);
            }
            returnCode = uWifiSockReceiveFrom(gHandles.devHandle,
                                              gSockHandleUdp,
                                              &rxAddress,
                                              pBuffer,
                                              U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES);
            if (returnCode != sizeof(gAllChars)) {
                U_TEST_PRINT_LINE("failed to receive UDP echo on try %d.", x + 1);
                continue;
            }

            // We are done
            break;
        }
        U_TEST_PRINT_LINE("%d byte(s) echoed over UDP.", returnCode);
        TEST_CHECK_TRUE(returnCode == sizeof(gAllChars));

        // Compare data
        TEST_CHECK_TRUE(memcmp(pBuffer, gAllChars,
                               sizeof(gAllChars)) == 0);
    }

    if (!TEST_HAS_ERROR()) {
        // Socket should still be open
        TEST_CHECK_TRUE(!gClosedCallbackCalledUdp);
        TEST_CHECK_TRUE(!gAsyncClosedCallbackCalledUdp);
    }

    // Close UDP socket with asynchronous callback
    U_TEST_PRINT_LINE("closing sockets...");
    returnCode = uWifiSockClose(gHandles.devHandle, gSockHandleUdp,
                                &asyncClosedCallbackUdp);
    if (!TEST_HAS_ERROR()) {
        // Try to close the socket regardless if there are any previous errors.
        // But if is the first error we need to log it.
        if (returnCode != 0) {
            U_TEST_PRINT_LINE("unable to close socket, return code: %d.", returnCode);
            TEST_CHECK_TRUE(false);
        }
    }

    //Socket cleanup
    uWifiSockRegisterCallbackData(gHandles.devHandle, gSockHandleUdp,
                                  NULL);
    uWifiSockRegisterCallbackClosed(gHandles.devHandle, gSockHandleUdp,
                                    NULL);

    if (uWifiSockDeinitInstance(gHandles.devHandle) != 0) {
        U_TEST_PRINT_LINE("unable to deinit socket instance.");
        TEST_CHECK_TRUE(false);
    }
    // Deinit wifi sockets
    uWifiSockDeinit();

    // Cleanup
    disconnectWifi();
    uWifiTestPrivatePostamble(&gHandles);

    // Free memory
    uPortFree(pBuffer);

    // Now do all assert checking after cleanup

    if (TEST_HAS_ERROR()) {
        U_TEST_PRINT_LINE(__FILE__ ":%d:FAIL", TEST_GET_ERROR_LINE());
        U_PORT_TEST_ASSERT(false);
    }

    U_PORT_TEST_ASSERT_EQUAL(gCallbackErrorNum, 0);
    U_PORT_TEST_ASSERT(gClosedCallbackCalledUdp);
    U_PORT_TEST_ASSERT(gAsyncClosedCallbackCalledUdp);

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}


#endif // U_SHORT_RANGE_TEST_WIFI()


// End of file
