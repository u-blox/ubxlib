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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of memory pool
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "string.h"
#include "stdlib.h"    // malloc() and free()
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_mempool.h"
#include "u_error_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void freeAllMem(uMemPoolDesc_t *pMemPool)
{
    pMemPool->currBlkIndex = -1;
    pMemPool->pFreedList = NULL;
}

// realloc not available in zephyr
static void *resizePool(uMemPoolDesc_t *pMemPool, uint32_t blkCount)
{
    void *pNewBlk;

    pNewBlk = malloc(blkCount * sizeof(uint8_t *));

    if (pNewBlk != NULL) {
        //zeroize the contents
        memset(pNewBlk, 0, blkCount * sizeof(uint8_t *));
        //Copy the contents from old pool to new pool
        memcpy(pNewBlk, pMemPool->ppBlk, pMemPool->cfgedBlkCount * sizeof(uint8_t *));
        // free the old pool
        free(pMemPool->ppBlk);
    }

    return pNewBlk;
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int32_t uMemPoolInit(uMemPoolDesc_t *pMemPool, uint32_t elementSize, int32_t blkCount)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (pMemPool != NULL) {

        err = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        memset(pMemPool, 0, sizeof(uMemPoolDesc_t));
        freeAllMem(pMemPool);
        pMemPool->elementSize = MAX(elementSize, sizeof(uint32_t));
        pMemPool->currBlkCount = blkCount;
        pMemPool->cfgedBlkCount = blkCount;
        pMemPool->maxBlkCount = pMemPool->cfgedBlkCount * 2;
        pMemPool->ppBlk = (uint8_t **)malloc(sizeof(uint8_t *) * blkCount);

        if (pMemPool->ppBlk != NULL) {
            for (int32_t i = 0; i < blkCount; i++) {
                pMemPool->ppBlk[i] = NULL;
            }
            uPortMutexCreate(&pMemPool->mutex);
            uPortLog("U_MEM_POOL: Allocated pool %p\n", pMemPool->ppBlk);
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
        }
    }

    return err;
}

void uMemPoolDeinit(uMemPoolDesc_t *pMemPool)
{
    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {

        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        // run through the pool to free
        for (int32_t i = 0; ((i < pMemPool->currBlkCount) && pMemPool->ppBlk[i]); ) {
            uPortLog("U_MEM_POOL: Freeing block address %p\n", pMemPool->ppBlk[i]);
            free(pMemPool->ppBlk[i]);
            i += pMemPool->cfgedBlkCount;
        }
        uPortLog("U_MEM_POOL: Freeing pool address %p\n", pMemPool->ppBlk);
        free(pMemPool->ppBlk);
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);

        uPortMutexDelete(pMemPool->mutex);
        memset(pMemPool, 0, sizeof(uMemPoolDesc_t));
    }
}

void *uMemPoolAllocMem(uMemPoolDesc_t *pMemPool)
{
    void *pAllocMem = NULL;
    int32_t i;
    uint8_t *pBuff;
    bool noMemAvailable = false;

    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {

        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        // Grab the free memory available in the free list
        if (pMemPool->pFreedList) {
            pAllocMem = pMemPool->pFreedList;
            pMemPool->pFreedList = pMemPool->pFreedList->pNext;
        } else {
            pMemPool->currBlkIndex++;
            // Check if all the blocks are consumed
            // in this case, pool is reallocated to store more blocks.
            if (pMemPool->currBlkIndex == pMemPool->currBlkCount) {

                //increment the current block count in the granularity
                // of configured factor
                pMemPool->currBlkCount += pMemPool->cfgedBlkCount;

                if (pMemPool->currBlkCount <= pMemPool->maxBlkCount) {

                    // reallocate the pool to include more blocks
                    // where each block is of elementSize
                    pMemPool->ppBlk = (uint8_t **)resizePool(pMemPool,
                                                             (uint32_t)(sizeof(uint8_t *) * pMemPool->currBlkCount));
                    if (pMemPool->ppBlk == NULL) {
                        noMemAvailable = true;
                    }
                    uPortLog("U_MEM_POOL: Resized pool %p to allot more blocks\n", pMemPool->ppBlk);
                } else {
                    noMemAvailable = true;
                }
            }

            if (noMemAvailable == false) {
                if (pMemPool->ppBlk[pMemPool->currBlkIndex] == NULL) {

                    int32_t blkCount = pMemPool->currBlkCount - pMemPool->currBlkIndex;

                    // Allocate memory for the total number of blocks
                    // in one shot and then subdivide it
                    //lint -e{647} suppress suspicious truncation
                    pBuff = (uint8_t *)malloc(blkCount * pMemPool->elementSize);

                    for (i = 0; i < pMemPool->currBlkCount && pBuff; i++) {
                        //lint -e{679} suppress suspicious truncation in arithmetic expression combining with pointer.
                        pMemPool->ppBlk[pMemPool->currBlkIndex + i] = pBuff + (i * pMemPool->elementSize);
                    }
                }
                pAllocMem = pMemPool->ppBlk[pMemPool->currBlkIndex];
            } else {
                // if we reach here, subsequent alloc will succeed
                // only if freed memory is present in the free list.
                pMemPool->currBlkCount -= pMemPool->cfgedBlkCount;
                pMemPool->currBlkIndex--;
            }
        }

        if (noMemAvailable == false) {
            // zeroize the contents
            //lint -e{668} suppress Possibly passing a. null pointer to function memset
            memset(pAllocMem, 0, pMemPool->elementSize);
        }

        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }
    //lint -e{429} suppress pBuff (line 148) not been freed
    return pAllocMem;
}

void uMemPoolFreeMem(uMemPoolDesc_t *pMemPool, void *pMem)
{
    void *pMemNext;

    if ((pMemPool != NULL) && (pMem != NULL) && (pMemPool->mutex != NULL)) {
        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        // Add the freed memory reference before the head
        pMemNext = pMemPool->pFreedList;
        pMemPool->pFreedList = (uMemPoolFreedList_t *)pMem;
        pMemPool->pFreedList->pNext = (uMemPoolFreedList_t *)pMemNext;
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }
}

void uMemPoolFreeAllMem(uMemPoolDesc_t *pMemPool)
{
    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {
        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        freeAllMem(pMemPool);
        for (int32_t i = 0; (i < pMemPool->currBlkCount && (pMemPool->ppBlk != NULL)); i++) {
            memset(pMemPool->ppBlk[i], 0, pMemPool->elementSize);
        }
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }
}

// End of file
