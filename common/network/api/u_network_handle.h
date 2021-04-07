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

#ifndef _U_NETWORK_HANDLE_H_
#define _U_NETWORK_HANDLE_H_

/* No #includes allowed here */

/** @file
 *@brief This header file defines the network handle ranges
 * for ubxlib between sho, cellular, Wifi and GNSS so that
 * the same network handle can be used across ubxlib without fear
 * of collision.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Base handle code: you may override this if you wish.
 */
#ifndef U_NETWORK_HANDLE_BASE
# define U_NETWORK_HANDLE_BASE 0
#endif

/** Determine if the network handle is valid.
 */
#define U_NETWORK_HANDLE_IS_VALID(handle) ((handle) >= (int32_t) U_NETWORK_HANDLE_BLE_MIN && \
                                           (handle) <= (int32_t) U_NETWORK_HANDLE_WIFI_MAX)

/** Determine if the network handle is a Bluetooth one.
 */
#define U_NETWORK_HANDLE_IS_BLE(handle) ((handle) >= (int32_t) U_NETWORK_HANDLE_BLE_MIN && \
                                         (handle) <= (int32_t) U_NETWORK_HANDLE_BLE_MAX)

/** Determine if the network handle is a cellular one.
 */
#define U_NETWORK_HANDLE_IS_CELL(handle) ((handle) >= (int32_t) U_NETWORK_HANDLE_CELL_MIN && \
                                          (handle) <= (int32_t) U_NETWORK_HANDLE_CELL_MAX)

/** Determine if the network handle is a Wifi one.
 */
#define U_NETWORK_HANDLE_IS_WIFI(handle) ((handle) >= (int32_t) U_NETWORK_HANDLE_WIFI_MIN && \
                                          (handle) <= (int32_t) U_NETWORK_HANDLE_WIFI_MAX)

/** Determine if the network handle is a GNSS one.
 */
#define U_NETWORK_HANDLE_IS_GNSS(handle) ((handle) >= (int32_t) U_NETWORK_HANDLE_GNSS_MIN && \
                                          (handle) <= (int32_t) U_NETWORK_HANDLE_GNSS_MAX)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Network handle ranges for BLE, cell, Wifi and GNSS.
 * IMPORTANT: though this is defined as an enumeration it
 * is a requirement that any handle returned to or accepted
 * from the user is cast to an anonymous int32_t type.
 */
typedef enum {
    U_NETWORK_HANDLE_COMMON_FORCE_INT32 = 0x7FFFFFFF, /* Force this enum to be 32 bit
                                                       * as it can be used as a size
                                                       * also. */
    U_NETWORK_HANDLE_BLE_MIN = U_NETWORK_HANDLE_BASE,
    U_NETWORK_HANDLE_BLE_MAX = U_NETWORK_HANDLE_BASE + 255,
    U_NETWORK_HANDLE_CELL_MIN = U_NETWORK_HANDLE_BASE + 256,
    U_NETWORK_HANDLE_CELL_MAX = U_NETWORK_HANDLE_BASE + 511,
    U_NETWORK_HANDLE_WIFI_MIN = U_NETWORK_HANDLE_BASE + 512,
    U_NETWORK_HANDLE_WIFI_MAX = U_NETWORK_HANDLE_BASE + 1024,
    U_NETWORK_HANDLE_GNSS_MIN = U_NETWORK_HANDLE_BASE + 1025,
    U_NETWORK_HANDLE_GNSS_MAX = U_NETWORK_HANDLE_BASE + 2048
} uNetworkHandle_t;

#endif // _U_NETWORK_HANDLE_H_

// End of file
