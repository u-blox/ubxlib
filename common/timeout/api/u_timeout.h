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

#ifndef _U_TIMEOUT_H_
#define _U_TIMEOUT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __timeout __Timeout
 *  @{
 */

/** @file
 * @brief Functions to handle time-outs in a wrap-safe manner.
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

/** "Anonymous" structure to hold the start time, used in time-out
 * calculations. The contents of this structure MUST NEVER BE
 * REFERENCED except by the code here.
 */
typedef struct {
    uint32_t timeMs;
} uTimeoutStart_t;

/** It is sometimes necessary to carry around a start time and
 * a duration in order to effect a "stop time".  This structure
 * may be used for convenience.
 */
typedef struct {
    uTimeoutStart_t timeoutStart;
    uint32_t durationMs; /** you might use a duration of 0 to mean
                             "not set", but you MUST THEN CHECK
                             this YOURSELF before passing the contents
                             of this structure into uTimeoutExpiredMs()
                             or uTimeoutExpiredSeconds(), otherwise
                             the time-out will expire IMMEDIATELY. */
} uTimeoutStop_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise a time-out with the current time; the value returned
 * by this function may be passed to the uTimeoutExpiredMs(),
 * uTimeoutExpiredSeconds(), uTimeoutElapsedMs() or
 * uTimeoutElapsedSeconds() functions, which must be used for
 * wrap-safe time handling.
 *
 * The underlying source of the tick is uPortGetTickTimeMs() and the
 * same restrictions apply.
 *
 * @return the current time in a form that can be used for time-out
 *         checks.
 */
uTimeoutStart_t uTimeoutStart();

/** Perform a time-out check in a way that will behave predictably
 * across a tick-counter wrap.  See also uTimeoutExpiredSeconds()
 * and uTimeoutElapsed().
 *
 * Where you might have been going to write:
 *
 * ```
 * int32_t startTimeMs = uPortGetTickTimeMs();
 * if (uPortGetTickTimeMs() - startTimeMs > timeoutMs) {
 *     // Do something because the time-out has expired
 * }
 * ```
 *
 * ...then write this instead:
 *
 * ```
 * uTimeoutStart_t startTime = uTimeoutStart();
 * if (uTimeoutExpiredMs(startTime, timeoutMs)) {
 *     // Do something because the time-out has expired
 * }
 * ```
 *
 * @param startTime   the start time, populated using uTimeoutStart().
 * @param durationMs  the duration of the time-out in milliseconds.
 * @return            true if the given duration has passed
 *                    since the start time.
 */
bool uTimeoutExpiredMs(uTimeoutStart_t startTime, uint32_t durationMs);

/** As uTimeoutExpiredMs() but for values in seconds.
 *
 * @param startTime       the start time, populated using uTimeoutStart().
 * @param durationSeconds the duration of the time-out in seconds.
 * @return                true if the given duration has passed
 *                        since the start time.
 */
bool uTimeoutExpiredSeconds(uTimeoutStart_t startTime,
                            uint32_t durationSeconds);

/** Return how much time has passed since the start of a time-out.
 *
 * @param startTime the start time, populated using uTimeoutStart().
 * @return          the amount of time that has elapsed since
 *                  startTime in milliseconds.
 */
uint32_t uTimeoutElapsedMs(uTimeoutStart_t startTime);

/** As uTimeoutElapsedMs() but returning a value in seconds.
 *
 * @param startTime the start time, populated using uTimeoutStart().
 * @return          the amount of time that has elapsed since
 *                  startTime in seconds.
 */
uint32_t uTimeoutElapsedSeconds(uTimeoutStart_t startTime);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_TIMEOUT_H_

// End of file
