
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
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"
#include "u_port_os.h"

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

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */
#define U_WIFI_MQTT_DATA_EVENT_STACK_SIZE 1536
#define U_WIFI_MQTT_DATA_EVENT_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)

typedef struct uWifiMqttTopic_t {
    char *pTopicStr;
    int32_t edmChannel;
    int32_t peerHandle;
    bool isTopicUnsubscribed;
    bool isPublish;
    uMqttQos_t qos;
    bool retain;
    struct uWifiMqttTopic_t *pNext;
} uWifiMqttTopic_t;

typedef struct uWifiMqttTopicList_t {
    uWifiMqttTopic_t *pHead;
    uWifiMqttTopic_t *pTail;
} uWifiMqttTopicList_t;

typedef struct uWifiMqttSession_t {
    char *pBrokerNameStr;
    char *pClientIdStr;
    char *pUserNameStr;
    char *pPasswordStr;
    bool isConnected;
    bool keepAlive;
    uShortRangePktList_t rxPkt;
    uWifiMqttTopicList_t topicList;
    int32_t sessionHandle;
    uAtClientHandle_t atHandle;
    int32_t localPort;
    int32_t unreadMsgsCount;
    uPortSemaphoreHandle_t semaphore;
    void *pCbParam;
    void (*pDataCb)(int32_t unreadMsgsCount, void *pCbParam);
    void (*pDisconnectCb)(int32_t status, void *pCbParam);
} uWifiMqttSession_t;

typedef struct {
    uWifiMqttSession_t *pMqttSession;
    void *pCbParam;
    void (*pDataCb)(int32_t unreadMsgsCount, void *pCbParam);
    int32_t disconnStatus;
    void (*pDisconnectCb)(int32_t disconnStatus, void *pCbParam);
} uCallbackEvent_t;

static uWifiMqttSession_t gMqttSessions[U_WIFI_MQTT_MAX_NUM_CONNECTIONS];
static uPortMutexHandle_t gMqttSessionMutex = NULL;
static int32_t gCallbackQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
static int32_t gEdmChannel = -1;

/**
 * Fetch the topic string in a given MQTT session associated to particular EDM channel
 */
static char *getTopicStrForEdmChannel(uWifiMqttSession_t *pMqttSession, int32_t edmChannel)
{
    uWifiMqttTopic_t *pTopic;
    char *pTopicNameStr = NULL;

    for (pTopic = pMqttSession->topicList.pHead; pTopic != NULL; pTopic = pTopic->pNext) {

        if (pTopic->edmChannel == edmChannel) {

            pTopicNameStr = pTopic->pTopicStr;

        }
    }

    return pTopicNameStr;
}

/**
 * Fetch the topic object in a given MQTT session
 */
static uWifiMqttTopic_t *findTopic (uWifiMqttSession_t *pMqttSession, const char *pTopicStr,
                                    bool isPublish)
{
    uWifiMqttTopic_t *pTemp;
    uWifiMqttTopic_t *pFoundTopic = NULL;

    for (pTemp = pMqttSession->topicList.pHead; pTemp != NULL; pTemp = pTemp->pNext) {
        //lint -save -e731
        if ((strcmp(pTemp->pTopicStr, pTopicStr) == 0) && (isPublish == pTemp->isPublish)) {
            pFoundTopic = pTemp;
        }
        //lint -restore
    }

    return pFoundTopic;
}

/**
 * Allocate topic object and associate it to a given MQTT session
 */
static uWifiMqttTopic_t *pAllocateMqttTopic (uWifiMqttSession_t *pMqttSession, bool isPublish)
{
    uWifiMqttTopic_t *pTopic = NULL;

    if (pMqttSession != NULL) {

        pTopic = (uWifiMqttTopic_t *)pUPortMalloc(sizeof(uWifiMqttTopic_t));

        if (pTopic != NULL) {

            pTopic->pNext = NULL;

            if (pMqttSession->topicList.pHead == NULL) {

                pMqttSession->topicList.pHead = pTopic;

            } else {

                pMqttSession->topicList.pTail->pNext = pTopic;
            }

            pMqttSession->topicList.pTail = pTopic;

            pTopic->peerHandle = -1;
            pTopic->edmChannel = -1;
            pTopic->isTopicUnsubscribed = false;
            pTopic->isPublish = isPublish;
        }
    }

    return pTopic;
}
/**
 * Free a specific topic object associated to given MQTT session
 */
static void freeMqttTopic(uWifiMqttSession_t *pMqttSession, uWifiMqttTopic_t *pTopic)
{
    uWifiMqttTopic_t *pPrev;
    uWifiMqttTopic_t *pCurr;

    for (pPrev = NULL, pCurr = pMqttSession->topicList.pHead; pCurr != NULL;
         pPrev = pCurr, pCurr = pCurr->pNext) {

        if (strcmp(pCurr->pTopicStr, pTopic->pTopicStr) == 0) {

            if (pPrev == NULL) {

                pMqttSession->topicList.pHead = pCurr->pNext;

            } else {

                pPrev->pNext = pCurr->pNext;

            }
            uPortFree(pCurr->pTopicStr);
            uPortFree(pCurr);
            break;
        }
    }
}

/**
 * Free all topic objects associated to given MQTT session
 */
static void freeAllMqttTopics(uWifiMqttSession_t *pMqttSession)
{
    uWifiMqttTopic_t *pTemp;
    uWifiMqttTopic_t *pNext;

    for (pTemp = pMqttSession->topicList.pHead; pTemp != NULL; pTemp = pNext) {

        pNext = pTemp->pNext;
        uPortFree(pTemp->pTopicStr);
        uPortFree(pTemp);
    }

    pMqttSession->topicList.pHead = NULL;
    pMqttSession->topicList.pTail = NULL;
}

static int32_t copyConnectionParams(char **ppMqttSessionParams,
                                    const char *pConnectionParams)
{
    size_t len;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (pConnectionParams != NULL) {

        len = strlen(pConnectionParams) + 1;

        if (ppMqttSessionParams != NULL) {

            *ppMqttSessionParams = (char *)pUPortMalloc(len);

            if (*ppMqttSessionParams != NULL) {

                memset(*ppMqttSessionParams, 0, len);
                strncpy(*ppMqttSessionParams, pConnectionParams, len);
                err = (int32_t)U_ERROR_COMMON_SUCCESS;

            } else {

                err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }
    }

    return err;
}

static uShortRangeSecTlsContext_t *getShoTlsContext(const uMqttClientContext_t *pContext)
{
    uShortRangeSecTlsContext_t *pMqttTlsContext = NULL;

    if (pContext != NULL) {
        if (pContext->pSecurityContext != NULL) {
            if (pContext->pSecurityContext->pNetworkSpecific != NULL) {
                pMqttTlsContext = (uShortRangeSecTlsContext_t *)pContext->pSecurityContext->pNetworkSpecific;
            }
        }
    }
    uPortLog("MQTT SHO TLS context %p\n", pMqttTlsContext);

    return pMqttTlsContext;
}

/**
 * Establish connection to a given broker. Report the disconnection
 * to the user via a callback in case of connection failure
 */
static int32_t establishMqttConnectionToBroker(const uMqttClientContext_t *pContext,
                                               uWifiMqttSession_t *pMqttSession,
                                               uWifiMqttTopic_t *pTopic,
                                               bool isPublish)
{
    char url[200];
    uAtClientHandle_t atHandle;
    int32_t err = (int32_t)U_ERROR_COMMON_SUCCESS;
    int32_t len;
    uShortRangeSecTlsContext_t *pMqttTlsContext;

    memset(url, 0, sizeof(url));
    // Add the port number unless it is already present
    char port[10] = {0};
    if (!strstr(pMqttSession->pBrokerNameStr, ":")) {
        snprintf(port, sizeof(port), ":%d", (int)pMqttSession->localPort);
    }

    if (isPublish) {
        len = snprintf(url, sizeof(url), "mqtt://%s%s/?pt=%s&retain=%d&qos=%d",
                       pMqttSession->pBrokerNameStr,
                       port,
                       pTopic->pTopicStr,
                       pTopic->retain,
                       pTopic->qos);

    } else {
        len = snprintf(url, sizeof(url), "mqtt://%s%s/?st=%s&qos=%d",
                       pMqttSession->pBrokerNameStr,
                       port,
                       pTopic->pTopicStr,
                       pTopic->qos);
    }

    if (pMqttSession->pClientIdStr) {

        if (len < (int32_t)sizeof(url)) {
            len += snprintf(&url[len], sizeof(url), "&client=%s", pMqttSession->pClientIdStr);
        } else {
            err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        }
    }

    if (pMqttSession->pUserNameStr) {

        if (len < (int32_t)sizeof(url)) {
            len += snprintf(&url[len], sizeof(url), "&user=%s", pMqttSession->pUserNameStr);
        } else {
            err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        }
    }

    if (pMqttSession->pPasswordStr) {

        if (len < (int32_t)sizeof(url)) {
            len += snprintf(&url[len], sizeof(url), "&passwd=%s", pMqttSession->pPasswordStr);
        } else {
            err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        }
    }

    // TBD: KeepAlive parameter in uMqttClientConnection_t is a bool and it needs to
    // be changed to uint16_t so that user try out different value acceptable by the broker.

    if (pMqttSession->keepAlive) {

        if (len < (int32_t)sizeof(url)) {
            len += snprintf(&url[len], sizeof(url), "&keepAlive=%d", 60);
        } else {
            err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        }
    }

    pMqttTlsContext = getShoTlsContext(pContext);

    if (pMqttTlsContext != NULL) {

        if (pMqttTlsContext->pRootCaCertificateName) {

            if (len < (int32_t)sizeof(url)) {
                len += snprintf(&url[len], sizeof(url), "&ca=%s", pMqttTlsContext->pRootCaCertificateName);
            } else {
                err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }

        if (pMqttTlsContext->pClientCertificateName) {

            if (len < (int32_t)sizeof(url)) {
                len += snprintf(&url[len], sizeof(url), "&cert=%s", pMqttTlsContext->pClientCertificateName);
            } else {
                err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }

        if (pMqttTlsContext->pClientPrivateKeyName) {

            if (len < (int32_t)sizeof(url)) {
                len += snprintf(&url[len], sizeof(url), "&privKey=%s", pMqttTlsContext->pClientPrivateKeyName);
            } else {
                err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }
    }

    if (len >= (int32_t)sizeof(url)) {
        err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
    }

    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
        atHandle = pMqttSession->atHandle;
        uPortLog("U_WIFI_MQTT: Sending AT+UDCP\n");
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDCP=");
        uAtClientWriteString(atHandle, (char *)&url[0], false);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UDCP:");
        pTopic->peerHandle = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        err = uAtClientUnlock(atHandle);

        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
            if (uPortSemaphoreTryTake(pMqttSession->semaphore, 5000) != (int32_t)U_ERROR_COMMON_SUCCESS) {
                err = (int32_t)U_ERROR_COMMON_TIMEOUT;
            }
        }
        // Report to user that we are disconnected
        if (err == (int32_t)U_ERROR_COMMON_TIMEOUT) {

            pMqttSession->isConnected = false;

            // Remove the topic from the mqtt session
            freeMqttTopic(pMqttSession, pTopic);

            if (pMqttSession->pDisconnectCb) {
                //lint -save -e785
                uCallbackEvent_t event = {
                    .pDataCb = NULL,
                    .pDisconnectCb = pMqttSession->pDisconnectCb,
                    .pCbParam = pMqttSession->pCbParam,
                    .pMqttSession = pMqttSession,
                    .disconnStatus = err
                };
                //lint -restore
                uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
            }
        }
    }

    uPortLog("U_WIFI_MQTT: MQTT connection err = %d\n", err);
    return err;
}

/**
 * Disconnect from a given broker.
 */
static int32_t disconnectMqttConnectionToBroker(uWifiMqttSession_t *pMqttSession)
{
    uAtClientHandle_t atHandle;
    uWifiMqttTopic_t *topic;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    atHandle = pMqttSession->atHandle;

    // Possible bug in u-connect S/W requires a
    // delay between EDM data write and disconnect
    uPortTaskBlock(1000);

    for (topic = pMqttSession->topicList.pHead; topic != NULL; topic = topic->pNext) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDCPC=");
        uAtClientWriteInt(atHandle, topic->peerHandle);
        uAtClientCommandStopReadResponse(atHandle);
        err = uAtClientUnlock(atHandle);
        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
            if (uPortSemaphoreTryTake(pMqttSession->semaphore, 5000) != (int32_t)U_ERROR_COMMON_SUCCESS) {
                err = (int32_t)U_ERROR_COMMON_TIMEOUT;
            }
        }
        uPortLog("U_WIFI_MQTT: MQTT disconnection err = %d\n", err);
    }
    return err;
}

/**
 *  Callback to handle both data available and disconnection events
 */
static void onCallbackEvent(void *pParam, size_t eventSize)
{
    uCallbackEvent_t *pCbEvent = (uCallbackEvent_t *)pParam;
    uWifiMqttSession_t *pMqttSession = (uWifiMqttSession_t *)pCbEvent->pMqttSession;
    int unreadMsgsCount = 0;
    (void) eventSize;

    if (pCbEvent->pDataCb) {
        U_PORT_MUTEX_LOCK(gMqttSessionMutex);

        if (pMqttSession != NULL) {
            unreadMsgsCount = pMqttSession->unreadMsgsCount;
        }
        U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);

        pCbEvent->pDataCb(unreadMsgsCount, pCbEvent->pCbParam);
    } else if (pCbEvent->pDisconnectCb) {

        pCbEvent->pDisconnectCb(pCbEvent->disconnStatus, pCbEvent->pCbParam);
    }
}

static void edmMqttDataCallback(int32_t edmHandle, int32_t edmChannel,
                                uShortRangePbufList_t *pBufList,
                                void *pCallbackParameter)
{
    uWifiMqttSession_t *pMqttSession = NULL;
    uWifiMqttTopic_t *pTopic;
    int32_t i;
    (void) edmHandle;
    (void)pCallbackParameter;

    U_PORT_MUTEX_LOCK(gMqttSessionMutex);

    for (i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        pMqttSession = &gMqttSessions[i];

        for (pTopic = pMqttSession->topicList.pHead; pTopic != NULL; pTopic = pTopic->pNext) {

            if ((pTopic->edmChannel == edmChannel) && (!pTopic->isTopicUnsubscribed)) {

                uPortLog("U_WIFI_MQTT: EDM data event for channel %d\n", edmChannel);
                if (uShortRangePktListAppend(&pMqttSession->rxPkt,
                                             pBufList) == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    pMqttSession->unreadMsgsCount = pMqttSession->rxPkt.pktCount;
                    // Schedule user data pDataCb
                    if (pMqttSession->pDataCb) {
                        //lint -save -e785
                        uCallbackEvent_t event = {
                            .pDataCb = pMqttSession->pDataCb,
                            .pDisconnectCb = NULL,
                            .pCbParam = pMqttSession->pCbParam,
                            .pMqttSession = pMqttSession
                        };
                        //lint -restore
                        uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
                    }
                } else {
                    uPortLog("U_WIFI_MQTT: Pkt insert failed\n");
                    uShortRangePbufListFree(pBufList);
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
}


static void edmIpConnectionCallback(int32_t edmHandle,
                                    int32_t edmChannel,
                                    uShortRangeConnectionEventType_t eventType,
                                    const uShortRangeConnectDataIp_t *pConnectData,
                                    void *pCallbackParameter)
{
    (void)pConnectData;
    (void)edmHandle;
    (void)pCallbackParameter;

    switch (eventType) {
        case U_SHORT_RANGE_EVENT_CONNECTED:
            uPortLog("U_WIFI_MQTT: EDM connect event for channel %d\n", edmChannel);
            gEdmChannel = edmChannel;
            break;
        case U_SHORT_RANGE_EVENT_DISCONNECTED:
            uPortLog("U_WIFI_MQTT: EDM disconnect event for channel %d\n", edmChannel);
            gEdmChannel = -1;
            break;
    }

}

static void atMqttConnectionCallback(uDeviceHandle_t devHandle,
                                     int32_t connHandle,
                                     uShortRangeConnectionEventType_t eventType,
                                     uShortRangeConnectDataIp_t *pConnectData,
                                     void *pCallbackParameter)
{
    uWifiMqttSession_t *pMqttSession;
    uWifiMqttTopic_t *pTopic;
    int32_t i;
    bool topicFound = false;
    (void)devHandle;
    (void)pConnectData;
    (void)pCallbackParameter;

    U_PORT_MUTEX_LOCK(gMqttSessionMutex);

    for (i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        pMqttSession = &gMqttSessions[i];

        for (pTopic = pMqttSession->topicList.pHead; pTopic != NULL; pTopic = pTopic->pNext) {

            if (pTopic->peerHandle == connHandle) {

                switch (eventType) {
                    case U_SHORT_RANGE_EVENT_CONNECTED:
                        uPortLog("U_WIFI_MQTT: AT+UUDCPC connect event for connHandle %d\n", connHandle);
                        pTopic->edmChannel = gEdmChannel;
                        pTopic->peerHandle = connHandle;
                        topicFound = true;
                        break;
                    case U_SHORT_RANGE_EVENT_DISCONNECTED:
                        uPortLog("U_WIFI_MQTT: AT+UUDCPC disconnect event for connHandle %d\n", connHandle);
                        pTopic->peerHandle = -1;
                        pTopic->edmChannel = -1;
                        topicFound = true;
                        pMqttSession->isConnected = false;
                        // Report to user that we are disconnected
                        if (pMqttSession->pDisconnectCb) {
                            //lint -save -e785
                            uCallbackEvent_t event = {
                                .pDataCb = NULL,
                                .pDisconnectCb = pMqttSession->pDisconnectCb,
                                .pCbParam = pMqttSession->pCbParam,
                                .pMqttSession = pMqttSession,
                                .disconnStatus = (int32_t)U_ERROR_COMMON_SUCCESS
                            };
                            //lint -restore
                            uPortEventQueueSend(gCallbackQueue, &event, sizeof(event));
                        }
                        break;
                }
            }

            if (topicFound) {
                break;
            }
        }

        if (topicFound) {
            break;
        }
    }

    U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
    uPortSemaphoreGive(pMqttSession->semaphore);
}

static int32_t getInstance(uDeviceHandle_t devHandle, uShortRangePrivateInstance_t **ppInstance)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (ppInstance != NULL) {
        *ppInstance = pUShortRangePrivateGetInstance(devHandle);
        if (*ppInstance != NULL) {
            if ((*ppInstance)->mode == U_SHORT_RANGE_MODE_EDM) {
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        } else {
            uPortLog("U_WIFI_MQTT: sho instance failed err = %d\n", err);
        }
    }
    return err;
}

static int32_t getMqttInstance(const uMqttClientContext_t *pContext,
                               uShortRangePrivateInstance_t **ppInstance,
                               uWifiMqttSession_t **ppMqttSession)
{
    int32_t err;

    err = getInstance(pContext->devHandle, ppInstance);

    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {

        err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

        if (ppMqttSession != NULL) {

            err = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
            *ppMqttSession = (uWifiMqttSession_t *)pContext->pPriv;
            if (*ppMqttSession != NULL) {
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return err;
}

static void freeMqttSession(uWifiMqttSession_t *pMqttSession)
{
    if (pMqttSession != NULL) {

        if (pMqttSession->pClientIdStr) {
            uPortFree(pMqttSession->pClientIdStr);
        }
        if (pMqttSession->pPasswordStr) {
            uPortFree(pMqttSession->pPasswordStr);
        }

        if (pMqttSession->pUserNameStr) {
            uPortFree(pMqttSession->pUserNameStr);
        }

        if (pMqttSession->pBrokerNameStr) {
            uPortFree(pMqttSession->pBrokerNameStr);
        }
        if (pMqttSession->semaphore) {
            uPortSemaphoreDelete(pMqttSession->semaphore);

        }
        if (pMqttSession->topicList.pHead) {
            freeAllMqttTopics(pMqttSession);
        }

        memset(pMqttSession, 0, sizeof(uWifiMqttSession_t));
        pMqttSession->sessionHandle = -1;
    }
}

static int32_t initMqttSessions(void)
{
    int32_t err;

    err = uPortMutexCreate(&gMqttSessionMutex);

    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
        for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {
            freeMqttSession(&gMqttSessions[i]);
        }
    }
    uPortLog("U_WIFI_MQTT: init MQTT session err = %d\n", err);

    return err;
}

static void freeMqtt(uMqttClientContext_t *pContext)
{
    int32_t count = 0;
    uShortRangePrivateInstance_t *pInstance;

    for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {
        if (gMqttSessions[i].sessionHandle == -1) {
            count++;
        }
    }

    if (count == U_WIFI_MQTT_MAX_NUM_CONNECTIONS) {
        uPortMutexDelete(gMqttSessionMutex);
        gMqttSessionMutex = NULL;
        if (getInstance(pContext->devHandle, &pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            uShortRangeSetMqttConnectionStatusCallback(pContext->devHandle, NULL, NULL);

            uShortRangeEdmStreamMqttEventCallbackSet(pInstance->streamHandle, NULL, NULL);

            uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                     U_SHORT_RANGE_CONNECTION_TYPE_MQTT,
                                                     NULL,
                                                     NULL);
            uPortEventQueueClose(gCallbackQueue);
            gCallbackQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;

        }
    }
}

static uWifiMqttSession_t *allocateMqttSession(void)
{
    uWifiMqttSession_t *pMqttSession = NULL;

    for (int32_t i = 0; i < U_WIFI_MQTT_MAX_NUM_CONNECTIONS; i++) {

        if (gMqttSessions[i].sessionHandle == -1) {

            pMqttSession = &gMqttSessions[i];
            pMqttSession->sessionHandle = i;
            break;

        }
    }

    return pMqttSession;
}

/**
 * Allocate MQTT session based on the given connection params
 */
static int32_t configureMqttSessionConnection(uWifiMqttSession_t *pMqttSession,
                                              const uMqttClientConnection_t *pConnection)
{
    int32_t err = (int32_t)U_ERROR_COMMON_NO_MEMORY;

    if ((pMqttSession != NULL) && (pConnection != NULL)) {
        err = copyConnectionParams(&pMqttSession->pBrokerNameStr,
                                   pConnection->pBrokerNameStr);

        if (err == 0) {
            if (pConnection->pClientIdStr) {
                err = copyConnectionParams(&pMqttSession->pClientIdStr,
                                           pConnection->pClientIdStr);
            }
        }
        if (err == 0) {
            if (pConnection->pUserNameStr) {
                err = copyConnectionParams(&pMqttSession->pUserNameStr,
                                           pConnection->pUserNameStr);
            }
        }
        if (err == 0) {
            if (pConnection->pPasswordStr) {
                err = copyConnectionParams(&pMqttSession->pPasswordStr,
                                           pConnection->pPasswordStr);
            }
        }
        if (err == 0) {
            pMqttSession->localPort = pConnection->localPort;
            pMqttSession->keepAlive = pConnection->keepAlive;
            err = uPortSemaphoreCreate(&(pMqttSession->semaphore), 0, 1);
        }
        memset((void *)(&pMqttSession->rxPkt), 0, sizeof(uShortRangePktList_t));
    }

    if (err != 0) {
        if (err == (int32_t)U_ERROR_COMMON_NO_MEMORY) {
            uPortLog("U_WIFI_MQTT: %s Out of memory\n", __func__);
        }
        if (pMqttSession) {
            freeMqttSession(pMqttSession);
        }
    }

    return err;
}

int32_t uWifiMqttInit(uDeviceHandle_t devHandle, void **ppMqttSession)
{
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        if (gMqttSessionMutex == NULL) {

            if (initMqttSessions() == (int32_t)U_ERROR_COMMON_SUCCESS) {

                if (getInstance(devHandle, &pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    if (pInstance->devHandle == NULL) {
                        pInstance->devHandle = devHandle;
                    }
                    err = uShortRangeSetMqttConnectionStatusCallback(devHandle,
                                                                     atMqttConnectionCallback,
                                                                     pInstance);
                    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {

                        err = uShortRangeEdmStreamMqttEventCallbackSet(pInstance->streamHandle,
                                                                       edmIpConnectionCallback,
                                                                       pInstance);
                        if (err != (int32_t)U_ERROR_COMMON_SUCCESS) {
                            uPortLog("U_WIFI_MQTT: EDM IP event cb register failed err = %d\n", err);
                        }

                    } else {

                        uPortLog("U_WIFI_MQTT: MQTT conn status cb register failed err = %d\n", err);
                    }

                    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {

                        err = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                                       U_SHORT_RANGE_CONNECTION_TYPE_MQTT,
                                                                       edmMqttDataCallback,
                                                                       pInstance);
                        if (err != (int32_t)U_ERROR_COMMON_SUCCESS) {
                            uPortLog("U_WIFI_MQTT: EDM stream event cb register failed err = %d\n", err);
                        }
                    }
                }
            }
        } else {
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
        }

        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {

            *ppMqttSession = (void *)allocateMqttSession();

            if (*ppMqttSession == NULL) {

                err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }
        uShortRangeUnlock();
    } else {
        uPortLog("U_WIFI_MQTT: sho lock failed err = %d\n", err);
    }
    return err;
}

int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                         const uMqttClientConnection_t *pConnection)
{
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uWifiMqttSession_t *pMqttSession = (uWifiMqttSession_t *)pContext->pPriv;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        if (getInstance(pContext->devHandle, &pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            err = configureMqttSessionConnection(pMqttSession, pConnection);

            if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {

                if (pConnection->localPort == -1) {
                    if (pContext->pSecurityContext != NULL) {
                        pMqttSession->localPort = U_MQTT_BROKER_PORT_SECURE;
                    } else {
                        pMqttSession->localPort = U_MQTT_BROKER_PORT_UNSECURE;
                    }
                }

                pMqttSession->atHandle = pInstance->atHandle;
                pMqttSession->isConnected = true;
            }
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    } else {
        uPortLog("U_WIFI_MQTT: sho lock failed err = %d\n", err);
    }
    return err;
}

int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        err = getMqttInstance(pContext, &pInstance, &pMqttSession);
        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            pMqttSession->pDataCb = pCallback;
            pMqttSession->pCbParam = pCallbackParam;

            if (gCallbackQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                gCallbackQueue = uPortEventQueueOpen(onCallbackEvent,
                                                     "uWifiMqttCallbackQueue",
                                                     sizeof(uCallbackEvent_t),
                                                     U_WIFI_MQTT_DATA_EVENT_STACK_SIZE,
                                                     U_WIFI_MQTT_DATA_EVENT_PRIORITY,
                                                     2 * U_WIFI_MQTT_MAX_NUM_CONNECTIONS);
            }
            err = (gCallbackQueue >= 0) ? (int32_t)U_ERROR_COMMON_SUCCESS : (int32_t)
                  U_ERROR_COMMON_NOT_INITIALISED;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }
    return err;
}

int32_t uWifiMqttSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        err = getMqttInstance(pContext, &pInstance, &pMqttSession);
        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            pMqttSession->pDisconnectCb = pCallback;
            pMqttSession->pCbParam = pCallbackParam;

            if (gCallbackQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                gCallbackQueue = uPortEventQueueOpen(onCallbackEvent,
                                                     "uWifiMqttCallbackQueue",
                                                     sizeof(uCallbackEvent_t),
                                                     U_WIFI_MQTT_DATA_EVENT_STACK_SIZE,
                                                     U_WIFI_MQTT_DATA_EVENT_PRIORITY,
                                                     2 * U_WIFI_MQTT_MAX_NUM_CONNECTIONS);
            }
            err = (gCallbackQueue >= 0) ? (int32_t)U_ERROR_COMMON_SUCCESS : (int32_t)
                  U_ERROR_COMMON_NOT_INITIALISED;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }
    return err;

}

int32_t uWifiMqttPublish(const uMqttClientContext_t *pContext,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uMqttQos_t qos,
                         bool retain)
{
    uWifiMqttSession_t *pMqttSession;
    uWifiMqttTopic_t *pTopic;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            // Check if we have this pTopic already mapped to this session
            pTopic = findTopic(pMqttSession, pTopicNameStr, true);

            if (pTopic == NULL) {

                // Create a new pTopic and insert it to this session
                pTopic = pAllocateMqttTopic(pMqttSession, true);

                if (pTopic != NULL) {

                    pTopic->retain = retain;
                    pTopic->qos = qos;

                    err = copyConnectionParams(&pTopic->pTopicStr,
                                               pTopicNameStr);

                    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                        err = establishMqttConnectionToBroker(pContext, pMqttSession, pTopic, true);
                    }

                }

            } else {

                err = (int32_t)U_ERROR_COMMON_SUCCESS;

            }

            if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                //lint -esym(613, pTopic) Suppress possible use of NULL pointer in future
                err = uShortRangeEdmStreamWrite(pInstance->streamHandle,
                                                pTopic->edmChannel,
                                                pMessage,
                                                messageSizeBytes,
                                                U_WIFI_MQTT_WRITE_TIMEOUT_MS);
                uPortLog("EDM write for channel %d message bytes %d written bytes %d\n", pTopic->edmChannel,
                         messageSizeBytes,
                         err);
            }
            if (err == messageSizeBytes) {
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }

    return err;
}

int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                           const char *pTopicFilterStr,
                           uMqttQos_t maxQos)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    uWifiMqttTopic_t *pTopic;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            // Check if we have this pTopic already mapped to this session
            pTopic = findTopic(pMqttSession, pTopicFilterStr, false);

            if (pTopic == NULL) {

                pTopic = pAllocateMqttTopic(pMqttSession, false);

                if (pTopic != NULL) {

                    pTopic->qos = maxQos;

                    err = copyConnectionParams(&pTopic->pTopicStr,
                                               pTopicFilterStr);

                    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                        err = establishMqttConnectionToBroker(pContext, pMqttSession, pTopic, false);
                    }

                    if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                        err = (int32_t)pTopic->qos;
                    }
                } else {

                    err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
                }

            } else {
                //lint -esym(613, pTopic) Suppress possible use of NULL pointer in future
                pTopic->isTopicUnsubscribed = false;
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }

    return err;
}

int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr)
{
    uWifiMqttSession_t *pMqttSession;
    uWifiMqttTopic_t *pTopic;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;


    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            // Fetch the pTopic object that contains this pTopic string
            pTopic = findTopic(pMqttSession, pTopicFilterStr, false);

            if (pTopic != NULL) {
                // By unsubscribing, we avoid buffering the data
                pTopic->isTopicUnsubscribed = true;
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            } else {
                uPortLog("U_WIFI_MQTT: Topic not found in session %p\n", pMqttSession);
            }

            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return err;
}

int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    bool isMqttConnected;

    isMqttConnected = uWifiMqttIsConnected(pContext);


    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            if (isMqttConnected) {
                // initiate disconnection if we are connected
                err = disconnectMqttConnectionToBroker(pMqttSession);
            } else {
                err = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }
    return err;
}

void uWifiMqttClose(uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    bool isMqttConnected;

    isMqttConnected = uWifiMqttIsConnected(pContext);


    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            if (isMqttConnected) {
                disconnectMqttConnectionToBroker(pMqttSession);
                pContext->pPriv = NULL;
            }
            // Release the memory for all the topics associated to this session
            // as well as the session itself.
            freeMqttSession(pMqttSession);
            // Deregister the EDM, MQTT, AT callbacks
            freeMqtt(pContext);
        }
        uShortRangeUnlock();
    }
}

int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t unReadMsgsCount = 0;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);
            unReadMsgsCount = pMqttSession->unreadMsgsCount;
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return unReadMsgsCount;
}

int32_t uWifiMqttMessageRead(const uMqttClientContext_t *pContext,
                             char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage,
                             size_t *pMessageSizeBytes,
                             uMqttQos_t *pQos)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t edmChannel;
    char *pFoundTopicStr;
    size_t foundTopicLen;
    (void) pQos;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            if ((pTopicNameStr != NULL) &&
                (pMessage != NULL) &&
                (pMessageSizeBytes != NULL)) {

                memset(pMessage, 0, *pMessageSizeBytes);
                memset(pTopicNameStr, 0, topicNameSizeBytes);
                err = uShortRangePktListConsumePacket(&pMqttSession->rxPkt, pMessage, pMessageSizeBytes,
                                                      &edmChannel);

                pMqttSession->unreadMsgsCount = pMqttSession->rxPkt.pktCount;
                if (err == 0) {
                    err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
                    pFoundTopicStr = getTopicStrForEdmChannel(pMqttSession, edmChannel);

                    if (pFoundTopicStr != NULL) {
                        foundTopicLen = strlen(pFoundTopicStr);
                        if ((foundTopicLen + 1) <= topicNameSizeBytes) {
                            strncpy(pTopicNameStr, pFoundTopicStr, foundTopicLen);
                            err = (int32_t)U_ERROR_COMMON_SUCCESS;
                        }
                    }

                }
                if (err != 0) {
                    // clear the partial message that was copied
                    memset(pMessage, 0, *pMessageSizeBytes);
                }
            }
            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return err;
}

bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext)
{
    uWifiMqttSession_t *pMqttSession;
    uShortRangePrivateInstance_t *pInstance;
    bool isConnected = false;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {

        // Check WiFi SHO handle and MQTT session exists
        if (getMqttInstance(pContext, &pInstance, &pMqttSession) == (int32_t)U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMqttSessionMutex);

            isConnected = pMqttSession->isConnected;

            U_PORT_MUTEX_UNLOCK(gMqttSessionMutex);
        }
        uShortRangeUnlock();
    }

    return isConnected;
}

//End of file
