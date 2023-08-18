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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief This file contains the implementation of helper functions
 * that operate on #uGnssDecUbxNavPvt_t.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "time.h"      // mktime()

#include "u_error_common.h"

#include "u_port_clib_mktime64.h"
#include "u_port.h"

#include "u_gnss_dec_ubx_nav_pvt.h"

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

// Derive Unix time (in nanoseconds) from uGnssDecUbxNavPvt_t.
int64_t uGnssDecUbxNavPvtGetTimeUtc(const uGnssDecUbxNavPvt_t *pPvt)
{
    int64_t timeUtcNanoseconds = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    struct tm tmStruct = {0};

    if (pPvt != NULL) {
        timeUtcNanoseconds = 0;
        // mktime() will spot if any of the elements of tmStruct are
        // invalid, no need to do any range-checking here.
        if (pPvt->valid & (1 << U_GNSS_DEC_UBX_NAV_PVT_VALID_DATE)) {
            // struct tm needs years from 1900
            tmStruct.tm_year = pPvt->year - 1900;
            // struct tm needs months from 0
            tmStruct.tm_mon = pPvt->month - 1;
            tmStruct.tm_mday = pPvt->day;
            if (pPvt->valid & (1 << U_GNSS_DEC_UBX_NAV_PVT_VALID_TIME)) {
                tmStruct.tm_hour = pPvt->hour;
                tmStruct.tm_min = pPvt->min;
                tmStruct.tm_sec = pPvt->sec;
            }
            // Use our own mktime64() as that assumes a UTC input value
            timeUtcNanoseconds = mktime64(&tmStruct);
            if (timeUtcNanoseconds >= 0) {
                timeUtcNanoseconds *= 1000000000;
                timeUtcNanoseconds += pPvt->nano;
            }
        }
    }

    return timeUtcNanoseconds;
}

// End of file
