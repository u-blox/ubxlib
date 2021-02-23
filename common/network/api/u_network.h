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

#ifndef _U_NETWORK_H_
#define _U_NETWORK_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the network API. These functions are
 * thread-safe.
 *
 * The functions here should be used in the following sequence; think
 * of it as "ready, steady, go ... off".
 *
 * uNetworkInit():   call this at start of day in order to make this API
 *                   available. READY.
 * uNetworkAdd():    call this when you would like to begin using the
 *                   network: when it returns the module is powered-up
 *                   and ready for configuration. STEADY.
 * uNetworkUp():     call this when you would like the network to connect;
 *                   after this is called you can send and receive stuff
 *                   over the network. GO.
 * uNetworkDown():   disconnect and shut-down the network; once this has
 *                   returned the module may enter a lower-power or
 *                   powered-off state: you must call uNetworkUp() to
 *                   talk with it again. OFF.
 * uNetworkRemove(): call this to clear up any resources belonging to
 *                   the network; once this is called uNetworkAdd()
 *                   must be called once more to re-instantiate the
 *                   network.
 * uNetworkDeinit(): call this at end of day in order to clear up any
 *                   resources owned by this API.  This internally calls
 *                   uNetworkRemove() for any networks that haven't already
 *                   been cleaned-up.
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
    U_NETWORK_TYPE_MAX_NUM
} uNetworkType_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the network API.  If the network API has already
 * been initialised this function returns success without doing
 * anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uNetworkInit();

/** Deinitialise the network API.  Any network instances will
 * be removed internally with a call to uNetworkRemove().
 */
void uNetworkDeinit();

/** Add a network instance. When this returns successfully
 * the module is powered up and available for configuration but
 * is not yet connected to anything.
 *
 * @param type             the type of network to create,
 *                         Wifi (in future BLE), cellular, etc.
 * @param pConfiguration   a pointer to the configuration
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
 *                         void * here. The first entry in
 *                         all of these structures is of type
 *                         uNetworkType_t to indicate the
 *                         type and allow cross-checking.
 * @return                 on success the handle of the
 *                         network instance, else negative
 *                         error code.  This handle may also
 *                         be used with the underlying sho/cell
 *                         API to perform operations that
 *                         cannot be carried out through
 *                         this network API.
 */
int32_t uNetworkAdd(uNetworkType_t type,
                    const void *pConfiguration);

/** Remove a network instance.  It is up to the caller to ensure
 * that the network in question is disconnected and/or powered
 * down etc.; all this function does is remove the logical
 * instance, clearing up resources.
 *
 * @param handle  the handle of the network instance to remove.
 * @return        zero on success else negative error code.
 */
int32_t uNetworkRemove(int32_t handle);

/** Bring up the given network instance, connecting it as defined
 * in the configuration passed to uNetworkAdd().  If the network
 * is already up the implementation should return success without
 * doing anything.
 *
 * @param handle the handle of the instance to bring up.
 * @return       zero on success else negative error code.
 */
int32_t uNetworkUp(int32_t handle);

/** Take down the given network instance, disconnecting
 * it from any peer entity.  After this function returns
 * uNetworkUp() must be called once more to ensure that the
 * module is brought back to a responsive state.
 *
 * @param handle the handle of the instance to take down.
 * @return       zero on success else negative error code.
 */
int32_t uNetworkDown(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_H_

// End of file
