/*
 * Copyright 2022 u-blox
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

#ifndef _U_DEBUG_UTILS_H_
#define _U_DEBUG_UTILS_H_

/* No #includes allowed here. */

/** @file
 * @brief Various debug utilities
 *
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
 * FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_DEBUG_UTILS_DUMP_THREADS

/** Dump all current threads.
 */
void uDebugUtilsDumpThreads(void);

/** Print the call stack for a stack pointer.
 *
 * For each call stack entry only the PC will be printed (as a hex).
 * To decode the corresponding source code file and line number you will need
 * to use <toolchain_prefix>addr2line.
 *
 * Example output for this function with a call stack depth of 2:
 * "Backtrace: 0x000ec4df 0x000df5a6"
 *
 * @param pSp          the stack pointer.
 * @param pStackTop    the top of the stack.
 * @param maxDepth     max call stack depth to print.
 * @return             the actual call stack depth on success else negative error code.
 */
int32_t uDebugUtilsPrintCallStack(uint32_t *pSp,
                                  uint32_t *pStackTop,
                                  size_t maxDepth);

#endif

#ifdef __cplusplus
}
#endif

#endif // _U_DEBUG_UTILS_H_

// End of file
