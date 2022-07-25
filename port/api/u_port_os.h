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

#ifndef _U_PORT_OS_H_
#define _U_PORT_OS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. _except_, under special circumstances,
 * if we want to sneak mutex debug in under-cover, see the section
 * under #U_CFG_MUTEX_DEBUG that is snuck in at the very end of this
 * file.
 */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for OS functions.  These functions are
 * thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Helper to make sure that lock/unlock pairs are always balanced.
 */
#define U_PORT_MUTEX_LOCK(x)      { uPortMutexLock(x)

/** Helper to make sure that lock/unlock pairs are always balanced.
 */
#define U_PORT_MUTEX_UNLOCK(x)    } uPortMutexUnlock(x)

/** Constants related to acquiring executable chunks of RAM memory
 */
#define U_PORT_EXECUTABLE_CHUNK_NO_FLAGS      0

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Note: see here:
 * https://stackoverflow.com/questions/72415062/c-compiler-checking-of-a-typedefed-void
 * for a discussion of why we should never have used void * for the
 * type definitions below.
 * However, we did, so please just note that it is up to the user
 * to pass the correct handle type into each of the uPortOsXxx()
 * functions, the compiler will not emit an error, or a warning,
 * if the wrong handle type is passed.
 */

/** Mutex handle.
 */
typedef void *uPortMutexHandle_t;

/** Semaphore handle.
 */
typedef void *uPortSemaphoreHandle_t;

/** Queue handle.
 */
typedef void *uPortQueueHandle_t;

/** Task handle.
 */
typedef void *uPortTaskHandle_t;

/** Timer handle.
 */
typedef void *uPortTimerHandle_t;

typedef enum {
    U_PORT_NO_EXECUTABLE_CHUNK      =  -1,
    U_PORT_EXECUTABLE_CHUNK_INDEX_0 =   0,
} uPortChunkIndex_t;

/** For future implementations. Will likely hold
 *  features such as cacheable, shareable, bufferable etc.
 *  as typically available in MPU settings if they can be
 *  set during runtime.
 */
typedef uint32_t uPortExeChunkFlags_t;

/** The function signature for a timer callback.
 */
typedef void (pTimerCallback_t) (const uPortTimerHandle_t, void *);

/* ----------------------------------------------------------------
 * FUNCTIONS: TASKS
 * -------------------------------------------------------------- */

/** Create, and start, a task.
 *
 * @param[in] pFunction    the function that forms the task.
 * @param[in] pName        a null-terminated string naming the task,
 *                         may be NULL.
 * @param stackSizeBytes   the number of bytes of memory to dynamically
 *                         allocate for stack.
 * @param[in] pParameter   a pointer that will be passed to pFunction
 *                         when the task is started.
 *                         The thing at the end of this pointer must be
 *                         there for the lifetime of the task, it is
 *                         not copied.  May be NULL.
 * @param priority         the priority at which to run the task,
 *                         the meaning of which is platform dependent.
 * @param[out] pTaskHandle a place to put the handle of the created
 *                         task.
 * @return                 zero on success else negative error code.
 */
int32_t uPortTaskCreate(void (*pFunction)(void *),
                        const char *pName,
                        size_t stackSizeBytes,
                        void *pParameter,
                        int32_t priority,
                        uPortTaskHandle_t *pTaskHandle);

/** Delete the given task.
 *
 * @param taskHandle  the handle of the task to be deleted.
 *                    Use NULL to delete the current task.
 *                    It is often the case in embedded
 *                    systems that only the current task can
 *                    delete itself, hence use of anything
 *                    other than NULL for taskHandle may not
 *                    be permitted, depending on the underlying
 *                    RTOS. Note also that the task may not
 *                    actually be deleted until the idle task
 *                    runs; this can be effected by calling
 *                    uPortTaskBlock(U_CFG_OS_YIELD_MS).
 * @return            zero on success else negative error code.
 */
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle);

/** Check if the current task handle is equal to the given
 * task handle.
 *
 * @param taskHandle  the task handle to check against.
 * @return            true if the task handle pointed to by
 *                    pTaskHandle is the current task handle,
 *                    otherwise false.
 */
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle);

/** Block the current task for a time.  Note that this will only
 * yield to another task if delayMs is longer than one tick: for
 * this specify a delay of at least U_CFG_OS_YIELD_MS.
 *
 * @param delayMs the amount of time to block for in milliseconds.
 */
void uPortTaskBlock(int32_t delayMs);

/** Get the stack high watermark, the minimum amount
 * of stack free, in bytes, for a given task.
 *
 * @param taskHandle  the task handle to check.  If NULL is given
 *                    the handle of the current task is used.
 * @return            the minimum amount of stack free for the
 *                    lifetime of the task in bytes, else
 *                    negative error code.
 */
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle);

/** Get the current task handle.
 * It is NOT a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.
 *
 * @param[out] pTaskHandle a place to put the task handle; cannot
 *                         be NULL.
 * @return                 zero on success else negative error code.
 */
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

/** Create a queue.
 *
 * @param queueLength       the maximum length of the queue in units
 *                          of itemSizeBytes.
 * @param itemSizeBytes     the size of each item on the queue.
 * @param[out] pQueueHandle a place to put the handle of the queue.
 * @return                  zero on success else negative error code.
 */
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle);

/** Delete the given queue.
 *
 * @param queueHandle  the handle of the queue to be deleted.
 * @return             zero on success else negative error code.
 */
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle);

/** Send to the given queue.  If the queue is full this function
 * will block until room is available.
 *
 * @param queueHandle    the handle of the queue.
 * @param[in] pEventData pointer to the data to send.  The data will
 *                       be copied into the queue and hence can be
 *                       destroyed by the caller once this functions
 *                       returns.
 * @return               zero on success else negative error code.
 */
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData);

/** Send to the given queue from an interrupt.  If the queue is
 * full this function will return an error.  Note that not all
 * platforms support this function (e.g. Windows doesn't).
 *
 * @param queueHandle    the handle of the queue.
 * @param[in] pEventData pointer to the data to send.  The data will
 *                       be copied into the queue and hence can be
 *                       destroyed by the caller once this functions
 *                       returns.
 * @return               zero on success else negative error code.
 */
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData);

/** Receive from the given queue, blocking until something is
 * received.
 *
 * @param queueHandle     the handle of the queue.
 * @param[out] pEventData pointer to a place to put incoming data.
 * @return                zero on success else negative error code.
 */
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData);

/** Receive from the given queue from ISR.  Note that not all
 * platforms support this function (e.g. Windows doesn't).
 *
 * @param queueHandle     the handle of the queue.
 * @param[out] pEventData pointer to a place to put incoming data.
 * @return                zero on success else negative error code.
 */
int32_t uPortQueueReceiveIrq(const uPortQueueHandle_t queueHandle,
                             void *pEventData);


/** Try to receive from the given queue, waiting for the given
 * time for something to arrive.
 *
 * @param queueHandle     the handle of the queue.
 * @param waitMs          the amount of time to wait in milliseconds.
 * @param[out] pEventData pointer to a place to put incoming data.
 * @return                zero if someting is received else negative
 *                        error code.
 */
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData);

/** Peek the given queue; the data is copied out of the queue but
 * is NOT removed from the queue. If the queue is empty
 * #U_ERROR_COMMON_TIMEOUT is returned.  It is NOT a requirement
 * that this API is implemented: where it is not implemented
 * #U_ERROR_COMMON_NOT_IMPLEMENTED should be returned.
 *
 * @param queueHandle     the handle of the queue.
 * @param[out] pEventData pointer to a place to put incoming data.
 * @return                zero on success else negative error code.
 */
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData);

/** Get the number of free spaces in the given queue.
 * It is NOT a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.
 *
 * @param queueHandle the handle of the queue.
 * @return            on success the number of spaces available,
 *                    else negative error code.
 */
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

/** Create a mutex.  Note that the mutex created is NOT a recursive
 * mutex, a task may only lock it once.
 *
 * @param[out] pMutexHandle a place to put the mutex handle.
 * @return                  zero on success else negative error code.
 */
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle);

/** Destroy a mutex.  Note that it is not permitted to delete a
 * mutex which is currently locked, hence it is good practice
 * in any de-initialisation code to lock and then unlock a mutex
 * before destroying it, just to be sure there is no asynchronous
 * thing that hasn't quite finished yet.
 *
 * @param mutexHandle the handle of the mutex.
 * @return            zero on success else negative error code.
 */
int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle);

/** Lock the given mutex, waiting until it is available if
 * it is already locked  Note that a lock can only be taken
 * once, EVEN IF the lock attempt is from within the same task.
 * In other words this is NOT a counting mutex, it is a simple
 * binary mutex.
 *
 * @param mutexHandle  the handle of the mutex.
 * @return             zero on success else negative error code.
 */
int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle);

/** Try to lock the given mutex, waiting up to delayMs
 * if it is currently locked.
 *
 * @param mutexHandle  the handle of the mutex.
 * @param delayMs      the maximum time to wait in milliseconds.
 * @return             zero on success else negative error code.
 */
int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs);

/** Unlock the given mutex.
 *
 * @param mutexHandle   the handle of the mutex.
 * @return              zero on success else negative error code.
 */
int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: SEMAPHORES
 * -------------------------------------------------------------- */

/** Create a semaphore.
 *
 * @param[out] pSemaphoreHandle a place to put the semaphore handle.
 * @param initialCount          initial semaphore count
 * @param limit                 maximum permitted semaphore count
 * @return                      zero on success else negative error code
 */
int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit);

/** Destroy a semaphore.
 *
 * @param semaphoreHandle the handle of the semaphore.
 * @return                zero on success else negative error code.
 */
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle);

/** Take the given semaphore, waiting until it is available if
 * it is already taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle);

/** Try to take the given semaphore, waiting up to delayMs
 * if it is currently taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @param delayMs          the maximum time to wait in milliseconds.
 * @return                 zero on success else negative error code.
 */
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs);

/** Give a semaphore, unless the semaphore is already at its maximum permitted
 * count.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle);

/** Give a semaphore from interrupt, unless the semaphore is already at its
 * maximum permitted count.  Note that not all platforms support this
 * function (e.g. Windows doesn't).
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

/** Create a timer.  uPortTimerStart() must be called to start the timer
 * once it has been successfully created.  It is good practice to create
 * all required timers at initialisation and delete them on exit, only
 * starting/stopping them inbetween, to avoid potential race conditions
 * with timer creation/deletion and timer expiries.
 * IMPORTANT: there is a single timer task/queue and the execution of a timer
 * callback will take time in that queue, potentially delaying the execution
 * of the next timer callback.  The task/queue is implemented as a separate
 * entity to the rest of the OS, so it doesn't take time away from a
 * customer's timer functions, but the "ubxlib" users of this timer API
 * should respect each others' need for accurate timer callback execution
 * by keeping their callbacks short in duration and certainly never blocking.
 * It is NOT currently a requirement that this API is implemented: where
 * it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED should be returned.
 *
 * @param[out] pTimerHandle         a place to put the timer handle.
 * @param[in] pName                 a name for the timer, used for debug
 *                                  purposes only; should be a null-terminated
 *                                  string, may be NULL.  The value will be
 *                                  copied.
 * @param[in] pCallback             the timer callback routine.  The stack size
 *                                  of the context within which the callback
 *                                  is called will be specific to your OS and
 *                                  configured in your OS; should not be NULL.
 * @param[in] pCallbackParam        a parameter that will be provided to the
 *                                  timer callback routine as its second parameter
 *                                  when it is called; may be NULL.
 * @param intervalMs                the time interval in milliseconds.
 * @param periodic                  if true the timer will be restarted after it
 *                                  has expired, else the timer will be one-shot.
 * @return                          zero on success else negative error code.
 */
int32_t uPortTimerCreate(uPortTimerHandle_t *pTimerHandle,
                         const char *pName,
                         pTimerCallback_t pCallback,
                         void *pCallbackParam,
                         uint32_t intervalMs,
                         bool periodic);

/** Destroy a timer.  If the timer is already running it will be stopped
 * and then destroyed.  It is NOT currently a requirement that this API is
 * implemented: where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.
 *
 * @param timerHandle       the handle of the timer.
 * @return                  zero on success else negative error code.
 */
int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle);

/** Start a timer.  If the timer is already running it is restarted.
 * It is NOT currently a requirement that this API is implemented: where it
 * is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED should be returned.
 *
 * @param timerHandle       the handle of the timer.
 * @return                  zero on success else negative error code.
 */
int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle);

/** Stop a timer.  If the timer is not running this function returns
 * success.  It is NOT currently a requirement that this API is
 * implemented: where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.
 *
 * @param timerHandle       the handle of the timer.
 * @return                  zero on success else negative error code.
 */
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle);

/** Change a timer interval.  It is OS dependent as to whether the
 * interval of a timer that is currently running is changed by this
 * or not; it is wise to stop the timer first if you care about
 * that.  It is NOT currently a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED should be
 * returned.  If the other timer API functions are supported then this
 * one must also be supported.
 *
 * @param timerHandle       the handle of the timer.
 * @param intervalMs        the new time interval in milliseconds.
 * @return                  zero on success else negative error code.
 */
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs);

/* ----------------------------------------------------------------
 * FUNCTIONS: ACQUIRING EXECUTABLE MEMORY
 * -------------------------------------------------------------- */

/** Create or prepare a chunk of RAM for executing for example a
 *  library loaded by lib_common functionality.
 *
 * @param[in] pChunkToMakeExecutable for implementations where a chunk's
 *                                   permissions can be dynamically changed.
 *                                   Set to NULL if not used.
 * @param[in,out] pSize              for implementations where a chunk's
 *                                   size needs to be given.
 *                                   For all implementations returns size of
 *                                   chunk.
 * @param flags                      for implementations where a chunk's
 *                                   MPU flags can be set at runtime.
 *                                   Set to #U_PORT_EXECUTABLE_CHUNK_NO_FLAGS
 *                                   if not used.
 * @param index                      for implementations where a chunk
 *                                   can only be specified at compile time.
 *                                   Index allows the user to specify several
 *                                   chunks at compile time.
 *                                   Set to #U_PORT_NO_EXECUTABLE_CHUNK if not used.
 * @return                           pointer to memory area or NULL if failed
 */
void *uPortAcquireExecutableChunk(void *pChunkToMakeExecutable,
                                  size_t *pSize,
                                  uPortExeChunkFlags_t flags,
                                  uPortChunkIndex_t index);

#ifdef __cplusplus
}
#endif

/* ----------------------------------------------------------------
 * INCLUDE FOR U_CFG_MUTEX_DEBUG
 * -------------------------------------------------------------- */

/* This is included down here as we (a) need it to be brought into
 * everywhere that the OS port functions are called, (b) it needs
 * the types above and (c) we don't want its macros to modify the
 * function prototypes above.
 */
#ifdef U_CFG_MUTEX_DEBUG
# include "u_mutex_debug.h"
#endif

/** @}*/

#endif // _U_PORT_OS_H_

// End of file
