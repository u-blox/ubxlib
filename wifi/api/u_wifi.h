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

#define U_WIFI_BSSID_SIZE 6        /**< Binary BSSID size*/
#define U_WIFI_SSID_SIZE (32 + 1)  /**< Null terminated SSID string size*/


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

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_H_

// End of file
