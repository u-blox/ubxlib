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
 * @brief Implementation of generic porting functions for the Zephyr platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "malloc.h"    // For mallinfo

#include "u_cfg_sw.h"
#include "u_compiler.h" // For U_INLINE
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"
#include "u_port_private.h"

#include "zephyr.h"
#include <device.h>

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Key for Zephyr's irq_lock().
 */
static uint32_t gIrqLockKey;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Start the platform.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    (void) stackSizeBytes;
    (void) priority;

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        pEntryPoint(pParameter);
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    uErrorCode_t errorCode;

    // Workaround for Zephyr thread resource pool bug
    uPortOsPrivateInit();
    errorCode = uPortEventQueuePrivateInit();
    if (errorCode == 0) {
        errorCode = uPortUartInit();
    }
    if (errorCode == 0) {
        errorCode = uPortPrivateInit();
    }
    return errorCode;
}

// Deinitialise the porting layer.
void uPortDeinit()
{
    uPortPrivateDeinit();
    uPortUartDeinit();
    uPortEventQueuePrivateDeinit();
    // Workaround for Zephyr thread resource pool bug
    uPortOsPrivateDeinit();
}

// Get the current tick converted to a time in milliseconds.
int32_t uPortGetTickTimeMs()
{
    return k_uptime_get();
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    // No way to get this on Zephyr
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    int32_t heapFreeOrError = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#ifdef U_CFG_ZEPHYR_USE_NEWLIB
    // IMPORTANT: this ISN'T actually the free heap, it is
    // simply the heap which newlib has asked for from
    // which is the real source of heap sbrk(). However,
    // on Zephyr there is no access to the status of sbrk()
    // so this will have to do, just note that as heap
    // reduces it may suddenly jump up again when newlib asks
    // for more room
    // Note: there's currently no way to do this
    // with the built-in Zephyr malloc()ator
    struct mallinfo mallInfo = mallinfo();

    heapFreeOrError = (int32_t) mallInfo.fordblks;
#endif

    return heapFreeOrError;
}

// Enter a critical section.
U_INLINE int32_t uPortEnterCritical()
{
    gIrqLockKey = irq_lock();
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Leave a critical section.
U_INLINE void uPortExitCritical()
{
    irq_unlock(gIrqLockKey);
}

static int ubxlib_preinit(const struct device *arg)
{
    ARG_UNUSED(arg);

    k_thread_system_pool_assign(k_current_get());
    return 0;
}

SYS_INIT(ubxlib_preinit, PRE_KERNEL_1, 0);

// End of file
