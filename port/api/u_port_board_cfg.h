/*
 * Copyright 2019-2024 u-blox
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

#ifndef _U_PORT_BOARD_CFG_H_
#define _U_PORT_BOARD_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */
#include "u_network_type.h"
#include "u_device_handle.h"

/** \addtogroup __port __Port
 *  @{
 */

/** @file
 * @brief Board configuration API, only implemented on platforms
 * that have a concept of compile-time board configuration, like
 * the Zephyr device tree, which you might wish to use to populate
 * the device and network configuration structures
 * (see /common/device/api/device.h and /common/network/api/network_*.h).
 * A default implementation of the functions, which do nothing and
 * return succes, is provided in u_port_board_config.c.
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

/** Modify a device configuration.
 *
 * You do not need to implement this function: where it is not
 * implemented a #U_WEAK implementation will return success.
 *
 * @param[in,out] pDeviceCfg a pointer to the device configuration
 *                           to modify, which must be a pointer to
 *                           a uDeviceCfg_t structure; the function
 *                           may modify the contents of this structure
 *                           in any way it sees fit before returning;
 *                           note that NO VALIDITY CHECKING is carried
 *                           out on pDeviceCfg, it is up to you to be
 *                           sure that points to a valid uDeviceCfg_t
 *                           structure.
 * @return                   zero on success else negative error code.
 */
int32_t uPortBoardCfgDevice(void *pDeviceCfg);

/** Modify a network configuration.
 *
 * You do not need to implement this function: where it is not
 * implemented a #U_WEAK implementation will return success.
 *
 * @param devHandle           the device handle.
 * @param networkType         the network type, used to figure out what
 *                            pNetworkCfg is pointing to.
 * @param[in,out] pNetworkCfg a pointer to the network configuration
 *                            to modify, which must be a pointer to a
 *                            uNetworkCfgXxx_t structure; the function
 *                            may modify the contents of this
 *                            structure in any way it sees fit before
 *                            returning; note that NO VALIDITY CHECKING
 *                            is carried out on pNetworkCfg, it is up
 *                            to you to be sure that points to a valid
 *                            uNetworkCfgXxx_t structure.
 * @return                    zero on success else negative error code.
 */
int32_t uPortBoardCfgNetwork(uDeviceHandle_t devHandle,
                             uNetworkType_t networkType,
                             void *pNetworkCfg);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_BOARD_CFG_H_

// End of file
