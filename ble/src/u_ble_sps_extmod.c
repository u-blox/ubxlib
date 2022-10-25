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

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_cfg_sw.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"
#include "u_ble_sps.h"
#include "u_ble_private.h"
#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

#define U_BLE_SPS_EVENT_STACK_SIZE 1536
#define U_BLE_SPS_EVENT_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    uShortRangePrivateInstance_t *pInstance;
    int32_t connHandle;
    int32_t type;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    int32_t dataChannel;
    int32_t mtu;
    void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *);
    void *pCallbackParameter;
} uBleSpsConnection_t;

// Linked list with channel info like rx buffer and tx timeout
typedef struct uBleSpsChannel_s {
    int32_t                       channel;
    uShortRangePrivateInstance_t  *pInstance;
    uShortRangePbufList_t         *pSpsRxBuff;
    uint32_t                      txTimeout;
    struct uBleSpsChannel_s       *pNext;
} uBleSpsChannel_t;

typedef struct {
    int32_t channel;
    uShortRangePrivateInstance_t *pInstance;
} bleSpsEvent_t;

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLC_urc(uAtClientHandle_t atHandle, void *pParameter);
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLD_urc(uAtClientHandle_t atHandle, void *pParameter);
static void createSpsChannel(uShortRangePrivateInstance_t *pInstance,
                             int32_t channel, uBleSpsChannel_t **ppListHead);
static uBleSpsChannel_t *getSpsChannel(const uShortRangePrivateInstance_t *pInstance,
                                       int32_t channel, uBleSpsChannel_t *pListHead);
static void deleteSpsChannel(const uShortRangePrivateInstance_t *pInstance,
                             int32_t channel, uBleSpsChannel_t **ppListHead);
static void deleteAllSpsChannels(uBleSpsChannel_t **ppListHead) ;
static void spsEventCallback(uAtClientHandle_t atHandle, void *pParameter);
static void btEdmConnectionCallback(int32_t edmStreamHandle,
                                    int32_t edmChannel,
                                    uShortRangeConnectionEventType_t eventType,
                                    const uShortRangeConnectDataBt_t *pConnectData,
                                    void *pCallbackParameter);
static void atConnectionEvent(uDeviceHandle_t devHandle,
                              int32_t connHandle,
                              uShortRangeConnectionEventType_t eventType,
                              uShortRangeConnectDataBt_t *pConnectData,
                              void *pCallbackParameter);
static void dataCallback(int32_t handle, int32_t channel, uShortRangePbufList_t *pBufList,
                         void *pParameters);
static void onBleSpsEvent(void *pParam, size_t eventSize);
static int32_t setBleConfig(const uAtClientHandle_t atHandle,
                            int32_t parameter, uint32_t value);

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
static uBleSpsChannel_t *gpChannelList = NULL;
static int32_t gBleSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
static uPortMutexHandle_t gBleSpsMutex;
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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLC_urc(uAtClientHandle_t atHandle,
                         void *pParameter)
{
    (void)pParameter;
    // We only need to read out to clean up for the at client, all data we need
    // will arrive in later events.
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];

    (void)uAtClientReadInt(atHandle); // Connection handle
    (void)uAtClientReadInt(atHandle); // Type (always 0 meaning GATT)
    (void)uAtClientReadString(atHandle, address, U_SHORT_RANGE_BT_ADDRESS_SIZE, false);
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLD_urc(uAtClientHandle_t atHandle,
                         void *pParameter)
{
    (void)pParameter;
    // We only need to read out to clean up for the at client, all data we need
    // will arrive in later events.
    (void)uAtClientReadInt(atHandle); // Connection handle
}

// Allocate and add SPS channel info to linked list
static void createSpsChannel(uShortRangePrivateInstance_t *pInstance,
                             int32_t channel, uBleSpsChannel_t **ppListHead)
{
    uBleSpsChannel_t *pChannel = *ppListHead;

    U_PORT_MUTEX_LOCK(gBleSpsMutex);

    if (pChannel == NULL) {
        pChannel = (uBleSpsChannel_t *)pUPortMalloc(sizeof(uBleSpsChannel_t));
        *ppListHead = pChannel;
    } else {
        uint32_t nbrOfChannels = 1;
        while (pChannel->pNext != NULL) {
            pChannel = pChannel->pNext;
            nbrOfChannels++;
        }
        if (nbrOfChannels < U_BLE_SPS_MAX_CONNECTIONS) {
            pChannel->pNext = (uBleSpsChannel_t *)pUPortMalloc(sizeof(uBleSpsChannel_t));
        }
        pChannel = pChannel->pNext;
    }

    if (pChannel != NULL) {
        pChannel->pSpsRxBuff = NULL;
        pChannel->channel = channel;
        pChannel->pInstance = pInstance;
        pChannel->pNext = NULL;
        pChannel->txTimeout = U_BLE_SPS_DEFAULT_SEND_TIMEOUT_MS;
    } else {
        uPortLog("U_BLE_SPS: Failed to create data channel!\n");
    }

    U_PORT_MUTEX_UNLOCK(gBleSpsMutex);
}

// Get SPS channel info related to channel at instance
static uBleSpsChannel_t *getSpsChannel(const uShortRangePrivateInstance_t *pInstance,
                                       int32_t channel, uBleSpsChannel_t *pListHead)
{
    uBleSpsChannel_t *pChannel;

    U_PORT_MUTEX_LOCK(gBleSpsMutex);

    pChannel = pListHead;
    while (pChannel != NULL) {
        if ((pChannel->pInstance == pInstance) && (pChannel->channel == channel)) {
            break;
        }
        pChannel = pChannel->pNext;
    }

    U_PORT_MUTEX_UNLOCK(gBleSpsMutex);

    return pChannel;
}

// Delete SPS channel info (after disconnection)
static void deleteSpsChannel(const uShortRangePrivateInstance_t *pInstance,
                             int32_t channel, uBleSpsChannel_t **ppListHead)
{
    uBleSpsChannel_t *pChannel;
    uBleSpsChannel_t *pPrevChannel = NULL;

    U_PORT_MUTEX_LOCK(gBleSpsMutex);

    pChannel = *ppListHead;
    while ((pChannel != NULL) &&
           ((pChannel->pInstance != pInstance) || (pChannel->channel != channel))) {
        pPrevChannel = pChannel;
        pChannel = pChannel->pNext;
    }

    if (pChannel != NULL) {
        // Relink the list and free the channel
        if (pPrevChannel != NULL) {
            pPrevChannel->pNext = pChannel->pNext;
        } else {
            // This happens when the list only has one item
            *ppListHead = NULL;
        }
        uShortRangePbufListFree(pChannel->pSpsRxBuff);

        uPortFree(pChannel);
    }

    U_PORT_MUTEX_UNLOCK(gBleSpsMutex);
}

static void deleteAllSpsChannels(uBleSpsChannel_t **ppListHead)
{
    uBleSpsChannel_t *pChannel = *ppListHead;

    while (pChannel != NULL) {
        uBleSpsChannel_t *pChanToFree;

        uShortRangePbufListFree(pChannel->pSpsRxBuff);
        pChanToFree = pChannel;
        pChannel = pChannel->pNext;
        uPortFree(pChanToFree);
    }
    *ppListHead = NULL;
}

static void spsEventCallback(uAtClientHandle_t atHandle,
                             void *pParameter)
{
    uBleSpsConnection_t *pStatus = (uBleSpsConnection_t *)pParameter;

    (void) atHandle;

    if (pStatus != NULL) {
        if (pStatus->pCallback != NULL) {
            // We have to create the SPS channel info before calling the
            // callback since it will assume that e.g. the rx buffer exists,
            // for the same reason we have to delete it after calling the callback
            if (pStatus->type == (int32_t)U_SHORT_RANGE_EVENT_CONNECTED) {
                createSpsChannel(pStatus->pInstance, pStatus->dataChannel, &gpChannelList);
            }
            pStatus->pCallback(pStatus->connHandle, pStatus->address, pStatus->type,
                               pStatus->dataChannel, pStatus->mtu, pStatus->pCallbackParameter);
            if (pStatus->type == (int32_t)U_SHORT_RANGE_EVENT_DISCONNECTED) {
                deleteSpsChannel(pStatus->pInstance, pStatus->dataChannel, &gpChannelList);
            }
        }
        if (pStatus->pInstance != NULL) {
            pStatus->pInstance->pPendingSpsConnectionEvent = NULL;
        }

        uPortFree(pStatus);
    }
}

static void btEdmConnectionCallback(int32_t edmStreamHandle,
                                    int32_t edmChannel,
                                    uShortRangeConnectionEventType_t eventType,
                                    const uShortRangeConnectDataBt_t *pConnectData,
                                    void *pCallbackParameter)
{
    (void) edmStreamHandle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pCallbackParameter;

    if (pInstance != NULL && pInstance->atHandle != NULL) {
        uBleSpsConnection_t *pStatus = (uBleSpsConnection_t *)
                                       pInstance->pPendingSpsConnectionEvent;
        bool send = false;

        if (pStatus == NULL) {
            //lint -esym(593, pStatus) Suppress pStatus not being uPortFree()ed here
            pStatus = (uBleSpsConnection_t *) pUPortMalloc(sizeof(*pStatus));
        } else {
            send = true;
        }

        if (pStatus != NULL) {
            pStatus->pInstance = pInstance;
            if (eventType == U_SHORT_RANGE_EVENT_CONNECTED) {
                pStatus->mtu = pConnectData->framesize;
                addrArrayToString(pConnectData->address, U_PORT_BT_LE_ADDRESS_TYPE_UNKNOWN, false,
                                  pStatus->address);
            }
            pStatus->type = (int32_t)eventType;
            pStatus->dataChannel = edmChannel;
            pStatus->pCallback = pInstance->pSpsConnectionCallback;
            pStatus->pCallbackParameter = pInstance->pSpsConnectionCallbackParameter;
        }
        if (send) {
            uAtClientCallback(pInstance->atHandle, spsEventCallback, pStatus);
        } else {
            pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
        }
    }
}

static void atConnectionEvent(uDeviceHandle_t devHandle,
                              int32_t connHandle,
                              uShortRangeConnectionEventType_t eventType,
                              uShortRangeConnectDataBt_t *pConnectData,
                              void *pCallbackParameter)
{
    (void)devHandle;
    (void)pConnectData;
    (void)eventType;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pCallbackParameter;
    bool send = false;

    if (pInstance->pSpsConnectionCallback != NULL) {
        uBleSpsConnection_t *pStatus = (uBleSpsConnection_t *)
                                       pInstance->pPendingSpsConnectionEvent;

        if (pStatus == NULL) {
            //lint -esym(429, pStatus) Suppress pStatus not being uPortFree()ed here
            pStatus = (uBleSpsConnection_t *) pUPortMalloc(sizeof(*pStatus));
        } else {
            send = true;
        }
        if (pStatus != NULL) {
            pStatus->connHandle = connHandle;
            // AT (this) event info: connHandle, type, profile, address, mtu
            // EDM event info: type, profile, address, mtu, channel
            // use connHandle from here, the rest from the EDM event

            if (send) {
                uAtClientCallback(pInstance->atHandle, spsEventCallback, pStatus);
            } else {
                pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
            }
        }
    }
}

static void dataCallback(int32_t handle, int32_t channel, uShortRangePbufList_t *pBufList,
                         void *pParameters)
{
    (void)handle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *)pParameters;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        if (pInstance != NULL) {

            if (pInstance->pBtDataAvailableCallback != NULL) {
                uBleSpsChannel_t *pChannel = getSpsChannel(pInstance, channel, gpChannelList);

                if (pChannel != NULL) {
                    bool bufferWasEmtpy = (pChannel->pSpsRxBuff == NULL);
                    if (pChannel->pSpsRxBuff == NULL) {
                        pChannel->pSpsRxBuff = pBufList;
                    } else {
                        uShortRangePbufListMerge(pChannel->pSpsRxBuff, pBufList);
                    }

                    if (bufferWasEmtpy) {
                        bleSpsEvent_t event;
                        event.channel = channel;
                        event.pInstance = pInstance;
                        uPortEventQueueSend(gBleSpsEventQueue, &event, sizeof(event));
                    }
                }
            }
        }
        uShortRangeUnlock();
    }
}

static void onBleSpsEvent(void *pParam, size_t eventSize)
{
    (void)eventSize;

    bleSpsEvent_t *pEvent = (bleSpsEvent_t *)pParam;
    if (pEvent->pInstance->pBtDataAvailableCallback != NULL) {
        pEvent->pInstance->pBtDataAvailableCallback(pEvent->channel,
                                                    pEvent->pInstance->pBtDataCallbackParameter);
    }
}

static void removeCallbacks(uDeviceHandle_t devHandle,
                            uShortRangePrivateInstance_t *pInstance)
{
    uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUBTACLC:");
    uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUBTACLD:");
    uShortRangeSetBtConnectionStatusCallback(devHandle, NULL, NULL);
    uShortRangeEdmStreamBtEventCallbackSet(pInstance->streamHandle, NULL, NULL);
    pInstance->pSpsConnectionCallback = NULL;
    pInstance->pSpsConnectionCallbackParameter = NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int32_t uBleSpsSetCallbackConnectionStatus(uDeviceHandle_t devHandle,
                                           uBleSpsConnectionStatusCallback_t pCallback,
                                           void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (pCallback != NULL) {
                removeCallbacks(devHandle, pInstance);
                pInstance->pSpsConnectionCallback = pCallback;
                pInstance->pSpsConnectionCallbackParameter = pCallbackParameter;

                errorCode = uAtClientSetUrcHandler(pInstance->atHandle, "+UUBTACLC:",
                                                   UUBTACLC_urc, pInstance);

                if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    errorCode = uAtClientSetUrcHandler(pInstance->atHandle, "+UUBTACLD:",
                                                       UUBTACLD_urc, pInstance);
                }

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = uShortRangeSetBtConnectionStatusCallback(devHandle, atConnectionEvent,
                                                                         (void *) pInstance);
                }

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = uShortRangeEdmStreamBtEventCallbackSet(pInstance->streamHandle, btEdmConnectionCallback,
                                                                       pInstance);
                }

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    removeCallbacks(devHandle, pInstance);
                }

            } else if (pCallback == NULL && pInstance->pSpsConnectionCallback != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                removeCallbacks(devHandle, pInstance);
            }

        }

        uShortRangeUnlock();
    }

    return errorCode;
}

static int32_t setBleConfig(const uAtClientHandle_t atHandle, int32_t parameter, uint32_t value)
{
    int32_t error;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UBTLECFG=");
    uAtClientWriteInt(atHandle, parameter);
    uAtClientWriteInt(atHandle, (int32_t)value);
    uAtClientCommandStopReadResponse(atHandle);
    error = uAtClientUnlock(atHandle);

    if (error != (int32_t) U_ERROR_COMMON_SUCCESS) {
        uPortLog("U_BLE_SPS: Could not set BLE config param %d with value %d\n", parameter, value);
    }

    return error;
}

int32_t uBleSpsConnectSps(uDeviceHandle_t devHandle,
                          const char *pAddress,
                          const uBleSpsConnParams_t *pConnParams)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                char url[32];
                snprintf(url, sizeof(url), "sps://%s", pAddress);
                atHandle = pInstance->atHandle;

                uPortLog("U_BLE_SPS: Setting config\n");

                if (pConnParams == NULL) {
                    pConnParams = &gConnParamsDefault;
                }

                errorCode = setBleConfig(atHandle, 4, 6); // set to min to avoid errors
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 5, pConnParams->connIntervalMax);
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 4, pConnParams->connIntervalMin);
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 6, pConnParams->connLatency);
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 7, pConnParams->linkLossTimeout);
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 8, pConnParams->createConnectionTmo);
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 10, 16); // set to min to avoid errors
                    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                        errorCode = setBleConfig(atHandle, 9, pConnParams->scanInterval);
                    }
                }
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = setBleConfig(atHandle, 10, pConnParams->scanWindow);
                }

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    uPortLog("U_BLE_SPS: Sending AT+UDCP\n");

                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UDCP=");
                    uAtClientWriteString(atHandle, (char *)&url[0], false);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UDCP:");
                    uAtClientReadInt(atHandle); // conn handle
                    uAtClientResponseStop(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                }
            }
        }

        uShortRangeUnlock();
    }

    return errorCode;
}

int32_t uBleSpsDisconnect(uDeviceHandle_t devHandle, int32_t connHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uPortLog("U_SHORT_RANGE: Sending disconnect\n");

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDCPC=");
            uAtClientWriteInt(atHandle, connHandle);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        uShortRangeUnlock();
    }

    return errorCode;
}

int32_t uBleSpsReceive(uDeviceHandle_t devHandle, int32_t channel, char *pData, int32_t length)
{
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    uShortRangePbufList_t *pBufList;
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        if (pInstance != NULL) {
            uBleSpsChannel_t *pChannel = getSpsChannel(pInstance, channel, gpChannelList);
            if (pChannel != NULL) {
                pBufList = pChannel->pSpsRxBuff;
                sizeOrErrorCode = (int32_t)uShortRangePbufListConsumeData(pBufList, pData, length);
                if ((pBufList != NULL) && (pBufList->totalLen == 0)) {
                    uShortRangePbufListFree(pBufList);
                    pChannel->pSpsRxBuff = NULL;
                }
            }
        }
        uShortRangeUnlock();
    }

    return sizeOrErrorCode;
}

int32_t uBleSpsSend(uDeviceHandle_t devHandle, int32_t channel, const char *pData, int32_t length)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            uBleSpsChannel_t *pChannel = getSpsChannel(pInstance, channel, gpChannelList);
            errorCode = uShortRangeEdmStreamWrite(pInstance->streamHandle, channel, pData, length,
                                                  pChannel->txTimeout);
        }

        uShortRangeUnlock();
    }

    return errorCode;
}

int32_t uBleSpsSetSendTimeout(uDeviceHandle_t devHandle, int32_t channel, uint32_t timeout)
{
    int32_t returnValue = (int32_t)U_ERROR_COMMON_UNKNOWN;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uShortRangePrivateInstance_t *pInstance;

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            uBleSpsChannel_t *pChannel = getSpsChannel(pInstance, channel, gpChannelList);

            if (pChannel != NULL) {
                pChannel->txTimeout = timeout;
                returnValue = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }

        uShortRangeUnlock();
    }

    return returnValue;
}

int32_t uBleSpsSetDataAvailableCallback(uDeviceHandle_t devHandle,
                                        uBleSpsAvailableCallback_t pCallback,
                                        void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {

        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (pCallback != NULL) {
                pInstance->pBtDataAvailableCallback = pCallback;
                pInstance->pBtDataCallbackParameter = pCallbackParameter;

                if (gBleSpsEventQueue == (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                    gBleSpsEventQueue = uPortEventQueueOpen(onBleSpsEvent,
                                                            "uBleSpsEventQueue", sizeof(bleSpsEvent_t),
                                                            U_BLE_SPS_EVENT_STACK_SIZE,
                                                            U_BLE_SPS_EVENT_PRIORITY,
                                                            2 * U_BLE_SPS_MAX_CONNECTIONS);
                    if (gBleSpsEventQueue < 0) {
                        gBleSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
                    }
                }

                errorCode =
                    uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                             U_SHORT_RANGE_CONNECTION_TYPE_BT,
                                                             dataCallback, pInstance);
            } else if (pInstance->pBtDataAvailableCallback != NULL && pCallback == NULL) {
                pInstance->pBtDataAvailableCallback = NULL;
                pInstance->pBtDataCallbackParameter = NULL;

                errorCode =
                    uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                             U_SHORT_RANGE_CONNECTION_TYPE_BT,
                                                             NULL, NULL);
                if (gBleSpsEventQueue != (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
                    uPortEventQueueClose(gBleSpsEventQueue);
                    gBleSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
                }
            }
        }

        uShortRangeUnlock();
    }

    return errorCode;
}

void uBleSpsPrivateInit(void)
{
    if (gBleSpsMutex == NULL) {
        uPortMutexCreate(&gBleSpsMutex);
    }
}

void uBleSpsPrivateDeinit(void)
{
    if (gBleSpsEventQueue != (int32_t)U_ERROR_COMMON_NOT_INITIALISED) {
        uPortEventQueueClose(gBleSpsEventQueue);
        gBleSpsEventQueue = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    }
    deleteAllSpsChannels(&gpChannelList);
    if (gBleSpsMutex != NULL) {
        uPortMutexDelete(gBleSpsMutex);
        gBleSpsMutex = NULL;
    }
}

//lint -esym(818, pHandles) Suppress pHandles could be const, need to
// follow prototype
int32_t uBleSpsGetSpsServerHandles(uDeviceHandle_t devHandle, int32_t channel,
                                   uBleSpsHandles_t *pHandles)
{
    (void)channel;
    (void)devHandle;
    (void)pHandles;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uBleSpsPresetSpsServerHandles(uDeviceHandle_t devHandle, const uBleSpsHandles_t *pHandles)
{
    (void)devHandle;
    (void)pHandles;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uBleSpsDisableFlowCtrlOnNext(uDeviceHandle_t devHandle)
{
    (void)devHandle;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

#endif

// End of file
