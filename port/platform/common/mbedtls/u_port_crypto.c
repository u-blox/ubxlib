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

/** @file
 * @brief Implementation of the crypto API using mbedTLS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "mbedtls/sha256.h"
#include "mbedtls/md.h"
#include "mbedtls/aes.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_crypto.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Perform a SHA256 calculation on a block of data.
int32_t uPortCryptoSha256(const char *pInput,
                          size_t inputLengthBytes,
                          char *pOutput)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Have to do this the long way since NRF5 doesn't have
    // the newer mbedtls_sha256_ret() function which just
    // does the lot for us, including the error checking

    if (((pInput != NULL) || (inputLengthBytes == 0)) &&
        (pOutput != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        mbedtls_sha256((const unsigned char *) pInput,
                       inputLengthBytes,
                       (unsigned char *) pOutput, 0);
    }

    return errorCode;
}

// Perform a HMAC SHA256 calculation on a block of data.
int32_t uPortCryptoHmacSha256(const char *pKey,
                              size_t keyLengthBytes,
                              const char *pInput,
                              size_t inputLengthBytes,
                              char *pOutput)
{
    // mbedTLS has it sorted
    const mbedtls_md_info_t *pInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return mbedtls_md_hmac(pInfo,
                           (const unsigned char *) pKey,
                           keyLengthBytes,
                           (const unsigned char *) pInput,
                           inputLengthBytes,
                           (unsigned char *) pOutput);
}

// Perform AES 128 CBC encryption of a block of data.
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    int32_t errorCode;
    mbedtls_aes_context context;

    errorCode = mbedtls_aes_setkey_enc(&context,
                                       (const unsigned char *) pKey,
                                       keyLengthBytes * 8);
    if (errorCode == 0) {
        errorCode = mbedtls_aes_crypt_cbc(&context,
                                          MBEDTLS_AES_ENCRYPT,
                                          lengthBytes,
                                          (unsigned char *) pInitVector,
                                          (const unsigned char *) pInput,
                                          (unsigned char *) pOutput);
    }

    return errorCode;
}

// Perform AES 128 CBC decryption of a block of data.
int32_t uPortCryptoAes128CbcDecrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    int32_t errorCode;
    mbedtls_aes_context context;

    errorCode = mbedtls_aes_setkey_dec(&context,
                                       (const unsigned char *) pKey,
                                       keyLengthBytes * 8);
    if (errorCode == 0) {
        errorCode = mbedtls_aes_crypt_cbc(&context,
                                          MBEDTLS_AES_DECRYPT,
                                          lengthBytes,
                                          (unsigned char *) pInitVector,
                                          (const unsigned char *) pInput,
                                          (unsigned char *) pOutput);
    }

    return errorCode;
}

// End of file
