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
#include "u_port_board_cfg.h"

#include "u_device.h"
#include "u_device_shared.h"

#include "u_location.h"
#include "u_location_shared.h"

#include "u_network_shared.h"

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

    // Workaround for Espressif linker missing out files that
    // only contain functions which also have weak alternatives
    // (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
    // Basically any file that might end up containing only functions
    // that also have WEAK linked counterparts will be lost, so we need
    // to add a dummy function in those files and call it from somewhere
    // that will always be present in the build, which for cellular we
    // choose to be here
    uDevicePrivateCellLink();
    uDevicePrivateGnssLink();
    uDevicePrivateShortRangeLink();

    if (errorCode == 0) {
        uDevicePrivateInit();
        errorCode = uDevicePrivateCellInit();
        if ((errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
            (errorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
            errorCode = 0;
        }
    }
    if (errorCode == 0) {
        errorCode = uDevicePrivateGnssInit();
        if ((errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
            (errorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
            errorCode = 0;
        }
    }
    if (errorCode == 0) {
        errorCode = uDevicePrivateShortRangeInit();
        if ((errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
            (errorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
            errorCode = 0;
        }
    }

    if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
        (errorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
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
                pDeviceCfg->transportCfg.cfgUart.pPrefix = NULL; // Relevant for Linux only
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
                pDeviceCfg->transportCfg.cfgUart.pPrefix = NULL; // Relevant for Linux only
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
                pDeviceCfg->transportCfg.cfgUart.pPrefix = NULL; // Relevant for Linux only
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
    uDeviceCfg_t localDeviceCfg = {0};
    // Lock the API
    int32_t errorCode = uDeviceLock();

    uDeviceHandle_t deviceHandleCandidate = NULL;

    if (errorCode == 0 && pDeviceCfg != NULL) {
        errorCode = uDeviceCallback("open", (void *)pDeviceCfg->deviceType, NULL);
        localDeviceCfg = *pDeviceCfg;
    }

    if (errorCode == 0) {
        // Allow the device configuration from the board
        // configuration of the platform to override what
        // we were given; only used by Zephyr
        errorCode = uPortBoardCfgDevice(&localDeviceCfg);
    }

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pDeviceHandle != NULL) && (localDeviceCfg.version == 0)) {
            switch (localDeviceCfg.deviceType) {
                case U_DEVICE_TYPE_CELL:
                    errorCode = uDevicePrivateCellAdd(&localDeviceCfg, &deviceHandleCandidate);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(deviceHandleCandidate)->moduleType = localDeviceCfg.deviceCfg.cfgCell.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_GNSS:
                    errorCode = uDevicePrivateGnssAdd(&localDeviceCfg, &deviceHandleCandidate);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(deviceHandleCandidate)->moduleType = localDeviceCfg.deviceCfg.cfgGnss.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE:
                    errorCode = uDevicePrivateShortRangeAdd(&localDeviceCfg, &deviceHandleCandidate);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(deviceHandleCandidate)->moduleType = localDeviceCfg.deviceCfg.cfgSho.moduleType;
                    }
                    break;
                case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                    errorCode = uDevicePrivateShortRangeOpenCpuAdd(&localDeviceCfg, &deviceHandleCandidate);
                    if (errorCode == 0) {
                        U_DEVICE_INSTANCE(deviceHandleCandidate)->moduleType = localDeviceCfg.deviceCfg.cfgSho.moduleType;
                    }
                    break;
                default:
                    break;
            }
            if (errorCode == 0) {
                U_DEVICE_INSTANCE(deviceHandleCandidate)->pCfgName = localDeviceCfg.pCfgName;
            }
        }

        // ...and done
        uDeviceUnlock();
    }

    if (errorCode == 0) {
        *pDeviceHandle = deviceHandleCandidate;
    }

    return errorCode;
}

int32_t uDeviceClose(uDeviceHandle_t devHandle, bool powerOff)
{
    int32_t errorCode;
    uDeviceType_t deviceType;

    // Lock the API
    errorCode = uDeviceLock();
    if (errorCode == 0) {
        deviceType = uDeviceGetDeviceType(devHandle);
        switch (deviceType) {
            case U_DEVICE_TYPE_CELL:
                uNetworkCfgFree(devHandle);
                errorCode = uDevicePrivateCellRemove(devHandle, powerOff);
                break;
            case U_DEVICE_TYPE_GNSS:
                uNetworkCfgFree(devHandle);
                errorCode = uDevicePrivateGnssRemove(devHandle, powerOff);
                break;
            case U_DEVICE_TYPE_SHORT_RANGE:
                if (!powerOff) {
                    uNetworkCfgFree(devHandle);
                    errorCode = uDevicePrivateShortRangeRemove(devHandle);
                }
                break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                if (!powerOff) {
                    uNetworkCfgFree(devHandle);
                    errorCode = uDevicePrivateShortRangeOpenCpuRemove(devHandle);
                }
                break;
            default:
                errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
                break;
        }

        if (errorCode == 0) {
            void *pDeviceType = (void *) deviceType;
            void *pPowerOff = (void *) powerOff;
            errorCode = uDeviceCallback("close", pDeviceType, pPowerOff);
        }

        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

void uDeviceSetUserContext(uDeviceHandle_t devHandle, void *pUserContext)
{
    if (devHandle != NULL) {
        U_DEVICE_INSTANCE(devHandle)->pUserContext = pUserContext;
    }
}

/** Get device attached user context.
 *
 * @return User context that was set using @ref uDeviceSetUserContext.
 */
void *pUDeviceGetUserContext(uDeviceHandle_t devHandle)
{
    if (devHandle == NULL) {
        return NULL;
    } else {
        return U_DEVICE_INSTANCE(devHandle)->pUserContext;
    }
}

// End of file
