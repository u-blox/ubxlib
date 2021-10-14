/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief Implementation of the port debug API for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdarg.h"
#include "stdio.h"         // vprintf()

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_debug.h"

#include "stm32f437xx.h" // For ITM_SendChar()

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

// This function will replace the weakly-linked _write() function in
// syscalls.c and will send output to the SWV trace port.
int _write(int file, char *pStr, int len)
{
    (void) file;

    for (size_t x = 0 ; x < len ; x++) {
        ITM_SendChar(*pStr);
        pStr++;
    }

    return len;
}

// printf()-style logging.
void uPortLogF(const char *pFormat, ...)
{
    va_list args;

    va_start(args, pFormat);
    vprintf(pFormat, args);
    va_end(args);
}

// End of file
