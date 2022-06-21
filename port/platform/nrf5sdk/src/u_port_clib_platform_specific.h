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

#ifndef _U_PORT_CLIB_PLATFORM_SPECIFIC_H_
#define _U_PORT_CLIB_PLATFORM_SPECIFIC_H_

/** @file
 * @brief Implementations of C library functions not available on this
 * platform.
 */

/** Note: the nRF5 SDK does not provide the non-floating
 * point versions of the stdio library functions, hence there
 * are no macros to bring them in here.
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
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** strtok().
 * Note: strtok_r is actually present in the C library but the GCC
 * Makefile inside NRF5 selects C99 which does not pull the prototype
 * for strtok_r into string.h.  Hence the need for this.
 */
char *strtok_r(char *pStr, const char *pDelimiters, char **ppSave);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_CLIB_PLATFORM_SPECIFIC_H_

// End of file
