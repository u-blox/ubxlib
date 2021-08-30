/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_CFG_APP_PLATFORM_SPECIFIC_H_
#define _U_CFG_APP_PLATFORM_SPECIFIC_H_

/* Only bring in #includes specifically related to running applications. */

#include "u_runner.h"

/** @file
 * @brief This header file contains configuration information for
 * a Zephyr platform that is fed in at application level.  It assumes
 * an nRF5x MCU, e.g. nRF52840 or nRF5340. You should override these
 * values as necessary for your particular platform.
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
 * In the case of Zephyr for the NRF52 and NRF53 platforms the number
 * refers to a bit-position in a register bank plus the index of that
 * register bank; you must refer to the data sheet for your chip to
 * determine which physical pin number that logical GPIO comes out on
 * (and then, if your chip is inside a u-blox module, the data sheet
 * for the u-blox module to determine what module pin number it comes
 * out on).  This is not simple!
 *
 * Specifically, there are 32 GPIO lines on each register bank,
 * referred to as a "port", and two ports, so bit 0 of port 0 is GPIO0
 * and you would refer to it as 0, bit 31 of port 0 is GPIO31 and you
 * would refer to it as 31, bit 0 of port 1 is GPIO32 and you would
 * refer to it as 32 and bit 15 of port 1 is GPIO 47 (the second port
 * is only half used), referred to as 47.
 *
 * Also, if you are using one of the DK boards from Nordic, a load of
 * the pins have pre-assigned functions so you have to read the back of
 * the PCB _very_ carefully to find any that are free.  In
 * general, port 1 is freer than port 0, hence the choices below.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A BLE/WIFI MODULE ON ZEPHYR/NRF5x: MISC
 * -------------------------------------------------------------- */

/** UART HW block with a connected short range module.
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART       -1
#endif

/** Short range module role.
 * Central: 1
 * Peripheral: 2
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        2
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR/NRF5x: PINS FOR BLE/WIFI (SHORT_RANGE)
 * -------------------------------------------------------------- */

/* IMPORTANT: the UART pins given here are required for compilation
 * but make NO DIFFERENCE WHATSOEVER to how the world works.  On this
 * platform the Zephyr device tree dictates what pins are used
 * by the UART.
 */
#ifndef U_CFG_APP_PIN_SHORT_RANGE_TXD
# define U_CFG_APP_PIN_SHORT_RANGE_TXD   -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_RXD
# define U_CFG_APP_PIN_SHORT_RANGE_RXD   -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_CTS
# define U_CFG_APP_PIN_SHORT_RANGE_CTS   -1
#endif

#ifndef U_CFG_APP_PIN_SHORT_RANGE_RTS
# define U_CFG_APP_PIN_SHORT_RANGE_RTS   -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON ZEPHYR/NRF5x: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UARTE HW block to use inside the NRF5x chip when
 * to communicate with a cellular module.
 */
# define U_CFG_APP_CELL_UART                  1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR/NRF5x: PINS FOR CELLULAR
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The NRF5x GPIO output that enables power to the cellular
 * module. -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The NRF5x GPIO output that that is connected to the PWR_ON
 * pin of the cellular module.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            33 // AKA 1.01
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The NRF5x GPIO input that is connected to the VInt pin of
 * the cellular module.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_VINT              -1
#endif

/* IMPORTANT: the UART pins given here are required for compilation
 * but make NO DIFFERENCE WHATSOEVER to how the world works.  On this
 * platform the Zephyr device tree dictates what pins are used
 * by the UART.
 */

#ifndef U_CFG_APP_PIN_CELL_TXD
# define U_CFG_APP_PIN_CELL_TXD               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RXD
# define U_CFG_APP_PIN_CELL_RXD               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_CTS
# define U_CFG_APP_PIN_CELL_CTS               -1
#endif

#ifndef U_CFG_APP_PIN_CELL_RTS
# define U_CFG_APP_PIN_CELL_RTS               -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON ZEPHYR/NRF5x: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_GNSS_UART
/** The UARTE HW block to use inside the NRF5x chip when
 * to communicate with a GNSS module.
 */
# define U_CFG_APP_GNSS_UART                  -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR ZEPHYR/NRF5x: PINS FOR GNSS
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_GNSS_ENABLE_POWER
/** The NRF5x GPIO output that that enables power to the GNSS
 * module, use -1 if there is no such control.
 */
# define U_CFG_APP_PIN_GNSS_ENABLE_POWER       -1
#endif

/* IMPORTANT: the UART pins given here are required for compilation
 * but make NO DIFFERENCE WHATSOEVER to how the world works.  On this
 * platform the Zephyr device tree dictates what pins are used
 * by the UART.
 */

#ifndef U_CFG_APP_PIN_GNSS_TXD
# define U_CFG_APP_PIN_GNSS_TXD               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RXD
# define U_CFG_APP_PIN_GNSS_RXD               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_CTS
/* u-blox GNSS modules do not use UART HW flow control. */
# define U_CFG_APP_PIN_GNSS_CTS               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_RTS
/* u-blox GNSS modules do not use UART HW flow control. */
# define U_CFG_APP_PIN_GNSS_RTS               -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A GNSS MODULE ON  ZEPHYR/NRF5x: CELLULAR MODULE PINS
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
