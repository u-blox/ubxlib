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

#ifndef _U_NETWORK_TEST_CFG_H_
#define _U_NETWORK_TEST_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** @file
 * @brief Types and network test configuration information shared
 * between the testing of the network API, u-blox security API and
 * the testing of the sockets API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_TEST_NETWORKS_MAX_NUM
/** The maximum number of networks supported by a given device.
 */
# define U_NETWORK_TEST_NETWORKS_MAX_NUM 2
#endif

/** Determine if the given network type supports location operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_LOCATION(type) ((type == U_NETWORK_TYPE_CELL) || \
                                                (type == U_NETWORK_TYPE_GNSS))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A device/network, intended to be used in a list of device/network
 * configurations that a test is to be conducted on.
 */
typedef struct uNetworkList_t {
    uDeviceHandle_t *pDevHandle; /**< a pointer to a place to store the device handle. */
    const uDeviceCfg_t *pDeviceCfg; /**< a pointer to the device configuration. */
    uNetworkType_t networkType; /**< the network type. */
    const void *pNetworkCfg; /**< a pointer to the network configuration. */
    struct uNetworkList_t *pNext; /**< the next entry in the list. */
} uNetworkTestList_t;

/** A function that can be called to check if a given device/network/
 * module combination is valid for a given test.
 *
 * @param deviceType  the device type (cellular, short-range, GNSS).
 * @param networkType the network type (BLE, W-Fi, cellular, GNSS).
 * @param moduleType  the module type (NINA-W15, SARA-R5, etc.).
 * @return            true if the combination is valid for the named
 *                    feature, else false.
 */
typedef bool (*uNetworkTestValidFunction_t) (uDeviceType_t deviceType,
                                             uNetworkType_t networkType,
                                             int32_t moduleType);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if U_CFG_ENABLE_LOGGING
/** Return a name for a network type.
 */
//lint -esym(843, gpUNetworkTestTypeName) Suppress could be declared
// as const: this may be used in position independent code
// and hence can't be const
extern const char *gpUNetworkTestTypeName[];

/** Return a name for a device type.
 */
//lint -esym(843, gpUNetworkTestDeviceTypeName) Suppress could be declared
// as const: this may be used in position independent code
// and hence can't be const
extern const char *gpUNetworkTestDeviceTypeName[];
#endif

#ifdef __cplusplus
}
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Allocate a list of devices/networks to operate on for a test.
 * When done the list must be free'ed with a call to
 * uNetworkTestListFree().  Note that there is a single list, this
 * function is NOT thread-safe.
 *
 * @param pValidFunction a function which should return true if
 *                       the given device/network/module combination
 *                       is valid for the purpose of a test; use
 *                       NULL to get everything.
 * @return               a pointer to a test list, a linked list of
 *                       devices/networks to operate on, or NULL if
 *                       there is no such list.
 */
uNetworkTestList_t *pUNetworkTestListAlloc(uNetworkTestValidFunction_t pValidFunction);

/** Free a list of devices/networks that was created with
 * pUNetworkTestListAlloc().  This does not close etc. the
 * devices/networks, it simply frees the allocated memory.
 */
void uNetworkTestListFree(void);

/** Close all of the devices, bringing down their networks.
 */
void uNetworkTestCleanUp(void);

/** Return true if the combination supports sockets.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if sockets is supported, else false.
 */
bool uNetworkTestHasSock(uDeviceType_t deviceType,
                         uNetworkType_t networkType,
                         int32_t moduleType);

/** Return true if the combination supports secure sockets.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if secure sockets is supported,
 *                    else false.
 */
bool uNetworkTestHasSecureSock(uDeviceType_t deviceType,
                               uNetworkType_t networkType,
                               int32_t moduleType);

/** Return true if the combination supports u-blox security.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if secure sockets is supported,
 *                    else false.
 */
bool uNetworkTestHasSecurity(uDeviceType_t deviceType,
                             uNetworkType_t networkType,
                             int32_t moduleType);

/** Return true if the combination supports MQTT.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if MQTT is supported, else false.
 */
bool uNetworkTestHasMqtt(uDeviceType_t deviceType,
                         uNetworkType_t networkType,
                         int32_t moduleType);

/** Return true if the combination supports MQTT-SN.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if MQTT-SN is supported, else false.
 */
bool uNetworkTestHasMqttSn(uDeviceType_t deviceType,
                           uNetworkType_t networkType,
                           int32_t moduleType);

/** Return true if the combination supports credential storage.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if credential storage is supported,
 *                    else false.
 */
bool uNetworkTestHasCredentialStorage(uDeviceType_t deviceType,
                                      uNetworkType_t networkType,
                                      int32_t moduleType);

/** Return true if the device is a short-range one.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if this is a short-range configuration,
 *                    else false.
 */
bool uNetworkTestIsDeviceShortRange(uDeviceType_t deviceType,
                                    uNetworkType_t networkType,
                                    int32_t moduleType);

/** Return true if the device is a cellular one.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if this is a cellular configuration,
 *                    else false.
 */
bool uNetworkTestIsDeviceCell(uDeviceType_t deviceType,
                              uNetworkType_t networkType,
                              int32_t moduleType);

/** Return true if the combination is a BLE one.
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if this is a BLE configuration,
 *                    else false.
 */
bool uNetworkTestIsBle(uDeviceType_t deviceType,
                       uNetworkType_t networkType,
                       int32_t moduleType);

/** Return true if the combination supports
 * uNetworkSetStatusCallback().
 *
 * @param deviceType  the device type.
 * @param networkType the network type.
 * @param moduleType  the module type.
 * @return            true if uNetworkSetStatusCallback()
 *                    is supported, else false.
 */
bool uNetworkTestHasStatusCallback(uDeviceType_t deviceType,
                                   uNetworkType_t networkType,
                                   int32_t moduleType);

#endif // _U_NETWORK_TEST_CFG_H_

// End of file
