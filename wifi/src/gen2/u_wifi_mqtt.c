
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
 * @brief Implementation of the u-blox MQTT client API for WiFi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "ctype.h"     // isdigit()
#include "string.h"    // memset(), strncpy(), strtok_r()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"
#include "u_wifi_mqtt.h"
#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"
#include "u_short_range_sec_tls.h"
#include "u_linked_list.h"

#include "u_cx_urc.h"
#include "u_cx_mqtt.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */

#define U_MQTT_CONNECT_TIMEOUT_S 5
#define MQTT_ID 0
#define EMPTY_IF_NULL(str) (str == NULL ? "" : str)
#define FAIL_IF_NULL(p) if (p == NULL) { return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER; }
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    uDeviceHandle_t devHandle;
    bool connected;
    uPortSemaphoreHandle_t semaphore;
    int32_t unReadCnt;
    void (*pMessageAvailableCallback)(int32_t, void *);
    void *pMessageAvailableCallbackParam;
    void (*pDisconnectCallback)(int32_t, void *);
    void *pDisconnectParam;
} uMqttDeviceState_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * ------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ------------------------------------------------------------- */

static uMqttDeviceState_t *pGetMqttDeviceState(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    return (pInstance != NULL ? (uMqttDeviceState_t *)pInstance->pMqttContext : NULL);
}

static void connectCallback(struct uCxHandle *pUcxHandle, int32_t mqttId)
{
    (void)mqttId;
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    uMqttDeviceState_t *pMqttDeviceState = pGetMqttDeviceState(pInstance->devHandle);
    if (pMqttDeviceState != NULL) {
        pMqttDeviceState->connected = true;
        uPortSemaphoreGive(pMqttDeviceState->semaphore);
    }
}

static void disconnectCallback(struct uCxHandle *pUcxHandle, int32_t mqttId,
                               int32_t disconnectReason)
{
    (void)mqttId;
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    uMqttDeviceState_t *pMqttDeviceState = pGetMqttDeviceState(pInstance->devHandle);
    if (pMqttDeviceState != NULL) {
        pMqttDeviceState->connected = false;
        if (pMqttDeviceState->pDisconnectCallback != NULL) {
            pMqttDeviceState->pDisconnectCallback(disconnectReason,
                                                  pMqttDeviceState->pDisconnectParam);
        }
    }
}

static void dataAvailableCallback(struct uCxHandle *pUcxHandle, int32_t mqttId, int32_t messageLen)
{
    (void)mqttId;
    (void)messageLen;
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    uMqttDeviceState_t *pMqttDeviceState = pGetMqttDeviceState(pInstance->devHandle);
    if (pMqttDeviceState != NULL) {
        pMqttDeviceState->unReadCnt++;
        if (pMqttDeviceState->pMessageAvailableCallback != NULL) {
            pMqttDeviceState->pMessageAvailableCallback(pMqttDeviceState->unReadCnt,
                                                        pMqttDeviceState->pMessageAvailableCallbackParam);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiMqttPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uWifiMqttInit(uDeviceHandle_t devHandle, void **ppMqttSession)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    uMqttDeviceState_t *pMqttDeviceState = pGetMqttDeviceState(devHandle);
    if (pMqttDeviceState == NULL) {
        uShortRangePrivateInstance_t *pInstance =
            pUShortRangePrivateGetInstance(devHandle);
        pMqttDeviceState = pUPortMalloc(sizeof(uMqttDeviceState_t));
        if (pMqttDeviceState != NULL) {
            memset(pMqttDeviceState, 0, sizeof(uMqttDeviceState_t));
            pMqttDeviceState->devHandle = devHandle;
            pMqttDeviceState->unReadCnt = 0;
            errorCode = uPortSemaphoreCreate(&(pMqttDeviceState->semaphore), 0, 1);
            if (errorCode == 0) {
                pInstance->pMqttContext = (void *)pMqttDeviceState;
                *ppMqttSession = (void *)pMqttDeviceState;
            } else {
                uPortFree(pMqttDeviceState);
            }
        } else {
            errorCode = U_ERROR_COMMON_NO_MEMORY;
        }
    }
    return errorCode;
}

int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                         const uMqttClientConnection_t *pConnection)
{
    FAIL_IF_NULL(pContext)
    FAIL_IF_NULL(pConnection)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if ((pMqttDeviceState != NULL) && (pUcxHandle != NULL) &&
        (pConnection->pBrokerNameStr != NULL) &&
        (strlen(pConnection->pBrokerNameStr) > 0)) {
        int32_t port = pConnection->localPort;
        if (port == -1) {
            port = pContext->pSecurityContext != NULL ? U_MQTT_BROKER_PORT_SECURE
                   : U_MQTT_BROKER_PORT_UNSECURE;
        }
        errorCode = (int32_t)(pMqttDeviceState->connected ? U_ERROR_COMMON_BUSY
                              : U_ERROR_COMMON_SUCCESS);
        if (errorCode == 0) {
            errorCode = uCxMqttSetConnectionParams6(pUcxHandle, MQTT_ID,
                                                    pConnection->pBrokerNameStr, port,
                                                    EMPTY_IF_NULL(pConnection->pClientIdStr),
                                                    EMPTY_IF_NULL(pConnection->pUserNameStr),
                                                    EMPTY_IF_NULL(pConnection->pPasswordStr));
        }
        if ((errorCode == 0) && (pConnection->keepAlive)) {
            errorCode = uCxMqttSetKeepAlive(pUcxHandle, MQTT_ID,
                                            pConnection->inactivityTimeoutSeconds);
        }
        if ((errorCode == 0) && (pConnection->pWill != NULL)) {
            errorCode = uCxMqttSetLastWillAndTestament5(
                            pUcxHandle, MQTT_ID, pConnection->pWill->pTopicNameStr,
                            pConnection->pWill->pMessage, pConnection->pWill->qos,
                            pConnection->pWill->retain);
        }
        if ((errorCode == 0) && (pContext->pSecurityContext != NULL)) {
            uShortRangeSecTlsContext_t *pMqttTlsContext =
                (uShortRangeSecTlsContext_t *)pContext->pSecurityContext->pNetworkSpecific;
            if ((pMqttTlsContext->pClientCertificateName != NULL &&
                 (pMqttTlsContext->pClientPrivateKeyName != NULL))) {
                errorCode = uCxMqttSetTlsConfig5(pUcxHandle, MQTT_ID,
                                                 pMqttTlsContext->tlsVersionMin,
                                                 pMqttTlsContext->pRootCaCertificateName,
                                                 pMqttTlsContext->pClientCertificateName,
                                                 pMqttTlsContext->pClientPrivateKeyName);
            } else {
                errorCode = uCxMqttSetTlsConfig3(pUcxHandle, MQTT_ID, 1, //pMqttTlsContext->tlsVersionMin,
                                                 pMqttTlsContext->pRootCaCertificateName);
            }
        }
        if (errorCode == 0) {
            uCxMqttRegisterConnect(pUcxHandle, connectCallback);
            uCxMqttRegisterDisconnect(pUcxHandle, disconnectCallback);
            uCxMqttRegisterDataAvailable(pUcxHandle, dataAvailableCallback);
            errorCode = uCxMqttConnect(pUcxHandle, MQTT_ID);
            if (errorCode == 0) {
                errorCode = uPortSemaphoreTryTake(pMqttDeviceState->semaphore,
                                                  U_MQTT_CONNECT_TIMEOUT_S * 1000);
                if (errorCode != 0) {
                    disconnectCallback(pUcxHandle, MQTT_ID,
                                       (int32_t)U_ERROR_COMMON_TIMEOUT);
                    uCxMqttRegisterConnect(pUcxHandle, NULL);
                    uCxMqttRegisterDisconnect(pUcxHandle, NULL);
                    uCxMqttRegisterDataAvailable(pUcxHandle, NULL);
                }
            }
        }
    }
    return errorCode;
}

int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    if (pMqttDeviceState != NULL) {
        pMqttDeviceState->pMessageAvailableCallback = pCallback;
        pMqttDeviceState->pMessageAvailableCallbackParam = pCallbackParam;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiMqttSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    if (pMqttDeviceState != NULL) {
        pMqttDeviceState->pDisconnectCallback = pCallback;
        pMqttDeviceState->pDisconnectParam = pCallbackParam;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiMqttPublish(const uMqttClientContext_t *pContext,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uMqttQos_t qos,
                         bool retain)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if (pUcxHandle != NULL) {
        if (pMessage != NULL) {
            errorCode = uCxMqttPublish(pUcxHandle, MQTT_ID, (uQos_t)qos, (uRetain_t)retain,
                                       pTopicNameStr, (uint8_t *)pMessage, messageSizeBytes);
        } else if (messageSizeBytes == 0) {
            errorCode = 0;
        }
    }
    return errorCode;
}

int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                           const char *pTopicFilterStr,
                           uMqttQos_t maxQos)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCodeOrQos = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if (pUcxHandle != NULL) {
        errorCodeOrQos = uCxMqttSubscribe4(pUcxHandle, MQTT_ID,
                                           U_SUBSCRIBE_ACTION_SUBSCRIBE, pTopicFilterStr,
                                           (uQos_t)maxQos);
        if (errorCodeOrQos == 0) {
            errorCodeOrQos = (int32_t)maxQos;
        }
    }
    return errorCodeOrQos;
}

int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if (pUcxHandle != NULL) {
        errorCode =
            uCxMqttSubscribe3(pUcxHandle, MQTT_ID,
                              U_SUBSCRIBE_ACTION_UNSUBSCRIBE, pTopicFilterStr);
    }
    return errorCode;
}

int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxMqttDisconnect(pUcxHandle, MQTT_ID);
        // Wait for confirmation
        if (errorCode == 0) {
            int32_t cnt = 0;
            while (uWifiMqttIsConnected(pContext) && cnt++ < 5) {
                uPortTaskBlock(1000);
            }
        }
    }
    return errorCode;
}

void uWifiMqttClose(uMqttClientContext_t *pContext)
{
    if (pContext != NULL) {
        uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
        if (pMqttDeviceState != NULL) {
            if (pMqttDeviceState->semaphore != NULL) {
                uPortSemaphoreDelete(pMqttDeviceState->semaphore);
            }
            uShortRangePrivateInstance_t *pInstance =
                pUShortRangePrivateGetInstance(pMqttDeviceState->devHandle);
            if (pInstance != NULL) {
                // Clear instance pointer as well
                pInstance->pMqttContext = NULL;
            }
            uPortFree(pMqttDeviceState);
            pContext->pPriv = NULL;
        }
    }
}

int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCodeOrCount = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    if (pMqttDeviceState != NULL) {
        errorCodeOrCount = pMqttDeviceState->unReadCnt;
    }
    return errorCodeOrCount;
}

int32_t uWifiMqttMessageRead(const uMqttClientContext_t *pContext,
                             char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage,
                             size_t *pMessageSizeBytes,
                             uMqttQos_t *pQos)
{
    FAIL_IF_NULL(pContext)
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(pContext->devHandle);
    if ((pMqttDeviceState != NULL) && (pUcxHandle != NULL)) {
        const char *topic;
        uint32_t len = uCxMqttReadBegin(pUcxHandle, MQTT_ID, (uint8_t *)pMessage, *pMessageSizeBytes,
                                        &topic);
        if (len > 0) {
            pMqttDeviceState->unReadCnt--;
            if (pQos != NULL) {
                *pQos = 0; // Not available in the response
            }
            memset(pTopicNameStr, 0, topicNameSizeBytes);
            strncpy(pTopicNameStr, topic, topicNameSizeBytes - 1);
            *pMessageSizeBytes = len;
        }
        errorCode = uCxEnd(pUcxHandle);
    }
    return (errorCode == 0 ? errorCode : U_ERROR_COMMON_EMPTY);
}

bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext)
{
    bool isConnected = false;
    uMqttDeviceState_t *pMqttDeviceState = (uMqttDeviceState_t *)pContext->pPriv;
    if (pMqttDeviceState != NULL) {
        isConnected = pMqttDeviceState->connected;
    }
    return isConnected;
}

//End of file
