/*
 * Copyright 2019-2024 u-blox
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
 * @brief Timeout functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_timeout.h"

#include "u_port.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CFG_TEST_TIMEOUT_SPEED_UP
/** Used during testing only, this allows the code here to perceive
 * the underlying tick as running faster in powers of 2, meaning
 * that it sees more tick timer wraps, without externally altering
 * the behaviour of the code (though obviously reducing the maximum
 * duration of any timeout).
 *
 * For instance, if you set #U_CFG_TEST_TIMEOUT_SPEED_UP to 14
 * then time will be 16,384 times faster and so, with a millisecond
 * tick, the wrap will be ever 262 seconds, just over 4 minutes,
 * which is longer than the duration of any timers used during
 * testing and shorter than a run of all tests, so would serve to
 * bring the 32-bit tick-wrap into play.
 */
# define U_CFG_TEST_TIMEOUT_SPEED_UP 0
#endif

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

// Initialise a time-out with the current time.
uTimeoutStart_t uTimeoutStart()
{
    uTimeoutStart_t timeoutStart;
    uint32_t nowTimeMs = (uint32_t) uPortGetTickTimeMs();
#if (U_CFG_TEST_TIMEOUT_SPEED_UP > 0)
    nowTimeMs  <<= U_CFG_TEST_TIMEOUT_SPEED_UP;
#endif
    timeoutStart.timeMs = nowTimeMs;
    return timeoutStart;
}

// Perform a time-out check in a wrap-safe way.
bool uTimeoutExpiredMs(uTimeoutStart_t startTime, uint32_t durationMs)
{
    uint32_t nowTimeMs = (uint32_t) uPortGetTickTimeMs();
#if (U_CFG_TEST_TIMEOUT_SPEED_UP > 0)
    nowTimeMs <<= U_CFG_TEST_TIMEOUT_SPEED_UP;
    durationMs <<= U_CFG_TEST_TIMEOUT_SPEED_UP;
#endif
    // Move the time-frame from the unaligned free running ticks
    // to 0 by subtracting the start time
    uint32_t elapsedTimeMs = nowTimeMs - startTime.timeMs;
    // This will evaluate to false during the next durationMs after
    // uTimeoutStart() was called
    return elapsedTimeMs > durationMs;
}

// As uTimeoutExpiredMs() but for values in seconds.
bool uTimeoutExpiredSeconds(uTimeoutStart_t startTime,
                            uint32_t durationSeconds)
{
    return uTimeoutExpiredMs(startTime, durationSeconds * 1000);
}

// Return how time has passed in milliseconds.
uint32_t uTimeoutElapsedMs(uTimeoutStart_t startTime)
{
    uint32_t nowTimeMs = (uint32_t) uPortGetTickTimeMs();
#if (U_CFG_TEST_TIMEOUT_SPEED_UP > 0)
    nowTimeMs <<= U_CFG_TEST_TIMEOUT_SPEED_UP;
#endif
    uint32_t elapsedTimeMs = nowTimeMs - startTime.timeMs;
#if (U_CFG_TEST_TIMEOUT_SPEED_UP > 0)
    elapsedTimeMs >>= U_CFG_TEST_TIMEOUT_SPEED_UP;
#endif
    return elapsedTimeMs;
}

// As uTimeoutElapsedMs() but returning a value in seconds.
uint32_t uTimeoutElapsedSeconds(uTimeoutStart_t startTime)
{
    return uTimeoutElapsedMs(startTime) / 1000;
}

// End of file
