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
 * @brief Implementation of the port OS API for the NRF52 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "assert.h"
#include "stdlib.h"

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

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {

        k_thread_stack_t *stack = (k_thread_stack_t * )k_malloc(stackSizeBytes);
        struct k_thread *t_data = (struct k_thread *)k_malloc(sizeof(struct k_thread));
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        if (t_data != NULL && stack != NULL) {
            *pTaskHandle = (uPortTaskHandle_t) k_thread_create(
                               t_data,
                               stack,
                               stackSizeBytes,
                               (k_thread_entry_t)pFunction,
                               pParameter, NULL, NULL,
                               K_PRIO_COOP(priority), 0, K_NO_WAIT);

            if (*pTaskHandle != NULL) {
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

        k_thread_abort(thread);
        k_free((k_thread_stack_t *)((struct k_thread *) thread)->stack_info.start); //free the stack
        k_free((struct k_thread *) thread); //free the thread
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
    if (taskHandle != NULL) {
        struct k_thread *thread = (struct k_thread *) taskHandle;
        size_t unused = 0;
        int status = k_thread_stack_space_get(thread, &unused);
        if (status != 0) {
            result = U_ERROR_COMMON_UNKNOWN;
        } else {
            result = (int32_t)unused;
        }
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
        if ((kmutex->lock_count == 0) &&
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
        if ((kmutex->lock_count == 0) &&
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

// End of file
