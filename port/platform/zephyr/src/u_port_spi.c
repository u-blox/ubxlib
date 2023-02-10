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
 * @brief Implementation of the port SPI API for the Zephyr platform.
 */

#include <zephyr/types.h>


#include <zephyr/zephyr.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/device.h>

#include <soc.h>

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_spi.h"
#include "u_port_private.h"
#include "version.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_SPI_MAX_NUM
/** The number of SPI HW blocks that are available; on NRF52 there
 * can be up to four SPI controllers while on NRF53 there are up
 * to five.
 */
# define U_PORT_SPI_MAX_NUM 5
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per SPI interface.
 */
typedef struct {
    const struct device *pDevice;  // NULL if not in use
    struct spi_config spiConfig;
    struct spi_cs_control spiCsControl;
    int32_t pinSelect; // Gotta keep this also as the one under
    // spiCsControl gets modulo'd by port number
} uPortSpiCfg_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** SPI configuration data.
 */
static uPortSpiCfg_t gSpiCfg[U_PORT_SPI_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the SPI configuration in the given SPI instance
static int32_t setSpiConfig(uPortSpiCfg_t *pSpiCfg,
                            const uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t offsetDuration;
    uint16_t operation = SPI_OP_MODE_MASTER;
    int32_t pinSelect;
    bool pinSelectInverted = ((pDevice->pinSelect & U_COMMON_SPI_PIN_SELECT_INVERTED) ==
                              U_COMMON_SPI_PIN_SELECT_INVERTED);

    if ((pDevice->mode & U_COMMON_SPI_MODE_CPOL_BIT_MASK) == U_COMMON_SPI_MODE_CPOL_BIT_MASK) {
        operation |= SPI_MODE_CPOL;
    }
    if ((pDevice->mode & U_COMMON_SPI_MODE_CPHA_BIT_MASK) == U_COMMON_SPI_MODE_CPHA_BIT_MASK) {
        operation |= SPI_MODE_CPHA;
    }
    // The word-size in the operation thingy is in bits
    operation |= SPI_WORD_SET(pDevice->wordSizeBytes * 8);
    if (pDevice->lsbFirst) {
        operation |= SPI_TRANSFER_LSB;
    }
    // Note that SPI_CS_ACTIVE_HIGH is not fiddled with: this one is
    // via the GPIO line configuration instead
    pSpiCfg->spiConfig.operation = operation;
    pSpiCfg->spiConfig.frequency = pDevice->frequencyHertz;

    pSpiCfg->spiConfig.cs = NULL;
#if KERNEL_VERSION_MAJOR < 3
    pSpiCfg->spiCsControl.gpio_dev = NULL;
#else
    pSpiCfg->spiCsControl.gpio.port = NULL;
#endif
    pSpiCfg->pinSelect = pDevice->pinSelect;
    if (pSpiCfg->pinSelect >= 0) {
        pinSelect = pSpiCfg->pinSelect & ~U_COMMON_SPI_PIN_SELECT_INVERTED;
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
#if KERNEL_VERSION_MAJOR < 3
        pSpiCfg->spiCsControl.gpio_dev = pUPortPrivateGetGpioDevice(pinSelect);
        if (pSpiCfg->spiCsControl.gpio_dev != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pSpiCfg->spiCsControl.gpio_pin = pinSelect % GPIO_MAX_PINS_PER_PORT;
            if (!pinSelectInverted) {
                pSpiCfg->spiCsControl.gpio_dt_flags = GPIO_ACTIVE_LOW;
            }
            pSpiCfg->spiConfig.cs = &pSpiCfg->spiCsControl;
        }
#else
        pSpiCfg->spiCsControl.gpio.port = pUPortPrivateGetGpioDevice(pinSelect);
        if (pSpiCfg->spiCsControl.gpio.port != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pSpiCfg->spiCsControl.gpio.pin = pinSelect % GPIO_MAX_PINS_PER_PORT;
            if (!pinSelectInverted) {
                pSpiCfg->spiCsControl.gpio.dt_flags = GPIO_ACTIVE_LOW;
            }
        }
#endif
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Separate stop and start offsets are not supported, just a single
            // "delay" value that serves for both
            offsetDuration = pDevice->startOffsetNanoseconds;
            if (pDevice->stopOffsetNanoseconds > offsetDuration) {
                offsetDuration = pDevice->stopOffsetNanoseconds;
            }
            pSpiCfg->spiCsControl.delay = offsetDuration / 1000;
            pSpiCfg->spiConfig.cs = &pSpiCfg->spiCsControl;
        }
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise SPI handling.
int32_t uPortSpiInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {
            for (size_t x = 0; x < sizeof(gSpiCfg) / sizeof(gSpiCfg[0]); x++) {
                gSpiCfg[x].pDevice = NULL;
            }
        }
    }

    return errorCode;
}

// Shutdown SPI handling.
void uPortSpiDeinit()
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open an SPI instance.
int32_t uPortSpiOpen(int32_t spi, int32_t pinMosi, int32_t pinMiso,
                     int32_t pinClk, bool controller)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice = NULL;
    uCommonSpiControllerDevice_t device = U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS(-1);

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // On Zephyr the pins are set at compile time so those passed
        // into here must be non-valid
        if ((spi >= 0) && (spi < sizeof(gSpiCfg) / sizeof(gSpiCfg[0])) &&
            (gSpiCfg[spi].pDevice == NULL) && controller &&
            (pinMosi < 0) && (pinMiso < 0) && (pinClk < 0)) {
            switch (spi) {
                case 0:
#if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("SPI_0");
#else
                    pDevice = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(spi0));
#endif
                    break;
                case 1:
#if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("SPI_1");
#else
                    pDevice = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(spi1));
#endif
                    break;
                case 2:
#if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("SPI_2");
#else
                    pDevice = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(spi2));
#endif
                    break;
                case 3:
#if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("SPI_3");
#else
                    pDevice = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(spi3));
#endif
                    break;
                case 4:
#if KERNEL_VERSION_MAJOR < 3
                    pDevice = device_get_binding("SPI_4");
#else
                    pDevice = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(spi4));
#endif
                    break;
                default:
                    break;
            }

            if (pDevice != NULL) {
                handleOrErrorCode = setSpiConfig(&(gSpiCfg[spi]), &device);
                if (handleOrErrorCode == 0) {
                    // Hook the device data structure into the entry
                    // to flag that it is in use
                    gSpiCfg[spi].pDevice = pDevice;
                    // Return the SPI HW block number as the handle
                    handleOrErrorCode = spi;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

// Close an SPI instance.
void uPortSpiClose(int32_t handle)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) && (handle < sizeof(gSpiCfg) / sizeof(gSpiCfg[0]))) {
            // Just set the device data structure to NULL to indicate that the device
            // is no longer in use
            gSpiCfg[handle].pDevice = NULL;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiCfg) / sizeof(gSpiCfg[0])) &&
            (gSpiCfg[handle].pDevice != NULL) && (pDevice != NULL)) {
            errorCode = setSpiConfig(&(gSpiCfg[handle]), pDevice);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the configuration of the device.
int32_t uPortSpiControllerGetDevice(int32_t handle,
                                    uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortSpiCfg_t *pSpiCfg;
    uint16_t operation;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiCfg) / sizeof(gSpiCfg[0])) &&
            (gSpiCfg[handle].pDevice != NULL) && (pDevice != NULL)) {
            pSpiCfg = &(gSpiCfg[handle]);
            pDevice->pinSelect = -1;
            pDevice->startOffsetNanoseconds = 0;
#if KERNEL_VERSION_MAJOR < 3
            if (pSpiCfg->spiCsControl.gpio_dev != NULL) {
                pDevice->pinSelect = pSpiCfg->pinSelect;
                if ((pSpiCfg->spiCsControl.gpio_dt_flags & GPIO_ACTIVE_LOW) != GPIO_ACTIVE_LOW) {
                    pDevice->pinSelect |= U_COMMON_SPI_PIN_SELECT_INVERTED;
                }
                pDevice->startOffsetNanoseconds = pSpiCfg->spiCsControl.delay * 1000;
            }
#else
            if (pSpiCfg->spiCsControl.gpio.port != NULL) {
                pDevice->pinSelect = pSpiCfg->pinSelect;
                if ((pSpiCfg->spiCsControl.gpio.dt_flags & GPIO_ACTIVE_LOW) != GPIO_ACTIVE_LOW) {
                    pDevice->pinSelect |= U_COMMON_SPI_PIN_SELECT_INVERTED;
                }
                pDevice->startOffsetNanoseconds = pSpiCfg->spiCsControl.delay * 1000;
            }
#endif
            pDevice->stopOffsetNanoseconds = pDevice->startOffsetNanoseconds;
            pDevice->sampleDelayNanoseconds = 0; // Not an option in Zephyr
            pDevice->frequencyHertz = pSpiCfg->spiConfig.frequency;
            pDevice->mode = 0;
            operation = pSpiCfg->spiConfig.operation;
            if ((operation & SPI_MODE_CPOL) == SPI_MODE_CPOL) {
                pDevice->mode |= U_COMMON_SPI_MODE_CPOL_BIT_MASK;
            }
            if ((operation & SPI_MODE_CPHA) == SPI_MODE_CPHA) {
                pDevice->mode |= U_COMMON_SPI_MODE_CPHA_BIT_MASK;
            }
            pDevice->lsbFirst = ((operation & SPI_TRANSFER_LSB) == SPI_TRANSFER_LSB);
            // The word size in the operation field is in bits
            // do this last as it affects the contents of operation
            pDevice->wordSizeBytes = SPI_WORD_SIZE_GET(operation) / 8;
            pDevice->fillWord = 0xFF; // Not an option in Zephyr
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Exchange a single word with an SPI device.
uint64_t uPortSpiControllerSendReceiveWord(int32_t handle, uint64_t value,
                                           size_t bytesToSendAndReceive)
{
    uint64_t valueReceived = 0;
    uPortSpiCfg_t *pSpiCfg;
    struct spi_buf sendBuffer;
    struct spi_buf_set sendBufferList;
    struct spi_buf receiveBuffer;
    struct spi_buf_set receiveBufferList;
    uint16_t operation;
    bool reverseBytes = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) && (handle < sizeof(gSpiCfg) / sizeof(gSpiCfg[0])) &&
            (gSpiCfg[handle].pDevice != NULL) &&
            (bytesToSendAndReceive <= sizeof(value))) {
            pSpiCfg = &(gSpiCfg[handle]);
            operation = pSpiCfg->spiConfig.operation;
            // Need to perform byte reversal if the length of the word we
            // are sending is greater than one byte, there is a mismatch between
            // the endianness of this processor and the endianness of
            // bit-transmission, and it will only work if the word size is set
            // to eight bits
            reverseBytes = ((bytesToSendAndReceive > 1) &&
                            (((operation & SPI_TRANSFER_LSB) == SPI_TRANSFER_LSB) !=
                             U_PORT_IS_LITTLE_ENDIAN) &&
                            (SPI_WORD_SIZE_GET(operation) == 8));

            if (reverseBytes) {
                U_PORT_BYTE_REVERSE(value, bytesToSendAndReceive);
            }

            sendBuffer.buf = &value;
            sendBuffer.len = bytesToSendAndReceive;
            sendBufferList.buffers = &sendBuffer;
            sendBufferList.count = 1;

            receiveBuffer.buf = &valueReceived;
            receiveBuffer.len = bytesToSendAndReceive;
            receiveBufferList.buffers = &receiveBuffer;
            receiveBufferList.count = 1;

            spi_transceive(pSpiCfg->pDevice, &(pSpiCfg->spiConfig),
                           &sendBufferList, &receiveBufferList);

            if (reverseBytes) {
                U_PORT_BYTE_REVERSE(valueReceived, bytesToSendAndReceive);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return valueReceived;
}

// Exchange a block of data with an SPI device.
int32_t uPortSpiControllerSendReceiveBlock(int32_t handle, const char *pSend,
                                           size_t bytesToSend, char *pReceive,
                                           size_t bytesToReceive)
{
    int32_t errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortSpiCfg_t *pSpiCfg;
    struct spi_buf sendBuffer;
    struct spi_buf_set sendBufferList;
    struct spi_buf_set *pSendBufferList = NULL;
    struct spi_buf receiveBuffer;
    struct spi_buf_set receiveBufferList;
    struct spi_buf_set *pReceiveBufferList = NULL;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiCfg) / sizeof(gSpiCfg[0])) &&
            (gSpiCfg[handle].pDevice != NULL)) {
            pSpiCfg = &(gSpiCfg[handle]);

            if (pSend != NULL) {
                sendBuffer.buf = (char *) pSend;
                sendBuffer.len = bytesToSend;
                sendBufferList.buffers = &sendBuffer;
                sendBufferList.count = 1;
                pSendBufferList = &sendBufferList;
            }

            if (pReceive != NULL) {
                receiveBuffer.buf = pReceive;
                receiveBuffer.len = bytesToReceive;
                receiveBufferList.buffers = &receiveBuffer;
                receiveBufferList.count = 1;
                pReceiveBufferList = &receiveBufferList;
            } else {
                bytesToReceive = 0;
            }

            errorCodeOrReceiveSize = spi_transceive(pSpiCfg->pDevice, &(pSpiCfg->spiConfig),
                                                    pSendBufferList, pReceiveBufferList);
            if (errorCodeOrReceiveSize == 0) {
                errorCodeOrReceiveSize = bytesToReceive;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrReceiveSize;
}

// End of file
