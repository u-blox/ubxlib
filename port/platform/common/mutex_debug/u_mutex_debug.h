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

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files.. */

#ifndef _U_MUTEX_DEBUG_H_
#define _U_MUTEX_DEBUG_H_

/** @file
 * @brief This file provides the API to some functions that may be
 * useful when debugging a mutex deadlock.  These functions are
 * thread-safe aside from uMutexDebugInit(), uMutexDebugDeinit() and
 * uMutexDebugWatchdog(), which should not be called at the same time as
 * any other of the other APIs.
 *
 * IMPORTANT: in order to support this debug feature, it must be possible
 * on your platform for a task and a mutex to be created BEFORE
 * uPortInit() is called, right at start of day, and such a task/mutex
 * must also survive uPortDeinit() being called.  This is because
 * uMutexDebugInit() must be able to create a mutex and
 * uMutexDebugWatchdog() must be able to create a task and these must
 * not be destroyed for the life of the application.
 *
 * The intermediate functions here are intended to be inserted
 * in place of the usual port OS mutex APIs if U_CFG_MUTEX_DEBUG is
 * defined.  To use them, add a call to uMutexDebugInit() at the very
 * start of your code, so after the operating system has started but
 * _before_ the first call to uPortInit(), e.g. at the start of the
 * function that is called by uPortPlatformStart().
 *
 * Your code should now run normally, with the mutex monitoring
 * in place.  If you see that any OS mutex functions are returning
 * failures then you may need to increase U_MUTEX_DEBUG_MUTEX_INFO_MAX_NUM
 * or U_MUTEX_DEBUG_FUNCTION_INFO_MAX_NUM.
 *
 * Then, to trap a mutex deadlock, immediately after the call to
 * uMutexDebugInit(), add a call to uMutexDebugWatchdog(), maybe
 * using uMutexDebugPrint as the callback, e.g.:
 *
 * uMutexDebugWatchdog(uMutexDebugPrint, NULL,
 *                     U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS);
 *
 * This will create a mutex watchdog task that will call the callback
 * (in this case uMutexDebugPrint()) if anything has been waiting on
 * a mutex for more than U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS seconds.
 * uMutexDebugPrint() will print the states of all the current mutexes
 * so that you can figure out who was doing what to whom.
 *
 * To check that it is working, insert the following code somewhere
 * in your application:
 *
 *     uPortMutexHandle_t mutexHandle;
 *
 *     if ((uPortMutexCreate(&mutexHandle) == 0) && (uPortMutexLock(mutexHandle) == 0)) {
 *         printf("Mutex 0x%08x locked, now locking it again.\n", mutexHandle);
 *         uPortMutexLock(mutexHandle);
 *     } else {
 *         printf("Unable to create or lock mutex!\n");
 *     }
 *
 * You will also need the usual \#includes of course (u_port_os.h
 * etc.) and you may wish to reduce the timer passed into
 *  uMutexDebugWatchdog() to something like 10000 to save time.
 *
 * When this code executes the task in which the code above is
 * included will be blocked at the second uPortMutexLock() and,
 * after about 10 seconds, the mutex watchdog task should call
 * the callback which will print something like the following
 * (where the if() line above was at line 2053 of u_port_test.c):
 *
 * U_MUTEX_DEBUG_0x2000e960: created by C:/projects/ubxlib/port/test/u_port_test.c:2053 approx. 12 second(s) ago is LOCKED.
 * U_MUTEX_DEBUG_0x2000e960: locker has been C:/projects/ubxlib/port/test/u_port_test.c:2053 for approx. 12 second(s).
 * U_MUTEX_DEBUG_0x2000e960: C:/projects/ubxlib/port/test/u_port_test.c:2055 has been **WAITING** for a lock for approx. 12 second(s).
 * U_MUTEX_DEBUG_0x2000a7e8: created by C:/projects/ubxlib/port/platform/stm32cube/src/u_port_uart.c:892 approx. 12 second(s) ago is not locked.
 * U_MUTEX_DEBUG_0x2000a840: created by C:/projects/ubxlib/port/platform/common/event_queue/u_port_event_queue.c:229 approx. 12 second(s) ago is not locked.
 * U_MUTEX_DEBUG: 3 mutex(es), 1 locked, a maximum of 1 waiting, max waiting time approx. 12 second(s).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_MUTEX_DEBUG_MUTEX_INFO_MAX_NUM
/** The maximum number of mutexes to track.
 */
# define U_MUTEX_DEBUG_MUTEX_INFO_MAX_NUM 64
#endif

#ifndef U_MUTEX_DEBUG_FUNCTION_INFO_MAX_NUM
/** The maximum number of function information entries to use;
 * these track who has created, locked or is waiting for a lock on a
 * mutex and are shared amongst the mutex information entries.
 */
# define U_MUTEX_DEBUG_FUNCTION_INFO_MAX_NUM 256
#endif

#ifndef U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS
/** A good default watchdog timeout (in seconds).
 */
# define U_MUTEX_DEBUG_WATCHDOG_TIMEOUT_SECONDS 60
#endif

#ifndef U_MUTEX_DEBUG_WATCHDOG_MAX_BARK_SECONDS
/** Don't have the watchdog go off any more frequently than this
 * time in seconds.
 */
# define U_MUTEX_DEBUG_WATCHDOG_MAX_BARK_SECONDS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: INTERMEDIATES FOR THE uPortMutex* FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a mutex.
 *
 * @param[out] pMutexHandle a place to put the mutex handle.
 * @param pFile             the name of the file that the calling
 *                          function is in.
 * @param line              the line in pFile that is calling this
 *                          function.
 * @return                  zero on success else negative error code.
 */
int32_t uMutexDebugCreate(uPortMutexHandle_t *pMutexHandle,
                          const char *pFile, int32_t line);

/** Macro to map uPortMutexCreate() to uMutexDebugCreate().
 */
//lint -esym(652, uPortMutexCreate) Suppress duplicate definition
#define uPortMutexCreate(x) uMutexDebugCreate(x, __FILE__, __LINE__)

/** Prototype for the _ version of uPortMutexCreate().
 */
int32_t _uPortMutexCreate(uPortMutexHandle_t *pMutexHandle);

/** Delete a mutex.
 *
 * @param mutexHandle  the handle of the mutex to delete.
 * @return             zero on success else negative error code.
 */
int32_t uMutexDebugDelete(uPortMutexHandle_t mutexHandle);

/** Macro to map uPortMutexDelete() to uMutexDebugDelete().
 */
//lint -esym(652, uPortMutexDelete) Suppress duplicate definition
#define uPortMutexDelete(x) uMutexDebugDelete(x)

/** Prototype for the _ version of uPortMutexDelete().
 */
int32_t _uPortMutexDelete(uPortMutexHandle_t mutexHandle);

/** Lock a mutex.
 *
 * @param mutexHandle  the handle of the mutex to lock.
 * @param pFile        the name of the file that the calling
 *                     function is in.
 * @param line         the line in pFile that is calling this
 *                     function.
 * @return             zero on success else negative error code.
 */
int32_t uMutexDebugLock(uPortMutexHandle_t mutexHandle,
                        const char *pFile, int32_t line);

/** Macro to map uPortMutexLock() to uMutexDebugLock().
 */
//lint -esym(652, uPortMutexLock) Suppress duplicate definition
#define uPortMutexLock(x) uMutexDebugLock(x, __FILE__, __LINE__)

/** Prototype for the _ version of uPortMutexLock().
 */
int32_t _uPortMutexLock(uPortMutexHandle_t mutexHandle);

/** Try to lock a mutex, waiting up to delayMs if the mutex is
 * currently locked.
 *
 * @param mutexHandle  the handle of the mutex to lock.
 * @param delayMs      the maximum time to wait in milliseconds.
 * @param pFile        the name of the file that the calling
 *                     function is in.
 * @param line         the line in pFile that is calling this
 *                     function.
 * @return             zero on success else negative error code.
 */
int32_t uMutexDebugTryLock(uPortMutexHandle_t mutexHandle,
                           int32_t delayMs,
                           const char *pFile,
                           int32_t line);

/** Macro to map uPortMutexTryLock() to uMutexDebugTryLock().
 */
//lint -esym(652, uPortMutexTryLock) Suppress duplicate definition
#define uPortMutexTryLock(x, y) uMutexDebugTryLock(x, y, __FILE__, __LINE__)

/** Prototype for the _ version of uPortMutexTryLock().
 */
int32_t _uPortMutexTryLock(uPortMutexHandle_t mutexHandle, int32_t delayMs);

/** Unlock a mutex.
 *
 * @param mutexHandle  the handle of the mutex to unlock.
 * @return             zero on success else negative error code.
 */
int32_t uMutexDebugUnlock(uPortMutexHandle_t mutexHandle);

/** Macro to map uPortMutexUnlock() to uMutexDebugUnlock().
 */
//lint -esym(652, uPortMutexUnlock) Suppress duplicate definition
#define uPortMutexUnlock(x) uMutexDebugUnlock(x)

/** Prototype for the _ version of uPortMutexUnlock().
 */
int32_t _uPortMutexUnlock(uPortMutexHandle_t mutexHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Initialise mutex debug; if mutex debug is already
 * initialised this will do nothing and return.
 *
 * @return zero on success else negative error code.
 */
int32_t uMutexDebugInit(void);

/** De-initialise mutex debug, freeing resources.
 */
void uMutexDebugDeinit(void);

/** Create a watchdog which will call the given callback if
 * a caller has been waiting on a mutex for more than the given number
 * of seconds.  It is up to pCallback to stop the system, assert,
 * etc. as required.  If pCallback returns the watchdog timer will
 * be called again in U_MUTEX_DEBUG_WATCHDOG_MAX_BARK_SECONDS seconds.
 * pCallback will be called from a task with stack size
 * U_MUTEX_DEBUG_WATCHDOG_TASK_STACK_SIZE_BYTES and priority
 * U_MUTEX_DEBUG_WATCHDOG_TASK_PRIORITY.  Set pCallback to NULL to
 * stop an existing mutex watchdog.
 *
 * @param pCallback       the callback (e.g. could be
 *                        uMutexDebugPrint()); use NULL to remove an
 *                        existing mutex watchdog.
 * @param pCallbackParam  a parameter that will be passed to pCallback,
 *                        may be NULL.
 * @param timeoutSeconds  the maximum number of seconds anything
 *                        should have been waiting on a mutex.
 */
int32_t uMutexDebugWatchdog(void (*pCallback) (void *),
                            void *pCallbackParam,
                            int32_t timeoutSeconds);

/** Print out the current state of all mutexes; may be passed
 * as a callback to uMutexDebugWatchdog().
 *
 * @param pParam  a dummy parameter so that this function matches
 *                the function signature for uMutexDebugWatchdog().
 */
void uMutexDebugPrint(void *pParam);

#ifdef __cplusplus
}
#endif

#endif // _U_MUTEX_DEBUG_H_

// End of file
