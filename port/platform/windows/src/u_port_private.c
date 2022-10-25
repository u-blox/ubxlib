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
 * @brief Stuff private to the Windows porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "windows.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_private.h"

#ifdef U_CFG_MUTEX_DEBUG
/** Grab the handle of the mutex watchdog task; this is so
 * that, on Windows, when we simulate a critical section, we can
 * leave it running to catch situations where we might end up
 * sitting in a critical section for _far_ longer than we should..
 */
extern uPortTaskHandle_t gMutexDebugWatchdogTaskHandle;
#else
uPortTaskHandle_t gMutexDebugWatchdogTaskHandle = NULL;
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The lowest value that a queue handle can have; we avoid 0 since
 * there may be checks for NULL-ness floating around that it
 * would be prudent to avoid.
 */
#define U_PORT_PRIVATE_QUEUE_HANDLE_MIN 1

/** Macro to convert milliseconds into a Windows waitable timer
 * "due time" value; the "due time" is in nanoseconds and must be
 * negative to give a relative time.
 */
#define U_PORT_PRIVATE_MS_TO_DUE_TIME(ms) (((int64_t) ms) * -10000)

#ifdef U_CFG_MUTEX_DEBUG
/** Bypass mutex debug for lock: On Windows the functions
 * uPortPrivateEnterCritical()/uPortPrivateExitCritical() are
 * _simulations_ of critical sections rather than actual critical
 * sections (which don't exist on Windows).  As such they _do_ lock
 * mutexes, which causes a problem when U_CFG_MUTEX_DEBUG is defined
 * since some other task that the critical section simulation has
 * suspended may have locked the gMutexList mutex which U_CFG_MUTEX_DEBUG
 * itself uses.  Hence, just for the uPortPrivateEnterCritical()/
 * uPortPrivateExitCritical() code, we need to go "around" the
 * mutex debug code by using the macro below.
 */
#define U_PORT_MUTEX_LOCK_NO_MUTEX_DEBUG(x)      { U_ASSERT(_uPortMutexLock(((uPortPrivatePartialMutexInfo_t *) x)->handle) == 0)

/** Bypass mutex debug for unlock: On Windows the functions
 * uPortPrivateEnterCritical()/uPortPrivateExitCritical() are
 * _simulations_ of critical sections rather than actual critical
 * sections (which don't exist on Windows).  As such they _do_ lock
 * mutexes, which causes a problem when U_CFG_MUTEX_DEBUG is defined
 * since some other task that the critical section simulation has
 * suspended may have locked the gMutexList mutex which U_CFG_MUTEX_DEBUG
 * itself uses.  Hence, just for the uPortPrivateEnterCritical()/
 * uPortPrivateExitCritical() code, we need to go "around" the
 * mutex debug code by using the macro below.
 */
#define U_PORT_MUTEX_UNLOCK_NO_MUTEX_DEBUG(x)    } U_ASSERT(_uPortMutexUnlock(((uPortPrivatePartialMutexInfo_t *) x)->handle) == 0)
#else
/** Mutex lock helper macro.
 */
#define U_PORT_MUTEX_LOCK_NO_MUTEX_DEBUG       U_PORT_MUTEX_LOCK

/** Mutex unlock helper macro.
 */
#define U_PORT_MUTEX_UNLOCK_NO_MUTEX_DEBUG     U_PORT_MUTEX_UNLOCK
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold a queue as part of a linked list.
 */
typedef struct uPortPrivateQueue_t {
    int32_t queueHandle;
    size_t itemSizeBytes;
    size_t bufferSizeBytes;
    char *pBuffer;
    char *pWrite;
    char *pRead;
    uPortSemaphoreHandle_t writeSemaphore;
    uPortSemaphoreHandle_t readSemaphore;
    uPortSemaphoreHandle_t accessSemaphore;
    struct uPortPrivateQueue_t *pNext;
} uPortPrivateQueue_t;

/** Type to hold timer information.
 */
typedef struct uPortPrivateTimer_t {
    uPortTimerHandle_t handle;
    pTimerCallback_t *pCallback;
    void *pCallbackParam;
    LARGE_INTEGER dueTime;
    uint32_t periodMs;
    struct uPortPrivateTimer_t *pNext;
} uPortPrivateTimer_t;

/** A structure that allows us to get at the mutex handle
 * when U_CFG_MUTEX_DEBUG is defined, letting us sneak around
 * the mutex debug operation when simulating critical sections,
 * but without having to export the uMutexInfo_t structure.
 */
typedef struct {
    uPortMutexHandle_t handle;
} uPortPrivatePartialMutexInfo_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the linked list of threads.
 */
static uPortMutexHandle_t gMutexThread = NULL;

/** For debug purposes.
 */
static bool gInCriticalSection = false;

/** Mutex to protect the linked list of queues.
 */
static uPortMutexHandle_t gMutexQueue = NULL;

/** A hook for the linked list of queues.
 */
static uPortPrivateQueue_t *gpQueueRoot = NULL;

/** The next queue handle to use.
 */
static int32_t gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;

/** Mutex to protect the linked list of timers.
 */
static uPortMutexHandle_t gMutexTimer = NULL;

/** A hook for the linked list of timers.
 */
static uPortPrivateTimer_t *gpTimerList = NULL;

/** Convert a local task priority value into a Windows one.
 */
static const int32_t localToWinPriority[] = {-2,  // 0
                                             -2,  // 1
                                             -2,  // 2
                                             -2,  // 3
                                             -1,  // 4
                                             -1,  // 5
                                             -1,  // 6
                                             -1,  // 7
                                             0,   // 8
                                             0,   // 9
                                             0,   // 10
                                             0,   // 11
                                             1,   // 12
                                             1,   // 13
                                             1,   // 14
                                             1    // 15
                                             };

/** List of thread IDs.  We only need to keep track of the IDs
 * in order to implement a version of crtitical section
 * on Windows, hence just a simple array.
 * From this article:
 * https://devblogs.microsoft.com/oldnewthing/20040223-00/?p=40503
 * ...zero would appear to be the invalid thread ID,
 * which is fortunate, 'cos it's a bit like NULL and
 * we use that to indicate "self".
 */
static DWORD gThreadIds[U_PORT_MAX_NUM_TASKS] = {0};

/** The list of thread IDs that have been suspended, so
 * that the critical section code can resume them.
 */
static DWORD gThreadIdsSuspended[U_PORT_MAX_NUM_TASKS] = {0};

/** A place to put the ID of the main thread, not static
 * because it is needed by uPortPlatformStart() over in
 * u_port.c, which kicks it off; it is the first entry in
 * the array of threads.
 */
DWORD *gpMainThreadId = &(gThreadIds[0]);

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: THREADS
 * -------------------------------------------------------------- */

// Find an entry in the array of thread IDs.
// gMutexThread should be locked before this is called.
static DWORD *pThreadIdFind(int32_t threadId)
{
    DWORD *pFound = NULL;

    for (size_t x = 0; (pFound == NULL) &&
         (x < sizeof(gThreadIds) / sizeof(gThreadIds[0])); x++) {
        if (gThreadIds[x] == threadId) {
            pFound = &(gThreadIds[x]);
        }
    }

    return pFound;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

// Find a queue in the list by handle (i.e. index) and return
// a pointer to it.
// gMutexQueue should be locked before this is called.
static uPortPrivateQueue_t *pQueueFind(int32_t handle)
{
    uPortPrivateQueue_t *pTmp = gpQueueRoot;
    uPortPrivateQueue_t *pFound = NULL;

    while ((pTmp != NULL) && (pFound == NULL)) {
        if (pTmp->queueHandle == handle) {
            pFound = pTmp;
        } else {
            pTmp = pTmp->pNext;
        }
    }

    return pFound;
}

// Return a copy of a queue from the list.
// gMutexQueue should be locked before this is called.
static int32_t queueGetCopy(int32_t handle, uPortPrivateQueue_t *pQueue)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pFound;

    pFound = pQueueFind(handle);
    if (pFound != NULL) {
        *pQueue = *pFound;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Write data to the queue and increment pointers.
// gMutexQueue should be locked before this is called.
static int32_t queueWrite(int32_t handle, const char *pData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pQueue;

    pQueue = pQueueFind(handle);
    if (pQueue != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        // Copy in the data and advance the write pointer
        memcpy(pQueue->pWrite, pData, pQueue->itemSizeBytes);
        pQueue->pWrite += pQueue->itemSizeBytes;
        if (pQueue->pWrite >= pQueue->pBuffer + pQueue->bufferSizeBytes) {
            pQueue->pWrite = pQueue->pBuffer;
        }
    }

    return errorCode;
}

// Read data from the queue and increment pointers.
// gMutexQueue should be locked before this is called.
static int32_t queueRead(int32_t handle, char *pData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pQueue;

    pQueue = pQueueFind(handle);
    if (pQueue != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (pData != NULL) {
            // Copy out the data
            memcpy(pData, pQueue->pRead, pQueue->itemSizeBytes);
        }
        // Advance the read pointer
        pQueue->pRead +=  pQueue->itemSizeBytes;
        if (pQueue->pRead >=  pQueue->pBuffer +  pQueue->bufferSizeBytes) {
            pQueue->pRead =  pQueue->pBuffer;
        }
    }

    return errorCode;
}

// Free memory held by a queue and the queue entry itself.
// gMutexQueue should be locked before this is called.
static void queueFree(uPortPrivateQueue_t *pQueue)
{
    uPortSemaphoreDelete(pQueue->accessSemaphore);
    uPortSemaphoreDelete(pQueue->writeSemaphore);
    uPortSemaphoreDelete(pQueue->readSemaphore);
    // It is valid C to free a NULL buffer
    uPortFree(pQueue->pBuffer);
    uPortFree(pQueue);
}

// Remove a queue from the list, freeing memory.
// gMutexQueue should be locked before this is called.
static void queueRemove(int32_t handle)
{
    uPortPrivateQueue_t *pTmp = gpQueueRoot;
    uPortPrivateQueue_t *pPrevious = NULL;

    while (pTmp != NULL) {
        if (pTmp->queueHandle == handle) {
            if (pPrevious == NULL) {
                // At head
                gpQueueRoot = pTmp->pNext;
            } else {
                pPrevious->pNext = pTmp->pNext;
            }
            // Free memory
            queueFree(pTmp);
            // Force exit
            pTmp = NULL;
        } else {
            pPrevious = pTmp;
            pTmp = pTmp->pNext;
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

// Find a timer in the list by handle (i.e. index) and return
// a pointer to it.
// gMutexTimer should be locked before this is called.
static uPortPrivateTimer_t *pTimerFind(uPortTimerHandle_t handle)
{
    uPortPrivateTimer_t *pTmp = gpTimerList;

    while ((pTmp != NULL) && (pTmp->handle != handle)) {
        pTmp = pTmp->pNext;
    }

    return pTmp;
}

// Remove a timer from the list, freeing memory.
// gMutexTimer should be locked before this is called.
static void timerRemove(uPortTimerHandle_t handle)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;
    uPortPrivateTimer_t *pPrevious = NULL;

    // Find the entry in the list
    while ((pTimer != NULL) && (pTimer->handle != handle)) {
        pPrevious = pTimer;
        pTimer = pTimer->pNext;
    }
    if (pTimer != NULL) {
        // Remove the entry from the list
        if (pPrevious != NULL) {
            pPrevious->pNext = pTimer->pNext;
        } else {
            // Must be at head
            gpTimerList = pTimer->pNext;
        }
        // Free the entry
        uPortFree(pTimer);
    }
}

// The timer expiry callback.
static void timerCallback(LPVOID handle, DWORD dwTimerLowValue,
                          DWORD dwTimerHighValue)
{
    uPortPrivateTimer_t *pTimer;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;

    if (gMutexTimer != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimer);

        pTimer = pTimerFind((uPortTimerHandle_t) handle);
        if (pTimer != NULL) {
            pCallback = pTimer->pCallback;
            pCallbackParam = pTimer->pCallbackParam;
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimer);

        // Call the callback outside the locks so that the
        // callback itself may call the timer API
        if (pCallback != NULL) {
            pCallback((uPortTimerHandle_t) handle, pCallbackParam);
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Wait on a semaphore for queue operation, optionally with debug.
static int32_t semphoreTake(const uPortSemaphoreHandle_t semaphoreHandle,
                            int32_t queueHandle)
{
    int32_t errorCode;

#ifdef U_CFG_QUEUE_DEBUG
    size_t x = 0;
    do {
        errorCode = uPortSemaphoreTryTake(semaphoreHandle, 0);
        if (errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) {
            if (x % (1000 / U_CFG_OS_YIELD_MS) == 0) {
                // Print this roughly once a second
                uPortLog("U_PORT_OS_QUEUE_DEBUG: queue %d is full, retrying...\n",
                         queueHandle);
            }
            x++;
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }
    } while (errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT);
#else
    (void) queueHandle;
    errorCode = uPortSemaphoreTake(semaphoreHandle);
#endif

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, MISC
 * -------------------------------------------------------------- */

// Initialise the private bits of the porting layer.
int32_t uPortPrivateInit(void)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexThread == NULL) {
        errorCode = uPortMutexCreate(&gMutexThread);
    }

    if ((errorCode == 0) && (gMutexQueue == NULL)) {
        errorCode = uPortMutexCreate(&gMutexQueue);
    }

    if ((errorCode == 0) && (gMutexTimer == NULL)) {
        errorCode = uPortMutexCreate(&gMutexTimer);
    }

    if (errorCode != 0) {
        // Tidy up on error
        if (gMutexThread != NULL) {
            uPortMutexDelete(gMutexThread);
            gMutexThread = NULL;
        }
        if (gMutexQueue != NULL) {
            uPortMutexDelete(gMutexQueue);
            gMutexQueue = NULL;
        }
        if (gMutexTimer != NULL) {
            uPortMutexDelete(gMutexTimer);
            gMutexTimer = NULL;
        }
    }

    return errorCode;
}

// Deinitialise the private bits of the porting layer.
void uPortPrivateDeinit(void)
{
    if (gMutexTimer != NULL) {
        U_PORT_MUTEX_LOCK(gMutexTimer);
        // Tidy away the timers
        while (gpTimerList != NULL) {
            CloseHandle((HANDLE) gpTimerList->handle);
            timerRemove(gpTimerList->handle);
        }
        U_PORT_MUTEX_UNLOCK(gMutexTimer);
        uPortMutexDelete(gMutexTimer);
        gMutexTimer = NULL;
    }

    if (gMutexQueue != NULL) {
        U_PORT_MUTEX_LOCK(gMutexQueue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);
        uPortMutexDelete(gMutexQueue);
        gMutexQueue = NULL;
    }

    if (gMutexThread != NULL) {
        // Note: cannot tidy away the tasks here,
        // we have no idea what state they are in,
        // that must be up to the user
        U_PORT_MUTEX_LOCK(gMutexThread);
        U_PORT_MUTEX_UNLOCK(gMutexThread);
        uPortMutexDelete(gMutexThread);
        gMutexThread = NULL;
    }
}

// Enter a critical section.
int32_t uPortPrivateEnterCritical()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    HANDLE threadHandle;
    DWORD thisThreadId = GetCurrentThreadId();

    if (gMutexThread != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_LOCK_NO_MUTEX_DEBUG(gMutexThread);

        U_ASSERT(!gInCriticalSection);

        // Suspend all of the tasks in the list except ourselves
        for (size_t x = 0; (errorCode == 0) &&
             (x < sizeof(gThreadIds) / sizeof(gThreadIds[0])); x++) {
            if ((gThreadIds[x] != 0) && (gThreadIds[x] != thisThreadId) &&
                ((gMutexDebugWatchdogTaskHandle == NULL) ||
                 (gThreadIds[x] != (DWORD) gMutexDebugWatchdogTaskHandle))) {
                threadHandle = OpenThread(THREAD_SUSPEND_RESUME, false, gThreadIds[x]);
                // Note: if we get INVALID_HANDLE_VALUE back from OpenThread then the
                // thread must have exited already
                if (threadHandle != NULL) {
                    if (SuspendThread(threadHandle) >= 0) {
                        gThreadIdsSuspended[x] = gThreadIds[x];
                    } else if (GetLastError() != ERROR_INVALID_HANDLE) {
                        // If SuspendThread returns non-zero and it's NOT because
                        // the handle has become invalid (e.g. if the thread
                        // has been terminated or has terminated itself
                        // between us opening a handle on it and trying to
                        // terminate it) then this has failed
                        U_ASSERT(false);
                    }
                    CloseHandle(threadHandle);
                }
            }
        }

        gInCriticalSection = true;

        U_PORT_MUTEX_UNLOCK_NO_MUTEX_DEBUG(gMutexThread);
    }

    return (int32_t) errorCode;
}

// Leave a critical section.
int32_t uPortPrivateExitCritical()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    HANDLE threadHandle;
    DWORD thisThreadId = GetCurrentThreadId();

    if (gMutexThread != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_LOCK_NO_MUTEX_DEBUG(gMutexThread);

        U_ASSERT(gInCriticalSection);

        // Resume all of the suspended tasks
        for (size_t x = 0; (errorCode == 0) &&
             (x < sizeof(gThreadIdsSuspended) / sizeof(gThreadIdsSuspended[0])); x++) {
            if (gThreadIdsSuspended[x] != 0) {
                threadHandle = OpenThread(THREAD_SUSPEND_RESUME, false, gThreadIdsSuspended[x]);
                // Note: if we get INVALID_HANDLE_VALUE back from OpenThread then the
                // thread must have been terminated
                if (threadHandle != NULL) {
                    if ((ResumeThread(threadHandle) < 0) && (GetLastError() != ERROR_INVALID_HANDLE)) {
                        // If ResumeThread returns -1 and it's NOT because
                        // the handle has become invalid (e.g. if the thread
                        // has been terminated between us opening a handle on
                        // it and trying to terminate it) then this has failed
                        U_ASSERT(false);
                    }
                    CloseHandle(threadHandle);
                }
            }
        }
        memset(gThreadIdsSuspended, 0, sizeof(gThreadIdsSuspended));

        gInCriticalSection = false;

        U_PORT_MUTEX_UNLOCK_NO_MUTEX_DEBUG(gMutexThread);
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, TASKS
 * -------------------------------------------------------------- */

// Create a task.
int32_t uPortPrivateTaskCreate(void (*pFunction)(void *),
                               size_t stackSizeBytes,
                               void *pParameter,
                               int32_t priority,
                               uPortTaskHandle_t *pTaskHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    DWORD *pThreadId = NULL;
    HANDLE threadHandle;

    // Important: it is possible for this to be called at start
    // of day, e.g. by the uMutexDebug initialisation code,
    // before port initialsation has been called, hence we only
    // lock the mutex if it has been set up.
    if (gMutexThread != NULL) {
        // Can't use the lock macro here as it adds bracketing
        // for safety and calling it within braces in this
        // way would mess us up
        uPortMutexLock(gMutexThread);
    }

    // Find a free entry in the array
    pThreadId = pThreadIdFind(0);
    if (pThreadId != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
        threadHandle = CreateThread(NULL, // default security attributes
                                    (DWORD) stackSizeBytes,
                                    (LPTHREAD_START_ROUTINE) pFunction, pParameter,
                                    0,     // default creation flags
                                    pThreadId);
        if (threadHandle != INVALID_HANDLE_VALUE) {
            if (SetThreadPriority(threadHandle,
                                  (DWORD) uPortPrivateTaskPriorityConvert(priority))) {
                *pTaskHandle = (uPortTaskHandle_t) * pThreadId;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                TerminateThread(threadHandle, 0);
            }
            // Don't need this handle any more: this does
            // not delete the thread, don't worry
            CloseHandle(threadHandle);
        }
    }

    if (gMutexThread != NULL) {
        uPortMutexUnlock(gMutexThread);
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t uPortPrivateTaskDelete(const uPortTaskHandle_t taskHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    DWORD *pThreadId = NULL;
    DWORD threadId = (DWORD) taskHandle;
    HANDLE threadHandle;

    if (gMutexThread != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

        U_PORT_MUTEX_LOCK(gMutexThread);

        if (taskHandle == NULL) {
            threadId = GetCurrentThreadId();
        }
        // Find the entry in the array, only so that we can clear it
        pThreadId = pThreadIdFind(threadId);
        if (pThreadId != NULL) {
            if (taskHandle == NULL) {
                // Current thread: just exit, unlocking gMutexThread first
                *pThreadId = 0;
                uPortMutexUnlock(gMutexThread);
                ExitThread(0);
                // CODE EXECUTION NEVER GETS HERE
            } else {
                threadHandle = OpenThread(THREAD_TERMINATE, false, threadId);
                if (threadHandle != INVALID_HANDLE_VALUE) {
                    if (TerminateThread(threadHandle, 0) || (GetLastError() == ERROR_INVALID_HANDLE)) {
                        // Success if the terminate succeeds or if it returns
                        // the error "invalid handle", as that must mean the
                        // task has terminated itself between us opening a
                        // handle on it and trying to terminate it
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                    CloseHandle(threadHandle);
                } else {
                    // OpenThread returns INVALID_HANDLE_VALUE if the
                    // thread in question has already terminated so
                    // this is actually a success
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }

            // Clear the entry in the array
            if (errorCode == 0) {
                *pThreadId = 0;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexThread);
    }

    return (int32_t) errorCode;
}

// Convert to Windows thread priority.
int32_t uPortPrivateTaskPriorityConvert(int32_t priority)
{
    if (priority < 0) {
        priority = 0;
    }
    if (priority > sizeof(localToWinPriority) - 1) {
        priority = sizeof(localToWinPriority) - 1;
    }

    return localToWinPriority[priority];
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, QUEUES
 * -------------------------------------------------------------- */

/* Note: this implementation may seem more complex than it needs to
 * be!  Reasoning goes like this:
 *
 * 1.  We need a global mutex to protect the linked list of queues.
 * 2.  However, we obviously can't use that global mutex to protect
 *     _usage_ of any one queue, as holding the lock during the blocking
 *     read() would prevent a write() occurring.
 * 3.  So we take a copy of the queue entry while we work on it.
 * 4.  We employ a "write" semaphore whose maximum value is the maximum
 *     number of items in the queue, this way we can take() that
 *     semaphore to know that there is space to write to.
 * 5.  To actually do a write(), manipulating the pointers, we need to
 *     lock the global mutex again, to prevent the pointers being
 *     accessed by another write or disappearing from under us should
 *     the queue be closed.
 * 6.  However, how do we know that won't have happened between taking
 *     the "write" semaphore and obtaining the global mutex lock? To
 *     cover this, each queue also has an "access" semaphore.
 * 7.  When we have taken the "write" semaphore we take the "access"
 *     semaphore and then we can lock the global mutex.
 * 8.  Now of course, the queue may have been closed in the time
 *     between taking the "access" semaphore and locking the global
 *     mutex.  So, once we get inside the global mutex lock, we
 *     give() the "access" semaphore: if that semaphore has been
 *     vapourised by the queue being closed, give()ing it will return
 *     an error and we know not to go writing things to pointers that
 *     no longer exist.
 * 9.  We write the data into the queue and advance pointers.
 * 10. We indicate that something has been written by give()ing a
 *     "read" semaphore.
 * 11. The global mutex is released, job done.
 * 12. The read() process is similar, this time waiting on the "read"
 *     semaphore (which was give()n by the write() function) and
 *     give()ing the "write" semaphore.
 */

// Add a queue to the list, returning its handle.
int32_t uPortPrivateQueueAdd(size_t itemSizeBytes, size_t maxNumItems)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t x;
    size_t bufferSizeBytes = itemSizeBytes * maxNumItems;
    uPortPrivateQueue_t *pNew;
    bool success = true;

    if (gMutexQueue != NULL) {
        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;

        U_PORT_MUTEX_LOCK(gMutexQueue);

        // Find a unique handle
        x = gNextQueueHandle;
        while ((pQueueFind(gNextQueueHandle) != NULL) && success) {
            gNextQueueHandle++;
            if (gNextQueueHandle < U_PORT_PRIVATE_QUEUE_HANDLE_MIN) {
                gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;
            }
            if (gNextQueueHandle == x) {
                // Wrapped
                success = false;
            }
        }

        if (success) {
            // Allocate memory for the queue
            pNew = (uPortPrivateQueue_t *) pUPortMalloc(sizeof(uPortPrivateQueue_t));
            if (pNew != NULL) {
                pNew->writeSemaphore = INVALID_HANDLE_VALUE;
                pNew->readSemaphore = INVALID_HANDLE_VALUE;
                pNew->accessSemaphore = INVALID_HANDLE_VALUE;
                pNew->pBuffer = NULL;
                // Create the semaphores
                // The write and read semaphores are created with the number of items
                // in the queue, write starting at maxNumItems and read starting at 0 items
                if ((uPortSemaphoreCreate(&pNew->writeSemaphore, maxNumItems, maxNumItems) == 0) &&
                    (uPortSemaphoreCreate(&pNew->readSemaphore, 0, maxNumItems) == 0) &&
                    (uPortSemaphoreCreate(&pNew->accessSemaphore, 1, 1) == 0)) {
                    // Allocate memory for the buffer
                    pNew->pBuffer = (char *) pUPortMalloc(bufferSizeBytes);
                    if (pNew->pBuffer != NULL) {
                        // Populate the queue entry and add it to the front
                        // of the list
                        pNew->queueHandle = gNextQueueHandle;
                        pNew->itemSizeBytes = itemSizeBytes;
                        pNew->bufferSizeBytes = bufferSizeBytes;
                        pNew->pWrite = pNew->pBuffer;
                        pNew->pRead = pNew->pBuffer;
                        pNew->pNext = gpQueueRoot;
                        gpQueueRoot = pNew;
                        errorCodeOrHandle = pNew->queueHandle;
                        gNextQueueHandle++;
                        if (gNextQueueHandle < U_PORT_PRIVATE_QUEUE_HANDLE_MIN) {
                            gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;
                        }
                    }
                }
                if (errorCodeOrHandle < 0) {
                    // If we couldn't get memory for something, tidy up
                    queueFree(pNew);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexQueue);
    }

    return errorCodeOrHandle;
}

// Write a block of data to the given queue.
int32_t uPortPrivateQueueWrite(int32_t handle, const char *pData)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {
        errorCode =  (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pData != NULL) {
            // Get a copy of the queue while within the locks,
            // then release them so that the entire system
            // doesn't jam up while we wait for space to be available
            U_PORT_MUTEX_LOCK(gMutexQueue);
            errorCode = queueGetCopy(handle, &queue);
            U_PORT_MUTEX_UNLOCK(gMutexQueue);

            if (errorCode == 0) {
                // Wait for space to be available
                errorCode = semphoreTake(queue.writeSemaphore, handle);
                if (errorCode == 0) {
                    // Space is available, wait for the access semaphore
                    // to read it
                    errorCode = uPortSemaphoreTake(queue.accessSemaphore);
                    if (errorCode == 0) {

                        // While within the locks give back the access
                        // semaphore, write the data and potentially give back
                        // the space available semaphore
                        U_PORT_MUTEX_LOCK(gMutexQueue);

                        // NOTHING WITHIN THIS LOCK must block

                        // If the queue was closed or some such between the
                        // time we unlocked and relocked the mutex then the
                        // following line should return an error
                        errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                        if (errorCode == 0) {
                            // Now actually write the data and increment pointers
                            errorCode = queueWrite(queue.queueHandle, pData);
                            if (errorCode == 0) {
                                // Data is now available for reading
                                errorCode = uPortSemaphoreGive(queue.readSemaphore);
                            }
                        }

                        U_PORT_MUTEX_UNLOCK(gMutexQueue);
                    }
                }
            }
        }
    }

    return errorCode;
}

// Read a block of data from the given queue.
int32_t uPortPrivateQueueRead(int32_t handle, char *pData, int32_t waitMs)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;
    int64_t timeUsed;
    int64_t waitMs64 = (int64_t) waitMs;

    if (gMutexQueue != NULL) {

        // Get a copy of the queue while within the locks,
        // then release them so that the entire system
        // doesn't jam up while we wait for data to be available
        U_PORT_MUTEX_LOCK(gMutexQueue);
        errorCode = queueGetCopy(handle, &queue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);

        if (errorCode == 0) {
            // Wait for data to be available
            if (waitMs < 0) {
                errorCode = uPortSemaphoreTake(queue.readSemaphore);
            } else {
                timeUsed = uPortGetTickTimeMs();
                errorCode = uPortSemaphoreTryTake(queue.readSemaphore, waitMs);
            }
            if (errorCode == 0) {
                // Data is available, wait for the access semaphore
                // to read it
                if (waitMs < 0) {
                    errorCode = uPortSemaphoreTake(queue.accessSemaphore);
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    // Compensate for the time already spent waiting
                    timeUsed = uPortGetTickTimeMs() - timeUsed;
                    if (timeUsed > 0) { // Handle wrap in tick time
                        waitMs64 -= timeUsed;
                    }
                    if ((waitMs64 >= 0) && (waitMs64 <= INT_MAX)) {
                        errorCode = uPortSemaphoreTryTake(queue.accessSemaphore, (int32_t) waitMs64);
                    }
                }
                if (errorCode == 0) {

                    // While within the locks give back the access
                    // semaphore, read the data and potentially give back
                    // the data available semaphore
                    U_PORT_MUTEX_LOCK(gMutexQueue);

                    // NOTHING WITHIN THIS LOCK must block

                    // If the queue was closed or some such between the
                    // time we unlocked and relocked the mutex then the
                    // following line should return an error
                    errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                    if (errorCode == 0) {
                        // Now actually read the data and increment pointers
                        errorCode = queueRead(queue.queueHandle, pData);
                        if (errorCode == 0) {
                            // Space is now available for writing
                            errorCode = uPortSemaphoreGive(queue.writeSemaphore);
                        }
                    }

                    U_PORT_MUTEX_UNLOCK(gMutexQueue);
                } else {
                    // If we didn't get the access semaphore, just give the
                    // data available semaphore back
                    uPortSemaphoreGive(queue.readSemaphore);
                }
            }
        }
    }

    return errorCode;
}

// Peek the given queue.
int32_t uPortPrivateQueuePeek(int32_t handle, char *pData)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {
        errorCode =  (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pData != NULL) {

            U_PORT_MUTEX_LOCK(gMutexQueue);

            // Since there are no blocking calls here we can do
            // it all in one go within the mutex lock

            // Get a copy of the queue
            errorCode = queueGetCopy(handle, &queue);
            if (errorCode == 0) {
                // See if there is any data available right now, i.e.
                // with zero wait time
                errorCode = uPortSemaphoreTryTake(queue.readSemaphore, 0);
                if (errorCode == 0) {
                    // There is data available: copy it out and give the
                    // data available semaphore back.  No need to wait
                    // for the access semaphore as we have the mutex
                    // locked; no-one else can have got in to modify
                    // anything
                    memcpy(pData, queue.pRead, queue.itemSizeBytes);
                    errorCode = uPortSemaphoreGive(queue.readSemaphore);
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexQueue);
        }
    }

    return errorCode;
}

// Get the number of free spaces in the given queue.
int32_t uPortPrivateQueueGetFree(int32_t handle)
{
    int32_t errorCodeOrFree =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {

        U_PORT_MUTEX_LOCK(gMutexQueue);

        // Since there are no blocking calls here we can do
        // it all in one go within the mutex lock

        // Get a copy of the queue
        errorCodeOrFree = queueGetCopy(handle, &queue);
        if (errorCodeOrFree == 0) {
            if (queue.pWrite == queue.pRead) {
                // There are two cases where the read and write pointers are equal:
                // when the queue is completely free and when it is completely full.
                // The difference is that in the full case there are no write
                // semaphores left so nothing can be written.
                // Hence if we can take the write semaphore then the queue is
                // completely free
                if (uPortSemaphoreTryTake(queue.writeSemaphore, 0) == 0) {
                    uPortSemaphoreGive(queue.writeSemaphore);
                    errorCodeOrFree = (int32_t) (queue.bufferSizeBytes / queue.itemSizeBytes);
                }
            } else if (queue.pWrite > queue.pRead) {
                errorCodeOrFree = (int32_t) ((queue.bufferSizeBytes - (queue.pWrite - queue.pRead)) /
                                             queue.itemSizeBytes);
            } else {
                errorCodeOrFree = (int32_t) ((queue.bufferSizeBytes - (queue.pRead - queue.pWrite)) /
                                             queue.itemSizeBytes);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexQueue);
    }

    return errorCodeOrFree;
}

// Remove a queue from the list.
int32_t uPortPrivateQueueRemove(int32_t handle)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {

        // Get a copy of the queue while within the locks,
        // then release them so that the entire system
        // doesn't jam up while we wait for the closing
        // semaphore
        U_PORT_MUTEX_LOCK(gMutexQueue);
        errorCode = queueGetCopy(handle, &queue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);

        if (errorCode == 0) {
            // Wait for the buffer access semaphore so that
            // we don't collide with a read or write call
            errorCode = uPortSemaphoreTake(queue.accessSemaphore);
            if (errorCode == 0) {

                // While within the locks give back the buffer access
                // semaphore and remove the queue

                U_PORT_MUTEX_LOCK(gMutexQueue);

                // NOTHING WITHIN THESE LOCKS must block

                // Just in case anything is waiting on either of the
                // read or write semaphores, give them all up.
                for (size_t x = 0; x < (queue.bufferSizeBytes / queue.itemSizeBytes); x++) {
                    uPortSemaphoreGive(queue.readSemaphore);
                    uPortSemaphoreGive(queue.writeSemaphore);
                }

                // Now release the access semaphore
                errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                if (errorCode == 0) {
                    // Finally remove the queue
                    queueRemove(queue.queueHandle);
                }

                U_PORT_MUTEX_UNLOCK(gMutexQueue);
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, TIMERS
 * -------------------------------------------------------------- */

// Add a timer to the list, returning its handle.
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                const char *pName,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexTimer != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimer);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pHandle != NULL) {
            // Allocate memory for the timer
            pTimer = (uPortPrivateTimer_t *) pUPortMalloc(sizeof(uPortPrivateTimer_t));
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if (pTimer != NULL) {
                // Create the timer
                pTimer->handle = CreateWaitableTimer(NULL,   // Default security attributes
                                                     false,  // No manual reset
                                                     pName); // Name
                if (pTimer->handle != NULL) {
                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                    if (GetLastError() != ERROR_ALREADY_EXISTS) {
                        // Populate the rest of the timer entry and add
                        // it to the front of the list
                        pTimer->pCallback = pCallback;
                        pTimer->pCallbackParam = pCallbackParam;
                        // Convert the interval into a LARGE_INTEGER in Microsoft units
                        pTimer->dueTime.QuadPart = U_PORT_PRIVATE_MS_TO_DUE_TIME(intervalMs);
                        pTimer->periodMs = 0;
                        // In case the timer is periodic, update periodic
                        // interval in milliseconds.
                        if (periodic) {
                            pTimer->periodMs = intervalMs;
                        }
                        pTimer->pNext = gpTimerList;
                        gpTimerList = pTimer;
                        *pHandle = pTimer->handle;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        // The name already exists: what we've got
                        // back is the handle of another timer, which is
                        // an error for us
                        uPortFree(pTimer);
                    }
                } else {
                    uPortFree(pTimer);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimer);
    }

    return errorCode;
}

// Remove a timer from the list.
int32_t uPortPrivateTimerDelete(uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutexTimer != NULL) {

        // Close the timer in Windows, outside the mutex in case
        // it blocks
        CloseHandle((HANDLE) handle);

        U_PORT_MUTEX_LOCK(gMutexTimer);

        // Remove the timer
        timerRemove(handle);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(gMutexTimer);
    }

    return errorCode;
}

// Start a timer.
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;
    LARGE_INTEGER dueTime;
    uint32_t periodMs;

    if (gMutexTimer != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimer);

        pTimer = pTimerFind(handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            dueTime = pTimer->dueTime;
            periodMs = pTimer->periodMs;
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimer);

        // Start the timer outside the locks in case the call blocks
        if (pTimer != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            // Activate timer. In case of failure, return value is zero.
            if (SetWaitableTimer((HANDLE) handle,  // Handle of timer
                                 &dueTime,         // Expiration interval in Microsoft weird units
                                 periodMs,         // Periodic expiration interval in milliseconds
                                 (PTIMERAPCROUTINE) timerCallback, // Callback function called at expiration
                                 // interval has passed
                                 handle,           // Parameter to be passed to callback function
                                 true)) {          // Restore system from power save on timer expiry
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Change a timer interval.
int32_t uPortPrivateTimerChange(const uPortTimerHandle_t handle,
                                uint32_t intervalMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexTimer != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimer);

        pTimer = pTimerFind(handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            pTimer->dueTime.QuadPart =  U_PORT_PRIVATE_MS_TO_DUE_TIME(intervalMs);
            // If the timer was periodic then update that entry also
            if (pTimer->periodMs > 0) {
                pTimer->periodMs = intervalMs;
            }
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimer);
    }

    return errorCode;
}

// End of file
