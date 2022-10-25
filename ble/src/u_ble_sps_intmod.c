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
 * @brief Implementation of the data API for ble.
 */

#ifdef U_CFG_BLE_MODULE_INTERNAL

//lint -e740

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"
#include "u_cfg_os_platform_specific.h"
#include "u_ringbuffer.h"

#include "u_ble_sps.h"
#include "u_ble_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_BLE_PDU_HEADER_SIZE 3

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
/** SPS state
 *
 * The SPS link is not considered connected until proper characteristics
 * subscriptions are setup in the case we are client, or client characteristics
 * configuration is properly written in the case we are server.
 * */
typedef enum {
    SPS_STATE_DISCONNECTED = 0,
    SPS_STATE_CONNECTED
} spsState_t;

/** SPS events
 *
 * Events generated when client connects to remote server
 * and when data arrives (both client and server)
 * */
typedef enum {
    EVENT_GAP_CONNECTED,
    EVENT_SPS_SERVICE_DISCOVERED,
    EVENT_SPS_FIFO_CHAR_DISCOVERED,
    EVENT_SPS_CREDIT_CHAR_DISCOVERED,
    EVENT_SPS_CCCS_DISCOVERED,
    EVENT_SPS_MTU_EXCHANGED,
    EVENT_SPS_CREDITS_SUBSCRIBED,
    EVENT_SPS_FIFO_SUBSCRIBED,
    EVENT_SPS_CONNECTING_FAILED,
    EVENT_SPS_RX_DATA_AVAILABLE
} spsEventType_t;

/** SPS Role
 * */
typedef enum {
    SPS_SERVER,
    SPS_CLIENT
} spsRole_t;

/** SPS Connection information
 * */
typedef struct {
    int32_t      gapConnHandle;
    char         remoteAddr[14]; // 12 (mac) + 1 (p/r) + 1 ("\0")
    union {
        struct {
            uBleSpsHandles_t       attHandle;
            uPortGattSubscribeParams_t creditSubscripe;
            uPortGattSubscribeParams_t fifoSubscripe;
        } client;
        struct {
            uint16_t fifoClientConf;
            uint16_t creditsClientConf;
        } server;
    };
    uint8_t                rxCreditsOnRemote;
    uint8_t                txCredits;
    spsState_t             spsState;
    uint16_t               mtu;
    uPortSemaphoreHandle_t txCreditsSemaphore;
    char                   rxData[U_BLE_SPS_BUFFER_SIZE];
    uRingBuffer_t          rxRingBuffer;
    uint32_t               dataSendTimeoutMs;
    spsRole_t              localSpsRole;
    bool                   flowCtrlEnabled;
} spsConnection_t;

/** SPS Client event
 * */
typedef struct {
    spsEventType_t type;
    int32_t        spsConnHandle;
} spsEvent_t;

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */
static int32_t findSpsConnHandle(int32_t gapConnHandle);
static spsConnection_t *pGetSpsConn(int32_t spsConnHandle);
static int32_t findFreeSpsConnHandle(void);
static void freeSpsConnection(int32_t spsConnHandle);
static bool validSpsConnHandle(int32_t spsConnHandle);
static spsConnection_t *initSpsConnection(int32_t spsConnHandle, int32_t gapConnHandle,
                                          spsRole_t localSpsRole);
static void addLocalTxCredits(int32_t spsConnHandle, uint8_t credits);
static void addReceivedDataToBuffer(int32_t spsConnHandle, const void *pData, uint16_t length);
static bool sendDataToRemoteFifo(const spsConnection_t *pSpsConn, const char *pData,
                                 uint16_t bytesToSendNow);
static void updateRxCreditsOnRemote(spsConnection_t *pSpsConn);
static void gapConnectionEvent(int32_t gapConnHandle, uPortGattGapConnStatus_t status,
                               void *pParameter);

/**  SPS Client specific functions */
static uPortGattIter_t onCreditsNotified(int32_t gapConnHandle,
                                         struct uPortGattSubscribeParams_s *pParams,
                                         const void *pData, uint16_t length);
static uPortGattIter_t onFifoNotified(int32_t gapConnHandle,
                                      struct uPortGattSubscribeParams_s *pParams,
                                      const void *pData, uint16_t length);
static void onFifoSubscribed(int32_t gapConnHandle, uint8_t err);
static void onCreditsSubscribed(int32_t gapConnHandle, uint8_t err);
static void startCreditSubscription(spsConnection_t *pSpsConn);
static void startFifoSubscription(spsConnection_t *pSpsConn);
static void mtuXchangeResp(int32_t gapConnHandle, uint8_t err);
static uPortGattIter_t onCccDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                      uint16_t attrHandle);
static uPortGattIter_t onCreditCharDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                             uint16_t attrHandle, uint16_t valueHandle,
                                             uint8_t properties);
static uPortGattIter_t onFifoCharDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                           uint16_t attrHandle, uint16_t valueHandle,
                                           uint8_t properties);
static uPortGattIter_t onSpsServiceDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                             uint16_t attrHandle, uint16_t endHandle);
static void onBleSpsEvent(void *pParam, size_t eventSize);

/** SPS Server specific functions */
static bool write16BitValue(const void *buf, uint16_t len, uint16_t offset, uint16_t *pVal);
static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                     uint16_t offset, uint8_t flags);
static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags);

/** Other server and client common functions */
static int32_t hexToInt(const char *pIn, uint8_t *pOut);
static int32_t addrStringToArray(const char *pAddrIn, uint8_t *pAddrOut,
                                 uPortBtLeAddressType_t *pType);

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
static uPortMutexHandle_t gBleSpsMutex = NULL;
static int32_t gSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
static uBleSpsConnectionStatusCallback_t gpSpsConnStatusCallback;
static void *gpSpsConnStatusCallbackParam;
static uBleSpsAvailableCallback_t gpSpsDataAvailableCallback;
static void *gpSpsDataAvailableCallbackParam;
static spsConnection_t *gpSpsConnections[U_BLE_SPS_MAX_CONNECTIONS];
static uBleSpsHandles_t gNextConnServerHandles;
static bool gFlowCtrlOnNext = true;

static uPortGattUuid128_t gSpsCreditsCharUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x04, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24}
};

static uPortGattUuid128_t gSpsFifoCharUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x03, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24},
};

static uPortGattUuid128_t gSpsServiceUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x01, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24},
};

static const uPortGattCharDescriptor_t gSpsFifoClientConf = {
    .descriptorType = U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
    .att = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesFifoCcc,
        .read = NULL,
    },
    .pNextDescriptor = NULL,
};

static const uPortGattCharDescriptor_t gSpsCreditsClientConf = {
    .descriptorType = U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
    .att = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesCreditCcc,
        .read = NULL,
    },
    .pNextDescriptor = NULL,
};

static const uPortGattCharacteristic_t gSpsCreditsChar = {
    .pUuid = (uPortGattUuid_t *) &gSpsCreditsCharUuid,
    .properties = U_PORT_GATT_CHRC_NOTIFY | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP,
    .valueAtt = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesCreditChar,
        .read = NULL,
    },
    .pFirstDescriptor = &gSpsCreditsClientConf,
    .pNextChar = NULL,
};

static const uPortGattCharacteristic_t gSpsFifoChar = {
    .pUuid = (uPortGattUuid_t *) &gSpsFifoCharUuid,
    .properties = U_PORT_GATT_CHRC_NOTIFY | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP,
    .valueAtt = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesFifoChar,
        .read = NULL,
    },
    .pFirstDescriptor = &gSpsFifoClientConf,
    .pNextChar = &gSpsCreditsChar,
};

static const uBleSpsConnParams_t gConnParamsDefault = {
    U_BLE_SPS_CONN_PARAM_SCAN_INT_DEFAULT,
    U_BLE_SPS_CONN_PARAM_SCAN_WIN_DEFAULT,
    U_BLE_SPS_CONN_PARAM_TMO_DEFAULT,
    U_BLE_SPS_CONN_PARAM_CONN_INT_MIN_DEFAULT,
    U_BLE_SPS_CONN_PARAM_CONN_INT_MAX_DEFAULT,
    U_BLE_SPS_CONN_PARAM_CONN_LATENCY_DEFAULT,
    U_BLE_SPS_CONN_PARAM_LINK_LOSS_TMO_DEFAULT
};

/* ----------------------------------------------------------------
 * EXPORTED VARIABLES
 * -------------------------------------------------------------- */
uPortGattService_t const gSpsService = {
    .pUuid = (uPortGattUuid_t *) &gSpsServiceUuid,
    .pFirstChar = &gSpsFifoChar,
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static int32_t findSpsConnHandle(int32_t gapConnHandle)
{
    int32_t i;
    int32_t spsConnHandle = U_BLE_SPS_INVALID_HANDLE;

    for (i = 0; i < U_BLE_SPS_MAX_CONNECTIONS; i++) {
        if ((gpSpsConnections[i] != NULL) &&
            (gpSpsConnections[i]->gapConnHandle) == gapConnHandle) {
            spsConnHandle = i;
        }
    }

    return spsConnHandle;
}

static spsConnection_t *pGetSpsConn(int32_t spsConnHandle)
{
    spsConnection_t *pSpsConn = NULL;

    if ((spsConnHandle >= 0) && (spsConnHandle < U_BLE_SPS_MAX_CONNECTIONS)) {
        pSpsConn = gpSpsConnections[spsConnHandle];
    }

    return pSpsConn;
}

static int32_t findFreeSpsConnHandle(void)
{
    int32_t i;
    int32_t spsConnHandle = U_BLE_SPS_INVALID_HANDLE;

    for (i = 0; i < U_BLE_SPS_MAX_CONNECTIONS; i++) {
        if (gpSpsConnections[i] == NULL) {
            spsConnHandle = i;
            break;
        }
    }
    return spsConnHandle;
}

static void freeSpsConnection(int32_t spsConnHandle)
{
    if (validSpsConnHandle(spsConnHandle)) {
        spsConnection_t *pSpsConn = gpSpsConnections[spsConnHandle];
        uRingBufferDelete(&pSpsConn->rxRingBuffer);
        uPortSemaphoreDelete(pSpsConn->txCreditsSemaphore);
        uPortFree(pSpsConn);
        gpSpsConnections[spsConnHandle] = NULL;
    }
}

static bool validSpsConnHandle(int32_t spsConnHandle)
{
    if ((spsConnHandle >= 0) &&
        (spsConnHandle < U_BLE_SPS_MAX_CONNECTIONS) &&
        (gpSpsConnections[spsConnHandle] != NULL)) {
        return true;
    }
    return false;
}

static spsConnection_t *initSpsConnection(int32_t spsConnHandle, int32_t gapConnHandle,
                                          spsRole_t localSpsRole)
{
    if ((spsConnHandle < 0) || (spsConnHandle >= U_BLE_SPS_MAX_CONNECTIONS)) {
        return NULL;
    }

    if (gpSpsConnections[spsConnHandle] == NULL) {
        gpSpsConnections[spsConnHandle] = (spsConnection_t *)pUPortMalloc(sizeof(spsConnection_t));
    }

    if (gpSpsConnections[spsConnHandle] != NULL) {
        spsConnection_t *pSpsConn = gpSpsConnections[spsConnHandle];
        pSpsConn->gapConnHandle = gapConnHandle;
        pSpsConn->rxCreditsOnRemote = 0;
        pSpsConn->txCredits = 0;
        pSpsConn->mtu = 23;
        pSpsConn->client.attHandle.service = 0;
        pSpsConn->client.attHandle.fifoValue = 0;
        pSpsConn->client.attHandle.fifoCcc = 0;
        pSpsConn->client.attHandle.creditsValue = 0;
        pSpsConn->client.attHandle.creditsCcc = 0;
        pSpsConn->server.fifoClientConf = 0;
        pSpsConn->server.creditsClientConf = 0;
        pSpsConn->spsState = SPS_STATE_DISCONNECTED;
        uPortSemaphoreCreate(&(pSpsConn->txCreditsSemaphore), 0, 1);
        uRingBufferCreate(&pSpsConn->rxRingBuffer, pSpsConn->rxData, sizeof(pSpsConn->rxData));
        uRingBufferReset(&pSpsConn->rxRingBuffer);
        pSpsConn->dataSendTimeoutMs = U_BLE_SPS_DEFAULT_SEND_TIMEOUT_MS;
        pSpsConn->localSpsRole = localSpsRole;
        pSpsConn->flowCtrlEnabled = true;
    }

    return gpSpsConnections[spsConnHandle];
}

static void addLocalTxCredits(int32_t spsConnHandle, uint8_t credits)
{
    spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);

    if (credits != 0xff) {
        pSpsConn->txCredits += credits;
        if (pSpsConn->txCredits > 0) {
            uPortLog("U_BLE_SPS: TX credits = %d\n", pSpsConn->txCredits);
            // We have received more credits, dataSend function might
            // be waiting for the semaphore indicating the we now have TX credits
            uPortSemaphoreGive(pSpsConn->txCreditsSemaphore);
        }
        if ((pSpsConn->spsState == SPS_STATE_DISCONNECTED) && pSpsConn->flowCtrlEnabled) {
            pSpsConn->spsState = SPS_STATE_CONNECTED;
            uPortLog("U_BLE_SPS: Connected as SPS server. Handle %d, remote addr: %s\n",
                     spsConnHandle, pSpsConn->remoteAddr);
            updateRxCreditsOnRemote(pSpsConn);
            if (gpSpsConnStatusCallback != NULL) {
                gpSpsConnStatusCallback(spsConnHandle,
                                        pSpsConn->remoteAddr,
                                        (int32_t)U_BLE_SPS_CONNECTED,
                                        spsConnHandle,
                                        pSpsConn->mtu,
                                        gpSpsConnStatusCallbackParam);
            }
        }
    }
}

static void addReceivedDataToBuffer(int32_t spsConnHandle, const void *pData, uint16_t length)
{
    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        bool bufferWasEmpty = (uRingBufferDataSize(&(pSpsConn->rxRingBuffer)) == 0);

        if (pSpsConn->rxCreditsOnRemote > 0) {
            // Keep track of how many credits the remote has left
            pSpsConn->rxCreditsOnRemote--;
        } else {
            if (pSpsConn->flowCtrlEnabled) {
                uPortLog("U_BLE_SPS: Remote sent %d bytes without credits!\n", length);
            }
        }

        if (uRingBufferAdd(&(pSpsConn->rxRingBuffer), (const char *)pData, length)) {
            if (bufferWasEmpty) {
                spsEvent_t event;
                event.type = EVENT_SPS_RX_DATA_AVAILABLE;
                event.spsConnHandle = spsConnHandle;
                uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
            }
        } else {
            // This should not happen if credits are sent and regarded properly
            uPortLog("U_BLE_SPS: Received data could not be stored, dropping data!\n");
        }
    }
}

static bool sendDataToRemoteFifo(const spsConnection_t *pSpsConn, const char *pData,
                                 uint16_t bytesToSendNow)
{
    bool success;

    if ((pSpsConn->localSpsRole == SPS_SERVER) && (pSpsConn->server.fifoClientConf & 1)) {
        success = (uPortGattNotify(pSpsConn->gapConnHandle,
                                   &gSpsFifoChar, pData, bytesToSendNow) == (int32_t)U_ERROR_COMMON_SUCCESS);
    } else {
        success = (uPortGattWriteAttribute(pSpsConn->gapConnHandle, pSpsConn->client.attHandle.fifoValue,
                                           pData, bytesToSendNow) == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    return success;
}

static void updateRxCreditsOnRemote(spsConnection_t *pSpsConn)
{
    size_t avaibleBufferSize = uRingBufferAvailableSize(&(pSpsConn->rxRingBuffer));
    uint8_t availableRxCredits = 0;
    size_t maxPacketDataSize = pSpsConn->mtu - U_BLE_PDU_HEADER_SIZE;
    int16_t rxCreditsWeCanSend;

    // First we calculate how many full size packets would fit into the current buffer
    while ((avaibleBufferSize > maxPacketDataSize) && (availableRxCredits < 255)) {
        avaibleBufferSize -= maxPacketDataSize;
        availableRxCredits++;
    }

    // We maybe give the remote permission to send some more packets, but never more than
    // that the total space occupied by the packets could overflow the current free
    // buffer space i.e. availableRxCredits = rxCreditsWeCanSend + rxCreditsOnRemote
    rxCreditsWeCanSend = (int16_t)availableRxCredits - (int16_t)(pSpsConn->rxCreditsOnRemote);
    // Only send new credits when we at least can double the amount available on the remote,
    // to minimize credits traffic, i.e. when we can send more credits than exists on remote
    if ((rxCreditsWeCanSend > (int16_t)(pSpsConn->rxCreditsOnRemote)) &&
        (rxCreditsWeCanSend > 0)) {
        bool success = false;

        if (pSpsConn->localSpsRole == SPS_SERVER) {
            if (pSpsConn->server.creditsClientConf & 1) {
                success = (uPortGattNotify(pSpsConn->gapConnHandle,
                                           &gSpsCreditsChar, &rxCreditsWeCanSend, 1) == 0);
            } else {
                // Credit Characteristics Client Configuration notification
                // bit is not set
                success = false;
            }
        } else { // SPS_CLIENT
            success = (uPortGattWriteAttribute(pSpsConn->gapConnHandle,
                                               pSpsConn->client.attHandle.creditsValue, &rxCreditsWeCanSend, 1) == 0);
        }

        if (success) {
            uPortLog("U_BLE_SPS: Sent %d credits\n", rxCreditsWeCanSend);
            pSpsConn->rxCreditsOnRemote += (uint8_t)rxCreditsWeCanSend;
        }
    }
}

//lint -esym(818, pParameter)
static void gapConnectionEvent(int32_t gapConnHandle, uPortGattGapConnStatus_t status,
                               void *pParameter)
{
    int32_t spsConnHandle;
    (void)pParameter;

    U_PORT_MUTEX_LOCK(gBleSpsMutex);

    spsConnHandle = findSpsConnHandle(gapConnHandle);

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        switch (status) {
            case U_PORT_GATT_GAP_CONNECTED: {
                // If we get a GAP connected event and there already is a handle
                // it means we initiated the connection and are SPS client
                // In this case the SPS connection is alread initiated.
                spsEvent_t event;
                event.type = EVENT_GAP_CONNECTED;
                event.spsConnHandle = spsConnHandle;
                uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
                uPortLog("U_BLE_SPS: Connecting SPS, conn handle: %d\n", spsConnHandle);
                break;
            }

            case U_PORT_GATT_GAP_DISCONNECTED:
                if (pSpsConn->spsState != SPS_STATE_DISCONNECTED) {
                    uPortLog("U_BLE_SPS: Disconnected SPS, conn handle: %d\n", spsConnHandle);
                    pSpsConn->spsState = SPS_STATE_DISCONNECTED;
                } else {
                    uPortLog("U_BLE_SPS: SPS connection failed!\n");
                    // If a connection attempt failed we have not yet
                    // communicated the connection handle to the upper layer
                    // We still report the failed connection attempt but with
                    // invalid connection handle, but first we need to free
                    // the allocated slot
                    freeSpsConnection(spsConnHandle);
                    spsConnHandle = U_BLE_SPS_INVALID_HANDLE;
                }
                if (gpSpsConnStatusCallback != NULL) {
                    gpSpsConnStatusCallback(spsConnHandle,
                                            pSpsConn->remoteAddr,
                                            (int32_t)U_BLE_SPS_DISCONNECTED,
                                            spsConnHandle,
                                            pSpsConn->mtu,
                                            gpSpsConnStatusCallbackParam);
                }
                if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
                    freeSpsConnection(spsConnHandle);
                }
                break;
        }
    } else {
        switch (status) {
            case U_PORT_GATT_GAP_CONNECTED: {
                // If we get a GAP connected event and there is no handle present
                // it means the remoted side initiated the connection and we are
                // SPS server.  In this case we initiate the SPS connection here.
                spsConnHandle = findFreeSpsConnHandle();
                if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
                    uint8_t addr[6];
                    uPortBtLeAddressType_t addrType;
                    spsConnection_t *pSpsConn = initSpsConnection(spsConnHandle, gapConnHandle, SPS_SERVER);
                    uPortGattGetRemoteAddress(gapConnHandle, addr, &addrType);
                    addrArrayToString(addr, addrType, true, pSpsConn->remoteAddr);
                    uPortLog("U_BLE_SPS: Remote GAP connected, SPS conn handle: %d\n", spsConnHandle);
                } else {
                    uPortLog("U_BLE_SPS: We already have maximum nbr of allowed SPS connections!\n", spsConnHandle);
                    uPortGattDisconnectGap(gapConnHandle);
                }
                break;
            }

            case U_PORT_GATT_GAP_DISCONNECTED:
                break;
        }
    }

    U_PORT_MUTEX_UNLOCK(gBleSpsMutex);
}

//lint -esym(818, pParams)
static uPortGattIter_t onCreditsNotified(int32_t gapConnHandle,
                                         struct uPortGattSubscribeParams_s *pParams,
                                         const void *pData, uint16_t length)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    (void)pParams;
    (void)length;

    if ((spsConnHandle != U_BLE_SPS_INVALID_HANDLE) && pData && (length > 0)) {
        addLocalTxCredits(spsConnHandle, *(const uint8_t *)pData);
    }

    return U_PORT_GATT_ITER_CONTINUE; // Continue to subscribe to credits characteristic
}

//lint -esym(818, pParams)
static uPortGattIter_t onFifoNotified(int32_t gapConnHandle,
                                      struct uPortGattSubscribeParams_s *pParams,
                                      const void *pData, uint16_t length)
{
    (void)pParams;
    if (pData && (length > 0)) {
        addReceivedDataToBuffer(findSpsConnHandle(gapConnHandle), pData, length);
    }

    return U_PORT_GATT_ITER_CONTINUE; // Continue to subcribe to FIFO characteristic
}

static void onFifoSubscribed(int32_t gapConnHandle, uint8_t err)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsEvent_t event;

        event.spsConnHandle = spsConnHandle;
        if (err == 0) {
            event.type = EVENT_SPS_FIFO_SUBSCRIBED;
        } else {
            uPortLog("U_BLE_SPS: FIFO subscription failed!\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }
}

static void onCreditsSubscribed(int32_t gapConnHandle, uint8_t err)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsEvent_t event;

        event.spsConnHandle = spsConnHandle;
        if (err == 0) {
            event.type = EVENT_SPS_CREDITS_SUBSCRIBED;
        } else {
            uPortLog("U_BLE_SPS: Credits subscription failed!\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }
}

static void startCreditSubscription(spsConnection_t *pSpsConn)
{
    pSpsConn->client.creditSubscripe.notifyCb = onCreditsNotified;
    pSpsConn->client.creditSubscripe.cccWriteRespCb = onCreditsSubscribed;
    pSpsConn->client.creditSubscripe.valueHandle = pSpsConn->client.attHandle.creditsValue;
    pSpsConn->client.creditSubscripe.cccHandle = pSpsConn->client.attHandle.creditsCcc;
    pSpsConn->client.creditSubscripe.receiveNotifications = true;
    pSpsConn->client.creditSubscripe.receiveIndications = false;
    uPortGattSubscribe(pSpsConn->gapConnHandle, &(pSpsConn->client.creditSubscripe));
}

static void startFifoSubscription(spsConnection_t *pSpsConn)
{
    pSpsConn->client.fifoSubscripe.notifyCb = onFifoNotified;
    pSpsConn->client.fifoSubscripe.cccWriteRespCb = onFifoSubscribed;
    pSpsConn->client.fifoSubscripe.valueHandle = pSpsConn->client.attHandle.fifoValue;
    pSpsConn->client.fifoSubscripe.cccHandle = pSpsConn->client.attHandle.fifoCcc;
    pSpsConn->client.fifoSubscripe.receiveNotifications = true;
    pSpsConn->client.fifoSubscripe.receiveIndications = false;
    uPortGattSubscribe(pSpsConn->gapConnHandle, &(pSpsConn->client.fifoSubscripe));
}

static void mtuXchangeResp(int32_t gapConnHandle, uint8_t err)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    (void)err;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        spsEvent_t event;
        int32_t mtu;

        event.spsConnHandle = spsConnHandle;
        mtu = uPortGattGetMtu(gapConnHandle);
        if (mtu > 0) {
            pSpsConn->mtu = (uint16_t)mtu;
            uPortLog("U_BLE_SPS: MTU = %d\n", pSpsConn->mtu);
            event.type = EVENT_SPS_MTU_EXCHANGED;
        } else {
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }
}

//lint -esym(818, pUuid)
static uPortGattIter_t onCccDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                      uint16_t attrHandle)
{
    uPortGattIter_t returnValue = U_PORT_GATT_ITER_STOP;
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        spsEvent_t event;

        // The event might or might not be sent, depending
        // on whether there are more descriptors to discover
        event.spsConnHandle = spsConnHandle;
        if (pUuid != NULL) {
            if (pSpsConn->client.attHandle.fifoCcc == 0) {
                pSpsConn->client.attHandle.fifoCcc = attrHandle;
                if (pSpsConn->flowCtrlEnabled) {
                    // We have to discover the credits descriptor as well
                    returnValue = U_PORT_GATT_ITER_CONTINUE;
                } else {
                    event.type = EVENT_SPS_CCCS_DISCOVERED;
                }
            } else if (pSpsConn->client.attHandle.creditsCcc == 0) {
                pSpsConn->client.attHandle.creditsCcc = attrHandle;
                event.type = EVENT_SPS_CCCS_DISCOVERED;
            }
        } else {
            uPortLog("U_BLE_SPS: CCC Discovery failed!\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }

        if (returnValue == U_PORT_GATT_ITER_STOP) {
            uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
        }
    }

    return returnValue;
}

//lint -esym(818, pUuid)
static uPortGattIter_t onCreditCharDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                             uint16_t attrHandle, uint16_t valueHandle,
                                             uint8_t properties)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    (void)attrHandle;
    (void)properties;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsEvent_t event;

        event.spsConnHandle = spsConnHandle;
        if (pUuid != NULL) {
            pGetSpsConn(spsConnHandle)->client.attHandle.creditsValue = valueHandle;
            event.type = EVENT_SPS_CREDIT_CHAR_DISCOVERED;
        } else {
            uPortLog("U_BLE_SPS: SPS Credit Char Discovery failed\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }

    return U_PORT_GATT_ITER_STOP;
}

//lint -esym(818, pUuid)
static uPortGattIter_t onFifoCharDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                           uint16_t attrHandle, uint16_t valueHandle, uint8_t properties)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    (void)attrHandle;
    (void)properties;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsEvent_t event;

        event.spsConnHandle = spsConnHandle;
        if (pUuid != NULL) {
            pGetSpsConn(spsConnHandle)->client.attHandle.fifoValue = valueHandle;
            event.type = EVENT_SPS_FIFO_CHAR_DISCOVERED;
        } else {
            uPortLog("U_BLE_SPS: SPS FIFO Char Discovery failed\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }

    return U_PORT_GATT_ITER_STOP;
}

static uPortGattIter_t onSpsServiceDiscovery(int32_t gapConnHandle, uPortGattUuid_t *pUuid,
                                             uint16_t attrHandle, uint16_t endHandle)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    (void)endHandle;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsEvent_t event;

        event.spsConnHandle = spsConnHandle;
        if (pUuid != NULL) {
            pGetSpsConn(spsConnHandle)->client.attHandle.service = attrHandle;
            event.type = EVENT_SPS_SERVICE_DISCOVERED;
        } else {
            uPortLog("U_BLE_SPS: SPS Service Discovery failed!\n");
            event.type = EVENT_SPS_CONNECTING_FAILED;
        }
        uPortEventQueueSend(gSpsEventQueue, &event, sizeof(event));
    }

    return U_PORT_GATT_ITER_STOP;
}

static void onBleSpsEvent(void *pParam, size_t eventSize)
{
    spsEvent_t *pEvent = (spsEvent_t *)pParam;
    spsConnection_t *pSpsConn;
    (void)eventSize;

    if ((pEvent == NULL) || !validSpsConnHandle(pEvent->spsConnHandle)) {
        return;
    }

    pSpsConn = pGetSpsConn(pEvent->spsConnHandle);

    switch (pEvent->type) {

        case EVENT_GAP_CONNECTED:
            if (pSpsConn->client.attHandle.service == 0) {
                // If service handle is 0 we assume the handles was not
                // preset and we have to discover them
                // Start with the primary service handle
                uPortGattStartPrimaryServiceDiscovery(pSpsConn->gapConnHandle,
                                                      (uPortGattUuid_t *)&gSpsServiceUuid, onSpsServiceDiscovery);
            } else {
                // If service handle is different from zero we assume all
                // the service handles are preset and we can jump directly
                // to MTU exchange
                uPortGattExchangeMtu(pSpsConn->gapConnHandle, mtuXchangeResp);
            }
            break;

        case EVENT_SPS_SERVICE_DISCOVERED:
            // Primary service handle discovered
            // continue with FIFO characteristics handle
            uPortGattStartCharacteristicDiscovery(pSpsConn->gapConnHandle,
                                                  (uPortGattUuid_t *)&gSpsFifoCharUuid,
                                                  pSpsConn->client.attHandle.service + 1,
                                                  onFifoCharDiscovery);
            break;

        case EVENT_SPS_FIFO_CHAR_DISCOVERED:
            // FIFO characteristics handle discovered
            if (pSpsConn->flowCtrlEnabled) {
                // continue with credits characteristics handle
                uPortGattStartCharacteristicDiscovery(pSpsConn->gapConnHandle,
                                                      (uPortGattUuid_t *)&gSpsCreditsCharUuid,
                                                      pSpsConn->client.attHandle.fifoValue + 1,
                                                      onCreditCharDiscovery);
            } else {
                // Jump directly to descriptor discovery since
                // we are not interested in the credits handle
                uPortGattStartDescriptorDiscovery(pSpsConn->gapConnHandle,
                                                  U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                                  pSpsConn->client.attHandle.service + 2, onCccDiscovery);
            }
            break;

        case EVENT_SPS_CREDIT_CHAR_DISCOVERED:
            // Credits characteristics handle discovered
            // continue with characteristics descriptors
            // We expect to find two, one for FIFO and one
            // for Credits characteristics
            uPortGattStartDescriptorDiscovery(pSpsConn->gapConnHandle,
                                              U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                              pSpsConn->client.attHandle.service + 2, onCccDiscovery);
            break;

        case EVENT_SPS_CCCS_DISCOVERED:
            // The two characteristics descriptors are discovered
            // continue with MTU exchange
            uPortGattExchangeMtu(pSpsConn->gapConnHandle, mtuXchangeResp);
            break;

        case EVENT_SPS_MTU_EXCHANGED:
            // MTU exchanged
            // continue and start subscription to Credit notifications
            // from server if we want flow control, otherwise go
            // directly to FIFO subscription
            if (pSpsConn->flowCtrlEnabled) {
                startCreditSubscription(pSpsConn);
            } else {
                startFifoSubscription(pSpsConn);
            }
            break;

        case EVENT_SPS_CREDITS_SUBSCRIBED:
            // Credits subscription active
            // continue and start subscription to FIFO notifications
            startFifoSubscription(pSpsConn);
            break;

        case EVENT_SPS_FIFO_SUBSCRIBED:
            // FIFO subscribed, we can now receive data from server
            uPortLog("U_BLE_SPS: Connected as SPS client. Handle %d, remote addr: %s\n",
                     pEvent->spsConnHandle, pSpsConn->remoteAddr);
            pSpsConn->spsState = SPS_STATE_CONNECTED;
            if (gpSpsConnStatusCallback != NULL) {
                gpSpsConnStatusCallback(pEvent->spsConnHandle,
                                        pSpsConn->remoteAddr,
                                        (int32_t)U_BLE_SPS_CONNECTED,
                                        pEvent->spsConnHandle,
                                        pSpsConn->mtu,
                                        gpSpsConnStatusCallbackParam);
            }
            if (pSpsConn->flowCtrlEnabled) {
                updateRxCreditsOnRemote(pSpsConn);
            }
            break;

        case EVENT_SPS_CONNECTING_FAILED:
            // Callback gapConnectionEvent will be
            // called later and then reset the SPS connection
            uPortGattDisconnectGap(pSpsConn->gapConnHandle);
            break;

        case EVENT_SPS_RX_DATA_AVAILABLE:
            if (gpSpsDataAvailableCallback != NULL) {
                gpSpsDataAvailableCallback(pEvent->spsConnHandle, gpSpsDataAvailableCallbackParam);
            }
            break;
    }
}

static bool write16BitValue(const void *buf, uint16_t len, uint16_t offset, uint16_t *pVal)
{
    if (offset + len <= 2) {
        uint32_t i;
        const uint8_t *pBuf = (const uint8_t *)buf;
        for (i = offset; i < offset + len; i++) {
            *((uint8_t *)pVal + i) = *pBuf++;
        }
        return true;
    }

    return false;
}

static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    int32_t returnValue = -1;
    (void)flags;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        if (write16BitValue(buf, len, offset, &(pSpsConn->server.fifoClientConf))) {
            if ((pSpsConn->server.fifoClientConf & 1) &&
                !(pSpsConn->server.creditsClientConf & 1)) {
                // Client has configured FIFO notifications without
                // Credits notification, indicating a credit less SPS connection
                pSpsConn->flowCtrlEnabled = false;
                pSpsConn->spsState = SPS_STATE_CONNECTED;
                uPortLog("U_BLE_SPS: Connected as SPS server. Handle %d, remote addr: %s\n",
                         spsConnHandle, pSpsConn->remoteAddr);
                if (gpSpsConnStatusCallback != NULL) {
                    gpSpsConnStatusCallback(spsConnHandle,
                                            pSpsConn->remoteAddr,
                                            (int32_t)U_BLE_SPS_CONNECTED,
                                            spsConnHandle,
                                            pSpsConn->mtu,
                                            gpSpsConnStatusCallbackParam);
                }
            }
            returnValue = len;
        }
    }

    return returnValue;
}

static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                     uint16_t offset, uint8_t flags)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    int32_t returnValue = -1;
    (void)flags;
    (void)offset;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        if (write16BitValue(buf, len, offset, &(pSpsConn->server.creditsClientConf))) {
            returnValue = len;
        }
    }

    return returnValue;
}

static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    (void)offset;
    (void)flags;

    if (len > 0) {
        addReceivedDataToBuffer(findSpsConnHandle(gapConnHandle), buf, len);
    }

    return len;
}

static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags)
{
    int32_t spsConnHandle = findSpsConnHandle(gapConnHandle);
    int32_t returnValue = -1;
    (void)offset;
    (void)flags;

    if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
        uint8_t receivedCredits = *(const uint8_t *)buf;
        addLocalTxCredits(spsConnHandle, receivedCredits);

        returnValue = len;
    }

    return returnValue;
}

static int32_t hexToInt(const char *pIn, uint8_t *pOut)
{
    uint32_t i;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;

    *pOut = 0;
    for (i = 0; i < 2; i++) {
        char inChar = *pIn;
        uint8_t nibbleVal;

        if (inChar >= '0' && inChar <= '9') {
            nibbleVal = inChar - '0';
        } else if (inChar >= 'a' && inChar <= 'f') {
            nibbleVal = inChar + 10 - 'a';
        } else if (inChar >= 'A' && inChar <= 'F') {
            nibbleVal = inChar + 10 - 'A';
        } else {
            errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            break;
        }
//lint -save -e701 -e734
        *pOut |= nibbleVal << (4 * (1 - i));
//lint -restore
        pIn++;
    }

    return errorCode;
}

static int32_t addrStringToArray(const char *pAddrIn, uint8_t *pAddrOut,
                                 uPortBtLeAddressType_t *pType)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    uint32_t i;
    char lastChar = pAddrIn[12];

    for (i = 0; i < 6; i++) {
//lint -save -e679
        if (hexToInt(&pAddrIn[2 * i], &pAddrOut[5 - i]) != (int32_t)U_ERROR_COMMON_SUCCESS) {
//lint -restore
            errorCode = (int32_t)U_ERROR_COMMON_INVALID_ADDRESS;
            break;
        }
    }
    if (lastChar == 'p' || lastChar == 'P' || lastChar == '\0') {
        *pType = U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC;
    } else if (lastChar == 'r' || lastChar == 'R') {
        *pType = U_PORT_BT_LE_ADDRESS_TYPE_RANDOM;
    } else {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_ADDRESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
void uBleSpsPrivateInit(void)
{
    if (gSpsEventQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
        uPortMutexCreate(&gBleSpsMutex);
        uPortGattSetGapConnStatusCallback(gapConnectionEvent, NULL);

        gSpsEventQueue = uPortEventQueueOpen(onBleSpsEvent,
                                             "uBleSpsEventQueue", sizeof(spsEvent_t),
                                             U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                             U_CFG_OS_APP_TASK_PRIORITY + 1, 2 * U_BLE_SPS_MAX_CONNECTIONS);
    }
}

void uBleSpsPrivateDeinit(void)
{
    if (gSpsEventQueue != (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
        uPortGattSetGapConnStatusCallback(NULL, NULL);

        for (int32_t i = 0; i < U_BLE_SPS_MAX_CONNECTIONS; i++) {
            if (validSpsConnHandle(i)) {
                spsConnection_t *pSpsConn = pGetSpsConn(i);
                uPortGattDisconnectGap(pSpsConn->gapConnHandle);
                freeSpsConnection(i);
            }
        }

        uPortEventQueueClose(gSpsEventQueue);
        gSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
        uPortMutexDelete(gBleSpsMutex);
        gBleSpsMutex = NULL;
    }
}

int32_t uBleSpsSetCallbackConnectionStatus(uDeviceHandle_t devHandle,
                                           uBleSpsConnectionStatusCallback_t pCallback,
                                           void *pCallbackParameter)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    gpSpsConnStatusCallback = pCallback;
    gpSpsConnStatusCallbackParam = pCallbackParameter;

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

int32_t uBleSpsConnectSps(uDeviceHandle_t devHandle,
                          const char *pAddress,
                          const uBleSpsConnParams_t *pConnParams)
{
    uint8_t address[6];
    uPortBtLeAddressType_t addrType;
    int32_t gapConnHandle = U_PORT_GATT_GAP_INVALID_CONNHANDLE;
    int32_t spsConnHandle = U_BLE_SPS_INVALID_HANDLE;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    int32_t errorCode = addrStringToArray(pAddress, address, &addrType);

    U_PORT_MUTEX_LOCK(gBleSpsMutex);

    if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
        if (pConnParams == NULL) {
            pConnParams = &gConnParamsDefault;
        }
        gapConnHandle = uPortGattConnectGap(address, addrType, (const uPortGattGapParams_t *)pConnParams);

        if (!uPortGattIsAdvertising()) {
            // If we are advertising we are peripheral, then the connect
            // above will be a directed advertisement. The remote will
            // connect to our SPS server, just as if the connection was initiated
            // by the remote side. So then we don't have to do more here

            if (gapConnHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                spsConnHandle = findFreeSpsConnHandle();

                if (spsConnHandle != U_BLE_SPS_INVALID_HANDLE) {
                    spsConnection_t *pSpsConn = initSpsConnection(spsConnHandle, gapConnHandle, SPS_CLIENT);
                    if (pSpsConn != NULL) {
                        memcpy(pSpsConn->remoteAddr, pAddress, sizeof(pSpsConn->remoteAddr) - 1);
                        // Preset server handles (if they are not preset gNextConnServerHandles
                        // is all zero, which will trigger discovery later)
                        memcpy(&(pSpsConn->client.attHandle), &gNextConnServerHandles, sizeof(uBleSpsHandles_t));
                        // Maybe disable flow control
                        pSpsConn->flowCtrlEnabled = gFlowCtrlOnNext;
                        gFlowCtrlOnNext = true;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            }
        }
        // Reset the preset-handles so we don't accidentially reuse them for the next connection
        memset(&gNextConnServerHandles, 0x00, sizeof(gNextConnServerHandles));
    }

    U_PORT_MUTEX_UNLOCK(gBleSpsMutex);

    return errorCode;
}

int32_t uBleSpsDisconnect(uDeviceHandle_t devHandle, int32_t spsConnHandle)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (!validSpsConnHandle(spsConnHandle)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    return uPortGattDisconnectGap(pGetSpsConn(spsConnHandle)->gapConnHandle);
}

int32_t uBleSpsSetSendTimeout(uDeviceHandle_t devHandle, int32_t channel, uint32_t timeout)
{
    int32_t spsConnHandle = channel;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (!validSpsConnHandle(spsConnHandle)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    pGetSpsConn(spsConnHandle)->dataSendTimeoutMs = timeout;

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

int32_t uBleSpsSend(uDeviceHandle_t devHandle, int32_t channel, const char *pData, int32_t length)
{
    int32_t spsConnHandle = channel;
    int32_t bytesLeftToSend = length;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if ((pData == NULL) || (length < 0)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (!validSpsConnHandle(spsConnHandle)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    int64_t startTime = uPortGetTickTimeMs();
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
    if (pSpsConn->spsState == SPS_STATE_CONNECTED) {
        uint32_t timeout = pSpsConn->dataSendTimeoutMs;
        int64_t time = startTime;

        while ((bytesLeftToSend > 0) && (time - startTime < timeout)) {
            int32_t bytesToSendNow = bytesLeftToSend;
            int32_t maxDataLength = pSpsConn->mtu - U_BLE_PDU_HEADER_SIZE;

            if (bytesToSendNow > maxDataLength) {
                bytesToSendNow = maxDataLength;
            }
            if (pSpsConn->flowCtrlEnabled) {
                // If flow control is enabled we first have to make sure we have TX credits
                // before sending
                // If the semaphore is already given we first have to take it, so it can be given
                // again later if we are out of credits.
                (void)uPortSemaphoreTryTake(pSpsConn->txCreditsSemaphore, 0);
                if (pSpsConn->txCredits == 0) {
                    int32_t timeoutLeft = (int32_t)timeout - (int32_t)(time - startTime);
                    if (timeoutLeft < 0) {
                        timeoutLeft = 0;
                    }
                    // We are out of credits, wait for more
                    if (uPortSemaphoreTryTake(pSpsConn->txCreditsSemaphore, timeoutLeft) != 0) {
                        uPortLog("U_BLE_SPS: SPS Timed out waiting for new TX credits!\n");
                        break;
                    }
                }
            }
            if (!pSpsConn->flowCtrlEnabled || (pSpsConn->txCredits > 0)) {
                if (sendDataToRemoteFifo(pSpsConn, pData, (uint16_t)bytesToSendNow)) {
                    pData += bytesToSendNow;
                    bytesLeftToSend -= bytesToSendNow;
                    pSpsConn->txCredits--;
                }
            } else {
                // We have flow control enabled, we didn't time out waiting
                // for TX credits above, but we anyway don't have any TX credits.
                // Something is very wrong.
                errorCode = (int32_t)U_ERROR_COMMON_UNKNOWN;
                break;
            }
            if (bytesLeftToSend > 0) {
                time = uPortGetTickTimeMs();
            }
        }
    }

    if (errorCode < 0) {
        return errorCode;
    } else {
        return (length - bytesLeftToSend);
    }
}

int32_t uBleSpsSetDataAvailableCallback(uDeviceHandle_t devHandle,
                                        uBleSpsAvailableCallback_t pCallback,
                                        void *pCallbackParameter)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    gpSpsDataAvailableCallback = pCallback;
    gpSpsDataAvailableCallbackParam = pCallbackParameter;

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

int32_t uBleSpsReceive(uDeviceHandle_t devHandle, int32_t channel, char *pData, int32_t length)
{
    int32_t spsConnHandle = channel;
    int32_t sizeOrErrorCode;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (validSpsConnHandle(spsConnHandle)) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        sizeOrErrorCode = (int32_t)uRingBufferRead(&(pSpsConn->rxRingBuffer), pData, length);
        if ((sizeOrErrorCode > 0) && (pSpsConn->flowCtrlEnabled)) {
            updateRxCreditsOnRemote(pSpsConn);
        }
    } else {
        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    return sizeOrErrorCode;
}

int32_t uBleSpsGetSpsServerHandles(uDeviceHandle_t devHandle, int32_t channel,
                                   uBleSpsHandles_t *pHandles)
{
    int32_t spsConnHandle = channel;

    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    int32_t returnValue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (validSpsConnHandle(spsConnHandle)) {
        spsConnection_t *pSpsConn = pGetSpsConn(spsConnHandle);
        if  ((pSpsConn->localSpsRole == SPS_CLIENT) &&
             (pSpsConn->spsState == SPS_STATE_CONNECTED) &&
             pSpsConn->flowCtrlEnabled) {
            memcpy(pHandles, &(pSpsConn->client.attHandle), sizeof(uBleSpsHandles_t));
            returnValue = (int32_t)U_ERROR_COMMON_SUCCESS;
        }
    }

    return returnValue;
}

int32_t uBleSpsPresetSpsServerHandles(uDeviceHandle_t devHandle, const uBleSpsHandles_t *pHandles)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    memcpy(&gNextConnServerHandles, pHandles, sizeof(uBleSpsHandles_t));

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

int32_t uBleSpsDisableFlowCtrlOnNext(uDeviceHandle_t devHandle)
{
    if (uDeviceGetDeviceType(devHandle) != (int32_t)U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    gFlowCtrlOnNext = false;

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

#endif

// End of file
