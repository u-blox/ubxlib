/*
 * Copyright 2019-2024 u-blox
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
 * @brief Tests to check use of the Zephyr device tree to dictate
 * device and network configuration.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

// Conditional on not being U_UCONNECT_GEN2 since NINA-W15/W13 are used
#ifndef U_UCONNECT_GEN2

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), memcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_board_cfg.h"

#include "u_device.h"
#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_gnss_module_type.h"
#include "u_short_range_module_type.h"

#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_gnss.h"
#include "u_network_config_wifi.h"

#include "u_ble_cfg.h"   // For uBleCfgRole_t
#include "u_cell_net.h"  // For uCellNetAuthenticationMode_t

#include "u_test_util_resource_check.h"

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
# include <zephyr/kernel.h>
# include <zephyr/device.h>
#else
# include <kernel.h>
# include <device.h>
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||    \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||        \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_ZEPHYR_PORT_BOARD_CFG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

# ifdef CONFIG_BOARD_UBX_EVKNINAB3_NRF52840

// Dummy keepGoingCallback function that uNetworkCfgCell_t can
// be pointed at.
static bool keepGoingCallback (uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return true;
}

// Set up a device configuration before testing.
static void setDeviceCfg(uDeviceCfg_t *pDevCfg,
                         uDeviceInstance_t *pInstance,
                         uDeviceType_t type,
                         const char *pCfgName)
{
    memset(pDevCfg, 0xA5, sizeof(*pDevCfg));
    memset(pInstance, 0xA5, sizeof(*pInstance));
    pDevCfg->deviceType = type;
    pDevCfg->pCfgName = pCfgName;
    uDeviceInitInstance(pInstance, type);
    pInstance->pCfgName = pCfgName;
}

// Set up a network configuration before testing.
static void setNetworkCfg(void *pNetCfg, uNetworkType_t type)
{
    switch (type) {
        case U_NETWORK_TYPE_BLE:
            memset(pNetCfg, 0xA5, sizeof(uNetworkCfgBle_t));
            break;
        case U_NETWORK_TYPE_CELL:
            memset(pNetCfg, 0xA5, sizeof(uNetworkCfgCell_t));
            // Since we allow the pKeepGoingCallback in a cellular
            // network configuration to be used unchanged, even
            // when a device tree override is in place, set it
            // to something valid here.
            ((uNetworkCfgCell_t *) pNetCfg)->pKeepGoingCallback = keepGoingCallback;
            break;
        case U_NETWORK_TYPE_GNSS:
            memset(pNetCfg, 0xA5, sizeof(uNetworkCfgGnss_t));
            break;
        case U_NETWORK_TYPE_WIFI:
            memset(pNetCfg, 0xA5, sizeof(uNetworkCfgWifi_t));
            break;
        default:
            break;
    }
}
# endif // #ifdef CONFIG_BOARD_UBX_EVKNINAB3_NRF52840

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

// This test only works with ubx_evkninab3_nrf52840.overlay
# ifdef CONFIG_BOARD_UBX_EVKNINAB3_NRF52840

/** Basic check that the uPortBoardCfgXxx() functions have an effect.
 * This test is designed to work when built with the file overlay
 * ubx_evkninab3_nrf52840.overlay.
 */
U_PORT_TEST_FUNCTION("[zephyrPortBoardCfg]", "zephyrPortBoardCfgBasic")
{
    int32_t resourceCount;
    uDeviceInstance_t instance = {0};
    uDeviceCfg_t deviceCfg = {0};
    uDeviceCfg_t deviceCfgSaved;
    uDeviceCfgCell_t *pCfgCell = &(deviceCfg.deviceCfg.cfgCell);
    uDeviceCfgGnss_t *pCfgGnss = &(deviceCfg.deviceCfg.cfgGnss);
    uDeviceCfgShortRange_t *pCfgShortRange = &(deviceCfg.deviceCfg.cfgSho);
    uDeviceCfgUart_t *pCfgUart = &(deviceCfg.transportCfg.cfgUart);
    uDeviceCfgI2c_t *pCfgI2c = &(deviceCfg.transportCfg.cfgI2c);
    uDeviceCfgSpi_t *pCfgSpi = &(deviceCfg.transportCfg.cfgSpi);
    uCommonSpiControllerDevice_t *pSpiDevice = &(deviceCfg.transportCfg.cfgSpi.device);
    uNetworkCfgBle_t networkCfgBle;
    uNetworkCfgCell_t networkCfgCell;
    uNetworkCfgGnss_t networkCfgGnss;
    uNetworkCfgWifi_t networkCfgWifi;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();
    uPortInit();

    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_NONE, NULL);
    U_TEST_PRINT_LINE("test not being able to determine the configuration"
                      " from the device tree...");
    // This should return an error since more than one
    // device type is included in the .overlay file and
    // we have not specified a device type in deviceCfg
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);

    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_CELL, NULL);
    U_TEST_PRINT_LINE("test not being able get cellular configuration from"
                      " the device tree...");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Wrong configuration name should not cause an error
    // and should not change the contents of the configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_CELL, "cfg-device-cellular-3");
    deviceCfgSaved = deviceCfg;
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    U_PORT_TEST_ASSERT(memcmp((void *) &deviceCfg, (void *) &deviceCfgSaved, sizeof(deviceCfg)) == 0);
    // Not setting the type should cause an error though
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_NONE, "cfg-device-cellular-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Let debug printing catch up
    uPortTaskBlock(100);
    U_TEST_PRINT_LINE("test getting cellular configuration from the"
                      " device tree...");
    // Set the first valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_CELL, "cfg-device-cellular-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgCell, U_NETWORK_TYPE_CELL);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_CELL, &networkCfgCell);
    setNetworkCfg(&networkCfgGnss, U_NETWORK_TYPE_GNSS);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_GNSS, &networkCfgGnss);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.version == 0);
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_CELL);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-cellular-0") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 0);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 57600);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->version == 0);
    U_PORT_TEST_ASSERT(pCfgCell->moduleType == U_CELL_MODULE_TYPE_SARA_R422);
    U_PORT_TEST_ASSERT(pCfgCell->pSimPinCode == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->pinEnablePower == 0);
    U_PORT_TEST_ASSERT(pCfgCell->pinPwrOn == 10);
    U_PORT_TEST_ASSERT(pCfgCell->pinVInt == 35);
    U_PORT_TEST_ASSERT(pCfgCell->pinDtrPowerSaving == 36);
    U_PORT_TEST_ASSERT(networkCfgCell.version == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.type == U_NETWORK_TYPE_CELL);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pApn, "blah") == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.timeoutSeconds == 30);
    U_PORT_TEST_ASSERT(networkCfgCell.pKeepGoingCallback == keepGoingCallback);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pUsername, "fred") == 0);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pPassword, "blogs") == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.authenticationMode == U_CELL_NET_AUTHENTICATION_MODE_PAP);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pMccMnc, "23410") == 0);
    U_PORT_TEST_ASSERT(networkCfgGnss.version == 0);
    U_PORT_TEST_ASSERT(networkCfgGnss.type == U_NETWORK_TYPE_GNSS);
    U_PORT_TEST_ASSERT(networkCfgGnss.moduleType == U_GNSS_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinPwr == -1);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinDataReady == -1);
    // Set the next valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_CELL, "cfg-device-cellular-1");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgCell, U_NETWORK_TYPE_CELL);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_CELL, &networkCfgCell);
    setNetworkCfg(&networkCfgGnss, U_NETWORK_TYPE_GNSS);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_GNSS, &networkCfgGnss);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.version == 0);
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_CELL);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-cellular-1") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 3);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 115200);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->version == 0);
    U_PORT_TEST_ASSERT(pCfgCell->moduleType == U_CELL_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(pCfgCell->pSimPinCode == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->pinEnablePower == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinPwrOn == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinVInt == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinDtrPowerSaving == -1);
    U_PORT_TEST_ASSERT(networkCfgCell.version == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.type == U_NETWORK_TYPE_CELL);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pApn, "blah") == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.timeoutSeconds == 30);
    U_PORT_TEST_ASSERT(networkCfgCell.pKeepGoingCallback == keepGoingCallback);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pUsername, "fred") == 0);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pPassword, "blogs") == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.authenticationMode == U_CELL_NET_AUTHENTICATION_MODE_PAP);
    U_PORT_TEST_ASSERT(strcmp(networkCfgCell.pMccMnc, "23410") == 0);
    U_PORT_TEST_ASSERT(networkCfgGnss.version == 0);
    U_PORT_TEST_ASSERT(networkCfgGnss.type == U_NETWORK_TYPE_GNSS);
    U_PORT_TEST_ASSERT(networkCfgGnss.moduleType == U_GNSS_MODULE_TYPE_M10);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinPwr == 9);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinDataReady == 32);
    // Set the final valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_CELL, "cfg-device-cellular-2");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgCell, U_NETWORK_TYPE_CELL);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_CELL, &networkCfgCell);
    setNetworkCfg(&networkCfgGnss, U_NETWORK_TYPE_GNSS);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_GNSS, &networkCfgGnss);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.version == 0);
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_CELL);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-cellular-2") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 2);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 115200);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->version == 0);
    U_PORT_TEST_ASSERT(pCfgCell->moduleType == U_CELL_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(pCfgCell->pSimPinCode == NULL);
    U_PORT_TEST_ASSERT(pCfgCell->pinEnablePower == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinPwrOn == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinVInt == -1);
    U_PORT_TEST_ASSERT(pCfgCell->pinDtrPowerSaving == -1);
    U_PORT_TEST_ASSERT(networkCfgCell.version == 0);
    U_PORT_TEST_ASSERT(networkCfgCell.type == U_NETWORK_TYPE_CELL);
    U_PORT_TEST_ASSERT(networkCfgCell.pApn == NULL);
    U_PORT_TEST_ASSERT(networkCfgCell.timeoutSeconds == -1);
    U_PORT_TEST_ASSERT(networkCfgCell.pKeepGoingCallback == keepGoingCallback);
    U_PORT_TEST_ASSERT(networkCfgCell.pUsername == NULL);
    U_PORT_TEST_ASSERT(networkCfgCell.pPassword == NULL);
    U_PORT_TEST_ASSERT(networkCfgCell.authenticationMode == U_CELL_NET_AUTHENTICATION_MODE_NOT_SET);
    U_PORT_TEST_ASSERT(networkCfgCell.pMccMnc == NULL);
    U_PORT_TEST_ASSERT(networkCfgGnss.version == 0);
    U_PORT_TEST_ASSERT(networkCfgGnss.type == U_NETWORK_TYPE_GNSS);
    U_PORT_TEST_ASSERT(networkCfgGnss.moduleType == U_GNSS_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinPwr == -1);
    U_PORT_TEST_ASSERT(networkCfgGnss.devicePinDataReady == -1);
    // Let debug printing catch up
    uPortTaskBlock(100);

    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_GNSS, NULL);
    U_TEST_PRINT_LINE("test not being able get GNSS configuration from"
                      " the device tree...");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Wrong configuration name should not cause an error
    // and should not change the contents of the configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_GNSS, "cfg-device-gnss-4");
    deviceCfgSaved = deviceCfg;
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    U_PORT_TEST_ASSERT(memcmp((void *) &deviceCfg, (void *) &deviceCfgSaved, sizeof(deviceCfg)) == 0);
    // Not setting the type should cause an error though
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_NONE, "cfg-device-gnss-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Let debug printing catch up
    uPortTaskBlock(100);
    U_TEST_PRINT_LINE("test getting GNSS configuration from the device"
                      " tree...");
    // Set the first valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_GNSS, "cfg-device-gnss-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.version == 0);
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_GNSS);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_I2C);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-gnss-0") == 0);
    U_PORT_TEST_ASSERT(pCfgI2c->version == 0);
    U_PORT_TEST_ASSERT(pCfgI2c->i2c == 0);
    U_PORT_TEST_ASSERT(pCfgI2c->pinSda == -1);
    U_PORT_TEST_ASSERT(pCfgI2c->pinScl == -1);
    U_PORT_TEST_ASSERT(pCfgI2c->clockHertz == 1000);
    U_PORT_TEST_ASSERT(pCfgI2c->alreadyOpen);
    U_PORT_TEST_ASSERT(pCfgI2c->maxSegmentSize == 256);
    U_PORT_TEST_ASSERT(pCfgGnss->version == 0);
    U_PORT_TEST_ASSERT(pCfgGnss->moduleType == U_GNSS_MODULE_TYPE_M9);
    U_PORT_TEST_ASSERT(pCfgGnss->i2cAddress == 0x43);
    U_PORT_TEST_ASSERT(pCfgGnss->pinEnablePower == 1);
    U_PORT_TEST_ASSERT(pCfgGnss->pinDataReady == 36);
    // Set the second valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_GNSS, "cfg-device-gnss-1");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.version == 0);
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_GNSS);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_SPI);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-gnss-1") == 0);
    U_PORT_TEST_ASSERT(pCfgSpi->version == 0);
    U_PORT_TEST_ASSERT(pCfgSpi->spi == 2);
    U_PORT_TEST_ASSERT(pCfgSpi->pinMosi == -1);
    U_PORT_TEST_ASSERT(pCfgSpi->pinMiso == -1);
    U_PORT_TEST_ASSERT(pCfgSpi->pinClk == -1);
    U_PORT_TEST_ASSERT(pCfgSpi->maxSegmentSize == 255);
    U_PORT_TEST_ASSERT(pSpiDevice->pinSelect == -1);
    U_PORT_TEST_ASSERT(pSpiDevice->indexSelect == 0);
    U_PORT_TEST_ASSERT(pSpiDevice->frequencyHertz == 2000000);
    U_PORT_TEST_ASSERT(pSpiDevice->mode == 2);
    U_PORT_TEST_ASSERT(pSpiDevice->wordSizeBytes == 3);
    U_PORT_TEST_ASSERT(pSpiDevice->lsbFirst);
    U_PORT_TEST_ASSERT(pSpiDevice->startOffsetNanoseconds == 5);
    U_PORT_TEST_ASSERT(pSpiDevice->stopOffsetNanoseconds == 10);
    U_PORT_TEST_ASSERT(pSpiDevice->sampleDelayNanoseconds == U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS);
    U_PORT_TEST_ASSERT(pSpiDevice->fillWord == U_COMMON_SPI_FILL_WORD);
    U_PORT_TEST_ASSERT(pCfgGnss->version == 0);
    U_PORT_TEST_ASSERT(pCfgGnss->moduleType == U_GNSS_MODULE_TYPE_M8);
    U_PORT_TEST_ASSERT(pCfgGnss->pinEnablePower == 2);
    U_PORT_TEST_ASSERT(pCfgGnss->pinDataReady == 37);
    // Set the final valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_GNSS, "cfg-device-gnss-2");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_GNSS);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART_2);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-gnss-2") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 4);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 230400);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgGnss->version == 0);
    U_PORT_TEST_ASSERT(pCfgGnss->moduleType == U_GNSS_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(pCfgGnss->pinEnablePower == -1);
    U_PORT_TEST_ASSERT(pCfgGnss->pinDataReady == -1);

    // Let debug printing catch up
    uPortTaskBlock(100);

    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU, NULL);
    U_TEST_PRINT_LINE("test not being able get short-range open CPU"
                      " configuration from the device tree...");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Wrong configuration name should not cause an error
    // and should not change the contents of the configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU, "cfg-device-short-range-3");
    deviceCfgSaved = deviceCfg;
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    U_PORT_TEST_ASSERT(memcmp((void *) &deviceCfg, (void *) &deviceCfgSaved, sizeof(deviceCfg)) == 0);
    // Not setting the type should cause an error though
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_NONE, "cfg-device-short-range-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    U_TEST_PRINT_LINE("test getting short-range open CPU configuration from "
                      " the device tree...");
    // Set a valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU, "cfg-device-short-range-0");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgBle, U_NETWORK_TYPE_BLE);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_BLE, &networkCfgBle);
    setNetworkCfg(&networkCfgWifi, U_NETWORK_TYPE_WIFI);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_WIFI, &networkCfgWifi);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_NONE);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-short-range-0") == 0);
    U_PORT_TEST_ASSERT(pCfgShortRange->version == 0);
    U_PORT_TEST_ASSERT(pCfgShortRange->moduleType == U_SHORT_RANGE_MODULE_TYPE_NINA_W13);
    U_PORT_TEST_ASSERT(networkCfgBle.version == 0);
    U_PORT_TEST_ASSERT(networkCfgBle.type == U_NETWORK_TYPE_BLE);
    U_PORT_TEST_ASSERT(networkCfgBle.role == U_BLE_CFG_ROLE_DISABLED);
    U_PORT_TEST_ASSERT(!networkCfgBle.spsServer);
    U_PORT_TEST_ASSERT(networkCfgWifi.version == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.type == U_NETWORK_TYPE_WIFI);
    U_PORT_TEST_ASSERT(networkCfgWifi.pSsid == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.authentication == 1);
    U_PORT_TEST_ASSERT(networkCfgWifi.pPassPhrase == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.pHostName == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.mode == U_WIFI_MODE_NONE);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApSssid == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.apAuthentication == 1);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApPassPhrase == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApIpAddress == NULL);

    // Let debug printing catch up
    uPortTaskBlock(100);

    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE, NULL);
    U_TEST_PRINT_LINE("test not being able get short-range configuration from"
                      " the device tree...");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    // Wrong configuration name should not cause an error
    // and should not change the contents of the configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE, "cfg-device-short-range-3");
    deviceCfgSaved = deviceCfg;
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    U_PORT_TEST_ASSERT(memcmp((void *) &deviceCfg, (void *) &deviceCfgSaved, sizeof(deviceCfg)) == 0);
    // Not setting the type should cause an error though
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_NONE, "cfg-device-short-range-1");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) < 0);
    U_TEST_PRINT_LINE("test getting short-range configuration from the"
                      " device tree...");
    // Set the first valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE, "cfg-device-short-range-1");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgBle, U_NETWORK_TYPE_BLE);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_BLE, &networkCfgBle);
    setNetworkCfg(&networkCfgWifi, U_NETWORK_TYPE_WIFI);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_WIFI, &networkCfgWifi);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_SHORT_RANGE);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-short-range-1") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 2);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 9600);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgShortRange->version == 0);
    U_PORT_TEST_ASSERT(pCfgShortRange->moduleType == U_SHORT_RANGE_MODULE_TYPE_NINA_W15);
    U_PORT_TEST_ASSERT(networkCfgBle.version == 0);
    U_PORT_TEST_ASSERT(networkCfgBle.type == U_NETWORK_TYPE_BLE);
    U_PORT_TEST_ASSERT(networkCfgBle.role == U_BLE_CFG_ROLE_PERIPHERAL);
    U_PORT_TEST_ASSERT(networkCfgBle.spsServer);
    U_PORT_TEST_ASSERT(networkCfgWifi.version == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.type == U_NETWORK_TYPE_WIFI);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pSsid, "my_home_ssid") == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.authentication == 2);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pPassPhrase, "my_pass_phrase") == 0);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pHostName, "my_host_name") == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.mode == U_WIFI_MODE_STA_AP);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pApSssid, "my_home_ap_ssid") == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.apAuthentication == 6);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pApPassPhrase, "my_ap_pass_phrase") == 0);
    U_PORT_TEST_ASSERT(strcmp(networkCfgWifi.pApIpAddress, "1.1.1.100") == 0);
    // Set the final valid configuration
    setDeviceCfg(&deviceCfg, &instance, U_DEVICE_TYPE_SHORT_RANGE, "cfg-device-short-range-2");
    U_PORT_TEST_ASSERT(uPortBoardCfgDevice(&deviceCfg) == 0);
    setNetworkCfg(&networkCfgBle, U_NETWORK_TYPE_BLE);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_BLE, &networkCfgBle);
    setNetworkCfg(&networkCfgWifi, U_NETWORK_TYPE_WIFI);
    uPortBoardCfgNetwork((uDeviceHandle_t *) &instance, U_NETWORK_TYPE_WIFI, &networkCfgWifi);
    // Check that the values are as inserted by
    // ubx_evkninab3_nrf52840.overlay
    U_PORT_TEST_ASSERT(deviceCfg.deviceType == U_DEVICE_TYPE_SHORT_RANGE);
    U_PORT_TEST_ASSERT(deviceCfg.transportType == U_DEVICE_TRANSPORT_TYPE_UART);
    U_PORT_TEST_ASSERT(strcmp(deviceCfg.pCfgName, "cfg-device-short-range-2") == 0);
    U_PORT_TEST_ASSERT(pCfgUart->version == 0);
    U_PORT_TEST_ASSERT(pCfgUart->uart == 2);
    U_PORT_TEST_ASSERT(pCfgUart->baudRate == 115200);
    U_PORT_TEST_ASSERT(pCfgUart->pinTxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRxd == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinCts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pinRts == -1);
    U_PORT_TEST_ASSERT(pCfgUart->pPrefix == NULL);
    U_PORT_TEST_ASSERT(pCfgShortRange->version == 0);
    U_PORT_TEST_ASSERT(pCfgShortRange->moduleType == U_SHORT_RANGE_MODULE_TYPE_ANY);
    U_PORT_TEST_ASSERT(networkCfgBle.version == 0);
    U_PORT_TEST_ASSERT(networkCfgBle.type == U_NETWORK_TYPE_BLE);
    U_PORT_TEST_ASSERT(networkCfgBle.role == U_BLE_CFG_ROLE_DISABLED);
    U_PORT_TEST_ASSERT(!networkCfgBle.spsServer);
    U_PORT_TEST_ASSERT(networkCfgWifi.version == 0);
    U_PORT_TEST_ASSERT(networkCfgWifi.type == U_NETWORK_TYPE_WIFI);
    U_PORT_TEST_ASSERT(networkCfgWifi.pSsid == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.authentication == 1);
    U_PORT_TEST_ASSERT(networkCfgWifi.pPassPhrase == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.pHostName == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.mode == U_WIFI_MODE_NONE);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApSssid == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.apAuthentication == 1);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApPassPhrase == NULL);
    U_PORT_TEST_ASSERT(networkCfgWifi.pApIpAddress == NULL);

    // Let debug printing catch up
    uPortTaskBlock(100);

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

# endif // #ifdef CONFIG_BOARD_UBX_EVKNINAB3_NRF52840

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[zephyrPortBoardCfg]", "zephyrPortBoardCfgCleanUp")
{
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

#endif // #ifndef U_UCONNECT_GEN2

// End of file
