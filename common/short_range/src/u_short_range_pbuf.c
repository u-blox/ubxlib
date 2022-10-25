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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "u_assert.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_error_common.h"
#include "u_short_range_pbuf.h"
#include "u_mempool.h"
#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#ifndef U_SHORT_RANGE_PBUFLIST_COUNT
#define U_SHORT_RANGE_PBUFLIST_COUNT  (32)
#endif

#ifndef U_SHORT_RANGE_PBUF_COUNT
#define U_SHORT_RANGE_PBUF_COUNT      (32)
#endif
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */


/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */
static uMemPoolDesc_t gPBufListPool;
static uMemPoolDesc_t gPBufPool;
/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void freePbuf(uShortRangePbuf_t *pBuf, bool freeWholeChain)
{
    if (freeWholeChain) {
        while (pBuf != NULL) {
            uShortRangePbuf_t *pNext = pBuf->pNext;
            // Basic sanity check - pbuf length should never be longer than pool block size
            U_ASSERT(pBuf->length <= gPBufPool.blockSize);
            uMemPoolFreeMem(&gPBufPool, pBuf);
            pBuf = pNext;
        }
    } else if (pBuf != NULL) {
        // Basic sanity check - pbuf length should never be longer than pool block size
        U_ASSERT(pBuf->length <= gPBufPool.blockSize);
        uMemPoolFreeMem(&gPBufPool, pBuf);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeMemPoolInit(void)
{
    int32_t err;

    err = uMemPoolInit(&gPBufListPool, sizeof(uShortRangePbufList_t),
                       U_SHORT_RANGE_PBUFLIST_COUNT);

    if (err == 0) {

        err = uMemPoolInit(&gPBufPool, sizeof(uShortRangePbuf_t) + U_SHORT_RANGE_EDM_BLK_SIZE,
                           U_SHORT_RANGE_EDM_BLK_COUNT);

        if (err != (int32_t)U_ERROR_COMMON_SUCCESS) {
            uMemPoolDeinit(&gPBufListPool);
        }
    }

    return err;
}

void uShortRangeMemPoolDeInit(void)
{
    uMemPoolDeinit(&gPBufPool);
    uMemPoolDeinit(&gPBufListPool);
}

int32_t uShortRangePbufAlloc(uShortRangePbuf_t **ppBuf)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    *ppBuf = (uShortRangePbuf_t *)uMemPoolAllocMem(&gPBufPool);
    if (*ppBuf != NULL) {
        (*ppBuf)->length = 0;
        (*ppBuf)->pNext = NULL;
        errorCode = gPBufPool.blockSize - sizeof(uShortRangePbuf_t);
    }
    return errorCode;
}

uShortRangePbufList_t *pUShortRangePbufListAlloc(void)
{
    uShortRangePbufList_t *pList;
    pList = (uShortRangePbufList_t *)uMemPoolAllocMem(&gPBufListPool);
    if (pList != NULL) {
        memset(pList, 0, sizeof(uShortRangePbufList_t));
    }
    return pList;
}

void uShortRangePbufListFree(uShortRangePbufList_t *pBufList)
{
    if (pBufList != NULL) {
        freePbuf(pBufList->pBufHead, true);
        pBufList->totalLen = 0;
        uMemPoolFreeMem(&gPBufListPool, pBufList);
    }
}

int32_t uShortRangePbufListAppend(uShortRangePbufList_t *pBufList, uShortRangePbuf_t *pBuf)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pBuf != NULL) && (pBufList != NULL)) {
        if (pBufList->pBufHead == NULL) {
            pBufList->pBufHead = pBuf;
        } else {
            pBufList->pBufTail->pNext = pBuf;
        }
        pBufList->pBufTail = pBuf;
        pBufList->totalLen += pBuf->length;

        err = (int32_t)U_ERROR_COMMON_SUCCESS;
    }

    return err;
}

void uShortRangePbufListMerge(uShortRangePbufList_t *pOldList, uShortRangePbufList_t *pNewList)
{
    if ((pOldList != NULL) &&
        (pNewList != NULL) &&
        (pOldList->totalLen > 0) &&
        (pNewList->totalLen > 0)) {

        if (pOldList->pBufTail != NULL) {
            pOldList->pBufTail->pNext = pNewList->pBufHead;
            pOldList->pBufTail = pNewList->pBufTail;
            pOldList->totalLen += pNewList->totalLen;
        } else {
            *pOldList = *pNewList;
        }

        uMemPoolFreeMem(&gPBufListPool, pNewList);
    }
}


size_t uShortRangePbufListConsumeData(uShortRangePbufList_t *pBufList, char *pData, size_t len)
{
    size_t copiedLen = 0;
    uShortRangePbuf_t *pTemp;
    uShortRangePbuf_t *pNext = NULL;

    if ((pBufList != NULL) && (pData != NULL)) {

        for (pTemp = pBufList->pBufHead; (len != 0 && pTemp != NULL); pTemp = pNext) {
            // Basic sanity check - pbuf length should never be longer than pool block size
            U_ASSERT(pTemp->length <= gPBufPool.blockSize);

            if (pTemp->length <= len) {
                // Copy the data to the given buffer
                memcpy(&pData[copiedLen], &pTemp->data[0], pTemp->length);
                copiedLen += pTemp->length;
                pBufList->totalLen -= pTemp->length;
                len -= pTemp->length;
                pNext = pTemp->pNext;
                // We are done with this pbuf - put it back in the pool
                freePbuf(pTemp, false);
                pBufList->pBufHead = pNext;
                if (pBufList->pBufHead == NULL) {
                    pBufList->pBufTail = NULL;
                }
            } else {
                // Do partial copy
                memcpy(&pData[copiedLen], &pTemp->data[0], len);
                copiedLen += len;
                pBufList->totalLen -= (uint16_t)len;
                pTemp->length -= (uint16_t)len;
                // move the remaining data to start
                memmove(&pTemp->data[0], &pTemp->data[len], pTemp->length);
                len = 0;
            }
        }
    }

    return copiedLen;
}


int32_t uShortRangePktListAppend(uShortRangePktList_t *pPktList,
                                 uShortRangePbufList_t *pPbufList)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pPktList != NULL) &&
        (pPbufList != NULL) &&
        (pPbufList->totalLen > 0)) {

        if (pPktList->pBufListHead == NULL) {
            pPktList->pBufListHead = pPbufList;
        } else {
            pPktList->pBufListTail->pNext = pPbufList;
        }

        pPktList->pBufListTail = pPbufList;
        pPktList->pktCount++;

        err = (int32_t)U_ERROR_COMMON_SUCCESS;
    }

    return err;
}

int32_t uShortRangePktListConsumePacket(uShortRangePktList_t *pPktList, char *pData, size_t *pLen,
                                        int32_t *pEdmChannel)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePbufList_t *pTemp;
    uShortRangePbufList_t **ppTemp;

    if ((pPktList != NULL) &&
        (pData != NULL) &&
        (pPktList->pktCount > 0) &&
        (pLen != NULL)) {

        err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        pTemp = pPktList->pBufListHead;
        ppTemp = &pPktList->pBufListHead;

        if ((pTemp != NULL) && (pTemp->totalLen > 0)) {

            if (pEdmChannel != NULL) {
                *pEdmChannel = pTemp->edmChannel;
            }

            *pLen = uShortRangePbufListConsumeData(pTemp, pData, *pLen);
            err = (int32_t)U_ERROR_COMMON_SUCCESS;

            if (pTemp->totalLen > 0) {
                err = (int32_t)U_ERROR_COMMON_TEMPORARY_FAILURE;
            }

            *ppTemp = pTemp->pNext;
            pPktList->pktCount--;
            uShortRangePbufListFree(pTemp);

            if (pPktList->pktCount == 0) {
                memset((void *)pPktList, 0, sizeof(uShortRangePktList_t));
            }
        }
    }

    return err;
}
// End of file
