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
 * @brief Implementation of the API to read general information from
 * a GNSS chip; for position information please see the u_gnss_pos.h
 * API instead.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_error_common.h"

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

// Get the version string from the GNSS chip.
int32_t uGnssInfoGetFirmwareVersionStr(int32_t gnssHandle,
                                       char *pStr, size_t size)
{
    (void) gnssHandle;
    (void) pStr;
    (void) size;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the chip ID from the GNSS chip.
int32_t uGnssInfoGetIdStr(int32_t gnssHandle,
                          char *pStr, size_t size)
{
    (void) gnssHandle;
    (void) pStr;
    (void) size;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the UTC time according to GNSS.
int32_t uGnssInfoGetTimeUtc(int32_t gnssHandle)
{
    (void) gnssHandle;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// End of file
