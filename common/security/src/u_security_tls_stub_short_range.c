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
 * Wi-Fi; if you call a Wi-Fi API function from the source code here you
 * must also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when Wi-Fi is not included in the build.
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
#include "u_short_range_sec_tls.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK uShortRangeSecTlsContext_t *pUShortRangeSecTlsAdd(uSecurityTlsVersion_t tlsVersionMin,
                                                         const char *pRootCaCertificateName,
                                                         const char *pClientCertificateName,
                                                         const char *pClientPrivateKeyName,
                                                         bool certificateCheckOn)
{
    (void) tlsVersionMin;
    (void) pRootCaCertificateName;
    (void) pClientCertificateName;
    (void) pClientPrivateKeyName;
    (void) certificateCheckOn;
    return NULL;
}

U_WEAK void uShortRangeSecTlsRemove(uShortRangeSecTlsContext_t *pContext)
{
    (void) pContext;
}

U_WEAK int32_t uShortRangeSecTlsResetLastError()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}
// End of file
