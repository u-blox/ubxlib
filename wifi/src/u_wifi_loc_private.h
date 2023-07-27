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

#ifndef _U_WIFI_LOC_PRIVATE_H_
#define _U_WIFI_LOC_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines the functions of Wi-Fi LOC
 * that should only be used internally within Wi-Fi.
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
 * FUNCTIONS:  WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

/** Workaround for Espressif linker missing out files that
 * only contain functions which also have weak alternatives
 * (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
 *
 * You can ignore this function.
 */
void uWifiLocPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** A handler for LOC URCs; this function expects the first integer
 * parameter of the URC, which is the HTTP handle, to have been
 * already removed from the stream (for example by
 * uWifiHttpPrivateUrc() checking if the URC was actually for it).
 *
 * @param atHandle           the handle of the AT client.
 * @param[in,out] pParameter a void * parameter that should be a pointer
 *                           to #uShortRangePrivateInstance_t.
 */
void uWifiLocPrivateUrc(uAtClientHandle_t atHandle, void *pParameter);

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_LOC_PRIVATE_H_

// End of file
