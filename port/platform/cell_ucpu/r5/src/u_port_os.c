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
 * @brief Implementation of the port OS API for the sarar5ucpu platform.
 */

#define TXM_MODULE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "txm_module.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // For configuration override
#include "u_cfg_test_platform_specific.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"

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

// Check if calling thread already owns the mutex.
static int32_t isThreadAlreadyHasMutex(uPortMutexHandle_t mutexHandle, bool *pHasMutex)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    TX_THREAD *threadPtr;
    char *pName;
    ULONG count;
    TX_THREAD *pFirstSuspended;
    ULONG suspendedCount;
    TX_MUTEX *pNextMutex;

    // Retreive information of mutex owner.
    result = (int32_t)tx_mutex_info_get((TX_MUTEX *)mutexHandle, &pName,
                                        &count, &threadPtr,
                                        &pFirstSuspended, &suspendedCount,
                                        &pNextMutex);

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        // Checking if calling thread is already owner of the mutex.
        *pHasMutex = (threadPtr == tx_thread_identify()) ? true : false;
    }

    return errorCode;
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
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX) &&
        (stackSizeBytes >= THREAD_STACK_MINIMUM) &&
        (stackSizeBytes <= THREAD_STACK_MAXIMUM)) {

        // Inversing priority according to our platform,
        // where 0 represent highest priority.
        priority = (TX_MAX_PRIORITIES - 1) - priority;

        errorCode = uPortPrivateTaskCreate(pFunction, pName, stackSizeBytes, pParameter, priority,
                                           pTaskHandle);
    } else {
        uPortLog("uPortTaskCreate: uport thread create invalid parameters.");
    }

    return errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;

    errorCode = uPortPrivateTaskDelete(taskHandle);

    return errorCode;
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return tx_thread_identify() == (TX_THREAD *)taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    tx_thread_sleep(delayMs);
}


// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    // Not supported on SARAR5UCPU platform, returning fixed
    // amount of stack.
    int32_t minStackFree = (1024 * 5);
    (void) taskHandle;

    return minStackFree;
}

// Get the current task handle.
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;

    if (pTaskHandle != NULL) {
        *pTaskHandle = (uPortTaskHandle_t *)tx_thread_identify();
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

// Create a queue.
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pQueueHandle != NULL) && (itemSizeBytes > 0) && (itemSizeBytes <= U_QUEUE_MAX_MSG_SIZE)) {

        // If itemSizeBytes is not multiple of 4, align it to 4.
        if ((itemSizeBytes % 4) != 0) {
            itemSizeBytes = (itemSizeBytes + 3) & (~3);
        }

        errorCode = uPortPrivateQueueCreate(queueLength, itemSizeBytes, pQueueHandle);
    }

    return errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        errorCode = uPortPrivateQueueDelete(queueHandle);
    }

    return errorCode;
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
            if (tx_queue_send((TX_QUEUE *)queueHandle, (void *)pEventData, TX_NO_WAIT) == 0) {
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
        if (tx_queue_send((TX_QUEUE *)queueHandle, (void *)pEventData, TX_WAIT_FOREVER) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
#endif
    }

    return errorCode;
}

// Send to the given queue from an interrupt.
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (tx_queue_send((TX_QUEUE *)queueHandle, (void *)pEventData, TX_NO_WAIT) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Receive from the given queue, blocking.
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (tx_queue_receive((TX_QUEUE *)queueHandle, pEventData, TX_WAIT_FOREVER) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Receive from the given queue, with a wait time.
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (tx_queue_receive((TX_QUEUE *)queueHandle, pEventData, waitMs) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Peek the given queue
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle, void *pEventData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = uPortPrivateQueuePeek(queueHandle, pEventData);
    }

    return errorCode;
}

// Get the number of free spaces in the given queue.
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    int32_t errorCodeOrFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t availableStorage; // Number of messages the queue currently has space for.
    int32_t enqueued;
    int32_t suspendedCount;
    char *name = TX_NULL;
    TX_THREAD *firstSuspended = TX_NULL;
    TX_QUEUE *nextQueue = TX_NULL;

    if (queueHandle != NULL) {
        errorCodeOrFree = U_ERROR_COMMON_PLATFORM;
        if (tx_queue_info_get((TX_QUEUE *)queueHandle,
                              &name, (ULONG *)&enqueued, (ULONG *)&availableStorage,
                              &firstSuspended, (ULONG *)&suspendedCount, &nextQueue) == 0) {
            errorCodeOrFree = availableStorage;
        }
    }

    return errorCodeOrFree;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    TX_MUTEX *mutex;
    int32_t result = -1;

    if (pMutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        result = (int32_t)txm_module_object_allocate((void *)&mutex, sizeof(TX_MUTEX));
        if (result == 0) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            result = (int32_t)tx_mutex_create(mutex, "module mutex", TX_NO_INHERIT);
        }
        if ((result == 0) && (mutex != NULL)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            *pMutexHandle = (uPortMutexHandle_t *)mutex;
        }
    }

    return errorCode;
}

// Destroy a mutex.
// Note: No need to call tx_object_deallocate() while deleting a mutex.
// It is done automatically by tx_mutex_delete().
int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t result = -1;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        result = (int32_t)tx_mutex_delete(mutexHandle);
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Lock the given mutex.
int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t result = -1;
    bool hasMutex = false;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

        // Check if calling thread already owns the mutex.
        result = isThreadAlreadyHasMutex(mutexHandle, &hasMutex);
        // If the calling thread already owns the mutex, return error.
        if ((result == 0) && (hasMutex)) {
            // Reset result variable to -1 to indicate error.
            result = -1;
        }

        // Try to get the mutex.
        if (result == 0) {
            result = (int32_t)tx_mutex_get(mutexHandle, TX_WAIT_FOREVER);
        }
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Try to lock the given mutex.
int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t result = -1;
    bool hasMutex = false;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

        // Check if calling thread already owns the mutex.
        result = isThreadAlreadyHasMutex(mutexHandle, &hasMutex);
        // If the calling thread already owns the mutex, return timeout.
        if ((result == 0) && (hasMutex)) {
            // Reset result variable to -1 to indicate error.
            result = -1;
            errorCode = U_ERROR_COMMON_TIMEOUT;
        }

        // Try to get the mutex.
        if (result == 0) {
            errorCode = U_ERROR_COMMON_TIMEOUT;
            result = (int32_t)tx_mutex_get(mutexHandle, delayMs);
        }
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Unlock the given mutex.
int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t result = -1;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        result = (int32_t)tx_mutex_put(mutexHandle);
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEMAPHORES
 * -------------------------------------------------------------- */

// Create a semaphore.
int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pSemaphoreHandle != NULL) && (limit != 0) && (initialCount <= limit)) {
        errorCode = uPortPrivateSemaphoreCreate(pSemaphoreHandle, initialCount, limit);
    }

    return errorCode;
}

// Destroy a semaphore.
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = uPortPrivateSemaphoreDelete(semaphoreHandle);
    }

    return errorCode;
}

// Take the given semaphore.
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = uPortPrivateSemaphoreTake(semaphoreHandle);
    }

    return errorCode;
}

// Try to take the given semaphore.
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = uPortPrivateSemaphoreTryTake(semaphoreHandle, delayMs);
    }
    return errorCode;
}

// Give the semaphore.
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = uPortPrivateSemaphoreGive(semaphoreHandle);
    }

    return errorCode;
}

// Give the semaphore from interrupt.
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    // Same API is used to give semaphore from ISR. ThreadX allows
    // calling tx_semaphore_put API from ISR as well. That's why
    // calling uPortSemaphoreGive API here.
    return uPortSemaphoreGive(semaphoreHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

int32_t uPortTimerCreate(uPortTimerHandle_t *pTimerHandle,
                         const char *pName,
                         pTimerCallback_t pCallback,
                         void *pCallbackParam,
                         uint32_t intervalMs,
                         bool periodic)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pTimerHandle != NULL) && (pCallback != NULL) && (intervalMs > 0)) {
        errorCode = uPortPrivateTimerCreate(pTimerHandle, pName, pCallback,
                                            pCallbackParam, intervalMs,
                                            periodic);
    }

    return errorCode;
}

int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (timerHandle != NULL) {
        errorCode = uPortPrivateTimerDelete(timerHandle);
    }

    return errorCode;
}

int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (timerHandle != NULL) {
        errorCode = uPortPrivateTimerStart(timerHandle);
    }

    return errorCode;
}

int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (timerHandle != NULL) {
        errorCode = uPortPrivateTimerStop(timerHandle);
    }

    return errorCode;
}

int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uint32_t rescheduleIntervalMs;

    if (timerHandle != NULL) {
        errorCode = uPortPrivateTimerChangeInterval(timerHandle, intervalMs);
    }

    return errorCode;
}

// End of file
