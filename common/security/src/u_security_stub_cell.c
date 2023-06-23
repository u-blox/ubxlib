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
 * @brief Stubs to allow the Security API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when cellular is not included in the build.
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
#include "u_security.h"
#include "u_cell_sec.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK bool uCellSecIsSupported(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK bool uCellSecIsBootstrapped(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellSecGetSerialNumber(uDeviceHandle_t cellHandle,
                                       char *pSerialNumber)
{
    (void) cellHandle;
    (void) pSerialNumber;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecGetRootOfTrustUid(uDeviceHandle_t cellHandle,
                                         char *pRootOfTrustUid)
{
    (void) cellHandle;
    (void) pRootOfTrustUid;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecSealSet(uDeviceHandle_t cellHandle,
                               const char *pDeviceProfileUid,
                               const char *pDeviceSerialNumberStr,
                               bool (*pKeepGoingCallback) (void))
{
    (void) cellHandle;
    (void) pDeviceProfileUid;
    (void) pDeviceSerialNumberStr;
    (void) pKeepGoingCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellSecIsSealed(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellSecZtpGetDeviceCertificate(uDeviceHandle_t cellHandle,
                                               char *pData,
                                               size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) pData;
    (void) dataSizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecZtpGetPrivateKey(uDeviceHandle_t cellHandle,
                                        char *pData,
                                        size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) pData;
    (void) dataSizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecZtpGetCertificateAuthorities(uDeviceHandle_t cellHandle,
                                                    char *pData,
                                                    size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) pData;
    (void) dataSizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecPskGenerate(uDeviceHandle_t cellHandle,
                                   size_t pskSizeBytes, char *pPsk,
                                   char *pPskId)
{
    (void) cellHandle;
    (void) pskSizeBytes;
    (void) pPsk;
    (void) pPskId;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecHeartbeatTrigger(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
