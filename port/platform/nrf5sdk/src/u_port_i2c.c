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
 * @brief Implementation of the port I2C API for the NRF52 platform.
 *
 * Note: unlike with the NRF52 UART API, here we use the Nordic nrfx
 * layer and hence, to use an I2C HW block, it must be *enabled* in your
 * sdk_config.h file.  So, to use instance 0, NRFX_TWIM0_ENABLED must be
 * set to 1 in your sdk_config.h file, to use instance 1 NRFX_TWIM1_ENABLED
 * must be set to 1 in your sdk_config.h file.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"  // memset()

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_i2c.h"
#include "u_port_private.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "nrf.h"
#include "nrf_gpio.h"  // For bus recovery
#include "nrfx_twim.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_I2C_MAX_NUM
/** The number of I2C HW blocks that are available; on NRF52 there
 * are two but the first one, TWI 0, I don't think has EasyDMA and
 * hence I _think_ the NRFx TWIM functions won't work with it.  So
 * I suggest you only use I2C HW block 1.
 */
# define U_PORT_I2C_MAX_NUM 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per I2C interface.
 */
typedef struct {
    nrfx_twim_t instance;
    int32_t clockHertz;
    int32_t timeoutMs;
    int32_t pinSda; // Need to remember these in order to perform
    int32_t pinSdc; // bus recovery
    volatile int32_t xferErrorCode;
    uPortSemaphoreHandle_t completionSemaphore;
    bool adopted;
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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert clock rate in Hertz to the NRF5 SDK enum value.
static int32_t clockHertzToEnum(int32_t clockHertz)
{
    int32_t enumValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    switch (clockHertz) {
        case 100000:
            enumValue = (int32_t) NRF_TWIM_FREQ_100K;
            break;
        case 250000:
            enumValue = (int32_t) NRF_TWIM_FREQ_250K;
            break;
        case 400000:
            enumValue = (int32_t) NRF_TWIM_FREQ_400K;
            break;
        default:
            break;
    }

    return enumValue;
}

// Close an I2C instance.
static void closeI2c(uPortI2cData_t *pI2c)
{
    if ((pI2c != NULL) && (pI2c->instance.p_twim != NULL)) {
        if (!pI2c->adopted) {
            nrfx_twim_uninit(&pI2c->instance);
        }
        uPortSemaphoreDelete(pI2c->completionSemaphore);
        // Zero the instance to indicate that it is no longer in use
        memset(&pI2c->instance, 0, sizeof(pI2c->instance));
    }
}

// Event handler.
// Note: will be called from interrupt context
static void eventHandlerIrq(nrfx_twim_evt_t const *pEvent, void *pContext)
{
    uPortI2cData_t *pI2cData = (uPortI2cData_t *) pContext;

    switch (pEvent->type) {
        case NRFX_TWIM_EVT_DONE:
            pI2cData->xferErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case NRFX_TWIM_EVT_ADDRESS_NACK:
            pI2cData->xferErrorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
            break;
        case NRFX_TWIM_EVT_DATA_NACK:
            pI2cData->xferErrorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            break;
        default:
            break;
    }

    uPortSemaphoreGiveIrq(pI2cData->completionSemaphore);
}

// Attempt to unblock the I2C bus.  This function appears as
// nrfx_twi_twim_bus_recover() in version 17 of the NRFSDK;
// re-implementing it in here so as not to have to move
// forward a version just yet since  Nordic tend to make
// breaking changes.
static int32_t busRecover(int32_t pinSda, int32_t pinSdc)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    nrf_gpio_pin_set(pinSda);
    nrf_gpio_pin_set(pinSdc);

    nrf_gpio_cfg(pinSda,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);
    nrf_gpio_cfg(pinSdc,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);

    NRFX_DELAY_US(4);

    for (size_t x = 0; (x < 9) && !nrf_gpio_pin_read(pinSda); x++) {
        // Pulse CLOCK signal
        nrf_gpio_pin_clear(pinSdc);
        NRFX_DELAY_US(4);
        nrf_gpio_pin_set(pinSdc);
        NRFX_DELAY_US(4);
    }

    // Generate a STOP condition on the bus
    nrf_gpio_pin_clear(pinSda);
    NRFX_DELAY_US(4);
    nrf_gpio_pin_set(pinSda);
    NRFX_DELAY_US(4);

    if (nrf_gpio_pin_read(pinSda)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Open an I2C instance; unlike the other static functions
// this does all the mutex locking etc.
static int32_t openI2c(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                       bool controller, bool adopt)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    nrfx_twim_t instance = {0};
    nrfx_twim_config_t cfg = NRFX_TWIM_DEFAULT_CONFIG;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((i2c >= 0) && (i2c < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[i2c].instance.p_twim == NULL) && controller &&
            (adopt || ((pinSda >= 0) && (pinSdc >= 0)))) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            switch (i2c) {
                case 0:
#if NRFX_TWIM0_ENABLED
                    instance.p_twim       = NRF_TWIM0;
                    instance.drv_inst_idx = NRFX_TWIM0_INST_IDX;
#endif
                    break;
                case 1:
#if NRFX_TWIM1_ENABLED
                    instance.p_twim       = NRF_TWIM1;
                    instance.drv_inst_idx = NRFX_TWIM1_INST_IDX;
#endif
                    break;
                default:
                    break;
            }
            if (instance.p_twim != NULL) {
                handleOrErrorCode = uPortSemaphoreCreate(&(gI2cData[i2c].completionSemaphore), 0, 1);
                if (handleOrErrorCode == 0) {
                    cfg.scl = pinSdc;
                    cfg.sda = pinSda;
                    cfg.frequency = clockHertzToEnum(U_PORT_I2C_CLOCK_FREQUENCY_HERTZ);
                    if (adopt ||
                        (nrfx_twim_init(&instance, &cfg, eventHandlerIrq, &(gI2cData[i2c])) == NRFX_SUCCESS)) {
                        gI2cData[i2c].clockHertz = U_PORT_I2C_CLOCK_FREQUENCY_HERTZ;
                        gI2cData[i2c].timeoutMs = U_PORT_I2C_TIMEOUT_MILLISECONDS;
                        gI2cData[i2c].pinSda = pinSda;
                        gI2cData[i2c].pinSdc = pinSdc;
                        gI2cData[i2c].adopted = adopt;
                        gI2cData[i2c].instance = instance;
                        // Return the I2C HW block number as the handle
                        handleOrErrorCode = i2c;
                    }
                }
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
#if NRFX_TWIM0_ENABLED || NRFX_TWIM1_ENABLED
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {
            for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
                memset(&gI2cData[x].instance, 0, sizeof(gI2cData[x].instance));
                gI2cData[x].pinSda = -1;
                gI2cData[x].pinSdc = -1;
                gI2cData[x].completionSemaphore = NULL;
            }
        }
    }

    return errorCode;
#else
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#endif
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Shut down any open instances
        for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
            closeI2c(&(gI2cData[x]));
        }

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
    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gI2cData) / sizeof(gI2cData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        closeI2c(&(gI2cData[handle]));

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Close an I2C instance and attempt to recover the I2C bus.
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t pinSda;
    int32_t pinSdc;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                pinSda = gI2cData[handle].pinSda;
                pinSdc = gI2cData[handle].pinSdc;
                closeI2c(&(gI2cData[handle]));
                errorCode = busRecover(pinSda, pinSdc);
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
    int32_t clockEnumValue = clockHertzToEnum(clockHertz);

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL) && (clockEnumValue >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                nrf_twim_frequency_set(gI2cData[handle].instance.p_twim, clockEnumValue);
                gI2cData[handle].clockHertz = clockHertz;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
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
            (gI2cData[handle].instance.p_twim != NULL)) {
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL) && (timeoutMs > 0)) {
            gI2cData[handle].timeoutMs = timeoutMs;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the timeout for I2C.
int32_t uPortI2cGetTimeout(int32_t handle)
{
    int32_t errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL)) {
            errorCodeOrTimeout = gI2cData[handle].timeoutMs;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrTimeout;
}

// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    nrfx_twim_xfer_desc_t xferDescription = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
            nrfx_twim_enable(&(gI2cData[handle].instance));
            if (pSend != NULL) {
                xferDescription.type = NRFX_TWIM_XFER_TX;
                xferDescription.address = (uint8_t) address;
                xferDescription.primary_length = bytesToSend;
                xferDescription.p_primary_buf = (uint8_t *) pSend;
                // Make sure the semaphore is empty
                uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore, 0);
                gI2cData[handle].xferErrorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                if (nrfx_twim_xfer(&(gI2cData[handle].instance),
                                   &xferDescription, 0) == NRFX_SUCCESS) {
                    // Wait for the event handler to give the semaphore
                    errorCodeOrLength = uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore,
                                                              gI2cData[handle].timeoutMs * bytesToSend);
                    if (errorCodeOrLength == 0) {
                        errorCodeOrLength = gI2cData[handle].xferErrorCode;
                    }
                }
            }
            if ((errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) &&
                (pReceive != NULL)) {
                xferDescription.type = NRFX_TWIM_XFER_RX;
                xferDescription.address = (uint8_t) address;
                xferDescription.primary_length = bytesToReceive;
                xferDescription.p_primary_buf = (uint8_t *) pReceive;
                // Make sure the semaphore is empty
                uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore, 0);
                gI2cData[handle].xferErrorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                if (nrfx_twim_xfer(&(gI2cData[handle].instance),
                                   &xferDescription, 0) == NRFX_SUCCESS) {
                    // Wait for the event handler to give the semaphore
                    errorCodeOrLength = uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore,
                                                              gI2cData[handle].timeoutMs * bytesToReceive);
                    if (errorCodeOrLength == 0) {
                        errorCodeOrLength = gI2cData[handle].xferErrorCode;
                        if (errorCodeOrLength == 0) {
                            errorCodeOrLength = (int32_t) bytesToReceive;
                        }
                    }
                }
            }
            nrfx_twim_disable(&(gI2cData[handle].instance));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

// Perform a send over the I2C interface as a controller.
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    nrfx_twim_xfer_desc_t xferDescription = {0};
    uint32_t flags = 0;
    char emptyBuffer = 0;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].instance.p_twim != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0))) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            nrfx_twim_enable(&(gI2cData[handle].instance));
            xferDescription.type = NRFX_TWIM_XFER_TX;
            xferDescription.address = (uint8_t) address;
            xferDescription.primary_length = bytesToSend;
            if (pSend == NULL) {
                // If pSend is NULL this fails the NRFSDK check that
                // the buffer is in RAM, so assign an empty buffer to
                // it here, then the user can still do a "scan" for addresses
                // present on the bus using a NULL buffer
                pSend = &emptyBuffer;
                // Also, from this Nordic support question:
                // https://devzone.nordicsemi.com/f/nordic-q-a/37665/twim-clock-pin-is-pull-low-after-sending-zero-bytes-data
                // it is clear that the nrfx_twim_xfer() function does not support
                // sending zero bytes of data: the STOP signal will never be sent
                // as it is shorted to the LAST_TX event and with no TX that will
                // never happen.
            }
            xferDescription.p_primary_buf = (uint8_t *) pSend;
            if (noStop) {
                flags |= NRFX_TWIM_FLAG_TX_NO_STOP;
            }
            // Make sure the semaphore is empty
            uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore, 0);
            gI2cData[handle].xferErrorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
            if (nrfx_twim_xfer(&(gI2cData[handle].instance),
                               &xferDescription, flags) == NRFX_SUCCESS) {
                // Wait for the event handler to give the semaphore,
                // +1 below to make sure we at least wait a little while,
                // since bytesToSend might be zero
                errorCode = uPortSemaphoreTryTake(gI2cData[handle].completionSemaphore,
                                                  gI2cData[handle].timeoutMs * (bytesToSend + 1));
                if (errorCode == 0) {
                    errorCode = gI2cData[handle].xferErrorCode;
                }
            }
            nrfx_twim_disable(&(gI2cData[handle].instance));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// End of file
