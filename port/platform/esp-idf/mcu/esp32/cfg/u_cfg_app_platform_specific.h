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

#ifndef _U_PORT_APP_PLATFORM_SPECIFIC_H_
#define _U_PORT_APP_PLATFORM_SPECIFIC_H_

/** @file
 * @brief This header file contains configuration information for
 * an ESP32 platform that is fed in at application level.  You should
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
 * In the ESP-IDF case the number refers to the physical pin number
 * of the ESP32 chip.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON ESP32: MISC
 * -------------------------------------------------------------- */

/** UART HW block with a connected short range module.
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART        2
#endif

/** Short range module role.
 * Central: 1
 * Peripheral: 2
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON ESP32: PINS
 * -------------------------------------------------------------- */

/** Tx pin for UART connected to short range module.  -1 should be
 * used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD
# define U_CFG_APP_PIN_SHORT_RANGE_TXD      17
#endif

/** Rx pin for UART connected to short range module.  -1 should be
 * used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD
# define U_CFG_APP_PIN_SHORT_RANGE_RXD      16
#endif

/** CTS pin for UART connected to short range module.  -1 should be
 * used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS
# define U_CFG_APP_PIN_SHORT_RANGE_CTS      -1
#endif

/** RTS pin for UART connected to short range module.  -1 should be
 * used where there is no such connection.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS
# define U_CFG_APP_PIN_SHORT_RANGE_RTS      -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON ESP32: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UART HW block to use inside the ESP32 chip to talk to a
 * cellular module.
 */
# define U_CFG_APP_CELL_UART                  1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON ESP32: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The ESP32 GPIO output that enables power to the cellular
 * module. -1 should be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER      2
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The ESP32 GPIO output that is connected to the PWR_ON
 * pin of the cellular module.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            25
#endif

#ifndef U_CFG_APP_PIN_CELL_RESET
/** The ESP32 GPIO output that is connected to the reset
 * pin of the cellular module; use -1 where there is no such
 * connection.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The ESP32 GPIO input that is connected to the VInt pin of the
 * cellular module. -1 should be used where there is no such
 * connection.
 * Note for Arduino users: we use this as the default pin for
 * VInt because it happens to be how some of our own-made
 * internal boards are wired but pin 36 is, optionally, an RTC
 * input pin when using an external oscillator on ESP32 and the
 * SDKCONFIG used when building the ESP-IDF library that comes
 * with Arduino appears to set that RTC functionality and hence,
 * in the Arduino case, you probably need to override this default
 * (i.e. wire the module's VInt output to a different ESP32 pin
 * and change this value to match), otherwise this code will
 * think that the cellular module is on when in fact it is not.
 */
# define U_CFG_APP_PIN_CELL_VINT              36
#endif

#ifndef U_CFG_APP_PIN_CELL_DTR
/** The ESP32 GPIO output that is connected to the DTR pin of the
 * cellular module, only required if the application is to use the
 * DTR pin to tell the module whether it is permitted to sleep.
 * -1 should be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_DTR         -1
#endif

#ifndef U_CFG_APP_PIN_CELL_TXD
/** The ESP32 GPIO output pin that sends UART data to the
 * cellular module.
 */
# define U_CFG_APP_PIN_CELL_TXD              4
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
/** The ESP32 GPIO input pin that receives UART data from the
 * cellular module.
 */
# define U_CFG_APP_PIN_CELL_RXD              15
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
/** The ESP32 GPIO input pin that the cellular modem will use to
 * indicate that data can be sent to it.  -1 should be used where
 * there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_CTS              -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
/** The ESP32 GPIO output pin that tells the cellular modem
 * that it can send more data to the host processor.  -1 should
 * be used where there is no such connection. If this is *not* -1
 * then be sure to set up U_CFG_HW_CELLULAR_RTS_THRESHOLD also.
 */
# define U_CFG_APP_PIN_CELL_RTS              -1
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
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON ESP32: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** The UART HW block to use inside the ESP32 chip to talk to a
 * GNSS module.
 */
# define U_CFG_APP_GNSS_UART                  -1
#endif

#ifndef U_CFG_APP_GNSS_I2C
/** The I2C HW block to use inside the ESP32 chip to communicate
 * with a GNSS module.
 */
# define U_CFG_APP_GNSS_I2C                  -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON ESP32: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** The ESP32 GPIO output that that enables power to the GNSS
 * module, use -1 if there is no such control.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_TXD
/** The ESP32 GPIO output pin that sends UART data to the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_TXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
/** The ESP32 GPIO input pin that receives UART data from the
 * GNSS module.
 */
# define U_CFG_APP_PIN_GNSS_RXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/** The ESP32 GPIO input pin that the GNSS module will use to
 * indicate that data can be sent to it.  -1 should be used where
 * there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_CTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/** The ESP32 GPIO output pin that tells the GNSS module
 * that it can send more data to the host processor.  -1 should
 * be used where there is no such connection.
 * This is included for consistency: u-blox GNSS modules do not use
 * UART HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_RTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SDA
/** The ESP32 GPIO input/output pin that is the I2C data pin;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SDA               21
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
/** The ESP32 GPIO output pin that is the I2C clock pin;
 * use -1 where there is no such connection.
 */
# define U_CFG_APP_PIN_GNSS_SCL               22
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON ESP32: CELLULAR MODULE PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_PIN_GNSS_POWER
/** Only relevant when a GNSS chip is connected via a cellular module:
 * this is the cellular module pin (i.e. not the pin of this MCU,
 * the pin of the cellular module which this MCU is using) which controls
 * power to GNSS. This is the cellular module pin number NOT the cellular
 * module GPIO number.  Use -1 if there is no such connection.
 */
# define U_CFG_APP_CELL_PIN_GNSS_POWER  23 // AKA GPIO2
#endif

#ifndef U_CFG_APP_CELL_PIN_GNSS_DATA_READY
/** Only relevant when a GNSS chip is connected via a cellular module:
 * this is the cellular module pin (i.e. not the pin of this MCU,
 * the pin of the cellular module which this MCU is using) which is
 * connected to the Data Ready signal from the GNSS chip. This is the
 * cellular module pin number NOT the cellular module GPIO number.
 * Use -1 if there is no such connection.
 */
# define U_CFG_APP_CELL_PIN_GNSS_DATA_READY  24 // AKA GPIO3
#endif

#endif // _U_PORT_APP_PLATFORM_SPECIFIC_H_

// End of file
