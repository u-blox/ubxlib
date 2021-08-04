/*
 * Copyright 2020 u-blox Ltd
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

/* No #includes allowed here */

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

/** Determine if the given network type supports MQTT operations.
 */
#define U_NETWORK_TEST_TYPE_HAS_MQTT(type) ((type == U_NETWORK_TYPE_CELL) || \
                                            (type == U_NETWORK_TYPE_WIFI))

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
    int32_t handle;
    uNetworkType_t type;
    void *pConfiguration;
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
extern const char *gpUNetworkTestTypeName[];
#endif

#ifdef __cplusplus
}
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Update a GNSS network configuration for use with the AT
 * interface.
 *
 * @param networkHandleAt    the handle of the network providing the
 *                           AT interface (e.g. cellular).  NOT the
 *                           AT client handle, the handle of the network.
 * @param pGnssConfiguration a pointer to a structure of type
 *                           uNetworkConfigurationGnss_t where the first
 *                           element is set to U_NETWORK_TYPE_GNSS.
 */
void uNetworkTestGnssAtConfiguration(int32_t networkHandleAt,
                                     void *pGnssConfiguration);

#endif // _U_NETWORK_TEST_CFG_H_

// End of file
