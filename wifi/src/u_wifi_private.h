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

#ifndef _U_WIFI_PRIVATE_H_
#define _U_WIFI_PRIVATE_H_

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to the wifi API.
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
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Convert a wifi handle to a short range handle
 *
 * @param wifiHandle  the wifi handle to convert
 * @return           a short range handle on success,
 *                   on failure negative value.
 */
int32_t uWifiToShoHandle(int32_t wifiHandle);

/** Convert a short range handle to a wifi handle
 *
 * @param shortRangeHandle  the short range handle to convert
 * @return                  a wifi handle on success,
 *                          on failure negative value.
 */
int32_t uShoToWifiHandle(int32_t shortRangeHandle);


#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_PRIVATE_H_

// End of file
