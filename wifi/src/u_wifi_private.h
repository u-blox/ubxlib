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

#ifndef _U_WIFI_PRIVATE_H_
#define _U_WIFI_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions used only within Wifi.
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
 * FUNCTIONS:
 * -------------------------------------------------------------- */

/** A handler for +UUDHTTP URCs which chains together the HTTP and LOC
 * URC handlers, uWifiHttpPrivateUrc() and uWifiLocPrivateUrc(),
 * either of which might own the response.
 *
 * @param atHandle           the handle of the AT client.
 * @param[in,out] pParameter a void * parameter that will be passed on to
 *                           uWifiLocPrivateUrc() or uWifiHttpPrivateUrc().
 */
void uWifiPrivateUudhttpUrc(uAtClientHandle_t atHandle, void *pParameter);

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_PRIVATE_H_

// End of file
