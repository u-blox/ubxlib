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

#ifndef _U_SHORT_RANGE_MODULE_TYPE_H_
#define _U_SHORT_RANGE_MODULE_TYPE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _short-range
 *  @{
 */

/** @file
 * @brief This header file defines the module types for ShortRange.
 * These types are not intended to be used directly, they are used only
 * via the ble/wifi APIs.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The possible types of short range module.
 *  Note: If you add a new module type here you must also:
 *  1. Add an entry to gUShortRangePrivateModuleList in
 *     u_short_range_private.c.
 *  2. Add an entry to #U_SHORT_RANGE_MODULE_LIST() in this file.
 */
#define U_SHORT_RANGE_MODULE_TYPE_INTERNAL 0
#define U_SHORT_RANGE_MODULE_TYPE_ANNA_B1  1
#define U_SHORT_RANGE_MODULE_TYPE_NINA_B1  2
#define U_SHORT_RANGE_MODULE_TYPE_NINA_B2  3
#define U_SHORT_RANGE_MODULE_TYPE_NINA_B3  4
#define U_SHORT_RANGE_MODULE_TYPE_NINA_B4  5
#define U_SHORT_RANGE_MODULE_TYPE_NINA_W13 6
#define U_SHORT_RANGE_MODULE_TYPE_NINA_W15 7
#define U_SHORT_RANGE_MODULE_TYPE_ODIN_W2  8

#define U_SHORT_RANGE_MODULE_TYPE_INVALID -1
#define U_SHORT_RANGE_MODULE_TYPE_UNSUPPORTED -2

/** This is a X macro table of the radio features for all supported
 *  short range modules. This is used for automatically generating some
 *  boilerplate code and for enabling test \c \#ifdef by radio features
 *  (see u_short_range_test_selector.h).
 *  The table is linked to the U_SHORT_RANGE_MODULE_TYPE_xx defines
 *  above through the "Module" field.
 */
#define U_SHORT_RANGE_MODULE_LIST \
                       /*  Module   | +GMM Name  | BLE   | BT Classic | WiFi  */ \
    U_SHORT_RANGE_MODULE(  ANNA_B1  , "ANNA-B1"  , U_YES ,   U_NO     , U_NO   ) \
    U_SHORT_RANGE_MODULE(  NINA_B1  , "NINA-B1"  , U_YES ,   U_NO     , U_NO   ) \
    U_SHORT_RANGE_MODULE(  NINA_B2  , "NINA-B2"  , U_YES ,   U_YES    , U_NO   ) \
    U_SHORT_RANGE_MODULE(  NINA_B3  , "NINA-B3"  , U_YES ,   U_NO     , U_NO   ) \
    U_SHORT_RANGE_MODULE(  NINA_B4  , "NINA-B4"  , U_YES ,   U_NO     , U_NO   ) \
    U_SHORT_RANGE_MODULE(  NINA_W13 , "NINA-W13" , U_NO  ,   U_NO     , U_YES  ) \
    U_SHORT_RANGE_MODULE(  NINA_W15 , "NINA-W15" , U_YES ,   U_YES    , U_YES  ) \
    U_SHORT_RANGE_MODULE(  ODIN_W2  , "ODIN-W2"  , U_YES ,   U_YES    , U_YES  )

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef int32_t uShortRangeModuleType_t;

/** @}*/

#endif // _U_SHORT_RANGE_MODULE_TYPE_H_

// End of file
