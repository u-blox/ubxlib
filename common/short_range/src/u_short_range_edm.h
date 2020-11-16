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
#ifndef _U_SHORT_RANGE_EDM_H_
#define _U_SHORT_RANGE_EDM_H_

/** @file */

#define U_SHORT_RANGE_EDM_OK                  0
#define U_SHORT_RANGE_EDM_ERROR               -1
#define U_SHORT_RANGE_EDM_ERROR_PARAM         -2
#define U_SHORT_RANGE_EDM_ERROR_INVALID       -3
#define U_SHORT_RANGE_EDM_ERROR_INCOMPLETE    -4
#define U_SHORT_RANGE_EDM_ERROR_SIZE          -5
#define U_SHORT_RANGE_EDM_ERROR_CORRUPTED     -6

#define U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH   6
#define U_SHORT_RANGE_EDM_IPv4_ADDRESS_LENGTH 4
#define U_SHORT_RANGE_EDM_IPv6_ADDRESS_LENGTH 16

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


typedef enum {
    U_SHORT_RANGE_EDM_BT_PROFILE_SPP,
    U_SHORT_RANGE_EDM_BT_PROFILE_DUN,
    U_SHORT_RANGE_EDM_BT_PROFILE_SPS,
    U_SHORT_RANGE_EDM_BT_PROFILE_INVALID
} uShortRangeEdmBtProfile_t;

typedef enum {
    U_SHORT_RANGE_EDM_IP_PROTOCOL_TCP,
    U_SHORT_RANGE_EDM_IP_PROTOCOL_UDP,
    U_SHORT_RANGE_EDM_IP_PROTOCOL_INVALID
} uShortRangeEdmIpProtocol_t;

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

typedef struct uShortRangeEdmConnectionEventBT_t {
    uint8_t channel;
    uShortRangeEdmBtProfile_t profile;
    uint8_t address[U_SHORT_RANGE_EDM_BT_ADDRESS_LENGTH];
    uint16_t framesize;
} uShortRangeEdmConnectionEventBT_t;

typedef struct uShortRangeEdmConnectionEventIPv4_t {
    uint8_t channel;
    uShortRangeEdmIpProtocol_t protocol;
    uint8_t remoteAddress[U_SHORT_RANGE_EDM_IPv4_ADDRESS_LENGTH];
    uint16_t remotePort;
    uint8_t localAddress[U_SHORT_RANGE_EDM_IPv4_ADDRESS_LENGTH];
    uint16_t localPort;
} uShortRangeEdmConnectionEventIPv4_t;

typedef struct uShortRangeEdmConnectionEventIPv6_t {
    uint8_t channel;
    uShortRangeEdmIpProtocol_t protocol;
    uint8_t remoteAddress[U_SHORT_RANGE_EDM_IPv6_ADDRESS_LENGTH];
    uint16_t remotePort;
    uint8_t localAddress[U_SHORT_RANGE_EDM_IPv6_ADDRESS_LENGTH];
    uint16_t localPort;
} uShortRangeEdmConnectionEventIPv6_t;

typedef struct uShortRangeEdmDisconnectEvent_t {
    uint8_t channel;
} uShortRangeEdmDisconnectEvent_t;

typedef struct uShortRangeEdmDataEvent_t {
    uint8_t channel;
    uint16_t length;
    char *pData;
} uShortRangeEdmDataEvent_t;

typedef struct uShortRangeEdmAtEvent_t {
    uint16_t length;
    char *pData;
} uShortRangeEdmAtEvent_t;

typedef struct uShortRangeEdmEvent_t {
    uShortRangeEdmEventType_t type;
    union {
        uShortRangeEdmConnectionEventBT_t btConnectEvent;
        uShortRangeEdmConnectionEventIPv4_t ipv4ConnectEvent;
        uShortRangeEdmConnectionEventIPv6_t ipv6ConnectEvent;
        uShortRangeEdmDisconnectEvent_t disconnectEvent;
        uShortRangeEdmDataEvent_t dataEvent;
        uShortRangeEdmAtEvent_t atEvent;
    } params;
} uShortRangeEdmEvent_t;

/**
 *
 * @brief Function for parsing binary EDM data into the event structure
 *
 * @note  Only the first found EDM packet is parsed. So (after handling the event
 *        data output) rerun the function until all data is consumed.<br>
 *        Special case U_SHORT_RANGE_EDM_ERROR_INCOMPLETE in which case all data has probably not
 *        been received yet.
 *
 * @param[in] pInData Pointer to the binary data to parse
 * @param[in] size Size of the binary data. Must be U_SHORT_RANGE_EDM_MIN_SIZE bytes or larger.
 * @param[out] pEventData Output structure, needs to be allocated by caller before call.
 *             Only valid if result code is U_SHORT_RANGE_EDM_OK.
 * @param[out] pExpectedSize Pointer containing the expected size, this is the special case in which the
 *             packet is incomplete, but the length data field is valid and read. So this can be use to
 *             determine how much data is needed before there is any chance of a complete packet.
 * @param[out] pConsumed The number of bytes of the input that has been parsed. See function
               description on how to handle.
 *
 * @post If output type is U_SHORT_RANGE_EDM_EVENT_DATA or U_SHORT_RANGE_EDM_EVENT_AT the data/command pointer in eventData
 *       points to memory in the original inData. So inData most still be in scoop when eventData
 *       is used.
 *
 * @retval U_SHORT_RANGE_EDM_OK Successful parsing
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM inData is NULL or size in invalid
 * @retval U_SHORT_RANGE_EDM_ERROR_INVALID Not a valid EDM packet
 * @retval U_SHORT_RANGE_EDM_ERROR_INCOMPLETE Start of a valid EDM packet, but does not contain a full packet
 * @retval U_SHORT_RANGE_EDM_ERROR_SIZE EDM size header does not match actual data, should never happen
 * @retval U_SHORT_RANGE_EDM_ERROR_CORRUPTED Invalid value of data inside the EDM packet, should never happen
 */
int32_t uShortRangeEdmParse(char *pInData, size_t size, uShortRangeEdmEvent_t *pEventData,
                            int32_t *pExpectedSize, size_t *pConsumed);

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
 * @brief Creates an EDM AT request packet header
 *
 * @details This is a way to avoid a memcpy and can e.g. be useful if the packet is sent
 *          over a stream based channel the accepts bytes/chunks. The module will assemble
 *          the input and execute when a full EDM packet is received.<br>
 *          Valid EDM packet: head + AT request + tail.
 *
 * @param[in] size Size of the AT request.
 * @param[out] pHead Pointer to a memory where the EDM packet is created. This need to be
 *             an allocated memory area of U_SHORT_RANGE_EDM_REQUEST_OVERHEAD.
 *
 * @retval Number of bytes used in the head memory.
 * @retval U_SHORT_RANGE_EDM_ERROR_PARAM Input pointer and null or size is to large.
 */
int32_t uShortRangeEdmZeroCopyHeadRequest(uint32_t size, char *pHead);

/**
 *
 * @brief Creates an EDM data packet header
 *
 * @details This is a way to avoid a memcpy and can e.g. be useful if the packet is sent
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
