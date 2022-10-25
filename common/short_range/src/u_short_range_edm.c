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

/** @file
 * @brief Extended data mode implementation
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "u_error_common.h"
#include "u_assert.h"
#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
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
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef enum {
    EDM_PARSER_STATE_PARSE_START_BYTE,
    EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH,
    EDM_PARSER_STATE_PARSE_HEADER_LENGTH,
    EDM_PARSER_STATE_ALLOCATE_PBUFLIST,
    EDM_PARSER_STATE_ALLOCATE_PAYLOAD,
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
static uShortRangeEdmEvent_t *parseConnectBtEvent(uint8_t channel, char *buffer,
                                                  uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectIpv4Event(uint8_t channel, char *buffer,
                                                    uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectIpv6Event(uint8_t channel, char *buffer,
                                                    uint16_t payloadLength);
static uShortRangeEdmEvent_t *parseConnectEvent(uint8_t channel, uShortRangePbufList_t *pBufList);
static uShortRangeEdmEvent_t *parseDisconnectEvent(uint8_t channel);
static uShortRangeEdmEvent_t *parseDataEvent(uint8_t channel, uShortRangePbufList_t *pBufList);
static uShortRangeEdmEvent_t *parseAtResponseOrEvent(uShortRangePbufList_t *pBufList);
static uShortRangeEdmEvent_t *parseEdmPayload(uint16_t idAndType, uint8_t channel,
                                              uShortRangePbufList_t *pBufList);

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
static edmParserState_t gEdmParserState = EDM_PARSER_STATE_PARSE_START_BYTE;
static uShortRangePbufList_t *gCurPBufList = NULL;
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

static uShortRangeEdmEvent_t *parseConnectBtEvent(uint8_t channel, char *pBuffer,
                                                  uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeBtProfile_t profile = 0;
    int32_t result = getBtProfile(pBuffer[1], &profile);

    if ((payloadLength == 10) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventBt_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_BT;
        pEvtData = &pEvent->params.btConnectEvent;
        pEvtData->channel = channel;
        pEvtData->connection.profile = profile;
        memcpy(pEvtData->connection.address, &pBuffer[2], U_SHORT_RANGE_BT_ADDRESS_LENGTH);
        pEvtData->connection.framesize = ((uint16_t)(uint8_t)pBuffer[8] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[9];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectIpv4Event(uint8_t channel, char *pBuffer,
                                                    uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeIpProtocol_t protocol = 0;
    int32_t result = getIpProtocol(pBuffer[1], &protocol);

    if ((payloadLength == 14) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventIpv4_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4;
        pEvtData = &pEvent->params.ipv4ConnectEvent;
        pEvtData->channel = channel;
        pEvtData->connection.protocol = protocol;
        memcpy(pEvtData->connection.remoteAddress, &pBuffer[2],
               U_SHORT_RANGE_IPv4_ADDRESS_LENGTH);
        pEvtData->connection.remotePort = ((uint16_t)(uint8_t)pBuffer[6] << 8) |
                                          (uint16_t)(uint8_t)pBuffer[7];
        memcpy(pEvtData->connection.localAddress, &pBuffer[8],
               U_SHORT_RANGE_IPv4_ADDRESS_LENGTH);
        pEvtData->connection.localPort = ((uint16_t)(uint8_t)pBuffer[12] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[13];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectIpv6Event(uint8_t channel, char *pBuffer,
                                                    uint16_t payloadLength)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uShortRangeIpProtocol_t protocol = 0;
    uint32_t result = getIpProtocol(pBuffer[1], &protocol);

    if ((payloadLength == 38) && (result == U_SHORT_RANGE_EDM_OK)) {
        uShortRangeEdmConnectionEventIpv6_t *pEvtData;
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6;
        pEvtData = &pEvent->params.ipv6ConnectEvent;
        pEvtData->channel = channel;
        pEvtData->connection.protocol = protocol;
        memcpy(pEvtData->connection.remoteAddress, &pBuffer[2],
               U_SHORT_RANGE_IPv6_ADDRESS_LENGTH);
        pEvtData->connection.remotePort = ((uint16_t)(uint8_t)pBuffer[18] << 8) |
                                          (uint16_t)(uint8_t)pBuffer[19];
        memcpy(pEvtData->connection.localAddress, &pBuffer[20],
               U_SHORT_RANGE_IPv6_ADDRESS_LENGTH);
        pEvtData->connection.localPort = ((uint16_t)(uint8_t)pBuffer[36] << 8) |
                                         (uint16_t)(uint8_t)pBuffer[37];
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseConnectEvent(uint8_t channel, uShortRangePbufList_t *pBufList)
{
    uShortRangeEdmEvent_t *pEvent = NULL;
    uint16_t payloadLength = 0;
    char *pBuffer = NULL;

    if ((pBufList != NULL) &&
        (pBufList->totalLen > 2)) {

        pBuffer = &pBufList->pBufHead->data[0];
        payloadLength = (uint16_t)pBufList->totalLen;
        uint8_t type = (uint8_t)pBuffer[0];

        switch (type) {

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_BT:
                pEvent = parseConnectBtEvent(channel, pBuffer, payloadLength);
                break;

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv4:
                pEvent = parseConnectIpv4Event(channel, pBuffer, payloadLength);
                break;

            case U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv6:
                pEvent = parseConnectIpv6Event(channel, pBuffer, payloadLength);
                break;

            default:
                break;
        }
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseDisconnectEvent(uint8_t channel)
{
    uShortRangeEdmEvent_t *pEvent;

    pEvent = allocateEdmEvent();
    pEvent->type = U_SHORT_RANGE_EDM_EVENT_DISCONNECT;
    pEvent->params.disconnectEvent.channel = channel;

    return pEvent;
}

static uShortRangeEdmEvent_t *parseDataEvent(uint8_t channel, uShortRangePbufList_t *pBufList)
{
    uShortRangeEdmEvent_t *pEvent = NULL;

    if ((pBufList != NULL) && (pBufList->totalLen > 0)) {
        pEvent = allocateEdmEvent();
        pEvent->type = U_SHORT_RANGE_EDM_EVENT_DATA;
        pEvent->params.dataEvent.channel = channel;
        pEvent->params.dataEvent.pBufList = pBufList;
    }

    return pEvent;
}

static uShortRangeEdmEvent_t *parseAtResponseOrEvent(uShortRangePbufList_t *pBufList)
{
    uShortRangeEdmEvent_t *pEvent = allocateEdmEvent();
    pEvent->type = U_SHORT_RANGE_EDM_EVENT_AT;
    pEvent->params.atEvent.pBufList = pBufList;
    return pEvent;
}

static uShortRangeEdmEvent_t *parseEdmPayload(uint16_t idAndType, uint8_t channel,
                                              uShortRangePbufList_t *pBufList)
{
    uShortRangeEdmEvent_t *pEvent = NULL;

    switch (idAndType) {

        case U_SHORT_RANGE_EDM_TYPE_CONNECT_EVENT:
            pEvent = parseConnectEvent(channel, pBufList);
            uShortRangePbufListFree(pBufList);
            break;

        case U_SHORT_RANGE_EDM_TYPE_DISCONNECT_EVENT:
            pEvent = parseDisconnectEvent(channel);
            uShortRangePbufListFree(pBufList);
            break;

        case U_SHORT_RANGE_EDM_TYPE_DATA_EVENT:
            pEvent = parseDataEvent(channel, pBufList);
            break;

        case U_SHORT_RANGE_EDM_TYPE_AT_RESPONSE:
        case U_SHORT_RANGE_EDM_TYPE_AT_EVENT:
            pEvent = parseAtResponseOrEvent(pBufList);
            break;

        case U_SHORT_RANGE_EDM_TYPE_START_EVENT:
            pEvent = allocateEdmEvent();
            pEvent->type = U_SHORT_RANGE_EDM_EVENT_STARTUP;
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
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
bool uShortRangeEdmParserReady(void)
{
    return (gEdmParserState != EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING);
}

void uShortRangeEdmResetParser(void)
{
    gEdmParserState = EDM_PARSER_STATE_PARSE_START_BYTE;
}

bool uShortRangeEdmParse(char c, uShortRangeEdmEvent_t **ppResultEvent, bool *pMemAvailable)
{
    edmParserState_t newState = gEdmParserState;
    static uint16_t payloadLength;
    static uShortRangePbuf_t *pBuf;
    static int32_t pBufSize;
    static char header[U_SHORT_RANGE_EDM_HEADER_SIZE];
    static uint32_t headerIndex;
    static uint16_t idAndType;
    static uint8_t channel;
    bool charConsumed = false;
    int32_t result;

    *pMemAvailable = true;
    switch (gEdmParserState) {

        case EDM_PARSER_STATE_PARSE_START_BYTE:
            if (c == U_SHORT_RANGE_EDM_HEAD) {
                headerIndex = 0;
                newState = EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH;
            }
            charConsumed = true;
            break;

        case EDM_PARSER_STATE_PARSE_PAYLOAD_LENGTH:
            if (headerIndex == 0) {
                payloadLength = (uint16_t)(uint8_t)c << 8;
                headerIndex++;
            } else {
                payloadLength |= (uint16_t)(uint8_t)c;
                if (payloadLength < 2) {
                    // Something is wrong, start over
                    newState = EDM_PARSER_STATE_PARSE_START_BYTE;
                } else {
                    headerIndex = 0;
                    newState = EDM_PARSER_STATE_PARSE_HEADER_LENGTH;
                }
            }
            charConsumed = true;
            break;
        case EDM_PARSER_STATE_PARSE_HEADER_LENGTH:
            header[headerIndex++] = c;
            payloadLength--;

            if (headerIndex == 2) {

                idAndType = ((uint16_t)(uint8_t)header[0] << 8) | (uint16_t)(uint8_t)header[1];

                if ((idAndType == U_SHORT_RANGE_EDM_TYPE_AT_RESPONSE) ||
                    (idAndType == U_SHORT_RANGE_EDM_TYPE_AT_EVENT)    ||
                    (idAndType == U_SHORT_RANGE_EDM_TYPE_START_EVENT) ||
                    (idAndType == U_SHORT_RANGE_EDM_TYPE_AT_REQUEST)) {

                    // Channel does not exist for these types so
                    // fill in -1
                    header[headerIndex++] = -1;
                }
            }

            if (headerIndex == U_SHORT_RANGE_EDM_HEADER_SIZE) {
                channel = header[2];
                // gCurPBufChain should always be NULL here
                // If it's not we have a leak
                U_ASSERT(gCurPBufList == NULL);
                pBuf = NULL;
                newState = EDM_PARSER_STATE_ALLOCATE_PBUFLIST;
                // For disconnect event there is no payload
                // so directly head to parse tail byte
                if ((idAndType == U_SHORT_RANGE_EDM_TYPE_DISCONNECT_EVENT) ||
                    (idAndType == U_SHORT_RANGE_EDM_TYPE_START_EVENT)) {
                    newState = EDM_PARSER_STATE_PARSE_TAIL_BYTE;
                }
            }
            charConsumed = true;
            break;

        case EDM_PARSER_STATE_ALLOCATE_PBUFLIST:

            // if allocation fails stay back until
            // we have some free memory in their respective pool
            gCurPBufList = pUShortRangePbufListAlloc();
            if (gCurPBufList != NULL) {
                gCurPBufList->edmChannel = channel;
                newState = EDM_PARSER_STATE_ALLOCATE_PAYLOAD;
            } else {
                *pMemAvailable = false; // remain at same state, try again later
            }
            // we dont consume the input char in this state
            charConsumed = false;
            break;

        case EDM_PARSER_STATE_ALLOCATE_PAYLOAD:

            // if allocation fails stay back until
            // we have some free memory in their respective pool
            pBufSize = uShortRangePbufAlloc(&pBuf);
            if (pBufSize > 0) {
                headerIndex = 0;
                newState = EDM_PARSER_STATE_ACCUMULATE_PAYLOAD;
            } else {
                *pMemAvailable = false; // remain at same state, try again later
            }
            // we dont consume the input char in this state
            charConsumed = false;
            break;

        case EDM_PARSER_STATE_ACCUMULATE_PAYLOAD:

            U_ASSERT(pBufSize > 0);
            U_ASSERT(pBuf != NULL);
            U_ASSERT(pBuf->length < pBufSize);

            pBuf->data[pBuf->length++] = c;
            payloadLength--;

            if ((pBuf->length == pBufSize) ||
                (payloadLength == 0)) {
                result = uShortRangePbufListAppend(gCurPBufList, pBuf);
                U_ASSERT(result == 0);
                if (payloadLength == 0) {
                    newState = EDM_PARSER_STATE_PARSE_TAIL_BYTE;
                } else if (pBuf->length == pBufSize) {
                    // we have some more data coming in
                    // so allocate memory for payload
                    newState = EDM_PARSER_STATE_ALLOCATE_PAYLOAD;
                }
                pBuf = NULL;
            }
            charConsumed = true;
            break;

        case EDM_PARSER_STATE_PARSE_TAIL_BYTE:
            newState = EDM_PARSER_STATE_PARSE_START_BYTE;
            if (c == U_SHORT_RANGE_EDM_TAIL) {
                if (ppResultEvent != NULL) {
                    *ppResultEvent = parseEdmPayload(idAndType, channel, gCurPBufList);
                    if (*ppResultEvent == NULL) {
                        // No event was generated
                        // Reset parser
                        newState = EDM_PARSER_STATE_PARSE_START_BYTE;
                    } else {
                        newState = EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING;
                    }
                }
            }
            if (newState == EDM_PARSER_STATE_PARSE_START_BYTE) {
                // Always de-allocate the buffer when we reset the parser
                uShortRangePbufListFree(gCurPBufList);
            }
            gCurPBufList = NULL;
            charConsumed = true;
            break;

        case EDM_PARSER_STATE_WAIT_FOR_EVENT_PROCESSING:
            // Parser will stay in this state until parser is reset.
            // This to avoid the parser overwriting data in an unprocessed event
            // Any user of the parser thus have to reset the parser when it has
            // processed a generated event, to make it ready for parsing
            break;
    }

    gEdmParserState = newState;

    return charConsumed;
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

