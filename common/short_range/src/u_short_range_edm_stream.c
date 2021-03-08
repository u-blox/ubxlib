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

/** @file
 * @brief Implementation of the short range edm stream.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#include "u_port_uart.h"
#include "u_at_client.h"
#include "u_short_range_edm_stream.h"
#include "u_short_range_edm.h"

#include "string.h" // For memcpy()

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SHORT_RANGE_EDM_STREAM_UART_RETRIES
#define U_SHORT_RANGE_EDM_STREAM_UART_RETRIES       1
#endif
#ifndef U_SHORT_RANGE_EDM_STREAM_UART_RETRY_TIMEOUT
#define U_SHORT_RANGE_EDM_STREAM_UART_RETRY_TIMEOUT 1
#endif

#define U_SHORT_RANGE_EDM_STREAM_BUFFER_SIZE        0x1001
#define U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH  200
// TODO: is this value correc?
#define U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH 500
#define U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS    9

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef enum {
    U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_HEADER,
    U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_LENGTH,
    U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_REST
} uShortRangeEdmStreamParseState_t;

typedef enum {
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT,
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI,
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID
} uShortRangeEdmStreamConnectionType_t;

typedef enum {
    U_SHORT_RANGE_EDM_STREAM_CONNECTED,
    U_SHORT_RANGE_EDM_STREAM_DISCONNECTED
} uShortRangeEdmStreamConnectionEvent_t;

typedef struct uShortRangeEdmStreamBtEvent_t {
    uShortRangeEdmStreamConnectionEvent_t type;
    uint32_t channel;
    bool ble;
    uint8_t address[U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH];
    uint32_t frameSize;
} uShortRangeEdmStreamBtEvent_t;

typedef struct uShortRangeEdmStreamDataEvent_t {
    int32_t channel;
    char *pData;
    int32_t length;
} uShortRangeEdmStreamDataEvent_t;

typedef struct uShortRangeEdmStreamConnections_t {
    int32_t channel;
    uShortRangeEdmStreamConnectionType_t type;
    int32_t frameSize;
} uShortRangeEdmStreamConnections_t;

typedef struct uEdmStreamInstance_t {
    int32_t handle;
    int32_t uartHandle;
    void *atHandle;
    int32_t atEventQueueHandle;
    int32_t btEventQueueHandle;
    int32_t dataEventQueueHandle;
    void (*pAtCallback)(int32_t, uint32_t, void *);
    void *pAtCallbackParam;
    void (*pBtEventCallback)(int32_t, uint32_t, uint32_t, bool, int32_t, char *, void *);
    void *pBtEventCallbackParam;
    //void (*pWifiEventCallback)(int32_t, uint32_t, void *);
    //void *pWifiEventCallbackParam;
    void (*pBtDataCallback)(int32_t, int32_t, int32_t, char *, void *);
    void *pBtDataCallbackParam;
    void (*pWifiDataCallback)(int32_t, int32_t, int32_t, char *, void *);
    void *pWifiDataCallbackParam;
    bool uartBufferAvailable;
    char *pUartBuffer;
    char *pAtCommandBuffer;
    int32_t atCommandCurrent;
    char *pAtResponseBuffer;
    int32_t atResponseLength;
    int32_t atResponseRead;
    uShortRangeEdmStreamConnections_t connections[U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS];
} uShortRangeEdmStreamInstance_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static uPortMutexHandle_t gMutex = NULL;
static uShortRangeEdmStreamInstance_t gEdmStream;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void fillBuffer(void);
static void flushUart(int32_t uartHandle);

// Find connection from channel, use -1 to get the first free slot
static uShortRangeEdmStreamConnections_t *findConnection(int32_t channel)
{
    uShortRangeEdmStreamConnections_t *pConnection = NULL;

    for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
        if (gEdmStream.connections[i].channel == channel) {
            pConnection = &gEdmStream.connections[i];
            break;
        }
    }

    return pConnection;
}

// Event handler, calls the user's event callback.
static void atEventHandler(void *pParam, size_t paramLength)
{
    (void) pParam;
    (void) paramLength;

    if (gEdmStream.pAtCallback != NULL) {
        gEdmStream.pAtCallback(gEdmStream.handle,
                               U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                               gEdmStream.pAtCallbackParam);
    }
}

static void btEventHandler(void *pParam, size_t paramLength)
{
    uShortRangeEdmStreamBtEvent_t *pBtEvent = (uShortRangeEdmStreamBtEvent_t *) pParam;

    (void) paramLength;

    if (pBtEvent != NULL) {
        if (gEdmStream.pBtEventCallback != NULL) {
            gEdmStream.pBtEventCallback(gEdmStream.handle, (uint32_t) pBtEvent->type, pBtEvent->channel,
                                        pBtEvent->ble, (int32_t) pBtEvent->frameSize, (char *)&pBtEvent->address[0],
                                        gEdmStream.pBtEventCallbackParam);
        }
    }

    gEdmStream.uartBufferAvailable = true;
    uPortUartEventSend(gEdmStream.uartHandle,
                       U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
}

static void dataEventHandler(void *pParam, size_t paramLength)
{
    uShortRangeEdmStreamDataEvent_t *pDataEvent = (uShortRangeEdmStreamDataEvent_t *) pParam;

    (void) paramLength;

    if (pDataEvent != NULL) {
        uShortRangeEdmStreamConnections_t *pConnection = findConnection(pDataEvent->channel);

        if (pConnection != NULL && pConnection->type == U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) {
            if (gEdmStream.pBtDataCallback != NULL) {
                gEdmStream.pBtDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                           pDataEvent->pData, gEdmStream.pBtDataCallbackParam);
            }
        } else if (pConnection != NULL &&
                   pConnection->type == U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI) {
            if (gEdmStream.pWifiDataCallback != NULL) {
                gEdmStream.pWifiDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                             pDataEvent->pData, gEdmStream.pWifiDataCallbackParam);
            }
        }
    }

    gEdmStream.uartBufferAvailable = true;
    uPortUartEventSend(gEdmStream.uartHandle,
                       U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
}

static void uartCallback(int32_t uartHandle, uint32_t eventBitmask,
                         void *pParameters)
{
    (void)pParameters;
    if (gEdmStream.uartHandle == uartHandle &&
        eventBitmask == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        if (gEdmStream.uartBufferAvailable) {
            fillBuffer();
        }
    }
}

//gEdmStream.uartBufferMutex must be locked before calling this function
static void fillBuffer(void)
{
    int32_t sizeOrError = -1;
    size_t read = 0;
    int32_t length = 0;
    uShortRangeEdmStreamParseState_t state = U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_HEADER;

    do {
        uShortRangeEdmEvent_t evt;
        size_t consumed;
        if (state == U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_HEADER) {
            sizeOrError = uPortUartRead(gEdmStream.uartHandle,
                                        gEdmStream.pUartBuffer,
                                        1);
            if (sizeOrError > 0) {
                int32_t dummy;
                int32_t res = uShortRangeEdmParse(gEdmStream.pUartBuffer, 1, &evt, &dummy, &consumed);
                if (res == U_SHORT_RANGE_EDM_ERROR_INCOMPLETE) {
                    read = 1;
                    state = U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_LENGTH;
                }
            } else {
                gEdmStream.uartBufferAvailable = true;
            }
        } else if (state == U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_LENGTH) {
            sizeOrError = uPortUartRead(gEdmStream.uartHandle,
                                        gEdmStream.pUartBuffer + read,
                                        2);
            if (sizeOrError == 1) {
                read++;
            } else if ((sizeOrError == 1 && read == 2) || sizeOrError == 2) {
                int32_t res = uShortRangeEdmParse(gEdmStream.pUartBuffer, 3, &evt, &length, &consumed);
                if (res == U_SHORT_RANGE_EDM_ERROR_INCOMPLETE && length != -1) {
                    //Add for header and tail
                    length += 4;
                    if (length > U_SHORT_RANGE_EDM_STREAM_BUFFER_SIZE) {
                        //Packet is to large, discard by setting base state
                        read = 0;
                        state = U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_HEADER;
                    } else {
                        read = 3;
                        state = U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_REST;
                    }
                }
            } else {
                gEdmStream.uartBufferAvailable = true;
            }
        } else if (state == U_SHORT_RANGE_EDM_STREAM_PARSE_STATE_REST) {
            int8_t retryCounter = 0;
            //We can only handle a full EDM packet and the module will always try to send
            //a full packet, so if there are not enough data retry after a short delay to
            //make sure this is not due to a slow UART.
            //If this still fails, returning from this function at least recover so that
            //the next EDM packet are handle correctly.
            do {
                sizeOrError = uPortUartRead(gEdmStream.uartHandle,
                                            gEdmStream.pUartBuffer + read,
                                            length - read);

                if (sizeOrError > 0) {
                    break;
                }
                uPortTaskBlock(U_SHORT_RANGE_EDM_STREAM_UART_RETRY_TIMEOUT);
                retryCounter++;
            } while (retryCounter <= U_SHORT_RANGE_EDM_STREAM_UART_RETRIES);

            if (sizeOrError > 0) {
                //parse if ok
                read += sizeOrError;
                if (length == (int32_t) read) {
                    //Full packet process
                    gEdmStream.uartBufferAvailable = false;
                    int res = uShortRangeEdmParse(gEdmStream.pUartBuffer, read, &evt, &length, &consumed);
                    if (res != U_SHORT_RANGE_EDM_OK) {
                        gEdmStream.uartBufferAvailable = true;
                        sizeOrError = 0;
                    } else {
                        if (evt.type == U_SHORT_RANGE_EDM_EVENT_AT) {
                            gEdmStream.atResponseLength = evt.params.atEvent.length;
                            gEdmStream.atResponseRead = 0;
                            memcpy(gEdmStream.pAtResponseBuffer, evt.params.atEvent.pData, gEdmStream.atResponseLength);

                            uPortEventQueueSendIrq(gEdmStream.atEventQueueHandle,
                                                   &gEdmStream.handle, sizeof(int32_t));

                            sizeOrError = 0;
                        } else if (evt.type == U_SHORT_RANGE_EDM_EVENT_CONNECT_BT) {
                            uShortRangeEdmStreamConnections_t *pConnection = findConnection(evt.params.btConnectEvent.channel);
                            if (pConnection == NULL) {
                                pConnection = findConnection(-1);
                            }
                            if (pConnection != NULL) {
                                uShortRangeEdmStreamBtEvent_t event;
                                pConnection->channel = evt.params.btConnectEvent.channel;
                                pConnection->type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT;
                                pConnection->frameSize = evt.params.btConnectEvent.framesize;

                                event.type = U_SHORT_RANGE_EDM_STREAM_CONNECTED;
                                event.ble = (evt.params.btConnectEvent.profile == U_SHORT_RANGE_EDM_BT_PROFILE_SPS);
                                event.channel = evt.params.btConnectEvent.channel;
                                memcpy(event.address, evt.params.btConnectEvent.address, U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH);
                                event.frameSize = evt.params.btConnectEvent.framesize;

                                uPortEventQueueSendIrq(gEdmStream.btEventQueueHandle,
                                                       &event, sizeof(uShortRangeEdmStreamBtEvent_t));

                                sizeOrError = 0;
                            }
                        } else if (evt.type == U_SHORT_RANGE_EDM_EVENT_DISCONNECT) {
                            int32_t channel = (int32_t)evt.params.disconnectEvent.channel;
                            uShortRangeEdmStreamConnections_t *pConnection = findConnection(channel);

                            if (pConnection != NULL && pConnection->type == U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) {
                                uShortRangeEdmStreamBtEvent_t event;

                                pConnection->channel = -1;
                                pConnection->type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID;
                                pConnection->frameSize = -1;
                                event.type = U_SHORT_RANGE_EDM_STREAM_DISCONNECTED;
                                event.channel = channel;

                                uPortEventQueueSendIrq(gEdmStream.btEventQueueHandle,
                                                       &event, sizeof(uShortRangeEdmStreamBtEvent_t));
                            } else if (pConnection != NULL &&
                                       pConnection->type == U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI) {
                                gEdmStream.uartBufferAvailable = true;
                            } else {
                                gEdmStream.uartBufferAvailable = true;
                            }

                            sizeOrError = 0;
                        } else if (evt.type == U_SHORT_RANGE_EDM_EVENT_DATA) {
                            uShortRangeEdmStreamDataEvent_t event;
                            event.channel = evt.params.dataEvent.channel;
                            event.pData = evt.params.dataEvent.pData;
                            event.length = evt.params.dataEvent.length;

                            uPortEventQueueSendIrq(gEdmStream.dataEventQueueHandle,
                                                   &event, sizeof(uShortRangeEdmStreamDataEvent_t));

                            sizeOrError = 0;
                        } else if (evt.type == U_SHORT_RANGE_EDM_EVENT_STARTUP) {
                            gEdmStream.uartBufferAvailable = true;
                        } else {
                            gEdmStream.uartBufferAvailable = true;
                        }
                    }
                }
            }
        }
    } while ((sizeOrError > 0) && (read <= U_SHORT_RANGE_EDM_STREAM_BUFFER_SIZE));
}

static void flushUart(int32_t uartHandle)
{
    int32_t length = uPortUartGetReceiveSize(uartHandle);

    if (length > 0) {
        char *pDummy = (char *)malloc(length);
        uPortUartRead(uartHandle, pDummy, length);
        free(pDummy);
    }
}

static int32_t uartWrite(const void *pData, size_t length)
{
    return uPortUartWrite(gEdmStream.uartHandle,
                          pData, length);
}

// Do an EDM send.  Returns the amount written, including
// EDM packet overhead.
static int32_t edmSend(const uShortRangeEdmStreamInstance_t *pEdmStream)
{
    char *pPacket;
    size_t written = 0;
    int32_t sizeOrError = (int32_t) U_ERROR_COMMON_NO_MEMORY;

    pPacket = (char *) malloc(U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH +
                              U_SHORT_RANGE_EDM_REQUEST_OVERHEAD);
    if (pPacket != NULL) {
        sizeOrError = uShortRangeEdmRequest(pEdmStream->pAtCommandBuffer,
                                            pEdmStream->atCommandCurrent,
                                            pPacket);
        if (sizeOrError > 0) {
            while (written < (uint32_t) sizeOrError) {
                written += uartWrite((void *) (pPacket + written),
                                     (uint32_t) sizeOrError - written);
            }
        }
    }

    free(pPacket);

    return sizeOrError;
}

// A transmit intercept function.
//lint -e{818} Suppress 'pContext' could be declared as const:
// need to follow function signature
static const char *pInterceptTx(uAtClientHandle_t atHandle,
                                const char **ppData,
                                size_t *pLength,
                                void *pContext)
{
    int32_t x = 0;

    (void) pContext;
    (void) atHandle;

    if ((*pLength != 0) || (ppData == NULL)) {
        if (ppData == NULL) {
            // We're being flushed, create and send EDM packet
            edmSend(&gEdmStream);
            // Reset buffer
            gEdmStream.atCommandCurrent = 0;
        } else {
            // Send any whole buffer's worths we have
            while ((*pLength + gEdmStream.atCommandCurrent > U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH) &&
                   (x >= 0)) {
                x = U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH - gEdmStream.atCommandCurrent;
                memcpy(gEdmStream.pAtCommandBuffer + gEdmStream.atCommandCurrent, *ppData, x);
                *pLength -= x;
                *ppData += x;
                gEdmStream.atCommandCurrent = U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH;
                // Send a chunk
                x = edmSend(&gEdmStream);
                if (x < 0) {
                    // Error recovery: tell the caller we've consumed the lot
                    *ppData += *pLength;
                    *pLength = 0;
                }
                gEdmStream.atCommandCurrent = 0;
            }
            // Copy in any partial buffer, will be sent when we are flushed
            memcpy(gEdmStream.pAtCommandBuffer + gEdmStream.atCommandCurrent, *ppData, *pLength);
            gEdmStream.atCommandCurrent += (int32_t) * pLength;
            // Tell the caller what we've consumed.
            *ppData += *pLength;
        }
    }

    // All data is handled here, this makes the AT client know
    // that there is nothing to send on to a UART or whatever
    *pLength = 0;

    return 0;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeEdmStreamInit()
{
    uErrorCode_t errorCodeOrHandle = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCodeOrHandle = (uErrorCode_t)uPortMutexCreate(&gMutex);
        gEdmStream.handle = -1;
    }

    return (int32_t) errorCodeOrHandle;
}

void uShortRangeEdmStreamDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

int32_t uShortRangeEdmStreamOpen(int32_t uartHandle)
{
    uErrorCode_t handleOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);
        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;

        if (uartHandle >= 0 && gEdmStream.handle == -1
            && gEdmStream.pUartBuffer == NULL) {

            int32_t errorCode = uPortUartEventCallbackSet(uartHandle,
                                                          U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                          uartCallback, NULL,
                                                          U_EDM_STREAM_TASK_STACK_SIZE_BYTES,
                                                          U_EDM_STREAM_TASK_PRIORITY);

            if (errorCode == 0) {
                gEdmStream.pUartBuffer = (char *)malloc(U_SHORT_RANGE_EDM_STREAM_BUFFER_SIZE);
                memset(gEdmStream.pUartBuffer, 0, U_SHORT_RANGE_EDM_STREAM_BUFFER_SIZE);
                gEdmStream.pAtCommandBuffer = (char *)malloc(U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH);
                memset(gEdmStream.pAtCommandBuffer, 0, U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH);
                gEdmStream.pAtResponseBuffer = (char *)malloc(U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH);
                memset(gEdmStream.pAtResponseBuffer, 0, U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH);
                if (gEdmStream.pUartBuffer == NULL || gEdmStream.pAtCommandBuffer == NULL ||
                    gEdmStream.pAtResponseBuffer == NULL) {
                    handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
                    uPortUartEventCallbackRemove(uartHandle);
                } else {
                    gEdmStream.handle = 0;
                    gEdmStream.uartHandle = uartHandle;
                    gEdmStream.atHandle = NULL;
                    gEdmStream.atEventQueueHandle = -1;
                    gEdmStream.btEventQueueHandle = -1;
                    gEdmStream.dataEventQueueHandle = -1;
                    gEdmStream.pAtCallback = NULL;
                    gEdmStream.pAtCallbackParam = NULL;
                    gEdmStream.pBtEventCallback = NULL;
                    gEdmStream.pBtEventCallbackParam = NULL;
                    gEdmStream.pBtDataCallback = NULL;
                    gEdmStream.pBtDataCallbackParam = NULL;
                    gEdmStream.pWifiDataCallback = NULL;
                    gEdmStream.pWifiDataCallbackParam = NULL;
                    gEdmStream.atCommandCurrent = 0;
                    gEdmStream.uartBufferAvailable = true;

                    for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
                        gEdmStream.connections[i].channel = -1;
                        gEdmStream.connections[i].frameSize = -1;
                        gEdmStream.connections[i].type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID;
                    }

                    handleOrErrorCode = (uErrorCode_t)gEdmStream.handle;
                    flushUart(uartHandle);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) handleOrErrorCode;
}

void uShortRangeEdmStreamClose(int32_t handle)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle != -1) && (handle == gEdmStream.handle)) {
            gEdmStream.handle = -1;
            if (gEdmStream.uartHandle >= 0) {
                uPortUartEventCallbackRemove(gEdmStream.uartHandle);
            }
            gEdmStream.uartHandle = -1;
            if (gEdmStream.atEventQueueHandle >= 0) {
                uPortEventQueueClose(gEdmStream.atEventQueueHandle);
            }
            gEdmStream.atEventQueueHandle = -1;
            if (gEdmStream.btEventQueueHandle >= 0) {
                uPortEventQueueClose(gEdmStream.btEventQueueHandle);
            }
            gEdmStream.btEventQueueHandle = -1;
            if (gEdmStream.dataEventQueueHandle >= 0) {
                uPortEventQueueClose(gEdmStream.dataEventQueueHandle);
            }
            gEdmStream.dataEventQueueHandle = -1;
            if (gEdmStream.atHandle != NULL) {
                uAtClientStreamInterceptTx(gEdmStream.atHandle, NULL, NULL);
            }
            gEdmStream.atHandle = NULL;
            gEdmStream.pAtCallback = NULL;
            gEdmStream.pAtCallbackParam = NULL;
            gEdmStream.pBtEventCallback = NULL;
            gEdmStream.pBtEventCallbackParam = NULL;
            gEdmStream.pBtDataCallback = NULL;
            gEdmStream.pBtDataCallbackParam = NULL;
            gEdmStream.pWifiDataCallback = NULL;
            gEdmStream.pWifiDataCallbackParam = NULL;
            free(gEdmStream.pUartBuffer);
            gEdmStream.pUartBuffer = NULL;
            free(gEdmStream.pAtCommandBuffer);
            gEdmStream.pAtCommandBuffer = NULL;
            free(gEdmStream.pAtResponseBuffer);
            gEdmStream.pAtResponseBuffer = NULL;
            for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
                gEdmStream.connections[i].channel = -1;
                gEdmStream.connections[i].frameSize = -1;
                gEdmStream.connections[i].type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

//lint -esym(593, pParam) Suppress pParam not being freed here
int32_t uShortRangeEdmStreamAtCallbackSet(int32_t handle,
                                          void (*pFunction)(int32_t, uint32_t,
                                                            void *),
                                          void *pParam,
                                          size_t stackSizeBytes,
                                          int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == gEdmStream.handle) &&
            (gEdmStream.atEventQueueHandle < 0) &&
            (pFunction != NULL)) {
            // Open an event queue to eventHandler()
            // useful name for debug purposes
            int32_t result = uPortEventQueueOpen(atEventHandler, "eventEdmAT",
                                                 sizeof(int32_t),
                                                 stackSizeBytes,
                                                 priority,
                                                 U_EDM_STREAM_AT_EVENT_QUEUE_SIZE);
            if (result >= 0) {
                gEdmStream.atEventQueueHandle = result;
                gEdmStream.pAtCallback = pFunction;
                gEdmStream.pAtCallbackParam = pParam;

                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

//lint -esym(593, pParam) Suppress pParam not being freed here
int32_t uShortRangeEdmStreamBtEventCallbackSet(int32_t handle,
                                               void (*pFunction)(int32_t, uint32_t, uint32_t,
                                                                 bool, int32_t, char *, void *),
                                               void *pParam,
                                               size_t stackSizeBytes,
                                               int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            if ((gEdmStream.btEventQueueHandle < 0) && (pFunction != NULL)) {
                // Open an event queue to eventHandler()
                int32_t result = uPortEventQueueOpen(btEventHandler, "eventEdmBT",
                                                     sizeof(uShortRangeEdmStreamBtEvent_t),
                                                     stackSizeBytes,
                                                     priority,
                                                     U_EDM_STREAM_BT_EVENT_QUEUE_SIZE);
                if (result >= 0) {
                    gEdmStream.btEventQueueHandle = result;
                    gEdmStream.pBtEventCallback = pFunction;
                    gEdmStream.pBtEventCallbackParam = pParam;

                    errorCode = U_ERROR_COMMON_SUCCESS;
                }
            } else if ((gEdmStream.btEventQueueHandle >= 0) && (pFunction == NULL)) {
                uPortEventQueueClose(gEdmStream.btEventQueueHandle);
                gEdmStream.btEventQueueHandle = -1;
                gEdmStream.pBtEventCallback = NULL;
                gEdmStream.pBtEventCallbackParam = NULL;
            }

        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

int32_t uShortRangeEdmStreamDataEventCallbackSet(int32_t handle,
                                                 int32_t type,
                                                 void (*pFunction)(int32_t, int32_t, int32_t,
                                                                   char *, void *),
                                                 void *pParam,
                                                 size_t stackSizeBytes,
                                                 int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == gEdmStream.handle) &&
            (type >= (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) &&
            (type <= (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI)) {
            if (pFunction != NULL) {
                if ((type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT &&
                     gEdmStream.pBtDataCallback == NULL) ||
                    (type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI &&
                     gEdmStream.pWifiDataCallback == NULL)) {

                    int32_t result = 0;
                    if (gEdmStream.pBtDataCallback == NULL && gEdmStream.pWifiDataCallback == NULL) {
                        // Open an event queue to eventHandler()
                        result = uPortEventQueueOpen(dataEventHandler, "eventEdmData",
                                                     sizeof(uShortRangeEdmStreamBtEvent_t),
                                                     stackSizeBytes,
                                                     priority,
                                                     U_EDM_STREAM_DATA_EVENT_QUEUE_SIZE);
                        if (result >= 0) {
                            gEdmStream.dataEventQueueHandle = result;
                        }
                    }

                    if (result >= 0) {
                        if (type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) {
                            gEdmStream.pBtDataCallback = pFunction;
                            gEdmStream.pBtDataCallbackParam = pParam;
                        } else {
                            gEdmStream.pWifiDataCallback = pFunction;
                            gEdmStream.pWifiDataCallbackParam = pParam;
                        }

                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                }
            } else {
                if ((type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT &&
                     gEdmStream.pBtDataCallback != NULL) ||
                    (type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI &&
                     gEdmStream.pWifiDataCallback != NULL)) {

                    if (type == (int32_t) U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) {
                        gEdmStream.pBtDataCallback = NULL;
                        gEdmStream.pBtDataCallbackParam = NULL;
                    } else {
                        gEdmStream.pWifiDataCallback = NULL;
                        gEdmStream.pWifiDataCallbackParam = NULL;
                    }

                    if (gEdmStream.pBtDataCallback == NULL && gEdmStream.pWifiDataCallback == NULL) {
                        uPortEventQueueClose(gEdmStream.dataEventQueueHandle);
                        gEdmStream.dataEventQueueHandle = -1;
                    }
                    errorCode = U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

void uShortRangeEdmStreamSetAtHandle(int32_t handle, void *atHandle)
{
    if (handle == gEdmStream.handle) {
        uAtClientStreamInterceptTx(atHandle, pInterceptTx, NULL);
        gEdmStream.atHandle = atHandle;
    }
}

int32_t uShortRangeEdmStreamAtWrite(int32_t handle, const void *pBuffer,
                                    size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        if (gEdmStream.handle == handle && pBuffer != NULL && sizeBytes != 0) {
            sizeOrErrorCode = (int32_t)U_ERROR_COMMON_PLATFORM;

            int32_t result;
            uint32_t sent = 0;

            do {
                result = uartWrite(pBuffer, sizeBytes);
                if (result > 0) {
                    sent += result;
                }
            } while (result > 0 && sent < sizeBytes);

            if (sent > 0) {
                sizeOrErrorCode = (int32_t)sent;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

int32_t uShortRangeEdmStreamAtRead(int32_t handle, void *pBuffer,
                                   size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        if (gEdmStream.handle == handle && pBuffer != NULL && sizeBytes != 0) {
            sizeOrErrorCode = (int32_t)(gEdmStream.atResponseLength - gEdmStream.atResponseRead);
            if (sizeOrErrorCode > 0) {
                if (sizeBytes < (uint32_t)sizeOrErrorCode) {
                    sizeOrErrorCode = (int32_t)sizeBytes;
                }
                memcpy(pBuffer, gEdmStream.pAtResponseBuffer + gEdmStream.atResponseRead, sizeOrErrorCode);
                gEdmStream.atResponseRead += sizeOrErrorCode;

                if (gEdmStream.atResponseRead >= gEdmStream.atResponseLength) {
                    gEdmStream.atResponseLength = 0;
                    gEdmStream.atResponseRead = 0;
                }
                gEdmStream.uartBufferAvailable = true;
                uPortUartEventSend(gEdmStream.uartHandle,
                                   U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

int32_t uShortRangeEdmStreamWrite(int32_t handle, int32_t channel,
                                  const void *pBuffer, size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        if (gEdmStream.handle == handle && channel >= 0 &&
            pBuffer != NULL && sizeBytes != 0) {
            uShortRangeEdmStreamConnections_t *pConnection = findConnection(channel);
            if (pConnection != NULL) {
                int32_t sent;
                int32_t send;
                char head[U_SHORT_RANGE_EDM_DATA_HEAD_SIZE];
                char tail[U_SHORT_RANGE_EDM_TAIL_SIZE];
                sizeOrErrorCode = 0;

                do {
                    if (((int32_t)sizeBytes - sizeOrErrorCode) > pConnection->frameSize) {
                        send = pConnection->frameSize;
                    } else {
                        send = ((int32_t)sizeBytes - sizeOrErrorCode);
                    }

                    (void)uShortRangeEdmZeroCopyHeadData((uint8_t)channel, send, (char *)&head[0]);
                    sent = uartWrite((void *)&head[0], U_SHORT_RANGE_EDM_DATA_HEAD_SIZE);
                    sent += uartWrite((const void *)((const char *)pBuffer + sizeOrErrorCode), send);
                    (void)uShortRangeEdmZeroCopyTail((char *)&tail[0]);
                    sent += uartWrite((void *)&tail[0], U_SHORT_RANGE_EDM_TAIL_SIZE);

                    if (sent != (send + U_SHORT_RANGE_EDM_DATA_HEAD_SIZE + U_SHORT_RANGE_EDM_TAIL_SIZE)) {
                        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_DEVICE_ERROR;
                        break;
                    } else {
                        sizeOrErrorCode += send;
                    }
                } while ((int32_t)sizeBytes > sizeOrErrorCode);
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

int32_t uShortRangeEdmStreamAtEventSend(int32_t handle, uint32_t eventBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == 0) &&
            (gEdmStream.atEventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            errorCode = uPortEventQueueSend(gEdmStream.atEventQueueHandle,
                                            &gEdmStream.handle, sizeof(int32_t));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

bool uShortRangeEdmStreamAtEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle == gEdmStream.handle) &&
            (gEdmStream.atEventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(gEdmStream.atEventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

void uShortRangeEdmStreamAtCallbackRemove(int32_t handle)
{
    int32_t eventQueueHandle = -1;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle == gEdmStream.handle) &&
            (gEdmStream.atEventQueueHandle >= 0)) {
            eventQueueHandle = gEdmStream.atEventQueueHandle;
            gEdmStream.atEventQueueHandle = -1;
            gEdmStream.pAtCallback = NULL;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        if (eventQueueHandle >= 0) {
            uPortEventQueueClose(eventQueueHandle);
        }
    }
}


int32_t uPortShortRangeEdmStremAtEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == gEdmStream.handle) &&
            (gEdmStream.atEventQueueHandle >= 0)) {
            sizeOrErrorCode = uPortEventQueueStackMinFree(gEdmStream.atEventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

int32_t uShortRangeEdmStreamAtGetReceiveSize(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            sizeOrErrorCode = gEdmStream.atResponseLength - gEdmStream.atResponseRead;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// End of file
