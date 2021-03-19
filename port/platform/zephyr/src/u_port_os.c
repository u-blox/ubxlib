/*
 * Copyright 2020 u-blox Ltd
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

/** @file
 * @brief Implementation of the port OS API for the Zephyr platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"    // memset

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"

#include <zephyr.h>

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#ifndef portMAX_DELAY
#define portMAX_DELAY K_FOREVER
#endif

#ifndef U_PORT_STACK_GUARD_SIZE
#define U_PORT_STACK_GUARD_SIZE 50
#endif
static uint8_t __aligned(U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE)
exe_chunk_0[U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE];
// make this ram part executable
K_MEM_PARTITION_DEFINE(chunk0_reloc, exe_chunk_0, sizeof(exe_chunk_0),
                       K_MEM_PARTITION_P_RWX_U_RWX);

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    struct k_thread *pThread;
    k_thread_stack_t *pStack;
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
                k_free(threadPtr->pStack);
            }
            threadPtr->pThread = (struct k_thread *)k_malloc(sizeof(struct k_thread));
            threadPtr->pStack = (k_thread_stack_t *)k_malloc(stackSizeBytes + U_PORT_STACK_GUARD_SIZE);
            memset(threadPtr->pThread, 0, sizeof(struct k_thread));
            memset(threadPtr->pStack, 0, stackSizeBytes + U_PORT_STACK_GUARD_SIZE);
            threadPtr->stackSize = stackSizeBytes;
            threadPtr->isAllocated = true;
            break;
        }
    }
    if (threadPtr == NULL) {
        uPortLogF("No more threads available in thread pool, please increase U_CFG_OS_MAX_THREADS\n");
    } else if (threadPtr->pThread == NULL || threadPtr->pStack == NULL) {
        uPortLogF("Unable to allocate memory for thread with stack size %d\n", stackSizeBytes);
        k_free(threadPtr->pThread);
        k_free(threadPtr->pStack);
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
    int32_t i = 0;
    for (i = 0; i < U_CFG_OS_MAX_THREADS; i++) {
        gThreadInstances[i].pThread = NULL;
        gThreadInstances[i].pStack = NULL;
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
            k_free(gThreadInstances[i].pStack);
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
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    k_tid_t thread = (k_tid_t) taskHandle;

    // Can only delete oneself in freeRTOS so we keep that behaviour
    if (taskHandle == NULL) {
        thread = k_current_get();
        freeThreadInstance((struct k_thread *)thread);
        k_thread_abort(thread);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    return (int32_t)errorCode;
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
        *pQueueHandle = (uPortQueueHandle_t) k_malloc(sizeof(struct k_msgq));
        if (*pQueueHandle != NULL) {
            char *buffer = (char *) k_malloc(itemSizeBytes * queueLength);
            if (buffer) {
                k_msgq_init((struct k_msgq *)*pQueueHandle, buffer, itemSizeBytes, queueLength);
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

    }

    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        k_msgq_purge((struct k_msgq *) queueHandle);
        if (0 == k_msgq_cleanup((struct k_msgq *) queueHandle)) {
            k_free((struct k_msgq *) queueHandle);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Send to the given queue.
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (0 == k_msgq_put((struct k_msgq *)queueHandle, (void *)pEventData, portMAX_DELAY)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
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
        if (0 == k_msgq_get((struct k_msgq *)queueHandle, pEventData, portMAX_DELAY)) {
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle)
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
int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        k_free((struct k_mutex *) mutexHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Lock the given mutex.
int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    struct k_mutex *kmutex = (struct k_mutex *) mutexHandle;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (( k_current_get() != (k_tid_t) kmutex->owner) &&
            (k_mutex_lock((struct k_mutex *) mutexHandle,
                          (k_timeout_t ) portMAX_DELAY) == 0)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to lock the given mutex.
int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs)
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
int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle)
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
        if (k_sem_take(ksemaphore, (k_timeout_t ) portMAX_DELAY) == 0) {
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

// Simple implementation of making a chunk of RAM executable in Zephyr
void *uPortAcquireExecutableChunk(void *pChunkToMakeExecutable,
                                  size_t *pSize,
                                  uPortExeChunkFlags_t flags,
                                  uPortChunkIndex_t index)
{
    static struct k_mem_domain dom0;
    struct k_mem_partition *app_parts[] = { &chunk0_reloc };
    (void)pChunkToMakeExecutable;
    (void)flags;
    (void)index;

    k_mem_domain_init(&dom0, ARRAY_SIZE(app_parts), app_parts);
    k_mem_domain_add_thread(&dom0, k_current_get());

    *pSize = U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE;

    return exe_chunk_0;
}

// End of file
