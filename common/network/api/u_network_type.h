/*
 * Copyright 2019-2024 u-blox
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

#ifndef _U_NETWORK_TYPE_H_
#define _U_NETWORK_TYPE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup network Network
 *  @{
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

/** A version number for the network configuration structure. In
 * general you should allow the compiler to initialise any variable
 * of this type to zero and ignore it.  It is only set to a value
 * other than zero when variables in a new and extended version of
 * the structure it is a part of are being used, the version number
 * being employed by this code to detect that and, more importantly,
 * to adopt default values for any new elements when the version
 * number is STILL ZERO, maintaining backwards compatibility with
 * existing application code.  The structure this is a part of will
 * include instructions as to when a non-zero version number should
 * be set.
 */
typedef int32_t uNetworkCfgVersion_t;

/** Network types.
 *
 * Note: order is important, this is used to index into an array.
 */
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_MAX_NUM)
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_NONE)
//  Suppress not used within defaulted switch
typedef enum {
    U_NETWORK_TYPE_NONE,
    U_NETWORK_TYPE_BLE,
    U_NETWORK_TYPE_CELL,
    U_NETWORK_TYPE_WIFI,
    U_NETWORK_TYPE_GNSS,
    U_NETWORK_TYPE_MAX_NUM
} uNetworkType_t;

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_NETWORK_TYPE_H_

// End of file
