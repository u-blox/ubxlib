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
 * @brief Implementation of generic porting functions for Windows.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "limits.h"    // INT_MAX

#include "windows.h"

#include "u_cfg_sw.h"
#include "u_compiler.h" // For U_INLINE
#include "u_cfg_hw_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"
#include "u_assert.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_private.h"
#include "u_port_event_queue_private.h"

// A place to put the ID of the main thread, held over in
// u_port_private.c.
extern DWORD *gpMainThreadId;

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if defined(_MSV_VER) && !defined(_CHAR_UNSIGNED)
/** As explained in the README.md under the win32 directory, in Microsoft
 * Visual C++ char types are signed, which can lead to unexpected behaviours
 * e.g. a character value which contains 0xaa, when compared with the literal value
 * 0xaa, will return false; the character value is interpreted as being negative
 * because it has the top bit set, while the literal value 0xaa is positive.
 * To avoid this problem the command-line switch /J to the compiler must be
 * used.
 */
#error Please use the compilation switch /J to ensure char types are unsigned.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Keep track of whether we've been initialised or not.
static bool gInitialised = false;

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
    HANDLE threadHandle;

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

        threadHandle = CreateThread(NULL, // default security attributes
                                    (DWORD) U_CFG_OS_APP_TASK_STACK_SIZE_BYTES,
                                    (LPTHREAD_START_ROUTINE) pEntryPoint, pParameter,
                                    0,     // default creation flags
                                    gpMainThreadId);
        if (threadHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            if (!SetThreadPriority(threadHandle,
                                   (DWORD) uPortPrivateTaskPriorityConvert(priority))) {
                uPortLog("U_PORT: WARNING unable to set thread to priority %d [%d].\n",
                         priority, uPortPrivateTaskPriorityConvert(priority));
            }
            WaitForSingleObject(threadHandle, INFINITE);
        }
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    int32_t errorCode = 0;

    if (!gInitialised) {
        errorCode = uPortPrivateInit();
        if (errorCode == 0) {
            errorCode = uPortEventQueuePrivateInit();
            if (errorCode == 0) {
                errorCode = uPortUartInit();
            }
        }
        gInitialised = (errorCode == 0);
    }

    return errorCode;
}

// Deinitialise the porting layer.
void uPortDeinit()
{
    if (gInitialised) {
        uPortUartDeinit();
        uPortEventQueuePrivateDeinit();
        uPortPrivateDeinit();
        gInitialised = false;
    }
}

// Get the current tick in milliseconds.
int32_t uPortGetTickTimeMs()
{
    return GetTickCount() % INT_MAX;
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    return U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    return U_ERROR_COMMON_NOT_SUPPORTED;
}

// Enter a critical section.
int32_t uPortEnterCritical()
{
    return uPortPrivateEnterCritical();
}

// Leave a critical section.
void uPortExitCritical()
{
    U_ASSERT(uPortPrivateExitCritical() == 0);
}

// End of file
