/*
 * Copyright 2019-2022 u-blox Ltd
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
 * @brief Implementation of generic porting functions for the sarar5ucpu platform.
 */

#define TXM_MODULE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "txm_module.h"
#include "u_compiler.h" // For U_INLINE
#include "u_cfg_os_platform_specific.h" // For configuration override
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include <stdarg.h>    // For va_x().

#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"
#include "u_port_private.h"
#include "u_assert.h"

#include "ucpu_sdk_debug.h"

/* --------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES: STATIC
 * -------------------------------------------------------------- */

/** Keep track of whether we've been initialised or not.
 */
static bool gInitialised = false;

/** Define the pool space in the bss section of the module for usage
 * of stack memory. ULONG is used to get the word alignment.
 */
static ULONG threadStackSpace[THREAD_STACK_POOL_SIZE / 4];

/** Define the pool space for the usage of heap memory.
 * ULONG is used to get the word alignment.
 */
static ULONG heapPoolSpace[HEAP_POOL_SIZE / 4];

/** Variable used to store the interrupt posture before the
 * interrupt is disabled.
 */
static UINT gInterruptPosture;

/* ----------------------------------------------------------------
 * VARIABLES: PUBLIC
 * -------------------------------------------------------------- */

/** Pointer to heap pool space.
 */
TX_BYTE_POOL *pHeapPool;

/** Pointer to byte pool space. Pool space to allocate memory for
 * all threads and queues.
 */
TX_BYTE_POOL *pThreadStack;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

void cell_ucpu_assert(const char *pFileStr, int32_t line)
{
    ucpu_sdk_assert(pFileStr, line);

    // As the firmware works on message dispatching mechanism, so it takes
    // a few millisecond to unload and stop the running the module. This
    // causes the execution to continue which may result in a crash.
    // So, staying in an infinite loop until the module is unloaded and stopped.
    while (1) {
        uPortTaskBlock(100);
    }
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Start the platform.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    (void) stackSizeBytes;
    (void) priority;

    // OS is already running, just call pEntryPoint
    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        pEntryPoint(pParameter);
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    // Flag to keep track of memory pool initialization.
    static bool poolInitialised = false;
    int32_t errorCode = U_ERROR_COMMON_SUCCESS;
    int32_t result = -1;

    // Register an assertFailed() callback.
    uAssertHookSet(cell_ucpu_assert);

    if (!gInitialised) {
        errorCode = uPortEventQueuePrivateInit();
        if (errorCode == 0) {
            errorCode = uPortPrivateInit();
            if (errorCode == 0) {
                errorCode = uPortUartInit();
            }
        }
        gInitialised = (errorCode == 0);
    }

    // Initialize memory pools at the startup
    if (!poolInitialised) {
        if (errorCode == 0) {
            errorCode = U_ERROR_COMMON_NO_MEMORY;
            // Allocate a byte bool for stack usage
            result = (int32_t)txm_module_object_allocate((void *)&pThreadStack, sizeof(TX_BYTE_POOL));
            uPortLog("Thread stack pool object allocate result = %d\n", result);
            if (result == 0) {
                errorCode = U_ERROR_COMMON_PLATFORM;
                // Create a byte memory pool from which to allocate the thread stacks
                result = (int32_t)tx_byte_pool_create(pThreadStack, "thread stack", threadStackSpace,
                                                      THREAD_STACK_POOL_SIZE);
                uPortLog("Thread stack pool create result = %d\n", result);
            }
        }

        if (result == 0) {
            errorCode = U_ERROR_COMMON_NO_MEMORY;
            // Allocate a byte pool for heap usage
            result = (int32_t)txm_module_object_allocate((void *)&pHeapPool, sizeof(TX_BYTE_POOL));
            uPortLog("Heap pool object allocate result = %d\n", result);

            if (result == 0) {
                errorCode = U_ERROR_COMMON_PLATFORM;
                // Create a byte memory pool from which to allocate the heap
                result = (int32_t)tx_byte_pool_create(pHeapPool, "heap pool",
                                                      heapPoolSpace,
                                                      HEAP_POOL_SIZE);
                uPortLog("Heap pool create result = %d\n", result);
            }
        }

        if (result == 0) {
            poolInitialised = true;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Deinitialise the porting layer.
void uPortDeinit()
{
    if (gInitialised) {
        uPortUartDeinit();
        uPortPrivateDeinit();
        uPortEventQueuePrivateDeinit();
        gInitialised = false;
    }
}

// Get the current tick converted to a time in milliseconds.
int32_t uPortGetTickTimeMs()
{
    return tx_time_get();
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    int32_t errorCode = U_ERROR_COMMON_NO_MEMORY;
    uint32_t result;
    ULONG availableBytes;
    ULONG fragments;
    ULONG suspended_count;
    CHAR *name = TX_NULL;
    TX_THREAD *first_suspended = TX_NULL;
    TX_BYTE_POOL *next_pool = TX_NULL;

    // Retrieve information about the previously created
    // block pool
    result = tx_byte_pool_info_get(pHeapPool, &name,
                                   &availableBytes, &fragments,
                                   &first_suspended, &suspended_count,
                                   &next_pool);
    if (result == 0) {
        errorCode = availableBytes;
    }

    return errorCode;
}

// Enter a critical section.
U_INLINE int32_t uPortEnterCritical()
{
    gInterruptPosture = tx_interrupt_control(TX_INT_DISABLE);
    return U_ERROR_COMMON_SUCCESS;
}

// Leave a critical section.
U_INLINE void uPortExitCritical()
{
    tx_interrupt_control(gInterruptPosture);
}

// End of file
