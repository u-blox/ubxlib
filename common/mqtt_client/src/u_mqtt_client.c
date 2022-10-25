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
 * @brief Implementation of the u-blox MQTT/MQTT-SN client API.
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

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen(), strncpy()

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#include "u_cell_sec_tls.h"
#include "u_cell_mqtt.h"
#include "u_wifi_mqtt.h"

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
int32_t cellConnect(uDeviceHandle_t devHandle,
                    const uMqttClientConnection_t *pConnection,
                    const uSecurityTlsContext_t *pSecurityContext,
                    bool doNotConnect)
{
    int32_t errorCode;
    const uMqttWill_t *pWill = pConnection->pWill;

    errorCode = uCellMqttInit(devHandle,
                              pConnection->pBrokerNameStr,
                              pConnection->pClientIdStr,
                              pConnection->pUserNameStr,
                              pConnection->pPasswordStr,
                              pConnection->pKeepGoingCallback,
                              pConnection->mqttSn);

    if ((errorCode == 0) && (pConnection->localPort >= 0)) {
        // A local port has been specified, set it
        errorCode = uCellMqttSetLocalPort(devHandle,
                                          (uint16_t) (pConnection->localPort));
    }

    if ((errorCode == 0) && (pConnection->inactivityTimeoutSeconds >= 0)) {
        // An inactivity timeout has been specified, set it
        errorCode = uCellMqttSetInactivityTimeout(devHandle,
                                                  // Cast twice to keep Lint happy
                                                  (size_t) (int32_t) (pConnection->inactivityTimeoutSeconds));
    }

    if ((errorCode == 0) && pConnection->retain) {
        // Retention has been specified, set it
        errorCode = uCellMqttSetRetainOn(devHandle);
    }

    if (errorCode == 0 && (pSecurityContext != NULL)) {
        // Switch on security
        errorCode = uCellMqttSetSecurityOn(devHandle,
                                           ((uCellSecTlsContext_t *) (pSecurityContext->pNetworkSpecific))->profileId);
    }

    if ((errorCode == 0) && (pWill != NULL)) {
        // A "will" has been requested, set it
        errorCode = uCellMqttSetWill(devHandle,
                                     pWill->pTopicNameStr,
                                     pWill->pMessage,
                                     pWill->messageSizeBytes,
                                     (uCellMqttQos_t) pWill->qos,
                                     pWill->retain);
    }

    if ((errorCode == 0) && (!doNotConnect)) {
        // If everything went well, do the actual connection
        errorCode = uCellMqttConnect(devHandle);
        if ((errorCode == 0) && pConnection->keepAlive) {
            // "keep alive" or ping can only be set after connecting
            uCellMqttSetKeepAliveOn(devHandle);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT AND MQTT-SN
 * -------------------------------------------------------------- */

// Initialise an MQTT client.
uMqttClientContext_t *pUMqttClientOpen(uDeviceHandle_t devHandle,
                                       const uSecurityTlsSettings_t *pSecurityTlsSettings)
{
    uMqttClientContext_t *pContext = NULL;
    void *pPriv = NULL;

    gLastOpenError = U_ERROR_COMMON_NOT_SUPPORTED;
    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        // For cellular, check that MQTT is supported by the
        // given module at this point.
        // Note that this implies that a module that supports MQTT-SN
        // also supports MQTT, which is currently the case.
        if (uCellMqttIsSupported(devHandle)) {
            gLastOpenError = U_ERROR_COMMON_SUCCESS;
        }
    } else if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
        // For WiFi
        if (uWifiMqttInit(devHandle, &pPriv) == 0) {
            gLastOpenError = U_ERROR_COMMON_SUCCESS;
        }
    } else {
        // Other underlying network types may need to
        // do something here, currently returning not
        // implemented in any case
        gLastOpenError = U_ERROR_COMMON_NOT_IMPLEMENTED;

    }

    if (gLastOpenError == U_ERROR_COMMON_SUCCESS) {
        gLastOpenError = U_ERROR_COMMON_NO_MEMORY;
        pContext = (uMqttClientContext_t *) pUPortMalloc(sizeof(*pContext));
        if (pContext != NULL) {
            pContext->devHandle = devHandle;
            pContext->mutexHandle = NULL;
            pContext->pSecurityContext = NULL;
            pContext->totalMessagesSent = 0;
            pContext->totalMessagesReceived = 0;
            pContext->pPriv = pPriv;
            if (uPortMutexCreate((uPortMutexHandle_t *) & (pContext->mutexHandle)) == 0) {
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
            uPortFree(pContext);
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

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            uCellMqttDeinit(pContext->devHandle);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            uWifiMqttClose(pContext);
        }

        if (pContext->pSecurityContext != NULL) {
            // Free the security context
            uSecurityTlsRemove(pContext->pSecurityContext);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        uPortMutexDelete((uPortMutexHandle_t) (pContext->mutexHandle));
        uPortFree(pContext);
    }
}

// Start an MQTT session.
int32_t uMqttClientConnect(uMqttClientContext_t *pContext,
                           const uMqttClientConnection_t *pConnection)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pConnection != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = cellConnect(pContext->devHandle,
                                    pConnection,
                                    pContext->pSecurityContext, false);
            // For cellular MQTT connections the pContext->pPriv is not
            // used, however for MQTT-SN the "will" data may be updated
            // and so a pointer to the "will" data is hooked into
            // pContext->pPriv so that it is carred around with the
            // context and can be updated.
            pContext->pPriv = (void *) pConnection->pWill;
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {

            errorCode = uWifiMqttConnect(pContext, pConnection);
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
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttDisconnect(pContext->devHandle);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttDisconnect(pContext);
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

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            isConnected = uCellMqttIsConnected(pContext->devHandle);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            isConnected = uWifiMqttIsConnected(pContext);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return isConnected;
}

// Set a callback to be called on new message arrival.
int32_t uMqttClientSetMessageCallback(const uMqttClientContext_t *pContext,
                                      void (*pCallback) (int32_t, void *),
                                      void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSetMessageCallback(pContext->devHandle,
                                                    pCallback,
                                                    pCallbackParam);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttSetMessageCallback(pContext,
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
        errorCodeOrUnread = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCodeOrUnread = uCellMqttGetUnread(pContext->devHandle);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCodeOrUnread = uWifiMqttGetUnread(pContext);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCodeOrUnread;
}

// Get the last MQTT error code.
int32_t uMqttClientGetLastErrorCode(const uMqttClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttGetLastErrorCode(pContext->devHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Get the total number of message sent by yhe MQTT client.
int32_t uMqttClientGetTotalMessagesSent(const uMqttClientContext_t *pContext)
{
    int32_t errorCodeOrSentMessages = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCodeOrSentMessages = pContext->totalMessagesSent;
    }

    return errorCodeOrSentMessages;
}

// Get the total number of messages received and read by the MQTT client.
int32_t uMqttClientGetTotalMessagesReceived(const uMqttClientContext_t *pContext)
{
    int32_t errorCodeOrReceivedMessages = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCodeOrReceivedMessages = pContext->totalMessagesReceived;
    }

    return errorCodeOrReceivedMessages;
}

// Set a callback for when the MQTT connection is dropped.
int32_t uMqttClientSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                         void (*pCallback) (int32_t, void *),
                                         void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSetDisconnectCallback(pContext->devHandle,
                                                       pCallback,
                                                       pCallbackParam);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttSetDisconnectCallback(pContext,
                                                       pCallback,
                                                       pCallbackParam);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT ONLY
 * -------------------------------------------------------------- */

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
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttPublish(pContext->devHandle,
                                         pTopicNameStr,
                                         pMessage, messageSizeBytes,
                                         (uCellMqttQos_t) qos, retain);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttPublish(pContext,
                                         pTopicNameStr,
                                         pMessage, messageSizeBytes,
                                         (uMqttQos_t)qos, retain);
        }
        if (errorCode == 0) {
            pContext->totalMessagesSent++;
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
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSubscribe(pContext->devHandle,
                                           pTopicFilterStr,
                                           (uCellMqttQos_t) maxQos);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttSubscribe(pContext,
                                           pTopicFilterStr,
                                           (uMqttQos_t)maxQos);
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
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttUnsubscribe(pContext->devHandle,
                                             pTopicFilterStr);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttUnsubscribe(pContext, pTopicFilterStr);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
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
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttMessageRead(pContext->devHandle,
                                             pTopicNameStr,
                                             topicNameSizeBytes,
                                             pMessage,
                                             pMessageSizeBytes,
                                             (uCellMqttQos_t *) pQos);
        } else if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCode = uWifiMqttMessageRead(pContext,
                                             pTopicNameStr,
                                             topicNameSizeBytes,
                                             pMessage,
                                             pMessageSizeBytes,
                                             (uMqttQos_t *) pQos);
        }
        if (errorCode == 0) {
            pContext->totalMessagesReceived++;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT-SN ONLY
 * -------------------------------------------------------------- */

// Determine if MQTT-SN is supported.
bool uMqttClientSnIsSupported(const uMqttClientContext_t *pContext)
{
    bool isSupported = false;

    if (pContext != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            isSupported = uCellMqttSnIsSupported(pContext->devHandle);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return isSupported;
}

// Connect with the option of not connecting.
int32_t uMqttClientSnConnect(uMqttClientContext_t *pContext,
                             const uMqttClientConnection_t *pConnection,
                             bool doNotConnect)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pConnection != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = cellConnect(pContext->devHandle,
                                    pConnection,
                                    pContext->pSecurityContext,
                                    doNotConnect);
            // For cellular MQTT connections the pContext->pPriv is not
            // used, however for MQTT-SN the "will" data may be updated
            // and so a pointer to the "will" data is hooked into
            // pContext->pPriv so that it is carred around with the
            // context and can be updated.
            pContext->pPriv = (void *) pConnection->pWill;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Populate an MQTT-SN topic name with a predefined topic ID.
int32_t uMqttClientSnSetTopicIdPredefined(uint16_t topicId,
                                          uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTopicName != NULL) {
        pTopicName->name.id = topicId;
        pTopicName->type = U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Populate an MQTT-SN topic name with a short topic name string.
int32_t uMqttClientSnSetTopicNameShort(const char *pTopicNameShortStr,
                                       uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pTopicName != NULL) && (pTopicNameShortStr != NULL) &&
        (strlen(pTopicNameShortStr) == sizeof(pTopicName->name.nameShort))) {
        strncpy(pTopicName->name.nameShort, pTopicNameShortStr,
                sizeof(pTopicName->name.nameShort));
        pTopicName->type = U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Get the type of an MQTT-SN topic name.
uMqttSnTopicNameType_t uMqttClientSnGetTopicNameType(const uMqttSnTopicName_t *pTopicName)
{
    uMqttSnTopicNameType_t errorCodeOrType = (uMqttSnTopicNameType_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTopicName != NULL) {
        errorCodeOrType = pTopicName->type;
    }

    return errorCodeOrType;
}

// Get the ID from an MQTT-SN topic name.
int32_t uMqttClientSnGetTopicId(const uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCodeOrId = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pTopicName != NULL) &&
        ((pTopicName->type == U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL) ||
         (pTopicName->type == U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED))) {
        errorCodeOrId = (int32_t) pTopicName->name.id;
    }

    return errorCodeOrId;
}

// Get the short name from an MQTT-SN topic name.
int32_t uMqttClientSnGetTopicNameShort(const uMqttSnTopicName_t *pTopicName,
                                       char *pTopicNameShortStr)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTopicName != NULL) {
        if (pTopicName->type == U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT) {
            // Must use memcpy() as nameShort does not include a terminator
            memcpy(pTopicNameShortStr, pTopicName->name.nameShort,
                   sizeof(pTopicName->name.nameShort));
            // Ensure a terminator
            *(pTopicNameShortStr + sizeof(pTopicName->name.nameShort)) = 0;
            errorCodeOrLength = (int32_t) strlen(pTopicNameShortStr);
        }
    }

    return errorCodeOrLength;
}

// Ask the MQTT broker for an MQTT-SN topic name for the given normal MQTT topic.
int32_t uMqttClientSnRegisterNormalTopic(const uMqttClientContext_t *pContext,
                                         const char *pTopicNameStr,
                                         uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicNameStr != NULL) && (pTopicName != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnRegisterNormalTopic(pContext->devHandle,
                                                       pTopicNameStr,
                                                       //lint -e(740) Suppress unusual pointer cast
                                                       (uCellMqttSnTopicName_t *) pTopicName);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Publish a message.
int32_t uMqttClientSnPublish(uMqttClientContext_t *pContext,
                             const uMqttSnTopicName_t *pTopicName,
                             const char *pMessage,
                             size_t messageSizeBytes,
                             uMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicName != NULL) &&
        (pMessage != NULL) && (messageSizeBytes > 0)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnPublish(pContext->devHandle,
                                           //lint -e(740) Suppress unusual pointer cast
                                           (const uCellMqttSnTopicName_t *) pTopicName,
                                           pMessage, messageSizeBytes,
                                           (uCellMqttQos_t) qos, retain);
        }
        if (errorCode == 0) {
            pContext->totalMessagesSent++;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Subscribe to an MQTT-SN topic.
int32_t uMqttClientSnSubscribe(const uMqttClientContext_t *pContext,
                               const uMqttSnTopicName_t *pTopicName,
                               uMqttQos_t maxQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicName != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnSubscribe(pContext->devHandle,
                                             //lint -e(740) Suppress unusual pointer cast
                                             (const uCellMqttSnTopicName_t *) pTopicName,
                                             (uCellMqttQos_t) maxQos);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Subscribe to a normal MQTT topic.
int32_t uMqttClientSnSubscribeNormalTopic(const uMqttClientContext_t *pContext,
                                          const char *pTopicFilterStr,
                                          uMqttQos_t maxQos,
                                          uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicFilterStr != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnSubscribeNormalTopic(pContext->devHandle,
                                                        pTopicFilterStr,
                                                        (uCellMqttQos_t) maxQos,
                                                        //lint -e(740) Suppress unusual pointer cast
                                                        (uCellMqttSnTopicName_t *) pTopicName);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Unsubscribe from an MQTT-SN topic.
int32_t uMqttClientSnUnsubscribe(const uMqttClientContext_t *pContext,
                                 const uMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicName != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnUnsubscribe(pContext->devHandle,
                                               //lint -e(740) Suppress unusual pointer cast
                                               (const uCellMqttSnTopicName_t *) pTopicName);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Unsubscribe from a normal MQTT topic.
int32_t uMqttClientSnUnsubscribeNormalTopic(const uMqttClientContext_t *pContext,
                                            const char *pTopicFilterStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicFilterStr != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnUnsubscribeNormalTopic(pContext->devHandle,
                                                          pTopicFilterStr);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Read a message.
int32_t uMqttClientSnMessageRead(uMqttClientContext_t *pContext,
                                 uMqttSnTopicName_t *pTopicName,
                                 char *pMessage,
                                 size_t *pMessageSizeBytes,
                                 uMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pContext != NULL) && (pTopicName != NULL) &&
        ((pMessageSizeBytes != NULL) || (pMessage == NULL))) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnMessageRead(pContext->devHandle,
                                               //lint -e(740) Suppress unusual pointer cast
                                               (uCellMqttSnTopicName_t *) pTopicName,
                                               pMessage, pMessageSizeBytes,
                                               (uCellMqttQos_t *) pQos);
        }
        if (errorCode == 0) {
            pContext->totalMessagesReceived++;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Notify the MQTT-SN broker that the "will" message has been updated.
int32_t uMqttClientSnWillMessageUpdate(const uMqttClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uMqttWill_t *pWill;

    // For cellular MQTT-SN connections the pContext->pPriv is used
    // to carry the "will" data around.
    if ((pContext != NULL) && (pContext->pPriv != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pWill = (const uMqttWill_t *) pContext->pPriv;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnSetWillMessaage(pContext->devHandle,
                                                   pWill->pMessage,
                                                   pWill->messageSizeBytes);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// Notify the MQTT-SN broker that the parameters of the "will" message
// have been updated.
int32_t uMqttClientSnWillParametersUpdate(const uMqttClientContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uMqttWill_t *pWill;

    // For cellular MQTT-SN connections the pContext->pPriv is used
    // to carry the "will" data around.
    if ((pContext != NULL) && (pContext->pPriv != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pWill = (const uMqttWill_t *) pContext->pPriv;

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) (pContext->mutexHandle));

        if (U_DEVICE_IS_TYPE(pContext->devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellMqttSnSetWillParameters(pContext->devHandle,
                                                     pWill->pTopicNameStr,
                                                     (uCellMqttQos_t) pWill->qos,
                                                     pWill->retain);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) (pContext->mutexHandle));
    }

    return errorCode;
}

// End of file
