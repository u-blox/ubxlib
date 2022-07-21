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

#ifndef _U_WIFI_CFG_H_
#define _U_WIFI_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that configure Wifi.
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

typedef struct {
    bool notUsed; //DHCP/static TODO
} uWifiCfg_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure wifi for a short range module, may requirer module restarts
 *  so can take up to 500 ms before it returns.
 *
 * @param devHandle the handle of the wifi instance.
 * @param[in] pCfg  pointer to the configuration data, must not be NULL.
 * @return          zero on success or negative error code
 *                  on failure.
 */
int32_t uWifiCfgConfigure(uDeviceHandle_t devHandle,
                          const uWifiCfg_t *pCfg);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_CFG_H_

// End of file
