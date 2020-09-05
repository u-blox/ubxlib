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
 * @brief Stuff private to the STM32F4 porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_hw_platform_specific.h" // For U_CFG_HW_SWO_CLOCK_HZ
#include "u_error_common.h"
#include "u_port.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_bus.h"

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

#include "assert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Address of The ITM Enable register.
 */
#define ITM_ENA   (*(volatile uint32_t *) 0xE0000E00)

/** Address of the ITM Trace Privilege register.
 */
#define ITM_TPR   (*(volatile uint32_t *) 0xE0000E40)

/** Address of the ITM Trace Control register.
 */
#define ITM_TCR   (*(volatile uint32_t *) 0xE0000E80)

/** Address of the ITM Lock Status register.
 */
#define ITM_LSR   (*(volatile uint32_t *) 0xE0000FB0)

/** Address of the Debug register.
 */
#define DHCSR     (*(volatile uint32_t *) 0xE000EDF0)

/** Address of another Debug register.
 */
#define DEMCR     (*(volatile uint32_t *) 0xE000EDFC)

/** Address of the Trace Unit Async Clock prescaler register.
 */
#define TPIU_ACPR (*(volatile uint32_t *) 0xE0040010)

/** Address of the Trace Unit Selected Pin Protocol register.
 */
#define TPIU_SPPR (*(volatile uint32_t *) 0xE00400F0)

/** Address of the DWT Control register.
 */
#define DWT_CTRL  (*(volatile uint32_t *) 0xE0001000)

/** Address of the Formattr And Flush Control register.
 */
#define FFCR      (*(volatile uint32_t *) 0xE0040304)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Counter to keep track of RTOS ticks: NOT static
// so that the stm32f4xx_it.c can update it.
int32_t gTickTimerRtosCount;

// Get the GPIOx address for a given GPIO port.
static GPIO_TypeDef *const gpGpioReg[] = {GPIOA,
                                          GPIOB,
                                          GPIOC,
                                          GPIOD,
                                          GPIOE,
                                          GPIOF,
                                          GPIOG,
                                          GPIOH,
                                          GPIOI,
                                          GPIOJ,
                                          GPIOK
                                         };

// Get the LL driver peripheral number for a given GPIO port.
static const int32_t gLlApbGrpPeriphGpioPort[] = {LL_AHB1_GRP1_PERIPH_GPIOA,
                                                  LL_AHB1_GRP1_PERIPH_GPIOB,
                                                  LL_AHB1_GRP1_PERIPH_GPIOC,
                                                  LL_AHB1_GRP1_PERIPH_GPIOD,
                                                  LL_AHB1_GRP1_PERIPH_GPIOE,
                                                  LL_AHB1_GRP1_PERIPH_GPIOF,
                                                  LL_AHB1_GRP1_PERIPH_GPIOG,
                                                  LL_AHB1_GRP1_PERIPH_GPIOH,
                                                  LL_AHB1_GRP1_PERIPH_GPIOI,
                                                  LL_AHB1_GRP1_PERIPH_GPIOJ,
                                                  LL_AHB1_GRP1_PERIPH_GPIOK
                                                 };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** This code from https://wiki.segger.com/J-Link_SWO_Viewer.
 * It enables SWO so that logging continues if the target resets
 * without the external debug tool beign aware.
 * This can be switched off by overriding U_CFG_HW_SWO_CLOCK_HZ to
 * -1, in which case the external debug tool will set it up instead.
 */
static void enableSwo()
{
#if U_CFG_HW_SWO_CLOCK_HZ >= 0

    uint32_t stimulusRegs;

    // Enable access to SWO registers
    DEMCR |= (1 << 24);
    ITM_LSR = 0xC5ACCE55;

    // Initially disable ITM and stimulus port
    // To make sure that nothing is transferred via SWO
    // when changing the SWO prescaler etc.
    stimulusRegs = ITM_ENA;
    stimulusRegs &= ~(1 << 0); // Disable stimulus port 0
    ITM_ENA = stimulusRegs;
    ITM_TCR = 0; // Disable ITM

    // Initialize SWO (prescaler, etc.)
    TPIU_SPPR = 0x00000002; // Select NRZ mode
    TPIU_ACPR = (SystemCoreClock / U_CFG_HW_SWO_CLOCK_HZ) - 1;
    ITM_TPR = 0x00000000;
    DWT_CTRL = 0x400003FE;
    FFCR = 0x00000100;

    // Enable ITM and stimulus port
    ITM_TCR = 0x1000D; // Enable ITM
    ITM_ENA = stimulusRegs | (1 << 0); // Enable stimulus port 0

#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT
 * -------------------------------------------------------------- */

// Initalise the private stuff.
int32_t uPortPrivateInit()
{
    gTickTimerRtosCount = 0;

    enableSwo();

    return U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the private stuff.
void uPortPrivateDeinit()
{
    // Nothing to do
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: GET TIME TICK
 * -------------------------------------------------------------- */

// Get the current tick converted to a time in milliseconds.
int64_t uPortPrivateGetTickTimeMs()
{
    return gTickTimerRtosCount;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: MISC
 * -------------------------------------------------------------- */

// Return the base address for a given GPIO pin.
GPIO_TypeDef *const pUPortPrivateGpioGetReg(int32_t pin)
{
    int32_t port = U_PORT_STM32F4_GPIO_PORT(pin);

    assert(port >= 0);
    assert(port < sizeof(gpGpioReg) / sizeof(gpGpioReg[0]));

    return gpGpioReg[port];
}

// Enable the clock to the register of the given GPIO pin.
void uPortPrivateGpioEnableClock(int32_t pin)
{
    int32_t port = U_PORT_STM32F4_GPIO_PORT(pin);

    assert(port >= 0);
    assert(port < sizeof(gLlApbGrpPeriphGpioPort) /
           sizeof(gLlApbGrpPeriphGpioPort[0]));
    LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[port]);
}

// End of file
