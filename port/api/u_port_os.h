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

#ifndef _U_PORT_OS_H_
#define _U_PORT_OS_H_

/* No #includes allowed here */

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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Mutex handle.
 */
typedef void *uPortMutexHandle_t;

/** Queue handle.
 */
typedef void *uPortQueueHandle_t;

/** Task handle.
 */
typedef void *uPortTaskHandle_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: TASKS
 * -------------------------------------------------------------- */

/** Create task, and start, a task.
 *
 * @param pFunction      the function that forms the task.
 * @param pName          a null-terminated string naming the task,
 *                       may be NULL.
 * @param stackSizeBytes the number of bytes of memory to dynamically
 *                       allocate for stack.
 * @param pParameter     a pointer that will be passed to pFunction
 *                       when the task is started.
 *                       The thing at the end of this pointer must be
 *                       there for the lifetime of the task, it is
 *                       not copied.  May be NULL.
 * @param priority       the priority at which to run the task,
 *                       the meaning of which is platform dependent.
 * @param pTaskHandle    a place to put the handle of the created
 *                       task.
 * @return               zero on success else negative error code.
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

/** Get the stack high watermark, i.e. the minimum amount
 * of stack free, in bytes, for a given task.
 *
 * @param taskHandle  the task handle to check.
 * @return            the minimum amount of stack free for
 *                    the lifetime of the task in bytes,
 *                    else negative error code.
 */
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

/** Create a queue.
 *
 * @param queueLength    the maximum length of the queue in units
 *                       of itemSizeBytes.
 * @param itemSizeBytes  the size of each item on the queue.
 * @param pQueueHandle   a place to put the handle of the queue.
 * @return               zero on success else negative error code.
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
 * @param queueHandle  the handle of the queue.
 * @param pEventData   pointer to the data to send.  The data will
 *                     be copied into the queue and hence can be
 *                     destroyed by the caller once this functions
 *                     returns.
 * @return             zero on success else negative error code.
 */
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData);

/** Send to the given queue from an interrupt.  If the queue is
 * full this function will return an error.
 *
 * @param queueHandle  the handle of the queue.
 * @param pEventData   pointer to the data to send.  The data will
 *                     be copied into the queue and hence can be
 *                     destroyed by the caller once this functions
 *                     returns.
 * @return             zero on success else negative error code.
 */
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData);

/** Receive from the given queue, blocking until something is
 * received.
 *
 * @param queueHandle the handle of the queue.
 * @param pEventData  pointer to a place to put incoming data.
 * @return            zero on success else negative error code.
 */
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData);

/** Try to receive from the given queue, waiting for the given
 * time for something to arrive.
 *
 * @param queueHandle the handle of the queue.
 * @param waitMs      the amount of time to wait in milliseconds.
 * @param pEventData  pointer to a place to put incoming data.
 * @return            zero if someting is received else negative
 *                    error code.
 */
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData);

/* ----------------------------------------------------------------
 * FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

/** Create a mutex.
 *
 * @param pMutexHandle a place to put the mutex handle.
 * @return             zero on success else negative error code.
 */
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle);

/** Destroy a mutex.
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

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_OS_H_

// End of file
