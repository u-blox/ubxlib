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
 * @brief Implementation of the port GPIO API for the Zephyr platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_gpio.h"

#include "zephyr.h"
#include "device.h"
#include "drivers/gpio.h"
#include "version.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/*
    **** NOTE ****
    The statements below is due to a change made in Zephyr 3.x.
    For some reason the gpio drive mode macros have changed from
    being generic to soc specific. The old macros are still there
    but marked as deprecated and hence giving build warnings.
    There is currently no directly correlated macros for Nrf and
    we also want to use the code in this file for other
    Zephyr targets. However in coming versions of ubxlib we have
    to deal with some solution for this.
*/
#undef __DEPRECATED_MACRO
#define __DEPRECATED_MACRO

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static const struct device *getGpioDevice(uint32_t pin)
{
    const struct device *pDev = NULL;
    // common practice in device trees that one gpio port holds 32 pins
#if KERNEL_VERSION_MAJOR < 3
    char dtDeviceNodeLabel[7];
    strncpy(dtDeviceNodeLabel, "GPIO_x", sizeof(dtDeviceNodeLabel));
    // replace x with corresponding port number
    dtDeviceNodeLabel[5] = '0' + (pin / GPIO_MAX_PINS_PER_PORT);
    dtDeviceNodeLabel[6] = 0;
    pDev = device_get_binding(dtDeviceNodeLabel);
#else
    int32_t port = pin / GPIO_MAX_PINS_PER_PORT;
    if (port == 0) {
        pDev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0));
    } else if (port == 1) {
        pDev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1));
    }
#endif
    return pDev;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure a GPIO.
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    bool badConfig = false;
    const struct device *pPort;
    gpio_flags_t flags = 0;
    int zerr;

    pPort = getGpioDevice(pConfig->pin);
    if (pPort == NULL) {
        return errorCode;
    }

    switch (pConfig->direction) {
        case U_PORT_GPIO_DIRECTION_NONE:
            flags = GPIO_DISCONNECTED;
            break;

        case U_PORT_GPIO_DIRECTION_INPUT: {
            flags |= GPIO_INPUT;

            switch (pConfig->pullMode) {
                case U_PORT_GPIO_PULL_MODE_NONE:
                    break;
                case U_PORT_GPIO_PULL_MODE_PULL_UP:
                    flags |= GPIO_PULL_UP;
                    break;
                case U_PORT_GPIO_PULL_MODE_PULL_DOWN:
                    flags |= GPIO_PULL_DOWN;
                    break;
                default:
                    badConfig = true;
                    break;
            }
            break;
        }


        case U_PORT_GPIO_DIRECTION_INPUT_OUTPUT:
        case U_PORT_GPIO_DIRECTION_OUTPUT: {
            flags |= GPIO_OUTPUT;
            if (pConfig->direction == U_PORT_GPIO_DIRECTION_INPUT_OUTPUT) {
                flags |= GPIO_INPUT;
            }
            switch (pConfig->driveMode) {
                case U_PORT_GPIO_DRIVE_MODE_NORMAL:
                    break;
                case U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN:
                    flags |= GPIO_OPEN_DRAIN;
                    break;
                default:
                    badConfig = true;
                    break;
            }
            switch (pConfig->driveCapability) {
                case U_PORT_GPIO_DRIVE_CAPABILITY_STRONG:
                case U_PORT_GPIO_DRIVE_CAPABILITY_WEAK:
                case U_PORT_GPIO_DRIVE_CAPABILITY_WEAKEST:
                    flags |= GPIO_DS_DFLT_HIGH | GPIO_DS_DFLT_LOW;
                    break;
                case U_PORT_GPIO_DRIVE_CAPABILITY_STRONGEST:
                    // presuming that the alternative drive strength is stronger
                    flags |= GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW;
                    break;
                default:
                    badConfig = true;
                    break;
            }
            break;
        }

        default:
            badConfig = true;
            break;
    }

    if (!badConfig) {
        zerr = gpio_pin_configure(pPort, pConfig->pin % GPIO_MAX_PINS_PER_PORT,
                                  flags);
        if (!zerr) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Set the state of a GPIO.
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    const struct device *pPort;
    int zerr;

    pPort = getGpioDevice(pin);
    if (pPort == NULL) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    zerr = gpio_pin_set_raw(pPort, pin % GPIO_MAX_PINS_PER_PORT, (int)level);
    if (zerr) {
        return (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Get the state of a GPIO.
int32_t uPortGpioGet(int32_t pin)
{
    const struct device *pPort;
    int zerr;
    gpio_port_value_t val;

    pPort = getGpioDevice(pin);
    if (pPort == NULL) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    zerr = gpio_port_get_raw(pPort, &val);
    if (zerr) {
        return (int32_t)U_ERROR_COMMON_DEVICE_ERROR;
    }

    return (val & (1 << (pin % GPIO_MAX_PINS_PER_PORT))) ? 1 : 0;
}

// End of file
