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
 * @brief Implementation of the port SPI API for the NRF52 platform.
 *
 * Note: unlike with the NRF52 UART API, here we use the Nordic nrfx
 * layer and hence, to use an SPI HW block, it must be *enabled* in your
 * sdk_config.h file.  So, to use instance 0, SPI0_ENABLED, NRFX_SPI0_ENABLED,
 * and NRFX_SPIM0_ENABLED must be set to 1 in your sdk_config.h file,
 * to use instance 1 SPI1_ENABLED, NRFX_SPI1_ENABLED, and NRFX_SPIM1_ENABLED
 * must be set to 1 in your sdk_config.h file, etc.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"   // memset()
#include "limits.h"   // UINT8_MAX

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_spi.h"

#include "nrf.h"
#include "nrfx_spi.h"
#include "nrfx_spim.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_SPI_MAX_NUM
/** The number of SPI HW blocks that are available; on NRF52 there
 * can be up to four SPI controllers.
 */
# define U_PORT_SPI_MAX_NUM 4
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per SPI interface.
 */
typedef struct {
    nrfx_spim_t instance;
    nrfx_spim_config_t cfg;
    uPortSemaphoreHandle_t completionSemaphore;
} uPortSpiData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Storage for the SPI instances.
 */
static uPortSpiData_t gSpiData[U_PORT_SPI_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Close an SPI instance.
// gMutex should be locked before this is called.
static void closeSpi(uPortSpiData_t *pSpi)
{
    if ((pSpi != NULL) && (pSpi->instance.p_reg != NULL)) {
        nrfx_spim_uninit(&pSpi->instance);
        uPortSemaphoreDelete(pSpi->completionSemaphore);
        // Zero the instance to indicate that it is no longer in use
        memset(&pSpi->instance, 0, sizeof(pSpi->instance));
    }
}

// Convert a clock frequency in Hertx to an nRF52 number.
static nrf_spim_frequency_t frequencyHertzToNrf52(int32_t hertz)
{
    nrf_spim_frequency_t nrf52 = NRF_SPIM_FREQ_125K;

    if (hertz >= 32000000) {
        nrf52 = NRF_SPIM_FREQ_32M;
    } else if (hertz >= 16000000) {
        nrf52 = NRF_SPIM_FREQ_16M;
    } else if (hertz >= 8000000) {
        nrf52 = NRF_SPIM_FREQ_8M;
    } else if (hertz >= 4000000) {
        nrf52 = NRF_SPIM_FREQ_4M;
    } else if (hertz >= 2000000) {
        nrf52 = NRF_SPIM_FREQ_2M;
    } else if (hertz >= 1000000) {
        nrf52 = NRF_SPIM_FREQ_1M;
    } else if (hertz >= 500000) {
        nrf52 = NRF_SPIM_FREQ_500K;
    } else if (hertz >= 250000) {
        nrf52 = NRF_SPIM_FREQ_250K;
    } else if (hertz >= 125000) {
        nrf52 = NRF_SPIM_FREQ_125K;
    }

    return nrf52;
}

// Convert an nRF52 clock frequency number to Hertz.
static int32_t frequencyNrf52ToHertz(nrf_spim_frequency_t nrf52)
{
    int32_t hertz = 0;

    switch (nrf52) {
        case NRF_SPIM_FREQ_125K:
            hertz = 125000;
            break;
        case NRF_SPIM_FREQ_250K:
            hertz = 250000;
            break;
        case NRF_SPIM_FREQ_500K:
            hertz = 500000;
            break;
        case NRF_SPIM_FREQ_1M:
            hertz = 1000000;
            break;
        case NRF_SPIM_FREQ_2M:
            hertz = 2000000;
            break;
        case NRF_SPIM_FREQ_4M:
            hertz = 4000000;
            break;
        case NRF_SPIM_FREQ_8M:
            hertz = 8000000;
            break;
        case NRF_SPIM_FREQ_16M:
            hertz = 16000000;
            break;
        case NRF_SPIM_FREQ_32M:
            hertz = 32000000;
            break;
        default:
            break;
    }

    return hertz;
}

// Convert a duration in nanoseconds to a peripheral clock cycle.
static uint8_t nanosecondsToClocks(int32_t nanoseconds)
{
    int64_t nanosecondsInt64;

    // NRF52 counts duration in units of 15.625 ns
    nanosecondsInt64 = nanoseconds * 1000;
    nanosecondsInt64 /= 15625000;
    if (nanosecondsInt64 > UINT8_MAX) {
        nanosecondsInt64 = UINT8_MAX;
    }

    return (uint8_t) nanosecondsInt64;
}

// Convert a peripheral clock cycle into nanoseconds.
static int32_t clocksToNanoseconds(uint8_t nrf52)
{
    return (((int32_t) nrf52) * 15625) / 1000;
}

// Get the configuration of the device from pCfg into pDevice.
static void getDevice(const nrfx_spim_config_t *pCfg,
                      uCommonSpiControllerDevice_t *pDevice)
{
    memset(pDevice, 0, sizeof(*pDevice));
    pDevice->pinSelect = -1;
    if (pCfg->ss_pin != NRFX_SPIM_PIN_NOT_USED) {
        pDevice->pinSelect = pCfg->ss_pin;
        if (pCfg->ss_active_high) {
            pDevice->pinSelect |= U_COMMON_SPI_PIN_SELECT_INVERTED;
        }
    }
    pDevice->frequencyHertz = frequencyNrf52ToHertz(pCfg->frequency);
    // The mode enums are a direct match
    pDevice->mode = (uCommonSpiMode_t) pCfg->mode;
    pDevice->wordSizeBytes = 1; // There can be only one
    pDevice->lsbFirst = (pCfg->bit_order == NRF_SPIM_BIT_ORDER_LSB_FIRST);
    pDevice->fillWord = (uint8_t) pCfg->orc;
    if (pCfg->use_hw_ss) {
        pDevice->startOffsetNanoseconds = clocksToNanoseconds(pCfg->ss_duration);
        pDevice->stopOffsetNanoseconds = pDevice->startOffsetNanoseconds;
        pDevice->sampleDelayNanoseconds = clocksToNanoseconds(pCfg->rx_delay);
    }
}

// Determine if the configuration in pDevice differs from the given one.
static bool configIsDifferent(const nrfx_spim_config_t *pCfgCurrent,
                              const uCommonSpiControllerDevice_t *pDevice)
{
    uCommonSpiControllerDevice_t deviceCurrent;

    getDevice(pCfgCurrent, &deviceCurrent);

    return (deviceCurrent.pinSelect != pDevice->pinSelect) ||
           (deviceCurrent.frequencyHertz != pDevice->frequencyHertz) ||
           (deviceCurrent.mode != pDevice->mode) ||
           (deviceCurrent.wordSizeBytes != pDevice->wordSizeBytes) ||
           (deviceCurrent.lsbFirst != pDevice->lsbFirst) ||
           (deviceCurrent.startOffsetNanoseconds != pDevice->startOffsetNanoseconds) ||
           (deviceCurrent.stopOffsetNanoseconds != pDevice->stopOffsetNanoseconds) ||
           (deviceCurrent.sampleDelayNanoseconds != pDevice->sampleDelayNanoseconds) ||
           (deviceCurrent.fillWord != (uint8_t) pDevice->fillWord);
}

// Event handler.
// Note: will be called from interrupt context
static void eventHandlerIrq(nrfx_spim_evt_t const *pEvent, void *pContext)
{
    uPortSpiData_t *pSpiData = (uPortSpiData_t *) pContext;

    (void) pEvent;

    // No concept of an error here, we're done
    uPortSemaphoreGiveIrq(pSpiData->completionSemaphore);
}

// Perform a transfer.
static int32_t transfer(uPortSpiData_t *pSpi, nrfx_spim_xfer_desc_t *pXferDesc)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    // Make sure the semaphore is taken
    uPortSemaphoreTryTake(pSpi->completionSemaphore, 0);
    // Do the transfer and wait for the event handler to give the semaphore
    if (nrfx_spim_xfer(&(pSpi->instance), pXferDesc, 0) == NRFX_SUCCESS) {
        errorCode = uPortSemaphoreTake(pSpi->completionSemaphore);
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
                memset(&gSpiData[x].instance, 0, sizeof(gSpiData[x].instance));
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
            closeSpi(&(gSpiData[x]));
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
    nrfx_spim_t instance = {0};
    nrfx_spim_config_t cfg = NRFX_SPIM_DEFAULT_CONFIG;
    uPortSemaphoreHandle_t completionSemaphore;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((spi >= 0) && (spi < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[spi].instance.p_reg == NULL) && controller &&
            ((pinMosi >= 0) || (pinMiso >= 0)) && (pinClk >= 0)) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            switch (spi) {
                case 0:
#if NRFX_SPIM0_ENABLED
                    instance.p_reg        = NRF_SPIM0;
                    instance.drv_inst_idx = NRFX_SPIM0_INST_IDX;
#endif
                    break;
                case 1:
#if NRFX_SPIM1_ENABLED
                    instance.p_reg        = NRF_SPIM1;
                    instance.drv_inst_idx = NRFX_SPIM1_INST_IDX;
#endif
                    break;
                case 2:
#if NRFX_SPIM2_ENABLED
                    instance.p_reg        = NRF_SPIM2;
                    instance.drv_inst_idx = NRFX_SPIM2_INST_IDX;
#endif
                    break;
                case 3:
#if NRFX_SPIM3_ENABLED
                    instance.p_reg        = NRF_SPIM3;
                    instance.drv_inst_idx = NRFX_SPIM3_INST_IDX;
#endif
                    break;
                default:
                    break;
            }
            if (instance.p_reg != NULL) {
                handleOrErrorCode = uPortSemaphoreCreate(&completionSemaphore, 0, 1);
                if (handleOrErrorCode == 0) {
                    cfg.sck_pin = pinClk;
                    if (pinMosi >= 0) {
                        cfg.mosi_pin = pinMosi;
                    }
                    if (pinMiso >= 0) {
                        cfg.miso_pin = pinMiso;
                    }
                    handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                    if (nrfx_spim_init(&instance, &cfg, eventHandlerIrq, &(gSpiData[spi])) == NRFX_SUCCESS) {
                        // Copy the values into our instance storage
                        gSpiData[spi].instance = instance;
                        gSpiData[spi].cfg = cfg;
                        gSpiData[spi].completionSemaphore = completionSemaphore;
                        // Return the SPI HW block number as the handle
                        handleOrErrorCode = spi;
                    } else {
                        // Clean up on error
                        uPortSemaphoreDelete(completionSemaphore);
                    }
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

        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0]))) {
            closeSpi(&(gSpiData[handle]));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    nrfx_spim_config_t cfg = NRFX_SPIM_DEFAULT_CONFIG;
    int32_t offsetDuration;
    int32_t pinSelect;
    bool pinSelectInverted;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].instance.p_reg != NULL) && (pDevice != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (configIsDifferent(&(gSpiData[handle].cfg), pDevice)) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // If the configuration we're given is not the same as
                // the current one we need to un-initialise and re-initialise
                // SPI with the new configuration
                nrfx_spim_uninit(&(gSpiData[handle].instance));
                cfg.sck_pin = gSpiData[handle].cfg.sck_pin;
                cfg.mosi_pin = gSpiData[handle].cfg.mosi_pin;
                cfg.miso_pin = gSpiData[handle].cfg.miso_pin;
                if (gSpiData[handle].instance.p_reg == NRF_SPIM3) {
                    // Apparently only instance 3 can support this and the
                    // other things under NRFX_SPIM_EXTENDED_ENABLED,
                    // i.e. rx_delay and ss_duration; we use this as a marker
                    // (and it is in any case required for ss_duration support)
                    cfg.use_hw_ss = true;
                }
                cfg.ss_pin = NRFX_SPIM_PIN_NOT_USED;
                if (pDevice->pinSelect >= 0) {
                    pinSelectInverted = ((pDevice->pinSelect & U_COMMON_SPI_PIN_SELECT_INVERTED) ==
                                         U_COMMON_SPI_PIN_SELECT_INVERTED);
                    pinSelect = pDevice->pinSelect & ~U_COMMON_SPI_PIN_SELECT_INVERTED;
                    cfg.ss_pin = pinSelect;
                    cfg.ss_active_high = pinSelectInverted;
                    if (cfg.use_hw_ss) {
                        offsetDuration = pDevice->startOffsetNanoseconds;
                        if (pDevice->stopOffsetNanoseconds > offsetDuration) {
                            offsetDuration = pDevice->stopOffsetNanoseconds;
                        }
                        cfg.ss_duration = nanosecondsToClocks(offsetDuration);
                    }
                }
                cfg.orc = (uint8_t) pDevice->fillWord;
                cfg.frequency = frequencyHertzToNrf52(pDevice->frequencyHertz);
                // These enums are a direct match
                cfg.mode = (nrf_spim_mode_t) pDevice->mode;
                cfg.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
                if (pDevice->lsbFirst) {
                    cfg.bit_order = NRF_SPIM_BIT_ORDER_LSB_FIRST;
                }
                if (cfg.use_hw_ss) {
                    cfg.rx_delay = nanosecondsToClocks(pDevice->sampleDelayNanoseconds);
                }
                if (nrfx_spim_init(&(gSpiData[handle].instance), &cfg, eventHandlerIrq,
                                   &(gSpiData[handle])) == NRFX_SUCCESS) {
                    // Now we can store the new configuration
                    gSpiData[handle].cfg = cfg;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
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

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].instance.p_reg != NULL) && (pDevice != NULL)) {
            getDevice(&(gSpiData[handle].cfg), pDevice);
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
    bool reverseBytes = false;
    nrfx_spim_xfer_desc_t xferDesc = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].instance.p_reg != NULL) &&
            (bytesToSendAndReceive <= sizeof(value))) {

            // Need to perform byte reversal if the length of the word we
            // are sending is greater than one byte and if there is a
            // mismatch between the endianness of this processor and the
            // endianness of bit-transmission
            reverseBytes = ((bytesToSendAndReceive > 1) &&
                            ((gSpiData[handle].cfg.bit_order == NRF_SPIM_BIT_ORDER_LSB_FIRST) !=
                             U_PORT_IS_LITTLE_ENDIAN));

            if (reverseBytes) {
                U_PORT_BYTE_REVERSE(value, bytesToSendAndReceive);
            }

            if (gSpiData[handle].cfg.mosi_pin != NRFX_SPIM_PIN_NOT_USED) {
                xferDesc.p_tx_buffer = (uint8_t const *) &value;
                xferDesc.tx_length = bytesToSendAndReceive;
            }
            if (gSpiData[handle].cfg.miso_pin != NRFX_SPIM_PIN_NOT_USED) {
                xferDesc.p_rx_buffer = (uint8_t *) &valueReceived;
                xferDesc.rx_length = bytesToSendAndReceive;
            }

            transfer(&(gSpiData[handle]), &xferDesc);

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
    nrfx_spim_xfer_desc_t xferDesc = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].instance.p_reg != NULL) &&
            ((gSpiData[handle].cfg.mosi_pin != NRFX_SPIM_PIN_NOT_USED) || (bytesToSend == 0)) &&
            ((gSpiData[handle].cfg.miso_pin != NRFX_SPIM_PIN_NOT_USED) || (bytesToReceive == 0))) {

            if (gSpiData[handle].cfg.mosi_pin != NRFX_SPIM_PIN_NOT_USED) {
                xferDesc.p_tx_buffer = (uint8_t const *) pSend;
                xferDesc.tx_length = bytesToSend;
            }
            if (gSpiData[handle].cfg.miso_pin != NRFX_SPIM_PIN_NOT_USED) {
                xferDesc.p_rx_buffer = (uint8_t *) pReceive;
                xferDesc.rx_length = bytesToReceive;
            }

            errorCodeOrReceiveSize = transfer(&(gSpiData[handle]), &xferDesc);
            if (errorCodeOrReceiveSize == 0) {
                errorCodeOrReceiveSize = (int32_t) bytesToReceive;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrReceiveSize;
}

// End of file
