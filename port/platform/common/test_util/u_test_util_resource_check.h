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

#ifndef _U_TEST_UTIL_RESOURCE_CHECK_H_
#define _U_TEST_UTIL_RESOURCE_CHECK_H_

/** @file
 * @brief Functions to check for leakage of heap, OS resources (tasks etc.)
 * and transports (UARTs etc.).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_TEST_UTIL_RESOURCE_CHECK_ERROR_MARKER
/** A default error marker, may be passed as the pErrorMarker
 * parameter to uTestUtilResourceCheck() if you wish to
 * highlight errors.
 */
# define U_TEST_UTIL_RESOURCE_CHECK_ERROR_MARKER "*** ERROR *** "
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the current number of dynamic resources; this includes calls
 * to pUPortMalloc() less calls to uPortFree(), all outstanding OS
 * resources (tasks, mutexes, semaphores, queues and timers) and all
 * open transports (UART, I2C, SPI, etc.) BUT IGNORES any that
 * have been added perpetually, i.e. those marked by calls to
 * uPortOsResourcePerpetualAdd() or uPortHeapPerpetualAllocAdd().
 *
 * This might be called at the beginning and end of a test: the
 * differences is the resource leakage.
 *
 * @return  the number of heap allocations/tasks/mutexes/semaphores/
 *          queues/timers outstanding, plus the open UARTs, I2Cs or
 *          SPIs, minus the number of perpetually OS resources or
 *          heap allocations.
 */
int32_t uTestUtilGetDynamicResourceCount();

/** Check that resources have been cleaned up; to be called at the
 * end of each set of tests.
 *
 * @param[in] pPrefix       a prefix to use with any informative
 *                          prints; may be NULL.
 * @param[in] pErrorMarker  a string to use (after pPrefix) as an
 *                          error marker; may be NULL, for instance
 *                          if the check is meant to be informative.
 * @param printIt           print into the log output.
 * @return                  true if resources have been cleaned up,
 *                          else false.
 */
bool uTestUtilResourceCheck(const char *pPrefix,
                            const char *pErrorMarker,
                            bool printIt);

/** Get the current number of failed tests; since clean-up processes
 * are skipped when a test fails, you may call this determine
 * whether it is worth asserting about failed resources, which only
 * adds more noise.
 *
 * @return  the number of failed tests.
 */
size_t uTestUtilGetNumFailed();

#ifdef __cplusplus
}
#endif

#endif // _U_TEST_UTIL_RESOURCE_CHECK_H_

// End of file
