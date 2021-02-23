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

#ifndef _U_CELL_H_
#define _U_CELL_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the general cellular-wide APIs,
 * basically initialise and deinitialise.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_UART_BUFFER_LENGTH_BYTES
/** The recommended UART buffer length for the cellular driver,
 * large enough to run AT sockets using the IP stack on the
 * cellular module (where the maximum packet length is 1024 bytes)
 * without flow control.  See U_CELL_AT_BUFFER_LENGTH_BYTES for
 * where the overhead numbers come from (for the C2C case).
 */
# define U_CELL_UART_BUFFER_LENGTH_BYTES (1024 + 16 + 16 + 16 + 16 + 16 + 6)
#endif

#ifndef U_CELL_AT_BUFFER_LENGTH_BYTES
/** The AT client buffer length required in the AT client by the
 * cellular driver.
 *
 * When chip to chip security is employed the size of each
 * secure frame adds a considerable overhead.  Maximum
 * chunk size is 1024 + 16 bytes (for an AT command,
 * see u_cell_sec_c2c.h) plus the maximum padding length of 16,
 * bytes plus the length of a truncated MAC (16 bytes) plus the
 * length of the initial value (16 bytes) plus the length of
 * the HMAC SHA tag for the V2 scheme (16 bytes) plus
 * start/length/CRC/stop field totalling 6 bytes. Then it is
 * possible for there to be part of one of these in the buffer
 * being processed by the AT client when another is meant to
 * turn up so allow for at least two.
 */
# define U_CELL_AT_BUFFER_LENGTH_BYTES (U_AT_CLIENT_BUFFER_OVERHEAD_BYTES + \
                                        ((1024 + 16 + 16 + 16 + 16 + 16 + 6) * 2))
#endif

#ifndef U_CELL_UART_BAUD_RATE
/** The default baud rate to communicate with a cellular module.
 */
# define U_CELL_UART_BAUD_RATE 115200
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to cellular.
 */
typedef enum {
    U_CELL_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< Force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_CELL_ERROR_AT = U_ERROR_CELL_MAX,      /**< -256 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_NOT_CONFIGURED = U_ERROR_CELL_MAX - 1, /**< -257 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_PIN_ENTRY_NOT_SUPPORTED = U_ERROR_CELL_MAX - 2, /**< -258 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_NOT_REGISTERED = U_ERROR_CELL_MAX - 3, /**< -259 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_ATTACH_FAILURE = U_ERROR_CELL_MAX - 4, /**< -260 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_CONTEXT_ACTIVATION_FAILURE = U_ERROR_CELL_MAX - 5, /**< -261 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_CONNECTED = U_ERROR_CELL_MAX - 6,  /**< This is an ERROR code used, for instance, to
                                                         indicate that a disconnect attempt has failed.
                                                         -262 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_NOT_CONNECTED = U_ERROR_CELL_MAX - 7, /**< -263 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_NOT_FOUND = U_ERROR_CELL_MAX - 8,  /**< -264 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_VALUE_OUT_OF_RANGE = U_ERROR_CELL_MAX - 9,  /**< -265 if U_ERROR_BASE is 0. */
    U_CELL_ERROR_TEMPORARY_FAILURE = U_ERROR_CELL_MAX - 10  /**< -266 if U_ERROR_BASE is 0. */
} uCellErrorCode_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the cellular driver.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uCellInit();

/** Shut-down the cellular driver.  All cellular instances
 * will be removed internally with calls to uCellRemove().
 */
void uCellDeinit();

/** Add a cellular instance.
 *
 * @param moduleType       the cellular module type.
 * @param atHandle         the handle of the AT client to use.  This must
 *                         already have been created by the caller with
 *                         a buffer of size U_CELL_AT_BUFFER_LENGTH_BYTES.
 *                         If a cellular instance has already been added
 *                         for this atHandle an error will be returned.
 * @param pinEnablePower   the pin that switches on the power
 *                         supply to the cellular module. The
 *                         Sense of the pin should be such that
 *                         low means off and high means on.
 *                         Set to -1 if there is no such pin.
 * @param pinPwrOn         the pin that signals power-on to the
 *                         cellular module, i.e. the pin
 *                         that is connected to the module's PWR_ON pin.
 *                         Set to -1 if there is no such pin.
 * @param pinVInt          the pin that can be monitored to detect
 *                         that the cellular module is powered up.
 *                         This pin should be connected to the
 *                         VInt pin of the module and is used to
 *                         make sure that the modem is truly off before
 *                         power to it is disabled.  Set to -1 if
 *                         there is no such pin.
 * @param leavePowerAlone  set this to true if initialisation should
 *                         not modify the state of pinEnablePower or
 *                         pinPwrOn, else it will ensure that pinEnablePower
 *                         is low to disable power to the module and pinPwrOn
 *                         is high so that it can be pulled low to logically
 *                         power the module on.
 * @return                 on success the handle of the cellular instance,
 *                         else negative error code.
 */
int32_t uCellAdd(uCellModuleType_t moduleType,
                 uAtClientHandle_t atHandle,
                 int32_t pinEnablePower, int32_t pinPwrOn,
                 int32_t pinVInt, bool leavePowerAlone);

/** Remove a cellular instance.  It is up to the caller to ensure
 * that the cellular module for the given instance has been disconnected
 * and/or powered down etc.; all this function does is remove the logical
 * instance.
 *
 * @param cellHandle  the handle of the cellular instance to remove.
 */
void uCellRemove(int32_t cellHandle);

/** Get the handle of the AT client used by the given
 * cellular instance.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pAtHandle   a place to put the AT client handle.
 * @return            zero on success else negative error code.
 */
int32_t uCellAtClientHandleGet(int32_t cellHandle,
                               uAtClientHandle_t *pAtHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_H_

// End of file
