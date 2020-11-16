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
 * @brief Extended data mode implementation
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "u_short_range_edm.h"

#define U_SHORT_RANGE_EDM_SIZE_HEAD               1
#define U_SHORT_RANGE_EDM_SIZE_TAIL               1
#define U_SHORT_RANGE_EDM_SIZE_LENGTH             2
#define U_SHORT_RANGE_EDM_HEAD                    0xAA
#define U_SHORT_RANGE_EDM_TAIL                    0x55
#define U_SHORT_RANGE_EDM_LENGTH_FILTER           0x0F

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

static uShortRangeEdmEventType_t findEventType(char evt, char connectionType)
{
    uShortRangeEdmEventType_t type;

    switch (evt) {
        case U_SHORT_RANGE_EDM_TYPE_CONNECT_EVENT:
            if (connectionType == U_SHORT_RANGE_EDM_CONNECTION_TYPE_BT) {
                type = U_SHORT_RANGE_EDM_EVENT_CONNECT_BT;
            } else if (connectionType == U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv4) {
                type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4;
            } else if (connectionType == U_SHORT_RANGE_EDM_CONNECTION_TYPE_IPv6) {
                type = U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6;
            } else {
                type = U_SHORT_RANGE_EDM_EVENT_INVALID;
            }
            break;
        case U_SHORT_RANGE_EDM_TYPE_DISCONNECT_EVENT:
            type = U_SHORT_RANGE_EDM_EVENT_DISCONNECT;
            break;
        case U_SHORT_RANGE_EDM_TYPE_DATA_EVENT:
            type = U_SHORT_RANGE_EDM_EVENT_DATA;
            break;
        case U_SHORT_RANGE_EDM_TYPE_AT_RESPONSE:
        case U_SHORT_RANGE_EDM_TYPE_AT_EVENT:
            type = U_SHORT_RANGE_EDM_EVENT_AT;
            break;
        case U_SHORT_RANGE_EDM_TYPE_START_EVENT:
            type = U_SHORT_RANGE_EDM_EVENT_STARTUP;
            break;
        default:
            type = U_SHORT_RANGE_EDM_EVENT_INVALID;
            break;
    }

    return type;
}

static uShortRangeEdmBtProfile_t getBtProfile(char value)
{
    uShortRangeEdmBtProfile_t profile;

    switch (value) {
        case 0:
            profile = U_SHORT_RANGE_EDM_BT_PROFILE_SPP;
            break;
        case 1:
            profile = U_SHORT_RANGE_EDM_BT_PROFILE_DUN;
            break;
        case 14:
            profile = U_SHORT_RANGE_EDM_BT_PROFILE_SPS;
            break;
        default:
            profile = U_SHORT_RANGE_EDM_BT_PROFILE_INVALID;
            break;
    }

    return profile;
}

static uShortRangeEdmIpProtocol_t getIpProtocol(char value)
{
    uShortRangeEdmIpProtocol_t protocol;

    switch (value) {
        case 0x00:
            protocol = U_SHORT_RANGE_EDM_IP_PROTOCOL_TCP;
            break;
        case 0x01:
            protocol = U_SHORT_RANGE_EDM_IP_PROTOCOL_UDP;
            break;
        default:
            protocol = U_SHORT_RANGE_EDM_IP_PROTOCOL_INVALID;
            break;
    }

    return protocol;
}

int32_t uShortRangeEdmParse(char *pInData, size_t size, uShortRangeEdmEvent_t *pEventData,
                            int32_t *pExpectedSize, size_t *pConsumed)
{
    int32_t error = U_SHORT_RANGE_EDM_OK;
    uint32_t length;
    uint32_t used;
    char *packetHead = pInData;

    *pExpectedSize = -1;

    if (packetHead == NULL || size == 0) {
        *pConsumed = 0;
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    for (used = 0; used < size; used++) {
        if (*(packetHead + used) == (char)U_SHORT_RANGE_EDM_HEAD) {
            packetHead += used;
            break;
        }
    }

    if (used == size) {
        *pConsumed = used;
        return U_SHORT_RANGE_EDM_ERROR_INVALID;
    }

    if ((uint32_t)size < used + 2) {
        *pConsumed = used;
        return U_SHORT_RANGE_EDM_ERROR_INCOMPLETE;
    }

    length = ((*(packetHead + 1) & U_SHORT_RANGE_EDM_LENGTH_FILTER) << 8) + (*(packetHead + 2) & 0xFF);

    if (((uint32_t)size - used) < (U_SHORT_RANGE_EDM_SIZE_HEAD + U_SHORT_RANGE_EDM_SIZE_LENGTH + length
                                   +
                                   U_SHORT_RANGE_EDM_SIZE_TAIL)) {
        *pExpectedSize = (int32_t)length;
        *pConsumed = used;
        return U_SHORT_RANGE_EDM_ERROR_INCOMPLETE;
    }

    if (*(packetHead + length + U_SHORT_RANGE_EDM_SIZE_HEAD + U_SHORT_RANGE_EDM_SIZE_LENGTH) !=
        (char)U_SHORT_RANGE_EDM_TAIL) {
        *pConsumed = (size_t)used + U_SHORT_RANGE_EDM_SIZE_HEAD + U_SHORT_RANGE_EDM_SIZE_LENGTH +
                     (size_t)length +
                     U_SHORT_RANGE_EDM_SIZE_TAIL;
        return U_SHORT_RANGE_EDM_ERROR_SIZE;
    }

    pEventData->type = findEventType(*(packetHead + 4), *(packetHead + 6));

    switch (pEventData->type) {
        case U_SHORT_RANGE_EDM_EVENT_CONNECT_BT: {
            uShortRangeEdmBtProfile_t profile = getBtProfile(*(packetHead + 7));
            if (profile == U_SHORT_RANGE_EDM_BT_PROFILE_INVALID) {
                error = U_SHORT_RANGE_EDM_ERROR_CORRUPTED;
                pEventData->type = U_SHORT_RANGE_EDM_EVENT_INVALID;
            } else {
                pEventData->params.btConnectEvent.profile = profile;
                pEventData->params.btConnectEvent.channel = (*(packetHead + 5) & 0xFF);
                pEventData->params.btConnectEvent.framesize = ((*(packetHead + 14) & 0xFF) << 8) +
                                                              (*(packetHead + 15) & 0xFF);
                memcpy(&pEventData->params.btConnectEvent.address[0], packetHead + 8,
                       U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH);
            }
            used += 17;
            break;
        }
        case U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4:
            pEventData->params.ipv4ConnectEvent.channel = (*(packetHead + 5) & 0xFF);
            pEventData->params.ipv4ConnectEvent.protocol = getIpProtocol(*(packetHead + 7));
            memcpy(&pEventData->params.ipv4ConnectEvent.remoteAddress[0], packetHead + 8,
                   U_SHORT_RANGE_EDM_IPv4_ADDRESS_LENGTH);
            pEventData->params.ipv4ConnectEvent.remotePort = ((*(packetHead + 12) & 0xFF) << 8) +
                                                             (*(packetHead + 13) & 0xFF);
            memcpy(&pEventData->params.ipv4ConnectEvent.localAddress[0], packetHead + 14,
                   U_SHORT_RANGE_EDM_IPv4_ADDRESS_LENGTH);
            pEventData->params.ipv4ConnectEvent.localPort = ((*(packetHead + 18) & 0xFF) << 8) + (*
                                                                                                  (packetHead + 19) & 0xFF);
            used += 21;
            break;
        case U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6:
            pEventData->params.ipv6ConnectEvent.channel = (*(packetHead + 5) & 0xFF);
            pEventData->params.ipv6ConnectEvent.protocol = getIpProtocol(*(packetHead + 7));
            memcpy(&pEventData->params.ipv6ConnectEvent.remoteAddress[0], packetHead + 8,
                   U_SHORT_RANGE_EDM_IPv6_ADDRESS_LENGTH);
            pEventData->params.ipv6ConnectEvent.remotePort = ((*(packetHead + 24) & 0xFF) << 8) + (*
                                                                                                   (packetHead + 25) & 0xFF);
            memcpy(&pEventData->params.ipv6ConnectEvent.localAddress[0], packetHead + 26,
                   U_SHORT_RANGE_EDM_IPv6_ADDRESS_LENGTH);
            pEventData->params.ipv6ConnectEvent.localPort = ((*(packetHead + 42) & 0xFF) << 8) + (*
                                                                                                  (packetHead + 43) & 0xFF);
            used += 45;
            break;
        case U_SHORT_RANGE_EDM_EVENT_DISCONNECT:
            pEventData->params.disconnectEvent.channel = (*(packetHead + 5) & 0xFF);
            used += 7;
            break;
        case U_SHORT_RANGE_EDM_EVENT_DATA:
            pEventData->params.dataEvent.channel = *(packetHead + 5);
            pEventData->params.dataEvent.pData = packetHead + 6;
            length = ((*(packetHead + 1) & U_SHORT_RANGE_EDM_LENGTH_FILTER) << 8) + *(packetHead + 2);
            pEventData->params.dataEvent.length = (uint16_t)(length - 3);
            used += length + 4;
            break;
        case U_SHORT_RANGE_EDM_EVENT_AT:
            pEventData->params.atEvent.pData = packetHead + 5;
            length = ((*(packetHead + 1) & U_SHORT_RANGE_EDM_LENGTH_FILTER) << 8) + *(packetHead + 2);
            pEventData->params.atEvent.length = (uint16_t)(length - 2);
            used += length + 4;
            break;
        case U_SHORT_RANGE_EDM_EVENT_STARTUP:
            used += 6;
            break;
        case U_SHORT_RANGE_EDM_EVENT_INVALID:
        default:
            used += U_SHORT_RANGE_EDM_SIZE_HEAD + U_SHORT_RANGE_EDM_SIZE_LENGTH + length +
                    U_SHORT_RANGE_EDM_SIZE_TAIL;
            error = U_SHORT_RANGE_EDM_ERROR_CORRUPTED;
            break;
    }

    *pConsumed = used;

    return error;
}

int32_t uShortRangeEdmZeroCopyHeadData(uint8_t channel, uint32_t size, char *pHead)
{
    if (pHead == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    uint32_t edmSize = size + 3;

    *pHead = (char)U_SHORT_RANGE_EDM_HEAD;
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

int32_t uShortRangeEdmZeroCopyHeadRequest(uint32_t size, char *pHead)
{
    if (pHead == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR_PARAM;
    }

    uint32_t edmSize = size + 2;

    *pHead = (char)U_SHORT_RANGE_EDM_HEAD;
    *(pHead + 1) = (char)(edmSize >> 8);
    *(pHead + 2) = (char)(edmSize & 0xFF);
    *(pHead + 3) = 0x00;
    *(pHead + 4) = (char)U_SHORT_RANGE_EDM_TYPE_AT_REQUEST;

    return 5;
}

int32_t uShortRangeEdmRequest(const char *pAt, int32_t size, char *pPacket)
{
    if (pPacket == NULL || pAt == NULL || size > U_SHORT_RANGE_EDM_MAX_SIZE) {
        return U_SHORT_RANGE_EDM_ERROR;
    }

    uShortRangeEdmZeroCopyHeadRequest(size, pPacket);
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

