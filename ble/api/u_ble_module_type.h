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

#ifndef _U_BLE_MODULE_TYPE_H_
#define _U_BLE_MODULE_TYPE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the module types for ble.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible types of BLE module.
 */
typedef enum {
    U_BLE_MODULE_TYPE_NINA_B1 = U_SHORT_RANGE_MODULE_TYPE_NINA_B1,   /**< Modules NINA-B1. BLE only*/
    U_BLE_MODULE_TYPE_ANNA_B1 = U_SHORT_RANGE_MODULE_TYPE_ANNA_B1,   /**< Modules ANNA-B1. BLE only */
    U_BLE_MODULE_TYPE_NINA_B3 = U_SHORT_RANGE_MODULE_TYPE_NINA_B3,   /**< Modules NINA-B3. BLE only */
    U_BLE_MODULE_TYPE_NINA_B4 = U_SHORT_RANGE_MODULE_TYPE_NINA_B4,   /**< Modules NINA-B4. BLE only */
    U_BLE_MODULE_TYPE_NINA_B2 = U_SHORT_RANGE_MODULE_TYPE_NINA_B2,   /**< Modules NINA-B2. BLE and Classic */
    U_BLE_MODULE_TYPE_NINA_W15 = U_SHORT_RANGE_MODULE_TYPE_NINA_W15, /**< Modules NINA-W15. Wifi, BLE and Classic */
    U_BLE_MODULE_TYPE_ODIN_W2 = U_SHORT_RANGE_MODULE_TYPE_ODIN_W2,   /**< Modules NINA-B1. Wifi, BLE and Classic */
    U_BLE_MODULE_TYPE_INVALID = U_SHORT_RANGE_MODULE_TYPE_INVALID,   /**< Invalid */
    U_BLE_MODULE_TYPE_INTERNAL = U_SHORT_RANGE_MODULE_TYPE_INTERNAL, /**< Internal module */
    U_BLE_MODULE_TYPE_UNSUPPORTED                                    /**< Valid module, but not supporting BLE */
} uBleModuleType_t;

#endif // _U_BLE_MODULE_TYPE_H_

// End of file
