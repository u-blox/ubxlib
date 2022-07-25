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

#ifndef _U_PORT_EVENT_QUEUE_H_
#define _U_PORT_EVENT_QUEUE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief An event queue.  Simply put, allows the user to run a
 * function in its own task context, driven asynchronously, with
 * parameters sent through an OS queue.  These functions are
 * thread-safe except that an event queue should not be closed
 * while uPortEventQueueSend() or uPortEventQueueSendIrq() are
 * in progress.
 *
 * It works like this.  If you have function of the form, say:
 *
 * ```
 * void myFunction(int32_t a, char *pBuffer)
 * {
 *     *pBuffer = (char) a;
 * )
 * ```
 *
 * ...which you would like to run asynchronously, you would
 * re-write it as:
 *
 * ```
 * typedef struct {
 *     int32_t a;
 *     char *pBuffer;
 * } myStruct_t;
 *
 * void myFunction(void *pParam, size_t paramLengthBytes)
 * {
 *     myStruct_t *pMyStruct = (myStruct_t *) pParam;
 *
 *     *(pMyStruct->pBuffer) = (char) pMyStruct->a;
 * )
 * ```
 *
 * In other words, your parameters would be defined as a
 * struct and you then cast the `void *` parameter that your
 * function receives to that struct before proceeding as
 * normal.  `paramLengthBytes` (passed through from
 * `uPortEventQueueSend()`) may be useful if `pParam` is of
 * variable size but it may be ignored (i.e. just add a line
 *
 * ```
 * (void) paramLengthBytes;
 * ```
 *
 * ...to your function to keep Lint and the compiler happy) if
 * the size is fixed.
 *
 * `uPortEventQueueOpen()` creates the OS task in
 * which `myFunction()` will run and the associated queue.
 *
 * A call to `uPortEventQueueSend()` with a parameter
 * block will copy that parameter block onto the queue from
 * where `myFunction()` will be invoked with it. This may be
 * repeated as necessary. `uPortEventQueueSendIrq()` is
 * a version which is safe to call from an interrupt.
 *
 * `uPortEventQueueClose()` shuts down the queue and deletes
 * the task.  This is a cooperative process: your function
 * must have emptied the queue and exited for shut-down to
 * complete.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_EVENT_QUEUE_MAX_NUM
/** The maximum number of event queues.
 */
# define U_PORT_EVENT_QUEUE_MAX_NUM 20
#endif

#ifndef U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES
/** The maximum length of parameter block that can be sent on an
 * event queue.
 */
# define U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES 128
#endif

/** The length of uEventQueueControlOrSize_t (see implementation).
 */
#define U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES 4

#ifndef U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES
/** The minimum stack size for an event queue task.
 */
# define U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES  768 +                 \
                           U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES + \
                           U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Open an event queue.
 *
 * @param[in] pFunction        the function that will be called by
 *                             the queue, cannot be NULL.
 * @param[in] pName            a name to give the task that is
 *                             at the end of the event queue.  May
 *                             be NULL in which case a default name
 *                             will be used.
 * @param paramMaxLengthBytes  the maximum length of the parameters
 *                             structure to pass to the function,
 *                             cannot be larger than
 *                             #U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES.
 * @param stackSizeBytes       the stack size of the task that the
 *                             function will be run in, must be
 *                             at least
 *                             #U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
 * @param priority             the priority of the task that the
 *                             function will be run in; see
 *                             u_cfg_os_platform_specific.h for
 *                             your platform for more information.
 *                             The default application, for instance,
 *                             runs at U_CFG_OS_APP_TASK_PRIORITY.  Unless
 *                             you know what you are doing it is STRONGLY
 *                             advised to run all your event queues at
 *                             the same U_CFG_OS_APP_TASK_PRIORITY; this
 *                             way the OS will round-robin schedule the
 *                             event tasks and no-one will be starved. If
 *                             you chose different priorities it is very
 *                             easy for a task to become starved of run-time,
 *                             meaning it will not be able to empty its queue,
 *                             the queue may become full and then
 *                             uPortEventQueueSend() will block. This holds
 *                             true even for an event queue being fed from
 *                             an interrupt if the receiving task is
 *                             forwarding the events to another queue: all
 *                             the event tasks should run at the same
 *                             priority otherwise the effective queue depth
 *                             is that of one link in the chain, not the sum
 *                             of the links in the chain, and you risk dropping
 *                             characters at uPortEventQueueSendIrq().
 * @param queueLength          the number of items to let onto the
 *                             queue before blocking or returning an
 *                             error, must be at least 1.
 * @return                     a handle for the event queue on success,
 *                             else negative error code.
 */
int32_t uPortEventQueueOpen(void (*pFunction) (void *, size_t),
                            const char *pName,
                            size_t paramMaxLengthBytes,
                            size_t stackSizeBytes,
                            int32_t priority,
                            size_t queueLength);

/** Send to an event queue.  The data at pParam will be copied
 * onto the queue.  If the queue is full this function will block
 * until room is available.  An event queue should not be closed
 * while this function is in progress.
 *
 * @param handle            the handle for the event queue.
 * @param[in] pParam        a pointer to the parameters structure
 *                          to send.  May be NULL, in which case
 *                          paramLengthBytes must be zero.
 * @param paramLengthBytes  the length of the parameters
 *                          structure.  Must be less than or
 *                          equal to paramMaxLengthBytes as
 *                          given to uPortEventQueueOpen().
 * @return                  zero on success else negative error code.
 */
int32_t uPortEventQueueSend(int32_t handle, const void *pParam,
                            size_t paramLengthBytes);

/** Send to an event queue from an interrupt.  The data at
 * pParam will be copied onto the queue.  If the queue is full
 * the event will not be sent and an error will be returned.
 * Note: you must ensure that your interrupt stack is large
 * enough to hold an array of size paramLengthBytes +
 * #U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES. An event
 * queue should not be closed while this function is in
 * progress.
 *
 * @param handle            the handle for the event queue.
 * @param[in] pParam        a pointer to the parameters structure
 *                          to send.  May be NULL, in which case
 *                          paramLengthBytes must be zero.
 * @param paramLengthBytes  the length of the parameters
 *                          structure.  Must be less than or
 *                          equal to paramMaxLengthBytes as
 *                          given to uPortEventQueueOpen().
 * @return                  zero on success else negative error code.
 */
int32_t uPortEventQueueSendIrq(int32_t handle, const void *pParam,
                               size_t paramLengthBytes);

/** Detect whether the task currently executing is the
 * event task for the given event queue.  Useful if you
 * have code which is called a few levels down from the
 * event handler both by event code and other code and
 * needs to know which context it is in.
 *
 * @param handle  the handle for the event queue.
 * @return        true if the current task is the event
 *                task for the given handle, else false.
 */
bool uPortEventQueueIsTask(int32_t handle);

/** Get the stack high watermark, the minimum free
 * stack, for the task at the end of the given event
 * queue in bytes.
 *
 * @param handle   the handle of the queue to check.
 * @return         the minimum stack free for the lifetime
 *                 of the event task in bytes, else
 *                 negative error code.
 */
int32_t uPortEventQueueStackMinFree(int32_t handle);

/** Close an event queue.
 *
 * COMMON CODING ERROR: there is a common coding error
 * in the use of this function which can lead to a mutex
 * deadlock.  It goes as follows:
  *
 * - an event queue is used by an API, and that API
 *   protects all of its functions for re-entrancy with
 *   a mutex M,
 * - the event callback function passed to
 *   uPortEventQueueOpen(), let's call it C(), also locks
     mutex M,
 * - when the API is closed, the function that closes the API
 *   locks mutex M and then calls uPortEventQueueClose(),
 * - in order to exit, the event queue code has to shut-down
 *   the task that it launched to run the call-back in, and
 *   such an event, as is the nature of events, can happen
 *   at any time...
 * - so, after the function that closes the API has locked
 *   mutex M, such an even goes off; C() is now going to be
 *   called but it can't have mutex M 'cos it has already
 *   been locked, it sits there waiting for the mutex,
 * - the function that closes the API goes on to call
 *   uPortEventQueueClose(), which cannot complete because
 *   the task it is running to call C() cannot exit.
 *
 * We're mutex locked.
 *
 * To avoid this pitfall you MUST MAKE SURE that C()
 * either (a) does not lock your API mutex or, if it does,
 * you let it know when a shut-down is in progress so that
 * it can ignore any events during that time and not try
 * to lock the mutex at all.
 *
 * @param handle  the handle of the event queue to close.
 * @return        zero on success else negative error code.
 */
int32_t uPortEventQueueClose(int32_t handle);

/** Get the number of entries free on the given event queue.
 * It is NOT a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.
 *
 * @param handle  the handle of the event queue.
 * @return        on success the number of entries free, else
 *                negative error code.
 */
int32_t uPortEventQueueGetFree(int32_t handle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_EVENT_QUEUE_H_

// End of file
