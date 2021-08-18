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

//lint -efile(766, ctype.h)
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"

//lint -efile(766, u_port_debug.h)
#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#include "u_port_uart.h"
#include "u_port_debug.h"
#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"
#include "u_short_range_edm.h"

#include "string.h" // For memcpy()

// To enable anonymous unions inclusion for
// ARM compiler
#ifdef __arm__
// Stop GCC complaining
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma anon_unions
#pragma GCC diagnostic pop
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define U_SHORT_RANGE_EDM_STREAM_AT_COMMAND_LENGTH  200
// TODO: is this value correc?
#define U_SHORT_RANGE_EDM_STREAM_AT_RESPONSE_LENGTH 500
#define U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS    9

// Debug logging for EDM activity
// You can activate debug log output for EDM activity with the defines below
//
// U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG:
// Define to enable EDM debug log
//
// U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR:
// Define to enable ANSI color for EDM debug log
//
// U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA:
// Define to dump EDM RX/TX data
//
//lint -esym(750, uEdmChLogStart) Suppress not reference
//lint -esym(750, uEdmChLogEnd) Suppress not reference
//lint -esym(750, uEdmChLogLine) Suppress not reference
#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
# ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR
#  define ANSI_BLU "\e[0;34m"
#  define ANSI_CYN "\e[0;36m"
#  define ANSI_GRN "\e[0;32m"
#  define ANSI_MAG "\e[0;35m"
#  define ANSI_YEL "\e[0;33m"
#  define ANSI_RST "\e[0m"
# else
#  define ANSI_BLU
#  define ANSI_CYN
#  define ANSI_GRN
#  define ANSI_MAG
#  define ANSI_YEL
#  define ANSI_RST
# endif
# define LOG_CH_AT_TX ANSI_CYN "[EDM AT TX]"
# define LOG_CH_AT_RX ANSI_MAG "[EDM AT RX]"
# define LOG_CH_IP    ANSI_YEL "[EDM IP   ]"
# define LOG_CH_BT    ANSI_BLU "[EDM BT   ]"
# define LOG_CH_DATA  ANSI_GRN "[EDM DATA ]"
# define uEdmChLogStart(log_ch, format, ...)    uPortLog(log_ch " " format, ##__VA_ARGS__)
# define uEdmChLogEnd(format, ...)              uPortLog(format ANSI_RST "\n", ##__VA_ARGS__)
# define uEdmChLogLine(log_ch, format, ...)     uEdmChLogStart(log_ch, format, ##__VA_ARGS__); uEdmChLogEnd("");
#else /* U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG */
# undef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA
# undef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR
# define uEdmChLogStart(log_ch, format, ...)
# define uEdmChLogEnd()
# define uEdmChLogLine(log_ch, format, ...)
#endif
//lint -restore

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef enum {
    U_SHORT_RANGE_EDM_STREAM_EVENT_AT,
    U_SHORT_RANGE_EDM_STREAM_EVENT_BT,
    U_SHORT_RANGE_EDM_STREAM_EVENT_IP,
    U_SHORT_RANGE_EDM_STREAM_EVENT_MQTT,
    U_SHORT_RANGE_EDM_STREAM_EVENT_DATA
} uShortRangeEdmStreamEventType_t;

typedef struct {
    uint8_t channel;
    uShortRangeConnectionEventType_t type;
    uShortRangeConnectDataBt_t conData;
} uShortRangeEdmStreamBtEvent_t;

typedef struct {
    uint8_t channel;
    uShortRangeConnectionEventType_t type;
    uShortRangeConnectDataIp_t conData;
} uShortRangeEdmStreamIpEvent_t;

typedef struct {
    int32_t channel;
    char *pData;
    int32_t length;
} uShortRangeEdmStreamDataEvent_t;

typedef struct {
    uShortRangeEdmStreamEventType_t type;
    union {
        // no content in at event       at;
        uShortRangeEdmStreamBtEvent_t   bt;
        uShortRangeEdmStreamIpEvent_t   ip;
        uShortRangeEdmStreamIpEvent_t   mqtt;
        uShortRangeEdmStreamDataEvent_t data;
    };
} uShortRangeEdmStreamEvent_t;

typedef struct {
    int32_t frameSize;
} uBtConnectionParams_t;

typedef struct {
    uint16_t localPort;
    uShortRangeIpProtocol_t protocol;
} uIpConnectionParams_t;

typedef struct {
    int32_t channel;
    uShortRangeConnectionType_t type;
    union {
        uBtConnectionParams_t bt;
        uIpConnectionParams_t ip;
    };
} uShortRangeEdmStreamConnections_t;

typedef struct uEdmStreamInstance_t {
    int32_t handle;
    int32_t uartHandle;
    void *atHandle;
    int32_t eventQueueHandle;
    uEdmAtEventCallback_t pAtCallback;
    void *pAtCallbackParam;
    uEdmBtConnectionStatusCallback_t pBtEventCallback;
    void *pBtEventCallbackParam;
    uEdmIpConnectionStatusCallback_t pIpEventCallback;
    void *pIpEventCallbackParam;
    uEdmIpConnectionStatusCallback_t pMqttEventCallback;
    void *pMqttEventCallbackParam;
    uEdmDataEventCallback_t pBtDataCallback;
    void *pBtDataCallbackParam;
    uEdmDataEventCallback_t pIpDataCallback;
    void *pIpDataCallbackParam;
    uEdmDataEventCallback_t pMqttDataCallback;
    void *pMqttDataCallbackParam;
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

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
static inline void dumpAtData(const char *pBuffer, size_t length)
{
    for (int i = 0; i < length; i++) {
        char ch = pBuffer[i];
        if (isprint(ch)) {
            uPortLog("%c", ch);
        } else if (ch == '\r') {
            uPortLog("\\r", ch);
        } else if (ch == '\n') {
            uPortLog("\\n", ch);
        } else {
            uPortLog("\\x%02x", ch);
        }
    }
}

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA
static inline void dumpHexData(const uint8_t *pBuffer, size_t length)
{
    for (int i = 0; i < length; i++) {
        uPortLog("%02x ", pBuffer[i]);
    }
}
#endif

static inline void dumpBdAddr(const uint8_t *pBdAddr)
{
    for (int i = 0; i < U_SHORT_RANGE_BT_ADDRESS_LENGTH; i++) {
        uPortLog("%02x%s", pBdAddr[i], (i < U_SHORT_RANGE_BT_ADDRESS_LENGTH - 1) ? ":" : "");
    }
}

static const char *getProtocolText(uShortRangeIpProtocol_t protocol)
{
    switch (protocol) {
        case U_SHORT_RANGE_IP_PROTOCOL_TCP:
            return "TCP";
        case U_SHORT_RANGE_IP_PROTOCOL_UDP:
            return "UDP";
        case U_SHORT_RANGE_IP_PROTOCOL_MQTT:
            return "MQTT";
        default:
            break;
    }
    return "Unknown protocol";
}

#endif

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

static void atEventHandler(void)
{
    if (gEdmStream.pAtCallback != NULL) {
        gEdmStream.pAtCallback(gEdmStream.handle,
                               U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                               gEdmStream.pAtCallbackParam);
    }
    // This event is not fully processed until uShortRangeEdmStreamAtRead has been called
    // and all event data been read out
}

// Event handler, calls the user's event callback.
static void btEventHandler(uShortRangeEdmStreamBtEvent_t *pBtEvent)
{
    if (gEdmStream.pBtEventCallback != NULL) {
        gEdmStream.pBtEventCallback(gEdmStream.handle, pBtEvent->channel, pBtEvent->type,
                                    &pBtEvent->conData, gEdmStream.pBtEventCallbackParam);
    }
    uEdmChLogLine(LOG_CH_BT, "processed");
    processedEvent();
}

// Event handler, calls the user's event callback.
static void ipEventHandler(uShortRangeEdmStreamIpEvent_t *pIpEvent)
{
    if (gEdmStream.pIpEventCallback != NULL) {
        gEdmStream.pIpEventCallback(gEdmStream.handle, pIpEvent->channel, pIpEvent->type,
                                    &pIpEvent->conData, gEdmStream.pIpEventCallbackParam);
    }

    uEdmChLogLine(LOG_CH_IP, "processed");
    processedEvent();
}

// Event handler, calls the user's event callback.
static void mqttEventHandler(uShortRangeEdmStreamIpEvent_t *pMqttEvent)
{
    if (gEdmStream.pMqttEventCallback != NULL) {
        gEdmStream.pMqttEventCallback(gEdmStream.handle, pMqttEvent->channel, pMqttEvent->type,
                                      &pMqttEvent->conData, gEdmStream.pMqttEventCallbackParam);
    }
    uEdmChLogLine(LOG_CH_IP, "processed");
    processedEvent();
}

static void dataEventHandler(uShortRangeEdmStreamDataEvent_t *pDataEvent)
{
    uShortRangeEdmStreamConnections_t *pConnection;

    U_PORT_MUTEX_LOCK(gMutex);
    pConnection = findConnection(pDataEvent->channel);

    if (pConnection != NULL) {
        switch (pConnection->type) {

            case U_SHORT_RANGE_CONNECTION_TYPE_BT:
                if (gEdmStream.pBtDataCallback != NULL) {
                    gEdmStream.pBtDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                               pDataEvent->pData, gEdmStream.pBtDataCallbackParam);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_IP:
                if (gEdmStream.pIpDataCallback != NULL) {
                    gEdmStream.pIpDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                               pDataEvent->pData, gEdmStream.pIpDataCallbackParam);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_MQTT:
                if (gEdmStream.pMqttDataCallback != NULL) {
                    gEdmStream.pMqttDataCallback(gEdmStream.handle, pDataEvent->channel, pDataEvent->length,
                                                 pDataEvent->pData, gEdmStream.pMqttDataCallbackParam);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_INVALID:
            default:
                break;
        }
    }

    uEdmChLogLine(LOG_CH_DATA, "processed");
    processedEvent();
    U_PORT_MUTEX_UNLOCK(gMutex);
}

static void eventHandler(void *pParam, size_t paramLength)
{
    uShortRangeEdmStreamEvent_t *pEvent = (uShortRangeEdmStreamEvent_t *)pParam;
    (void)paramLength;

    if (pEvent == NULL) {
        return;
    }

    switch (pEvent->type) {

        case U_SHORT_RANGE_EDM_STREAM_EVENT_AT:
            atEventHandler();
            break;

        case U_SHORT_RANGE_EDM_STREAM_EVENT_BT:
            btEventHandler(&(pEvent->bt));
            break;

        case U_SHORT_RANGE_EDM_STREAM_EVENT_IP:
            ipEventHandler(&(pEvent->ip));
            break;

        case U_SHORT_RANGE_EDM_STREAM_EVENT_MQTT:
            mqttEventHandler(&(pEvent->mqtt));
            break;

        case U_SHORT_RANGE_EDM_STREAM_EVENT_DATA:
            dataEventHandler(&(pEvent->data));
            break;

        default:
            break;
    }
}

static void processEdmAtEvent(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamEvent_t event;

    gEdmStream.atResponseLength = pEvent->params.atEvent.length;
    gEdmStream.atResponseRead = 0;
    memcpy(gEdmStream.pAtResponseBuffer, pEvent->params.atEvent.pData, gEdmStream.atResponseLength);

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
    uEdmChLogStart(LOG_CH_AT_RX, "\"");
    dumpAtData(gEdmStream.pAtResponseBuffer, gEdmStream.atResponseLength);
    uEdmChLogEnd("\"");
#endif

    event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_AT;
    uPortEventQueueSend(gEdmStream.eventQueueHandle,
                        &event, sizeof(uShortRangeEdmStreamEvent_t));
}

static void processEdmConnectBtEvent(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamConnections_t *pConnection =
        findConnection(pEvent->params.btConnectEvent.channel);

    if (pConnection == NULL) {
        pConnection = findConnection(-1);
    }
    if (pConnection != NULL) {
        uShortRangeEdmStreamEvent_t event;
        pConnection->channel = pEvent->params.btConnectEvent.channel;
        pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_BT;
        pConnection->bt.frameSize = pEvent->params.btConnectEvent.connection.framesize;

        event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_BT;
        event.bt.type = U_SHORT_RANGE_EVENT_CONNECTED;
        event.bt.channel = pEvent->params.btConnectEvent.channel;
        event.bt.conData = pEvent->params.btConnectEvent.connection;

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
        uEdmChLogStart(LOG_CH_BT, "Connected ");
        dumpBdAddr(event.bt.conData.address);
        uEdmChLogEnd("");
#endif

        uPortEventQueueSend(gEdmStream.eventQueueHandle,
                            &event, sizeof(uShortRangeEdmStreamEvent_t));
    }
}

static void processEdmConnectIpv4Event(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamConnections_t *pConnection =
        findConnection(pEvent->params.ipv4ConnectEvent.channel);

    if (pConnection == NULL) {
        pConnection = findConnection(-1);
    }
    if (pConnection != NULL) {
        uShortRangeEdmStreamEvent_t event;
        uShortRangeEdmConnectionEventIpv4_t *ipv4Evt = &pEvent->params.ipv4ConnectEvent;
        uShortRangeIpProtocol_t protocol = ipv4Evt->connection.protocol;
        // IPv4 events are generated by TCP, UDP and MQTT connections
        // Since MQTT and TCP/UDP have separate callbacks we need to
        // check whether the protocol is MQTT or TCP/UDP here
        if ((protocol == U_SHORT_RANGE_IP_PROTOCOL_TCP) ||
            (protocol == U_SHORT_RANGE_IP_PROTOCOL_UDP)) {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_IP;
            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_IP;
        } else if (protocol == U_SHORT_RANGE_IP_PROTOCOL_MQTT) {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_MQTT;
            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_MQTT;
        } else {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
        }

        if (pConnection->type != U_SHORT_RANGE_CONNECTION_TYPE_INVALID) {
            pConnection->channel = ipv4Evt->channel;
            pConnection->ip.localPort = ipv4Evt->connection.localPort;
            pConnection->ip.protocol = protocol;

            event.ip.type = U_SHORT_RANGE_EVENT_CONNECTED;
            event.ip.channel = ipv4Evt->channel;
            event.ip.conData.ipv4 = ipv4Evt->connection;

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
            const char *protocolTxt = getProtocolText(protocol);
            uint8_t *rIp = event.ip.conData.ipv4.remoteAddress;
            uint16_t rPort = event.ip.conData.ipv4.remotePort;
            uint8_t *lIp = event.ip.conData.ipv4.localAddress;
            uint16_t lPort = event.ip.conData.ipv4.localPort;
            uEdmChLogLine(LOG_CH_IP, "ch: %d, IPv4 %s connected %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d",
                          event.ip.channel,
                          protocolTxt,
                          lIp[0], lIp[1], lIp[2], lIp[3], lPort,
                          rIp[0], rIp[1], rIp[2], rIp[3], rPort);
#endif

            uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                &event, sizeof(uShortRangeEdmStreamEvent_t));
        }
    }
}

static void processEdmConnectIpv6Event(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamConnections_t *pConnection =
        findConnection(pEvent->params.ipv6ConnectEvent.channel);

    if (pConnection == NULL) {
        pConnection = findConnection(-1);
    }
    if (pConnection != NULL) {
        uShortRangeEdmStreamEvent_t event;
        uShortRangeEdmConnectionEventIpv6_t *ipv6Evt = &pEvent->params.ipv6ConnectEvent;
        uShortRangeIpProtocol_t protocol = ipv6Evt->connection.protocol;
        // IPv4 events are generated by TCP, UDP and MQTT connections
        // Since MQTT and TCP/UDP have separate callbacks we need to
        // check whether the protocol is MQTT or TCP/UDP here
        if ((protocol == U_SHORT_RANGE_IP_PROTOCOL_TCP) ||
            (protocol == U_SHORT_RANGE_IP_PROTOCOL_UDP)) {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_IP;
            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_IP;
        } else if (protocol == U_SHORT_RANGE_IP_PROTOCOL_MQTT) {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_MQTT;
            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_MQTT;
        } else {
            pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
        }

        if (pConnection->type != U_SHORT_RANGE_CONNECTION_TYPE_INVALID) {
            pConnection->channel = ipv6Evt->channel;
            pConnection->ip.localPort = ipv6Evt->connection.localPort;
            pConnection->ip.protocol = protocol;

            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_IP;
            event.ip.type = U_SHORT_RANGE_EVENT_CONNECTED;
            event.ip.conData.ipv6 = ipv6Evt->connection;

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
            const char *protocolTxt = getProtocolText(protocol);
            uint16_t rPort = event.ip.conData.ipv6.remotePort;
            uint16_t lPort = event.ip.conData.ipv6.localPort;
            uEdmChLogLine(LOG_CH_IP, "ch %d, IPv6 %s connected port %d -> %d",
                          event.ip.channel, protocolTxt, lPort, rPort);
#endif

            uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                &event, sizeof(uShortRangeEdmStreamEvent_t));
        }
    }
}

static void processEdmDisconnectEvent(uShortRangeEdmEvent_t *pEvent)
{
    uint8_t channel = pEvent->params.disconnectEvent.channel;
    uShortRangeEdmStreamConnections_t *pConnection = findConnection(channel);

    if (pConnection != NULL) {
        uShortRangeEdmStreamEvent_t event;
        switch (pConnection->type) {
            case U_SHORT_RANGE_CONNECTION_TYPE_BT:
                event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_BT;
                event.bt.type = U_SHORT_RANGE_EVENT_DISCONNECTED;
                event.bt.channel = channel;
#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
                uEdmChLogLine(LOG_CH_BT, "ch: %d, disconnect", channel);
#endif
                uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                    &event, sizeof(uShortRangeEdmStreamEvent_t));
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_MQTT:
                event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_MQTT;
                event.mqtt.type = U_SHORT_RANGE_EVENT_DISCONNECTED;
                event.mqtt.channel = channel;
#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
                uEdmChLogLine(LOG_CH_IP, "ch: %d, disconnect", channel);
#endif
                uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                    &event, sizeof(uShortRangeEdmStreamEvent_t));
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_IP:
                event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_IP;
                event.ip.type = U_SHORT_RANGE_EVENT_DISCONNECTED;
                event.ip.channel = channel;
#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
                uEdmChLogLine(LOG_CH_IP, "ch: %d, disconnect", channel);
#endif
                uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                    &event, sizeof(uShortRangeEdmStreamEvent_t));
                break;

            default:
                break;
        }
        pConnection->channel = -1;
        pConnection->type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
    }
}

static void processEdmDataEvent(uShortRangeEdmEvent_t *pEvent)
{
    uShortRangeEdmStreamEvent_t event;
    event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_DATA;
    event.data.channel = pEvent->params.dataEvent.channel;
    event.data.pData = pEvent->params.dataEvent.pData;
    event.data.length = pEvent->params.dataEvent.length;

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
# ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA
    uEdmChLogStart(LOG_CH_DATA, "RX (%d bytes): ", event.data.length);
    dumpHexData(event.data.pData, event.data.length);
    uEdmChLogEnd("");
# else
    uEdmChLogLine(LOG_CH_DATA, "RX (%d bytes)", event.data.length);
# endif
#endif

    uPortEventQueueSend(gEdmStream.eventQueueHandle,
                        &event, sizeof(uShortRangeEdmStreamEvent_t));
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
            processEdmConnectIpv4Event(pEvent);
            break;

        case U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6:
            processEdmConnectIpv6Event(pEvent);
            break;

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
#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
            uEdmChLogStart(LOG_CH_AT_TX, "\"");
            dumpAtData(pEdmStream->pAtCommandBuffer, pEdmStream->atCommandCurrent);
            uEdmChLogEnd("\"");
#endif
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

        if (gEdmStream.eventQueueHandle >= 0) {
            uPortEventQueueClose(gEdmStream.eventQueueHandle);
        }
        gEdmStream.eventQueueHandle = -1;

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
                    gEdmStream.eventQueueHandle
                        = uPortEventQueueOpen(eventHandler, "eventEdmStream",
                                              sizeof(uShortRangeEdmStreamEvent_t),
                                              U_EDM_STREAM_TASK_STACK_SIZE_BYTES,
                                              U_EDM_STREAM_TASK_PRIORITY,
                                              U_EDM_STREAM_EVENT_QUEUE_SIZE);
                    if (gEdmStream.eventQueueHandle < 0) {
                        gEdmStream.eventQueueHandle = -1;
                    }

                    gEdmStream.handle = 0;
                    gEdmStream.uartHandle = uartHandle;
                    gEdmStream.atHandle = NULL;
                    gEdmStream.pAtCallback = NULL;
                    gEdmStream.pAtCallbackParam = NULL;
                    gEdmStream.pBtEventCallback = NULL;
                    gEdmStream.pBtEventCallbackParam = NULL;
                    gEdmStream.pBtDataCallback = NULL;
                    gEdmStream.pBtDataCallbackParam = NULL;
                    gEdmStream.pIpEventCallback = NULL;
                    gEdmStream.pIpEventCallbackParam = NULL;
                    gEdmStream.pIpDataCallback = NULL;
                    gEdmStream.pIpDataCallbackParam = NULL;
                    gEdmStream.pMqttEventCallback = NULL;
                    gEdmStream.pMqttEventCallbackParam = NULL;
                    gEdmStream.pMqttDataCallback = NULL;
                    gEdmStream.pMqttDataCallbackParam = NULL;
                    gEdmStream.atCommandCurrent = 0;

                    for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
                        gEdmStream.connections[i].channel = -1;
                        gEdmStream.connections[i].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
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
            if (gEdmStream.eventQueueHandle >= 0) {
                uPortEventQueueClose(gEdmStream.eventQueueHandle);
            }
            gEdmStream.eventQueueHandle = -1;
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
            gEdmStream.pIpEventCallback = NULL;
            gEdmStream.pIpEventCallbackParam = NULL;
            gEdmStream.pIpDataCallback = NULL;
            gEdmStream.pIpDataCallbackParam = NULL;
            gEdmStream.pMqttEventCallback = NULL;
            gEdmStream.pMqttEventCallbackParam = NULL;
            gEdmStream.pMqttDataCallback = NULL;
            gEdmStream.pMqttDataCallbackParam = NULL;
            free(gEdmStream.pAtCommandBuffer);
            gEdmStream.pAtCommandBuffer = NULL;
            free(gEdmStream.pAtResponseBuffer);
            gEdmStream.pAtResponseBuffer = NULL;
            for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
                gEdmStream.connections[i].channel = -1;
                gEdmStream.connections[i].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
            }
        }

        uShortRangeEdmResetParser();
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

//lint -esym(593, pParam) Suppress pParam not being freed here
int32_t uShortRangeEdmStreamAtCallbackSet(int32_t handle,
                                          uEdmAtEventCallback_t pFunction,
                                          void *pParam)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == gEdmStream.handle) && (pFunction != NULL)) {
            gEdmStream.pAtCallback = pFunction;
            gEdmStream.pAtCallbackParam = pParam;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

int32_t uShortRangeEdmStreamIpEventCallbackSet(int32_t handle,
                                               uEdmIpConnectionStatusCallback_t pFunction,
                                               void *pParam)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            if (pFunction != NULL && gEdmStream.pIpEventCallback == NULL) {
                gEdmStream.pIpEventCallback = pFunction;
                gEdmStream.pIpEventCallbackParam = pParam;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (pFunction == NULL) {
                gEdmStream.pIpEventCallback = NULL;
                gEdmStream.pIpEventCallbackParam = NULL;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

void uShortRangeEdmStreamIpEventCallbackRemove(int32_t handle)
{
    uShortRangeEdmStreamIpEventCallbackSet(handle,
                                           (uEdmIpConnectionStatusCallback_t)NULL,
                                           NULL);
}

int32_t uShortRangeEdmStreamMqttEventCallbackSet(int32_t handle,
                                                 uEdmIpConnectionStatusCallback_t pFunction,
                                                 void *pParam)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            if (pFunction != NULL && gEdmStream.pMqttEventCallback == NULL) {
                gEdmStream.pMqttEventCallback = pFunction;
                gEdmStream.pMqttEventCallbackParam = pParam;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (pFunction == NULL) {
                gEdmStream.pMqttEventCallback = NULL;
                gEdmStream.pMqttEventCallbackParam = NULL;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

void uShortRangeEdmStreamMqttEventCallbackRemove(int32_t handle)
{
    uShortRangeEdmStreamMqttEventCallbackSet(handle,
                                             (uEdmIpConnectionStatusCallback_t)NULL,
                                             NULL);
}

//lint -esym(593, pParam) Suppress pParam not being freed here
int32_t uShortRangeEdmStreamBtEventCallbackSet(int32_t handle,
                                               uEdmBtConnectionStatusCallback_t pFunction,
                                               void *pParam)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            if (pFunction != NULL && gEdmStream.pBtEventCallback == NULL) {
                gEdmStream.pBtEventCallback = pFunction;
                gEdmStream.pBtEventCallbackParam = pParam;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (pFunction == NULL) {
                gEdmStream.pBtEventCallback = NULL;
                gEdmStream.pBtEventCallbackParam = NULL;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }

        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

void uShortRangeEdmStreamBtEventCallbackRemove(int32_t handle)
{
    uShortRangeEdmStreamBtEventCallbackSet(handle,
                                           (uEdmBtConnectionStatusCallback_t)NULL,
                                           NULL);
}

int32_t uShortRangeEdmStreamDataEventCallbackSet(int32_t handle,
                                                 uShortRangeConnectionType_t type,
                                                 uEdmDataEventCallback_t pFunction,
                                                 void *pParam)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if (handle == gEdmStream.handle) {
            switch (type) {

                case U_SHORT_RANGE_CONNECTION_TYPE_BT:
                    if (pFunction != NULL && gEdmStream.pBtDataCallback == NULL) {
                        gEdmStream.pBtDataCallback = pFunction;
                        gEdmStream.pBtDataCallbackParam = pParam;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    } else if (pFunction == NULL) {
                        gEdmStream.pBtDataCallback = NULL;
                        gEdmStream.pBtDataCallbackParam = NULL;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                    break;

                case U_SHORT_RANGE_CONNECTION_TYPE_IP:
                    if (pFunction != NULL && gEdmStream.pIpDataCallback == NULL) {
                        gEdmStream.pIpDataCallback = pFunction;
                        gEdmStream.pIpDataCallbackParam = pParam;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    } else if (pFunction == NULL) {
                        gEdmStream.pIpDataCallback = NULL;
                        gEdmStream.pIpDataCallbackParam = NULL;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                    break;

                case U_SHORT_RANGE_CONNECTION_TYPE_MQTT:
                    if (pFunction != NULL && gEdmStream.pMqttDataCallback == NULL) {
                        gEdmStream.pMqttDataCallback = pFunction;
                        gEdmStream.pMqttDataCallbackParam = pParam;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    } else if (pFunction == NULL) {
                        gEdmStream.pMqttDataCallback = NULL;
                        gEdmStream.pMqttDataCallbackParam = NULL;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                    break;

                default:
                    break;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t)errorCode;
}

void uShortRangeEdmStreamDataEventCallbackRemove(int32_t handle,
                                                 uShortRangeConnectionType_t type)
{
    uShortRangeEdmStreamDataEventCallbackSet(handle,
                                             type,
                                             (uEdmDataEventCallback_t)NULL,
                                             NULL);
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
                    uEdmChLogLine(LOG_CH_AT_RX, "processed");
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
                    send = ((int32_t)sizeBytes - sizeOrErrorCode);
                    if (pConnection->type == U_SHORT_RANGE_CONNECTION_TYPE_BT) {
                        if (((int32_t)sizeBytes - sizeOrErrorCode) > pConnection->bt.frameSize) {
                            send = pConnection->bt.frameSize;
                        }
                    }

#ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
# ifdef U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA
                    uEdmChLogStart(LOG_CH_DATA, "TX (%d bytes): ", send);
                    dumpHexData(((const char *)pBuffer + sizeOrErrorCode), send);
                    uEdmChLogEnd("");
# else
                    uEdmChLogLine(LOG_CH_DATA, "TX (%d bytes)", send);
# endif
#endif

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
            (gEdmStream.eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            uShortRangeEdmStreamEvent_t event;
            event.type = U_SHORT_RANGE_EDM_STREAM_EVENT_AT;
            errorCode = uPortEventQueueSend(gEdmStream.eventQueueHandle,
                                            &event, sizeof(uShortRangeEdmStreamEvent_t));
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
            (gEdmStream.eventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(gEdmStream.eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

void uShortRangeEdmStreamAtCallbackRemove(int32_t handle)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if (handle == gEdmStream.handle) {
            gEdmStream.pAtCallback = NULL;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}


int32_t uPortShortRangeEdmStremAtEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle == gEdmStream.handle) &&
            (gEdmStream.eventQueueHandle >= 0)) {
            sizeOrErrorCode = uPortEventQueueStackMinFree(gEdmStream.eventQueueHandle);
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

int32_t uShortRangeEdmStreamFindIpConnection(int32_t handle,
                                             const uShortRangeConnectDataIp_t *pIpConnection)
{
    // IMPORTANT NOTE:
    // Currently this function only search for local port + protocol.
    // This is fine as long as we only support outgoing sockets.
    // The reason for this limitation is to save RAM.
    // If we would store all IP data for each connection that would
    // consume >400 bytes for a table of 10 entries due to IPv6.
    // When server sockets are added this needs to be addressed.
    // One solution would be to use a connection hash value.

    uShortRangeEdmStreamConnections_t *pConnection = NULL;

    if ((handle != -1) && (handle == gEdmStream.handle)) {
        uint16_t localPort;
        uShortRangeIpProtocol_t protocol;

        switch (pIpConnection->type) {
            case U_SHORT_RANGE_CONNECTION_IPv6:
                localPort = pIpConnection->ipv6.localPort;
                protocol =  pIpConnection->ipv6.protocol;
                break;
            case U_SHORT_RANGE_CONNECTION_IPv4:
            default:
                localPort = pIpConnection->ipv4.localPort;
                protocol =  pIpConnection->ipv4.protocol;
                break;
        }

        U_PORT_MUTEX_LOCK(gMutex);

        for (uint32_t i = 0; i < U_SHORT_RANGE_EDM_STREAM_MAX_CONNECTIONS; i++) {
            uShortRangeEdmStreamConnections_t *pIter = &gEdmStream.connections[i];
            if ((pIter->type == U_SHORT_RANGE_CONNECTION_TYPE_IP) ||
                (pIter->type == U_SHORT_RANGE_CONNECTION_TYPE_MQTT)) {
                if ((pIter->ip.localPort == localPort) && (pIter->ip.protocol == protocol)) {
                    pConnection = &gEdmStream.connections[i];
                    break;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return pConnection ? pConnection->channel : (int)U_ERROR_COMMON_UNKNOWN;
}

// End of file
