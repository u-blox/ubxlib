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
 * @brief an implementation of mktime().
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.
#include "time.h"      // struct tm

#include "u_port_clib_mktime64.h"

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

// mktime().
//
// IMPORTANT: according to the standard mktime() should consider
// pTm to be _local_ time and return a value in _UTC_, i.e. with the
// known timezone offset (which newlib sets in the system's
// environment with the function tzset()) subtracted from it.
// The implementation below does NOT do that, i.e. pTm is assumed
// to also be UTC, or with a timezone offset of zero.
//
//lint -esym(818, pTm) Suppress could be pointer to
// const, need to follow function signature.
time_t mktime(struct tm *pTm)
{
    return (time_t) mktime64(pTm);
}

// End of file
