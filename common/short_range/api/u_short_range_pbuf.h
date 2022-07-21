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
#ifndef _U_SHORT_RANGE_PBUF_H_
#define _U_SHORT_RANGE_PBUF_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_compiler.h"

/** \addtogroup _short-range
 *  @{
 */

/** @file
 * @brief This header file defines buffer management mechanism used
 * by the Wifi/BLE modules.  These functions are not intended to be
 * called directly, they are called internally within ubxlib.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
/**
 * List of Pointer to payload
 */
// *INDENT-OFF*
#ifdef _MSC_VER
// Suppress zero-sized array in struct/union - it's intentional
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif
typedef U_PACKED_STRUCT(uShortRangePbuf_t) {
    struct uShortRangePbuf_t *pNext; /**< Used for linked list of pBuf */
    uint16_t length; /**< Number of used bytes in the data buffer */
    char data[];  /**< Data buffer */
} uShortRangePbuf_t;
#ifdef _MSC_VER
#pragma warning( pop )
#endif
// *INDENT-ON*

/**
 * List of pbufs. Each pbuf list corresponds to one EDM payload
 */
// *INDENT-OFF*
typedef U_PACKED_STRUCT(uShortRangePbufList_t) {
    uShortRangePbuf_t *pBufHead;
    uShortRangePbuf_t *pBufTail;
    struct uShortRangePbufList_t *pNext;
    // total length of the packet data
    uint16_t totalLen;
    // edm channel of this payload
    int8_t edmChannel;
} uShortRangePbufList_t;
// *INDENT-ON*

/** List of pbuf list. Packet list contains multiple
 * EDM payloads. Packet list are mainly used in message based
 * datapath clients like MQTT, UDP
 */
typedef struct uShortRangePktList_t {
    uShortRangePbufList_t *pBufListHead;
    uShortRangePbufList_t *pBufListTail;
    int32_t pktCount;
} uShortRangePktList_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */
/** Initialize the memory pool for shortrange.
 *
 * @return zero on success else negative error code.
 */
int32_t uShortRangeMemPoolInit(void);

/** Release all the associated memory pools for shortrange.
 *
 */
void uShortRangeMemPoolDeInit(void);

/** Allocate fixed size memory from gEdmPayLoadPool memory pool.
 * Refer to gEdmPayLoadPool in u_short_range_pbuf.c
 * Memory pool should have been initialized before using this
 * API. Refer to uShortRangeMemPoolInit()
 *
 * @param[out] ppBuf a double pointer to destination pbuf.
 * @return  data size of the returned pbuf, on failure negative error code.
 *
 */
int32_t uShortRangePbufAlloc(uShortRangePbuf_t **ppBuf);

/** Allocate memory for pbuf list from the pbuf list
 * memory pool. Refer to gPBufListPool in u_short_range_pbuf.c
 * Memory pool should have been initialized before using this
 * API. Refer to uShortRangeMemPoolInit()
 *
 * @return  pointer to uShortRangePbufList_t or NULL.
 *
 */
uShortRangePbufList_t *pUShortRangePbufListAlloc(void);

/** Put the allocated memory for pbufs and packet in to their
 * free list of respective pool.
 *
 * @param[in] pBufList Pointer to the packet.
 */
void uShortRangePbufListFree(uShortRangePbufList_t *pBufList);

/** Append a pbuf to a pbuf list.
 *
 * @param[out] pBufList pointer to the destination pbuf list.
 * @param[in] pBuf      the pbuf to be added to the list.
 * @return              zero on success, on failure negative error code.
 */
int32_t uShortRangePbufListAppend(uShortRangePbufList_t *pBufList, uShortRangePbuf_t *pBuf);

/** Reads and consume data from the pbuf list.
 *  At the end of move operation, next pbuf position to read will be updated in the pbuflist.
 *
 * @param[in] pBufList pointer to the pbuf list.
 * @param[out] pData   pointer to the destination buffer.
 * @param len          length of the destination buffer.
 * @return             copied length.
 */
size_t uShortRangePbufListConsumeData(uShortRangePbufList_t *pBufList, char *pData, size_t len);

/** Link a new pbuf list to the existing pbuf list.
 *  The pointer allocated for the new pbuf list from the pbuf list pool
 *  will be added to its free list.
 *
 * @param[in] pOldList  pointer to the existing pbuf list.
 * @param[out] pNewList pointer to the new pbuf list.
 */
void uShortRangePbufListMerge(uShortRangePbufList_t *pOldList, uShortRangePbufList_t *pNewList);

/** Insert a pbuf list to the packet list.
 *
 * @param[in,out] pPktList pointer to the packet list.
 * @param[in] pBufList     pointer to the pbuf list.
 * @return                 zero on success or negative error code.
 */
int32_t uShortRangePktListAppend(uShortRangePktList_t *pPktList,
                                 uShortRangePbufList_t *pBufList);

/** Read and consume a packet in a packet list.
 * If the given buffer size cannot accommodate the size of a complete packet, partial
 * data will be copied.
 *
 * @param[in,out] pPktList pointer to the packet list.
 * @param[out] pData       pointer to the destination buffer.
 * @param[in,out] pLen     on entry this should point to length of destination buffer.
 *                         on return this will be updated to copied length.
 * @param[out] pEdmChannel on return EDM channel corresponding to the read packet will be updated.
 * @return                 zero on success or negative error code.
 *                         #U_ERROR_COMMON_TEMPORARY_FAILURE, if given buffer cannot accommodate
 *                         the entire message - in this case the packet will be flushed.
 */
int32_t uShortRangePktListConsumePacket(uShortRangePktList_t *pPktList, char *pData, size_t *pLen,
                                        int32_t *pEdmChannel);
#ifdef __cplusplus
}
#endif

/** @}*/

#endif //_U_SHORT_RANGE_PBUF_H_

// End of file
