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

#ifndef _U_HEX_BIN_CONVERT_H_
#define _U_HEX_BIN_CONVERT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __utils
 *  @{
 */

/** @file
 * @brief This header file defines functions that convert a buffer
 * that is ASCII hex encoded into a buffer of binary and vice-versa.
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

/** Convert a buffer into the ASCII hex equivalent.
 *
 * @param pBin      a pointer to the binary buffer.
 * @param binLength the number of bytes pointed to by pBin.
 * @param pHex      a pointer to a buffer of length twice
 *                  binLength bytes to store the ASCII hex version.
 * @return          the number of bytes at pHex.
 */
size_t uBinToHex(const char *pBin, size_t binLength, char *pHex);

/** Convert a buffer of ASCII hex into the binary equivalent.
 * If it is not possible to convert character (e.g. because
 * it is not valid ASCII hex) then conversion stops there.
 *
 * @param pHex      a pointer to the ASCII hex data.
 * @param hexLength the number of bytes pointed to by pHex.
 * @param pBin      a pointer to a buffer of length half hexLength
 *                  bytes to store the binary version.
 * @return          the number of bytes at pBin.
 */
size_t uHexToBin(const char *pHex, size_t hexLength, char *pBin);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_HEX_BIN_CONVERT_H_

// End of file
