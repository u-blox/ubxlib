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
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_assert.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_mempool.h"
#include "u_error_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Configuration value if a "fence" should be added between
// the mempool buffer chunks for detecting buffer overflows.
// Since this only adds 2 bytes per chunk it is enabled by default.
#ifndef U_MEMPOOL_USE_BUF_FENCE
# define U_MEMPOOL_USE_BUF_FENCE 1
#endif

#if U_MEMPOOL_USE_BUF_FENCE
# define U_REAL_BLOCK_SIZE(userBlockSize) \
    (userBlockSize + sizeof(uint16_t))
#else
# define U_REAL_BLOCK_SIZE(userBlockSize) (userBlockSize)
#endif

#define U_BUFFER_SIZE(pMemPool) \
    (U_REAL_BLOCK_SIZE(pMemPool->blockSize) * pMemPool->totalBlockCount)

#define U_FENCE_MAGIC 0xBEEF

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct uMemPoolFree {
    struct uMemPoolFree *pNext;
} uMemPoolFreeList_t;

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void initFreeList(uMemPoolDesc_t *pMemPool)
{
    // Initialize the freed linked list
    U_ASSERT(pMemPool->pBuffer != NULL);
    uMemPoolFreeList_t *pLastFree = (uMemPoolFreeList_t *)pMemPool->pBuffer;
    pMemPool->pFreeList = pLastFree;
    for (int i = 1; i < pMemPool->totalBlockCount; i++) {
        uMemPoolFreeList_t *pFree;
        size_t realBlockSize = U_REAL_BLOCK_SIZE(pMemPool->blockSize);
        pFree = (uMemPoolFreeList_t *)&pMemPool->pBuffer[i * realBlockSize];
        pLastFree->pNext = pFree;
        pLastFree = pFree;
    }
    pLastFree->pNext = NULL;
    pMemPool->usedBlockCount = 0;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uMemPoolInit(uMemPoolDesc_t *pMemPool, uint32_t blockSize, int32_t blkCount)
{
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pMemPool != NULL) && (blockSize >= sizeof(uMemPoolFreeList_t))) {
        memset(pMemPool, 0, sizeof(uMemPoolDesc_t));
        pMemPool->blockSize = blockSize;
        pMemPool->usedBlockCount = 0;
        pMemPool->totalBlockCount = blkCount;

        err = uPortMutexCreate(&pMemPool->mutex);
    }

    return err;
}

void uMemPoolDeinit(uMemPoolDesc_t *pMemPool)
{
    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {

        U_PORT_MUTEX_LOCK(pMemPool->mutex);

        if (pMemPool->pBuffer != NULL) {
            uPortLog("U_MEM_POOL: Freeing buffer: %p\n", pMemPool->pBuffer);
            uPortFree(pMemPool->pBuffer);
        }
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);

        uPortMutexDelete(pMemPool->mutex);
        memset(pMemPool, 0, sizeof(uMemPoolDesc_t));
    }
}

void *uMemPoolAllocMem(uMemPoolDesc_t *pMemPool)
{
    void *pAllocMem = NULL;

    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {

        U_PORT_MUTEX_LOCK(pMemPool->mutex);

        // If this is the first call to uMemPoolAllocMem we need to
        // allocate the buffer
        if (pMemPool->pBuffer == NULL) {
            pMemPool->pBuffer = (uint8_t *)pUPortMalloc(U_BUFFER_SIZE(pMemPool));
            uPortLog("U_MEM_POOL: Allocated buffer %p\n", pMemPool->pBuffer);
            if (pMemPool->pBuffer != NULL) {
                initFreeList(pMemPool);
            }
        }

        // Grab the free memory available in the free list
        if (pMemPool->pFreeList) {
            pAllocMem = pMemPool->pFreeList;
            pMemPool->pFreeList = pMemPool->pFreeList->pNext;
            pMemPool->usedBlockCount++;
        }

#if U_MEMPOOL_USE_BUF_FENCE
        // Add the memory fence right after the user allocation
        if (pAllocMem != NULL) {
            uint8_t *pDataPtr = (uint8_t *)pAllocMem;
            uint16_t *pMagic = (uint16_t *)&pDataPtr[pMemPool->blockSize];
            *pMagic = U_FENCE_MAGIC;
        }
#endif

        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }

    return pAllocMem;
}

void uMemPoolFreeMem(uMemPoolDesc_t *pMemPool, void *pMem)
{
    void *pMemNext;

    if ((pMemPool != NULL) && (pMem != NULL) && (pMemPool->mutex != NULL)) {
        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        // Make sure the memory segment is within our buffer
        uPortLog("pMem: %08x\n", pMem);
        U_ASSERT((uint8_t *)pMem >= pMemPool->pBuffer);
        U_ASSERT((uint8_t *)pMem < (pMemPool->pBuffer + U_BUFFER_SIZE(pMemPool)));

#if U_MEMPOOL_USE_BUF_FENCE
        // Validate the magic number
        uint8_t *pDataPtr = (uint8_t *)pMem;
        uint16_t *pMagic = (uint16_t *)&pDataPtr[pMemPool->blockSize];
        U_ASSERT(*pMagic == U_FENCE_MAGIC);
        // Invalidate
        *pMagic = 0;
#endif

        // Add the freed memory reference before the head
        pMemNext = pMemPool->pFreeList;
        pMemPool->pFreeList = (uMemPoolFreeList_t *)pMem;
        pMemPool->pFreeList->pNext = (uMemPoolFreeList_t *)pMemNext;
        pMemPool->usedBlockCount--;
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }
}

void uMemPoolFreeAllMem(uMemPoolDesc_t *pMemPool)
{
    if ((pMemPool != NULL) && (pMemPool->mutex != NULL)) {
        U_PORT_MUTEX_LOCK(pMemPool->mutex);
        initFreeList(pMemPool);
        U_PORT_MUTEX_UNLOCK(pMemPool->mutex);
    }
}

// End of file
