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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
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
static uMemPoolDesc_t pBufListPool;
static uMemPoolDesc_t pBufPool;
static uMemPoolDesc_t gEdmPayLoadPool;
/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeMemPoolInit(void)
{
    int32_t err;

    err = uMemPoolInit(&pBufListPool, sizeof(uShortRangePbufList_t), U_SHORT_RANGE_PBUFLIST_COUNT);

    if (err == 0) {

        err = uMemPoolInit(&pBufPool, sizeof(uShortRangePbuf_t), U_SHORT_RANGE_PBUF_COUNT);

        if (err == 0) {
            err = uMemPoolInit(&gEdmPayLoadPool, U_SHORT_RANGE_EDM_BLK_SIZE, U_SHORT_RANGE_EDM_BLK_COUNT);
        }

        if (err != (int32_t)U_ERROR_COMMON_SUCCESS) {
            uMemPoolDeinit(&pBufListPool);
            uMemPoolDeinit(&pBufPool);
        }
    }

    return err;
}

void uShortRangeMemPoolDeInit(void)
{
    uMemPoolDeinit(&pBufPool);
    uMemPoolDeinit(&pBufListPool);
    uMemPoolDeinit(&gEdmPayLoadPool);
    return;
}


uShortRangePbufList_t *pUShortRangeAllocPbufList(void)
{
    uShortRangePbufList_t *pList;
    pList = (uShortRangePbufList_t *)uMemPoolAllocMem(&pBufListPool);
    return pList;
}

void *pUShortRangeAllocPayload(void)
{
    void *pPayload;
    pPayload = uMemPoolAllocMem(&gEdmPayLoadPool);
    return pPayload;
}

void uShortRangeFreePbufList(uShortRangePbufList_t *pList)
{
    uShortRangePbuf_t *pTemp;
    uShortRangePbuf_t *pNext;

    if (pList != NULL) {

        for (pTemp = pList->pBufHead; pTemp != NULL; pTemp = pNext) {

            pNext = pTemp->pNext;
            uMemPoolFreeMem(&gEdmPayLoadPool, pTemp->pData);
            uMemPoolFreeMem(&pBufPool, pTemp);
        }
        pList->totalLen = 0;
        uMemPoolFreeMem(&pBufListPool, pList);

    }
}

int32_t uShortRangeInsertPayloadToPbufList(uShortRangePbufList_t *pList, char *pData, size_t len)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePbuf_t *pBuf;

    pBuf = (uShortRangePbuf_t *)uMemPoolAllocMem(&pBufPool);

    if ((pBuf != NULL) && (pList != NULL)) {

        pBuf->pData = pData;
        pBuf->len = len;

        if (pList->pBufHead == NULL) {
            pList->pBufHead = pBuf;
            pList->pBufCurr = pBuf;
        } else {
            pList->pBufTail->pNext = pBuf;
        }
        pList->pBufTail = pBuf;
        pList->totalLen += len;

        err = (int32_t)U_ERROR_COMMON_SUCCESS;
    }

    return err;
}

void uShortRangeMergePbufList(uShortRangePbufList_t *pOldList, uShortRangePbufList_t *pNewList)
{
    if ((pOldList != NULL) &&
        (pNewList != NULL) &&
        (pOldList->totalLen > 0) &&
        (pNewList->totalLen > 0)) {

        pOldList->pBufTail->pNext = pNewList->pBufHead;
        pOldList->pBufTail = pNewList->pBufTail;
        pOldList->totalLen += pNewList->totalLen;

        uMemPoolFreeMem(&pBufListPool, pNewList);
    }
}


size_t uShortRangeMovePayloadFromPbufList(uShortRangePbufList_t *pList, char *pData, size_t len)
{
    size_t copiedLen = 0;
    uShortRangePbuf_t *pTemp;
    uShortRangePbuf_t *pNext = NULL;

    if ((pList != NULL) && (pData != NULL)) {

        for (pTemp = pList->pBufCurr; (len != 0 && pTemp != NULL); pTemp = pNext) {

            if (pTemp->pData != NULL) {

                if (pTemp->len < len) {

                    // Copy the data to the given buffer
                    memcpy(&pData[copiedLen], pTemp->pData, pTemp->len);
                    copiedLen += pTemp->len;
                    pList->totalLen -= pTemp->len;
                    len -= pTemp->len;
                    pNext = pTemp->pNext;
                    pList->pBufCurr = pNext;
                } else {

                    // Do partial copy
                    memcpy(&pData[copiedLen], pTemp->pData, len);
                    copiedLen += len;
                    pList->totalLen -= len;
                    pTemp->len -= len;
                    // move the remaining data to start
                    memcpy(pTemp->pData, &pTemp->pData[len], pTemp->len);
                    len = 0;
                }
            }
        }
    }

    return copiedLen;
}


int32_t uShortRangeInsertPktToPktList(uShortRangePktList_t *pPktList,
                                      uShortRangePbufList_t *pPbufList)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pPktList != NULL) &&
        (pPbufList != NULL) &&
        (pPbufList->totalLen > 0)) {

        if (pPktList->pPbufListHead == NULL) {
            pPktList->pPbufListHead = pPbufList;
        } else {
            pPktList->pPbufListTail->pNext = pPbufList;
        }

        pPktList->pPbufListTail = pPbufList;
        pPktList->pktCount++;

        err = (int32_t)U_ERROR_COMMON_SUCCESS;
    }

    return err;
}

int32_t uShortRangeReadPktFromPktList(uShortRangePktList_t *pPktList, char *pData, size_t *pLen,
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
        pTemp = pPktList->pPbufListHead;
        ppTemp = &pPktList->pPbufListHead;

        if ((pTemp != NULL) && (pTemp->totalLen > 0)) {

            if (pEdmChannel != NULL) {
                *pEdmChannel = pTemp->edmChannel;
            }

            *pLen = uShortRangeMovePayloadFromPbufList(pTemp, pData, *pLen);
            err = (int32_t)U_ERROR_COMMON_SUCCESS;

            if (pTemp->totalLen > 0) {
                err = (int32_t)U_ERROR_COMMON_TEMPORARY_FAILURE;
            }

            *ppTemp = pTemp->pNext;
            pPktList->pktCount--;
            uShortRangeFreePbufList(pTemp);

            if (pPktList->pktCount == 0) {
                memset((void *)pPktList, 0, sizeof(uShortRangePktList_t));
            }
        }
    }

    return err;
}
// End of file
