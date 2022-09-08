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
 * the Windows platform that is fed in at application level.  On
 * Windows many of the values are irrelevant, e.g. processor pin
 * numbers are not required.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON WINDOWS: MISC
 * -------------------------------------------------------------- */

/** COM port for a connected short range module; e.g. to use COM1
 * set this to 1.  Specify -1 where there is no such connection.
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
 * COMPILE-TIME MACROS FOR WINDOWS: PINS FOR BLE/WIFI (SHORT_RANGE)
 * -------------------------------------------------------------- */

/** Tx pin for UART connected to short range module;
 * not relevant for Windows and so set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD
# define U_CFG_APP_PIN_SHORT_RANGE_TXD   -1
#endif

/** Rx pin for UART connected to short range module;
 * not relevant for Windows and so set to -1.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD
# define U_CFG_APP_PIN_SHORT_RANGE_RXD   -1
#endif

/** CTS pin for UART connected to short range module;
 * on Windows this simply serves as a "disable/enable" CTS
 * flow control flag, negative for disable, else enable.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS
# define U_CFG_APP_PIN_SHORT_RANGE_CTS   -1
#endif

/** RTS pin for UART connected to short range module;
 * on Windows this simply serves as a "disable/enable" RTS
 * flow control flag, negative for disable, else enable.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS
# define U_CFG_APP_PIN_SHORT_RANGE_RTS   -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON WINDOWS: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The COM port used to communicate with a cellular module; e.g.
 * to use COM1 set this to 1.  Specify -1 where there is no such
 * connection.
 */
# define U_CFG_APP_CELL_UART             -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR WINDOWS: PINS FOR CELLULAR
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The GPIO output that enables power to the cellular module;
 * not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The GPIO output that that is connected to the PWR_ON pin of the
 * cellular module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RESET
/** The GPIO output that is connected to the reset pin of the
 * cellular module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The GPIO input that is connected to the VInt pin of
 * the cellular module; not relevant for Windows and so set
 * to -1.
 */
# define U_CFG_APP_PIN_CELL_VINT              -1
#endif

#ifndef U_CFG_APP_PIN_CELL_DTR
/** The GPIO output that is connected to the DTR pin of the
 * cellular module, only required if the application is to use the
 * DTR pin to tell the module whether it is permitted to sleep.
 * -1 should be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_DTR               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_TXD
/** The GPIO output pin that sends UART data to the cellular
 * module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_CELL_TXD               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
/** The GPIO input pin that receives UART data from the
 * cellular module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_CELL_RXD               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
/** The GPIO input pin that the cellular modem will use
 * to indicate that data can be sent to it; on Windows
 * this simply serves as a "disable/enable" CTS flow
 * control flag, negative for disable, else enable.
 */
# define U_CFG_APP_PIN_CELL_CTS               0
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
/** The GPIO output pin that tells the cellular modem
 * that it can send more data; on Windows this simply
 * serves as a "disable/enable" RTS flow control flag,
 * negative for disable, else enable.
 */
# define U_CFG_APP_PIN_CELL_RTS               0
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
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON WINDOWS: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** The COM port to use for a GNSS module; e.g. to use COM1 set
 * this to 1.  Specify -1 where there is no such connection.
 */
# define U_CFG_APP_GNSS_UART                  -1
#endif

#ifndef U_CFG_APP_GNSS_I2C
/** The COM port that ends up as I2C to use for a GNSS module;
 * e.g. to use COM1 set this to 1.  Specify -1 where there is no
 * such connection.
 */
# define U_CFG_APP_GNSS_I2C                  -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON WINDOWS: PINS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** The GPIO output that that enables power to the GNSS
 * module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_TXD
/** The GPIO output pin that sends UART data to the GNSS module;
 * not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_GNSS_TXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
/** The GPIO input pin that receives UART data from the
 * GNSS module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_GNSS_RXD              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/** The GPIO input pin that the GNSS module will use to indicate
 * that data can be sent to it. This is included for consistency:
 * u-blox GNSS modules do not use HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_CTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/** The GPIO output pin that tells the GNSS module that it can
 * send more data to the host processor; this is included for
 * consistency: u-blox GNSS modules do not use HW flow control.
 */
# define U_CFG_APP_PIN_GNSS_RTS              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SDA
/** The GPIO input/output pin that is the I2C data pin to the
 * GNSS module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_GNSS_SDA              -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
/** The GPIO output pin that is the I2C clock line for the GNSS
 * module; not relevant for Windows and so set to -1.
 */
# define U_CFG_APP_PIN_GNSS_SCL              -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON WINDOWS: CELLULAR MODULE PINS
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
