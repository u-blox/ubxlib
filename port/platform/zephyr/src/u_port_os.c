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
 * @brief Implementation of the port OS API for the Zephyr platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* The remaining include files come after the mutex debug macros. */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR MUTEX DEBUG
 * -------------------------------------------------------------- */

#ifdef U_CFG_MUTEX_DEBUG
/** If we're adding mutex debug intermediate functions to
 * the build then the usual implementations of the mutex
 * functions get an underscore before them
 */
# define MAKE_MTX_FN(x, ...) _ ## x __VA_OPT__(,) __VA_ARGS__
#else
/** The normal case: a mutex function is not fiddled with.
 */
# define MAKE_MTX_FN(x, ...) x __VA_OPT__(,) __VA_ARGS__
#endif

/** This macro, working in conjunction with the MAKE_MTX_FN()
 * macro above, should wrap all of the uPortOsMutex* functions
 * in this file.  The functions are then pre-fixed with an
 * underscore if U_CFG_MUTEX_DEBUG is defined, allowing the
 * intermediate mutex macros/functions over in u_mutex_debug.c
 * to take their place.  Those functions subsequently call
 * back into the "underscore versions" of the uPortOsMutex*
 * functions here.
 */
#define MTX_FN(x, ...) MAKE_MTX_FN(x __VA_OPT__(,) __VA_ARGS__)

// Now undef U_CFG_MUTEX_DEBUG so that this file is not polluted
// by the u_mutex_debug.h stuff brought in through u_port_os.h.
#undef U_CFG_MUTEX_DEBUG

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"    // memset

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_private.h"

#include <zephyr.h>

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if !defined(CONFIG_ARCH_POSIX) && defined(CONFIG_USERSPACE)
// Not supported on Linux/Posix
static uint8_t __aligned(U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE)
exe_chunk_0[U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE];
// make this ram part executable
K_MEM_PARTITION_DEFINE(chunk0_reloc, exe_chunk_0, sizeof(exe_chunk_0),
                       K_MEM_PARTITION_P_RWX_U_RWX);
#endif
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    struct k_thread *pThread;
    k_thread_stack_t *pStack;
    void *pStackAllocation;
    size_t stackSize;
    bool isAllocated;
} uPortOsThreadInstance_t;
/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */
/** Array to keep track of the thread instances.
 */
static uPortOsThreadInstance_t gThreadInstances[U_CFG_OS_MAX_THREADS];
/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uPortOsThreadInstance_t *getNewThreadInstance(size_t stackSizeBytes)
{
    uPortOsThreadInstance_t *threadPtr = NULL;
    int32_t i = 0;
    for (i = 0; i < U_CFG_OS_MAX_THREADS; i++) {
        if (!gThreadInstances[i].isAllocated) {
            threadPtr = &gThreadInstances[i];
            //Free if previously used instance
            if (threadPtr->stackSize > 0) {
                k_free(threadPtr->pThread);
                k_free(threadPtr->pStackAllocation);
            }

            threadPtr->pThread = (struct k_thread *)k_malloc(sizeof(struct k_thread));

            // Zephyr doesn't officially support dynamically allocated stack memory.
            // For this reason we need to do some alignment hacks here.
            // When CONFIG_USERSPACE is enabled Zephyr will check if the stack is
            // "user capable" and then decide whether to use kernel or user space hosted
            // threads. When we pass it a stack pointer from the heap Zephyr will
            // decide to use kernel hosted thread. This is very important at least for
            // 32 bit ARM arch where MPU is enabled. For user space hosted threads
            // the stack alignment requirement is the nearest 2^x of the stack size.
            // Since the only way to align dynamically allocated memory is to adjust the
            // pointer after allocation we would in this case need to allocate the double
            // stack size which of course isn't a solution.
            // Luckily when the thread is kernel hosted the stack alignment is much lower
            // since then only a small MPU guard region is added at the top of the stack.
            // This decreases the stack alignment requirement to 32 bytes.
            //
            // For the above reason the code below will use the Z_KERNEL_STACK_xx defines
            // instead of Z_THREAD_STACK_xx.

            size_t stackAllocSize;
            // Other architectures may have other alignment requirements so just add
            // a simple check that we don't waste a huge amount dynamic memory due to
            // aligment.
            U_ASSERT(Z_KERNEL_STACK_OBJ_ALIGN <= 512);
            // Z_KERNEL_STACK_SIZE_ADJUST() will add extra space that Zephyr may require and
            // to make sure correct allignment we allocate Z_KERNEL_STACK_OBJ_ALIGN extra.
            stackAllocSize = Z_KERNEL_STACK_OBJ_ALIGN + Z_KERNEL_STACK_SIZE_ADJUST(stackSizeBytes);
            threadPtr->pStackAllocation = k_malloc(stackAllocSize);
            // Do the stack alignment
            threadPtr->pStack = (k_thread_stack_t *)ROUND_UP(threadPtr->pStackAllocation,
                                                             Z_KERNEL_STACK_OBJ_ALIGN);

            if (threadPtr->pThread && threadPtr->pStackAllocation) {
                memset(threadPtr->pThread, 0, sizeof(struct k_thread));
                threadPtr->stackSize = stackSizeBytes;
                threadPtr->isAllocated = true;
            }
            break;
        }
    }
    if (threadPtr == NULL) {
        uPortLogF("No more threads available in thread pool, please increase U_CFG_OS_MAX_THREADS\n");
    } else if (threadPtr->pThread == NULL || threadPtr->pStack == NULL) {
        uPortLogF("Unable to allocate memory for thread with stack size %d\n", stackSizeBytes);
        k_free(threadPtr->pThread);
        k_free(threadPtr->pStackAllocation);
        threadPtr->pThread = NULL;
        threadPtr->pStackAllocation = NULL;
        threadPtr = NULL;
    }
    return threadPtr;
}

static void freeThreadInstance(struct k_thread *threadPtr)
{
    int32_t i = 0;
    for (i = 0; i < U_CFG_OS_MAX_THREADS; i++) {
        if (threadPtr == gThreadInstances[i].pThread ) {
            gThreadInstances[i].isAllocated = false;
            break;
        }
    }
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: BUT ONES THAT SHOULD BE CALLED INTERNALLY ONLY
 * -------------------------------------------------------------- */

// Initialise thread pool.
void uPortOsPrivateInit()
{
    // systempool is now allocated during startup in z_sys_init_run_level.
    // threads created afterwards will inherit this pool.
    // this resolves a knownn issue in Zephyr when calling UBXLIB API from
    // threads that isn't the Zephyr main thread.
    for (int32_t i = 0; i < U_CFG_OS_MAX_THREADS; i++) {
        gThreadInstances[i].pThread = NULL;
        gThreadInstances[i].pStack = NULL;
        gThreadInstances[i].pStackAllocation = NULL;
        gThreadInstances[i].stackSize = 0;
        gThreadInstances[i].isAllocated = false;
    }
}
void uPortOsPrivateDeinit()
{
    int32_t i = 0;
    for (i = 0; i < U_CFG_OS_MAX_THREADS; i++) {
        if (gThreadInstances[i].stackSize > 0) {
            k_free(gThreadInstances[i].pThread);
            gThreadInstances[i].pThread = NULL;
            k_free(gThreadInstances[i].pStackAllocation);
            gThreadInstances[i].pStackAllocation = NULL;
            gThreadInstances[i].pStack = NULL;
            gThreadInstances[i].stackSize = 0;
            gThreadInstances[i].isAllocated = false;
        }
    }
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TASKS
 * -------------------------------------------------------------- */

// Create a task.
int32_t uPortTaskCreate(void (*pFunction)(void *),
                        const char *pName,
                        size_t stackSizeBytes,
                        void *pParameter,
                        int32_t priority,
                        uPortTaskHandle_t *pTaskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortOsThreadInstance_t *newThread =  NULL;

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {

        newThread =  getNewThreadInstance(stackSizeBytes);
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        if (newThread != NULL) {
            *pTaskHandle = (uPortTaskHandle_t) k_thread_create(
                               newThread->pThread,
                               newThread->pStack,
                               newThread->stackSize,
                               (k_thread_entry_t)pFunction,
                               pParameter, NULL, NULL,
                               K_PRIO_COOP(priority), 0, K_NO_WAIT);

            if (*pTaskHandle != NULL) {
                k_thread_system_pool_assign(newThread->pThread);
                if (pName != NULL) {
                    k_thread_name_set((k_tid_t)*pTaskHandle, pName);
                }
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    k_tid_t thread = (k_tid_t) taskHandle;

    if (taskHandle == NULL) {
        thread = k_current_get();
    }
    freeThreadInstance((struct k_thread *)thread);
    k_thread_abort(thread);

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return k_current_get() == (k_tid_t) taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    k_msleep(delayMs);
}

// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    int32_t result = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_thread *thread = (struct k_thread *) taskHandle;

    if (thread == NULL) {
        thread = (struct k_thread *) k_current_get();
    }
    size_t unused = 0;
    int status = k_thread_stack_space_get(thread, &unused);
    if (status != 0) {
        result = U_ERROR_COMMON_UNKNOWN;
    } else {
        result = (int32_t)unused;
    }

    return result;
}

// Get the current task handle.
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTaskHandle != NULL) {
        *pTaskHandle = (uPortTaskHandle_t) k_current_get();
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

// Create a queue.
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pQueueHandle != NULL) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        // Actually create the queue
        struct k_msgq *pMsgQ = (struct k_msgq *) k_malloc(sizeof(struct k_msgq));
        if (pMsgQ != NULL) {
            if (k_msgq_alloc_init(pMsgQ, itemSizeBytes, queueLength) == 0) {
                *pQueueHandle = (uPortQueueHandle_t) pMsgQ;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    struct k_msgq *pMsgQ = (struct k_msgq *)queueHandle;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        k_msgq_purge(pMsgQ);
        if (0 == k_msgq_cleanup(pMsgQ)) {
            k_free(pMsgQ);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Send to the given queue.
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
#ifdef U_CFG_QUEUE_DEBUG
        size_t x = 0;
        do {
            if (0 == k_msgq_put((struct k_msgq *)queueHandle, (void *)pEventData, K_NO_WAIT)) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else {
                if (x % (1000 / U_CFG_OS_YIELD_MS) == 0) {
                    // Print this roughly once a second
                    uPortLog("U_PORT_OS_QUEUE_DEBUG: queue 0x%08x is full, retrying...\n",
                             queueHandle);
                }
                x++;
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
            }
        } while (errorCode != U_ERROR_COMMON_SUCCESS);
#else
        if (0 == k_msgq_put((struct k_msgq *)queueHandle, (void *)pEventData, K_FOREVER)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
#endif
    }

    return errorCode;
}
// Send to the given queue from IRQ.
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_msgq_put((struct k_msgq *)queueHandle, (void *)pEventData, K_NO_WAIT)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}
// Receive from the given queue, blocking.
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_msgq_get((struct k_msgq *)queueHandle, pEventData, K_FOREVER)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Receive from the given queue, non-blocking.
int32_t uPortQueueReceiveIrq(const uPortQueueHandle_t queueHandle,
                             void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_msgq_get((struct k_msgq *)queueHandle, pEventData, K_NO_WAIT)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Receive from the given queue, with a wait time.
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_msgq_get((struct k_msgq *)queueHandle, pEventData, K_MSEC(waitMs))) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Peek the given queue.
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (0 == k_msgq_peek((struct k_msgq *)queueHandle, pEventData)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Get the number of free spaces in the given queue.
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    int32_t errorCodeOrFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        errorCodeOrFree = k_msgq_num_free_get((struct k_msgq *)queueHandle);
    }

    return errorCodeOrFree;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t MTX_FN(uPortMutexCreate(uPortMutexHandle_t *pMutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pMutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        // Actually create the mutex
        *pMutexHandle = (uPortMutexHandle_t) k_malloc(sizeof(struct k_mutex));
        if (*pMutexHandle != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (0 == k_mutex_init((struct k_mutex *)*pMutexHandle)) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Destroy a mutex.
int32_t MTX_FN(uPortMutexDelete(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        k_free((struct k_mutex *) mutexHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Lock the given mutex.
int32_t MTX_FN(uPortMutexLock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_mutex *kmutex = (struct k_mutex *) mutexHandle;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (( k_current_get() != (k_tid_t) kmutex->owner) &&
            (k_mutex_lock((struct k_mutex *) mutexHandle, K_FOREVER) == 0)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to lock the given mutex.
int32_t MTX_FN(uPortMutexTryLock(const uPortMutexHandle_t mutexHandle, int32_t delayMs))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_mutex *kmutex = (struct k_mutex *) mutexHandle;

    if (kmutex != NULL) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (( k_current_get() != (k_tid_t) kmutex->owner) &&
            (k_mutex_lock(kmutex,
                          K_MSEC(delayMs)) == 0)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Unlock the given mutex.
int32_t MTX_FN(uPortMutexUnlock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_mutex_unlock((struct k_mutex *) mutexHandle)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEMAPHORES
 * -------------------------------------------------------------- */

// Create a semaphore
int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pSemaphoreHandle != NULL) && (limit != 0) && (initialCount <= limit)) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        // Actually create the semaphore
        *pSemaphoreHandle = (uPortSemaphoreHandle_t) k_malloc(sizeof(struct k_sem));
        if (*pSemaphoreHandle != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (0 == k_sem_init((struct k_sem *)*pSemaphoreHandle, initialCount, limit)) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Destroy a semaphore
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        k_free((struct k_sem *) semaphoreHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Take a semaphore
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_sem *ksemaphore = (struct k_sem *) semaphoreHandle;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (k_sem_take(ksemaphore, K_FOREVER) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to take a semaphore
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_sem *ksemaphore = (struct k_sem *) semaphoreHandle;

    if (ksemaphore != NULL) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (k_sem_take(ksemaphore, K_MSEC(delayMs)) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Give a semaphore
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        k_sem_give((struct k_sem *) semaphoreHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Give a semaphore from interrupt
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    return uPortSemaphoreGive(semaphoreHandle);
}

/* ----------------------------------------------------------------
 * FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

// Create a timer.
int32_t uPortTimerCreate(uPortTimerHandle_t *pTimerHandle,
                         const char *pName,
                         pTimerCallback_t *pCallback,
                         void *pCallbackParam,
                         uint32_t intervalMs,
                         bool periodic)
{
    // Zephyr does not support use of a name for a timer
    (void) pName;

    return uPortPrivateTimerCreate(pTimerHandle,
                                   pCallback,
                                   pCallbackParam,
                                   intervalMs,
                                   periodic);
}

// Destroy a timer.
int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle)
{
    return uPortPrivateTimerDelete(timerHandle);
}

// Start a timer.
int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle)
{
    return uPortPrivateTimerStart(timerHandle);
}

// Stop a timer.
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    k_timer_stop((struct k_timer *) timerHandle);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Change a timer interval.
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    return uPortPrivateTimerChange(timerHandle, intervalMs);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CHUNK
 * -------------------------------------------------------------- */

// Simple implementation of making a chunk of RAM executable in Zephyr
void *uPortAcquireExecutableChunk(void *pChunkToMakeExecutable,
                                  size_t *pSize,
                                  uPortExeChunkFlags_t flags,
                                  uPortChunkIndex_t index)
{
    uint8_t *pExeChunk = NULL;

#if !defined(CONFIG_ARCH_POSIX) && defined(CONFIG_USERSPACE)
    static struct k_mem_domain dom0;
    struct k_mem_partition *app_parts[] = { &chunk0_reloc };
    (void)pChunkToMakeExecutable;
    (void)flags;
    (void)index;

    k_mem_domain_init(&dom0, ARRAY_SIZE(app_parts), app_parts);
    k_mem_domain_add_thread(&dom0, k_current_get());
    // Need to switch context to make the memory domain changes active
    k_yield();
    *pSize = U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE;

    pExeChunk = exe_chunk_0;
#endif

    return pExeChunk;
}

// End of file
