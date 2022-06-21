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
 * @brief nRF exception handler.
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "hardfault.h"

#include "FreeRTOS.h"
#include "task.h"
#include "task_snapshot.h"

#include "u_assert.h"

#include "u_port_debug.h"

#include "u_debug_utils_internal.h"
#include "u_debug_utils.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

extern uint32_t uDebugUtilsGetThreadStackTop(TaskHandle_t handle);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void dumpData(HardFault_stack_t *frame)
{
    if (frame != NULL) {
        uPortLogF("  PC:  0x%08x  LR:   0x%08x\n", frame->pc, frame->lr);
        uPortLogF("  R0:  0x%08x  R1:   0x%08x  R2:  0x%08x  R3:  0x%08x\n",
                  frame->r0, frame->r1, frame->r2, frame->r3);
        uPortLogF("  R12: 0x%08x  XPSR: 0x%08x\n", frame->r12, frame->psr);

#ifndef U_DEBUG_UTILS_DUMP_THREADS
        // Our monitor will automatically call addr2line for target strings
        // that starts with "Backtrace: ", so we print PC and LR again
        // as a backtrace:
        uPortLogF("  Backtrace: 0x%08x 0x%08x\n", frame->pc, frame->lr);
#else
        uStackFrame_t sFrame;
        TaskSnapshot_t snapShot;
        char *pName;
        uint32_t psp = ((uint32_t)frame) + sizeof(HardFault_stack_t);
        uint32_t stackTop;

        vTaskGetSnapshot(xTaskGetCurrentTaskHandle(), &snapShot);
        pName = pcTaskGetName(xTaskGetCurrentTaskHandle());
        stackTop = (uint32_t)snapShot.pxTopOfStack;

        uPortLogF("### Dumping current thread (%s) ###\n", pName);
        uPortLogF("  Backtrace: 0x%08x 0x%08x ", frame->pc, frame->lr);
        if (uDebugUtilsInitStackFrame(psp, stackTop, &sFrame)) {
            for (int depth = 0; depth < 16; depth++) {
                if (uDebugUtilsGetNextStackFrame(stackTop, &sFrame)) {
                    if ((depth > 0) || (sFrame.pc != frame->lr)) {
                        uPortLogF("0x%08x ", (unsigned int)sFrame.pc);
                    }
                } else {
                    break;
                }
            }
        }
        uPortLogF("\n\n");
#endif
    }
    while (1);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

void HardFault_process(HardFault_stack_t *pStack)
{
    uPortLogF("\n### Caught HardFault exception ###\n");
    uPortLogF("  HFSR: 0x%08x\n", SCB->HFSR);
    uPortLogF("  CFSR: 0x%08x\n", SCB->CFSR);
    dumpData(pStack);
}

// End of file
