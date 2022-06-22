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

#ifndef _U_PORT_PRIVATE_H_
#define _U_PORT_PRIVATE_H_

/** @defgroup SARAR5UCPU  SARA R5 Private
* @{
*/

/** @file
 * @brief Stuff private to SARAR5UCPU platform.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Max length for the timer name supported.
 */
#ifndef U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES
# define U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES 8
#endif

/** Maximum number of timers supported.
 */
#ifndef U_PORT_PRIVATE_MAXIMUM_NO_OF_TIMERS
# define U_PORT_PRIVATE_MAXIMUM_NO_OF_TIMERS 8
#endif

/** Maximum number of counting semaphores supported.
 */
#ifndef U_PORT_PRIVATE_MAXIMUM_NO_OF_SEMAPHORES
# define U_PORT_PRIVATE_MAXIMUM_NO_OF_SEMAPHORES 8
#endif

/** Maximum number of threads supported.
 */
#ifndef U_PORT_PRIVATE_MAXIMUM_NO_OF_THREADS
# define U_PORT_PRIVATE_MAXIMUM_NO_OF_THREADS 16
#endif

/** Maximum number of queues supported.
 */
#ifndef U_PORT_PRIVATE_MAXIMUM_NO_OF_QUEUES
# define U_PORT_PRIVATE_MAXIMUM_NO_OF_QUEUES 32
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the private stuff.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateInit();

/** Deinitialise the private stuff.
 */
void uPortPrivateDeinit();

/** Create task, and start a task.
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
int32_t uPortPrivateTaskCreate(void (*pFunction)(void *),
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
int32_t uPortPrivateTaskDelete(const uPortTaskHandle_t taskHandle);

/** Create a queue.
 *
 * @param queueLength    the maximum length of the queue in units
 *                       of itemSizeBytes.
 * @param itemSizeBytes  the size of each item on the queue.
 * @param pQueueHandle   a place to put the handle of the queue.
 * @return               zero on success else negative error code.
 */
int32_t uPortPrivateQueueCreate(size_t queueLength,
                                size_t itemSizeBytes,
                                uPortQueueHandle_t *pQueueHandle);

/** Delete the given queue.
 *
 * @param queueHandle  the handle of the queue to be deleted.
 * @return             zero on success else negative error code.
 */
int32_t uPortPrivateQueueDelete(const uPortQueueHandle_t queueHandle);

/** Peek the given queue; the data is copied out of the queue but
 * is NOT removed from the queue. If the queue is empty
 * U_ERROR_COMMON_TIMEOUT is returned.
 *
 * @param queueHandle the handle of the queue.
 * @param pEventData  pointer to a place to put incoming data.
 * @return            zero on success else negative error code.
 */
int32_t uPortPrivateQueuePeek(const uPortQueueHandle_t queueHandle,
                              void *pEventData);

/** Add a timer entry to the list.
 *
 * @param pHandle         a place to put the timer handle.
 * @param pName           a name for the timer, used for debug
 *                        purposes only; should be a null-terminated
 *                        string, may be NULL.  The value will be
 *                        copied.
 * @param pCallback       the timer callback routine.
 * @param pCallbackParam  a parameter that will be provided to the
 *                        timer callback routine as its second parameter
 *                        when it is called; may be NULL.
 * @param intervalMs      the time interval in milliseconds.
 * @param periodic        if true the timer will be restarted after it
 *                        has expired, else the timer will be one-shot.
 * @return                zero on success else negative error code.
 */
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                const char *pName,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic);

/** Remove a timer entry from the list.
 *
 * @param handle  the handle of the timer to be removed.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerDelete(const uPortTimerHandle_t handle);

/** Change the timer interval.
 *
 * @param handle      the handle of the timer to be changed.
 * @param intervalMs  the new time interval in milliseconds.
 * @return            zero on success else negative error code.
 */
int32_t uPortPrivateTimerChangeInterval(const uPortTimerHandle_t handle,
                                        uint32_t intervalMs);

/** Start a timer.
 *
 * @param handle  the handle of the timer to be started.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle);

/** Stop a timer.
 *
 * @param handle  the handle of the timer to be stopped.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerStop(const uPortTimerHandle_t handle);

/** Create a semaphore.
 *
 * @param pSemaphoreHandle a place to put the semaphore handle.
 * @param initialCount     initial semaphore count
 * @param limit            maximum permitted semaphore count
 * @return                 zero on success else negative error code
 */
int32_t uPortPrivateSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                                    uint32_t initialCount,
                                    uint32_t limit);

/** Destroy a semaphore.
 *
 * @param semaphoreHandle the handle of the semaphore.
 * @return                zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle);

/** Take the given semaphore, waiting until it is available if
 * it is already taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle);

/** Try to take the given semaphore, waiting up to delayMs
 * if it is currently taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @param delayMs          the maximum time to wait in milliseconds.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                                     int32_t delayMs);

/** Give a semaphore, unless the semaphore is already at its maximum permitted
 *  count.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle);

#ifdef __cplusplus
}
#endif

/** @} */ // end of SARAR5UCPU

#endif // _U_PORT_PRIVATE_H_

// End of file
