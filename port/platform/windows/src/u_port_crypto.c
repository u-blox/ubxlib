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
 * @brief Implementation of the crypto API on Windows.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "windows.h"
#include "bcrypt.h"
#pragma comment(lib, "bcrypt.lib")

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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    BCRYPT_ALG_HANDLE algorithmHandle = NULL;
    BCRYPT_HASH_HANDLE hashHandle  = NULL;

    // Open an algorithm handle for SHA256
    if (BCryptOpenAlgorithmProvider(&algorithmHandle,
                                    BCRYPT_SHA256_ALGORITHM,
                                    NULL, 0) >= 0) {
        // Create the hash object, letting Windows allocate
        // the memory for it
        if (BCryptCreateHash(algorithmHandle, &hashHandle,
                             NULL, 0, NULL, 0, 0) >= 0) {
            // Now actually hash the data
            if (BCryptHashData(hashHandle, (PBYTE) pInput,
                               inputLengthBytes, 0) >= 0) {
                // Finish the hash to get the result
                if (BCryptFinishHash(hashHandle, (PUCHAR) pOutput,
                                     U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES,
                                     0) >= 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
            // Destroy the hash object
            BCryptDestroyHash(hashHandle);
        }
        // Free the algorithm handle
        BCryptCloseAlgorithmProvider(algorithmHandle, 0);
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    BCRYPT_ALG_HANDLE algorithmHandle = NULL;
    BCRYPT_HASH_HANDLE hashHandle  = NULL;

    // Open an algorithm handle for SHA256 with HMAC
    if (BCryptOpenAlgorithmProvider(&algorithmHandle,
                                    BCRYPT_SHA256_ALGORITHM,
                                    NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) >= 0) {
        // Create the hash object, letting Windows allocate
        // the memory for it, and add the key
        if (BCryptCreateHash(algorithmHandle, &hashHandle,
                             NULL, 0, pKey, keyLengthBytes,
                             0) >= 0) {
            // Now actually hash the data
            if (BCryptHashData(hashHandle, (PBYTE) pInput,
                               inputLengthBytes, 0) >= 0) {
                // Finish the hash to get the result
                if (BCryptFinishHash(hashHandle, (PUCHAR) pOutput,
                                     U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES,
                                     0) >= 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
            // Destroy the hash object
            BCryptDestroyHash(hashHandle);
        }
        // Free the algorithm handle
        BCryptCloseAlgorithmProvider(algorithmHandle, 0);
    }

    return errorCode;
}

// Perform AES 128 CBC encryption of a block of data.
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    BCRYPT_ALG_HANDLE algorithmHandle = NULL;
    BCRYPT_KEY_HANDLE keyHandle  = NULL;
    DWORD resultLength = 0;

    // Open an algorithm handle for AES encryption
    if (BCryptOpenAlgorithmProvider(&algorithmHandle,
                                    BCRYPT_AES_ALGORITHM,
                                    NULL, 0) >= 0) {
        // Set CBC
        if (BCryptSetProperty(algorithmHandle, BCRYPT_CHAINING_MODE,
                              (PBYTE) BCRYPT_CHAIN_MODE_CBC,
                              sizeof(BCRYPT_CHAIN_MODE_CBC),
                              0) >= 0) {
            // Generate the key object, letting Windows create the
            // allocation of memory for it
            if (BCryptGenerateSymmetricKey(algorithmHandle,
                                           &keyHandle, NULL, 0,
                                           pKey, keyLengthBytes,
                                           0) >= 0) {
                // Now perform the encryption
                if (BCryptEncrypt(keyHandle, (PUCHAR) pInput,
                                  lengthBytes, NULL,
                                  pInitVector,
                                  U_PORT_CRYPTO_AES128_INITIALISATION_VECTOR_LENGTH_BYTES,
                                  (PUCHAR) pOutput, lengthBytes,
                                  &resultLength, 0) >= 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
            // Destroy the key object
            BCryptDestroyKey(keyHandle);
        }
        // Free the algorithm handle
        BCryptCloseAlgorithmProvider(algorithmHandle, 0);
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    BCRYPT_ALG_HANDLE algorithmHandle = NULL;
    BCRYPT_KEY_HANDLE keyHandle  = NULL;
    DWORD resultLength = 0;

    // Open an algorithm handle for AES encryption
    if (BCryptOpenAlgorithmProvider(&algorithmHandle,
                                    BCRYPT_AES_ALGORITHM,
                                    NULL, 0) >= 0) {
        // Set CBC
        if (BCryptSetProperty(algorithmHandle, BCRYPT_CHAINING_MODE,
                              (PBYTE) BCRYPT_CHAIN_MODE_CBC,
                              sizeof(BCRYPT_CHAIN_MODE_CBC),
                              0) >= 0) {
            // Generate the key object, letting Windows create the
            // allocation of memory for it
            if (BCryptGenerateSymmetricKey(algorithmHandle,
                                           &keyHandle, NULL, 0,
                                           pKey, keyLengthBytes,
                                           0) >= 0) {
                // Now perform the decryption
                if (BCryptDecrypt(keyHandle, (PUCHAR) pInput,
                                  lengthBytes, NULL,
                                  pInitVector,
                                  U_PORT_CRYPTO_AES128_INITIALISATION_VECTOR_LENGTH_BYTES,
                                  (PUCHAR) pOutput, lengthBytes,
                                  &resultLength, 0) >= 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
            // Destroy the key object
            BCryptDestroyKey(keyHandle);
        }
        // Free the algorithm handle
        BCryptCloseAlgorithmProvider(algorithmHandle, 0);
    }

    return errorCode;
}

// End of file
