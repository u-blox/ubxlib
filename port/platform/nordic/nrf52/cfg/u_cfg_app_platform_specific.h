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
 * an NRF52 platform that is fed in at application level.  You should
 * override these values as necessary for your particular platform.
 */

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

/* NRF52 uses a port numbering scheme with 32 GPIO lines
 * on each port and two ports, so GPIO 0 you will see written as
 * 0.00, GPIO 31 as 0.31, GPIO 32 as 1.00 and GPIO 48 as 1.15.
 * Also, if you are using the NRF52 DK board a load of these
 * have pre-assigned functions so you have to read the back of
 * the PCB _very_ carefully to find any that are free.  In
 * general, port 1 is freer than port 0, hence the choices below.
 */

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

#ifndef U_CFG_APP_PIN_CELL_VINT
/** The NRF52 GPIO input that is connected to the VInt pin of
 * the cellular module.
 * -1 is used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_VINT              -1
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
/** The NRF5 GPIO output pin that tells the cellular modem
 * that it can send more data to the NRF52 UART. -1 should
 * be used where there is no such connection.
 */
# define U_CFG_APP_PIN_CELL_RTS               -1
#endif

#endif // _U_CFG_APP_PLATFORM_SPECIFIC_H_

// End of file
