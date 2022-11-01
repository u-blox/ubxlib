/*
 * Copyright 2019-2022 u-blox Ltd
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

/** @file
 * @brief This header file contains OS configuration information for
 * an sara5ucpu board.
 */

#define TXM_MODULE
#include "txm_module.h"

extern TX_BYTE_POOL *pThreadStack;

extern TX_BYTE_POOL *pHeapPool;

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR SARAR5UCPU: STACK
 * -------------------------------------------------------------- */

/** Total size of byte pool space.
 * 32 KB of pool availble for thread stack and queue usage,
 * both for ubxlib and user application.
 */
#define THREAD_STACK_POOL_SIZE (1024 * 48)

/** Minimum stack size allowed for any thread,
 * stack size should be equal to or greater than this.
 * TX_MINIMUM_STACK is 200 bytes.
 */
#define THREAD_STACK_MINIMUM TX_MINIMUM_STACK

/** Maximum stack size allowed for any thread,
 * stack size should be equal to or less than this.
 * Maximum 8 threads of 4 KB of stack size are allowed.
 */
#define THREAD_STACK_MAXIMUM (1024 * 10)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR SARAR5UCPU: HEAP
 * -------------------------------------------------------------- */

/** Total size of heap pool space.
 * 128 KB of pool available for heap usage,
 * both for ubxlib and user application.
*/
#define HEAP_POOL_SIZE (1024 * 128)

/** Not stricty speaking part of the OS but there's nowhere better
 * to put this. Set this to 1 if the C library does not free memory
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
 * COMPILE-TIME MACROS FOR SARAR5UCPU: OS GENERIC
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_PRIORITY_MIN
/** The minimum task priority.
 */
# define U_CFG_OS_PRIORITY_MIN 0
#endif

#ifndef U_CFG_OS_PRIORITY_MAX
/** The maximum task priority.
 */
# define U_CFG_OS_PRIORITY_MAX (12)
#endif

#ifndef U_CFG_OS_YIELD_MS
/** The amount of time to block for to ensure that a yield
 * occurs. This set to 2 ms as the sarar5ucpu platform has a
 * 1 ms tick.
 */
# define U_CFG_OS_YIELD_MS 2
#endif

#define U_CFG_OS_APP_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 2)

#define U_AT_CLIENT_URC_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 1)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR SARAR5UCPU: EVENT QUEUE
 * -------------------------------------------------------------- */

/** Maximum message size allowed for queue in bytes.
 */
#define U_QUEUE_MAX_MSG_SIZE (16 * 4)

/** The maximum length of parameter/message block that can be sent
 * on an event queue. U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES is
 * the size of uEventQueueControlOrSize_t, which is prefixed to the
 * parameter/message block sent to the queue, subtracting that
 * size from maximum message size.
 */
#define U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES (U_QUEUE_MAX_MSG_SIZE - U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES)

#endif // _U_CFG_OS_PLATFORM_SPECIFIC_H_

// End of file
