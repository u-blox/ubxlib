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

#ifndef _U_PORT_DEBUG_H_
#define _U_PORT_DEBUG_H_

/* No #includes allowed here */

/** @file
 * @brief Porting layer for debug functions.  These functions are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Define U_CFG_ENABLE_LOGGING to enable debug prints.  How they
 * leave the building is dictated by the platform.
 */
#if U_CFG_ENABLE_LOGGING
# define uPortLog(format, ...) \
             /*lint -e{507} suppress size incompatibility warnings in printf() */ \
             uPortLogF(format, ##__VA_ARGS__)
#else
# define uPortLog(...)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** printf()-style logging.
 *
 * @param pFormat a printf() style format string.
 * @param ...     variable argument list.
 */
void uPortLogF(const char *pFormat, ...);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_DEBUG_H_

// End of file
