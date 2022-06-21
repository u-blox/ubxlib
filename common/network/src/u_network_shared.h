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

#ifndef _U_NETWORK_SHARED_H_
#define _U_NETWORK_SHARED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief Functions for handling networks that do not form part of
 * the network API but are shared internally for use within ubxlib.
 */

#include "u_network.h"

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** This function may seem redundant but there are situations
 * where bringing up a network on a device results in a "hidden"
 * device handle being created, one which is held inside ubxlib.
 * This will be the case when, for instance, a GNSS network is
 * brought up on a cellular device: the GNSS network has its own
 * "device" handle, held within ubxlib, and if you want to use
 * the GNSS API functions directly you will need to obtain that
 * handle.  If there is no such hidden handle, devHandle will
 * just be returned, so there is never any harm in calling this
 * function.
 * This function is only guaranteed to work in all cases if
 * the network interface is up at the time.
 *
 * @param devHandle  the handle of the device.
 * @param netType    the module interface to obtain the
 *                   handle for.
 * @return           the device handle or NULL in case of error.
 */
uDeviceHandle_t uNetworkGetDeviceHandle(uDeviceHandle_t devHandle,
                                        uNetworkType_t netType);

/** Get the network data for the given network type from the
 * device instance. IMPORTANT: there is, of course, nothing
 * to stop someone calling uDeviceClose() and vapourising the
 * data you have a pointer to here, hence it is advisable to
 * only call this between a uDeviceLock()/uDeviceUnlock() pair.
 *
 * @param pInstance the device instance.
 * @param netType   the network type.
 * @return          a pointer to the network data or NULL on failure.
 */
uDeviceNetworkData_t *pUNetworkGetNetworkData(uDeviceInstance_t *pInstance,
                                              uNetworkType_t netType);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_SHARED_H_

// End of file
