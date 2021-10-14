/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicawifi law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_WIFI_H_
#define _U_WIFI_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the general wifi APIs,
 * basically initialise and deinitialise.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_WIFI_UART_BUFFER_LENGTH_BYTES
/** The recommended UART buffer length for the short range driver,
 * large enough for large AT or EDM packet using WiFi.
 */
# define U_WIFI_UART_BUFFER_LENGTH_BYTES 600
#endif

#ifndef U_WIFI_AT_BUFFER_LENGTH_BYTES
/** The AT client buffer length required in the AT client by the
 * wifi driver.
 */
# define U_WIFI_AT_BUFFER_LENGTH_BYTES U_AT_CLIENT_BUFFER_LENGTH_BYTES
#endif

#ifndef U_WIFI_UART_BAUD_RATE
/** The default baud rate to communicate with a short range module.
 */
# define U_WIFI_UART_BAUD_RATE 115200
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to wifi.
 */
typedef enum {
    U_WIFI_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< Force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_WIFI_ERROR_AT = U_ERROR_WIFI_MAX,      /**< -512 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_NOT_CONFIGURED = U_ERROR_WIFI_MAX - 1, /**< -511 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_NOT_FOUND = U_ERROR_WIFI_MAX - 2,  /**< -510 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_INVALID_MODE = U_ERROR_WIFI_MAX - 3,  /**< -509 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_TEMPORARY_FAILURE = U_ERROR_WIFI_MAX - 4,  /**< -508 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_CONNECTED = U_ERROR_WIFI_MAX - 5,  /**< -507 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID = U_ERROR_WIFI_MAX - 6,  /**< -506 if U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_DISCONNECTED = U_ERROR_WIFI_MAX - 7  /**< -505 if U_ERROR_BASE is 0. */
} uWifiErrorCode_t;



/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise wifi.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uWifiInit();

/** Shut-down wifi.  All instances will be removed internally
 * with calls to uWifiRemove().
 */
void uWifiDeinit();

/** Add a wifi instance.
 *
 * @param moduleType       the short range module type.
 * @param atHandle         the handle of the AT client to use.  This must
 *                         already have been created by the caller with
 *                         a buffer of size U_BLE_AT_BUFFER_LENGTH_BYTES.
 *                         If a wifi instance has already been added
 *                         for this atHandle an error will be returned.
 * @return                 on success the handle of the wifi instance,
 *                         else negative error code.
 */
int32_t uWifiAdd(uWifiModuleType_t moduleType,
                 uAtClientHandle_t atHandle);

/** Remove a wifi instance.  It is up to the caller to ensure
 * that the short range module for the given instance has been disconnected
 * and/or powered down etc.; all this function does is remove the logical
 * instance.
 *
 * @param wifiHandle  the handle of the wifi instance to remove.
 */
void uWifiRemove(int32_t wifiHandle);

/** Detect the module connected to the handle. Will attempt to change the mode on
 * the module to communicate with it. No change to UART configuration is done,
 * so even if this fails with U_WIFI_MODULE_TYPE_INVALID, as last attempt to recover,
 * it could work to re-init the UART on a different baud rate. This should recover
 * that module if another rate than the default one has been used.
 * If the response is U_WIFI_MODULE_TYPE_UNSUPPORTED, the module repondes as expected but
 * does not support wifi.
 *
 * @param wifiHandle   the handle of the wifi instance.
 * @return             Module on success, U_WIFI_MODULE_TYPE_INVALID or U_WIFI_MODULE_TYPE_UNSUPPORTED
 *                     on failure.
 */
uWifiModuleType_t uWifiDetectModule(int32_t wifiHandle);

/** Get the handle of the AT client used by the given
 * wifi instance.
 *
 * @param wifiHandle      the handle of the wifi instance.
 * @param[out] pAtHandle  a place to put the AT client handle.
 * @return                zero on success else negative error code.
 */
int32_t uWifiAtClientHandleGet(int32_t wifiHandle,
                               uAtClientHandle_t *pAtHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_H_

// End of file
