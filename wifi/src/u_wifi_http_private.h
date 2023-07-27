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

#ifndef _U_WIFI_HTTP_PRIVATE_H_
#define _U_WIFI_HTTP_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines the functions of Wi-Fi HTTP
 * the should only be used within Wifi.
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
void uWifiHttpPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** A handler for HTTP URCs.
 *
 * @param atHandle           the handle of the AT client.
 * @param[in,out] pParameter a void * parameter that should be a pointer
 *                           to #uShortRangePrivateInstance_t.
 * @return                   true if the reply was a true HTTP one and
 *                           was handled, or false if the reply was not
 *                           a HTTP one, likely a LOC one (which uses
 *                           the same URC) and hence should be handled
 *                           by uWifiLocPrivateUrc(); the code determines
 *                           if the URC was an HTTP one by looking at
 *                           the first integer parameter, hence this will
 *                           be GONE when the function returns false.
 */
bool uWifiHttpPrivateUrc(uAtClientHandle_t atHandle, void *pParameter);

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_HTTP_PRIVATE_H_

// End of file
