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

#ifndef _U_CFG_HW_PLATFORM_SPECIFIC_H_
#define _U_CFG_HW_PLATFORM_SPECIFIC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file contains hardware configuration information
 * for an STM32U5 platform that is built into this porting code.
 * Some of the values may be configured for your hardware, others
 * are fixed inside the STM32U5 chip; each section describes
 * which is the case.
 *
 * Note that "UART" is used throughout this code, rather than
 * switching between "UART" and "USART" and having to remember
 * which number UART/USART is which.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32U5: SWO CLOCK RATE
 * -------------------------------------------------------------- */

#ifndef U_CFG_HW_SWO_CLOCK_HZ
/** The SWO clock in Hz.  Set this to -1 to let an external
 * debugger configure it rather than it being configured
 * automagically during startup; the drawback of that approach
 * is that if the target resets while the debugger is running
 * all further output will be lost.  However, it seems that
 * NUCLEO-U575ZI-Q boards don't like the embedded code setting
 * the SWO clock speed; if one does so the debugger on the board
 * will fail to connect if the board is power-cycled.  Hence
 * we leave this as -1 for STM32U5 (otherwise 125000 is the lowest,
 * and quite sufficient, rate and 2000000 is the usual maximum rate).
 */
# define U_CFG_HW_SWO_CLOCK_HZ -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32U5: UART/USART
 * -------------------------------------------------------------- */

/** The STM32U5 chip has 5 "normal" UARTs and one lower power LPUART.
 * The UARTs that may be used by the UART driver should be set to
 * 1 here.
 *
 * Note that in the STM32U5 chip the UART to pin relationship
 * is pretty much fixed, you can't just chose any UART and
 * expect it to connect to your chosen pins.  You have
 * to look in the STM32U5 data sheet for your particular
 * flavour of STM32U5 (e.g. table 28 in the STM32U575
 * datasheet) to determine what connects to what.
 */

#ifndef U_CFG_HW_LPUART1_AVAILABLE
/** Whether LPUART1 is available to the UART driver or not.
 * This is set to 1 because the unit tests for the UART
 * driver on this platform use LPUART1 (see
 * u_cfg_test_platform_specific.h).  If you are not going
 * to run the unit tests you can set this to 0.
 */
# define U_CFG_HW_LPUART1_AVAILABLE  1
#endif

#ifndef U_CFG_HW_UART1_AVAILABLE
/** Whether USART1 is available to the UART driver or not.
 * Note that on a NUCLEO-U575ZI-Q board UART1 is by default
 * connected to the debug chip.
 */
# define U_CFG_HW_UART1_AVAILABLE  0
#endif

#ifndef U_CFG_HW_UART2_AVAILABLE
/** Whether USART2 is available to the UART driver or not.
 * This set to 1 because, in the ubxlib test system we
 * connect a cellular module on this UART port (see
 * u_cfg_app_platform_specific.h).  If you are not going
 * to run the ubxlib tests or you are using a different
 * UART port for cellular when you do so, you can set this
 * to 0.
 */
# define U_CFG_HW_UART2_AVAILABLE  1
#endif

#ifndef U_CFG_HW_UART3_AVAILABLE
/** Whether USART3 is available to the UART driver or not.
 */
# define U_CFG_HW_UART3_AVAILABLE  0
#endif

#ifndef U_CFG_HW_UART4_AVAILABLE
/** Whether UART4 is available to the UART driver or not.
 */
# define U_CFG_HW_UART4_AVAILABLE  0
#endif

#ifndef U_CFG_HW_UART5_AVAILABLE
/** Whether UART5 is available to the UART driver or not.
 */
# define U_CFG_HW_UART5_AVAILABLE  0
#endif

#endif // _U_CFG_HW_PLATFORM_SPECIFIC_H_

// End of file
