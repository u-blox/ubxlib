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

#include "u_error_common.h"

#include "u_at_client.h"

//lint -efile(766, u_port.h) Suppress header file not used, which
// is true if U_CELL_TEST_CFG_APN is not defined
#include "u_port.h" // For U_PORT_STRINGIFY_QUOTED()
#include "u_port_debug.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#include "u_cell.h" // For U_CELL_UART_BAUD_RATE
#endif

#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
#include "u_short_range_module_type.h"
#include "u_short_range.h" // For U_SHORT_RANGE_UART_BAUD_RATE
#endif

#include "u_short_range_test_selector.h"
#if U_SHORT_RANGE_TEST_WIFI()
# include "u_wifi_test_cfg.h"
#endif

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

/** Cellular device configuration used during testing.
 */
static uDeviceCfg_t gDeviceCfgCell = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = U_CELL_TEST_CFG_SIM_PIN,
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT
        }
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_CELL_UART,
            .baudRate = U_CELL_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_CELL_TXD,
            .pinRxd = U_CFG_APP_PIN_CELL_RXD,
            .pinCts = U_CFG_APP_PIN_CELL_CTS,
            .pinRts = U_CFG_APP_PIN_CELL_RTS
        }
    }
#else
    .deviceType = U_DEVICE_TYPE_NONE
#endif
};

/** Cellular network configuration used during testing.
 */
static uNetworkCfgCell_t gNetworkCfgCell = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    .type = U_NETWORK_TYPE_CELL,
# ifdef U_CELL_TEST_CFG_APN
    .pApn = U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
# else
    .pApn = NULL,
# endif
    .timeoutSeconds = U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS
#else
    .type = U_NETWORK_TYPE_NONE
#endif
};

/** Short range device configuration used during testing.
 */
static uDeviceCfg_t gDeviceCfgShortRange = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
        }
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
        }
    }
#else
    .deviceType = U_DEVICE_TYPE_NONE
#endif
};

/** Wifi network configuration used during testing.
 */
static uNetworkCfgWifi_t gNetworkCfgWifi = {
    // Deliberately don't set version to test that the compiler zeroes the field
#if U_SHORT_RANGE_TEST_WIFI()
    .type = U_NETWORK_TYPE_WIFI,
    .pSsid = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
    .authentication = U_WIFI_TEST_CFG_AUTHENTICATION,
    .pPassPhrase = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE)
#else
    .type = U_NETWORK_TYPE_NONE
#endif
};

/** BLE network configuration used during testing.
 */
static uNetworkCfgBle_t gNetworkCfgBle = {
    // Deliberately don't set version to test that the compiler zeroes the field
#if U_SHORT_RANGE_TEST_BLE()
    .type = U_NETWORK_TYPE_BLE,
    .role = U_CFG_APP_SHORT_RANGE_ROLE,
    .spsServer = true
#else
    .type = U_NETWORK_TYPE_NONE
#endif
};

/** GNSS device configuration used during testing.
 */
static uDeviceCfg_t gDeviceCfgGnss = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg = {
        .cfgGnss = {
            .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
            .transportType = U_GNSS_TRANSPORT_NMEA_UART,
            .pinGnssEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
            .gnssAtPinPwr = U_CFG_APP_CELL_PIN_GNSS_POWER,
            .gnssAtPinDataReady = U_CFG_APP_CELL_PIN_GNSS_DATA_READY
        }
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_GNSS_UART,
            .baudRate = U_GNSS_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
            .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
            .pinCts = U_CFG_APP_PIN_GNSS_CTS,
            .pinRts = U_CFG_APP_PIN_GNSS_RTS
        }
    }
#else
    .deviceType = U_DEVICE_TYPE_NONE
#endif
};

/** GNSS network configuration used during testing.
 */
static uNetworkCfgGnss_t gNetworkCfgGnss = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    .type = U_NETWORK_TYPE_GNSS
#else
    .type = U_NETWORK_TYPE_NONE
#endif
};

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** All of the information for the underlying network
 * types as an array.  Order is important: CELL must come before
 * GNSS so that the cellular handle can be passed on to GNSS.
 */
uNetworkTestCfg_t gUNetworkTestCfg[] = {
    {
        NULL, U_NETWORK_TYPE_BLE, &gDeviceCfgShortRange,
        (void *) &gNetworkCfgBle
    },
    {
        NULL, U_NETWORK_TYPE_CELL, &gDeviceCfgCell,
        (void *) &gNetworkCfgCell
    },
    {
        NULL, U_NETWORK_TYPE_WIFI, &gDeviceCfgShortRange,
        (void *) &gNetworkCfgWifi
    },
    {
        NULL, U_NETWORK_TYPE_GNSS, &gDeviceCfgGnss,
        (void *) &gNetworkCfgGnss
    }
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

static bool validNetwork(int32_t index)
{
    bool isOk;
    switch (gUNetworkTestCfg[index].type) {
        case U_NETWORK_TYPE_BLE:
            isOk = gNetworkCfgBle.type != U_NETWORK_TYPE_NONE;
            break;
        case U_NETWORK_TYPE_CELL:
            isOk = gNetworkCfgCell.type != U_NETWORK_TYPE_NONE;
            break;
        case U_NETWORK_TYPE_WIFI:
            isOk = gNetworkCfgWifi.type != U_NETWORK_TYPE_NONE;
            break;
        case U_NETWORK_TYPE_GNSS:
            isOk = gNetworkCfgGnss.type != U_NETWORK_TYPE_NONE;
            break;
        default:
            isOk = false;
            break;
    }
    return isOk;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Update a GNSS network configuration for use with the CELL AT interface.
void uNetworkTestGnssAtCfg(uDeviceHandle_t devHandleAt, uDeviceCfg_t *pUDeviceCfg)
{
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    if ((devHandleAt != NULL) &&
        (pUDeviceCfg->deviceType == U_DEVICE_TYPE_GNSS)) {
        pUDeviceCfg->transportType = U_DEVICE_TRANSPORT_TYPE_NONE;
        pUDeviceCfg->deviceCfg.cfgGnss.transportType = U_GNSS_TRANSPORT_UBX_AT;
        pUDeviceCfg->deviceCfg.cfgGnss.devHandleAt = devHandleAt;
    }
#else
    (void)devHandleAt;
    (void)pUDeviceCfg;
#endif
}

bool uNetworkTestDeviceValidForOpen(int32_t index)
{
    bool isOk = gUNetworkTestCfg[index].pDeviceCfg->deviceType != U_DEVICE_TYPE_NONE &&
                validNetwork(index);
    if (isOk) {
        // Check if this device is already open
        for (size_t i = 0; isOk && i < gUNetworkTestCfgSize; i++) {
            if ((i != index) &&
                (gUNetworkTestCfg[index].pDeviceCfg == gUNetworkTestCfg[i].pDeviceCfg) &&
                gUNetworkTestCfg[i].devHandle != NULL) {
                // Was open
                isOk = false;
            }
        }
    }
    return isOk;
}

int32_t uNetworkTestClose(int32_t index)
{
    // Close the device at the specified index.
    // Find possible multiple use of this device and
    // mark them as closed as well.
    int32_t errorCode = U_ERROR_COMMON_SUCCESS;
    uPortLogF("uNetworkTestClose %d\n", index);
    if (gUNetworkTestCfg[index].devHandle != NULL) {
        errorCode = uDeviceClose(gUNetworkTestCfg[index].devHandle);
        if (errorCode == 0) {
            for (size_t i = 0; i < gUNetworkTestCfgSize; i++) {
                if ((i != index) &&
                    (gUNetworkTestCfg[index].pDeviceCfg == gUNetworkTestCfg[i].pDeviceCfg)) {
                    gUNetworkTestCfg[i].devHandle = NULL;
                }
            }
            gUNetworkTestCfg[index].devHandle = NULL;
        }
    }
    return errorCode;
}

int32_t uNetworkTestGetModuleType(int32_t index)
{
    int32_t type = 0;
    switch (gUNetworkTestCfg[index].pDeviceCfg->deviceType) {
        case U_DEVICE_TYPE_CELL:
            type = gUNetworkTestCfg[index].pDeviceCfg->deviceCfg.cfgCell.moduleType;
            break;

        case U_DEVICE_TYPE_GNSS:
            type = gUNetworkTestCfg[index].pDeviceCfg->deviceCfg.cfgGnss.moduleType;
            break;

        case U_DEVICE_TYPE_SHORT_RANGE:
            type = gUNetworkTestCfg[index].pDeviceCfg->deviceCfg.cfgSho.moduleType;
            break;

        default:
            type = (int32_t) U_SHORT_RANGE_MODULE_TYPE_INTERNAL;
            break;
    }
    return type;
}

// End of file
