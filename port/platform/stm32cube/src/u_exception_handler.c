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

/** @file
 * @brief Cortex M4/M33 exception handlers.
 */

#include "stdbool.h"

#ifdef STM32U575xx
# include "stm32u5xx.h"
# include "core_cm33.h"
#else
# include "stm32f4xx.h"
# include "core_cm4.h"
#endif

#include "u_assert.h"
#include "u_port_debug.h"

#include "u_debug_utils.h"
#include "u_debug_utils_internal.h"

#ifdef CMSIS_V2
# include "cmsis_os2.h"
#else
# include "cmsis_os.h"
#endif

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
// The ThreadX SysTick handler over in tx_initialise_low_level.S
extern void TxSysTick_Handler(void);
#else
# include "task_snapshot.h"
# include "FreeRTOS.h" // For xPortGetFreeHeapSize()
# include "task.h"     // For xTaskGetSchedulerState()
// FreeRTOS tick timer interrupt handler prototype
extern void xPortSysTickHandler(void);
#endif

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

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* Need to update this value in u_port_private.c. */
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

#if defined(U_PORT_STM32_PURE_CMSIS) && defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
// Note: for whatever reason, the CM33 port of FreeRTOS (i.e. STM32U5)
// grabs the SysTick_Handler for itself (see the FreeRTOS file port.c)
// and so in that case we enable the tick hook in FreeRTOSConfig.h
// and increment gTickTimerRtosCount in the hook function (see below).
#else
/**
  * @brief  SysTick handler.
  */
void SysTick_Handler(void)
{
    gTickTimerRtosCount++;
# ifdef CMSIS_V2
#  ifdef U_PORT_STM32_PURE_CMSIS
    // Must be CMSIS on ThreadX
    if (osKernelGetState() >= osKernelRunning) {
        TxSysTick_Handler();
    }
#  else
    // A FreeRTOS where SysTick_Handler() isn't nabbed and
    // the function xPortSysTickHandler() exists
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        /* Call tick handler */
        xPortSysTickHandler();
    }
#  endif
# else
    // CMSIS v1
    osSystickHandler();
# endif
}
#endif

#if configUSE_TICK_HOOK
// Tick hook function used to increment our tick counter in
// the CM33 (STM32U5) case.
void vApplicationTickHook()
{
    gTickTimerRtosCount++;
}
#endif

// End of file
