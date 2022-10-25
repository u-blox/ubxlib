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
 * @brief Implementation of the cfg API for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port_os.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_wifi_module_type.h"
#include "u_wifi_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_WIFI_CFG_STARTUP_MODE_EDM 2

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static int32_t getStartupMode(const uAtClientHandle_t atHandle)
{
    int32_t modeOrError;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UMSM?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UMSM:");
    modeOrError = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    return modeOrError;
}

static int32_t setStartupMode(const uAtClientHandle_t atHandle, int32_t mode)
{
    int32_t error;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UMSM=");
    uAtClientWriteInt(atHandle, mode);
    uAtClientCommandStopReadResponse(atHandle);
    error = uAtClientUnlock(atHandle);

    return error;
}

static int32_t restart(const uAtClientHandle_t atHandle, bool store)
{
    int32_t error = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (store) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT&W");
        uAtClientCommandStopReadResponse(atHandle);
        error = uAtClientUnlock(atHandle);
    }

    if (error == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CPWROFF");
        uAtClientCommandStopReadResponse(atHandle);
        error = uAtClientUnlock(atHandle);

        if (error == (int32_t) U_ERROR_COMMON_SUCCESS) {
            uPortTaskBlock(500);
            uAtClientFlush(atHandle);
        }
    }

    return error;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int32_t uWifiCfgConfigure(uDeviceHandle_t devHandle,
                          const uWifiCfg_t *pCfg)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pCfg != NULL) {

            U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

            pInstance = pUShortRangePrivateGetInstance(devHandle);
            if (pInstance != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                bool restartNeeded = false;
                atHandle = pInstance->atHandle;


                int32_t mode = getStartupMode(atHandle);
                if (mode != U_WIFI_CFG_STARTUP_MODE_EDM) {
                    errorCode = setStartupMode(atHandle, U_WIFI_CFG_STARTUP_MODE_EDM);
                    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                        restartNeeded = true;
                    }
                }


                if (errorCode >= 0 && restartNeeded) {
                    restart(atHandle, true);
                }
            }

            U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        }
    }

    return errorCode;
}

// End of file
