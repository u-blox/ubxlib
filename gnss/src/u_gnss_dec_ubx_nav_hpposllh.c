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
 * that operate on #uGnssDecUbxNavHpposllh_t.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_gnss_dec_ubx_nav_hpposllh.h"

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

// Derive a high precision position structure from uGnssDecUbxNavHpposllh_t.
void uGnssDecUbxNavHpposllhGetPos(const uGnssDecUbxNavHpposllh_t *pHpposllh,
                                  uGnssDecUbxNavHpposllhPos_t *pPos)
{
    if ((pHpposllh != NULL) && (pPos != NULL)) {
        pPos->longitudeX1e9 = (((int64_t) pHpposllh->lon) * 100) + pHpposllh->lonHp;
        pPos->latitudeX1e9 = (((int64_t) pHpposllh->lat) * 100) + pHpposllh->latHp;
        pPos->heightMillimetresX1e1 = (((int64_t) pHpposllh->height) * 10) + pHpposllh->heightHp;
        pPos->heightMeanSeaLevelMillimetresX1e1 = (((int64_t) pHpposllh->hMSL) * 10) + pHpposllh->hMSLHp;
    }
}

// End of file
