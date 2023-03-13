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

#ifndef _U_CFG_APP_PLATFORM_SPECIFIC_H_
#define _U_CFG_APP_PLATFORM_SPECIFIC_H_

/* This inclusion is required to get the UART CTS/RTS pin assignments
 * from the Zephyr device tree.
 */
#include "devicetree.h"

/* This inclusion is required to get the Zephyr version.
 */
#include "version.h"

/** @file
 * @brief This header file contains configuration information for
 * a Zephyr platform that is fed in at application level, with one
 * exception (see below).  It assumes an nRF5x MCU, e.g. nRF52840 or
 * nRF5340. You should override these values as necessary for your particular
 * platform.
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
 *
 * EXCEPTION: U_CFG_APP_PIN_CELL_DTR is a compile-time configuration
 * built into this code rather than passed in as a run-time parameter.
 * It is required where a cellular module is connected which must be
 * allowed to enter power saving mode and, also, HW flow control is
 * required on the UART interface to that cellular module. See the notes
 * above the pin definition below for further details.
 */

/** Required for Zephyr device tree query:
 * Since U_CFG_APP_CELL_UART is a macro, DT_CAT() won't work on it
 * directly, it needs this intermediate to cause U_CFG_TEST_UART_A
 * to be expand first, e.g. if U_CFG_APP_CELL_UART is defined as 1
 * then U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART) spits out uart1.
 */
#define U_CFG_APP_CAT(a, b) DT_CAT(a, b)

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

#if defined(CONFIG_BOARD_UBX_EVKNORAB1_NRF5340_CPUAPP) || \
    defined(CONFIG_BOARD_NRF5340PDK_NRF5340_CPUAPP)    || \
    defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
/** The UARTE HW block to use inside the NRF53 chip when
 * communicating with a cellular module.
 * NOTE: this used to be 1 however, with I2C added, which has to
 * be on 1 because that's the only I2C port that has DMA, it was
 * moved to 2, but then we added SPI and that is best off using
 * the exising Zephyr-mapped SPI 2 so we have bumped it to 3 as
 * you can't have I2C/SPI and UART on the same HW block and there
 * are more UARTs available on NRF53.
 */
# ifndef U_CFG_APP_CELL_UART
#  define U_CFG_APP_CELL_UART       3
# endif
#else
/** The UARTE HW block to use inside the NRF52 chip or on Linux
 * when communicating with a cellular module.
 */
# ifndef U_CFG_APP_CELL_UART
#  define U_CFG_APP_CELL_UART       1
# endif
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
/** The NRF5x GPIO output that is connected to the PWR_ON
 * pin of the cellular module.  Note that if you are using
 * the Nordic NRF5340 DK board configuration and the version of
 * Zephry that comes with NCS from about v1.6.1 then Zephyr
 * use this pin for something and hence it is better to change
 * it, e.g. to pin 36 (AKA 1.04).
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            33 // AKA 1.01
#endif

#ifndef U_CFG_APP_PIN_CELL_RESET
/** The NRF5x GPIO output that is connected to the reset
 * pin of the cellular module; use -1 where there is no such
 * connection.
 */
# define U_CFG_APP_PIN_CELL_RESET             -1
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The NRF5x GPIO input that is connected to the VInt pin of
 * the cellular module.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_VINT              -1
#endif

/** The Zephyr platform does not permit temporary suspension
 * of CTS flow control, which causes a problem when employing
 * power saving on some (e.g. SARA-R5) cellular modules as
 * the module's flow control line floats high during sleep,
 * preventing the wake-up character being sent to the module
 * by this MCU to get it out of sleep mode.  So, for this
 * platform, if HW flow control is required and cellular module
 * sleep is also required, the DTR pin of the module should
 * be connected to this MCU and its value should be set here,
 * i.e. this is a compile-time configuration for ubxlib.
 * NOTE: the DTR pin CANNOT be used to wake up a SARA-R4 cellular
 * module but that is fine as SARA-R4 holds CTS low during sleep.
 * and hence a wake-up character can always be sent.  Putting
 * it another way, do NOT set U_CFG_APP_PIN_CELL_DTR to a
 * non-negative value if you're using power saving with a
 * SARA-R4 cellular module, it will work without it.
 */
#ifndef U_CFG_APP_PIN_CELL_DTR
# define U_CFG_APP_PIN_CELL_DTR               -1
#endif

/* IMPORTANT: the UART pins given here are required for compilation
 * but make NO DIFFERENCE WHATSOEVER to how the world works.  On this
 * platform the Zephyr device tree dictates what pins are used
 * by the UART.
 *
 * ZEPHYR VERSION 3: the _GET macros are present here for the
 * flow control lines so that one has a chance of finding out,
 * in the TEST code, what pins have ended up being assigned to what
 * functions.  However, in Zephyr 3, an abstraction (pinctrl) has
 * been introduced to allow the pins to be set at run-time and,
 * while that is a good thing, it has made it impossible, now,
 * to determine what pin has ended up being assigned to what HW
 * block function either at compile time or at run-time.  Sorry!
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

#if KERNEL_VERSION_MAJOR < 3
/** Macro to return the CTS pin for cellular: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
# if (U_CFG_APP_CELL_UART < 0)
#  define U_CFG_APP_PIN_CELL_CTS_GET -1
# else
#  if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), cts_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), cts_pin) < 0xffffffff)
#   define U_CFG_APP_PIN_CELL_CTS_GET DT_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), cts_pin)
#  else
#   define U_CFG_APP_PIN_CELL_CTS_GET -1
#  endif
# endif
#endif

#if KERNEL_VERSION_MAJOR < 3
/** Macro to return the RTS pin for cellular: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
# if (U_CFG_APP_CELL_UART < 0)
#  define U_CFG_APP_PIN_CELL_RTS_GET -1
# else
#  if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), rts_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), rts_pin) < 0xffffffff)
#   define U_CFG_APP_PIN_CELL_RTS_GET DT_PROP(DT_NODELABEL(U_CFG_APP_CAT(uart, U_CFG_APP_CELL_UART)), rts_pin)
#  else
#   define U_CFG_APP_PIN_CELL_RTS_GET -1
#  endif
# endif
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

#ifndef U_CFG_APP_GNSS_I2C
/** The I2C HW block to use inside the NRF5x chip to communicate
 * with a GNSS module.  If this is required, please use number 1
 * as that is the one that the NRFx drivers used by Zephyr supports.
 * You will also need to set the following in your prj.cnf file:
 *
 * CONFIG_I2C=y
 */
# define U_CFG_APP_GNSS_I2C                  -1
#endif

#ifndef U_CFG_APP_GNSS_SPI
/** The SPI HW block to use inside the NRF5x chip to communicate
 * with a GNSS module.  NRF53 has up to five SPIs while NRF52 has
 * four; SPI 0 cannot be used at the same time as I2C 0 and there
 * are other restrictions, for instance SPI 2 and UART 2 can't
 * be used at the same time, etc.  In the default Zephyr NRF5340DK
 * board configuration SPI 2 is brought out on pins P1.13 [45, MOSI],
 * P1.14 [46, MISO] and P1.15 [47, SCLK], making it a good choice,
 * but in the default Zephyr NRF52840DK board configuration this is
 * the SPI port that is by default electrically connected to the
 * on-board flash chip. Hence, in order to avoid cutting/shorting
 * jumpers and to still allow I2C 1 to be used at the same time,
 * the overlay files used by the runner build here for NRF52840
 * adds SPI 3, coming out on pins P0.26 [MOSI], P0.27 [MISO] and
 * P0.28 [SCLK] (though note that P0.26 and P0.27 are the default
 * pins for I2C 0, which is OK since we're not using it).
 *
 * You will also need to set the following in your prj.cnf file:
 *
 * CONFIG_SPI=y
 */
# define U_CFG_APP_GNSS_SPI                  -1
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

/* IMPORTANT: the UART/I2C pins given here are required for compilation
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

#ifndef U_CFG_APP_PIN_GNSS_SDA
# define U_CFG_APP_PIN_GNSS_SDA               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SCL
# define U_CFG_APP_PIN_GNSS_SCL               -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_MOSI
# define U_CFG_APP_PIN_GNSS_SPI_MOSI          -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_MISO
# define U_CFG_APP_PIN_GNSS_SPI_MISO          -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_CLK
# define U_CFG_APP_PIN_GNSS_SPI_CLK           -1
#endif

#ifndef U_CFG_APP_PIN_GNSS_SPI_SELECT
# define U_CFG_APP_PIN_GNSS_SPI_SELECT        -1
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
