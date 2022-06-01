/*
 * Copyright 2022 u-blox
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

#ifndef _U_NETWORK_H_
#define _U_NETWORK_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** @file
 * @brief This header file defines the network API. These functions are
 * thread-safe.
 *
 * The functions here should be used in conjunction with those in the
 * uDevice API in the following sequence.
 *
 * uDeviceInit():           call this at start of day in order to make
 *                          the device API available.
 * uDeviceOpen():           call this with a pointer to a const structure
 *                          containing the physical configuration for the
 *                          device (module type, physical interface (UART
 *                          etc.), pins used, etc.): when the function
 *                          returns the module is powered-up and ready to
 *                          support a network.
 * uNetworkInterfaceUp():   call this with the device handle and a pointer
 *                          to a const structure containing the network
 *                          configuration (e.g. SSID in the case of Wifi,
 *                          APN in the case of cellular, etc.) when you
 *                          would like the network to connect; after this
 *                          is called you can send and receive stuff over
 *                          the network.
 * uNetworkInterfaceDown(): disconnect the network; the nework remains
 *                          powered-up and may be reconfigured etc.: you
 *                          must call uNetworkInterfaceUp() to talk with
 *                          it again.
 * uDeviceClose():          call this to power the device down and clear
 *                          up any resources belonging to it; uDeviceOpen()
 *                          must be called re-instantiate the device.
 * uDeviceDeinit():         call this at end of day in order to clear up any
 *                          resources owned by the device API.
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

/** Network types.
 */
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_MAX_NUM)
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_NONE)
//  Suppress not used within defaulted switch
typedef enum {
    U_NETWORK_TYPE_NONE,
    U_NETWORK_TYPE_BLE,
    U_NETWORK_TYPE_CELL,
    U_NETWORK_TYPE_WIFI,
    U_NETWORK_TYPE_GNSS,
    U_NETWORK_TYPE_MAX_NUM
} uNetworkType_t;

/** A version number for the network configuration structure. In
 * general you should allow the compiler to initialise any variable
 * of this type to zero and ignore it.  It is only set to a value
 * other than zero when variables in a new and extended version of
 * the structure it is a part of are being used, the version number
 * being employed by this code to detect that and, more importantly,
 * to adopt default values for any new elements when the version
 * number is STILL ZERO, maintaining backwards compatibility with
 * existing application code.  The structure this is a part of will
 * include instructions as to when a non-zero version number should
 * be set.
 */
typedef int32_t uNetworkCfgVersion_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Bring up the given network interface on a device. If the network
 * is already up the implementation should return success without
 * doing anything.
 *
 * @param devHandle        the handle of the device to bring up.
 * @param netType          which of the module interfaces.
 * @param[in] pCfg         a pointer to the configuration
 *                         information for the given network
 *                         type.  This must be stored
 *                         statically, a true constant: the
 *                         contents are not copied by this
 *                         function. The configuration
 *                         structures are defined by this
 *                         API in the u_network_xxx.h header
 *                         files and have the name
 *                         uNetworkConfigurationXxx_t, where
 *                         xxx is replaced by one of Cell,
 *                         Ble or Wifi.  The configuration
 *                         is passed transparently through to
 *                         the given API, hence the use of
 *                         void * here. The second entry in
 *                         all of these structures is of type
 *                         uNetworkType_t to indicate the
 *                         type and allow cross-checking.
 *                         Can be set to NULL on subsequent calls
 *                         if the configuration is unchanged.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkInterfaceUp(uDeviceHandle_t devHandle, uNetworkType_t netType,
                            const void *pCfg);

/** Take down the given network interface on a device, disconnecting
 * it from any peer entity.  After this function returns
 * uNetworkInterfaceUp() must be called once more to ensure that the
 * module is brought back to a responsive state.
 *
 * @param devHandle the handle of the device to take down.
 * @param netType   which of the module interfaces.
 * @return          zero on success else negative error code.
 */
int32_t uNetworkInterfaceDown(uDeviceHandle_t devHandle, uNetworkType_t netType);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_H_

// End of file
