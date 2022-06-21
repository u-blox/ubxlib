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

#ifndef _U_SHORT_RANGE_TEST_SELECTOR_H_
#define _U_SHORT_RANGE_TEST_SELECTOR_H_

#include "u_short_range_module_type.h"

/** @file
 * @brief This header defines macros that is used for deciding
 *        what radios are available for U_CFG_TEST_SHORT_RANGE_MODULE_TYPE.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/**
 * This is some macro hacking that is used for generating a bitmask of the
 * supported radios for U_CFG_TEST_SHORT_RANGE_MODULE_TYPE.
 * The mask can be read from U_CFG_TEST_SHORT_RANGE_MODULE_RADIO_MASK
 * where bits are defined like this:
 *  bit 0: Has BLE support
 *  bit 1: Has BT classic support
 *  bit 2: Has Wifi support
 */
#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
# define U_YES 1
# define U_NO  0
# define U_SHORT_RANGE_MODULE(_TYPE_NAME, _GMM_NAME, _BLE, _BT_CLASSIC, _WIFI) \
    ((U_CFG_TEST_SHORT_RANGE_MODULE_TYPE == U_SHORT_RANGE_MODULE_TYPE_##_TYPE_NAME) ? \
    ((_WIFI ? 4 : 0) | (_BT_CLASSIC ? 2 : 0) | (_BLE ? 1 : 0)) : 0) |

# define U_CFG_TEST_SHORT_RANGE_MODULE_RADIO_MASK \
    (U_SHORT_RANGE_MODULE_LIST 0)
#else
# define U_CFG_TEST_SHORT_RANGE_MODULE_RADIO_MASK 0
#endif


/**
 * The defines below are used for deciding what tests to compile
 *
 * U_SHORT_RANGE_TEST_BLE()
 * Returns 1 if BLE tests should be compiled otherwise 0.
 * If U_CFG_BLE_MODULE_INTERNAL is defined U_SHORT_RANGE_TEST_BLE()
 * will always return 1.
 *
 * U_SHORT_RANGE_TEST_WIFI()
 * Returns 1 if Wifi tests should be compiled otherwise 0
 */
#ifdef U_CFG_BLE_MODULE_INTERNAL
# define U_SHORT_RANGE_TEST_BLE() 1
#else
# define U_SHORT_RANGE_TEST_BLE() \
    ((U_CFG_TEST_SHORT_RANGE_MODULE_RADIO_MASK & 1) > 0)
#endif

#define U_SHORT_RANGE_TEST_WIFI() \
    ((U_CFG_TEST_SHORT_RANGE_MODULE_RADIO_MASK & 4) > 0)


/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_TEST_SELECTOR_H_

// End of file
