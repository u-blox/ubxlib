/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_DEVICE_SERIAL_H_
#define _U_DEVICE_SERIAL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup device Device
 *  @{
 */

/** @file
 * @brief Definition of a generic serial device, a virtualised
 * version of u_port_uart.h, used in special cases (for example
 * where a real serial interface is multiplexed into many serial
 * interfaces, as in 3GPP 27.010 CMUX); usage pattern as follows:
 *
 * - Implement the device functions; where a function
 *   is not supported it may be left empty if returning the
 *   default value of #U_ERROR_COMMON_NOT_IMPLEMENTED is
 *   considered appropriate.
 * - Create a callback of type #uDeviceSerialInit_t which
 *   populates #uDeviceSerial_t with your functions.
 * - Your implementation may request context data for its private
 *   use: this can be accessed in your functions (including
 *   #uDeviceSerialInit_t, if required) by calling
 *   #pUInterfaceContext() with pDeviceSerial.
 * - Call #pUDeviceSerialCreate() to create the serial instance
 *   and the context; this will call your #uDeviceSerialInit_t
 *   callback.
 * - The device can now be used by calling the functions in the
 *   vector table, e.g.:
 *
 * ```
 *   pDeviceSerial->open(pDeviceSerial, NULL, 1024);
 *   pDeviceSerial->write(pDeviceSerial, pBuffer, 64);
 *   pDeviceSerial->read(pDeviceSerial, pBuffer, 12);
 *   pDeviceSerial->close(pDeviceSerial);
 * ```
 *
 * - You may call uInterfaceVersion() to obtain the version
 *   of this interface (i.e. #U_DEVICE_SERIAL_VERSION).
 * - When done, uDeviceSerialDelete() should be called to release
 *   memory etc.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The version of this API.
 */
#define U_DEVICE_SERIAL_VERSION 2

/** The event which means that received data is available; this
 * will be sent if the receive buffer goes from empty to containing
 * one or more bytes of received data. It is used as a bit-mask.
 * It is the only U_DEVICE_SERIAL_EVENT_BITMASK_xxx currently
 * supported.
 */
#define U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED 0x01

/* ----------------------------------------------------------------
 * TYPES: THE FUNCTIONS OF A SERIAL INTERFACE
 * -------------------------------------------------------------- */

/* NOTE TO MAINTAINERS:
 *
 * If you add a new function here, don't forget to add a default
 * implementation for it in u_device_serial.c and include that
 * default entry in the initialisation of the vector table in the
 * same file.
 *
 * ALSO don't forget to increment #U_DEVICE_SERIAL_VERSION and
 * please mention the version number from which a new function is
 * available in the comment above the function (see
 * #uDeviceSerialDiscardOnOverflow_t for an example of how to do this).
 */

// Forward declaration.
struct uDeviceSerial_t;

/** Open a serial device.  If the device has already been
 * opened this function returns an error.
 *
 * @param pDeviceSerial               the serial device; cannot be NULL.
 * @param[in] pReceiveBuffer          a receive buffer to use, should be
 *                                    NULL and a buffer will be allocated
 *                                    by the driver. If non-NULL then the
 *                                    given buffer will be used.
 * @param receiveBufferSizeBytes      the amount of memory to allocate
 *                                    for the receive buffer.  If
 *                                    pReceiveBuffer is not NULL then this
 *                                    is the amount of memory at
 *                                    pReceiveBuffer.
 * @return                            zero on success else negative error
 *                                    code.
 */
typedef int32_t (*uDeviceSerialOpen_t)(struct uDeviceSerial_t *pDeviceSerial,
                                       void *pReceiveBuffer,
                                       size_t receiveBufferSizeBytes);

/** Close a serial device; this should NOT be called if
 * a serial read or write could be in progress.
 *
 * @param pDeviceSerial   the serial device; cannot be NULL.
 */
typedef void (*uDeviceSerialClose_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Get the number of bytes waiting in the receive buffer
 * of a serial device.
 *
 * @param pDeviceSerial the serial device; cannot be NULL.
 * @return              the number of bytes in the receive buffer
 *                      or negative error code.
 */
typedef int32_t (*uDeviceSerialGetReceiveSize_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Read from the given serial device, non-blocking:
 * up to sizeBytes of data already in the serial buffer will
 * be returned.
 *
 * @param pDeviceSerial   the serial device; cannot be NULL.
 * @param[out] pBuffer    a pointer to a buffer in which to store
 *                        received bytes.
 * @param sizeBytes       the size of buffer pointed to by pBuffer.
 * @return                the number of bytes received else negative
 *                        error code.
 */
typedef int32_t (*uDeviceSerialRead_t)(struct uDeviceSerial_t *pDeviceSerial,
                                       void *pBuffer, size_t sizeBytes);

/** Write to the given serial device.  Will block until
 * all of the data has been written or an error has occurred.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @param[in] pBuffer    a pointer to a buffer of data to send.
 * @param sizeBytes      the number of bytes in pBuffer.
 * @return               the number of bytes sent or negative
 *                       error code.
 */
typedef int32_t (*uDeviceSerialWrite_t)(struct uDeviceSerial_t *pDeviceSerial,
                                        const void *pBuffer, size_t sizeBytes);

/** Set a callback to be called when an event occurs on the serial
 * interface. pFunction will be called asynchronously in its own
 * task, for which the stack size and priority can be specified.
 * Only one callback may be set per device; the callback receives
 * pDeviceSerial as its first parameter and the event bit-map as its
 * second parameter. If a callback has already been set for the
 * device this function will return an error.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @param filter         a bit-mask to filter the events
 *                       on which pFunction will be called.
 *                       1 in a bit position means include
 *                       that event, 0 means don't; at least
 *                       one bit must be set.  Select bits
 *                       from one or more of
 *                       U_DEVICE_SERIAL_EVENT_BITMASK_xxx or
 *                       set all bits to enable everything.
 * @param[in] pFunction  the function to call, cannot be NULL.
 * @param[in] pParam     a parameter which will be passed
 *                       to pFunction as its last parameter
 *                       when it is called.
 * @param stackSizeBytes the number of bytes of stack for
 *                       the task in which pFunction is
 *                       called, must be at least
 *                       #U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES.
 * @param priority       the priority of the task in which
 *                       pFunction is called; see
 *                       u_cfg_os_platform_specific.h for
 *                       your platform for more information.
 *                       The default application, for instance,
 *                       runs at U_CFG_OS_APP_TASK_PRIORITY,
 *                       so if you want pFunction to be
 *                       scheduled before it you might set a
 *                       priority of
 *                       U_CFG_OS_APP_TASK_PRIORITY + 1.
 * @return               zero on success else negative error
 *                       code.
 */
typedef int32_t (*uDeviceSerialEventCallbackSet_t)(struct uDeviceSerial_t *pDeviceSerial,
                                                   uint32_t filter,
                                                   void (*pFunction)(struct uDeviceSerial_t *,
                                                                     uint32_t,
                                                                     void *),
                                                   void *pParam,
                                                   size_t stackSizeBytes,
                                                   int32_t priority);

/** Remove a serial event callback.
 *
 * NOTE: under the hood, this function likely calls
 * uPortEventQueueClose() - PLEASE READ THE NOTE against
 * that function concerning the potential for mutex lock-ups
 * in the design of your re-entrancy protection.  You might
 * use the pParam context pointer that is passed to the event
 * callback (see #uDeviceSerialEventCallbackSet_t) to inform your
 * callback when it is being shut-down, and hence avoid such
 * mutex lock-up issues.
 *
 * @param pDeviceSerial the serial device; cannot be NULL.
 */
typedef void (*uDeviceSerialEventCallbackRemove_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Get the filter for which a callback is currently set.
 * This can be used to determine whether a callback is
 * set: if a callback is not set the return value will be
 * zero.
 *
 * @param pDeviceSerial the serial device; cannot be NULL.
 * @return              the filter bit-mask for the currently set
 *                      callback.
 */
typedef uint32_t (*uDeviceSerialEventCallbackFilterGet_t)(struct uDeviceSerial_t
                                                          *pDeviceSerial);

/** Change the callback filter bit-mask.  If no event
 * callback is set an error will be returned.
 *
 * @param pDeviceSerial the serial device; cannot be NULL.
 * @param filter        the new filter bit-mask, must be non-zero.
 * @return              zero on success else negative error code.
 */
typedef int32_t (*uDeviceSerialEventCallbackFilterSet_t)(struct uDeviceSerial_t
                                                         *pDeviceSerial,
                                                         uint32_t filter);

/** Send an event to the callback.  This allows the user to
 * re-trigger events: for instance, if a data event has only
 * been partially handled it can be re-triggered by calling
 * this function with #U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED
 * set.  This call will block until there is room in the queue
 * to send the event; if you want the function to return
 * if there is no room in the queue to send the event then use
 * #uDeviceSerialEventTrySend_t instead
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @param eventBitMap    the events bit-map with at least one
 *                       of U_DEVICE_SERIAL_EVENT_BITMASK_xxx set.
 * @return               zero on success else negative error
 *                       code.
 */
typedef int32_t (*uDeviceSerialEventSend_t)(struct uDeviceSerial_t *pDeviceSerial,
                                            uint32_t eventBitMap);

/** Send an event to the callback, returning if there is no
 * room in the queue to send the event within the given time.
 * This allows the user to re-trigger events: for instance,
 * if a data event has only been partially handled it can be
 * re-triggered by calling this function with
 * #U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED set.  Note that NOT
 * ALL PLATFORMS support this API: where it is not implemented
 * #U_ERROR_COMMON_NOT_IMPLEMENTED or #U_ERROR_COMMON_NOT_SUPPORTED
 * should be returned.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @param eventBitMap    the events bit-map with at least one of
 *                       U_DEVICE_SERIAL_EVENT_BITMASK_xxx set.
 * @param delayMs        the maximum time to wait in milliseconds.
 * @return               zero on success else negative error code.
 */
typedef int32_t (*uDeviceSerialEventTrySend_t)(struct uDeviceSerial_t *pDeviceSerial,
                                               uint32_t eventBitMap,
                                               int32_t delayMs);

/** Detect whether the task currently executing is the event
 * callback for this serial device.  Useful if you have code
 * which is called a few levels down from the callback both
 * by event code and other code and needs to know which context
 * it is in.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               true if the current task is the event
 *                       callback for this serial device, else
 *                       false.
 */
typedef bool (*uDeviceSerialEventIsCallback_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Get the stack high watermark, the minimum amount of free
 * stack, in bytes, for the task at the end of the event queue.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               the minimum amount of free stack for the
 *                       lifetime of the task at the end of the event
 *                       queue in bytes, else negative error code.
 */
typedef int32_t (*uDeviceSerialEventStackMinFree_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Determine if RTS flow control, that is a signal from
 * the module to this software that the module is ready to
 * receive data, is enabled.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               true if RTS flow control is enabled
 *                       on this serial device, else false.
 */
typedef bool (*uDeviceSerialIsRtsFlowControlEnabled_t)(struct uDeviceSerial_t
                                                       *pDeviceSerial);

/** Determine if CTS flow control, that is a signal from
 * this software to the module that this sofware is ready
 * to accept data, is enabled.  Note that this returns
 * true even if CTS flow control is currently suspended
 * by a call to #uDeviceSerialCtsSuspend_t.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               true if CTS flow control is enabled
 *                       on this serial device, else false.
 */
typedef bool (*uDeviceSerialIsCtsFlowControlEnabled_t)(struct uDeviceSerial_t
                                                       *pDeviceSerial);

/** Suspend CTS flow control.  This is useful if the device
 * can enter a sleep state during which the CTS line may float
 * such as to prevent the serial interface from communicating
 * with the device.  When that happens, this function may be
 * called while the device is revived from sleep state (e.g.
 * by sending it "wake-up" characters), then CTS flow control
 * should be resumed afterwards with a call to #uDeviceSerialCtsResume_t.
 * This function may NOT be supported in all cases; where it is
 * not supported the function will return
 * #U_ERROR_COMMON_NOT_SUPPORTED.  If suspension of CTS is
 * supported but CTS flow control is not being used this function
 * will return successfully.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               zero on success else negative error code.
 */
typedef int32_t (*uDeviceSerialCtsSuspend_t)(struct uDeviceSerial_t *pDeviceSerial);

/** Resume CTS flow control; should be called after
 * #uDeviceSerialCtsSuspend_t to resume normal flow control operation.
 * This function must be supported if #uDeviceSerialCtsSuspend_t is
 * supported.  Where #uDeviceSerialCtsSuspend_t is not supported
 * this function may still be called but will have no effect.
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 */
typedef void (*uDeviceSerialCtsResume_t)(struct uDeviceSerial_t *pDeviceSerial);

/** If set to true then, should there be no room in the receive
 * buffer for data arriving from the far end, that data will be
 * discarded instead of causing a flow control signal to be sent
 * to the far end.  This is useful when the received data is frequent
 * and periodic in nature (e.g. GNSS information, where "stale"
 * data is of no interest) and sending flow control on, for instance,
 * a multiplexed bearer, might result in flow control being applied
 * to other, more important, virtual serial devices.
 * Where this is not supported #U_ERROR_COMMON_NOT_SUPPORTED will
 * be returned.
 *
 * This function is only present in #U_DEVICE_SERIAL_VERSION 1 and later
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @param onNotOff       use true to enable discard on overflow, else
 *                       false.
 * @return               zero on success else negative error code.
 */
typedef int32_t (*uDeviceSerialDiscardOnOverflow_t)(struct uDeviceSerial_t *pDeviceSerial,
                                                    bool onNotOff);

/** Read the state of #uDeviceSerialDiscardOnOverflow_t.
 *
 * This function is only present in #U_DEVICE_SERIAL_VERSION 1 and later
 *
 * @param pDeviceSerial  the serial device; cannot be NULL.
 * @return               true if discard on overflow is enabled,
 *                       else false.
 */
typedef bool (*uDeviceSerialIsDiscardOnOverflowEnabled_t)(struct uDeviceSerial_t *pDeviceSerial);

/* ----------------------------------------------------------------
 * TYPES: VECTOR TABLE
 * -------------------------------------------------------------- */

/** The vector table that constitutes a serial interface.
*/
typedef struct uDeviceSerial_t {
    uDeviceSerialOpen_t open;
    uDeviceSerialClose_t close;
    uDeviceSerialGetReceiveSize_t getReceiveSize;
    uDeviceSerialRead_t read;
    uDeviceSerialWrite_t write;
    uDeviceSerialEventCallbackSet_t eventCallbackSet;
    uDeviceSerialEventCallbackRemove_t eventCallbackRemove;
    uDeviceSerialEventCallbackFilterGet_t eventCallbackFilterGet;
    uDeviceSerialEventCallbackFilterSet_t eventCallbackFilterSet;
    uDeviceSerialEventSend_t eventSend;
    uDeviceSerialEventTrySend_t eventTrySend;
    uDeviceSerialEventIsCallback_t eventIsCallback;
    uDeviceSerialEventStackMinFree_t eventStackMinFree;
    uDeviceSerialIsRtsFlowControlEnabled_t isRtsFlowControlEnabled;
    uDeviceSerialIsCtsFlowControlEnabled_t isCtsFlowControlEnabled;
    uDeviceSerialCtsSuspend_t ctsSuspend;
    uDeviceSerialCtsResume_t ctsResume;
    uDeviceSerialDiscardOnOverflow_t discardOnOverflow;
    uDeviceSerialIsDiscardOnOverflowEnabled_t isDiscardOnOverflowEnabled;
} uDeviceSerial_t;

/** The initialisation callback; this should populate the table
 * with the interface functions and can, if required, also set-up
 * the context (which will otherwise be zeroed).
 *
 * To obtain the address of the context data in the implementations
 * of any of your serial functions, call pUInterfaceContext() with
 * pDeviceSerial, which is always the first parameter to each
 * function.
 *
 * @param pDeviceSerial a pointer to the serial device.
 * @return              zero or more on success, else negative
 *                      error code.
 */
typedef void (*uDeviceSerialInit_t)(uDeviceSerial_t *pDeviceSerial);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a serial device.
 *
 * @param pDeviceInit  a pointer to the initialisation function;
 *                     if NULL then uDeviceSerial_t will be filled
 *                     with NULLs.
 * @param contextSize  the size of the private context data that the
 *                     implementation of the serial device requires;
 *                     may be zero. To obtain the address of the context
 *                     data in the implementations of your serial
 *                     functions, call #pUInterfaceContext() with
 *                     the pointer this function returns.
 * @return             on success a pointer to the serial device, else
 *                     NULL.
 */
uDeviceSerial_t *pUDeviceSerialCreate(uDeviceSerialInit_t pDeviceInit,
                                      size_t contextSize);

/** Delete a serial device.
 *
 * @param pDeviceSerial  a pointer to the device that was returned
 *                       by pUDeviceSerialCreate().
 */
void uDeviceSerialDelete(uDeviceSerial_t *pDeviceSerial);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_DEVICE_SERIAL_H_

// End of file
