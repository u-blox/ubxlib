/*
 * Copyright 2022 u-blox
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
#ifndef _U_SHORT_RANGE_PBUF_H_
#define _U_SHORT_RANGE_PBUF_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

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
typedef struct uShortRangePbuf_t {
    // pointer to payload
    char *pData;

    // next link
    struct uShortRangePbuf_t *pNext;

    // length of the payload
    size_t len;
} uShortRangePbuf_t;

/**
 * List of pbufs. Each pbuf list corresponds to one EDM payload
 */
typedef struct uShortRangePbufList_t {
    uShortRangePbuf_t *pBufHead;
    uShortRangePbuf_t *pBufTail;
    uShortRangePbuf_t *pBufCurr;
    struct uShortRangePbufList_t *pNext;
    // edm channel of this payload
    int32_t edmChannel;
    // total length of the payload
    size_t totalLen;
} uShortRangePbufList_t;

/** List of pbuf list. Packet list contains multiple
 * EDM payloads. Packet list are mainly used in message based
 * datapath clients like MQTT, UDP
 */
typedef struct uShortRangePktList_t {
    uShortRangePbufList_t *pPbufListHead;
    uShortRangePbufList_t *pPbufListTail;
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

/** Allocate memory for pbuf list from the pbuf list
 * memory pool. Refer to pBufListPool in u_short_range_pbuf.c
 * Memory pool should have been initialized before using this
 * API. Refer to uShortRangeMemPoolInit()
 *
 * @return  Pointer to uShortRangePbufList_t or NULL
 *
 */
uShortRangePbufList_t *pUShortRangeAllocPbufList(void);

/** Allocate fixed size memory from gEdmPayLoadPool memory pool.
 * Refer to gEdmPayLoadPool in u_short_range_pbuf.c
 * Memory pool should have been initialized before using this
 * API. Refer to uShortRangeMemPoolInit()
 *
 * @return  Pointer to the allocated payload or NULL
 *
 */
void *pUShortRangeAllocPayload(void);

/** Insert the payload to the pbuf list.
 *
 * @param pList Pointer to the pbuf list
 * @param pData Pointer to the payload allocated from payload pool.
 *              address of the stack memory should not be passed to this function.
 * @param len   Length of the payload
 */
int32_t uShortRangeInsertPayloadToPbufList(uShortRangePbufList_t *pList, char *pData, size_t len);

/** Move the payload contained in each pbufs in a pbuflist to the given destination buffer.
 *  At the end of move operation, next pbuf position to read will be updated in the pbuflist.
 *
 * @param pList Pointer to the pbuf list.
 * @param pData Pointer to the destination buffer.
 * @param len   Length of the destination buffer.
 * @return      Copied length
 */
size_t uShortRangeMovePayloadFromPbufList(uShortRangePbufList_t *pList, char *pData, size_t len);

/** Put the allocated memory for pbuf, pbuf list, payload in to their
 * free list of respective pool.
 *
 * @param pList Pointer to the pbuf list.
 */
void uShortRangeFreePbufList(uShortRangePbufList_t *pList);

/** Link a new pbuf list to the existing pbuf list.
 *  The pointer allocated for the new pbuf list from the pbuf list pool
 *  will be added to its free list.
 *
 * @param pOldList Pointer to the existing pbuf list.
 * @param pNewList Pointer to the new pbuf list.
 */
void uShortRangeMergePbufList(uShortRangePbufList_t *pOldList, uShortRangePbufList_t *pNewList);

/** Insert a pbuf list to the packet list.
 *
 * @param pPktList  Pointer to the packet list.
 * @param pPbufList Pointer to the pbuf list.
 * @return          zero on success or negative error code.
 */
int32_t uShortRangeInsertPktToPktList(uShortRangePktList_t *pPktList,
                                      uShortRangePbufList_t *pPbufList);

/** Copy the contents of one packet from the packet list in to the given buffer.
 * If the given buffer size cannot accommodate the size of a single packet, Partial
 * data will be copied.
 *
 * @param pPktList    Pointer to the packet list
 * @param pData       Pointer to the destination buffer
 * @param pLen        On entry this should point to length of destination buffer.
 *                    On return this will be updated to copied length.
 * @param pEdmChannel On return EDM channel corresponding to the read packet will be updated.
 * @return            zero on success or negative error code.
 *                    U_ERROR_COMMON_TEMPORARY_FAILURE, if given buffer cannot accommodate
 *                    the entire message.
 */
int32_t uShortRangeReadPktFromPktList(uShortRangePktList_t *pPktList, char *pData, size_t *pLen,
                                      int32_t *pEdmChannel);
#ifdef __cplusplus
}
#endif

#endif //_U_SHORT_RANGE_PBUF_H_

// End of file
