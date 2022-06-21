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

/** @file
 * @brief Stuff private to the Windows porting layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_MAX_NUM_TASKS
/** The maximum number of tasks that can be created.
 */
# define U_PORT_MAX_NUM_TASKS 64
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Initialise the private bits of the porting layer.
 *
 * @return: zero on success else negative error code.
 */
int32_t uPortPrivateInit(void);

/** Deinitialise the private bits of the porting layer.
 */
void uPortPrivateDeinit(void);

/** Enter a critical section: no tasks will be rescheduled.
 * until uPortExitCritical() is called.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateEnterCritical();

/** Leave a critical section.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateExitCritical();

/* ----------------------------------------------------------------
 * FUNCTIONS: TASKS
 * -------------------------------------------------------------- */

/** Create and start a task.
 *
 * @param pFunction      the function that forms the task.
 * @param stackSizeBytes the number of bytes of memory to dynamically
 *                       allocate for stack.
 * @param pParameter     a pointer that will be passed to pFunction
 *                       when the task is started.
 *                       The thing at the end of this pointer must be
 *                       there for the lifetime of the task, it is
 *                       not copied.  May be NULL.
 * @param priority       the priority at which to run the task.
 * @param pTaskHandle    a place to put the handle of the created
 *                       task.
 * @return               zero on success else negative error code.
 */
int32_t uPortPrivateTaskCreate(void (*pFunction)(void *),
                               size_t stackSizeBytes,
                               void *pParameter,
                               int32_t priority,
                               uPortTaskHandle_t *pTaskHandle);

/** Delete the given task.
 *
 * @param taskHandle  the handle of the task to be deleted.
 *                    Use NULL to delete the current task.
 * @return            zero on success else negative error code.
 */
int32_t uPortPrivateTaskDelete(const uPortTaskHandle_t taskHandle);

/** For convenience the task priorities are kept in a 0 to 15 range,
 * however within the Windows thread API the priorities are -2 to
 * +2: this function converts the 0 to 15 values into the Windows
 * native values.
 *
 * @param priority  the 0 to 15 priority value range.
 * @return          the value in -2 to +2 Windows value range.
 */
int32_t uPortPrivateTaskPriorityConvert(int32_t priority);

/* ----------------------------------------------------------------
 * FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

/** Add a queue to the list of queues.
 *
 * @param itemSizeBytes the size of a queue item.
 * @param maxNumItems   the maximum number of items the queue can
 *                      accommodate.
 * @return              on success the handle for the queue, else
 *                      negative error code.
 */
int32_t uPortPrivateQueueAdd(size_t itemSizeBytes, size_t maxNumItems);

/** Write a block of data to the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData   the block of data to write, of size itemSizeBytes,
 *                as was used in the uPortPrivateQueueAdd() call that
 *                created the queue; cannot be NULL.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueWrite(int32_t handle, const char *pData);

/** Read a block of data from the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData   storage for the data, of size itemSizeBytes, as was
 *                used in the uPortPrivateQueueAdd() call that
 *                created the queue; may be NULL in which case the
 *                block is thrown away.
 * @param waitMs  the time to wait for data to become available;
 *                specify -1 for blocking.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueRead(int32_t handle, char *pData, int32_t waitMs);

/** Peek the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData  storage for the data, of size itemSizeBytes, as was
 *                used in the uPortPrivateQueueAdd() call that
 *                created the queue; cannot be NULL.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueuePeek(int32_t handle, char *pData);

/** Get the number of free spaces in the given queue.
 *
 * @param handle  the handle of the queue.
 * @return        on success the number of free spaces, else negative
 *                error code.
 */
int32_t uPortPrivateQueueGetFree(int32_t handle);

/** Remove a queue from the list of queues.
 *
 * @param handle  the handle of the queue.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueRemove(int32_t handle);

/* ----------------------------------------------------------------
 * FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

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

/** Start a timer.
 *
 * @param handle  the handle of the timer.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle);

/** Change a timer interval.
 *
 * @param handle       the handle of the timer.
 * @param intervalMs   the new time interval in milliseconds.
 * @return             zero on success else negative error code.
 */
int32_t uPortPrivateTimerChange(const uPortTimerHandle_t handle,
                                uint32_t intervalMs);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
