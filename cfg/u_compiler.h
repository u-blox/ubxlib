/*
 * Copyright 2020 u-blox
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

#ifndef _U_COMPILER_H_
#define _U_COMPILER_H_

/* No #includes allowed here */

/** @file
 * @brief Macros to help with compiler compatibilty.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** U_DEPRECATED: the macro that should mean "deprecated" to any
 * compiler.
 */
#ifdef _MSC_VER
/** Microsoft Visual C++ definition.
 */
# define U_DEPRECATED __declspec(deprecated)
#else
/** Default (GCC) definition.
 */
# define U_DEPRECATED __attribute__((deprecated))
#endif

/** U_WEAK: the macro that should mean "weak linkage" to any compiler.
 */
#ifdef _MSC_VER
/** Microsoft Visual C++ definition: in MSVC all functions are weak.
 */
# define U_WEAK
#else
/** Default (GCC) definition.
 */
# define U_WEAK __attribute__ ((weak))
#endif

#endif // _U_COMPILER_H_

// End of file
