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
 * @brief Implementation of generic porting functions for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "inttypes.h"
#include "stdbool.h"

#include "u_compiler.h" // For U_INLINE
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"

#include "u_heap_check.h"

#include "FreeRTOS.h" // For xPortGetFreeHeapSize()
#include "task.h"     // For taskENTER_CRITICAL()/taskEXIT_CRITICAL()
#include "stm32f437xx.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

#include "stdio.h"

#include "u_port_private.h" // Down here 'cos it needs GPIO_TypeDef

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

// System Clock Configuration
static void systemClockConfig(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure the main internal regulator output voltage
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    // Initialize the CPU, AHB and APB bus clocks
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = HSE_VALUE / 1000000U;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        U_ASSERT(false);
    }

    // Initialize the CPU, AHB and APB bus clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        U_ASSERT(false);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *pFile, uint32_t line)
{
    // printf() rather than uPortLog so that it is always
    // emitted, irrespective of whether debug is on or not
    printf("assert %s: %"PRIu32"\n", pFile, line);
    U_ASSERT(false);
}
#endif /* USE_FULL_ASSERT */

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
    osThreadId threadId;

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

        // Reset all peripherals, initialize the Flash interface and the Systick
        HAL_Init();

        // Configure the system clock
        systemClockConfig();

        // TODO: if I put an iprintf() here then all is fine.
        // If I don't then any attempt to print later
        // results in a hard fault.  Need to find out why.
        iprintf("\n\nU_APP: starting RTOS...\n");

        // Create the task, noting that the stack
        // size is in words not bytes
        osThreadDef(EntryPoint, (os_pthread) pEntryPoint,
                    priority, 0, stackSizeBytes >> 2);
        threadId = osThreadCreate(osThread(EntryPoint), pParameter);

        if (threadId != NULL) {
            // Start the scheduler.
            osKernelStart();
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
    return (int32_t) uHeapCheckGetMinFree();
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
// Also note that the system tick is disabled
// during a critical section (that's how it does
// what it does) and in the case of this STM32F4
// port that will stop time since uPortGetTickTimeMs()
// is incremented by the system tick.
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
