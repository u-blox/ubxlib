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
 * @brief Tests for the cellular sockets API: these should pass on all
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

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

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
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_sock.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

#include "u_sock_test_shared_cfg.h"   // For some of the test macros

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// Definition of a supported socket option.
typedef struct {
    uint32_t excludeModulesBitmap;
    int32_t level;
    uint32_t option;
    size_t length;
    bool (*pComparer) (const void *, const void *);
    void (*pChanger) (void *);
} uCellSockTestOption_t;

/* ----------------------------------------------------------------
 * VARIABLES: MISC
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Generic handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

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

/** Flag to indicate that the async closed
 * callback has been called.
 */
static volatile bool gAsyncClosedCallbackCalled = false;

/** A string of all possible characters, including strings
 * that might appear as terminators in the AT interface.
 */
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n \r\nABORTED\r\n";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SOCKET OPTIONS RELATED
 * These functions are not marked as static as they are only used
 * as callbacks in a table: the compiler can't tell they are
 * are being used and complains if they are marked as static.
 * -------------------------------------------------------------- */

// Compare two int32_t values.
bool compareInt32(const void *p1, const void *p2)
{
    return *(const int32_t *) p1 == *(const int32_t *) p2;
}

// Change an int32_t value.
void changeInt32(void *p)
{
    (*((int32_t *) p))++;
}

// Change an int32_t keeping it positive.
void changeInt32Positive(void *p)
{
    // Add 1000 because the SARA-R5 keep idle socket option
    // is in increments of 1000 only.
    (*((int32_t *) p)) += 1000;
    if (*(int32_t *) p < 0) {
        *(int32_t *) p = 0;
    }
}

// Change value modulo 256.
void changeMod256(void *p)
{
    *(int32_t *) p = (*((int32_t *) p) + 1) % 256;
}

// Change value modulo 256 and non-zero.
void changeMod256NonZero(void *p)
{
    *(int32_t *) p = (*((int32_t *) p) + 1) % 256;
    if (*(int32_t *) p == 0) {
        *(int32_t *) p = 1;
    }
}

// Change a value modulo 2.
void changeMod2(void *p)
{
    *(int32_t *) p = (*((int32_t *) p) + 1) % 2;
}

// Compare two uSockLinger_t values.
bool compareLinger(const void *p1, const void *p2)
{
    bool result = false;

    result = (((const uSockLinger_t *) p1)->onNotOff == ((const uSockLinger_t *) p2)->onNotOff);
    if (((const uSockLinger_t *) p1)->onNotOff || ((const uSockLinger_t *) p2)->onNotOff) {
        result = (((const uSockLinger_t *) p1)->lingerSeconds == ((const uSockLinger_t *)
                                                                  p2)->lingerSeconds);
    }

    return result;
}

// Increment the contents of a uSockLinger_t value.
// Note: changes both the on/off and the value
void changeLinger(void *p)
{
    // If linger is not on the linger value will not be filled
    // in so set it to something sensible
    if (((uSockLinger_t *) p)->onNotOff == 0) {
        ((uSockLinger_t *) p)->lingerSeconds = 0;
    }

    ((uSockLinger_t *) p)->onNotOff = (((uSockLinger_t *) p)->onNotOff + 1) % 2;
    ((uSockLinger_t *) p)->lingerSeconds = (((uSockLinger_t *) p)->lingerSeconds + 1) % 32768;
}

/* ----------------------------------------------------------------
 * MORE VARIABLES: SUPPORTED SOCKET OPTIONS
 * -------------------------------------------------------------- */

// Table of supported socket options.
static uCellSockTestOption_t gSupportedOptions[] = {
    {
        (1UL << U_CELL_MODULE_TYPE_SARA_R422), /* Not SARA-R422 */
        U_SOCK_OPT_LEVEL_SOCK, U_SOCK_OPT_REUSEADDR, sizeof(int32_t), compareInt32, changeMod2
    },
    {
        0, /* All modules */
        U_SOCK_OPT_LEVEL_SOCK, U_SOCK_OPT_KEEPALIVE, sizeof(int32_t), compareInt32, changeMod2
    },
    {
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_02B) |  /* Not SARA-R4 */
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_02B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R422),
        U_SOCK_OPT_LEVEL_SOCK, U_SOCK_OPT_BROADCAST, sizeof(int32_t), compareInt32, changeMod2
    },
    {
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_02B) |  /* Not SARA-R4 */
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_02B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R422),
        U_SOCK_OPT_LEVEL_SOCK, U_SOCK_OPT_REUSEPORT, sizeof(int32_t), compareInt32, changeMod2
    },
    // This next one removed for SARA-R4, SARA-R5 and SARA-U201 as none will let me switch linger off, i.e.
    // "AT+USOSO=0,65535,128,0" returns "+CME ERROR: Operation not permitted/allowed"
    {
        (1UL << U_CELL_MODULE_TYPE_SARA_U201)      | /* Not SARA_U201 or SARA-R4 or SARA-R5 */
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_02B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_02B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R422)      |
        (1UL << U_CELL_MODULE_TYPE_SARA_R5),
        U_SOCK_OPT_LEVEL_SOCK, U_SOCK_OPT_LINGER, sizeof(uSockLinger_t), compareLinger, changeLinger
    },
    {
        0, /* All modules */
        U_SOCK_OPT_LEVEL_IP, U_SOCK_OPT_IP_TOS, sizeof(int32_t), compareInt32, changeMod256
    },
    {
        0, /* All modules */
        U_SOCK_OPT_LEVEL_IP, U_SOCK_OPT_IP_TTL, sizeof(int32_t), compareInt32, changeMod256NonZero
    },
    {
        0, /* All modules */
        U_SOCK_OPT_LEVEL_TCP, U_SOCK_OPT_TCP_NODELAY, sizeof(int32_t), compareInt32, changeMod2
    },
    {
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_02B) | /* Not SARA-R4 */
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_02B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R412M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R410M_03B) |
        (1UL << U_CELL_MODULE_TYPE_SARA_R422),
        U_SOCK_OPT_LEVEL_TCP, U_SOCK_OPT_TCP_KEEPIDLE, sizeof(int32_t), compareInt32, changeInt32Positive
    },
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SETTING AND GETTING SOCKET OPTIONS
 * -------------------------------------------------------------- */

// Check getting an option.
static void checkOptionGet(uDeviceHandle_t cellHandle, int32_t sockHandle,
                           int32_t level, uint32_t option,
                           void *pValue, size_t valueLength,
                           bool (*pComparer) (const void *,
                                              const void *))
{
    int32_t errorCode;
    void *pValueAgain;
    size_t length = 0xFFFFFFFF;
    size_t *pLength = &length;

    // Malloc memory for testing that values are consistent
    pValueAgain = malloc(valueLength);
    U_PORT_TEST_ASSERT(pValueAgain != NULL);

    uPortLog("U_CELL_SOCK_TEST: testing uCellSockOptionGet()"
             " with level %d, option 0x%04x (%d):\n", level,
             option, option);
    memset(pValue, 0xFF, valueLength);
    errorCode = uCellSockOptionGet(cellHandle, sockHandle,
                                   level, option, NULL,
                                   pLength);
    uPortLog("U_CELL_SOCK_TEST: ...with NULL value pointer,"
             " error code %d, length %d.\n", errorCode, *pLength);
    U_PORT_TEST_ASSERT(errorCode >= 0);
    U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, sockHandle) >= 0);
    U_PORT_TEST_ASSERT(*pLength == valueLength);
    errorCode = uCellSockOptionGet(cellHandle, sockHandle,
                                   level, option,
                                   (void *) pValue, pLength);
    uPortLog("U_CELL_SOCK_TEST: ...with non-NULL value"
             " pointer, error code %d, length %d.\n",
             errorCode, *pLength);
    U_PORT_TEST_ASSERT(errorCode >= 0);
    U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, sockHandle) >= 0);
    U_PORT_TEST_ASSERT(*pLength == valueLength);
    (*pLength)++;
    memset(pValueAgain, 0xFF, valueLength);
    errorCode = uCellSockOptionGet(cellHandle, sockHandle,
                                   level, option,
                                   (void *) pValueAgain,
                                   pLength);
    uPortLog("U_CELL_SOCK_TEST: with excess length, error"
             " code %d, length %d.\n", errorCode, *pLength);
    U_PORT_TEST_ASSERT(errorCode >= 0);
    U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, sockHandle) >= 0);
    U_PORT_TEST_ASSERT(pComparer(pValue, pValueAgain));
    U_PORT_TEST_ASSERT(*pLength == valueLength);

    // Free memory again
    free(pValueAgain);
}

// Check setting an option.
static void checkOptionSet(uDeviceHandle_t cellHandle, int32_t sockHandle,
                           int32_t level, int32_t option,
                           const void *pValue, size_t valueLength,
                           bool (*pComparer) (const void *,
                                              const void *))
{
    int32_t errorCode;
    char *pValueRead;
    size_t length = 0xFFFFFFFF;
    size_t *pLength = &length;

    // Malloc memory for testing that value has been set
    pValueRead = (char *) malloc(valueLength);
    U_PORT_TEST_ASSERT(pValueRead != NULL);

    uPortLog("U_CELL_SOCK_TEST: testing uCellSockOptionSet()"
             " with level %d, option 0x%04x (%d):\n", level,
             option, option);
    errorCode = uCellSockOptionSet(cellHandle, sockHandle,
                                   level, option, pValue,
                                   valueLength);
    uPortLog("U_CELL_SOCK_TEST: ...returned error code %d.\n",
             errorCode);
    U_PORT_TEST_ASSERT(errorCode >= 0);
    U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, sockHandle) >= 0);

    if (pComparer != NULL) {
        memset(pValueRead, 0xFF, valueLength);
        errorCode = uCellSockOptionGet(cellHandle, sockHandle,
                                       level, option,
                                       pValueRead, pLength);
        uPortLog("U_CELL_SOCK_TEST: ...reading it back returned"
                 " error code %d, length %d.\n", errorCode,
                 *pLength);
        U_PORT_TEST_ASSERT(errorCode >= 0);
        U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, sockHandle) >= 0);
        U_PORT_TEST_ASSERT(*pLength == valueLength);
        if (pComparer(pValue, pValueRead)) {
            uPortLog("U_CELL_SOCK_TEST: ...and the same value.\n");
        } else {
            uPortLog("U_CELL_SOCK_TEST: ...but a different value.\n");
            U_PORT_TEST_ASSERT(false);
        }
    }

    // Free memory again
    free(pValueRead);
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CALLBACKS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback for data being available, UDP.
static void dataCallbackUdp(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 1;
    } else if (sockHandle != gSockHandleUdp)  {
        gCallbackErrorNum = 2;
    }

    gDataCallbackCalledUdp = true;
}

// Callback for data being available, TCP.
static void dataCallbackTcp(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 3;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 4;
    }
    gDataCallbackCalledTcp = true;
}

// Callback for socket closed, UDP.
static void closedCallbackUdp(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 5;
    } else if (sockHandle != gSockHandleUdp)  {
        gCallbackErrorNum = 6;
    }

    gClosedCallbackCalledUdp = true;
}

// Callback for socket closed, TCP.
static void closedCallbackTcp(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 7;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 8;
    }

    gClosedCallbackCalledTcp = true;
}

// Callback for async socket closed.
static void asyncClosedCallback(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 9;
    } else if (sockHandle != gSockHandleTcp)  {
        gCallbackErrorNum = 10;
    }

    gAsyncClosedCallbackCalled = true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** A basic test of the cellular sockets API. This test merely
 * serves as a basic test of the uCellSockXxx functions to ensure
 * that they can be run independently of the the u_sock and u_network
 * APIs.  More comprehensive testing of this API is carred out
 * via the tests under the u_sock API.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellSock]", "cellSockBasic")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    uSockAddress_t echoServerAddressUdp;
    uSockAddress_t echoServerAddressTcp;
    uSockAddress_t address;
    int32_t y;
    int32_t w;
    int32_t z;
    size_t count;
    char *pBuffer;
    int32_t heapUsed;

    // In case a previous test failed
    uCellSockDeinit();
    uCellTestPrivateCleanup(&gHandles);

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // If we memset these here we can do memcmp's afterwards
    // 'cos we don't have to worry about the bits in the packing
    memset(&echoServerAddressUdp, 0, sizeof(echoServerAddressUdp));
    memset(&echoServerAddressTcp, 0, sizeof(echoServerAddressTcp));
    memset(&address, 0, sizeof(address));

    // Malloc a buffer to receive things into.
    pBuffer = (char *) malloc(U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Connect to the network
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
    y = uCellNetConnect(cellHandle, NULL,
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

    // Get the current value of the data counters, if supported
    y = uCellNetGetDataCounterTx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        U_PORT_TEST_ASSERT(y >= 0);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }
    y = uCellNetGetDataCounterRx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        U_PORT_TEST_ASSERT(y >= 0);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }

    // Init cell sockets
    U_PORT_TEST_ASSERT(uCellSockInit() == 0);
    U_PORT_TEST_ASSERT(uCellSockInitInstance(cellHandle) == 0);

    // Look up the address of the server we use for UDP echo
    U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                              U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                              &(echoServerAddressUdp.ipAddress)) == 0);
    // Add the port number we will use
    echoServerAddressUdp.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

    // Look up the address of the server we use for TCP echo
    U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                              U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                              &(echoServerAddressTcp.ipAddress)) == 0);
    // Add the port number we will use
    echoServerAddressTcp.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

    // Create a UDP socket
    gSockHandleUdp = uCellSockCreate(cellHandle, U_SOCK_TYPE_DGRAM,
                                     U_SOCK_PROTOCOL_UDP);

    // Create a TCP socket
    gSockHandleTcp = uCellSockCreate(cellHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);

    // Add callbacks
    uCellSockRegisterCallbackData(cellHandle, gSockHandleUdp,
                                  dataCallbackUdp);
    uCellSockRegisterCallbackClosed(cellHandle, gSockHandleUdp,
                                    closedCallbackUdp);
    uCellSockRegisterCallbackData(cellHandle, gSockHandleTcp,
                                  dataCallbackTcp);
    uCellSockRegisterCallbackClosed(cellHandle, gSockHandleTcp,
                                    closedCallbackTcp);

    // Set blocking on both: should always be false whatever we do
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleTcp));
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleUdp));
    uCellSockBlockingSet(cellHandle, gSockHandleUdp, false);
    uCellSockBlockingSet(cellHandle, gSockHandleTcp, false);
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleTcp));
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleUdp));
    uCellSockBlockingSet(cellHandle, gSockHandleUdp, true);
    uCellSockBlockingSet(cellHandle, gSockHandleTcp, true);
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleTcp));
    U_PORT_TEST_ASSERT(!uCellSockBlockingGet(cellHandle, gSockHandleUdp));

    // Connect the TCP socket
    U_PORT_TEST_ASSERT(uCellSockConnect(cellHandle, gSockHandleTcp,
                                        &echoServerAddressTcp) == 0);

    // No data should have yet flowed
    U_PORT_TEST_ASSERT(!gDataCallbackCalledUdp);
    U_PORT_TEST_ASSERT(!gDataCallbackCalledTcp);

    // Do this twice: once with binary mode and once with hex mode
    for (size_t a = 0; a < 2; a++) {
        gDataCallbackCalledUdp = false;
        if (a == 0) {
            U_PORT_TEST_ASSERT(!uCellSockHexModeIsOn(cellHandle));
        } else {
            U_PORT_TEST_ASSERT(uCellSockHexModeOn(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellSockHexModeIsOn(cellHandle));
        }
        // Send and wait for the UDP echo data, trying a few
        // times to reduce the chance of internet loss getting
        // in the way
        uPortLog("U_CELL_SOCK_TEST: sending %d byte(s) to %s:%d...\n",
                 sizeof(gAllChars),
                 U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                 U_SOCK_TEST_ECHO_UDP_SERVER_PORT);
        y = 0;
        memset(pBuffer, 0, U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES);
        for (size_t x = 0; (x < U_SOCK_TEST_UDP_RETRIES) &&
             (y != sizeof(gAllChars)); x ++) {
            y = uCellSockSendTo(cellHandle, gSockHandleUdp,
                                &echoServerAddressUdp,
                                gAllChars, sizeof(gAllChars));
            if (y == sizeof(gAllChars)) {
                // Wait a little while to get a data callback
                // triggered by a URC
                for (size_t a = 10; (a > 0) && !gDataCallbackCalledUdp; a--) {
                    uPortTaskBlock(1000);
                }
                y = 0;
                for (size_t z = 10; (z > 0) &&
                     (y != sizeof(gAllChars)); z--) {
                    y = uCellSockReceiveFrom(cellHandle,
                                             gSockHandleUdp,
                                             &address, pBuffer,
                                             U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES);
                    if (y <= 0) {
                        uPortTaskBlock(500);
                    }
                }
                if (y != sizeof(gAllChars)) {
                    uPortLog("U_CELL_SOCK_TEST: failed to receive UDP echo"
                             " on try %d.\n", x + 1);
                }
            } else {
                uPortLog("U_CELL_SOCK_TEST: failed to send UDP data on"
                         " try %d.\n", x + 1);
                U_PORT_TEST_ASSERT(uCellSockGetLastError(cellHandle, gSockHandleUdp) > 0);
            }
        }
        uPortLog("U_CELL_SOCK_TEST: %d byte(s) echoed over UDP.\n", y);
        U_PORT_TEST_ASSERT(y == sizeof(gAllChars));
        if (!gDataCallbackCalledUdp) {
            uPortLog("U_CELL_SOCK_TEST: *** WARNING *** the data callback"
                     " was not called during the test.  This can happen"
                     " legimitately if all the reads from the module"
                     " happened to coincide with data receptions and so"
                     " the URC was not involved.  However if it happens"
                     " too often something may be wrong.\n");
        }
        U_PORT_TEST_ASSERT(gCallbackErrorNum == 0);
        U_PORT_TEST_ASSERT(memcmp(pBuffer, gAllChars, sizeof(gAllChars)) == 0);
        U_PORT_TEST_ASSERT(memcmp(&(address.ipAddress),
                                  &(echoServerAddressUdp.ipAddress),
                                  sizeof(address.ipAddress)) == 0);
        U_PORT_TEST_ASSERT(address.port == echoServerAddressUdp.port);
        U_PORT_TEST_ASSERT(!gClosedCallbackCalledUdp);
    }

    // Hex mode off again
    U_PORT_TEST_ASSERT(uCellSockHexModeOff(cellHandle) == 0);
    U_PORT_TEST_ASSERT(!uCellSockHexModeIsOn(cellHandle));

    // Do this twice: once with binary mode and once with hex mode
    for (size_t a = 0; a < 2; a++) {
        gDataCallbackCalledTcp = false;
        if (a == 0) {
            U_PORT_TEST_ASSERT(!uCellSockHexModeIsOn(cellHandle));
        } else {
            U_PORT_TEST_ASSERT(uCellSockHexModeOn(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellSockHexModeIsOn(cellHandle));
        }
        // Send the TCP echo data in random sized chunks
        uPortLog("U_CELL_SOCK_TEST: sending %d byte(s) to %s:%d in"
                 " random sized chunks...\n", sizeof(gAllChars),
                 U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                 U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
        y = 0;
        count = 0;
        while ((y < sizeof(gAllChars)) && (count < 100)) {
            if ((sizeof(gAllChars) - y) > 1) {
                w = rand() % (sizeof(gAllChars) - y);
            } else {
                w = 1;
            }
            if (w > 0) {
                count++;
            }
            z = uCellSockWrite(cellHandle, gSockHandleTcp,
                               gAllChars + y, w);
            if (z > 0) {
                y += z;
            } else {
                uPortTaskBlock(500);
            }
        }
        uPortLog("U_CELL_SOCK_TEST: %d byte(s) sent in %d chunks.\n",
                 y, count);

        // Wait a little while to get a data callback
        // triggered by a URC
        for (size_t x = 10; (x > 0) && !gDataCallbackCalledTcp; x--) {
            uPortTaskBlock(1000);
        }

        // Get the data back again
        uPortLog("U_CELL_SOCK_TEST: receiving TCP echo data back"
                 " in random sized chunks...\n");
        y = 0;
        count = 0;
        memset(pBuffer, 0, U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES);
        while ((y < sizeof(gAllChars)) && (count < 100)) {
            if ((sizeof(gAllChars) - y) > 1) {
                w = rand() % (sizeof(gAllChars) - y);
            } else {
                w = 1;
            }
            if (w > 0) {
                count++;
            }
            z = uCellSockRead(cellHandle, gSockHandleTcp,
                              pBuffer + y, w);
            if (z > 0) {
                y += z;
            } else {
                uPortTaskBlock(500);
            }
        }
        uPortLog("U_CELL_SOCK_TEST: %d byte(s) echoed over TCP, received"
                 " in %d receive call(s).\n", y, count);
        if (!gDataCallbackCalledTcp) {
            uPortLog("U_CELL_SOCK_TEST: *** WARNING *** the data callback"
                     " was not called during the test.  This can happen"
                     " legimitately if all the reads from the module"
                     " happened to coincide with data receptions and so"
                     " the URC was not involved.  However if it happens"
                     " too often something may be wrong.\n");
        }
        U_PORT_TEST_ASSERT(gCallbackErrorNum == 0);
        // Compare the data
        U_PORT_TEST_ASSERT(memcmp(pBuffer, gAllChars,
                                  sizeof(gAllChars)) == 0);
        U_PORT_TEST_ASSERT(!gClosedCallbackCalledTcp);
    }

    // Sockets should both still be open
    U_PORT_TEST_ASSERT(!gClosedCallbackCalledUdp);
    U_PORT_TEST_ASSERT(!gClosedCallbackCalledTcp);
    U_PORT_TEST_ASSERT(!gAsyncClosedCallbackCalled);

    // Get the local address of the TCP socket,
    // though there's not much we can do to check it.
    U_PORT_TEST_ASSERT(uCellSockGetLocalAddress(cellHandle,
                                                gSockHandleTcp,
                                                &address) == 0);

    // Check that the byte counts have incremented
    // Note: not checking exact values as there may have been retries
    U_PORT_TEST_ASSERT(uCellSockGetBytesSent(cellHandle, gSockHandleUdp) > 0);
    U_PORT_TEST_ASSERT(uCellSockGetBytesReceived(cellHandle, gSockHandleUdp) > 0);
    U_PORT_TEST_ASSERT(uCellSockGetBytesSent(cellHandle, gSockHandleTcp) > 0);
    U_PORT_TEST_ASSERT(uCellSockGetBytesReceived(cellHandle, gSockHandleTcp) > 0);

    // Close TCP socket with asynchronous callback
    uPortLog("U_CELL_SOCK_TEST: closing sockets...\n");
    U_PORT_TEST_ASSERT(uCellSockClose(cellHandle, gSockHandleTcp,
                                      asyncClosedCallback) == 0);
    U_PORT_TEST_ASSERT(!gClosedCallbackCalledUdp);
    // Close the UDP socket
    U_PORT_TEST_ASSERT(uCellSockClose(cellHandle, gSockHandleUdp,
                                      NULL) == 0);
    // Allow a task switch to let the close callback be called
    uPortTaskBlock(U_CFG_OS_YIELD_MS);
    U_PORT_TEST_ASSERT(gClosedCallbackCalledUdp);
    uPortLog("U_CELL_SOCK_TEST: waiting up to %d second(s) for TCP"
             " socket to close...\n",
             U_SOCK_TEST_TCP_CLOSE_SECONDS);
    for (size_t x = 0; (x < U_SOCK_TEST_TCP_CLOSE_SECONDS) &&
         !gClosedCallbackCalledTcp; x++) {
        uPortTaskBlock(1000);
    }
    U_PORT_TEST_ASSERT(gClosedCallbackCalledTcp);
    U_PORT_TEST_ASSERT(gCallbackErrorNum == 0);

    // Deinit cell sockets
    uCellSockDeinit();

    // Get the new value of the data counters, if supported
    y = uCellNetGetDataCounterTx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        uPortLog("U_CELL_SOCK_TEST: %d byte(s) sent.\n", y);
        U_PORT_TEST_ASSERT(y > 0);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }
    y = uCellNetGetDataCounterRx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        uPortLog("U_CELL_SOCK_TEST: %d byte(s) received.\n", y);
        U_PORT_TEST_ASSERT(y > 0);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }

    // Reset the data counters and check that they were reset
    y = uCellNetResetDataCounters(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        U_PORT_TEST_ASSERT(y == 0);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }
    y = uCellNetGetDataCounterTx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        // Note that we don't check for zero here: the closure of sockets
        // is not necessarily synchronous with closure indication at the AT
        // interface and so sometimes 52 bytes will be logged here
        U_PORT_TEST_ASSERT(y <= 52);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }
    y = uCellNetGetDataCounterRx(cellHandle);
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
        // Note that we don't check for zero here: the closure of sockets
        // is not necessarily synchronous with closure indication at the AT
        // interface and so sometimes 52 bytes will be logged here
        U_PORT_TEST_ASSERT(y <= 52);
    } else {
        U_PORT_TEST_ASSERT(y < 0);
    }

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Free memory
    free(pBuffer);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_SOCK_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test setting/getting socket options.
 * TODO: error cases.
 */
U_PORT_TEST_FUNCTION("[cellSock]", "cellSockOptionSetGet")
{
    uDeviceHandle_t cellHandle;
    void *pValue;
    void *pValueSaved;
    size_t length = 0;
    int32_t heapUsed;
    int32_t y;

    // In case a previous test failed
    uCellSockDeinit();
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Connect to the network
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
    y = uCellNetConnect(cellHandle, NULL,
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

    // Init cell sockets
    U_PORT_TEST_ASSERT(uCellSockInit() == 0);
    U_PORT_TEST_ASSERT(uCellSockInitInstance(cellHandle) == 0);

    // Create a TCP socket: needs to be TCP as some
    // options only apply to TCP. We don't actually
    // connect the socket or send any data during
    // this test though.
    gSockHandleTcp = uCellSockCreate(cellHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);

    // Add callback
    gClosedCallbackCalledTcp = false;
    uCellSockRegisterCallbackClosed(cellHandle, gSockHandleTcp,
                                    closedCallbackTcp);
    // Determine the maximum size of storage we need for all supported options
    for (size_t x = 0; x < sizeof(gSupportedOptions) /
         sizeof(gSupportedOptions[0]); x++) {
        if (((gSupportedOptions[x].excludeModulesBitmap &
              (1UL << U_CFG_TEST_CELL_MODULE_TYPE)) == 0) &&
            (gSupportedOptions[x].length > length)) {
            length = gSupportedOptions[x].length;
        }
    }

    // Malloc memory for our testing
    pValue = malloc(length);
    U_PORT_TEST_ASSERT(pValue != NULL);
    pValueSaved = malloc(length);
    U_PORT_TEST_ASSERT(pValueSaved != NULL);

    // Now test all supported options
    for (size_t x = 0; x < sizeof(gSupportedOptions) /
         sizeof(gSupportedOptions[0]); x++) {
        if ((gSupportedOptions[x].excludeModulesBitmap &
             (1UL << U_CFG_TEST_CELL_MODULE_TYPE)) == 0) {
            // Check that we can get the option value
            checkOptionGet(cellHandle, gSockHandleTcp,
                           gSupportedOptions[x].level,
                           gSupportedOptions[x].option,
                           pValue,
                           gSupportedOptions[x].length,
                           gSupportedOptions[x].pComparer);
            // Check that we are able to set an option
            // value that is different to the current
            // value and then put it back to normal
            // again.
            memcpy(pValueSaved, pValue, gSupportedOptions[x].length);
            gSupportedOptions[x].pChanger(pValue);
            checkOptionSet(cellHandle, gSockHandleTcp,
                           gSupportedOptions[x].level,
                           gSupportedOptions[x].option,
                           pValue,
                           gSupportedOptions[x].length,
                           gSupportedOptions[x].pComparer);
            memcpy(pValue, pValueSaved, gSupportedOptions[x].length);
            checkOptionSet(cellHandle, gSockHandleTcp,
                           gSupportedOptions[x].level,
                           gSupportedOptions[x].option,
                           pValue,
                           gSupportedOptions[x].length,
                           gSupportedOptions[x].pComparer);
        }
    }

    // Free memory again
    free(pValue);
    free(pValueSaved);

    // Close TCP socket, immediately since it was never
    // connected
    uPortLog("U_CELL_SOCK_TEST: closing sockets...\n");
    U_PORT_TEST_ASSERT(!gClosedCallbackCalledTcp);
    U_PORT_TEST_ASSERT(uCellSockClose(cellHandle,
                                      gSockHandleTcp,
                                      NULL) == 0);
    uPortTaskBlock(U_CFG_OS_YIELD_MS);
    U_PORT_TEST_ASSERT(gClosedCallbackCalledTcp);
    U_PORT_TEST_ASSERT(gCallbackErrorNum == 0);

    // Deinit cell sockets
    uCellSockDeinit();

    // Disconnect
    U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_SOCK_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellSock]", "cellSockCleanUp")
{
    int32_t x;

    uCellSockDeinit();
    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_CELL_SOCK_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_CELL_SOCK_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
