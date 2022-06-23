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

#ifndef _U_DEVICE_SHARED_H_
#define _U_DEVICE_SHARED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** @file
 * @brief Functions for initializing a u-blox device (chip or module),
 * that do not form part of the device API but are shared internally
 * for use within ubxlib.
 * IMPORTANT: unless otherwise stated, the individual functions here
 * are not thread-safe. They are intended to be called in-sequence by
 * the implementations of ubxlib API functions within a
 * uDeviceLock()/uDeviceUnlock() pair to guarantee thread-safety.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Convenience macro to get the uDeviceInstance_t from a uDeviceHandle_t.
 *  Note: if you also want to validate the handle you should instead use
 *  uDeviceGetInstance()
 */
#define U_DEVICE_INSTANCE(devHandle) ((uDeviceInstance_t *) devHandle)

/** Convenience macro to check if a uDeviceHandle_t is of a specific
 * uDeviceType_t.
 */
#define U_DEVICE_IS_TYPE(devHandle, devType) \
    (devHandle == NULL ? false : U_DEVICE_INSTANCE(devHandle)->deviceType == devType)

#ifndef U_DEVICE_NETWORKS_MAX_NUM
/** The maximum number of networks supported by a given device.
 */
# define U_DEVICE_NETWORKS_MAX_NUM 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Data structure for network stuff that is hooked into the device
 * structure.
 */
typedef struct {
    int32_t networkType; /**< the type for this network. */
    const void *pCfg; /**< constant network configuration provided by application. */
    void *pContext; /**< optional context data for this network interface. */
    void *pStatusCallbackData; /**< optional status callback for this network interface. */
} uDeviceNetworkData_t;

/** Internal data structure that uDeviceHandle_t points at.
 * This structure may be "inherited" by each device type to provide
 * custom data needed for each driver implementation.
 */
typedef struct {
    uint32_t magic;             /**< magic number for detecting a stale uDeviceInstance_t. */
    uDeviceType_t deviceType;   /**< type of device. */
    int32_t moduleType;         /**< module identification (when applicable). */
    void *pContext;             /**< private instance data for the device. */
    uDeviceNetworkData_t networkData[U_DEVICE_NETWORKS_MAX_NUM]; /**< network cfg and private data. */
    // Note: In the future structs of function pointers for socket, MQTT etc.
    // implementations may be added here.
} uDeviceInstance_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create the mutex that protects the device API.  Only the function
 * uDeviceInit() is allowed to call this.
 *
 * @return zero on success or negative error code.
 */
int32_t uDeviceMutexCreate();

/** Destroy the mutex that protects the device API.  Only the function
 * uDeviceDeinit() is allowed to call this.
 */
void uDeviceMutexDestroy();

/** Create a device instance. uDeviceInstance_t is the internal
 * structure that uDeviceHandle_t will point at.
 * Note: it is OK to call this even if uDeviceInit()/uDeviceLock()
 * has not been called.
 *
 * @param type  the u-blox device type.
 * @return      an allocated uDeviceInstance_t struct or NULL if
 *              out of memory.
 */
uDeviceInstance_t *pUDeviceCreateInstance(uDeviceType_t type);

/** Destroy/deallocate a device instance created by
 * pUDeviceCreateInstance.
 * Note: it is OK to call this even if uDeviceInit()/
 * uDeviceLock() has not been called, provided you know that
 * the instance is not being used by any other task.
 *
 * @param[in] pInstance  the instance to destroy.
 */
void uDeviceDestroyInstance(uDeviceInstance_t *pInstance);

/** Lock the device API.  This should be called internally by the
 * implementations of the uDevice and uNetwork APIs to ensure
 * thread-safety when a sequence of uDevice API calls are being
 * made.  This call will block until the device becomes available.
 *
 * @return zero on success or negative error code, for example
 *         if the device API is not currently initialised.
 */
int32_t uDeviceLock();

/** Unlock the device API, to be called by any function that
 * has called uDeviceLock() after it has completed its work.
 *
 * @return zero on success or negative error code.
 */
int32_t uDeviceUnlock();

/** Initialize a device instance. This is useful when
 * pUDeviceCreateInstance() is not used and the
 * uDeviceInstance_t is allocated manually.
 * Note: it is OK to call this even if uDeviceInit()/
 * uDeviceLock() has not been called, provided you know that
 * the instance is not being used by any other task.
 *
 * @param[in] pInstance  the device instance to initialize.
 * @param type           the u-blox device type.
 */
void uDeviceInitInstance(uDeviceInstance_t *pInstance,
                         uDeviceType_t type);

/** Check if a device instance is valid.
 * Note: it is OK to call this even if uDeviceInit()/uDeviceLock()
 * has not been called.
 *
 * @param[in] pInstance  the device instance to check.
 * @return               true if the instance is valid.
 */
bool uDeviceIsValidInstance(const uDeviceInstance_t *pInstance);

/** Get a device instance from a device handle. This will
 * also validate the handle.
 * Note: it is OK to call this even if uDeviceInit()/uDeviceLock()
 * has not been called.
 *
 * @param devHandle       the device handle.
 * @param[out] ppInstance a place to put the output device
 *                        instance, cannot be NULL.
 * @return                zero on success else a negative error code.
 */
int32_t uDeviceGetInstance(uDeviceHandle_t devHandle,
                           uDeviceInstance_t **ppInstance);

/** Get a device type from a device handle. This will also
 * validate the handle.
 * Note: it is OK to call this even if uDeviceInit()/uDeviceLock()
 * has not been called.
 *
 * @param devHandle  the device handle.
 * @return           the device type (see uDeviceType_t) on success
 *                   else a negative error code.
 */
int32_t uDeviceGetDeviceType(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_SHARED_H_

// End of file
