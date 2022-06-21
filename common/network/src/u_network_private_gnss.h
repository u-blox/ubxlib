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

#ifndef _U_NETWORK_PRIVATE_GNSS_H_
#define _U_NETWORK_PRIVATE_GNSS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/* This header file defines the GNSS specific part of the
 * the network API. These functions perform NO error checking
 * and are NOT thread-safe; they should only be called from
 * within the network API which sorts all that out.
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

/** Take up or down the given GNSS network instance. uDeviceOpen()
 * must have been called first to create the device handle.
 *
 * @param devHandle        the handle of the instance to take down.
 * @param[in] pCfg         a pointer to the configuration for this
 *                         instance. Only required for up.
 * @param upNotDown        take the GNSS interface up or down.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkPrivateChangeStateGnss(uDeviceHandle_t devHandle,
                                       const uNetworkCfgGnss_t *pCfg,
                                       bool upNotDown);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_PRIVATE_GNSS_H_

// End of file
