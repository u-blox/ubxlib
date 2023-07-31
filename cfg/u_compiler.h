/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_COMPILER_H_
#define _U_COMPILER_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup cfg Compile-time configuration
 *  @{
 */

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

/** U_INLINE: the macro that should make a compiler ALWAYS inline a
 * function, no matter what optimisation level it is set to.
 */
#ifdef _MSC_VER
/** Microsoft Visual C++ definition: no need to inline, lots of
 * oomph on Windows.
 */
# define U_INLINE
#else
/** Default (GCC) definition.
 */
# define U_INLINE __attribute__ ((always_inline)) inline
#endif


/** U_PACKED_STRUCT: macro for creating packed structs
 */
#ifdef _MSC_VER
/** Microsoft Visual C++ definition.
 */
# define U_PACKED_STRUCT(NAME) __pragma( pack(push, 1) ) struct NAME __pragma( pack(pop))
#else
/** Default (GCC) definition.
 */
# define U_PACKED_STRUCT(NAME) struct __attribute__((packed)) NAME
#endif

/** U_NO_RETURN: attribute used for telling compiler that
 *  a function never returns. Typically functions such as assert handlers.
 */

#ifdef _MSC_VER
/** Microsoft Visual C++ definition.
 */
# define U_NO_RETURN
#else
/** Default (GCC) definition.
 */
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  include <stdnoreturn.h>
#  define U_NO_RETURN noreturn
# else
#  define U_NO_RETURN __attribute__((noreturn))
# endif
#endif

/** Pointer-int32_t conversion. Can be used to to store
 * int32_t values in a pointer variable. Works also for
 * 64 bit pointers.
*/
#define U_PTR_TO_INT32(p) (int32_t)((uintptr_t)p)
#define U_INT32_TO_PTR(i) (void *)((uintptr_t)i)

/** @}*/

#endif // _U_COMPILER_H_

// End of file
