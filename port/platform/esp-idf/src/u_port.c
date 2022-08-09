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
 * @brief Implementation of generic porting functions for the ESP32 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // For U_INLINE
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"
#include "u_port_private.h"

#include "freertos/FreeRTOS.h" // For xPortGetFreeHeapSize()
#include "freertos/task.h"

#include "esp_timer.h" // For esp_timer_get_time()
#include "esp_system.h" // For esp_get_minimum_free_heap_size()

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Keep track of whether we've been initialised or not.
static bool gInitialised = false;

/** Mutex required for the ESP-IDF modified taskENTER_CRITICAL().
 */
static portMUX_TYPE gSpinlock = portMUX_INITIALIZER_UNLOCKED;

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

#ifndef ARDUINO
    (void) stackSizeBytes;
    (void) priority;
#endif

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
#ifndef ARDUINO
        // RTOS is already running, just call pEntryPoint
        pEntryPoint(pParameter);
#else
        // Under Arduino it is not possible to set the stack size
        // we would like for the main task since there is only one
        // global sdkconfig file that cannot be overridden, so in that
        // case we do start a task for our main task and delete this
        // one
        TaskHandle_t taskHandle;
        if (xTaskCreate(pEntryPoint, "EntryPoint",
                        stackSizeBytes, pParameter,
                        priority, &taskHandle) == pdPASS) {
            vTaskDelete(NULL);
        }
#endif
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    int32_t errorCode = 0;

    if (!gInitialised) {
        errorCode = uPortEventQueuePrivateInit();;
        if (errorCode == 0) {
            errorCode = uPortPrivateInit();
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
        uPortPrivateDeinit();
        uPortEventQueuePrivateDeinit();
        gInitialised = false;
    }
}

// Get the current tick converted to a time in milliseconds.
int32_t uPortGetTickTimeMs()
{
    return esp_timer_get_time() / 1000;
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    return (int32_t) esp_get_minimum_free_heap_size();
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    return (int32_t) xPortGetFreeHeapSize();
}

// Enter a critical section.
// Implementation note: FreeRTOS only locks-out tasks
// with interrupt priority up
// to configMAX_SYSCALL_INTERRUPT_PRIORITY, interrupts
// at a higher priority than that are NOT masked
// during a critical section, so beware!
// Also be careful if you have an interrupt watchdog
// running to make sure it is respected, see
// CONFIG_ESP_INT_WDT_TIMEOUT_MS in the ESP-IDF
// documentation.
U_INLINE int32_t uPortEnterCritical()
{
    portENTER_CRITICAL(&gSpinlock);
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Leave a critical section.
U_INLINE void uPortExitCritical()
{
    portEXIT_CRITICAL(&gSpinlock);
}

// End of file
