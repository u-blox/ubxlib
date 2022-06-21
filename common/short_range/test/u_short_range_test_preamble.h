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

#ifndef _U_SHORT_RANGE_TEST_PREAMBLE_H_
#define _U_SHORT_RANGE_TEST_PREAMBLE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions that should be called
 * before running any other short-range tests or examples.
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

/** The standard preamble that must be run before any short-range
 * tests/examples.
 *
 * @param moduleType  the module type.
 * @return            zero on success else negative error code.
 */
int32_t uShortRangeTestPreamble(uShortRangeModuleType_t moduleType);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_TEST_PREAMBLE_H_

// End of file
