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
 * @brief Cortex M4 exception handlers.
 */

#include "stdbool.h"

#include "stm32f4xx.h"
#include "cmsis_os.h"
#include "core_cm4.h"

#include "task_snapshot.h"

#include "u_assert.h"
#include "u_port_debug.h"
#include "u_debug_utils.h"
#include "u_debug_utils_internal.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define CALL_EXCEPTION_HANDLER(handler)    \
  __asm volatile(                          \
      "tst lr, #4 \n"                      \
      "ite eq \n"                          \
      "mrseq r0, msp \n"                   \
      "mrsne r0, psp \n"                   \
      "b %0 \n"                            \
      : : "i"(handler) )

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct __attribute__((packed))
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
}
uExceptionFrame_t;

extern uint32_t uDebugUtilsGetThreadStackTop(TaskHandle_t handle);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* Need to update this value in cellular_port_private.c. */
extern int32_t gTickTimerRtosCount;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_DEBUG_UTILS_DUMP_THREADS
static void dummyAssert(const char *pFileStr, int32_t line)
{
    (void)pFileStr;
    (void)line;
}
#endif

__attribute__((optimize("O0")))
static void dumpData(uExceptionFrame_t *frame)
{
    if (frame != NULL) {
        uPortLogF("  PC:  0x%08x  LR:   0x%08x\n", frame->pc, frame->lr);
        uPortLogF("  R0:  0x%08x  R1:   0x%08x  R2:  0x%08x  R3:  0x%08x\n",
                  frame->r0, frame->r1, frame->r2, frame->r3);
        uPortLogF("  R12: 0x%08x  XPSR: 0x%08x\n", frame->r12, frame->xpsr);

#ifndef U_DEBUG_UTILS_DUMP_THREADS
        // Our monitor will automatically call addr2line for target strings
        // that starts with "Backtrace: ", so we print PC and LR again
        // as a backtrace:
        uPortLogF("  Backtrace: 0x%08x 0x%08x\n", frame->pc, frame->lr);
#else
        uStackFrame_t sFrame;
        TaskSnapshot_t snapShot;
        char *pName;
        uint32_t psp = ((uint32_t)frame) + sizeof(uExceptionFrame_t);
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

#ifdef U_DEBUG_UTILS_DUMP_THREADS
    // When calling uDebugUtilsDumpThreads() vPortEnterCritical
    // will be called. vPortEnterCritical is not interrupt safe
    // which we clearly don't care about during an exception.
    // However, this will make an assert fail which would result
    // in an endless loop halfway through uDebugUtilsDumpThreads().
    // For this reason we replace the current assert handler
    // with a dummy handler that does nothing.
    uAssertHookSet(dummyAssert);

    uDebugUtilsDumpThreads();
#endif
}

static void uHardfaultHandler(uExceptionFrame_t *frame)
{
    uPortLogF("\n### Caught HardFault exception ###\n");
    uPortLogF("  HFSR: 0x%08x\n", SCB->HFSR);
    uPortLogF("  CFSR: 0x%08x\n", SCB->CFSR);
    dumpData(frame);
    // TODO: Should probably reboot here instead
    while (1) {}
}

static void uMemManageHandler(uExceptionFrame_t *frame)
{
    uPortLogF("\n### Caught MemManage exception ###\n");
    uPortLogF("  MMFAR: 0x%08x\n", SCB->MMFAR);
    uPortLogF("  CFSR: 0x%08x\n", SCB->CFSR);
    dumpData(frame);
    // TODO: Should probably reboot here instead
    while (1) {}
}

static void uUsageFaultHandler(uExceptionFrame_t *frame)
{
    uPortLogF("\n### Caught UsageFault exception ###\n");
    uPortLogF("  CFSR: 0x%08x\n", SCB->CFSR);
    dumpData(frame);
    // TODO: Should probably reboot here instead
    while (1) {}
}

static void uBusFaultHandler(uExceptionFrame_t *frame)
{
    uPortLogF("\n### Caught BusFault exception ###\n");
    uPortLogF("  BFAR: 0x%08x\n", SCB->BFAR);
    uPortLogF("  CFSR: 0x%08x\n", SCB->CFSR);
    dumpData(frame);
    // TODO: Should probably reboot here instead
    while (1) {}
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
  * @brief  NMI handler.
  */
void NMI_Handler(void)
{
}

/**
  * @brief  Hard Fault hook. Actual handler is uHardfaultHandler.
  */
__attribute__((naked))
void HardFault_Handler(void)
{
    CALL_EXCEPTION_HANDLER(uHardfaultHandler);
}

/**
  * @brief  Memory Manage hook. Actual handler is uMemManageHandler.
  */
__attribute__((naked))
void MemManage_Handler(void)
{
    CALL_EXCEPTION_HANDLER(uMemManageHandler);
}

/**
  * @brief  Bus Fault hook. Actual handler is uBusFaultHandler.
  */
__attribute__((naked))
void BusFault_Handler(void)
{
    CALL_EXCEPTION_HANDLER(uBusFaultHandler);
}

/**
  * @brief  Usage Fault hook. Actual handler is uUsageFaultHandler.
  */
__attribute__((naked))
void UsageFault_Handler(void)
{
    CALL_EXCEPTION_HANDLER(uUsageFaultHandler);
}

/**
  * @brief  This function handles Debug Monitor exception.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  SysTick handler.
  */
void SysTick_Handler(void)
{
    gTickTimerRtosCount++;
    osSystickHandler();
}

// End of file
