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
 * @brief Implementation of the port GPIO API for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_cfg_hw_platform_specific.h"

#include "u_port.h"
#include "u_port_gpio.h"
#include "u_port_debug.h"

#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"

#include "u_port_os.h"
#include "u_port_private.h" // Down here 'cos it needs GPIO_TypeDef

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The maximum number of EXTI HW blocks on an STM32F4.
#define U_PORT_MAX_NUM_EXTI 16

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table of IRQs for each EXTI.
 */
static const IRQn_Type gIrqNType[] = {
    EXTI0_IRQn,     // EXTI 0
    EXTI1_IRQn,     // EXTI 1
    EXTI2_IRQn,     // EXTI 2
    EXTI3_IRQn,     // EXTI 3
    EXTI4_IRQn,     // EXTI 4
    EXTI9_5_IRQn,   // EXTI 5
    EXTI9_5_IRQn,   // EXTI 6
    EXTI9_5_IRQn,   // EXTI 7
    EXTI9_5_IRQn,   // EXTI 8
    EXTI9_5_IRQn,   // EXTI 9
    EXTI15_10_IRQn, // EXTI 10
    EXTI15_10_IRQn, // EXTI 11
    EXTI15_10_IRQn, // EXTI 12
    EXTI15_10_IRQn, // EXTI 13
    EXTI15_10_IRQn, // EXTI 14
    EXTI15_10_IRQn  // EXTI 15
};

/** Array of EXTIs, so that the code can determine which ones
 * are available.
 */
static bool gExtiAvailable[] = {
    U_CFG_HW_EXTI_0_AVAILABLE,
    U_CFG_HW_EXTI_1_AVAILABLE,
    U_CFG_HW_EXTI_2_AVAILABLE,
    U_CFG_HW_EXTI_3_AVAILABLE,
    U_CFG_HW_EXTI_4_AVAILABLE,
    U_CFG_HW_EXTI_5_AVAILABLE,
    U_CFG_HW_EXTI_6_AVAILABLE,
    U_CFG_HW_EXTI_7_AVAILABLE,
    U_CFG_HW_EXTI_8_AVAILABLE,
    U_CFG_HW_EXTI_9_AVAILABLE,
    U_CFG_HW_EXTI_10_AVAILABLE,
    U_CFG_HW_EXTI_11_AVAILABLE,
    U_CFG_HW_EXTI_12_AVAILABLE,
    U_CFG_HW_EXTI_13_AVAILABLE,
    U_CFG_HW_EXTI_14_AVAILABLE,
    U_CFG_HW_EXTI_15_AVAILABLE
};

/** Array of callbacks to handle interrupts.
 */
static void (*gpCallback[U_PORT_MAX_NUM_EXTI])(void) = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set NVIC for the given pin/EXTI.
static void setNvic(int32_t pinOrExti, bool enableNotDisable)
{
    IRQn_Type irgNType = gIrqNType[U_PORT_STM32F4_GPIO_PIN(pinOrExti)];

    if (enableNotDisable) {
        NVIC_SetPriority(irgNType,
                         NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
        NVIC_EnableIRQ(irgNType);
    } else {
        NVIC_DisableIRQ(irgNType);
    }
}

#if U_CFG_HW_EXTI_0_AVAILABLE  || U_CFG_HW_EXTI_1_AVAILABLE  || U_CFG_HW_EXTI_2_AVAILABLE  || U_CFG_HW_EXTI_3_AVAILABLE  || \
    U_CFG_HW_EXTI_4_AVAILABLE  || U_CFG_HW_EXTI_5_AVAILABLE  || U_CFG_HW_EXTI_6_AVAILABLE  || U_CFG_HW_EXTI_7_AVAILABLE  || \
    U_CFG_HW_EXTI_8_AVAILABLE  || U_CFG_HW_EXTI_9_AVAILABLE  || U_CFG_HW_EXTI_10_AVAILABLE || U_CFG_HW_EXTI_11_AVAILABLE || \
    U_CFG_HW_EXTI_12_AVAILABLE || U_CFG_HW_EXTI_13_AVAILABLE || U_CFG_HW_EXTI_14_AVAILABLE || U_CFG_HW_EXTI_15_AVAILABLE

// Generic EXTI interrupt handler.
static void extiIrq(size_t exti)
{
    uint16_t pin = exti;
    // EXTI->PR is the EXTI(s) (plural) that
    // have been triggered
    uint16_t pr = (uint16_t) EXTI->PR;

    // Need to check if a bit is set as interrupts can have
    // been queued and get triggered when SYSCFG is changed.
    // even when there isn't one (or at least, any there
    // were should have been cancelled)
    if (pr) {
        if (exti < 5) {
            // For exti 0 to 4 things are simple as there
            // is a separate interrupt for each EXTI, so
            // there is nothing to do, the EXTI _is_ the pin
        } else if (exti < 10) {
            // There is a single interrupt for all of EXTI
            // 5 to 9 so we have to find the pin by seeing
            // which port we had set in the 5 to 9 range
            if (pr & (1 << 5)) {
                pin = 5;
            } else if (pr & (1 << 6)) {
                pin = 6;
            } else if (pr & (1 << 7)) {
                pin = 7;
            } else if (pr & (1 << 8)) {
                pin = 8;
            } else {
                pin = 9;
            }
        } else {
            // Same for EXTI 10 to 15
            if (pr & (1 << 10)) {
                pin = 10;
            } else if (pr & (1 << 11)) {
                pin = 11;
            } else if (pr & (1 << 12)) {
                pin = 12;
            } else if (pr & (1 << 13)) {
                pin = 13;
            } else if (pr & (1 << 14)) {
                pin = 14;
            } else {
                pin = 15;
            }
        }

        // Now that we have a unique pin, clear that
        // pending bit and call the callback
        EXTI->PR = 1 << pin;
        if (gpCallback[pin] != NULL) {
            gpCallback[pin]();
        }
    }
}

#endif

// Set the port number in the given EXTI control register of SYSCFG.
static void setSyscfgExtiCr(size_t exti, uint16_t port)
{
    uint16_t extiBitMap = (uint16_t) (1 << exti);
    // The 16 EXTIs are split over the lower 16 bits of four
    // SYSCFG->EXTICR registers
    size_t extiCrRegIndex = exti >> 2;
    // The bit-offset to the correct 4-bit region within the
    // EXTICR register for the given EXTI
    size_t extiCrRegOffset = (exti % 4) << 2;

    // Zero the 4-bit region of the EXTICR
    SYSCFG->EXTICR[extiCrRegIndex] &= (~0x0F) << extiCrRegOffset;
    // Now set the register bit; I have seen the corresponding
    // EXTI->PR register bit be set here, even when the external
    // input has not done anything yet, so mask the corresponding
    // interrupt while we configure it and reset the EXTI->PR bit
    // afterwards, before unmasking it
    EXTI->IMR &= ~extiBitMap;
    SYSCFG->EXTICR[extiCrRegIndex] |= (port & 0x0F) << extiCrRegOffset;
    EXTI->PR = 1 << exti;
    EXTI->IMR |= extiBitMap;
}

// The inner part of setInterruptHandler().
static int32_t setInterruptHandlerInner(int32_t pin,
                                        void (*pCallback)(void))
{
    uint16_t exti = U_PORT_STM32F4_GPIO_PIN(pin);

    // Set the callback first in case it goes off immediately
    // as a result of the configuration
    gpCallback[exti] = pCallback;
    setSyscfgExtiCr(exti, U_PORT_STM32F4_GPIO_PORT(pin));
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Set up the interrupt handler for a given interrupt pin.
static int32_t setInterruptHandler(int32_t pin,
                                   void (*pCallback)(void))
{
    // "Configuration" error to hint to the user that there
    // is a configuratio issue (i.e. the relevant EXTI
    // interrupt has not been made available to this code
    // in the u_cfg_hw_platform_specific.h for STM32Cube).
    int32_t errorCode = (int32_t) U_ERROR_COMMON_CONFIGURATION;
    uint16_t pinWithinPort = U_PORT_STM32F4_GPIO_PIN(pin);

    if (gExtiAvailable[pinWithinPort]) {
        errorCode = setInterruptHandlerInner(pin, pCallback);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * INTERRUPT HANDLERS
 * -------------------------------------------------------------- */

#if U_CFG_HW_EXTI_0_AVAILABLE
// EXTI 0 interrupt handler.
void EXTI0_IRQHandler()
{
    // Pin 0 on the port configured in the relevant SYSCFG_EXTICR
    // has changed state
    extiIrq(0);
}
#endif

#if U_CFG_HW_EXTI_1_AVAILABLE
// EXTI 1 interrupt handler.
void EXTI1_IRQHandler()
{
    // Pin 1 on the port configured in the relevant SYSCFG_EXTICR
    // has changed state
    extiIrq(1);
}
#endif

#if U_CFG_HW_EXTI_2_AVAILABLE
// EXTI 2 interrupt handler.
void EXTI2_IRQHandler()
{
    // Pin 2 on the port configured in the relevant SYSCFG_EXTICR
    // has changed state
    extiIrq(2);
}
#endif

#if U_CFG_HW_EXTI_3_AVAILABLE
// EXTI 3 interrupt handler.
void EXTI3_IRQHandler()
{
    // Pin 3 on the port configured in the relevant SYSCFG_EXTICR
    // has changed state
    extiIrq(3);
}
#endif

#if U_CFG_HW_EXTI_4_AVAILABLE
// EXTI 4 interrupt handler.
void EXTI4_IRQHandler()
{
    // Pin 4 on the port configured in the relevant SYSCFG_EXTICR
    // has changed state
    extiIrq(4);
}
#endif

#if U_CFG_HW_EXTI_5_AVAILABLE || U_CFG_HW_EXTI_6_AVAILABLE || U_CFG_HW_EXTI_7_AVAILABLE || \
    U_CFG_HW_EXTI_8_AVAILABLE || U_CFG_HW_EXTI_9_AVAILABLE
// EXTI 5 to 9 interrupt handler.
void EXTI9_5_IRQHandler()
{
    // Pin 5, 6, 7, 8 or 9 on the port configured in the relevant
    // SYSCFG_EXTICR has changed state
    extiIrq(5);
}
#endif

#if U_CFG_HW_EXTI_10_AVAILABLE || U_CFG_HW_EXTI_11_AVAILABLE || U_CFG_HW_EXTI_12_AVAILABLE || \
    U_CFG_HW_EXTI_13_AVAILABLE || U_CFG_HW_EXTI_14_AVAILABLE || U_CFG_HW_EXTI_15_AVAILABLE
// EXTI 10 to 15 interrupt handler.
void EXTI15_10_IRQHandler()
{
    // Pin 10, 11, 12, 13, 14 or 15 on the port configured in the relevant
    // SYSCFG_EXTICR has changed state
    extiIrq(10);
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure a GPIO.
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    bool badConfig = false;
    GPIO_InitTypeDef gpioConfig = {0};

    // Note that Pin is a bitmap
    gpioConfig.Pin = 1U << U_PORT_STM32F4_GPIO_PIN(pConfig->pin);
    gpioConfig.Mode = GPIO_MODE_INPUT;
    gpioConfig.Pull = GPIO_NOPULL;
    gpioConfig.Speed = GPIO_SPEED_FREQ_LOW;

    // Note: level triggered interrupts not supported on STM32F4
    if ((pConfig != NULL) && ((pConfig->pInterrupt == NULL) ||
                              !pConfig->interruptLevel)) {
        // Set the direction and mode
        switch (pConfig->direction) {
            case U_PORT_GPIO_DIRECTION_NONE:
            case U_PORT_GPIO_DIRECTION_INPUT:
                // Nothing to do
                break;
            case U_PORT_GPIO_DIRECTION_INPUT_OUTPUT:
            case U_PORT_GPIO_DIRECTION_OUTPUT:
                switch (pConfig->driveMode) {
                    case U_PORT_GPIO_DRIVE_MODE_NORMAL:
                        gpioConfig.Mode = GPIO_MODE_OUTPUT_PP;
                        break;
                    case U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN:
                        gpioConfig.Mode = GPIO_MODE_OUTPUT_OD;
                        break;
                    default:
                        badConfig = true;
                        break;
                }
                break;
            default:
                badConfig = true;
                break;
        }

        // Set pull up/down
        switch (pConfig->pullMode) {
            case U_PORT_GPIO_PULL_MODE_NONE:
                // No need to do anything
                break;
            case U_PORT_GPIO_PULL_MODE_PULL_UP:
                gpioConfig.Pull = GPIO_PULLUP;
                break;
            case U_PORT_GPIO_PULL_MODE_PULL_DOWN:
                gpioConfig.Pull = GPIO_PULLDOWN;
                break;
            default:
                badConfig = true;
                break;
        }

        // Setting drive strength is not supported on this platform

        // Actually do the configuration
        if (!badConfig) {
            // Enable the clocks to the port for this pin
            uPortPrivateGpioEnableClock(pConfig->pin);
            // The GPIO init function for STM32F4 takes a pointer
            // to the port register, the index for which is the upper
            // nibble of pin (they are in banks of 16), and then
            // the configuration structure which has the pin number
            // within that port.
            HAL_GPIO_Init(pUPortPrivateGpioGetReg(pConfig->pin),
                          &gpioConfig);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }

        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            if (pConfig->pInterrupt != NULL) {
                // Make sure the SYSCFG block that configures
                // the EXTI block has a clock, and that EXTI
                // has a clock
                LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
#ifdef LL_APB2_GRP1_PERIPH_EXTI
                LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_EXTI);
#endif
                // Set up the interrupt
                errorCode = setInterruptHandler(pConfig->pin,
                                                pConfig->pInterrupt);
                if (errorCode == 0) {
                    // Set rising or falling edge (using the bit-map version, Pin)
                    if (pConfig->interruptActiveLow) {
                        EXTI->FTSR |= gpioConfig.Pin;
                        EXTI->RTSR &= ~gpioConfig.Pin;
                    } else {
                        EXTI->RTSR |= gpioConfig.Pin;
                        EXTI->FTSR &= ~gpioConfig.Pin;
                    }
                    // Do the NVIC part and we're off
                    setNvic(pConfig->pin, true);
                }
            } else {
                // If an interrupt is NOT requested then, in case it had been
                // previously, unset it here
                if (setInterruptHandler(pConfig->pin, NULL) == 0) {
                    setNvic(pConfig->pin, false);
                }
            }
        }
    }

    return (int32_t) errorCode;
}

// Set the state of a GPIO.
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    // Enable the clocks to the port for this pin
    uPortPrivateGpioEnableClock(pin);

    HAL_GPIO_WritePin(pUPortPrivateGpioGetReg(pin),
                      (uint16_t) (1U << U_PORT_STM32F4_GPIO_PIN(pin)),
                      level);

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Get the state of a GPIO.
int32_t uPortGpioGet(int32_t pin)
{
    // Enable the clocks to the port for this pin
    uPortPrivateGpioEnableClock(pin);

    return HAL_GPIO_ReadPin(pUPortPrivateGpioGetReg(pin),
                            (uint16_t) (1U << U_PORT_STM32F4_GPIO_PIN(pin)));
}

// Interrupt support.
bool uPortGpioInterruptSupported()
{
    return U_CFG_HW_EXTI_0_AVAILABLE  || U_CFG_HW_EXTI_1_AVAILABLE  || U_CFG_HW_EXTI_2_AVAILABLE  ||
           U_CFG_HW_EXTI_3_AVAILABLE  || U_CFG_HW_EXTI_4_AVAILABLE  || U_CFG_HW_EXTI_5_AVAILABLE  ||
           U_CFG_HW_EXTI_6_AVAILABLE  || U_CFG_HW_EXTI_7_AVAILABLE  || U_CFG_HW_EXTI_8_AVAILABLE  ||
           U_CFG_HW_EXTI_9_AVAILABLE  || U_CFG_HW_EXTI_10_AVAILABLE || U_CFG_HW_EXTI_11_AVAILABLE ||
           U_CFG_HW_EXTI_12_AVAILABLE || U_CFG_HW_EXTI_13_AVAILABLE || U_CFG_HW_EXTI_14_AVAILABLE ||
           U_CFG_HW_EXTI_15_AVAILABLE;
}

// End of file
