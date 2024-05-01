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
 * @brief Implementation of the port SPI API for the STM32 platform.
 *
 * Note that the SPI HW of the STM32U5 chip, while not as utterly
 * different to the STM32F4 HW as for the I2C case, is still quite
 * different (e.g. has FIFOs and the like now) but poor old SPI3
 * remains its old unaugmented self so, to give the application
 * the ability to use any SPI, we ignore the new capabilities, just
 * accommodating the differences.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"   // memset()

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_spi.h"

#ifdef STM32U575xx
# include "stm32u5xx_ll_bus.h"
# include "stm32u5xx_ll_rcc.h"
# include "stm32u5xx_ll_gpio.h"
# include "stm32u5xx_ll_spi.h"
# include "stm32u5xx_hal_spi.h"
#else
# include "stm32f4xx_ll_bus.h"
# include "stm32f4xx_ll_rcc.h"
# include "stm32f4xx_ll_gpio.h"
# include "stm32f4xx_ll_spi.h"
# include "stm32f4xx_hal_spi.h"
#endif

#include "u_port_private.h"      // Down here 'cos it needs GPIO_TypeDef

/* This code uses the LL API as otherwise we have to keep
 * an entire structure of type SPI_HandleTypeDef in memory
 * for no very good reason.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_SPI_MAX_NUM
/** The number of SPI HW blocks that are available; there can be up
 * to six SPI controllers on an STM32F4, only up to 3 on an STM32U5.
 */
# ifndef STM32U575xx
#  define U_PORT_SPI_MAX_NUM 6
# else
#  define U_PORT_SPI_MAX_NUM 3
#  endif
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of SPI pin.
 */
typedef enum {
    U_PORT_SPI_PIN_TYPE_MOSI,
    U_PORT_SPI_PIN_TYPE_MISO,
    U_PORT_SPI_PIN_TYPE_CLK,
    U_PORT_SPI_PIN_TYPE_SELECT
} uPortSpiPinType_t;

/** Structure of the things we need to keep track of per SPI instance.
 */
typedef struct {
    SPI_TypeDef *pReg; // Set to NULL if this entry is not in use
    int32_t pinMosi;
    int32_t pinMiso;
    int32_t pinSelect;
    uint16_t fillWord;
} uPortSpiData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Table of the HW addresses for each SPI block.
 */
static SPI_TypeDef *const gpSpiReg[] = {NULL,  // This to avoid having to -1
                                        SPI1,
                                        SPI2,
                                        SPI3,
#ifndef STM32U575xx
                                        SPI4,
                                        SPI5,
                                        SPI6
#endif
                                       };

/** Storage for the SPI instances.
 */
static uPortSpiData_t gSpiData[U_PORT_SPI_MAX_NUM + 1]; // +1 to avoid having to -1

/** Variable to keep track of the number of SPI interfaces open.
 */
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the SPI number from a register address.
static int32_t getSpi(const SPI_TypeDef *pReg)
{
    int32_t errorCodeOrSpi = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Start at 1 below 'cos the first entry in gpI2cReg is empty
    for (size_t x = 1; (x < sizeof(gpSpiReg) / sizeof (gpSpiReg[0])) &&
         (errorCodeOrSpi < 0); x++) {
        if (gpSpiReg[x] == pReg) {
            errorCodeOrSpi = x;
        }
    }

    return errorCodeOrSpi;
}

// Configure clock source to be peripheral clock: only needed
// for STM32U5, where the default of PCLK runs too fast for
// us to achieve a divisor that brings the clock low enough,
// so we use MSIK instead which defaults to 4 MHz.
static void clockCfg(int32_t spi)
{
#ifdef STM32U575xx
    switch (spi) {
        case 1:
            __HAL_RCC_SPI1_CONFIG(RCC_SPI1CLKSOURCE_MSIK);
            break;
        case 2:
            __HAL_RCC_SPI2_CONFIG(RCC_SPI2CLKSOURCE_MSIK);
            break;
        case 3:
            __HAL_RCC_SPI3_CONFIG(RCC_SPI3CLKSOURCE_MSIK);
            break;
        default:
            break;
    }
#else
    (void) spi;
#endif
}

// Enable clock to an SPI block; these are macros so can't be
// entries in a table.
static int32_t clockEnable(const SPI_TypeDef *pReg)
{
    int32_t errorCodeOrSpi = getSpi(pReg);

    clockCfg(errorCodeOrSpi);
    switch (errorCodeOrSpi) {
        case 1:
            __HAL_RCC_SPI1_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 2:
            __HAL_RCC_SPI2_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 3:
            __HAL_RCC_SPI3_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#ifndef STM32U575xx
        case 4:
            __HAL_RCC_SPI4_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 5:
            __HAL_RCC_SPI5_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 6:
            __HAL_RCC_SPI6_CLK_ENABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#endif
        default:
            break;
    }

    return errorCodeOrSpi;
}

// Disable clock to an SPI block; these are macros so can't be
// entries in a table.
static int32_t clockDisable(const SPI_TypeDef *pReg)
{
    int32_t errorCodeOrSpi = getSpi(pReg);

    switch (errorCodeOrSpi) {
        case 1:
            __HAL_RCC_SPI1_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 2:
            __HAL_RCC_SPI2_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 3:
            __HAL_RCC_SPI3_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#ifndef STM32U575xx
        case 4:
            __HAL_RCC_SPI4_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 5:
            __HAL_RCC_SPI5_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 6:
            __HAL_RCC_SPI6_CLK_DISABLE();
            errorCodeOrSpi = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#endif
        default:
            break;
    }

    return errorCodeOrSpi;
}

// Get the clock for the given SPI.
static uint32_t getClock(const SPI_TypeDef *pReg)
{
    uint32_t clock = LL_RCC_PERIPH_FREQUENCY_NO;
    int32_t spi = getSpi(pReg);

#ifndef STM32U575xx
    LL_RCC_ClocksTypeDef rccClocks;
    LL_RCC_GetSystemClocksFreq(&rccClocks);
    // The clock for the STM32F4 case is always the peripheral
    // clock; from figure 4 of the STM32F437 data sheet:
    switch (spi) {
        case 1:
            clock = rccClocks.PCLK2_Frequency;
            break;
        case 2:
            clock = rccClocks.PCLK1_Frequency;
            break;
        case 3:
            clock = rccClocks.PCLK1_Frequency;
            break;
        case 4:
            clock = rccClocks.PCLK2_Frequency;
            break;
        case 5:
            clock = rccClocks.PCLK2_Frequency;
            break;
        case 6:
            clock = rccClocks.PCLK2_Frequency;
            break;
        default:
            break;
    }
#else
    // For the STM32U5 case the clock can be from
    // four different sources for any given SPI.
    switch (spi) {
        case 1:
            clock = LL_RCC_GetSPIClockFreq(LL_RCC_SPI1_CLKSOURCE);
            break;
        case 2:
            clock = LL_RCC_GetSPIClockFreq(LL_RCC_SPI2_CLKSOURCE);
            break;
        case 3:
            clock = LL_RCC_GetSPIClockFreq(LL_RCC_SPI3_CLKSOURCE);
            break;
        default:
            break;
    }
#endif
    return clock;
}

// Get a power-of-two divisor for the APB frequency to
// achieve at or less than the desired SPI frequency.
static int32_t getPowerOfTwoDivisor(uint32_t apbFrequencyHertz, int32_t frequencyHertz)
{
    int32_t shift = 0;
    uint32_t result;

    do {
        result = apbFrequencyHertz >> shift;
        shift++;
    }  while (result > (uint32_t) frequencyHertz);
    shift--;

    return shift;
}

// Get the alternate function for an SPI pin.
static int32_t getAf(int32_t spi, int32_t pin, uPortSpiPinType_t pinType)
{
    int32_t af = LL_GPIO_AF_5;

    // From the data sheet for the STM32F437, alternate function
    // is AF5 in all cases except SPI3 which is AF6 unless this
    // is the MOSI pin and it is on PD6.  For the STM32U5 the same is
    // true but there are two additional exceptions for SPI2 where,
    // if the CLK pin is on PA9/PD3 or the MOSI pin is on PC1, it is AF3.
    if ((spi == 3) && (pinType != U_PORT_SPI_PIN_TYPE_MOSI) && (pin != 0x36)) {
        af = LL_GPIO_AF_6;
#ifdef STM32U575xx
    } else if ((spi == 2) &&
               (((pinType == U_PORT_SPI_PIN_TYPE_CLK) && ((pin == 0x09) || (pin = 0x33))) ||
                ((pinType == U_PORT_SPI_PIN_TYPE_MOSI) && (pin == 0x21)))) {
        af = LL_GPIO_AF_3;
#endif
    }

    return af;
}

// Initialise a GPIO for SPI.
static int32_t initGpio(int32_t spi, int32_t pin, uPortSpiPinType_t pinType)
{
    LL_GPIO_InitTypeDef gpioInitStruct = {0};

    gpioInitStruct.Pin = (1U << U_PORT_STM32F4_GPIO_PIN(pin));
    gpioInitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    // Note: we used to set the speed to LL_GPIO_SPEED_FREQ_VERY_HIGH
    // but that seemed to cause significant comms failures; setting
    // the speed to medium (up to 50 MHz) is more reliable and perfectly
    // sufficient for what is needed here
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpioInitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpioInitStruct.Pull = LL_GPIO_PULL_UP;
    gpioInitStruct.Alternate = getAf(spi, pin, pinType);

    return LL_GPIO_Init(pUPortPrivateGpioGetReg(pin),
                        &gpioInitStruct);
}

// Configure the SPI registers; a much reduced version of HAL_SPI_Init(),
// returning zero on success else negative error code.
static int32_t configureSpi(SPI_TypeDef *pReg,
                            const uCommonSpiControllerDevice_t *pDevice,
                            int32_t pinMosi)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t powerOfTwoClockDivisor;

    // Disable the SPI block
    LL_SPI_Disable(pReg);

#ifdef STM32U575xx
    // On STM32U5 the SPI HW can get confused if it sees SELECT
    // as being low when master mode is enabled: it thinks the
    // slave may have pulled it low to perform some sort of
    // multi-master trick so will raise a MODF error and then not
    // let SPI be enabled (i.e. the PE bit cannot be set).  To
    // avoid this, if we do not yet have a SELECT pin, set the SW
    // version of SELECT and set the SW version of the pin high
    if (pDevice->pinSelect < 0) {
        LL_SPI_SetNSSMode(pReg, LL_SPI_NSS_SOFT);
        LL_SPI_SetInternalSSLevel(pReg, LL_SPI_SS_LEVEL_HIGH);
    }
#endif

    // Set master mode
    LL_SPI_SetMode(pReg, LL_SPI_MODE_MASTER);

    // Set the clock frequency: we don't dare change the
    // APB bus frequency, since that may affect many things,
    // we just get as close as we can with the clock divisor
    powerOfTwoClockDivisor = getPowerOfTwoDivisor(getClock(pReg),
                                                  pDevice->frequencyHertz);

    // Baud rate control is a 3-bit value where 0 means /2, 1 means /4, etc.
    if ((powerOfTwoClockDivisor > 0) && (powerOfTwoClockDivisor <= 8)) {
        powerOfTwoClockDivisor--;
#ifndef STM32U575xx
        LL_SPI_SetBaudRatePrescaler(pReg, powerOfTwoClockDivisor << SPI_CR1_BR_Pos);
#else
        LL_SPI_SetBaudRatePrescaler(pReg, powerOfTwoClockDivisor << SPI_CFG1_MBR_Pos);
#endif
        // Set clock polarity and phase
        if ((pDevice->mode & U_COMMON_SPI_MODE_CPOL_BIT_MASK) == U_COMMON_SPI_MODE_CPOL_BIT_MASK) {
            LL_SPI_SetClockPolarity(pReg, LL_SPI_POLARITY_HIGH);
        }
        if ((pDevice->mode & U_COMMON_SPI_MODE_CPHA_BIT_MASK) == U_COMMON_SPI_MODE_CPHA_BIT_MASK) {
            LL_SPI_SetClockPhase(pReg, LL_SPI_PHASE_2EDGE);
        }
        // Set word size
        if (pDevice->wordSizeBytes > 1) {
            // Note: for the STM32U5 case SPI HW blocks
            // 1 and 2 can be more flexible than this,
            // however HW block 3 cannot and so we stick
            // to the simple choice of 8 or 16 bits
            LL_SPI_SetDataWidth(pReg, LL_SPI_DATAWIDTH_16BIT);
        }
        // Set bit order
        if (pDevice->lsbFirst) {
            LL_SPI_SetTransferBitOrder(pReg, LL_SPI_LSB_FIRST);
        }
        // Now set the right NSS mode, noting that the pin
        // must be high or we'll be back in mode-fault again
        if (pDevice->pinSelect >= 0) {
            LL_SPI_SetNSSMode(pReg, LL_SPI_NSS_HARD_OUTPUT);
        }
        // Set RX only mode
        if (pinMosi < 0) {
            LL_SPI_SetTransferDirection(pReg, LL_SPI_SIMPLEX_RX);
        }
        // Note: since the CS/NSS/Select line goes low when SPI
        // is enabled and high when SPI is disabled, we keep
        // SPI disabled here so that it can be toggled during
        // transmission
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Perform an SPI transfer.
// gMutex should be locked before this is called.
static int32_t transfer(int32_t spi, const char *pSend, size_t bytesToSend,
                        char *pReceive, size_t bytesToReceive)
{
    int32_t errorCodeOrReceiveSize = 0;
    SPI_TypeDef *pReg = gpSpiReg[spi];
    size_t transferSize = bytesToSend;
    uint16_t fillWord = gSpiData[spi].fillWord;
    size_t wordSize = 1;

    if (bytesToReceive > transferSize) {
        transferSize = bytesToReceive;
    }

    if (LL_SPI_GetDataWidth(pReg) == LL_SPI_DATAWIDTH_16BIT) {
        wordSize++;
    }

    if (transferSize > 0) {
#ifdef STM32U575xx
        // For STM32U5 have to set transfer size
        // (noting that threshold is by default 0,
        // meaning no RX FIFO, so we can just leave
        // it alone).
        LL_SPI_SetTransferSize(pReg, wordSize);
#endif
        // Enable SPI, which sets CS/NSS/Select
        LL_SPI_Enable(pReg);
        if (bytesToSend == 0) {
            // If there's nothing to send, send fill
            pSend = (const char *) &fillWord;
        }
        // Do the blocking send/receive
        while (transferSize > 0) {
#ifdef STM32U575xx
            // For STM32U5 have to set CSTART also
            LL_SPI_StartMasterTransfer(pReg);
#endif
            // Note: for the STM32U5 case SPI HW blocks
            // 1 and 2 can be more flexible than this,
            // however HW block 3 cannot and so we stick
            // to this simple choice
            if (wordSize > 1) {
                LL_SPI_TransmitData16(pReg, *(uint16_t *) pSend);
            } else {
                LL_SPI_TransmitData8(pReg, *pSend);
            }
            // Wait for the data to be sent
#ifndef STM32U575xx
            while (!LL_SPI_IsActiveFlag_TXE(pReg)) {}
#else
            while (!LL_SPI_IsActiveFlag_TXP(pReg)) {}
#endif
            if (bytesToReceive > 0) {
                // Wait for data to be received
#ifndef STM32U575xx
                while (!LL_SPI_IsActiveFlag_RXNE(pReg)) {}
#else
                while (!LL_SPI_IsActiveFlag_RXP(pReg)) {}
#endif
                // Read it (which will reset RXNE/RXP)
                if (wordSize > 1) {
                    *(uint16_t *) pReceive = LL_SPI_ReceiveData16(pReg);
                } else {
                    *pReceive = LL_SPI_ReceiveData8(pReg);
                }
                bytesToReceive -= wordSize;
                errorCodeOrReceiveSize += wordSize;
                pReceive += wordSize;
            }
            transferSize -= wordSize;
            if ((bytesToSend > 0) || (wordSize > 1)) {
                // Increment send pointer if we're actually
                // sending stuff or if we're sending fill
                // and there is more than one byte of it
                pSend += wordSize;
            }
            if (bytesToSend >= wordSize) {
                bytesToSend -= wordSize;
                if (bytesToSend == 0) {
                    // Start sending fill
                    pSend = (const char *) &fillWord;
                }
            }
            if ((bytesToSend == 0) &&
                (pSend >= ((const char *) &fillWord) + sizeof(fillWord))) {
                // Make sure to stay within the fill word if
                // sending fill
                pSend = (const char *) &fillWord;
            }

#ifdef STM32U575xx
            // For STM32U5 need to wait for end of
            // transaction and then clear both the
            // EOT and the TXTF flags otherwise the
            // HW won't send again
            while (!LL_SPI_IsActiveFlag_EOT(pReg)) {}
            LL_SPI_ClearFlag_EOT(pReg);
            LL_SPI_ClearFlag_TXTF(pReg);
#endif
        }

        // Disable SPI, which will switch off CS/NSS/Select
        LL_SPI_Disable(pReg);
    }

    return errorCodeOrReceiveSize;
}

// Get the configuration of the given SPI into pDevice.
// gMutex should be locked before this is called.
static void getDevice(int32_t spi, uCommonSpiControllerDevice_t *pDevice)
{
    SPI_TypeDef *pReg = gpSpiReg[spi];

    memset(pDevice, 0, sizeof(*pDevice));
    pDevice->pinSelect = gSpiData[spi].pinSelect;
#ifndef STM32U575xx
    pDevice->frequencyHertz = getClock(pReg) >> ((LL_SPI_GetBaudRatePrescaler(
                                                      pReg) >> SPI_CR1_BR_Pos) + 1);
#else
    pDevice->frequencyHertz = getClock(pReg) >> ((LL_SPI_GetBaudRatePrescaler(
                                                      pReg) >> SPI_CFG1_MBR_Pos) + 1);
#endif
    if (LL_SPI_GetClockPolarity(pReg) == LL_SPI_POLARITY_HIGH) {
        pDevice->mode |= U_COMMON_SPI_MODE_CPOL_BIT_MASK;
    }
    if (LL_SPI_GetClockPhase(pReg) == LL_SPI_PHASE_2EDGE) {
        pDevice->mode |= U_COMMON_SPI_MODE_CPHA_BIT_MASK;
    }
    pDevice->wordSizeBytes = 1;
    if (LL_SPI_GetDataWidth(pReg) == LL_SPI_DATAWIDTH_16BIT) {
        pDevice->wordSizeBytes++;
    }
    if (LL_SPI_GetTransferBitOrder(pReg) == LL_SPI_LSB_FIRST) {
        pDevice->lsbFirst = true;
    }
    pDevice->fillWord = gSpiData[spi].fillWord;
}

// Compare fill words, doing it for the right word length.
static bool fillWordIsDifferent(uint16_t wordA, uint16_t wordB, size_t lengthBytes)
{
    bool isDifferent = true;

    switch (lengthBytes) {
        case 1:
            isDifferent = !((wordA & 0xFF) == (wordB & 0xFF));
            break;
        case 2:
            isDifferent = !((wordA & 0xFFFF) == (wordB & 0xFFFF));
            break;
        default:
            break;
    }

    return isDifferent;
}

// Determine if the configuration in pDevice differs from the given one.
static bool configIsDifferent(int32_t spi,
                              const uCommonSpiControllerDevice_t *pDevice)
{
    uCommonSpiControllerDevice_t deviceCurrent;

    getDevice(spi, &deviceCurrent);

    return (deviceCurrent.pinSelect != pDevice->pinSelect) ||
           (deviceCurrent.frequencyHertz != pDevice->frequencyHertz) ||
           (deviceCurrent.mode != pDevice->mode) ||
           (deviceCurrent.wordSizeBytes != pDevice->wordSizeBytes) ||
           (deviceCurrent.lsbFirst != pDevice->lsbFirst) ||
           fillWordIsDifferent((uint16_t) deviceCurrent.fillWord,
                               (uint16_t) pDevice->fillWord,
                               pDevice->wordSizeBytes);
}

// Close an SPI instance.
static void closeSpi(uPortSpiData_t *pSpiData)
{
    SPI_TypeDef *pReg = pSpiData->pReg;

    if (pReg != NULL) {
        // Disable the SPI block
        LL_SPI_Disable(pReg);
        // Stop the bus
        clockDisable(pReg);
        // Set the register in the entry to NULL to indicate that it is
        // no longer in use
        pSpiData->pReg = NULL;
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
    }
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
                gSpiData[x].pReg = NULL;
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

        // Free the mutex so that we can delete it
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
    SPI_TypeDef *pReg;
    uCommonSpiControllerDevice_t device = U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS(-1);
    int32_t configOutcome;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their SPIs from 1
        if ((spi > 0) && (spi < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[spi].pReg == NULL) && controller &&
            (((pinMosi >= 0) || (pinMiso >= 0)) && (pinClk >= 0))) {
            pReg = gpSpiReg[spi];

            // Enable the clocks to the APB bus for this SPI
            handleOrErrorCode = clockEnable(pReg);
            if (handleOrErrorCode >= 0) {
                handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

                // Enable clock to the registers for the pins
                uPortPrivateGpioEnableClock(pinClk);
                if (pinMosi >= 0) {
                    uPortPrivateGpioEnableClock(pinMosi);
                }
                if (pinMiso >= 0) {
                    uPortPrivateGpioEnableClock(pinMiso);
                }
                // Unlike the I2C case, the GPIOs for SPI aren't always
                // on the same port, hence we initialise each separately
                configOutcome = initGpio(spi, pinClk, U_PORT_SPI_PIN_TYPE_CLK);
                if ((configOutcome == SUCCESS) && (pinMosi >= 0)) {
                    configOutcome = initGpio(spi, pinMosi, U_PORT_SPI_PIN_TYPE_MOSI);
                }
                if ((configOutcome == SUCCESS) && (pinMiso >= 0)) {
                    configOutcome = initGpio(spi, pinMiso, U_PORT_SPI_PIN_TYPE_MISO);
                }

                // Configure the SPI registers
                if ((configOutcome == SUCCESS) &&
                    (configureSpi(pReg, &device, pinMosi) == 0)) {
                    // All good, store the configuration
                    gSpiData[spi].pinMosi = pinMosi;
                    gSpiData[spi].pinMiso = pinMiso;
                    gSpiData[spi].pinSelect = device.pinSelect;
                    gSpiData[spi].fillWord = (uint16_t) device.fillWord;
                    // Now we're good to go
                    gSpiData[spi].pReg = pReg;
                    U_ATOMIC_INCREMENT(&gResourceAllocCount);
                    // Return the SPI HW block number as the handle
                    handleOrErrorCode = spi;
                } else {
                    // Put the bus back to sleep on error
                    clockDisable(pReg);
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
    // > 0 rather than >= 0 below 'cos ST number their SPIs from 1
    if ((gMutex != NULL) && (handle > 0) &&
        (handle < sizeof(gSpiData) / sizeof(gSpiData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        closeSpi(&(gSpiData[handle]));

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t configOutcome = SUCCESS;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].pReg != NULL) && (pDevice != NULL) &&
            ((pDevice->pinSelect < 0) ||
             ((pDevice->pinSelect & U_COMMON_SPI_PIN_SELECT_INVERTED) != U_COMMON_SPI_PIN_SELECT_INVERTED))) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (configIsDifferent(handle, pDevice)) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // If the configuration we're given is not the same as
                // the current one, sort the new configuration, starting
                // with the select pin, if there is one
                if (pDevice->pinSelect >= 0) {
                    uPortPrivateGpioEnableClock(pDevice->pinSelect);
                    configOutcome = initGpio(handle, pDevice->pinSelect,
                                             U_PORT_SPI_PIN_TYPE_SELECT);
                }
                if ((configOutcome == SUCCESS) &&
                    (configureSpi(gSpiData[handle].pReg, pDevice, 0) == 0)) {
                    // All good, store the new configuration
                    gSpiData[handle].pinSelect = pDevice->pinSelect;
                    gSpiData[handle].fillWord = (uint16_t) pDevice->fillWord;
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
            (gSpiData[handle].pReg != NULL) && (pDevice != NULL)) {
            getDevice(handle, pDevice);
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
    SPI_TypeDef *pReg;
    bool reverseBytes = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].pReg != NULL) &&
            (bytesToSendAndReceive <= sizeof(value))) {
            pReg = gpSpiReg[handle];

            // Need to perform byte reversal if the length of the word we
            // are sending is greater than one byte, if there is a
            // mismatch between the endianness of this processor and the
            // endianness of bit-transmission, and it will only work
            // if the word length is set to 1
            reverseBytes = ((bytesToSendAndReceive > 1) &&
                            (((LL_SPI_GetTransferBitOrder(pReg) == LL_SPI_LSB_FIRST) !=
                              U_PORT_IS_LITTLE_ENDIAN)) &&
                            (LL_SPI_GetDataWidth(pReg) == LL_SPI_DATAWIDTH_8BIT));

            if (reverseBytes) {
                U_PORT_BYTE_REVERSE(value, bytesToSendAndReceive);
            }

            transfer(handle, (const char *) &value, bytesToSendAndReceive,
                     (char *) &valueReceived, bytesToSendAndReceive);

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

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gSpiData) / sizeof(gSpiData[0])) &&
            (gSpiData[handle].pReg != NULL) &&
            ((gSpiData[handle].pinMosi >= 0) || (bytesToSend == 0)) &&
            ((gSpiData[handle].pinMiso >= 0) || (bytesToReceive == 0))) {

            errorCodeOrReceiveSize = transfer(handle, pSend, bytesToSend, pReceive, bytesToReceive);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrReceiveSize;
}

// Get the number of SPI interfaces currently open.
int32_t uPortSpiResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
