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

#ifndef _U_SHORT_RANGE_SEC_TLS_H_
#define _U_SHORT_RANGE_SEC_TLS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _short-range
 *  @{
 */

/** @file
 * @brief This header file defines the TLS security APIs for a u-blox
 * Wifi/BLE module.  Note that these functions are not intended to be
 * called directly, they are called internally within ubxlib by the
 * common TLS security API (common/security/api/u_security_tls.h)
 * when a secure connection is requested by one of the common
 * protocol APIs (e.g. common/sock). These functions are not
 * thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A short-range security context.
 */
typedef struct {
    uSecurityTlsVersion_t tlsVersionMin;
    char *pRootCaCertificateName;
    char *pClientCertificateName;
    char *pClientPrivateKeyName;
    bool certificateCheckOn;
} uShortRangeSecTlsContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Add a short-range TLS security context.  This function is called
 * internally within ubxlib by the common TLS security API
 * (common/security/api/u_security_tls.h) when a secure connection
 * is requested by one of the common protocol APIs (e.g. common/sock).
 *
 * @param tlsVersionMin               the minimum TLS version to use.
 * @param[in] pRootCaCertificateName  the null-terminated name of the
 *                                    root X.509 certificate, which must
 *                                    have been stored using
 *                                    uSecurityCredentialStore().
 * @param[in] pClientCertificateName  the null-terminated name of the
 *                                    client X.509 certificate, which
 *                                    must have been stored using
 *                                    uSecurityCredentialStore().
 * @param[in] pClientPrivateKeyName   the null-terminated name of the
 *                                    client private key, which must have
 *                                    been stored using
 *                                    uSecurityCredentialStore().
 * @param certificateCheckOn          set to true if certificate checking
 *                                    is to be performed.
 * @return                            on success a pointer to the TLS security
 *                                    context, else NULL (in which case
 *                                    uShortRangeSecTlsResetLastError() should
 *                                    be called to find out why).
 */
uShortRangeSecTlsContext_t *pUShortRangeSecTlsAdd(uSecurityTlsVersion_t tlsVersionMin,
                                                  const char *pRootCaCertificateName,
                                                  const char *pClientCertificateName,
                                                  const char *pClientPrivateKeyName,
                                                  bool certificateCheckOn);

/** Remove a short-range TLS security context.  This function is
 * called internally within ubxlib by the common TLS security API
 * (common/security/api/u_security_tls.h) when a secure connection
 * is closed by one of the common protocol APIs (e.g. common/sock).
 *
 * @param[in,out] pContext a pointer to the TLS security context.
 */
void uShortRangeSecTlsRemove(uShortRangeSecTlsContext_t *pContext);

/** Get the last error that occurred in this API.  This should
 * be called if pUShortRangeSecTlsAdd() returned NULL to find out
 * why.  The error code is reset to "success" by this function.
 *
 * @return the last error code.
 */
int32_t uShortRangeSecTlsResetLastError();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif //_U_SHORT_RANGE_SEC_TLS_H_

// End of file
