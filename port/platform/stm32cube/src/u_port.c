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
 * @brief Implementation of generic porting functions for the STM32 platform.
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
#include "u_port_heap.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue_private.h"

#include "u_heap_check.h"

#ifdef STM32U575xx
# include "stm32u575xx.h"
# include "stm32u5xx_hal.h"
#else
# include "FreeRTOS.h" // For xPortGetFreeHeapSize()
# include "task.h"     // For taskENTER_CRITICAL()/taskEXIT_CRITICAL()
# include "stm32f437xx.h"
# include "stm32f4xx_hal.h"
#endif

#ifdef CMSIS_V2
# include "cmsis_os2.h"
#else
# include "cmsis_os.h"
#endif

#include "stdio.h"

#include "u_port_private.h" // Down here 'cos it needs GPIO_TypeDef

#if defined(U_PORT_STM32_PURE_CMSIS) && defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
// xPortGetFreeHeapSize(), which we implement in heap_useNewlib.c,
// is a FreeRTOS function signature but, in fact, the implementation
// is nothing at all to do with FreeRTOS and so we can use it even
// in the pure CMSIS case.
extern size_t xPortGetFreeHeapSize(void);
#endif

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

#ifdef STM32U575xx
    // Configure the main internal regulator output voltage
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        U_ASSERT(false);
    }

    // Initialize the CPU, AHB and APB buses clocks
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 80;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        U_ASSERT(false);
    }

    // Initialize the CPU, AHB and APB buses clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        U_ASSERT(false);
    }
#else
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
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *pFile, uint32_t line)
{
    // printf() rather than uPortLog so that it is always
    // emitted, irrespective of whether debug is on or not
    printf("assert %s: %"PRIu32"\n", pFile, line);
    U_ASSERT(false);
}
#endif /* USE_FULL_ASSERT */

// Start the platform.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
#ifdef CMSIS_V2
    osThreadId_t threadId;
    osThreadAttr_t attr = {0};
#else
    osThreadId threadId;
    osThreadDef_t threadDef = {0};
#endif

    if (pEntryPoint != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;

        // Reset all peripherals, initialize the Flash interface and the Systick
        HAL_Init();

        // Configure the system clock
        systemClockConfig();

        // Create the task, noting that the stack
        // size is in words not bytes
#ifdef CMSIS_V2
        osKernelInitialize();

        attr.name = "EntryPoint";
        attr.priority = priority;
        // For the CMSIS V2 port atop FreeRTOS the stack size is in bytes
        attr.stack_size = stackSizeBytes;
        threadId = osThreadNew(pEntryPoint, pParameter, &attr);
#else
        // TODO: if I put an iprintf() here then all is fine.
        // If I don't then any attempt to print later
        // results in a hard fault.  Need to find out why.
        iprintf("\n\nU_APP: starting RTOS...\n");

        threadDef.name = "EntryPoint";
        threadDef.pthread = (void (*) (void const *)) pEntryPoint;
        threadDef.tpriority = priority;
        threadDef.instances = 0;
        // Stack size is in words here, not bytes
        threadDef.stacksize = stackSizeBytes >> 2;
        threadId = osThreadCreate(&threadDef, pParameter);
#endif

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
        errorCode = uPortHeapMonitorInit(NULL, NULL, NULL);
        if (errorCode == 0) {
            errorCode = uPortEventQueuePrivateInit();
            if (errorCode == 0) {
                errorCode = uPortPrivateInit();
                if (errorCode == 0) {
                    errorCode = uPortUartInit();
                }
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
#if !defined(U_PORT_STM32_PURE_CMSIS) || defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
    //  Can get this if on FreeRTOS as we just use newlib's mallocator
    return (int32_t) uHeapCheckGetMinFree();
#else
    // Can't get this information from the ST_provided
    // CMSIS layer or from ThreadX's memory pool implementation
    // directly
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
#endif
}

// Get the current free heap.
int32_t uPortGetHeapFree()
{
#if !defined(U_PORT_STM32_PURE_CMSIS) || defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
    //  Can get this if on FreeRTOS as we just use newlib's mallocator
    return (int32_t) xPortGetFreeHeapSize();
#else
    // Can't get this information from the ST_provided
    // CMSIS layer or from ThreadX's memory pool implementation
    // directly
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
#endif
}

// Enter a critical section.
// Implementation note: FreeRTOS only locks-out tasks
// with interrupt priority up to
// configMAX_SYSCALL_INTERRUPT_PRIORITY, interrupts
// at a higher priority than that are NOT masked
// during a critical section, so beware!
// Also note that the system tick is disabled
// during a critical section (that's how it does
// what it does) and in the case of this STM32
// port that will stop time since uPortGetTickTimeMs()
// is incremented by the system tick.
U_INLINE int32_t uPortEnterCritical()
{
#ifdef U_PORT_STM32_PURE_CMSIS
    return uPortPrivateEnterCriticalCmsis();
#else
    taskENTER_CRITICAL();
    return (int32_t) U_ERROR_COMMON_SUCCESS;
#endif
}

// Leave a critical section.
U_INLINE void uPortExitCritical()
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uPortPrivateExitCriticalCmsis();
#else
    taskEXIT_CRITICAL();
#endif
}

// End of file
