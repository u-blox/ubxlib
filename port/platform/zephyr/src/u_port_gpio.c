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
#include "u_port_os.h"      // Needed by u_port_private.h
#include "u_port_gpio.h"

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#else
#include <kernel.h>
#include <device.h>
#include <drivers/gpio.h>
#endif

#include "u_port_private.h"  // Down here because it needs to know about the Zephyr device tree

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
    const struct device *pPort;
    gpio_flags_t flags = 0;
    int zerr;

    pPort = pUPortPrivateGetGpioDevice(pConfig->pin);
    if (pPort == NULL) {
        return errorCode;
    }

#if defined(CONFIG_ARCH_POSIX)
    // Set up interrupt flags
    if (pConfig->pInterrupt != NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    }
#endif

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
#if KERNEL_VERSION_MAJOR < 3
                    // For some reason the gpio drive mode macros have
                    // changed from being generic to soc specific in
                    // Zephyr 3 and later, hence we can't easily
                    // support them here.
                    flags |= GPIO_DS_DFLT_HIGH | GPIO_DS_DFLT_LOW;
#endif
                    break;
                case U_PORT_GPIO_DRIVE_CAPABILITY_STRONGEST:
#if KERNEL_VERSION_MAJOR < 3
                    // presuming that the alternative drive strength is stronger
                    flags |= GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW;
#endif
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
        zerr = gpio_pin_configure(pPort,
                                  (gpio_pin_t) (pConfig->pin % uPortPrivateGetGpioPortMaxPins()),
                                  flags);
        if (zerr == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    if (errorCode == U_ERROR_COMMON_SUCCESS) {
#if !defined(CONFIG_ARCH_POSIX)
        // In case there is already an interrupt handler for this pin,
        // remove it before we continue either (a) setting up a
        // new one or (b) leaving the pin as a non-interrupt pin
        uPortPrivateGpioCallbackRemove(pConfig->pin);
        if (pConfig->pInterrupt != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            // Set up the flags
            flags = 0;
            if (pConfig->interruptLevel) {
                if (pConfig->interruptActiveLow) {
                    flags |= GPIO_INT_LEVEL_LOW;
                } else {
                    flags |= GPIO_INT_LEVEL_HIGH;
                }
            } else {
                if (pConfig->interruptActiveLow) {
                    flags |= GPIO_INT_EDGE_FALLING;
                } else {
                    flags |= GPIO_INT_EDGE_RISING;
                }
            }
            zerr = gpio_pin_interrupt_configure(pPort,
                                                (gpio_pin_t) (pConfig->pin % uPortPrivateGetGpioPortMaxPins()),
                                                flags);
            if ((zerr == 0) || (zerr = -EWOULDBLOCK)) {
                // Set the callback
                errorCode = uPortPrivateGpioCallbackAdd(pConfig->pin,
                                                        pConfig->pInterrupt);
            } else {
                // Since all kinds of platform-dependent things could
                // go wrong here, try to give the user a useful error code
                switch (zerr) {
                    case -ENOSYS:
                        errorCode = U_ERROR_COMMON_NOT_SUPPORTED;
                        break;
                    case -ENOTSUP:
                    case -EINVAL:
                        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
                        break;
                    case -EBUSY:
                        errorCode = U_ERROR_COMMON_BUSY;
                        break;
                    case -EIO:
                        errorCode = U_ERROR_COMMON_NOT_RESPONDING;
                        break;
                    default:
                        break;
                }
            }
        }
#endif
    }

    return (int32_t) errorCode;
}

// Set the state of a GPIO.
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    const struct device *pPort;
    int zerr;

    pPort = pUPortPrivateGetGpioDevice(pin);
    if (pPort == NULL) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    zerr = gpio_pin_set_raw(pPort,
                            (gpio_pin_t) (pin % uPortPrivateGetGpioPortMaxPins()),
                            (int)level);
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

    pPort = pUPortPrivateGetGpioDevice(pin);
    if (pPort == NULL) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    zerr = gpio_port_get_raw(pPort, &val);
    if (zerr) {
        return (int32_t)U_ERROR_COMMON_DEVICE_ERROR;
    }

    return (val & (1 << (pin % uPortPrivateGetGpioPortMaxPins()))) ? 1 : 0;
}

// Interrupt support.
bool uPortGpioInterruptSupported()
{
#if !defined(CONFIG_ARCH_POSIX)
    return true;
#else
    return false;
#endif
}

// End of file
