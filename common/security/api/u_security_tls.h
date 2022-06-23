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

#ifndef _U_SECURITY_TLS_H_
#define _U_SECURITY_TLS_H_

/* No other #includes allowed here.  Also, note that this header file
 * is allowed to be include inside other API header files which
 * hide it (e.g. u_sock_security.h) and hence it is important
 * that this fle is self-contained, must not drag in any
 * types aside from the primitive ones except uDeviceHandle_t.
 */

#include "u_device.h"

/** \addtogroup security
 *  @{
 */

/** @file
 * @brief This header file defines the types for configuring
 * an SSL/[D]TLS session for use e.g. on a socket or an MQTT
 * connection etc. TL;DR: look at #uSecurityTlsSettings_t.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SECURITY_TLS_PSK_MAX_LENGTH_BYTES
/** The maximum length of a PSK array
 * (binary, not hex encoded as ASCII).
 */
# define U_SECURITY_TLS_PSK_MAX_LENGTH_BYTES 64
#endif

#ifndef U_SECURITY_TLS_PSK_ID_MAX_LENGTH_BYTES
/** The maximum length of a PSK ID array
 * (binary, not hex encoded as ASCII).
 */
# define U_SECURITY_TLS_PSK_ID_MAX_LENGTH_BYTES 128
#endif

#ifndef U_SECURITY_TLS_MAX_NUM_CIPHER_SUITES
/** The maximum number of cipher suites that can be chosen (where a
 * choice of cipher suites is supported).
 */
# define U_SECURITY_TLS_MAX_NUM_CIPHER_SUITES 10
#endif

#ifndef U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES
/** The maximum length of the expected server URL string.
 */
# define U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES 256
#endif

#ifndef U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES
/** The maximum length of the optional SNI string used during
 * TLS negotiation.
 */
# define U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES 128
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** TLS version.
 * NOTE: the values used here are chosen to map to directly to
 * u_cell_secs_tls.h without the need for conversion.
 */
typedef enum {
    U_SECURITY_TLS_VERSION_ANY = 0,
    U_SECURITY_TLS_VERSION_1_0 = 10,
    U_SECURITY_TLS_VERSION_1_1 = 11,
    U_SECURITY_TLS_VERSION_1_2 = 12,
    U_SECURITY_TLS_VERSION_MAX_NUM
} uSecurityTlsVersion_t;

/** The types of certificate checking that can be performed.
 * NOTE: this must use the same values as the equivalent enum in
 * u_cell_secs_tls.h.
 */
typedef enum {
    U_SECURITY_TLS_CERTIFICATE_CHECK_NONE              = 0x00, /**< no checking. */
    U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA           = 0x01, /**< check root CA. */
    U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA_URL       = 0x02, /**< check root CA and URL,
                                                                    only supported on cellular modules. */
    U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA_URL_DATE  = 0x03, /**< check root CA, URL and expiry date,
                                                                    only supported on cellular modules. */
    U_SECURITY_TLS_CERTIFICATE_CHECK_MAX_NUM
} uSecurityTlsCertficateCheck_t;

/** The types of cipher suites that can be chosen where a choice
 * of cipher suites is supported.  The number is the IANA designation
 * with the upper byte being byte 1 and the lower byte byte 2.
 * Not all u-blox modules support all versions, consult the security
 * section of your u-blox module AT manual for further information.
 */
typedef enum {
    U_SECURITY_TLS_CIPHER_SUITE_NULL_WITH_NULL_NULL                      = 0x0000,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_3DES_EDE_CBC_SHA5               = 0x000A,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_DSS_WITH_3DES_EDE_CBC_SHA            = 0x0013,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_DES_CBC_SHA                 = 0x0015,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_3DES_EDE_CBC_SHA            = 0x0016,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_DES_CBC_SHA                 = 0x001A,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_3DES_EDE_CBC_SHA            = 0x001B,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_128_CBC_SHA1                = 0x002F,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_DSS_WITH_AES_128_CBC_SHA             = 0x0032,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_128_CBC_SHA             = 0x0033,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_AES_128_CBC_SHA             = 0x0034,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_256_CBC_SHA                 = 0x0035,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_256_CBC_SHA             = 0x0039,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_AES_256_CBC_SHA             = 0x003A,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_128_CBC_SHA256              = 0x003C,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_256_CBC_SHA256              = 0x003D,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_DSS_WITH_AES_128_CBC_SHA256          = 0x0040,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_128_CBC_SHA            = 0x0041,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA        = 0x0045,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_128_CBC_SHA256          = 0x0067,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_256_CBC_SHA256          = 0x006B,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_AES_128_CBC_SHA256          = 0x006C,
    U_SECURITY_TLS_CIPHER_SUITE_DH_anon_WITH_AES_256_CBC_SHA256          = 0x006D,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_256_CBC_SHA            = 0x0084,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA        = 0x0088,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_RC4_128_SHA                     = 0x008A,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_3DES_EDE_CBC_SHA                = 0x008B,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_128_CBC_SHA                 = 0x008C,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_256_CBC_SHA                 = 0x008D,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_RC4_128_SHA                 = 0x008E,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_3DES_EDE_CBC_SHA            = 0x008F,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_128_CBC_SHA             = 0x0090,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_256_CBC_SHA             = 0x0091,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_RC4_128_SHA                 = 0x0092,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_3DES_EDE_CBC_SHA            = 0x0093,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_128_CBC_SHA             = 0x0094,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_256_CBC_SHA             = 0x0095,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_128_GCM_SHA256              = 0x009C,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_256_GCM_SHA384              = 0x009D,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_128_GCM_SHA256          = 0x009E,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_256_GCM_SHA384          = 0x009F,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_128_GCM_SHA256              = 0x00A8,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_256_GCM_SHA384              = 0x00A9,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_128_GCM_SHA256          = 0x00AA,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_256_GCM_SHA384          = 0x00AB,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_128_GCM_SHA256          = 0x00AC,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_256_GCM_SHA384          = 0x00AD,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_128_CBC_SHA256              = 0x00AE,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_256_CBC_SHA384              = 0x00AF,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_128_CBC_SHA256          = 0x00B2,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_256_CBC_SHA384          = 0x00B3,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_128_CBC_SHA256          = 0x00B6,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_AES_256_CBC_SHA384          = 0x00B7,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_128_CBC_SHA256         = 0x00BA,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256     = 0x00BE,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_256_CBC_SHA256         = 0x00C0,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256     = 0x00C4,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_RC4_128_SHA              = 0xC002,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA         = 0xC003,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_128_CBC_SHA          = 0xC004,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_256_CBC_SHA          = 0xC005,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_RC4_128_SHA             = 0xC007,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA        = 0xC008,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_128_CBC_SHA         = 0xC009,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_256_CBC_SHA         = 0xC00A,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_RC4_128_SHA                = 0xC00C,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_3DES_EDE_CBC_SHA           = 0xC00D,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_128_CBC_SHA            = 0xC00E,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_256_CBC_SHA            = 0xC00F,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_NULL_SHA                  = 0xC010,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_RC4_128_SHA               = 0xC011,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA          = 0xC012,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_128_CBC_SHA           = 0xC013,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_CBC_SHA           = 0xC014,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_anon_WITH_3DES_EDE_CBC_SHA          = 0xC017,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_anon_WITH_AES_128_CBC_SHA           = 0xC018,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_anon_WITH_AES_256_CBC_SHA           = 0xC019,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256      = 0xC023,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384      = 0xC024,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_128_CBC_SHA256       = 0xC025,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_256_CBC_SHA384       = 0xC026,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_128_CBC_SHA256        = 0xC027,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_CBC_SHA384        = 0xC028,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_128_CBC_SHA256         = 0xC029,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_256_CBC_SHA384         = 0xC02A,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256      = 0xC02B,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384      = 0xC02C,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_128_GCM_SHA256       = 0xC02D,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_AES_256_GCM_SHA384       = 0xC02E,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_128_GCM_SHA256        = 0xC02F,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384        = 0xC030,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_128_GCM_SHA256         = 0xC031,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_AES_256_GCM_SHA384         = 0xC032,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_RC4_128_SHA               = 0xC033,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA          = 0xC034,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_AES_128_CBC_SHA           = 0xC035,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_AES_256_CBC_SHA           = 0xC036,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_AES_128_CBC_SHA256        = 0xC037,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_AES_256_CBC_SHA384        = 0xC038,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 = 0xC072,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 = 0xC073,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256  = 0xC074,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384  = 0xC075,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   = 0xC076,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384   = 0xC077,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256    = 0xC078,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384    = 0xC079,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_128_GCM_SHA256         = 0xC07A,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_CAMELLIA_256_GCM_SHA384         = 0xC07B,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256     = 0xC07C,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384     = 0xC07D,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256 = 0xC086,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384 = 0xC087,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256  = 0xC088,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384  = 0xC089,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256   = 0xC08A,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384   = 0xC08B,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256    = 0xC08C,
    U_SECURITY_TLS_CIPHER_SUITE_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384    = 0xC08D,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_CAMELLIA_128_GCM_SHA256         = 0xC08E,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_CAMELLIA_256_GCM_SHA384         = 0xC08F,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256     = 0xC090,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384     = 0xC091,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256     = 0xC092,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384     = 0xC093,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_CAMELLIA_128_CBC_SHA256         = 0xC094,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_CAMELLIA_256_CBC_SHA384         = 0xC095,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256     = 0xC096,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384     = 0xC097,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256     = 0xC098,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384     = 0xC099,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256   = 0xC09A,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384   = 0xC09B,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_128_CCM                     = 0xC09C,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_256_CCM                     = 0xC09D,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_128_CCM                 = 0xC09E,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_256_CCM                 = 0xC09F,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_128_CCM_8                   = 0xC0A0,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_WITH_AES_256_CCM_8                   = 0xC0A1,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_128_CCM_8               = 0xC0A2,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_AES_256_CCM_8               = 0xC0A3,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_128_CCM                     = 0xC0A4,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_256_CCM                     = 0xC0A5,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_128_CCM                 = 0xC0A6,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_AES_256_CCM                 = 0xC0A7,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_128_CCM_8                   = 0xC0A8,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_AES_256_CCM_8                   = 0xC0A9,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_DHE_WITH_AES_128_CCM_8               = 0xC0AA,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_DHE_WITH_AES_256_CCM_8               = 0xC0AB,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_128_CCM             = 0xC0AC,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_256_CCM             = 0xC0AD,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_128_CCM_8           = 0xC0AE,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_AES_256_CCM_8           = 0xC0AF,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_CHACHA20_POL1305_SHA256   = 0xCCA8,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_ECDSA_WITH_CHACHA20_POL1305_SHA256 = 0xCCA9,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_RSA_WITH_CHACHA20_POL1305_SHA256     = 0xCCAA,
    U_SECURITY_TLS_CIPHER_SUITE_PSK_WITH_CHACHA20_POL1305_SHA256         = 0xCCAB,
    U_SECURITY_TLS_CIPHER_SUITE_ECDHE_PSK_WITH_CHACHA20_POL1305_SHA256   = 0xCCAC,
    U_SECURITY_TLS_CIPHER_SUITE_DHE_PSK_WITH_CHACHA20_POL1305_SHA256     = 0xCCAD,
    U_SECURITY_TLS_CIPHER_SUITE_RSA_PSK_WITH_CHACHA20_POL1305_SHA256     = 0xCCAE
} uSecurityTlsCipherSuiteIana_t;

/** A choice of cipher suites: supported on cellular modules only.
 */
typedef struct {
    size_t num; /**< the number of valid entries in the
                     cipherSuites array; set to 0 for
                     automatic selection. */
    uSecurityTlsCipherSuiteIana_t suite[U_SECURITY_TLS_MAX_NUM_CIPHER_SUITES];
} uSecurityTlsCipherSuites_t;

/** A structure to describe a binary sequence.
 */
typedef struct {
    char *pBin;   /**< pointer to the binary data. */
    size_t size; /**< the number of bytes of data at pBin (not including
                      the null-terminator if pBin is a string). */
} uSecurityTlsBinary_t;

/** Structure describing the security configuration for a [D]TLS session.
 * Fields marked as supported only on cellular modules will be ignored
 * by BLE/Wifi modules.
 * IMPORTANT: whenever this structure is instantiated it should be assigned
 * to #U_SECURITY_TLS_SETTINGS_DEFAULT to ensure that the correct default
 * settings are applied.
 * If this structure is updated #U_SECURITY_TLS_SETTINGS_DEFAULT must be
 * updated as well.
 */
typedef struct {
    uSecurityTlsVersion_t tlsVersionMin; /**< the minimum [D]TLS version to use;
                                              DTLS is only supported on cellular
                                              modules. */
    const char *pRootCaCertificateName;   /**< the null-terminated name of
                                               the root X.509 certificate,
                                               as stored using
                                               uSecurityCredentialStore(). */
    const char *pClientCertificateName;   /**< the null-terminated name of the
                                               client X.509 certificate, as stored
                                               using uSecurityCredentialStore();
                                               see also useDeviceCertificate below.*/
    const char *pClientPrivateKeyName;    /**< the null-terminated name of the
                                               client private key, as stored using
                                               uSecurityCredentialStore(). */
    uSecurityTlsCertficateCheck_t certificateCheck; /**< the type of certificate
                                                         checking to perform. */

    /* The options from here onwards are supported on cellular modules only. */

    const char *pClientPrivateKeyPassword; /**< where required, NULL if not required;
                                                this field is supported on cellular
                                                modules only. */
    uSecurityTlsCipherSuites_t cipherSuites; /** supported on cellular modules only;
                                                 on all other modules the choice is
                                                 made automatically by the module. */
    uSecurityTlsBinary_t psk; /**< the pre-shared key as a binary sequence or
                                   an ASCII string (not hex encoded), maximum length
                                   #U_SECURITY_TLS_PSK_MAX_LENGTH_BYTES;
                                   supported on cellular modules only and, on
                                   all currently supported modules, no ASCII
                                   control characters may be included. */
    uSecurityTlsBinary_t pskId; /**< the pre-shared key ID as a binary sequence or
                                     an ASCII string (not hex-encoded), maximum length
                                     #U_SECURITY_TLS_PSK_ID_MAX_LENGTH_BYTES;
                                     supported on cellular modules only and, on
                                     all currently supported modules, no ASCII
                                     control characters may be included. */
    bool pskGeneratedByRoT; /**< if set to true then the root of trust inside the
                                 module will generate the pre-shared key and
                                 associated ID as part of the u-blox security
                                 service.  pPsk and pPskId are ignored if this is
                                 set to true.  Supported only on cellular modules
                                 that include u-blox security. */
    const char *pExpectedServerUrl; /**< the expected URL of the server, must be non-NULL
                                         if the value of certificateCheck includes a URL
                                         check, max length
                                         #U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES,
                                         otherwise must be NULL; supported on cellular
                                         modules only. */
    const char *pSni; /**< the Server Name Indication string used during TLS
                           negotiation, maximum length #U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES;
                           this is optional on cellular modules while for Wifi modules it
                           is set automatically if the connect string is a URL. */
    bool enableSessionResumption; /**< set to true to enable session resumption; currently
                                       only false is supported. */
    bool useDeviceCertificate; /**< if this is set to true then pClientCertificateName should
                                    be set to NULL and instead, for a module that supports
                                    u-blox security and has been security sealed, the device
                                    public X.509 certificate that was generated during the
                                    sealing process is used instead; currently supported on
                                    some cellular modules only, see the AT+USECPRF=14 command
                                    in the AT manual for you module for further information. */
    bool includeCaCertificates; /**< if useDeviceCertificate is true then setting this to
                                     true will cause the X.509 certificates of the certificate
                                     authorities that were used to sign the device certificates
                                     at sealing to ALSO be included; currently supported on
                                     cellular modules only. */
} uSecurityTlsSettings_t;

/** The default settings for security: whenever uSecurityTlsSettings_t
 * is instantiated it should be assigned to this to ensure that the
 * correct default settings are applied.
 */
#define U_SECURITY_TLS_SETTINGS_DEFAULT {U_SECURITY_TLS_VERSION_ANY, /* tlsVersion */ \
                                         NULL, /* Root CA name */               \
                                         NULL, /* Client CA name */             \
                                         NULL, /* Private key name */           \
                                         U_SECURITY_TLS_CERTIFICATE_CHECK_NONE, \
                                         NULL, /* Private key PW */             \
                                         {0},  /* Cipher suites */              \
                                         {NULL, 0}, /* PSK */                   \
                                         {NULL, 0}, /* PSK ID */                \
                                         false, /* pskGeneratedByRoT */         \
                                         NULL, /* Expected server URL */        \
                                         NULL, /* SNI */                        \
                                         false, /* Session resumption */        \
                                         false, /* use device certificate */    \
                                         false}; /* include CA certificates */

/** Security context structure.
 */
typedef struct uSecurityTlsContext_t {
    int32_t errorCode;      /**< zero if this is a valid security context,
                                 else negative error code. */
    uDeviceHandle_t devHandle;  /**< the network handle with which this security
                                 context is associated. */
    void *pNetworkSpecific; /**< pointer to a network-specific context structure
                                 which will be passed to the BLE/Cellular/Wifi
                                 layer (appropriately cast) when this security
                                 context is used. */
} uSecurityTlsContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: FOR INTERNAL USE ONLY
 * -------------------------------------------------------------- */

/** Create a TLS security context for the given network.  This
 * function is thread-safe.
 * IMPORTANT: this function is NOT INTENDED FOR CUSTOMER USE.  It is
 * called internally by the ubxlib APIs (e.g. sock, MQTT) in order
 * to configure security for a TLS session.
 * ALSO IMPORTANT: if security configuration fails it will STILL
 * return a pointer to a structure containing the error code and
 * hence uSecurityTlsRemove() must ALWAYS be called afterwards to
 * clean this up, even on failure.
 *
 * @param devHandle     the handle of the device which the
 *                      TLS security context is associated, e.g.
 *                      obtained using uDeviceOpen().
 * @param pSettings     a pointer to the TLS security settings
 *                      to use.  May be NULL in which case default
 *                      settings are applied; consult the security
 *                      section of your u-blox module AT manual to
 *                      determine what the default settings are,
 *                      but typically they will offer no validation
 *                      of the server, only encryption of data.
 * @return              a pointer to a TLS security context which
 *                      includes the error code, EVEN ON FAILURE.
 *                      On success the errorCode field of the
 *                      returned structure will be zero, else it
 *                      will be a negative error code.
 */
uSecurityTlsContext_t *pUSecurityTlsAdd(uDeviceHandle_t devHandle,
                                        const uSecurityTlsSettings_t *pSettings);

/** Free the given security context.  This function is thread-safe.
 * IMPORTANT: this function is NOT INTENDED FOR CUSTOMER USE.  It is
 * called internally by the ubxlib APIs (e.g. sock, MQTT) in order
 * to free a given TLS security context.
 *
 * @param pContext the TLS security context, as returned by
 *                 pUSecurityTlsAdd().
 */
void uSecurityTlsRemove(uSecurityTlsContext_t *pContext);

/** Clean-up memory from TLS security contexts.
 * pUSecurityTlsAdd() creates a mutex, if not already created,
 * to ensure thread-safety.  This function may be called if
 * you're completely done with TLS security in order to free
 * the memory held by that mutex once more.  This function
 * should not be called at the same time as any of the other
 * functions in this API.
 */
void uSecurityTlsCleanUp();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SECURITY_TLS_H_

// End of file
