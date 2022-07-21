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

#ifndef _U_ASSERT_H_
#define _U_ASSERT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_compiler.h"

/** \addtogroup assert Assert hook
 *  @{
 */

/** @file
 * @brief Assert macro, function and hook.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_ASSERT
# ifndef U_CFG_DISABLE_ASSERT
/** The assert() macro: all ubxlib code must call this and NOT call
 * the compiler/C-library assert() function.  When condition is false
 * uAssertFailed() will be called.
 */
//lint -emacro(774, U_ASSERT) Suppress condition always evaluates to true.
#  define U_ASSERT(condition) if (!(condition)) {uAssertFailed(__FILE__, __LINE__);}
# else
#  define U_ASSERT(condition)
# endif
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The function signature for the assertFailed() callback.
 * The first parameter is a pointer to the name and path of the file
 * where the assert failure occurred as a null-terminated string,
 * i.e. from __FILE__, the second parameter is the line number in
 * pFileStr where the assert failure occurred, i.e. from __LINE__.
 */
typedef void (upAssertFailed_t) (const char *pFileStr, int32_t line);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Register an assertFailed() callback.  When #U_ASSERT is called
 * with a false assert condition then pAssertFailed will be called
 * with the file string and line number of the assert; no other
 * action will be taken, it is entirely up to the pAssertFailed function
 * to do whatever it wishes (print something log, something, restart
 * the system, etc.).  If the pAssertFailed function returns then
 * code execution will resume at the line after the assert failure
 * occurred.
 *
 * @param[in] pAssertFailed the assert failure function to register.
 */
void uAssertHookSet(upAssertFailed_t *pAssertFailed);

/** The default assertFailed() function.  If no assert hook has been
 * registered (with uAssertHookSet()) then the assertFailed() function
 * will print the file and line number of the assert and then enter an
 * infinite loop.
 *
 * @param[in] pFileStr pointer to the name and path of the file where
 *                     the assert failure occurred, as a null-terminated
 *                     string, from __FILE__.
 * @param line         the line number in pFileStr where the assert
 *                     failure occurred, from __LINE__.
 */
//lint -function(exit, uAssertFailed) tell Lint that this has the same
// proprties as exit()
void uAssertFailed(const char *pFileStr, int32_t line) U_CLANG_ANALYZER_NORETURN;

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_ASSERT_H_

// End of file
