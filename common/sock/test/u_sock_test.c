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
 * @brief Test for the sockets API: these should pass on all platforms
 * that include the appropriate communications hardware, and will be
 * run for all bearers for which the network API tests have
 * configuration information, i.e. cellular or BLE/Wifi for short range).
 * These tests use the network API and the test configuration information
 * from the network API to provide the communication path.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "errno.h"
#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "sys/time.h"      // struct timeval in most cases
#include "string.h"        // strncpy(), strcmp(), memcpy(), memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_port_clib_platform_specific.h" /* struct timeval in some cases. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"

#include "u_network.h"                  // In order to provide a comms
#include "u_network_test_shared_cfg.h"  // path for the socket

#include "u_sock_errno.h" // For U_SOCK_EWOULDBLOCK
#include "u_sock.h"
#include "u_sock_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES
/** The guard length to include before and after a packet buffer
 * when testing.
 */
# define U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES 256
#endif

/** The fill character that should be in
 * U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES.
 */
#define U_SOCK_TEST_FILL_CHARACTER 0xAA

#ifndef U_SOCK_TEST_TASK_STACK_SIZE_BYTES
/** The stack size to use for the test task created during
 * sockets testing.
 */
# define U_SOCK_TEST_TASK_STACK_SIZE_BYTES 2048
#endif

#ifndef U_SOCK_TEST_TASK_PRIORITY
/** The priority to use for the test task created during
 * sockets testing.  If an AT client is running make sure
 * that this is lower priority than its URC handler.
 */
# define U_SOCK_TEST_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 5)
#endif

#ifndef U_SOCK_TEST_RECEIVE_QUEUE_LENGTH
/** The queue length, used for asynchronous tests.
 */
# define U_SOCK_TEST_RECEIVE_QUEUE_LENGTH 10
#endif

#ifndef U_SOCK_TEST_MAX_UDP_PACKET_SIZE
/** A sensible maximum size for UDP packets sent over
 * the public internet when testing.
 */
# define U_SOCK_TEST_MAX_UDP_PACKET_SIZE 500
#endif

#ifndef U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE
/** The maximum TCP read/write size to use during testing.
 */
# define U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE 1024
#endif

#ifndef U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE
/** Sending just one byte doesn't always cause all
 * modules to actually send the data in a reasonable
 * time so set a sensible minimum here for testing.
 */
# define U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE 128
#endif

#ifndef U_SOCK_TEST_NON_BLOCKING_TIME_MS
/** Expected return time for non-blocking operation
 *in ms during testing.
 */
# define U_SOCK_TEST_NON_BLOCKING_TIME_MS (U_SOCK_RECEIVE_POLL_INTERVAL_MS + 250)
#endif

#ifndef U_SOCK_TEST_TIME_MARGIN_PLUS_MS
/** Positive margin on timers during sockets testing.
 * This has to be pretty sloppy because any AT command
 * delay will contribute to it in the case of a
 * cellular module.
 */
# define U_SOCK_TEST_TIME_MARGIN_PLUS_MS 1000
#endif

#ifndef U_SOCK_TEST_TIME_MARGIN_MINUS_MS
/** Negative margin on timers during sockets testing:
 * should be pretty small, certainly not larger than
 * 2 seconds which is the smallest timeout
 * we set in these tests.
 */
# define U_SOCK_TEST_TIME_MARGIN_MINUS_MS 100
#endif

// Do some cross-checking
#ifdef U_AT_CLIENT_URC_TASK_PRIORITY
# if (U_AT_CLIENT_URC_TASK_PRIORITY) <= (U_SOCK_TEST_TASK_PRIORITY)
#  error U_AT_CLIENT_URC_TASK_PRIORITY must be greater than U_SOCK_TEST_TASK_PRIORITY
# endif
#endif

#if U_SOCK_TEST_TIME_MARGIN_PLUS_MS > U_SOCK_RECEIVE_TIMEOUT_DEFAULT_MS
# error U_SOCK_TEST_TIME_MARGIN_PLUS_MS cannot be larger than U_SOCK_RECEIVE_TIMEOUT_DEFAULT_MS
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type for testing address string conversion.
 */
typedef struct {
    const char *pAddressString;
    uSockAddress_t address;
    bool hasPort;
    bool shouldError;
} uSockTestAddress_t;

/** Type for testing removing the port number from an address string.
 */
typedef struct {
    const char *pAddressStringOriginal;
    int32_t port;
    const char *pAddressStringNoPort;
} uSockTestPortRemoval_t;

/** Struct to pass to rxAsyncEventTask().
 */
typedef struct {
    uSockDescriptor_t descriptor;
    bool isTcp;
    char *pBuffer;
    size_t bufferLength;
    size_t bytesToSend;
    size_t bytesReceived;
    size_t packetsReceived;
    int32_t eventQueueHandle;
} uSockTestConfig_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Data structure passed around during
 * aynchronous reception of UDP packets.
 */
//lint -esym(785, gTestConfig) Suppress too few initialisers
static uSockTestConfig_t gTestConfig = {.eventQueueHandle = -1};

/** Array of inputs for address string testing.
 */
//lint -e{708} Suppress union initialisation.
static const uSockTestAddress_t gTestAddressList[] = {
    // IPV4
    {"0.0.0.0", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, false, false},
    {"0.0.0.0:0", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, false},
    {"0.1.2.3", {{U_SOCK_ADDRESS_TYPE_V4, {0x00010203}}, 0}, false, false},
    {"0.1.2.3:0", {{U_SOCK_ADDRESS_TYPE_V4, {0x00010203}}, 0}, true, false},
    {"255.255.255.255", {{U_SOCK_ADDRESS_TYPE_V4, {0xffffffff}}, 0}, false, false},
    {"255.255.255.255:65535", {{U_SOCK_ADDRESS_TYPE_V4, {0xffffffff}}, 65535}, true, false},
    // IPV4 error cases
    {"256.255.255.255:65535", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, true},
    {"255.256.255.255:65535", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, true},
    {"255.255.256.255:65535", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, true},
    {"255.255.255.256:65535", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, true},
    {"255.255.255.255:65536", {{U_SOCK_ADDRESS_TYPE_V4, {0x00000000}}, 0}, true, true},
    // IPV6
    {"0:0:0:0:0:0:0:0", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, false, false},
    {"[0:0:0:0:0:0:0:0]:0", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, false},
    // Note: the answer looks peculiar but remember that element 0 of the array is at the lowest address in memory and element 3 at
    // the highest address so, for network byte order, the lowest two values (b and c in the first case below) are
    // stored in the lowest array index, etc.
    {"0:1:2:3:4:a:b:c", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x000b000c, 0x0004000a, 0x00020003, 0x00000001}}}, 0}, false, false},
    {"[0:1:2:3:4:a:b:c]:0", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x000b000c, 0x0004000a, 0x00020003, 0x00000001}}}, 0}, true, false},
    {"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}}}, 0}, false, false},
    {"[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}}}, 65535}, true, false},
    // IPV6 error cases
    {"[1ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:1ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:1ffff:ffff:ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:1ffff:ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:ffff:1ffff:ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:ffff:ffff:1ffff:ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:ffff:ffff:ffff:1ffff:ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:ffff:ffff:ffff:ffff:1ffff]:65535", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true},
    {"[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65536", {{U_SOCK_ADDRESS_TYPE_V6, .address = {.ipv6 = {0x00000000, 0x00000000, 0x00000000, 0x00000000}}}, 0}, true, true}
};

/** A further set of inputs for address port removal.
 */
static const uSockTestPortRemoval_t gTestAddressPortRemoval[] = {
    {"0.0.0.0", -1, "0.0.0.0"},
    {"0.0.0.0:0", 0, "0.0.0.0"},
    {"0.0.0.0:65535", 65535, "0.0.0.0"},
    {"0:0:0:0:0:0:0:0", -1, "0:0:0:0:0:0:0:0"},
    {"[0:0:0:0:0:0:0:0]:0", 0, "0:0:0:0:0:0:0:0"},
    {"[0:0:0:0:0:0:0:0]:65535", 65535, "0:0:0:0:0:0:0:0"},
    {"fred.com", -1, "fred.com"},
    {"fred.com:0", 0, "fred.com"},
    {"fred.com:65535", 65535, "fred.com"}
};

/** Data to exchange.
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

/** A string of all possible characters, including strings
 * that might appear as terminators in an AT interface.
 */
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n \r\nABORTED\r\n";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print out an address structure.
//lint -esym(522, printAddress) Suppress "lacks side effects"
// when compiled out
static void printAddress(const uSockAddress_t *pAddress,
                         bool hasPort)
{
#if U_CFG_ENABLE_LOGGING
    switch (pAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            uPortLog("IPV4");
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            uPortLog("IPV6");
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
            uPortLog("IPV4V6");
            break;
        default:
            uPortLog("unknown type (%d)", pAddress->ipAddress.type);
            break;
    }

    uPortLog(" ");

    if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%u",
                     (pAddress->ipAddress.address.ipv4 >> (x * 8)) & 0xFF);
            if (x > 0) {
                uPortLog(".");
            }
        }
        if (hasPort) {
            uPortLog(":%u", pAddress->port);
        }
    } else if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
        if (hasPort) {
            uPortLog("[");
        }
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%x:%x", pAddress->ipAddress.address.ipv6[x] >> 16,
                     pAddress->ipAddress.address.ipv6[x] & 0xFFFF);
            if (x > 0) {
                uPortLog(":");
            }
        }
        if (hasPort) {
            uPortLog("]:%u", pAddress->port);
        }
    }
#else
    (void) pAddress;
    (void) hasPort;
#endif
}

// Test that two address structures are the same
static void addressAssert(const uSockAddress_t *pAddress1,
                          const uSockAddress_t *pAddress2,
                          bool hasPort)
{
    U_PORT_TEST_ASSERT(pAddress1->ipAddress.type == pAddress2->ipAddress.type);

    switch (pAddress1->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            U_PORT_TEST_ASSERT(pAddress1->ipAddress.address.ipv4 ==
                               pAddress2->ipAddress.address.ipv4);
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            U_PORT_TEST_ASSERT(memcmp(pAddress1->ipAddress.address.ipv6,
                                      pAddress2->ipAddress.address.ipv6,
                                      sizeof(pAddress2->ipAddress.address.ipv6)) == 0);
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
        // fall-through
        default:
            //lint -e(506, 774) Suppress constant Boolean value
            U_PORT_TEST_ASSERT(false);
            break;
    }

    if (hasPort) {
        U_PORT_TEST_ASSERT(pAddress1->port == pAddress2->port);
    }
}

// Make sure that size is greater than 0 and no more than limit,
// useful since, when moduloing a very large number number,
// compilers sometimes screw up and produce a small *negative*
// number.  Who knew?  For example, GCC decided that
// 492318453 (0x1d582ef5) modulo 508 was -47 (0xffffffd1).
static size_t fix(size_t size, size_t limit)
{
    if (size == 0) {
        size = limit / 2; // better than 1
    } else if (size > limit) {
        size = limit;
    }

    return size;
}

// Do this before every test to ensure there is a usable network.
static void stdPreamble()
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type if its not already been added
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle < 0) {
            if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
                uPortLog("U_SOCK_TEST: adding %s network...\n",
                         gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                         gUNetworkTestCfg[x].pConfiguration);
                U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
            }
        }
    }

    // It is possible for socket closure in an
    // underlying layer to have failed in a previous
    // test, leaving sockets hanging, so just in case,
    // clear them up here
    uSockDeinit();

    // Bring up each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle >= 0) {
            uPortLog("U_SOCK_TEST: bringing up %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkUp(gUNetworkTestCfg[x].handle) == 0);
        }
    }

    // Reset errno at the start
    errno = 0;
}

// Check a buffer of what was sent against
// what was echoed back and print out
// useful info if they differ
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
        for (x = 0; ((*(pDataReceived + x +
                        U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES) == *(pDataSent + x))) &&
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
            uPortLog("U_SOCK_TEST: difference at character %d (sent"
                     " \"%*.*s\", received \"%*.*s\").\n",
                     x + 1, z, z, pDataSent + y,
                     z, z, pDataReceived + y +
                     U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES);
#endif
            success = false;
        } else {
            // If they were all the same, check for overrun and underrun
            for (x = 0; x < U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES; x++) {
                if (*(pDataReceived + x) != (char) U_SOCK_TEST_FILL_CHARACTER) {
                    uPortLog("U_SOCK_TEST: guard area %d byte(s) before start"
                             " of buffer has been overwritten (expected 0x%02x,"
                             " got 0x%02x %d '%c').\n",
                             U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES - x,
                             U_SOCK_TEST_FILL_CHARACTER,
                             *(pDataReceived + x), *(pDataReceived + x),
                             *(pDataReceived + x));
                    success = false;
                    break;
                }
                if (*(pDataReceived + U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES +
                      dataSentSizeBytes + x) != (char) U_SOCK_TEST_FILL_CHARACTER) {
                    uPortLog("U_SOCK_TEST: guard area %d byte(s) after end of"
                             " buffer has been overwritten (expected 0x%02x, got"
                             " 0x%02x %d '%c').\n",
                             x, U_SOCK_TEST_FILL_CHARACTER,
                             *(pDataReceived + dataSentSizeBytes + x),
                             *(pDataReceived + dataSentSizeBytes + x),
                             *(pDataReceived + dataSentSizeBytes + x));
                    success = false;
                    break;
                }
            }
        }
    } else {
        uPortLog("U_SOCK_TEST: %d byte(s) missing (%d byte(s)"
                 " received when %d were expected)).\n",
                 dataSentSizeBytes - dataReceivedSizeBytes,
                 dataReceivedSizeBytes, dataSentSizeBytes);
        success = false;
    }

    return success;
}

// Do a UDP socket echo test to a given host of a given packet size.
static int32_t doUdpEchoBasic(uSockDescriptor_t descriptor,
                              const uSockAddress_t *pRemoteAddress,
                              const char *pSendData,
                              size_t sendSizeBytes)
{
    char *pDataReceived = (char *) malloc(sendSizeBytes +
                                          (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
    uSockAddress_t senderAddress;
    int32_t sentSizeBytes;
    int32_t receivedSizeBytes = 0;
#if U_CFG_ENABLE_LOGGING
    int64_t timeNowMs;
#endif

    U_PORT_TEST_ASSERT(pDataReceived != NULL);

    // Retry this a few times, don't want to fail due to a flaky link
    for (size_t x = 0; (receivedSizeBytes != sendSizeBytes) &&
         (x < U_SOCK_TEST_UDP_RETRIES); x++) {
        uPortLog("U_SOCK_TEST: echo testing UDP packet size %d byte(s), try %d.\n",
                 sendSizeBytes, x + 1);
        sentSizeBytes = uSockSendTo(descriptor, pRemoteAddress,
                                    (const void *) pSendData, sendSizeBytes);
        if (sentSizeBytes >= 0) {
            uPortLog("U_SOCK_TEST: sent %d byte(s) of UDP data.\n", sentSizeBytes);
        } else {
            uPortLog("U_SOCK_TEST: failed to send over UDP.\n");
            // Reset errno 'cos we're going to retry and subsequent things might be upset by it
            errno = 0;
        }
        if (sentSizeBytes == sendSizeBytes) {
#if U_CFG_ENABLE_LOGGING
            timeNowMs = (int32_t) uPortGetTickTimeMs();
#endif
            //lint -e(668) Suppress possible use of NULL pointer
            // for pDataReceived (it is checked above)
            memset(pDataReceived, U_SOCK_TEST_FILL_CHARACTER,
                   sendSizeBytes + (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
            receivedSizeBytes = uSockReceiveFrom(descriptor, &senderAddress,
                                                 pDataReceived +
                                                 U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                                 sendSizeBytes);
            if (receivedSizeBytes >= 0) {
                uPortLog("U_SOCK_TEST: received %d byte(s) of UDP data from ",
                         receivedSizeBytes);
                printAddress(&senderAddress, true);
                uPortLog(".\n");
            } else {
                uPortLog("U_SOCK_TEST: received no UDP data back after %d ms.\n",
                         (int32_t) (uPortGetTickTimeMs() - timeNowMs));
                // Reset errno 'cos we're going to retry and subsequent things might be upset by it
                errno = 0;
            }
            if (receivedSizeBytes == sendSizeBytes) {
                U_PORT_TEST_ASSERT(memcmp(pSendData, pDataReceived +
                                          U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                          sendSizeBytes) == 0);
                for (size_t y = 0; y < U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES; y++) {
                    U_PORT_TEST_ASSERT(*(pDataReceived + y) == (char) U_SOCK_TEST_FILL_CHARACTER);
                    U_PORT_TEST_ASSERT(*(pDataReceived +
                                         U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES +
                                         sendSizeBytes + y) == (char) U_SOCK_TEST_FILL_CHARACTER);
                }
                if (pRemoteAddress != NULL) {
                    addressAssert(pRemoteAddress, &senderAddress, true);
                }
            } else {
                // Give us something to search for in the log
                uPortLog("U_SOCK_TEST: *** WARNING *** RETRY UDP.\n");
            }
        }
    }

    free(pDataReceived);

    return receivedSizeBytes;
}

// Event task triggered by the arrival of data.
//lint -e{818} Suppress could be const, need to follow
// function signature
static void rxAsyncEventTask(void *pParameter, size_t parameterLength)
{
    int32_t sizeBytes;
    // The parameter that arrives here is a pointer to the
    // payload which is itself a pointer to gTestConfig,
    // hence the need to double dereference here.
    uSockTestConfig_t *pTestConfig = *((uSockTestConfig_t **) pParameter);

    (void) parameterLength;

    // Read from the socket until there's nothing left to read
    //lint -e{776} Suppress possible truncation of addition
    do {
        if (pTestConfig->isTcp) {
            sizeBytes = uSockRead(pTestConfig->descriptor,
                                  pTestConfig->pBuffer +
                                  pTestConfig->bytesReceived +
                                  U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                  pTestConfig->bytesToSend -
                                  pTestConfig->bytesReceived);
        } else {
            sizeBytes = uSockReceiveFrom(pTestConfig->descriptor,
                                         NULL,
                                         pTestConfig->pBuffer +
                                         pTestConfig->bytesReceived +
                                         U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                         pTestConfig->bytesToSend -
                                         pTestConfig->bytesReceived);
        }
        if (sizeBytes > 0) {
            uPortLog("U_SOCK_TEST: received %d byte(s) of data @%d ms.\n",
                     sizeBytes, (int32_t) uPortGetTickTimeMs());
            pTestConfig->bytesReceived += sizeBytes;
            pTestConfig->packetsReceived++;
        }
    } while (sizeBytes > 0);
}

// Send an entire TCP data buffer until done
static size_t sendTcp(uSockDescriptor_t descriptor,
                      const char *pData, size_t sizeBytes)
{
    int32_t x;
    size_t sentSizeBytes = 0;
    int64_t startTimeMs;

    uPortLog("U_SOCK_TEST: sending %d byte(s) of TCP data...\n",
             sizeBytes);
    startTimeMs = uPortGetTickTimeMs();
    while ((sentSizeBytes < sizeBytes) &&
           ((uPortGetTickTimeMs() - startTimeMs) < 10000)) {
        x = uSockWrite(descriptor, (const void *) pData,
                       sizeBytes - sentSizeBytes);
        if (x > 0) {
            sentSizeBytes += x;
            uPortLog("U_SOCK_TEST: sent %d byte(s) of TCP data @%d ms.\n",
                     sentSizeBytes, (int32_t) uPortGetTickTimeMs());
        }
    }

    return sentSizeBytes;
}

// Open a socket and use it
static uSockDescriptor_t openSocketAndUseIt(int32_t networkHandle,
                                            const uSockAddress_t *pRemoteAddress,
                                            uSockType_t type,
                                            uSockProtocol_t protocol,
                                            int32_t *pHeapXxxSockInitLoss)
{
    int32_t errorCode = -1;
    uSockDescriptor_t descriptor;
    uSockAddress_t address;

    uPortLog("U_SOCK_TEST: creating socket...\n");
    // Creating a socket may use heap in the underlying
    // network layer which will be reclaimed when the
    // network layer is closed but we don't do that here
    // to save time so need to allow for it in the heap loss
    // calculation
    *pHeapXxxSockInitLoss += uPortGetHeapFree();
    descriptor = uSockCreate(networkHandle, type, protocol);
    *pHeapXxxSockInitLoss -= uPortGetHeapFree();
    uPortLog("U_SOCK_TEST: socket descriptor %d, errno %d.\n",
             descriptor, errno);
    if (descriptor >= 0) {
        U_PORT_TEST_ASSERT(errno == 0);

        uPortLog("U_SOCK_TEST: connecting socket to ");
        printAddress(pRemoteAddress, true);
        uPortLog("...\n");

        // Connections can fail so allow this a few goes
        for (size_t x = 2; (x > 0) && (errorCode < 0); x--) {
            errorCode = uSockConnect(descriptor, pRemoteAddress);
            uPortLog("U_SOCK_TEST: uSockConnect() returned %d, errno %d.\n",
                     errorCode, errno);
            if (errorCode < 0) {
                U_PORT_TEST_ASSERT(errno != 0);
                errno = 0;
            }
        }
        U_PORT_TEST_ASSERT(errorCode == 0);

        uPortLog("U_SOCK_TEST: check that uSockGetRemoteAddress()"
                 " works...\n");
        U_PORT_TEST_ASSERT(uSockGetRemoteAddress(descriptor,
                                                 &address) == 0);
        addressAssert(pRemoteAddress, &address, true);
        U_PORT_TEST_ASSERT(errno == 0);

        // UDP because of the 30 second TCP socket close
        // time on celular SARA-R4 module
        uPortLog("U_SOCK_TEST: testing that we can send and"
                 " receive to server...\n");
        U_PORT_TEST_ASSERT(doUdpEchoBasic(descriptor, NULL, gAllChars,
                                          sizeof(gAllChars)) == sizeof(gAllChars));
    }

    return descriptor;
}

// Callback to set the passed-in parameter pointer
// to be true.
static void setBoolCallback(void *pParameter)
{
    if (pParameter != NULL) {
        *((bool *) pParameter) = true;
    }
}

// Callback to send to event queue triggered by
// data arriving.
//lint -e{818} Suppress could be const, need to follow
// function signature
static void sendToEventQueue(void *pParameter)
{
    // Forward the pointer to rxAsyncEventTask().
    // Note: uPortEventQueueSend() expects to
    // receive a pointer to a payload, so here
    // we give it the address of pParameter,
    // so that it will send on a copy
    // of the pointer that is pParameter.
    uPortEventQueueSend(((uSockTestConfig_t *) pParameter)->eventQueueHandle,
                        &pParameter, sizeof(uSockTestConfig_t *));
}

// Release OS resources that may have been left hanging
// by a failed test
static void osCleanup()
{
    if (gTestConfig.eventQueueHandle >= 0) {
        uPortEventQueueClose(gTestConfig.eventQueueHandle);
        gTestConfig.eventQueueHandle = -1;
    }
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test conversion of address strings into structs and
 * back again.  This test is purely local, no network
 * connection is required.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockAddressStrings")
{
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
    int32_t errorCode;
    int32_t port;
    uSockAddress_t address;
    char *pAddress;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // No need to initialise anything for this test
    for (size_t x = 0; x < sizeof(gTestAddressList) /
         sizeof(gTestAddressList[0]); x++) {
        uPortLog("U_SOCK_TEST: %d: original address string \"%s\""
                 " (%d byte(s)).\n", x,
                 gTestAddressList[x].pAddressString,
                 strlen(gTestAddressList[x].pAddressString));
        // Convert string to struct
        memset(&address, 0xFF, sizeof(address));
        errorCode = uSockStringToAddress(gTestAddressList[x].pAddressString,
                                         &address);
        uPortLog("U_SOCK_TEST: %d: uSockStringToAddress() returned %d.\n",
                 x, errorCode);
        if (gTestAddressList[x].shouldError) {
            U_PORT_TEST_ASSERT(errorCode < 0);
        } else {
            U_PORT_TEST_ASSERT(errorCode == 0);

            uPortLog("U_SOCK_TEST: %d: address struct should contain ", x);
            printAddress(&gTestAddressList[x].address,
                         gTestAddressList[x].hasPort);
            uPortLog(".\n");

            uPortLog("U_SOCK_TEST: %d: address struct contains ", x);
            printAddress(&address, gTestAddressList[x].hasPort);
            uPortLog(".\n");

            addressAssert(&address, &gTestAddressList[x].address,
                          gTestAddressList[x].hasPort);

            // Copy the address string into the buffer so that
            // uSockDomainGetPort can write to it
            strncpy(buffer, gTestAddressList[x].pAddressString,
                    sizeof(buffer));
            if (gTestAddressList[x].hasPort) {
                U_PORT_TEST_ASSERT(uSockDomainGetPort(buffer) == address.port);
                // Now convert back to a string again
                memset(buffer, 0xFF, sizeof(buffer));
                errorCode = uSockAddressToString(&address, buffer,
                                                 sizeof(buffer));
                uPortLog("U_SOCK_TEST: %d: uSockAddressToString()"
                         " returned %d", x, errorCode);
                if (errorCode >= 0) {
                    uPortLog(", string is \"%s\" (%d byte(s))", buffer,
                             strlen(buffer));
                }
                uPortLog(".\n");
                U_PORT_TEST_ASSERT(errorCode == strlen(buffer));
                U_PORT_TEST_ASSERT(strcmp(gTestAddressList[x].pAddressString,
                                          buffer) == 0);
            } else {
                U_PORT_TEST_ASSERT(uSockDomainGetPort(buffer) == -1);
                // For ones without a port number we can converting the non-port
                // part of the address back into a string also
                memset(buffer, 0xFF, sizeof(buffer));
                errorCode = uSockIpAddressToString(&(address.ipAddress),
                                                   buffer,
                                                   sizeof(buffer));
                uPortLog("U_SOCK_TEST: %d: uSockIpAddressToString()"
                         " returned %d", x, errorCode);
                if (errorCode >= 0) {
                    uPortLog(", address string is \"%s\" (%d byte(s))",
                             buffer, strlen(buffer));
                }
                uPortLog(".\n");
                U_PORT_TEST_ASSERT(errorCode == strlen(buffer));
                U_PORT_TEST_ASSERT(strcmp(gTestAddressList[x].pAddressString,
                                          buffer) == 0);
            }
            // Leave a gap in order not overwhelm the debug output
            uPortTaskBlock(1);
        }
    }

    // Test removing port numbers from an address string
    for (size_t x = 0; x < sizeof(gTestAddressPortRemoval) /
         sizeof(gTestAddressPortRemoval[0]); x++) {
        strncpy(buffer, gTestAddressPortRemoval[x].pAddressStringOriginal,
                sizeof(buffer));
        uPortLog("U_SOCK_TEST: %d: original address string \"%s\""
                 " expected port number %d,"
                 " expected address string after port removal \"%s\".\n",
                 x, buffer, gTestAddressPortRemoval[x].port,
                 gTestAddressPortRemoval[x].pAddressStringNoPort);
        port = uSockDomainGetPort(buffer);
        uPortLog("U_SOCK_TEST: port number is %d.\n", port);
        U_PORT_TEST_ASSERT(port == gTestAddressPortRemoval[x].port);
        pAddress = pUSockDomainRemovePort(buffer);
        uPortLog("U_SOCK_TEST: result of port removal \"%s\".\n",
                 pAddress);
        U_PORT_TEST_ASSERT(strcmp(pAddress,
                                  gTestAddressPortRemoval[x].pAddressStringNoPort) == 0);
        port = uSockDomainGetPort(pAddress);
        uPortLog("U_SOCK_TEST: port number is now %d.\n", port);
        U_PORT_TEST_ASSERT(port == -1);
        // Leave a gap in order not overwhelm the debug output
        uPortTaskBlock(10);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_SOCK_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Basic UDP test.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockBasicUdp")
{
    int32_t errorCode;
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockAddress_t address;
    uSockDescriptor_t descriptor;
    bool dataCallbackCalled;
    size_t sizeBytes;
    bool success = false;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing basic UDP test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for UDP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Quite often nothing at all comes back so retry this
            // if that is the case
            for (size_t retries = 2; !success && (retries > 0); retries--) {
                success = true;
                // Create a UDP socket
                // Creating a socket may use heap in the underlying
                // network layer which will be reclaimed when the
                // network layer is closed but we don't do that here
                // to save time so need to allow for it in the heap loss
                // calculation
                heapXxxSockInitLoss += uPortGetHeapFree();
                descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_DGRAM,
                                         U_SOCK_PROTOCOL_UDP);
                heapXxxSockInitLoss -= uPortGetHeapFree();
                U_PORT_TEST_ASSERT(descriptor >= 0);
                U_PORT_TEST_ASSERT(errno == 0);

                uPortLog("U_SOCK_TEST: get local address...\n");
                U_PORT_TEST_ASSERT(uSockGetLocalAddress(descriptor,
                                                        &address) == 0);
                uPortLog("U_SOCK_TEST: local address is: ");
                printAddress(&address, true);
                uPortLog(".\n");

                // Set up the data callback
                dataCallbackCalled = false;
                uSockRegisterCallbackData(descriptor, setBoolCallback,
                                          &dataCallbackCalled);
                U_PORT_TEST_ASSERT(!dataCallbackCalled);

                uPortLog("U_SOCK_TEST: first test run without connect(),"
                         " sending to address ");
                printAddress(&remoteAddress, true);
                uPortLog("...\n");
                // Test min size
                if (doUdpEchoBasic(descriptor, &remoteAddress,
                                   gSendData, 1) != 1) {
                    success = false;
                }

                if (!dataCallbackCalled) {
                    success = false;
                }
                dataCallbackCalled = false;
                // Remove the data callback
                uSockRegisterCallbackData(descriptor, NULL, NULL);

                // Test max size
                if (doUdpEchoBasic(descriptor, &remoteAddress,
                                   gSendData,
                                   U_SOCK_TEST_MAX_UDP_PACKET_SIZE) != U_SOCK_TEST_MAX_UDP_PACKET_SIZE) {
                    success = false;
                }

                // Test some random sizes in-between
                for (size_t y = 0; (y < 10) && success; y++) {
                    sizeBytes = (rand() % U_SOCK_TEST_MAX_UDP_PACKET_SIZE) + 1;
                    sizeBytes = fix(sizeBytes, U_SOCK_TEST_MAX_UDP_PACKET_SIZE);
                    // Test max size
                    if (doUdpEchoBasic(descriptor, &remoteAddress,
                                       gSendData, sizeBytes) != sizeBytes) {
                        success = false;
                    }
                }

                uPortLog("U_SOCK_TEST: check that uSockGetRemoteAddress()"
                         " fails...\n");
                U_PORT_TEST_ASSERT(uSockGetRemoteAddress(descriptor,
                                                         &address) < 0);
                U_PORT_TEST_ASSERT(errno > 0);
                errno = 0;

                uPortLog("U_SOCK_TEST: now connect socket to \"%s:%d\"...\n",
                         U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                         U_SOCK_TEST_ECHO_UDP_SERVER_PORT);
                // Connections can fail so allow this a few goes
                errorCode = -1;
                for (size_t y = 2; (y > 0) && (errorCode < 0); y--) {
                    errorCode = uSockConnect(descriptor, &remoteAddress);
                    uPortLog("U_SOCK_TEST: uSockConnect() returned %d, errno %d.\n",
                             errorCode, errno);
                    if (errorCode < 0) {
                        U_PORT_TEST_ASSERT(errno != 0);
                        errno = 0;
                    }
                }
                U_PORT_TEST_ASSERT(errorCode == 0);

                uPortLog("U_SOCK_TEST: check that uSockGetRemoteAddress()"
                         " works...\n");
                U_PORT_TEST_ASSERT(uSockGetRemoteAddress(descriptor,
                                                         &address) == 0);
                addressAssert(&remoteAddress, &address, true);
                U_PORT_TEST_ASSERT(errno == 0);

                uPortLog("U_SOCK_TEST: second run after connect()...\n");
                // Test min and max
                if (doUdpEchoBasic(descriptor, NULL,
                                   gSendData, 1) != 1) {
                    success = false;
                }
                if (doUdpEchoBasic(descriptor, NULL, gSendData,
                                   U_SOCK_TEST_MAX_UDP_PACKET_SIZE) != U_SOCK_TEST_MAX_UDP_PACKET_SIZE) {
                    success = false;
                }

                uPortLog("U_SOCK_TEST: testing that we can send and receive all"
                         " possible characters...\n");
                if (doUdpEchoBasic(descriptor, NULL,
                                   gSendData, sizeof(gAllChars)) != sizeof(gAllChars)) {
                    success = false;
                }

                // Close the socket
                U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
                uSockCleanUp();

                // Check for memory leaks
                heapUsed -= uPortGetHeapFree();
                uPortLog("U_SOCK_TEST: during this part of the test %d"
                         " byte(s) were lost to sockets initialisation;"
                         " we have leaked %d byte(s).\n",
                         heapSockInitLoss + heapXxxSockInitLoss,
                         heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
                U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
            }
        }
    }
}

/** Basic TCP test.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockBasicTcp")
{
    int32_t errorCode;
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockAddress_t address;
    uSockDescriptor_t descriptor;
    bool dataCallbackCalled;
    bool closedCallbackCalled;
    size_t sizeBytes;
    size_t offset;
    int32_t y;
    char *pDataReceived;
    int64_t startTimeMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing basic TCP test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME);

            // Look up the address of the server we use for TCP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

            // Create a TCP socket
            // Creating a socket may use heap in the underlying
            // network layer which will be reclaimed when the
            // network layer is closed but we don't do that here
            // to save time so need to allow for it in the heap loss
            // calculation
            heapXxxSockInitLoss += uPortGetHeapFree();
            descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);
            heapXxxSockInitLoss -= uPortGetHeapFree();
            U_PORT_TEST_ASSERT(descriptor >= 0);
            U_PORT_TEST_ASSERT(errno == 0);

            uPortLog("U_SOCK_TEST: get local address...\n");
            U_PORT_TEST_ASSERT(uSockGetLocalAddress(descriptor,
                                                    &address) == 0);
            uPortLog("U_SOCK_TEST: local address is: ");
            printAddress(&address, true);
            uPortLog(".\n");

            // Set up the data callback
            dataCallbackCalled = false;
            uSockRegisterCallbackData(descriptor, setBoolCallback,
                                      &dataCallbackCalled);
            U_PORT_TEST_ASSERT(!dataCallbackCalled);

            // Set up the closed callback
            closedCallbackCalled = false;
            uSockRegisterCallbackClosed(descriptor, setBoolCallback,
                                        &closedCallbackCalled);
            U_PORT_TEST_ASSERT(!closedCallbackCalled);

            // Connect the socket
            uPortLog("U_SOCK_TEST: connect socket to \"%s:%d\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                     U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
            // Connections can fail so allow this a few goes
            errorCode = -1;
            for (y = 2; (y > 0) && (errorCode < 0); y--) {
                errorCode = uSockConnect(descriptor, &remoteAddress);
                if (errorCode < 0) {
                    U_PORT_TEST_ASSERT(errno != 0);
                    errno = 0;
                }
            }
            U_PORT_TEST_ASSERT(errorCode == 0);

            uPortLog("U_SOCK_TEST: check that uSockGetRemoteAddress()"
                     " works...\n");
            U_PORT_TEST_ASSERT(uSockGetRemoteAddress(descriptor,
                                                     &address) == 0);
            addressAssert(&remoteAddress, &address, true);
            U_PORT_TEST_ASSERT(errno == 0);

            uPortLog("U_SOCK_TEST: sending/receiving data over a"
                     " TCP socket...\n");

            // Throw random sized TCP segments up...
            offset = 0;
            y = 0;
            while (offset < sizeof(gSendData) - 1) {
                sizeBytes = (rand() % U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE) + 1;
                sizeBytes = fix(sizeBytes,
                                U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE);
                if (offset + sizeBytes > sizeof(gSendData) - 1) {
                    sizeBytes = (sizeof(gSendData) - 1) - offset;
                }
                if (sendTcp(descriptor, gSendData + offset,
                            sizeBytes) == sizeBytes) {
                    offset += sizeBytes;
                }
                y++;
            }
            sizeBytes = offset;
            uPortLog("U_SOCK_TEST: %d byte(s) sent via TCP @%d ms,"
                     " now receiving...\n", sizeBytes,
                     (int32_t) uPortGetTickTimeMs());

            // ...and capture them all again afterwards
            pDataReceived = (char *) malloc((sizeof(gSendData) - 1) +
                                            (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
            U_PORT_TEST_ASSERT(pDataReceived != NULL);
            //lint -e(668) Suppress possible use of NULL pointer
            // for pDataReceived
            memset(pDataReceived,
                   U_SOCK_TEST_FILL_CHARACTER,
                   (sizeof(gSendData) - 1) + (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
            startTimeMs = uPortGetTickTimeMs();
            offset = 0;
            //lint -e{441} Suppress loop variable not found in
            // condition: we're using time instead
            for (y = 0; (offset < sizeof(gSendData) - 1) &&
                 (uPortGetTickTimeMs() - startTimeMs < 20000); y++) {
                sizeBytes = uSockRead(descriptor,
                                      pDataReceived + offset +
                                      U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                      (sizeof(gSendData) - 1) - offset);
                if (sizeBytes > 0) {
                    uPortLog("U_SOCK_TEST: received %d byte(s) on TCP socket.\n",
                             sizeBytes);
                    offset += sizeBytes;
                }
            }
            sizeBytes = offset;
            if (sizeBytes < sizeof(gSendData) - 1) {
                uPortLog("U_SOCK_TEST: only %d byte(s) received after %d ms.\n",
                         sizeBytes,
                         (int32_t) (uPortGetTickTimeMs() - startTimeMs));
            } else {
                uPortLog("U_SOCK_TEST: all %d byte(s) received back after"
                         " %d ms, checking if they were as expected...\n",
                         sizeBytes,
                         (int32_t) (uPortGetTickTimeMs() - startTimeMs));
            }

            // Check that we reassembled everything correctly
            U_PORT_TEST_ASSERT(checkAgainstSentData(gSendData,
                                                    sizeof(gSendData) - 1,
                                                    pDataReceived,
                                                    sizeBytes));

            uPortLog("U_SOCK_TEST: shutting down socket for read...\n");
            errorCode = uSockShutdown(descriptor,
                                      U_SOCK_SHUTDOWN_READ);
            uPortLog("U_SOCK_TEST: uSockShutdown() returned %d, errno %d.\n",
                     errorCode, errno);
            U_PORT_TEST_ASSERT(errorCode >= 0);
            U_PORT_TEST_ASSERT(errno == 0);
            U_PORT_TEST_ASSERT(uSockRead(descriptor,
                                         pDataReceived +
                                         U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                         sizeof(gSendData) - 1) < 0);
            U_PORT_TEST_ASSERT(errno > 0);
            errno = 0;

            uPortLog("U_SOCK_TEST: shutting down socket for write...\n");
            errorCode = uSockShutdown(descriptor,
                                      U_SOCK_SHUTDOWN_WRITE);
            uPortLog("U_SOCK_TEST: uSockShutdown() returned %d, errno %d.\n",
                     errorCode, errno);
            U_PORT_TEST_ASSERT(errorCode >= 0);
            U_PORT_TEST_ASSERT(errno == 0);
            U_PORT_TEST_ASSERT(uSockWrite(descriptor, gSendData,
                                          sizeof(gSendData) - 1) < 0);
            U_PORT_TEST_ASSERT(errno > 0);
            errno = 0;

            // Close the socket
            U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
            uPortLog("U_SOCK_TEST: waiting up to %d second(s) for TCP"
                     " socket to close...\n",
                     U_SOCK_TEST_TCP_CLOSE_SECONDS);
            for (y = 0; (y < U_SOCK_TEST_TCP_CLOSE_SECONDS) &&
                 !closedCallbackCalled; y++) {
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(closedCallbackCalled);
            uSockCleanUp();

            free(pDataReceived);

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test %d"
                     " byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
        }
    }
}

/** Test maximum number of sockets.
 * TODO: this test assumes that all underlying bearers
 * are able to support U_SOCK_MAX_NUM_SOCKETS
 * simultaneously, TBD if this is the case.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockMaxNumSockets")
{
    int32_t errorCode;
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockDescriptor_t descriptor[U_SOCK_MAX_NUM_SOCKETS + 1];
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: testing max num sockets on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for UDP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Open as many sockets as we are allowed to simultaneously
            // and use each one of them
            uPortLog("U_SOCK_TEST: opening %d socket(s) at the same time.\n",
                     (sizeof(descriptor) / sizeof(descriptor[0])) - 1);
            for (size_t y = 0; y < (sizeof(descriptor) /
                                    sizeof(descriptor[0])) - 1; y++) {
                uPortLog("U_SOCK_TEST: socket %d.\n", y + 1);
                descriptor[y] = openSocketAndUseIt(networkHandle,
                                                   &remoteAddress,
                                                   U_SOCK_TYPE_DGRAM,
                                                   U_SOCK_PROTOCOL_UDP,
                                                   &heapXxxSockInitLoss);
                U_PORT_TEST_ASSERT(descriptor[y] >= 0);
                U_PORT_TEST_ASSERT(errno == 0);
            }

            // Now try to open one more and it should fail
            uPortLog("U_SOCK_TEST: opening one more, should fail.\n");
            descriptor[(sizeof(descriptor) /
                                            sizeof(descriptor[0])) - 1] = openSocketAndUseIt(networkHandle,
                                                                         &remoteAddress,
                                                                         U_SOCK_TYPE_DGRAM,
                                                                         U_SOCK_PROTOCOL_UDP,
                                                                         &heapXxxSockInitLoss);
            U_PORT_TEST_ASSERT(descriptor[(sizeof(descriptor) /
                                                               sizeof(descriptor[0])) - 1] < 0);
            U_PORT_TEST_ASSERT(errno > 0);
            errno = 0;

            // Close one and should be able to open another
            uPortLog("U_SOCK_TEST: closing socket %d (may take"
                     " some time).\n", descriptor[0]);
            errorCode = uSockClose(descriptor[0]);
            uPortLog("U_SOCK_TEST: uSockClose() returned %d, errno %d.\n",
                     errorCode, errno);
            U_PORT_TEST_ASSERT(errorCode == 0);
            U_PORT_TEST_ASSERT(errno == 0);
            uPortLog("U_SOCK_TEST: opening one more, should succeed.\n");
            descriptor[0] = openSocketAndUseIt(networkHandle,
                                               &remoteAddress,
                                               U_SOCK_TYPE_DGRAM,
                                               U_SOCK_PROTOCOL_UDP,
                                               &heapXxxSockInitLoss);
            U_PORT_TEST_ASSERT(descriptor[0] >= 0);
            U_PORT_TEST_ASSERT(errno == 0);

            // Now close the lot
            uPortLog("U_SOCK_TEST: closing them all.\n");
            for (size_t y = 0; y < (sizeof(descriptor) /
                                    sizeof(descriptor[0])) - 1; y++) {
                uPortLog("U_SOCK_TEST: closing socket %d.\n", y + 1);
                errorCode = uSockClose(descriptor[y]);
                U_PORT_TEST_ASSERT(errorCode == 0);
                U_PORT_TEST_ASSERT(errno == 0);
            }

            uPortLog("U_SOCK_TEST: \"test\" clean up...\n");
            uSockCleanUp();

            // Make sure that we can still open one and use it
            uPortLog("U_SOCK_TEST: check that we can still open,"
                     " use and close a socket...\n");
            descriptor[0] = openSocketAndUseIt(networkHandle,
                                               &remoteAddress,
                                               U_SOCK_TYPE_DGRAM,
                                               U_SOCK_PROTOCOL_UDP,
                                               &heapXxxSockInitLoss);
            U_PORT_TEST_ASSERT(descriptor[0] >= 0);
            U_PORT_TEST_ASSERT(errno == 0);
            uPortLog("U_SOCK_TEST: closing socket %d again.\n",
                     descriptor[0]);
            errorCode = uSockClose(descriptor[0]);
            U_PORT_TEST_ASSERT(errorCode == 0);
            U_PORT_TEST_ASSERT(errno == 0);

            uPortLog("U_SOCK_TEST: cleaning up properly...\n");
            uSockCleanUp();

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test"
                     " 0 byte(s) of heap were lost to the C library"
                     " and %d byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
        }
    }
}

/** Test socket options (actually only timeout since
 * that's all that it tested at this level).
 */
U_PORT_TEST_FUNCTION("[sock]", "sockOptionsSetGet")
{
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockDescriptor_t descriptor;
    size_t length = 0;
    size_t *pLength;
    struct timeval timeout;
    char *pData[1];
    int64_t startTimeMs;
    int64_t timeoutMs;
    int64_t elapsedMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing socket options test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for UDP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Create a UDP socket, which is sufficient
            // for the options we can test here and doesn't
            // require a potentially long uSockClose() time.
            // Creating a socket may use heap in the underlying
            // network layer which will be reclaimed when the
            // network layer is closed but we don't do that here
            // to save time so need to allow for it in the heap loss
            // calculation
            heapXxxSockInitLoss += uPortGetHeapFree();
            descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_DGRAM,
                                     U_SOCK_PROTOCOL_UDP);
            heapXxxSockInitLoss -= uPortGetHeapFree();
            U_PORT_TEST_ASSERT(descriptor >= 0);
            U_PORT_TEST_ASSERT(errno == 0);

            // Test that setting the socket receive timeout
            // option has an effect
            uPortLog("U_SOCK_TEST: check that receive timeout has an effect"
                     " (please wait for %d second(s))...\n",
                     U_SOCK_RECEIVE_TIMEOUT_DEFAULT_MS / 1000);
            length = sizeof(timeout);
            pLength = &length;
            U_PORT_TEST_ASSERT(uSockOptionGet(descriptor,
                                              U_SOCK_OPT_LEVEL_SOCK,
                                              U_SOCK_OPT_RCVTIMEO,
                                              (void *) &timeout,
                                              pLength) == 0);
            timeoutMs = ((int64_t) timeout.tv_sec) * 1000 + timeout.tv_usec / 1000;
            U_PORT_TEST_ASSERT(timeoutMs == U_SOCK_RECEIVE_TIMEOUT_DEFAULT_MS);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockReceiveFrom(descriptor, NULL,
                                                pData, sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockReceiveFrom() of nothing took"
                     " %d millisecond(s)...\n", (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;
            timeoutMs = (((int64_t) timeout.tv_sec) * 1000) +
                        (timeout.tv_usec / 1000);
            uPortLog("U_SOCK_TEST: setting timeout to %d"
                     " millisecond(s)...\n", (int32_t) timeoutMs);
            U_PORT_TEST_ASSERT(uSockOptionSet(descriptor,
                                              U_SOCK_OPT_LEVEL_SOCK,
                                              U_SOCK_OPT_RCVTIMEO,
                                              (void *) &timeout,
                                              sizeof(timeout)) == 0);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockReceiveFrom(descriptor, NULL,
                                                pData,
                                                sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockReceiveFrom() of nothing took"
                     " %d millisecond(s)...\n", (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);

            // Close the UDP socket
            U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
            uSockCleanUp();

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test"
                     " 0 byte(s) of heap were lost to the C library"
                     " and %d byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
        }
    }
}

/** Test setting non-blocking and blocking.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockNonBlocking")
{
    int32_t errorCode = -1;
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockDescriptor_t descriptor;
    bool closedCallbackCalled;
    bool isBlocking;
    struct timeval timeout;
    char *pData[1];
    int64_t startTimeMs;
    int64_t timeoutMs;
    int64_t elapsedMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing non-blocking test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for TCP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

            // Create the TCP socket
            // Creating a socket may use heap in the underlying
            // network layer which will be reclaimed when the
            // network layer is closed but we don't do that here
            // to save time so need to allow for it in the heap loss
            // calculation
            heapXxxSockInitLoss += uPortGetHeapFree();
            descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);
            heapXxxSockInitLoss -= uPortGetHeapFree();
            U_PORT_TEST_ASSERT(descriptor >= 0);
            U_PORT_TEST_ASSERT(errno == 0);

            // Set up the closed callback
            closedCallbackCalled = false;
            uSockRegisterCallbackClosed(descriptor, setBoolCallback,
                                        &closedCallbackCalled);
            U_PORT_TEST_ASSERT(!closedCallbackCalled);

            uPortLog("U_SOCK_TEST: connect socket to \"%s:%d\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                     U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
            // Connections can fail so allow this a few goes
            for (int32_t y = 2; (y > 0) && (errorCode < 0); y--) {
                errorCode = uSockConnect(descriptor, &remoteAddress);
                uPortLog("U_SOCK_TEST: uSockConnect() returned %d,"
                         " errno %d.\n", errorCode, errno);
                if (errorCode < 0) {
                    U_PORT_TEST_ASSERT(errno != 0);
                    errno = 0;
                    if (y > 1) {
                        // Give us something to search for in the log
                        uPortLog("U_SOCK_TEST: *** WARNING *** RETRY CONNECTION.\n");
                    }
                }
            }
            U_PORT_TEST_ASSERT(errorCode == 0);

            // Set a short time-out so that we're not hanging around
            // Not setting it so short, though, that the margins we
            // allow could overlap (i.e. a lot less than
            // U_SOCK_TEST_TIME_MARGIN_PLUS_MS)
            uPortLog("U_SOCK_TEST: setting a short socket timeout to save time...\n");
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            timeoutMs = ((int64_t) timeout.tv_sec) * 1000 + timeout.tv_usec / 1000;
            U_PORT_TEST_ASSERT(uSockOptionSet(descriptor,
                                              U_SOCK_OPT_LEVEL_SOCK,
                                              U_SOCK_OPT_RCVTIMEO,
                                              (void *) &timeout,
                                              sizeof(timeout)) == 0);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockReceiveFrom(descriptor, NULL,
                                                pData, sizeof(pData)) < 0);
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            uPortLog("U_SOCK_TEST: uSockReceiveFrom() of nothing"
                     " took %d millisecond(s)...\n", (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockRead(descriptor, pData,
                                         sizeof(pData)) < 0);
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            uPortLog("U_SOCK_TEST: uSockRead() of nothing took %d "
                     " millisecond(s)...\n", (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);

            uPortLog("U_SOCK_TEST: get current non-blocking state...\n");
            isBlocking = uSockBlockingGet(descriptor);
            uPortLog("U_SOCK_TEST: blocking is currently %s.\n",
                     isBlocking ? "on" : "off");
            // Should be true
            U_PORT_TEST_ASSERT(isBlocking);
            U_PORT_TEST_ASSERT(errno == 0);

            uPortLog("U_SOCK_TEST: set non-blocking...\n");
            uSockBlockingSet(descriptor, false);
            U_PORT_TEST_ASSERT(!uSockBlockingGet(descriptor));

            uPortLog("U_SOCK_TEST: check that it has worked for"
                     " receive...\n");
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockReceiveFrom(descriptor, NULL, pData,
                                                sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockReceiveFrom() of nothing with"
                     " blocking off took %d millisecond(s)...\n",
                     (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs < U_SOCK_TEST_NON_BLOCKING_TIME_MS +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockRead(descriptor, pData,
                                         sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockRead() of nothing with"
                     " blocking off took %d millisecond(s)...\n",
                     (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs < U_SOCK_TEST_NON_BLOCKING_TIME_MS +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);

            uPortLog("U_SOCK_TEST: set blocking again...\n");
            uSockBlockingSet(descriptor, true);
            U_PORT_TEST_ASSERT(uSockBlockingGet(descriptor));

            uPortLog("U_SOCK_TEST: check that we're blocking again...\n");
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockReceiveFrom(descriptor, NULL, pData,
                                                sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockReceiveFrom() of nothing with"
                     " blocking on took %d millisecond(s)...\n",
                     (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);
            startTimeMs = uPortGetTickTimeMs();
            U_PORT_TEST_ASSERT(uSockRead(descriptor, pData,
                                         sizeof(pData)) < 0);
            elapsedMs = uPortGetTickTimeMs() - startTimeMs;
            U_PORT_TEST_ASSERT(errno == U_SOCK_EWOULDBLOCK);
            errno = 0;
            uPortLog("U_SOCK_TEST: uSockRead() of nothing with"
                     " blocking on took %d millisecond(s)...\n",
                     (int32_t) elapsedMs);
            U_PORT_TEST_ASSERT(elapsedMs > timeoutMs -
                               U_SOCK_TEST_TIME_MARGIN_MINUS_MS);
            U_PORT_TEST_ASSERT(elapsedMs < timeoutMs +
                               U_SOCK_TEST_TIME_MARGIN_PLUS_MS);

            // Close the socket
            U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
            uPortLog("U_CELL_TEST: waiting up to %d second(s) for TCP"
                     " socket to close...\n",
                     U_SOCK_TEST_TCP_CLOSE_SECONDS);
            for (size_t y = 0; (y < U_SOCK_TEST_TCP_CLOSE_SECONDS) &&
                 !closedCallbackCalled; y++) {
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(closedCallbackCalled);
            uSockCleanUp();

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test"
                     " 0 byte(s) of heap were lost to the C library"
                     " and %d byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
        }
    }
}

/** UDP echo test that throws up multiple packets
 * before addressing the received packets.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockUdpEchoNonPingPong")
{
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    uSockDescriptor_t descriptor;
    bool dataCallbackCalled = false;
    bool allPacketsReceived = false;
    bool success;
    int32_t tries = 0;
    size_t sizeBytes = 0;
    size_t offset;
//lint -esym(550, y) Suppress y not accessed ('tis true
// if U_CFG_ENABLE_LOGGING is 0)
    int32_t y;
    int32_t z;
    char *pDataReceived;
    int64_t startTimeMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing UDP non-ping-pong test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for UDP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Quite often nothing at all comes back so retry this
            // if that is the case
            for (size_t retries = 2; (sizeBytes == 0) &&  (retries > 0); retries--) {
                // Create the UDP socket
                // Creating a socket may use heap in the underlying
                // network layer which will be reclaimed when the
                // network layer is closed but we don't do that here
                // to save time so need to allow for it in the heap loss
                // calculation
                heapXxxSockInitLoss += uPortGetHeapFree();
                descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_DGRAM,
                                         U_SOCK_PROTOCOL_UDP);
                heapXxxSockInitLoss -= uPortGetHeapFree();
                U_PORT_TEST_ASSERT(descriptor >= 0);
                U_PORT_TEST_ASSERT(errno == 0);

                // Set up the data callback
                dataCallbackCalled = false;
                uSockRegisterCallbackData(descriptor, setBoolCallback,
                                          &dataCallbackCalled);
                U_PORT_TEST_ASSERT(!dataCallbackCalled);

                uPortLog("U_SOCK_TEST: sending to address ");
                printAddress(&remoteAddress, true);
                uPortLog("...\n");

                do {
                    // Reset errno 'cos we might retry and subsequent
                    // things might be upset by it
                    errno = 0;
                    // Throw random sized UDP packets up...
                    offset = 0;
                    y = 0;
                    while (offset < sizeof(gSendData) - 1) {
                        sizeBytes = (rand() % U_SOCK_TEST_MAX_UDP_PACKET_SIZE) + 1;
                        sizeBytes = fix(sizeBytes, U_SOCK_TEST_MAX_UDP_PACKET_SIZE);
                        if (offset + sizeBytes > sizeof(gSendData) - 1) {
                            sizeBytes = (sizeof(gSendData) - 1) - offset;
                        }
                        success = false;
                        for (z = 0; !success &&
                             (z < U_SOCK_TEST_UDP_RETRIES); z++) {
                            uPortLog("U_SOCK_TEST: sending UDP packet number %d,"
                                     " size %d byte(s), send try %d.\n", y + 1,
                                     sizeBytes, z + 1);
                            if (uSockSendTo(descriptor, &remoteAddress,
                                            gSendData + offset, sizeBytes) == sizeBytes) {
                                success = true;
                                offset += sizeBytes;
                            } else {
                                // Reset errno 'cos we're going to retry and subsequent
                                // things might be upset by it
                                errno = 0;
                            }
                        }
                        y++;
                        U_PORT_TEST_ASSERT(success);
                    }
                    uPortLog("U_SOCK_TEST: a total of %d UDP packet(s) sent, now"
                             " receiving...\n", y + 1);

                    // ...and capture them all again afterwards
                    pDataReceived = (char *) malloc((sizeof(gSendData) - 1) +
                                                    (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
                    U_PORT_TEST_ASSERT(pDataReceived != NULL);
                    //lint -e(668) Suppress possible use of NULL pointer
                    // for pDataReceived (it is checked above)
                    memset(pDataReceived, U_SOCK_TEST_FILL_CHARACTER,
                           (sizeof(gSendData) - 1) + (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2));
                    startTimeMs = uPortGetTickTimeMs();
                    offset = 0;
                    //lint -e{441} Suppress loop variable not found in
                    // condition: we're using time instead
                    for (y = 0; (offset < sizeof(gSendData) - 1) &&
                         (uPortGetTickTimeMs() - startTimeMs < 15000); y++) {
                        z = uSockReceiveFrom(descriptor, NULL,
                                             pDataReceived + offset +
                                             U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES,
                                             (sizeof(gSendData) - 1) - offset);
                        if (z > 0) {
                            uPortLog("U_SOCK_TEST: received UDP packet number %d,"
                                     " size %d byte(s).\n", y + 1, z);
                            offset += z;
                        }
                    }
                    sizeBytes = offset;
                    uPortLog("U_SOCK_TEST: either received everything back"
                             " or timed out waiting.\n");

                    // Check that we reassembled everything correctly
                    allPacketsReceived = checkAgainstSentData(gSendData,
                                                              sizeof(gSendData) - 1,
                                                              pDataReceived,
                                                              sizeBytes);
                    free(pDataReceived);
                    tries++;
                } while (!allPacketsReceived && (tries < U_SOCK_TEST_UDP_RETRIES));

                // Close the socket
                U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
                uSockCleanUp();

                if (!allPacketsReceived) {
                    // If we're going to try again, take the
                    // network down and up again and reset errno
                    uPortLog("U_SOCK_TEST: failed to get everything, back"
                             " cycling network layer before trying again...\n");
                    // Give us something to search for in the log
                    uPortLog("U_SOCK_TEST: *** WARNING *** RETRY UDP.\n");
                    U_PORT_TEST_ASSERT(uNetworkDown(networkHandle) == 0);
                    U_PORT_TEST_ASSERT(uNetworkUp(networkHandle) == 0);
                    errno = 0;
                }
            }

            U_PORT_TEST_ASSERT(allPacketsReceived);
            if (!dataCallbackCalled) {
                // Only print a warning if the data callback wasn't
                // called: in the cellular implementation the callback
                // isn't called if the uSockReceiveFrom() or uSockRead()
                // call is active when the data arrives (to avoid recursion)
                // and this can, statistically, happen in this test since
                // it calls uSockReceiveFrom() blindly without waiting for
                // the data callback to be called.
                uPortLog("U_SOCK_TEST: *** WARNING *** the data callback"
                         " wasn't called; this might be legitimate but if"
                         " it happens frequently it is worth checking.\n");
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test %d"
                     " byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
        }
    }
}

/** UDP echo test that does asynchronous receive.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockAsyncUdpEchoMayFailDueToInternetDatagramLoss")
{
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    bool success;
    size_t sizeBytes = 0;
    size_t offset;
    int32_t y;
    int32_t stackMinFreeBytes;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing UDP asynchronous"
                     " receive test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);

            memset(&gTestConfig, 0, sizeof(gTestConfig));

            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);

            // Look up the address of the server we use for UDP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Quite often nothing at all comes back so retry this
            // if that is the case
            for (size_t retries = 2; (gTestConfig.packetsReceived == 0) &&
                 (retries > 0); retries--) {
                gTestConfig.bytesReceived = 0;
                // Create the UDP socket
                // Creating a socket may use heap in the underlying
                // network layer which will be reclaimed when the
                // network layer is closed but we don't do that here
                // to save time so need to allow for it in the heap loss
                // calculation
                heapXxxSockInitLoss += uPortGetHeapFree();
                gTestConfig.descriptor = uSockCreate(networkHandle,
                                                     U_SOCK_TYPE_DGRAM,
                                                     U_SOCK_PROTOCOL_UDP);
                heapXxxSockInitLoss -= uPortGetHeapFree();
                U_PORT_TEST_ASSERT(gTestConfig.descriptor >= 0);
                U_PORT_TEST_ASSERT(errno == 0);
                gTestConfig.isTcp = false;

                // We're sending all of gSendData except the
                // null terminator on the end
                gTestConfig.bytesToSend = sizeof(gSendData) - 1;

                // Malloc a buffer to receive UDP packets into
                // and put the fill value into it
                gTestConfig.bufferLength = gTestConfig.bytesToSend +
                                           (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2);
                gTestConfig.pBuffer = (char *) malloc(gTestConfig.bufferLength);
                U_PORT_TEST_ASSERT(gTestConfig.pBuffer != NULL);
                //lint -e(668) Suppress possible use of NULL pointer
                // for gTestConfig.pBuffer (it is checked above)
                memset(gTestConfig.pBuffer, U_SOCK_TEST_FILL_CHARACTER,
                       gTestConfig.bufferLength);

                // Create the event queue with, at the end of it,
                // a task that will handle the received UDP packets.
                // The thing it gets sent on the event queue is a pointer
                // to gTestConfig
                gTestConfig.eventQueueHandle = uPortEventQueueOpen(rxAsyncEventTask,
                                                                   "testTaskRxData",
                                                                   //lint -e(866) Suppress unusual
                                                                   // use of & in sizeof()
                                                                   sizeof(&gTestConfig),
                                                                   U_SOCK_TEST_TASK_STACK_SIZE_BYTES,
                                                                   U_SOCK_TEST_TASK_PRIORITY,
                                                                   U_SOCK_TEST_RECEIVE_QUEUE_LENGTH);
                U_PORT_TEST_ASSERT(gTestConfig.eventQueueHandle >= 0);

                // Ask the sockets API for a pointer to gTestConfig
                // to be sent to our trampoline function,
                // sendToEventQueue(), whenever UDP data arrives.
                // sendToEventQueue() will then forward the
                // pointer to the event queue and hence to
                // rxAsyncEventTask()
                uSockRegisterCallbackData(gTestConfig.descriptor,
                                          sendToEventQueue,
                                          &gTestConfig);

                // Set the port to be non-blocking; we will pick up
                // the UDP packet that we have been called-back to
                // say has arrived and then if we ask again we want
                // to know that there is nothing more to receive
                // without hanging about so that we can leave the
                // event handler toot-sweet.
                uSockBlockingSet(gTestConfig.descriptor, false);

                uPortLog("U_SOCK_TEST: sending UDP packets to echo server ");
                printAddress(&remoteAddress, true);
                uPortLog("...\n");

                // Throw random sized UDP packets up...
                offset = 0;
                y = 0;
                while (offset < gTestConfig.bytesToSend) {
                    sizeBytes = (rand() % U_SOCK_TEST_MAX_UDP_PACKET_SIZE) + 1;
                    sizeBytes = fix(sizeBytes, U_SOCK_TEST_MAX_UDP_PACKET_SIZE);
                    if (offset + sizeBytes > gTestConfig.bytesToSend) {
                        sizeBytes = gTestConfig.bytesToSend - offset;
                    }
                    success = false;
                    for (size_t z = 0; !success &&
                         (z < U_SOCK_TEST_UDP_RETRIES); z++) {
                        uPortLog("U_SOCK_TEST: sending UDP packet number %d, size"
                                 " %d byte(s), send try %d.\n", y + 1,
                                 sizeBytes, z + 1);
                        if (uSockSendTo(gTestConfig.descriptor,
                                        &remoteAddress, gSendData + offset,
                                        sizeBytes) == sizeBytes) {
                            success = true;
                            offset += sizeBytes;
                            y++;
                        } else {
                            // Reset errno 'cos we're going to retry and
                            // subsequent things might be upset by it
                            errno = 0;
                        }
                    }
                    U_PORT_TEST_ASSERT(success);
                }
                uPortLog("U_SOCK_TEST: a total of %d UDP packet(s) sent,"
                         " %d byte(s).\n", y, offset);

                // Give the data time to come back
                for (size_t z = 15; (z > 0) &&
                     (gTestConfig.bytesReceived < gTestConfig.bytesToSend); z--) {
                    uPortTaskBlock(1000);
                }

                uPortLog("U_SOCK_TEST: UDP async data task received"
                         " %d packet(s) totalling %d byte(s).\n",
                         gTestConfig.packetsReceived,
                         gTestConfig.bytesReceived);

                if (gTestConfig.packetsReceived == y) {
                    // Check that we reassembled everything
                    U_PORT_TEST_ASSERT(checkAgainstSentData(gSendData,
                                                            gTestConfig.bytesToSend,
                                                            gTestConfig.pBuffer,
                                                            gTestConfig.bytesReceived));
                } else {
                    // Only print a warning if a packet went missing
                    // as the chances of failure due to datagram
                    // loss across an RF link is too high
                    uPortLog("U_SOCK_TEST: *** WARNING *** %d UDP packet(s)"
                             " were lost.\n", y - gTestConfig.packetsReceived);
                }

                // As a sanity check, make sure that
                // U_SOCK_TEST_TASK_STACK_SIZE_BYTES
                // was big enough
                stackMinFreeBytes = uPortEventQueueStackMinFree(gTestConfig.eventQueueHandle);
                uPortLog("U_SOCK_TEST: event queue task had %d byte(s)"
                         " free at a minimum.\n", stackMinFreeBytes);
                U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);


                // Close the socket
                U_PORT_TEST_ASSERT(uSockClose(gTestConfig.descriptor) == 0);
                uSockCleanUp();

                // Close the event queue
                U_PORT_TEST_ASSERT(uPortEventQueueClose(gTestConfig.eventQueueHandle) == 0);
                gTestConfig.eventQueueHandle = -1;

                // Free memory
                free(gTestConfig.pBuffer);

                if (gTestConfig.packetsReceived == 0) {
                    // If we're going to try again, take the
                    // network down and up again and reset errno
                    uPortLog("U_SOCK_TEST: nothing came back, cycling"
                             " network layer before trying again...\n");
                    // Give us something to search for in the log
                    uPortLog("U_SOCK_TEST: *** WARNING *** RETRY UDP.\n");
                    U_PORT_TEST_ASSERT(uNetworkDown(networkHandle) == 0);
                    U_PORT_TEST_ASSERT(uNetworkUp(networkHandle) == 0);
                    errno = 0;
                }
            }

            U_PORT_TEST_ASSERT(gTestConfig.packetsReceived > 0);

#if !U_CFG_OS_CLIB_LEAKS
            // Check for memory leaks but only
            // if we don't have a leaky C library:
            // if we do there's no telling what
            // it might have left hanging after
            // the creation and deletion of the
            // tasks above.
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test %d"
                     " byte(s) of heap were lost to the C library"
                     " and %d byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
#else
            (void) heapUsed;
            (void) heapSockInitLoss;
            (void) heapXxxSockInitLoss;
#endif
        }
    }
}

/** TCP async echo test
 */
U_PORT_TEST_FUNCTION("[sock]", "sockAsyncTcpEcho")
{
    int32_t networkHandle;
    uSockAddress_t remoteAddress;
    bool closedCallbackCalled;
    size_t sizeBytes = 0;
    size_t offset;
    int32_t y;
    int32_t z;
    int32_t stackMinFreeBytes;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Call clean up to release OS resources that may
    // have been left hanging by a previous failed test
    osCleanup();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if ((networkHandle >= 0) &&
            U_NETWORK_TEST_TYPE_HAS_SOCK(gUNetworkTestCfg[x].type)) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SOCK_TEST: doing TCP asynchronous"
                     " receive test on %s.\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);

            memset(&gTestConfig, 0, sizeof(gTestConfig));

            uPortLog("U_SOCK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME);

            // Look up the address of the server we use for TCP echo
            // The first call to a sockets API needs to
            // initialise the underlying sockets layer; take
            // account of that initialisation heap cost here.
            heapSockInitLoss = uPortGetHeapFree();
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                  &(remoteAddress.ipAddress)) == 0);
            heapSockInitLoss -= uPortGetHeapFree();

            // Add the port number we will use
            remoteAddress.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

            // Create the TCP socket
            // Creating a socket may use heap in the underlying
            // network layer which will be reclaimed when the
            // network layer is closed but we don't do that here
            // to save time so need to allow for it in the heap loss
            // calculation
            heapXxxSockInitLoss += uPortGetHeapFree();
            gTestConfig.descriptor = uSockCreate(networkHandle,
                                                 U_SOCK_TYPE_STREAM,
                                                 U_SOCK_PROTOCOL_TCP);
            heapXxxSockInitLoss -= uPortGetHeapFree();
            U_PORT_TEST_ASSERT(gTestConfig.descriptor >= 0);
            U_PORT_TEST_ASSERT(errno == 0);
            gTestConfig.isTcp = true;

            // We're sending all of gSendData except the
            // null terminator on the end
            gTestConfig.bytesToSend = sizeof(gSendData) - 1;

            // Malloc a buffer to receive TCP packets into
            // and put the fill value into it
            gTestConfig.bufferLength = gTestConfig.bytesToSend +
                                       (U_SOCK_TEST_GUARD_LENGTH_SIZE_BYTES * 2);
            gTestConfig.pBuffer = (char *) malloc(gTestConfig.bufferLength);
            U_PORT_TEST_ASSERT(gTestConfig.pBuffer != NULL);
            //lint -e(668) Suppress possible use of NULL pointer
            // for gTestConfig.pBuffer (it is checked above)
            memset(gTestConfig.pBuffer, U_SOCK_TEST_FILL_CHARACTER,
                   gTestConfig.bufferLength);

            // Create the event queue with, at the end of it,
            // a task that will handle the received TCP packets.
            // The thing it gets sent on the event queue is a pointer
            // to gTestConfig
            gTestConfig.eventQueueHandle = uPortEventQueueOpen(rxAsyncEventTask,
                                                               "testTaskRxData",
                                                               //lint -e(866) Suppress unusual
                                                               // use of & in sizeof()
                                                               sizeof(&gTestConfig),
                                                               U_SOCK_TEST_TASK_STACK_SIZE_BYTES,
                                                               U_SOCK_TEST_TASK_PRIORITY,
                                                               U_SOCK_TEST_RECEIVE_QUEUE_LENGTH);
            U_PORT_TEST_ASSERT(gTestConfig.eventQueueHandle >= 0);

            // Ask the sockets API for a pointer to gTestConfig
            // to be sent to our trampoline function,
            // sendToEventQueue(), whenever UDP data arrives.
            // sendToEventQueue() will then forward the
            // pointer to the event queue and hence to
            // rxAsyncEventTask()
            uSockRegisterCallbackData(gTestConfig.descriptor,
                                      sendToEventQueue,
                                      &gTestConfig);

            // Set the port to be non-blocking; we will pick up
            // the TCP data that we have been called-back to
            // say has arrived and then if we ask again we want
            // to know that there is nothing more to receive
            // without hanging about so that we can leave the
            // event handler toot-sweet.
            uSockBlockingSet(gTestConfig.descriptor, false);

            // Set up the closed callback
            closedCallbackCalled = false;
            uSockRegisterCallbackClosed(gTestConfig.descriptor,
                                        setBoolCallback,
                                        &closedCallbackCalled);
            U_PORT_TEST_ASSERT(!closedCallbackCalled);

            // Connect the socket
            uPortLog("U_SOCK_TEST: connect socket to \"%s:%d\"...\n",
                     U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                     U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
            // Connections can fail so allow this a few goes
            z = -1;
            for (y = 2; (y > 0) && (z < 0); y--) {
                z = uSockConnect(gTestConfig.descriptor,
                                 &remoteAddress);
                if (z < 0) {
                    U_PORT_TEST_ASSERT(errno != 0);
                    errno = 0;
                    if (y > 1) {
                        // Give us something to search for in the log
                        uPortLog("U_SOCK_TEST: *** WARNING *** RETRY CONNECTION.\n");
                    }
                }
            }
            U_PORT_TEST_ASSERT(z == 0);

            uPortLog("U_SOCK_TEST: sending TCP data to echo server ");
            printAddress(&remoteAddress, true);
            uPortLog("...\n");

            // Throw random sized segments up...
            offset = 0;
            y = 0;
            while (offset < gTestConfig.bytesToSend) {
                sizeBytes = (rand() % U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE) + 1;
                sizeBytes = fix(sizeBytes, U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE);
                if (sizeBytes < U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE) {
                    sizeBytes = U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE;
                }
                if (offset + sizeBytes > gTestConfig.bytesToSend) {
                    sizeBytes = gTestConfig.bytesToSend - offset;
                }
                uPortLog("U_SOCK_TEST: write number %d.\n", y + 1);
                U_PORT_TEST_ASSERT(sendTcp(gTestConfig.descriptor,
                                           gSendData + offset,
                                           sizeBytes) == sizeBytes);
                offset += sizeBytes;
                y++;
            }
            uPortLog("U_SOCK_TEST: a total of %d byte(s) sent in"
                     " %d write(s).\n", offset, y);

            // Give the data time to come back
            for (z = 10; (z > 0) &&
                 (gTestConfig.bytesReceived < gTestConfig.bytesToSend); z--) {
                uPortTaskBlock(1000);
            }

            uPortLog("U_SOCK_TEST: TCP async data task received"
                     " %d segment(s) totalling %d byte(s).\n",
                     gTestConfig.packetsReceived,
                     gTestConfig.bytesReceived);

            // Check that we reassembled everything correctly
            U_PORT_TEST_ASSERT(checkAgainstSentData(gSendData,
                                                    gTestConfig.bytesToSend,
                                                    gTestConfig.pBuffer,
                                                    gTestConfig.bytesReceived));

            // As a sanity check, make sure that
            // U_SOCK_TEST_TASK_STACK_SIZE_BYTES
            // was big enough
            stackMinFreeBytes = uPortEventQueueStackMinFree(gTestConfig.eventQueueHandle);
            uPortLog("U_SOCK_TEST: event queue task had %d byte(s)"
                     " free at a minimum.\n", stackMinFreeBytes);
            U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);


            // Close the socket
            U_PORT_TEST_ASSERT(!closedCallbackCalled);
            U_PORT_TEST_ASSERT(uSockClose(gTestConfig.descriptor) == 0);
            uPortLog("U_CELL_TEST: waiting up to %d second(s) for TCP"
                     " socket to close...\n",
                     U_SOCK_TEST_TCP_CLOSE_SECONDS);
            for (y = 0; (y < U_SOCK_TEST_TCP_CLOSE_SECONDS) &&
                 !closedCallbackCalled; y++) {
                uPortTaskBlock(1000);
            }
            U_PORT_TEST_ASSERT(closedCallbackCalled);
            uSockCleanUp();

            // Close the event queue
            U_PORT_TEST_ASSERT(uPortEventQueueClose(gTestConfig.eventQueueHandle) == 0);
            gTestConfig.eventQueueHandle = -1;

            // Free memory
            free(gTestConfig.pBuffer);

#if !U_CFG_OS_CLIB_LEAKS
            // Check for memory leaks but only
            // if we don't have a leaky C library:
            // if we do there's no telling what
            // it might have left hanging after
            // the creation and deletion of the
            // tasks above.
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SOCK_TEST: during this part of the test %d"
                     " byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
#else
            (void) heapUsed;
            (void) heapSockInitLoss;
            (void) heapXxxSockInitLoss;
#endif
        }
    }
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[sock]", "sockCleanUp")
{
    int32_t y;

    osCleanup();

    // The network test configuration is shared
    // between the network and sockets tests so
    // must reset the handles here in case the
    // network API tests are coming next.
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    uPortLog("U_SOCK_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", y);
    U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_SOCK_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
