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

#ifndef _U_CFG_APP_PLATFORM_SPECIFIC_H_
#define _U_CFG_APP_PLATFORM_SPECIFIC_H_

/** @file
 * @brief This header file contains configuration information for
 * an STM32U5 platform that is fed in at application level.  You should
 * override these values as necessary for your particular platform.  NONE
 * of the parameters here are compiled into ubxlib itself.
 *
 * Note that the pin numbers used below should be those of the MCU: if you
 * are using an MCU inside a u-blox module the IO pin numbering for
 * the module is likely different to that from the MCU: check the data
 * sheet for the module to determine the mapping.
 *
 * Also, note that the convention used by each platform SDK for pin
 * numbering is different: some platform SDKs use physical pin numbers,
 * others a bit-position in a register bank, or sometimes a bit-position
 * in a register bank plus an index to that bank: expect no commonality!
 *
 * In the case the STM32U5 SDK the number refers to a bit-position in a
 * register bank plus the index of that register bank; you must refer to
 * the data sheet for your chip to determine which physical pin number
 * that logical GPIO comes out on.
 *
 * Specifically, the pin numbering has the bank number in the upper nibble
 * and the pin number in the lower nibble, so pin 15, also known as PA_15,
 * would be referred to as 0x0f and pin 16, also known as PB_0, would be
 * referred to as 0x10, etc.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON STM32U5: UART/USART
 * -------------------------------------------------------------- */

/** UART HW block with a connected short range module.
 * Note: make sure that the corresponding U_CFG_UARTx_AVAILABLE
 * for this UART is set to 1 in u_cfg_hw_platform_specific.h
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART        -1 //
#endif

/** Short range module role.
 * Central: 1
 * Peripheral: 2
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON STM32U5: PINS
 * -------------------------------------------------------------- */

/** Tx pin for UART connected to short range module.
 * -1 is used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD
# define U_CFG_APP_PIN_SHORT_RANGE_TXD   -1
#endif

/** Rx pin for UART connected to short range module.
 * -1 is used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD
# define U_CFG_APP_PIN_SHORT_RANGE_RXD   -1
#endif

/** CTS pin for UART connected to short range module.
 * -1 is used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS
# define U_CFG_APP_PIN_SHORT_RANGE_CTS   -1
#endif

/** RTS pin for UART connected to short range module.
 * -1 is used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS
# define U_CFG_APP_PIN_SHORT_RANGE_RTS   -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON STM32U5: UART/USART
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UART/USART/LPUART to use when talking to the cellular module,
 * a number between 0 and 5.
 */
# define U_CFG_APP_CELL_UART                   2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON STM32U5: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_RESET
/** The pin that is connect to the cellular module's
 * reset pin.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The STM32U5 GPIO output that enables power to the
 * cellular module.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER      -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The STM32U5 GPIO output that that is connected to the
 * PWR_ON pin of the cellular module.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON     0x5e // AKA PF_14 or D4 on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The STM32U5 GPIO input that is connected to the VInt
 * pin of the cellular module. -1 is used where there
 * is no such connection.
 */
# define U_CFG_APP_PIN_CELL_VINT      -1
#endif

#ifndef U_CFG_APP_PIN_CELL_DTR
/** The STM32U5 GPIO output that is connected to the DTR
 * pin of the cellular module, only required if the
 * application is to use the DTR pin to tell the module
 * whether it is permitted to sleep. -1 should be used
 * where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_DTR         -1
#endif

#ifndef U_CFG_APP_PIN_CELL_TXD
/** The STM32U5 GPIO output pin that sends UART data to
 * the cellular module.
 */
# define U_CFG_APP_PIN_CELL_TXD       0x35 // AKA PD_5 or D53 (marked "USART TX") on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
/** The STM32U5 GPIO input pin that receives UART data
 * from the cellular module.
 */
# define U_CFG_APP_PIN_CELL_RXD       0x36 // AKA PD_6 or D52 (marked "USART RX") on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
/** The STM32U5 GPIO input pin that the cellular modem
 * will use to indicate that data can be sent to it.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_CTS       -1 //
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
/** The STM32U5 GPIO output pin that tells the cellular
 * modem that it can send more data to the STM32U5 UART.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_RTS       -1 //
#endif

/** Macro to return the CTS pin for cellular: on some
 * platforms this is not a simple define.
 */
#define U_CFG_APP_PIN_CELL_CTS_GET U_CFG_APP_PIN_CELL_CTS

/** Macro to return the RTS pin for cellular: on some
 * platforms this is not a simple define.
 */
#define U_CFG_APP_PIN_CELL_RTS_GET U_CFG_APP_PIN_CELL_RTS

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON STM32U5: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** The UART HW block to use inside the STM32U5 chip to talk to a
 * GNSS module.
 */
# define U_CFG_APP_GNSS_UART                 -1
#endif

#ifndef U_CFG_APP_GNSS_I2C
/** The I2C HW block to use inside the STM32U5 chip to communicate
 * with a GNSS module.  Note that ST number their HW I2C blocks
 * starting at 1 rather than 0.
 */
# define U_CFG_APP_GNSS_I2C                  1
#endif

#ifndef U_CFG_APP_GNSS_SPI
/** The SPI HW block to use inside the STM32U5 chip to communicate
 * with a GNSS module.  Note that ST number their HW SPI blocks
 * starting at 1 rather than 0.
 */
# define U_CFG_APP_GNSS_SPI                  1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON STM32U5: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** The STM32U5 GPIO output that that enables power to the GNSS
 * module, use -1 if there is no such control.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER      -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_TXD
/** The STM32U5 GPIO output pin that sends UART data to the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_TXD               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
/** The STM32U5 GPIO input pin that receives UART data from the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_RXD               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/** The STM32U5 GPIO input pin that the GNSS module will use to
 * indicate that data can be sent to it.  -1 should be used where
 * there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_CTS               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/** The STM32U5 GPIO output pin that tells the GNSS module
 * that it can send more data to the host processor.  -1 should
 * be used where there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_RTS               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SDA
/** The STM32U5 GPIO input/output pin that is the I2C data pin;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SDA               0x19  // AKA PB_9 or D14 on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
/** The STM32U5 GPIO output pin that is the I2C clock pin;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SCL               0x18  // AKA PB_8 or D15 on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_MOSI
/** The GPIO output pin for SPI towards the GNSS module;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SPI_MOSI          0x07  // AKA PA_7 or D11 on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_MISO
/** The GPIO input pin for SPI from the GNSS module;
 * use -1 where there is no such connection.
 *
 * Note: the ubxlib test system runs tests on a NUCLEO-U575ZI-Q
 * board.  In order to test using the LPUART and avoid the application
 * having to call HAL_PWREx_EnableVddIO2() (since the usual pins
 * for the LPUART, port G, are not normally powered), it uses
 * PA_2/PA_3/PA_6/PB_1 for the LPUART pins and, unfortunately
 * PA_6 is also the MISO pin for SPI1, so here we use PE_14 instead,
 * which is an alternate (see table 27 of the STM32U575 data sheet).
 */
# define U_CFG_APP_PIN_GNSS_SPI_MISO          0x4e  // AKA PE_14 or D31, labelled "IO2", on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_CLK
/** The GPIO output pin that is the clock for SPI;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SPI_CLK           0x05 // AKA PA_5 or D13 on a NUCLEO-U575ZI-Q board
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_SELECT
/** The GPIO output pin that is the chip select for the GNSS
 * module; use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SPI_SELECT        0x10  // AKA PB_0 or D29, labelled "IO1", on a NUCLEO-U575ZI-Q board
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON STM32U5: CELLULAR MODULE PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_PIN_GNSS_POWER
/** Only relevant when a GNSS chip is connected via a cellular module:
 * this is the the cellular module pin (i.e. not the pin of this MCU,
 * the pin of the cellular module which this MCU is using) which controls
 * power to GNSS. This is the cellular module pin number NOT the cellular
 * module GPIO number.  Use -1 if there is no such connection.
 */
# define U_CFG_APP_CELL_PIN_GNSS_POWER  -1
#endif

#ifndef U_CFG_APP_CELL_PIN_GNSS_DATA_READY
/** Only relevant when a GNSS chip is connected via a cellular module:
 * this is the the cellular module pin (i.e. not the pin of this MCU,
 * the pin of the cellular module which this MCU is using) which is
 * connected to the Data Ready signal from the GNSS chip. This is the
 * cellular module pin number NOT the cellular module GPIO number.
 * Use -1 if there is no such connection.
 */
# define U_CFG_APP_CELL_PIN_GNSS_DATA_READY  -1
#endif

#endif // _U_CFG_APP_PLATFORM_SPECIFIC_H_

// End of file
