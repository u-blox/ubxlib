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
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_APP_TASK_PRIORITY

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"         // Order is important

#include "u_wifi_http.h"

#include "u_http_client.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_hex_bin_convert.h"

#include "u_port_debug.h"
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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Return true if the given string is allowed
// in a message for an HttpRequest
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

// HTTP URC handler.
static void UUDHTTP_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    int32_t httpHandle;
    size_t replyLen = 0;
    size_t readBytes;
    uShortRangePrivateInstance_t *pWiFiInstance = (uShortRangePrivateInstance_t *) pParameter;
    uHttpClientContext_t *pHttpContext = pWiFiInstance->pHttpContext;
    uHttpClientContextWifi_t *pContextWifi = pHttpContext->pPriv;
    char *hexBuffer;

    httpHandle = uAtClientReadInt(atHandle);
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
                        hexBuffer = (char *)pUPortMalloc(readBytes);
                        if (uAtClientReadBytes(atHandle, hexBuffer,
                                               readBytes,
                                               true) == readBytes) {
                            uHexToBin(hexBuffer, readBytes, pHttpContext->pResponse + pContextWifi->replyOffset);
                        }
                        uPortFree(hexBuffer);
                    } else {
                        if (uAtClientReadBytes(atHandle, pHttpContext->pResponse + pContextWifi->replyOffset, replyLen,
                                               true) == replyLen) {
                        }
                    }
                }
                pContextWifi->replyOffset += replyLen;
                if (pHttpContext->pResponseSize) {
                    *pHttpContext->pResponseSize = pContextWifi->replyOffset;
                }
            }
#if !U_CFG_OS_CLIB_LEAKS
            uPortLog("U_WIFI_HTTP: Total replysize: %d\n", pContextWifi->replyOffset);
#endif
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
                if (pContextWifi->replyOffset == 0) { //firstFragment) {
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
                        hexBuffer = (char *)pUPortMalloc(readBytes);
                        if (uAtClientReadBytes(atHandle, hexBuffer,
                                               readBytes,
                                               true) == readBytes) {
                            uHexToBin(hexBuffer, readBytes, pHttpContext->pResponse + pContextWifi->replyOffset);
                            pContextWifi->replyOffset = pContextWifi->replyOffset + replyLen;
                        }
                        uPortFree(hexBuffer);
                    } else {
                        if (uAtClientReadBytes(atHandle, pHttpContext->pResponse + pContextWifi->replyOffset,
                                               replyLen,
                                               true) == replyLen) {
                            pContextWifi->replyOffset = pContextWifi->replyOffset + replyLen;
                        }
                    }
                }
            }
        } else {
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiHttpPrivateLink()
{
    //dummy
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
    int32_t httpHandle = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t peerType;
    int32_t errorCode = U_ERROR_COMMON_SUCCESS;

    if ((pServerName != NULL) && (strlen(pServerName) <= U_WIFI_HTTP_SERVER_NAME_MAX_LEN_BYTES) &&
        ((pUserName != NULL) || (pPassword == NULL)) &&
        (timeoutSeconds >= 0)) {
        if (gUShortRangePrivateMutex != NULL) {
            U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

            pInstance = pUShortRangePrivateGetInstance(wifiHandle);
            if (pInstance != NULL) {
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
                uPortLog("U_WIFI_HTTP: Sending AT+UDCP\n");
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UDCP=http-tcp://");
                uAtClientWriteString(atHandle, urlBuffer, false);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UUDPC:");
                httpHandle = uAtClientReadInt(atHandle);
                peerType = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if (peerType < 2 || peerType > 3) { // check that IPV4/6
                    errorCode = U_ERROR_COMMON_UNKNOWN;
                }
                uAtClientUnlock(atHandle);
                // Report to user that we are disconnected
                if (errorCode == (int32_t)U_ERROR_COMMON_TIMEOUT) {
                    httpHandle = U_ERROR_COMMON_TIMEOUT;
                }
                //}
                if (errorCode == 0) {
                    // Done: hook in the URC
                    uAtClientRemoveUrcHandler(atHandle, "+UUDHTTP:"); // remove any existing
                    errorCode = uAtClientSetUrcHandler(atHandle, "+UUDHTTP:",
                                                       UUDHTTP_urc, pInstance);
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

    if (gUShortRangePrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uPortLog("U_WIFI_HTTP: Sending AT+UDCPC\n");
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDCPC=");
            uAtClientWriteInt(atHandle, httpHandle);
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UUDPC");
            uAtClientResponseStop(atHandle);
            uAtClientRemoveUrcHandler(atHandle, "+UUDHTTP:");
            uAtClientUnlock(atHandle);
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
        if ((pInstance != NULL) &&
            (httpHandle > 0) &&
            (httpCommand >= 0)) {
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
                uAtClientCommandStart(atHandle, "AT+UDHTTP=");
                uAtClientWriteInt(atHandle, httpHandle);
                uAtClientWriteInt(atHandle, httpCommand);
                uAtClientWriteString(atHandle, pPath, true);
                if (isAllowedHttpRequestStr(pContentType,
                                            U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES) &&
                    isAllowedHttpRequestStr(pContent, U_WIFI_HTTP_DATA_MAX_LENGTH_BYTES)) {
                    uAtClientWriteString(atHandle, pContentType, true);
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
                if (requestType != U_WIFI_HTTP_REQUEST_GET_BINARY
                    && requestType != U_WIFI_HTTP_REQUEST_GET) {
                    if (isAllowedHttpRequestStr(pContentType,
                                                U_WIFI_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES)) {
                        if (contentLength <= U_WIFI_HTTP_BLOB_MAX_LENGTH_BYTES) {
                            uAtClientWriteString(atHandle, pContentType, true);
                            uAtClientWriteInt(atHandle, contentLength);
                            uAtClientCommandStop(atHandle);
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
                                uPortLog("\nU_WIFI_HTTP: Wrote %d bytes\n", offset);
                            }
                        }
                    }
                } else {
                    errorCode = U_ERROR_COMMON_SUCCESS;
                }
                uAtClientCommandStopReadResponse(atHandle);

                err = uAtClientUnlock(atHandle);
                if (err != U_ERROR_COMMON_SUCCESS) {
                    errorCode = err;
                }
                U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
            }
        }
    }
    return errorCode;
}

// End of file
