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

/** @file
 * @brief Internal high-level API for initializing an u-blox device (chip or module).
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include <stddef.h>    // NULL, size_t etc.
#include <stdint.h>    // int32_t etc.
#include <stdbool.h>   // bool.
#include <stdlib.h>
#include <string.h>

#include "u_cfg_sw.h"
#include "u_compiler.h" // for U_INLINE
#include "u_error_common.h"

#include "u_port.h"

#include "u_at_client.h"

#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_network_private_cell.h"

#include "u_network_config_gnss.h"
#include "u_network_private_gnss.h"

#include "u_network_private_short_range.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_network_config_ble.h"
#include "u_network_private_ble.h"

#include "u_port_debug.h"

#include "u_device_internal.h"

/*

 ***************** PLEASE NOTE *****************

 The current implementation just calls the old interfaces for now.
 This is done in order to facilitate the conversion of all the test
 programs to the new APIs

*/

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_DEVICE_MAGIC_NUMBER 0x0EA7BEEF

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static int32_t uDeviceCellAdd(const uDeviceConfig_t *pDevCfg, uDeviceHandle_t *pUDeviceHandle)
{
    if (pDevCfg->transport != U_DEVICE_TRANSPORT_TYPE_UART) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    uNetworkConfigurationCell_t cellCfg;
    cellCfg.type = U_NETWORK_TYPE_CELL;
    cellCfg.moduleType = pDevCfg->deviceCfg.cellCfg.moduleType;

    cellCfg.uart = pDevCfg->transportCfg.uartCfg.uart;
    cellCfg.pinTxd = pDevCfg->transportCfg.uartCfg.pinTxd;
    cellCfg.pinRxd = pDevCfg->transportCfg.uartCfg.pinRxd;
    cellCfg.pinCts = pDevCfg->transportCfg.uartCfg.pinCts;
    cellCfg.pinRts = pDevCfg->transportCfg.uartCfg.pinRts;

    cellCfg.pinEnablePower = pDevCfg->deviceCfg.cellCfg.pinEnablePower;
    cellCfg.pinPwrOn = pDevCfg->deviceCfg.cellCfg.pinPwrOn;
    cellCfg.pinVInt = pDevCfg->deviceCfg.cellCfg.pinVInt;

    return uNetworkAddCell(&cellCfg, pUDeviceHandle);
}

static int32_t uDeviceGnssAdd(const uDeviceConfig_t *pDevCfg, uDeviceHandle_t *pUDeviceHandle)
{
    if (pDevCfg->transport != U_DEVICE_TRANSPORT_TYPE_UART) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    uNetworkConfigurationGnss_t gnssCfg;
    gnssCfg.type = U_NETWORK_TYPE_GNSS;
    gnssCfg.moduleType = pDevCfg->deviceCfg.gnssCfg.moduleType;
    gnssCfg.transportType = pDevCfg->deviceCfg.gnssCfg.transportType;

    gnssCfg.uart = pDevCfg->transportCfg.uartCfg.uart;
    gnssCfg.pinTxd = pDevCfg->transportCfg.uartCfg.pinTxd;
    gnssCfg.pinRxd = pDevCfg->transportCfg.uartCfg.pinRxd;
    gnssCfg.pinCts = pDevCfg->transportCfg.uartCfg.pinCts;
    gnssCfg.pinRts = pDevCfg->transportCfg.uartCfg.pinRts;

    gnssCfg.pinGnssEnablePower = pDevCfg->deviceCfg.gnssCfg.pinGnssEnablePower;
    gnssCfg.transportType = pDevCfg->deviceCfg.gnssCfg.transportType;
    gnssCfg.gnssAtPinPwr = pDevCfg->deviceCfg.gnssCfg.gnssAtPinPwr;
    gnssCfg.gnssAtPinDataReady = pDevCfg->deviceCfg.gnssCfg.gnssAtPinDataReady;

    return uNetworkAddGnss(&gnssCfg, pUDeviceHandle);
}

static int32_t uDeviceShortRangeAdd(const uDeviceConfig_t *pDevCfg,
                                    uDeviceHandle_t *pUDeviceHandle)
{
    if (pDevCfg->transport != U_DEVICE_TRANSPORT_TYPE_UART) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    uShortRangeUartConfig_t uartCfg;
    uartCfg.uartPort = pDevCfg->transportCfg.uartCfg.uart;
    uartCfg.baudRate = pDevCfg->transportCfg.uartCfg.baudRate;
    uartCfg.pinTx = pDevCfg->transportCfg.uartCfg.pinTxd;
    uartCfg.pinRx = pDevCfg->transportCfg.uartCfg.pinRxd;
    uartCfg.pinCts = pDevCfg->transportCfg.uartCfg.pinCts;
    uartCfg.pinRts = pDevCfg->transportCfg.uartCfg.pinRts;

    return uShortRangeOpenUart(pDevCfg->deviceCfg.shoCfg.module, &uartCfg, false, pUDeviceHandle);
}

static int32_t uDeviceShortRangeOpenCpuAdd(const uDeviceConfig_t *pDevCfg,
                                           uDeviceHandle_t *pUDeviceHandle)
{
    // Open CPU == BLE for now
    (void)pDevCfg;
    uNetworkConfigurationBle_t bleCfg;
    bleCfg.type = U_NETWORK_TYPE_BLE;
    bleCfg.module = U_SHORT_RANGE_MODULE_TYPE_INTERNAL;
    return uNetworkAddBle(&bleCfg, pUDeviceHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

uDeviceInstance_t *pUDeviceCreateInstance(uDeviceType_t type)
{
    uDeviceInstance_t *pInstance;
    pInstance = (uDeviceInstance_t *)malloc(sizeof(uDeviceInstance_t));
    if (pInstance) {
        uDeviceInitInstance(pInstance, type);
    }
    return pInstance;
}

void uDeviceDestroyInstance(uDeviceInstance_t *pInstance)
{
    if (uDeviceIsValidInstance(pInstance)) {
        // Invalidate the instance
        pInstance->magic = 0;
        free(pInstance);
    } else {
        uPortLog("U_DEVICE: Warning: Trying to destroy an already destroyed instance");
    }
}

U_INLINE void uDeviceInitInstance(uDeviceInstance_t *pInstance,
                                  uDeviceType_t type)
{
    //lint -esym(429, pInstance) Suppress "custodial pointer not been freed or returned"
    memset(pInstance, 0, sizeof(uDeviceInstance_t));
    pInstance->magic = U_DEVICE_MAGIC_NUMBER;
    pInstance->deviceType = type;
}

U_INLINE bool uDeviceIsValidInstance(const uDeviceInstance_t *pInstance)
{
    return pInstance && (pInstance->magic == U_DEVICE_MAGIC_NUMBER);
}

U_INLINE int32_t uDeviceGetInstance(uDeviceHandle_t devHandle,
                                    uDeviceInstance_t **ppInstance)
{
    *ppInstance = U_DEVICE_INSTANCE(devHandle);
    bool isValid = uDeviceIsValidInstance(*ppInstance);
    return isValid ? (int32_t)U_ERROR_COMMON_SUCCESS : (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
}

U_INLINE int32_t uDeviceGetDeviceType(uDeviceHandle_t devHandle)
{
    uDeviceInstance_t *pInstance;
    int32_t returnCode = uDeviceGetInstance(devHandle, &pInstance);
    if (returnCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
        returnCode = (int32_t)pInstance->deviceType;
    }
    return returnCode;
}

int32_t uDeviceOpen(const uDeviceConfig_t *pDevCfg, uDeviceHandle_t *pUDeviceHandle)
{
    if (!pDevCfg || !pUDeviceHandle) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    int32_t returnCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    switch (pDevCfg->deviceType) {
        case U_DEVICE_TYPE_CELL:
            returnCode = uDeviceCellAdd(pDevCfg, pUDeviceHandle);
            break;
        case U_DEVICE_TYPE_GNSS:
            returnCode = uDeviceGnssAdd(pDevCfg, pUDeviceHandle);
            break;
        case U_DEVICE_TYPE_SHORT_RANGE:
            returnCode = uDeviceShortRangeAdd(pDevCfg, pUDeviceHandle);
            if (returnCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                uDeviceInstance_t *pUDeviceInstance = U_DEVICE_INSTANCE(*pUDeviceHandle);
                pUDeviceInstance->module = pDevCfg->deviceCfg.shoCfg.module;
            }
            break;
        case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
            returnCode = uDeviceShortRangeOpenCpuAdd(pDevCfg, pUDeviceHandle);
            break;
        default:
            break;
    }
    return returnCode;
}

int32_t uDeviceClose(uDeviceHandle_t pUDeviceHandle)
{
    int32_t returnCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    switch (uDeviceGetDeviceType(pUDeviceHandle)) {
        case U_DEVICE_TYPE_CELL:
            returnCode = uNetworkRemoveCell(pUDeviceHandle);
            break;
        case U_DEVICE_TYPE_GNSS:
            returnCode = uNetworkRemoveGnss(pUDeviceHandle);
            break;
        case U_DEVICE_TYPE_SHORT_RANGE:
            uShortRangeClose(pUDeviceHandle);
            returnCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            break;
        case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
            returnCode = uNetworkRemoveBle(pUDeviceHandle);
            break;
        default:
            break;
    }
    return returnCode;
}

// End of file
