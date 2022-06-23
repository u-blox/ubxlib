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

#ifndef _U_TIME_H_
#define _U_TIME_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __utils
 *  @{
 */

/** @file
 * @brief This header file defines functions to help with time
 * manipulation.
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

/** Check if the given year is a leap year.  Not a UTC thing; any
 * year will work.
 *
 * @param year the year.
 * @return     true if the year is a leap year, else false.
 */
bool uTimeIsLeapYear(int32_t year);

/** Return the number of UTC seconds that have elapsed in
 * the given number of UTC months, months since the
 * start of 1970 (counting from zero), taking into account
 * leap years.  Useful when converting a day/month/year count
 * into a UTC time.
 *
 * @param monthsUtc the number of months since the start of
 *                  1970, counting from zero.
 * @return          the number of seconds in the given number
 *                  of months, taking into account leap years.
 */
int64_t uTimeMonthsToSecondsUtc(int32_t monthsUtc);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_TIME_H_

// End of file
