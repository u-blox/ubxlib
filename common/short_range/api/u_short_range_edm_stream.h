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

#ifndef _U_SHORT_RANGE_EDM_STREAM_H_
#define _U_SHORT_RANGE_EDM_STREAM_H_

/* No #includes allowed here */

/** @file
 * @brief EDM (extended data mode) stream API for short range modules.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_EDM_STREAM_TASK_STACK_SIZE_BYTES
#define U_EDM_STREAM_TASK_STACK_SIZE_BYTES  1536
#endif

#ifndef U_EDM_STREAM_AT_EVENT_QUEUE_SIZE
#define U_EDM_STREAM_AT_EVENT_QUEUE_SIZE 1
#endif

#ifndef U_EDM_STREAM_BT_EVENT_QUEUE_SIZE
#define U_EDM_STREAM_BT_EVENT_QUEUE_SIZE 1
#endif

#ifndef U_EDM_STREAM_DATA_EVENT_QUEUE_SIZE
#define U_EDM_STREAM_DATA_EVENT_QUEUE_SIZE 1
#endif


#ifndef U_EDM_STREAM_TASK_PRIORITY
# define U_EDM_STREAM_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 4)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef enum {
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_BT,
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_WIFI,
    U_SHORT_RANGE_EDM_STREAM_CONNECTION_TYPE_INVALID
} uShortRangeEdmStreamConnectionType_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise stream handling.
 *
 * @return  zero on success else negative error code.
 */
int32_t uShortRangeEdmStreamInit();

/** Shutdown stream handling.
 */
void uShortRangeEdmStreamDeinit();

/** Open an instance. Needs an open UART instance that is not accessed
 * by any other module.
 *
 * @param uartHandle             the UART HW block to use.
 *
 * @return                       a stream handle else negative
 *                               error code.
 */
int32_t uShortRangeEdmStreamOpen(int32_t uartHandle);

/** Close a stream.
 *
 * @param handle  the handle of the stream instance to close.
 */
void uShortRangeEdmStreamClose(int32_t handle);

/** Set at handle, needed so that the stream can intercept outgoing
 * at commands.
 *
 * @param handle   the handle of the stream instance.
 * @param atHandle the at handle.
 */
void uShortRangeEdmStreamSetAtHandle(int32_t handle, void *atHandle);

/** Raw write to the underlaying (UART) layer, bypassing any convertion
 * into EDM packet. Useful if raw at commands needs to be sent in special
 * circumstances.
 *
 * @param handle    the handle of the stream instance.
 * @param pBuffer   a pointer to a buffer of command to send.
 * @param sizeBytes the number of bytes in pBuffer.
 * @return          the number of bytes sent or negative
 *                  error code.
 */
int32_t uShortRangeEdmStreamAtWrite(int32_t handle, const void *pBuffer,
                                    size_t sizeBytes);

/** Get the number of bytes waiting in the receive buffer
 * of a stream instance.
 *
 * @param handle the handle of the stream instance.
 * @return       the number of bytes in the receive buffer
 *               or negative error code.
 */
int32_t uShortRangeEdmStreamAtGetReceiveSize(int32_t handle);

/** Read from the given stream instance, non-blocking:
 * up to sizeBytes of data already in the stream buffer will
 * be returned.
 *
 * @param handle    the handle of the stream instance.
 * @param pBuffer   a pointer to a buffer in which to store
 *                  received bytes.
 * @param sizeBytes the size of buffer pointed to by pBuffer.
 * @return          the number of bytes received else negative
 *                  error code.
 */
int32_t uShortRangeEdmStreamAtRead(int32_t handle, void *pBuffer,
                                   size_t sizeBytes);

/** Write to the given interface on given channel.  Will block until
 * all of the data has been written or an error has occurred.
 *
 * @param handle    the handle of the stream instance.
 * @param channel   the number of for the connection channel given in
 *                  the connected event callback.
 * @param pBuffer   a pointer to a buffer of data to send.
 * @param sizeBytes the number of bytes in pBuffer.
 * @param timeoutMs timeout in ms. If timeout is reached, sending is
 *                  interrupted and the actual number of bytes sent returned.
 *                  Reaching timeout is not considered an error.
 * @return          the number of bytes sent or negative
 *                  error code.
 */
int32_t uShortRangeEdmStreamWrite(int32_t handle, int32_t channel,
                                  const void *pBuffer, size_t sizeBytes,
                                  uint32_t timeoutMs);

/** Set a callback to be called when an AT event occurs.
 * pFunction will be called asynchronously in its own task.
 *
 * @param handle           the handle of the stream instance.
 * @param pFunction        the function to call, cannot be
 *                         NULL.
 * @param pParam           a parameter which will be passed
 *                         to pFunction as its last parameter
 *                         when it is called.
 * @param stackSizeBytes   the number of bytes of stack for
 *                         the task in which pFunction is
 *                         called, must be at least
 *                         U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
 * @param priority         the priority of the task in which
 *                         pFunction is called; see
 *                         u_cfg_os_platform_specific.h for
 *                         your platform for more information.
 *                         The default application, for instance,
 *                         runs at U_CFG_OS_APP_TASK_PRIORITY,
 *                         so if you want pFunction to be
 *                         scheduled before it you might set a
 *                         priority of
 *                         U_CFG_OS_APP_TASK_PRIORITY + 1.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uShortRangeEdmStreamAtCallbackSet(int32_t handle,
                                          void (*pFunction)(int32_t, uint32_t,
                                                            void *),
                                          void *pParam,
                                          size_t stackSizeBytes,
                                          int32_t priority);

/** Remove an AT event callback.
 *
 * @param handle  the handle of the stream instance for
 *                which the callback is to be removed.
 */
void uShortRangeEdmStreamAtCallbackRemove(int32_t handle);

/** Set a callback to be called when an wifi event occurs.
 * pFunction will be called asynchronously in its own task.
 *
 * @param handle           the handle of the stream instance.
 * @param pFunction        the function to call, cannot be
 *                         NULL.
 * @param pParam           a parameter which will be passed
 *                         to pFunction as its last parameter
 *                         when it is called.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uShortRangeEdmStreamWifiEventCallbackSet(int32_t handle,
                                                 void (*pFunction)(int32_t, uint32_t,
                                                                   void *),
                                                 void *pParam);

/** Remove a wifi event callback.
 *
 * @param handle  the handle of the stream instance for
 *                which the callback is to be removed.
 */
void uShortRangeEdmStreamWifiEventCallbackRemove(int32_t handle);


/** Set a callback to be called when a bt event occurs.
 * pFunction will be called asynchronously in its own task.
 *
 * @param handle           the handle of the stream instance.
 * @param pFunction        the function to call, cannot be
 *                         NULL.
 * @param pParam           a parameter which will be passed
 *                         to pFunction as its last parameter
 *                         when it is called.
 * @param stackSizeBytes   the number of bytes of stack for
 *                         the task in which pFunction is
 *                         called, must be at least
 *                         U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
 * @param priority         the priority of the task in which
 *                         pFunction is called; see
 *                         u_cfg_os_platform_specific.h for
 *                         your platform for more information.
 *                         The default application, for instance,
 *                         runs at U_CFG_OS_APP_TASK_PRIORITY,
 *                         so if you want pFunction to be
 *                         scheduled before it you might set a
 *                         priority of
 *                         U_CFG_OS_APP_TASK_PRIORITY + 1.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uShortRangeEdmStreamBtEventCallbackSet(int32_t handle,
                                               void (*pFunction)(int32_t, uint32_t, uint32_t,
                                                                 bool, int32_t, uint8_t *, void *),
                                               void *pParam,
                                               size_t stackSizeBytes,
                                               int32_t priority);

/** Remove a bt event callback.
 *
 * @param handle  the handle of the stream instance for
 *                which the callback is to be removed.
 */
void uShortRangeEdmStreamBtEventCallbackRemove(int32_t handle);

/** Set a callback to be called when data is available.
 * pFunction will be called asynchronously in its own task.
 *
 * @param handle           the handle of the stream instance.
 * @param type             the type of connection.
 * @param pFunction        the function to call, cannot be
 *                         NULL.
 * @param pParam           a parameter which will be passed
 *                         to pFunction as its last parameter
 *                         when it is called.
 * @param stackSizeBytes   the number of bytes of stack for
 *                         the task in which pFunction is
 *                         called, must be at least
 *                         U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
 * @param priority         the priority of the task in which
 *                         pFunction is called; see
 *                         u_cfg_os_platform_specific.h for
 *                         your platform for more information.
 *                         The default application, for instance,
 *                         runs at U_CFG_OS_APP_TASK_PRIORITY,
 *                         so if you want pFunction to be
 *                         scheduled before it you might set a
 *                         priority of
 *                         U_CFG_OS_APP_TASK_PRIORITY + 1.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uShortRangeEdmStreamDataEventCallbackSet(int32_t handle,
                                                 int32_t type,
                                                 void (*pFunction)(int32_t, int32_t, int32_t,
                                                                   char *, void *),
                                                 void *pParam,
                                                 size_t stackSizeBytes,
                                                 int32_t priority);

/** Remove a data callback.
 *
 * @param handle  the handle of the stream instance for
 *                which the callback is to be removed.
 */
void uShortRangeEdmStreamDataCallbackRemove(int32_t handle);


/** Send an event to the callback.  This allows the user to
 * re-trigger events: for instance, if a data event has only
 * been partially handled it can be re-triggered by calling
 * this function with U_SHORT_RANGE_EDM_STREAM_EVENT_BITMASK_DATA_RECEIVED
 * set.
 *
 * @param handle      the handle of the stream instance.
 * @param eventBitMap the events bit-map with at least one of
 *                    U_SHORT_RANGE_EDM_STREAM_EVENT_BITMASK_xxx set.
 * @return            zero on success else negative error code.
 */
int32_t uShortRangeEdmStreamAtEventSend(int32_t handle, uint32_t eventBitMap);

/** Get the stack high watermark, i.e. the minimum amount of
 * free stack, in bytes, for the task at the end of the event
 * queue.
 *
 * @param handle  the handle of the stream instance.
 * @return        the minimum amount of free stack for the
 *                lifetime of the task at the end of the event
 *                queue in bytes, else negative error code.
 */
int32_t uPortShortRangeEdmStremAtEventStackMinFree(int32_t handle);

/** Detect whether the task currently executing is the
 * event callback for this stream. Useful if you have code which
 * is called a few levels down from the callback both by
 * event code and other code and needs to know which context it
 * is in.
 *
 * @param handle  the handle of the edm stream instance.
 * @return        true if the current task is the event
 *                callback for this stream, else false.
 */
bool uShortRangeEdmStreamAtEventIsCallback(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_EDM_STREAM_H_

// End of file
