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

/** @file
 * @brief Functions associated with a short-range device, i.e. one
 * supporting either BLE or Wifi or both.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_device.h"
#include "u_device_shared.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_ble_module_type.h"
#include "u_ble.h"

#include "u_device_shared_short_range.h"
#include "u_device_private_short_range.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise short-range.
int32_t uDevicePrivateShortRangeInit()
{
    int32_t errorCode = uShortRangeEdmStreamInit();
    if (errorCode == 0) {
        errorCode = uAtClientInit();
    }
    if (errorCode == 0) {
        errorCode = uBleInit();
    }
    if (errorCode == 0) {
        errorCode = uShortRangeInit();
    }
    return errorCode;
}

// Deinitialise short-range.
void uDevicePrivateShortRangeDeinit()
{
    uShortRangeDeinit();
    uBleDeinit();
    uShortRangeEdmStreamDeinit();
    uAtClientDeinit();
}

// Power up a short-range device that is external to the MCU,
// making it available for configuration.
int32_t uDevicePrivateShortRangeAdd(const uDeviceCfg_t *pDevCfg,
                                    uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uDeviceCfgUart_t *pCfgUart;
    const uDeviceCfgShortRange_t *pCfgSho;
    uShortRangeUartConfig_t uartCfg;

    if ((pDevCfg != NULL) &&
        (pDevCfg->transportType == U_DEVICE_TRANSPORT_TYPE_UART) &&
        (pDeviceHandle != NULL)) {
        pCfgUart = &(pDevCfg->transportCfg.cfgUart);
        pCfgSho = &(pDevCfg->deviceCfg.cfgSho);
        if (pCfgSho->version == 0) {
            uartCfg.uartPort = pCfgUart->uart;
            uartCfg.baudRate = pCfgUart->baudRate;
            uartCfg.pinTx = pCfgUart->pinTxd;
            uartCfg.pinRx = pCfgUart->pinRxd;
            uartCfg.pinCts = pCfgUart->pinCts;
            uartCfg.pinRts = pCfgUart->pinRts;
            // Open the short range UART, which creates pDeviceHandle
            errorCode = uShortRangeOpenUart(pCfgSho->moduleType, &uartCfg,
                                            false, pDeviceHandle);
        }
    }

    return errorCode;
}

// Power up a short-range device that is on-board the MCU, making
// it available for configuration.
int32_t uDevicePrivateShortRangeOpenCpuAdd(const uDeviceCfg_t *pDevCfg,
                                           uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const uDeviceCfgShortRange_t *pCfgSho;

    if ((pDevCfg != NULL) && (pDeviceHandle != NULL)) {
        pCfgSho = &(pDevCfg->deviceCfg.cfgSho);
        if ((pCfgSho->version == 0) &&
            ((uBleModuleType_t) pCfgSho->moduleType == U_BLE_MODULE_TYPE_INTERNAL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            *pDeviceHandle = (uDeviceHandle_t) pUDeviceCreateInstance(U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU);
            if (*pDeviceHandle != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Remove a short-range device that is external to the MCU.
int32_t uDevicePrivateShortRangeRemove(uDeviceHandle_t devHandle)
{
    uShortRangeClose(devHandle);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Remove a short-range  that is on-board the MCU.
int32_t uDevicePrivateShortRangeOpenCpuRemove(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (uDeviceGetDeviceType(devHandle) == (int32_t) U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) {
        uDeviceDestroyInstance(U_DEVICE_INSTANCE(devHandle));
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// End of file
