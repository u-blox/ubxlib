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
 * @brief Implementation of the port I2C API for the Zephyr platform.
 */

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#else
#include <zephyr/types.h>
#include <kernel.h>
#include <device.h>
#include <drivers/i2c.h>
#endif

#include <soc.h>

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_cfg_os_platform_specific.h"
#include "u_port_i2c.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_I2C_MAX_NUM
/** The number of I2C HW blocks that are available; on NRF53 there
 * is only one, though it is called "I2C 1", while on NRF52 there
 * are two but the first one, I2C 0, is called "Arduino I2C" and
 * I don't _think_ the Zephyr drivers work with it because, under
 * the hood, they use the NRFx TWIM functions which require EasyDMA,
 * something which I2C (in Nordic speak "TWI") 0 doesn't have.
 * So, basically, use I2C HW block 1.
 *
 * STM32, on the other hand, has up to 4.
 */
# define U_PORT_I2C_MAX_NUM 4
#endif

#ifndef I2C_MODE_CONTROLLER
// MASTER deprecated in Zephyr 3.x
# define I2C_MODE_CONTROLLER I2C_MODE_MASTER
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per I2C interface.
 */
typedef struct {
    const struct device *pDevice;
    int32_t clockHertz;
    bool adopted;
    size_t maxSegmentSize;
} uPortI2cData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** I2C device data.
 */
static uPortI2cData_t gI2cData[U_PORT_I2C_MAX_NUM];

/** Table to convert clock speed into the Zephyr index value.
 */
static int32_t gClockHertzToIndex[] = {-1,
                                       100000,   /* 1: I2C_SPEED_STANDARD */
                                       400000,   /* 2: I2C_SPEED_FAST */
                                       1000000,  /* 3: I2C_SPEED_FAST_PLUS */
                                       3400000,  /* 4: I2C_SPEED_HIGH */
                                       5000000   /* 5: I2C_SPEED_ULTRA */
                                      };

/** Variable to keep track of the number of I2C interfaces open.
 */
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert clock rate in Hertz to the Zephyr index value.
static int32_t clockHertzToIndex(int32_t clockHertz)
{
    int32_t index = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (clockHertz >= 0) {
        for (size_t x = 0; (index < 0) &&
             (x < sizeof(gClockHertzToIndex) / sizeof(gClockHertzToIndex[0])); x++) {
            if (clockHertz == gClockHertzToIndex[x]) {
                index = x;
            }
        }
    }

    return index;
}

// Open an I2C instance; unlike the other static functions
// this does all the mutex locking etc.
static int32_t openI2c(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                       bool controller, bool adopt)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    uint32_t i2cDeviceCfg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // On Zephyr the pins are set at compile time so those passed
        // into here must be non-valid
        if ((i2c >= 0) && (i2c < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[i2c].pDevice == NULL) && controller &&
            (pinSda < 0) && (pinSdc < 0)) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            switch (i2c) {
#ifdef CONFIG_I2C
                case 0:
# if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("I2C_0");
# else
                    pDevice = U_DEVICE_DT_GET_OR_NULL(i2c0);
# endif
                    break;
                case 1:
# if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("I2C_1");
# else
                    pDevice = U_DEVICE_DT_GET_OR_NULL(i2c1);
# endif
                    break;
                case 2:
# if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("I2C_2");
# else
                    pDevice = U_DEVICE_DT_GET_OR_NULL(i2c2);
# endif
                    break;
                case 3:
# if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("I2C_3");
# else
                    pDevice = U_DEVICE_DT_GET_OR_NULL(i2c3);
# endif
                    break;
#endif // #ifdef CONFIG_I2C
                default:
                    break;
            }
            i2cDeviceCfg = I2C_SPEED_SET(clockHertzToIndex(U_PORT_I2C_CLOCK_FREQUENCY_HERTZ)) |
                           I2C_MODE_CONTROLLER;
            if ((pDevice != NULL) &&
                (!adopt || (i2c_configure(pDevice, i2cDeviceCfg) == 0))) {
                gI2cData[i2c].clockHertz = U_PORT_I2C_CLOCK_FREQUENCY_HERTZ;
                // Hook the device data structure into the entry
                // to flag that it is in use
                gI2cData[i2c].pDevice = pDevice;
                gI2cData[i2c].adopted = adopt;
                gI2cData[i2c].maxSegmentSize = 0;
                U_ATOMIC_INCREMENT(&gResourceAllocCount);
                // Return the I2C HW block number as the handle
                handleOrErrorCode = i2c;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise I2C handling.
int32_t uPortI2cInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {
            for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
                gI2cData[x].pDevice = NULL;
            }
        }
    }

    return errorCode;
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    if (gMutex != NULL) {
        // Zephyr doesn't have an I2C deinitialisation
        // API so nothing in particular to do here
        // aside from free the mutex
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open an I2C instance.
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller)
{
    return openI2c(i2c, pinSda, pinSdc, controller, false);
}

// Adopt an I2C instance.
int32_t uPortI2cAdopt(int32_t i2c, bool controller)
{
    return openI2c(i2c, -1, -1, controller, true);
}

// Close an I2C instance.
void uPortI2cClose(int32_t handle)
{
    if ((gMutex != NULL) && (handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Just set the device data structure to NULL to indicate that the device
        // is no longer in use
        gI2cData[handle].pDevice = NULL;
        U_ATOMIC_DECREMENT(&gResourceAllocCount);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Close an I2C instance and attempt to recover the I2C bus.
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    int32_t x;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                pDevice = gI2cData[handle].pDevice;
                gI2cData[handle].pDevice = NULL;
                U_ATOMIC_DECREMENT(&gResourceAllocCount);
                x = i2c_recover_bus(pDevice);
                if (x == -ENOSYS) {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                } else if (x == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Set the I2C clock frequency.
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    uint32_t i2cDeviceCfg;
    int32_t clockIndex = clockHertzToIndex(clockHertz);

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL) && (clockIndex >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                pDevice = gI2cData[handle].pDevice;
                i2cDeviceCfg = I2C_SPEED_SET(clockIndex) | I2C_MODE_CONTROLLER;
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (i2c_configure(pDevice, i2cDeviceCfg) == 0) {
                    gI2cData[handle].clockHertz = clockHertz;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the I2C clock frequency.
int32_t uPortI2cGetClock(int32_t handle)
{
    int32_t errorCodeOrClock = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrClock = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL)) {
            errorCodeOrClock = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                errorCodeOrClock = gI2cData[handle].clockHertz;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrClock;
}

// Set the timeout for I2C.
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs)
{
    // Can't set this at run-time on Zephyr
    (void) handle;
    (void) timeoutMs;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the timeout for I2C.
int32_t uPortI2cGetTimeout(int32_t handle)
{
    // Can't read this at run-time on Zephyr
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;;
}

// Set the maximum I2C segment size.
int32_t uPortI2cSetMaxSegmentSize(int32_t handle, size_t maxSegmentSize)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL)) {
            gI2cData[handle].maxSegmentSize = maxSegmentSize;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the maximum I2C segment size.
int32_t uPortI2cGetMaxSegmentSize(int32_t handle)
{
    int32_t errorCodeOrMaxSegmentSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrMaxSegmentSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL)) {
            errorCodeOrMaxSegmentSize = (int32_t) gI2cData[handle].maxSegmentSize;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrMaxSegmentSize;
}

// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerExchange(int32_t handle, uint16_t address,
                                   const char *pSend, size_t bytesToSend,
                                   char *pReceive, size_t bytesToReceive,
                                   bool noInterveningStop)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    struct i2c_msg message[2];
    size_t x;
    size_t thisReceiveBytes;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            pDevice = gI2cData[handle].pDevice;
            errorCodeOrLength = 0;
            // This is constructed as a do/while loop since
            // it is permissible to send zero length data,
            // e.g. when polling the bus for a device address
            do {
                memset(message, 0, sizeof(message));
                x = 0;
                if (bytesToSend > 0) {
                    message[x].buf = (uint8_t *) pSend;
                    message[x].len = (uint32_t) bytesToSend;
                    if ((gI2cData[handle].maxSegmentSize > 0) &&
                        (message[x].len > gI2cData[handle].maxSegmentSize)) {
                        message[x].len = gI2cData[handle].maxSegmentSize;
                    }
                    bytesToSend -= message[x].len;
                    pSend += message[x].len;
                    message[x].flags = I2C_MSG_WRITE;
                    if (address > 127) {
                        message[x].flags |= I2C_MSG_ADDR_10_BITS;
                    }
                    if ((pReceive == NULL) && !noInterveningStop) {
                        // If there's nothing to receive and we are not-not
                        // going to insert a stop bit then do that
                        message[x].flags |= I2C_MSG_STOP;
                    }
                    x++;
                }
                thisReceiveBytes = 0;
                if (bytesToReceive > 0) {
                    message[x].buf = (uint8_t *) pReceive;
                    message[x].len = (uint32_t) bytesToReceive;
                    if ((gI2cData[handle].maxSegmentSize > 0) &&
                        (message[x].len > gI2cData[handle].maxSegmentSize)) {
                        message[x].len = gI2cData[handle].maxSegmentSize;
                    }
                    bytesToReceive -= message[x].len;
                    pReceive += message[x].len;
                    thisReceiveBytes = message[x].len;
                    // We're definitely stopping after this message
                    message[x].flags = I2C_MSG_READ | I2C_MSG_STOP;
                    if (address > 127) {
                        message[x].flags |= I2C_MSG_ADDR_10_BITS;
                    }
                    // If something was sent, make sure that there is
                    // a start marker at the front of the message
                    if (pSend != NULL) {
                        message[x].flags |= I2C_MSG_RESTART;
                    }
                    x++;
                }
                if (i2c_transfer(pDevice, message, (uint8_t) x, address) == 0) {
                    errorCodeOrLength += (int32_t) thisReceiveBytes;
                } else {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                }
            } while ((errorCodeOrLength >= 0) &&
                     ((bytesToSend > 0) || (bytesToReceive > 0)));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

/** \deprecated please use uPortI2cControllerExchange() instead. */
// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    struct i2c_msg message[2];
    size_t x;
    size_t thisReceiveBytes;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            pDevice = gI2cData[handle].pDevice;
            errorCodeOrLength = 0;
            while ((errorCodeOrLength >= 0) &&
                   ((bytesToSend > 0) || (bytesToReceive > 0))) {
                memset(message, 0, sizeof(message));
                x = 0;
                if (bytesToSend > 0) {
                    message[x].buf = (uint8_t *) pSend;
                    message[x].len = (uint32_t) bytesToSend;
                    if ((gI2cData[handle].maxSegmentSize > 0) &&
                        (message[x].len > gI2cData[handle].maxSegmentSize)) {
                        message[x].len = gI2cData[handle].maxSegmentSize;
                    }
                    bytesToSend -= message[x].len;
                    pSend += message[x].len;
                    message[x].flags = I2C_MSG_WRITE;
                    if (address > 127) {
                        message[x].flags |= I2C_MSG_ADDR_10_BITS;
                    }
                    if (pReceive == NULL) {
                        // If there's nothing to receive then add a stop marker
                        // at the end of the message
                        message[x].flags |= I2C_MSG_STOP;
                    }
                    x++;
                }
                thisReceiveBytes = 0;
                if (bytesToReceive > 0) {
                    message[x].buf = (uint8_t *) pReceive;
                    message[x].len = (uint32_t) bytesToReceive;
                    if ((gI2cData[handle].maxSegmentSize > 0) &&
                        (message[x].len > gI2cData[handle].maxSegmentSize)) {
                        message[x].len = gI2cData[handle].maxSegmentSize;
                    }
                    bytesToReceive -= message[x].len;
                    pReceive += message[x].len;
                    thisReceiveBytes = message[x].len;
                    // We're definitely stopping after this message
                    message[x].flags = I2C_MSG_READ | I2C_MSG_STOP;
                    if (address > 127) {
                        message[x].flags |= I2C_MSG_ADDR_10_BITS;
                    }
                    // If something was sent, make sure that there is
                    // a start marker at the front of the message
                    if (pSend != NULL) {
                        message[x].flags |= I2C_MSG_RESTART;
                    }
                    x++;
                }
                if (i2c_transfer(pDevice, message, (uint8_t) x, address) == 0) {
                    errorCodeOrLength += (int32_t) thisReceiveBytes;
                } else {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

/** \deprecated please use uPortI2cControllerExchange() instead. */
// Perform a send over the I2C interface as a controller.
//
// IMPORTANT: if this function is called with noStop set to true, that
// will work for nRF52/nRF53 but it will NOT work for STM32 and may
// not work on other chipsets underneath Zephyr.  This is because
// leaving off I2C_MSG_STOP is not guaranteed to work in all cases:
// zome Zephry drivers insist that an I2C transaction ends with a
// stop bit.  uPortI2cControllerExchange() should be used instead
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    struct i2c_msg message;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pDevice != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0))) {
            pDevice = gI2cData[handle].pDevice;
            // This is constructed as a do/while loop since
            // it is permissible to send zero length data,
            // e.g. when polling the bus for a device address
            do {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                message.buf = (uint8_t *) pSend;
                message.len = (uint32_t) bytesToSend;
                if ((gI2cData[handle].maxSegmentSize > 0) &&
                    (message.len > gI2cData[handle].maxSegmentSize)) {
                    message.len = gI2cData[handle].maxSegmentSize;
                }
                bytesToSend -= message.len;
                pSend += message.len;
                message.flags = I2C_MSG_WRITE;
                if (address > 127) {
                    message.flags |= I2C_MSG_ADDR_10_BITS;
                }
                if (!noStop) {
                    message.flags |= I2C_MSG_STOP;
                }
                if (i2c_transfer(pDevice, &message, 1, address) == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            } while ((errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) && (bytesToSend > 0));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the number of I2C interfaces currently open.
int32_t uPortI2cResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
