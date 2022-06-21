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

#ifndef _U_DEBUG_UTILS_INTERNAL_H_
#define _U_DEBUG_UTILS_INTERNAL_H_

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

typedef struct {
    uint32_t pc;
    uint32_t sp;
    void *pContext;
} uStackFrame_t;


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialize a frame iterator.
 *
 * @param sp           the current stack pointer.
 * @param stackTop     the top of the stack (end of stack).
 * @param[out] pFrame  the stack frame to initialise.
 * @return             true on success.
 *
 * NOTE: uDebugUtilsGetNextStackFrame() must also be called before the PC in
 *       pFrame becomes valid.
 */
bool uDebugUtilsInitStackFrame(uint32_t sp, uint32_t stackTop, uStackFrame_t *pFrame);

/** Get next stack frame.
 *
 * Initialize the first stack frame using uDebugUtilsInitStackFrame() then
 * call this function to iterate through the stack frames.
 *
 * @param stackTop        the top of the stack.
 * @param[in,out] pFrame  the stack frame to iterate.
 * @return                the true if a valid frame is found, false if iteration
 *                        failed or if end of the frame chain has been reached.
 */
bool uDebugUtilsGetNextStackFrame(uint32_t stackTop, uStackFrame_t *pFrame);

/** Print the call stack for a stack pointer.
 *
 * For each call stack entry only the PC will be printed (as a hex).
 * To decode the corresponding source code file and line number you will need
 * to use [toolchain_prefix]addr2line.
 *
 * Example output for this function with a call stack depth of 2:
 * "Backtrace: 0x000ec4df 0x000df5a6"
 *
 * @param sp           the stack pointer.
 * @param stackTop     the top of the stack.
 * @param maxDepth     max call stack depth to print.
 * @return             the actual call stack depth on success else negative error code.
 */
int32_t uDebugUtilsPrintCallStack(uint32_t sp,
                                  uint32_t stackTop,
                                  size_t maxDepth);



#ifdef __cplusplus
}
#endif

#endif // _U_DEBUG_UTILS_H_

// End of file
