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
 * @brief Implementation of the GNSS APIs to read position.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

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

// Get the current position.
int32_t uGnssPosGet(int32_t gnssHandle,
                    int32_t *pLatitudeX1e6, int32_t *pLongitudeX1e6,
                    int32_t *pAltitudeMillimetres,
                    int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond,
                    int32_t *pSvs, int32_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (int32_t))
{
    (void) gnssHandle;
    (void) pLatitudeX1e6;
    (void) pLongitudeX1e6;
    (void) pAltitudeMillimetres;
    (void) pRadiusMillimetres;
    (void) pSpeedMillimetresPerSecond;
    (void) pSvs;
    (void) pTimeUtc;
    (void) pKeepGoingCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Get the current position, non-blocking version.
int32_t uGnssPosGetStart(int32_t gnssHandle,
                         void (*pCallback) (int32_t latitudeX1e6,
                                            int32_t longitudeX1e6,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int32_t timeUtc))
{
    (void) gnssHandle;
    (void) pCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Cancel a uGnssPosGetStart().
void uGnssPosGetStop(int32_t gnssHandle)
{
    (void) gnssHandle;

    // TODO
}

// End of file
