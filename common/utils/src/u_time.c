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
 * @brief functions to assist with time manipulation.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_time.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static const char gDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31,
                                    31, 30, 31, 30, 31
                                   };
static const char gDaysInMonthLeapYear[] = {31, 29, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31
                                           };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

bool uTimeIsLeapYear(int32_t year)
{
    bool isLeapYear = false;

    if (year % 400 == 0) {
        isLeapYear = true;
    } else if (year % 4 == 0) {
        isLeapYear = true;
    }

    return isLeapYear;
}

int64_t uTimeMonthsToSecondsUtc(int32_t monthsUtc)
{
    int64_t secondsUtc = 0;

    for (int32_t x = 0; x < monthsUtc; x++) {
        if (uTimeIsLeapYear((x / 12) + 1970)) {
            secondsUtc += gDaysInMonthLeapYear[x % 12] * 3600 * 24;
        } else {
            secondsUtc += gDaysInMonth[x % 12] * 3600 * 24;
        }
    }

    return secondsUtc;
}

// End of file
