/*
 * Copyright 2019-2023 u-blox
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
 * @brief Implementation of the port OS API for Linux.
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

#define _GNU_SOURCE

#include "stdlib.h"    // malloc() / free(), needed in special case of mutexCreate()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "assert.h"
#include "string.h"

#include "unistd.h"
#include "semaphore.h"
#include "sched.h"
#include "pthread.h"
#include "fcntl.h"
#include "time.h"
#include "signal.h"
#include "errno.h"


#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_private.h"

// Prototypes for the _ versions of uPortMutexXxx() in case U_CFG_MUTEX_DEBUG is defined
int32_t _uPortMutexCreate(uPortMutexHandle_t *pMutexHandle);
int32_t _uPortMutexDelete(uPortMutexHandle_t mutexHandle);
int32_t _uPortMutexLock(uPortMutexHandle_t mutexHandle);
int32_t _uPortMutexTryLock(uPortMutexHandle_t mutexHandle, int32_t delayMs);
int32_t _uPortMutexUnlock(uPortMutexHandle_t mutexHandle);

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Structures for storing os specific type data to be kept in linked lists. */

/** Queues are implemented using Posix pipes. A mutex is protecting the
 *  available count and a semaphore is used for synchronisation.
 *  Pipe descriptor 0 is used for reading and 1 for writing.
*/
typedef struct {
    uPortMutexHandle_t mutex;
    uPortSemaphoreHandle_t semHandle;
    int fd[2];            /*!< Pipe in/out descriptor. */
    size_t queueLength;   /*!< Max number of elements. */
    size_t itemSizeBytes; /*!< Element size */
    size_t readCount;     /*!< Unread bytes in the queue. */
} uPortQueue_t;

/** Timers are implemented using Posix timer_t timers.
*/
typedef struct {
    timer_t timerId;             /*!< Posix timer ID. */
    struct itimerspec timerSpec; /*!< Posix structure for timer start values and intervals.*/
    bool periodic;
    pTimerCallback_t *pCallback;
    void *pCallbackParam;
} uPortTimer_t;

/** Threads are implemented using Posix pthreads. As the Posix api wants the callback
 *  to return a void pointer we have to use this struct as a middle man.
*/
typedef struct {
    void (*pFunction)(void *);
    void *param;
} uPortThread_t;

/** Semaphores are implemented using Posix sem_t functions.
 *  These have no upper limit as required by ubxlib and we have
 *  to handle this ourselves.
 */
typedef struct {
    sem_t semaphore;
    uint32_t limit;
} uPortSemaphore_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Linked lists and their mutexes. These are needed for cleanup.
uPortMutexHandle_t gMutexThread = NULL;
uPortPrivateList_t *gpThreadList = NULL;

uPortMutexHandle_t gMutexTimer = NULL;
uPortPrivateList_t *gpTimerList = NULL;

// Posix has no suspend/resume functions for threads and this is needed
// for the critical section implementation of the port layer. We therefore
// use a mutex in combination with a Linux signal USR1 to achieve this.
// ** However this function is now disabled by default due to problems when
// interrupting things like uart reads.
// Can be enabled via U_PORT_LINUX_ENABLE_CRITICAL_SECTIONS

uPortMutexHandle_t gMutexCriticalSection = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Create a time structure by adding the specified number of
// milliseconds to the current clock time.
static void msToTimeSpec(int32_t ms, struct timespec *t, bool fromNow)
{
    struct timespec now = {0};
    if (fromNow) {
        timespec_get(&now, TIME_UTC);
    }
    t->tv_sec = now.tv_sec + ms / 1000;
    t->tv_nsec = now.tv_nsec + (ms % 1000) * 1000000;
    if (t->tv_nsec >= 1000000000) {
        t->tv_nsec -= 1000000000;
        t->tv_sec++;
    }
}

void threadSignalCallback(int sig)
{
    // Blocked wait for mutex when signal received.
    MTX_FN(uPortMutexLock(gMutexCriticalSection));
    MTX_FN(uPortMutexUnlock(gMutexCriticalSection));
}

// Posix threads want function returning void*
static void *taskProc(void *pParam)
{
    uPortThread_t info = *((uPortThread_t *)pParam);
    uPortFree(pParam);

    // Setup the signal used for suspending the thread.
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = threadSignalCallback;
    sigaction(SIGUSR1, &act, NULL);

    // Launch
    info.pFunction(info.param);
    return NULL;
}

// Suspend or resume all tasks but the current.
int32_t suspendOrResumeAllTasks(bool suspend)
{
    uErrorCode_t errorCode = errorCode = U_ERROR_COMMON_SUCCESS;
    MTX_FN(uPortMutexLock(gMutexThread));
    if (suspend) {
        errorCode = MTX_FN(uPortMutexTryLock(gMutexCriticalSection, 0));
        uPortPrivateList_t *p = gpThreadList;
        // Signal all tasks to suspend.
        while ((errorCode == U_ERROR_COMMON_SUCCESS) && (p != NULL)) {
            pthread_t threadId = (pthread_t)(p->ptr);
            if (threadId != pthread_self()) {
                if (pthread_kill(threadId, SIGUSR1) != 0) {
                    errorCode = U_ERROR_COMMON_PLATFORM;
                }
            }
            p = p->pNext;
        }
    } else {
        MTX_FN(uPortMutexUnlock(gMutexCriticalSection));
    }
    MTX_FN(uPortMutexUnlock(gMutexThread));
    uPortTaskBlock(100);
    return errorCode;
}

// Posix timer callback function in required format.
static void timerCallback(union sigval sv)
{
    uPortTimer_t *pTimer = (uPortTimer_t *)sv.sival_ptr;
    pTimer->pCallback(pTimer, pTimer->pCallbackParam);
}

// Read from a queue if an event is available.
static uErrorCode_t readFromQueue(uPortQueue_t *pQueue,
                                  void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_EMPTY;
    MTX_FN(uPortMutexLock(pQueue->mutex));
    if (pQueue->readCount >= pQueue->itemSizeBytes) {
        errorCode = U_ERROR_COMMON_TRUNCATED;
        size_t readCount = read(pQueue->fd[0], pEventData, pQueue->itemSizeBytes);
        if (readCount == pQueue->itemSizeBytes) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
        if (readCount >= 0) {
            pQueue->readCount -= readCount;
        } else {
            errorCode = U_ERROR_COMMON_PLATFORM;
        }
    }
    MTX_FN(uPortMutexUnlock(pQueue->mutex));
    return errorCode;
}

// Special version of uPortMutexCreate() which does not use
// pUPortMalloc(): required by uPortHeapMonitorInit() on this
// platform.
static int32_t mutexCreate(uPortMutexHandle_t *pMutexHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (pMutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        pthread_mutex_t *pMutex = malloc(sizeof(pthread_mutex_t));
        if (pMutex != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (pthread_mutex_init(pMutex, NULL) == 0) {
                *pMutexHandle = pMutex;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }
    return (int32_t)errorCode;
}

// Special version of uPortMutexLock() to go with mutexCreate().
static int32_t mutexLock(const uPortMutexHandle_t mutexHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (pthread_mutex_lock((pthread_mutex_t *)mutexHandle) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t) errorCode;
}

// Special version of uPortMutexUnLock() to go with mutexCreate().
static int32_t mutexUnlock(const uPortMutexHandle_t mutexHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (pthread_mutex_unlock((pthread_mutex_t *)mutexHandle) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

/* ----------------------------------------------------------------
 * PRIVATE FUNCTIONS SPECIFIC TO THIS PORT, MISC
 * -------------------------------------------------------------- */

// Initialise the private bits of the porting layer.
int32_t uPortPrivateInit(void)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    errorCode = uPortHeapMonitorInit(mutexCreate, mutexLock, mutexUnlock);
    if ((errorCode == 0) && (gMutexThread == NULL)) {
        errorCode = MTX_FN(uPortMutexCreate(&gMutexThread));
    }
    if ((errorCode == 0) && (gMutexTimer == NULL)) {
        errorCode = MTX_FN(uPortMutexCreate(&gMutexTimer));
    }
    if ((errorCode == 0) && (gMutexCriticalSection == NULL)) {
        errorCode = MTX_FN(uPortMutexCreate(&gMutexCriticalSection));
    }
    if (errorCode != 0) {
        // Tidy up on error
        if (gMutexThread != NULL) {
            MTX_FN(uPortMutexDelete(gMutexThread));
            gMutexThread = NULL;
        }
        if (gMutexTimer != NULL) {
            MTX_FN(uPortMutexDelete(gMutexTimer));
            gMutexTimer = NULL;
        }
        if (gMutexCriticalSection != NULL) {
            MTX_FN(uPortMutexDelete(gMutexCriticalSection));
            gMutexCriticalSection = NULL;
        }
    }
    return errorCode;
}

// De-initialise the private bits of the porting layer.
void uPortPrivateDeinit(void)
{
    if (gMutexTimer != NULL) {
        MTX_FN(uPortMutexLock(gMutexTimer));
        // Tidy away the timers
        uPortPrivateList_t *p = gpTimerList;
        while (p != NULL) {
            uPortTimer_t *pTimer = (uPortTimer_t *)(p->ptr);
            uPortTimerDelete(pTimer);
            uPortPrivateList_t *pNext = p->pNext;
            uPortPrivateListRemove(&gpTimerList, p);
            p = pNext;
        }
        MTX_FN(uPortMutexUnlock(gMutexTimer));
        MTX_FN(uPortMutexDelete(gMutexTimer));
        gMutexTimer = NULL;
    }

    if (gMutexThread != NULL) {
        // Note: cannot tidy away the tasks here,
        // we have no idea what state they are in,
        // that must be up to the user. Still we delete
        // the list and the mutex
        MTX_FN(uPortMutexLock(gMutexThread));
        uPortPrivateList_t *p = gpThreadList;
        while (p != NULL) {
            uPortPrivateList_t *pNext = p->pNext;
            uPortPrivateListRemove(&gpThreadList, p);
            p = pNext;
        }
        MTX_FN(uPortMutexUnlock(gMutexThread));
        MTX_FN(uPortMutexDelete(gMutexThread));
        gMutexThread = NULL;
    }
    if (gMutexCriticalSection != NULL) {
        MTX_FN(uPortMutexDelete(gMutexCriticalSection));
        gMutexCriticalSection = NULL;
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
    if (gMutexThread == NULL) {
        return U_ERROR_COMMON_NOT_INITIALISED;
    }
    uPortThread_t *pInfo = pUPortMalloc(sizeof(uPortThread_t));
    if (pInfo == NULL) {
        return U_ERROR_COMMON_NO_MEMORY;
    }
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    (void)pName;
    if ((pFunction != NULL) && (pTaskHandle != NULL) &&
        (priority >= U_CFG_OS_PRIORITY_MIN) &&
        (priority <= U_CFG_OS_PRIORITY_MAX)) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        pthread_attr_t attr;
        struct sched_param param;
        pthread_attr_init(&attr);
        pthread_attr_getschedparam(&attr, &param);
        param.sched_priority = priority;
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setstacksize(&attr, stackSizeBytes);
        // We assume that the sizeof threadId is equal to the size of a pointer.
        // Applies to both 32 and 64 bit Linux
        pthread_t threadId;
        pInfo->pFunction = pFunction;
        pInfo->param = pParameter;
        if (pthread_create(&threadId, &attr, taskProc, (void *)pInfo) == 0) {
            *pTaskHandle = (void *)threadId;
            errorCode = U_ERROR_COMMON_SUCCESS;
            MTX_FN(uPortMutexLock(gMutexThread));
            uPortPrivateListAdd(&gpThreadList, (void *)threadId);
            MTX_FN(uPortMutexUnlock(gMutexThread));
        }
    }
    return (int32_t)errorCode;
}

// Delete the given task.
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    if (gMutexThread == NULL) {
        return U_ERROR_COMMON_NOT_INITIALISED;
    }
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    pthread_t tread = taskHandle == NULL ? pthread_self() : (pthread_t)taskHandle;
    if (pthread_cancel(tread) == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    MTX_FN(uPortMutexLock(gMutexThread));
    uPortPrivateListRemove(&gpThreadList, (void *)tread);
    MTX_FN(uPortMutexUnlock(gMutexThread));
    return (int32_t)errorCode;
}

// Check if the current task handle is equal to the given task handle.
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return pthread_self() == (pthread_t)taskHandle;
}

// Block the current task for a time.
void uPortTaskBlock(int32_t delayMs)
{
    usleep(delayMs * 1000);
}

// Get the minimum free stack for a given task.
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    (void) taskHandle;
    return U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the current task handle.
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (pTaskHandle != NULL) {
        *pTaskHandle = (uPortTaskHandle_t)pthread_self();
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
        uPortQueue_t *pQueue = (uPortQueue_t *)pUPortMalloc(sizeof(uPortQueue_t));
        if (pQueue) {
            uPortMutexHandle_t mutex;
            uPortSemaphoreHandle_t semHandle;
            if (uPortSemaphoreCreate(&semHandle, 0, queueLength) != 0) {
                uPortFree(pQueue);
            } else if (MTX_FN(uPortMutexCreate(&mutex)) == 0) {
                // Create a non blocking pipe.
                if (pipe2(pQueue->fd, O_NONBLOCK) == 0) {
                    if (queueLength * itemSizeBytes < fcntl(pQueue->fd[1], F_GETPIPE_SZ)) {
                        pQueue->mutex = mutex;
                        pQueue->semHandle = semHandle;
                        pQueue->queueLength = queueLength;
                        pQueue->itemSizeBytes = itemSizeBytes;
                        pQueue->readCount = 0;
                        *pQueueHandle = pQueue;
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    MTX_FN(uPortMutexDelete(mutex));
                    uPortFree(pQueue);
                }
            }
        }
    }
    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortQueue_t *pQueue = (uPortQueue_t *)queueHandle;
    if (pQueue != NULL) {
        close(pQueue->fd[0]);
        close(pQueue->fd[1]);
        MTX_FN(uPortMutexDelete(pQueue->mutex));
        uPortSemaphoreDelete(pQueue->semHandle);
        uPortFree(pQueue);
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    return (int32_t)errorCode;
}

// Send to the given queue.
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortQueue_t *pQueue = (uPortQueue_t *)queueHandle;
    if (pQueue != NULL) {
        errorCode = U_ERROR_COMMON_TRUNCATED;
        MTX_FN(uPortMutexLock(pQueue->mutex));
        size_t writeCount = write(pQueue->fd[1], pEventData, pQueue->itemSizeBytes);
        if (writeCount == pQueue->itemSizeBytes) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
        if (writeCount >= 0) {
            pQueue->readCount += writeCount;
        } else {
            errorCode = U_ERROR_COMMON_PLATFORM;
        }
        MTX_FN(uPortMutexUnlock(pQueue->mutex));
        uPortSemaphoreGive(pQueue->semHandle);
    }
    return (int32_t)errorCode;
}

// Send to the given queue from an interrupt; not relevant
// on Linux
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Receive from the given queue, blocking.
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    uPortQueue_t *pQueue = (uPortQueue_t *)queueHandle;
    uErrorCode_t errorCode = readFromQueue(pQueue, pEventData);
    while (errorCode == U_ERROR_COMMON_EMPTY) {
        // Not available, blocking wait.
        uPortSemaphoreTake(pQueue->semHandle);
        errorCode = readFromQueue(pQueue, pEventData);
    }
    return (int32_t)errorCode;
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
    uPortQueue_t *pQueue = (uPortQueue_t *)queueHandle;
    uErrorCode_t errorCode = readFromQueue(pQueue, pEventData);
    if (errorCode != U_ERROR_COMMON_SUCCESS) {
        // Not available, timeout wait.
        errorCode = uPortSemaphoreTryTake(pQueue->semHandle, waitMs);
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            errorCode = readFromQueue(pQueue, pEventData);
        }
    }
    return (int32_t)errorCode;
}

// Peek the given queue.
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData)
{
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the number of free spaces in the given queue.
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    uPortQueue_t *pQueue = (uPortQueue_t *)queueHandle;
    if (pQueue != NULL) {
        size_t maxFree = pQueue->queueLength * pQueue->itemSizeBytes;
        size_t freeBytes = fcntl(pQueue->fd[1], F_GETPIPE_SZ) - pQueue->readCount;
        if (freeBytes > maxFree) {
            freeBytes = maxFree;
        }
        return (freeBytes / pQueue->itemSizeBytes);
    } else {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
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
        pthread_mutex_t *pMutex = pUPortMalloc(sizeof(pthread_mutex_t));
        if (pMutex != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (pthread_mutex_init(pMutex, NULL) == 0) {
                *pMutexHandle = pMutex;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }
    return (int32_t)errorCode;
}

// Destroy a mutex.
int32_t MTX_FN(uPortMutexDelete(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (pthread_mutex_destroy((pthread_mutex_t *)mutexHandle) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            uPortFree(mutexHandle);
        }
    }
    return (int32_t)errorCode;
}

// Lock the given mutex.
int32_t MTX_FN(uPortMutexLock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (pthread_mutex_lock((pthread_mutex_t *)mutexHandle) == 0) {
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
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (delayMs == 0) {
            int32_t sta = pthread_mutex_trylock((pthread_mutex_t *)mutexHandle);
            if (sta == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (errno == EAGAIN) {
                errorCode = U_ERROR_COMMON_TIMEOUT;
                // Clear errno in this case
                errno = 0;
            }
        } else {
            struct timespec t;
            msToTimeSpec(delayMs, &t, true);
            int32_t sta = pthread_mutex_timedlock((pthread_mutex_t *)mutexHandle, &t);
            if (sta == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (sta == ETIMEDOUT) {
                errorCode = U_ERROR_COMMON_TIMEOUT;
            }
        }
    }
    return (int32_t)errorCode;
}

// Unlock the given mutex.
int32_t MTX_FN(uPortMutexUnlock(const uPortMutexHandle_t mutexHandle))
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (mutexHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (pthread_mutex_unlock((pthread_mutex_t *)mutexHandle) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEMAPHORES
 * -------------------------------------------------------------- */

// Create a semaphore.
int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit)
{
    bool valid = pSemaphoreHandle != NULL &&
                 limit > 0 &&
                 initialCount <= limit;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (valid) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        uPortSemaphore_t *pSemaphore = pUPortMalloc(sizeof(uPortSemaphore_t));
        if (pSemaphore != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (sem_init(&(pSemaphore->semaphore), 0, initialCount) == 0) {
                pSemaphore->limit = limit;
                *pSemaphoreHandle = pSemaphore;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else {
                uPortFree(pSemaphore);
            }
        }
    }
    return (int32_t)errorCode;
}

// Destroy a semaphore.
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortSemaphore_t *pSemaphore = (uPortSemaphore_t *)semaphoreHandle;
        if (sem_destroy(&pSemaphore->semaphore) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            uPortFree(semaphoreHandle);
        }
    }
    return (int32_t)errorCode;
}

// Take the given semaphore.
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortSemaphore_t *pSemaphore = (uPortSemaphore_t *)semaphoreHandle;
        if (sem_wait(&pSemaphore->semaphore) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

// Try to take the given semaphore.
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortSemaphore_t *pSemaphore = (uPortSemaphore_t *)semaphoreHandle;
        if (delayMs == 0) {
            int32_t sta = sem_trywait(&pSemaphore->semaphore);
            if (sta == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (errno == EAGAIN) {
                errorCode = U_ERROR_COMMON_TIMEOUT;
                // Clear errno in this case
                errno = 0;
            }
        } else {
            struct timespec t;
            msToTimeSpec(delayMs, &t, true);
            int32_t sta = sem_timedwait(&pSemaphore->semaphore, &t);
            if (sta == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else if (errno == ETIMEDOUT) {
                errorCode = U_ERROR_COMMON_TIMEOUT;
            }
        }
    }
    return (int32_t)errorCode;
}

// Give the semaphore.
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortSemaphore_t *pSemaphore = (uPortSemaphore_t *)semaphoreHandle;
        int currVal;
        if (sem_getvalue(&pSemaphore->semaphore, &currVal) == 0) {
            if (currVal >= pSemaphore->limit) {
                // Limit has been reached, wait for release.
                sem_wait(&pSemaphore->semaphore);
            }
            if (sem_post(&pSemaphore->semaphore) == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }
    return (int32_t)errorCode;
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
    (void)pName;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (pTimerHandle != NULL) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        uPortTimer_t *pTimer = pUPortMalloc(sizeof(uPortTimer_t));
        if (pTimer != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            struct sigevent sev = {0};
            sev.sigev_notify = SIGEV_THREAD;
            sev.sigev_notify_function = timerCallback;
            sev.sigev_value.sival_ptr = (void *)pTimer;
            pTimer->periodic = periodic;
            pTimer->pCallback = pCallback;
            pTimer->pCallbackParam = pCallbackParam;
            msToTimeSpec((int32_t)intervalMs, &(pTimer->timerSpec.it_value), false);
            msToTimeSpec(pTimer->periodic ? (int32_t)intervalMs : 0,
                         &(pTimer->timerSpec.it_interval), false);
            if (timer_create(CLOCK_REALTIME, &sev, &(pTimer->timerId)) == 0) {
                *pTimerHandle = (uPortTimerHandle_t *)pTimer;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else {
                uPortFree(pTimer);
            }
        }
    }
    return (int32_t)errorCode;
}

// Destroy a timer.
int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (timerHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortTimer_t *pTimer = (uPortTimer_t *)timerHandle;
        if (timer_delete(pTimer->timerId) == 0) {
            uPortFree(pTimer);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

// Start a timer.
int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (timerHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortTimer_t *pTimer = (uPortTimer_t *)timerHandle;
        if (timer_settime(pTimer->timerId, 0, &(pTimer->timerSpec), NULL) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

// Stop a timer.
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (timerHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        uPortTimer_t *pTimer = (uPortTimer_t *)timerHandle;
        struct itimerspec its;
        memset(&its, 0, sizeof(its));
        if (timer_settime(pTimer->timerId, 0, &its, NULL) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

// Change a timer interval.
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (timerHandle != NULL) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        uPortTimer_t *pTimer = (uPortTimer_t *)timerHandle;
        msToTimeSpec((int32_t)intervalMs, &(pTimer->timerSpec.it_value), false);
        msToTimeSpec(pTimer->periodic ? (int32_t)intervalMs : 0,
                     &(pTimer->timerSpec.it_interval), false);
    }
    return (int32_t)errorCode;
}

// Enter a critical section.
int32_t uPortEnterCritical()
{
#ifdef U_PORT_LINUX_ENABLE_CRITICAL_SECTIONS
    return suspendOrResumeAllTasks(true);
#else
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
#endif
}

// Leave a critical section.
void uPortExitCritical()
{
#ifdef U_PORT_LINUX_ENABLE_CRITICAL_SECTIONS
    suspendOrResumeAllTasks(false);
#endif
}

// End of file
