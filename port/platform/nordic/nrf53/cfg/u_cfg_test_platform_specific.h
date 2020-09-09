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
 * @brief Porting layer and configuration items passed in at application
 * level when executing tests on the NRF5340 platform.
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
#define U_PORT_TEST_FUNCTION(name, group) U_PORT_UNITY_TEST_FUNCTION(name,  \
                                                                     group)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS RELATED
 * -------------------------------------------------------------- */

/** The stack size to use for the test task created during OS testing.
 */
#define U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES (1024 * 2)

/** The task priority to use for the task created during OS
 * testing: make sure that the priority of the task RUNNING
 * the tests is lower than this.
 */
#define U_CFG_TEST_OS_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 5)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HW RELATED
 * -------------------------------------------------------------- */

/** Pin A for GPIO testing: will be used as an output and
 * must be connected to pin B via a 1k resistor.
 */
#ifndef U_CFG_TEST_PIN_A
# define U_CFG_TEST_PIN_A         38 // AKA 1.06
#endif

/** Pin B for GPIO testing: will be used as both an input and
 * and open drain output and must be connected both to pin A via
 * a 1k resistor and directly to pin C.
 */
#ifndef U_CFG_TEST_PIN_B
# define U_CFG_TEST_PIN_B         39 // AKA 1.07
#endif
/** Pin C for GPIO testing: must be connected to pin B,
 * will be used as an input only.
 */
#ifndef U_CFG_TEST_PIN_C
# define U_CFG_TEST_PIN_C         40 // AKA 1.08
#endif

/** UART HW block for UART driver testing.
 */
#ifndef U_CFG_TEST_UART
# define U_CFG_TEST_UART          1
#endif

/** The baud rate to test the UART at.
 */
#ifndef U_CFG_TEST_BAUD_RATE
# define U_CFG_TEST_BAUD_RATE 115200
#endif

/** Tx pin for UART testing: should be connected to the Rx UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_0_TXD
# define U_CFG_TEST_PIN_UART_0_TXD   -1 // AKA 1.10
#endif

/** Rx pin for UART testing: should be connected to the Tx UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_0_RXD
# define U_CFG_TEST_PIN_UART_0_RXD   -1// AKA 1.11
#endif

/** CTS pin for UART testing: should be connected to the RTS UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_CTS
# define U_CFG_TEST_PIN_UART_CTS   44 // AKA 1.12
#endif

/** RTS pin for UART testing: should be connected to the CTS UART pin.
 */
#ifndef U_CFG_TEST_PIN_UART_RTS
# define U_CFG_TEST_PIN_UART_RTS   45 // AKA 1.13
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_H_

// End of file
