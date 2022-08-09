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
 * @brief Implementation of generic porting functions for the NRF52 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_compiler.h" // For U_INLINE
#include "u_cfg_hw_platform_specific.h"

#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_private.h"
#include "u_port_event_queue_private.h"

#ifndef __SES_ARM
#include "u_heap_check.h"
#endif

#include "FreeRTOS.h"
#include "task.h"

#include "nrf_drv_clock.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

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
    TaskHandle_t taskHandle;

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

#if NRF_LOG_ENABLED
        NRF_LOG_INIT(NULL);
        NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif

#if configTICK_SOURCE == FREERTOS_USE_RTC
        // If the clock has not already been started, start it
        nrf_drv_clock_init();
#endif
        // Need to have the high frequency clock
        // running for the UART driver, otherwise
        // it can drop characters at 115,200 baud.
        // If you do NOT use the UART driver you don't
        // need this line: it is put here rather than
        // down in the UART driver as it should be the
        // application's responsibility to configure
        // global clocks, not some random driver code
        // that has no context.
        nrfx_clock_hfclk_start();

        // Note that stack size is in words on the native FreeRTOS
        // that NRF52 uses, hence the divide by four here.
        if (xTaskCreate(pEntryPoint, "EntryPoint",
                        stackSizeBytes / 4, pParameter,
                        priority, &taskHandle) == pdPASS) {

            // Activate deep sleep mode.
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

            // Initialise logging.
            uPortPrivateLoggingInit();

            // Start the scheduler.
            vTaskStartScheduler();

            // Should never get here
        }
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t uPortInit()
{
    int32_t errorCode = 0;

    if (!gInitialised) {
        errorCode = uPortEventQueuePrivateInit();
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
    int32_t tickTime = 0;

    if (gInitialised) {
        tickTime = uPortPrivateGetTickTimeMs();
    }

    return tickTime;
}

// Get the minimum amount of heap free, ever, in bytes.
int32_t uPortGetHeapMinFree()
{
    int32_t minFree = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

#ifndef __SES_ARM
    // Segger Embedded Studio uses its own C libraries which
    // do not offer a way to monitor max heap usage
    minFree = (int32_t) uHeapCheckGetMinFree();
#endif

    return minFree;
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
    return (int32_t) xPortGetFreeHeapSize();
}

// Enter a critical section.
U_INLINE int32_t uPortEnterCritical()
{
    taskENTER_CRITICAL();
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Leave a critical section.
U_INLINE void uPortExitCritical()
{
    taskEXIT_CRITICAL();
}

// End of file
