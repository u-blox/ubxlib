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

#ifndef _U_PORT_H_
#define _U_PORT_H_

/* No #includes allowed here */

/** @file
 * @brief
 * Common stuff for porting layer.  These functions are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Used only by U_PORT_STRINGIFY_QUOTED.
 */
#define U_PORT_STRINGIFY_LITERAL(x) #x

/** Stringify a macro, i.e. if you have:
 *
 * \#define foo bar
 *
 * ...U_PORT_STRINGIFY_QUOTED(foo) is "bar".
 */
#define U_PORT_STRINGIFY_QUOTED(x) U_PORT_STRINGIFY_LITERAL(x)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Start the platform.  This configures clocks, resources, etc.
 * and then calls pEntryPoint, i.e. the application, in an RTOS task.
 * This is used as a standard way to start the system for all of the
 * u-blox examples and all of the u-blox tests.
 * You may have your own mechanism for initialisating the HW and
 * starting an RTOS task, in which case you need not use this
 * function.
 * This function only returns if there is an error;
 * code execution ends up in pEntryPoint, which should
 * never return.
 *
 * @param pEntryPoint    the function to run.
 * @param pParameter     a pointer that will be passed to pEntryPoint
 *                       when it is called. Usually NULL.
 * @param stackSizeBytes the number of bytes of memory to dynamically
 *                       allocate for stack; ignored if the RTOS
 *                       has already been started and no new
 *                       task needs to be created for the entry point.
 * @param priority       the priority at which to run a task that
 *                       is pEntryPoint; ignored if the RTOS
 *                       has already been started and no new
 *                       task needs to be created for the entry point.
 * @return               negative error code.
 */
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority);

/** Initialise the porting layer.  Should be called by
 * the application entry point before running any other
 * ubxlib function except uPortPlatformStart().
 * If the port is already initialised this function does
 * nothing and returns success, hence it can safely
 * be called at any time.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortInit();

/** Deinitialise the porting layer.
 */
void uPortDeinit();

/** Get the current OS tick converted to a time in milliseconds.
 * This is guaranteed to be unaffected by any time setting activity.
 * It is NOT maintained while the processor is in deep sleep, i.e.
 * with clocks stopped; port initialisation should be called on
 * return from deep sleep and that will restart this time from
 * zero once more.
 *
 * @return the current OS tick converted to milliseconds.
 */
int64_t uPortGetTickTimeMs();

/** Get the heap high watermark, the minimum amount of heap
 * free, ever.
 *
 * @return the minimum amount of heap free in bytes or negative
 *         error code.
 */
int32_t uPortGetHeapMinFree();

/** Get the current free heap size.  This may be called at
 * any time, even before uPortInit() or after uPortDeinit().
 *
 * @return the amount of free heap in bytes or negative
 *         error code.
 */
int32_t uPortGetHeapFree();

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_H_

// End of file
