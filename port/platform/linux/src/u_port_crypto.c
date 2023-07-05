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

/** @file
 * @brief Implementation of the crypto API on Linux using openssl.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "openssl/sha.h"
#include "openssl/aes.h"
#include "openssl/hmac.h"

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

// Ignore warning of: expected ‘unsigned char *’ but argument is of type ‘char *’
#pragma GCC diagnostic ignored "-Wpointer-sign"
// Ignore warning of (for now): is deprecated: Since OpenSSL 3.0
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Perform a SHA256 calculation on a block of data.
int32_t uPortCryptoSha256(const char *pInput,
                          size_t inputLengthBytes,
                          char *pOutput)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, pInput, inputLengthBytes);
    if (SHA256_Final(pOutput, &sha256) == 1) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    return (int32_t)errorCode;
}

// Perform a HMAC SHA256 calculation on a block of data.
int32_t uPortCryptoHmacSha256(const char *pKey,
                              size_t keyLengthBytes,
                              const char *pInput,
                              size_t inputLengthBytes,
                              char *pOutput)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    HMAC_CTX *h = HMAC_CTX_new();
    HMAC_Init_ex(h, pKey, keyLengthBytes, EVP_sha256(), NULL);
    HMAC_Update(h, pInput, inputLengthBytes);
    unsigned int len;
    if (HMAC_Final(h, pOutput, &len) == 1) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    HMAC_CTX_free(h);
    return (int32_t)errorCode;
}

// Perform AES 128 CBC encryption of a block of data.
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    EVP_CIPHER_CTX *ctx;
    ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, pKey, pInitVector);
    EVP_CIPHER_CTX_set_key_length(ctx, keyLengthBytes);
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    unsigned int len;
    EVP_EncryptUpdate(ctx, pOutput, &len, pInput, lengthBytes);
    if (EVP_EncryptFinal_ex(ctx, pOutput + len, &len) == 1) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    EVP_CIPHER_CTX_free(ctx);
    return (int32_t)errorCode;
}

// Perform AES 128 CBC decryption of a block of data.
int32_t uPortCryptoAes128CbcDecrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    EVP_CIPHER_CTX *ctx;
    ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, pKey, pInitVector);
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    EVP_CIPHER_CTX_set_key_length(ctx, keyLengthBytes);
    unsigned int len = lengthBytes + 1;
    EVP_DecryptUpdate(ctx, pOutput, &len, pInput, lengthBytes);
    if (EVP_DecryptFinal_ex(ctx, pOutput + len, &len) == 1) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }
    EVP_CIPHER_CTX_free(ctx);
    return (int32_t)errorCode;
}

// End of file
