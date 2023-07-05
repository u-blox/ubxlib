/*
 * Copyright 2019-2023 u-blox
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
 * @brief Implementation of the port GPIO API on Linux.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "gpiod.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_gpio.h"
#include "u_port_private.h"
#include "u_compiler.h"

/* This implementation uses the gpiod library from the Linux kernel,
   hence libgpiod-dev must be installed.
*/


/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_GPIO_CHIP_NAME
# define U_PORT_GPIO_CHIP_NAME "gpiochip0"
#endif

#ifndef U_PORT_GPIO_CONSUMER_NAME
# define U_PORT_GPIO_CONSUMER_NAME "ubxlib"
#endif

// These are missing in version < 1.5 of gpiod

#define _GPIOD_LINE_REQUEST_FLAG_BIAS_DISABLE GPIOD_BIT(3)
#define _GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN GPIOD_BIT(4)
#define _GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP GPIOD_BIT(5)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static struct gpiod_chip *gpGpioChip = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static struct gpiod_line *getChip_line(int32_t pin)
{
    struct gpiod_line *pLine = NULL;
    if (gpGpioChip == NULL) {
        gpGpioChip = gpiod_chip_open_by_name(U_PORT_GPIO_CHIP_NAME);
    }
    if (gpGpioChip != NULL) {
        pLine = gpiod_chip_get_line(gpGpioChip, (unsigned int)pin);
    }
    return pLine;
}

/* Release pin if it has been taken before. */
static void releaseIfAllocated(struct gpiod_line *pLine)
{
    if (gpiod_line_consumer(pLine) != NULL) {
        gpiod_line_release(pLine);
    }
}

/* Check if a pin has already been configured as output. */
static bool isOutput(struct gpiod_line *pLine)
{
    return ((gpiod_line_consumer(pLine) != NULL) &&
            (gpiod_line_direction(pLine) == GPIOD_LINE_DIRECTION_OUTPUT));
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure a GPIO.
U_WEAK int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    struct gpiod_line *pLine = getChip_line(pConfig->pin);
    if (pLine != NULL) {
        int flags = 0;
        if ((pConfig->direction == U_PORT_GPIO_DIRECTION_OUTPUT) ||
            (pConfig->direction == U_PORT_GPIO_DIRECTION_INPUT_OUTPUT)) {
            // There is no difference between U_PORT_GPIO_DIRECTION_OUTPUT and
            // U_PORT_GPIO_DIRECTION_INPUT_OUTPUT as we can always read
            // the current output value.
            if (pConfig->driveMode == U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN) {
                flags = GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN;
            }
            // gpiod_line_request_output_flags requires an initial level value.
            // If the pin was not configured earlier as an output we can not obtain
            // it and hence we set it to 0.
            int level = gpiod_line_get_value(pLine);
            if (level < 0) {
                level = 0;
            }
            releaseIfAllocated(pLine);
            if (gpiod_line_request_output_flags(pLine, U_PORT_GPIO_CONSUMER_NAME, flags, level) == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        } else if (pConfig->direction == U_PORT_GPIO_DIRECTION_INPUT) {
            if (pConfig->pullMode == U_PORT_GPIO_PULL_MODE_PULL_UP) {
                flags = _GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
            } else if (pConfig->pullMode == U_PORT_GPIO_PULL_MODE_PULL_DOWN) {
                flags = _GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN;
            } else if (pConfig->pullMode == U_PORT_GPIO_PULL_MODE_NONE) {
                flags = _GPIOD_LINE_REQUEST_FLAG_BIAS_DISABLE;
            }
            releaseIfAllocated(pLine);
            if (gpiod_line_request_input_flags(pLine, U_PORT_GPIO_CONSUMER_NAME, flags) == 0) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }
    return (int32_t)errorCode;
}

// Set the state of a GPIO.
U_WEAK int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_PLATFORM;
    struct gpiod_line *pLine = getChip_line(pin);
    if (pLine != NULL) {
        // The pin may not yet have been defined as output via uPortGpioConfig.
        int res;
        if (isOutput(pLine)) {
            res = gpiod_line_set_value(pLine, level);
        } else {
            releaseIfAllocated(pLine);
            res = gpiod_line_request_output(pLine, U_PORT_GPIO_CONSUMER_NAME, level);
        }
        if (res == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t)errorCode;
}

// Get the state of a GPIO.
U_WEAK int32_t uPortGpioGet(int32_t pin)
{
    int32_t level = (int32_t)U_ERROR_COMMON_PLATFORM;
    struct gpiod_line *pLine = getChip_line(pin);
    if (pLine != NULL) {
        level = gpiod_line_get_value(pLine);
    }
    return level;
}

// End of file
