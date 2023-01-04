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

#ifndef _U_CFG_OS_PLATFORM_SPECIFIC_H_
#define _U_CFG_OS_PLATFORM_SPECIFIC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file contains OS configuration information for
 * the Zephyr platform.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR: HEAP
 * -------------------------------------------------------------- */

/** Not stricty speaking part of the OS but there's nowhere better
 * to put this.  Set this to 1 if the C library does not free memory
 * that it has alloced internally when a task is deleted.
 * For instance, newlib when it is compiled in a certain way
 * does this on some platforms.
 *
 * There is a down-side to setting this to 1, which is that URCs
 * received from a module will not be printed-out by the AT client
 * (since prints from a dynamic task often cause such leaks), and
 * this can be a pain when debugging, so please set this to 0 if you
 * can.
 */
#define U_CFG_OS_CLIB_LEAKS 0

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR: OS GENERIC
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_PRIORITY_MIN
/** The minimum task priority.
 * In Zephyr, as used on this platform, low numbers indicate
 * lower priority.
 */
# define U_CFG_OS_PRIORITY_MIN  0
#endif

#ifndef U_CFG_OS_PRIORITY_MAX
/** The maximum task priority, default is set to 30 (31-1).
 * Idletask is 0.
 */
# define U_CFG_OS_PRIORITY_MAX  30
#endif

#ifndef U_CFG_OS_YIELD_MS
/** The amount of time to block for to ensure that a yield occurs.
 */
# define U_CFG_OS_YIELD_MS 2
#endif

#ifndef U_CFG_OS_APP_TASK_PRIORITY
/** The priority of the task running the examples and tests: should
 * be low but must be higher than the minimum.
 */
# define U_CFG_OS_APP_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 1)
#endif

#ifndef U_CFG_OS_MAX_THREADS
/**  Max number of threads to be used by ubxlib.
 * Can be tweaked if memory usage should be optimized
 */
#define U_CFG_OS_MAX_THREADS 10
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR: OS TIMERS
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_TIMER_MAX_NUM
/** The maximum number of timers that can be active at any one time.
  */
# define U_CFG_OS_TIMER_MAX_NUM 16
#endif

#ifndef U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES
/** The amount of stack to allocate to the task context within which
 * the timer callback runs.
 */
# define U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES (1024 * 2)
#endif

#ifndef U_CFG_OS_TIMER_EVENT_TASK_PRIORITY
/** The priority assigned to the timer event task: should be as high
 * as possible.
 */
# define U_CFG_OS_TIMER_EVENT_TASK_PRIORITY U_CFG_OS_PRIORITY_MAX
#endif

#ifndef U_CFG_OS_TIMER_EVENT_QUEUE_SIZE
/** The number of things that can be in the timer event queue
 * at any one time.  If this is not big enough then timer expiries
 * may be lost.
 */
# define U_CFG_OS_TIMER_EVENT_QUEUE_SIZE (U_CFG_OS_TIMER_MAX_NUM * 2)
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR: SIZES OF EXECUTABLE CHUNKS OF RAM
 * -------------------------------------------------------------- */

/**  Defined here as an example, used in nRF53 lib_common tests
 */
#define U_CFG_OS_EXECUTABLE_CHUNK_INDEX_0_SIZE  256

#endif // _U_CFG_OS_PLATFORM_SPECIFIC_H_

// End of file
