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
 * @brief Functions for initializing a u-blox device (chip or module).
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"   //

#include "u_error_common.h"

#include "u_port_os.h"

#include "u_device.h"
#include "u_device_shared.h"

#include "u_location.h"
#include "u_location_shared.h"

#include "u_device_private.h"
#include "u_device_private_cell.h"
#include "u_device_private_gnss.h"
#include "u_device_private_short_range.h"

// Includes needed for uDeviceGetDefaults
#include "u_compiler.h"
#include "u_cfg_app_platform_specific.h"
#include "u_at_client.h"
#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_short_range.h"
#include "u_gnss_type.h"


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

/** This is a possible injection hook for the uDevice operations below.
 * It is declared weak so that it can be overridden by source which is
 * unique e.g. for a specific board. In this case, possible GPIO
 * operations currently not handled by ubxlib can be executed when
 * devices are opened or closed. It also enables filling in uDevice
 * default settings values. The parameters are just void so that it
 * can be used as a jack-of-all-trades.
 * The implementation here does nothing
 *
 * @param[in] pOperationType   string specifying actual operation
 * @param[in] pOperationParam1 operation specific parameter
 * @param[in] pOperationParam2 operation specific parameter
 * @return                     zero on success else a negative error code.
 */
U_WEAK
int32_t uDeviceCallback(const char *pOperationType,
                        void *pOperationParam1,
                        void *pOperationParam2)
{
    (void)pOperationType;
    (void)pOperationParam1;
    (void)pOperationParam2;
    return 0;
}

int32_t uDeviceInit()
{
    int32_t errorCode = uDeviceMutexCreate();

    if (errorCode == 0) {
        uDevicePrivateInit();
        errorCode = uDevicePrivateCellInit();
        if (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) {
            errorCode = 0;
        }
    }
    if (errorCode == 0) {
        errorCode = uDevicePrivateGnssInit();
        if (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) {
            errorCode = 0;
        }
    }
    if (errorCode == 0) {
        errorCode = uDevicePrivateShortRangeInit();
        if (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) {
            errorCode = 0;
        }
    }

    if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED)) {
        // Initialise the internally shared location API
        errorCode = uLocationSharedInit();
    }

    // Clean up on error
    if (errorCode != 0) {
        uLocationSharedDeinit();
        uDevicePrivateShortRangeDeinit();
        uDevicePrivateCellDeinit();
        uDevicePrivateGnssDeinit();
        uDeviceMutexDestroy();
    } else {
        errorCode = uDeviceCallback("init", NULL, NULL);
    }

    return errorCode;
}

int32_t uDeviceDeinit()
{
    uLocationSharedDeinit();
    uDevicePrivateShortRangeDeinit();
    uDevicePrivateGnssDeinit();
    uDevicePrivateCellDeinit();
    uDeviceMutexDestroy();
    uDeviceCallback("deinit", NULL, NULL);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

int32_t uDeviceGetDefaults(uDeviceType_t deviceType,
                           uDeviceCfg_t *pDeviceCfg)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pDeviceCfg != NULL) {
        errorCode = 0;
        pDeviceCfg->deviceType = deviceType;
        pDeviceCfg->transportType = U_DEVICE_TRANSPORT_TYPE_UART;
        switch (deviceType) {
            case U_DEVICE_TYPE_CELL:
                pDeviceCfg->deviceCfg.cfgCell.moduleType =
#ifdef U_CFG_CELL_MODULE_TYPE
                    U_CFG_CELL_MODULE_TYPE;
#else
                    -1;
#endif
                pDeviceCfg->deviceCfg.cfgCell.pinDtrPowerSaving = U_CFG_APP_PIN_CELL_DTR;
                pDeviceCfg->deviceCfg.cfgCell.pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER;
                pDeviceCfg->deviceCfg.cfgCell.pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON;
                pDeviceCfg->deviceCfg.cfgCell.pinVInt = U_CFG_APP_PIN_CELL_VINT;

                pDeviceCfg->transportCfg.cfgUart.uart = U_CFG_APP_CELL_UART;
                pDeviceCfg->transportCfg.cfgUart.baudRate = U_CELL_UART_BAUD_RATE;
                pDeviceCfg->transportCfg.cfgUart.pinCts = U_CFG_APP_PIN_CELL_CTS;
                pDeviceCfg->transportCfg.cfgUart.pinRts = U_CFG_APP_PIN_CELL_RTS;
                pDeviceCfg->transportCfg.cfgUart.pinRxd = U_CFG_APP_PIN_CELL_RXD;
                pDeviceCfg->transportCfg.cfgUart.pinTxd = U_CFG_APP_PIN_CELL_TXD;
                break;

            case U_DEVICE_TYPE_SHORT_RANGE:
                pDeviceCfg->deviceCfg.cfgSho.moduleType =
#ifdef U_CFG_SHORT_RANGE_MODULE_TYPE
                    U_CFG_SHORT_RANGE_MODULE_TYPE;
#else
                    -1;
#endif
                pDeviceCfg->transportCfg.cfgUart.uart = U_CFG_APP_SHORT_RANGE_UART;
                pDeviceCfg->transportCfg.cfgUart.baudRate = U_SHORT_RANGE_UART_BAUD_RATE;
                pDeviceCfg->transportCfg.cfgUart.pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS;
                pDeviceCfg->transportCfg.cfgUart.pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS;
                pDeviceCfg->transportCfg.cfgUart.pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD;
                pDeviceCfg->transportCfg.cfgUart.pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD;
                break;

            case U_DEVICE_TYPE_GNSS:
                pDeviceCfg->deviceCfg.cfgGnss.moduleType =
#ifdef U_CFG_GNSS_MODULE_TYPE
                    U_CFG_GNSS_MODULE_TYPE;
#else
                    -1;
#endif
                pDeviceCfg->deviceCfg.cfgGnss.pinDataReady =
#ifdef U_CFG_APP_PIN_GNSS_DATA_READY
                    U_CFG_APP_PIN_GNSS_DATA_READY;
#else
                    -1;
#endif
                pDeviceCfg->deviceCfg.cfgGnss.pinEnablePower =
#ifdef U_CFG_APP_PIN_GNSS_ENABLE_POWER
                    U_CFG_APP_PIN_GNSS_ENABLE_POWER;
#else
                    -1;
#endif

                pDeviceCfg->transportCfg.cfgUart.uart = U_CFG_APP_GNSS_UART;
                pDeviceCfg->transportCfg.cfgUart.baudRate = U_GNSS_UART_BAUD_RATE;
                pDeviceCfg->transportCfg.cfgUart.pinCts = U_CFG_APP_PIN_GNSS_CTS;
                pDeviceCfg->transportCfg.cfgUart.pinRts = U_CFG_APP_PIN_GNSS_RTS;
                pDeviceCfg->transportCfg.cfgUart.pinRxd = U_CFG_APP_PIN_GNSS_RXD;
                pDeviceCfg->transportCfg.cfgUart.pinTxd = U_CFG_APP_PIN_GNSS_TXD;
                break;

            default:
                errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
                break;
        }
        if (errorCode == 0) {
            errorCode = uDeviceCallback("def", (void *)pDeviceCfg, NULL);
        }
    }
    return errorCode;
}

int32_t uDeviceOpen(const uDeviceCfg_t *pDeviceCfg, uDeviceHandle_t *pDeviceHandle)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();

    if (errorCode == 0 && pDeviceCfg != NULL) {
        errorCode = uDeviceCallback("open", (void *)pDeviceCfg->deviceType, NULL);
    }

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pDeviceCfg != NULL) && (pDeviceCfg->version == 0) && (pDeviceHandle != NULL)) {
            switch (pDeviceCfg->deviceType) {
                case U_DEVICE_TYPE_CELL:
                    errorCode = uDevicePrivateCellAdd(pDeviceCfg, pDeviceHandle);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(*pDeviceHandle)->moduleType = pDeviceCfg->deviceCfg.cfgCell.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_GNSS:
                    errorCode = uDevicePrivateGnssAdd(pDeviceCfg, pDeviceHandle);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(*pDeviceHandle)->moduleType = pDeviceCfg->deviceCfg.cfgGnss.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE:
                    errorCode = uDevicePrivateShortRangeAdd(pDeviceCfg, pDeviceHandle);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(*pDeviceHandle)->moduleType = pDeviceCfg->deviceCfg.cfgSho.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                    errorCode = uDevicePrivateShortRangeOpenCpuAdd(pDeviceCfg, pDeviceHandle);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(*pDeviceHandle)->moduleType = pDeviceCfg->deviceCfg.cfgSho.moduleType;
                    }
                    break;
                default:
                    break;
            }
        }

        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

int32_t uDeviceClose(uDeviceHandle_t devHandle, bool powerOff)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();

    if (errorCode == 0) {
        switch (uDeviceGetDeviceType(devHandle)) {
            case U_DEVICE_TYPE_CELL:
                errorCode = uDevicePrivateCellRemove(devHandle, powerOff);
                break;
            case U_DEVICE_TYPE_GNSS:
                errorCode = uDevicePrivateGnssRemove(devHandle, powerOff);
                break;
            case U_DEVICE_TYPE_SHORT_RANGE:
                if (!powerOff) {
                    errorCode = uDevicePrivateShortRangeRemove(devHandle);
                }
                break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                if (!powerOff) {
                    errorCode = uDevicePrivateShortRangeOpenCpuRemove(devHandle);
                }
                break;
            default:
                errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
                break;
        }

        if (errorCode == 0) {
            errorCode = uDeviceCallback("close", (void *)(U_DEVICE_INSTANCE(devHandle)->deviceType),
                                        (void *)powerOff);
        }

        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

// End of file
