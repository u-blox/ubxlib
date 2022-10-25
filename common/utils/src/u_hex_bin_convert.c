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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of functions that convert a buffer that is
 * ASCII hex encoded into a buffer of binary and vice-versa.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_assert.h"

#include "u_hex_bin_convert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static const char gHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
                           };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

size_t uBinToHex(const char *pBin, size_t binLength, char *pHex)
{
    U_ASSERT(pHex != NULL);

    for (size_t x = 0; x < binLength; x++) {
        *pHex = gHex[((unsigned char) * pBin) >> 4];
        pHex++;
        *pHex = gHex[*pBin & 0x0f];
        pHex++;
        pBin++;
    }

    return binLength * 2;
}

size_t uHexToBin(const char *pHex, size_t hexLength, char *pBin)
{
    bool success = true;
    size_t length;
    char z[2];

    U_ASSERT(pBin != NULL);

    for (length = 0; (length < hexLength / 2) && success; length++) {
        z[0] = *pHex - '0';
        pHex++;
        z[1] = *pHex - '0';
        pHex++;
        for (size_t y = 0; (y < sizeof(z)) && success; y++) {
            if (z[y] > 9) {
                // Must be A to F or a to f
                z[y] -= 'A' - '0';
                z[y] += 10;
            }
            if (z[y] > 15) {
                // Must be a to f
                z[y] -= 'a' - 'A';
            }
            // Cast here to shut-up a warning under ESP-IDF
            // which appears to have chars as unsigned and
            // hence thinks the first condition is always true
            success = ((signed char) z[y] >= 0) && (z[y] <= 15);
        }
        if (success) {
            *pBin = (char) (((z[0] & 0x0f) << 4) | z[1]);
            pBin++;
        }
    }

    return length;
}

// End of file
