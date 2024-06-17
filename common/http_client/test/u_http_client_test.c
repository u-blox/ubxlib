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
 * @brief Test for the u-blox HTTP client API: these should pass on
 * all platforms that include the appropriate communications hardware,
 * and will be run for all bearers for which the network API tests have
 * configuration information, i.e. cellular or BLE/Wifi for short range.
 * These tests use the network API and the test configuration information
 * from the network API to provide the communication path.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // memset()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_short_range.h" // For uShortRangeAtClientHandleGet() and uShortRangeGetUartHandle()

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"  // For uPortUartIsRtsFlowControlEnabled()

#include "u_test_util_resource_check.h"

#include "u_timeout.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"
#include "u_http_client_test_shared_cfg.h"

#include "u_security.h"
#include "u_security_tls.h"

#include "u_http_client.h"
#include "u_device_shared.h"

// For uCellPwrReboot() and MUX
#include "u_at_client.h"
#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"
#include "u_cell_pwr.h"
#ifdef U_CELL_TEST_MUX_ALWAYS
#include "u_cell_mux.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_HTTP_CLIENT_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_HTTP_CLIENT_TEST_MAX_NUM
/** The maximum number of HTTP clients that can be active at any
 * one time (this is 4 for cellular modules and 2 for shortrange modules (default)).
 */
# define U_HTTP_CLIENT_TEST_MAX_NUM 4
# define U_HTTP_SHORT_RANGE_CLIENT_TEST_MAX_NUM 2
#endif

#ifndef U_HTTP_CLIENT_TEST_DATA_SIZE_BYTES
/** The amount of data to HTTP PUT/POST/GET; must be able to allocate
 * this much.
 */
# define U_HTTP_CLIENT_TEST_DATA_SIZE_BYTES (1024 * 5)
# define U_HTTP_CLIENT_TEST_DATA_SHORT_RANGE_SIZE_BYTES 2000
#endif

#ifndef U_HTTP_CLIENT_TEST_CONTENT_TYPE
/** The content type to use/expect when PUT/POST/HEAD/GETting our
 * test data.
 */
# define U_HTTP_CLIENT_TEST_CONTENT_TYPE "application/octet-stream"
#endif

#ifndef U_HTTP_CLIENT_TEST_CONTENT_TYPE_MIN_LENGTH_BYTES
/** The minimum expected length of a content-type string
 * returned by a GET request.
 */
# define U_HTTP_CLIENT_TEST_CONTENT_TYPE_MIN_LENGTH_BYTES 10
#endif

#ifndef U_HTTP_CLIENT_TEST_HEAD_MIN_LENGTH_BYTES
/** The minimum expected length of the headers returned
 * by a HEAD request.
 */
# define U_HTTP_CLIENT_TEST_HEAD_MIN_LENGTH_BYTES 16
#endif

#ifndef U_HTTP_CLIENT_TEST_RESPONSE_TIMEOUT_EXTRA_SECONDS
/** The amount of slack to add to the response timeout when testing.
 */
# define U_HTTP_CLIENT_TEST_RESPONSE_TIMEOUT_EXTRA_SECONDS 5
#endif

#ifndef HTTP_CLIENT_TEST_MAX_TRIES_ON_BUSY
/** How many times to try an HTTP request when error-on-busy
 * is on; this will be once per second.
 */
# define HTTP_CLIENT_TEST_MAX_TRIES_ON_BUSY (U_HTTP_CLIENT_RESPONSE_WAIT_SECONDS + \
                                              U_HTTP_CLIENT_TEST_RESPONSE_TIMEOUT_EXTRA_SECONDS)
#endif

#ifndef HTTP_CLIENT_TEST_MAX_TRIES_FLOW_CONTROL
/** How many times to try a PUT/POST operation if the response
 * appears to be truncated and this may be because RTS flow
 * control is not wired to the module.
 */
# define HTTP_CLIENT_TEST_MAX_TRIES_FLOW_CONTROL 3
#endif

#ifndef HTTP_CLIENT_TEST_MAX_TRIES_UNKNOWN
/** How many times to try an HTTP request if an unknown error
 * is returned: each HTTP request is sent on a separately
 * established TCP connection so, in effect, it is a bit
 * like UDP and needs a retry mechanism to be reliable.
 */
# define HTTP_CLIENT_TEST_MAX_TRIES_UNKNOWN 10
#endif

#ifndef HTTP_CLIENT_TEST_OVERALL_TRIES_COUNT
/** An overall guard limit for trying any given HTTP request type.
 */
# define HTTP_CLIENT_TEST_OVERALL_TRIES_COUNT 30
#endif

#ifndef HTTP_CLIENT_TEST_CHUNK_LENGTH_BYTES
/** The chunk length to use when testing with the chunked
 * API: not just using the default since that would extend
 * the test time unnecesarily.
 */
# define HTTP_CLIENT_TEST_CHUNK_LENGTH_BYTES 1024
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible HTTP request operations, used in the main
 * switch statement of the test httpClient() IN THIS ORDER.
 */
typedef enum {
    U_HTTP_CLIENT_TEST_OPERATION_PUT,
    U_HTTP_CLIENT_TEST_OPERATION_GET_PUT,
    U_HTTP_CLIENT_TEST_OPERATION_DELETE_PUT,
    U_HTTP_CLIENT_TEST_OPERATION_GET_DELETED,
    U_HTTP_CLIENT_TEST_OPERATION_POST,
    U_HTTP_CLIENT_TEST_OPERATION_HEAD,
    U_HTTP_CLIENT_TEST_OPERATION_GET_POST,
    U_HTTP_CLIENT_TEST_OPERATION_DELETE_POST,
    U_HTTP_CLIENT_TEST_OPERATION_MAX_NUM
} uHttpClientTestOperation_t;

/** Structure to contain the parameters received by the HTTP callback.
 */
typedef struct {
    bool called;
    uDeviceHandle_t devHandle;
    int32_t statusCodeOrError;
    size_t responseSize;
} uHttpClientTestCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/**  The test HTTP contexts.
 */
static uHttpClientContext_t *gpHttpContext[U_HTTP_CLIENT_TEST_MAX_NUM] = {0};

/** A place to hook the data buffer for PUT/POST.
 */
static char *gpDataBufferOut = NULL;

/** A place to hook the data buffer for GET.
 */
static char *gpDataBufferIn = NULL;

/** The amount of data pointer-to by gpDataBufferIn.
 */
static size_t gSizeDataBufferIn = 0;

/** A place to hook the buffer for content type.
 */
static char *gpContentTypeBuffer = NULL;

/** How much data we are exchanging in each HTTP transaction.
 */
static size_t gTestDataSizeBytes = 0;

/** Keep track of whether we're testing chunked or not this time.
 */
static bool gChunkedApi = false;

/** Offset into the output buffer for a chunked PUT/POST.
 */
static size_t gDataBufferOutOffset = 0;

/** Offset into the input buffer for a chunked POST/GET.
 */
static size_t gDataBufferInOffset = 0;

/** Keep track of whether the PUT/POST outgoing chunked
 * data transfer ended correctly.
 */
static bool gDataCallbackCalledWithNull = false;

/** Keep track of whether the POST/GET downlink data
 * transfer ended correctly.
 */
static bool gResponseBodyCallbackCalledWithNull = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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

// Callback in case we lose the network.
static void networkStatusCallback(uDeviceHandle_t devHandle,
                                  uNetworkType_t netType,
                                  bool isUp,
                                  uNetworkStatus_t *pStatus,
                                  void *pParameter)
{
    uNetworkTestList_t *pNetworkTestList = (uNetworkTestList_t *) pParameter;

    (void) devHandle;
    (void) netType;
    (void) pStatus;

    if ((pNetworkTestList != NULL) && !pNetworkTestList->lossOfConnection && !isUp) {
        // Just flag a loss so that the main body of the test can retry
        pNetworkTestList->lossOfConnection = true;
    }
}

// Do this before every test to ensure there is a usable network.
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Add the device for each network configuration
    // if not already added
    pList = pUNetworkTestListAlloc(uNetworkTestHasHttp);
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
#ifdef U_CELL_TEST_MUX_ALWAYS
            if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) {
                U_PORT_TEST_ASSERT(uCellMuxEnable(*pTmp->pDevHandle) == 0);
            }
#endif
        }
    }

    // Bring up each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(*pTmp->pDevHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);
        // Some modules can, occasionally, lose service, briefly,
        // during the test; capture this so that the test can recover
        U_PORT_TEST_ASSERT(uNetworkSetStatusCallback(*pTmp->pDevHandle, pTmp->networkType,
                                                     networkStatusCallback, pTmp) == 0);
    }

    // It is possible for HTTP client closure in an
    // underlying layer to have failed in a previous
    // test, leaving HTTP hanging, so just in case,
    // clear it up here
    for (size_t x = 0; x < sizeof(gpHttpContext) / sizeof(gpHttpContext[0]); x++) {
        if (gpHttpContext[x] != NULL) {
            uHttpClientClose(gpHttpContext[x]);
            gpHttpContext[x] = NULL;
        }
    }

    return pList;
}

// Callback for the non-blocking case.
static void httpCallback(uDeviceHandle_t devHandle,
                         int32_t statusCodeOrError,
                         size_t responseSize,
                         void *pResponseCallbackParam)
{
    uHttpClientTestCallback_t *pCallbackData = (uHttpClientTestCallback_t *) pResponseCallbackParam;

    if (pCallbackData != NULL) {
        pCallbackData->called = true;
        pCallbackData->devHandle = devHandle;
        pCallbackData->statusCodeOrError = statusCodeOrError;
        pCallbackData->responseSize = responseSize;
        U_TEST_PRINT_LINE("HTTP callback, response size %d.\n", responseSize);
    }
}

// Data callback for chunked PUTs/POSTs (i.e. uplink).
// pUserParam should point to gTestDataSizeBytes.
static size_t dataCallback(uDeviceHandle_t devHandle,
                           char *pData, size_t size,
                           void *pUserParam)
{
    size_t testDataSizeBytes = *(size_t *) pUserParam;
    size_t dataSize = size;

    (void) devHandle;

    if (pData != NULL) {
        if (gDataBufferOutOffset + dataSize > testDataSizeBytes) {
            dataSize = testDataSizeBytes - gDataBufferOutOffset;
        }
        memcpy(pData, gpDataBufferOut + gDataBufferOutOffset, dataSize);
        gDataBufferOutOffset += dataSize;
        U_PORT_TEST_ASSERT(gDataBufferOutOffset <= testDataSizeBytes);
        U_PORT_TEST_ASSERT(!gDataCallbackCalledWithNull);
    } else {
        gDataCallbackCalledWithNull = true;
    }

    U_PORT_TEST_ASSERT((pData != NULL) || (size == 0));

    return dataSize;
}

// Data callback for chunked POSTs/GETs (i.e. downlink).
// pUserParam should point to gSizeDataBufferIn.
static bool responseBodyCallback(uDeviceHandle_t devHandle,
                                 const char *pResponseBody,
                                 size_t size,
                                 void *pUserParam)
{
    size_t sizeDataBufferIn = *(size_t *) pUserParam;

    (void) devHandle;

    if (pResponseBody != NULL) {
        memcpy(gpDataBufferIn + gDataBufferInOffset, pResponseBody, size);
        gDataBufferInOffset += size;
        U_PORT_TEST_ASSERT(gDataBufferInOffset <= sizeDataBufferIn);
        U_PORT_TEST_ASSERT(!gResponseBodyCallbackCalledWithNull);
    } else {
        gResponseBodyCallbackCalledWithNull = true;
    }

    U_PORT_TEST_ASSERT((pResponseBody != NULL) || (size == 0));

    return true;
}

// Fill a buffer with binary 0 to 255.
static void bufferFill(char *pBuffer, size_t size)
{
    for (size_t x = 0; x < size; x++, pBuffer++) {
        *pBuffer = (char) x;
    }
}

// Check that a buffer contains binary 0 to 255, returning
// 0 or a positive number representing the point at which
// the buffer is not as expected (counting from 1).
static int32_t bufferCheck(const char *pBuffer, size_t size)
{
    size_t differentOffset = 0;

    for (size_t x = 0; (x < size) && (differentOffset == 0); x++, pBuffer++) {
        if (*pBuffer != (char) x) {
            differentOffset = x;
        }
    }

    return differentOffset;
}

// Fill a buffer with printable ASCII 32 to 126.
static void bufferFillASCII(char *pBuffer, size_t size)
{
    char c = 32;

    for (size_t x = 0; x < size; x++, pBuffer++) {
        *pBuffer = (char) c;
        c = c < 126 ? c + 1 : 32;
    }
}

// Check that a buffer contains printable ASCII 32 to 126, returning
// 0 or a positive number representing the point at which
// the buffer is not as expected (counting from 1).
static int32_t bufferCheckASCII(const char *pBuffer, size_t size)
{
    size_t differentOffset = 0;
    char c = 32;

    for (size_t x = 0; (x < size) && (differentOffset == 0); x++, pBuffer++) {
        if (*pBuffer != (char) c) {
            differentOffset = x;
        }
        c = c < 126 ? c + 1 : 32;
    }

    return differentOffset;
}

// Check the respone, including hanging around for it in the non-blocking case.
static int32_t checkResponse(uHttpClientTestOperation_t operation,
                             int32_t errorOrStatusCode,
                             uHttpClientConnection_t *pConnection,
                             const char *pResponse,
                             int32_t expectedResponseSize,
                             size_t responseSizeBlocking,
                             const char *pContentTypeBuffer,
                             volatile uHttpClientTestCallback_t *pCallbackData,
                             bool checkBinary, bool rtsFlowControlEnabled,
                             bool chunkedApi)
{
    int32_t outcome = (int32_t) U_ERROR_COMMON_SUCCESS;
    uTimeoutStart_t timeoutStart;
    int32_t x;
    const char *pTmp;
    int32_t y;
    int32_t z;
    size_t responseSize = responseSizeBlocking;
    int32_t expectedStatusCode = 200;

    if (errorOrStatusCode != U_ERROR_COMMON_NOT_SUPPORTED) {
        if (operation == U_HTTP_CLIENT_TEST_OPERATION_GET_DELETED) {
            expectedStatusCode = 404;
        }
        if ((errorOrStatusCode == (int32_t) U_ERROR_COMMON_TRUNCATED) &&
            ((operation == U_HTTP_CLIENT_TEST_OPERATION_PUT) ||
             (operation == U_HTTP_CLIENT_TEST_OPERATION_POST)) &&
            !rtsFlowControlEnabled) {
            // In the cellular case, it is possible that a PUT/POST request
            // failed at the file-write stage if flow control is not connected,
            // in which case we should retry the PUT/POST request
            U_TEST_PRINT_LINE("U_ERROR_COMMON_TRUNCATED returned by a PUT/POST"
                              " request and RTS flow control is not enabled.\n");
            // Return U_ERROR_COMMON_BAD_DATA, which we can use to trigger a
            // retry of the PUT/POST operation
            outcome = (int32_t) U_ERROR_COMMON_BAD_DATA;
        } else {
            if (pConnection->pResponseCallback != NULL) {
                // For the non-blocking case, should have an initial
                // error code of zero
                if (errorOrStatusCode == 0) {
                    timeoutStart = uTimeoutStart();
                    // Wait for twice as long as the timeout as a guard
                    U_TEST_PRINT_LINE("waiting for asynchronous response for up to"
                                      " %d second(s)...", (pConnection->timeoutSeconds * 2) +
                                      U_HTTP_CLIENT_TEST_RESPONSE_TIMEOUT_EXTRA_SECONDS);
                    while (!pCallbackData->called &&
                           !uTimeoutExpiredSeconds(timeoutStart, (pConnection->timeoutSeconds * 2) +
                                                   U_HTTP_CLIENT_TEST_RESPONSE_TIMEOUT_EXTRA_SECONDS)) {
                        uPortTaskBlock(100);
                    }

                    if (pCallbackData->called) {
                        responseSize = pCallbackData->responseSize;
                        U_TEST_PRINT_LINE("response received in %u ms.\n",
                                          uTimeoutElapsedMs(timeoutStart));
                        if (pCallbackData->statusCodeOrError != expectedStatusCode) {
                            U_TEST_PRINT_LINE("expected status code %d, got %d.\n",
                                              expectedStatusCode, pCallbackData->statusCodeOrError);
                            if (pCallbackData->statusCodeOrError < 0) {
                                // If the module reported an error, pass it back
                                // so that we may retry
                                outcome = pCallbackData->statusCodeOrError;
                            } else {
                                outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                            }
                        }
                    } else {
                        U_TEST_PRINT_LINE("callback not called after %u second(s).\n",
                                          uTimeoutElapsedSeconds(timeoutStart));
                        outcome = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    }
                } else {
                    if (pConnection->errorOnBusy && (errorOrStatusCode == (int32_t) U_ERROR_COMMON_BUSY)) {
                        U_TEST_PRINT_LINE("non-blocking case with error-on-busy, gotta try again...\n");
                        outcome = (int32_t) U_ERROR_COMMON_BUSY;
                        uPortTaskBlock(1000);
                    } else {
                        U_TEST_PRINT_LINE("non-blocking case, error-on-busy %s, expected"
                                          " uHttpClientXxxRequest() to return 0 but got %d.\n",
                                          pConnection->errorOnBusy ? "ON" : "off", errorOrStatusCode);
                        outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    }
                }
            } else {
                // For the blocking case, errorOrStatusCode should be expectedStatusCode
                if (errorOrStatusCode != expectedStatusCode) {
                    U_TEST_PRINT_LINE("expected status code %d, got %d.\n",
                                      expectedStatusCode, errorOrStatusCode);
                    if (errorOrStatusCode < 0) {
                        // If the module reported an error, pass it back
                        // so that we may retry
                        outcome = errorOrStatusCode;
                    } else {
                        outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    }
                }
            }
        }
        if (outcome == (int32_t) U_ERROR_COMMON_SUCCESS) {
            if (((operation == U_HTTP_CLIENT_TEST_OPERATION_GET_PUT) ||
                 (operation == U_HTTP_CLIENT_TEST_OPERATION_POST)) &&
                (responseSize < (size_t) expectedResponseSize) &&
                !rtsFlowControlEnabled) {
                // If we are doing a GET or a POST and the data we got back
                // is less than what we PUT/POSTed and the module is unable
                // to flow control us off then it is _possible_ that data was
                // lost between us and the module; this is different to the
                // case up at the top of this function as it can occur for
                // Wi-Fi also (where there is no file-write stage). In this
                // case, return U_ERROR_COMMON_BAD_DATA, which we can use
                // to trigger a retry of the PUT or POST operation
                U_TEST_PRINT_LINE("expected %d byte(s) of body from GET"
                                  " but got %d byte(s) (RTS flow control is"
                                  " not enabled).\n",
                                  expectedResponseSize, responseSize);
                outcome = (int32_t) U_ERROR_COMMON_BAD_DATA;
            } else {
                if (((operation == U_HTTP_CLIENT_TEST_OPERATION_GET_PUT) ||
                     (operation == U_HTTP_CLIENT_TEST_OPERATION_POST) ||
                     (operation == U_HTTP_CLIENT_TEST_OPERATION_GET_POST)) &&
                    (expectedResponseSize >= 0)) {
                    if (responseSize != (size_t) expectedResponseSize) {
                        U_TEST_PRINT_LINE("expected %d byte(s) of body from GET"
                                          " but got %d byte(s).\n",
                                          expectedResponseSize, responseSize);
                        outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    } else {
                        if (checkBinary) {
                            x = (int32_t) bufferCheck(pResponse, responseSize);
                        } else {
                            x = (int32_t) bufferCheckASCII(pResponse, responseSize);
                        }
                        if (x != 0) {
                            x--; // Since bufferCheck counts from 1
                            U_TEST_PRINT_LINE("body of GET does not match what was expected"
                                              " at offset %d:\n", x);
                            pTmp = pResponse + x - 40;
                            y = 40;
                            if (pTmp < pResponse) {
                                pTmp = pResponse;
                                y = 0;
                            }
                            z = 80;
                            if (z > ((int32_t) responseSize) - (pResponse - pTmp)) {
                                z = ((int32_t) responseSize) - (pResponse - pTmp);
                            }
                            printBuffer(pTmp, z);
                            uPortLog("\n%.*s%s\n", y, "                                        ", "^");
                            outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                        }
                        U_TEST_PRINT_LINE("%d byte(s), all good, content-type \"%s\".\n",
                                          responseSize, pContentTypeBuffer);
                        x = strlen(pContentTypeBuffer);
                        if (x < U_HTTP_CLIENT_TEST_CONTENT_TYPE_MIN_LENGTH_BYTES) {
                            U_TEST_PRINT_LINE("expected at least %d byte(s) of content type"
                                              " string but only got %d.",
                                              U_HTTP_CLIENT_TEST_CONTENT_TYPE_MIN_LENGTH_BYTES, x);
                            outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                        }
                    }
                } else if (operation == U_HTTP_CLIENT_TEST_OPERATION_HEAD) {
                    U_TEST_PRINT_LINE("HEAD returned %d byte(s):\n", responseSize);
                    printBuffer(pResponse, responseSize);
                    uPortLog("\n");
                    if (responseSize < U_HTTP_CLIENT_TEST_HEAD_MIN_LENGTH_BYTES) {
                        U_TEST_PRINT_LINE("expected at least %d byte(s) of headers"
                                          " but only got %d.",
                                          U_HTTP_CLIENT_TEST_HEAD_MIN_LENGTH_BYTES,
                                          responseSize);
                        outcome = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    }
                }
            }
            if (chunkedApi) {
                // Finally, check that the chunked API callbacks were called correctly
                if (gDataCallbackCalledWithNull) {
                    U_TEST_PRINT_LINE("dataCallback() was called with NULL",
                                      " during a chunked PUT/POST.");
                    outcome = (int32_t) U_ERROR_COMMON_BAD_DATA;
                }
                if (((operation == U_HTTP_CLIENT_TEST_OPERATION_POST) ||
                     (operation == U_HTTP_CLIENT_TEST_OPERATION_GET_PUT) ||
                     (operation == U_HTTP_CLIENT_TEST_OPERATION_GET_DELETED) ||
                     (operation == U_HTTP_CLIENT_TEST_OPERATION_GET_POST)) &&
                    !gResponseBodyCallbackCalledWithNull) {
                    U_TEST_PRINT_LINE("responseBodyCallback() was not called with",
                                      " NULL at the end of a chunked POST/GET.");
                    outcome = (int32_t) U_ERROR_COMMON_BAD_DATA;
                }
            }
        }
    }
    // Reset the callback data for next time
    memset((void *) pCallbackData, 0, sizeof(*pCallbackData));

    return outcome;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test HTTP connectivity.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[httpClient]", "httpClient")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    uHttpClientConnection_t connection = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    int32_t resourceCount = 0;
    char urlBuffer[64];
    int32_t port = U_HTTP_CLIENT_TEST_SERVER_PORT;
    char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    size_t httpClientMaxNumConn = U_HTTP_CLIENT_TEST_MAX_NUM;
    char pathBuffer[32];
    uHttpClientTestCallback_t callbackData = {0};
    int32_t errorOrStatusCode;
    int32_t requestOperation;
    int32_t outcome;
    size_t busyCount;
    size_t moduleErrorCount;
    size_t flowControlErrorCount;
    size_t tries;
    int32_t deviceType;
    bool checkBinary;
    bool rtsFlowControlEnabled;
    uAtClientHandle_t atHandle = NULL;
    uAtClientStreamHandle_t stream;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Get the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    pList = pStdPreamble();

    // Get storage for what we're going to PUT/POST/GET
    gpDataBufferOut = (char *) pUPortMalloc(U_HTTP_CLIENT_TEST_DATA_SIZE_BYTES);
    U_PORT_TEST_ASSERT(gpDataBufferOut != NULL);
    gpDataBufferIn = (char *) pUPortMalloc(U_HTTP_CLIENT_TEST_DATA_SIZE_BYTES);
    U_PORT_TEST_ASSERT(gpDataBufferIn != NULL);

    // Get storage for the content-type of a GET
    gpContentTypeBuffer = (char *) pUPortMalloc(U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpContentTypeBuffer != NULL);

    // Repeat for all bearers that support HTTP/HTTPS
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        // Get a unique number we can use to stop parallel
        // tests colliding at the HTTP server
        U_PORT_TEST_ASSERT(uSecurityGetSerialNumber(devHandle, serialNumber) > 0);
        deviceType = uDeviceGetDeviceType(devHandle);

        // Set the HTTP test data length and determine if flow control towards us is enabled
        rtsFlowControlEnabled = true;
        atHandle = NULL;
        stream.type = U_AT_CLIENT_STREAM_TYPE_NONE;
        if ((deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) ||
            (deviceType == U_DEVICE_TYPE_SHORT_RANGE)) {
            httpClientMaxNumConn = U_HTTP_SHORT_RANGE_CLIENT_TEST_MAX_NUM;
            gTestDataSizeBytes = U_HTTP_CLIENT_TEST_DATA_SHORT_RANGE_SIZE_BYTES;
#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
            uShortRangeAtClientHandleGet(devHandle, &atHandle);
#endif
        } else {
            gTestDataSizeBytes = U_HTTP_CLIENT_TEST_DATA_SIZE_BYTES;
            uCellAtClientHandleGet(devHandle, &atHandle);
        }
        if (atHandle != NULL) {
            uAtClientStreamGetExt(atHandle, &stream);
        }
        switch (stream.type) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                rtsFlowControlEnabled = uPortUartIsRtsFlowControlEnabled(stream.handle.int32);
                break;
            case U_AT_CLIENT_STREAM_TYPE_EDM:
#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
                // Can't get the UART handle from the AT handle in the EDM case but
                // can get the UART handle directly
                rtsFlowControlEnabled = uPortUartIsRtsFlowControlEnabled(uShortRangeGetUartHandle(devHandle));
#endif
                break;
            case U_AT_CLIENT_STREAM_TYPE_VIRTUAL_SERIAL:
                rtsFlowControlEnabled = stream.handle.pDeviceSerial->isRtsFlowControlEnabled(
                                            stream.handle.pDeviceSerial);
                break;
            default:
                break;
        }

        connection.maxChunkLengthBytes = HTTP_CLIENT_TEST_CHUNK_LENGTH_BYTES;

        // Repeat for HTTP and HTTPS
        for (size_t x = 0; x < 2; x++) {
            if (x == 1) {
                // Secure
                port = U_HTTP_CLIENT_TEST_SERVER_SECURE_PORT;
            } else {
                port = U_HTTP_CLIENT_TEST_SERVER_PORT;
            }
            // Create a complete URL from the domain name and port number
            snprintf(urlBuffer, sizeof(urlBuffer), "%s:%d",
                     U_HTTP_CLIENT_TEST_SERVER_DOMAIN_NAME, (int) port);
            // Configure the server in the connection
            connection.pServerName = urlBuffer;

            // Do this for as many times as we have HTTP/HTTPS instances, opening a new
            // one each time and alternating between blocking (with/without
            // errorOnBusy) and non-blocking behaviours, also between chunked
            // and non-chunked APIs, where supported
            for (size_t y = 0; y < httpClientMaxNumConn; y++) {
                connection.pResponseCallback = NULL;
                connection.pResponseCallbackParam = NULL;
                gChunkedApi = false;

                if (y % 2) {
                    // non-blocking
                    connection.pResponseCallback = httpCallback;
                    connection.pResponseCallbackParam = &callbackData;
                    // Flip between error on busy and not
                    connection.errorOnBusy = !connection.errorOnBusy;
                }

                if ((y % 3) && (deviceType == U_DEVICE_TYPE_CELL)) {
                    // Chunked API
                    gChunkedApi = true;
                    // Use one less than the previous chunk length each time,
                    // just to be awkward
                    connection.maxChunkLengthBytes--;
                }

                uPortLog(U_TEST_PREFIX "opening HTTP%s client %d of %d on %s, %sblocking",
                         x == 0 ? "" : "S", y + 1, httpClientMaxNumConn, urlBuffer,
                         (connection.pResponseCallback == NULL) ? "" : "non-");
                if ((connection.pResponseCallback != NULL) && connection.errorOnBusy) {
                    uPortLog(", error on busy");
                }
                uPortLog(".\n");

                if (x == 0) {
                    gpHttpContext[y] = pUHttpClientOpen(devHandle, &connection, NULL);
                } else {
                    gpHttpContext[y] = pUHttpClientOpen(devHandle, &connection, &tlsSettings);
                }
                if (gpHttpContext[y] == NULL) {
                    U_PORT_TEST_ASSERT(uHttpClientOpenResetLastError() == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
                } else {
                    U_PORT_TEST_ASSERT(uHttpClientOpenResetLastError() == 0);
                }

                if (gpHttpContext[y] != NULL) {
                    // Create a path
                    snprintf(pathBuffer, sizeof(pathBuffer), "/%.16s_%d_%d.html", serialNumber,
                             (int)x, (int)y);

                    // For every request operation...
                    busyCount = 0;
                    moduleErrorCount = 0;
                    requestOperation = 0;
                    flowControlErrorCount = 0;
                    while (requestOperation < (int32_t) U_HTTP_CLIENT_TEST_OPERATION_MAX_NUM) {
                        tries = 0;
                        checkBinary = true;
                        do {
                            errorOrStatusCode = 0;
                            gDataCallbackCalledWithNull = false;
                            gResponseBodyCallbackCalledWithNull = false;
                            gDataBufferOutOffset = 0;
                            gDataBufferInOffset = 0;
                            switch (requestOperation) {
                                case U_HTTP_CLIENT_TEST_OPERATION_PUT:
                                    // Fill the data buffer with data to PUT and PUT it
                                    bufferFill(gpDataBufferOut, gTestDataSizeBytes);
                                    uPortLog(U_TEST_PREFIX "PUT %d byte(s) to %s", gTestDataSizeBytes, pathBuffer);
                                    if (gChunkedApi) {
                                        uPortLog(", chunked API...\n");
                                        errorOrStatusCode = uHttpClientPutRequestChunked(gpHttpContext[y],
                                                                                         pathBuffer, dataCallback,
                                                                                         &gTestDataSizeBytes,
                                                                                         U_HTTP_CLIENT_TEST_CONTENT_TYPE);
                                    } else {
                                        uPortLog("...\n");
                                        errorOrStatusCode = uHttpClientPutRequest(gpHttpContext[y],
                                                                                  pathBuffer, gpDataBufferOut,
                                                                                  gTestDataSizeBytes,
                                                                                  U_HTTP_CLIENT_TEST_CONTENT_TYPE);
                                    }
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_GET_PUT:
                                    // Fill the data buffer and the content-type buffer
                                    // with rubbish and GET the file again
                                    memset(gpDataBufferIn, 0xFF, gTestDataSizeBytes);
                                    memset(gpContentTypeBuffer, 0xFF, U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES);
                                    gSizeDataBufferIn = gTestDataSizeBytes;
                                    uPortLog(U_TEST_PREFIX "GET of %s", pathBuffer);
                                    if (gChunkedApi) {
                                        uPortLog(", chunked API...\n");
                                        errorOrStatusCode = uHttpClientGetRequestChunked(gpHttpContext[y],
                                                                                         pathBuffer, responseBodyCallback,
                                                                                         &gSizeDataBufferIn,
                                                                                         gpContentTypeBuffer);
                                    } else {
                                        uPortLog("...\n");
                                        errorOrStatusCode = uHttpClientGetRequest(gpHttpContext[y],
                                                                                  pathBuffer, gpDataBufferIn,
                                                                                  &gSizeDataBufferIn,
                                                                                  gpContentTypeBuffer);
                                    }
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_DELETE_PUT:
                                    // DELETE it
                                    U_TEST_PRINT_LINE("DELETE %s...", pathBuffer);
                                    errorOrStatusCode = uHttpClientDeleteRequest(gpHttpContext[y], pathBuffer);
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_GET_DELETED:
                                    // Try to GET the deleted file
                                    memset(gpDataBufferIn, 0xFF, gTestDataSizeBytes);
                                    memset(gpContentTypeBuffer, 0xFF, U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES);
                                    gSizeDataBufferIn = gTestDataSizeBytes;
                                    uPortLog(U_TEST_PREFIX "GET of deleted file %s", pathBuffer);
                                    if (gChunkedApi) {
                                        uPortLog(", chunked API...\n");
                                        errorOrStatusCode = uHttpClientGetRequestChunked(gpHttpContext[y],
                                                                                         pathBuffer, responseBodyCallback,
                                                                                         &gSizeDataBufferIn,
                                                                                         gpContentTypeBuffer);
                                    } else {
                                        uPortLog("...\n");
                                        errorOrStatusCode = uHttpClientGetRequest(gpHttpContext[y],
                                                                                  pathBuffer, gpDataBufferIn,
                                                                                  &gSizeDataBufferIn,
                                                                                  gpContentTypeBuffer);
                                    }
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_POST:
                                    // Fill the data buffer with data to POST and POST it
                                    if ((deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) ||
                                        (deviceType == U_DEVICE_TYPE_SHORT_RANGE)) {
                                        bufferFillASCII(gpDataBufferOut, gTestDataSizeBytes);
                                        checkBinary = false; // only printable ASCII supported for uconnectX POST
                                    } else {
                                        bufferFill(gpDataBufferOut, gTestDataSizeBytes);
                                    }
                                    memset(gpDataBufferIn, 0xFF, gTestDataSizeBytes);
                                    memset(gpContentTypeBuffer, 0xFF, U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES);
                                    gSizeDataBufferIn = gTestDataSizeBytes;
                                    uPortLog(U_TEST_PREFIX "POST %d byte(s) to %s", gTestDataSizeBytes, pathBuffer);
                                    if (gChunkedApi) {
                                        uPortLog(", chunked API...\n");
                                        errorOrStatusCode = uHttpClientPostRequestChunked(gpHttpContext[y],
                                                                                          pathBuffer, dataCallback,
                                                                                          &gTestDataSizeBytes,
                                                                                          U_HTTP_CLIENT_TEST_CONTENT_TYPE,
                                                                                          responseBodyCallback,
                                                                                          &gSizeDataBufferIn,
                                                                                          gpContentTypeBuffer);
                                    } else {
                                        uPortLog("...\n");
                                        errorOrStatusCode = uHttpClientPostRequest(gpHttpContext[y],
                                                                                   pathBuffer, gpDataBufferOut,
                                                                                   gTestDataSizeBytes,
                                                                                   U_HTTP_CLIENT_TEST_CONTENT_TYPE,
                                                                                   gpDataBufferIn,
                                                                                   &gSizeDataBufferIn,
                                                                                   gpContentTypeBuffer);
                                    }
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_HEAD:
                                    // Fill the data buffer with rubbish and get HEAD
                                    memset(gpDataBufferIn, 0xFF, gTestDataSizeBytes);
                                    gSizeDataBufferIn = gTestDataSizeBytes;
                                    U_TEST_PRINT_LINE("HEAD of %s...", pathBuffer);
                                    errorOrStatusCode = uHttpClientHeadRequest(gpHttpContext[y],
                                                                               pathBuffer, gpDataBufferIn,
                                                                               &gSizeDataBufferIn);
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_GET_POST:
                                    // Fill the data buffer and the content-type buffer
                                    // with rubbish and GET the whole file
                                    if ((deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) ||
                                        (deviceType == U_DEVICE_TYPE_SHORT_RANGE)) {
                                        checkBinary = false;
                                    }
                                    memset(gpDataBufferIn, 0xFF, gTestDataSizeBytes);
                                    memset(gpContentTypeBuffer, 0xFF, U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES);
                                    gSizeDataBufferIn = gTestDataSizeBytes;
                                    uPortLog(U_TEST_PREFIX "GET of %s", pathBuffer);
                                    if (gChunkedApi) {
                                        uPortLog(", chunked API...\n");
                                        errorOrStatusCode = uHttpClientGetRequestChunked(gpHttpContext[y],
                                                                                         pathBuffer, responseBodyCallback,
                                                                                         &gSizeDataBufferIn,
                                                                                         gpContentTypeBuffer);
                                    } else {
                                        uPortLog("...\n");
                                        errorOrStatusCode = uHttpClientGetRequest(gpHttpContext[y],
                                                                                  pathBuffer, gpDataBufferIn,
                                                                                  &gSizeDataBufferIn,
                                                                                  gpContentTypeBuffer);
                                    }
                                    break;
                                case U_HTTP_CLIENT_TEST_OPERATION_DELETE_POST:
                                    // Finally DELETE the file again
                                    U_TEST_PRINT_LINE("DELETE of %s...", pathBuffer);
                                    errorOrStatusCode = uHttpClientDeleteRequest(gpHttpContext[y], pathBuffer);
                                    break;
                                default:
                                    break;
                            }
                            U_TEST_PRINT_LINE("result %d\n", errorOrStatusCode);
                            // Check whether it worked or not
                            outcome = checkResponse((uHttpClientTestOperation_t) requestOperation,
                                                    errorOrStatusCode, &connection,
                                                    gpDataBufferIn, gTestDataSizeBytes,
                                                    gSizeDataBufferIn, gpContentTypeBuffer,
                                                    (volatile uHttpClientTestCallback_t *) &callbackData,
                                                    checkBinary, rtsFlowControlEnabled, gChunkedApi);
                            if ((outcome == (int32_t) U_ERROR_COMMON_UNKNOWN) ||
                                (outcome == (int32_t) U_ERROR_COMMON_DEVICE_ERROR)) {
                                // U_ERROR_COMMON_UNKNOWN or U_ERROR_COMMON_DEVICE_ERROR is reported
                                // when the module indicates that the HTTP request has failed
                                // for some reason
                                moduleErrorCount++;
                            } else if (outcome == (int32_t) U_ERROR_COMMON_BUSY) {
                                // U_ERROR_COMMON_BUSY is what we get when error-on-busy
                                // is used and so we just need to retry
                                busyCount++;
                            }
                            tries++;
                            // Give the module a rest betweeen tries
                            uPortTaskBlock(1000);
                            if (outcome == (int32_t) U_ERROR_COMMON_BAD_DATA) {
                                // We got back less than we sent, or a PUT/POST request
                                // came back as truncated (cellular only), and this might
                                // be because outgoing data was lost through lack of flow
                                // control; retry the PUT or POST operation
                                if (requestOperation == U_HTTP_CLIENT_TEST_OPERATION_GET_PUT) {
                                    requestOperation = U_HTTP_CLIENT_TEST_OPERATION_PUT;
                                }
                                flowControlErrorCount++;
                                U_TEST_PRINT_LINE("*** WARNING *** retrying PUT/POST operation.");
                            } else {
                                if (pTmp->lossOfConnection ||
                                    ((outcome == (int32_t) U_ERROR_COMMON_UNKNOWN) ||
                                     (outcome == (int32_t) U_ERROR_COMMON_DEVICE_ERROR))) {
                                    // If we lost the connection, or otherwise the device didn't
                                    // behave, get it back
                                    U_TEST_PRINT_LINE("device error, recovering.");
                                    if (deviceType == U_DEVICE_TYPE_CELL) {
                                        // In the cellular case, experience suggests that
                                        // a reboot is required to make the module happy again
                                        uNetworkInterfaceDown(devHandle, pTmp->networkType);
                                        uCellPwrReboot(devHandle, NULL);
                                        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                                               pTmp->networkType,
                                                                               pTmp->pNetworkCfg) == 0);
                                        U_PORT_TEST_ASSERT(uNetworkSetStatusCallback(devHandle, pTmp->networkType,
                                                                                     networkStatusCallback, pTmp) == 0);
                                    }
                                    uHttpClientClose(gpHttpContext[y]);
                                    gpHttpContext[y] = NULL;
                                    if (x == 0) {
                                        gpHttpContext[y] = pUHttpClientOpen(devHandle, &connection, NULL);
                                    } else {
                                        gpHttpContext[y] = pUHttpClientOpen(devHandle, &connection, &tlsSettings);
                                    }
                                    pTmp->lossOfConnection = false;
                                }
                            }
                        } while ((outcome < 0) && (outcome != (int32_t) U_ERROR_COMMON_BAD_DATA) &&
                                 (flowControlErrorCount < HTTP_CLIENT_TEST_MAX_TRIES_FLOW_CONTROL) &&
                                 (moduleErrorCount < HTTP_CLIENT_TEST_MAX_TRIES_UNKNOWN) &&
                                 (busyCount < HTTP_CLIENT_TEST_MAX_TRIES_ON_BUSY) &&
                                 (tries < HTTP_CLIENT_TEST_OVERALL_TRIES_COUNT));
                        if (outcome != (int32_t) U_ERROR_COMMON_BAD_DATA) {
                            // If the outgoing data was not bad then we can move
                            // on to the next operation
                            requestOperation++;
                            U_PORT_TEST_ASSERT(outcome == 0);
                        } else {
                            // If the outgoing data was thought to be bad due to lack
                            // of flow control, make sure we have enough tries left
                            U_PORT_TEST_ASSERT(flowControlErrorCount  < HTTP_CLIENT_TEST_MAX_TRIES_FLOW_CONTROL);
                        }
                    }  // while (requestOperation < U_HTTP_CLIENT_TEST_OPERATION_MAX_NUM)
                } else { // if (gpHttpContext[y] != NULL)
                    U_TEST_PRINT_LINE("device does not support HTTP%sclient, not testing it.", x == 0 ? " " : "S ");
                }
            } // for (HTTP/HTTPS instance)

            U_TEST_PRINT_LINE("closing HTTP instances...");
            for (size_t y = 0; y < httpClientMaxNumConn; y++) {
                uHttpClientClose(gpHttpContext[y]);
                gpHttpContext[y] = NULL;
            }
        } // for (HTTP and HTTPS)
    }

    // Free memory
    uPortFree(gpDataBufferOut);
    gpDataBufferOut = NULL;
    uPortFree(gpDataBufferIn);
    gpDataBufferIn = NULL;
    uPortFree(gpContentTypeBuffer);
    gpContentTypeBuffer = NULL;

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();
    // Clean-up TLS security mutex; an application wouldn't normally,
    // do this, we only do it here to make the sums add up
    uSecurityTlsCleanUp();
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
U_PORT_TEST_FUNCTION("[httpClient]", "httpClientCleanUp")
{
    U_TEST_PRINT_LINE("cleaning up any outstanding resources.\n");

    for (size_t x = 0; x < sizeof(gpHttpContext) / sizeof(gpHttpContext[0]); x++) {
        uHttpClientClose(gpHttpContext[x]);
    }

    uPortFree(gpDataBufferOut);
    uPortFree(gpDataBufferIn);
    uPortFree(gpContentTypeBuffer);

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    // Clean-up TLS security mutex; an application wouldn't normally,
    // do this, we only do it here to make the sums add up
    uSecurityTlsCleanUp();
    uDeviceDeinit();
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

// End of file
