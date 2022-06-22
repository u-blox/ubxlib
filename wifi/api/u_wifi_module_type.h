/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicawifi law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_WIFI_MODULE_TYPE_H_
#define _U_WIFI_MODULE_TYPE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_short_range_module_type.h"

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the module types for WiFi.
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

// Macro magic for uWifiModuleType_t
#define U_NO(_TYPE_NAME)
#define U_YES(_TYPE_NAME) \
    U_WIFI_MODULE_TYPE_##_TYPE_NAME = U_SHORT_RANGE_MODULE_TYPE_##_TYPE_NAME,
#define U_SHORT_RANGE_MODULE(_TYPE_NAME, _GMM_NAME, _BLE, _BT_CLASSIC, _WIFI) \
    _WIFI(_TYPE_NAME)

typedef enum {
// X macro is used to generate this enum from #U_SHORT_RANGE_MODULE_LIST
// for all entries with field "WiFi" set to U_YES.
// It will prefix the "Module" field from #U_SHORT_RANGE_MODULE_LIST with
// U_WIFI_MODULE_TYPE_, thus creating the enum values below:
//    U_WIFI_MODULE_TYPE_NINA_W13 < Modules NINA-W13. Wifi only
//    U_WIFI_MODULE_TYPE_NINA_W15 < Modules NINA-W15. Wifi, BLE and Classic
//    U_WIFI_MODULE_TYPE_ODIN_W2  < Modules Odin W2. Wifi, BLE and Classic
    U_SHORT_RANGE_MODULE_LIST

    U_WIFI_MODULE_TYPE_INVALID = U_SHORT_RANGE_MODULE_TYPE_INVALID,   /**< Invalid */
    U_WIFI_MODULE_TYPE_INTERNAL = U_SHORT_RANGE_MODULE_TYPE_INTERNAL, /**< Internal module */
    U_WIFI_MODULE_TYPE_UNSUPPORTED = U_SHORT_RANGE_MODULE_TYPE_UNSUPPORTED /**< Unsupported module */

} uWifiModuleType_t;

#undef U_NO
#undef U_YES
#undef U_SHORT_RANGE_MODULE

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_MODULE_TYPE_H_

// End of file
