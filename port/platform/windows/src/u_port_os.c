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
 * @brief Implementation of the port OS API for Windows.
 *
 * Implementation note 1: the thread functions return the thread ID
 * instead of the thread handle.  This is because you can get a thread
 * ID from a handle but not the other way around and the only way to
 * verify who you are is with the ID.
 * Implementation note 2: the win32 API doesn't have adhoc queues.  The
 * closest thing is a pipe and the ideal thing would be just anonymous pipes,
 * which seem perfect _except_ that our APIs require us to be able to
 * peek the queue and the win32 pipe peek() call will block on an anonymous
 * pipe as an anonymous pipe can _only_ be opened with _synchronous_ file
 * pointers.  I tried named pipes with asynchronous access but they have huge
 * latency; stuff being placed into a queue often doesn't appear at the other
 * end for hundreds of milliseconds.  Hence we have a home-grown queuing
 * system in u_port_private.c using semaphore protection.
 * Implementation note 3: mutexes under Windows are always recursive so,
 * since we do not normally require that (and we test for it) a semaphore
 * with a count of 1 is used instead.
 * Implementation note 4: it is worth remembering that if you need a handle
 * to be asynchronous for one purpose it is asynchronous for ALL purposes.
 * For instance, if you want to do asynchronous reads from a handle you
 * MUST also do asynchronous writes, you can't split it.  For instance,
 * the COM API needs to be asynchronous for events, since the task that's
 * waiting on them needs to be able to wait on both the COM event and an
 * event to end itself; has to be asynchronous, can't block on the COM event
 * 'cos then you can't exit.  But that means that the COM port reads and
 * writes _also_ have to be asynchronous.  Similar with pipes: the reads
 * need to be asynchronous so that we can do a try-receive but that means
 * the writes must handle asynchronicity also.
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
# define MAKE_MTX_FN(x, ...) _ ## x ##__VA_ARGS__
#else
/** The normal case: a mutex function is not fiddled with.
 */
# define MAKE_MTX_FN(x, ...) x ##__VA_ARGS__
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
#define MTX_FN(x, ...) MAKE_MTX_FN(x ##__VA_ARGS__)

// Now undef U_CFG_MUTEX_DEBUG so that this file is not polluted
// by the u_mutex_debug.h stuff brought in through u_port_os.h.
#undef U_CFG_MUTEX_DEBUG

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "assert.h"

#include "windows.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
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

    // Could do this with SetThreadDescription() but that's not
    // available in the win32 API version packaged by all compilers.
    (void) pName;

    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {
        errorCode = uPortPrivateTaskCreate(pFunction,
                                           stackSizeBytes,
                                           pParameter,
                                           priority,
                                           pTaskHandle);
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    return uPortPrivateTaskDelete(taskHandle);
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return GetCurrentThreadId() == (DWORD) taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    int64_t startTimeMs;
    int64_t thisSleepMs;

    // Note: has to be SleepEx(), not Sleep(), so that the
    // thread is left in an alertable state and can therefore
    // be woken up by timers.
    while (delayMs > 0) {
        startTimeMs = GetTickCount64();
        SleepEx((DWORD) delayMs, true);
        // Sleep() may return early due to IO completion functions
        // going off; if this happens, go back to sleep again
        // for the remainder of the period
        thisSleepMs = GetTickCount64() - startTimeMs;
        if (thisSleepMs >= 0) {
            delayMs -= (int32_t) thisSleepMs;
        } else {
            // If the tick count happens to wrap then exit
            delayMs = 0;
        }
    }
}

// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    (void) taskHandle;
    // This makes no sense on Windows where stack sizes start
    // at around 1 Mbyte
    return U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the current task handle.
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pTaskHandle != NULL) {
        *pTaskHandle = (uPortTaskHandle_t) GetCurrentThreadId();
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pQueueHandle != NULL) {
        errorCode = uPortPrivateQueueAdd(itemSizeBytes, queueLength);
        if (errorCode >= 0) {
            *pQueueHandle = (uPortQueueHandle_t) errorCode;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    return uPortPrivateQueueRemove((int32_t) queueHandle);
}

// Send to the given queue.
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    return uPortPrivateQueueWrite((int32_t) queueHandle,
                                  (const char *) pEventData);
}

// Send to the given queue from an interrupt; not relevant
// on Windows
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Receive from the given queue, blocking.
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pEventData != NULL) {
        errorCode = uPortPrivateQueueRead((int32_t) queueHandle,
                                          (char *) pEventData, -1);
    }

    return (int32_t) errorCode;
}

// Receive from the given queue, non-blocking.
int32_t uPortQueueReceiveIrq(const uPortQueueHandle_t queueHandle,
                             void *pEventData)
{
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Receive from the given queue, with a wait time.
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pEventData != NULL) {
        errorCode = uPortPrivateQueueRead((int32_t) queueHandle,
                                          (char *) pEventData, waitMs);
    }

    return (int32_t) errorCode;
}

// Peek the given queue.
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData)
{
    return uPortPrivateQueuePeek((int32_t) queueHandle, (char *) pEventData);
}

// Get the number of free spaces in the given queue.
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    return uPortPrivateQueueGetFree((int32_t) queueHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t MTX_FN(uPortMutexCreate(uPortMutexHandle_t *pMutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    HANDLE mutexHandle;

    if (pMutexHandle != NULL) {
        mutexHandle = CreateSemaphore(NULL,  // default security attributes
                                      1, 1,  // initial count 1, max count 1
                                      NULL); // No name
        if (mutexHandle != NULL) {
            *pMutexHandle = (uPortMutexHandle_t) mutexHandle;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Destroy a mutex.
int32_t MTX_FN(uPortMutexDelete(const uPortMutexHandle_t mutexHandle))
{
    CloseHandle((HANDLE) mutexHandle);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Lock the given mutex.
int32_t MTX_FN(uPortMutexLock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != INVALID_HANDLE_VALUE) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (WaitForSingleObject((HANDLE) mutexHandle,
                                INFINITE) == WAIT_OBJECT_0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to lock the given mutex.
int32_t MTX_FN(uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                                 int32_t delayMs))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t x;

    if (mutexHandle != INVALID_HANDLE_VALUE) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        x = (int32_t) WaitForSingleObject((HANDLE) mutexHandle,
                                          (DWORD) delayMs);
        if (x == WAIT_OBJECT_0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        } else if (x == WAIT_TIMEOUT) {
            errorCode = U_ERROR_COMMON_TIMEOUT;
        }
    }

    return (int32_t) errorCode;
}

// Unlock the given mutex.
int32_t MTX_FN(uPortMutexUnlock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (mutexHandle != INVALID_HANDLE_VALUE) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (ReleaseSemaphore((HANDLE) mutexHandle, 1, NULL)) {
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
    HANDLE semaphoreHandle;

    if ((pSemaphoreHandle != NULL) && (limit != 0) && (initialCount <= limit)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        semaphoreHandle = CreateSemaphore(NULL,  // default security attributes
                                          (DWORD) initialCount,
                                          (DWORD) limit,
                                          NULL); // No name
        if (semaphoreHandle != INVALID_HANDLE_VALUE) {
            *pSemaphoreHandle = (uPortSemaphoreHandle_t) semaphoreHandle;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Destroy a semaphore.
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    CloseHandle((HANDLE) semaphoreHandle);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Take the given semaphore.
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != INVALID_HANDLE_VALUE) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (WaitForSingleObject((HANDLE) semaphoreHandle, INFINITE) == WAIT_OBJECT_0) {
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
    int32_t x;

    if (semaphoreHandle != INVALID_HANDLE_VALUE) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        x = (int32_t) WaitForSingleObject((HANDLE) semaphoreHandle, (DWORD) delayMs);
        if (x == WAIT_OBJECT_0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        } else if (x == WAIT_TIMEOUT) {
            errorCode = U_ERROR_COMMON_TIMEOUT;
        }
    }

    return (int32_t) errorCode;
}

// Give the semaphore.
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != INVALID_HANDLE_VALUE) {
        if (ReleaseSemaphore((HANDLE) semaphoreHandle, 1, NULL)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        } else {
            if (GetLastError() == ERROR_TOO_MANY_POSTS) {
                // Giving too many times is not an error
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Give the semaphore from interrupt.
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
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
    return uPortPrivateTimerStart(timerHandle);
}

// Stop a timer.
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    int32_t errroCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    if (CancelWaitableTimer((HANDLE) timerHandle)) {
        errroCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errroCode;
}

// Change a timer interval.
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    return uPortPrivateTimerChange(timerHandle, intervalMs);
}

// End of file
