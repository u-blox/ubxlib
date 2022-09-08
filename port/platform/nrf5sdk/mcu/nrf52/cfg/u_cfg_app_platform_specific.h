/*
 * Copyright 2019-2022 u-blox
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
 * an NRF52 platform that is fed in at application level.  You should
 * override these values as necessary for your particular platform.
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
 * In the NRF5 SDK the number refers to a bit-position in a register
 * bank plus the index of that register bank; you must refer to the
 * data sheet for your chip to determine which physical pin number that
 * logical GPIO comes out on (and then, if your chip is inside a u-blox
 * module, the data sheet for the u-blox module to determine what module
 * pin number it comes out on).  This is not simple!
 *
 * Specifically, there are 32 GPIO lines on each register bank,
 * referred to as a "port", and two ports, so bit 0 of port 0 is GPIO0
 * and you would refer to it as 0, bit 31 of port 0 is GPIO31 and you
 * would refer to it as 31, bit 0 of port 1 is GPIO32 and you would
 * refer to it as 32 and bit 15 of port 1 is GPIO 47 (the second port
 * is only half used), referred to as 47.
 *
 * Also, if you are using the NRF52 DK board from Nordic, a load of
 * the pins have pre-assigned functions so you have to read the back of
 * the PCB _very_ carefully to find any that are free.  In
 * general, port 1 is freer than port 0, hence the choices below.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON NRF52: MISC
 * -------------------------------------------------------------- */

/** UART HW block with a connected short range module.
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART        -1
#endif

/** Short range module role.
 * Central: 1
 * Peripheral: 2
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR NRF52: PINS FOR BLE/WIFI (SHORT_RANGE)
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
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON NRF52: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UARTE HW block to use inside the NRF52 chip when
 * to communicate with a cellular module.
 * IMPORTANT: this code provides its own UARTE driver and hence
 * the UARTE chosen here must be set to 0 in sdk_config.h so that
 * the Nordic NRF5 driver does not use it, e.g. if the
 * value of U_CFG_APP_CELL_UART is set to 0 then
 * NRFX_UARTE0_ENABLED must be set to 0 in sdk_config.h.
 */
# define U_CFG_APP_CELL_UART                 0
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR NRF52: PINS FOR CELLULAR
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The NRF52 GPIO output that enables power to the cellular
 * module. -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The NRF52 GPIO output that that is connected to the PWR_ON
 * pin of the cellular module.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            33 // AKA 1.01
#endif

#ifndef U_CFG_APP_PIN_CELL_RESET
/** The NRF52 GPIO output that is connected to the reset
 * pin of the cellular module; use -1 where there is no such
 * connection.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The NRF52 GPIO input that is connected to the VInt pin of
 * the cellular module.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_VINT              -1
#endif

#ifndef U_CFG_APP_PIN_CELL_DTR
/** The NRF52 GPIO output that is connected to the DTR pin of the
 * cellular module, only required if the application is to use the
 * DTR pin to tell the module whether it is permitted to sleep.
 * -1 should be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_DTR         -1
#endif

#ifndef U_CFG_APP_PIN_CELL_TXD
/** The NRF52 GPIO output pin that sends UART data to the
 * cellular module.
 */
# define U_CFG_APP_PIN_CELL_TXD               34 // AKA 1.02
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
/** The NRF52 GPIO input pin that receives UART data from
 * the cellular module.
 */
# define U_CFG_APP_PIN_CELL_RXD               35 // AKA 1.03
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
/** The NRF52 GPIO input pin that the cellular modem will
 * use to indicate that data can be sent to it. -1 should
 * be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_CTS               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
/** The NRF52 GPIO output pin that tells the cellular modem
 * that it can send more data to the NRF52 UART. -1 should
 * be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_RTS               -1
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
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON NRF52: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** The UART HW block to use inside the NRF52 chip to talk to a
 * GNSS module.
 */
# define U_CFG_APP_GNSS_UART                  -1
#endif

#ifndef U_CFG_APP_GNSS_I2C
/** The I2C HW block to use inside the NRF52 chip to communicate
 * with a GNSS module.  If this is required, please use number 1
 * and don't forget to enable all of the right things in your
 * sdk_config.h file (e.g. TWI_ENABLED, TWI1_ENABLED, TWI1_USE_EASY_DMA,
 * NRFX_TWI_ENABLED, NRFX_TWIM_ENABLED and NRFX_TWIM1_ENABLED).
 */
# define U_CFG_APP_GNSS_I2C                  -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON NRF52: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** The NRF52 GPIO output that that enables power to the GNSS
 * module, use -1 if there is no such control.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_TXD
/** The NRF52 GPIO output pin that sends UART data to the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_TXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
/** The NRF52 GPIO input pin that receives UART data from the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_RXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/** The NRF52 GPIO input pin that the GNSS module will use to
 * indicate that data can be sent to it.  -1 should be used where
 * there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_CTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/** The NRF52 GPIO output pin that tells the GNSS module
 * that it can send more data to the host processor.  -1 should
 * be used where there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_RTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SDA
/** The NRF52 GPIO input/output pin that is the I2C data pin;
 * use -1 where there is no such connection.  This pin chosen
 * as it is the default for the NRF52840 DK board.
 */
# define U_CFG_APP_PIN_GNSS_SDA               30 // AKA 0.30
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
/** The NRF52 GPIO output pin that is the I2C clock pin;
 * use -1 where there is no such connection.  This pin chosen
 * as it is the default for the NRF52840 DK board.
 */
# define U_CFG_APP_PIN_GNSS_SCL               31 // AKA 0.31
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON NRF52: CELLULAR MODULE PINS
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
