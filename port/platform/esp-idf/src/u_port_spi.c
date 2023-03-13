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
 * @brief Implementation of the port SPI API for the ESP-IDF platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"  // memset()

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_spi.h"

#include "driver/spi_common.h"
#include "driver/spi_master.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_SPI_MAX_NUM
/** The number of SPI HW blocks that are available on ESP32.  Note
 * that the first two of these are used to access the ESP32's own
 * flash memory and so are not actually allowed here but we keep
 * the array at this number to avoid having to -2 everywhere.
 */
# define U_PORT_SPI_MAX_NUM 4
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per SPI instance.
 */
typedef struct {
    int32_t pinMosi;
    int32_t pinMiso;
    uint8_t fillByte;
    spi_device_handle_t deviceHandle; // NULL if no device has been opened
    spi_device_interface_config_t *pDeviceCfg;
    bool initialised; // false if entry not in use.
} uPortSpiData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** SPI device data.
 */
static uPortSpiData_t gSpiData[U_PORT_SPI_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Close an SPI instance.
static void closeSpi(int32_t index)
{
    if (gSpiData[index].initialised) {
        if (gSpiData[index].deviceHandle != NULL) {
            spi_bus_remove_device(gSpiData[index].deviceHandle);
        }
        uPortFree(gSpiData[index].pDeviceCfg);
        spi_bus_free(index);
        // Mark as no longer in use
        memset(&(gSpiData[index]), 0, sizeof(gSpiData[index]));
    }
}

// Perform a transfer.
static int32_t transfer(uPortSpiData_t *pSpiData, spi_transaction_t *pTransaction)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    spi_transaction_t *pTransactionExecuted = NULL;

    // Do the transfer and wait for the callback to give the semaphore
    if (spi_device_queue_trans(pSpiData->deviceHandle, pTransaction,
                               portMAX_DELAY) == ESP_OK) {
        if (spi_device_get_trans_result(pSpiData->deviceHandle,
                                        &pTransactionExecuted,
                                        portMAX_DELAY) == ESP_OK) {
            // Since we only ever have one transaction in flight
            // we have no need to check pTransactionExecuted;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
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
            for (size_t x = 0; x < sizeof(gSpiData) / sizeof(gSpiData[0]); x++) {
                memset(&(gSpiData[x]), 0, sizeof(gSpiData[x]));
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

        // Shut down any open instances
        for (size_t x = 0; x < sizeof(gSpiData) / sizeof(gSpiData[0]); x++) {
            closeSpi(x);
        }

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
    uPortSpiData_t *pSpiData;
    spi_bus_config_t busCfg = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // We only allow SPI's 2 and 3 here since SPIs 1 and 2 are used
        // for talking to the ESP32's own internal flash
        if ((spi >= 2) && (spi < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            !gSpiData[spi].initialised && controller &&
            ((pinMosi >= 0) || (pinMiso >= 0)) && (pinClk >= 0)) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            pSpiData = &(gSpiData[spi]);
            busCfg.mosi_io_num = pinMosi;
            busCfg.miso_io_num = pinMiso;
            busCfg.sclk_io_num = pinClk;
            busCfg.quadwp_io_num = -1;
            busCfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO |
                           SPICOMMON_BUSFLAG_SCLK;
            if (spi_bus_initialize(spi, &busCfg, SPI_DMA_CH_AUTO) == ESP_OK) {
                // All good, store the pins and a default fill byte
                pSpiData->pinMosi = pinMosi;
                pSpiData->pinMiso = pinMiso;
                pSpiData->fillByte = (uint8_t) U_COMMON_SPI_FILL_WORD;
                pSpiData->initialised = true;
                // Return the SPI HW block number as the handle
                handleOrErrorCode = spi;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

// Close an SPI instance.
void uPortSpiClose(int32_t handle)
{
    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gSpiData) / sizeof(gSpiData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        closeSpi(handle);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const  uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortSpiData_t *pSpiData;
    spi_device_interface_config_t *pDeviceCfg;
    int32_t pinSelect;
    bool pinSelectInverted;
    spi_device_handle_t deviceHandle;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            gSpiData[handle].initialised && (pDevice != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pSpiData = &(gSpiData[handle]);
            pDeviceCfg = pSpiData->pDeviceCfg;
            // If there is already a device, remove the old one
            if (pSpiData->deviceHandle != NULL) {
                spi_bus_remove_device(pSpiData->deviceHandle);
                pSpiData->deviceHandle = NULL;
            }
            if (pDeviceCfg == NULL) {
                pDeviceCfg = (spi_device_interface_config_t *) pUPortMalloc(sizeof(spi_device_interface_config_t));
            }
            if (pDeviceCfg != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                memset(pDeviceCfg, 0, sizeof(*pDeviceCfg));
                pinSelect = pDevice->pinSelect & ~U_COMMON_SPI_PIN_SELECT_INVERTED;
                pinSelectInverted = ((pDevice->pinSelect & U_COMMON_SPI_PIN_SELECT_INVERTED) ==
                                     U_COMMON_SPI_PIN_SELECT_INVERTED);
                // A direct match to the enum
                pDeviceCfg->mode = (uint8_t) pDevice->mode;
                pDeviceCfg->cs_ena_posttrans = (uint8_t) ((((uint64_t) pDevice->frequencyHertz) *
                                                           pDevice->stopOffsetNanoseconds) / 1000000000);
                pDeviceCfg->clock_speed_hz = pDevice->frequencyHertz;
                pDeviceCfg->input_delay_ns = pDevice->sampleDelayNanoseconds;
                pDeviceCfg->spics_io_num = pinSelect;
                if (pinSelectInverted) {
                    pDeviceCfg->flags |= SPI_DEVICE_POSITIVE_CS;
                }
                if (pDevice->lsbFirst) {
                    pDeviceCfg->flags |= SPI_DEVICE_BIT_LSBFIRST;
                }
                pDeviceCfg->queue_size = 1;
                pSpiData->fillByte = (uint8_t) pDevice->fillWord;
                if (spi_bus_add_device(handle, pDeviceCfg, &deviceHandle) == ESP_OK) {
                    // All good, store the device handle
                    pSpiData->deviceHandle = deviceHandle;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Clean up on error
                    uPortFree(pDeviceCfg);
                    pDeviceCfg = NULL;
                }
                // Store the configuration
                pSpiData->pDeviceCfg = pDeviceCfg;
            }
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
    uPortSpiData_t *pSpiData;
    spi_device_interface_config_t *pDeviceCfg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].initialised) &&
            (gSpiData[handle].pDeviceCfg != NULL) && (pDevice != NULL)) {
            pSpiData = &(gSpiData[handle]);
            memset(pDevice, 0, sizeof(*pDevice));
            pDeviceCfg = pSpiData->pDeviceCfg;
            pDevice->pinSelect = pDeviceCfg->spics_io_num;
            if ((pDeviceCfg->flags & SPI_DEVICE_POSITIVE_CS) == SPI_DEVICE_POSITIVE_CS) {
                pDevice->pinSelect |= U_COMMON_SPI_PIN_SELECT_INVERTED;
            }
            pDevice->frequencyHertz = pDeviceCfg->clock_speed_hz;
            // Mode is a direct match to our enum
            pDevice->mode = (uCommonSpiMode_t) pDeviceCfg->mode;
            pDevice->wordSizeBytes = 1; // There can be only one
            pDevice->lsbFirst = ((pDeviceCfg->flags & SPI_DEVICE_BIT_LSBFIRST) == SPI_DEVICE_BIT_LSBFIRST);
            pDevice->stopOffsetNanoseconds = (int32_t) ((((uint64_t) pDeviceCfg->cs_ena_posttrans) *
                                                         1000000000) / pDevice->frequencyHertz);
            pDevice->sampleDelayNanoseconds = pDeviceCfg->input_delay_ns;
            pDevice->fillWord = pSpiData->fillByte;
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
    uint64_t valueReceived = {0};
    uPortSpiData_t *pSpiData;
    spi_transaction_t transaction = {0};
    bool reverseBytes = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            gSpiData[handle].initialised && (gSpiData[handle].deviceHandle != NULL) &&
            (bytesToSendAndReceive <= sizeof(value))) {
            pSpiData = &(gSpiData[handle]);

            // Need to perform byte reversal if the length of the word we
            // are sending is greater than one byte and if there is a
            // mismatch between the endianness of this processor and the
            // endianness of bit-transmission
            reverseBytes = ((bytesToSendAndReceive > 1) &&
                            (((pSpiData->pDeviceCfg->flags & SPI_DEVICE_BIT_LSBFIRST) == SPI_DEVICE_BIT_LSBFIRST) !=
                             U_PORT_IS_LITTLE_ENDIAN));

            if (reverseBytes) {
                U_PORT_BYTE_REVERSE(value, bytesToSendAndReceive);
            }

            if (pSpiData->pinMosi >= 0) {
                if (bytesToSendAndReceive <= sizeof(transaction.tx_data)) {
                    // More efficient for small transactions
                    transaction.flags |= SPI_TRANS_USE_TXDATA;
                    memcpy(transaction.tx_data, &value, bytesToSendAndReceive);
                } else {
                    transaction.tx_buffer = &value;
                }
                // Length is in bits
                transaction.length = bytesToSendAndReceive * 8;
            }
            if (pSpiData->pinMiso >= 0) {
                if (bytesToSendAndReceive <= sizeof(transaction.rx_data)) {
                    // More efficient for small transactions
                    transaction.flags |= SPI_TRANS_USE_RXDATA;
                } else {
                    transaction.rx_buffer = &valueReceived;
                }
                // Length is in bits
                transaction.rxlength = bytesToSendAndReceive * 8;
            }

            transfer(pSpiData, &transaction);
            if ((transaction.flags & SPI_TRANS_USE_RXDATA) == SPI_TRANS_USE_RXDATA) {
                memcpy(&valueReceived, transaction.rx_data, bytesToSendAndReceive);
            }
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
    uPortSpiData_t *pSpiData;
    char *pSendWithFill = NULL;
    spi_transaction_t transaction = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            gSpiData[handle].initialised && (gSpiData[handle].deviceHandle != NULL) &&
            ((gSpiData[handle].pinMosi >= 0) || (bytesToSend == 0)) &&
            ((gSpiData[handle].pinMiso >= 0) || (bytesToReceive == 0))) {
            pSpiData = &(gSpiData[handle]);

            errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pSpiData->pinMosi >= 0) {
                // On ESP32 have to send at least as many bytes as
                // we want to receive, adding our own fill, hence we
                // check bytesToReceive here also
                if ((bytesToSend <= sizeof(transaction.tx_data)) &&
                    (bytesToReceive <= sizeof(transaction.tx_data))) {
                    // More efficient for small transactions
                    transaction.flags |= SPI_TRANS_USE_TXDATA;
                    memcpy(transaction.tx_data, pSend, bytesToSend);
                    if (bytesToReceive > bytesToSend) {
                        memset(&(transaction.tx_data[bytesToSend]),
                               pSpiData->fillByte, bytesToReceive - bytesToSend);
                        bytesToSend = bytesToReceive;
                    }
                } else {
                    // In order to send less than we receive, have to
                    // create a new buffer with added fill in it, or
                    // the transaction will be rejected
                    if (bytesToReceive > bytesToSend) {
                        errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        pSendWithFill = (char *) pUPortMalloc(bytesToReceive);
                        if (pSendWithFill != NULL) {
                            errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                            memcpy(pSendWithFill, pSend, bytesToSend);
                            memset(pSendWithFill + bytesToSend,
                                   pSpiData->fillByte,
                                   bytesToReceive - bytesToSend);
                            bytesToSend = bytesToReceive;
                            transaction.tx_buffer = pSendWithFill;
                        }
                    } else {
                        transaction.tx_buffer = pSend;
                    }
                }
                // Length is in bits
                transaction.length = bytesToSend * 8;
            }
            if (pSpiData->pinMiso >= 0) {
                if ((bytesToReceive <= sizeof(transaction.rx_data)) &&
                    (pSendWithFill == NULL)) {
                    // More efficient for small transactions
                    transaction.flags |= SPI_TRANS_USE_RXDATA;
                } else {
                    transaction.rx_buffer = pReceive;
                }
                // Length is in bits
                transaction.rxlength = bytesToReceive * 8;
            }

            if (errorCodeOrReceiveSize == (int32_t) U_ERROR_COMMON_SUCCESS) {
                errorCodeOrReceiveSize = transfer(pSpiData, &transaction);
                if (errorCodeOrReceiveSize == 0) {
                    errorCodeOrReceiveSize = (int32_t) bytesToReceive;
                    if ((transaction.flags & SPI_TRANS_USE_RXDATA) == SPI_TRANS_USE_RXDATA) {
                        memcpy(pReceive, transaction.rx_data, bytesToReceive);
                    }
                }

                // Free memory
                uPortFree(pSendWithFill);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrReceiveSize;
}

// End of file
