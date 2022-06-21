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
 * @brief Implementation of the port GPIO API for the ESP32 platform.
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

#include "driver/gpio.h"
#include "driver/rtc_io.h"

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
    gpio_config_t config;

    if (pConfig != NULL) {
        // Set the things that won't change
        config.intr_type = GPIO_INTR_DISABLE;

        // Set the direction and drive mode
        switch (pConfig->direction) {
            case U_PORT_GPIO_DIRECTION_NONE:
                config.mode = GPIO_MODE_DISABLE;
                break;
            case U_PORT_GPIO_DIRECTION_INPUT:
                config.mode = GPIO_MODE_INPUT;
                break;
            case U_PORT_GPIO_DIRECTION_OUTPUT:
                config.mode = GPIO_MODE_OUTPUT;
                if (pConfig->driveMode == U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN) {
                    config.mode = GPIO_MODE_OUTPUT_OD;
                }
                break;
            case U_PORT_GPIO_DIRECTION_INPUT_OUTPUT:
                config.mode = GPIO_MODE_INPUT_OUTPUT;
                if (pConfig->driveMode == U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN) {
                    config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
                }
                break;
            default:
                badConfig = true;
                break;
        }

        // Set the pull up/down:
        // Note that pulling both up and down is apparently
        // valid for ESP32
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        switch (pConfig->pullMode) {
            case U_PORT_GPIO_PULL_MODE_NONE:
                break;
            case U_PORT_GPIO_PULL_MODE_PULL_UP:
                config.pull_up_en = GPIO_PULLUP_ENABLE;
                break;
            case U_PORT_GPIO_PULL_MODE_PULL_DOWN:
                config.pull_down_en = GPIO_PULLDOWN_ENABLE;
                break;
            default:
                badConfig = true;
                break;
        }

        // Set the pin
        config.pin_bit_mask = 1ULL << pConfig->pin;

        // Actually do the configuration
        if (!badConfig) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            if (gpio_config(&config) == ESP_OK) {
                // If it's an output pin, set the drive capability
                if (((pConfig->direction == U_PORT_GPIO_DIRECTION_OUTPUT) ||
                     (pConfig->direction == U_PORT_GPIO_DIRECTION_INPUT_OUTPUT)) &&
                    (gpio_set_drive_capability((gpio_num_t) pConfig->pin,
                                               (gpio_drive_cap_t) pConfig->driveCapability) == ESP_OK)) {
                    errorCode = U_ERROR_COMMON_SUCCESS;
                } else {
                    // It's not an output pin so we're done
                    errorCode = U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    return (int32_t) errorCode;
}

// Set the state of a GPIO.
// Note there used to be code here which tried to
// handle the case of a GPIO being made to hold
// its state during sleep.  However, a side-effect
// of doing that was that setting a GPIO when it had
// not yet been made an output, so that when it was made
// an output it immediately had the right level,
// did not work, so that code was removed.
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (gpio_set_level((gpio_num_t) pin, level) == ESP_OK) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Get the state of a GPIO.
int32_t uPortGpioGet(int32_t pin)
{
    return gpio_get_level((gpio_num_t) pin);
}

// End of file
