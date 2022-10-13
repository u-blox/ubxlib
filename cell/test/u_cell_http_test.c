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
 * @brief Tests for the configuration calls of the cellular HTTP API.
 * These test should pass on all platforms that have a cellular module
 * connected to them.  They are only compiled if U_CFG_TEST_CELL_MODULE_TYPE
 * is defined.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

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
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_info.h"    // For uCellInfoGetImei()
#include "u_cell_http.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"
#include "u_http_client_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_HTTP_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_HTTP_TEST_RESPONSE_FILE_NAME
/** Name to use when giving an explicit response file name.
 */
# define  U_CELL_HTTP_TEST_RESPONSE_FILE_NAME "ubxlib_test_http_response"
#endif

#ifndef U_CELL_HTTP_TEST_DATA_FILE_NAME
/** File name to use when PUT/POSTing data from file.
 */
# define  U_CELL_HTTP_TEST_DATA_FILE_NAME "ubxlib_test_http_putpost"
#endif

/** The first line of an HTTP response indicating success.
 */
#define U_CELL_HTTP_TEST_FIRST_LINE_200 "HTTP/1.0 200 OK"

/** The first line of an HTTP response indicating delete failure.
 */
#define U_CELL_HTTP_TEST_FIRST_LINE_404 "HTTP/1.0 404 Not Found"

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
} uCellHttpTestCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** Data to send over HTTP; all printable characters except double-quotes.
 */
static const char gSendData[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789!#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

/** Data to send over HTTP via file; all printable characters WITH double-quotes.
 */
static const char gSendDataFile[] = "\"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789!#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

/** Storage for data seen by the HTTP callback().
 */
static volatile uCellHttpTestCallback_t gCallbackData = {0};

/** The possible HTTP request types as strings.
 */
static const char *gpRequestTypeStr[] = {"HEAD",   // U_CELL_HTTP_REQUEST_HEAD
                                         "GET",    // U_CELL_HTTP_REQUEST_GET
                                         "DELETE", // U_CELL_HTTP_REQUEST_DELETE
                                         "PUT",    // U_CELL_HTTP_REQUEST_PUT
                                         "POST"    // U_CELL_HTTP_REQUEST_POST
                                        };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Return a string for the given HTTP request type.
static const char *pHttpRequestTypeStr(uCellHttpRequest_t requestType)
{
    const char *pStr = "unknown";

    if ((requestType >= 0) && (requestType < sizeof(gpRequestTypeStr) / sizeof(gpRequestTypeStr[0]))) {
        pStr = gpRequestTypeStr[requestType];
    }

    return pStr;
}


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

// Compare the contents of a file in the cellular module's
// file system with the given string
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
        pFileContents = (char *) malloc(fileSize);
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
            free(pFileContents);
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
static void callback(uDeviceHandle_t cellHandle, int32_t httpHandle,
                     uCellHttpRequest_t requestType, bool error,
                     const char *pFileNameResponse, void *pCallbackParam)
{
    uCellHttpTestCallback_t *pCallbackData = (uCellHttpTestCallback_t *) pCallbackParam;

    pCallbackData->cellHandle = cellHandle;
    pCallbackData->httpHandle = httpHandle;
    pCallbackData->requestType = requestType;
    pCallbackData->error = error;
    strncpy(pCallbackData->fileNameResponse, pFileNameResponse,
            sizeof(pCallbackData->fileNameResponse));
    pCallbackData->contentsMismatch = !checkFile(cellHandle,
                                                 pFileNameResponse,
                                                 pCallbackData->pExpectedFirstLine,
                                                 false);
    pCallbackData->called = true;
}

// Check an HTTP response, return true if it is good, else false.
static bool waitCheckHttpResponse(int32_t timeoutSeconds,
                                  volatile uCellHttpTestCallback_t *pCallbackData,
                                  uDeviceHandle_t cellHandle, int32_t httpHandle,
                                  uCellHttpRequest_t requestType,
                                  const char *pFileNameResponse)
{
    bool isOk = false;
    int32_t startTimeMs = uPortGetTickTimeMs();

    U_TEST_PRINT_LINE("waiting up to %d second(s) for response to request type %s...",
                      timeoutSeconds, pHttpRequestTypeStr(requestType));
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
            U_TEST_PRINT_LINE("expected response type %s, got %s (%d).",
                              pHttpRequestTypeStr(requestType),
                              pHttpRequestTypeStr(pCallbackData->requestType),
                              pCallbackData->requestType);
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

/** A test of the cellular HTTP API.  This test doesn't do a lot
 * of "thrashing" of the API, and doesn't test HTTPS; that's done
 * in the testing over in u_http_client_test.c.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellHttp]", "cellHttp")
{
    uDeviceHandle_t cellHandle;
    int32_t httpHandle;
    int32_t y;
    int32_t heapUsed;
    char urlBuffer[64];
    char imeiBuffer[U_CELL_INFO_IMEI_SIZE + 1];
    char pathBuffer[64];

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Create the complete URL from the IP address of the server
    // and the port number; testing with the domain name of the
    // server is done in the tests of u_http_client_test.c.
    snprintf(urlBuffer, sizeof(urlBuffer), "%s:%d",
             U_HTTP_CLIENT_TEST_SERVER_IP_ADDRESS, U_HTTP_CLIENT_TEST_SERVER_PORT);

    // Use the cellular module's IMEI as a "uniquifier" to avoid
    // collisions with other devices using the same HTTP test server
    U_PORT_TEST_ASSERT(uCellInfoGetImei(cellHandle, imeiBuffer) == 0);
    imeiBuffer[sizeof(imeiBuffer) - 1] = 0;

    // Make a cellular connection, since we will need to do a
    // DNS look-up on the HTTP server domain name
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

    // Try using NULL parameters where they are not permitted in the open() call
    U_PORT_TEST_ASSERT(uCellHttpOpen(cellHandle, NULL, NULL, NULL,
                                     U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                     callback, NULL) < 0);
    U_PORT_TEST_ASSERT(uCellHttpOpen(cellHandle, urlBuffer, NULL, NULL,
                                     U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                     NULL, NULL) < 0);
    U_PORT_TEST_ASSERT(uCellHttpOpen(cellHandle, urlBuffer, NULL, "pw",
                                     U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                     callback, NULL) < 0);

    // Now do it properly, and give it a pointer to the
    // callback data storage as a parameter
    U_TEST_PRINT_LINE("HTTP test server will be %s.", urlBuffer);
    httpHandle = uCellHttpOpen(cellHandle, urlBuffer, NULL, NULL,
                               U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                               callback, (void *) &gCallbackData);
    U_PORT_TEST_ASSERT(httpHandle >= 0);
    U_PORT_TEST_ASSERT(!uCellHttpIsSecured(cellHandle, httpHandle, NULL));

    // Note: we don't test with HTTPS here, that's done when the
    // code is tested from the common HTTP Client level.

    // POST something
    gCallbackData.pExpectedFirstLine = U_CELL_HTTP_TEST_FIRST_LINE_200;
    snprintf(pathBuffer, sizeof(pathBuffer), "/%s.html", imeiBuffer);
    U_TEST_PRINT_LINE("HTTP POST file %s containing string \"%s\"...",
                      pathBuffer, gSendData);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_POST,
                                        pathBuffer, NULL, gSendData,
                                        "application/text") == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_POST, NULL));

    // GET it
    U_TEST_PRINT_LINE("HTTP GET file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_GET,
                                        pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET, NULL));

    // GET it again but using an explicit response file name this time
    U_TEST_PRINT_LINE("HTTP GET file %s again...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_GET,
                                        pathBuffer,
                                        U_CELL_HTTP_TEST_RESPONSE_FILE_NAME,
                                        NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET,
                                             U_CELL_HTTP_TEST_RESPONSE_FILE_NAME));

    // GET just the headers
    U_TEST_PRINT_LINE("HTTP HEAD for file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_HEAD,
                                        pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_HEAD, NULL));

    // DELETE it
    U_TEST_PRINT_LINE("HTTP DELETE file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_DELETE,
                                        pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_DELETE, NULL));

    // Try to GET it again
    gCallbackData.pExpectedFirstLine = U_CELL_HTTP_TEST_FIRST_LINE_404;
    U_TEST_PRINT_LINE("HTTP GET deleted file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_GET,
                                        pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET, NULL));

    // Now call uCellHttpRequestFile() with the various request types

    // First, write our data to the file system; delete it first as
    // as uCellFileWrite() always appends
    uCellFileDelete(cellHandle, U_CELL_HTTP_TEST_DATA_FILE_NAME);
    U_PORT_TEST_ASSERT(uCellFileWrite(cellHandle, U_CELL_HTTP_TEST_DATA_FILE_NAME,
                                      gSendDataFile, sizeof(gSendDataFile)) == sizeof(gSendDataFile));

    // PUT something
    gCallbackData.pExpectedFirstLine = U_CELL_HTTP_TEST_FIRST_LINE_200;
    U_TEST_PRINT_LINE("HTTP PUT file %s from file %s in the module file system...",
                      pathBuffer, U_CELL_HTTP_TEST_DATA_FILE_NAME);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_PUT,
                                            pathBuffer, NULL,
                                            U_CELL_HTTP_TEST_DATA_FILE_NAME,
                                            "application/text") == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_PUT, NULL));

    // GET it, using uCellHttpRequest() and with an explicit response file name
    U_TEST_PRINT_LINE("HTTP GET file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequest(cellHandle, httpHandle,
                                        U_CELL_HTTP_REQUEST_GET,
                                        pathBuffer, U_CELL_HTTP_TEST_RESPONSE_FILE_NAME,
                                        NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET,
                                             U_CELL_HTTP_TEST_RESPONSE_FILE_NAME));

    // POST something
    U_TEST_PRINT_LINE("HTTP POST file %s from file %s in the module file system...",
                      pathBuffer, U_CELL_HTTP_TEST_DATA_FILE_NAME);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_POST,
                                            pathBuffer, NULL,
                                            U_CELL_HTTP_TEST_DATA_FILE_NAME,
                                            "application/text") == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_POST, NULL));

    // GET it, with uCellHttpRequestFile() and an explicit response file name
    U_TEST_PRINT_LINE("HTTP GET file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_GET,
                                            pathBuffer,
                                            U_CELL_HTTP_TEST_RESPONSE_FILE_NAME,
                                            NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET,
                                             U_CELL_HTTP_TEST_RESPONSE_FILE_NAME));

    // GET just the headers
    U_TEST_PRINT_LINE("HTTP HEAD for file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_HEAD,
                                            pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_HEAD, NULL));

    // DELETE it, with an explicit response file name again
    U_TEST_PRINT_LINE("HTTP DELETE file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_DELETE,
                                            pathBuffer,
                                            U_CELL_HTTP_TEST_RESPONSE_FILE_NAME,
                                            NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_DELETE,
                                             U_CELL_HTTP_TEST_RESPONSE_FILE_NAME));

    // Try to GET it again
    gCallbackData.pExpectedFirstLine = U_CELL_HTTP_TEST_FIRST_LINE_404;
    U_TEST_PRINT_LINE("HTTP GET deleted file %s...", pathBuffer);
    U_PORT_TEST_ASSERT(uCellHttpRequestFile(cellHandle, httpHandle,
                                            U_CELL_HTTP_REQUEST_GET,
                                            pathBuffer, NULL, NULL, NULL) == 0);
    U_PORT_TEST_ASSERT(waitCheckHttpResponse(U_CELL_HTTP_TIMEOUT_SECONDS_MIN,
                                             &gCallbackData, cellHandle,
                                             httpHandle,
                                             U_CELL_HTTP_REQUEST_GET, NULL));

    // Obtain the last error code - there's no way to check its validity
    // since it is utterly module-specific, just really checking that it
    // doesn't bring the roof down
    uCellHttpGetLastErrorCode(cellHandle, httpHandle);

    // Close the HTTP instance once more
    uCellHttpClose(cellHandle, httpHandle);

    // Delete our data file for neatness
    uCellFileDelete(cellHandle, U_CELL_HTTP_TEST_DATA_FILE_NAME);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellHttp]", "cellHttpCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
