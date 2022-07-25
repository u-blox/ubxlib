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

#ifndef _U_PORT_CRYPTO_H_
#define _U_PORT_CRYPTO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for cryptographic functions, mapped to
 * mbedTLS on most platforms.  These functions are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The size of output buffer required for a SHA256 calculation.
 */
#define U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES 32

/** The size of initialisation vector required for an AES 128
 * calculation.
 */
#define U_PORT_CRYPTO_AES128_INITIALISATION_VECTOR_LENGTH_BYTES 16

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Perform a SHA256 calculation on a block of data.
 *
 * @param pInput           a pointer to the input data; cannot be
 *                         NULL unless inputLengthBytes is zero.
 * @param inputLengthBytes the length of the input data.
 * @param[out] pOutput     a pointer to at least 32 bytes of space
 *                         to which the output will be written.
 * @return                 zero on success else negative error code;
 *                         the error code is returned directly
 *                         from the underlying cryptographic library,
 *                         for the given platform, it is not one from
 *                         the U_ERROR_COMMON_xxx enumeration.
 */
int32_t uPortCryptoSha256(const char *pInput,
                          size_t inputLengthBytes,
                          char *pOutput);

/** Perform a HMAC SHA256 calculation on a block of data.
 *
 * @param pKey             a pointer to the key; cannot be NULL.
 * @param keyLengthBytes   the length of the key.
 * @param[in] pInput       a pointer to the input data; cannot be
 *                         NULL.
 * @param inputLengthBytes the length of the input data.
 * @param[out] pOutput     a pointer to at least 32 bytes of space
 *                         to which the output will be written.
 * @return                 zero on success else negative error code;
 *                         the error code is returned directly
 *                         from the underlying cryptographic library,
 *                         for the given platform, it is not one from
 *                         the U_ERROR_COMMON_xxx enumeration.
 */
int32_t uPortCryptoHmacSha256(const char *pKey,
                              size_t keyLengthBytes,
                              const char *pInput,
                              size_t inputLengthBytes,
                              char *pOutput);

/** Perform AES 128 CBC encryption of a block of data.
 *
 * @param pKey                a pointer to the key; cannot be NULL.
 * @param keyLengthBytes      the length of the key; must be 16, 24
 *                            or 32 bytes.
 * @param[in,out] pInitVector a pointer to the 16 byte initialisation
 *                            vector; cannot be NULL, must be writeable
 *                            and WILL be modified by this function.
 * @param[in] pInput          a pointer to the input data; cannot be
 *                            NULL.
 * @param lengthBytes         the length of the input data; must be
 *                            a multiple of 16 bytes.
 * @param[out] pOutput        a pointer to at least lengthBytes
 *                            bytes of space to which the output will
 *                            be written.
 * @return                    zero on success else negative error code;
 *                            the error code is returned directly
 *                            from the underlying cryptographic library,
 *                            for the given platform, it is not one from
 *                            the U_ERROR_COMMON_xxx enumeration.
 */
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput);

/** Perform AES 128 CBC decryption of a block of data.
 *
 * @param pKey                a pointer to the key; cannot be NULL.
 * @param keyLengthBytes      the length of the key; must be 16, 24
 *                            or 32 bytes.
 * @param[in,out] pInitVector a pointer to the 16 byte initialisation
 *                            vector; cannot be NULL, must be writeable
 *                            and WILL be modified by this function.
 * @param[in] pInput          a pointer to the input data; cannot be
 *                            NULL.
 * @param lengthBytes         the length of the input data; must be
 *                            a multiple of 16 bytes.
 * @param[out] pOutput        a pointer to at least lengthBytes
 *                            bytes of space to which the output will
 *                            be written.
 * @return                    zero on success else negative error code;
 *                            the error code is returned directly
 *                            from the underlying cryptographic library,
 *                            for the given platform, it is not one from
 *                            the U_ERROR_COMMON_xxx enumeration.
 */
int32_t uPortCryptoAes128CbcDecrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_CRYPTO_H_

// End of file
