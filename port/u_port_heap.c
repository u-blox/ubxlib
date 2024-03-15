/*
 * Copyright 2019-2024 u-blox
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

/** @file
 * @brief Default implementation of pUPortMalloc() / uPortFree() and
 * uPortHeapAllocCount().
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stdlib.h"    // malloc()/free().
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), memcpy()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "stdint.h"      // int32_t etc.
#include "u_compiler.h"  // U_WEAK

#include "u_error_common.h"
#include "u_assert.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_HEAP_GUARD
/** The uint32_t guard to put before and after each heap block, allowing
 * us to check for overruns ("DEADBEEF", readable in a hex dump on
 * a little-endian MCU, which they pretty much all are these days).
 */
# define U_PORT_HEAP_GUARD 0xefbeaddeUL
#endif

/** The size of #U_PORT_HEAP_GUARD; must be 4.
 */
#define U_PORT_HEAP_GUARD_SIZE sizeof(uint32_t)

/** The size of uPortHeapBlock_t, _without_ any packing on the end.
 */
#define U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING ((sizeof(void *) * 2) + (sizeof(int32_t) * 3))

#ifndef U_PORT_HEAP_BUFFER_OVERRUN_MARKER
/** The string to prefix a buffer overrun with.
 */
# define U_PORT_HEAP_BUFFER_OVERRUN_MARKER " *** BUFFER OVERRUN *** "
#endif

#ifndef U_PORT_HEAP_BUFFER_UNDERRUN_MARKER
/** The string to prefix a buffer underrun with.
 */
# define U_PORT_HEAP_BUFFER_UNDERRUN_MARKER " *** BUFFER UNDERRUN *** "
#endif

/** Local version of the lock helper, since this can't necessarily
 * use the normal one.
 */
#define U_PORT_HEAP_MUTEX_LOCK(x)      { gpMutexLock ? gpMutexLock(x) : uPortMutexLock(x)

/** Local version of the unlock helper, since this can't necessarily
 * use the normal one.
 */
#define U_PORT_HEAP_MUTEX_UNLOCK(x)    } gpMutexUnlock ? gpMutexUnlock(x) : uPortMutexUnlock(x)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to track a memory block on the heap.  When
 * U_CFG_HEAP_MONITOR is defined and a heap allocation is done
 * the allocation will be increased in size to include this at the
 * start, then a guard of length #U_PORT_HEAP_GUARD_SIZE, then
 * the actual callers memory block and finally another guard of
 * length #U_PORT_HEAP_GUARD_SIZE on the end.
 *
 * IMPORTANT: this structure must work out to be a multiple of
 * the worst-case pointer size of any supported platform (currently
 * 64-bit for LINUX64) minus #U_PORT_HEAP_GUARD_SIZE, otherwise the
 * memory passed back to the user will not be properly aligned;
 * the structure is ordered as it is, with the smallest members last,
 * in order to ensure this is the case.
 *
 * IF you change this structure make sure that
 * #U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING is updated to match
 * and make sure to use #U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING
 * and not sizeof(uPortHeapBlock_t).
 */
typedef struct uPortHeapBlock_t {
    struct uPortHeapBlock_t *pNext;
    const char *pFile;
    int32_t line;
    int32_t size;
    int32_t timeMilliseconds;
} uPortHeapBlock_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Variable to keep track of the total number of heap allocations
 * outstanding.
 */
static int32_t gHeapAllocCount = 0;

/** Variable to keep track of the total number of perpetual heap
 * allocations.
 */
static int32_t gHeapPerpetualAllocCount = 0;

#ifdef U_CFG_HEAP_MONITOR
/** Root of linked list of blocks on the heap.
 */
static uPortHeapBlock_t *gpHeapBlockList = NULL;

/** Mutex to protect the linked list.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Hook for platform-specific mutex lock function, if required
 * (e.g. the Linux port needs this).
 */
static int32_t (*gpMutexLock) (const uPortMutexHandle_t) = NULL;

/** Hook for platform-specific mutex unlock function, if required
 * (e.g. the Linux port needs this).
 */
static int32_t (*gpMutexUnlock) (const uPortMutexHandle_t) = NULL;

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_HEAP_MONITOR
static void printBlock(const char *pPrefix, const uPortHeapBlock_t *pBlock)
{
    if (pPrefix == NULL) {
        pPrefix = "";
    }
    if (pBlock != NULL) {
        uPortLog("%sBLOCK address %p %6d byte(s) allocated by %s:%d @ %d.\n",
                 pPrefix,
                 pBlock + sizeof(uPortHeapBlock_t) + U_PORT_HEAP_GUARD_SIZE,
                 pBlock->size, pBlock->pFile, pBlock->line, pBlock->timeMilliseconds);
    }
}

static void printMemory(const char *pMemory, size_t size)
{
    for (size_t x = 0; x < size; x++, pMemory++) {
        if (!isprint((int32_t) *pMemory)) {
            uPortLog("[%02x]", (const unsigned char) *pMemory);
        } else {
            uPortLog("%c", *pMemory);
        }
    }
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_HEAP_MONITOR
// For heap monitoring, pUPortMalloc() becomes static _pUPortMalloc()
// which we call internally from pUPortMallocMonitor().
static void *_pUPortMalloc(size_t sizeBytes)
#else
U_WEAK void *pUPortMalloc(size_t sizeBytes)
#endif
{
    void *pMalloc = malloc(sizeBytes);
    if (pMalloc != NULL) {
        gHeapAllocCount++;
    }
    return pMalloc;
}

#ifdef U_CFG_HEAP_MONITOR
// The malloc call that replaces pUPortMalloc() when U_CFG_HEAP_MONITOR
// is defined,
void *pUPortMallocMonitor(size_t sizeBytes, const char *pFile,
                          int32_t line)
{
    void *pMemory = NULL;
    uPortHeapBlock_t *pBlock;
    uPortHeapBlock_t *pBlockTmp;
    char *pTmp;
    size_t blockSizeBytes;
    uint32_t heapGuard = U_PORT_HEAP_GUARD;

    if (gMutex != NULL) {
        // Allocate enough memory for what the caller wanted,
        // plus our monitoring structure, plus two guards
        blockSizeBytes = sizeBytes + U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING +
                         (U_PORT_HEAP_GUARD_SIZE * 2);
        pBlock = (uPortHeapBlock_t *) _pUPortMalloc(blockSizeBytes);
        if (pBlock != NULL) {
            // Populate the structure
            memset(pBlock, 0, sizeof(*pBlock));
            pBlock->pFile = pFile;
            pBlock->line = line;
            pBlock->size = sizeBytes;
            pBlock->timeMilliseconds = uPortGetTickTimeMs();
            pTmp = ((char *) pBlock) + U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING;
            // Add the opening guard after the block
            memcpy(pTmp, &heapGuard, U_PORT_HEAP_GUARD_SIZE);
            pTmp += U_PORT_HEAP_GUARD_SIZE;
            // This is what we'll return to the caller
            pMemory = pTmp;
            // Add the closing guard on the very end
            pTmp += sizeBytes;
            memcpy(pTmp, &heapGuard, U_PORT_HEAP_GUARD_SIZE);

            U_PORT_HEAP_MUTEX_LOCK(gMutex);

            // Add the block to the list
            pBlockTmp = gpHeapBlockList;
            gpHeapBlockList = pBlock;
            pBlock->pNext = pBlockTmp;

            U_PORT_HEAP_MUTEX_UNLOCK(gMutex);
        }
    }

    return pMemory;
}
#endif

U_WEAK void uPortFree(void *pMemory)
{
#ifdef U_CFG_HEAP_MONITOR
    uPortHeapBlock_t *pBlock;
    uPortHeapBlock_t *pBlockTmp1;
    uPortHeapBlock_t *pBlockTmp2 = NULL;
    char *pTmp;
    const char *pMarker = NULL;
    uint32_t heapGuard = U_PORT_HEAP_GUARD;

    if ((pMemory != NULL) && (gMutex != NULL)) {
        // Wind back to the start of the block
        pBlock = (uPortHeapBlock_t *) (((char *) pMemory) - (U_PORT_HEAP_STRUCTURE_SIZE_NO_END_PACKING +
                                                             U_PORT_HEAP_GUARD_SIZE));
        // Check the guards
        pTmp = ((char *) pMemory) - U_PORT_HEAP_GUARD_SIZE;
        if (memcmp(pTmp, &heapGuard, U_PORT_HEAP_GUARD_SIZE) != 0) {
            pMarker = U_PORT_HEAP_BUFFER_UNDERRUN_MARKER;
            uPortLog("%sexpected: ", pMarker);
            printMemory((char *) &heapGuard, U_PORT_HEAP_GUARD_SIZE);
            uPortLog(", got: ");
            printMemory(pTmp, U_PORT_HEAP_GUARD_SIZE);
            uPortLog("\n");
        }
        pTmp += U_PORT_HEAP_GUARD_SIZE + pBlock->size;
        if (memcmp(pTmp, &heapGuard, U_PORT_HEAP_GUARD_SIZE) != 0) {
            pMarker = U_PORT_HEAP_BUFFER_OVERRUN_MARKER;
            uPortLog("%sexpected: ", pMarker);
            printMemory((char *) &heapGuard, U_PORT_HEAP_GUARD_SIZE);
            uPortLog(", got: ");
            printMemory(pTmp, U_PORT_HEAP_GUARD_SIZE);
            uPortLog("\n");
        }
        if (pMarker != NULL) {
            printBlock(pMarker, pBlock);
        }

        U_PORT_HEAP_MUTEX_LOCK(gMutex);

        // Remove the block from the list
        pBlockTmp1 = gpHeapBlockList;
        while ((pBlockTmp1 != NULL) && (pBlockTmp1 != pBlock)) {
            pBlockTmp2 = pBlockTmp1;
            pBlockTmp1 = pBlockTmp1->pNext;
        }
        if (pBlockTmp1 != NULL) {
            if (pBlockTmp2 == NULL) {
                // Must be at head
                gpHeapBlockList = pBlockTmp1->pNext;
            } else {
                pBlockTmp2->pNext = pBlockTmp1->pNext;
            }
        }

        U_PORT_HEAP_MUTEX_UNLOCK(gMutex);

        U_ASSERT(pMarker == NULL);
        // pBlock is what we need to free
        pMemory = pBlock;
    }
#endif

    if (pMemory != NULL) {
        gHeapAllocCount--;
    }
    free(pMemory);
}

U_WEAK int32_t uPortHeapAllocCount()
{
    return gHeapAllocCount;
}

U_WEAK void uPortHeapPerpetualAllocAdd()
{
    gHeapPerpetualAllocCount++;
}

U_WEAK int32_t uPortHeapPerpetualAllocCount()
{
    return gHeapPerpetualAllocCount;
}

// Print out the contents of the heap.
int32_t uPortHeapDump(const char *pPrefix)
{
    int32_t x = 0;

#ifdef U_CFG_HEAP_MONITOR
    uPortHeapBlock_t *pBlock = gpHeapBlockList;
    while (pBlock != NULL) {
        printBlock(pPrefix, pBlock);
        pBlock = pBlock->pNext;
        x++;
    }
    if (pPrefix == NULL) {
        pPrefix = "";
    }
    uPortLog("%s%d block(s).\n", pPrefix, x);
#else
    (void) pPrefix;
#endif

    return x;
}

// Initialise heap monitoring.
int32_t uPortHeapMonitorInit(int32_t (*pMutexCreate) (uPortMutexHandle_t *),
                             int32_t (*pMutexLock) (const uPortMutexHandle_t),
                             int32_t (*pMutexUnlock) (const uPortMutexHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

#ifdef U_CFG_HEAP_MONITOR
    if (gMutex == NULL) {
        if (pMutexCreate != NULL) {
            errorCode = pMutexCreate(&gMutex);
        } else {
            errorCode = uPortMutexCreate(&gMutex);
        }
        if (errorCode == 0) {
            if (pMutexCreate == NULL) {
                // Mark the call to uPortMutexCreate() as perpetual for accounting purposes
                uPortOsResourcePerpetualAdd(U_PORT_OS_RESOURCE_TYPE_MUTEX);
            }
            gpMutexLock = pMutexLock;
            gpMutexUnlock = pMutexUnlock;
        }
    }
#endif

    return errorCode;
}

// End of file
