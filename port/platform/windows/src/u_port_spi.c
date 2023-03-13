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
 * @brief Implementation of the port SPI API for the Windows platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_error_common.h"
#include "u_port_spi.h"

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

// Initialise SPI handling.
int32_t uPortSpiInit()
{
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Shutdown SPI handling.
void uPortSpiDeinit()
{
    // Not supported.
}

// Open an SPI instance.
int32_t uPortSpiOpen(int32_t spi, int32_t pinMosi, int32_t pinMiso,
                     int32_t pinClk, bool controller)
{
    (void) spi;
    (void) pinMosi;
    (void) pinMiso;
    (void) pinClk;
    (void) controller;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Close an SPI instance.
void uPortSpiClose(int32_t handle)
{
    (void) handle;
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice)
{
    (void) handle;
    (void) pDevice;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the configuration of the device.
int32_t uPortSpiControllerGetDevice(int32_t handle,
                                    uCommonSpiControllerDevice_t *pDevice)
{
    (void) handle;
    (void) pDevice;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Exchange a single word with an SPI device.
uint64_t uPortSpiControllerSendReceiveWord(int32_t handle, uint64_t value,
                                           size_t bytesToSendAndReceive)
{
    (void) handle;
    (void) value;
    (void) bytesToSendAndReceive;

    return 0;
}

// Exchange a block of data with an SPI device.
int32_t uPortSpiControllerSendReceiveBlock(int32_t handle, const char *pSend,
                                           size_t bytesToSend, char *pReceive,
                                           size_t bytesToReceive)
{
    (void) handle;
    (void) pSend;
    (void) bytesToSend;
    (void) pReceive;
    (void) bytesToReceive;

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
