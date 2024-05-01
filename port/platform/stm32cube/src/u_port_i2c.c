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
 * @brief Implementation of the port I2C API for the STM32 platform.
 *
 * Note: the I2C HW block implementation between the STM32F4 and STM32U5
 * series processors is utterly different.
 */

#include "limits.h" // UINT32_MAX
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"

#include "u_timeout.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_gpio.h" // For unblocking
#include "u_port_i2c.h"

#ifdef STM32U575xx
# include "stm32u5xx_ll_bus.h"
# include "stm32u5xx_ll_gpio.h"
# include "stm32u5xx_ll_i2c.h"
# include "stm32u5xx_hal_i2c.h"
# include "i2c_timing_utility.h"
#else
# include "stm32f4xx_ll_bus.h"
# include "stm32f4xx_ll_gpio.h"
# include "stm32f4xx_ll_i2c.h"
# include "stm32f4xx_hal_i2c.h"
#endif

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
# define U_PORT_I2C_MAX_NUM 4
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

#ifdef STM32U575xx
/** The transfer size limit: only used for STM32U5.
 */
# define U_PORT_I2C_TRANSFER_LIMIT 255
#else
# define U_PORT_I2C_TRANSFER_LIMIT UINT32_MAX
#endif

/** Version of __HAL_I2C_GET_FLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#ifndef STM32U575xx
# define U_PORT_HAL_I2C_GET_FLAG(__PREG__, __FLAG__) ((((uint8_t)((__FLAG__) >> 16U)) == 0x01U) ? \
                                                      (((((__PREG__)->SR1) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET) : \
                                                      (((((__PREG__)->SR2) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET))
#else
# define U_PORT_HAL_I2C_GET_FLAG(__PREG__, __FLAG__) (((((__PREG__)->ISR) & (__FLAG__)) == (__FLAG__)) ? SET : RESET)
#endif

/** Version of __HAL_I2C_CLEAR_FLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#ifndef STM32U575xx
# define U_PORT_HAL_I2C_CLEAR_FLAG(__PREG__, __FLAG__) ((__PREG__)->SR1 = ~((__FLAG__) & I2C_FLAG_MASK))
#else
# define U_PORT_HAL_I2C_CLEAR_FLAG(__PREG__, __FLAG__) (((__PREG__)->ICR = (__FLAG__)))
#endif

/** Version of __HAL_I2C_CLEAR_ADDRFLAG that doesn't require us
 * to carry around an entire I2C_HandleTypeDef.
 */
#ifndef STM32U575xx
# define U_PORT_HAL_I2C_CLEAR_ADDRFLAG(__PREG__)  \
   do{                                            \
     __IO uint32_t tmpreg = 0x00U;                \
     tmpreg = (__PREG__)->SR1;                    \
     tmpreg = (__PREG__)->SR2;                    \
     UNUSED(tmpreg);                              \
   } while(0)
#else
# define U_PORT_HAL_I2C_CLEAR_ADDRFLAG(__PREG__) // Not used for STM32U575
#endif

/** Flag which indicates that a new TX byte can be written:
 * TXE for STM32F4 but TXIS for STM32U5 (which is NOT set
 * if there has been a NACK).
 */
#ifdef STM32U575xx
# define U_PORT_I2C_TX_FLAG I2C_FLAG_TXIS
#else
# define U_PORT_I2C_TX_FLAG I2C_FLAG_TXE
#endif

/** Write data.
 */
#ifdef STM32U575xx
# define U_PORT_I2C_WRITE_DATA(__PREG__, data) (__PREG__)->TXDR = (char) (data)
#else
# define U_PORT_I2C_WRITE_DATA(__PREG__, data) (__PREG__)->DR = (char) (data)
#endif

/** Read data.
 */
#ifdef STM32U575xx
# define U_PORT_I2C_READ_DATA(__PREG__) (char) (__PREG__)->RXDR
#else
# define U_PORT_I2C_READ_DATA(__PREG__) (char) (__PREG__)->DR
#endif

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
                                        I2C3,
#ifdef I2C4
                                        I2C4
#endif
                                       };

/** I2C device data.
 */
static uPortI2cData_t gI2cData[U_PORT_I2C_MAX_NUM + 1]; // +1 to avoid having to -1

/** Variable to keep track of the number of I2C interfaces open.
 */
static volatile int32_t gResourceAllocCount = 0;

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
#ifdef I2C4
        case 4:
            __HAL_RCC_I2C4_CLK_ENABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#endif
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
#ifdef I2C4
        case 4:
            __HAL_RCC_I2C4_CLK_DISABLE();
            errorCodeOrI2c = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
#endif
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
#ifndef STM32U575xx
    uint32_t x = I2C_FREQRANGE(pclk1);
#else
    // I2C_GetTiming() can be found in the ST-provided i2c_timing_utility.c
    uint32_t x = I2C_GetTiming(pclk1, clockHertz);
#endif
    // Disable the I2C block
    CLEAR_BIT(pReg->CR1, I2C_CR1_PE);

    // Reset it
    pReg->CR1 |= I2C_CR1_SWRST;
    pReg->CR1 &= ~I2C_CR1_SWRST;

    // Check the minimum allowed PCLK1 frequency
#ifndef STM32U575xx
    if (I2C_MIN_PCLK_FREQ(pclk1, clockHertz) == 0) {
        // Configure the frequency range
        MODIFY_REG(pReg->CR2, I2C_CR2_FREQ, x);
        // Configure rise time
        MODIFY_REG(pReg->TRISE, I2C_TRISE_TRISE, I2C_RISE_TIME(x, clockHertz));
        // Configure the speed and timing
        MODIFY_REG(pReg->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), I2C_SPEED(pclk1, clockHertz,
                                                                                   U_PORT_I2C_DUTY_CYCLE));
#else
    if (x > 0) {
        // Configure the single timing register
        pReg->TIMINGR = x;
#endif
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
    uTimeoutStart_t timeoutStart = uTimeoutStart();
    bool wait;

    while ((wait = (U_PORT_HAL_I2C_GET_FLAG(pReg, flag) != status)) &&
           !uTimeoutExpiredMs(timeoutStart, timeoutMs)) {
    }

    return !wait;
}

// Check for an ACK being sent back to us, handling the
// case that it happens to be a NACK.
static bool checkForAck(I2C_TypeDef *pReg)
{
    bool ackReceived = true;

    if (U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_AF) == SET) {
        // If there's been an acknowledgement failure,
        // give up in an organised way
#ifndef STM32U575xx
        SET_BIT(pReg->CR1, I2C_CR1_STOP);
#else
        // STM32U5 sends STOP after a NACK automagically
#endif
        U_PORT_HAL_I2C_CLEAR_FLAG(pReg, I2C_FLAG_AF);
        ackReceived = false;
    }

    return ackReceived;
}

// Wait for an address or address header or a transmit
// (depending on the flag) to be acknowledged with a timeout.
// A STOP is generated if a nack is received, true is returned
// on success.
static bool waitTransmitOk(I2C_TypeDef *pReg, uint32_t flag,
                           int32_t timeoutMs)
{
    uTimeoutStart_t timeoutStart = uTimeoutStart();
    bool wait;
    bool ackFailed = false;

    while ((wait = (U_PORT_HAL_I2C_GET_FLAG(pReg, flag) == RESET)) &&
           !uTimeoutExpiredMs(timeoutStart, timeoutMs) &&
           !ackFailed) {
        ackFailed = !checkForAck(pReg);
    }

    return !ackFailed && !wait;
}

// Send an address, which starts any message transaction from the
// controller, returning zero on success else negative error code.
// Note: this is essentially what I2C_MasterRequestWrite()/
// I2C_MasterRequestRead() do in the original STM32F4 code or
// what I2C_TransferConfig() does in the original STM32U5 code.
static int32_t sendAddress(I2C_TypeDef *pReg, uint16_t address,
                           int32_t timeoutMs, bool readNotWrite,
                           bool *pIgnoreBusy, size_t size,
                           bool noStop)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;

    // Wait until the BUSY flag is reset, if required
    if (*pIgnoreBusy || waitFlagOk(pReg, I2C_FLAG_BUSY, RESET, timeoutMs)) {
        *pIgnoreBusy = false;
#ifndef STM32U575xx
        // The STM32F4 version is quite complex because each element
        // of the address has to be transmitted separately
        bool keepGoing = true;
        (void) size; // Not required in the STM32F4 case
        (void) noStop; // Not required in the STM32F4 case
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
#else // #ifndef STM32U575xx
        // The STM32U5 version involves setting up CR2 and that's
        // pretty much it, a transfer is left to fly after that
        uTimeoutStart_t timeoutStart = uTimeoutStart();
        uint32_t cr2 = I2C_CR2_START;
        // Deal with address length
        if (address > 127) {
            cr2 |= I2C_CR2_ADD10;
        } else {
            address <<= 1;
        }
        // TODO: I _think_ this handles 10 bit addresses correctly,
        // at least it does no less than the ST LL code does, but
        // there is a HEAD10R bit in CR2 which it _might_ be
        // necessary to do something with for correct 10-bit address
        // mode read-direction operation; we have nothing to test
        // 10-bit address mode operation against so it is not
        // possible to tell.
        cr2 |= address & I2C_CR2_SADD;
        if (readNotWrite) {
            cr2 |= I2C_CR2_RD_WRN;
        }
        // Indicate the length
        cr2 |= (size << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES;
        if (!noStop) {
            cr2 |= I2C_CR2_AUTOEND;
        }
        // Clear any stop condition that might have
        // been flagged previously
        U_PORT_HAL_I2C_CLEAR_FLAG(pReg, I2C_FLAG_STOPF);
        // Set CR2 to the value we've assembled
        pReg->CR2 = cr2;
        // For STM32U5 the TXE (and TXIS) flags are not involved in
        // the transmission of the address, they are only associated
        // with the activity of the TXDR register, so instead we
        // wait for the START flag in CR2 to be cleared, which the
        // HW does when the address has been sent, and then check
        // whether we've got an ACK for that.
        while ((pReg->CR2 & I2C_CR2_START) &&
               !uTimeoutExpiredMs(timeoutStart, timeoutMs)) {
        }
        if ((pReg->CR2 & I2C_CR2_START) == 0) {
            // Address was sent: check for ACK
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
            if (checkForAck(pReg)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
#endif // #ifndef STM32U575xx
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
    size_t bytesToSend = size;

    errorCode = sendAddress(pReg, address, timeoutMs, false, pIgnoreBusy, size, noStop);
    if (errorCode == 0) {
        // Clear the ADDR flag (only amounts to anything for STM32F4)
        U_PORT_HAL_I2C_CLEAR_ADDRFLAG(pReg);
        // Now send the data
        while ((bytesToSend > 0) && (errorCode == 0)) {
            if (waitTransmitOk(pReg, U_PORT_I2C_TX_FLAG, timeoutMs)) {
                // Write a byte
                U_PORT_I2C_WRITE_DATA(pReg, *pData);
                pData++;
                bytesToSend--;
#ifndef STM32U575xx
                // The weirdo BTF stuff, only relevant to STM32F4
                if ((U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_BTF) == SET) && (bytesToSend > 0)) {
                    // Write another byte
                    pReg->DR = *pData;
                    pData++;
                    bytesToSend--;
                }
                // Wait for BTF flag to be set
                if (!waitTransmitOk(pReg, I2C_FLAG_BTF, timeoutMs)) {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
                }
#endif
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
            }
        }
#ifdef STM32U575xx
        // On STM32U5, if we have set "no stop", we need to wait for
        // transmission to complete
        if (noStop && (size > 0) && (errorCode == 0) &&
            !waitTransmitOk(pReg, I2C_FLAG_TC, timeoutMs)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
        }
#endif

        if ((errorCode == 0) && !noStop) {
#ifndef STM32U575xx
            // Generate stop
            SET_BIT(pReg->CR1, I2C_CR1_STOP);
#else
            // For STM32U5 stop is automatically generated, set up when
            // when we called sendAddress(), we just need to wait for
            // it to finish.
            if (!waitTransmitOk(pReg, I2C_FLAG_STOPF, timeoutMs)) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
            }
            U_PORT_HAL_I2C_CLEAR_FLAG(pReg, I2C_FLAG_STOPF);
#endif
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

    errorCodeOrLength = sendAddress(pReg, address, timeoutMs, true, pIgnoreBusy, size, false);
    if (errorCodeOrLength == 0) {
        // The only thing that can go wrong from here on is a timeout
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
#ifndef STM32U575xx
        // All this complexity is only required for STM32F4
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
#endif // #ifndef STM32U575xx
        while ((bytesToReceive > 0) && keepGoing) {
#ifndef STM32U575xx
            // This rather mad BTF stuff only applies for STM32F4
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
#endif // #ifndef STM32U575xx
                // For both STM32F4 and STM32U5, wait until the
                // RXNE flag is set
                keepGoing = waitFlagOk(pReg, I2C_FLAG_RXNE, SET, timeoutMs);
                if (keepGoing) {
                    // Read the data
                    *pData = U_PORT_I2C_READ_DATA(pReg);
                    pData++;
                    bytesToReceive--;
                }
#ifndef STM32U575xx
                // More BTF stuff, STM32F4 only
                if (U_PORT_HAL_I2C_GET_FLAG(pReg, I2C_FLAG_BTF) == SET) {
                    // Read the data from DR
                    *pData = (char) pReg->DR;
                    pData++;
                    bytesToReceive--;
                }
            } // YES, this _IS_ meant to be within the #ifndef
#endif
        }

#ifdef STM32U575xx
        // For STM32U5, send STOPF if we finished early, and then
        // wait for it (in the normal course of things it is set
        // automatically)
        if (!keepGoing) {
            pReg->CR2 = I2C_FLAG_STOPF;
        }
        waitFlagOk(pReg, I2C_FLAG_STOPF, SET, timeoutMs);
        U_PORT_HAL_I2C_CLEAR_FLAG(pReg, I2C_FLAG_STOPF);
#endif

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
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
    }
}

// Our bus recovery function needs a short delay, of the order of
// 10 microseconds, which the STM32 HAL doesn't have a function for,
// so here we just do 125 increments which, with a core clock of
// 168 MHz, should be somewhere around that.
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
        // > 0 rather than >= 0 below 'cos ST number their SPIs from 1
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
                    // Note: we used to set the speed to LL_GPIO_SPEED_FREQ_VERY_HIGH
                    // but that seemed to cause significant comms failures; setting
                    // the speed to low (up to 8 MHz) is more reliable and perfectly
                    // sufficient for what is needed here
                    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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
                    U_ATOMIC_INCREMENT(&gResourceAllocCount);
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
int32_t uPortI2cControllerExchange(int32_t handle, uint16_t address,
                                   const char *pSend, size_t bytesToSend,
                                   char *pReceive, size_t bytesToReceive,
                                   bool noInterveningStop)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    I2C_TypeDef *pReg;
    size_t size;
    int32_t x;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {

            pReg = gI2cData[handle].pReg;

            // A do()/while() loop so that we can send zero bytes
            do {
                size = bytesToSend;
                if (size > U_PORT_I2C_TRANSFER_LIMIT) {
                    size = U_PORT_I2C_TRANSFER_LIMIT;
                }
                errorCodeOrLength = send(gI2cData[handle].pReg, address, pSend, size,
                                         gI2cData[handle].timeoutMs, noInterveningStop,
                                         &(gI2cData[handle].ignoreBusy));
                if (errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    bytesToSend -= size;
                    pSend += size;
                }
            } while ((bytesToSend > 0) &&
                     (errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS));

            if ((errorCodeOrLength == 0) && noInterveningStop) {
                // Ignore the busy flag next since we haven't sent a stop
                gI2cData[handle].ignoreBusy = true;
            }

            while ((bytesToReceive > 0) && (errorCodeOrLength >= 0)) {
                size = bytesToReceive;
                if (size > U_PORT_I2C_TRANSFER_LIMIT) {
                    size = U_PORT_I2C_TRANSFER_LIMIT;
                }
                x = receive(pReg, address, pReceive, size,
                            gI2cData[handle].timeoutMs,
                            &(gI2cData[handle].ignoreBusy));
                if (x >= 0) {
                    bytesToReceive -= x;
                    pReceive += x;
                    errorCodeOrLength += x;
                } else {
                    errorCodeOrLength = x;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

/** \deprecated, and not supported for STM32U5, please use
 * uPortI2cControllerExchange() instead. */
// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    I2C_TypeDef *pReg;
    size_t size;
    int32_t x;

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
            while ((bytesToSend > 0) &&
                   (errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS)) {
                size = bytesToSend;
                if (size > U_PORT_I2C_TRANSFER_LIMIT) {
                    size = U_PORT_I2C_TRANSFER_LIMIT;
                }
                errorCodeOrLength = send(pReg, address, pSend, size,
                                         gI2cData[handle].timeoutMs, false,
                                         &(gI2cData[handle].ignoreBusy));
                if (errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    bytesToSend -= size;
                }
            }
            while ((bytesToReceive > 0) && (errorCodeOrLength >= 0)) {
                size = bytesToReceive;
                if (size > U_PORT_I2C_TRANSFER_LIMIT) {
                    size = U_PORT_I2C_TRANSFER_LIMIT;
                }
                x = receive(pReg, address, pReceive, size,
                            gI2cData[handle].timeoutMs,
                            &(gI2cData[handle].ignoreBusy));
                if (x >= 0) {
                    bytesToReceive -= x;
                    errorCodeOrLength += x;
                } else {
                    errorCodeOrLength = x;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

/** \deprecated, and not supported for STM32U5, please use
 * uPortI2cControllerExchange() instead. */
// Perform a send over the I2C interface as a controller.
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    size_t size;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // > 0 rather than >= 0 below 'cos ST number their UARTs from 1
        if ((handle > 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].pReg != NULL) &&
            ((pSend != NULL) || (bytesToSend == 0))) {
            // A do()/while() loop so that we can send zero bytes
            do {
                size = bytesToSend;
                if (size > U_PORT_I2C_TRANSFER_LIMIT) {
                    size = U_PORT_I2C_TRANSFER_LIMIT;
                }
                errorCode = send(gI2cData[handle].pReg, address, pSend, size,
                                 gI2cData[handle].timeoutMs, noStop,
                                 &(gI2cData[handle].ignoreBusy));
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    bytesToSend -= size;
                }
            } while ((bytesToSend > 0) &&
                     (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS));
            if ((errorCode == 0) && noStop) {
                // Ignore the busy flag next time since we haven't sent a stop
                gI2cData[handle].ignoreBusy = true;
            }
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
