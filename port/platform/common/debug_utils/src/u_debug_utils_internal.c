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

/** @file
 * @brief Internal functions used for the debug utils
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "u_port_debug.h"

#include "u_debug_utils_internal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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

#ifdef U_DEBUG_UTILS_DUMP_THREADS
int32_t uDebugUtilsPrintCallStack(uint32_t sp, uint32_t stackTop, size_t maxDepth)
{
    size_t depth;
    uStackFrame_t frame;
    uPortLogF("Backtrace: ");
    if (!uDebugUtilsInitStackFrame(sp, stackTop, &frame)) {
        return 0;
    }
    for (depth = 0; depth < maxDepth; depth++) {
        if (uDebugUtilsGetNextStackFrame(stackTop, &frame)) {
            uPortLogF("0x%08x ", (unsigned int)frame.pc);
        } else {
            break;
        }
    }
    uPortLogF("\n");
    return depth;
}
#endif