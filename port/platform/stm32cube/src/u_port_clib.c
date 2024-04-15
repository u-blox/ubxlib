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
 * @brief maps malloc() and free() to ThreadX memory pools.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#ifndef U_PORT_STM32_CMSIS_ON_FREERTOS

#include "tx_api.h"

// The global pointer required by ThreadX when using dynamic memory
// which points to the start of the memory region it may use.
extern char *_tx_initialize_unused_memory;

// Variable provided by the linker file which is used by the startup
// code to initialise _tx_initialize_unused_memory.
extern char *__RAM_segment_used_end__;

// Variable in the ST-provided Threadx CMSIS adaptation layer
// (see their cmsis_os2.c) which identifies the heap memory pool.
extern TX_BYTE_POOL HeapBytePool;

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Map malloc
void *malloc(size_t size)
{
    void *pMemory = NULL;

    // Check if the ST-provided CMSIS layer has configured the heap
    // memory yet
    if (_tx_initialize_unused_memory != __RAM_segment_used_end__) {
        // It has: have some memory
        tx_byte_allocate(&HeapBytePool, &pMemory, size, TX_NO_WAIT);
    }

    return pMemory;
}

void free(void *pMemory)
{
    if (pMemory != NULL) {
        tx_byte_release(pMemory);
    }
}

#endif // #ifndef U_PORT_STM32_CMSIS_ON_FREERTOS

// End of file
