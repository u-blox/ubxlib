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

#ifndef _U_PORT_CLIB_MKTIME64_H_
#define _U_PORT_CLIB_MKTIME64_H_

/** @file
 * @brief This header file is somewhat of a special case: usually
 * the C library functions in this directory have no header file, they
 * are brought in as necessary through being added to
 * u_port_clib_platform_specific.h specifically for each platform.
 * However, a 64-bit version of mktime() is required by the credential
 * security code and hence it is presented here in a separate header
 * so that source file can include it alone, without everyone and
 * their dog having to get both it and the definition of struct tm
 * in all the places that u_port_clib_platform_specific.h is included.
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

/** mktime() with a guaranteed 64-bit return value.
 */
int64_t mktime64(struct tm *pTm);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_CLIB_MKTIME64_H_

// End of file
