/*
 * Copyright 2020 u-blox Cambourne Ltd
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

#ifndef _U_NETWORK_PRIVATE_BLE_H_
#define _U_NETWORK_PRIVATE_BLE_H_

/* No #includes allowed here */

/* This header file defines the BLE specific part of the
 * network API. These functions perform NO error checking
 * and are NOT thread-safe; they should only be called
 * from within the network API which sorts all that out.
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

/** Initialise the network API for BLE.  Should not be
 * called if this API is already initialised.
 *
 * @return  zero on success else negative error code.
 */
int32_t uNetworkInitBle(void);

/** Deinitialise the BLE network API; should only be
 * called if this API was previously initialised.  BEFORE this is
 * called all BLE network instances must have been removed
 * with a call to uNetworkRemoveBle().
 */
void uNetworkDeinitBle(void);

/** Add a BLE network instance.  uNetworkInitBle() must have
 * been called before this is called.
 *
 * @param pConfiguration   a pointer to the configuration.
 * @return                 on success the handle of the
 *                         instance, else negative error code.
 */
int32_t uNetworkAddBle(const uNetworkConfigurationBle_t *pConfiguration);

/** Remove a BLE network instance.  It is up to the caller
 * to ensure that the network is disconnected and/or powered
 * down etc.; all this function does is remove the logical
 * instance.  uNetworkInitBle() must have been called before
 * this is called.
 *
 * @param handle  the handle of the BLE instance to remove.
 * @return        zero on success else negative error code.
 */
int32_t uNetworkRemoveBle(int32_t handle);

/** Bring up the given BLE network instance. uNetworkAddBle()
 * must have been called first to create this instance.
 *
 * @param handle           the handle of the instance to bring up.
 * @param pConfiguration   a pointer to the configuration for this
 *                         instance.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkUpBle(int32_t handle,
                      const uNetworkConfigurationBle_t *pConfiguration);

/** Take down the given BLE network instance. uNetworkAddBle()
 * must have been called first to create this instance.
 *
 * @param handle           the handle of the instance to take down.
 * @param pConfiguration   a pointer to the configuration for this
 *                         instance.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkDownBle(int32_t handle,
                        const uNetworkConfigurationBle_t *pConfiguration);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_PRIVATE_BLE_H_

// End of file
