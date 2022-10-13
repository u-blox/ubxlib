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
 * @brief Implementation of the u-blox HTTP client API.  These
 * functions are thread-safe except that uHttpClientClose() must
 * NOT be called while a uHttpClientXxxRequest function is still
 * waiting for a response.
 *
 * This implementation expects to call on the underlying APIs
 * of cellular or Wifi for the functions that meet the HTTP
 * client API.  Note that the these underlying APIs are all
 * "one in one out", i.e. when a HTTP request has been
 * initiated the caller has to wait for either the response
 * or a timeout before issuing the next HTTP request, otherwise
 * the underlying layer will return an error; the design here
 * takes that behaviour into account.
 *
 * IMPORTANT: parameters will be error checked before the
 * underlying APIs are called *EXCEPT* for lengths, since
 * these are generally module specific.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strstr()/memcmp()/strncpy()/strlen()/strtol()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_assert.h"

#include "u_at_client.h"

#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"
#include "u_cell_sec_tls.h"
#include "u_cell_http.h"

#include "u_http_client.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Helper macro to make sure that both the entry and exit functions
 * are called.
 */
#define U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, pErrorCode, alwaysWait)  \
                                            { entryFunctionRequest(pContext,    \
                                                                   pErrorCode,  \
                                                                   alwaysWait)

/** Helper macro to make sure that both the entry and exit functions
 * are called.
 */
#define U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode) }        \
                                            exitFunctionRequest(pContext, \
                                                                errorCode)

#ifndef U_HTTP_CLIENT_ADDITIONAL_TIMEOUT_SECONDS
/** The underlying layer/module should do the timeout however we also
 * run a local timeout, just in case, but with an additional guard time
 * of this many seconds.
 */
# define U_HTTP_CLIENT_ADDITIONAL_TIMEOUT_SECONDS  5
#endif

/** The length of buffer to use for the cellular case to store
 * a file name used in PUT/POST operations.  This buffer is populated
 * by cellPutPost(); must include room for a null terminator.
 */
#define U_HTTP_CLIENT_CELL_FILE_NAME_BUFFER_LENGTH 32

#ifndef U_HTTP_CLIENT_CELL_FILE_CHUNK_LENGTH
/** The maximum length of data to read or write from/to a file
 * (i.e. in the cellular case) at any one time; if you have
 * a really reliable UART link with solid handshaking you
 * can probably increase this, but bear in mind that the
 * cellular module can only write to flash so fast.
 */
# define U_HTTP_CLIENT_CELL_FILE_CHUNK_LENGTH 1024
#endif

/** The maximum length of the first line of an HTTP response.
 */
#define U_HTTP_CLIENT_CELL_FILE_READ_FIRST_LINE_LENGTH 64

/** The maximum length of the headers of an HTTP response.
 */
#define U_HTTP_CLIENT_CELL_FILE_READ_HEADERS_LENGTH 1024

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Private context structure for HTTP, cellular-flavour.
 */
typedef struct {
    int32_t httpHandle;
} uHttpClientContextCell_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The last error code from pUHttpClientOpen()
 */
static uErrorCode_t gLastOpenError = U_ERROR_COMMON_SUCCESS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CELLULAR SPECIFIC
 * -------------------------------------------------------------- */

// Read the start of a response file to find the HTTP status code.
static int32_t cellFileResponseReadStatusCode(uDeviceHandle_t cellHandle,
                                              const char *pFileNameResponse,
                                              size_t *pOffset)
{
    int32_t errorOrStatusCode;
    // +1 in order that this can be made into a string
    char responseBuffer[U_HTTP_CLIENT_CELL_FILE_READ_FIRST_LINE_LENGTH + 1];
    char *pSave;
    char *pTmp;

    // Read enough of the response file to get the status code
    errorOrStatusCode = uCellFileBlockRead(cellHandle, pFileNameResponse,
                                           responseBuffer, 0,
                                           // -1 so that we can make it a string
                                           sizeof(responseBuffer) - 1);
    if (errorOrStatusCode >= 0) {
        // Make it a string
        responseBuffer[errorOrStatusCode] = 0;

        // What we expect to get is something like:
        // "HTTP/1.0 200 OK\r\nAccept-Ranges: ..."

        // Before we modify the buffer by tokenising it,
        // find the end of the line and put it in pOffset
        if (pOffset != NULL) {
            // As a bonus find the end of line and
            // put the distance to it into pOffset
            *pOffset = 0;
            pTmp = strstr(responseBuffer, "\r\n");
            if (pTmp != NULL) {
                *pOffset = (pTmp - responseBuffer) + 2; // +2 for \r\n
            }
        }

        // Now tokenise on space
        pTmp = strtok_r(responseBuffer, " ", &pSave);
        // Confirm that there's an HTTP at the start
        if ((pTmp != NULL) && (strlen(pTmp) >= 4) && (memcmp(pTmp, "HTTP", 4) == 0)) {
            // The 3-digit status code should be next
            pTmp = strtok_r(NULL, " ", &pSave);
            if ((pTmp != NULL) && (strlen(pTmp) >= 3)) {
                errorOrStatusCode = strtol(pTmp, &pTmp, 10);
            }
        }
    }

    return errorOrStatusCode;
}

// Read the header from the response file, starting at offset,
// and deriving the content type on the way if required.
static int32_t cellFileResponseReadHead(uDeviceHandle_t cellHandle,
                                        const char *pFileNameResponse,
                                        size_t offset,
                                        char *pResponseHeader,
                                        size_t responseHeaderSize,
                                        char *pContentType)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    char *pBuffer = pResponseHeader;
    size_t size = responseHeaderSize;
    bool malloced = false;
    char *pTmp;
    char *pEnd;
    size_t sizeContentType;

    if (pBuffer == NULL) {
        size = U_HTTP_CLIENT_CELL_FILE_READ_HEADERS_LENGTH;
        pBuffer = (char *) malloc(size + 1); // +1 to add a terminator
        malloced = true;
    }

    if (pBuffer != NULL) {
        errorCodeOrSize = uCellFileBlockRead(cellHandle, pFileNameResponse,
                                             pBuffer, offset, size);
        if ((errorCodeOrSize >= 0) && malloced) {
            // Need to be able to treat pBuffer as a string for
            // the rest of this
            *(pBuffer + errorCodeOrSize) = 0;
            // Find the end of the headers region, which is marked by
            // a blank line
            pTmp = strstr(pBuffer, "\r\n\r\n");
            if (pTmp != NULL) {
                errorCodeOrSize = pTmp - pBuffer;
            }
            if (pContentType != NULL) {
                *pContentType = 0;
                // Find the content type in the headers
                pTmp = strstr(pBuffer, "Content-Type:");
                if (pTmp != NULL) {
                    pTmp += 13; // For "Content-Type:"
                    // Need to turn this into a string of max length
                    // U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES - 1
                    // First, remove any initial spaces
                    while (*pTmp == ' ') {
                        pTmp++;
                    }
                    // Assuming the end of the content-type text is the end
                    // of the headers, then work out the size
                    sizeContentType = errorCodeOrSize - (pTmp - pBuffer);
                    // See if there is actually a line-end on the content-type
                    pEnd = strstr(pTmp, "\r\n");
                    if (pEnd != NULL) {
                        sizeContentType = pEnd - pTmp;
                    }
                    // Populate pContentType
                    if (sizeContentType > U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES - 1) {
                        sizeContentType = U_HTTP_CLIENT_CONTENT_TYPE_LENGTH_BYTES - 1;
                    }
                    memcpy(pContentType, pTmp, sizeContentType);
                    // Add a terminator
                    *(pContentType + sizeContentType) = 0;
                }
            }
        }

        if (malloced) {
            // Free memory
            free(pBuffer);
        }
    }

    return errorCodeOrSize;
}

// Callback for HTTP responses in the cellular case.
static void cellCallback(uDeviceHandle_t cellHandle, int32_t httpHandle,
                         uCellHttpRequest_t requestType, bool error,
                         const char *pFileNameResponse, void *pCallbackParam)
{
    int32_t statusCodeOrError = (int32_t) U_ERROR_COMMON_UNKNOWN;
    uHttpClientContext_t *pContext = (uHttpClientContext_t *) pCallbackParam;
    uAtClientHandle_t atHandle = NULL;
    bool atPrintOn = false;
    size_t offset = 0;
    int32_t responseSize = 0;
    int32_t thisSize = 0;
    int32_t totalSize = 0;

    (void) httpHandle;

    if (pContext != NULL) {
        if (!error) {
            if (uCellAtClientHandleGet(pContext->devHandle, &atHandle) == 0) {
                // Switch AT printing off for this as it is quite a load
                atPrintOn = uAtClientPrintAtGet(atHandle);
                uAtClientPrintAtSet(atHandle, false);
            }

            // Read the status code from the file
            statusCodeOrError = cellFileResponseReadStatusCode(cellHandle,
                                                               pFileNameResponse,
                                                               &offset);
            if (statusCodeOrError >= 0) {
                // Read data from the response file, where required
                if ((pContext->pResponse != NULL) &&
                    (pContext->pResponseSize != NULL) &&
                    (*pContext->pResponseSize > 0)) {
                    switch (requestType) {
                        case U_CELL_HTTP_REQUEST_HEAD:
                            responseSize = cellFileResponseReadHead(cellHandle,
                                                                    pFileNameResponse,
                                                                    offset,
                                                                    pContext->pResponse,
                                                                    *pContext->pResponseSize,
                                                                    NULL);
                            break;
                        case U_CELL_HTTP_REQUEST_GET:
                            // Read the headers to get the content type and allow
                            // us to work out the offset to the start of the body
                            responseSize = cellFileResponseReadHead(cellHandle,
                                                                    pFileNameResponse,
                                                                    offset, NULL, 0,
                                                                    pContext->pContentType);
                            if (responseSize >= 0) {
                                // Read the body, starting from the end of the headers
                                // It _should_ be possible to read this all at once,
                                // however it puts some stress on the AT interface
                                // and so here we chunk it.
                                offset += (size_t) responseSize + 4; // +4 for "\r\n\r\n"
                                do {
                                    thisSize = U_HTTP_CLIENT_CELL_FILE_CHUNK_LENGTH;
                                    if (thisSize > ((int32_t) *pContext->pResponseSize) - totalSize) {
                                        thisSize = ((int32_t) * pContext->pResponseSize);
                                    }
                                    if (thisSize > 0) {
                                        thisSize = uCellFileBlockRead(cellHandle,
                                                                      pFileNameResponse,
                                                                      pContext->pResponse + totalSize,
                                                                      offset + totalSize,
                                                                      thisSize);
                                        if (thisSize > 0) {
                                            totalSize += thisSize;
                                        }
                                    }
                                } while (thisSize > 0);
                                responseSize = totalSize;
                            }
                            break;
                        default:
                            break;
                    }
                    if (responseSize < 0) {
                        // There's no way to pass back a file read error
                        // (better to pass back the valid HTTP status code)
                        // so just indicate zero length
                        responseSize = 0;
                    }
                    *pContext->pResponseSize = (size_t) responseSize;
                }
            }

            if (atPrintOn) {
                uAtClientPrintAtSet(atHandle, true);
            }
        }

        // Call the callback, if required
        if (pContext->pResponseCallback != NULL) {
            pContext->pResponseCallback(cellHandle,
                                        statusCodeOrError,
                                        (size_t) responseSize,
                                        pContext->pResponseCallbackParam);
        }

        // Set the status code for block() to read if required and
        // give the semaphore back
        pContext->statusCodeOrError = statusCodeOrError;
        uPortSemaphoreGive((uPortSemaphoreHandle_t) pContext->semaphoreHandle);
    }
}

// Do the cellular-specific bits of opening an HTTP instance.
static int32_t cellOpen(uHttpClientContext_t *pContext,
                        const uHttpClientConnection_t *pConnection)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uHttpClientContextCell_t *pContextCell;

    pContext->pPriv = malloc(sizeof(uHttpClientContextCell_t));
    if (pContext->pPriv != NULL) {
        pContextCell = (uHttpClientContextCell_t *) pContext->pPriv;
        memset(pContextCell, 0, sizeof(*pContextCell));
        pContextCell->httpHandle = uCellHttpOpen(pContext->devHandle,
                                                 pConnection->pServerName,
                                                 pConnection->pUserName,
                                                 pConnection->pPassword,
                                                 pConnection->timeoutSeconds,
                                                 cellCallback,
                                                 (void *) pContext);
        if (pContextCell->httpHandle < 0) {
            errorCode = (uErrorCode_t) pContextCell->httpHandle;
            free(pContext->pPriv);
        } else {
            if (pContext->pSecurityContext != NULL) {
                errorCode = (uErrorCode_t) uCellHttpSetSecurityOn(pContext->devHandle,
                                                                  pContextCell->httpHandle,
                                                                  ((uCellSecTlsContext_t *) (pContext->pSecurityContext->pNetworkSpecific))->profileId);
            } else {
                errorCode = (uErrorCode_t) uCellHttpSetSecurityOff(pContext->devHandle,
                                                                   pContextCell->httpHandle);
            }
            if (errorCode < 0) {
                // Clean up on error
                uCellHttpClose(pContext->devHandle, pContextCell->httpHandle);
                free(pContext->pPriv);
            }
        }
    }

    return errorCode;
}

// Do the cellular-specific bits of closing an HTTP instance.
static void cellClose(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    uCellHttpClose(cellHandle, httpHandle);
    void *pReentrant;
    char buffer[U_HTTP_CLIENT_CELL_FILE_NAME_BUFFER_LENGTH];
    char *pFileName;

    // Clear out any files from PUT/POST operations
    pFileName = (char *) malloc(U_CELL_FILE_NAME_MAX_LENGTH + 1);
    if (pFileName != NULL) {
        // The thing to match with: any files from this httpHandle
        snprintf(buffer, sizeof(buffer), "%s%d",
                 U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX, (int) httpHandle);
        // Read through all the files in the file system checking
        // if they start with our HTTP instance file name
        for (int32_t numFiles = uCellFileListFirst_r(cellHandle,
                                                     pFileName,
                                                     &pReentrant);
             numFiles >= 0;
             numFiles = uCellFileListNext_r(pFileName, &pReentrant)) {
            if (strstr(pFileName, buffer) == pFileName) {
                // It is one of ours: delete it
                uCellFileDelete(cellHandle, pFileName);
            }
        }
        uCellFileListLast_r(&pReentrant);
        free(pFileName);
    }
}

// Perform PUT or POST requests, cellular style.
static int32_t cellPutPost(uDeviceHandle_t devHandle, int32_t httpHandle,
                           uCellHttpRequest_t requestType,
                           const char *pPath, const char *pData,
                           size_t size, const char *pContentType)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    char fileName[U_HTTP_CLIENT_CELL_FILE_NAME_BUFFER_LENGTH];
    uAtClientHandle_t atHandle = NULL;
    bool atPrintOn = false;
    int32_t thisSize = 0;

    // If you change the string here, you may need to change the one
    // in cellClose() to match
    snprintf(fileName, sizeof(fileName),
             "%s%d_%s", U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX,
             (int) httpHandle,  "putpost");
    if (uCellAtClientHandleGet(devHandle, &atHandle) == 0) {
        // Switch AT printing off for this as it is quite a load
        atPrintOn = uAtClientPrintAtGet(atHandle);
        uAtClientPrintAtSet(atHandle, false);
    }
    // Always delete first as uCellFileWrite() appends
    uCellFileDelete(devHandle, fileName);
    // Write in chunks so as not to stress the capabilities
    // of the UART or the flash-write speed of the module
    while ((size > 0) && (thisSize >= 0)) {
        thisSize = U_HTTP_CLIENT_CELL_FILE_CHUNK_LENGTH;
        if (thisSize > (int32_t) size) {
            thisSize = (int32_t) size;
        }
        thisSize = uCellFileWrite(devHandle, fileName, pData, thisSize);
        if (thisSize > 0) {
            pData += thisSize;
            size -= thisSize;
        }
    }
    if (thisSize < 0) {
        errorCode = thisSize;
    }
    if (atPrintOn) {
        uAtClientPrintAtSet(atHandle, true);
    }
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        errorCode = uCellHttpRequestFile(devHandle, httpHandle,
                                         requestType, pPath,
                                         NULL, fileName,
                                         pContentType);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: GENERAL
 * -------------------------------------------------------------- */

// Clear out user pointers etc. from the last request and give the
// semaphore back.
static void clearLastRequest(uHttpClientContext_t *pContext)
{
    pContext->pResponse = NULL;
    pContext->pResponseSize = NULL;
    pContext->pContentType = NULL;
    pContext->lastRequestTimeMs = -1;
    pContext->statusCodeOrError = 0;
    uPortSemaphoreGive((uPortSemaphoreHandle_t) pContext->semaphoreHandle);
}

// MUST be called at the start of every uHttpClientXxxRequest() function;
// use the helper macro U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION to be
// sure of this, rather than calling this function directly.
static void entryFunctionRequest(uHttpClientContext_t *pContext, int32_t *pErrorCode,
                                 bool alwaysWait)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t waitTimeMs = 0;
    int32_t timeoutMs;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_BUSY;
        // Work out how long we've been waiting
        if (pContext->lastRequestTimeMs >= 0) {
            waitTimeMs = uPortGetTickTimeMs() - pContext->lastRequestTimeMs;
            timeoutMs = (pContext->timeoutSeconds + U_HTTP_CLIENT_ADDITIONAL_TIMEOUT_SECONDS) * 1000;
            if (waitTimeMs >= 0) {
                if (waitTimeMs < timeoutMs) {
                    waitTimeMs = timeoutMs - waitTimeMs;
                } else {
                    // The previous request has taken too long, reset it
                    clearLastRequest(pContext);
                    waitTimeMs = 0;
                }
            } else {
                // Handle wrap
                waitTimeMs = timeoutMs;
            }
        }
        // We now have the time we are to wait in waitTimeMs,
        // see if we actually want to wait
        if ((waitTimeMs == 0) || !pContext->errorOnBusy || alwaysWait) {
            // Either we're waiting or there was nothing to wait for
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (uPortSemaphoreTryTake((uPortSemaphoreHandle_t) pContext->semaphoreHandle, waitTimeMs) != 0) {
                // The current request has taken too long, reset it
                // and carry on with this one
                clearLastRequest(pContext);
            }
        }
    }

    if (pErrorCode != NULL) {
        *pErrorCode = errorCode;
    }
}

// MUST be called at the end of every uHttpClientXxxRequest() function;
// use the helper macro U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION to be sure
// of this, rather than calling this function directly.
static void exitFunctionRequest(uHttpClientContext_t *pContext, int32_t errorCode)
{
    if (pContext != NULL) {
        pContext->lastRequestTimeMs = -1;
        if ((pContext->pResponseCallback != NULL) && (errorCode == 0)) {
            // The request was successful and we're non-blocking,
            // so remember the time
            pContext->lastRequestTimeMs = uPortGetTickTimeMs();
        }
    }
}

// Handle blocking, or not, as the case may be.
static int32_t block(volatile uHttpClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t startTimeMs;
    int32_t statusCodeOrError = 0;

    if (pContext->pResponseCallback == NULL) {
        // We're blocking
        startTimeMs = uPortGetTickTimeMs();
        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        // Wait for the underlying layer to give a response
        // or pKeepGoingCallback=false or the timeout to occur
        // A status code of 0 means no result: no error, no
        // HTTP status code
        while ((statusCodeOrError == 0) &&
               ((pContext->pKeepGoingCallback == NULL) || pContext->pKeepGoingCallback()) &&
               (uPortGetTickTimeMs() - startTimeMs < (pContext->timeoutSeconds +
                                                      U_HTTP_CLIENT_ADDITIONAL_TIMEOUT_SECONDS) * 1000)) {
            if (uPortSemaphoreTryTake((uPortSemaphoreHandle_t) pContext->semaphoreHandle, 100) == 0) {
                statusCodeOrError = pContext->statusCodeOrError;
            }
        }

        if (statusCodeOrError != 0) {
            errorCode = statusCodeOrError;
        }
        // Clear out any data from the request; this will
        // also give back the semaphore
        clearLastRequest((uHttpClientContext_t *) pContext);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open an HTTP client session.
uHttpClientContext_t *pUHttpClientOpen(uDeviceHandle_t devHandle,
                                       const uHttpClientConnection_t *pConnection,
                                       const uSecurityTlsSettings_t *pSecurityTlsSettings)
{
    uHttpClientContext_t *pContext = NULL;

    gLastOpenError = U_ERROR_COMMON_INVALID_PARAMETER;
    if (pConnection != NULL) {
        // Sort out common resources
        gLastOpenError = U_ERROR_COMMON_NO_MEMORY;
        pContext = (uHttpClientContext_t *) malloc(sizeof(*pContext));
        if (pContext != NULL) {
            // Populate our HTTP context and set up security
            memset(pContext, 0, sizeof (*pContext));
            pContext->lastRequestTimeMs = -1;
            pContext->devHandle = devHandle;
            pContext->timeoutSeconds = pConnection->timeoutSeconds;
            pContext->errorOnBusy = pConnection->errorOnBusy;
            pContext->pResponseCallback = pConnection->pResponseCallback;
            pContext->pResponseCallbackParam = pConnection->pResponseCallbackParam;
            pContext->pKeepGoingCallback = pConnection->pKeepGoingCallback;
            if (uPortSemaphoreCreate((uPortSemaphoreHandle_t *) & (pContext->semaphoreHandle), 1,
                                     1) == 0) { // *NOPAD*
                gLastOpenError = U_ERROR_COMMON_SUCCESS;
                if (pSecurityTlsSettings != NULL) {
                    // Call the common security layer
                    gLastOpenError = U_ERROR_COMMON_NO_MEMORY;
                    pContext->pSecurityContext = pUSecurityTlsAdd(devHandle,
                                                                  pSecurityTlsSettings);
                    if (pContext->pSecurityContext != NULL) {
                        gLastOpenError = (uErrorCode_t) pContext->pSecurityContext->errorCode;
                    }
                }
            }
        }
        if ((gLastOpenError == U_ERROR_COMMON_SUCCESS) &&
            (pContext != NULL)) { // this to keep clang happy
            // Sort out the technology-specific bits
            gLastOpenError = U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
                gLastOpenError = cellOpen(pContext, pConnection);
            } else if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
                gLastOpenError = U_ERROR_COMMON_NOT_IMPLEMENTED;
            }
        }

        if (gLastOpenError != U_ERROR_COMMON_SUCCESS) {
            // Recover all allocated memory if there was an error
            if (pContext != NULL) {
                if ((uPortSemaphoreHandle_t) pContext->semaphoreHandle != NULL) {
                    uPortSemaphoreDelete((uPortSemaphoreHandle_t) pContext->semaphoreHandle);
                }
                if (pContext->pSecurityContext != NULL) {
                    uSecurityTlsRemove(pContext->pSecurityContext);
                }
                free(pContext);
                pContext = NULL;
            }
        }
    }

    return pContext;
}

// Get an error code resulting from pUHttpClientOpen().
int32_t uHttpClientOpenResetLastError()
{
    int32_t lastOpenError = (int32_t) gLastOpenError;
    gLastOpenError = U_ERROR_COMMON_SUCCESS;
    return lastOpenError;
}

// Close the given HTTP client session.
void uHttpClientClose(uHttpClientContext_t *pContext)
{
    if (pContext != NULL) {
        // Call this so as not to pull pContext out from under an
        // existing HTTP request
        U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, NULL, true);

        // Deal with any technology-specific closing things
        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            cellClose(pContext->devHandle, ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            // TODO
        }

        if (pContext->pSecurityContext != NULL) {
            // Free the security context
            uSecurityTlsRemove(pContext->pSecurityContext);
        }

        uPortSemaphoreDelete((uPortSemaphoreHandle_t) pContext->semaphoreHandle);
        free(pContext->pPriv);
        free(pContext);
        pContext = NULL;

        U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, 0);
    }
}

// Make an HTTP PUT request.
int32_t uHttpClientPutRequest(uHttpClientContext_t *pContext,
                              const char *pPath,
                              const char *pData, size_t size,
                              const char *pContentType)
{
    int32_t errorCode;

    U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, &errorCode, false);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pPath != NULL) &&
            ((pData != NULL) || (size == 0)) &&
            ((pContentType != NULL) || (pData == NULL))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
                errorCode = cellPutPost(pContext->devHandle,
                                        ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle,
                                        U_CELL_HTTP_REQUEST_PUT,
                                        pPath, pData, size, pContentType);
            } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
            }

            if (errorCode == 0) {
                // Handle blocking
                errorCode = block((volatile uHttpClientContext_t *) pContext);
            }
        }
    }

    U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode);

    return errorCode;
}

// Make an HTTP POST request.
int32_t uHttpClientPostRequest(uHttpClientContext_t *pContext,
                               const char *pPath,
                               const char *pData, size_t size,
                               const char *pContentType)
{
    int32_t errorCode;

    U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, &errorCode, false);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pPath != NULL) &&
            ((pData != NULL) || (size == 0)) &&
            ((pContentType != NULL) || (pData == NULL))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
                errorCode = cellPutPost(pContext->devHandle,
                                        ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle,
                                        U_CELL_HTTP_REQUEST_POST,
                                        pPath, pData, size, pContentType);
            } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
            }

            if (errorCode == 0) {
                // Handle blocking
                errorCode = block((volatile uHttpClientContext_t *) pContext);
            }
        }
    }

    U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode);

    return errorCode;
}

// Make an HTTP GET request.
int32_t uHttpClientGetRequest(uHttpClientContext_t *pContext,
                              const char *pPath,
                              char *pResponseBody, size_t *pSize,
                              char *pContentType)
{
    int32_t errorCode;

    U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, &errorCode, false);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pPath != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
                pContext->pResponse = pResponseBody;
                pContext->pResponseSize = pSize;
                pContext->pContentType = pContentType;
                errorCode = uCellHttpRequest(pContext->devHandle,
                                             ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle,
                                             U_CELL_HTTP_REQUEST_GET, pPath,
                                             NULL, NULL, NULL);
                if (errorCode != 0) {
                    // Make sure to forget the user's pointers on error
                    pContext->pResponse = NULL;
                    pContext->pResponseSize = NULL;
                    pContext->pContentType = NULL;
                }
            } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
            }

            if (errorCode == 0) {
                // Handle blocking
                errorCode = block((volatile uHttpClientContext_t *) pContext);
            }
        }
    }

    U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode);

    return errorCode;
}

// Make an HTTP HEAD request.
int32_t uHttpClientHeadRequest(uHttpClientContext_t *pContext,
                               const char *pPath,
                               char *pResponseHeader, size_t *pSize)
{
    int32_t errorCode;

    U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, &errorCode, false);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pPath != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
                // Remember the user's pointers
                pContext->pResponse = pResponseHeader;
                pContext->pResponseSize = pSize;
                errorCode = uCellHttpRequest(pContext->devHandle,
                                             ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle,
                                             U_CELL_HTTP_REQUEST_HEAD, pPath,
                                             NULL, NULL, NULL);
                if (errorCode != 0) {
                    // Forget any user-provided pointers on error
                    pContext->pResponse = NULL;
                    pContext->pResponseSize = NULL;
                }
            } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
            }

            if (errorCode == 0) {
                // Handle blocking
                errorCode = block((volatile uHttpClientContext_t *) pContext);
            }
        }
    }

    U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode);

    return errorCode;
}

// Make an HTTP DELETE request.
int32_t uHttpClientDeleteRequest(uHttpClientContext_t *pContext,
                                 const char *pPath)
{
    int32_t errorCode;

    U_HTTP_CLIENT_REQUEST_ENTRY_FUNCTION(pContext, &errorCode, false);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pPath != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
                errorCode = uCellHttpRequest(pContext->devHandle,
                                             ((uHttpClientContextCell_t *) pContext->pPriv)->httpHandle,
                                             U_CELL_HTTP_REQUEST_DELETE, pPath,
                                             NULL, NULL, NULL);
            } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
                // TODO
            }

            if (errorCode == 0) {
                // Handle blocking
                errorCode = block((volatile uHttpClientContext_t *) pContext);
            }
        }
    }

    U_HTTP_CLIENT_REQUEST_EXIT_FUNCTION(pContext, errorCode);

    return errorCode;
}

// End of file
