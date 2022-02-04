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
 * @brief Test for the u-blox security API: these should pass on all
 * platforms that include the appropriate communications hardware,
 * i.e. currently cellular SARA-R5.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free(), rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_CLIB_LEAKS.

#include "u_error_common.h" // For U_ERROR_COMMON_NOT_SUPPORTED

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
#include "u_port_event_queue.h"
#include "u_sock.h"
#include "u_sock_test_shared_cfg.h"
# ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
extern void uAtClientDetailedDebugOn();
extern void uAtClientDetailedDebugOff();
extern void uAtClientDetailedDebugPrint();
# endif
#endif

#include "u_security.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
/** Timeout for the security sealing operation.
 */
# define U_SECURITY_TEST_SEAL_TIMEOUT_SECONDS (60 * 4)
#endif

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET

#ifndef U_SECURITY_TEST_TASK_STACK_SIZE_BYTES
/** The stack size to use for the test task created during
 * async sockets testing with C2C.
 */
# define U_SECURITY_TEST_TASK_STACK_SIZE_BYTES 2048
#endif

#ifndef U_SECURITY_TEST_TASK_PRIORITY
/** The priority to use for the test task created during
 * async sockets testing with C2C.  If an AT client is running
 * make sure that this is lower priority than its URC handler.
 */
# define U_SECURITY_TEST_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 5)
#endif

#ifndef U_SECURITY_TEST_RECEIVE_QUEUE_LENGTH
/** The queue length, used for asynchronous tests.
 */
# define U_SECURITY_TEST_RECEIVE_QUEUE_LENGTH 10
#endif

# ifndef U_SECURITY_TEST_C2C_MAX_TCP_READ_WRITE_SIZE
/** The maximum TCP read/write size to use during C2C testing.
 */
#  define U_SECURITY_TEST_C2C_MAX_TCP_READ_WRITE_SIZE 1024
# endif

# ifndef U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE
/** The small packet size to send when what we're actually
 * trying to test is the URC behaviour of C2C.
 */
#  define U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE 50
# endif

# ifndef U_SECURITY_TEST_C2C_SMALL_CHUNK_TOTAL_SIZE
/** The total amount of data to send during the small
 * chunks test.
 */
#  define U_SECURITY_TEST_C2C_SMALL_CHUNK_TOTAL_SIZE 250
# endif

# ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
#  define LOG_ON uAtClientDetailedDebugOn()
#  define LOG_OFF uAtClientDetailedDebugOff()
#  define LOG_PRINT uAtClientDetailedDebugPrint()
# else
#  define LOG_ON
#  define LOG_OFF
#  define LOG_PRINT
# endif

#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;
#endif

// A string of all possible characters, used
// when testing end to end encryption
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f";

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
/** Data to exchange in a sockets test.
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
                                 "01234567890123456789012345678901234567890123456789";

/** Descriptor for asynchronous data reception.
 */
uSockDescriptor_t gDescriptor;

/** Handle for the event queue used during asynchronous data testing.
 */
int32_t gEventQueueHandle = -1;

/** Pointer to buffer for asynchronous data reception.
 */
char *gpBuffer = NULL;

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
// Callback function for the security sealing processes.
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}
#endif

// Standard preamble for all security tests
static void stdPreamble()
{
#if (U_CFG_APP_GNSS_UART < 0)
    int32_t networkHandle = -1;
#endif

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type if its not already been added
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle < 0) {
            if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
                uPortLog("U_SECURITY_TEST: adding %s network...\n",
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
#if (U_CFG_APP_GNSS_UART < 0)
                if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                    networkHandle = gUNetworkTestCfg[x].handle;
                }
#endif
            }
        }
    }

    // Bring up each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle >= 0) {
            uPortLog("U_SECURITY_TEST: bringing up %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkUp(gUNetworkTestCfg[x].handle) == 0);
        }
    }
}

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
// Send an entire TCP data buffer until done
static size_t sendTcp(uSockDescriptor_t descriptor,
                      const char *pData, size_t sizeBytes)
{
    int32_t x;
    size_t sentSizeBytes = 0;
    int64_t startTimeMs;

    uPortLog("U_SECURITY_TEST: sending %d byte(s) of TCP data...\n",
             sizeBytes);
    startTimeMs = uPortGetTickTimeMs();
    while ((sentSizeBytes < sizeBytes) &&
           ((uPortGetTickTimeMs() - startTimeMs) < 10000)) {
        x = uSockWrite(descriptor, (const void *) pData,
                       sizeBytes - sentSizeBytes);
        if (x > 0) {
            sentSizeBytes += x;
            uPortLog("U_SECURITY_TEST: sent %d byte(s) of TCP data @%d ms.\n",
                     sentSizeBytes, (int32_t) uPortGetTickTimeMs());
        } else {
            uPortLog("U_SECURITY_TEST: send returned %d.\n", x);
        }
    }

    return sentSizeBytes;
}

// Make sure that size is greater than 0 and no more than limit,
// useful since, when moduloing a very large number number,
// compilers sometimes screw up and produce a small *negative*
// number.
static size_t fix(size_t size, size_t limit)
{
    if (size == 0) {
        size = limit / 2; // better than 1
    } else if (size > limit) {
        size = limit;
    }

    return size;
}

// Event task triggered by the arrival of data.
static void rxAsyncEventTask(void *pParameter, size_t parameterLength)
{
    int32_t thisSizeReceived;
    int32_t totalSizeReceived = 0;
    // The parameter that arrives here is a pointer to the
    // payload which is itself a pointer to sizeBytesReceive,
    // hence the need to double dereference here.
    int32_t *pSizeBytes = *((int32_t **) pParameter);

    (void) parameterLength;

    if (gpBuffer != NULL) {
        // Read from the socket until there's nothing left to read
        //lint -e{776} Suppress possible truncation of addition
        do {
            thisSizeReceived = uSockRead(gDescriptor, gpBuffer + totalSizeReceived,
                                         U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE - totalSizeReceived);
            if (thisSizeReceived > 0) {
                totalSizeReceived += thisSizeReceived;
            }
        } while ((thisSizeReceived > 0) && (totalSizeReceived < U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE));
        *pSizeBytes += totalSizeReceived;
    }
}

// Callback to send to event queue triggered by
// data arriving.
//lint -e{818} Suppress could be const, need to follow
// function signature
static void sendToEventQueue(void *pParameter)
{
    U_PORT_TEST_ASSERT(gEventQueueHandle >= 0);

    // Forward the pointer to rxAsyncEventTask().
    // Note: uPortEventQueueSend() expects to
    // receive a pointer to a payload, so here
    // we give it the address of pParameter,
    // so that it will send on a copy
    // of the pointer that is pParameter.
    uPortEventQueueSend(gEventQueueHandle,
                        &pParameter, sizeof(size_t *));
}

#endif // U_CFG_TEST_SECURITY_C2C_TE_SECRET

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET

/** Test chip to chip security, basic test.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[security]", "securityC2cBasic")
{
    int32_t networkHandle;
    int32_t heapUsed;
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmac[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];

    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                // Note: don't check sealed status here, C2C key pairing
                // is intended to be performed by a customer only BEFORE
                // bootstrapping or sealing is completed, in a sanitized
                // environment where the returned values can be stored
                // in the MCU.
                // On the u-blox test farm we enable the feature
                // LocalC2CKeyPairing via the u-blox security services REST
                // API for all our modules so that we can complete the
                // pairing process even after sealing.

                // Test that closing a session that is not open is fine
                U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
                uPortLog("U_SECURITY_TEST: pairing...\n");
                LOG_ON;
                U_PORT_TEST_ASSERT(uSecurityC2cPair(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);
                // Make sure it's still fine
                U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
                uPortLog("U_SECURITY_TEST: opening a secure session...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cOpen(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);
                uPortLog("U_SECURITY_TEST: closing the session again...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
                LOG_OFF;
                LOG_PRINT;
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: we have leaked %d byte(s).\n",
                     heapUsed);
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= 0);
        }
    }
}

/** Test chip to chip security but this time there's a sock in it.
 */
U_PORT_TEST_FUNCTION("[security]", "securityC2cSock")
{
    int32_t errorCode;
    int32_t networkHandle;
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmac[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];
    uSockAddress_t remoteAddress;
    uSockDescriptor_t descriptor;
    size_t sizeBytes;
    size_t offset;
    int32_t y;
    char *pDataReceived;
    int64_t startTimeMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                // Note: don't check sealed status here, C2C key pairing
                // is intended to be performed by a customer only BEFORE
                // bootstrapping or sealing is completed, in a sanitized
                // environment where the returned values can be stored
                // in the MCU.
                // On the u-blox test farm we enable the feature
                // LocalC2CKeyPairing via the u-blox security services REST
                // API for all our modules so that we can complete the
                // pairing process even after sealing.

                uPortLog("U_SECURITY_TEST: pairing...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cPair(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);

                // Open a new secure session and perform a sockets operation
                uPortLog("U_SECURITY_TEST: opening a secure session...\n");
                LOG_ON;
                U_PORT_TEST_ASSERT(uSecurityC2cOpen(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);

                uPortLog("U_SECURITY_TEST: looking up echo server \"%s\"...\n",
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

                // Connect the socket
                uPortLog("U_SECURITY_TEST: connect socket to \"%s:%d\"...\n",
                         U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                         U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
                // Connections can fail so allow this a few goes
                errorCode = -1;
                for (y = 2; (y > 0) && (errorCode < 0); y--) {
                    errorCode = uSockConnect(descriptor, &remoteAddress);
                }
                U_PORT_TEST_ASSERT(errorCode == 0);

                uPortLog("U_SECURITY_TEST: sending/receiving %d bytes"
                         " of data over a  TCP socket with data reception"
                         " into the same task...\n",
                         sizeof(gSendData) - 1);

                // Throw random sized TCP segments up...
                offset = 0;
                y = 0;
                startTimeMs = uPortGetTickTimeMs();
                while ((offset < sizeof(gSendData) - 1) &&
                       (uPortGetTickTimeMs() - startTimeMs < 20000)) {
                    sizeBytes = (rand() % U_SECURITY_TEST_C2C_MAX_TCP_READ_WRITE_SIZE) + 1;
                    sizeBytes = fix(sizeBytes,
                                    U_SECURITY_TEST_C2C_MAX_TCP_READ_WRITE_SIZE);
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
                uPortLog("U_SECURITY_TEST: %d byte(s) sent via TCP @%d ms,"
                         " now receiving...\n", sizeBytes,
                         (int32_t) uPortGetTickTimeMs());
                U_PORT_TEST_ASSERT(sizeBytes >= sizeof(gSendData) - 1);

                // ...and capture them all again afterwards
                pDataReceived = (char *) malloc(sizeof(gSendData) - 1);
                U_PORT_TEST_ASSERT(pDataReceived != NULL);
                startTimeMs = uPortGetTickTimeMs();
                offset = 0;
                //lint -e{441} Suppress loop variable not found in
                // condition: we're using time instead
                for (y = 0; (offset < sizeof(gSendData) - 1) &&
                     (uPortGetTickTimeMs() - startTimeMs < 20000); y++) {
                    //lint -e{613} Suppress possible use of NULL pointer
                    // for pDataReceived
                    sizeBytes = uSockRead(descriptor,
                                          pDataReceived + offset,
                                          (sizeof(gSendData) - 1) - offset);
                    if (sizeBytes > 0) {
                        offset += sizeBytes;
                        uPortLog("U_SECURITY_TEST: received %d byte(s) out of"
                                 " %d on TCP socket.\n",
                                 offset, sizeof(gSendData) - 1);
                    }
                }
                sizeBytes = offset;
                if (sizeBytes < sizeof(gSendData) - 1) {
                    uPortLog("U_SECURITY_TEST: only %d byte(s) received after %d ms.\n",
                             sizeBytes,
                             (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                    //lint -e(506, 774) Suppress constant Boolean always evaluates to false
                    U_PORT_TEST_ASSERT(false);
                } else {
                    uPortLog("U_SECURITY_TEST: all %d byte(s) received back after"
                             " %d ms, checking if they were as expected...\n",
                             sizeBytes,
                             (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                    // Check the characters are the same
                    //lint -e(668) Suppress possible use of NULL pointer
                    // for pDataReceived
                    U_PORT_TEST_ASSERT(memcmp(pDataReceived, gSendData, sizeBytes) == 0);
                }

                // Close the socket
                U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
                uSockCleanUp();

                free(pDataReceived);

                uPortLog("U_SECURITY_TEST: closing the session again...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
                LOG_OFF;
                LOG_PRINT;
            }

#ifndef __XTENSA__
            // Check for memory leaks
            // This if'ed out for ESP32 (xtensa compiler) as
            // the way it's heap work means that if blocks are
            // freed in a different order to they were allocated and
            // any one of those blocks remains allocated (which sockets
            // will do here as we allocate two mutexes when they are first
            // used) then the amount of heap remaining is not possible
            // to calculate with any degree of confidence (a four byte
            // variant due to block length tracking in their
            // implementation).
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: during this part of the test %d"
                     " byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
#else
            (void) heapUsed;
            (void) heapSockInitLoss;
            (void) heapXxxSockInitLoss;
#endif
        }
    }
}

/** Test chip to chip security but this time with asynchronous
 * data reception in order to test URCs are properly handled.
 */
U_PORT_TEST_FUNCTION("[security]", "securityC2cSockAsync")
{
    int32_t errorCode;
    int32_t networkHandle;
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmac[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];
    uSockAddress_t remoteAddress;
    size_t sizeBytesSend;
    size_t sizeBytesReceive = 0;
    size_t offset;
    int32_t y;
    int64_t startTimeMs;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                // Note: don't check sealed status here, C2C key pairing
                // is intended to be performed by a customer only BEFORE
                // bootstrapping or sealing is completed, in a sanitized
                // environment where the returned values can be stored
                // in the MCU.
                // On the u-blox test farm we enable the feature
                // LocalC2CKeyPairing via the u-blox security services REST
                // API for all our modules so that we can complete the
                // pairing process even after sealing.

                uPortLog("U_SECURITY_TEST: pairing...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cPair(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);

                // Open a new secure session and perform a sockets operation
                uPortLog("U_SECURITY_TEST: opening a secure session...\n");
                LOG_ON;
                U_PORT_TEST_ASSERT(uSecurityC2cOpen(networkHandle,
                                                    U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                    key, hmac) == 0);

                uPortLog("U_SECURITY_TEST: looking up echo server \"%s\"...\n",
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
                gDescriptor = uSockCreate(networkHandle, U_SOCK_TYPE_STREAM,
                                          U_SOCK_PROTOCOL_TCP);
                heapXxxSockInitLoss -= uPortGetHeapFree();
                U_PORT_TEST_ASSERT(gDescriptor >= 0);

                // Connect the socket
                uPortLog("U_SECURITY_TEST: connect socket to \"%s:%d\"...\n",
                         U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                         U_SOCK_TEST_ECHO_TCP_SERVER_PORT);
                // Connections can fail so allow this a few goes
                errorCode = -1;
                for (y = 2; (y > 0) && (errorCode < 0); y--) {
                    errorCode = uSockConnect(gDescriptor, &remoteAddress);
                }
                U_PORT_TEST_ASSERT(errorCode == 0);

                // Create the event queue with, at the end of it,
                // a task that will handle the received TCP packets.
                // The thing it gets sent on the event queue is a pointer
                // to sizeBytesReceive
                gEventQueueHandle = uPortEventQueueOpen(rxAsyncEventTask,
                                                        "testTaskRxData",
                                                        //lint -e(866) Suppress unusual
                                                        // use of & in sizeof()
                                                        sizeof(&sizeBytesReceive),
                                                        U_SECURITY_TEST_TASK_STACK_SIZE_BYTES,
                                                        U_SECURITY_TEST_TASK_PRIORITY,
                                                        U_SECURITY_TEST_RECEIVE_QUEUE_LENGTH);
                U_PORT_TEST_ASSERT(gEventQueueHandle >= 0);

                // Ask the sockets API for a pointer to sizeBytesReceive
                // to be sent to our trampoline function,
                // sendToEventQueue(), whenever UDP data arrives.
                // sendToEventQueue() will then forward the
                // pointer to the event queue and hence to
                // rxAsyncEventTask()
                uSockRegisterCallbackData(gDescriptor,
                                          sendToEventQueue,
                                          &sizeBytesReceive);

                // Set the port to be non-blocking; we will pick up
                // the TCP data that we have been called-back to
                // say has arrived and then if we ask again we want
                // to know that there is nothing more to receive
                // without hanging about so that we can leave the
                // event handler quickly.
                uSockBlockingSet(gDescriptor, false);

                uPortLog("U_SECURITY_TEST: sending/receiving data over a"
                         " TCP socket with data reception into another"
                         " task...\n");

                // Throw small TCP segments up and wait
                // for them to come back...
                gpBuffer = (char *) malloc(U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE);
                U_PORT_TEST_ASSERT(gpBuffer != NULL);
                offset = 0;
                y = 0;
                startTimeMs = uPortGetTickTimeMs();
                while ((offset < U_SECURITY_TEST_C2C_SMALL_CHUNK_TOTAL_SIZE) &&
                       (uPortGetTickTimeMs() - startTimeMs < 120000)) {
                    sizeBytesSend = U_SECURITY_TEST_C2C_SMALL_CHUNK_SIZE;
                    if (offset + sizeBytesSend > sizeof(gSendData) - 1) {
                        sizeBytesSend = (sizeof(gSendData) - 1) - offset;
                    }
                    sizeBytesReceive = 0;
                    if (sendTcp(gDescriptor, gSendData + offset,
                                sizeBytesSend) == sizeBytesSend) {
                        uPortLog("U_SECURITY_TEST: %d byte(s) sent via TCP @%d ms,"
                                 " now receiving...\n", sizeBytesSend,
                                 (int32_t) uPortGetTickTimeMs());
                        // Give the data time to come back
                        for (size_t z = 20; (z > 0) &&
                             (sizeBytesReceive < sizeBytesSend); z--) {
                            uPortTaskBlock(1000);
                        }
                        if (sizeBytesReceive < sizeBytesSend) {
                            uPortLog("U_SECURITY_TEST: after sending a total"
                                     " of %d byte(s), receiving failed.\n",
                                     sizeBytesSend + offset);
                            //lint -e(506, 774) Suppress constant Boolean always
                            // evaluates to false
                            U_PORT_TEST_ASSERT(false);
                        }
                        // Check it
                        //lint -e(668) Suppress possible use of NULL pointer
                        // for gpBuffer
                        if (memcmp(gpBuffer, gSendData + offset, sizeBytesReceive) != 0) {
                            uPortLog("U_SECURITY_TEST: expected received data contents"
                                     " not what was expected.\n");
                            uPortLog("U_SECURITY_TEST: expected \"%*s\", received"
                                     " \"%*s\".\n",
                                     sizeBytesSend, gSendData + offset,
                                     sizeBytesReceive, gpBuffer);
                            //lint -e(506, 774) Suppress constant Boolean always
                            // evaluates to false
                            U_PORT_TEST_ASSERT(false);
                        }
                        offset += sizeBytesSend;
                    }
                    y++;
                }

                sizeBytesSend = offset;
                if (sizeBytesSend < U_SECURITY_TEST_C2C_SMALL_CHUNK_TOTAL_SIZE) {
                    uPortLog("U_SECURITY_TEST: only %d byte(s) sent after %d ms.\n",
                             sizeBytesSend,
                             (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                    //lint -e(506, 774) Suppress constant Boolean always evaluates to false
                    U_PORT_TEST_ASSERT(false);
                }

                // As a sanity check, make sure that
                // U_SECURITY_TEST_TASK_STACK_SIZE_BYTES
                // was big enough
                y = uPortEventQueueStackMinFree(gEventQueueHandle);
                uPortLog("U_SOCK_TEST: event queue task had %d byte(s)"
                         " free at a minimum.\n", y);
                U_PORT_TEST_ASSERT((y > 0) ||
                                   (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));

                // Close the socket
                U_PORT_TEST_ASSERT(uSockClose(gDescriptor) == 0);
                uSockCleanUp();

                // Close the event queue
                U_PORT_TEST_ASSERT(uPortEventQueueClose(gEventQueueHandle) == 0);
                gEventQueueHandle = -1;

                free(gpBuffer);

                uPortLog("U_SECURITY_TEST: closing the session again...\n");
                U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
                LOG_OFF;
                LOG_PRINT;
            }

#if !defined(__XTENSA__) && !U_CFG_OS_CLIB_LEAKS
            // Check for memory leaks, if the platform isn't leaky
            // This if'ed out for ESP32 (xtensa compiler) as
            // the way it's heap work means that if blocks are
            // freed in a different order to they were allocated and
            // any one of those blocks remains allocated (which sockets
            // will do here as we allocate two mutexes when they are first
            // used) then the amount of heap remaining is not possible
            // to calculate with any degree of confidence (a four byte
            // variant due to block length tracking in their
            // implementation).
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: during this part of the test %d"
                     " byte(s) were lost to sockets initialisation;"
                     " we have leaked %d byte(s).\n",
                     heapSockInitLoss + heapXxxSockInitLoss,
                     heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
#else
            (void) heapUsed;
            (void) heapSockInitLoss;
            (void) heapXxxSockInitLoss;
#endif
        }
    }
}

#endif // U_CFG_TEST_SECURITY_C2C_TE_SECRET

/** Test security sealing, requires a network connection.
 * Note: this test will *only* attempt a seal if
 * U_CFG_SECURITY_DEVICE_PROFILE_UID is defined to contain
 * a valid device profile UID string (without quotes).
 */
U_PORT_TEST_FUNCTION("[security]", "securitySeal")
{
    int32_t networkHandle;
    int32_t heapUsed;
    int32_t z;
    char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    char rotUid[U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES];

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");

                // Get the serial number
                z = uSecurityGetSerialNumber(networkHandle, serialNumber);
                U_PORT_TEST_ASSERT((z > 0) && (z < (int32_t) sizeof(serialNumber)));
                uPortLog("U_SECURITY_TEST: module serial number is \"%s\".\n",
                         serialNumber);

                // Get the root of trust UID with NULL rotUid
                U_PORT_TEST_ASSERT(uSecurityGetRootOfTrustUid(networkHandle, NULL) >= 0);
                // Get the root of trust UID properly
                U_PORT_TEST_ASSERT(uSecurityGetRootOfTrustUid(networkHandle,
                                                              rotUid) == sizeof(rotUid));
                uPortLog("U_SECURITY_TEST: root of trust UID is 0x");
                for (size_t y = 0; y < sizeof(rotUid); y++) {
                    uPortLog("%02x", rotUid[y]);
                }
                uPortLog(".\n");

                uPortLog("U_SECURITY_TEST: waiting for bootstrap status...\n");
                // Try 10 times with a wait in-between to get bootstrapped
                // status
                for (size_t y = 10; (y > 0) &&
                     !uSecurityIsBootstrapped(networkHandle); y--) {
                    uPortTaskBlock(5000);
                }
                if (uSecurityIsBootstrapped(networkHandle)) {
                    uPortLog("U_SECURITY_TEST: device is bootstrapped.\n");
                    if (!uSecurityIsSealed(networkHandle)) {
#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
                        uPortLog("U_SECURITY_TEST: device is bootstrapped, performing"
                                 " security seal with device profile UID string \"%s\""
                                 " and serial number \"%s\"...\n",
                                 U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                 serialNumber);
                        gStopTimeMs = uPortGetTickTimeMs() +
                                      (U_SECURITY_TEST_SEAL_TIMEOUT_SECONDS * 1000);
                        if (uSecuritySealSet(networkHandle,
                                             U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                             serialNumber, keepGoingCallback) == 0) {
                            uPortLog("U_SECURITY_TEST: device is security sealed"
                                     " with device profile UID string \"%s\""
                                     " and serial number \"%s\".\n",
                                     U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                     serialNumber);
                            U_PORT_TEST_ASSERT(uSecurityIsSealed(networkHandle));
                        } else {
                            uPortLog("U_SECURITY_TEST: unable to security seal device.\n");
                            U_PORT_TEST_ASSERT(!uSecurityIsSealed(networkHandle));
                            //lint -e(774) Suppress always evaluates to false
                            U_PORT_TEST_ASSERT(false);
                        }
#else
                        uPortLog("U_SECURITY_TEST: device is bootstrapped but"
                                 " U_CFG_SECURITY_DEVICE_PROFILE_UID is not"
                                 " defined so no test of security sealing"
                                 " will be performed.\n");
#endif
                    } else {
                        uPortLog("U_SECURITY_TEST: this device supports u-blox"
                                 " security and is already security sealed, no"
                                 " test of security sealing will be carried out.\n");
                    }
                } else {
                    uPortLog("U_SECURITY_TEST: this device supports u-blox"
                             " security but will not bootstrap.\n");
                    U_PORT_TEST_ASSERT(!uSecurityIsSealed(networkHandle));
                    //lint -e(506, 774) Suppress constant Boolean always evaluates to false
                    U_PORT_TEST_ASSERT(false);
                }
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: we have leaked %d byte(s).\n",
                     heapUsed);
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= 0);
        }
    }
}

/** Test end to end encryption.
 */
U_PORT_TEST_FUNCTION("[security]", "securityE2eEncryption")
{
    int32_t networkHandle;
    //lint -esym(838, y) Suppress not used, which will be true
    // if logging is compiled out
    int32_t y;
    int32_t heapUsed;
    void *pData;
    int32_t version;
    int32_t headerLengthBytes = U_SECURITY_E2E_V1_HEADER_LENGTH_BYTES;

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                uPortLog("U_SECURITY_TEST: waiting for seal status...\n");
                if (uSecurityIsSealed(networkHandle)) {
                    uPortLog("U_SECURITY_TEST: device is sealed.\n");

                    // Ask for a security heartbeat to be triggered:
                    // this very likely won't be permitted since
                    // it is quite severely rate limited (e.g. just once
                    // in 24 hours) so we're really only checking that it
                    // doesn't crash here
                    // TODO: temporarily remove the security heartbeat
                    // call here.  One of the test instances is misbehaving
                    // in this function (taking too long to return), will
                    // disable while the problem is investigated.
                    //y = uSecurityHeartbeatTrigger(networkHandle);
                    //uPortLog("U_SECURITY_TEST: uSecurityHeartbeatTrigger()"
                    //         " returned %d.\n", y);
                    uPortLog("U_SECURITY_TEST: testing end to end encryption...\n");

                    // First get the current E2E encryption version
                    version = uSecurityE2eGetVersion(networkHandle);
                    if (version > 0) {
                        U_PORT_TEST_ASSERT((version == 1) || (version == 2));
                        uPortLog("U_SECURITY_TEST: end to end encryption is v%d\n", version);
                        if (version == 2) {
                            // On all current modules where V2 is supported and
                            // selected V1 is also supported; this may change
                            // in future of course
                            version = 1;
                            uPortLog("U_SECURITY_TEST: setting end to end encryption v%d.\n", version);
                            U_PORT_TEST_ASSERT(uSecurityE2eSetVersion(networkHandle, version) == 0);
                            U_PORT_TEST_ASSERT(uSecurityE2eGetVersion(networkHandle) == version);
                            version = 2;
                            uPortLog("U_SECURITY_TEST: setting end to end encryption v%d again.\n", version);
                            U_PORT_TEST_ASSERT(uSecurityE2eSetVersion(networkHandle, version) == 0);
                            U_PORT_TEST_ASSERT(uSecurityE2eGetVersion(networkHandle) == version);
                            headerLengthBytes = U_SECURITY_E2E_V2_HEADER_LENGTH_BYTES;
                        }
                        uPortLog("U_SECURITY_TEST: end to end encryption is v%d\n", version);

                    } else {
                        uPortLog("U_SECURITY_TEST: end to end encryption version"
                                 " check not supported, assuming v1.\n");
                        version = 1;
                    }

                    // Allocate memory to receive into
                    pData = malloc(sizeof(gAllChars) + headerLengthBytes);
                    U_PORT_TEST_ASSERT(pData != NULL);
                    // Copy the output data into the input buffer, just to have
                    // something in there we can compare against
                    //lint -e(668) Suppress possible NULL pointer, it is checked above
                    memcpy(pData, gAllChars, sizeof(gAllChars));
                    uPortLog("U_SECURITY_TEST: requesting end to end encryption of %d"
                             " byte(s) of data...\n", sizeof(gAllChars));
                    y = uSecurityE2eEncrypt(networkHandle, gAllChars,
                                            pData, sizeof(gAllChars));
                    U_PORT_TEST_ASSERT(y == sizeof(gAllChars) + headerLengthBytes);
                    uPortLog("U_SECURITY_TEST: %d byte(s) of data returned.\n", y);
                    //lint -e(668) Suppress possible NULL pointer, it is checked above
                    U_PORT_TEST_ASSERT(memcmp(pData, gAllChars, sizeof(gAllChars)) != 0);
                    free(pData);
                } else {
                    uPortLog("U_SECURITY_TEST: this device supports u-blox"
                             " security but has not been security sealed,"
                             " no testing of end to end encryption will be"
                             " carried out.\n");
                }
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: we have leaked %d byte(s).\n",
                     heapUsed);
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= 0);
        }
    }
}

/** Test PSK generation.
 */
U_PORT_TEST_FUNCTION("[security]", "securityPskGeneration")
{
    int32_t networkHandle;
    //lint -esym(838, z) Suppress not used, which will be true
    // if logging is compiled out
    int32_t z;
    int32_t pskIdSize;
    int32_t heapUsed;
    char psk[U_SECURITY_PSK_MAX_LENGTH_BYTES];
    char pskId[U_SECURITY_PSK_ID_MAX_LENGTH_BYTES];

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                uPortLog("U_SECURITY_TEST: waiting for seal status...\n");
                if (uSecurityIsSealed(networkHandle)) {
                    uPortLog("U_SECURITY_TEST: device is sealed.\n");

                    // Ask for a security heartbeat to be triggered:
                    // this very likely won't be permitted since
                    // it is quite severely rate limited (e.g. just once
                    // in 24 hours) so we're really only checking that it
                    // doesn't crash here
                    // TODO: temporarily remove the security heartbeat
                    // call here.  One of the test instances is misbehaving
                    // in this function (taking too long to return), will
                    // disable while the problem is investiated.
                    //z = uSecurityHeartbeatTrigger(networkHandle);
                    //uPortLog("U_SECURITY_TEST: uSecurityHeartbeatTrigger()"
                    //         " returned %d.\n", z);
                    uPortLog("U_SECURITY_TEST: testing PSK generation...\n");
                    memset(psk, 0, sizeof(psk));
                    memset(pskId, 0, sizeof(pskId));
                    pskIdSize = uSecurityPskGenerate(networkHandle, 16,
                                                     psk, pskId);
                    U_PORT_TEST_ASSERT(pskIdSize > 0);
                    U_PORT_TEST_ASSERT(pskIdSize < (int32_t) sizeof(pskId));
                    // Check that the PSK ID isn't still all zeroes
                    // expect beyond pskIdSize
                    z = 0;
                    for (size_t y = 0; y < sizeof(pskId); y++) {
                        if ((int32_t) y < pskIdSize) {
                            if (pskId[y] == 0) {
                                z++;
                            }
                        } else {
                            U_PORT_TEST_ASSERT(pskId[y] == 0);
                        }
                    }
                    U_PORT_TEST_ASSERT(z < pskIdSize);
                    // Check that the first 16 bytes of the PSK aren't still
                    // all zero but that the remainder are
                    z = 0;
                    for (size_t y = 0; y < sizeof(psk); y++) {
                        if (y < 16) {
                            if (psk[y] == 0) {
                                z++;
                            }
                        } else {
                            U_PORT_TEST_ASSERT(psk[y] == 0);
                        }
                    }
                    U_PORT_TEST_ASSERT(z < 16);
                    memset(psk, 0, sizeof(psk));
                    memset(pskId, 0, sizeof(pskId));
                    pskIdSize = uSecurityPskGenerate(networkHandle, 32,
                                                     psk, pskId);
                    U_PORT_TEST_ASSERT(pskIdSize > 0);
                    U_PORT_TEST_ASSERT(pskIdSize < (int32_t) sizeof(pskId));
                    // Check that the PSK ID isn't still all zeroes
                    // expect beyond pskIdSize
                    z = 0;
                    for (size_t y = 0; y < sizeof(pskId); y++) {
                        if ((int32_t) y < pskIdSize) {
                            if (pskId[y] == 0) {
                                z++;
                            }
                        } else {
                            U_PORT_TEST_ASSERT(pskId[y] == 0);
                        }
                    }
                    U_PORT_TEST_ASSERT(z < pskIdSize);
                    // Check that the PSK isn't still all zeroes
                    z = 0;
                    for (size_t y = 0; y < sizeof(psk); y++) {
                        if (psk[y] == 0) {
                            z++;
                        }
                    }
                    U_PORT_TEST_ASSERT(z < (int32_t) sizeof(psk));
                } else {
                    uPortLog("U_SECURITY_TEST: this device supports u-blox"
                             " security but has not been security sealed,"
                             " no testing of end to end encryption will be"
                             " carried out.\n");
                }
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: we have leaked %d byte(s).\n",
                     heapUsed);
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= 0);
        }
    }
}

/** Test reading the certificate/key/authorities from sealing.
 */
U_PORT_TEST_FUNCTION("[security]", "securityZtp")
{
    int32_t networkHandle;
    int32_t y;
    int32_t z;
    int32_t heapUsed;
    char *pData;
#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmac[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];
#endif

    // Do the standard preamble to make sure there is
    // a network underneath us
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            uPortLog("U_SECURITY_TEST: checking if u-blox security"
                     " is supported by handle %d...\n", networkHandle);
            if (uSecurityIsSupported(networkHandle)) {
                uPortLog("U_SECURITY_TEST: security is supported.\n");
                uPortLog("U_SECURITY_TEST: waiting for seal status...\n");
                if (uSecurityIsSealed(networkHandle)) {
                    uPortLog("U_SECURITY_TEST: device is sealed.\n");

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
                    // If C2C security is in place for a module then the
                    // certificates can only be read if a C2C session is
                    // open
                    uPortLog("U_SECURITY_TEST: pairing for C2C...\n");
                    LOG_ON;
                    U_PORT_TEST_ASSERT(uSecurityC2cPair(networkHandle,
                                                        U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                        key, hmac) == 0);
                    uPortLog("U_SECURITY_TEST: opening a C2C session...\n");
                    U_PORT_TEST_ASSERT(uSecurityC2cOpen(networkHandle,
                                                        U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                                        key, hmac) == 0);
                    LOG_OFF;
                    LOG_PRINT;
#endif
                    // First get the size of the device public certificate
                    y = uSecurityZtpGetDeviceCertificate(networkHandle, NULL, 0);
                    uPortLog("U_SECURITY_TEST: device public X.509 certificate is %d bytes.\n", y);
                    U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                    if (y > 0) {
                        // Allocate memory to receive into and zero it for good measure
                        pData = (char *) malloc(y);
                        U_PORT_TEST_ASSERT(pData != NULL);
                        //lint -e(668) Suppress possible use of NULL pointer for pData
                        memset(pData, 0, y);
                        uPortLog("U_SECURITY_TEST: getting device public X.509 certificate...\n", y);
                        z = uSecurityZtpGetDeviceCertificate(networkHandle, pData, y);
                        U_PORT_TEST_ASSERT(z == y);
                        // Can't really check the data but can check that it is
                        // of the correct length
                        U_PORT_TEST_ASSERT(strlen(pData) == z - 1);
                        free(pData);
                    } else {
                        uPortLog("U_SECURITY_TEST: module does not support reading device"
                                 " public certificate.\n");
                    }

                    // Get the size of the device private certificate
                    y = uSecurityZtpGetPrivateKey(networkHandle, NULL, 0);
                    uPortLog("U_SECURITY_TEST: private key is %d bytes.\n", y);
                    U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                    if (y > 0) {
                        // Allocate memory to receive into and zero it for good measure
                        pData = (char *) malloc(y);
                        U_PORT_TEST_ASSERT(pData != NULL);
                        //lint -e(668) Suppress possible use of NULL pointer for pData
                        memset(pData, 0, y);
                        uPortLog("U_SECURITY_TEST: getting private key...\n", y);
                        z = uSecurityZtpGetPrivateKey(networkHandle, pData, y);
                        U_PORT_TEST_ASSERT(z == y);
                        // Can't really check the data but can check that it is
                        // of the correct length
                        U_PORT_TEST_ASSERT(strlen(pData) == z - 1);
                        free(pData);
                    } else {
                        uPortLog("U_SECURITY_TEST: module does not support reading device"
                                 " private key.\n");
                    }

                    // Get the size of the certificate authorities
                    y = uSecurityZtpGetCertificateAuthorities(networkHandle, NULL, 0);
                    uPortLog("U_SECURITY_TEST: X.509 certificate authorities are %d bytes.\n", y);
                    U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                    if (y > 0) {
                        // Allocate memory to receive into and zero it for good measure
                        pData = (char *) malloc(y);
                        U_PORT_TEST_ASSERT(pData != NULL);
                        //lint -e(668) Suppress possible use of NULL pointer for pData
                        memset(pData, 0, y);
                        uPortLog("U_SECURITY_TEST: getting X.509 certificate authorities...\n", y);
                        z = uSecurityZtpGetCertificateAuthorities(networkHandle, pData, y);
                        U_PORT_TEST_ASSERT(z == y);
                        // Can't really check the data but can check that it is
                        // of the correct length
                        U_PORT_TEST_ASSERT(strlen(pData) == z - 1);
                        free(pData);
                    } else {
                        uPortLog("U_SECURITY_TEST: module does not support reading "
                                 " certificate authorities.\n");
                    }

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
                    uPortLog("U_SECURITY_TEST: closing C2C session again...\n");
                    U_PORT_TEST_ASSERT(uSecurityC2cClose(networkHandle) == 0);
#endif

                } else {
                    uPortLog("U_SECURITY_TEST: this device supports u-blox"
                             " security but has not been security sealed,"
                             " no testing of reading ZTP items can be"
                             " carried out.\n");
                }
            }

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_SECURITY_TEST: we have leaked %d byte(s).\n",
                     heapUsed);
            // heapUsed < 0 for the Zephyr case where the heap can look
            // like it increases (negative leak)
            U_PORT_TEST_ASSERT(heapUsed <= 0);
        }
    }
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[security]", "securityCleanUp")
{
    int32_t y;

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET
    if (gEventQueueHandle >= 0) {
        uPortEventQueueClose(gEventQueueHandle);
        gEventQueueHandle = -1;
    }
#endif

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
        uPortLog("U_SECURITY_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_SECURITY_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
