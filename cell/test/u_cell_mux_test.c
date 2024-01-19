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
 * @brief Tests for the cellular MUX API.
 * These test should pass on all platforms that have a cellular module
 * connected to them.  They are only compiled if U_CFG_TEST_CELL_MODULE_TYPE
 * is defined and can be disabled with U_CFG_TEST_DISABLE_MUX, however
 * they are also disabled if U_CFG_PPP_ENABLE is defined since stopping
 * the mux while PPP is using it upsets just about everyone.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#if defined(U_CFG_TEST_CELL_MODULE_TYPE) && !defined(U_CFG_TEST_DISABLE_MUX) && !defined(U_CFG_PPP_ENABLE)

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy()/strncmp()
#include "stdio.h"     // snprintf()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_info.h"
#include "u_cell_pwr.h"
#include "u_cell_sock.h"
#include "u_cell_mqtt.h"
#include "u_cell_http.h"
#include "u_cell_sec.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

#include "u_sock_test_shared_cfg.h"   // For some of the test macros
#include "u_http_client_test_shared_cfg.h"

#include "u_cell_mux.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_CELL_MUX_TEST"

/** The string to put at the start of all prints from this test
 * that do not require an iteration on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_MUX_TEST_BASIC_NUM_ITERATIONS
/** Run the main body of the basic mux test this many times.
 */
# define U_CELL_MUX_TEST_BASIC_NUM_ITERATIONS 10
#endif

#ifndef U_CELL_MUX_TEST_MQTT_SERVER_IP_ADDRESS
/** Server to use for the MQTT part of the mux test.
 */
# define U_CELL_MUX_TEST_MQTT_SERVER_IP_ADDRESS  ubxlib.com
#endif

#ifndef U_CELL_MUX_TEST_MQTT_RESPONSE_TIMEOUT_MS
/** How long to wait for an MQTT response in the MQTT mux test.
 */
# define U_CELL_MUX_TEST_MQTT_RESPONSE_TIMEOUT_MS (10 * 1000)
#endif

#ifndef U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME
/** Name to use when giving an explicit response file name.
 */
# define  U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME "ubxlib_test_http_response"
#endif

#ifndef U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME
/** File name to use when PUT/POSTing data from file.
 */
# define  U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME "ubxlib_test_http_putpost"
#endif

/** The first line of an HTTP response indicating success, normal case.
 */
#define U_CELL_MUX_TEST_HTTP_FIRST_LINE_200_DEFAULT "HTTP/1.0 200 OK"

/** The first line of an HTTP response indicating success, LENA-R8 case.
 */
#define U_CELL_MUX_TEST_HTTP_FIRST_LINE_200_LENA_R8 "HTTP/1.1 200 OK"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold the stuff seen by the HTTP callback().
 */
typedef struct {
    bool called;
    uDeviceHandle_t cellHandle;
    int32_t httpHandle;
    uCellHttpRequest_t requestType;
    bool error;
    char fileNameResponse[U_CELL_FILE_NAME_MAX_LENGTH + 1];
    const char *pExpectedFirstLine;
    bool contentsMismatch;
} uCellMuxHttpTestCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** Flag to keep track of whether the CMUX test failed (so that
 * we can recover if it did).
 */
static bool gTestPassed = false;

/** TCP socket handle.
 */
static int32_t gSockHandle = -1;

/** Error indicator for call-backs: not using asserts
 * in call-backs as when they go off the seem to cause
 * stack overflows.
 */
static volatile int32_t gCallbackErrorNum = 0;

/** Flag to indicate that the socket data
 * callback has been called.
 */
static volatile bool gSockDataCallbackCalled = false;

/** A string of all possible characters.
 */
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f";

/** Keep track of MQTT messages available.
 */
static volatile int32_t gMqttMessagesAvailable = 0;

/** Storage for data seen by the HTTP callback.
 */
static volatile uCellMuxHttpTestCallback_t gHttpCallbackData = {0};

/** Data to send over MQTT; all printable characters.
 */
static const char gMqttSendData[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789\"!#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process.
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Print a buffer.
static void printBuffer(const char *pBuffer, size_t length)
{
    for (size_t x = 0; x < length; x++, pBuffer++) {
        if (!isprint((int32_t) *pBuffer)) {
            uPortLog("[%02x]", (unsigned char) *pBuffer);
        } else {
            uPortLog("%c", *pBuffer);
        }
    }
}

// Make a cellular connection
static int32_t connect(uDeviceHandle_t cellHandle)
{
    gStopTimeMs = uPortGetTickTimeMs() +
                  (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
    return uCellNetConnect(cellHandle, NULL,
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
}

// Callback for socket data being available.
static void sockDataCallback(uDeviceHandle_t cellHandle, int32_t sockHandle)
{
    if (cellHandle != gHandles.cellHandle) {
        gCallbackErrorNum = 1;
    } else if (sockHandle != gSockHandle)  {
        gCallbackErrorNum = 2;
    }
    gSockDataCallbackCalled = true;
}

// MQTT unread messages callback.
static void mqttCallback(int32_t numMessages, void *pParam)
{
    volatile int32_t *pMessagesAvailable = (volatile int32_t *) pParam;

    *pMessagesAvailable = numMessages;
}

// Compare the contents of a file in the cellular module's
// file system with the given string.
static bool checkFile(uDeviceHandle_t cellHandle, const char *pFileName,
                      const char *pExpectedFirstLine, bool printIt)
{
    bool isOk = false;
    int32_t fileSize;
    char *pFileContents;
    int32_t expectedFirstLineLength;

    // For a GET request we check the contents
    fileSize = uCellFileSize(cellHandle, pFileName);
    if (fileSize >= 0) {
        pFileContents = (char *) pUPortMalloc(fileSize);
        if (pFileContents != NULL) {
            if (uCellFileRead(cellHandle, pFileName,
                              pFileContents, (size_t) fileSize) == fileSize) {
                if (printIt) {
                    U_TEST_PRINT_LINE("\"%s\" contains (%d byte(s)):",
                                      pFileName, fileSize);
                    printBuffer(pFileContents, fileSize);
                    uPortLog("\n");
                }
                if (pExpectedFirstLine != NULL) {
                    expectedFirstLineLength = strlen(pExpectedFirstLine);
                    if (fileSize < expectedFirstLineLength) {
                        if (printIt) {
                            U_TEST_PRINT_LINE("expected at least %d byte(s), got %d byte(s).",
                                              expectedFirstLineLength, fileSize);
                        }
                    } else if (memcmp(pFileContents, pExpectedFirstLine, expectedFirstLineLength) != 0) {
                        if (printIt) {
                            U_TEST_PRINT_LINE("first line of file is not as expected, expected (%d byte(s)):",
                                              expectedFirstLineLength);
                            uPortLog("\"\n");
                            printBuffer(pExpectedFirstLine, expectedFirstLineLength);
                            uPortLog("\"\n");
                        }
                    } else {
                        isOk = true;
                    }
                } else {
                    isOk = true;
                }
            } else if (printIt) {
                U_TEST_PRINT_LINE("unable to read all %d byte(s) of \"%s\".",
                                  fileSize, pFileName);
            }
            // Free memory
            uPortFree(pFileContents);
        } else if (printIt) {
            U_TEST_PRINT_LINE("unable to get %d byte(s) of memory to read file \"%s\".",
                              fileSize, pFileName);
        }
    } else if (printIt) {
        U_TEST_PRINT_LINE("getting file size of \"%s\" returned error %d.",
                          pFileName, fileSize);
    }

    return isOk;
}

// Callback for HTTP responses.
static void httpCallback(uDeviceHandle_t cellHandle, int32_t httpHandle,
                         uCellHttpRequest_t requestType, bool error,
                         const char *pFileNameResponse, void *pCallbackParam)
{
    uCellMuxHttpTestCallback_t *pCallbackData = (uCellMuxHttpTestCallback_t *) pCallbackParam;

    pCallbackData->cellHandle = cellHandle;
    pCallbackData->httpHandle = httpHandle;
    pCallbackData->requestType = requestType;
    pCallbackData->error = error;
    strncpy(pCallbackData->fileNameResponse, pFileNameResponse,
            sizeof(pCallbackData->fileNameResponse));
    pCallbackData->contentsMismatch = !checkFile(cellHandle,
                                                 pFileNameResponse,
                                                 pCallbackData->pExpectedFirstLine,
                                                 true);
    pCallbackData->called = true;
}

// Check an HTTP response, return true if it is good, else false.
static bool httpWaitCheckResponse(int32_t timeoutSeconds,
                                  volatile uCellMuxHttpTestCallback_t *pCallbackData,
                                  uDeviceHandle_t cellHandle, int32_t httpHandle,
                                  uCellHttpRequest_t requestType,
                                  const char *pFileNameResponse)
{
    bool isOk = false;
    int32_t startTimeMs = uPortGetTickTimeMs();

    U_TEST_PRINT_LINE("waiting up to %d second(s) for response to HTTP request...",
                      timeoutSeconds);
    while ((uPortGetTickTimeMs() - startTimeMs < (timeoutSeconds * 1000)) &&
           !pCallbackData->called) {
        uPortTaskBlock(100);
    }

    if (pCallbackData->called) {
        isOk = true;
        // The callback was called, check everything
        U_TEST_PRINT_LINE("response received after %d millisecond(s).",
                          uPortGetTickTimeMs() - startTimeMs);
        if (pCallbackData->cellHandle != cellHandle) {
            U_TEST_PRINT_LINE("expected cell handle 0x%08x, got 0x%08x.",
                              cellHandle, pCallbackData->cellHandle);
            isOk = false;
        }
        if (pCallbackData->httpHandle != httpHandle) {
            U_TEST_PRINT_LINE("expected HTTP handle %d, got %d.",
                              httpHandle, pCallbackData->httpHandle);
            isOk = false;
        }
        if (pCallbackData->requestType != requestType) {
            U_TEST_PRINT_LINE("expected response type %d, got %d.",
                              requestType, pCallbackData->requestType);
            isOk = false;
        }
        if (pCallbackData->error) {
            U_TEST_PRINT_LINE("result was an error.");
            isOk = false;
        }
        if (pFileNameResponse != NULL) {
            if (strncmp((const char *) pCallbackData->fileNameResponse, pFileNameResponse,
                        sizeof(pCallbackData->fileNameResponse)) != 0) {
                U_TEST_PRINT_LINE("expected response file name \"%s\", got \"%s\".",
                                  pFileNameResponse, pCallbackData->fileNameResponse);
                isOk = false;
            }
        } else {
            U_TEST_PRINT_LINE("response file name was \"%s\".",
                              pCallbackData->fileNameResponse);
        }
        if (pCallbackData->contentsMismatch) {
            U_TEST_PRINT_LINE("contents of response were not as expected.");
            isOk = false;
        }
    } else {
        U_TEST_PRINT_LINE("callback not called after %d second(s).",
                          (uPortGetTickTimeMs() - startTimeMs) / 1000);
    }

    // Reset for next time
    memset((void *) pCallbackData, 0, sizeof(*pCallbackData));

    return isOk;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** A basic test of the CMUX API.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellMux]", "cellMuxBasic")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t resourceCount;
    // +1 and zero init so that we can treat it as a string
    char buffer1[U_CELL_INFO_IMEI_SIZE + 1] = {0};
    char buffer2[U_CELL_INFO_IMEI_SIZE + 1] = {0};

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data so that we can check for CMUX support
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CMUX)) {
        // We do something simple to show that AT commands work,
        // which is to read the IMEI.  First read it before enabling
        // the mux
        U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer1) == 0);
        U_TEST_PRINT_LINE("IMEI is %s.", buffer1);

        for (size_t x = 0; x < U_CELL_MUX_TEST_BASIC_NUM_ITERATIONS; x++) {

            uPortLog(U_TEST_PREFIX_BASE "_%d: enabling CMUX...\n", x + 1);
            U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellMuxIsEnabled(cellHandle));
            U_PORT_TEST_ASSERT(pUCellMuxChannelGetDeviceSerial(cellHandle, 0) != NULL);
            U_PORT_TEST_ASSERT(pUCellMuxChannelGetDeviceSerial(cellHandle, 1) != NULL);

            // Read the IMEI again and check that the value is the same
            U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer2) == 0);
            uPortLog(U_TEST_PREFIX_BASE "_%d: IMEI read over a CMUX channel gives %s.\n", x + 1, buffer2);
            U_PORT_TEST_ASSERT(strncmp(buffer1, buffer2, sizeof(buffer1)) == 0);

            uPortLog(U_TEST_PREFIX_BASE "_%d: disabling CMUX...\n", x + 1);
            U_PORT_TEST_ASSERT(uCellMuxDisable(cellHandle) == 0);
            U_PORT_TEST_ASSERT(!uCellMuxIsEnabled(cellHandle));
            U_PORT_TEST_ASSERT(pUCellMuxChannelGetDeviceSerial(cellHandle, 0) == NULL);
            U_PORT_TEST_ASSERT(pUCellMuxChannelGetDeviceSerial(cellHandle, 1) == NULL);

            U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, buffer2) == 0);
            uPortLog(U_TEST_PREFIX_BASE "_%d: IMEI read after disabling CMUX gives %s.\n", x + 1, buffer2);
            U_PORT_TEST_ASSERT(strncmp(buffer1, buffer2, sizeof(buffer1)) == 0);
        }
    } else {
        U_TEST_PRINT_LINE("CMUX is not supported, not running tests.");
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) < 0);
        U_PORT_TEST_ASSERT(!uCellMuxIsEnabled(cellHandle));
    }

    gTestPassed = true;

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test sockets over CMUX.
 */
U_PORT_TEST_FUNCTION("[cellMux]", "cellMuxSock")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t resourceCount;
    uSockAddress_t echoServerAddress;
    int32_t w;
    int32_t y;
    int32_t z;
    size_t count;
    char *pBuffer;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    gTestPassed = false;

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data so that we can check for CMUX support
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CMUX)) {
        // Malloc a buffer to receive things into.
        pBuffer = (char *) pUPortMalloc(U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES);
        U_PORT_TEST_ASSERT(pBuffer != NULL);

        U_TEST_PRINT_LINE("enabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);

        // Make a cellular connection
        U_PORT_TEST_ASSERT(connect(cellHandle) == 0);

        U_PORT_TEST_ASSERT(uCellSockInit() == 0);
        U_PORT_TEST_ASSERT(uCellSockInitInstance(cellHandle) == 0);

        // Look up the address of the server we use for TCP echo
        U_PORT_TEST_ASSERT(uCellSockGetHostByName(cellHandle,
                                                  U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME,
                                                  &(echoServerAddress.ipAddress)) == 0);
        // Add the port number we will use
        echoServerAddress.port = U_SOCK_TEST_ECHO_TCP_SERVER_PORT;

        // Create a TCP socket
        gSockHandle = uCellSockCreate(cellHandle, U_SOCK_TYPE_STREAM,
                                      U_SOCK_PROTOCOL_TCP);

        // Add a callback
        uCellSockRegisterCallbackData(cellHandle, gSockHandle, sockDataCallback);

        // Connect the TCP socket
        U_PORT_TEST_ASSERT(uCellSockConnect(cellHandle, gSockHandle,
                                            &echoServerAddress) == 0);

        // No data should have yet flowed
        U_PORT_TEST_ASSERT(!gSockDataCallbackCalled);

        // Send the TCP echo data in random sized chunks
        U_TEST_PRINT_LINE("sending %d byte(s) to %s:%d in random sized"
                          " chunks...", sizeof(gAllChars),
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
            z = uCellSockWrite(cellHandle, gSockHandle,
                               gAllChars + y, w);
            if (z > 0) {
                y += z;
            } else {
                uPortTaskBlock(500);
            }
        }
        U_TEST_PRINT_LINE("%d byte(s) sent in %d chunks.", y, count);

        // Wait a little while to get a data callback
        // triggered by a URC
        for (size_t x = 10; (x > 0) && !gSockDataCallbackCalled; x--) {
            uPortTaskBlock(1000);
        }

        // Get the data back again
        U_TEST_PRINT_LINE("receiving TCP echo data back in random"
                          " sized chunks...");
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
            z = uCellSockRead(cellHandle, gSockHandle, pBuffer + y, w);
            if (z > 0) {
                y += z;
            } else {
                uPortTaskBlock(500);
            }
        }
        U_TEST_PRINT_LINE("%d byte(s) echoed over TCP, received in %d"
                          " receive call(s).", y, count);
        if (!gSockDataCallbackCalled) {
            U_TEST_PRINT_LINE("*** WARNING *** the data callback was not"
                              " called during the test.  This can happen"
                              " legimitately if all the reads from the module"
                              " happened to coincide with data receptions and so"
                              " the URC was not involved.  However if it happens"
                              " too often something may be wrong.");
        }
        // Compare the data
        U_PORT_TEST_ASSERT(memcmp(pBuffer, gAllChars, sizeof(gAllChars)) == 0);

        // Close socket
        U_TEST_PRINT_LINE("closing sockets...");
        U_PORT_TEST_ASSERT(uCellSockClose(cellHandle, gSockHandle, NULL) == 0);

        // Deinit cell sockets
        uCellSockDeinit();

        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

        U_TEST_PRINT_LINE("disabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxDisable(cellHandle) == 0);

        U_PORT_TEST_ASSERT(gCallbackErrorNum == 0);

        // Free memory
        uPortFree(pBuffer);
    } else {
        U_TEST_PRINT_LINE("CMUX is not supported, not running tests.");
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) < 0);
    }

    gTestPassed = true;

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test MQTT over CMUX.
 */
U_PORT_TEST_FUNCTION("[cellMux]", "cellMuxMqtt")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t resourceCount;
    int32_t x;
    const char *pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MUX_TEST_MQTT_SERVER_IP_ADDRESS);
    // +1 and zero init so that we can treat it as a string
    char topic[U_CELL_INFO_IMEI_SIZE + 1] = {0};
    char *pMessageIn;
    char *pTopicStrIn;
    int32_t startTimeMs;
    size_t messageSize = sizeof(gMqttSendData) - 1;
    uCellMqttQos_t qos;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    gTestPassed = false;

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data so that we can check for CMUX support
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CMUX) &&
        uCellMqttIsSupported(cellHandle)) {

        // Get some memory to put a received MQTT message/topic in
        pMessageIn = (char *) pUPortMalloc(messageSize);
        U_PORT_TEST_ASSERT(pMessageIn != NULL);
        *pMessageIn = 0;
        pTopicStrIn = (char *) pUPortMalloc(sizeof(topic));
        U_PORT_TEST_ASSERT(pTopicStrIn != NULL);
        *pTopicStrIn = 0;

        U_TEST_PRINT_LINE("enabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);

        // Make a cellular connection
        U_PORT_TEST_ASSERT(connect(cellHandle) == 0);

        // Initialise the MQTT client.
        x = uCellMqttInit(cellHandle, pServerAddress, NULL,
# ifdef U_CELL_MUX_TEST_MQTT_USERNAME
                          U_PORT_STRINGIFY_QUOTED(U_CELL_MUX_TEST_MQTT_USERNAME),
# else
                          NULL,
# endif
# ifdef U_CELL_MUX_TEST_MQTT_PASSWORD
                          U_PORT_STRINGIFY_QUOTED(U_CELL_MUX_TEST_MQTT_PASSWORD),
# else
                          NULL,
# endif
                          NULL, false);
        U_PORT_TEST_ASSERT(x == 0);

        // Set a callback for messages arriving
        U_PORT_TEST_ASSERT(uCellMqttSetMessageCallback(cellHandle, mqttCallback,
                                                       (void *) &gMqttMessagesAvailable) == 0);

        // Connect to the MQTT broker
        U_TEST_PRINT_LINE("connecting to broker \"%s\"...", pServerAddress);
        U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

        // Get the IMEI as our unique topic name
        U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, topic) == 0);
        U_TEST_PRINT_LINE("topic name will be %s.", topic);

        U_TEST_PRINT_LINE("subscribing to topic \"%s\"...", topic);
        U_PORT_TEST_ASSERT(uCellMqttSubscribe(cellHandle, topic,
                                              U_CELL_MQTT_QOS_AT_MOST_ONCE) == 0);

        U_TEST_PRINT_LINE("publishing \"%s\" to topic \"%s\"...", gMqttSendData, topic);
        startTimeMs = uPortGetTickTimeMs();
        gMqttMessagesAvailable = 0;
        U_PORT_TEST_ASSERT(uCellMqttPublish(cellHandle, topic, gMqttSendData,
                                            sizeof(gMqttSendData) - 1,
                                            U_CELL_MQTT_QOS_AT_MOST_ONCE, false) == 0);

        // Wait for us to be notified that our new message is available on the broker
        U_TEST_PRINT_LINE("waiting %d second(s) for message to be sent back...",
                          U_CELL_MUX_TEST_MQTT_RESPONSE_TIMEOUT_MS);
        while ((gMqttMessagesAvailable == 0) &&
               (uPortGetTickTimeMs() - startTimeMs < U_CELL_MUX_TEST_MQTT_RESPONSE_TIMEOUT_MS)) {
            uPortTaskBlock(1000);
        }

        U_PORT_TEST_ASSERT(gMqttMessagesAvailable > 0);

        // Read the message
        U_PORT_TEST_ASSERT(uCellMqttGetUnread(cellHandle) > 0);
        U_PORT_TEST_ASSERT(uCellMqttMessageRead(cellHandle, pTopicStrIn, sizeof(topic),
                                                pMessageIn, &messageSize, &qos) == 0);
        U_TEST_PRINT_LINE("read message \"%.*s\" (%d character(s)) from topic \"%s\".",
                          messageSize, pMessageIn, messageSize, pTopicStrIn);

        // Disconnect
        U_TEST_PRINT_LINE("disconnecting from broker...");
        U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

        // Finally deinitialise MQTT
        uCellMqttDeinit(cellHandle);

        U_TEST_PRINT_LINE("disabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxDisable(cellHandle) == 0);

        // Free memory
        uPortFree(pMessageIn);
        uPortFree(pTopicStrIn);

    } else {
        U_TEST_PRINT_LINE("Either MQTT or CMUX are not supported, skipping...");
    }

    gTestPassed = true;

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test HTTP over CMUX.
 */
U_PORT_TEST_FUNCTION("[cellMux]", "cellMuxHttp")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t httpHandle;
    int32_t resourceCount;
    char urlBuffer[64];
    // +1 and zero init so that we can treat it as a string
    char imeiBuffer[U_CELL_INFO_IMEI_SIZE + 1] = {0};
    char pathBuffer[64];

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    gTestPassed = false;

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data so that we can check for CMUX support
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // For some reason clang complains about the two conditions in the
    // if() check below being equivalent when they really not
    // NOLINTNEXTLINE(misc-redundant-expression)
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_CMUX) &&
        U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_HTTP)) {
        // Create the complete URL from the IP address of the server
        // and the port number; testing with the domain name of the
        // server is done in the tests of u_http_client_test.c.
        snprintf(urlBuffer, sizeof(urlBuffer), "%s:%d",
                 U_HTTP_CLIENT_TEST_SERVER_IP_ADDRESS, U_HTTP_CLIENT_TEST_SERVER_PORT);

        // Use the IMEI as a "uniquifier"
        U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, imeiBuffer) == 0);

        U_TEST_PRINT_LINE("enabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxEnable(cellHandle) == 0);

        // Make a cellular connection
        U_PORT_TEST_ASSERT(connect(cellHandle) == 0);

        // Open an HTTP session with callback data storage as a parameter
        U_TEST_PRINT_LINE("HTTP test server will be %s.", urlBuffer);
        httpHandle = uCellHttpOpen(cellHandle, urlBuffer, NULL, NULL,
                                   U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                   httpCallback, (void *) &gHttpCallbackData);
        U_PORT_TEST_ASSERT(httpHandle >= 0);

        // Write our data to the file system, deleting it first as
        // as uCellFileWrite() always appends
        uCellFileDelete(cellHandle, U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME);
        U_PORT_TEST_ASSERT(uCellFileWrite(cellHandle, U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME,
                                          gAllChars, sizeof(gAllChars)) == sizeof(gAllChars));

        // PUT something
        gHttpCallbackData.pExpectedFirstLine = U_CELL_MUX_TEST_HTTP_FIRST_LINE_200_DEFAULT;
        if (pModule->moduleType == U_CELL_MODULE_TYPE_LENA_R8) {
            gHttpCallbackData.pExpectedFirstLine = U_CELL_MUX_TEST_HTTP_FIRST_LINE_200_LENA_R8;
        }
        snprintf(pathBuffer, sizeof(pathBuffer), "/%s.html", imeiBuffer);
        U_TEST_PRINT_LINE("HTTP PUT file %s from file %s in the module file system...",
                          pathBuffer, U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME);
        U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                                U_CELL_HTTP_REQUEST_PUT,
                                                pathBuffer, NULL,
                                                U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME,
                                                "application/text") == 0);
        U_PORT_TEST_ASSERT(httpWaitCheckResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                                 &gHttpCallbackData, cellHandle,
                                                 httpHandle,
                                                 U_CELL_HTTP_REQUEST_PUT, NULL));

        // GET it again
        U_TEST_PRINT_LINE("HTTP GET file %s...", pathBuffer);
        U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_GET,
                                            pathBuffer, U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME,
                                            NULL, NULL) == 0);
        U_PORT_TEST_ASSERT(httpWaitCheckResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                                 &gHttpCallbackData, cellHandle,
                                                 httpHandle,
                                                 U_CELL_HTTP_REQUEST_GET,
                                                 U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME));

        // DELETE it
        U_TEST_PRINT_LINE("HTTP DELETE file %s...", pathBuffer);
        U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                                U_CELL_HTTP_REQUEST_DELETE,
                                                pathBuffer,
                                                U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME,
                                                NULL, NULL) == 0);
        U_PORT_TEST_ASSERT(httpWaitCheckResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                                 &gHttpCallbackData, cellHandle,
                                                 httpHandle,
                                                 U_CELL_HTTP_REQUEST_DELETE,
                                                 U_CELL_MUX_TEST_HTTP_RESPONSE_FILE_NAME));

        // Close the HTTP instance once more
        uCellHttpClose(cellHandle, httpHandle);

        // Delete our data file for neatness
        uCellFileDelete(cellHandle, U_CELL_MUX_TEST_HTTP_DATA_FILE_NAME);

        U_TEST_PRINT_LINE("disabling CMUX...\n");
        U_PORT_TEST_ASSERT(uCellMuxDisable(cellHandle) == 0);

        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);
    } else {
        U_TEST_PRINT_LINE("CMUX or HTTP is not supported, not running tests.");
    }

    gTestPassed = true;

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

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
U_PORT_TEST_FUNCTION("[cellMux]", "cellMuxCleanUp")
{
    if (!gTestPassed && (gHandles.cellHandle != NULL)) {
        // If anything failed above we are likely still in CMUX
        // mode so the clean-up is to do a hard reset or
        // power-off.
# if U_CFG_APP_PIN_CELL_RESET >= 0
        uCellPwrResetHard(gHandles.cellHandle, U_CFG_APP_PIN_CELL_RESET);
# elif U_CFG_APP_PIN_CELL_PWR_ON >= 0
        uCellPwrOffHard(gHandles.cellHandle, false, NULL);
# endif
    }

    uCellTestPrivateCleanup(&gHandles);
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #if defined(U_CFG_TEST_CELL_MODULE_TYPE) && !defined(U_CFG_TEST_DISABLE_MUX)
//  !defined(U_CFG_PPP_ENABLE)

// End of file
