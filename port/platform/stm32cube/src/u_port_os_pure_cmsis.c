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
 * @brief Implementation of the port OS API for the STM32U5 platform;
 * this assume pure CMSIS (V2 only) and relies on no native RTOS calls.
 * To use this you should make sure that U_PORT_STM32_PURE_CMSIS is
 * defined for your build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#ifndef U_PORT_STM32_PURE_CMSIS
# error U_PORT_STM32_PURE_CMSIS must be defined to use this file.
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
#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"
#include "u_assert.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"

#include "cmsis_os2.h"

#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_gpio.h"

#if defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
# include "FreeRTOS.h" // For TickType_t
# include "FreeRTOSConfig.h" // For configTICK_RATE_HZ
#else
# include "tx_api.h" // For TX_TIMER_TICKS_PER_SECOND
#endif

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef, TX_TIMER_TICKS_PER_SECOND and configTICK_RATE_HZ

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Variable to keep track of OS resource usage.
 */
static volatile int32_t gResourceAllocCount = 0;

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
    osThreadAttr_t attr = {0};

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {
        attr.name = pName;
        attr.priority = priority;
        attr.stack_size = stackSizeBytes;
        *pTaskHandle = (uPortTaskHandle_t *) osThreadNew(pFunction,
                                                         pParameter,
                                                         &attr);
        if (*pTaskHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            U_ATOMIC_INCREMENT(&gResourceAllocCount);
            U_PORT_OS_DEBUG_PRINT_TASK_CREATE(*pTaskHandle, pName, stackSizeBytes, priority);
        }
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    osThreadId_t threadId = (osThreadId_t) taskHandle;

    if (threadId == NULL) {
        // Pass a valid task ID in case NULL is not accepted by the underlying RTOS
        threadId = osThreadGetId();
    }

    // Need to call this macro before we terminate stdout
    U_PORT_OS_DEBUG_PRINT_TASK_DELETE(threadId);

    // Below is a workaround for a memory leak when using newlib built with _LITE_EXIT enabled.
    // When _LITE_EXIT is enabled the stdio streams stdout, stdin and stderr are not closed
    // when deallocating the task, resulting in memory leaks if the deleted task have been using
    // these io streams.
    // Note: The workaround below only works when a task deletes itself, which is always
    // the case with CMSIS on FreeRTOS and is never the case otherwise (i.e. with ThreadX).
#if defined(U_PORT_STM32_CMSIS_ON_FREERTOS) && \
    defined(__NEWLIB__) && defined(_LITE_EXIT) && defined(_REENT_SMALL) && \
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

    U_ATOMIC_DECREMENT(&gResourceAllocCount);
    // With CMSIS on ThreadX, must detach first as otherwise
    // resources are not free'd.
    if (
#ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
        (osThreadDetach(threadId) == osOK) &&
#endif
        (osThreadTerminate(threadId) == osOK)) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return osThreadGetId() == (osThreadId_t) taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    // Make sure the scheduler has been started
    // or this may fly off into space
    U_ASSERT(osKernelGetState() == osKernelRunning);
    osDelay(MS_TO_TICKS(delayMs));
}

// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    osThreadId_t threadId = (osThreadId_t) taskHandle;

    if (threadId == NULL) {
        threadId = osThreadGetId();
    }

    return osThreadGetStackSpace(threadId);
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
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pQueueHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        *pQueueHandle = (uPortQueueHandle_t) osMessageQueueNew(queueLength,
                                                               itemSizeBytes,
                                                               NULL);
        if (*pQueueHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            U_ATOMIC_INCREMENT(&gResourceAllocCount);
            U_PORT_OS_DEBUG_PRINT_QUEUE_CREATE(*pQueueHandle, queueLength, itemSizeBytes);
        }
    }

    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        osMessageQueueDelete((osMessageQueueId_t) queueHandle);
        errorCode = U_ERROR_COMMON_SUCCESS;
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
        U_PORT_OS_DEBUG_PRINT_QUEUE_DELETE(queueHandle);
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
            if (osMessageQueuePut((osMessageQueueId_t) queueHandle,
                                  pEventData, 0, 0) == 0) {
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
        if (osMessageQueuePut((osMessageQueueId_t) queueHandle,
                              pEventData, 0, osWaitForever) == 0) {
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
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        // osMessageQueuePut() is safe for use from within IRQs
        if (osMessageQueuePut((osMessageQueueId_t) queueHandle,
                              pEventData, 0, 0) == 0) {
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
        if (osMessageQueueGet((osMessageQueueId_t) queueHandle,
                              pEventData, NULL,
                              osWaitForever) == 0) {
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
        // osMessageQueueGet() is safe for use from within IRQs
        if (osMessageQueueGet((osMessageQueueId_t) queueHandle,
                              pEventData, NULL, 0) == 0) {
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
        if (osMessageQueueGet((osMessageQueueId_t) queueHandle,
                              pEventData, NULL, MS_TO_TICKS(waitMs)) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Peek the given queue.
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData)
{
    (void) queueHandle;
    (void) pEventData;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the number of free spaces in the given queue.
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    int32_t errorCodeOrFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        errorCodeOrFree = (int32_t) osMessageQueueGetSpace((osMessageQueueId_t) queueHandle);
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
        errorCode = U_ERROR_COMMON_PLATFORM;
        *pMutexHandle = (uPortMutexHandle_t) osMutexNew(NULL);
        if (*pMutexHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            U_ATOMIC_INCREMENT(&gResourceAllocCount);
            U_PORT_OS_DEBUG_PRINT_MUTEX_CREATE(*pMutexHandle);
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
        if (osMutexDelete((osMutexId_t) mutexHandle) == osOK) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            U_ATOMIC_DECREMENT(&gResourceAllocCount);
            U_PORT_OS_DEBUG_PRINT_MUTEX_DELETE(mutexHandle);
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
        if (osMutexAcquire((osMutexId_t) mutexHandle, osWaitForever) == osOK) {
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
        if (osMutexAcquire((osMutexId_t) mutexHandle, MS_TO_TICKS(delayMs)) == osOK) {
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
        if (osMutexRelease((osMutexId_t) mutexHandle) == osOK) {
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
    uErrorCode_t errorCode = uPortPrivateSemaphoreCreateCmsis(pSemaphoreHandle, initialCount, limit);

    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        U_ATOMIC_INCREMENT(&gResourceAllocCount);
        U_PORT_OS_DEBUG_PRINT_SEMAPHORE_CREATE(*pSemaphoreHandle, initialCount, limit);
    }

    return (int32_t) errorCode;
}

// Destroy a semaphore.
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = uPortPrivateSemaphoreDeleteCmsis(semaphoreHandle);

    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
        U_PORT_OS_DEBUG_PRINT_SEMAPHORE_DELETE(semaphoreHandle);
    }

    return (int32_t) errorCode;
}

// Take the given semaphore.
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    return uPortPrivateSemaphoreTakeCmsis(semaphoreHandle);
}

// Try to take the given semaphore.
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    return uPortPrivateSemaphoreTryTakeCmsis(semaphoreHandle, delayMs);
}

// Give the semaphore.
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    return uPortPrivateSemaphoreGiveCmsis(semaphoreHandle);
}

// Give the semaphore from interrupt.
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    // Note: though the CMSIS function does support giving a semaphore
    // from IRQ, the limitation with the ThreadX API means we have to keep
    // a list of semaphores, which of course we have to protect with
    // a mutex, and hence we can no longer support calls in interrupt
    // context.
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
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
    int32_t errorCode = uPortPrivateTimerCreate(pTimerHandle,
                                                pName, pCallback,
                                                pCallbackParam,
                                                intervalMs,
                                                periodic);
    if (errorCode == 0) {
        U_ATOMIC_INCREMENT(&gResourceAllocCount);
        U_PORT_OS_DEBUG_PRINT_TIMER_CREATE(*pTimerHandle, pName, intervalMs, periodic);
    }
    return errorCode;
}

// Destroy a timer.
int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = uPortPrivateTimerDelete(timerHandle);
    if (errorCode == 0) {
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
        U_PORT_OS_DEBUG_PRINT_TIMER_DELETE(timerHandle);
    }
    return errorCode;
}

// Start a timer.
int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle)
{
    return uPortPrivateTimerStartCmsis(timerHandle);
}

// Stop a timer.
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    osStatus_t  status;

    // In the CMSIS API, stopping a timer that is not running
    // is considered an error, which is a bit strange 'cos
    // how are you meant to know that your one-shot timer
    // has expired?  And the error code (osErrorResource) is
    // the same as the one you'd get if the timer could not
    // be deactivated, which is even worse.  Anyway...
    status = osTimerStop((osTimerId_t) timerHandle);
    if ((status == osOK) || (status == osErrorResource)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Change a timer interval.
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    return uPortPrivateTimerChangeCmsis(timerHandle, intervalMs);
}

/* ----------------------------------------------------------------
 * FUNCTIONS: DEBUGGING/MONITORING
 * -------------------------------------------------------------- */

// Get the number of OS resources currently allocated.
int32_t uPortOsResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
