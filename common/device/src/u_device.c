/*
 * Copyright 2022 u-blox
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

#include "u_device_private_cell.h"
#include "u_device_private_gnss.h"
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

int32_t uDeviceInit()
{
    int32_t errorCode = uDeviceMutexCreate();

    if (errorCode == 0) {
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
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

int32_t uDeviceOpen(const uDeviceCfg_t *pDeviceCfg, uDeviceHandle_t *pDeviceHandle)
{
    // Lock the API
    int32_t errorCode = uDeviceLock();

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
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
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
                break;
        }

        // ...and done
        uDeviceUnlock();
    }

    return errorCode;
}

// End of file
