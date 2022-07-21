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

#ifndef _U_BLE_PRIVATE_H_
#define _U_BLE_PRIVATE_H_

#include "u_port_gatt.h"

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to the cellular API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialize data part of BLE
 */
void uBleSpsPrivateInit(void);

/** De-Initialize data part of BLE
 */
void uBleSpsPrivateDeinit(void);

/** Translate MAC address in byte array to string
 *
 * @param[in] pAddrIn   pointer to byte array
 * @param addrType      Public, Random or Unknown
 * @param msbLast       Last byte in array should be leftmost byte in string
 * @param[out] pAddrOut Output string
 */
void addrArrayToString(const uint8_t *pAddrIn, uPortBtLeAddressType_t addrType, bool msbLast,
                       char *pAddrOut);

#ifdef __cplusplus
}
#endif

#endif // _U_BLE_PRIVATE_H_

// End of file
