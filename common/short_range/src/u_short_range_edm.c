/*
 * Copyright 2020 u-blox
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
 * @brief Extended data mode implementation
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "u_error_common.h"
#include "u_short_range_module_type.h"
#include "u_at_client.h"
#include "u_short_range.h"
#include "u_short_range_edm.h"

//lint -e818 skip all "could be declared as const" warnings

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define U_SHORT_RANGE_EDM_HEAD                    ((char)0xAA)
#define U_SHORT_RANGE_EDM_TAIL                    ((char)0x55)

#define U_SHORT_RANGE_EDM_TYPE_CONNECT_EVENT      0x11
#define U_SHORT_RANGE_EDM_TYPE_DISCONNECT_EVENT   0x21
#define U_SHORT_RANGE_EDM_TYPE_DATA_EVENT         0x31
#define U_SHORT_RANGE_EDM_TYPE_DATA_COMMAND       0x36
#define U_SHORT_RANGE_EDM_TYPE_AT_REQUEST         0x44
#define U_SHORT_RANGE_EDM_TYPE_AT_RESPONSE        0x45
#define U_SHORT_RANGE_EDM_TYPE_AT_EVENT           0x41
//#define U_SHORT_RANGE_EDM_TYPE_RESEND_CONNECT     0x56
#define U_SHORT_RANGE_EDM_TYPE_START_EVENT        0x71

#define U_SHORT_RANGE_EDM_CONNECTION_TYPE_BT      0x01
#define U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv4    0x02
#define U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv6    0x03

#define U_SHORT_RANGE_EDM_SHORT_BUFFER_LENGTH     256

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef enum {
    EDM_PARSER_STATE_PARSE_START_BYTE,
    EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH,
    EDM_PARSER_STATE_ACCUMULATE_PAYLOAD,
    EDM_PARSER_STATE_PARSE_TAIL_BYTE,
    EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING
} edmParserState_t;

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */
static int32_t getBtProfile(char value, uShortRangeBtProfile_t *profile);
static int32_t getIpProtocol(char value, uShortRangeIpProtocol_t *protocol);
static uShortRangeEdmEvent_t *allocateEdmEvent(void);
static uShortRangeEdmEvent_t *parseConnectBtEvent(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectIpv4Event(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectIpv6Event(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectEvent(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseDisconnectEvent(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseDataEvent(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseAtResponseOrEvent(char *buffer, uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseEdmPayload(char *buffer, uint16_t payloadLength);
static void resetPayload(void);

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
static edmParserState_t gEdmParserState = EDM_PARSER_STATE_PARSE_START_BYTE;
static char gShortPayloadBuffer[U_SHORT_RANGE_EDM_SHORT_BUFFER_LENGTH];
static char *gpPayload = gShortPayloadBuffer;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static int32_t getBtProfile(char value, uShortRangeBtProfile_t *pProfile)
{
    switch (value) {
        case 0:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_SPP;
            break;
        case 1:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_DUN;
            break;
        case 14:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_SPS;
            break;
        default:
            return U_SHORT_RANGE_EDM_ERROR;
    }

    return U_SHORT_RANGE_EDM_OK;
}

static int32_t getIpProtocol(char value, uShortRangeIpProtocol_t *pProtocol)
{
    switch (value) {
        case 0x00:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_TCP;
            break;
        case 0x01:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_UDP;
            break;
        case 0x02:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_MQTT;
            break;
        default:
            return U_SHORT_RANGE_EDM_ERROR;
    }

    return U_SHORT_RANGE_EDM_OK;
}

static uShortRangeEdmEvent_t *allocateEdmEvent(void)
{
    static uShortRangeEdmEvent_t gEdmEvent;

    return &gEdmEvent;
}

static uShortRangeEdmEvent_t *parseConnectBtEvent(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeBtProfile_t profile;
    int32_t result = getBtProfile(pBuffer[2], &profile);

    if ((payloadLength == 11) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventBt_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_BT;
        pEvtData = &pEvent->params.btConnectEvent;
        pEvtData->channel = pBuffer[0];
        pEvtData->connection.profile = profile;
        memcpy(pEvtData->connection.address, &pBuffer[3], U_SHORT_RANGE_BT_ADDRESS_LENGTH);
        pEvtData->connection.framesize = ((uint16_t)(uint8_t)pBuffer[9] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[10];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectIpv4Event(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeIpProtocol_t protocol;
    int32_t result = getIpProtocol(pBuffer[2], &protocol);

    if ((payloadLength == 15) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventIpv4_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4;
        pEvtData = &pEvent->params.ipv4ConnectEvent;
        pEvtData->channel = pBuffer[0];
        pEvtData->connection.protocol = protocol;
        memcpy(pEvtData->connection.remoteAddress, &pBuffer[3],
               U_SHORT_RANGE_IPv4_ADDRESS_LENGTH);
        pEvtData->connection.remotePort = ((uint16_t)(uint8_t)pBuffer[7] << 8) |
                                          (uint16_t)(uint8_t)pBuffer[8];
        memcpy(pEvtData->connection.localAddress, &pBuffer[9],
               U_SHORT_RANGE_IPv4_ADDRESS_LENGTH);
        pEvtData->connection.localPort = ((uint16_t)(uint8_t)pBuffer[13] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[14];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectIpv6Event(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeIpProtocol_t protocol;
    uint32_t result = getIpProtocol(pBuffer[2], &protocol);

    if ((payloadLength == 39) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventIpv6_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6;
        pEvtData = &pEvent->params.ipv6ConnectEvent;
        pEvtData->channel = pBuffer[0];
        pEvtData->connection.protocol = protocol;
        memcpy(pEvtData->connection.remoteAddress, &pBuffer[3],
               U_SHORT_RANGE_IPv6_ADDRESS_LENGTH);
        pEvtData->connection.remotePort = ((uint16_t)(uint8_t)pBuffer[19] << 8) |
                                          (uint16_t)(uint8_t)pBuffer[20];
        memcpy(pEvtData->connection.localAddress, &pBuffer[21],
               U_SHORT_RANGE_IPv6_ADDRESS_LENGTH);
        pEvtData->connection.localPort = ((uint16_t)(uint8_t)pBuffer[37] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[38];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectEvent(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;

    if (payloadLength > 2) {
        uint8_t type = (uint8_t)pBuffer[1];

        switch (type) {

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_BT:
                pEvent = parseConnectBtEvent(pBuffer, payloadLength);
                break;

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv4:
                pEvent = parseConnectIpv4Event(pBuffer, payloadLength);
                break;

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv6:
                pEvent = parseConnectIpv6Event(pBuffer, payloadLength);
                break;

            default:
                break;
        }
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseDisconnectEvent(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;

    if (payloadLength == 1) {
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_DISCONNECT;
        pEvent->params.disconnectEvent.channel = (uint8_t)pBuffer[0];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseDataEvent(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;

    if (payloadLength > 1) {
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_DATA;
        pEvent->params.dataEvent.channel = (uint8_t)pBuffer[0];
        pEvent->params.dataEvent.pData = &pBuffer[1];
        pEvent->params.dataEvent.length = payloadLength - 1;
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseAtResponseOrEvent(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = allocateEdmEvent();
    pEvent->type = U_SHORT_RANGE_EDM_EVENT_AT;
    pEvent->params.atEvent.pData = pBuffer;
    pEvent->params.atEvent.length = payloadLength;
    return pEvent;
}

static uShortRangeEdmEvent_t *parseEdmPayload(char *pBuffer, uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uint16_t idAndType = ((uint16_t)(uint8_t)pBuffer[0] << 8) | (uint16_t)(uint8_t)pBuffer[1];
    char *pSubPayload = pBuffer + 2;
    uint16_t subPayloadLength = payloadLength - 2;

    switch (idAndType) {
        case U_SHORT_RANGE_EDM_TYPE_CONNECT_EVENT:
            pEvent = parseConnectEvent(pSubPayload, subPayloadLength);
            break;

        case U_SHORT_RANGE_EDM_TYPE_DISCONNECT_EVENT:
            pEvent = parseDisconnectEvent(pSubPayload, subPayloadLength);
            break;

        case U_SHORT_RANGE_EDM_TYPE_DATA_EVENT:
            pEvent = parseDataEvent(pSubPayload, subPayloadLength);
            break;

        case U_SHORT_RANGE_EDM_TYPE_AT_RESPONSE:
        case U_SHORT_RANGE_EDM_TYPE_AT_EVENT:
            pEvent = parseAtResponseOrEvent(pSubPayload, subPayloadLength);
            break;

        case U_SHORT_RANGE_EDM_TYPE_START_EVENT:
            if (subPayloadLength == 0) {
                pEvent = allocateEdmEvent();
                pEvent->type = U_SHORT_RANGE_EDM_EVENT_STARTUP;
            }
            break;

        //lint -e825
        case U_SHORT_RANGE_EDM_TYPE_DATA_COMMAND:
        case U_SHORT_RANGE_EDM_TYPE_AT_REQUEST:
            pEvent = NULL;
            break;

        default:
            pEvent = NULL;
            break;
    }

    return pEvent;
}

static void resetPayload(void)
{
    if (gpPayload != gShortPayloadBuffer) {
        free(gpPayload);
        gpPayload = gShortPayloadBuffer;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
bool uShortRangeEdmParserReady(void)
{
    return (gEdmParserState != EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING);
}

void uShortRangeEdmResetParser(void)
{
    resetPayload();
    gEdmParserState = EDM_PARSER_STATE_PARSE_START_BYTE;
}

uShortRangeEdmEvent_t *uShortRangeEdmParse(char c)
{
    uShortRangeEdmEvent_t *pResultEvent = NULL;
    edmParserState_t newState = gEdmParserState;
    static uint32_t byteIndex;
    static uint16_t payloadLength;

    switch (gEdmParserState) {

        case EDM_PARSER_STATE_PARSE_START_BYTE:
            if (c == U_SHORT_RANGE_EDM_HEAD) {
                newState = EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH;
                byteIndex = 0;
            }
            break;

        case EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH:
            if (byteIndex == 0) {
                payloadLength = (uint16_t)(uint8_t)c << 8;
                byteIndex++;
            } else {
                payloadLength |= (uint16_t)(uint8_t)c;
                if (payloadLength < 2) {
                    // Something is wrong, start over
                    newState = EDM_PARSER_STATE_PARSE_START_BYTE;
                } else {
                    newState = EDM_PARSER_STATE_ACCUMULATE_PAYLOAD;
                    byteIndex = 0;
                    if (payloadLength > U_SHORT_RANGE_EDM_SHORT_BUFFER_LENGTH) {
                        // Payload is too large to fit in the normal short buffer
                        // malloc a larger buffer and free it later when the event
                        // has been processed
                        if (payloadLength <= U_SHORT_RANGE_EDM_MAX_SIZE) {
                            gpPayload = (char *)malloc(payloadLength);
                        } else {
                            gpPayload = NULL;
                        }
                        if (gpPayload == NULL) {
                            // We could not allocate a buffer
                            // Reset parser
                            gpPayload = gShortPayloadBuffer;
                            newState = EDM_PARSER_STATE_PARSE_START_BYTE;
                        }
                    } else {
                        gpPayload = gShortPayloadBuffer;
                    }
                }
            }
            break;

        case EDM_PARSER_STATE_ACCUMULATE_PAYLOAD:
            gpPayload[byteIndex++] = c;
            if (byteIndex == payloadLength) {
                newState = EDM_PARSER_STATE_PARSE_TAIL_BYTE;
            }
            break;

        case EDM_PARSER_STATE_PARSE_TAIL_BYTE:
            if (c == U_SHORT_RANGE_EDM_TAIL) {
                pResultEvent = parseEdmPayload(gpPayload, payloadLength);
            }
            if (pResultEvent == NULL) {
                // No event was generated
                // Reset parser
                resetPayload();
                newState = EDM_PARSER_STATE_PARSE_START_BYTE;
            } else {
                newState = EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING;
            }
            break;

        case EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING:
            // Parser will stay in this state until parser is reset.
            // This to avoid the parser overwriting data in an unprocessed event
            // Any user of the parser thus have to reset the parser when it has
            // processed a generated event, to make it ready for parsing
            break;
    }

    gEdmParserState = newState;

    return pResultEvent;
}

int32_t uShortRangeEdmZeroCopyHeadData(uint8_t channel, uint32_t size, char *pHead)
{
    if (pHead == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    uint32_t edmSize = size + 3;

    *pHead = U_SHORT_RANGE_EDM_HEAD;
    *(pHead + 1) = (char)(edmSize >> 8);
    *(pHead + 2) = (char)(edmSize & 0xFF);
    *(pHead + 3) = 0x00;
    *(pHead + 4) = U_SHORT_RANGE_EDM_TYPE_DATA_COMMAND;
    *(pHead + 5) = (char)(channel);

    return 6;
}

//lint -e759 suppress "could be moved from header to module"
//lint -e765 suppress "could be made static"
//lint -e714 suppress "not referenced"
int32_t uShortRangeEdmData(uint8_t channel, const char *pData, int32_t size, char *pPacket)
{
    if (pPacket == NULL || pData == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    uShortRangeEdmZeroCopyHeadData(channel, size, pPacket);
    memcpy((pPacket + 6), pData, size);
    *(pPacket + size + 6) = U_SHORT_RANGE_EDM_TAIL;

    return U_SHORT_RANGE_EDM_OK;
}

int32_t uShortRangeEdmRequest(const char *pAt, int32_t size, char *pPacket)
{
    if (pPacket == NULL || pAt == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR;
    }

    *pPacket = U_SHORT_RANGE_EDM_HEAD;
    *(pPacket + 1) = (char)(((uint32_t)size + 2) >> 8);
    *(pPacket + 2) = (char)((size + 2) & 0xFF);
    *(pPacket + 3) = 0x00;
    *(pPacket + 4) = (char)U_SHORT_RANGE_EDM_TYPE_AT_REQUEST;
    memcpy((pPacket + 5), pAt, size);
    *(pPacket + size + 5) = U_SHORT_RANGE_EDM_TAIL;

    return size + 5 + 1;
}

int32_t uShortRangeEdmZeroCopyTail(char *pTail)
{
    if (pTail == NULL) {
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    *pTail = U_SHORT_RANGE_EDM_TAIL;

    return 1;
}

