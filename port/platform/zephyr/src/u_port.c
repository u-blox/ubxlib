/*
 * Copyright 2020 u-blox Ltd
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

/** @file
 * @brief Implementation of generic porting functions for the NRF52 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "malloc.h"    // For mallinfo

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"

#include "zephyr.h"

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

// Start the platform.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    (void) stackSizeBytes;
    (void) priority;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        pEntryPoint(pParameter);
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;
    errorCode = uPortUartInit();
    if (errorCode == U_ERROR_COMMON_SUCCESS) {
        errorCode = uPortEventQueuePrivateInit();
    }
    return errorCode;
}

// Deinitialise the porting layer.
void uPortDeinit()
{
    uPortUartDeinit();
    uPortEventQueuePrivateDeinit();
}

// Get the current tick converted to a time in milliseconds.
int64_t uPortGetTickTimeMs()
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
// IMPORTANT: this ISN'T actually the free heap, it is
// simply the heap which newlib has asked for from
// which is the real source of heap sbrk(). However,
// on Zephyr there is no access to the status of sbrk()
// so this will have to do, just note that as heap
// reduces it may suddenly jump up again when newlib asks
// for more room
int32_t uPortGetHeapFree()
{
    struct mallinfo mallInfo = mallinfo();

    return (int32_t) mallInfo.fordblks;
}

// End of file
