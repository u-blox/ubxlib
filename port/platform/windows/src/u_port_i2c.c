/*
 * Copyright 2019-2022 u-blox Ltd
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
 * @brief Implementation of the port I2C API for the Windows platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_error_common.h"
#include "u_port_i2c.h"

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

// Initialise I2C handling.
int32_t uPortI2cInit()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    // Not supported.
}

// Open an I2C instance.
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller)
{
    (void) i2c;
    (void) pinSda;
    (void) pinSdc;
    (void) controller;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Adopt an I2C instance.
int32_t uPortI2cAdopt(int32_t i2c, bool controller)
{
    (void) i2c;
    (void) controller;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Close an I2C instance.
void uPortI2cClose(int32_t handle)
{
    (void) handle;
}

// Close an I2C instance and attempt to recover the I2C bus.
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    (void) handle;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Set the I2C clock frequency.
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz)
{
    (void) handle;
    (void) clockHertz;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the I2C clock frequency.
int32_t uPortI2cGetClock(int32_t handle)
{
    (void) handle;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Set the timeout for I2C.
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs)
{
    (void) handle;
    (void) timeoutMs;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the timeout for I2C.
int32_t uPortI2cGetTimeout(int32_t handle)
{
    (void) handle;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    (void) handle;
    (void) address;
    (void) pSend;
    (void) bytesToSend;
    (void) pReceive;
    (void) bytesToReceive;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Perform a send over the I2C interface as a controller.
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    (void) handle;
    (void) address;
    (void) pSend;
    (void) bytesToSend;
    (void) noStop;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
