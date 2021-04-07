/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the common location API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_location.h"

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

// Get the current location.

int32_t uLocationGet(int32_t networkHandle, uLocationType_t type,
                     uLocationAssist_t *pLocationAssist,
                     const char *pAuthenticationTokenStr,
                     uLocation_t *pLocation,
                     bool (*pKeepGoingCallback) (int32_t))
{
    (void) networkHandle;
    (void) type;
    (void) pLocationAssist;
    (void) pAuthenticationTokenStr;
    (void) pLocation;
    (void) pKeepGoingCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the current location, non-blocking version.
int32_t uLocationGetStart(int32_t networkHandle, uLocationType_t type,
                          uLocationAssist_t *pLocationAssist,
                          const char *pAuthenticationTokenStr,
                          void (*pCallback) (int32_t, uLocation_t))
{
    (void) networkHandle;
    (void) type;
    (void) pLocationAssist;
    (void) pAuthenticationTokenStr;
    (void) pCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the current status of a location establishment attempt.
uLocationStatus_t uLocationGetStatus(int32_t networkHandle)
{
    (void) networkHandle;

    // TODO

    return U_LOCATION_STATUS_UNKNOWN;
}

// Cancel a uLocationGetStart().
void uLocationGetStop(int32_t networkHandle)
{
    (void) networkHandle;
    // TODO
}

// End of file
