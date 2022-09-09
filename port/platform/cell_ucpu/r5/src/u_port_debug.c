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

/** @file
 * @brief Implementation of the port debug API for the sarar5ucpu platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include <stdio.h> // For vprintf()
#include <stdarg.h> // For va_x()
#include <stdint.h>
#include <stdbool.h>

#include "u_error_common.h"

#include "ucpu_sdk_debug.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Keep track of whether logging is on or off.
 */
static bool gPortLogOn = true;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// printf()-style logging.
void uPortLogF(const char *pFormat, ...)
{
    va_list args;

    va_start(args, pFormat);
    ucpu_sdk_debug_va_list(gPortLogOn, pFormat, args);
    va_end(args);
}

// Switch logging off.
int32_t uPortLogOff(void)
{
    gPortLogOn = false;
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Switch logging on.
int32_t uPortLogOn(void)
{
    gPortLogOn = true;
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// End of file
