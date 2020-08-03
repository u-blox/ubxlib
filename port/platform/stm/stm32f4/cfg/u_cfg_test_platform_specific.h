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

#ifndef _U_CFG_TEST_PLATFORM_SPECIFIC_H_
#define _U_CFG_TEST_PLATFORM_SPECIFIC_H_

/* Only bring in #includes specifically related to the test framework. */

/** @file
 * @brief Porting layer and configuration items passed in at applicationfor
 * level when executing tests on the STM32F4 platform.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: UNITY RELATED
 * -------------------------------------------------------------- */

/** Macro to wrap a test assertion and map it to our Unity port.
 */
#define U_PORT_TEST_ASSERT(condition) U_PORT_UNITY_TEST_ASSERT(condition)

/** Macro to wrap the definition of a test function and
 * map it to our Unity port.
 */
#define U_PORT_TEST_FUNCTION(group, name) U_PORT_UNITY_TEST_FUNCTION(group, \
                                                                     name)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS RELATED
 * -------------------------------------------------------------- */

/** The stack size to use for the test task created during OS testing.
 */
#define U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES (1024 * 2)

/** The task priority to use for the task created during.
 * testing.
 */
#define U_CFG_TEST_OS_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 5)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HW RELATED
 * -------------------------------------------------------------- */

/** Pin A for GPIO testing: will be used as an output and
 * must be connected to pin B via a 1k resistor.
 */
#ifndef U_CFG_TEST_PIN_A
# define U_CFG_TEST_PIN_A         0x05 // AKA PA_5 or D5 on a C030 board
#endif

/** Pin B for GPIO testing: will be used as both an input and
 * and open drain output and must be connected both to pin A via
 * a 1k resistor and directly to pin C.
 */
#ifndef U_CFG_TEST_PIN_B
# define U_CFG_TEST_PIN_B         0x18 // AKA PB_8 or D6 on a C030 board
#endif

/** Pin C for GPIO testing: must be connected to pin B,
 * will be used as an input only.
 */
#ifndef U_CFG_TEST_PIN_C
# define U_CFG_TEST_PIN_C         0x1f // AKA PB_15 or D7 on a C030 board
#endif

/** UART HW block for UART driver testing.
 * Note: make sure that the corresponding U_CFG_UARTx_AVAILABLE
 * for this UART is set to 1 in u_cfg_hw_platform_specific.h
 */
#ifndef U_CFG_TEST_UART
# define U_CFG_TEST_UART          3 // UART3
#endif

/** The baud rate to test the UART at.
 */
#ifndef U_CFG_TEST_BAUD_RATE
# define U_CFG_TEST_BAUD_RATE 115200
#endif

/** Tx pin for UART testing: should be connected to the Rx UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_TXD
# define U_CFG_TEST_PIN_UART_TXD   0x38 // UART3 TX, PD_8 or D1 on a C030 board
#endif

/** Rx pin for UART testing: should be connected to the Tx UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_RXD
# define U_CFG_TEST_PIN_UART_RXD   0x39 // UART3 RX, PD_9 or D0 on a C030 board
#endif

/** CTS pin for UART testing: should be connected to the RTS UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_CTS
# define U_CFG_TEST_PIN_UART_CTS  0x3b // UART3 CTS, PD_11 or D2 on a C030 board
#endif

/** RTS pin for UART testing: should be connected to the CTS UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_RTS
# define U_CFG_TEST_PIN_UART_RTS  0x1e // UART3 RTS, PB_14 or D3 on a C030 board
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_H_

// End of file
