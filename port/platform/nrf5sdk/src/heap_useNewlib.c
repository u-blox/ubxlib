/**
 * \file heap_useNewlib.c
 * \brief Wrappers required to use newlib malloc-family within FreeRTOS.
 *
 * \par Overview
 * Route FreeRTOS memory management functions to newlib's malloc family.
 * Thus newlib and FreeRTOS share memory-management routines and memory pool,
 * and all newlib's internal memory-management requirements are supported.
 *
 * \author Dave Nadler
 * \date 7-August-2019
 * \version 23-Sep-2019 comments, check no malloc call inside ISR
 *
 * \see http://www.nadler.com/embedded/newlibAndFreeRTOS.html
 * \see https://sourceware.org/newlib/libc.html#Reentrancy
 * \see https://sourceware.org/newlib/libc.html#malloc
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fenv_005flock
 * \see https://sourceware.org/newlib/libc.html#index-_005f_005fmalloc_005flock
 * \see https://sourceforge.net/p/freertos/feature-requests/72/
 * \see http://www.billgatliff.com/newlib.html
 * \see http://wiki.osdev.org/Porting_Newlib
 * \see http://www.embecosm.com/appnotes/ean9/ean9-howto-newlib-1.0.html
 *
 *
 * \copyright
 * (c) Dave Nadler 2017-2019, All Rights Reserved.
 * Web:         http://www.nadler.com
 * email:       drn@nadler.com
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Use or redistributions of source code must retain the above copyright notice,
 *   this list of conditions, ALL ORIGINAL COMMENTS, and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ================================================================================================
// =======================================  Configuration  ========================================
// These configuration symbols could be provided by build...
#define SUPPORT_ISR_STACK_MONITOR     // #define to enable ISR (MSP) stack diagnostics
#define ISR_STACK_LENGTH_BYTES  512   // #define bytes to reserve for ISR (MSP) stack
// =======================================  Configuration  ========================================
// ================================================================================================

#include <stdlib.h> // maps to newlib...
#include <malloc.h> // mallinfo...
#include <errno.h>  // ENOMEM
#include <stdbool.h>
#include <stddef.h>

#include "newlib.h"
#if (__NEWLIB__ != 3) || ((__NEWLIB_MINOR__ != 0) && (__NEWLIB_MINOR__ != 1) && (__NEWLIB_MINOR__ != 3))
#warning "This wrapper was verified for newlib version 3.0.0 and then checked again for minor revisions 1 and 3; please ensure newlib's external requirements for malloc-family are unchanged!"
#endif

#include "FreeRTOS.h" // defines public interface we're implementing here
#if !defined(configUSE_NEWLIB_REENTRANT) ||  (configUSE_NEWLIB_REENTRANT!=1)
#warning "#define configUSE_NEWLIB_REENTRANT 1 // Required for thread-safety of newlib sprintf, strtok, etc..."
// If you're *REALLY* sure you don't need FreeRTOS's newlib reentrancy support, remove this warning...
#endif
#include "task.h"

// ================================================================================================
// External routines required by newlib's malloc (sbrk/_sbrk, __malloc_lock/unlock)
// ================================================================================================

// Simplistic sbrk implementations assume stack grows downwards from top of memory,
// and heap grows upwards starting just after BSS.
// FreeRTOS normally allocates task stacks from a pool placed within BSS or DATA.
// Thus within a FreeRTOS task, stack pointer is always below end of BSS.
// When using this module, stacks are allocated from malloc pool, still always prior
// current unused heap area...
register char *stack_ptr __asm__("sp");

// Note: DRN's K64F LD provided: __StackTop (byte beyond end of memory), __StackLimit, HEAP_SIZE, STACK_SIZE
// __HeapLimit was already adjusted to be below reserved stack area.
extern char HEAP_SIZE;  // make sure to define this symbol in linker LD command file
static int heapBytesRemaining = (int) & (HEAP_SIZE); // that's (&__HeapLimit)-(&__HeapBase)

#define DRN_ENTER_CRITICAL_SECTION() vTaskSuspendAll(); // Note: safe to use before FreeRTOS scheduler started, but not in ISR
#define DRN_EXIT_CRITICAL_SECTION()  xTaskResumeAll(); // Note: safe to use before FreeRTOS scheduler started, but not in ISR

#ifndef NDEBUG
static int totalBytesProvidedBySBRK = 0;
#endif
extern char __HeapBase, __HeapLimit;  // make sure to define these symbols in linker LD command file

// Include the following to track how sbrk is being called,
// which ultimately indicates whether the heap is big enough
#if 0

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_port_debug.h"

// Macro for detailed sbrk logging.
# define SBRK_DETAILED_LOG(_thisAlloc)                                                         \
                          gLog[gLogIndex].thisAlloc = _thisAlloc;                              \
                          gLog[gLogIndex].currentHeapEnd = (int32_t) currentHeapEnd;           \
                          gLogIndex++;                                                         \
                          if (gLogIndex >= sizeof(gLog) / sizeof(gLog[0])) {                   \
                              gLogIndex = 0;                                                   \
                          }                                                                    \

// Log struct.
typedef struct {
    int32_t thisAlloc;
    int32_t currentHeapEnd;
    int32_t stackPtr;
} uLogThing_t;

// Logging array for detailed sbrk logging
static uLogThing_t gLog[128];

// Index into the array
static size_t gLogIndex = 0;

// What it says.
void uPortPrintDetailedSbrkDebug()
{
    uLogThing_t *pThing = gLog;

    uPortLog("------- Sbrk() debug begins, %d item(s) --------\n", gLogIndex);
    uPortLog("Heap base 0x%08x, limit 0x%08x.\n",
             (int32_t) &__HeapBase, (int32_t) &__HeapLimit, (int32_t) &__StackTop);
    for (size_t x = 0; x < gLogIndex; x++) {
        uPortLog("This sbrk() increment %d, remaining %d, current end 0x%08x.\n",
                 pThing->thisAlloc, ((int32_t) &__HeapLimit) - pThing->currentHeapEnd,
                 pThing->currentHeapEnd);
        pThing++;
    }
    uPortLog("-------------- Sbrk() debug ends ---------------\n");
}

#else
# define SBRK_DETAILED_LOG(_event)
#endif

// Return the value of "heap bytes remaining", which
// is the size not yet passed to newlib by malloc().
// Since newlib only asks for memory when it needs
// more and it never comes back this is a measure of
// the minimum heap remaining EVER.
int uPortInternalGetSbrkFreeBytes()
{
    return heapBytesRemaining;
}

//! _sbrk_r version supporting reentrant newlib (depends upon above symbols defined by linker control file).
void *_sbrk_r(struct _reent *pReent, int incr)
{
    static char *currentHeapEnd = &__HeapBase;
    char *limit = (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) ?
                  stack_ptr   :  // Before scheduler is started, limit is stack pointer (risky!)
                  &__HeapLimit -
                  ISR_STACK_LENGTH_BYTES;  // Once running, OK to reuse all remaining RAM except ISR stack (MSP) stack
    DRN_ENTER_CRITICAL_SECTION();
    char *previousHeapEnd = currentHeapEnd;
    (void)pReent;

    SBRK_DETAILED_LOG(incr);
    if (currentHeapEnd + incr > limit) {
        // Ooops, no more memory available...
#if( configUSE_MALLOC_FAILED_HOOK == 1 )
        {
            extern void vApplicationMallocFailedHook( void );
            DRN_EXIT_CRITICAL_SECTION();
            vApplicationMallocFailedHook();
        }
#elif defined(configHARD_STOP_ON_MALLOC_FAILURE)
        // If you want to alert debugger or halt...
        // WARNING: brkpt instruction may prevent watchdog operation...
        while (1) {
            __asm__("bkpt #0");
        }; // Stop in GUI as if at a breakpoint (if debugging, otherwise loop forever)
#else
        // Default, if you prefer to believe your application will gracefully trap out-of-memory...
        pReent->_errno = ENOMEM; // newlib's thread-specific errno
        DRN_EXIT_CRITICAL_SECTION();
#endif
        return (char *) -1; // the malloc-family routine that called sbrk will return 0
    }
    // 'incr' of memory is available: update accounting and return it.
    currentHeapEnd += incr;
    heapBytesRemaining -= incr;
#ifndef NDEBUG
    totalBytesProvidedBySBRK += incr;
#endif
    DRN_EXIT_CRITICAL_SECTION();
    return (char *) previousHeapEnd;
}

//! non-reentrant sbrk uses is actually reentrant by using current context
// ... because the current _reent structure is pointed to by global _impure_ptr
char *sbrk(int incr)
{
    return _sbrk_r(_impure_ptr, incr);
}
//! _sbrk is a synonym for sbrk.
char *_sbrk(int incr)
{
    return sbrk(incr);
};

void __malloc_lock(struct _reent *r)
{
    // RM: original Dave Nadler code called xPortIsInsideInterrupt()
    // here but that's only available in a later version of FreeRTOS
    // than NRF5 uses, so get it from CMSIS directly
    bool insideAnISR = ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0);
    (void)r;
    configASSERT( !insideAnISR ); // Make damn sure no mallocs inside ISRs!!
    vTaskSuspendAll();
};

void __malloc_unlock(struct _reent *r)
{
    (void)r;
    (void)xTaskResumeAll();
};

// newlib also requires implementing locks for the application's environment memory space,
// accessed by newlib's setenv() and getenv() functions.
// As these are trivial functions, momentarily suspend task switching (rather than semaphore).
// ToDo: Move __env_lock/unlock to a separate newlib helper file.
void __env_lock()
{
    vTaskSuspendAll();
};
void __env_unlock()
{
    (void)xTaskResumeAll();
};

// ================================================================================================
// Implement FreeRTOS's memory API using newlib-provided malloc family.
// ================================================================================================

void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION {
    void *p = malloc(xSize);
    return p;
}
void vPortFree( void *pv ) PRIVILEGED_FUNCTION {
    free(pv);
};

size_t xPortGetFreeHeapSize( void ) PRIVILEGED_FUNCTION {
    struct mallinfo mi = mallinfo(); // available space now managed by newlib
    return mi.fordblks + heapBytesRemaining; // plus space not yet handed to newlib by sbrk
}

// GetMinimumEverFree is not available in newlib's malloc implementation.
// So, no implementation is provided: size_t xPortGetMinimumEverFreeHeapSize( void ) PRIVILEGED_FUNCTION;

//! No implementation needed, but stub provided in case application already calls vPortInitialiseBlocks
void vPortInitialiseBlocks( void ) PRIVILEGED_FUNCTION {};
