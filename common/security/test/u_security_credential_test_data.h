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

#ifndef _U_SECURITY_CREDENTIAL_TEST_DATA_H_
#define _U_SECURITY_CREDENTIAL_TEST_DATA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief  Test data for the u-blox security credential API testing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The subject ("Common Name") field of gSecurityCredentialTestCertX509.
 */
#define U_SECURITY_CREDENTIAL_TEST_X509_SUBJECT "ubxlib client"

/** The UTC expiration date of gSecurityCredentialTestCertX509.
 */
#define U_SECURITY_CREDENTIAL_TEST_X509_EXPIRATION_UTC 1770657548

/** The passphrase for the security keys.
 */
//lint -esym(755, U_SECURITY_CREDENTIAL_TEST_PASSPHRASE) Suppress
// macro not referenced.
#define U_SECURITY_CREDENTIAL_TEST_PASSPHRASE "ubxlib"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifdef U_SECURITY_CREDENTIAL_TEST_FORMATS
// Type used to form a table of all possible formats/encodings of
// credentials, used to explore what works on a given module.
typedef struct {
    //lint -e768 Suppress not referenced (it won't be if logging is off)
    const char *pDescription;
    uSecurityCredentialType_t type;
    const size_t size;
    const char *pPassword;
    const uint8_t contents[1024];
} uSecurityCredentialFormatTest_t;
#endif

/* ----------------------------------------------------------------
 * VARIABLES: 1024-BIT KEY
 * -------------------------------------------------------------- */

/** A test PKCS1-encoded PEM-format encrypted private key.
 */
extern const uint8_t gUSecurityCredentialTestKey1024Pkcs1Pem[];

/** Number of items in the gUSecurityCredentialTestKey1024Pkcs1Pem
 * array, has to be done in this file and externed or GCC complains
 * about asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestKey1024Pkcs1PemSize;

/** An unencrypted version of gUSecurityCredentialTestKey1024Pkcs1Pem.
 */
extern const uint8_t gUSecurityCredentialTestKey1024Pkcs1PemNoPass[];

/** Number of items in the gUSecurityCredentialTestKey1024Pkcs1PemNoPass
 * array, has to be done in this file and externed or GCC complains
 * about asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestKey1024Pkcs1PemNoPassSize;

/** An PKCS8-encoded, encrypted, version of
 * gUSecurityCredentialTestKey1024Pkcs1Pem.
 */
extern const uint8_t gUSecurityCredentialTestKey1024Pkcs8Pem[];

/** Number of items in the gUSecurityCredentialTestKey1024Pkcs8Pem
 * array, has to be done in this file and externed or GCC complains
 * about asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestKey1024Pkcs8PemSize;

/* ----------------------------------------------------------------
 * VARIABLES: CERTIFICATES
 * -------------------------------------------------------------- */

/** A test root/CA X.509 certificate.
 */
extern const uint8_t gUSecurityCredentialTestRootCaX509Pem[];

/** Number of items in the gUSecurityCredentialTestRootCaX509Pem array,
 * has to be done in this file and externed or GCC complains about
 * asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestRootCaX509PemSize;

/** A test client X.509 certificate.
 */
extern const uint8_t gUSecurityCredentialTestClientX509Pem[];

/** Number of items in the gUSecurityCredentialTestClientX509Pem array,
 * has to be done in this file and externed or GCC complains about
 * asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestClientX509PemSize;


#ifdef U_SECURITY_CREDENTIAL_TEST_FORMATS

/* ----------------------------------------------------------------
 * VARIABLES: FORMAT TEST DATA
 * -------------------------------------------------------------- */

/** A table of all possible formats/encodings of credentials, used
 * to explore what is supported by a given module.
 */
extern const uSecurityCredentialFormatTest_t gUSecurityCredentialTestFormat[];

/** Number of items in the gUSecurityCredentialTestFormat array,
 * has to be done in this file and externed or GCC complains about
 * asking for the size of a partially defined type.
 */
extern const size_t gUSecurityCredentialTestFormatSize;

#endif

#ifdef __cplusplus
}
#endif

#endif // _U_SECURITY_CREDENTIAL_TEST_DATA_H_

// End of file
