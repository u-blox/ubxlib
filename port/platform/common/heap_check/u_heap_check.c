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

/** @file
 * @brief Functions for heap checking, assuming GCC-compatible linker.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "malloc.h"    // For mallinfo
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

// The platform must provide this:
extern int uPortInternalGetSbrkFreeBytes();

// These are provided by the linker.
extern void *__real_malloc(size_t size);
extern void *__real__malloc_r(struct _reent *reent, size_t size);
extern void *__real_calloc(size_t count, size_t size);
extern void *__real__calloc_r(struct _reent *reent, size_t count, size_t size);
extern void *__real_realloc(void *pMem, size_t size);
extern void *__real__realloc_r(struct _reent *reent, void *pMem, size_t size);

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The total heap available.
 */
static size_t gHeapSizeBytes = 0;

/** The maximum amount of heap malloc()ed.
 */
static size_t gHeapUsedMaxBytes = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MALLOC WRAPPERS
 * To use these, add linker option:
 * -Wl,--wrap=malloc -Wl,--wrap=_malloc_r
 * -Wl,--wrap=calloc -Wl,--wrap=_calloc_r
 * -Wl,--wrap=realloc -Wl,--wrap=_realloc_r
 * -------------------------------------------------------------- */

// Wrapper for malloc() to allow us to track max heap usage.
void *__wrap_malloc(size_t sizeBytes)
{
    void *pMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to malloc()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks + uPortInternalGetSbrkFreeBytes();
    }

    pMem = __real_malloc(sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pMem;
}

// Wrapper for _malloc_r() to allow us to track max heap usage.
void *__wrap__malloc_r(void *pReent, size_t sizeBytes)
{
    void *pMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to _malloc_r()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks  + uPortInternalGetSbrkFreeBytes();
    }

    pMem = __real__malloc_r(pReent, sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pMem;
}

// Wrapper for calloc() to allow us to track max heap usage.
void *__wrap_calloc(size_t count, size_t sizeBytes)
{
    void *pMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to malloc()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks + uPortInternalGetSbrkFreeBytes();
    }

    pMem = __real_calloc(count, sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pMem;
}

// Wrapper for _calloc_r() to allow us to track max heap usage.
void *__wrap__calloc_r(void *pReent, size_t count, size_t sizeBytes)
{
    void *pMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to _malloc_r()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks  + uPortInternalGetSbrkFreeBytes();
    }

    pMem = __real__calloc_r(pReent, count, sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pMem;
}

// Wrapper for realloc() to allow us to track max heap usage.
void *__wrap_realloc(void *pMem, size_t sizeBytes)
{
    void *pReallocMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to malloc()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks + uPortInternalGetSbrkFreeBytes();
    }

    pReallocMem = __real_realloc(pMem, sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pReallocMem;
}

// Wrapper for realloc_r() to allow us to track max heap usage.
void *__wrap__realloc_r(void *pReent, void *pMem, size_t sizeBytes)
{
    void *pReallocMem;
    struct mallinfo mallInfo;

    // We don't know what the heap extent is
    // so find it out on the first call to malloc()
    if (gHeapSizeBytes == 0) {
        mallInfo = mallinfo();
        // Free memory is the amount in the newlib
        // pools plus any it has not claimed
        // yet from sbrk()
        gHeapSizeBytes = mallInfo.fordblks + uPortInternalGetSbrkFreeBytes();
    }

    pReallocMem = __real__realloc_r(pReent, pMem, sizeBytes);

    mallInfo = mallinfo();
    if (mallInfo.uordblks > gHeapUsedMaxBytes) {
        gHeapUsedMaxBytes = mallInfo.uordblks;
    }

    return pReallocMem;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the minimum heap free, ever.
size_t uHeapCheckGetMinFree(void)
{
    size_t minFree = 0;

    if (gHeapSizeBytes > 0) {
        if (gHeapUsedMaxBytes < gHeapSizeBytes) {
            minFree = gHeapSizeBytes - gHeapUsedMaxBytes;
        }
    }

    return minFree;
}

// End of file
