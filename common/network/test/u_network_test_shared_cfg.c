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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Test network configuration information.
 * IMPORTANT this is used when testing *both* the network API,
 * the sockets API and the u-blox security API, it is SHARED between
 * them.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

//lint -efile(766, u_port.h) Suppress header file not used, which
// is true if U_CELL_TEST_CFG_APN is not defined
#include "u_port.h" // For U_PORT_STRINGIFY_QUOTED()

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

//lint -efile(766, u_wifi_test_cfg.h)
#include "u_short_range_test_selector.h"
#if U_SHORT_RANGE_TEST_WIFI()
//# include "u_short_range_test_private.h"
//# include "u_short_range_module_type.h"
//# if U_CFG_TEST_SHORT_RANGE_MODULE_HAS_WIFI()
# include "u_wifi_test_cfg.h"
#endif
//#endif*/

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#endif

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"
#include "u_network_config_gnss.h"

#include "u_network_test_shared_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The network configuration for BLE.
 */
#if U_SHORT_RANGE_TEST_BLE()
static uNetworkConfigurationBle_t gConfigurationBle = {
# ifdef U_CFG_BLE_MODULE_INTERNAL
    .type = U_NETWORK_TYPE_BLE,
    .module = (int32_t)U_SHORT_RANGE_MODULE_TYPE_INTERNAL,
# else
    .type = U_NETWORK_TYPE_BLE,
    .module = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
# endif
    .uart = U_CFG_APP_SHORT_RANGE_UART,
    .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
    .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
    .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
    .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
    .role = U_CFG_APP_SHORT_RANGE_ROLE, // Peripheral
    .spsServer = true // Enable sps server
};
#else
static uNetworkConfigurationBle_t gConfigurationBle = {U_NETWORK_TYPE_NONE};
#endif

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
/** The network configuration for cellular.
 */
static uNetworkConfigurationCell_t gConfigurationCell = {
    .type = U_NETWORK_TYPE_CELL,
    .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
    .pPin = U_CELL_TEST_CFG_SIM_PIN,
# ifdef U_CELL_TEST_CFG_APN
    .pApn = U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
# else
    .pApn = NULL,
# endif
    .timeoutSeconds = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS,
    .uart = U_CFG_APP_CELL_UART,
    .pinTxd = U_CFG_APP_PIN_CELL_TXD,
    .pinRxd = U_CFG_APP_PIN_CELL_RXD,
    .pinCts = U_CFG_APP_PIN_CELL_CTS,
    .pinRts = U_CFG_APP_PIN_CELL_RTS,
    .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
    .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
    .pinVInt = U_CFG_APP_PIN_CELL_VINT
};
#else
static uNetworkConfigurationCell_t gConfigurationCell = {U_NETWORK_TYPE_NONE};
#endif

/** The network configuration for Wifi.
 */
#if U_SHORT_RANGE_TEST_WIFI()
static uNetworkConfigurationWifi_t gConfigurationWifi = {
    .type = U_NETWORK_TYPE_WIFI,
    .module = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
    .uart = U_CFG_APP_SHORT_RANGE_UART,
    .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
    .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
    .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
    .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
    .pSsid = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
    .authentication = U_WIFI_TEST_CFG_AUTHENTICATION,
    .pPassPhrase = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE)
};
#else
static uNetworkConfigurationWifi_t gConfigurationWifi = {U_NETWORK_TYPE_NONE};
#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
/** The network configuration for GNSS.
 */
static uNetworkConfigurationGnss_t gConfigurationGnss = {
    U_NETWORK_TYPE_GNSS,
    U_CFG_TEST_GNSS_MODULE_TYPE,
    U_CFG_APP_PIN_GNSS_ENABLE_POWER,
    U_GNSS_TRANSPORT_NMEA_UART,
    U_CFG_APP_GNSS_UART,
    U_CFG_APP_PIN_GNSS_TXD,
    U_CFG_APP_PIN_GNSS_RXD,
    U_CFG_APP_PIN_GNSS_CTS,
    U_CFG_APP_PIN_GNSS_RTS,
    0,
    U_CFG_APP_CELL_PIN_GNSS_POWER,
    U_CFG_APP_CELL_PIN_GNSS_DATA_READY
};
#else
static uNetworkConfigurationGnss_t gConfigurationGnss = {U_NETWORK_TYPE_NONE};
#endif

/** All of the information for the underlying network
 * types as an array.  Order is important: CELL must come before
 * GNSS so that the cellular handle can be passed on to GNSS.
 */
uNetworkTestCfg_t gUNetworkTestCfg[] = {
    {NULL, U_NETWORK_TYPE_BLE, (void *) &gConfigurationBle},
    {NULL, U_NETWORK_TYPE_CELL, (void *) &gConfigurationCell},
    {NULL, U_NETWORK_TYPE_WIFI, (void *) &gConfigurationWifi},
    {NULL, U_NETWORK_TYPE_GNSS, (void *) &gConfigurationGnss}
};

/** Number of items in the gNetwork array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUNetworkTestCfgSize = sizeof (gUNetworkTestCfg) /
                                    sizeof (gUNetworkTestCfg[0]);

#if U_CFG_ENABLE_LOGGING
/** Return a name for a network type.
 */
//lint -esym(843, gpUNetworkTestTypeName) Suppress could be declared
// as const: this may be used in position independent code
// and hence can't be const
const char *gpUNetworkTestTypeName[] = {"none",     // U_NETWORK_TYPE_NONE
                                        "BLE",      // U_NETWORK_TYPE_BLE
                                        "cellular", // U_NETWORK_TYPE_CELL
                                        "Wifi",     // U_NETWORK_TYPE_WIFI
                                        "GNSS"      // U_NETWORK_TYPE_GNSS
                                       };
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Update a GNSS network configuration for use with the AT interface.
void uNetworkTestGnssAtConfiguration(uDeviceHandle_t devHandleAt,
                                     void *pGnssConfiguration)
{
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    if ((devHandleAt != NULL) &&
        (*((uNetworkType_t *) (pGnssConfiguration)) == U_NETWORK_TYPE_GNSS)) {
        ((uNetworkConfigurationGnss_t *) pGnssConfiguration)->transportType = U_GNSS_TRANSPORT_UBX_AT;
        ((uNetworkConfigurationGnss_t *) pGnssConfiguration)->devHandleAt = devHandleAt;
    }
#else
    (void) devHandleAt;
    (void) pGnssConfiguration;
#endif
}

// End of file
