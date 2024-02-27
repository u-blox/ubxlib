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
 * @brief Default implementations of I2C functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h"  // U_WEAK

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

//  Default implementation of the I2C data exchange function.
U_WEAK int32_t uPortI2cControllerExchange(int32_t handle, uint16_t address,
                                          const char *pSend, size_t bytesToSend,
                                          char *pReceive, size_t bytesToReceive,
                                          bool noInterveningStop)
{
    int32_t errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (noInterveningStop) {
        errorCodeOrReceiveSize = uPortI2cControllerSend(handle, address,
                                                        pSend, bytesToSend,
                                                        noInterveningStop);
        pSend = NULL;
        bytesToSend = 0;
    }

    if (errorCodeOrReceiveSize == 0) {
        errorCodeOrReceiveSize = uPortI2cControllerSendReceive(handle, address,
                                                               pSend, bytesToSend,
                                                               pReceive, bytesToReceive);
    }

    return errorCodeOrReceiveSize;
}

// Default implmentation of setting maximum I2C segment size.
U_WEAK int32_t uPortI2cSetMaxSegmentSize(int32_t handle, size_t maxSegmentSize)
{
    (void) handle;
    (void) maxSegmentSize;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Default implmentation of getting maximum I2C segment size.
U_WEAK int32_t uPortI2cGetMaxSegmentSize(int32_t handle)
{
    (void) handle;
    return 0;
}

// End of file
