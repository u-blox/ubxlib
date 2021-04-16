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

#ifndef _U_SHORT_RANGE_MODULE_TYPE_H_
#define _U_SHORT_RANGE_MODULE_TYPE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the module types for ShortRange.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible types of short range module.
 * Note: if you add a new module type here, check the
 * U_SHORT_RANGE_PRIVATE_MODULE_xxx macros in u_short_range_private.h
 * to see if they need updating (amongst other things).
 * Note: order is important as these are used to index
 * into a statically defined array in u_short_range_cfg.c.
 */
//lint -estring(788, uShortRangeModuleType_t::U_SHORT_RANGE_MODULE_TYPE_MAX_NUM) Suppress not used within defaulted switch
typedef enum {
    U_SHORT_RANGE_MODULE_TYPE_NINA_B1 = 0, /**< Modules NINA-B1. BLE only*/
    U_SHORT_RANGE_MODULE_TYPE_ANNA_B1, /**< Modules ANNA-B1. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B3, /**< Modules NINA-B3. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B4, /**< Modules NINA-B4. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B2, /**< Modules NINA-B2. BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_NINA_W13, /**< Modules NINA-W13. Wifi */
    U_SHORT_RANGE_MODULE_TYPE_NINA_W15, /**< Modules NINA-W15. Wifi, BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_ODIN_W2, /**< Modules NINA-B1. Wifi, BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_MAX_NUM,
    U_SHORT_RANGE_MODULE_TYPE_INVALID = U_SHORT_RANGE_MODULE_TYPE_MAX_NUM,
    U_SHORT_RANGE_MODULE_TYPE_INTERNAL,
} uShortRangeModuleType_t;

#endif // _U_SHORT_RANGE_MODULE_TYPE_H_

// End of file
