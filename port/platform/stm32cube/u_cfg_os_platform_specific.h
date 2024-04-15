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

#ifndef _U_CFG_OS_PLATFORM_SPECIFIC_H_
#define _U_CFG_OS_PLATFORM_SPECIFIC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file contains OS configuration information for
 * STM32 processors.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32: HEAP
 * -------------------------------------------------------------- */

/** \deprecated Not stricty speaking part of the OS but there's nowhere
 * better to put this. Set this to 1 if the C library does not free memory
 * that it has alloced internally when a task is deleted.
 * For instance, newlib when it is compiled in a certain way
 * does this on some platforms.
 *
 * This macro is retained for compatibility purposes but is now
 * ALWAYS SET TO 0 and may be removed in future.
 *
 * There is a down-side to setting this to 1, which is that URCs
 * received from a module will not be printed-out by the AT client
 * (since prints from a dynamic task often cause such leaks), and
 * this can be a pain when debugging, so please set this to 0 if you
 * can.
 */
#define U_CFG_OS_CLIB_LEAKS 0

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32: OS GENERIC
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_PRIORITY_MIN
/** The minimum task priority. cmsis-os defines osPriorityIdle
 * as 1 (priority 0 is "undefined priority").
 */
# define U_CFG_OS_PRIORITY_MIN 1
#endif

#ifndef U_CFG_OS_PRIORITY_MAX
/** The maximum task priority, should be less than configMAX_PRIORITIES
 * defined in FreeRTOSConfig.h.
 */
# ifdef CMSIS_V2
#  define U_CFG_OS_PRIORITY_MAX 55
# else
#  define U_CFG_OS_PRIORITY_MAX 14
# endif
#endif

#ifndef U_CFG_OS_YIELD_MS
/** The amount of time to block for to ensure that a yield
 * occurs. This set to 2 ms for STM32F4 with FreeRTOS which has
 * a 1 ms tick, while for STM32U5, in the case of pure CMSIS
 * with ThreadX underneath it we have a 10 ms tick and so we
 * set CMSIS with FreeRTOS underneath it the same way.
 */
# if defined(U_PORT_STM32_PURE_CMSIS)
#  define U_CFG_OS_YIELD_MS 20
# else
#  define U_CFG_OS_YIELD_MS 2
# endif
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32: STACK SIZES/PRIORITIES
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
 * COMPILE-TIME MACROS FOR STM32: OS TIMERS
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
