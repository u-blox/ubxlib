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

#ifndef _U_MEMPOOL_H_
#define _U_MEMPOOL_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines a memory pool API, used internally by the short range
 * API for efficient EDM transport.  The API functions are thread-safe except for the
 * uMemPoolInit() and uMemPoolDeinit() APIs, which should not be called while any
 * of the other API calls are in progress.
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

typedef struct uMemPoolFreed {
    struct uMemPoolFreed *pNext;
} uMemPoolFreedList_t;

typedef struct {
    uint32_t elementSize;

    // count of number of blocks used
    int32_t currBlkCount;

    //number of blocks configured by user initially
    int32_t cfgedBlkCount;

    //Maximum threshold level, this need to be set to (n * cfedBlkCount)
    int32_t maxBlkCount;

    // pointer to the block
    int32_t currBlkIndex;

    // Free list
    uMemPoolFreedList_t *pFreedList;

    // Pool containing array of referenced to the block.
    // Each block is of elementSize.
    uint8_t **ppBlk;

    uPortMutexHandle_t mutex;
} uMemPoolDesc_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */
/** Initialize memory pool.
 *
 * @param pMemPool      pointer to empty memory pool.
 * @param elementSize   size of each element.
 * @param numOfBlks     Number of blocks each of elementSize.
 *
 * @return              zero on success else negative error code.
 */
int32_t uMemPoolInit(uMemPoolDesc_t *pMemPool, uint32_t elementSize, int32_t numOfBlks);

/** Deinitialize memory pool. This API will free all the references to the block
 *  and the pool itself.
 *
 * @param pMemPool      pointer to the memory pool.
 */
void uMemPoolDeinit(uMemPoolDesc_t *pMemPool);

/** Allocate memory from the given pool.
 *  The allocated memory will be of size configured during uMemPoolInit.
 *
 * @param pMemPool      pointer to the memory pool.
 * @return              pointer to the block.
 */
void *uMemPoolAllocMem(uMemPoolDesc_t *pMemPool);

/** Free the memory allocated from the given pool.
 *  After freeing the memory will be placed in the free list
 *  for the next consumption.
 *
 * @param pMemPool      pointer to the memory pool.
 * @param ptr           pointer to the block that need to be freed.
 *
 */
void uMemPoolFreeMem(uMemPoolDesc_t *pMemPool, void *ptr);

/** Free all the memory references present in the given pool.
 *
 * @param pMemPool      pointer to the memory pool.
 */
void uMemPoolFreeAllMem(uMemPoolDesc_t *pMemPool);

#ifdef __cplusplus
}
#endif

#endif // _U_MEMPOOL_H_

// End of file
