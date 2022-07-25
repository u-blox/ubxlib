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

#ifndef _U_CELL_SEC_TLS_H_
#define _U_CELL_SEC_TLS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the TLS security APIs for a u-blox
 * cellular module.  Note that these functions are not intended to be
 * called directly, they are called internally within ubxlib by the
 * common TLS security API (common/security/api/u_security_tls.h)
 * when a secure connection is requested by one of the common
 * protocol APIs (e.g. common/sock).  These functions are thread-safe
 * unless otherwise stated.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_SEC_TLS_PSK_MAX_LENGTH_BYTES
/** The maximum length of a PSK array (binary, not hex encoded as ASCII).
 */
# define U_CELL_SEC_TLS_PSK_MAX_LENGTH_BYTES 64
#endif

#ifndef U_CELL_SEC_TLS_PSK_ID_MAX_LENGTH_BYTES
/** The maximum length of a PSK ID array (binary, not hex encoded as ASCII).
 */
# define U_CELL_SEC_TLS_PSK_ID_MAX_LENGTH_BYTES 128
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of certificate checking that can be performed.
 * NOTE: this must use the same values as the equivalent enum in
 * u_security_tls.h.
 */
typedef enum {
    U_CELL_SEC_TLS_CERTIFICATE_CHECK_NONE              = 0x00, /**< no checking. */
    U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA           = 0x01, /**< check root CA. */
    U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL       = 0x02, /**< check root CA and URL. */
    U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL_DATE  = 0x03, /**< check root CA, URL and expiry date. */
    U_CELL_SEC_TLS_CERTIFICATE_CHECK_MAX_NUM
} uCellSecTlsCertficateCheck_t;

/** Storage for a list of ciphers.
 */
typedef struct {
    char *pString; /**< the cipher list string as returned by
                        AT+USECPRF, for example "C034;009e;CCAD...",
                        max length #U_CELL_SEC_CIPHERS_BUFFER_LENGTH_BYTES. */
    size_t index;  /**< which character we are at in the string. */
} uCellSecTlsCipherList_t;

/** A cellular TLS security context.
 */
typedef struct {
    uDeviceHandle_t cellHandle; /**< the associated cellular handle. */
    uCellSecTlsCipherList_t cipherList; /**< temporary storage for a cipher list. */
    uint8_t profileId;  /**< the associated security profile ID,
                             at the end to improve structure packing. */
} uCellSecTlsContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: ADD/REMOVE A TLS SECURITY CONTEXT
 * -------------------------------------------------------------- */

/** Add a cellular TLS security context (AKA profile) with default
 * settings.  This function is called internally within ubxlib by
 * the common TLS security API
 * (common/security/api/u_security_tls.h) when a secure connection
 * is requested by one of the common protocol APIs (e.g. common/sock).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            on success a pointer to the TLS security context,
 *                    else NULL, in which case
 *                    uCellSecTlsResetLastError() should be called to
 *                    determine the cause of the failure).
 */
uCellSecTlsContext_t *pUCellSecSecTlsAdd(uDeviceHandle_t cellHandle);

/** Remove a cellular TLS security context.  This function is called
 * internally within ubxlib by the common TLS security API
 * (common/security/api/u_security_tls.h) when a secure connection
 * is closed by one of the common protocol APIs (e.g. common/sock).
 *
 * @param[in] pContext a pointer to the TLS security context.
 */
void uCellSecTlsRemove(uCellSecTlsContext_t *pContext);

/** Get the last error that occurred in this API.  This must
 * be called if pUCellSecTlsAdd() returned NULL to find out
 * why.  The error code is reset to "success" by this function.
 *
 * @return the last error code.
 */
int32_t uCellSecTlsResetLastError();

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURE CERTIFICATES/SECRETS
 * -------------------------------------------------------------- */

/** Set the name of the root CA X.509 certificate to use.
 * The X.509 certificate must have been stored in the module
 * using uSecurityCredentialStore().
 *
 * @param[in] pContext a pointer to the security context.
 * @param pName        the null-terminated name of the root CA
 *                     X.509 certificate, as stored using
 *                     uSecurityCredentialStore().
 * @return             zero on success else negative error code.
 */
int32_t uCellSecTlsRootCaCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                            const char *pName);

/** Get the name of the root CA X.509 certificate in use.
 * The X.509 certificate must have been stored in the module
 * using uSecurityCredentialStore().
 *
 * @param[in] pContext    a pointer to the security context.
 * @param[out] pName      a pointer to a place to store the name of
 *                        the root CA X.509 certificate; the name
 *                        will be a null-terminated string.
 * @param size            the number of bytes of storage at
 *                        pName; to ensure sufficient space at least
 *                        #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES + 1
 *                        bytes should be provided.
 * @return                on success the length of the string
 *                        stored at pName (i.e. what strlen() would
 *                        return) else negative error code.
 */
int32_t uCellSecTlsRootCaCertificateNameGet(const uCellSecTlsContext_t *pContext,
                                            char *pName,
                                            size_t size);

/** Set the name of the client X.509 certificate to use.
 * The X.509 certificate must have been stored in the module
 * using uSecurityCredentialStore().  See also
 * uCellSecTlsUseDeviceCertificateSet() below.
 *
 * @param[in] pContext a pointer to the security context.
 * @param[in] pName    the null-terminated name of the client
 *                     X.509 certificate, as stored using
 *                     uSecurityCredentialStore().
 * @return             zero on success else negative error code.
 */
int32_t uCellSecTlsClientCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                            const char *pName);

/** Get the name of the client X.509 certificate in use.
 * The X.509 certificate must have been stored in the module
 * using uSecurityCredentialStore().
 *
 * @param[in] pContext    a pointer to the security context.
 * @param[out] pName      a pointer to a place to store the name of
 *                        the client X.509 certificate; the name
 *                        will be a null-terminated string.
 * @param size            the number of bytes of storage at
 *                        pName; to ensure sufficient space at least
 *                        #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES + 1
 *                        bytes should be provided.
 * @return                on success the length of the string
 *                        stored at pName (i.e. what strlen() would
 *                        return) else negative error code.
 */
int32_t uCellSecTlsClientCertificateNameGet(const uCellSecTlsContext_t *pContext,
                                            char *pName,
                                            size_t size);

/** Set the name of the client private key to use and, if required
 * the associated password.  The key must have been stored in the
 * module using uSecurityCredentialStore().
 *
 * @param[in] pContext  a pointer to the security context.
 * @param[in] pName     the null-terminated name of the client private
 *                      key, as stored using uSecurityCredentialStore().
 * @param[in] pPassword the null-terminated password for the client
 *                      private key; use NULL if the key is not
 *                      password-protected.
 * @return              zero on success else negative error code.
 */
int32_t uCellSecTlsClientPrivateKeyNameSet(const uCellSecTlsContext_t *pContext,
                                           const char *pName,
                                           const char *pPassword);

/** Get the name of the client private key in use.  The key
 * must have been stored in the module using uSecurityCredentialStore().
 *
 * @param[in] pContext    a pointer to the security context.
 * @param[out] pName      a pointer to a place to store the name of
 *                        the client private key; the name
 *                        will be a null-terminated string.
 * @param size            the number of bytes of storage at
 *                        pName; to ensure sufficient space at least
 *                        #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES + 1
 *                        bytes should be provided.
 * @return                on success the length of the string
 *                        stored at pName (i.e. what strlen() would
 *                        return) else negative error code.
 */
int32_t uCellSecTlsClientPrivateKeyNameGet(const uCellSecTlsContext_t *pContext,
                                           char *pName,
                                           size_t size);

/** Set the pre-shared key and pre-shared key identity to use.
 * IMPORTANT: on all currently supported modules the PSK and
 * PSK ID must include no ASCII control characters.
 *
 * @param[in] pContext     a pointer to the security context.
 * @param[in] pPsk         the pre-shared key to use, encoded as binary,
 *                         for example [0x0a, 0x04, 0xf0, 0x08... etc],
 *                         *not* hex encoded in ASCII; cannot be NULL.
 * @param pskLengthBytes   the amount of data at pPsk, must be no
 *                         more than #U_SECURITY_TLS_PSK_MAX_LENGTH_BYTES.
 * @param[in] pPskId       the pre-shared key ID to use, encoded as binary,
 *                         for example [0x0a, 0x04, 0xf0, 0x08... etc],
 *                         *not* hex encoded in ASCII; cannot be NULL.
 * @param pskIdLengthBytes the amount of data at pPskId, must be no
 *                         more than #U_SECURITY_TLS_PSK_ID_MAX_LENGTH_BYTES.
 * @param generate         if this is set to true then, where supported,
 *                         the root of trust inside the cellular module
 *                         will generate the pre-shared key and
 *                         pre-shared key identity; pPsk and pPskId
 *                         must be set to NULL.
 * @return                 zero on success else negative error code.
 */
int32_t uCellSecTlsClientPskSet(const uCellSecTlsContext_t *pContext,
                                const char *pPsk, size_t pskLengthBytes,
                                const char *pPskId, size_t pskIdLengthBytes,
                                bool generate);

/** If this returns successfully then, for a module which supports u-blox
 * security and has been security sealed, the device public X.509 certificate
 * that was generated at security sealing will be used as the client
 * certificate.
 *
 * @param[in] pContext           a pointer to the security context.
 * @param includeCaCertificates  if set to true then the CA X.509 certificates
 *                               that were used to sign the device
 *                               certificate during the security sealing
 *                               process will also be included.
 * @return                       zero on success else negative error code.
 */
int32_t uCellSecTlsUseDeviceCertificateSet(const uCellSecTlsContext_t *pContext,
                                           bool includeCaCertificates);

/** Get whether the device public X.509 certificate that was generated at
 * security sealing is being used as the client certificate.
 *
 * @param[in] pContext                a pointer to the security context.
 * @param[out] pIncludeCaCertificates a place to store whether the CA X.509
 *                                    certificates that were used to sign the device
 *                                    certificate during the security sealing
 *                                    process are also being included; may be NULL.
 * @return                            true if the device public X.509 certificate
 *                                    is being usd as the cient certificate, else
 *                                    false (and the name set with the call to
 *                                    uCellSecTlsClientCertificateNameSet() is being
 *                                    used instead).
 */
bool uCellSecTlsIsUsingDeviceCertificate(const uCellSecTlsContext_t *pContext,
                                         bool *pIncludeCaCertificates);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURE CIPHER SUITE
 * -------------------------------------------------------------- */

/** Add a cipher suite to the set in use. Not all u-blox modules
 * support all cipher suites, consult the security section of your
 * u-blox module AT manual for further information.
 * Note that the SARA-U201 and SARA-R4xx modules support only a
 * single configurable cipher suite and adding a new one will
 * overwrite the previous.
 *
 * @param[in] pContext a pointer to the security context.
 * @param ianaNumber   the IANA number of the cipher suite to add.
 * @return             zero on success else negative error code.
 */
int32_t uCellSecTlsCipherSuiteAdd(const uCellSecTlsContext_t *pContext,
                                  int32_t ianaNumber);

/** Remove a cipher suite from the set in use.
 * Note: since the SARA-U201 and SARA-R4xx modules support only a
 * single configurable cipher suite this function will remove that
 * cipher suite, whatever the value of ianaNumber; effectively
 * a "reset to default" call.
 *
 * @param[in] pContext a pointer to the security context.
 * @param ianaNumber   the IANA number of the cipher suite to remove.
 * @return             zero on success else negative error code.
 */
int32_t uCellSecTlsCipherSuiteRemove(const uCellSecTlsContext_t *pContext,
                                     int32_t ianaNumber);

/** Get the first cipher suite in use; uCellSecTlsCipherSuiteListNext()
 * should be called repeatedly to iterate through subsequent entries
 * in the list.  This function is not thread-safe in that there
 * is a single list of names for any given security context.
 * The SARA-U201 and SARA-R4xx modules do not support this feature.
 *
 * For instance, to print out all of the cipher suites in use:
 *
 * ```
 * for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
 *      x >= 0;
 *      x = uCellSecTlsCipherSuiteListNext(pContext)) {
 *     printf("0x%04x\n", x);
 * }
 * ```
 *
 * @param[in] pContext a pointer to the security context.
 * @return             the IANA number of the cipher suite or
 *                     negative error code.
 */
int32_t uCellSecTlsCipherSuiteListFirst(uCellSecTlsContext_t *pContext);

/** Get the subsequent cipher suite in use. Use
 * uCellSecTlsCipherSuiteListFirst() to get the first entry
 * and then call this until an error is returned to read out all
 * of the entries; this will free the memory that held the
 * list after the final call (otherwise it will be freed when the
 * security context is removed or another cipher suite list
 * is initiated, or can be freed with a call to
 * uCellSecTlsCipherSuiteListLast()).  This function is not
 * thread-safe in that there is a single list for any given
 * security context.
 * The SARA-U201 and SARA-R4xx modules do not support this feature.
 *
 * @param[in] pContext a pointer to the security context.
 * @return             the IANA number of the cipher suite or
 *                     negative error code.
 */
int32_t uCellSecTlsCipherSuiteListNext(uCellSecTlsContext_t *pContext);

/** It is good practice to call this to clear up memory from
 * uCellSecTlsCipherSuiteListFirst() if you are not going to
 * iterate through the whole list with
 * uCellSecTlsCipherSuiteListNext().
 *
 * @param[in] pContext a pointer to the security context.
 */
void uCellSecTlsCipherSuiteListLast(uCellSecTlsContext_t *pContext);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC SETTINGS
 * -------------------------------------------------------------- */

/** Set the minimum [D]TLS version to use. If this is not called
 * the version will be "any".
 *
 * @param[in] pContext   a pointer to the security context.
 * @param tlsVersionMin  the [D]TLS version encoded as an integer
 *                       where 0 = any, 10 = 1.0, 11 = 1.1 and
 *                       12 = 1.2.
 * @return               zero on success else negative error code.
 */
int32_t uCellSecTlsVersionSet(const uCellSecTlsContext_t *pContext,
                              int32_t tlsVersionMin);

/** Get the minimum [D]TLS version in use.
 *
 * @param[in] pContext a pointer to the security context.
 * @return             on success the [D]TLS version (see
 *                     uCellSecTlsVersionSet() for encoding), else
 *                     negative error code.
 */
int32_t uCellSecTlsVersionGet(const uCellSecTlsContext_t *pContext);

/** Set the type of checking to perform on certificates
 * received from the server. If this is not called the certificate
 * checking carried out will depend upon the module: for SARA-R5
 * and SARA-R422 root CA checking is conducted, on all other cellular
 * modules no checking is conducted.
 *
 * @param[in] pContext a pointer to the security context.
 * @param check        the certificate checks to perform.
 * @param[in] pUrl     if URL checking is included this must
 *                     be the null-terminated server URL
 *                     to check against, else it must be NULL;
 *                     should be no longer than
 *                     #U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES
 *                     bytes long, excluding the terminator.
 * @return             zero on success else negative error
 *                     code.
 */
int32_t uCellSecTlsCertificateCheckSet(const uCellSecTlsContext_t *pContext,
                                       uCellSecTlsCertficateCheck_t check,
                                       const char *pUrl);

/** Get the type of checking being performed on certificates
 * received from the server.
 *
 * @param[in] pContext a pointer to the security context.
 * @param[out] pUrl    a pointer to a place to store the
 *                     null-terminated server URL being checked
 *                     against; only populated if URL-checking
 *                     is included, may be NULL.
 * @param size         the amount of storage at pUrl. To ensure
 *                     no loss
 *                     #U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES + 1
 *                     bytes should be allowed.
 * @return             the certificate check being performed
 *                     or negative error code.
 */
int32_t uCellSecTlsCertificateCheckGet(const uCellSecTlsContext_t *pContext,
                                       char *pUrl, size_t size);

/** Set the optional Server Name Indication string which
 * can be used during TLS negotiation. If this is not called no
 * Server Name Indication checking will be carried out.
 * The SARA-U201 and SARA-R4xx modules do not support this feature.
 *
 * @param[in] pContext a pointer to the security context.
 * @param[in] pSni     the null-terminated server name
 *                     indication string to check against,
 *                     should be no longer than
 *                     #U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES
 *                     bytes long, excluding the terminator;
 *                     use NULL to cancel any existing Server
 *                     Name Indication checking.
 * @return             zero on success else negative error
 *                     code.
 */
int32_t uCellSecTlsSniSet(const uCellSecTlsContext_t *pContext,
                          const char *pSni);

/** Get the optional Server Name Indication string which is
 * being used during TLS negotiation.
 * The SARA-U201 and SARA-R4xx modules do not support this feature.
 *
 * @param pContext   a pointer to the security context.
 * @param[out] pSni  a pointer to a place to store the
 *                   null-terminated Server Name Indication
 *                   string.
 * @param size       the amount of storage at pSni; to ensure
 *                   no loss
 *                   #U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES + 1
 *                   bytes should be allowed.
 * @return           on success the length of the string
 *                   stored at pSni (i.e. what strlen() would
 *                   return) else negative error code.
 */
int32_t uCellSecTlsSniGet(const uCellSecTlsContext_t *pContext,
                          char *pSni, size_t size);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_SEC_TLS_H_

// End of file
