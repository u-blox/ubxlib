/*
 * Copyright 2022 u-blox
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

#ifndef _U_DEVICE_INTERNAL_H_
#define _U_DEVICE_INTERNAL_H_

// TODO: since we've not revealed this file on master yet, could
// we possibly rename it to u_device_shared.h to follow the
// convention used elsewhere in ubxlib (see u_location_shared.h,
// u_location_test_shared.h, u_sock_test_shared.h), i.e. this
// is in the src directory, so no one else should be calling the
// functions (in other words it is already implicitly "internal"),
// but actually uDevice is sharing these functions with the rest
// of ubxlib, just not the customer, so "shared" is the important/
// unusual thing about it.

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_network.h"
#include "u_device.h"

/** @file
 * @brief Functions for initializing a u-blox device (chip or module),
 * that do not form part of the device API but are shared internally
 * for use within ubxlib.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Convenience macro to get the uDeviceInstance_t from a uDeviceHandle_t.
 *  Note: If you also want to validate the handle you should instead use
 *        uDeviceGetInstance()
 */
#define U_DEVICE_INSTANCE(devHandle) ((uDeviceInstance_t*)devHandle)

/** Convenience macro to check if a uDeviceHandle_t is of a specific uDeviceType_t.
 */
#define U_DEVICE_IS_TYPE(devHandle, devType) \
    (devHandle == NULL ? false : U_DEVICE_INSTANCE(devHandle)->deviceType == devType)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Internal data structure that uDeviceHandle_t points at.
 *  This structure may be "inherited" by each device type to provide
 *  custom data needed for each driver implementation.
 */
typedef struct {
    uint32_t magic;             /**< Magic number for detecting a stale uDeviceInstance_t.*/
    uDeviceType_t deviceType;   /**< Type of device.*/
    int32_t module;             /**< Module identification (when applicable).*/
    const void *pNetworkCfg[U_NETWORK_TYPE_MAX_NUM]; /**< Network config for device interfaces.*/

    // TODO: is it intended that the network layer hangs its private data here?
    // If so, does the "multi-network" nature of SHO require something more
    // special (i.e. a linked list or some such), or is that complication handled
    // entirely within the short range bit of the device code (in a thread-safe
    // manner), rather than up here in the network-agnostic structure (which
    // would then require some form of thread-safety up here)?  In either case,
    // how is that thread-safety achieved (e.g. do we need some way to initialise
    // a mutex in the short range bit of this, which might bring us back to
    // needing a uDeviceInit() function).
    void *pNetworkPrivate;      /**< Possible network private data.*/

    // Note: In the future structs of function pointers for socket, MQTT etc.
    // implementations may be added here.
    int32_t netType;            /**< This is only temporarily used for migration for the new uDevice API.
                                     It should be removed when uNetwork have been adjusted.*/
    // TODO: remove once we're finished.
    void *pContext;             /**< This is only temporarily used for migration for the new uDevice API.
                                     It will point at the private instance struct for the specific device type.*/
} uDeviceInstance_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a device instance.
 * uDeviceInstance_t is the structure the uDeviceHandle_t will point at.
 * Note: This function will probably be removed in the next refactorization step.
 *
 * @param type  the u-blox device type.
 *
 * @return      an allocated uDeviceInstance_t struct or NULL if out of memory.
 */
uDeviceInstance_t *pUDeviceCreateInstance(uDeviceType_t type);

/** Destroy/deallocate a device instance created by pUDeviceCreateInstance.
 *
 * @param pInstance  the instance to destroy.
 */
void uDeviceDestroyInstance(uDeviceInstance_t *pInstance);

/** Initialize a device instance.
 * This is useful when pUDeviceCreateInstance() is not used and the uDeviceInstance_t
 * is allocated manually.
 *
 * @param pInstance  the device instance to initialize.
 * @param type       the u-blox device type.
 */
void uDeviceInitInstance(uDeviceInstance_t *pInstance,
                         uDeviceType_t type);

/** Check if a device instance is valid.
 *
 * @param pInstance  the device instance to check.
 *
 * @return           true if the instance is valid.
 */
bool uDeviceIsValidInstance(const uDeviceInstance_t *pInstance);

/** Get a device instance from a device handle.
 * This will also validate the handle.
 *
 * @param      devHandle  the device handle.
 * @param[out] ppInstance the output device instance.
 * @return                0 on success else a negative error code.
 */
int32_t uDeviceGetInstance(uDeviceHandle_t devHandle,
                           uDeviceInstance_t **ppInstance);

/** Get a device type from a device handle.
 * This will also validate the handle.
 *
 * @param devHandle  the device handle.
 *
 * @return           the device type (see uDeviceType_t) on success
 *                   else a negative error code.
 */
int32_t uDeviceGetDeviceType(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_INTERNAL_H_

// End of file
