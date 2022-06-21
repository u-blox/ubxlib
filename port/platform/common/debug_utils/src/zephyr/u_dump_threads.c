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
 * @brief Thread dumper for Zephyr.
 */
#include "zephyr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if !CONFIG_THREAD_NAME || !CONFIG_THREAD_STACK_INFO
# error "CONFIG_THREAD_NAME and CONFIG_THREAD_STACK_INFO must be enabled for uDebugUtilsDumpThreads()"
#endif

#ifdef __arm__
# include "../arch/arm/u_stack_frame_cortex.c"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void thread_dump(const struct k_thread *thread, void *user_data)
{
    uint32_t stackBottom = (uint32_t)thread->stack_info.start;
    uint32_t stackTop = stackBottom + thread->stack_info.size;
    uint32_t sp = thread->callee_saved.psp;
    (void)user_data;
    uPortLogF("  %s (%s): bottom: %08x, top: %08x, sp: %08x\n",
              thread->name,
              k_thread_state_str((k_tid_t)thread),
              (unsigned int)stackBottom,
              (unsigned int)stackTop,
              (unsigned int)sp);
    uPortLogF("    ");
    uDebugUtilsPrintCallStack(sp, stackTop, 16);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

void uDebugUtilsDumpThreads(void)
{
    uPortLogF("### Dumping threads ###\n");
    k_thread_foreach(thread_dump, NULL);
}
