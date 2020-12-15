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

/* These inclusions required to get the UART CTS/RTS pin
 * assignments from the Zephyr device tree.
 */
#include "devicetree.h"

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
 * COMPILE-TIME MACROS: HEAP RELATED
 * -------------------------------------------------------------- */

/** The minimum free heap space permitted, i.e. what's left for
 * user code.  Unfortunately Zephyr does not offer a way to measure
 * hte minimum free heap space left so settings this to -1 for now.
 */
#define U_CFG_TEST_HEAP_MIN_FREE_BYTES -1

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

/** Required for Zephyr device tree query:
 * Since U_CFG_TEST_UART_A is a macro, DT_CAT() won't work on it
 * directly, it needs this intermediate to cause U_CFG_TEST_UART_A
 * to be expand first, e.g. if U_CFG_TEST_UART_A is defined as 1
 * then U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A) spits out uart1.
 */
#define U_CFG_TEST_CAT(a, b) DT_CAT(a, b)

/** The minimum free stack space permitted for the main task,
 * basically what's left as a margin for user code.
 */
#define U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES (1024 * 5)

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

/** UART HW block for UART driver loopback testing.
 */
#ifndef U_CFG_TEST_UART_A
# define U_CFG_TEST_UART_A          1
#endif

/** UART HW block for UART driver loopback testing where
 * two UARTs are employed.
 */
#ifndef U_CFG_TEST_UART_B
# define U_CFG_TEST_UART_B          -1
#endif

/** The baud rate to test the UART at.
 */
#ifndef U_CFG_TEST_BAUD_RATE
# define U_CFG_TEST_BAUD_RATE 115200
#endif

/** The length of UART buffer to use during testing.
 */
#ifndef U_CFG_TEST_UART_BUFFER_LENGTH_BYTES
# define U_CFG_TEST_UART_BUFFER_LENGTH_BYTES 1024
#endif

/* IMPORTANT:
 * The pins used by the UART are NOT defined here,
 * they are defined at compile-time by the Zephyr
 * device tree defined in
 * nrf5340pdk_nrf5340_cpuapp_common.dts.  A .overlay file
 * will be found in the project directory which sets
 * the pins used during UART port testing for this
 * project.  The values defined here are simply to
 * satisfy the UART port API and are otherwise ignored.
 */

/** Tx pin for UART testing: should be connected either to the
 * Rx UART pin or to U_CFG_TEST_PIN_UART_B_RXD if that is
 * connected.
 */
#ifndef U_CFG_TEST_PIN_UART_A_TXD
# define U_CFG_TEST_PIN_UART_A_TXD   -1
#endif

/** Macro to return the TXD pin for UART A: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
#if (U_CFG_TEST_UART_A < 0)
# define U_CFG_TEST_PIN_UART_A_TXD_GET -1
#else
# if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), tx_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), tx_pin) < 0xffffffff)
#  define U_CFG_TEST_PIN_UART_A_TXD_GET DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), tx_pin)
# else
#  define U_CFG_TEST_PIN_UART_A_TXD_GET -1
# endif
#endif

/** Rx pin for UART testing: should be connected either to the
 * Tx UART pin or to U_CFG_TEST_PIN_UART_B_TXD if that is
 * connected.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RXD
# define U_CFG_TEST_PIN_UART_A_RXD   -1
#endif

/** Macro to return the RXD pin for UART A: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
#if (U_CFG_TEST_UART_A < 0)
# define U_CFG_TEST_PIN_UART_A_RXD_GET -1
#else
# if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rx_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rx_pin) < 0xffffffff)
#  define U_CFG_TEST_PIN_UART_A_RXD_GET DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rx_pin)
# else
#  define U_CFG_TEST_PIN_UART_A_RXD_GET -1
# endif
#endif

/** CTS pin for UART testing: should be connected either to the
 * RTS UART pin or to U_CFG_TEST_PIN_UART_B_RTS if that is
 * connected.
 */
#ifndef U_CFG_TEST_PIN_UART_A_CTS
# define U_CFG_TEST_PIN_UART_A_CTS   -1
#endif

/** Macro to return the CTS pin for UART A: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
#if (U_CFG_TEST_UART_A < 0)
# define U_CFG_TEST_PIN_UART_A_CTS_GET -1
#else
# if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), cts_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), cts_pin) < 0xffffffff)
#  define U_CFG_TEST_PIN_UART_A_CTS_GET DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), cts_pin)
# else
#  define U_CFG_TEST_PIN_UART_A_CTS_GET -1
# endif
#endif

/** RTS pin for UART testing: should be connected connected either to the
 * CTS UART pin or to U_CFG_TEST_PIN_UART_B_CTS if that is
 * connected.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RTS
# define U_CFG_TEST_PIN_UART_A_RTS   -1
#endif

/** Macro to return the RTS pin for UART A: note that dashes
 * in the DTS node name must be converted to underscores.
 * 0xffffffff is a magic value in nRF speak, mapping to
 * NRF_UARTE_PSEL_DISCONNECTED.
 */
#if (U_CFG_TEST_UART_A < 0)
# define U_CFG_TEST_PIN_UART_A_RTS_GET -1
#else
# if DT_NODE_HAS_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rts_pin) &&    \
     (DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rts_pin) < 0xffffffff)
#  define U_CFG_TEST_PIN_UART_A_RTS_GET DT_PROP(DT_NODELABEL(U_CFG_TEST_CAT(uart, U_CFG_TEST_UART_A)), rts_pin)
# else
#  define U_CFG_TEST_PIN_UART_A_RTS_GET -1
# endif
#endif

/** Tx pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_RXD.
 */
#ifndef U_CFG_TEST_PIN_UART_B_TXD
# define U_CFG_TEST_PIN_UART_B_TXD   -1
#endif

/** Rx pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_TXD.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RXD
# define U_CFG_TEST_PIN_UART_B_RXD   -1
#endif

/** CTS pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_RTS.
 */
#ifndef U_CFG_TEST_PIN_UART_B_CTS
# define U_CFG_TEST_PIN_UART_B_CTS   -1
#endif

/** RTS pin for UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_CTS.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RTS
# define U_CFG_TEST_PIN_UART_B_RTS   -1
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_H_

// End of file
