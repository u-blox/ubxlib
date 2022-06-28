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

#ifndef _U_BLE_H_
#define _U_BLE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _BLE _Bluetooth Low Energy
 *  @{
 */

/** @file
 * @brief This header file defines the general BLE APIs,
 * basically initialise and deinitialise.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_BLE_UART_BUFFER_LENGTH_BYTES
/** The recommended UART buffer length for the short range driver,
 * large enough for large AT or EDM packet using BLE.
 * Note: If module is also using WiFi, it is recommended to use that size.
 */
# define U_BLE_UART_BUFFER_LENGTH_BYTES 600
#endif

#ifndef U_BLE_AT_BUFFER_LENGTH_BYTES
/** The AT client buffer length required in the AT client by the
 * ble driver.
 */
# define U_BLE_AT_BUFFER_LENGTH_BYTES U_AT_CLIENT_BUFFER_LENGTH_BYTES
#endif

#ifndef U_BLE_UART_BAUD_RATE
/** The default baud rate to communicate with a short range module.
 */
# define U_BLE_UART_BAUD_RATE 115200
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to ble.
 */
typedef enum {
    U_BLE_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_BLE_ERROR_AT = U_ERROR_BLE_MAX,      /**< -512 if #U_ERROR_BASE is 0. */
    U_BLE_ERROR_NOT_CONFIGURED = U_ERROR_BLE_MAX - 1, /**< -511 if #U_ERROR_BASE is 0. */
    U_BLE_ERROR_NOT_FOUND = U_ERROR_BLE_MAX - 2,  /**< -510 if #U_ERROR_BASE is 0. */
    U_BLE_ERROR_INVALID_MODE = U_ERROR_BLE_MAX - 3,  /**< -509 if #U_ERROR_BASE is 0. */
    U_BLE_ERROR_TEMPORARY_FAILURE = U_ERROR_BLE_MAX - 4  /**< -508 if #U_ERROR_BASE is 0. */
} uBleErrorCode_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the BLE.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uBleInit(void);

/** Shut-down BLE.  All instances will be removed internally
 * with calls to uBleRemove().
 */
void uBleDeinit(void);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_BLE_H_

// End of file
