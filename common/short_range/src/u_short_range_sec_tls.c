/*
 * Copyright 2019-2022 u-blox
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
 * @brief Implementation of the TLS security APIs for a u-blox
 * Wifi/BLE module.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen(), strncpy(), memset()

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_port_heap.h"

#include "u_security_tls.h"
#include "u_security_credential.h"

#include "u_short_range_sec_tls.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The last error.
 */
static uErrorCode_t gLastErrorCode = U_ERROR_COMMON_SUCCESS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Store a string into the context.
static uErrorCode_t storeString(const char *pSrc, char **ppDest)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;
    size_t bufferLength;

    *ppDest = NULL;
    if (pSrc != NULL) {
        bufferLength = strlen(pSrc);
        if (bufferLength > U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES) {
            bufferLength = U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES;
        }
        // Add one more for the terminator
        bufferLength++;
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        *ppDest = (char *) pUPortMalloc(bufferLength);
        if (*ppDest != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            strncpy(*ppDest, pSrc, bufferLength);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add a short-range TLS security context.
uShortRangeSecTlsContext_t *pUShortRangeSecTlsAdd(uSecurityTlsVersion_t tlsVersionMin,
                                                  const char *pRootCaCertificateName,
                                                  const char *pClientCertificateName,
                                                  const char *pClientPrivateKeyName,
                                                  bool certificateCheckOn)
{
    uShortRangeSecTlsContext_t *pContext = (uShortRangeSecTlsContext_t *) pUPortMalloc(sizeof(
                                                                                           *pContext));
    uErrorCode_t errorCode = U_ERROR_COMMON_NO_MEMORY;

    // Error checking will have already been performed by
    // pUSecurityTlsAdd(), no need to do it again here.
    if (pContext != NULL) {
        memset(pContext, 0, sizeof(*pContext));
        pContext->tlsVersionMin = tlsVersionMin;
        pContext->certificateCheckOn = certificateCheckOn;
        errorCode = storeString(pRootCaCertificateName,
                                &(pContext->pRootCaCertificateName));
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            errorCode = storeString(pClientCertificateName,
                                    &(pContext->pClientCertificateName));
        }
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            errorCode = storeString(pClientPrivateKeyName,
                                    &(pContext->pClientPrivateKeyName));
        }
        if (errorCode != U_ERROR_COMMON_SUCCESS) {
            uPortFree(pContext->pRootCaCertificateName);
            uPortFree(pContext->pClientCertificateName);
            uPortFree(pContext->pClientPrivateKeyName);
            uPortFree(pContext);
            pContext = NULL;
        }
    }

    if (errorCode != U_ERROR_COMMON_SUCCESS) {
        gLastErrorCode = errorCode;
    }

    return pContext;
}

// Remove a TLS security context.
void uShortRangeSecTlsRemove(uShortRangeSecTlsContext_t *pContext)
{
    if (pContext != NULL) {
        uPortFree(pContext->pRootCaCertificateName);
        uPortFree(pContext->pClientCertificateName);
        uPortFree(pContext->pClientPrivateKeyName);
        uPortFree(pContext);
    }
}

// Get the last error that occurred and reset it.
int32_t uShortRangeSecTlsResetLastError()
{
    int32_t errorCode = (int32_t) gLastErrorCode;
    gLastErrorCode = U_ERROR_COMMON_SUCCESS;
    return errorCode;
}

// End of file
