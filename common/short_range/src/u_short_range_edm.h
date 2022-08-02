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
#ifndef _U_SHORT_RANGE_EDM_H_
#define _U_SHORT_RANGE_EDM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */
#include "u_short_range.h"

/** @file */

#define U_SHORT_RANGE_EDM_OK                  0
#define U_SHORT_RANGE_EDM_ERROR               -1
#define U_SHORT_RANGE_EDM_ERROR_PARAM         -2

#define U_SHORT_RANGE_EDM_REQUEST_OVERHEAD    6
//lint -esym(755, U_SHORT_RANGE_EDM_DATA_OVERHEAD) Suppress lack of a reference
#define U_SHORT_RANGE_EDM_DATA_OVERHEAD       7
//lint -esym(755, U_SHORT_RANGE_EDM_REQUEST_HEAD_SIZE) Suppress lack of a reference
#define U_SHORT_RANGE_EDM_REQUEST_HEAD_SIZE   5
#define U_SHORT_RANGE_EDM_DATA_HEAD_SIZE      6
#define U_SHORT_RANGE_EDM_TAIL_SIZE           1
//lint -esym(755, U_SHORT_RANGE_EDM_MAX_OVERHEAD) Suppress lack of a reference
#define U_SHORT_RANGE_EDM_MAX_OVERHEAD        7

#define U_SHORT_RANGE_EDM_MAX_SIZE            0xFFC
//lint -esym(755, U_SHORT_RANGE_EDM_MIN_SIZE) Suppress lack of a reference
#define U_SHORT_RANGE_EDM_MIN_SIZE            4

//lint -esym(755, U_SHORT_RANGE_EDM_MTU_IP_MAX_SIZE) Suppress lack of a reference
#define U_SHORT_RANGE_EDM_MTU_IP_MAX_SIZE     635
#define U_SHORT_RANGE_EDM_HEADER_SIZE         3 // (ID + TYPE)(2 bytes)  + CHANNEL ID (1 byte)
#define U_SHORT_RANGE_EDM_BLK_SIZE            64
#define U_SHORT_RANGE_EDM_BLK_COUNT           (U_SHORT_RANGE_EDM_MAX_SIZE / U_SHORT_RANGE_EDM_BLK_SIZE)

typedef enum {
    U_SHORT_RANGE_EDM_EVENT_CONNECT_BT,
    U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv4,
    U_SHORT_RANGE_EDM_EVENT_CONNECT_IPv6,
    U_SHORT_RANGE_EDM_EVENT_DISCONNECT,
    U_SHORT_RANGE_EDM_EVENT_DATA,
    U_SHORT_RANGE_EDM_EVENT_AT,
    U_SHORT_RANGE_EDM_EVENT_STARTUP,
    U_SHORT_RANGE_EDM_EVENT_INVALID
} uShortRangeEdmEventType_t;

typedef struct uShortRangeEdmConnectionEventBt_t {
    uint8_t channel;
    uShortRangeConnectDataBt_t connection;
} uShortRangeEdmConnectionEventBt_t;

typedef struct uShortRangeEdmConnectionEventIpv4_t {
    uint8_t channel;
    uShortRangeConnectionIpv4_t connection;
} uShortRangeEdmConnectionEventIpv4_t;

typedef struct uShortRangeEdmConnectionEventIpv6_t {
    uint8_t channel;
    uShortRangeConnectionIpv6_t connection;
} uShortRangeEdmConnectionEventIpv6_t;

typedef struct uShortRangeEdmDisconnectEvent_t {
    uint8_t channel;
} uShortRangeEdmDisconnectEvent_t;

typedef struct uShortRangeEdmDataEvent_t {
    uint8_t channel;
    uShortRangePbufList_t *pBufList;
} uShortRangeEdmDataEvent_t;

typedef struct uShortRangeEdmAtEvent_t {
    uShortRangePbufList_t *pBufList;
} uShortRangeEdmAtEvent_t;

typedef struct {
    uShortRangeEdmEventType_t type;
    union {
        uShortRangeEdmConnectionEventBt_t btConnectEvent;
        uShortRangeEdmConnectionEventIpv4_t ipv4ConnectEvent;
        uShortRangeEdmConnectionEventIpv6_t ipv6ConnectEvent;
        uShortRangeEdmDisconnectEvent_t disconnectEvent;
        uShortRangeEdmDataEvent_t dataEvent;
        uShortRangeEdmAtEvent_t atEvent;
    } params;
} uShortRangeEdmEvent_t;

/**
 *
 * @brief Check if EDM parser is available
 *
 * @note  Do not call the uShortRangeEdmParse function if this function
 *        returns false.
 *
 * @return True if EDM parser is available
 */
bool uShortRangeEdmParserReady(void);

/**
 *
 * @brief Reset the parser. Do this every time the latest EDM event
 *        has been processed to make the parser available again.
 */
void uShortRangeEdmResetParser(void);

/**
 *
 * @brief Function for parsing binary EDM data
 *
 * @note  Do not call this function if parser is not available,
 *        Check if parser is available with uShortRangeEdmParserAvailable
 *        If a packet is invalid it will be silently dropped.
 *
 * @param c Input character.
 *
 * @param[out] ppResultEvent Address of pointer to event, NULL if no event was generated
 *             An event is created when the last character in a EDM packet
 *             is parsed and the packet is valid.
 *
 * @param[out] pMemAvailable Pointer to a boolean that indicates if memory was allocated successfully.
 *
 * @return True when input character c is consumed else false.
 */
bool uShortRangeEdmParse(char c, uShortRangeEdmEvent_t **ppResultEvent, bool *pMemAvailable);

/**
 *
 * @brief Function packing an AT command request into an EDM packet
 *
 * @note This function will include a memcpy of size bytes, if there is desire to avoid
 *       this for performance and/or memory reasons use the edmZeroCopy functions instead.
 *
 * @param[in] pAt Pointer to the AT Command, must end with 0x0D.
 * @param[in] size Size of the AT Command not including terminating character. Must be less
 *             than U_SHORT_RANGE_EDM_MAX_SIZE.
 * @param[out] pPacket Pointer to a memory where the EDM packet is created. This need to be
 *             an allocated memory area of AT request size + U_SHORT_RANGE_EDM_REQUEST_OVERHEAD.
 *
 * @retval Number of bytes used in the packet memory. Typically size + U_SHORT_RANGE_EDM_REQUEST_OVERHEAD.
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM Input pointer and null or size is to large.
 */
int32_t uShortRangeEdmRequest(const char *pAt, int32_t size, char *pPacket);

/**
 *
 * @brief Function packing data into an EDM packet
 *
 * @note This function will include a memcpy of size bytes, if there is desire to avoid
 *       this for performance and/or memory reasons use the edmZeroCopy functions instead.
 *
 * @param[in] channel The EDM channel the data should be sent on.
 * @param[in] pData Pointer to the data.
 * @param[in] size Size of data. Must be less than U_SHORT_RANGE_EDM_MAX_SIZE.
 * @param[out] pPacket Pointer to a memory where the EDM packet is created. This need to be
 *             an allocated memory area of data size + U_SHORT_RANGE_EDM_DATA_OVERHEAD
 *
 * @retval Number of bytes used in the packet memory. Typically size + U_SHORT_RANGE_EDM_DATA_OVERHEAD
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM Input pointer and null or size is to large.
 */
int32_t uShortRangeEdmData(uint8_t channel, const char *pData, int32_t size, char *pPacket);

/**
 *
 * @brief Creates an EDM data packet header
 *
 * @details This is a way to avoid a memcpy and can, for example be useful if the packet is sent
 *          over a stream based channel the accepts bytes/chunks. The module will assemble
 *          the input and execute when a full EDM packet is received.<br>
 *          Valid EDM packet: head + data + tail.
 *
 * @param[in] channel The EDM channel the data should be sent on.
 * @param[in] size Size of the data.
 * @param[out] pHead Pointer to a memory where the EDM packet is created. This need to be
 *             an allocated memory area of U_SHORT_RANGE_EDM_DATA_OVERHEAD.
 *
 * @retval Number of bytes used in the head memory.
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM Input pointer and null or size is to large.
 */
int32_t uShortRangeEdmZeroCopyHeadData(uint8_t channel, uint32_t size, char *pHead);

/**
 *
 * @brief Creates an EDM data packet tail. Valid for both AT request and data.
 *
 * @details See uShortRangeEdmZeroCopyHeadData/uShortRangeEdmZeroCopyHeadRequest
 *
 * @param[out] pTail Pointer to a memory where the EDM packet is created. This need to be
 *             an allocated memory area of U_SHORT_RANGE_EDM_TAIL_OVERHEAD.
 *
 * @retval Number of bytes used in the tail memory.
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM Input pointer is null.
 */
int32_t uShortRangeEdmZeroCopyTail(char *pTail);

#endif

// End of file
