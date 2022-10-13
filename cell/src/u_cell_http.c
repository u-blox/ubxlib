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
 * @brief Implementation of the u-blox HTTP client API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
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
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_http.h"
#include "u_cell_http_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of HTTP profiles that a module can support.
 */
#define U_CELL_HTTP_PROFILE_MAX_NUM 4

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, ppCellInstance, \
                                   ppHttpInstance, pErrorCode) \
                                   { entryFunction(cellHandle, \
                                                   httpHandle, \
                                                   ppCellInstance, \
                                                   ppHttpInstance, \
                                                   pErrorCode)

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_HTTP_EXIT_FUNCTION() } exitFunction()

#ifndef U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES
/** The maximum length of the HTTP server name on any module (not
 * all modules support this length, this is the largest HTTP
 * server string length that is supported on any of the cellular
 * modules).
 */
# define U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES 1024
#endif

#ifndef U_CELL_HTTP_CALLBACK_TASK_STACK_SIZE_BYTES
/** The stack size for the task in which an asynchronous callback
 * will run; shouldn't need much.
 */
# define U_CELL_HTTP_CALLBACK_TASK_STACK_SIZE_BYTES 2304
#endif

#ifndef U_CELL_HTTP_CALLBACK_TASK_PRIORITY
/** The priority of the task in which the HTTP callback will run;
 * taking the standard approach of adopting U_CFG_OS_APP_TASK_PRIORITY.
 */
# define U_CELL_HTTP_CALLBACK_TASK_PRIORITY U_CFG_OS_APP_TASK_PRIORITY
#endif

/** The HTTP callback queue depth.
 */
#define U_CELL_HTTP_CALLBACK_QUEUE_LENGTH U_CELL_HTTP_PROFILE_MAX_NUM

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** All the parameters for the HTTP callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    int32_t httpHandle;
    uCellHttpRequest_t requestType;
    bool error;
    char *pFileNameResponse;
    uCellHttpCallback_t *pCallback;
    void *pCallbackParam;
} uCellHttpCallbackParameters_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print the error state of the HTTP client in the module.
//lint -esym(522, printErrorCodes) Suppress "lacks side effects"
// when compiled out.
static void printErrorCodes(uAtClientHandle_t atHandle,
                            int32_t profileId)
{
#if U_CFG_ENABLE_LOGGING
    int32_t err1;
    int32_t err2;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "+UHTTPER=");
    uAtClientWriteInt(atHandle, profileId);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UHTTPER:");
    // Skip the first parameter, which is our
    // profile ID being sent back to us
    uAtClientSkipParameters(atHandle, 1);
    err1 = uAtClientReadInt(atHandle);
    err2 = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    uPortLog("U_CELL_HTTP: error codes %d, %d.\n", err1, err2);
#else
    (void) atHandle;
    (void) profileId;
#endif
}

// Find an HTTP instance in the list.
static uCellHttpInstance_t *pFindHttpInstance(uCellPrivateInstance_t *pCellInstance,
                                              int32_t httpHandle)
{
    uCellHttpInstance_t *pHttpInstance = NULL;
    uCellHttpContext_t *pHttpContext = pCellInstance->pHttpContext;

    if (pHttpContext != NULL) {
        pHttpInstance = pHttpContext->pInstanceList;
        while ((pHttpInstance != NULL) &&
               (pHttpInstance->profileId != httpHandle)) {
            pHttpInstance = pHttpInstance->pNext;
        }
    }

    return pHttpInstance;
}

// Check all the basics and lock the mutex, MUST be called at the
// start of every API function; use the helper macro
// U_CELL_HTTP_ENTRY_FUNCTION to be sure of this, rather than calling
// this function directly.  ppCellInstance and ppHttpInstance will
// be populated if non-NULL; if they cannot be populated an error
// will be returned. Set ppHttpInstance to NULL if this is being
// called from uCellHttpOpen().
static void entryFunction(uDeviceHandle_t cellHandle,
                          int32_t httpHandle,
                          uCellPrivateInstance_t **ppCellInstance,
                          uCellHttpInstance_t **ppHttpInstance,
                          int32_t *pErrorCode)
{
    uCellPrivateInstance_t *pCellInstance = NULL;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gUCellPrivateMutex != NULL) {

        uPortMutexLock(gUCellPrivateMutex);

        pCellInstance = pUCellPrivateGetInstance(cellHandle);
        if (pCellInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (ppHttpInstance != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                *ppHttpInstance = pFindHttpInstance(pCellInstance, httpHandle);
                if (*ppHttpInstance != NULL) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    if (ppCellInstance != NULL) {
        *ppCellInstance = pCellInstance;
    }
    if (pErrorCode != NULL) {
        *pErrorCode = errorCode;
    }
}

// MUST be called at the end of every API function to unlock
// the cellular mutex; use the helper macro
// U_CELL_HTTP_EXIT_FUNCTION to be sure of this, rather than calling
// this function directly.
static void exitFunction()
{
    if (gUCellPrivateMutex != NULL) {
        uPortMutexUnlock(gUCellPrivateMutex);
    }
}

// Perform an AT+UHTTP operation that has a string parameter
static int32_t doUhttpString(uAtClientHandle_t atHandle,
                             int32_t profileId, int32_t opCode,
                             const char *pStr)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UHTTP=");
    uAtClientWriteInt(atHandle, profileId);
    uAtClientWriteInt(atHandle, opCode);
    uAtClientWriteString(atHandle, pStr, true);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Perform an AT+UHTTP operation that has an integer parameter
static int32_t doUhttpInteger(uAtClientHandle_t atHandle,
                              int32_t profileId, int32_t opCode,
                              int32_t value)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UHTTP=");
    uAtClientWriteInt(atHandle, profileId);
    uAtClientWriteInt(atHandle, opCode);
    uAtClientWriteInt(atHandle, value);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Set security on or off.
static int32_t setSecurity(uDeviceHandle_t cellHandle, int32_t httpHandle,
                           bool onNotOff, int32_t securityProfileId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpInstance_t *pHttpInstance;
    uAtClientHandle_t atHandle;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, &pCellInstance,
                               &pHttpInstance, &errorCode);

    if (errorCode == 0) {
        atHandle = pCellInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UHTTP=");
        uAtClientWriteInt(atHandle, pHttpInstance->profileId);
        uAtClientWriteInt(atHandle, 6);
        uAtClientWriteInt(atHandle, onNotOff);
        if (onNotOff) {
            uAtClientWriteInt(atHandle, securityProfileId);
        }
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return errorCode;
}

// Return true if the given string is allowed
// in a message for an HttpRequest, non-file version.
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

// Populate the response file name.  If no file name is given then
// create one. The file name is formed of:
// U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX + profile ID
// e.g.: ubxlibhttp_0
static void copyFileNameResponse(uCellHttpInstance_t *pHttpInstance,
                                 const char *pFileNameGiven)
{
    if (pFileNameGiven != NULL) {
        strncpy(pHttpInstance->fileNameResponse, pFileNameGiven,
                sizeof(pHttpInstance->fileNameResponse));
    } else {
        snprintf(pHttpInstance->fileNameResponse, sizeof(pHttpInstance->fileNameResponse),
                 "%s%d", U_CELL_HTTP_FILE_NAME_RESPONSE_AUTO_PREFIX,
                 (int) pHttpInstance->profileId);
    }
}

// Event queue callback, we end up here from UUHTTPCR_urc().
static void eventQueueCallback(void *pParameters, size_t paramLength)
{
    uCellHttpCallbackParameters_t *pCallback = (uCellHttpCallbackParameters_t *) pParameters;

    (void) paramLength;

    if (pCallback != NULL) {
        if (pCallback->pCallback != NULL) {
            pCallback->pCallback(pCallback->cellHandle,
                                 pCallback->httpHandle,
                                 pCallback->requestType,
                                 pCallback->error,
                                 pCallback->pFileNameResponse,
                                 pCallback->pCallbackParam);
        }
        // Free the pFileNameResponse memory
        // that was allocated by UUHTTPCR_urc();
        // the event queue mechanism will deal
        // with the memory occupied by pCallback.
        free(pCallback->pFileNameResponse);
    }
}

// HTTP URC handler.
static void UUHTTPCR_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    int32_t profileId;
    int32_t requestType;
    int32_t result;
    uCellPrivateInstance_t *pCellInstance = (uCellPrivateInstance_t *) pParameter;
    uCellHttpContext_t *pHttpContext = pCellInstance->pHttpContext;
    uCellHttpInstance_t *pHttpInstance;
    uCellHttpCallbackParameters_t *pCallback;

    // Read the three parameters
    profileId = uAtClientReadInt(atHandle);
    requestType = uAtClientReadInt(atHandle);
    result = uAtClientReadInt(atHandle);

    if ((profileId >= 0) && (requestType >= 0) && (result >= 0)) {
        // Convert POST_DATA to POST
        if (requestType == 5) {
            requestType = 4;
        }

        U_PORT_MUTEX_LOCK(pHttpContext->linkedListMutex);

        // Find the profile in the list
        pHttpInstance = pFindHttpInstance(pCellInstance, profileId);
        if (pHttpInstance != NULL) {
            pCallback = (uCellHttpCallbackParameters_t *) malloc(sizeof(*pCallback));
            if (pCallback != NULL) {
                // Note: eventQueueCallback() will free() pFileNameResponse.
                //lint -esym(429, pFileNameResponse) Suppress pFileNameResponse not being free()ed here
                //lint -esym(593, pFileNameResponse) Suppress pFileNameResponse not being free()ed here
                pCallback->pFileNameResponse = (char *) malloc(U_CELL_FILE_NAME_MAX_LENGTH + 1);
                if (pCallback->pFileNameResponse != NULL) {
                    strncpy(pCallback->pFileNameResponse, pHttpInstance->fileNameResponse,
                            U_CELL_FILE_NAME_MAX_LENGTH + 1);
                    pCallback->cellHandle = pCellInstance->cellHandle;
                    pCallback->httpHandle = pHttpInstance->profileId;
                    pCallback->requestType = (uCellHttpRequest_t) requestType;
                    pCallback->error = (result == 0);
                    pCallback->pCallback = pHttpInstance->pCallback;
                    pCallback->pCallbackParam = pHttpInstance->pCallbackParam;
                    if (uPortEventQueueSend(pHttpContext->eventQueueHandle,
                                            pCallback, sizeof(*pCallback)) < 0) {
                        // Free pFileNameResponse if we couldn't send it
                        free(pCallback->pFileNameResponse);
                    }
                }
                // Can always free pCallback as uPortEventQueueSend() will have
                // taken a copy of it
                free(pCallback);
            }
        }

        U_PORT_MUTEX_UNLOCK(pHttpContext->linkedListMutex);
    }

}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open a cellular HTTP client instance.
int32_t uCellHttpOpen(uDeviceHandle_t cellHandle, const char *pServerName,
                      const char *pUserName, const char *pPassword,
                      int32_t timeoutSeconds, uCellHttpCallback_t *pCallback,
                      void *pCallbackParam)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpContext_t *pHttpContext;
    uCellHttpInstance_t *pHttpInstance;
    int32_t profileId = -1;
    uSockAddress_t address;
    char *pServerNameTmp;
    char *pTmp;
    int32_t authenticationType = 0;
    uAtClientHandle_t atHandle;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, -1, &pCellInstance, NULL,
                               &errorCodeOrHandle);

    if (errorCodeOrHandle == 0) {
        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pServerName != NULL) && (strlen(pServerName) <= U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES) &&
            ((pUserName != NULL) || (pPassword == NULL)) &&
            (timeoutSeconds >= 0) && (pCallback != NULL)) {
            errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pHttpContext = pCellInstance->pHttpContext;
            if (pHttpContext == NULL) {
                // Get a context if we don't already have one; this
                // will be free'd only when the cellular instance is closed
                // to ensure thread-safety
                pHttpContext = (uCellHttpContext_t *) malloc(sizeof(uCellHttpContext_t));
                if (pHttpContext != NULL) {
                    memset(pHttpContext, 0, sizeof(*pHttpContext));
                    pHttpContext->eventQueueHandle = -1;
                    errorCodeOrHandle = uPortMutexCreate(&pHttpContext->linkedListMutex);
                    if (errorCodeOrHandle == 0) {
                        // We employ our own event queue as a GET request can require
                        // relatively large file reads from the file system of the
                        // cellular module, which would block the usual AT callback
                        // queue for too long
                        pHttpContext->eventQueueHandle = uPortEventQueueOpen(eventQueueCallback,
                                                                             "cellHttp",
                                                                             sizeof(uCellHttpCallbackParameters_t),
                                                                             U_CELL_HTTP_CALLBACK_TASK_STACK_SIZE_BYTES,
                                                                             U_CELL_HTTP_CALLBACK_TASK_PRIORITY,
                                                                             U_CELL_HTTP_CALLBACK_QUEUE_LENGTH);
                        if (pHttpContext->eventQueueHandle < 0) {
                            // Clean up on error
                            uPortMutexDelete(pHttpContext->linkedListMutex);
                            free(pHttpContext);
                            pHttpContext = NULL;
                        }
                    } else {
                        // Clean up on error
                        free(pHttpContext);
                        pHttpContext = NULL;
                    }
                }
                pCellInstance->pHttpContext = pHttpContext;
            }
            if (pHttpContext != NULL) {
                // Find a free profile ID
                for (size_t x = 0; (x < U_CELL_HTTP_PROFILE_MAX_NUM) && (profileId < 0); x++) {
                    if (pFindHttpInstance(pCellInstance, x) == NULL) {
                        profileId = x;
                    }
                }
                if (profileId >= 0) {
                    // Grab some temporary memory to manipulate the server name
                    // +1 for terminator
                    pServerNameTmp = (char *) malloc(U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES + 1);
                    if (pServerNameTmp != NULL) {
                        // Allocate memory for an HTTP instance
                        pHttpInstance = (uCellHttpInstance_t *) malloc(sizeof(uCellHttpInstance_t));
                        if (pHttpInstance != NULL) {
                            memset(pHttpInstance, 0, sizeof(*pHttpInstance));
                            pHttpInstance->profileId = profileId;
                            pHttpInstance->timeoutSeconds = timeoutSeconds;
                            pHttpInstance->pCallback = pCallback;
                            pHttpInstance->pCallbackParam = pCallbackParam;
                            atHandle = pCellInstance->atHandle;
                            // Determine if the server name given
                            // is an IP address or a domain name
                            // by processing it as an IP address
                            memset(&address, 0, sizeof(address));
                            if (uSockStringToAddress(pServerName,
                                                     &address) == 0) {
                                // We have an IP address
                                // Convert the bit that isn't a port
                                // number back into a string
                                if (uSockIpAddressToString(&(address.ipAddress),
                                                           pServerNameTmp,
                                                           U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES + 1) > 0) {
                                    // Set the server IP address
                                    errorCodeOrHandle = doUhttpString(atHandle, profileId, 0, pServerNameTmp);
                                    // The server port number is written later
                                }
                            } else {
                                // We must have a domain name,
                                // make a copy of it as we need to
                                // manipulate it
                                strncpy(pServerNameTmp, pServerName,
                                        U_CELL_HTTP_SERVER_NAME_MAX_LEN_BYTES);
                                // Grab any port number off the end
                                // and then remove it from the string
                                address.port = uSockDomainGetPort(pServerNameTmp);
                                pTmp = pUSockDomainRemovePort(pServerNameTmp);
                                // Set the domain name address
                                errorCodeOrHandle = doUhttpString(atHandle, profileId, 1, pTmp);
                            }
                            if (errorCodeOrHandle == 0) {
                                if (address.port == 0) {
                                    address.port = 80;
                                }
                                // If that went well, write the server port number
                                errorCodeOrHandle = doUhttpInteger(atHandle, profileId, 5, address.port);
                            }
                            if ((errorCodeOrHandle == 0) && (pUserName != NULL)) {
                                // Deal with credentials: user name
                                errorCodeOrHandle = doUhttpString(atHandle, profileId, 2, pUserName);
                            }
                            if ((errorCodeOrHandle == 0) && (pPassword != NULL)) {
                                // Deal with credentials: password
                                errorCodeOrHandle = doUhttpString(atHandle, profileId, 3, pPassword);
                            }
                            if (errorCodeOrHandle == 0) {
                                // Deal with credentials: set the authentication type
                                if (pUserName != NULL) {
                                    authenticationType = 1;
                                }
                                errorCodeOrHandle = doUhttpInteger(atHandle, profileId, 4, authenticationType);
                            }
                            if (errorCodeOrHandle == 0) {
                                // Finally: set the timeout.
                                errorCodeOrHandle = doUhttpInteger(atHandle, profileId, 7, timeoutSeconds);
                            }
                            if (errorCodeOrHandle == 0) {
                                // Done: hook in the URC
                                errorCodeOrHandle = uAtClientSetUrcHandler(atHandle, "+UUHTTPCR:",
                                                                           UUHTTPCR_urc, pCellInstance);
                            }
                            if (errorCodeOrHandle == 0) {
                                // Slot us into the linked list

                                U_PORT_MUTEX_LOCK(pHttpContext->linkedListMutex);

                                pHttpInstance->pNext = pHttpContext->pInstanceList;
                                pHttpContext->pInstanceList = pHttpInstance;

                                U_PORT_MUTEX_UNLOCK(pHttpContext->linkedListMutex);

                                // Return the profile ID as the handle
                                errorCodeOrHandle = profileId;
                            } else {
                                // Free memory
                                free(pHttpInstance);
                            }
                        }
                        // Free temporary memory
                        free(pServerNameTmp);
                    }
                }
            }
        }
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return errorCodeOrHandle;
}

// Shut-down the given cellular HTTP client instance.
void uCellHttpClose(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpContext_t *pHttpContext;
    uCellHttpInstance_t *pCurrent;
    uCellHttpInstance_t *pPrev = NULL;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, -1, &pCellInstance, NULL, NULL);

    if (pCellInstance != NULL) {
        pHttpContext = pCellInstance->pHttpContext;
        if (pHttpContext != NULL) {

            U_PORT_MUTEX_LOCK(pHttpContext->linkedListMutex);

            pCurrent = pHttpContext->pInstanceList;
            while (pCurrent != NULL) {
                if (httpHandle == pCurrent->profileId) {
                    // Unlink the entry
                    if (pPrev != NULL) {
                        pPrev->pNext = pCurrent->pNext;
                    } else {
                        pHttpContext->pInstanceList = pCurrent->pNext;
                    }
                    // Free memory
                    free(pCurrent);
                    pCurrent = NULL;
                } else {
                    pPrev = pCurrent;
                    pCurrent = pPrev->pNext;
                }
            }

            U_PORT_MUTEX_UNLOCK(pHttpContext->linkedListMutex);
        }
    }

    U_CELL_HTTP_EXIT_FUNCTION();
}

// Switch to HTTPS (i.e. switch on TLS-based security).
int32_t uCellHttpSetSecurityOn(uDeviceHandle_t cellHandle, int32_t httpHandle,
                               int32_t securityProfileId)
{
    return setSecurity(cellHandle, httpHandle, true, securityProfileId);
}

// Switch to HTTP (i.e. no TLS-based security).
int32_t uCellHttpSetSecurityOff(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    return setSecurity(cellHandle, httpHandle, false, -1);
}

// Determine whether HTTPS (i.e. TLS-based security) is on or not.
bool uCellHttpIsSecured(uDeviceHandle_t cellHandle, int32_t httpHandle,
                        int32_t *pSecurityProfileId)
{
    bool secured = false;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpInstance_t *pHttpInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, &pCellInstance,
                               &pHttpInstance, NULL);

    if (pHttpInstance != NULL) {
        atHandle = pCellInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UHTTP=");
        uAtClientWriteInt(atHandle, pHttpInstance->profileId);
        uAtClientWriteInt(atHandle, 6);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UHTTP:");
        // Skip the first parameter, which is just
        // our profile ID
        uAtClientSkipParameters(atHandle, 1);
        secured = uAtClientReadInt(atHandle) == 1;
        if (secured && (pSecurityProfileId != NULL)) {
            *pSecurityProfileId = uAtClientReadInt(atHandle);
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return secured;
}

// Perform an HTTP request with data for POST provided as a string.
int32_t uCellHttpRequest(uDeviceHandle_t cellHandle, int32_t httpHandle,
                         uCellHttpRequest_t requestType,
                         const char *pPath, const char *pFileNameResponse,
                         const char *pStrPost, const char *pContentTypePost)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpInstance_t *pHttpInstance = NULL;
    uAtClientHandle_t atHandle;
    int32_t httpCommand = (int32_t) requestType;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, &pCellInstance,
                               &pHttpInstance, &errorCode);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pPath != NULL) && (requestType != U_CELL_HTTP_REQUEST_PUT) &&
            ((requestType != U_CELL_HTTP_REQUEST_POST) || ((pStrPost != NULL) &&
                                                           isAllowedHttpRequestStr(pStrPost, U_CELL_HTTP_POST_REQUEST_STRING_MAX_LENGTH_BYTES) &&
                                                           (pContentTypePost != NULL) &&
                                                           isAllowedHttpRequestStr(pContentTypePost, U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES)))) {
            if (requestType == U_CELL_HTTP_REQUEST_POST) {
                // For this we use POST_DATA
                httpCommand = 5;
            }
            copyFileNameResponse(pHttpInstance, pFileNameResponse);
            atHandle = pCellInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UHTTPC=");
            uAtClientWriteInt(atHandle, pHttpInstance->profileId);
            uAtClientWriteInt(atHandle, httpCommand);
            uAtClientWriteString(atHandle, pPath, true);
            uAtClientWriteString(atHandle, pHttpInstance->fileNameResponse, true);
            if (requestType == U_CELL_HTTP_REQUEST_POST) {
                uAtClientWriteString(atHandle, pStrPost, true);
                uAtClientWriteInt(atHandle, 6);
                uAtClientWriteString(atHandle, pContentTypePost, true);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode != 0) {
                // Print what the module thinks went wrong
                printErrorCodes(atHandle, pHttpInstance->profileId);
            }
        }
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return errorCode;
}

// Perform an HTTP request with data for PUT/POST from the module's file system.
int32_t uCellHttpRequestFile(uDeviceHandle_t cellHandle, int32_t httpHandle,
                             uCellHttpRequest_t requestType,
                             const char *pPath, const char *pFileNameResponse,
                             const char *pFileNamePutPost,
                             const char *pContentTypePutPost)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpInstance_t *pHttpInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, &pCellInstance,
                               &pHttpInstance, &errorCode);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pPath != NULL) && ((int32_t) requestType >= 0) &&
            (requestType < U_CELL_HTTP_REQUEST_MAX_NUM) &&
            (((requestType != U_CELL_HTTP_REQUEST_PUT) && (requestType != U_CELL_HTTP_REQUEST_POST)) ||
             ((pFileNamePutPost != NULL) && (pContentTypePutPost != NULL) &&
              isAllowedHttpRequestStr(pContentTypePutPost,
                                      U_CELL_HTTP_CONTENT_TYPE_MAX_LENGTH_BYTES)))) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            copyFileNameResponse(pHttpInstance, pFileNameResponse);
            atHandle = pCellInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UHTTPC=");
            uAtClientWriteInt(atHandle, pHttpInstance->profileId);
            uAtClientWriteInt(atHandle, requestType);
            uAtClientWriteString(atHandle, pPath, true);
            uAtClientWriteString(atHandle, pHttpInstance->fileNameResponse, true);
            if ((requestType == U_CELL_HTTP_REQUEST_PUT) || (requestType == U_CELL_HTTP_REQUEST_POST)) {
                uAtClientWriteString(atHandle, pFileNamePutPost, true);
                uAtClientWriteInt(atHandle, 6);
                uAtClientWriteString(atHandle, pContentTypePutPost, true);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode != 0) {
                // Print what the module thinks went wrong
                printErrorCodes(atHandle, pHttpInstance->profileId);
            }
        }
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return errorCode;
}

// Get the last HTTP error code for the given HTTP instance.
int32_t uCellHttpGetLastErrorCode(uDeviceHandle_t cellHandle,
                                  int32_t httpHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pCellInstance = NULL;
    uCellHttpInstance_t *pHttpInstance = NULL;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_HTTP_ENTRY_FUNCTION(cellHandle, httpHandle, &pCellInstance,
                               &pHttpInstance, &errorCode);

    if (errorCode == 0) {
        atHandle = pCellInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UHTTPER=");
        uAtClientWriteInt(atHandle, pHttpInstance->profileId);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UHTTPER:");
        // Skip the first two parameters, which are our
        // profile ID being sent back to us and a generic
        // "error class"
        uAtClientSkipParameters(atHandle, 2);
        x = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode == 0) {
            errorCode = x;
        }
    }

    U_CELL_HTTP_EXIT_FUNCTION();

    return errorCode;
}

// End of file
