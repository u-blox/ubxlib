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

#ifndef _U_SECURITY_CREDENTIAL_H_
#define _U_SECURITY_CREDENTIAL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup security
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox API for X.509 certificate
 * and security key management.  These functions are thread-safe unless
 * otherwise specified in the function description.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum length of the name of an X.509 certificate
 * or security key.  This is the smallest maximum length: longer
 * name lengths may be supported on some modules in which case
 * this length can be overriden; this does NOT include room for
 * a null terminator, any buffer length should be this length
 * plus one.
 */
#ifndef U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES
# define U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES 32
#endif

/** The maximum length of an X.509 certificate or security key.
 * this is the smallest maximum length: longer certificates/keys
 * may be supported on some modules in which case this length
 * can be overriden; this does NOT include room for
 * a null terminator, any buffer length should be this length
 * plus one.
 */
#ifndef U_SECURITY_CREDENTIAL_MAX_LENGTH_BYTES
# define U_SECURITY_CREDENTIAL_MAX_LENGTH_BYTES (1024 * 8)
#endif

/** The maximum length of a security key password.
 * this is the smallest maximum length: longer password lengths
 * may be supported on some modules in which case this length
 * can be overriden; this does NOT include room for
 * a null terminator, any buffer length should be this length
 * plus one.
 */
#ifndef U_SECURITY_CREDENTIAL_PASSWORD_MAX_LENGTH_BYTES
# define U_SECURITY_CREDENTIAL_PASSWORD_MAX_LENGTH_BYTES 64
#endif

/** The maximum length of the subject field of an X.509
 * certificate; this does NOT include room for
 * a null terminator, any buffer length should be this length
 * plus one.
 */
#ifndef U_SECURITY_CREDENTIAL_X509_SUBJECT_MAX_LENGTH_BYTES
# define U_SECURITY_CREDENTIAL_X509_SUBJECT_MAX_LENGTH_BYTES 64
#endif

/** The length of an MD5 hash.
 */
#define U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES 16

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of security credential.  Note that not all u-blox
 * modules support all credential types, consult the security section
 * of your u-blox module AT manual, command AT+USECMNG, for further
 * information.
 */
typedef enum {
    U_SECURITY_CREDENTIAL_ROOT_CA_X509 = 0,
    U_SECURITY_CREDENTIAL_CLIENT_X509 = 1,
    U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE = 2,
    U_SECURITY_CREDENTIAL_SERVER_X509 = 3,
    U_SECURITY_CREDENTIAL_SIGNATURE_VERIFICATION_X509 = 4,
    U_SECURITY_CREDENTIAL_SIGNATURE_VERIFICATION_KEY_PUBLIC = 5,
    U_SECURITY_CREDENTIAL_MAX_NUM,
    U_SECURITY_CREDENTIAL_NONE
} uSecurityCredentialType_t;

/** Structure describing a security credential, used when listing
 * stored credentials.
 */
typedef struct {
    /** The name of the credential. */
    char name[U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES + 1];
    /** The type of the credential. */
    uSecurityCredentialType_t type;
    /** The subject of the X.509 certificate (only present
        for root and client certificates). */
    char subject[U_SECURITY_CREDENTIAL_X509_SUBJECT_MAX_LENGTH_BYTES + 1];
    /** The expiration of the X.509 certificate as a UTC timestamp
        (only present for root and client certificates). */
    int64_t expirationUtc;
} uSecurityCredential_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Store the given X.509 certificate or security key.  For an X.509
 * certificate PEM or DER format may be used; for a security key
 * unencrypted PEM format, specifically with the header
 * "BEGIN RSA PRIVATE KEY" rather than just "BEGIN PRIVATE KEY",
 * should preferably be used since use of a password, or the generic
 * "BEGIN PRIVATE KEY" header, or DER format, for security key storage
 * is not supported by all u-blox modules.  The certificate/key will
 * be converted to DER format inside the module before it is stored.
 *
 * The u-blox module may place limitations on the fields present in
 * an X.509 certificate; consult the security section of your u-blox
 * module AT manual, command AT+USECMNG, for further information.
 *
 * OpenSSL can be used to decrypt a PEM-format security key and
 * write it as PEM-with-RSA-format as follows:
 *
 * openssl rsa -in encrypted_key.pem -out decrypted_key.pem
 *
 * ...or to convert a DER-format security key to PEM-with-RSA-format
 * as follows:
 *
 * openssl rsa -inform DER -in key.der -out decrypted_key.pem
 *
 * ...or to convert an encrypted DER-format security key to
 * PEM-with-RSA-format as follows:
 *
 * openssl pkcs8 -inform DER -in encrypted_key.der -out temp.pem
 * ...then:
 * openssl rsa -in temp.der -out decrypted_key.pem
 *
 * In order to avoid character loss when downloading a security
 * credential it is best if the flow control lines are connected
 * on the interface to the module.
 *
 * @param devHandle            the handle of the instance to be used,
 *                             for example obtained using uDeviceOpen().
 * @param type                 the type of credential to be stored.
 * @param pName                the null-terminated name for the
 *                             X.509 certificate or security key, of
 *                             maximum length
 *                             #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES.
 *                             IMPORTANT: if the name already exists
 *                             then the existing X.509 certificate or
 *                             security key will be overwritten with
 *                             this one.
 * @param pContents            a pointer to the X.509 certificate or
 *                             security key to be stored.
 * @param size                 the number of bytes at pContents,
 *                             maximum value
 *                             #U_SECURITY_CREDENTIAL_MAX_LENGTH_BYTES.
 * @param pPassword            if required, the null-terminated password
 *                             for a PKCS8 encrypted private key, of
 *                             maximum length
 *                             #U_SECURITY_CREDENTIAL_PASSWORD_MAX_LENGTH_BYTES;
 *                             SARA-U201 and SARA-R4xx modules do not support
 *                             use of a password.
 * @param pMd5                 pointer to #U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES
 *                             of storage where the MD5 hash of the DER-format
 *                             credential as stored in the module can be
 *                             placed: this can be stored by the caller and
 *                             used later to verify that the credential is
 *                             unchanged; may be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uSecurityCredentialStore(uDeviceHandle_t devHandle,
                                 uSecurityCredentialType_t type,
                                 const char *pName,
                                 const char *pContents,
                                 size_t size,
                                 const char *pPassword,
                                 char *pMd5);

/** Read the MD5 hash of a stored X.509 certificate or security key
 * to compare with that originally returned by uSecurityCredentialStore().
 * The hash is that of the DER-format key as stored in the module.
 *
 * @param devHandle            the handle of the instance to be used,
 *                             for example obtained using uDeviceOpen().
 * @param type                 the type of credential, as was passed to
 *                             uSecurityCredentialStore() when storing it.
 * @param pName                the null-terminated name for the
 *                             X.509 certificate or security key, as was
 *                             passed to uSecurityCredentialStore()
 *                             when storing it, maximum length
 *                             #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES.
 * @param pMd5                 pointer to #U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES
 *                             of storage for the result.
 * @return                     zero on success else negative error code.
 */
int32_t uSecurityCredentialGetHash(uDeviceHandle_t devHandle,
                                   uSecurityCredentialType_t type,
                                   const char *pName,
                                   char *pMd5);

/** Get the description of the first X.509 certificate or security key
 * from storage; uSecurityCredentialListNext() should be called repeatedly
 * to iterate through subsequent entries in the list.  This function
 * is not thread-safe in that there is a single list of names for any
 * given devHandle    .
 *
 * For instance, to print out the names of all stored credentials:
 *
 * ```
 * uSecurityCredential_t buffer;
 *
 * for (int32_t x = uSecurityCredentialListFirst(handle, &buffer);
 *      x >= 0;
 *      x = uSecurityCredentialListNext(handle, &buffer)) {
 *     printf("%s\n", buffer.name);
 * }
 * ```
 *
 * NOTE: the certificates listed are ONLY those that have been loaded
 * using uSecurityCredentialStore() or were pre-stored in the module.
 * Certificates which have been generated automatically using
 * u-blox security are NOT listed here, please see instead the
 * uSecurityZtp*() APIs in u_security.h.
 *
 * @param devHandle            the handle of the instance to be used,
 *                             for example obtained using uDeviceOpen().
 * @param pCredential          pointer to somewhere to store the result.
 * @return                     the number of credentials in the list
 *                             or negative error code.
 */
int32_t uSecurityCredentialListFirst(uDeviceHandle_t devHandle,
                                     uSecurityCredential_t *pCredential);

/** Return subsequent descriptions of credentials in the list.  Use
 * uSecurityCredentialListFirst() to get the number of entries and
 * return the first result and then call this "number of
 * results" times to read out all of the entries. Calling this
 * "number of results" times will free the memory that held the
 * list after the final call (otherwise it will be freed when the
 * network instance is removed or another listing is initiated, or
 * can be freed with a call to uSecurityCredentialListLast()).
 * This function is not thread-safe in that there is a single list
 * for all threads.
 *
 * NOTE: the certificates listed are ONLY those that have been loaded
 * using uSecurityCredentialStore() or were pre-stored in the module.
 * Certificates which have been generated automatically using
 * u-blox security are NOT listed here, please see instead the
 * uSecurityZtp*() APIs in u_security.h.
 *
 * @param devHandle            the handle of the instance to be used,
 *                             for example obtained using uDeviceOpen().
 * @param pCredential          pointer to somewhere to store the result.
 * @return                     the number of entries remaining *after*
 *                             this one has been read or negative error
 *                             code.
 */
int32_t uSecurityCredentialListNext(uDeviceHandle_t devHandle,
                                    uSecurityCredential_t *pCredential);

/** It is good practice to call this to clear up memory from
 * uSecurityCredentialListFirst() if you are not going to
 * iterate through the whole list with uSecurityCredentialListNext().
 *
 * @param devHandle     the handle of the instance to be used,
 *                      for example obtained using uDeviceOpen().
 */
void uSecurityCredentialListLast(uDeviceHandle_t devHandle);

/** Remove the given X.509 certificate or security key from storage.
 *
 * @param devHandle      the handle of the instance to be used,
 *                       for example obtained using uDeviceOpen().
 * @param type           the type of credential to be removed, as
 *                       was passed to uSecurityCredentialStore()
 *                       when storing it.
 * @param pName          the null-terminated name for the X.509
 *                       certificate or security key to remove, as
 *                       was passed to uSecurityCredentialStore()
 *                       when storing it, maximum length
 *                       #U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES.
 * @return               zero on success else negative error code.
 */
int32_t uSecurityCredentialRemove(uDeviceHandle_t devHandle,
                                  uSecurityCredentialType_t type,
                                  const char *pName);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SECURITY_CREDENTIAL_H_

// End of file
