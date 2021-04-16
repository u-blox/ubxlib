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
#include "u_port.h"
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
#define U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH  200
// TODO: is this value correc?
#define U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH 500
#define U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS    9

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

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
    void (*pBtEventCallback)(int32_t, uint32_t, uint32_t, bool, int32_t, uint8_t *, void *);
    void *pBtEventCallbackParam;
    //void (*pWifiEventCallback)(int32_t, uint32_t, void *);
    //void *pWifiEventCallbackParam;
    void (*pBtDataCallback)(int32_t, int32_t, int32_t, char *, void *);
    void *pBtDataCallbackParam;
    void (*pWifiDataCallback)(int32_t, int32_t, int32_t, char *, void *);
    void *pWifiDataCallbackParam;
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

static void processedEvent(void)
{
    uShortRangeEdmResetParser();
    // Trigger an event from the uart to get parsing going again
    uPortUartEventSend(gEdmStream.uartHandle,
                       U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
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
    // This event is not fully processed until uShortRangeEdmStreamAtRead has been called
    // and all event data been read out
}

static void btEventHandler(void *pParam, size_t paramLength)
{
    uShortRangeEdmStreamBtEvent_t *pBtEvent = (uShortRangeEdmStreamBtEvent_t *) pParam;

    (void) paramLength;

    if (pBtEvent != NULL) {
        if (gEdmStream.pBtEventCallback != NULL) {
            gEdmStream.pBtEventCallback(gEdmStream.handle, (uint32_t) pBtEvent->type, pBtEvent->channel,
                                        pBtEvent->ble, (int32_t) pBtEvent->frameSize, pBtEvent->address,
                                        gEdmStream.pBtEventCallbackParam);
        }
    }
    processedEvent();
}

static void dataEventHandler(void *pParam, size_t paramLength)
{
    uShortRangeEdmStreamDataEvent_t *pDataEvent = (uShortRangeEdmStreamDataEvent_t *) pParam;

    (void) paramLength;


    if (pDataEvent != NULL) {
        uShortRangeEdmStreamConnections_t *pConnection;

        U_PORT_MUTEX_LOCK(gMutex);
        pConnection = findConnection(pDataEvent->channel);

        if (pConnection != NULL) {
            switch (pConnection->type) {

                case U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT:
                    if (gEdmStream.pBtDataCallback != NULL) {
                        gEdmStream.pBtDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                                   pDataEvent->pData, gEdmStream.pBtDataCallbackParam);
                    }
                    break;

                case U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI:
                    if (gEdmStream.pWifiDataCallback != NULL) {
                        gEdmStream.pWifiDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                                     pDataEvent->pData, gEdmStream.pWifiDataCallbackParam);
                    }
                    break;

                case U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID:
                default:
                    break;
            }
        }

        processedEvent();
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

static void processEdmAtEvent(uShortRangeEdmEvent_t *pEvent)
{
    gEdmStream.atResponseLength = pEvent->params.atEvent.length;
    gEdmStream.atResponseRead = 0;
    memcpy(gEdmStream.pAtResponseBuffer, pEvent->params.atEvent.pData, gEdmStream.atResponseLength);

    uPortEventQueueSend(gEdmStream.atEventQueueHandle,
                        &gEdmStream.handle, sizeof(int32_t));
}

static void processEdmConnectBtEvent(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamConnections_t *pConnection =
        findConnection(pEvent->params.btConnectEvent.channel);

    if (pConnection == NULL) {
        pConnection = findConnection(-1);
    }
    if (pConnection != NULL) {
        uShortRangeEdmStreamBtEvent_t event;
        pConnection->channel = pEvent->params.btConnectEvent.channel;
        pConnection->type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT;
        pConnection->frameSize = pEvent->params.btConnectEvent.framesize;

        event.type = U_SHORT_RANGE_EDM_STREAM_CONNECTED;
        event.ble = (pEvent->params.btConnectEvent.profile == U_SHORT_RANGE_EDM_BT_PROFILE_SPS);
        event.channel = pEvent->params.btConnectEvent.channel;
        memcpy(event.address, pEvent->params.btConnectEvent.address, U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH);
        event.frameSize = pEvent->params.btConnectEvent.framesize;

        uPortEventQueueSend(gEdmStream.btEventQueueHandle,
                            &event, sizeof(uShortRangeEdmStreamBtEvent_t));
    }
}

static void processEdmDisconnectEvent(uShortRangeEdmEvent_t *pEvent)
{
    int32_t channel = (int32_t)pEvent->params.disconnectEvent.channel;
    uShortRangeEdmStreamConnections_t *pConnection = findConnection(channel);

    if (pConnection != NULL && pConnection->type == U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT) {
        uShortRangeEdmStreamBtEvent_t event;

        pConnection->channel = -1;
        pConnection->type = U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID;
        pConnection->frameSize = -1;
        event.type = U_SHORT_RANGE_EDM_STREAM_DISCONNECTED;
        event.channel = channel;

        uPortEventQueueSend(gEdmStream.btEventQueueHandle,
                            &event, sizeof(uShortRangeEdmStreamBtEvent_t));
    }
}

static void processEdmDataEvent(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamDataEvent_t event;
    event.channel = pEvent->params.dataEvent.channel;
    event.pData = pEvent->params.dataEvent.pData;
    event.length = pEvent->params.dataEvent.length;

    uPortEventQueueSend(gEdmStream.dataEventQueueHandle,
                        &event, sizeof(uShortRangeEdmStreamDataEvent_t));
}

static void processEdmEvent(uShortRangeEdmEvent_t *pEvent)
{
    switch (pEvent->type) {

        case U_SHORT_RANGE_EDM_EVENT_AT:
            processEdmAtEvent(pEvent);
            break;

        case U_SHORT_RANGE_EDM_EVENT_CONNECT_BT:
            processEdmConnectBtEvent(pEvent);
            break;

        case U_SHORT_RANGE_EDM_EVENT_DISCONNECT:
            processEdmDisconnectEvent(pEvent);
            break;

        case U_SHORT_RANGE_EDM_EVENT_DATA:
            processEdmDataEvent(pEvent);
            break;

        case U_SHORT_RANGE_EDM_EVENT_STARTUP:
            processedEvent();
            break;

        case U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4:
        case U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6:
        case U_SHORT_RANGE_EDM_EVENT_INVALID:
            break;

        default:
            break;
    }
}

static void uartCallback(int32_t uartHandle, uint32_t eventBitmask,
                         void *pParameters)
{
    (void)pParameters;
    if (gEdmStream.uartHandle == uartHandle &&
        eventBitmask == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        bool uartEmpty = false;
        // We don't want to read one character at the time from the uart driver since that will be
        // quite an overhead when pumping a lot of data. Instead we read into a buffer
        // and then comsume characters from that. But we might not consume all read characters
        // before an EDM-event is generated by the parser which makes the parser unavailable
        // and we have to leave this callback. When the parser later is available this
        // uart-event will be placed on the queue again so that we come back here.
        // We thus need a static buffer, and if there are unparsed characters left in it we move
        // them to the beginning of the buffer befor leaving (instead of using a ring buffer).
        U_PORT_MUTEX_LOCK(gMutex);
        while (!uartEmpty && uShortRangeEdmParserReady()) {
            // Loop until we couldn't read any more characters from uart
            // or EDM parser is unavailable
            static char buffer[128];
            static size_t charsInBuffer = 0;
            size_t consumed = 0;

            // Check if there are any existing characters in the buffer and parse them
            while (uShortRangeEdmParserReady() && (consumed < charsInBuffer)) {
                //lint -esym(727, buffer)
                uShortRangeEdmEvent_t *pEvent = uShortRangeEdmParse(buffer[consumed++]);
                if (pEvent != NULL) {
                    processEdmEvent(pEvent);
                }
            }
            // Move unparsed data to beginning of buffer
            if ((consumed > 0) && (charsInBuffer - consumed) > 0) {
                memmove(buffer, buffer + consumed, charsInBuffer - consumed);
            }
            charsInBuffer -= consumed;

            // Read as much as possible from uart into rest of buffer
            if (charsInBuffer < sizeof buffer) {
                int32_t sizeOrError = uPortUartRead(gEdmStream.uartHandle, buffer + charsInBuffer,
                                                    sizeof buffer - charsInBuffer);
                if (sizeOrError > 0) {
                    charsInBuffer += sizeOrError;
                } else {
                    uartEmpty = true;
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
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
        free(pPacket);
    }

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

    uShortRangeEdmResetParser();

    return (int32_t) errorCodeOrHandle;
}

void uShortRangeEdmStreamDeinit()
{
    uShortRangeEdmResetParser();

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

        if (uartHandle >= 0 && gEdmStream.handle == -1) {

            int32_t errorCode = uPortUartEventCallbackSet(uartHandle,
                                                          U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                          uartCallback, NULL,
                                                          U_EDM_STREAM_TASK_STACK_SIZE_BYTES,
                                                          U_EDM_STREAM_TASK_PRIORITY);

            if (errorCode == 0) {
                gEdmStream.pAtCommandBuffer = (char *)malloc(U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH);
                memset(gEdmStream.pAtCommandBuffer, 0, U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH);
                gEdmStream.pAtResponseBuffer = (char *)malloc(U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH);
                memset(gEdmStream.pAtResponseBuffer, 0, U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH);
                if (gEdmStream.pAtCommandBuffer == NULL ||
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
        uShortRangeEdmResetParser();
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

        uShortRangeEdmResetParser();
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
                                                                 bool, int32_t, uint8_t *, void *),
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
                    processedEvent();
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

int32_t uShortRangeEdmStreamWrite(int32_t handle, int32_t channel,
                                  const void *pBuffer, size_t sizeBytes,
                                  uint32_t timeoutMs)
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
                int64_t startTime = uPortGetTickTimeMs();
                int64_t endTime;

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
                    endTime = uPortGetTickTimeMs();
                } while (((int32_t)sizeBytes > sizeOrErrorCode) &&
                         (endTime - startTime < timeoutMs));
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
