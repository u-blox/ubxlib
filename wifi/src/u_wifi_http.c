/*
 * Copyright 2019-2023 u-blox
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
 * @brief Implementation of the u-blox HTTP client API for Wi-Fi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strlen()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_APP_TASK_PRIORITY and U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"         // Order is important

#include "u_wifi_http.h"
#include "u_wifi_http_private.h"
#include "u_wifi_private.h"

#include "u_http_client.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_hex_bin_convert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_WIFI_HTTP_SERVER_NAME_MAX_LEN_BYTES
/** The maximum length of the HTTP server name on any module (not
 * all modules support this length, this is the largest HTTP
 * server string length that is supported on any of the Wi-Fi
 * modules).
 */
# define U_WIFI_HTTP_SERVER_NAME_MAX_LEN_BYTES 64
#endif

#ifndef U_WIFI_HTTP_CALLBACK_TASK_STACK_SIZE_BYTES
/** The stack size for the task in which an asynchronous callback
 * will run; shouldn't need much.
 */
# define U_WIFI_HTTP_CALLBACK_TASK_STACK_SIZE_BYTES 2304
#endif

#ifndef U_WIFI_HTTP_CALLBACK_TASK_PRIORITY
/** The priority of the task in which the HTTP callback will run;
 * taking the standard approach of adopting U_CFG_OS_APP_TASK_PRIORITY.
 */
# define U_WIFI_HTTP_CALLBACK_TASK_PRIORITY U_CFG_OS_APP_TASK_PRIORITY
#endif

/** The HTTP callback queue depth.
 */
#define U_WIFI_HTTP_CALLBACK_QUEUE_LENGTH U_WIFI_HTTP_PROFILE_MAX_NUM

#ifndef U_WIFI_HTTP_MAX_AT_PRINT_LENGTH
/** If PUT/POST requests are longer than this, or if the request is a
 * GET request, don't print them to avoid overwhelming the logging
 * stream; set this to -1 to always print everything (if the AT client
 * has AT printing on of course).
 */
# define U_WIFI_HTTP_MAX_AT_PRINT_LENGTH 128
#endif

#ifndef U_WIFI_HTTP_MAX_NUM
/** The maximum number of HTTP sessions that can be open at the same time.
 */
# define U_WIFI_HTTP_MAX_NUM 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A cache of HTTP handles, used so that we can descriminate between
 * user-driven HTTP ones and internally-driven location ones.
 */
static int32_t gHttpHandleCache[U_WIFI_HTTP_MAX_NUM] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Determine if the given HTTP handle is in the HTTP handle cache.
static bool httpHandleIsInCache(int32_t handle)
{
    bool found = false;

    for (size_t x = 0; !found && (x < sizeof(gHttpHandleCache) / sizeof(gHttpHandleCache[0])); x++) {
        if (gHttpHandleCache[x] == handle) {
            found = true;
        }
    }

    return found;
}

// Store an HTTP handle in the HTTP handle cache.
static int32_t httpHandleStoreInCache(int32_t handle)
{
    // A nice obvious error code, since this shouldn't really happen
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TEMPORARY_FAILURE;

    if (handle > 0) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        for (size_t x = 0; (x < sizeof(gHttpHandleCache) / sizeof(gHttpHandleCache[0])) &&
             (errorCode < 0); x++) {
            if (gHttpHandleCache[x] == 0) {
                gHttpHandleCache[x] = handle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Remove an HTTP handle from the HTTP handle cache.
static void httpHandleClearFromCache(int32_t handle)
{
    if (handle > 0) {
        for (size_t x = 0; x < sizeof(gHttpHandleCache) / sizeof(gHttpHandleCache[0]); x++) {
            if (gHttpHandleCache[x] == handle) {
                gHttpHandleCache[x] = 0;
            }
        }
    }
}

// Return true if the given string is allowed
// in a message for an HttpRequest.
static bool isAllowedHttpRequestStr(const char *pStr, size_t maxLength)
{
    size_t strLength;
    bool isAllowed = false;

    if (pStr != NULL) {
        strLength = strlen(pStr);
        if (strLength <= maxLength) {
            isAllowed = true;
            // Must be printable and not contain a quotation mark
            for (size_t x = 0; (x < strLength) && isAllowed; x++, pStr++) {
                if (!isprint((int32_t) *pStr) || (*pStr == '\"')) {
                    isAllowed = false;
                }
            }
        }
    }

    return isAllowed;
}

// Send the AT sequence to close an HTTP session.
static int32_t atCloseHttp(uAtClientHandle_t atHandle, int32_t httpHandle)
{
    uPortLog("U_WIFI_HTTP: sending AT+UDCPC\n");
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UDCPC=");
    uAtClientWriteInt(atHandle, httpHandle);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UUDPC");
    uAtClientResponseStop(atHandle);
    return uAtClientUnlock(atHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiHttpPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO HTTP WIFI
 * -------------------------------------------------------------- */

// Process a URC containing an HTTP response.
bool uWifiHttpPrivateUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    bool httpHandleInCache = false;
    int32_t httpHandle;
    size_t replyLen = 0;
    size_t readBytes;
    uShortRangePrivateInstance_t *pWiFiInstance = (uShortRangePrivateInstance_t *) pParameter;
    uHttpClientContext_t *pHttpContext = pWiFiInstance->pHttpContext;
    uHttpClientContextWifi_t *pContextWifi;
    char *pHexBuffer;

    httpHandle = uAtClientReadInt(atHandle);
    httpHandleInCache = httpHandleIsInCache(httpHandle);
    if (httpHandleInCache && (pHttpContext != NULL)) {
        // The HTTP handle was in the cache so it must be a true
        // HTTP handle resulting from a user HTTP request, rather
        // than one from a location request
        pContextWifi = pHttpContext->pPriv;
        pHttpContext->statusCodeOrError = uAtClientReadInt(
                                              atHandle); // read HTTP statuscode into httpcontext
        if (httpHandle) {
            if (pHttpContext->statusCodeOrError == 200) {
                replyLen = uAtClientReadInt(atHandle);
                if (replyLen > 0) {
                    if (pContextWifi->replyOffset) {
                        uAtClientSkipParameters(atHandle, 1);
                    } else {
                        if (pHttpContext->pContentType) {
                            uAtClientReadString(atHandle, pHttpContext->pContentType,
                                                U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES,
                                                true);
                        } else {
                            uAtClientSkipParameters(atHandle, 1); // no mem provided for contentType, skip
                        }
                    }
                    uAtClientIgnoreStopTag(atHandle);
                    if (pHttpContext->pResponse) {
                        if (pContextWifi->binary) {
                            readBytes = replyLen * 2; // got reply in hex
                            pHexBuffer = (char *)pUPortMalloc(readBytes);
                            if (uAtClientReadBytes(atHandle, pHexBuffer,
                                                   readBytes,
                                                   true) == readBytes) {
                                uHexToBin(pHexBuffer, readBytes, pHttpContext->pResponse + pContextWifi->replyOffset);
                            }
                            uPortFree(pHexBuffer);
                        } else {
                            uAtClientReadBytes(atHandle, pHttpContext->pResponse + pContextWifi->replyOffset,
                                               replyLen, true);
                        }
                    }
                    // Note: don't restore stop tag here, since we're not in
                    // a usual response, we're in a URC; as this is the last
                    // part of the URC the generic AT Client URC handling will
                    // do the right thing
                    pContextWifi->replyOffset += replyLen;
                    if (pHttpContext->pResponseSize) {
                        *pHttpContext->pResponseSize = pContextWifi->replyOffset;
                    }
                }
#if !U_CFG_OS_CLIB_LEAKS
                uPortLog("U_WIFI_HTTP: total reply size: %d.\n", pContextWifi->replyOffset);
#endif
                if (pContextWifi->atPrintWasOn) {
                    // AT printing can now be restored
                    uAtClientPrintAtSet(atHandle, true);
                    pContextWifi->atPrintWasOn = false;
                }
                // Call the http callback, if required
                if (pHttpContext->pResponseCallback != NULL) {
                    pHttpContext->pResponseCallback(pWiFiInstance->devHandle,
                                                    pHttpContext->statusCodeOrError,
                                                    (size_t) pContextWifi->replyOffset,
                                                    pHttpContext->pResponseCallbackParam);
                }
                // Call the wifi callback, if required
                if (pWiFiInstance->pWifiHttpCallBack != NULL) {
                    pWiFiInstance->pWifiHttpCallBack(pWiFiInstance->devHandle,
                                                     httpHandle,
                                                     pHttpContext->statusCodeOrError,
                                                     pWiFiInstance->pHttpContext);
                }
                pContextWifi->replyOffset = 0;
                uPortSemaphoreGive((uPortSemaphoreHandle_t) pHttpContext->semaphoreHandle);
            } else if (pHttpContext->statusCodeOrError == 206) { // fragmented multiple replies
                replyLen = uAtClientReadInt(atHandle);
                if (replyLen > 0) {
                    if (pContextWifi->replyOffset == 0) { // firstFragment
                        if (pHttpContext->pContentType) {
                            uAtClientReadString(atHandle, pHttpContext->pContentType,
                                                U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES,
                                                true);
                        } else {
                            uAtClientSkipParameters(atHandle, 1); // no mem provided for contentType, skip
                        }
                    } else {
                        uAtClientSkipParameters(atHandle, 1);
                    }
                    uAtClientIgnoreStopTag(atHandle);
                    if (pHttpContext->pResponse) {
                        if (pContextWifi->binary) {
                            readBytes = replyLen * 2; // got reply in hex
                            pHexBuffer = (char *)pUPortMalloc(readBytes);
                            if (uAtClientReadBytes(atHandle, pHexBuffer,
                                                   readBytes,
                                                   true) == readBytes) {
                                uHexToBin(pHexBuffer, readBytes, pHttpContext->pResponse + pContextWifi->replyOffset);
                                pContextWifi->replyOffset = pContextWifi->replyOffset + replyLen;
                            }
                            uPortFree(pHexBuffer);
                        } else {
                            if (uAtClientReadBytes(atHandle, pHttpContext->pResponse + pContextWifi->replyOffset,
                                                   replyLen,
                                                   true) == replyLen) {
                                pContextWifi->replyOffset = pContextWifi->replyOffset + replyLen;
                            }
                        }
                    }
                    // Note: don't restore stop tag here, since we're not in
                    // a usual response, we're in a URC; as this is the last
                    // part of the URC the generic AT Client URC handling will
                    // do the right thing
                }
            } else {
                if (pContextWifi->atPrintWasOn) {
                    // AT printing can now be restored
                    uAtClientPrintAtSet(atHandle, true);
                    pContextWifi->atPrintWasOn = false;
                }
                // Call the http callback, if required
                if (pHttpContext->pResponseCallback != NULL) {
                    pHttpContext->pResponseCallback(pWiFiInstance->devHandle,
                                                    pHttpContext->statusCodeOrError,
                                                    (size_t) replyLen,
                                                    pHttpContext->pResponseCallbackParam);
                }
                // Call the wifi callback, if required
                if (pWiFiInstance->pWifiHttpCallBack != NULL) {
                    pWiFiInstance->pWifiHttpCallBack(pWiFiInstance->devHandle,
                                                     httpHandle,
                                                     pHttpContext->statusCodeOrError,
                                                     pWiFiInstance->pHttpContext);
                }
                pContextWifi->replyOffset = 0;
                // Set the status code for block() to read if required and
                // give the semaphore back
                uPortSemaphoreGive((uPortSemaphoreHandle_t) pHttpContext->semaphoreHandle);
            }
        }
    }

    return httpHandleInCache;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open a Wi-Fi HTTP client instance.
int32_t uWifiHttpOpen(uDeviceHandle_t wifiHandle, const char *pServerName,
                      const char *pUserName, const char *pPassword,
                      int32_t timeoutSeconds, uWifiHttpCallback_t *pCallback,
                      void *pCallbackParam)
{
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char urlBuffer[128] = {0};
    char tempBuffer[64] = {0};
    int32_t httpHandle = (int32_t) U_ERROR_COMMON_UNKNOWN;
    int32_t peerType;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pServerName != NULL) && (strlen(pServerName) <= U_WIFI_HTTP_SERVER_NAME_MAX_LEN_BYTES) &&
        ((pUserName != NULL) || (pPassword == NULL)) &&
        (timeoutSeconds >= 0)) {
        if (gUShortRangePrivateMutex != NULL) {
            U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

            pInstance = pUShortRangePrivateGetInstance(wifiHandle);
            if (pInstance != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                              U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT)) {
                    pInstance->pWifiHttpCallBack = pCallback;
                    pInstance->pHttpContext = (uHttpClientContext_t *)pCallbackParam;
                    atHandle = pInstance->atHandle;
                    snprintf(urlBuffer, sizeof(urlBuffer), "%s/?", pServerName);
                    if (pUserName) {
                        snprintf(tempBuffer, sizeof(tempBuffer), "user=%s&", pUserName);
                        strncat(urlBuffer, tempBuffer, sizeof(urlBuffer) - strlen(urlBuffer) - 1);
                    }
                    if (pPassword) {
                        snprintf(tempBuffer, sizeof(tempBuffer), "passwd=%s&", pPassword);
                        strncat(urlBuffer, tempBuffer, sizeof(urlBuffer) - strlen(urlBuffer) - 1);
                    }
                    snprintf(tempBuffer, sizeof(tempBuffer), "http-timeout=%d", (int)timeoutSeconds * 1000);
                    strncat(urlBuffer, tempBuffer, sizeof(urlBuffer) - strlen(urlBuffer) - 1);
                    if (pInstance->pHttpContext->pSecurityContext) {
                        strncat(urlBuffer, "&encr=1", sizeof(urlBuffer) - strlen(urlBuffer) - 1);
                    }
                    // Configure the server in the connection
                    uPortLog("U_WIFI_HTTP: sending AT+UDCP\n");
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UDCP=http-tcp://");
                    uAtClientWriteString(atHandle, urlBuffer, false);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UUDPC:");
                    httpHandle = uAtClientReadInt(atHandle);
                    peerType = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        if (peerType < 2 || peerType > 3) { // check that IPV4/6
                            errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                        }
                        if (errorCode == 0) {
                            // Done: store the handle in the cache and hook in the URC
                            errorCode = httpHandleStoreInCache(httpHandle);
                            if (errorCode == 0) {
                                errorCode = uAtClientSetUrcHandler(atHandle, "+UUDHTTP:",
                                                                   uWifiPrivateUudhttpUrc,
                                                                   pInstance);
                            }
                        }
                        if (errorCode < 0) {
                            // Close the session again on error
                            atCloseHttp(atHandle, httpHandle);
                            httpHandleClearFromCache(httpHandle);
                        }
                    }
                }
            } else {
                errorCode = U_ERROR_COMMON_NOT_FOUND;
            }
            U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        }
    }
    if (errorCode != 0) {
        httpHandle = errorCode;
    }
    return httpHandle;
}

// Shut-down the given Wi-Fi HTTP client instance.
void uWifiHttpClose(uDeviceHandle_t wifiHandle, int32_t httpHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uHttpClientContextWifi_t *pContextWifi;

    if (gUShortRangePrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        if (pInstance != NULL) {
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT)) {
                atHandle = pInstance->atHandle;
                if (pInstance->pHttpContext != NULL) {
                    pContextWifi = pInstance->pHttpContext->pPriv;
                    if ((pContextWifi != NULL) && (pContextWifi->atPrintWasOn)) {
                        // AT printing can now be restored
                        uAtClientPrintAtSet(atHandle, true);
                    }
                }
                // Send the AT sequence to close a HTTP session
                atCloseHttp(atHandle, httpHandle);
                httpHandleClearFromCache(httpHandle);
                pInstance->pWifiHttpCallBack = NULL;
                pInstance->pHttpContext = NULL;
            }
        }
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
}

// Perform an HTTP request. Primary used for GET and DELETE
int32_t uWifiHttpRequest(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                         uWifiHttpRequest_t requestType, const char *pPath,
                         const char *pContent, const char *pContentType)
{
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uHttpClientContextWifi_t *pContextWifi;
    int32_t httpCommand = (int32_t) requestType;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (gUShortRangePrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        if ((pInstance != NULL) && (httpHandle > 0) && (httpCommand >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT) &&
                (isAllowedHttpRequestStr(pPath, U_WIFI_HTTP_PATH_MAX_LENGTH_BYTES)) &&
                ((requestType == U_WIFI_HTTP_REQUEST_GET) ||
                 (requestType == U_WIFI_HTTP_REQUEST_POST) ||
                 (requestType == U_WIFI_HTTP_REQUEST_PUT) ||
                 (requestType == U_WIFI_HTTP_REQUEST_PATCH) ||
                 (requestType == U_WIFI_HTTP_REQUEST_DELETE) ||
                 (requestType == U_WIFI_HTTP_REQUEST_OPTIONS) ||
                 (requestType == U_WIFI_HTTP_REQUEST_GET_BINARY))) {
                pContextWifi = pInstance->pHttpContext->pPriv;
                if (requestType == U_WIFI_HTTP_REQUEST_GET_BINARY) {
                    pContextWifi->binary = true;
                } else {
                    pContextWifi->binary = false;
                }
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UDHTTP=");
                uAtClientWriteInt(atHandle, httpHandle);
                uAtClientWriteInt(atHandle, httpCommand);
                uAtClientWriteString(atHandle, pPath, true);
                if (isAllowedHttpRequestStr(pContentType,
                                            U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES) &&
                    isAllowedHttpRequestStr(pContent, U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES)) {
                    uAtClientWriteString(atHandle, pContentType, true);
                    pContextWifi->atPrintWasOn = false;
#if U_WIFI_HTTP_MAX_AT_PRINT_LENGTH >= 0
                    if (uAtClientPrintAtGet(atHandle) &&
                        (((pContent != NULL) && (strlen(pContent) > U_WIFI_HTTP_MAX_AT_PRINT_LENGTH)) ||
                         (requestType == U_WIFI_HTTP_REQUEST_GET) ||
                         (requestType == U_WIFI_HTTP_REQUEST_GET_BINARY))) {
                        // Turn off AT command printing so as not to
                        // overwhelm the logging stream
                        uAtClientPrintAtSet(atHandle, false);
                        pContextWifi->atPrintWasOn = true;
                    }
#endif
                    uAtClientWriteString(atHandle, pContent, true);
                }
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
    return errorCode;
}

// Perform an extended HTTP request. Primary for POST,PUT,PATCH,OPTIONS and GET_BINARY
int32_t uWifiHttpRequestEx(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                           uWifiHttpRequest_t requestType, const char *pPath,
                           const char *pData, size_t contentLength, const char *pContentType)
{
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uHttpClientContextWifi_t *pContextWifi;
    int32_t httpCommand = (int32_t) requestType;
    int32_t bytesToWrite = 0;
    size_t offset = 0;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t err;

    if (gUShortRangePrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        if ((pInstance != NULL) &&
            (httpHandle > 0) &&
            (httpCommand >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_HTTP_CLIENT)) {
                if ((isAllowedHttpRequestStr(pPath, U_WIFI_HTTP_PATH_MAX_LENGTH_BYTES)) &&
                    ((requestType == U_WIFI_HTTP_REQUEST_GET) ||
                     (requestType == U_WIFI_HTTP_REQUEST_POST) ||
                     (requestType == U_WIFI_HTTP_REQUEST_PUT) ||
                     (requestType == U_WIFI_HTTP_REQUEST_PATCH) ||
                     (requestType == U_WIFI_HTTP_REQUEST_DELETE) ||
                     (requestType == U_WIFI_HTTP_REQUEST_OPTIONS) ||
                     (requestType == U_WIFI_HTTP_REQUEST_GET_BINARY))) {
                    pContextWifi = pInstance->pHttpContext->pPriv;
                    if (requestType == U_WIFI_HTTP_REQUEST_GET_BINARY) {
                        pContextWifi->binary = true;
                    } else {
                        pContextWifi->binary = false;
                    }

                    atHandle = pInstance->atHandle;
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UDHTTPE=");
                    uAtClientWriteInt(atHandle, httpHandle);
                    uAtClientWriteInt(atHandle, httpCommand);
                    uAtClientWriteString(atHandle, pPath, true);
                    pContextWifi->atPrintWasOn = false;
                    if ((requestType != U_WIFI_HTTP_REQUEST_GET_BINARY) &&
                        (requestType != U_WIFI_HTTP_REQUEST_GET)) {
                        if (isAllowedHttpRequestStr(pContentType,
                                                    U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES) &&
                            (contentLength <= U_WIFI_HTTP_BLOB_MAX_LENGTH_BYTES)) {
                            uAtClientWriteString(atHandle, pContentType, true);
                            uAtClientWriteInt(atHandle, contentLength);
                            uAtClientCommandStop(atHandle);
#if U_WIFI_HTTP_MAX_AT_PRINT_LENGTH >= 0
                            if (uAtClientPrintAtGet(atHandle) &&
                                (contentLength > U_WIFI_HTTP_MAX_AT_PRINT_LENGTH)) {
                                // Turn off AT command printing so as not to
                                // overwhelm the logging stream
                                uAtClientPrintAtSet(atHandle, false);
                                pContextWifi->atPrintWasOn = true;
                            }
#endif
                            // Wait for the prompt
                            if (uAtClientWaitCharacter(atHandle, '>') == 0) {
                                // Allow plenty of time for this to complete
                                uAtClientTimeoutSet(atHandle, 10000);
                                // Wait for it...
                                do {
                                    uPortTaskBlock(50);
                                    // Write the binary message
                                    if ((contentLength - offset) < U_HTTP_CLIENT_WIFI_CHUNK_LENGTH) {
                                        bytesToWrite = contentLength - offset;
                                    } else {
                                        bytesToWrite = U_HTTP_CLIENT_WIFI_CHUNK_LENGTH;
                                    }
                                    if (uAtClientWriteBytes(atHandle,
                                                            pData + offset,
                                                            bytesToWrite,
                                                            true) == bytesToWrite) {
                                        errorCode = U_ERROR_COMMON_SUCCESS;
                                    }
                                    offset += bytesToWrite;
                                } while ((offset < contentLength) && errorCode == U_ERROR_COMMON_SUCCESS);
                                uPortLog("\nU_WIFI_HTTP: wrote %d byte(s).\n", offset);
                            }
                        }
                    } else {
#if U_WIFI_HTTP_MAX_AT_PRINT_LENGTH >= 0
                        if (uAtClientPrintAtGet(atHandle)) {
                            // Turn off AT command printing so as not to
                            // overwhelm the logging stream
                            uAtClientPrintAtSet(atHandle, false);
                            pContextWifi->atPrintWasOn = true;
                        }
#endif
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                    uAtClientCommandStopReadResponse(atHandle);

                    err = uAtClientUnlock(atHandle);
                    if (err != U_ERROR_COMMON_SUCCESS) {
                        errorCode = err;
                    }
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
    return errorCode;
}

// End of file
