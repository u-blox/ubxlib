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
 * @brief bits of C library that the minimal Zephyr C library
 * doesn't provide and which we didn't think were worth adding to the
 * collection over in the clib directory.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "random/rand32.h"
#include "string.h"

#include "u_assert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef RAND_MAX
#define RAND_MAX 0x7fffffff
#endif

// The define below can be used for detecting heap buffer overflow.
// When enabled a extra field is added in the beginning and end for each
// allocation (hidden from the caller) which is then verified on free()
//#define U_MALLOC_FENCE

#ifdef U_MALLOC_FENCE
# define U_MALLOC_FENCE_HEADER_SIZE  (4+4) // 4 byte magic, 4 byte size of alloc
# define U_MALLOC_FENCE_TRAILER_SIZE 4
# define U_MALLOC_FENCE_HEADER_MAGIC 0xBEEFBEEF
# define U_MALLOC_FENCE_TRAILER_MAGIC 0xCAFECAFE
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_MALLOC_FENCE
// When U_MALLOC_FENCE is enabled this variable will contain
// the current total amount of allocated memory. Could potentially
// be used for finding memory leaks.
atomic_t gTotAllocSize = ATOMIC_INIT(0);
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// The minimal C library provided with Zephyr has a rand() function
// but doesn't call it that.  This just maps one to t'other.
int rand()
{
    uint32_t answer;

    while ((answer = sys_rand32_get()) > RAND_MAX) {}

    return (int) answer;
}

// We don't want to use the libc memory management since it does not use k_malloc
// resulting in two heaps which might waste a lot of memory.
// Therefore we map malloc/free directly to k_malloc/k_free and disable
// libc RAM with CONFIG_MINIMAL_LIBC_MALLOC=n.
void *malloc(size_t size)
{
#ifndef U_MALLOC_FENCE
    char *ptr = k_malloc(size);
#else
    size_t allocSize = size + U_MALLOC_FENCE_HEADER_SIZE + U_MALLOC_FENCE_TRAILER_SIZE;
    char *ptr = k_malloc(allocSize);
    if (ptr != NULL) {
        // Keep track of total amount of allocations
        atomic_add(&gTotAllocSize, allocSize);
        // Add the header (magic + allocation size)
        *((uint32_t *)&ptr[0]) = U_MALLOC_FENCE_HEADER_MAGIC;
        *((uint32_t *)&ptr[4]) = size;
        // Add trailer magic
        ptr += U_MALLOC_FENCE_HEADER_SIZE;
        *((uint32_t *)&ptr[size]) = U_MALLOC_FENCE_TRAILER_MAGIC;
    }
#endif
    return ptr;
}

void free(void *p)
{
    char *ptr = p;

#ifdef U_MALLOC_FENCE
    if (ptr != NULL) {
        ptr -= U_MALLOC_FENCE_HEADER_SIZE;
        // Check header magic
        U_ASSERT(*((uint32_t *)&ptr[0]) == U_MALLOC_FENCE_HEADER_MAGIC);
        uint32_t *pSize = (uint32_t *)&ptr[4];
        uint32_t trailerOffset = *pSize + U_MALLOC_FENCE_HEADER_SIZE;
        // Check trailer magic
        U_ASSERT(*((uint32_t *)&ptr[trailerOffset]) == U_MALLOC_FENCE_TRAILER_MAGIC);
        // Keep track of total amount of allocations
        size_t allocSize = *pSize + U_MALLOC_FENCE_HEADER_SIZE + U_MALLOC_FENCE_TRAILER_SIZE;
        atomic_sub(&gTotAllocSize, allocSize);
        // Clear the memory region to detect double free etc
        memset(ptr, 0xFF, *pSize);
    }
#endif

    k_free(ptr);
}

// End of file
