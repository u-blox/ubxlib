/*
 * Copyright 2020 u-blox Cambourne Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_NETWORK_CONFIG_BLE_H_
#define _U_NETWORK_CONFIG_BLE_H_

/* No #includes allowed here */

/* This header file defines the configuration structure for the
 * network API for BLE.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Note: try, wherever possible, to use only basic types in this
 * structure (i.e. int32_t, const char, bool, etc.) since otherwise
 * your BLE headers will have to be brought in to all files that
 * need this config type, so into all network examples, etc.
 * irrespective of whether BLE is used there.
 */

/** The network configuration for BLE.  Note that the pin
 * numbers are those of the MCU: if you are using an MCU inside
 * a u-blox module the IO pin numbering for the module is likely
 * different to that from the MCU: check the data sheet for the
 * module to determine the mapping.
 */
typedef struct {
    uNetworkType_t type; /**< All uNetworkConfigurationXxx structures
                              must begin with this for error checking
                              purposes. */
    int32_t module; /**< The module type that is connected,
                         see uShortRangeModuleType_t in u_short_range.h. */
    int32_t uart; /**< The UART HW block to use. */
    int32_t pinTxd; /** The output pin that sends UART data to
                        the cellular module. */
    int32_t pinRxd; /** The input pin that receives UART data from
                        the cellular module. */
    int32_t pinCts; /**< The input pin that the cellular module
                         will use to indicate that data can be sent
                         to it; use -1 if there is no such connection. */
    int32_t pinRts; /**< The output pin output pin that tells the
                         cellular module that it can send more UART
                         data; use -1 if there is no such connection. */
    int32_t role; /**< Peripheral, central or, peripheral and central,
                       see uShortRangeBleRole_t in u_short_range.h. */
    bool spsServer; /**< True if sps server is to be enabled. */
} uNetworkConfigurationBle_t;

#endif // _U_NETWORK_CONFIG_BLE_H_

// End of file
