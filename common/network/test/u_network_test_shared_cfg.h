/*
 * Copyright 2020 u-blox
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

/** Determine if the given network type supports sockets operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_SOCK(type) ((type == U_NETWORK_TYPE_CELL) || \
                                            (type == U_NETWORK_TYPE_WIFI))

/** Determine if the given network type supports secure sockets operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_SECURE_SOCK(type) (type == U_NETWORK_TYPE_CELL)

/** Determine if the given network type supports MQTT operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_MQTT(type) (type == U_NETWORK_TYPE_CELL)

/** Determine if the given network type supports location operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_LOCATION(type) ((type == U_NETWORK_TYPE_CELL) || \
                                                (type == U_NETWORK_TYPE_GNSS))

/** Determine if the given network and module combination supports
 * credential storage.
 */
#define U_NETWORK_TEST_TYPE_HAS_CREDENTIAL_STORAGE(type, module) ((type == U_NETWORK_TYPE_CELL) || \
                                                                  (type == U_NETWORK_TYPE_WIFI) || \
                                                                  ((type == U_NETWORK_TYPE_BLE) && \
                                                                   (module != (int32_t) U_SHORT_RANGE_MODULE_TYPE_INTERNAL)))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Network test configuration information with a type indicator,
 * a pointer to the configuration information and room for the
 * handle to be stored.
 * Note: order is important, this is statically initialsed.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    uNetworkType_t type;
    void *pConfiguration;  // TO BE REMOVED
    // Device and network config
    uDeviceCfg_t *pDeviceCfg;
    void *pNetworkCfg;
} uNetworkTestCfg_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** All of the information for the underlying network
 * types as an array.
 */
extern uNetworkTestCfg_t gUNetworkTestCfg[];

/** Number of items in the gNetworkTestCfg array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
extern const size_t gUNetworkTestCfgSize;

#if U_CFG_ENABLE_LOGGING
/** Return a name for a network type.
 */
//lint -esym(843, gpUNetworkTestTypeName) Suppress could be declared
// as const: this may be used in position independent code
// and hence can't be const
extern const char *gpUNetworkTestTypeName[];
#endif

#ifdef __cplusplus
}
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Update a GNSS device configuration for use with the AT
 *  interface of a another device
 *
 * @param devHandleAt    the device handle providing the
 *                       AT interface (e.g. cellular).  NOT the
 *                       AT client handle, the handle of the device.
 * @param pUDeviceCfg    a pointer to a possible GNSS device configuration.
 */
void uNetworkTestGnssAtCfg(uDeviceHandle_t devHandleAt, uDeviceCfg_t *pUDeviceCfg);

/** Check if a specified test configuration is valid within the current set
 *  of defines and that it hasn't been opened already
 *
 * @param index  test configuration index.
 * @return       true or false.
 */
bool uNetworkTestDeviceValidForOpen(int32_t index);

/** Close the device for the test configuration at the specified index.
 *  Handle possible multiple references to this device.
 *
 * @param index  test configuration index.
 * @return       zero on success else negative error code.
 */
int32_t uNetworkTestClose(int32_t index);

/** Get the module type of a specified test configuration
 *
 * @param index  test configuration index.
 * @return       module type.
 */
int32_t uNetworkTestGetModuleType(int32_t index);

#endif // _U_NETWORK_TEST_CFG_H_

// End of file
