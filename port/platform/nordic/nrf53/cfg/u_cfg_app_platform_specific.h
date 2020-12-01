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
 * an NRF5340 platform that is fed in at application level.  You should
 * override these values as necessary for your particular platform.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON NRF53: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_CELL_UART
/** The UARTE HW block to use inside the NRF53 chip when
 * to communicate with a cellular module.
 */
# define U_CFG_APP_CELL_UART                 -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR NRF53: PINS FOR CELLULAR
 * -------------------------------------------------------------- */

#ifndef U_CFG_APP_PIN_CELL_ENABLE_POWER
/** The NRF53 GPIO output that enables power to the cellular
 * module. -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_ENABLE_POWER     -1
#endif

#ifndef U_CFG_APP_PIN_CELL_PWR_ON
/** The NRF53 GPIO output that that is connected to the PWR_ON
 * pin of the cellular module.
 */
# define U_CFG_APP_PIN_CELL_PWR_ON            33 // AKA 1.01
#endif

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The NRF53 GPIO input that is connected to the VInt pin of
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

/** Short range module type, see uShortRangeModuleType_t
 */
#ifndef U_CFG_APP_SHORT_RANGE_TYPE
# define U_CFG_APP_SHORT_RANGE_TYPE            2
#endif

/** Tx pin for UART connected to short range module.
 */
#ifndef U_CFG_APP_SHORT_RANGE_PIN_UART_TXD
# define U_CFG_APP_SHORT_RANGE_PIN_UART_TXD   -1
#endif

/** Rx pin for UART connected to short range module..
 */
#ifndef U_CFG_APP_SHORT_RANGE_PIN_UART_RXD
# define U_CFG_APP_SHORT_RANGE_PIN_UART_RXD   -1
#endif

/** CTS pin for UART connected to short range module..
 */
#ifndef U_CFG_APP_SHORT_RANGE_PIN_UART_CTS
# define U_CFG_APP_SHORT_RANGE_PIN_UART_CTS   -1
#endif

/** RTS pin for UART connected to short range module..
 */
#ifndef U_CFG_APP_SHORT_RANGE_PIN_UART_RTS
# define U_CFG_APP_SHORT_RANGE_PIN_UART_RTS   -1
#endif

/** UART hw block with a connected short range module
 */
#ifndef U_CFG_APP_SHORT_RANGE_UART
# define U_CFG_APP_SHORT_RANGE_UART        -1
#endif

/** Short range module role
 * Central: 1
 * Peripheral: 2
 */
#ifndef U_CFG_APP_SHORT_RANGE_ROLE
# define U_CFG_APP_SHORT_RANGE_ROLE        2
#endif

#endif // _U_CFG_APP_PLATFORM_SPECIFIC_H_

// End of file
