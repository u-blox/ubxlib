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
 * @brief Stuff private to SARAR5UCPU platform.
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

#include "u_port_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold information about a thread. */
typedef struct {
    uPortTaskHandle_t handle;
    char *pStackStartAddress;
} uPortPrivateThreadInfo_t;

/** Structure to hold information about a queue. */
typedef struct {
    uPortQueueHandle_t handle;
    char *pMessageAreaStartAddress;
} uPortPrivateQueueInfo_t;

/** Structure to hold the information for timer name, callback and callback
 * parameter with it's respective handle.
 */
typedef struct {
    uPortTimerHandle_t handle;
    char name[U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES];
    bool periodic;
    uint32_t intervalMs;
    pTimerCallback_t *pCallback;
    void *pCallbackParam;
} uPortPrivateTimerInfo_t;

/** Structure to hold the information for semaphore maximum give limit
 * and give count with it's respective handle.
 */
typedef struct {
    uPortSemaphoreHandle_t handle; /** Semaphore handle. */
    uint32_t semaphoreGiveMaxLimit; /** Maximum limit for a which a sempahore can be given. */
    uint32_t semaphoreGiveCount; /** Semaphore give count. */
} uPortPrivateSemaphoreInfo_t;

/* ----------------------------------------------------------------
 * VARIABLES: Related to threads.
 * -------------------------------------------------------------- */

/** The thread list to hold thread's handle and it's stack start address.
 */
static uPortPrivateThreadInfo_t gThreadInfo[U_PORT_PRIVATE_MAXIMUM_NO_OF_THREADS];

/** To keep track of number of threads created.
  */
static uint32_t gNumOfThreadsCreated = 0;

/** Mutex to protect the list of threads.
 */
static uPortMutexHandle_t gMutexForThreads = NULL;

/* ----------------------------------------------------------------
 * VARIABLES: Related to queues.
 * -------------------------------------------------------------- */

/** The queue list to hold queue's handle and it's message area
 * starting address.
 */
static uPortPrivateQueueInfo_t gQueueInfo[U_PORT_PRIVATE_MAXIMUM_NO_OF_QUEUES];

/** To keep track of number of threads created.
*/
static uint32_t gNumOfQueuesCreated = 0;

/** Mutex to protect the list of threads.
 */
static uPortMutexHandle_t gMutexForQueues = NULL;

/* ----------------------------------------------------------------
 * VARIABLES: Related to timers.
 * -------------------------------------------------------------- */

/** The timer list to hold timer's handle, callback and callback
 * parameter.
 */
static uPortPrivateTimerInfo_t gTimerInfo[U_PORT_PRIVATE_MAXIMUM_NO_OF_TIMERS];

/** To keep track of number of timers created.
 */
static uint32_t gNumOfTimersCreated = 0;

/** Mutex to protect the list of timers.
 */
static uPortMutexHandle_t gMutexForTimers = NULL;

/* ----------------------------------------------------------------
 * VARIABLES: Related to semaphores.
 * -------------------------------------------------------------- */

/** The semaphore list to hold semaphore's handle, give count and
 * max give limit.
 */
static uPortPrivateSemaphoreInfo_t gSemaphoreInfo[U_PORT_PRIVATE_MAXIMUM_NO_OF_SEMAPHORES];

/** To keep track of number of semaphores created.
 */
static uint32_t gNumOfSemaphoresCreated = 0;

/** Mutex to protect the list of semaphores.
 */
static uPortMutexHandle_t gMutexForSemaphores = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: Related to threads.
 * -------------------------------------------------------------- */

// Find a thread entry in the list of threads.
// gMutexForThreads must be locked before calling this function.
static int32_t findIndexOfThread(uPortTaskHandle_t taskHandle)
{
    int32_t index = -1;

    for (uint32_t idx = 0 ; (index < 0) &&
         (idx < (sizeof(gThreadInfo) / sizeof(gThreadInfo[0]))) ; idx++) {
        if (gThreadInfo[idx].handle == taskHandle) {
            index = idx;
        }
    }

    return index;
}

// Add thread to the list of threads.
// gMutexForThreads must be locked before calling this function.
static int32_t addThreadToList(uPortTaskHandle_t taskHandle,
                               char *pStackStartAddress)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    // Find free entry in the list.
    int32_t index = findIndexOfThread(NULL);

    if (index >= 0) {
        gThreadInfo[index].handle = taskHandle;
        gThreadInfo[index].pStackStartAddress = pStackStartAddress;
        gNumOfThreadsCreated++;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: Related to queues.
 * -------------------------------------------------------------- */

// Find a queue entry in the list of queues.
// gMutexForQueues must be locked before calling this function.
static int32_t findIndexOfQueue(uPortQueueHandle_t queueHandle)
{
    int32_t index = -1;

    for (uint32_t idx = 0 ; (index < 0) &&
         (idx < (sizeof(gQueueInfo) / sizeof(gQueueInfo[0]))) ; idx++) {
        if (gQueueInfo[idx].handle == queueHandle) {
            index = idx;
        }
    }

    return index;
}

// Add queue to the list of queues.
// gMutexForQueues must be locked before calling this function.
static int32_t addQueueToList(uPortQueueHandle_t queueHandle,
                              char *pMessageAreaStartAddress)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    // Find free entry in the list.
    int32_t index = findIndexOfQueue(NULL);

    if (index >= 0) {
        gQueueInfo[index].handle = queueHandle;
        gQueueInfo[index].pMessageAreaStartAddress = pMessageAreaStartAddress;
        gNumOfQueuesCreated++;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: Related to timers.
 * -------------------------------------------------------------- */

// Find a timer entry from the list.
// gMutexForTimers should be locked before calling this function.
static int32_t findIndexOfTimer(uPortTimerHandle_t timerHandle)
{
    int32_t index = -1;

    for (uint32_t idx = 0 ; (index < 0) &&
         (idx < (sizeof(gTimerInfo) / sizeof(gTimerInfo[0]))) ; idx++) {
        if (gTimerInfo[idx].handle == timerHandle) {
            index = idx;
        }
    }

    return index;
}

// Add timer to the list.
// gMutexForTimers should be locked before calling this function.
static int32_t addTimerToList(uPortTimerHandle_t timerHandle,
                              const char *pName, bool periodic,
                              uint32_t intervalMs,
                              pTimerCallback_t *pCallback,
                              void *pCallbackParam)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index = -1;

    // Find free entry in the list.
    index = findIndexOfTimer(NULL);
    if (index >= 0) {
        gTimerInfo[index].handle = timerHandle;
        if (pName != NULL) {
            strncpy(gTimerInfo[index].name, pName, U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES);
            // Add null termination.
            gTimerInfo[index].name[sizeof(gTimerInfo[index].name) - 1] = '\0';
        }
        gTimerInfo[index].periodic = periodic;
        gTimerInfo[index].intervalMs = intervalMs;
        gTimerInfo[index].pCallback = pCallback;
        gTimerInfo[index].pCallbackParam = pCallbackParam;
        gNumOfTimersCreated++;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Remove timer from the list.
// gMutexForTimers should be locked before calling this function.
static int32_t removeTimerFromList(uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index = -1;

    // Find the timer in the list.
    index = findIndexOfTimer(timerHandle);

    if (index >= 0) {
        gTimerInfo[index].handle = NULL;
        gTimerInfo[index].name[0] = '\0';
        gTimerInfo[index].periodic = false;
        gTimerInfo[index].pCallback = NULL;
        gTimerInfo[index].pCallbackParam = NULL;
        gNumOfTimersCreated--;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Check if timer is periodic.
// gMutexForTimers should be locked before calling this function.
static int32_t isTimerPeriodic(uPortTimerHandle_t timerHandle, bool *gPeriodic)
{
    int32_t errorCode = -1;
    int32_t index = -1;

    // Find the timer in the list.
    index = findIndexOfTimer(timerHandle);

    if (index >= 0) {
        *gPeriodic = gTimerInfo[index].periodic;
        errorCode = 0;
    }

    return errorCode;
}

// The timer expiry callback, called by ThreadX.
static void timerCallback(ULONG param)
{
    uPortTimerHandle_t timerHandle = (uPortTimerHandle_t)param;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;
    int32_t index = -1;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    // Find the timer in the list.
    index = findIndexOfTimer(timerHandle);

    if ((index >= 0)) {
        timerHandle = gTimerInfo[index].handle;
        pCallback = gTimerInfo[index].pCallback;
        pCallbackParam = gTimerInfo[index].pCallbackParam;
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    if (pCallback != NULL) {
        // Call the actual callback.
        pCallback(timerHandle, pCallbackParam);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: Related to semaphores.
 * -------------------------------------------------------------- */

// Find a semaphore entry from the list.
// gMutexForSemaphores should be locked before calling this function.
static int32_t findIndexOfSemaphore(uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t index = -1;

    for (uint32_t idx = 0 ; (index < 0) &&
         (idx < (sizeof(gSemaphoreInfo) / sizeof(gSemaphoreInfo[0]))) ; idx++) {
        if (gSemaphoreInfo[idx].handle == semaphoreHandle) {
            index = idx;
        }
    }

    return index;
}

// Add semaphore to the list.
// gMutexForSemaphores should be locked before calling this function.
static int32_t addSemaphoreToList(uPortSemaphoreHandle_t semaphoreHandle,
                                  uint32_t initialCount,
                                  uint32_t limit)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index = -1;

    // Find free entry in the list.
    index = findIndexOfSemaphore(NULL);
    if (index >= 0) {
        gSemaphoreInfo[index].handle = semaphoreHandle;
        gSemaphoreInfo[index].semaphoreGiveMaxLimit = limit;
        gSemaphoreInfo[index].semaphoreGiveCount = initialCount;
        gNumOfSemaphoresCreated++;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Remove semaphore from the list.
// gMutexForSemaphores should be locked before calling this function.
static int32_t removeSemaphoreFromList(uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index = -1;

    // Find the semaphore in the list.
    index = findIndexOfSemaphore(semaphoreHandle);

    if (index >= 0) {
        gSemaphoreInfo[index].handle = NULL;
        gSemaphoreInfo[index].semaphoreGiveMaxLimit = 0;
        gSemaphoreInfo[index].semaphoreGiveCount = 0;
        gNumOfSemaphoresCreated--;
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT
 * -------------------------------------------------------------- */

int32_t uPortPrivateInit()
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;

    // Create mutex to protect the list of threads.
    errorCode = uPortMutexCreate(&gMutexForThreads);

    // Create mutex to protect the list of queues.
    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        errorCode = uPortMutexCreate(&gMutexForQueues);
    }

    // Create mutex to protect the list of timers.
    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        errorCode = uPortMutexCreate(&gMutexForTimers);
    }

    // Create mutex to protect the list of semaphores.
    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        errorCode = uPortMutexCreate(&gMutexForSemaphores);
    }

    return errorCode;
}

void uPortPrivateDeinit()
{
    // Lock thread mutex.
    U_PORT_MUTEX_LOCK(gMutexForThreads);

    // Terminate and delete all the threads.
    for (uint32_t idx = 0 ; idx < (sizeof(gThreadInfo) / sizeof(gThreadInfo[0])) ; idx++) {
        if (gThreadInfo[idx].handle != NULL) {

            if (gThreadInfo[idx].pStackStartAddress != NULL) {
                // Release the allocated bytes for the thread stack.
                tx_byte_release((VOID *)gThreadInfo[idx].pStackStartAddress);
                gThreadInfo[idx].pStackStartAddress = NULL;
            }
            // Terminate the thread.
            tx_thread_terminate((TX_THREAD *)gThreadInfo[idx].handle);
            // Delete the thread.
            tx_thread_delete((TX_THREAD *)gThreadInfo[idx].handle);
            // Free the thread handle.
            gThreadInfo[idx].handle = NULL;
        }
    }

    // Unlock thread mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForThreads);

    // Delete thread mutex.
    uPortMutexDelete(gMutexForThreads);

    // Lock queue mutex.
    U_PORT_MUTEX_LOCK(gMutexForQueues);

    // Delete all the queues.
    for (uint32_t idx = 0 ; idx < (sizeof(gQueueInfo) / sizeof(gQueueInfo[0])) ; idx++) {
        if (gQueueInfo[idx].handle != NULL) {

            if (gQueueInfo[idx].pMessageAreaStartAddress != NULL) {
                // Release the allocated bytes for the queue message area.
                tx_byte_release((VOID *)gQueueInfo[idx].pMessageAreaStartAddress);
                gQueueInfo[idx].pMessageAreaStartAddress = NULL;
            }
            // Delete the queue.
            tx_queue_delete((TX_QUEUE *)gQueueInfo[idx].handle);
            // Free the queue handle.
            gQueueInfo[idx].handle = NULL;
        }
    }

    // Unlock queue mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForQueues);

    // Delete queue mutex.
    uPortMutexDelete(gMutexForQueues);

    // Lock timer mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    // Stop and delete all the timers.
    for (uint32_t idx = 0; idx < (sizeof(gTimerInfo) / sizeof(gTimerInfo[0])); idx++) {
        if (gTimerInfo[idx].handle != NULL) {
            // Stop the timer.
            tx_timer_deactivate(gTimerInfo[idx].handle);
            // Delete the timer.
            tx_timer_delete(gTimerInfo[idx].handle);
            // Free the timer handle.
            gTimerInfo[idx].handle = NULL;
        }
    }

    // Unlock timer mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    // Delete timer mutex.
    uPortMutexDelete(gMutexForTimers);

    // Lock semaphore mutex.
    U_PORT_MUTEX_LOCK(gMutexForSemaphores);

    // Delete all the semaphores.
    for (uint32_t idx = 0; idx < (sizeof(gSemaphoreInfo) / sizeof(gSemaphoreInfo[0])); idx++) {
        if (gSemaphoreInfo[idx].handle != NULL) {
            // Delete the semaphore.
            tx_semaphore_delete(gSemaphoreInfo[idx].handle);
            // Free the semaphore handle.
            gSemaphoreInfo[idx].handle = NULL;
        }
    }

    // Unlock semaphore mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);

    // Delete semaphore mutex.
    uPortMutexDelete(gMutexForSemaphores);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THREADS
 * -------------------------------------------------------------- */

int32_t uPortPrivateTaskCreate(void (*pFunction)(void *),
                               const char *pName,
                               size_t stackSizeBytes,
                               void *pParameter,
                               int32_t priority,
                               uPortTaskHandle_t *pTaskHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    TX_THREAD *threadPtr;
    char *pBlock;

    // Lock thread mutex.
    U_PORT_MUTEX_LOCK(gMutexForThreads);

    if (gNumOfThreadsCreated < U_PORT_PRIVATE_MAXIMUM_NO_OF_THREADS) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        // Allocate the stack for thread
        result = (int32_t)txm_module_object_allocate((void *)&threadPtr, sizeof(TX_THREAD));
        if (result == 0) {
            result = (int32_t)tx_byte_allocate(pThreadStack, (void **)&pBlock, stackSizeBytes, TX_NO_WAIT);

            if (result == 0) {
                errorCode = U_ERROR_COMMON_PLATFORM;
                result = (int32_t)tx_thread_create(threadPtr, (char *)pName, (void (*)(ULONG))pFunction,
                                                   (ULONG)pParameter,
                                                   pBlock, stackSizeBytes,
                                                   priority, priority, TX_NO_TIME_SLICE, TX_AUTO_START);
                if ((result == 0) && (threadPtr != NULL)) {
                    // Thread created successfully, copying thread handle
                    *pTaskHandle = (uPortTaskHandle_t *)threadPtr;
                    // Add the thread to the list.
                    errorCode = addThreadToList(*pTaskHandle, pBlock);
                }
            }
        }
    } else {
        // Maximum number of threads created.
        uPortLog("uPortPrivateTaskCreate: Maximum number of threads created.");
    }

    // Unlock thread mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForThreads);

    return errorCode;
}

// Note: No need to call tx_object_deallocate() while deleting a task.
// It is done automatically by tx_thread_delete().
int32_t uPortPrivateTaskDelete(const uPortTaskHandle_t taskHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    TX_THREAD *threadPtr;

    // Lock thread mutex.
    U_PORT_MUTEX_LOCK(gMutexForThreads);

    // If delete API is called from a task attempting to delete itself.
    if (taskHandle == NULL) {
        // Identify calling thread.
        threadPtr = tx_thread_identify();
        // Find the thread in the list.
        int32_t index = findIndexOfThread((uPortTaskHandle_t)threadPtr);

        if (index >= 0) {
            // Relese the allocated bytes for the thread stack.
            result = tx_byte_release((VOID *)gThreadInfo[index].pStackStartAddress);
        }

        if (result == 0) {
            // Just clearing the pStackStartAdress and decrement number of threads created.
            // We need to keep the handle to delete the thread later from uPortPrivateDeinit() routine.
            gThreadInfo[index].pStackStartAddress = NULL;
            gNumOfThreadsCreated--;
            // Terminate the thread.
            result = (int32_t)tx_thread_terminate(threadPtr);
        }

        // Not calling threadX delete API here, tx_thread_delete
        // can not be called from a task attempting to
        // delete itself.
    } else {
        // Delete API is called from another task or handle is not NULL.
        result = -1;

        // Find the thread in the list.
        int32_t index = findIndexOfThread(taskHandle);

        if (index >= 0) {
            // Relese the allocated bytes for the thread stack.
            result = tx_byte_release((VOID *)gThreadInfo[index].pStackStartAddress);
        }

        if (result == 0) {
            // Terminate the thread.
            result = (int32_t)tx_thread_terminate((TX_THREAD *)taskHandle);
        }

        if (result == 0) {
            // Delete the thread.
            result = (int32_t)tx_thread_delete((TX_THREAD *)taskHandle);
        }

        if (result == 0) {
            // Remove the thread from the list.
            gThreadInfo[index].handle = NULL;
            gThreadInfo[index].pStackStartAddress = NULL;
            gNumOfThreadsCreated--;
        }
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    // Unlock thread mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForThreads);

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: Queues
 * -------------------------------------------------------------- */

int32_t uPortPrivateQueueCreate(size_t queueLength,
                                size_t itemSizeBytes,
                                uPortQueueHandle_t *pQueueHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result;
    TX_QUEUE *queuePtr;
    char *pBlock;

    // Lock queue mutex.
    U_PORT_MUTEX_LOCK(gMutexForQueues);

    if (gNumOfQueuesCreated < U_PORT_PRIVATE_MAXIMUM_NO_OF_QUEUES) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        result = (int32_t)txm_module_object_allocate((void *)&queuePtr, sizeof(TX_QUEUE));
        if (result == 0) {
            result = (int32_t)tx_byte_allocate(pThreadStack, (VOID **)&pBlock, queueLength * itemSizeBytes,
                                               TX_NO_WAIT);
            if (result == 0) {
                errorCode = U_ERROR_COMMON_PLATFORM;
                /* item size when passed to the tx_queue_create() must be in the units of word instead of byte */
                result = (int32_t)tx_queue_create(queuePtr, "module queue", (UINT) itemSizeBytes / sizeof(ULONG),
                                                  pBlock,
                                                  queueLength * itemSizeBytes);
            }
        }
        if ((result) == 0 && (queuePtr != NULL)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            *pQueueHandle = (uPortQueueHandle_t *)queuePtr;
            // Add the queue to the list.
            errorCode = addQueueToList(*pQueueHandle, pBlock);
        }
    } else {
        // Maximum number of queues created.
        uPortLog("uPortPrivateQueueCreate: Maximum number of queues created.");
    }

    // Unlock queue mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForQueues);

    return errorCode;
}

// Note: No need to call tx_object_deallocate() while deleting a queue.
// It is done automatically by tx_queue_delete().
int32_t uPortPrivateQueueDelete(const uPortQueueHandle_t queueHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;

    // Lock queue mutex.
    U_PORT_MUTEX_LOCK(gMutexForQueues);

    // Find the queue in the list.
    int32_t index = findIndexOfQueue(queueHandle);

    if (index >= 0) {
        // Relese the allocated bytes for the queue.
        result = tx_byte_release((VOID *)gQueueInfo[index].pMessageAreaStartAddress);
        if (result == 0) {
            gQueueInfo[index].pMessageAreaStartAddress = NULL;
        }
    }

    if (result == 0) {
        // Delete the queue.
        result = (int32_t)tx_queue_delete((TX_QUEUE *)queueHandle);
    }

    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        // Remove the queue from the list.
        gQueueInfo[index].handle = NULL;
        gNumOfQueuesCreated--;
    }

    // Unlock queue mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForQueues);

    return errorCode;
}

int32_t uPortPrivateQueuePeek(const uPortQueueHandle_t queueHandle,
                              void *pEventData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;

    // Lock queue mutex.
    U_PORT_MUTEX_LOCK(gMutexForQueues);

    // Try receiving the data
    if (tx_queue_receive((TX_QUEUE *)queueHandle, pEventData, TX_NO_WAIT) == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
        // Push back the data at front
        if (tx_queue_front_send((TX_QUEUE *)queueHandle, pEventData, TX_WAIT_FOREVER) == 0) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    // Unlock queue mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForQueues);

    return errorCode;
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: Timers
 * -------------------------------------------------------------- */

int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                const char *pName,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    TX_TIMER *timer;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    if (gNumOfTimersCreated < U_PORT_PRIVATE_MAXIMUM_NO_OF_TIMERS) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        // Allocate object for timer.
        result = (int32_t)txm_module_object_allocate((void *)&timer, sizeof(TX_TIMER));
        if (result == 0) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            // Create timer.
            result = (int32_t)tx_timer_create(timer,
                                              (char *)pName,
                                              (void (*)(ULONG))timerCallback,
                                              (ULONG)timer,
                                              intervalMs,
                                              periodic ? intervalMs : 0, // Set 0 for non-periodic timer.
                                              false);
            if ((result == 0) && (timer != NULL)) {
                *pHandle = (uPortTimerHandle_t *)timer;
                // Timer created, add timer to the timer list.
                errorCode = addTimerToList(*pHandle, pName, periodic, intervalMs, pCallback, pCallbackParam);
            }
        }
    } else {
        // Maximum number of timers created.
        uPortLog("uPortPrivateTimerCreate: Maximum number of timers created.");
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    return errorCode;
}

// Note: No need to call tx_object_deallocate() while deleting a timer.
// It is done automatically by tx_timer_delete().
int32_t uPortPrivateTimerDelete(const uPortTimerHandle_t handle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    // Stop the timer. If timer is already stopped then this
    // service will have no effect.
    result = (int32_t)tx_timer_deactivate((TX_TIMER *)handle);
    if (result == 0) {
        // Delete timer.
        result = (int32_t)tx_timer_delete((TX_TIMER *)handle);
        if (result == 0) {
            // Timer deleted, remove timer from the timer list.
            errorCode = removeTimerFromList(handle);
        }
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    return errorCode;
}

int32_t uPortPrivateTimerChangeInterval(const uPortTimerHandle_t handle,
                                        uint32_t intervalMs)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    bool isPeriodic = false;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    // Check if the timer is periodic.
    result = isTimerPeriodic(handle, &isPeriodic);

    if (result == 0) {
        // Change the timer interval.
        result = (int32_t)tx_timer_change(handle,
                                          intervalMs,
                                          isPeriodic ? intervalMs : 0); // Set 0 for non-periodic timer.
        if (result == 0) {
            // Find index of timer in timer list.
            int32_t index = findIndexOfTimer(handle);
            if (index >= 0) {
                // Update timer expiry interval.
                gTimerInfo[index].intervalMs = intervalMs;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    return errorCode;
}

int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result = -1;
    int32_t index = -1;
    bool isPeriodic = false;

    // If timer is already started, stop it first.
    // Note: This is required to keep the compatibility with
    // other platforms.
    uPortPrivateTimerStop(handle);

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForTimers);

    // If the timer is non-periodic then we first need to reset the
    // timer via uPortPrivateTimerChangeInterval().
    // Note: This is a requirement of the ThreadX API.
    result = isTimerPeriodic(handle, &isPeriodic);
    if (result == 0) {
        if (!isPeriodic) {
            uint32_t intervalMs = 0;
            // Find the timer in the timer list.
            index = findIndexOfTimer(handle);
            // Get the timer interval.
            intervalMs = gTimerInfo[index].intervalMs;
            // Change/reset the timer interval.
            uPortPrivateTimerChangeInterval(handle, intervalMs);
        }

        // Start the timer.
        result = (int32_t)tx_timer_activate((TX_TIMER *)handle);
        if (result == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForTimers);

    return errorCode;
}

int32_t uPortPrivateTimerStop(const uPortTimerHandle_t handle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result;

    // Stop the timer.
    result = (int32_t)tx_timer_deactivate((TX_TIMER *)handle);
    if (result == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: Semaphores
 * -------------------------------------------------------------- */

int32_t uPortPrivateSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                                    uint32_t initialCount,
                                    uint32_t limit)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result;
    TX_SEMAPHORE *semaphore;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForSemaphores);

    if (gNumOfSemaphoresCreated < U_PORT_PRIVATE_MAXIMUM_NO_OF_SEMAPHORES) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        result = (int32_t)txm_module_object_allocate((void *)&semaphore, sizeof(TX_SEMAPHORE));
        if (result == 0) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            result = (int32_t)tx_semaphore_create(semaphore, "module semaphore", initialCount);
            if ((result == 0) && (semaphore != NULL)) {
                *pSemaphoreHandle = (uPortSemaphoreHandle_t)semaphore;
                // Add the semaphore to the semaphore list.
                errorCode = addSemaphoreToList(*pSemaphoreHandle, initialCount, limit);
            }
        }
    } else {
        // Maximum number of semaphores created.
        uPortLog("uPortPrivateSemaphoreCreate: Maximum number of semaphores created.");
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);

    return errorCode;
}

// Note: No need to call tx_object_deallocate() while deleting a semaphore.
// It is done automatically by tx_semaphore_delete().
int32_t uPortPrivateSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t result;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForSemaphores);
    // Delete the semaphore.
    result = (int32_t)tx_semaphore_delete((TX_SEMAPHORE *)semaphoreHandle);
    if (result == 0) {
        // Remove the semaphore from the semaphore list.
        errorCode = removeSemaphoreFromList(semaphoreHandle);
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);

    return errorCode;
}

int32_t uPortPrivateSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index;

    // Take semaphore, outside mutex as it can block.
    if (tx_semaphore_get((TX_SEMAPHORE *)semaphoreHandle, TX_WAIT_FOREVER) == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;

        // Lock the mutex.
        U_PORT_MUTEX_LOCK(gMutexForSemaphores);

        // Find the index of current semaphore.
        index = findIndexOfSemaphore(semaphoreHandle);
        // Semaphore taken, decrement give count of current semaphore.
        if ((index >= 0) && (gSemaphoreInfo[index].semaphoreGiveCount > 0)) {
            gSemaphoreInfo[index].semaphoreGiveCount--;
        }

        // Unlock the mutex.
        U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
    }

    return errorCode;
}

int32_t uPortPrivateSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                                     int32_t delayMs)

{
    int32_t errorCode = U_ERROR_COMMON_TIMEOUT;
    int32_t index;

    // Take semaphore.
    if (tx_semaphore_get((TX_SEMAPHORE *)semaphoreHandle, (ULONG)delayMs) == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        // Lock the mutex.
        U_PORT_MUTEX_LOCK(gMutexForSemaphores);

        // Find the index of current semaphore.
        index = findIndexOfSemaphore(semaphoreHandle);
        // Semaphore taken, decrement give count of current semaphore.
        if (gSemaphoreInfo[index].semaphoreGiveCount > 0) {
            gSemaphoreInfo[index].semaphoreGiveCount--;
        }

        // Unlock the mutex.
        U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
    }

    return errorCode;
}

int32_t uPortPrivateSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;
    int32_t index;

    // Lock the mutex.
    U_PORT_MUTEX_LOCK(gMutexForSemaphores);

    // Find the index of current semaphore.
    index = findIndexOfSemaphore(semaphoreHandle);
    if (index >= 0) {
        // Here U_ERROR_COMMON_SUCCESS means that semaphore is not given if max
        // give count limit has reached.
        errorCode = U_ERROR_COMMON_SUCCESS;
        // Do not give sempahore if max give count limit has reached.
        // ThreadX does not provide the API to have limit on
        // max give count of semaphore. So, we have to handle it here.
        if (gSemaphoreInfo[index].semaphoreGiveCount <
            gSemaphoreInfo[index].semaphoreGiveMaxLimit) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            // Give semaphore.
            if (tx_semaphore_put((TX_SEMAPHORE *)semaphoreHandle) == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
                // Semaphore given, increment give count of current semaphore.
                gSemaphoreInfo[index].semaphoreGiveCount++;
            }
        }
    }

    // Unlock the mutex.
    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);

    return errorCode;
}

// End of file
