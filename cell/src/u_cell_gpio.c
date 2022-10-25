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
 * @brief Implementation of the GPIO API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // atoi()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Order is important here
#include "u_cell_private.h" // don't change it
#include "u_cell_gpio.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure a GPIO.
int32_t uCellGpioConfig(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId,
                        bool isOutput, int32_t level)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && ((int32_t) gpioId >= 0)) {
            atHandle = pInstance->atHandle;

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPIOC=");
            // Write GPIO ID.
            uAtClientWriteInt(atHandle, (int32_t) gpioId);
            // Write GPIO direction.
            uAtClientWriteInt(atHandle, isOutput ? 0 : 1);
            if (isOutput) {
                // Write initial output value
                uAtClientWriteInt(atHandle, level);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set the state of a GPIO.
int32_t uCellGpioSet(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId,
                     int32_t level)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && ((int32_t) gpioId >= 0)) {
            atHandle = pInstance->atHandle;

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPIOW=");
            // Write GPIO ID
            uAtClientWriteInt(atHandle, (int32_t) gpioId);
            // Write output level
            uAtClientWriteInt(atHandle, level);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the state of a GPIO.
int32_t uCellGpioGet(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t level;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && ((int32_t) gpioId >= 0)) {
            atHandle = pInstance->atHandle;

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPIOR=");
            // Write GPIO ID
            uAtClientWriteInt(atHandle, (int32_t) gpioId);
            uAtClientCommandStop(atHandle);
            // Note: need to use just +UGPIO" here since SARA-U201
            // returns "+UGPIO:" while all the other modules
            // return "+UGPIOR:"
            uAtClientResponseStart(atHandle, "+UGPIO");
            // Skip the first integer parameter, which is
            // just our GPIO ID again
            uAtClientSkipParameters(atHandle, 1);
            // Read the second integer parameter, which is the level
            level = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if ((errorCode == 0) && (level >= 0)) {
                errorCode = level ? 1 : 0;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set the state of the CTS line.
int32_t uCellGpioSetCts(uDeviceHandle_t cellHandle, int32_t level)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_CTS_CONTROL)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UCTS=");
                // Write output level
                uAtClientWriteInt(atHandle, level);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the state of the CTS line.
int32_t uCellGpioGetCts(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t level;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_CTS_CONTROL)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UCTS?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UCTS:");
                // Read the level
                level = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
                if ((errorCode == 0) && (level >= 0)) {
                    errorCode = level ? 1 : 0;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
