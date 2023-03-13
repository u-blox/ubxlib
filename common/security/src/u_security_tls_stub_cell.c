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
 * @brief Stubs to allow the TLS Security API to be compiled without
 * cellular; if you call a cellular API function from the source code
 * here you must also include a weak stub for it which will return
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
#include "u_security_tls.h"
#include "u_cell_sec_tls.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK uCellSecTlsContext_t *pUCellSecSecTlsAdd(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return NULL;
}

U_WEAK void uCellSecTlsRemove(uCellSecTlsContext_t *pContext)
{
    (void) pContext;
}

U_WEAK int32_t uCellSecTlsResetLastError()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsRootCaCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                                   const char *pName)
{
    (void) pContext;
    (void) pName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsClientCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                                   const char *pName)
{
    (void) pContext;
    (void) pName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsClientPrivateKeyNameSet(const uCellSecTlsContext_t *pContext,
                                                  const char *pName,
                                                  const char *pPassword)
{
    (void) pContext;
    (void) pName;
    (void) pPassword;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsClientPskSet(const uCellSecTlsContext_t *pContext,
                                       const char *pPsk, size_t pskLengthBytes,
                                       const char *pPskId, size_t pskIdLengthBytes,
                                       bool generate)
{
    (void) pContext;
    (void) pPsk;
    (void) pskLengthBytes;
    (void) pPskId;
    (void) pskIdLengthBytes;
    (void) generate;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsUseDeviceCertificateSet(const uCellSecTlsContext_t *pContext,
                                                  bool includeCaCertificates)
{
    (void) pContext;
    (void) includeCaCertificates;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsCipherSuiteAdd(const uCellSecTlsContext_t *pContext,
                                         int32_t ianaNumber)
{
    (void) pContext;
    (void) ianaNumber;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsVersionSet(const uCellSecTlsContext_t *pContext,
                                     int32_t tlsVersionMin)
{
    (void) pContext;
    (void) tlsVersionMin;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsCertificateCheckSet(const uCellSecTlsContext_t *pContext,
                                              uCellSecTlsCertficateCheck_t check,
                                              const char *pUrl)
{
    (void) pContext;
    (void) check;
    (void) pUrl;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellSecTlsSniSet(const uCellSecTlsContext_t *pContext,
                                 const char *pSni)
{
    (void) pContext;
    (void) pSni;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}


// End of file
