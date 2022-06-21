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
 * @brief Implementation of the port GPIO API for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_gpio.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"

#include "u_port_os.h"
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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure a GPIO.
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    bool badConfig = false;
    GPIO_InitTypeDef config = {0};

    // Note that Pin is a bitmap
    config.Pin = 1U << U_PORT_STM32F4_GPIO_PIN(pConfig->pin);
    config.Mode = GPIO_MODE_INPUT;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_LOW;

    if (pConfig != NULL) {
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
                        config.Mode = GPIO_MODE_OUTPUT_PP;
                        break;
                    case U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN:
                        config.Mode = GPIO_MODE_OUTPUT_OD;
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
                config.Pull = GPIO_PULLUP;
                break;
            case U_PORT_GPIO_PULL_MODE_PULL_DOWN:
                config.Pull = GPIO_PULLDOWN;
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
                          &config);
            errorCode = U_ERROR_COMMON_SUCCESS;
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
                      1U << U_PORT_STM32F4_GPIO_PIN(pin),
                      level);

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Get the state of a GPIO.
int32_t uPortGpioGet(int32_t pin)
{
    // Enable the clocks to the port for this pin
    uPortPrivateGpioEnableClock(pin);

    return HAL_GPIO_ReadPin(pUPortPrivateGpioGetReg(pin),
                            1U << U_PORT_STM32F4_GPIO_PIN(pin));
}

// End of file
