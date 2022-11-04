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
 * @brief Implementation of the port I2C API for the STM32F4 platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h" // For unblocking
#include "u_port_i2c.h"

#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_i2c.h"
#include "stm32f4xx_hal_i2c.h"

#include "cmsis_os.h"

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

/* This code uses the LL API as otherwise we have to keep
 * an entire structure of type I2C_HandleTypeDef in memory
 * for no very good reason.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_I2C_MAX_NUM
/** The number of I2C HW blocks that are available.
 */
# define U_PORT_I2C_MAX_NUM 3
#endif

#ifdef U_PORT_I2C_FAST_MODE_DUTY_CYCLE_OFFSET
/** Use a 16/9 low/high duty cycle to give the peripheral
 * longer to read the value set by the master after SCL
 * rises.  Only has an effect at 400 kHz clock.
 */
# define U_PORT_I2C_DUTY_CYCLE LL_I2C_DUTYCYCLE_16_9
#else
/** Use a normal 50% duty cycle.
 */
# define U_PORT_I2C_DUTY_CYCLE LL_I2C_DUTYCYCLE_2
#endif

/** Version of __HAL_I2C_GET_FLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#define U_PORT_HAL_I2C_GET_FLAG(__PREG__, __FLAG__) ((((uint8_t)((__FLAG__) >> 16U)) == 0x01U) ? \
                                                     (((((__PREG__)->SR1) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET) : \
                                                     (((((__PREG__)->SR2) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET))

/** Version of __HAL_I2C_CLEAR_FLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#define U_PORT_HAL_I2C_CLEAR_FLAG(__PREG__, __FLAG__) ((__PREG__)->SR1 = ~((__FLAG__) & I2C_FLAG_MASK))

/** Version of __HAL_I2C_CLEAR_ADDRFLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#define U_PORT_HAL_I2C_CLEAR_ADDRFLAG(__PREG__)  \
  do{                                            \
    __IO uint32_t tmpreg = 0x00U;                \
    tmpreg = (__PREG__)->SR1;                    \
    tmpreg = (__PREG__)->SR2;                    \
    UNUSED(tmpreg);                              \
  } while(0)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per I2C instance.
 */
typedef struct {
    I2C_TypeDef *pReg;
    int32_t clockHertz;
    int32_t timeoutMs;
    int32_t pinSda; // Need to remember these in order to perform
    int32_t pinSdc; // bus recovery
    bool ignoreBusy;
    bool adopted;
} uPortI2cData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Table of the HW addresses for each I2C block.
 */
static I2C_TypeDef *const gpI2cReg[] = {NULL,  // This to avoid having to -1
                                        I2C1,
                                        I2C2,
                                        I2C3
                                       };

/** I2C device data.
 */
static uPortI2cData_t gI2cData[U_PORT_I2C_MAX_NUM + 1]; // +1 to avoid having to -1

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the I2C number from a register address.
static int32_t getI2c(I2C_TypeDef *pReg)
{
    int32_t errorCodeOrI2c = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Start at 1 below 'cos the first entry in gpI2cReg is empty
    for (size_t x = 1; (x < sizeof(gpI2cReg) / sizeof (gpI2cReg[0])) &&
         (errorCodeOrI2c < 0); x++) {
        if (gpI2cReg[x] == pReg) {
            errorCodeOrI2c = x;
        }
    }

    return errorCodeOrI2c;
}

// Enable clock to an I2C block; these are macros so can't be
// entries in a table.
static int32_t clockEnable(I2C_TypeDef *pReg)
{
    int32_t errorCodeOrI2c = getI2c(pReg);

    switch (errorCodeOrI2c) {
        case 1:
            __HAL_RCC_I2C1_CLK_ENABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 2:
            __HAL_RCC_I2C2_CLK_ENABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 3:
            __HAL_RCC_I2C3_CLK_ENABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        default:
            break;
    }

    return errorCodeOrI2c;
}

// Disable clock to an I2C block; these are macros so can't be
// entries in a table.
static int32_t clockDisable(I2C_TypeDef *pReg)
{
    int32_t errorCodeOrI2c = getI2c(pReg);

    switch (errorCodeOrI2c) {
        case 1:
            __HAL_RCC_I2C1_CLK_DISABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 2:
            __HAL_RCC_I2C2_CLK_DISABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        case 3:
            __HAL_RCC_I2C3_CLK_DISABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        default:
            break;
    }

    return errorCodeOrI2c;
}

// Configure an I2C HW block; a much reduced version of HAL_I2C_Init(),
// returning zero on success else negative error code.
static int32_t configureHw(I2C_TypeDef *pReg, int32_t clockHertz)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t frequencyRange =  I2C_FREQRANGE(pclk1);

    // Disable the I2C block
    CLEAR_BIT(pReg->CR1, I2C_CR1_PE);

    // Reset it
    pReg->CR1 |= I2C_CR1_SWRST;
    pReg->CR1 &= ~I2C_CR1_SWRST;

    // Check the minimum allowed PCLK1 frequency
    if (I2C_MIN_PCLK_FREQ(pclk1, clockHertz) == 0) {
        // Configure the frequency range
        MODIFY_REG(pReg->CR2, I2C_CR2_FREQ, frequencyRange);
        // Configure rise time
        MODIFY_REG(pReg->TRISE, I2C_TRISE_TRISE, I2C_RISE_TIME(frequencyRange, clockHertz));
        // Configure the speed and timing
        MODIFY_REG(pReg->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), I2C_SPEED(pclk1, clockHertz,
                                                                                   U_PORT_I2C_DUTY_CYCLE));
        // Enable the I2C block again
        SET_BIT(pReg->CR1, I2C_CR1_PE);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Wait until the given flag is at the given state or the stop time
// has not been reached, returning true on success.
static bool waitFlagOk(I2C_TypeDef *pReg, uint32_t flag,
                       FlagStatus status, int32_t timeoutMs)
{
    int32_t startTimeMs = uPortGetTickTimeMs();
    bool wait;

    while ((wait = (U_PORT_HAL_I2C_GET_FLAG(pReg, flag) != status)) &&
           (uPortGetTickTimeMs() - startTimeMs < timeoutMs)) {
    }

    return !wait;
}

// Wait for an address or address header or a transmit
// (depending on the flag) to be acknowledged with a timeout.
// A STOP is generated if a nack is received, true is returned
// on success.
static bool waitTransmitOk(I2C_TypeDef *pReg, uint32_t flag,
                           int32_t timeoutMs)
{
    int32_t startTimeMs = uPortGetTickTimeMs();
    bool wait;
    bool ackFailed = false;

    while ((wait = (U_PORT_HAL_I2C_GET_FLAG(pReg, flag) == RESET)) &&
           (uPortGetTickTimeMs() - startTimeMs < timeoutMs) && !ackFailed) {
        if (U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_AF) == SET) {
            // If there's been an acknowledgement failure,
            // give up in an organised way
            SET_BIT(pReg->CR1, I2C_CR1_STOP);
            U_PORT_HAL_I2C_CLEAR_FLAG(pReg, I2C_FLAG_AF);
            ackFailed = true;
        }
    }

    return !ackFailed && !wait;
}

// Send an address, which starts any message transaction from the
// controller, returning zero on success else negative error code.
// Note: this is essentially what I2C_MasterRequestWrite()/
// I2C_MasterRequestRead() do in the original ST code.
static int32_t sendAddress(I2C_TypeDef *pReg, uint16_t address,
                           int32_t timeoutMs, bool readNotWrite,
                           bool *pIgnoreBusy)
{

    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
    bool keepGoing = true;

    // Wait until the BUSY flag is reset, if required
    if (*pIgnoreBusy || waitFlagOk(pReg, I2C_FLAG_BUSY, RESET, timeoutMs)) {
        *pIgnoreBusy = false;
        // Disable Pos
        CLEAR_BIT(pReg->CR1, I2C_CR1_POS);
        if (readNotWrite) {
            // Enable acknowledge
            SET_BIT(pReg->CR1, I2C_CR1_ACK);
        }
        // Generate start
        SET_BIT(pReg->CR1, I2C_CR1_START);
        // Wait until SB flag is set
        if (waitFlagOk(pReg, I2C_FLAG_SB, SET, timeoutMs)) {
            if (address > 127) {
                // Send the header for a 10-bit address with write set
                pReg->DR = I2C_10BIT_HEADER_WRITE(address);
                // Wait until ADD10 flag is set
                keepGoing = waitTransmitOk(pReg, I2C_FLAG_ADD10, timeoutMs);
                if (keepGoing) {
                    // Now send the 10-bit address
                    pReg->DR = I2C_10BIT_ADDRESS(address);
                    if (readNotWrite) {
                        // For reads on a 10-bit address there is more
                        // to do: wait until ADDR flag is set
                        keepGoing = waitTransmitOk(pReg, I2C_FLAG_ADDR, timeoutMs);
                        if (keepGoing) {
                            // Clear the ADDR flag
                            U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
                            // Generate a restart
                            SET_BIT(pReg->CR1, I2C_CR1_START);
                            // Wait until SB flag is set
                            keepGoing = waitFlagOk(pReg, I2C_FLAG_SB, SET, timeoutMs);
                            if (keepGoing) {
                                // Send the header for a 10-bit address with read set this time
                                pReg->DR = I2C_10BIT_HEADER_READ(address);
                            }
                        }
                    }
                }
            } else {
                // A 7-bit address can be sent immediately
                if (readNotWrite) {
                    pReg->DR = I2C_7BIT_ADD_READ(address << 1);
                } else {
                    pReg->DR = I2C_7BIT_ADD_WRITE(address << 1);
                }
            }
            if (keepGoing) {
                // Wait until ADDR flag is set
                errorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
                if (waitTransmitOk(pReg, I2C_FLAG_ADDR, timeoutMs)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    return errorCode;
}

// Send an I2C message; a simplified version of HAL_I2C_Master_Transmit(),
// returning zero on success else negative error code.
static int32_t send(I2C_TypeDef *pReg, uint16_t address,
                    const char *pData, size_t size,
                    int32_t timeoutMs, bool noStop,
                    bool *pIgnoreBusy)
{
    int32_t errorCode;

    errorCode = sendAddress(pReg, address, timeoutMs, false, pIgnoreBusy);
    if (errorCode == 0) {
        // Clear the ADDR flag
        U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
        // Now send the data
        while ((size > 0) && (errorCode == 0)) {
            if (waitTransmitOk(pReg, I2C_FLAG_TXE, timeoutMs)) {
                // Write a byte
                pReg->DR = *pData;
                pData++;
                size--;
                if ((U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_BTF) == SET) && (size > 0)) {
                    // Write another byte
                    pReg->DR = *pData;
                    pData++;
                    size--;
                }
                // Wait for BTF flag to be set
                if (!waitTransmitOk(pReg, I2C_FLAG_BTF, timeoutMs)) {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
                }
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
            }
        }
        if (errorCode == 0) {
            if (!noStop) {
                // Generate stop
                SET_BIT(pReg->CR1, I2C_CR1_STOP);
            }
        }
    }

    return errorCode;
}

// Receive an I2C message; a simplified version of HAL_I2C_Master_Receive(),
// returning number of bytes received on success else negative error code.
static int32_t receive(I2C_TypeDef *pReg, uint16_t address,
                       char *pData, size_t size, int32_t timeoutMs,
                       bool *pIgnoreBusy)
{
    int32_t errorCodeOrLength;
    size_t bytesToReceive = size;
    bool keepGoing = true;

    errorCodeOrLength = sendAddress(pReg, address, timeoutMs, true, pIgnoreBusy);
    if (errorCodeOrLength == 0) {
        // The only thing that can go wrong from here on is a timeout
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (bytesToReceive == 0) {
            // Clear the ADDR flag
            U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
            // Generate stop
            SET_BIT(pReg->CR1, I2C_CR1_STOP);
        } else if (bytesToReceive == 1) {
            // Disable acknowledge
            CLEAR_BIT(pReg->CR1, I2C_CR1_ACK);
            // Clear the ADDR flag
            U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
            // Generate stop
            SET_BIT(pReg->CR1, I2C_CR1_STOP);
        } else if (bytesToReceive == 2) {
            // Disable acknowledge
            CLEAR_BIT(pReg->CR1, I2C_CR1_ACK);
            // Enable Pos
            SET_BIT(pReg->CR1, I2C_CR1_POS);
            // Clear the ADDR flag
            U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
        } else {
            // Enable acknowledge
            SET_BIT(pReg->CR1, I2C_CR1_ACK);
            // Clear the ADDR flag
            U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
        }
        while ((bytesToReceive > 0) && keepGoing) {
            if (bytesToReceive <= 3) {
                if (bytesToReceive == 1) { // One byte
                    // Wait until the RXNE flag is set
                    // Note: the original ST code had a special function
                    // for this but the only special thing in it is related
                    // to slave operation (checking STOPF), which is not
                    // implemented here, hence the standard waitFlagOk()
                    // can be used.
                    keepGoing = waitFlagOk(pReg, I2C_FLAG_RXNE, SET, timeoutMs);
                    if (keepGoing) {
                        // Read the data from DR
                        *pData = (char) pReg->DR;
                        pData++;
                        bytesToReceive--;
                    }
                } else if (bytesToReceive == 2) { // Two bytes
                    // Wait until BTF flag is set
                    keepGoing = waitFlagOk(pReg, I2C_FLAG_BTF, SET, timeoutMs);
                    if (keepGoing) {
                        // Generate stop
                        SET_BIT(pReg->CR1, I2C_CR1_STOP);
                        // Read the data from DR
                        *pData = (char) pReg->DR;
                        pData++;
                        bytesToReceive--;
                        // Read the data from DR
                        *pData = (char) pReg->DR;
                        pData++;
                        bytesToReceive--;
                    }
                } else { // Last three bytes
                    // Wait until BTF flag is set
                    keepGoing = waitFlagOk(pReg, I2C_FLAG_BTF, SET, timeoutMs);
                    if (keepGoing) {
                        // Disable acknowledge
                        CLEAR_BIT(pReg->CR1, I2C_CR1_ACK);
                        // Read the data from DR
                        *pData = (char) pReg->DR;
                        pData++;
                        bytesToReceive--;
                        // Wait until BTF flag is set
                        keepGoing = waitFlagOk(pReg, I2C_FLAG_BTF, SET, timeoutMs);
                        if (keepGoing) {
                            // Generate stop
                            SET_BIT(pReg->CR1, I2C_CR1_STOP);
                            // Read the data from DR
                            *pData = (char) pReg->DR;
                            pData++;
                            bytesToReceive--;
                            // Read the data from DR
                            *pData = (char) pReg->DR;
                            pData++;
                            bytesToReceive--;
                        }
                    }
                }
            } else {
                // Wait until the RXNE flag is set
                keepGoing = waitFlagOk(pReg, I2C_FLAG_RXNE, SET, timeoutMs);
                if (keepGoing) {
                    // Read the data from DR
                    *pData = (char) pReg->DR;
                    pData++;
                    bytesToReceive--;
                }
                if (U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_BTF) == SET) {
                    // Read the data from DR
                    *pData = (char) pReg->DR;
                    pData++;
                    bytesToReceive--;
                }
            }
        }
        if (keepGoing) {
            errorCodeOrLength = (int32_t) (size - bytesToReceive);
        }
    }

    return errorCodeOrLength;
}

// Close an I2C instance.
static void closeI2c(uPortI2cData_t *pInstance)
{
    if ((pInstance != NULL) && (pInstance->pReg != NULL)) {
        if (!pInstance->adopted) {
            // Disable the I2C block
            CLEAR_BIT(pInstance->pReg->CR1, I2C_CR1_PE);
            // Stop the bus
            clockDisable(pInstance->pReg);
        }
        // Set the register to NULL to indicate that it is no longer in use
        pInstance->pReg = NULL;
    }
}

// Our bus recovery function needs a short delay, of the order of
// 10 microseconds, which the STM32 HAL doesn't have a function for,
// so here we just do 125 increments which, with a core clock of
// 168 MHz, should be somewhere around that
static void shortDelay()
{
    volatile int32_t x = 0;
    while (x < 125) {
        x++;
    }
}

// Following the advice from:
// https://www.i2c-bus.org/i2c-primer/analysing-obscure-problems/blocked-bus/
static int32_t busRecover(int32_t pinSda, int32_t pinSdc)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;

    gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT_OUTPUT;
    gpioConfig.pullMode = U_PORT_GPIO_PULL_MODE_PULL_UP;
    gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;

    gpioConfig.pin = pinSda;
    if (uPortGpioConfig(&gpioConfig) == 0) {
        gpioConfig.pin = pinSdc;
        if (uPortGpioConfig(&gpioConfig) == 0) {
            // Toggle the clock 16 times
            for (size_t x = 0; x < 16; x++) {
                uPortGpioSet(pinSdc, 0);
                shortDelay();
                uPortGpioSet(pinSdc, 1);
                shortDelay();
            }

            // Generate a stop
            uPortGpioSet(pinSda, 0);
            shortDelay();
            uPortGpioSet(pinSda, 1);

            if (uPortGpioGet(pinSda) == 1) {
                // If the SDA pin was allowed to rise, we've
                // probably succeeded
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Open an I2C instance; unlike the other static functions
// this does all the mutex locking etc.
static int32_t openI2c(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                       bool controller, bool adopt)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    LL_GPIO_InitTypeDef gpioInitStruct = {0};
    I2C_TypeDef *pReg;
    bool configurationOk = true;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((i2c > 0) && (i2c < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[i2c].pReg == NULL) && controller &&
            (adopt || ((pinSda >= 0) && (pinSdc >= 0)))) {
            pReg = gpI2cReg[i2c];
            // Enable the clocks to the bus
            handleOrErrorCode = clockEnable(pReg);
            if (handleOrErrorCode >= 0) {
                handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (!adopt) {
                    // Enable clock to the registers for the pins
                    uPortPrivateGpioEnableClock(pinSda);
                    uPortPrivateGpioEnableClock(pinSdc);
                    // The Pin field is a bitmap so we can do SDA and SCL
                    // at the same time as they are always on the same port
                    gpioInitStruct.Pin = (1U << U_PORT_STM32F4_GPIO_PIN(pinSda)) |
                                         (1U << U_PORT_STM32F4_GPIO_PIN(pinSdc));
                    gpioInitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
                    gpioInitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
                    gpioInitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
                    gpioInitStruct.Pull = LL_GPIO_PULL_UP;
                    // AF4 from the data sheet for the STM32F437VG
                    gpioInitStruct.Alternate = LL_GPIO_AF_4;
                    if ((LL_GPIO_Init(pUPortPrivateGpioGetReg(pinSda),
                                      &gpioInitStruct) != SUCCESS) ||
                        (configureHw(pReg, U_PORT_I2C_CLOCK_FREQUENCY_HERTZ) != 0)) {
                        configurationOk = false;
                    }
                }
                if (configurationOk) {
                    gI2cData[i2c].clockHertz = U_PORT_I2C_CLOCK_FREQUENCY_HERTZ;
                    gI2cData[i2c].timeoutMs = U_PORT_I2C_TIMEOUT_MILLISECONDS;
                    gI2cData[i2c].pinSda = pinSda;
                    gI2cData[i2c].pinSdc = pinSdc;
                    gI2cData[i2c].pReg = pReg;
                    gI2cData[i2c].adopted = adopt;
                    // Return the I2C HW block number as the handle
                    handleOrErrorCode = i2c;
                } else {
                    if (!adopt) {
                        // Put the bus back to sleep on error
                        clockDisable(pReg);
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {
            for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
                gI2cData[x].pReg = NULL;
                gI2cData[x].pinSda = -1;
                gI2cData[x].pinSdc = -1;
                gI2cData[x].ignoreBusy = false;
            }
        }
    }

    return errorCode;
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Shut down any open instances
        for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
            closeI2c(&gI2cData[x]);
        }

        // Free the mutex so that we can delete it
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
    // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
    if ((gMutex != NULL) && (handle > 0) &&
        (handle < sizeof(gI2cData) / sizeof(gI2cData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        closeI2c(&gI2cData[handle]);

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
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                pinSda = gI2cData[handle].pinSda;
                pinSdc = gI2cData[handle].pinSdc;
                // No longer adopted, we've been asked to fiddle
                gI2cData[handle].adopted = false;
                closeI2c(&gI2cData[handle]);
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

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (configureHw(gI2cData[handle].pReg, clockHertz) == 0) {
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
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL)) {
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
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL) && (timeoutMs > 0)) {
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
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL)) {
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
    I2C_TypeDef *pReg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            pReg = gI2cData[handle].pReg;
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pSend != NULL) {
                errorCodeOrLength = send(pReg, address, pSend, bytesToSend,
                                         gI2cData[handle].timeoutMs, false,
                                         &(gI2cData[handle].ignoreBusy));
            }
            if ((errorCodeOrLength == 0) && (pReceive != NULL)) {
                errorCodeOrLength = receive(pReg, address, pReceive, bytesToReceive,
                                            gI2cData[handle].timeoutMs,
                                            &(gI2cData[handle].ignoreBusy));
            }
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

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0))) {
            errorCode = send(gI2cData[handle].pReg, address, pSend, bytesToSend,
                             gI2cData[handle].timeoutMs, noStop,
                             &(gI2cData[handle].ignoreBusy));
            if ((errorCode == 0) && noStop) {
                // Ignore the busy flag next time since we haven't sent a stop
                gI2cData[handle].ignoreBusy = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// End of file
