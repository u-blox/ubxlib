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
 * @brief Stubs to allow the Location API to be compiled without GNSS;
 * if you call a GNSS API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED for when GNSS is not included in the
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
#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"
#include "u_gnss_pos.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uGnssGetTransportHandle(uDeviceHandle_t gnssHandle,
                                       uGnssTransportType_t *pTransportType,
                                       uGnssTransportHandle_t *pTransportHandle)
{
    (void) gnssHandle;
    (void) pTransportType;
    (void) pTransportHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uGnssPosGet(uDeviceHandle_t gnssHandle,
                           int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                           int32_t *pAltitudeMillimetres,
                           int32_t *pRadiusMillimetres,
                           int32_t *pSpeedMillimetresPerSecond,
                           int32_t *pSvs, int64_t *pTimeUtc,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void) gnssHandle;
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

U_WEAK int32_t uGnssPosGetStart(uDeviceHandle_t gnssHandle,
                                void (*pCallback) (uDeviceHandle_t gnssHandle,
                                                   int32_t errorCode,
                                                   int32_t latitudeX1e7,
                                                   int32_t longitudeX1e7,
                                                   int32_t altitudeMillimetres,
                                                   int32_t radiusMillimetres,
                                                   int32_t speedMillimetresPerSecond,
                                                   int32_t svs,
                                                   int64_t timeUtc))
{
    (void) gnssHandle;
    (void) pCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uGnssPosGetStop(uDeviceHandle_t gnssHandle)
{
    (void) gnssHandle;
}

U_WEAK int32_t uGnssCfgSetProtocolOut(uDeviceHandle_t gnssHandle,
                                      uGnssProtocol_t protocol,
                                      bool onNotOff)
{
    (void) gnssHandle;
    (void) protocol;
    (void) onNotOff;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uGnssPosGetStreamedStart(uDeviceHandle_t gnssHandle,
                                        int32_t rateMs,
                                        void (*pCallback) (uDeviceHandle_t gnssHandle,
                                                           int32_t errorCode,
                                                           int32_t latitudeX1e7,
                                                           int32_t longitudeX1e7,
                                                           int32_t altitudeMillimetres,
                                                           int32_t radiusMillimetres,
                                                           int32_t speedMillimetresPerSecond,
                                                           int32_t svs,
                                                           int64_t timeUtc))
{
    (void) gnssHandle;
    (void) rateMs;
    (void) pCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uGnssPosGetStreamedStop(uDeviceHandle_t gnssHandle)
{
    (void) gnssHandle;
}

U_WEAK int32_t uGnssPosSetRrlpMode(uDeviceHandle_t gnssHandle, uGnssRrlpMode_t mode)
{
    (void) gnssHandle;
    (void) mode;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uGnssPosGetRrlp(uDeviceHandle_t gnssHandle, char *pBuffer,
                               size_t sizeBytes, int32_t svsThreshold,
                               int32_t cNoThreshold,
                               int32_t multipathIndexLimit,
                               int32_t pseudorangeRmsErrorIndexLimit,
                               bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void) gnssHandle;
    (void) pBuffer;
    (void) sizeBytes;
    (void) svsThreshold;
    (void) cNoThreshold;
    (void) multipathIndexLimit;
    (void) pseudorangeRmsErrorIndexLimit;
    (void) pKeepGoingCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
