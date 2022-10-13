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

#include "stdlib.h"   // malloc()/free()
#include "stddef.h"   // NULL, size_t etc.
#include "stdint.h"   // int32_t etc.
#include "stdbool.h"
#include "string.h"   // memset()

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

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_NETWORK_TEST_SHARED: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A network configuration.
 */
typedef struct {
    uNetworkType_t type;
    const void *pCfg;
} uNetworkTestNetwork_t;

/** Network configurations with the underlying device configuration
 * plus room for the device handle to be stored.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    const uDeviceCfg_t *pCfg;
    uNetworkTestNetwork_t network[U_NETWORK_TEST_NETWORKS_MAX_NUM];
} uNetworkTestDevice_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Cellular device configuration used during testing.
 */
static const uDeviceCfg_t gDeviceCfgCell = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = U_CELL_TEST_CFG_SIM_PIN,
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT,
            .pinDtrPowerSaving = U_CFG_APP_PIN_CELL_DTR
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
static const uNetworkCfgCell_t gNetworkCfgCell = {
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
static const uDeviceCfg_t gDeviceCfgShortRange = {
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
static const uNetworkCfgWifi_t gNetworkCfgWifi = {
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
static const uNetworkCfgBle_t gNetworkCfgBle = {
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
static const uDeviceCfg_t gDeviceCfgGnss = {
    // Deliberately don't set version to test that the compiler zeroes the field
#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0))
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg = {
        .cfgGnss = {
            .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
            .pinEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
            .pinDataReady = -1
        }
    },
# if (U_CFG_APP_GNSS_I2C >= 0)
    .transportType = U_DEVICE_TRANSPORT_TYPE_I2C,
    .transportCfg = {
        .cfgI2c = {
            .i2c = U_CFG_APP_GNSS_I2C,
            .pinSda = U_CFG_APP_PIN_GNSS_SDA,
            .pinScl = U_CFG_APP_PIN_GNSS_SCL
        }
    }
# else
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
# endif
#else
    .deviceType = U_DEVICE_TYPE_NONE
#endif
};

/** GNSS network configuration used during testing.
 */
static const uNetworkCfgGnss_t gNetworkCfgGnss = {
    // Deliberately don't set version to test that the compiler zeroes the field
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    .type = U_NETWORK_TYPE_GNSS,
    .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
    .devicePinPwr = U_CFG_APP_CELL_PIN_GNSS_POWER,
    .devicePinDataReady = U_CFG_APP_CELL_PIN_GNSS_DATA_READY
#else
    .type = U_NETWORK_TYPE_NONE
#endif
};

/** All of the information for the underlying network
 * types as an array.
 */
static uNetworkTestDevice_t gUNetworkTest[] = {
    {
        .devHandle = NULL,
        .pCfg = &gDeviceCfgShortRange,
        .network =
        {
            {.type = U_NETWORK_TYPE_BLE, .pCfg = (const void *) &gNetworkCfgBle},
            {.type = U_NETWORK_TYPE_WIFI, .pCfg = (const void *) &gNetworkCfgWifi}
        }
    },
    {
        .devHandle = NULL,
        .pCfg = &gDeviceCfgCell,
        .network =
        {
            {.type = U_NETWORK_TYPE_CELL, .pCfg = (const void *) &gNetworkCfgCell},
#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0) && (U_CFG_APP_GNSS_I2C < 0)
            {.type = U_NETWORK_TYPE_GNSS, .pCfg = (const void *) &gNetworkCfgGnss}
#else
            {.type = U_NETWORK_TYPE_NONE, .pCfg = NULL}
#endif
        }
    },
    {
        .devHandle = NULL,
        .pCfg = &gDeviceCfgGnss,
        .network =
        {
            {.type = U_NETWORK_TYPE_GNSS, .pCfg = (const void *) &gNetworkCfgGnss},
            {.type = U_NETWORK_TYPE_NONE, .pCfg = NULL}
        }
    }
};

/** The root for a list of test networks.
 */
static uNetworkTestList_t *gpNetworkTestList = NULL;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

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

/** Return a name for a device type.
 */
//lint -esym(843, gpUNetworkTestDeviceTypeName) Suppress could be declared
// as const: this may be used in position independent code
// and hence can't be const
const char *gpUNetworkTestDeviceTypeName[] = {"none",               // U_DEVICE_TYPE_NONE
                                              "cellular",           // U_DEVICE_TYPE_CELL
                                              "GNSS",               // U_DEVICE_TYPE_GNSS
                                              "short range",        // U_DEVICE_TYPE_SHORT_RANGE
                                              "short range OpenCPU" // U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU
                                             };
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Return the network type from the uNetworkTestNetwork_t structure.
static uNetworkType_t getNetworkType(uNetworkTestNetwork_t *pNetwork)
{
    uNetworkType_t networkType = U_NETWORK_TYPE_NONE;

    // Note: can't rely on the type from the pNetwork
    // structure as conditional compilation may mean that there
    // isn't actually a network of that type; need to go find
    // the type in the config structure itself, which will
    // reflect conditional compilation correctly
    if (pNetwork->pCfg != NULL) {
        switch (pNetwork->type) {
            case U_NETWORK_TYPE_BLE:
                networkType = ((uNetworkCfgBle_t *) pNetwork->pCfg)->type;
                break;
            case U_NETWORK_TYPE_CELL:
                networkType = ((uNetworkCfgCell_t *) pNetwork->pCfg)->type;
                break;
            case U_NETWORK_TYPE_WIFI:
                networkType = ((uNetworkCfgWifi_t *) pNetwork->pCfg)->type;
                break;
            case U_NETWORK_TYPE_GNSS:
                networkType = ((uNetworkCfgGnss_t *) pNetwork->pCfg)->type;
                break;
            default:
                break;
        }
    }

    return networkType;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Allocate a list of devices/networks to operate on for a test.
uNetworkTestList_t *pUNetworkTestListAlloc(uNetworkTestValidFunction_t pValidFunction)
{
    uNetworkTestList_t *pList;
    uNetworkTestDevice_t *pDevice = gUNetworkTest;
    uNetworkTestNetwork_t *pNetwork;
    uNetworkType_t networkType;
    int32_t moduleType;

    if (gpNetworkTestList != NULL) {
        // Make sure any previous list is free'ed.
        uNetworkTestListFree();
    }
    // For each device that is populated...
    for (size_t x = 0; x < sizeof(gUNetworkTest) / sizeof(gUNetworkTest[0]); x++, pDevice++) {
        if ((pDevice->pCfg != NULL) && (pDevice->pCfg->deviceType != U_DEVICE_TYPE_NONE)) {
            moduleType = -1;
            switch (pDevice->pCfg->deviceType) {
                case U_DEVICE_TYPE_CELL:
                    moduleType = (int32_t) pDevice->pCfg->deviceCfg.cfgCell.moduleType;
                    break;
                case U_DEVICE_TYPE_GNSS:
                    moduleType = (int32_t) pDevice->pCfg->deviceCfg.cfgGnss.moduleType;
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE:
                case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                    moduleType = (int32_t) pDevice->pCfg->deviceCfg.cfgSho.moduleType;
                    break;
                default:
                    break;
            }
            if (moduleType >= 0) {
                pNetwork = pDevice->network;
                // For each network that is populated on that device...
                for (size_t y = 0; y < sizeof(gUNetworkTest[x].network) /
                     sizeof(gUNetworkTest[x].network[0]); y++, pNetwork++) {
                    networkType = getNetworkType(pNetwork);
                    if ((networkType != U_NETWORK_TYPE_NONE) &&
                        ((pValidFunction == NULL) ||
                         pValidFunction(pDevice->pCfg->deviceType,
                                        networkType,
                                        moduleType))) {
                        // The device/network/module is valid for the test,
                        // so allocate memory for it and add it to the
                        // front of the list
                        pList = (uNetworkTestList_t *) malloc(sizeof(*pList));
                        if (pList != NULL) {
                            memset(pList, 0, sizeof(*pList));
                            pList->pDevHandle = &(pDevice->devHandle);
                            pList->pDeviceCfg = pDevice->pCfg;
                            pList->networkType = networkType;
                            pList->pNetworkCfg = pNetwork->pCfg;
                            pList->pNext = gpNetworkTestList;
                            gpNetworkTestList = pList;
                        }
                    }
                }
            }
        }
    }

    return gpNetworkTestList;
}

// Free a list of networks that was created with
// pUNetworkTestListAlloc().
void uNetworkTestListFree(void)
{
    uNetworkTestList_t *pTmp;

    while (gpNetworkTestList != NULL) {
        pTmp = gpNetworkTestList->pNext;
        free(gpNetworkTestList);
        gpNetworkTestList = pTmp;
    }
}

// Clean up the devices.
void uNetworkTestCleanUp(void)
{
    bool closeDevice;
    uNetworkTestDevice_t *pDevice = gUNetworkTest;
    uNetworkTestNetwork_t *pNetwork;
    uNetworkType_t networkType;

    U_TEST_PRINT_LINE("running cleanup...");
    for (size_t x = 0; x < sizeof(gUNetworkTest) / sizeof(gUNetworkTest[0]); x++, pDevice++) {
        if (pDevice->devHandle != NULL) {
            // Bring down the networks; it is always safe to do this,
            // even if they were never brought up
            closeDevice = true;
            pNetwork = pDevice->network;
            for (size_t y = 0; y < sizeof(gUNetworkTest[x].network) /
                 sizeof(gUNetworkTest[x].network[0]); y++, pNetwork++) {
                networkType = getNetworkType(pNetwork);
                if ((networkType != U_NETWORK_TYPE_NONE) &&
                    (uNetworkInterfaceDown(pDevice->devHandle, networkType) != 0)) {
                    closeDevice = false;
                    U_TEST_PRINT_LINE("*** WARNING *** can't bring down %s network"
                                      " on %s device.",
                                      gpUNetworkTestTypeName[pNetwork->type],
                                      gpUNetworkTestDeviceTypeName[pDevice->pCfg->deviceType]);
                }
            }
            // Close the device, without powering it off
            if (closeDevice) {
                if (uDeviceClose(pDevice->devHandle, false) == 0) {
                    pDevice->devHandle = NULL;
                } else {
                    U_TEST_PRINT_LINE("*** WARNING *** unable to close %s device.",
                                      gpUNetworkTestDeviceTypeName[pDevice->pCfg->deviceType]);
                }
            } else {
                U_TEST_PRINT_LINE("not closing %s device.",
                                  gpUNetworkTestDeviceTypeName[pDevice->pCfg->deviceType]);
            }
        }
    }
    U_TEST_PRINT_LINE("cleanup complete.");
}

// Return true if the configuration supports sockets.
bool uNetworkTestHasSock(uDeviceType_t deviceType,
                         uNetworkType_t networkType,
                         int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_CELL) ||
           (networkType == U_NETWORK_TYPE_WIFI);
}

// Return true if the configuration supports secure sockets.
bool uNetworkTestHasSecureSock(uDeviceType_t deviceType,
                               uNetworkType_t networkType,
                               int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_CELL);
}

// Return true if the combination supports u-blox security.
bool uNetworkTestHasSecurity(uDeviceType_t deviceType,
                             uNetworkType_t networkType,
                             int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_CELL);
}

// Return true if the configuration supports MQTT.
bool uNetworkTestHasMqtt(uDeviceType_t deviceType,
                         uNetworkType_t networkType,
                         int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return ((networkType == U_NETWORK_TYPE_CELL) ||
            (networkType == U_NETWORK_TYPE_WIFI));
}

// Return true if the configuration supports MQTT-SN.
bool uNetworkTestHasMqttSn(uDeviceType_t deviceType,
                           uNetworkType_t networkType,
                           int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_CELL);
}

// Return true if the configuration supports HTTP.
bool uNetworkTestHasHttp(uDeviceType_t deviceType,
                         uNetworkType_t networkType,
                         int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    // TODO: add Wi-Fi
    // A couple of SARA-R5 cellular modules on the test system
    // fail this test intermittently, no obvious reason why,
    // hence allowing the option of disabling the test for
    // cellular for now.
#ifndef U_HTTP_CLIENT_CELL_DISABLE_TEST
    return (networkType == U_NETWORK_TYPE_CELL);
#else
    return false;
#endif
}

// Return true if the configuration supports credential storage.
bool uNetworkTestHasCredentialStorage(uDeviceType_t deviceType,
                                      uNetworkType_t networkType,
                                      int32_t moduleType)
{
    (void) deviceType;
    return ((networkType == U_NETWORK_TYPE_CELL) ||
            (networkType == U_NETWORK_TYPE_WIFI) ||
            ((networkType == U_NETWORK_TYPE_BLE) &&
             (moduleType != (int32_t) U_SHORT_RANGE_MODULE_TYPE_INTERNAL)));
}

// Return true if the configuration is short-range.
bool uNetworkTestIsDeviceShortRange(uDeviceType_t deviceType,
                                    uNetworkType_t networkType,
                                    int32_t moduleType)
{
    (void) networkType;
    (void) moduleType;
    return (deviceType == U_DEVICE_TYPE_SHORT_RANGE) ||
           (deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU);
}

// Return true if the configuration is cellular.
bool uNetworkTestIsDeviceCell(uDeviceType_t deviceType,
                              uNetworkType_t networkType,
                              int32_t moduleType)
{
    (void) networkType;
    (void) moduleType;
    return (deviceType == U_DEVICE_TYPE_CELL);
}

// Return true if the configuration is a BLE one.
bool uNetworkTestIsBle(uDeviceType_t deviceType,
                       uNetworkType_t networkType,
                       int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_BLE);
}

// Return true if uNetworkSetStatusCallback() is supported.
bool uNetworkTestHasStatusCallback(uDeviceType_t deviceType,
                                   uNetworkType_t networkType,
                                   int32_t moduleType)
{
    (void) deviceType;
    (void) moduleType;
    return (networkType == U_NETWORK_TYPE_BLE) || (networkType == U_NETWORK_TYPE_WIFI) ||
           (networkType == U_NETWORK_TYPE_CELL);
}

// End of file
