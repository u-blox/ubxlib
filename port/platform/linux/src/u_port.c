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
 * @brief Implementation of generic porting functions for the Linux platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MAX
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "time.h"
#include "unistd.h"
#include "malloc.h"

#include "u_cfg_sw.h"
#include "u_compiler.h" // For U_INLINE
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"
#include "u_port_os_private.h"
#include "u_port_ppp_private.h"

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
    (void)stackSizeBytes;
    (void)priority;
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
    uErrorCode_t errorCode;
    // uPortOsPrivateInit() must be called first for
    // U_CFG_MUTEX_DEBUG option to work on Linux
    errorCode = uPortOsPrivateInit();
    if (errorCode == 0) {
        errorCode = uPortEventQueuePrivateInit();
    }
    if (errorCode == 0) {
        errorCode = uPortUartInit();
    }
    if (errorCode == 0) {
        errorCode = uPortPppPrivateInit();
    }
    return errorCode;
}

// Deinitialise the porting layer.
void uPortDeinit()
{
    uPortPppPrivateDeinit();
    uPortUartDeinit();
    uPortEventQueuePrivateDeinit();
    uPortOsPrivateDeinit();
}

// Get the current tick converted to a time in milliseconds.
int32_t uPortGetTickTimeMs()
{
    int32_t ms = 0;
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
        // Ensure that the calculation wraps correctly
        ms = ((((int64_t) ts.tv_sec) * 1000) + (((int64_t) ts.tv_nsec) / 1000000)) % INT_MAX;
    }

    return ms;
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    // Not supported as mallinfo() doesn't seem usable.
    return U_ERROR_COMMON_NOT_SUPPORTED;
    /*
        struct mallinfo mallInfo;
        mallInfo = mallinfo();
        return mallInfo.fordblks;
    */
}

int32_t uPortGetTimezoneOffsetSeconds()
{
    struct tm utcTm;
    time_t utc;
    time_t mktimeSays;

    utc = time(NULL);
    utcTm = *gmtime(&utc);
    // Setting daylight saving flag to -1 causes mktime()
    // to decide whether DST is in effect
    utcTm.tm_isdst = -1;
    mktimeSays = mktime(&utcTm);
    // mktime will have subtracted the timezone from what it was
    // given in order to return local time, hence the timezone
    // offset is the difference

    return (int32_t)(utc - mktimeSays);
}

// End of file
