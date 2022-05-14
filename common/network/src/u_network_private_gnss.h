/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
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

/** Initialise the network API for GNSS.  Should not be
 * called if this API is already initialised.
 *
 * @return  zero on success else negative error code.
 */
int32_t uNetworkInitGnss(void);

/** Deinitialise the GNSS network API; should only be called
 * if this API was previously initialised.  BEFORE this is called
 * all GNSS network instances must have been removed with
 * a call to uNetworkRemoveGnss().
 */
void uNetworkDeinitGnss(void);

/** TODO: WILL BE REMOVED.
 * Add a GNSS network instance.  uNetworkInitGnss() must have
 * been called before this is called.
 *
 * @param pConfiguration   a pointer to the configuration.
 * @param[out] pDevHandle  a pointer to the output handle. Will only be set on success.
 * @return                 zero on success or negative error code on failure.
 */
int32_t uNetworkAddGnss(const uNetworkConfigurationGnss_t *pConfiguration,
                        uDeviceHandle_t *pDevHandle);

/** TODO: WILL BE REMOVED.
 * Remove a GNSS network instance.  It is up to the caller
 * to ensure that the network is disconnected and/or powered
 * down etc.; all this function does is remove the logical
 * instance.  uNetworkInitGnss() must have been called before
 * this is called.
 *
 * @param devHandle  the handle of the GNSS instance to remove.
 * @return           zero on success else negative error code.
 */
int32_t uNetworkRemoveGnss(uDeviceHandle_t devHandle);

/** TODO: WILL BE REMOVED.
 * Bring up the given GNSS network instance. uNetworkAddGnss()
 * must have been called first to create this instance.
 *
 * @param devHandle        the handle of the instance to bring up.
 * @param pConfiguration   a pointer to the configuration for this
 *                         instance.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkUpGnss(uDeviceHandle_t devHandle,
                       const uNetworkConfigurationGnss_t *pConfiguration);

/** TODO: WILL BE REMOVED.
 * Take down the given GNSS network instance. uNetworkAddGnss()
 * must have been called first to create this instance.
 *
 * @param devHandle        the handle of the instance to take down.
 * @param pConfiguration   a pointer to the configuration for this
 *                         instance.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkDownGnss(uDeviceHandle_t devHandle,
                         const uNetworkConfigurationGnss_t *pConfiguration);

/** Take up or down the given GNSS network instance. uDeviceOpen()
 * must have been called first to create the device handle.
 *
 * @param devHandle        the handle of the instance to take down.
 * @param pCfg             a pointer to the configuration for this
 *                         instance. Only required for up.
 * @param up               take the GNSS interface up or down.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkChangeStateGnss(uDeviceHandle_t devHandle,
                                uDeviceNetworkCfgGnss_t *pCfg, bool up);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_PRIVATE_GNSS_H_

// End of file
