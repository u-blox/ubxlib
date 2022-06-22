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

#ifndef _U_NETWORK_CONFIG_BLE_H_
#define _U_NETWORK_CONFIG_BLE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup network
 *  @{
 */

/** @file
 * @brief This header file defines the configuration structure for the
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

/** The network configuration for BLE.
 */
typedef struct {
    uNetworkCfgVersion_t version; /**< version of this network
                                       configuration; allow your
                                       compiler to initialise this
                                       to zero unless otherwise
                                       specified below. */
    uNetworkType_t type; /**< for error checking purposes. */
    int32_t role;        /**< peripheral, central or, peripheral and central,
                              see uShortRangeBleRole_t in u_short_range.h. */
    bool spsServer;      /**< true if sps server is to be enabled. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t magic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uNetworkCfgBle_t;

#endif // _U_NETWORK_CONFIG_BLE_H_

/** @}*/

// End of file
