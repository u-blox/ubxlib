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
 * @brief Implementation of the port OS API for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* The remaining include files come after the mutex debug macros. */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR MUTEX DEBUG
 * -------------------------------------------------------------- */

#ifdef U_CFG_MUTEX_DEBUG
/** If we're adding the mutex debug intermediate functions to
 * the build then the implementations of the mutex functions
 * here get an underscore before them
 */
# define MAKE_MTX_FN(x, ...) _ ## x, ##__VA_ARGS__
#else
/** The normal case: a mutex function is not fiddled with.
 */
# define MAKE_MTX_FN(x, ...) x, ##__VA_ARGS__
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
#define MTX_FN(x, ...) MAKE_MTX_FN(x, ##__VA_ARGS__)

// Now undef U_CFG_MUTEX_DEBUG so that this file is not polluted
// by the u_mutex_debug.h stuff brought in through u_port_os.h.
#undef U_CFG_MUTEX_DEBUG

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"

#include "cmsis_os.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

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
    osThreadDef_t threadDef = {0};

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {

        threadDef.name = (char *) pName;
        threadDef.pthread = (void (*) (void const *)) pFunction;
        threadDef.tpriority = priority;
        threadDef.instances = 0;
        // Stack size is in words here, not bytes
        threadDef.stacksize = stackSizeBytes >> 2;

        *pTaskHandle = (uPortTaskHandle_t *) osThreadCreate(&threadDef,
                                                            pParameter);
        if (*pTaskHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;

    // Below is a workaround for a memory leak when using newlib built with _LITE_EXIT enabled.
    // When _LITE_EXIT is enabled the stdio streams stdout, stdin and stderr are not closed
    // when deallocating the task, resulting in memory leaks if the deleted task have been using
    // these io streams.
    // Note: The workaround below only works when a task deletes itself.
#if defined(__NEWLIB__) && defined(_LITE_EXIT) && defined(_REENT_SMALL) && \
    !defined(_REENT_GLOBAL_STDIO_STREAMS) && !defined(_UNBUF_STREAM_OPT)
    if (taskHandle == NULL) {
        if (stdout) {
            fclose(stdout);
        }
        if (stderr) {
            fclose(stderr);
        }
        if (stdin) {
            fclose(stdin);
        }
    }
#endif
    if (osThreadTerminate((osThreadId) taskHandle) == osOK) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return osThreadGetId() == (osThreadId) taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    // Make sure the scheduler has been started
    // or this may fly off into space
    U_ASSERT(osKernelRunning());
    osDelay(delayMs);
}

// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    TaskHandle_t handle = (TaskHandle_t) taskHandle;

    if (handle == NULL) {
        handle = xTaskGetCurrentTaskHandle();
    }

    // FreeRTOS returns stack size in words on STM32F4, so
    // multiply by four here to get bytes
    return uxTaskGetStackHighWaterMark(handle) * 4;
}

// Get the current task handle.
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTaskHandle != NULL) {
        *pTaskHandle = (uPortTaskHandle_t) osThreadGetId();
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

// Create a queue.
// Note: CMSIS-OS has osMessage which, in the case
// of the STM32F4 platform, maps to FreeRTOS queues,
// however an osMessage is fixed at 32 bits in size.
// Could use osMail but that would result in lots
// of malloc/free operations which is undesirable
// hence we go straight to the underlying FreeRTOS
// xQueue interface here.
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pQueueHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        // Actually create the queue
        *pQueueHandle = (uPortQueueHandle_t) xQueueCreate(queueLength,
                                                          itemSizeBytes);
        if (*pQueueHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        vQueueDelete((QueueHandle_t) queueHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
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
            if (xQueueSend((QueueHandle_t) queueHandle,
                           pEventData, 0) == pdTRUE) {
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
        if (xQueueSend((QueueHandle_t) queueHandle,
                       pEventData,
                       (portTickType) portMAX_DELAY) == pdTRUE) {
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
    BaseType_t yield = false;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (xQueueSendFromISR((QueueHandle_t) queueHandle,
                              pEventData,
                              &yield) == pdTRUE) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    // Required for correct FreeRTOS operation
    portEND_SWITCHING_ISR(yield);

    return (int32_t) errorCode;
}

// Receive from the given queue, blocking.
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (xQueueReceive((QueueHandle_t) queueHandle,
                          pEventData,
                          (portTickType) portMAX_DELAY) == pdTRUE) {
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
        if (xQueueReceiveFromISR((QueueHandle_t) queueHandle,
                                 pEventData,
                                 NULL) == pdTRUE) {
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
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (xQueueReceive((QueueHandle_t) queueHandle,
                          pEventData,
                          waitMs / portTICK_PERIOD_MS) == pdTRUE) {
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
        if (xQueuePeek((QueueHandle_t) queueHandle,
                       pEventData,
                       (portTickType) portMAX_DELAY) == pdTRUE) {
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
        errorCodeOrFree = (int32_t) uxQueueSpacesAvailable((QueueHandle_t) queueHandle);
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
    osMutexDef_t mutexDef = {0}; /* Required but with no meaningful
                                  * content in this case */

    if (pMutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        *pMutexHandle = (uPortMutexHandle_t) osMutexCreate(&mutexDef);
        if (*pMutexHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Destroy a mutex.
int32_t MTX_FN(uPortMutexDelete(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (osMutexDelete((osMutexId) mutexHandle) == osOK) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Lock the given mutex.
int32_t MTX_FN(uPortMutexLock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (osMutexWait((osMutexId) mutexHandle, osWaitForever) == osOK) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to lock the given mutex.
int32_t MTX_FN(uPortMutexTryLock(const uPortMutexHandle_t mutexHandle, int32_t delayMs))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (osMutexWait((osMutexId) mutexHandle, delayMs) == osOK) {
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
        if (osMutexRelease((osMutexId) mutexHandle) == osOK) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEMAPHORES
 * -------------------------------------------------------------- */

// Create a semaphore.
int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pSemaphoreHandle != NULL) && (limit != 0) && (initialCount <= limit)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        // Actually create the semaphore
        *pSemaphoreHandle = (uPortSemaphoreHandle_t) xSemaphoreCreateCounting(limit, initialCount);
        if (*pSemaphoreHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Destroy a semaphore.
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t) semaphoreHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Take the given semaphore.
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (xSemaphoreTake((SemaphoreHandle_t) semaphoreHandle,
                           (portTickType) portMAX_DELAY) == pdTRUE) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to take the given semaphore.
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (xSemaphoreTake((SemaphoreHandle_t) semaphoreHandle,
                           MS_TO_TICKS(delayMs)) == pdTRUE) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Give the semaphore.
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        xSemaphoreGive((SemaphoreHandle_t) semaphoreHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Give the semaphore from interrupt.
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    BaseType_t yield = false;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (xSemaphoreGiveFromISR((uPortSemaphoreHandle_t) semaphoreHandle,
                                  &yield) == pdTRUE) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    // Required for correct FreeRTOS operation
    portEND_SWITCHING_ISR(yield);

    return (int32_t) errorCode;
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
    return uPortPrivateTimerCreate(pTimerHandle,
                                   pName, pCallback,
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    if (xTimerStart((TimerHandle_t) timerHandle,
                    (portTickType) portMAX_DELAY) == pdPASS) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Stop a timer.
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    if (xTimerStop((TimerHandle_t) timerHandle,
                   (portTickType) portMAX_DELAY) == pdPASS) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Change a timer interval.
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    if (xTimerChangePeriod((TimerHandle_t) timerHandle,
                           MS_TO_TICKS(intervalMs),
                           (portTickType) portMAX_DELAY) == pdPASS) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: HOOKS
 * -------------------------------------------------------------- */

// Stack overflow hook, employed when configCHECK_FOR_STACK_OVERFLOW is
// set to 1 in FreeRTOSConfig.h.
void vApplicationStackOverflowHook(TaskHandle_t taskHandle,
                                   char *pTaskName)
{
    uPortLog("U_PORT: task handle 0x%08x, \"%s\", overflowed its"
             " stack.\n", (int32_t) taskHandle, pTaskName);
    U_ASSERT(false);
}

// Malloc failed hook, employed when configUSE_MALLOC_FAILED_HOOK is
// set to 1 in FreeRTOSConfig.h.
void vApplicationMallocFailedHook()
{
    uPortLog("U_PORT: freeRTOS doesn't have enough heap, increase"
             " configTOTAL_HEAP_SIZE in FreeRTOSConfig.h.\n");
    U_ASSERT(false);
}

// End of file
