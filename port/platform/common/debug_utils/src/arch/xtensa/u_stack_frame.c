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

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_port_debug.h"

#include "u_debug_utils_internal.h"

#include "esp_debug_helpers.h"

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

bool uDebugUtilsInitStackFrame(uint32_t sp, uint32_t stackTop, uStackFrame_t *pFrame)
{
    /* NOTES:
     * The backtrace for current thread will not work. This is typically not an issue
     * since the backtrace is intended to be called from the inactivity task.
     *
     * The reason that the current thread doesn't get dumped correctly is that we only
     * look at the FreeRTOS TCB bottom stack pointer which is only updated on context
     * switches. To fix this the current thread must be handled differently and instead
     * look at the core SP.
     *
     * For ESP this can be quite easily done by using esp_backtrace_get_start(). But
     * there needs to be some minor adjustement to this function API so that current
     * thread can be identified. And if ESP is fixed we should also fix ARM version
     * by getting SP from MSP or PSP. Since there are no urgent need for this I
     * will leave this for now...
     *
     * IMPORTANT:
     * We need a reference to a esp_backtrace_frame_t that can be passed to
     * uDebugUtilsGetNextStackFrame(). malloc() can't be used here since we want the
     * thread dumper to work in interrupts (such as exceptions) so for this reason
     * this function uses a private static esp_backtrace_frame_t that we then point
     * pFrame at. This means that our backtrace for ESP is NOT reentrant.
     */
    uint32_t next_pc;
    uint32_t *pSp = (uint32_t *)sp;
    static esp_backtrace_frame_t espFrame;
    memset(&espFrame, 0, sizeof(esp_backtrace_frame_t));
    memset(pFrame, 0, sizeof(uStackFrame_t));

    if (esp_ptr_executable((void *)pSp[1])) {
        // Set next_pc to the return PC
        // In this way esp_backtrace_get_next_frame() (which is used in uDebugUtilsGetNextStackFrame())
        // will return this value as frame->pc
        espFrame.next_pc = pSp[1];
        // To make esp_backtrace_get_next_frame() point on correct
        // SP (which is located in pSp[4]) on next call we emulate a base save
        // in pSp[4+3] and store the address of it as SP.
        espFrame.sp = (uint32_t)&pSp[4 + 3];
    } else {
        // The return PC was not executeable so let's try next frame instead
        espFrame.next_pc = pSp[3];
        espFrame.sp = pSp[4];
    }

    pFrame->sp = sp;
    pFrame->pContext = &espFrame;

    next_pc = esp_cpu_process_stack_pc(espFrame.next_pc);

    return esp_ptr_executable((void *)next_pc);
}

bool uDebugUtilsGetNextStackFrame(uint32_t stackTop, uStackFrame_t *pFrame)
{
    esp_backtrace_frame_t *pEspFrame = (esp_backtrace_frame_t *)pFrame->pContext;
    bool result = esp_backtrace_get_next_frame(pEspFrame);
    if (result) {
        pFrame->pc = esp_cpu_process_stack_pc(pEspFrame->pc);
        pFrame->sp = pEspFrame->sp;
    }
    return result;
}
