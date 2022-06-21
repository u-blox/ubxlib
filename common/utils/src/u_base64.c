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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief functions to assist with time manipulation.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // size_t
#include "stdint.h"    // int32_t etc.

#include "u_base64.h"

// The base64.h implementation we use calls puts() so
// we define it here to remove it
//lint -e(683) Suppress warning about puts() being redefined.
#define puts(x)
#include "base64.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Perform a base 64 encode.
int32_t uBase64Encode(const char *pBinary, size_t binaryLengthBytes,
                      char *pBase64, size_t base64LengthBytes)
{
    int32_t bytesEncoded = 0;

    // Determine the required length of decode buffer by calling
    // base64() with NULL
    //lint -e(712) Suppress loss of precision
    base64(pBinary, binaryLengthBytes,
           (int *) &(bytesEncoded), NULL); // *NOPAD* stop AStyle offsetting &
    if ((pBase64 != NULL) && ((int32_t) base64LengthBytes >= bytesEncoded)) {
        // Now do the encode
        //lint -e(712) Suppress loss of precision
        base64((const void *) pBinary, binaryLengthBytes,
               (int *) &(bytesEncoded), pBase64);  // *NOPAD* stop AStyle offsetting &
    }

    return bytesEncoded;
}

// Perform a base 64 decode.
int32_t uBase64Decode(const char *pBase64, size_t base64LengthBytes,
                      char *pBinary, size_t binaryLengthBytes)
{
    int32_t bytesDecoded = 0;

    // Determine the required length of decode buffer by calling
    // unbase64() with NULL
    //lint -e(712) Suppress loss of precision
    unbase64(pBase64, base64LengthBytes, (int *) & (bytesDecoded),
             NULL);  // *NOPAD* stop AStyle offsetting &
    if ((pBinary != NULL) && ((int32_t) binaryLengthBytes >= bytesDecoded)) {
        // Now do the decode
        //lint -e(712) Suppress loss of precision
        unbase64(pBase64, base64LengthBytes,
                 (int *) &(bytesDecoded), pBinary);  // *NOPAD* stop AStyle offsetting &
    }

    return bytesDecoded;
}

// End of file
