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

#ifndef _U_BLE_MODULE_TYPE_H_
#define _U_BLE_MODULE_TYPE_H_

#include "u_short_range_module_type.h"

/** \addtogroup _BLE
 *  @{
 */

/** @file
 * @brief This header file defines the module types for BLE.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// Macro magic for uBleModuleType_t
#define U_NO(_TYPE_NAME)
#define U_YES(_TYPE_NAME) \
    U_BLE_MODULE_TYPE_##_TYPE_NAME = U_SHORT_RANGE_MODULE_TYPE_##_TYPE_NAME,
#define U_SHORT_RANGE_MODULE(_TYPE_NAME, _GMM_NAME, _BLE, _BT_CLASSIC, _WIFI) \
    _BLE(_TYPE_NAME)

/** The possible types of BLE module.
 */
typedef enum {
// X macro is used to generate this enum from #U_SHORT_RANGE_MODULE_LIST
// for all entries with field "BLE" set to U_YES.
// It will prefix the "Module" field from #U_SHORT_RANGE_MODULE_LIST with
// U_BLE_MODULE_TYPE_, thus creating the enum values below:
//    U_BLE_MODULE_TYPE_NINA_B1  < Modules NINA-B1. BLE only
//    U_BLE_MODULE_TYPE_ANNA_B1  < Modules ANNA-B1. BLE only
//    U_BLE_MODULE_TYPE_NINA_B3  < Modules NINA-B3. BLE only
//    U_BLE_MODULE_TYPE_NINA_B4  < Modules NINA-B4. BLE only
//    U_BLE_MODULE_TYPE_NINA_B2  < Modules NINA-B2. BLE and Classic
//    U_BLE_MODULE_TYPE_NINA_W15 < Modules NINA-W15. Wifi, BLE and Classic
//    U_BLE_MODULE_TYPE_ODIN_W2  < Modules NINA-B1. Wifi, BLE and Classic
    U_SHORT_RANGE_MODULE_LIST

    U_BLE_MODULE_TYPE_INVALID = U_SHORT_RANGE_MODULE_TYPE_INVALID,   /**< invalid. */
    U_BLE_MODULE_TYPE_INTERNAL = U_SHORT_RANGE_MODULE_TYPE_INTERNAL, /**< internal module. */
    U_BLE_MODULE_TYPE_UNSUPPORTED = U_SHORT_RANGE_MODULE_TYPE_UNSUPPORTED /**< valid module, but not supporting BLE. */
} uBleModuleType_t;
#undef U_NO
#undef U_YES
#undef U_SHORT_RANGE_MODULE

/** @}*/

#endif // _U_BLE_MODULE_TYPE_H_

// End of file
