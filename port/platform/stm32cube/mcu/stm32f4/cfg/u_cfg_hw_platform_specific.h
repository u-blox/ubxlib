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
 * @brief This header file contains hardware configuration information for
 * an STM32F4 platform that is built into this porting code.
 * Some of the values may be configured for your hardware,
 * others are fixed inside the STM32F4 chip; each section describes
 * which is the case.
 *
 * Note that "UART" is used throughout this code, rather than
 * switching between "UART" and "USART" and having to
 * remember which number UART/USART is which.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: SWO CLOCK RATE
 * -------------------------------------------------------------- */

#ifndef U_CFG_HW_SWO_CLOCK_HZ
/** The SWO clock in Hz.  Set this to -1 to let an external
 * debugger configure it rather than it being configured
 * automagically during startup; the drawback of that approach
 * is that if the target resets while the debugger is running
 * all further output will be lost. In theory this can run
 * at 2 MHz but I've found that is not reliable on all boards.
 * 125,000 chosen because it's one of the options in the STM32Cube
 * IDE configuration dialog that is around 100 kHz and seems stable.
 */
# define U_CFG_HW_SWO_CLOCK_HZ 125000
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: UART/USART
 * -------------------------------------------------------------- */

/** The STM32F4 chip has 8 UARTs.  The UARTs that may be used
 * by the UART driver should be set to 1 here.
 * Note that in the STM32F4 chip the UART to pin relationship
 * is pretty much fixed, you can't just chose any UART and
 * expect it to connect to your chosen pins.  You have
 * to look in the STM32F4 data sheet for your particular
 * flavour of STM32F4 (e.g. table 12 in the STM32F437
 * datasheet) to determine what connects to what.
 * The values here are correct for any of the u-blox C030
 * boards.  If you are using another board you should
 * configure these values yourself.
 */

#ifndef U_CFG_HW_UART1_AVAILABLE
/** Whether USART1 is available to the UART driver or not.
 * This USART should be made available when using a
 * C030-R412M board since the cellular module is connected
 * to the STM32F4 chip that way on that board.
 * This can also be routed to PB6 (TXD) /PB7 (RXD) which
 * come out on the SCL and SDA pins of the Arduino connector
 * on the same board (both C030-R412M and C020-U201).
 */
# define U_CFG_HW_UART1_AVAILABLE  1
#endif

#ifndef U_CFG_HW_UART2_AVAILABLE
/** Whether USART2 is available to the UART driver or not.
 * This USART must be available for the C030-U201 board
 * since the cellular module is connected to the STM32F4
 * chip that way on that board.
 */
# define U_CFG_HW_UART2_AVAILABLE  1
#endif

#ifndef U_CFG_HW_UART3_AVAILABLE
/** Whether USART3 is available to the UART driver or not.
 * This is set to 1 because the unit tests for the UART
 * driver on this platform use UART3
 * (see u_cfg_test_platform_specific.h), which comes
 * out of the D0/D1/D2/D3 pins of the Arduino connector
 * on a C030 board.  If you are not going to run
 * the unit tests you can set this to 0.
 */
# define U_CFG_HW_UART3_AVAILABLE  1
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

#ifndef U_CFG_HW_UART6_AVAILABLE
/** Whether USART6 is available to the UART driver or not.
 * This is set to 1 because the GNSS chip on a C030 board
 * is connected to pins PC_6/PC_7 (see
 * u_cfg_test_platform_specific.h), which are USART6.  If
 * you are not going to use this arrangement the value
 * may be set to 0.
 */
# define U_CFG_HW_UART6_AVAILABLE  1
#endif

#ifndef U_CFG_HW_UART7_AVAILABLE
/** Whether UART7 is available to the UART driver or not.
 */
# define U_CFG_HW_UART7_AVAILABLE  0
#endif

#ifndef U_CFG_HW_UART8_AVAILABLE
/** Whether UART8 is available to the UART driver or not.
 */
# define U_CFG_HW_UART8_AVAILABLE  0
#endif

/** For the UART driver to operate it needs a DMA
 * channel (0 to 7) on a DMA stream (0 to 7) on a
 * DMA engine (1 or 2) for each UART that it is
 * requested to use through the port UART API.
 *
 * The choice of DMA engine/stream/channel for
 * a given peripheral is fixed in the STM32F4 chip,
 * see table 42 of their RM0090 document.  It is the
 * fixed mapping of engine/stream/channel to
 * UART/USART RX which is represented below.
 */

/** The DMA engine for USART1 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART1_DMA_ENGINE              2

#ifndef U_CFG_HW_UART1_DMA_STREAM
/** The DMA stream for USART1 Rx: can also be set to 5
 * (with the same DMA engine/channel).
 */
# define U_CFG_HW_UART1_DMA_STREAM             2
#endif

/** The DMA channel for USART1 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART1_DMA_CHANNEL             4

/** The DMA engine for USART2 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART2_DMA_ENGINE              1

/** The DMA stream for USART2 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART2_DMA_STREAM              5

/** The DMA channel for USART2 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART2_DMA_CHANNEL             4

/** The DMA engine for USART3 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART3_DMA_ENGINE              1

/** The DMA stream for USART3 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART3_DMA_STREAM              1

/** The DMA channel for USART3 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART3_DMA_CHANNEL             4

/** The DMA engine for UART4 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART4_DMA_ENGINE              1

/** The DMA stream for UART4 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART4_DMA_STREAM              2

/** The DMA channel for UART4 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART4_DMA_CHANNEL             4

/** The DMA engine for UART5 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART5_DMA_ENGINE              1

/** The DMA stream for UART5 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART5_DMA_STREAM              0

/** The DMA channel for UART5 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART5_DMA_CHANNEL             4

/** The DMA engine for USART6 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART6_DMA_ENGINE              2

#ifndef U_CFG_HW_UART6_DMA_STREAM
/** The DMA stream for USART6 Rx: can also be set to 2
 * (with the same DMA engine/channel).
 */
# define U_CFG_HW_UART6_DMA_STREAM             1
#endif

/** The DMA channel for USART6 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART6_DMA_CHANNEL             5

/** The DMA engine for UART7 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART7_DMA_ENGINE              1

/** The DMA stream for UART7 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART7_DMA_STREAM              3

/** The DMA channel for UART7 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART7_DMA_CHANNEL             5

/** The DMA engine for UART8 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART8_DMA_ENGINE              1

/** The DMA stream for UART8 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART8_DMA_STREAM              6

/** The DMA channel for UART8 Rx: fixed in the
 * STM32F4 chip.
 */
#define U_CFG_HW_UART8_DMA_CHANNEL             5

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR STM32F4: EXTI (INTERRUPTS NEED FOR GPIO)
 * -------------------------------------------------------------- */

/** In order to support use of a GPIO line as an interrupt, the
 * relevant interrupt function needs to be made available to ubxlib.
 * For STM32F4 there are 16 interrupts available where EXTI 0 serves
 * pin 0 on any of the GPIO ports (A to I), EXTI 1 serves pin 1 on
 * any of the GPIO ports, etc, up to EXTI 15 serving pins 15.
 *
 * The only usage of GPIO interrupts by ubxlib is if a Data Ready
 * pin is to be used with a GNSS device; one EXTI interrupt is
 * required for each GNSS device where this is the case.
 *
 * So if, for instance, you want to use pin B5 as your Data Ready
 * pin, you would need to make sure that U_CFG_HW_EXTI_5_AVAILABLE,
 * the EXTI for GPIO pin 5, was set to 1; the uPortGpio driver
 * here will then set the relevant SYSCFG_EXTI register to indicate
 * that EXTI5 is to monitor port B: hence pin B5, et voila.
 */

#ifndef U_CFG_HW_EXTI_0_AVAILABLE
/** Whether EXTI 0 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_0_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_1_AVAILABLE
/** Whether EXTI 1 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_1_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_2_AVAILABLE
/** Whether EXTI 2 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_2_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_3_AVAILABLE
/** Whether EXTI 3 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_3_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_4_AVAILABLE
/** Whether EXTI 4 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_4_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_5_AVAILABLE
/** Whether EXTI 5 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_5_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_6_AVAILABLE
/** Whether EXTI 6 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_6_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_7_AVAILABLE
/** Whether EXTI 7 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_7_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_8_AVAILABLE
/** Whether EXTI 8 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_8_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_9_AVAILABLE
/** Whether EXTI 9 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_9_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_10_AVAILABLE
/** Whether EXTI 10 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_10_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_11_AVAILABLE
/** Whether EXTI 11 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_11_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_12_AVAILABLE
/** Whether EXTI 12 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_12_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_13_AVAILABLE
/** Whether EXTI 13 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_13_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_14_AVAILABLE
/** Whether EXTI 14 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_14_AVAILABLE  0
#endif

#ifndef U_CFG_HW_EXTI_15_AVAILABLE
/** Whether EXTI 15 is available to ubxlib.
 */
# define U_CFG_HW_EXTI_15_AVAILABLE  0
#endif

#endif // _U_CFG_HW_PLATFORM_SPECIFIC_H_

// End of file
