/*
 * Copyright 2019-2022 u-blox Ltd
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

#ifndef _U_BASE64_H_
#define _U_BASE64_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __utils __Utilities
 *  @{
 */

/** @file
 * @brief This header file defines base64 encode and decode functions.
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

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Perform a base 64 encode.
 *
 * @param pBinary           the binary data to be encoded.
 * @param binaryLengthBytes the amount of binary data.
 * @param pBase64           a place to store the base 64 encoded
 *                          data; set this to NULL to simply
 *                          obtain the length that the encoded data
 *                          would occupy without doing an encoding.
 *                          Note that no null-terminator is included.
 * @param base64LengthBytes the amount of storage at pBase64.
 * @return                  the number of bytes stored at pBase64
 *                          or the number of bytes that _would_ be
 *                          stored at pBase64 if it were not NULL.
 */
int32_t uBase64Encode(const char *pBinary, size_t binaryLengthBytes,
                      char *pBase64, size_t base64LengthBytes);

/** Perform a base 64 decode.
 *
 * @param pBase64           the base 64 data to be decoded.
 * @param base64LengthBytes the amount of base 64 data to decode.
 * @param pBinary           a place to store the decoded data;
 *                          set this to NULL to simply obtain the
 *                          length that the decoded data would occupy
 *                          without doing any decoding.  Since
 *                          the decoded binary length is less than
 *                          the original base 64 you _can_ put the
 *                          value of pBase64 here to decode back into
 *                          the same buffer.
 * @param binaryLengthBytes the amount of storage at pBinary.
 * @return                  the number of bytes stored at pBinary
 *                          or the number of bytes that _would_ be
 *                          stored at pBinary if it were not NULL.
 */
int32_t uBase64Decode(const char *pBase64, size_t base64LengthBytes,
                      char *pBinary, size_t binaryLengthBytes);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_BASE64_H_

// End of file
