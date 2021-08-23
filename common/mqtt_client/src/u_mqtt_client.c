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
 * @brief Implementation of the u-blox MQTT client API.
 *
 * This implementation expects to call on the underlying APIs
 * of cellular, Wifi or BLE for the functions that meet the MQTT
 * client API.
 *
 * In all cases the value of handle will be taken from the
 * appropriate range in u_network_handle.h. An error from
 * BLE/Wifi/cell must be indicated by a returning a
 * negative error value; zero means success and a positive
 * number may be used to indicate a length. See the function
 * and structure definitions in u_mqtt_client.h and u_mqtt_common.h
 * for the meanings of the parameters and return values.
 *
 * IMPORTANT: parameters will be error checked before the
 * underlying APIs are called *EXCEPT* for lengths, since
 * these are general module specific.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port_os.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#include "u_cell_sec_tls.h"
#include "u_cell_mqtt.h"

#include "u_network_handle.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The last error code from pUMqttClientOpen()
 */
static uErrorCode_t gLastOpenError = U_ERROR_COMMON_SUCCESS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Start an MQTT connection using cellular.
 * The mutex for this session must be locked before this is called.
 */
int32_t cellConnect(int32_t networkHandle,
                    const uMqttClientConnection_t *pConnection,
                    const uSecurityTlsContext_t *pSecurityContext)
{
    int32_t errorCode;
    const uMqttWill_t *pWill = pConnection->pWill;

    errorCode = uCellMqttInit(networkHandle,
                              pConnection->pBrokerNameStr,
                              pConnection->pClientIdStr,
                              pConnection->pUserNameStr,
                              pConnection->pPasswordStr,
                              pConnection->pKeepGoingCallback,
                              false);

    if ((errorCode == 0) && (pConnection->localPort >= 0)) {
        // A local port has been specified, set it
        errorCode = uCellMqttSetLocalPort(networkHandle,
                                          (uint16_t) (pConnection->localPort));
    }

    if ((errorCode == 0) && (pConnection->inactivityTimeoutSeconds >= 0)) {
        // An inactivity timeout has been specified, set it
        errorCode = uCellMqttSetInactivityTimeout(networkHandle,
                                                  // Cast twice to keep Lint happy
                                                  (size_t) (int32_t) (pConnection->inactivityTimeoutSeconds));
    }

    if ((errorCode == 0) && pConnection->retain) {
        // Retention has been specified, set it
        errorCode = uCellMqttSetRetainOn(networkHandle);
    }

    if (errorCode == 0 && (pSecurityContext != NULL)) {
        // Switch on security
        errorCode = uCellMqttSetSecurityOn(networkHandle,
                                           ((uCellSecTlsContext_t *) (pSecurityContext->pNetworkSpecific))->profileId);
    }

    if ((errorCode == 0) && (pWill != NULL)) {
        // A "will" has been requested, set it
        errorCode = uCellMqttSetWill(networkHandle,
                                     pWill->pTopicNameStr,
                                     pWill->pMessage,
                                     pWill->messageSizeBytes,
                                     (uCellMqttQos_t) pWill->qos,
                                     pWill->retain);
    }

    if (errorCode == 0) {
        // If everything went well, do the actual connection
        errorCode = uCellMqttConnect(networkHandle);
        if ((errorCode == 0) && pConnection->keepAlive) {
            // "keep alive" or ping can only be set after connecting
            uCellMqttSetKeepAliveOn(networkHandle);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise an MQTT client.
uMqttClientContext_t *pUMqttClientOpen(int32_t networkHandle,
                                       const uSecurityTlsSettings_t *pSecurityTlsSettings)
{
    uMqttClientContext_t *pContext = NULL;

    gLastOpenError = U_ERROR_COMMON_NOT_SUPPORTED;
    if (U_NETWORK_HANDLE_IS_CELL(networkHandle)) {
        // For cellular, check that MQTT is supported by the
        // given module at this point.
        if (uCellMqttIsSupported(networkHandle)) {
            gLastOpenError = U_ERROR_COMMON_SUCCESS;
        }
    } else {
        // Other underlying network types may need to do
        // do something here, currently returning not
        // implemented in any case
        gLastOpenError = U_ERROR_COMMON_NOT_IMPLEMENTED;
    }

    if (gLastOpenError == U_ERROR_COMMON_SUCCESS) {
        gLastOpenError = U_ERROR_COMMON_NO_MEMORY;
        pContext = (uMqttClientContext_t *) malloc(sizeof(*pContext));
        if (pContext != NULL) {
            pContext->networkHandle = networkHandle;
            pContext->mutexHandle = NULL;
            pContext->pSecurityContext = NULL;
            pContext->totalMessagesSent = 0;
            pContext->totalMessagesReceived = 0;
            if (uPortMutexCreate((uPortMutexHandle_t *) & (pContext->mutexHandle)) == 0) {
                gLastOpenError = U_ERROR_COMMON_SUCCESS;
                if (pSecurityTlsSettings != NULL) {
                    // Call the common security layer
                    gLastOpenError = U_ERROR_COMMON_NO_MEMORY;
                    pContext->pSecurityContext = pUSecurityTlsAdd(networkHandle,
                                                                  pSecurityTlsSettings);
                    if (pContext->pSecurityContext != NULL) {
                        gLastOpenError = (uErrorCode_t) pContext->pSecurityContext->errorCode;
                    }
                }

                // Note: in the case of the underlying cellular API
                // no further action is taken at this point.  That
                // may be different for underlying BLE and Wifi APIs
                // which may need hooks into here.
            }
        }
    }

    if (gLastOpenError != U_ERROR_COMMON_SUCCESS) {
        // Recover all allocated memory if there was an error
        if (pContext != NULL) {
            if (pContext->mutexHandle != NULL) {
                uPortMutexDelete(pContext->mutexHandle);
            }
            if (pContext->pSecurityContext != NULL) {
                uSecurityTlsRemove(pContext->pSecurityContext);
            }
            free(pContext);
            pContext = NULL;
        }
    }

    return pContext;
}

// Get the last error code from pUMqttClientOpen().
int32_t uMqttClientOpenResetLastError()
{
    int32_t lastOpenError = (int32_t) gLastOpenError;
    gLastOpenError = U_ERROR_COMMON_SUCCESS;
    return lastOpenError;
}

// Close an MQTT client.
void uMqttClientClose(uMqttClientContext_t *pContext)
{
    if (pContext != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            uCellMqttDeinit(pContext->networkHandle);
        }

        if (pContext->pSecurityContext != NULL) {
            // Free the security context
            uSecurityTlsRemove(pContext->pSecurityContext);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        uPortMutexDelete((uPortMutexHandle_t) (pContext->mutexHandle));
        free(pContext);
    }
}

// Start an MQTT session.
int32_t uMqttClientConnect(const uMqttClientContext_t *pContext,
                           const uMqttClientConnection_t *pConnection)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pConnection != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = cellConnect(pContext->networkHandle,
                                    pConnection,
                                    pContext->pSecurityContext);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Stop an MQTT session.
int32_t uMqttClientDisconnect(const uMqttClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttDisconnect(pContext->networkHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Determine whether an MQTT session is active or not.
bool uMqttClientIsConnected(const uMqttClientContext_t *pContext)
{
    bool isConnected = false;

    if (pContext != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            isConnected = uCellMqttIsConnected(pContext->networkHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return isConnected;
}

// Publish an MQTT message.
int32_t uMqttClientPublish(uMqttClientContext_t *pContext,
                           const char *pTopicNameStr,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicNameStr != NULL) &&
        (pMessage != NULL) && (messageSizeBytes > 0)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttPublish(pContext->networkHandle,
                                         pTopicNameStr,
                                         pMessage, messageSizeBytes,
                                         (uCellMqttQos_t) qos, retain);
            if (errorCode == 0) {
                pContext->totalMessagesSent++;
            }
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Subscribe to an MQTT topic.
int32_t uMqttClientSubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr,
                             uMqttQos_t maxQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicFilterStr != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttSubscribe(pContext->networkHandle,
                                           pTopicFilterStr,
                                           (uCellMqttQos_t) maxQos);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Unsubscribe from an MQTT topic.
int32_t uMqttClientUnsubscribe(const uMqttClientContext_t *pContext,
                               const char *pTopicFilterStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicFilterStr != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttUnsubscribe(pContext->networkHandle,
                                             pTopicFilterStr);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Set a callback to be called on new message arrival.
int32_t uMqttClientSetMessageCallback(const uMqttClientContext_t *pContext,
                                      void (*pCallback) (int32_t, void *),
                                      void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttSetMessageCallback(pContext->networkHandle,
                                                    pCallback,
                                                    pCallbackParam);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Get the current number of unread messages.
int32_t uMqttClientGetUnread(const uMqttClientContext_t *pContext)
{
    int32_t errorCodeOrUnread = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCodeOrUnread = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCodeOrUnread = uCellMqttGetUnread(pContext->networkHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCodeOrUnread;
}

// Read an MQTT message.
int32_t uMqttClientMessageRead(uMqttClientContext_t *pContext,
                               char *pTopicNameStr,
                               size_t topicNameSizeBytes,
                               char *pMessage,
                               size_t *pMessageSizeBytes,
                               uMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicNameStr != NULL) &&
        (topicNameSizeBytes > 0) &&
        ((pMessageSizeBytes != NULL) || (pMessage == NULL))) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttMessageRead(pContext->networkHandle,
                                             pTopicNameStr,
                                             topicNameSizeBytes,
                                             pMessage,
                                             pMessageSizeBytes,
                                             (uCellMqttQos_t *) pQos);
            if (errorCode == 0) {
                pContext->totalMessagesReceived++;
            }
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Get the last MQTT error code.
int32_t uMqttClientGetLastErrorCode(const uMqttClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_NETWORK_HANDLE_IS_CELL(pContext->networkHandle)) {
            errorCode = uCellMqttGetLastErrorCode(pContext->networkHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

int32_t uMqttClientGetTotalMessagesSent(const uMqttClientContext_t *pContext)
{
    int32_t errorCodeOrSentMessages = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCodeOrSentMessages = pContext->totalMessagesSent;
    }

    return errorCodeOrSentMessages;
}

int32_t uMqttClientGetTotalMessagesReceived(const uMqttClientContext_t *pContext)
{
    int32_t errorCodeOrReceivedMessages = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCodeOrReceivedMessages = pContext->totalMessagesReceived;
    }

    return errorCodeOrReceivedMessages;
}

// End of file
