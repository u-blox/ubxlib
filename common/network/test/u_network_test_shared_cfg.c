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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Test network configuration information.
 * IMPORTANT this is used when testing *both* the network API
 * and the sockets API, it is SHARED between them.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"

#include "u_network_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The network configuration for BLE.
 */
static const uNetworkConfigurationBle_t gConfigurationBle = {
    U_NETWORK_TYPE_NONE /* TODO: replace this with BLE config info. */
};

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
/** The network configuration for cellular.
 */
static const uNetworkConfigurationCell_t gConfigurationCell = {
    U_NETWORK_TYPE_CELL,
    U_CELL_TEST_CFG_SIM_PIN,
    U_CFG_TEST_CELL_MODULE_TYPE,
    U_CELL_TEST_CFG_APN,
    U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS,
    U_CFG_APP_CELL_UART,
    U_CFG_APP_PIN_CELL_TXD,
    U_CFG_APP_PIN_CELL_RXD,
    U_CFG_APP_PIN_CELL_CTS,
    U_CFG_APP_PIN_CELL_RTS,
    U_CFG_APP_PIN_CELL_ENABLE_POWER,
    U_CFG_APP_PIN_CELL_PWR_ON,
    U_CFG_APP_PIN_CELL_VINT
};
#else
static const uNetworkConfigurationCell_t gConfigurationCell = {U_NETWORK_TYPE_NONE};
#endif

/** The network configuration for Wifi.
 */
static const uNetworkConfigurationWifi_t gConfigurationWifi = {
    U_NETWORK_TYPE_NONE /* TODO: replace this with Wifi config info. */
};

/** All of the information for the underlying network
 * types as an array.
 */
uNetworkTestCfg_t gUNetworkTestCfg[] = {
    {-1, U_NETWORK_TYPE_BLE, (const void *) &gConfigurationBle},
    {-1, U_NETWORK_TYPE_CELL, (const void *) &gConfigurationCell},
    {-1, U_NETWORK_TYPE_WIFI, (const void *) &gConfigurationWifi}
};

/** Number of items in the gNetwork array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUNetworkTestCfgSize = sizeof(gUNetworkTestCfg) /
                                    sizeof (gUNetworkTestCfg[0]);


#if U_CFG_ENABLE_LOGGING
/** Return a name for a network type.
 */
const char *const gpUNetworkTestTypeName[] = {"none",     // U_NETWORK_TYPE_NONE
                                              "BLE",      // U_NETWORK_TYPE_BLE
                                              "cellular", // U_NETWORK_TYPE_CELL
                                              "Wifi"      // U_NETWORK_TYPE_WIFI
                                             };
#endif

// End of file
