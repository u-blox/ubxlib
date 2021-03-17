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

#ifndef _U_PORT_CLIB_PLATFORM_SPECIFIC_H_
#define _U_PORT_CLIB_PLATFORM_SPECIFIC_H_

/** @file
 * @brief Implementations of C library functions not available on this
 * platform.
 */

#ifdef U_CFG_ZEPHYR_USE_NEWLIB
/** Floating point is not required by ubxlib so switch to
 * the integer versions of the stdio library functions.
 * Note: ubxlib code will not log floating point values
 * (i.e. %f or %d types) and will not use maths functions
 * (e.g. pow(), log10()) or, of course, double or float types.
 * If we're not using newlib there's no need to worry as the
 * built-in Zephyr C library doesn't support floating point
 * in any case.
 */
#define snprintf sniprintf
#define printf iprintf
#define vprintf viprintf
#define sscanf siscanf
#else
#endif

#include "time.h"           // For struct tm

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
 */
char *strtok_r(char *pStr, const char *pDelimiters, char **ppSave);

/** rand().
 */
int rand();

/** isBlank().
 */
int isblank(int character);

/** mktime().
 */
time_t mktime(struct tm *pTm);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_CLIB_PLATFORM_SPECIFIC_H_

// End of file
