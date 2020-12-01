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
 * @brief Implementation function calling AT commands for short range modules.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_short_range.h"
#include "u_short_range_at_commands.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_MAX_NUM_SERVERS 7

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef struct {
    uShortRangeModuleType_t module;
    const char *pStr;
} uShortRangeStringToModule_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const uShortRangeStringToModule_t stringToModule[] = {
    {U_SHORT_RANGE_MODULE_TYPE_NINA_B1, "NINA-B1"},
    {U_SHORT_RANGE_MODULE_TYPE_ANNA_B1, "ANNA-B1"},
    {U_SHORT_RANGE_MODULE_TYPE_NINA_B3, "NINA-B3"},
    {U_SHORT_RANGE_MODULE_TYPE_NINA_B4, "NINA-B4"},
    {U_SHORT_RANGE_MODULE_TYPE_NINA_B2, "NINA-B2"},
    {U_SHORT_RANGE_MODULE_TYPE_NINA_W13, "NINA-W13"},
    {U_SHORT_RANGE_MODULE_TYPE_NINA_W15, "NINA-W15"},
    {U_SHORT_RANGE_MODULE_TYPE_ODIN_W2, "ODIN-W2"},
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uShortRangeModuleType_t convert(const char *pStr)
{
    for (uint32_t i = 0;  i < sizeof (stringToModule) / sizeof (stringToModule[0]);  ++i) {
        if (!strcmp (pStr, stringToModule[i].pStr)) {
            return stringToModule[i].module;
        }
    }
    return U_SHORT_RANGE_MODULE_TYPE_INVALID;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t getBleRole(const uAtClientHandle_t atHandle)
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

int32_t setBleRole(const uAtClientHandle_t atHandle, int32_t role)
{
    int32_t error;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UBTLE=");
    uAtClientWriteInt(atHandle, role);
    uAtClientCommandStop(atHandle);
    uAtClientCommandStopReadResponse(atHandle);
    error = uAtClientUnlock(atHandle);

    return error;
}

int32_t getServers(const uAtClientHandle_t atHandle, uShortRangeServerType_t type)
{
    int32_t error = -1;
    int32_t id;

    uAtClientLock(atHandle);
    // Short time out so we don't hand if number of set servers is less than max
    uAtClientTimeoutSet(atHandle, 50);
    uAtClientCommandStart(atHandle, "AT+UDSC");
    uAtClientCommandStop(atHandle);

    bool found = false;
    for (size_t y = 0; (y < U_SHORT_RANGE_MAX_NUM_SERVERS) &&
         !found; y++) {
        uAtClientResponseStart(atHandle, "+UDSC:");
        id = uAtClientReadInt(atHandle);
        if (uAtClientReadInt(atHandle) == (int32_t) type) {
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



int32_t setServer(const uAtClientHandle_t atHandle, uShortRangeServerType_t type)
{
    int32_t error;
    int32_t id = -1;
    bool found = false;

    uAtClientLock(atHandle);
    for (size_t y = 0; (y < U_SHORT_RANGE_MAX_NUM_SERVERS) &&
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
        uAtClientCommandStop(atHandle);
        uAtClientCommandStopReadResponse(atHandle);
        error = uAtClientUnlock(atHandle);
    }

    return error;
}

int32_t restart(const uAtClientHandle_t atHandle, bool store)
{
    int32_t error = (int32_t) U_ERROR_COMMON_SUCCESS;;

    if (store) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT&W");
        uAtClientCommandStop(atHandle);
        uAtClientCommandStopReadResponse(atHandle);
        error = (int32_t)uAtClientUnlock(atHandle);
    }

    if (error == (int32_t)U_ERROR_COMMON_SUCCESS) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CPWROFF");
        uAtClientCommandStop(atHandle);
        uAtClientCommandStopReadResponse(atHandle);
        error = (int32_t)uAtClientUnlock(atHandle);
    }

    return error;
}

int32_t setEchoOff(const uAtClientHandle_t atHandle, uint8_t retries)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;

    for (uint8_t i = 0; i < retries; i++) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "ATE0");
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            break;
        }
    }

    return errorCode;
}


uShortRangeModuleType_t getModule(const uAtClientHandle_t atHandle)
{
    uShortRangeModuleType_t module = U_SHORT_RANGE_MODULE_TYPE_INVALID;

    char buffer[20];
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+GMM");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, NULL);
    int32_t bytesRead = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS &&
        bytesRead >= 7) {
        module = convert(buffer);
    }

    return module;
}

// End of file
