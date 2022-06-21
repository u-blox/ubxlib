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
 * @brief Implementation of the port debug API for the ESP32 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"  // vprintf()
#include "stdarg.h" // For va_x()
#include "stdint.h"
#include "stdbool.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */

#include "u_error_common.h"

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

/** Only used for detecting inactivity
 */
volatile int32_t gStdoutCounter;

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

    if (gPortLogOn) {
        va_start(args, pFormat);
        vprintf(pFormat, args);
        va_end(args);
    }
    gStdoutCounter++;
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
