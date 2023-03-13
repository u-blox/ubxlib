/*
 * Copyright 2019-2023 u-blox
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
 * @brief Stubs to allow the Location API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when cellular is not included in the
 * build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_device.h"
#include "u_cell_loc.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK void uCellLocSetDesiredAccuracy(uDeviceHandle_t cellHandle,
                                       int32_t accuracyMillimetres)
{
    (void) cellHandle;
    (void) accuracyMillimetres;
}

U_WEAK void uCellLocSetDesiredFixTimeout(uDeviceHandle_t cellHandle,
                                         int32_t fixTimeoutSeconds)
{
    (void) cellHandle;
    (void) fixTimeoutSeconds;
}

U_WEAK void uCellLocSetGnssEnable(uDeviceHandle_t cellHandle, bool onNotOff)
{
    (void) cellHandle;
    (void) onNotOff;
}

U_WEAK int32_t uCellLocSetServer(uDeviceHandle_t cellHandle,
                                 const char *pAuthenticationTokenStr,
                                 const char *pPrimaryServerStr,
                                 const char *pSecondaryServerStr)
{
    (void) cellHandle;
    (void) pAuthenticationTokenStr;
    (void) pPrimaryServerStr;
    (void) pSecondaryServerStr;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellLocGet(uDeviceHandle_t cellHandle,
                           int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                           int32_t *pAltitudeMillimetres, int32_t *pRadiusMillimetres,
                           int32_t *pSpeedMillimetresPerSecond,
                           int32_t *pSvs,
                           int64_t *pTimeUtc,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void) cellHandle;
    (void) pLatitudeX1e7;
    (void) pLongitudeX1e7;
    (void) pAltitudeMillimetres;
    (void) pRadiusMillimetres;
    (void) pSpeedMillimetresPerSecond;
    (void) pSvs;
    (void) pTimeUtc;
    (void) pKeepGoingCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellLocGetStart(uDeviceHandle_t cellHandle,
                                void (*pCallback) (uDeviceHandle_t cellHandle,
                                                   int32_t errorCode,
                                                   int32_t latitudeX1e7,
                                                   int32_t longitudeX1e7,
                                                   int32_t altitudeMillimetres,
                                                   int32_t radiusMillimetres,
                                                   int32_t speedMillimetresPerSecond,
                                                   int32_t svs,
                                                   int64_t timeUtc))
{
    (void) cellHandle;
    (void) pCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellLocGetStatus(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uCellLocGetStop(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
}

// End of file
