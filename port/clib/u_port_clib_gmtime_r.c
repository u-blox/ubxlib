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
 * @brief An implementation of the C library function gmtime_r().
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "time.h"      // struct tm

#include "u_time.h"
#include "u_port_clib_platform_specific.h"

/** gmtime_r(): converts a Unix time_t into a struct tm.
 * This function is thread-safe.
 *
 * @param pTime  pointer to a time_t; cannot be NULL.
 * @param pBuf   pointer to a struct tm; cannot be NULL.
 * @return       the pointer to struct tm or NULL on error.
 */
struct tm *gmtime_r(const time_t *pTime, struct tm *pBuf)
{
    struct tm *pTm = NULL;
    int64_t time;
    int32_t months;
    int32_t years;

    if ((pTime != NULL) && (pBuf != NULL) && (*pTime >= 0)) {
        time = (int64_t) * pTime;
        memset(pBuf, 0, sizeof(*pBuf));
        // Work out the number of months since 1970
        months = uTimeSecondsToMonthsUtc(time);
        // From that, the number of years
        years = months / 12;
        // Years since 1900 for struct tm
        pBuf->tm_year = years + 70;
        // Months since January (0 to 11)
        pBuf->tm_mon = months - (years * 12);
        // Days in the year (0 to 365)
        pBuf->tm_yday = (int32_t) ((time - uTimeMonthsToSecondsUtc(years * 12)) / (3600 * 24));
        // Day of the week, from Sunday (0 to 6); the 1/1/1970 was a Wednesday (4)
        pBuf->tm_wday = (4 + (time / (3600 * 24))) % 7;
        time -= uTimeMonthsToSecondsUtc(months);
        // Day of the month (1 to 31)
        pBuf->tm_mday = (int32_t) (time / (3600 * 24));
        time -= pBuf->tm_mday * 3600 * 24;
        pBuf->tm_mday++;
        // Hours (0 to 23)
        pBuf->tm_hour = (int32_t)  (time / 3600);
        time -= pBuf->tm_hour * 3600;
        // Minutes (0 to 59)
        pBuf->tm_min = (int32_t) (time / 60);
        time -= pBuf->tm_min * 60;
        // Seconds (0 to 59ish)
        pBuf->tm_sec = (int32_t) time;
        // Since this function returns UTC, the
        // Daylight Saving Time flag is left unset.
        pTm = pBuf;
    }

    return pTm;
}

// End of file
