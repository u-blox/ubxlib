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

#ifndef _U_PORT_UART_H_
#define _U_PORT_UART_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for UART access functions.  These functions
 * are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_UART_EVENT_QUEUE_SIZE
/** The UART event queue size.
 */
# define U_PORT_UART_EVENT_QUEUE_SIZE 20
#endif

#ifndef U_PORT_UART_WRITE_TIMEOUT_MS
/** uPortUartWrite() should always succeed in sending all characters;
 * however, when flow control is enabled, it is possible that the
 * receiving UART at the far end blocks transmission, potentially
 * indefinitely, causing uPortUartWrite() to hang.  It is not
 * desirable to cause the whole application to fail because of an
 * IO function; this [deliberately very large] defensive time-out
 * may be employed by an implementation of uPortUartWrite()
 * as a guard against that.
 */
# define U_PORT_UART_WRITE_TIMEOUT_MS 30000
#endif

/** The event which means that received data is available; this
 * will be sent if the receive buffer goes from empty to containing
 * one or more bytes of received data. It is used as a bit-mask.
 * It is the only U_PORT_UART_EVENT_BITMASK_xxx currently supported.
 */
#define U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED 0x01

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise UART handling.  THERE IS NO NEED FOR THE USER
 * TO CALL THIS: it is called by uPortInit().
 *
 * @return  zero on success else negative error code.
 */
int32_t uPortUartInit();

/** Shutdown UART handling.  THERE IS NO NEED FOR THE USER
 * TO CALL THIS: it is called by uPortDeinit().
 */
void uPortUartDeinit();

/** Open a UART instance.  If a UART instance has already
 * been opened on the given UART HW block this function returns
 * an error.  Note that the pin numbers are those of the MCU:
 * if you are using an MCU inside a u-blox module the IO pin
 * numbering for the module is likely different to that from
 * the MCU: check the data sheet for the module to determine
 * the mapping.
 *
 * IMPORTANT: some platforms, specifically Zephyr, used on NRF53,
 * do not permit UART pin choices to be made at run-time, only at
 * compile time.  For such platforms the pins passed in here MUST
 * be -1 (otherwise an error will be returned) and you MUST check
 * the README.md for that platform to find out how the pins
 * are chosen.
 *
 * @param uart                   the UART HW block to use.
 * @param baudRate               the baud rate to use.
 * @param[in] pReceiveBuffer     a receive buffer to use,
 *                               should be NULL and a buffer
 *                               will be allocated by the driver.
 *                               If non-NULL then the given buffer
 *                               will be used, however some
 *                               platforms (e.g. ESP32) currently
 *                               do not support passing in a buffer
 *                               (an error will be returned) so to be
 *                               platform independent NULL must
 *                               be used.
 * @param receiveBufferSizeBytes the amount of memory to allocate
 *                               for the receive buffer.  If
 *                               pReceiveBuffer is not NULL
 *                               then this is the amount of
 *                               memory at pReceiveBuffer.
 * @param pinTx                  the transmit (output) pin,
 *                               a positive integer or -1 if the
 *                               pin choice has already been
 *                               determined at compile time.
 * @param pinRx                  the receive (input) pin,
 *                               a positive integer or -1 if the
 *                               pin choice has already been
 *                               determined at compile time.
 * @param pinCts                 the CTS (input) flow
 *                               control pin, asserted
 *                               by the modem when it is
 *                               ready to receive
 *                               data; use -1 for none or if
 *                               the pin choice has already been
 *                               determined at compile time.
 * @param pinRts                 the RTS (output) flow
 *                               control pin, asserted
 *                               when we are ready to
 *                               receive data from the
 *                               modem; use -1 for none or if
 *                               the pin choice has already been
 *                               determined at compile time.
 * @return                       a UART handle else negative
 *                               error code.
 */
int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t receiveBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts);

/** Close a UART.  Note that this should NOT be called if
 * a UART read or write could be in progress.
 *
 * @param handle the handle of the UART instance to close.
 */
void uPortUartClose(int32_t handle);

/** Get the number of bytes waiting in the receive buffer
 * of a UART instance.
 *
 * @param handle the handle of the UART instance.
 * @return       the number of bytes in the receive buffer
 *               or negative error code.
 */
int32_t uPortUartGetReceiveSize(int32_t handle);

/** Read from the given UART instance, non-blocking:
 * up to sizeBytes of data already in the UART buffer will
 * be returned.
 *
 * @param handle       the handle of the UART instance.
 * @param[out] pBuffer a pointer to a buffer in which to store
 *                     received bytes.
 * @param sizeBytes    the size of buffer pointed to by pBuffer.
 * @return             the number of bytes received else negative
 *                     error code.
 */
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes);

/** Write to the given UART interface.  Will block until
 * all of the data has been written or an error has occurred.
 *
 * @param handle      the handle of the UART instance.
 * @param[in] pBuffer a pointer to a buffer of data to send.
 * @param sizeBytes   the number of bytes in pBuffer.
 * @return            the number of bytes sent or negative
 *                    error code.
 */
int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes);

/** Set a callback to be called when a UART event occurs.
 * pFunction will be called asynchronously in its own task,
 * for which the stack size and priority can be specified.
 * Only one callback may be set per UART instance; the
 * callback receives the UART handle as its first parameter
 * and the event bit-map as its second parameter. If a
 * callback has already been set for a UART instance this
 * function will return an error.
 *
 * @param handle           the handle of the UART instance.
 * @param filter           a bit-mask to filter the events
 *                         on which pFunction will be called.
 *                         1 in a bit position means include
 *                         that event, 0 means don't; at least
 *                         one bit must be set.  Select bits
 *                         from one or more of
 *                         U_PORT_UART_EVENT_BITMASK_xxx or
 *                         set all bits to enable everything.
 * @param[in] pFunction    the function to call, cannot be
 *                         NULL.
 * @param[in] pParam       a parameter which will be passed
 *                         to pFunction as its last parameter
 *                         when it is called.
 * @param stackSizeBytes   the number of bytes of stack for
 *                         the task in which pFunction is
 *                         called, must be at least
 *                         #U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
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
int32_t uPortUartEventCallbackSet(int32_t handle,
                                  uint32_t filter,
                                  void (*pFunction)(int32_t, uint32_t,
                                                    void *),
                                  void *pParam,
                                  size_t stackSizeBytes,
                                  int32_t priority);

/** Remove a UART event callback.
 *
 * NOTE: under the hood, this function likely calls
 * uPortEventQueueClose() - PLEASE READ THE NOTE against
 * that function concerning the potential for mutex lock-ups
 * in the design of your re-entrancy protection.  You might
 * use the pParam context pointer that is passed to the event
 * callback (see uPortUartEventCallbackSet()) to inform your
 * callback when it is being shut-down, and hence avoid such
 * mutex lock-up issues.
 *
 * @param handle  the handle of the UART instance for
 *                which the callback is to be removed.
 */
void uPortUartEventCallbackRemove(int32_t handle);

/** Get the filter for which a callback is currently set.
 * This can be used to determine whether a callback is
 * set: if a callback is not set the return value will be
 * zero.
 *
 * @param handle  the handle of the UART instance.
 * @return        the filter bit-mask for the currently set
 *                callback.
 */
uint32_t uPortUartEventCallbackFilterGet(int32_t handle);

/** Change the callback filter bit-mask.  If no event
 * callback is set an error will be returned.
 *
 * @param handle  the handle of the UART instance.
 * @param filter  the new filter bit-mask, must be non-zero.
 * @return        zero on success else negative error code.
 */
int32_t uPortUartEventCallbackFilterSet(int32_t handle,
                                        uint32_t filter);

/** Send an event to the callback.  This allows the user to
 * re-trigger events: for instance, if a data event has only
 * been partially handled it can be re-triggered by calling
 * this function with #U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED
 * set.  This call will block until there is room in the queue
 * to send the event; if you want the function to return
 * if there is no room in the queue to send the event then use
 * uPortUartEventTrySend() instead
 *
 * @param handle      the handle of the UART instance.
 * @param eventBitMap the events bit-map with at least one of
 *                    U_PORT_UART_EVENT_BITMASK_xxx set.
 * @return            zero on success else negative error code.
 */
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap);

/** Send an event to the callback, returning if there is no
 * room in the queue to send the event within the given time.
 * This allows the user to re-trigger events: for instance,
 * if a data event has only been partially handled it can be
 * re-triggered by calling this function with
 * #U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED set.  Note that NOT
 * ALL PLATFORMS support this API: where it is not implemented
 * #U_ERROR_COMMON_NOT_IMPLEMENTED or #U_ERROR_COMMON_NOT_SUPPORTED
 * should be returned.
 *
 * @param handle      the handle of the UART instance.
 * @param eventBitMap the events bit-map with at least one of
 *                    U_PORT_UART_EVENT_BITMASK_xxx set.
 * @param delayMs     the maximum time to wait in milliseconds.
 * @return            zero on success else negative error code.
 */
int32_t uPortUartEventTrySend(int32_t handle,
                              uint32_t eventBitMap,
                              int32_t delayMs);

/** Detect whether the task currently executing is the
 * event callback for this UART.  Useful if you have code which
 * is called a few levels down from the callback both by
 * event code and other code and needs to know which context it
 * is in.
 *
 * @param handle  the handle of the UART instance.
 * @return        true if the current task is the event
 *                callback for this UART, else false.
 */
bool uPortUartEventIsCallback(int32_t handle);

/** Get the stack high watermark, the minimum amount of
 * free stack, in bytes, for the task at the end of the event
 * queue.
 *
 * @param handle  the handle of the UART instance.
 * @return        the minimum amount of free stack for the
 *                lifetime of the task at the end of the event
 *                queue in bytes, else negative error code.
 */
int32_t uPortUartEventStackMinFree(int32_t handle);

/** Determine if RTS flow control, that is a signal from
 * the module to this software that the module is ready to
 * receive data, is enabled.
 *
 * @param handle the handle of the UART instance.
 * @return       true if RTS flow control is enabled
 *               on this UART, else false.
 */
bool uPortUartIsRtsFlowControlEnabled(int32_t handle);

/** Determine if CTS flow control, that is a signal from
 * this software to the module that this sofware is ready
 * to accept data, is enabled.  Note that this returns
 * true even if CTS flow control is currently suspended
 * by a call to uPortUartCtsSuspend().
 *
 * @param handle the handle of the UART instance.
 * @return       true if CTS flow control is enabled
 *               on this UART, else false.
 */
bool uPortUartIsCtsFlowControlEnabled(int32_t handle);

/** Suspend CTS flow control.  This is useful if the device
 * on the other end of the UART can enter a sleep state during
 * which the CTS line may float such as to prevent the UART
 * from communicating with the device.  When that happens, this
 * function may be called while the device is revived from
 * sleep state (e.g. by sending it "wake-up" characters), then
 * CTS flow control should be resumed afterwards with a call to
 * uPortUartCtsResume().  This function may NOT be supported on all
 * platforms; where it is not supported the function will return
 * #U_ERROR_COMMON_NOT_SUPPORTED.
 * If suspension of CTS is supported but CTS flow control is
 * not being used this function will return successfully.
 *
 * @param handle the handle of the UART instance.
 * @return       zero on success else negative error code.
 */
int32_t uPortUartCtsSuspend(int32_t handle);

/** Resume CTS flow control; should be called after
 * uPortUartCtsSuspend() to resume normal flow control operation.
 * This function must be supported if uPortUartCtsSuspend() is
 * supported.  Where uPortUartCtsSuspend() is not supported
 * this function may still be called but will have no effect.
 *
 * @param handle the handle of the UART instance.
 */
void uPortUartCtsResume(int32_t handle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_UART_H_

// End of file
