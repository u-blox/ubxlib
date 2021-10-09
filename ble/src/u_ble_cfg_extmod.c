/*
 * Copyright 2020 u-blox Ltd
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
 * @brief Implementation of the cfg API for ble.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free(), strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_cfg_sw.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_ble_cfg.h"
#include "u_ble_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_BLE_CFG_SERVER_TYPE_SPS 6
#define U_BLE_CFG_MAX_NUM_SERVERS 7
#define U_BLE_CFG_STARTUP_MODE_EDM 2

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

static int32_t getBleRole(const uAtClientHandle_t atHandle)
{
    int32_t roleOrError;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UBTLE?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UBTLE:");
    roleOrError = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    return roleOrError;
}

static int32_t setBleRole(const uAtClientHandle_t atHandle, int32_t role)
{
    int32_t error;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UBTLE=");
    uAtClientWriteInt(atHandle, role);
    uAtClientCommandStopReadResponse(atHandle);
    error = uAtClientUnlock(atHandle);

    return error;
}

static int32_t getServer(const uAtClientHandle_t atHandle, int32_t type)
{
    int32_t error = -1;
    int32_t id;

    uAtClientLock(atHandle);
    // Short time out so we don't hang if number of set servers is less than max
    uAtClientTimeoutSet(atHandle, 50);
    uAtClientCommandStart(atHandle, "AT+UDSC");
    uAtClientCommandStop(atHandle);

    bool found = false;
    for (size_t y = 0; (y < U_BLE_CFG_MAX_NUM_SERVERS) &&
         !found; y++) {
        uAtClientResponseStart(atHandle, "+UDSC:");
        id = uAtClientReadInt(atHandle);
        if (uAtClientReadInt(atHandle) == type) {
            found = true;
            break;
        }
    }

    uAtClientResponseStop(atHandle);
    // Don't check for errors here as we will likely
    // have a timeout through waiting for a type that
    // didn't come.
    uAtClientUnlock(atHandle);

    if (found) {
        error = id;
    }

    return error;
}

static int32_t disableServer(const uAtClientHandle_t atHandle, int32_t serverId)
{
    int32_t error;

    uAtClientLock(atHandle);

    uAtClientCommandStart(atHandle, "AT+UDSC=");
    uAtClientWriteInt(atHandle, serverId);
    uAtClientWriteInt(atHandle, 0);
    uAtClientCommandStopReadResponse(atHandle);

    error = uAtClientUnlock(atHandle);

    return error;
}

static int32_t setServer(const uAtClientHandle_t atHandle, uShortRangeServerType_t type)
{
    int32_t error;
    int32_t id = -1;
    bool found = false;

    uAtClientLock(atHandle);
    for (size_t y = 0; (y < U_BLE_CFG_MAX_NUM_SERVERS) &&
         !found; y++) {
        uAtClientCommandStart(atHandle, "AT+UDSC=");
        uAtClientWriteInt(atHandle, (int32_t) y);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UDSC:");
        id = uAtClientReadInt(atHandle);
        if (uAtClientReadInt(atHandle) == (int32_t) U_SHORT_RANGE_SERVER_DISABLED) {
            found = true;
            uAtClientResponseStop(atHandle);
            break;
        }
        uAtClientResponseStop(atHandle);
    }
    error = uAtClientUnlock(atHandle);

    if (found) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDSC=");
        uAtClientWriteInt(atHandle, id);
        uAtClientWriteInt(atHandle, (int32_t) type);
        uAtClientCommandStopReadResponse(atHandle);
        error = uAtClientUnlock(atHandle);
    }

    return error;
}

static int32_t restart(const uAtClientHandle_t atHandle, bool store)
{
    int32_t error = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (store) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT&W");
        uAtClientCommandStopReadResponse(atHandle);
        error = (int32_t)uAtClientUnlock(atHandle);
    }

    if (error == (int32_t)U_ERROR_COMMON_SUCCESS) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CPWROFF");
        uAtClientCommandStopReadResponse(atHandle);
        error = (int32_t)uAtClientUnlock(atHandle);

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
int32_t uBleCfgConfigure(int32_t bleHandle,
                         const uBleCfg_t *pCfg)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t shoHandle = uBleToShoHandle(bleHandle);

    if (pCfg != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

        if (uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS) {
            pInstance = pUShortRangePrivateGetInstance(shoHandle);
            if (pInstance != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                bool restartNeeded = false;
                atHandle = pInstance->atHandle;

                int32_t role = getBleRole(atHandle);
                if (role != (int32_t) pCfg->role) {
                    errorCode = setBleRole(atHandle, (int32_t) pCfg->role);
                    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                        restartNeeded = true;
                    }
                }

                if (pCfg->spsServer) {
                    if (getServer(atHandle, U_BLE_CFG_SERVER_TYPE_SPS) < 0) {
                        errorCode = setServer(atHandle,
                                              (uShortRangeServerType_t) U_BLE_CFG_SERVER_TYPE_SPS);
                        if (errorCode >= 0) {
                            restartNeeded = true;
                        }
                    }
                } else {
                    int32_t spsServerId = getServer(atHandle, U_BLE_CFG_SERVER_TYPE_SPS);
                    if (spsServerId >= 0) {
                        disableServer(atHandle, spsServerId);
                        restartNeeded = true;
                    }
                }

                int32_t mode = getStartupMode(atHandle);
                if (mode != U_BLE_CFG_STARTUP_MODE_EDM) {
                    errorCode = setStartupMode(atHandle, U_BLE_CFG_STARTUP_MODE_EDM);
                    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                        restartNeeded = true;
                    }
                }


                if (errorCode >= 0 && restartNeeded) {
                    restart(atHandle, true);
                }
            }
            uShortRangeUnlock();
        }
    }

    return errorCode;
}

#endif

// End of file
