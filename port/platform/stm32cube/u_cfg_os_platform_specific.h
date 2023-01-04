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
 * an STM32F4 processor.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: HEAP
 * -------------------------------------------------------------- */

/** Not stricty speaking part of the OS but there's nowhere better
 * to put this.  newlib on this platform doesn't recover memory
 * properly on task deletion: if you printf() from a task, the first
 * time it will allocate 1468 bytes of memory and will never give
 * that back, even if you delete the task.  So either don't printf()
 * from the task at all or don't delete it.
 *
 * There is a down-side to setting this to 1, which is that URCs
 * received from a module will not be printed-out by the AT client
 * (since prints from a dynamic task often cause such leaks), and
 * this can be a pain when debugging, so please set this to 0 if you
 * can.
 */
#define U_CFG_OS_CLIB_LEAKS 0

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: OS GENERIC
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_PRIORITY_MIN
/** The minimum task priority. cmsis-os defines osPriorityIdle
 * as 1 (priority 0 is "undefined priority").
 */
# define U_CFG_OS_PRIORITY_MIN 1
#endif

#ifndef U_CFG_OS_PRIORITY_MAX
/** The maximum task priority, should be less than or
 * equal to configMAX_PRIORITIES defined in FreeRTOSConfig.h,
 * which is set to 15. cmsis-os defines osPriorityISR
 * as 56 but when this is mapped to FreeRTOS, as it is on
 * this platform, the range gets squished.
 */
# define U_CFG_OS_PRIORITY_MAX 15
#endif

#ifndef U_CFG_OS_YIELD_MS
/** The amount of time to block for to ensure that a yield
 * occurs. This set to 2 ms as the STM32F4 platform has a
 * 1 ms tick.
 */
# define U_CFG_OS_YIELD_MS 2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: STACK SIZES/PRIORITIES
 * -------------------------------------------------------------- */

/** How much stack the task running all the examples/tests needs in
 * bytes, plus space for the user to add code.
 */
#define U_CFG_OS_APP_TASK_STACK_SIZE_BYTES (1024 * 8)

/** The priority of the task running the examples/tests: should
 * be low but must be higher than the minimum.
 */
#define U_CFG_OS_APP_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 1)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: OS TIMERS
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

#endif // _U_CFG_OS_PLATFORM_SPECIFIC_H_

// End of file
