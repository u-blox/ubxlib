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

#ifndef _U_PORT_DEBUG_H_
#define _U_PORT_DEBUG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for debug functions.  These functions are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Define #U_CFG_ENABLE_LOGGING to enable debug prints.  How they
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

/** printf()-style logging; this macro is not usually called directly,
 * please call the uPortLog() macro instead so that
 * #U_CFG_ENABLE_LOGGING controls whether logging is on or off.
 *
 * @param[in] pFormat a printf() style format string.
 * @param ...        variable argument list.
 */
void uPortLogF(const char *pFormat, ...);

/** Switch logging off, so that it has no effect; it is NOT a requirement
 * that this API is implemented: where it is not implemented
 * #U_ERROR_COMMON_NOT_IMPLEMENTED should be returned.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortLogOff(void);

/** Switch logging on (the default); it is NOT a requirement
 * that this API is implemented: where it is not implemented
 * #U_ERROR_COMMON_NOT_IMPLEMENTED should be returned.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortLogOn(void);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_DEBUG_H_

// End of file
