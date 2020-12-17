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

#ifndef _U_SHORT_RANGE_H_
#define _U_SHORT_RANGE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the ShortRange APIs. These APIs are not
 * intended to be called directly, they are called only via the ble/wifi
 * APIs. The ShortRange APIs are NOT generally thread-safe: the ble/wifi
 * APIs add thread safety by calling uShortRangeLock()/uShortRangeUnlock()
 * where appropriate.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES
/** The buffer length required in the AT client by the ShortRange driver.
 */
# define U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES U_AT_CLIENT_BUFFER_LENGTH_BYTES
#endif

#ifndef U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES
/** Corresponds to the large posible short range EDM packet
 */
# define U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES 4000
#endif

#ifndef U_SHORT_RANGE_UART_BAUD_RATE
/** The default baud rate to communicate with short range module.
 */
# define U_SHORT_RANGE_UART_BAUD_RATE 115200
#endif

#define U_SHORT_RANGE_EVENT_CONNECTED    0
#define U_SHORT_RANGE_EVENT_DISCONNECTED 1

#define U_SHORT_RANGE_CONNECTION_TYPE_BT   0
#define U_SHORT_RANGE_CONNECTION_TYPE_WIFI 0

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to short range.
 */
typedef enum {
    U_SHORT_RANGE_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< Force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_SHORT_RANGE_ERROR_AT = U_ERROR_SHORT_RANGE_MAX,      /**< -4096 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_NOT_CONFIGURED = U_ERROR_SHORT_RANGE_MAX - 1, /**< -4097 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_VALUE_OUT_OF_RANGE = U_ERROR_SHORT_RANGE_MAX - 2, /**< -4098 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_INVALID_MODE = U_ERROR_SHORT_RANGE_MAX - 3, /**< -4099 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_NOT_DETECTED = U_ERROR_SHORT_RANGE_MAX - 4, /**< -4100 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_WRONG_TYPE = U_ERROR_SHORT_RANGE_MAX - 5, /**< -4101 if U_ERROR_BASE is 0. */
} uShortRangeErrorCode_t;

typedef enum {
    U_SHORT_RANGE_SERVER_DISABLED = 0, /**< Disabled status. */
    U_SHORT_RANGE_SERVER_SPS = 6 /**< SPS server. */
} uShortRangeServerType_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the short range driver.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uShortRangeInit();

/** Shut-down the short range driver.  All short range instances
 * will be removed internally with calls to uShortRangeRemove().
 */
void uShortRangeDeinit();

/** Locks the short range mutex.
 * MUST be called before any of the below functions are!
 * Will wait for the mutex if already locked.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uShortRangeLock();

/** Unlocks the short range mutex.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uShortRangeUnlock();

/** Add a short range instance.
 *
 * @param moduleType       the short range module type.
 * @param atHandle         the handle of the AT client to use.  This must
 *                         already have been created by the caller with
 *                         a buffer of size U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES.
 *                         If a short range instance has already been added
 *                         for this atHandle an error will be returned.
 * @return                 on success the handle of the short range instance,
 *                         else negative error code.
 */
int32_t uShortRangeAdd(uShortRangeModuleType_t moduleType,
                       uAtClientHandle_t atHandle);

/** Remove a short range instance.  It is up to the caller to ensure
 * that the short range module for the given instance has been disconnected
 * and/or powered down etc.; all this function does is remove the logical
 * instance.
 *
 * @param shortRangeHandle  the handle of the short range instance to remove.
 */
void uShortRangeRemove(int32_t shortRangeHandle);

/** Detect the module connected to the handle. Will attempt to change the mode on
 * the module to communicate with it. No change to UART configuration is done,
 * so even if this fails, as last attempt to recover, it could work to  re-init
 * the UART on a different baud rate. This sould recover that module if another
 * rate than the default one has been used.
 *
 * @param shortRangeHandle   the handle of the short range instance.
 * @return                   Module on success, U_SHORT_RANGE_MODULE_TYPE_INVALID
 *                           on failure.
 */
uShortRangeModuleType_t uShortRangeDetectModule(int32_t shortRangeHandle);

/** Sends "AT" to the short range module on which it should repond with "OK"
 * but takes no action. This checks that the module is ready to respond to commands.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeAttention(int32_t shortRangeHandle);

/** Change to command mode by sending a escape sequence, can be used at
 * startup if uShortRangeAttention is unresponsive.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @param pAtHandle         the place to put the new atHandle, cannot be NULL.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeCommandMode(int32_t shortRangeHandle, uAtClientHandle_t *pAtHandle);


/** Change to data mode, no commands will be accepted in this mode and
 * the caller can send, and must handle the incoming, data directly on
 * the stream.
 *
 * @note: A delay of 50 ms is required before start of data transmission
 * @note: The original atHandle is no longer valid after this is called, at client is
 *        re-added when calling uShortRangeCommandMode.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeDataMode(int32_t shortRangeHandle);

/** Set a callback for connection status.
  *
 * @param shortRangeHandle   the handle of the short range instance.
 * @param type               type of connection, bt or wifi.
 * @param pCallback          callback function.
 * @param pCallbackParameter parameter included with the callback.
 * @return                   zero on success or negative error code
 *                           on failure.
 */
int32_t uShortRangeConnectionStatusCallback(int32_t shortRangeHandle,
                                            int32_t type,
                                            void (*pCallback) (int32_t, int32_t, void *),
                                            void *pCallbackParameter);

/** Get the handle of the AT client used by the given
 * short range instance.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @param pAtHandle         a place to put the AT client handle.
 * @return                  zero on success else negative error code.
 */
int32_t uShortRangeAtClientHandleGet(int32_t shortRangeHandle,
                                     uAtClientHandle_t *pAtHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_H_

// End of file
