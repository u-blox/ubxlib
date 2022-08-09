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
 * @brief This file implements some functions that may be useful
 * when debugging a mutex deadlock.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#ifdef U_CFG_MUTEX_DEBUG

#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Local version of the lock/unlock helper, since this can't
 * use the normal one.
 */
#define U_MUTEX_DEBUG_PORT_MUTEX_LOCK(x)      { _uPortMutexLock(x)

/** Local version of the lock/unlock helper, since this can't
 * use the normal one.
 */
#define U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(x)    } _uPortMutexUnlock(x)

#ifndef U_MUTEX_DEBUG_WATCHDOG_TASK_STACK_SIZE_BYTES
/** The stack size for the mutex watchdog task.  This is
 * quite chunky to allow the callback task to call printf(),
 * which can cause higher stack usage on some platforms.
 */
# define U_MUTEX_DEBUG_WATCHDOG_TASK_STACK_SIZE_BYTES (1024 * 4)
#endif

#ifndef U_MUTEX_DEBUG_WATCHDOG_TASK_PRIORITY
/** The priority of the mutex watchdog task; it is not very
 * active but we don't want it masked by other tasks too often,
 * so this should be reasonably high.
 */
# define U_MUTEX_DEBUG_WATCHDOG_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 3)
#endif

#ifndef U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS
/** The interval at which the mutex watchdog task checks the
 * watchdog timeout in milliseconds.
 */
# define U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS 1000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to identify the file name and line number in a file
 * as a linked-list entry.
 */
typedef struct uMutexFunctionInfo_t {
    const char *pFile; // If this is NULL the entry is not in use.
    int32_t line;
    int32_t counter;
    struct uMutexFunctionInfo_t *pNext;
} uMutexFunctionInfo_t;

/** A structure to keep track of a mutex as part of a linked list.
 * Note that the handle MUST be the first member of the structure.
 * This is because, when simulating critical sections under Windows,
 * we need to bypass the mutex debug and so we can grab just the
 * first member of the structure without having to know about it.
 */
typedef struct uMutexInfo_t {
    uPortMutexHandle_t handle;
    uMutexFunctionInfo_t *pCreator; // If this is NULL the entry is not in use.
    uMutexFunctionInfo_t *pLocker;
    uMutexFunctionInfo_t *pWaiting;
    struct uMutexInfo_t *pNext;
} uMutexInfo_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The handle of the mutex watchdog task; not static and rather
 * more name-spaced because u_port_private.c needs access to it on
 * some platforms.
 */
uPortTaskHandle_t gMutexDebugWatchdogTaskHandle = NULL;

/** Mutex so that we can tell that the mutex watchdog task is
 * running.
 */
static uPortMutexHandle_t gWatchdogTaskRunningMutex = NULL;

/** Flag to indicate that the watchdog task should keep running.
 */
static bool gWatchdogTaskKeepGoingFlag = false;

/** The timeout for the mutex watchdog task in seconds.
 */
static int32_t gWatchdogTimeoutSeconds = 0;

/** The callback to be called when the mutex watchdog goes off.
 */
static void (*gpWatchdogCallback)(void *) = NULL;

/** The parameter to pass to the mutex watchdog callback.
 */
static void *gpWatchdogCallbackParam = NULL;

/** The root of the linked list of mutex information blocks.
 */
static uMutexInfo_t *gpMutexInfoList = NULL;

/** Mutex to protect the linked lists.
 */
static uPortMutexHandle_t gMutexList = NULL;

/** Array of uMutexInfo_t's to use.
 */
static uMutexInfo_t gMutexInfo[U_MUTEX_DEBUG_MUTEX_INFO_MAX_NUM];

/** Array of uMutexFunctionInfo_t's to use.
 */
static uMutexFunctionInfo_t gMutexFunctionInfo[U_MUTEX_DEBUG_FUNCTION_INFO_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS; ONES THAT DO NOT LOCK THE LIST MUTEX
 * -------------------------------------------------------------- */

// Allocate a function information block.
// gMutexList should be locked before this is called.
static uMutexFunctionInfo_t *pAllocFunctionInformationBlock()
{
    uMutexFunctionInfo_t *pFunctionInfo = NULL;

    for (size_t x = 0; (x < sizeof(gMutexFunctionInfo) / sizeof(gMutexFunctionInfo[0])) &&
         (pFunctionInfo == NULL); x++) {
        if (gMutexFunctionInfo[x].pFile == NULL) {
            // Must be free
            pFunctionInfo = &(gMutexFunctionInfo[x]);
            pFunctionInfo->line = -1;
            pFunctionInfo->counter = 0;
        }
    }

    return pFunctionInfo;
}

// Free a function information block.
// gMutexList should be locked before this is called.
static void freeFunctionInformationBlock(uMutexFunctionInfo_t *pFunctionInfo)
{
    if (pFunctionInfo != NULL) {
        // Mark the block as free
        pFunctionInfo->pFile = NULL;
    }
}

// Allocate a mutex information block.
// gMutexList should be locked before this is called.
static uMutexInfo_t *pAllocMutexInformationBlock()
{
    uMutexInfo_t *pMutexInfo = NULL;

    for (size_t x = 0; (x < sizeof(gMutexInfo) / sizeof(gMutexInfo[0])) &&
         (pMutexInfo == NULL); x++) {
        if (gMutexInfo[x].pCreator == NULL) {
            // Must be free
            pMutexInfo = &(gMutexInfo[x]);
            pMutexInfo->pLocker = NULL;
            pMutexInfo->pWaiting = NULL;
            pMutexInfo->handle = NULL;
            pMutexInfo->pNext = NULL;
        }
    }

    return pMutexInfo;
}

// Free a mutex information block.
// gMutexList should be locked before this is called.
static void freeMutexInformationBlock(uMutexInfo_t *pMutexInfo)
{
    uMutexFunctionInfo_t *pWaiting;

    if (pMutexInfo != NULL) {
        // Free the locker function information
        freeFunctionInformationBlock(pMutexInfo->pLocker);
        // Free any waiting entries
        while (pMutexInfo->pWaiting != NULL) {
            pWaiting = pMutexInfo->pWaiting->pNext;
            freeFunctionInformationBlock(pMutexInfo->pWaiting);
            pMutexInfo->pWaiting = pWaiting;
        }
        // Free the creator function information
        freeFunctionInformationBlock(pMutexInfo->pCreator);
        // Mark the mutex information block as free
        pMutexInfo->pCreator = NULL;
    }
}

// Free an entry in the mutex list.
// gMutexList should be locked before this is called.
static int32_t freeMutex(uMutexInfo_t *pMutexInfo)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uMutexInfo_t *pTmp = gpMutexInfoList;
    uMutexInfo_t *pPrevious = NULL;
    uPortMutexHandle_t handle;

    while (pTmp != NULL) {
        // Find it in the list
        if (pTmp == pMutexInfo) {
            // Unhook it
            if (pPrevious != NULL) {
                pPrevious->pNext = pTmp->pNext;
            } else {
                // Must be at head
                gpMutexInfoList = pTmp->pNext;
            }
            // Free the mutex information, delete the actual
            // mutex and NULL the pointer to exit
            handle = pTmp->handle;
            freeMutexInformationBlock(pTmp);
            errorCode = _uPortMutexDelete(handle);
            pTmp = NULL;
        } else {
            pPrevious = pTmp;
            pTmp = pTmp->pNext;
        }
    }

    return errorCode;
}

// Unlink a waiting entry from a mutex.
// gMutexList should be locked before this is called.
static bool unlinkWaiting(uMutexInfo_t *pMutexInfo,
                          uMutexFunctionInfo_t *pWaiting)
{
    uMutexFunctionInfo_t *pPrevious = NULL;
    uMutexFunctionInfo_t *pTmp = pMutexInfo->pWaiting;
    bool success = false;

    // Find the waiting entry in the list and remove it
    while (pTmp != NULL) {
        if (pTmp == pWaiting) {
            if (pPrevious != NULL) {
                pPrevious->pNext = pTmp->pNext;
            } else {
                // Must be at head
                pMutexInfo->pWaiting = pTmp->pNext;
            }
            // Exit loop and return success
            success = true;
            pTmp = NULL;
        } else {
            pPrevious = pTmp;
            pTmp = pTmp->pNext;
        }
    }

    return success;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: ONES THAT LOCK THE LIST MUTEX
 * -------------------------------------------------------------- */

// Add a waiting entry to a mutex, returning a pointer to it.
static uMutexFunctionInfo_t *pLockAddWaiting(uMutexInfo_t *pMutexInfo,
                                             const char *pFile,
                                             int32_t line)
{
    uMutexFunctionInfo_t *pWaiting = NULL;
    uMutexFunctionInfo_t *pTmp;

    if ((gMutexList != NULL) && (pMutexInfo != NULL)) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // Create a waiting information entry
        pWaiting = pAllocFunctionInformationBlock();
        if (pWaiting != NULL) {
            pWaiting->pFile = pFile;
            pWaiting->line = line;
            // Add it to the front of the waiting list
            pTmp = pMutexInfo->pWaiting;
            pMutexInfo->pWaiting = pWaiting;
            pWaiting->pNext = pTmp;
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return pWaiting;
}

// Move a waiting entry to become a locker entry.
static bool lockMoveWaitingToLocker(uMutexInfo_t *pMutexInfo,
                                    uMutexFunctionInfo_t *pWaiting)
{
    bool success = false;

    if ((gMutexList != NULL) && (pMutexInfo != NULL)) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // If there is a locker, free it, it's gone
        freeFunctionInformationBlock(pMutexInfo->pLocker);
        // Unlink the waiting entry in the list, noting
        // that it is possible that the mutex has
        // disappeared in the meantime
        success = unlinkWaiting(pMutexInfo, pWaiting);
        if (success) {
            // The waiting entry is now the locker
            pMutexInfo->pLocker = pWaiting;
            // Zero the counter
            pMutexInfo->pLocker->counter = 0;
            // For neatness
            pMutexInfo->pLocker->pNext = NULL;
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return success;
}

// Free a waiting entry.
static void lockFreeWaiting(uMutexInfo_t *pMutexInfo,
                            uMutexFunctionInfo_t *pWaiting)
{
    if ((gMutexList != NULL) && (pMutexInfo != NULL)) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // Find the waiting entry in the list and free it
        unlinkWaiting(pMutexInfo, pWaiting);
        freeFunctionInformationBlock(pWaiting);

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: WATCHDOG
 * -------------------------------------------------------------- */

// Mutex watchdog task.
static void watchdogTask(void *pParam)
{
    uMutexInfo_t *pMutexInfo;
    uMutexFunctionInfo_t *pWaiting;
    int64_t calledMs = 0;
    bool callCallback = false;

    (void) pParam;

    U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gWatchdogTaskRunningMutex);

    while (gWatchdogTaskKeepGoingFlag) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // Run through the entire list, incrementing
        // and checking the counters.
        pMutexInfo = gpMutexInfoList;
        while (pMutexInfo != NULL) {
            if (pMutexInfo->pCreator != NULL) {
                // Increment the creator count as useful information
                pMutexInfo->pCreator->counter++;
            }
            if (pMutexInfo->pLocker != NULL) {
                // Increment the locker count as useful information
                pMutexInfo->pLocker->counter++;
            }
            pWaiting = pMutexInfo->pWaiting;
            while (pWaiting != NULL) {
                // The main thing though: check and increment
                // the waiting entry counters
                if (pWaiting->counter * U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS > gWatchdogTimeoutSeconds * 1000) {
                    callCallback = true;
                }
                pWaiting->counter++;
                pWaiting = pWaiting->pNext;
            }
            pMutexInfo = pMutexInfo->pNext;

            // Don't call the callback too often though
            if (uPortGetTickTimeMs() - calledMs < (U_MUTEX_DEBUG_WATCHDOG_MAX_BARK_SECONDS * 1000)) {
                callCallback = false;
            }
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);

        if (callCallback) {
            // Call the callback outside the locks so that it can have them
            gpWatchdogCallback(gpWatchdogCallbackParam);
            calledMs = uPortGetTickTimeMs();
        }

        // Sleep until the next go
        uPortTaskBlock(U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS);
    }

    U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gWatchdogTaskRunningMutex);

    // Delete ourself
    uPortTaskDelete(NULL);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INTERMEDIATES FOR THE uPortMutex* FUNCTIONS
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t uMutexDebugCreate(uPortMutexHandle_t *pMutexHandle,
                          const char *pFile, int32_t line)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uMutexInfo_t *pMutexInfo;
    uMutexInfo_t *pTmp;

    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // Create an entry
        pMutexInfo = pAllocMutexInformationBlock();
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        if (pMutexInfo != NULL) {
            pMutexInfo->pCreator = pAllocFunctionInformationBlock();
            if (pMutexInfo->pCreator != NULL) {
                pMutexInfo->pCreator->pFile = pFile;
                pMutexInfo->pCreator->line = line;
                // For neatness
                pMutexInfo->pCreator->pNext = NULL;
                pMutexInfo->pLocker = NULL;
                pMutexInfo->pWaiting = NULL;
                if (_uPortMutexCreate(&(pMutexInfo->handle)) == 0) {
                    // Add the entry to the front of the list
                    pTmp = gpMutexInfoList;
                    gpMutexInfoList = pMutexInfo;
                    pMutexInfo->pNext = pTmp;
                    *pMutexHandle = (uPortMutexHandle_t) pMutexInfo;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Couldn't create the actual underlying
                    // mutex, clean up
                    freeMutexInformationBlock(pMutexInfo);
                }
            } else {
                // Couldn't get a function information block,
                // for the creator, clean up
                freeMutexInformationBlock(pMutexInfo);
            }
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return errorCode;
}

// Delete a mutex.
int32_t uMutexDebugDelete(uPortMutexHandle_t mutexHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        errorCode = freeMutex((uMutexInfo_t *) mutexHandle);

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return errorCode;
}

// Lock a mutex.
int32_t uMutexDebugLock(uPortMutexHandle_t mutexHandle,
                        const char *pFile, int32_t line)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uMutexInfo_t *pMutexInfo = (uMutexInfo_t *) mutexHandle;
    uMutexFunctionInfo_t *pWaiting;

    if (gMutexList != NULL) {

        // Can't lock the list mutex here since we will probably
        // be trapped, locked on the underlying mutex.  Instead
        // the individual linked-list functions do so.

        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pWaiting = pLockAddWaiting(pMutexInfo, pFile, line);
        if (pWaiting != NULL) {
            errorCode = _uPortMutexLock(pMutexInfo->handle);
            if (errorCode == 0) {
                if (!lockMoveWaitingToLocker(pMutexInfo, pWaiting)) {
                    lockFreeWaiting(pMutexInfo, pWaiting);
                }
            } else {
                lockFreeWaiting(pMutexInfo, pWaiting);
            }
        }
    }

    return errorCode;
}

// Try to lock a mutex.
int32_t uMutexDebugTryLock(uPortMutexHandle_t mutexHandle,
                           int32_t delayMs,
                           const char *pFile,
                           int32_t line)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uMutexInfo_t *pMutexInfo = (uMutexInfo_t *) mutexHandle;
    uMutexFunctionInfo_t *pWaiting;

    if (gMutexList != NULL) {

        // Can't lock the list mutex here since we will probably
        // be trapped, locked on the underlying mutex.  Instead
        // the individual linked-list functions do so.

        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pWaiting = pLockAddWaiting(pMutexInfo, pFile, line);
        if (pWaiting != NULL) {
            errorCode = _uPortMutexTryLock(pMutexInfo->handle, delayMs);
            if (errorCode == 0) {
                if (!lockMoveWaitingToLocker(pMutexInfo, pWaiting)) {
                    lockFreeWaiting(pMutexInfo, pWaiting);
                }
            } else {
                lockFreeWaiting(pMutexInfo, pWaiting);
            }
        }
    }

    return errorCode;
}

// Unlock a mutex.
int32_t uMutexDebugUnlock(uPortMutexHandle_t mutexHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uMutexInfo_t *pMutexInfo = (uMutexInfo_t *) mutexHandle;

    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        // Unlock the mutex and free the locker entry
        errorCode = _uPortMutexUnlock(pMutexInfo->handle);
        freeFunctionInformationBlock(pMutexInfo->pLocker);
        pMutexInfo->pLocker = NULL;

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Initialise mutex debug.
int32_t uMutexDebugInit(void)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexList == NULL) {
        memset(gMutexInfo, 0, sizeof(gMutexInfo));
        memset(gMutexFunctionInfo, 0, sizeof(gMutexFunctionInfo));
        errorCode = _uPortMutexCreate(&gMutexList);
    }

    return errorCode;
}

// De-initialise mutex debug.
void uMutexDebugDeinit(void)
{
    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        while (gpMutexInfoList != NULL) {
            freeMutex(gpMutexInfoList);
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);

        // Unlock the mutex while we clean up the
        // watchdog task if there is one
        if (gWatchdogTaskKeepGoingFlag) {
            gWatchdogTaskKeepGoingFlag = false;
            U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gWatchdogTaskRunningMutex);
            U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gWatchdogTaskRunningMutex);
            _uPortMutexDelete(gWatchdogTaskRunningMutex);
        }

        // Now delete the mutex
        _uPortMutexDelete(gMutexList);
        gMutexList = NULL;
    }
}

// Create a mutex watchdog.
int32_t uMutexDebugWatchdog(void (*pCallback) (void *),
                            void *pCallbackParam,
                            int32_t timeoutSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        if (gWatchdogTaskKeepGoingFlag) {
            // If a watchdog task is already running, shut it down
            // first to avoid race conditions
            gWatchdogTaskKeepGoingFlag = false;
            U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gWatchdogTaskRunningMutex);
            U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gWatchdogTaskRunningMutex);
            _uPortMutexDelete(gWatchdogTaskRunningMutex);
        }

        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        gWatchdogTimeoutSeconds = timeoutSeconds;
        gpWatchdogCallbackParam = pCallbackParam;
        gpWatchdogCallback = pCallback;
        if (gpWatchdogCallback != NULL) {
            errorCode = _uPortMutexCreate(&gWatchdogTaskRunningMutex);
            if (errorCode == 0) {
                gWatchdogTaskKeepGoingFlag = true;
                errorCode = uPortTaskCreate(watchdogTask,
                                            "mutexWatchdog",
                                            U_MUTEX_DEBUG_WATCHDOG_TASK_STACK_SIZE_BYTES,
                                            NULL,
                                            U_MUTEX_DEBUG_WATCHDOG_TASK_PRIORITY,
                                            &gMutexDebugWatchdogTaskHandle);
                if (errorCode != 0) {
                    // Couldn't create watchdog task, clean up
                    gWatchdogTaskKeepGoingFlag = false;
                    _uPortMutexDelete(gWatchdogTaskRunningMutex);
                }
            }
        }

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }

    return errorCode;
}

// Print out the current state of all mutexes.
void uMutexDebugPrint(void *pParam)
{
    int32_t x;
    int32_t mutexes = 0;
    int32_t locked = 0;
    int32_t maxNumWaiting = 0;
    int32_t maxWaitingCounter = 0;
    uMutexInfo_t *pMutexInfo;
    uMutexFunctionInfo_t *pWaiting;

    (void) pParam;

    if (gMutexList != NULL) {

        U_MUTEX_DEBUG_PORT_MUTEX_LOCK(gMutexList);

        pMutexInfo = gpMutexInfoList;
        while (pMutexInfo != NULL) {
            uPortLog("U_MUTEX_DEBUG_0x%08x: created by %s:%d approx. %d second(s) ago is %s.\n",
                     pMutexInfo->handle,
                     pMutexInfo->pCreator->pFile,
                     pMutexInfo->pCreator->line,
                     (pMutexInfo->pCreator->counter * U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS) / 1000,
                     pMutexInfo->pLocker != NULL ? "LOCKED" : "not locked");
            if (pMutexInfo->pLocker != NULL) {
                uPortLog("U_MUTEX_DEBUG_0x%08x: locker has been %s:%d for approx. %d second(s).\n",
                         pMutexInfo->handle,
                         pMutexInfo->pLocker->pFile,
                         pMutexInfo->pLocker->line,
                         (pMutexInfo->pLocker->counter * U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS) / 1000);
                pWaiting = pMutexInfo->pWaiting;
                x = 0;
                while (pWaiting != NULL) {
                    uPortLog("U_MUTEX_DEBUG_0x%08x: %s:%d has been **WAITING** for a"
                             " lock for approx. %d second(s).\n",
                             pMutexInfo->handle, pWaiting->pFile,
                             pWaiting->line,
                             (pWaiting->counter * U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS) / 1000);
                    if (pWaiting->counter > maxWaitingCounter) {
                        maxWaitingCounter = pWaiting->counter;
                    }
                    pWaiting = pWaiting->pNext;
                    x++;
                }
                if (x > maxNumWaiting) {
                    maxNumWaiting = x;
                }
                locked++;
            }
            pMutexInfo = pMutexInfo->pNext;
            mutexes++;
        }

        uPortLog("U_MUTEX_DEBUG: %d mutex(es), %d locked, a maximum"
                 " of %d waiting, max waiting time approx. %d second(s).\n",
                 mutexes, locked, maxNumWaiting,
                 (maxWaitingCounter * U_MUTEX_DEBUG_WATCHDOG_CHECK_INTERVAL_MS) / 1000);

        U_MUTEX_DEBUG_PORT_MUTEX_UNLOCK(gMutexList);
    }
}

#endif // U_CFG_MUTEX_DEBUG

// End of file
