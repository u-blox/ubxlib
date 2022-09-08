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

#ifndef _U_CFG_TEST_PLATFORM_SPECIFIC_H_
#define _U_CFG_TEST_PLATFORM_SPECIFIC_H_

/* Only bring in #includes specifically related to the test framework. */
#include "u_runner.h"

/** @file
 * @brief Porting layer and configuration items passed in at application
 * level when executing tests on Windows.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: UNITY RELATED
 * -------------------------------------------------------------- */

/** Macro to wrap a test assertion and map it to our Unity port.
 */
#define U_PORT_TEST_ASSERT(condition) U_PORT_UNITY_TEST_ASSERT(condition)
#define U_PORT_TEST_ASSERT_EQUAL(expected, actual) U_PORT_UNITY_TEST_ASSERT_EQUAL(expected, actual)

/** Macro to wrap the definition of a test function and
 * map it to our Unity port.
 *
 * IMPORTANT: in order for the test automation test filtering
 * to work correctly the group and name strings *must* follow
 * these rules:
 *
 * - the group string must begin with the API directory
 *   name converted to camel case, enclosed in square braces.
 *   So for instance if the API being tested was "short_range"
 *   (e.g. common/short_range/api) then the group name
 *   could be "[shortRange]" or "[shortRangeSubset1]".
 * - the name string must begin with the group string without
 *   the square braces; so in the example above it could
 *   for example be "shortRangeParticularTest" or
 *   "shortRangeSubset1ParticularTest" respectively.
 */
#define U_PORT_TEST_FUNCTION(name, group) U_PORT_UNITY_TEST_FUNCTION(name,  \
                                                                     group)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HEAP RELATED
 * -------------------------------------------------------------- */

/** The minimum free heap space permitted, i.e. what's left for
 * user code.
 */
#define U_CFG_TEST_HEAP_MIN_FREE_BYTES (1024 * 7)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS RELATED
 * -------------------------------------------------------------- */

/** The stack size to use for the test task created during OS testing.
 */
#define U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES 1280

/** The task priority to use for the task created during OS
 * testing: make sure that the priority of the task RUNNING
 * the tests is lower than this.
 */
#define U_CFG_TEST_OS_TASK_PRIORITY U_CFG_OS_PRIORITY_MIN + 12

/** The minimum free stack space permitted for the main task,
 * basically what's left as a margin for user code.  This makes
 * no sense on Windows so we set it to -1.
 */
#define U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES -1

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HW RELATED
 * -------------------------------------------------------------- */

/** Pin A for GPIO testing: will be used as an output and must be
 * connected to pin B via a 1k resistor; not relevant for
 * Windows and so set to -1.
 */
#ifndef U_CFG_TEST_PIN_A
# define U_CFG_TEST_PIN_A         -1
#endif

/** Pin B for GPIO testing: will be used as both an input and
 * and open drain output and must be connected both to pin A via
 * a 1k resistor and directly to pin C; not relevant for
 * Windows and so set to -1.
 */
#ifndef U_CFG_TEST_PIN_B
# define U_CFG_TEST_PIN_B         -1
#endif

/** Pin C for GPIO testing: must be connected to pin B,
 * will be used as an input only; not relevant for
 * Windows and so set to -1.
 */
#ifndef U_CFG_TEST_PIN_C
# define U_CFG_TEST_PIN_C         -1
#endif

/** COM port for UART driver testing; e.g. to use COM1 set this
 * to 1.  Specify -1 where there is no such connection.
 * To run the UART porting tests, use something like Com2Com
 * to set up a virtual COM port that is looped-back to itself
 */
#ifndef U_CFG_TEST_UART_A
# define U_CFG_TEST_UART_A        -1
#endif

/** COM port for UART driver loopback testing where two UARTs
 * are employed; e.g. to use COM1 set this to 1.  Specify -1
 * where there is no such connection.
 * To run tests requiring a pair of looped-back UARTs, use
 * something like Com2Com to create virtual COM ports.
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

/** Tx pin for UART testing: should be connected either to the
 * Rx UART pin or to U_CFG_TEST_PIN_UART_B_RXD if that is
 * connected; not relevant for Windows and so set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_TXD
# define U_CFG_TEST_PIN_UART_A_TXD   -1
#endif

/** Macro to return the TXD pin for UART A: on some
 * platforms this is not a simple define.
 */
#define U_CFG_TEST_PIN_UART_A_TXD_GET U_CFG_TEST_PIN_UART_A_TXD

/** Rx pin for UART testing: should be connected either to the
 * Tx UART pin or to U_CFG_TEST_PIN_UART_B_TXD if that is
 * connected; not relevant for Windows and so set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RXD
# define U_CFG_TEST_PIN_UART_A_RXD   -1
#endif

/** Macro to return the RXD pin for UART A: on some
 * platforms this is not a simple define.
 */
#define U_CFG_TEST_PIN_UART_A_RXD_GET U_CFG_TEST_PIN_UART_A_RXD

/** CTS pin for UART testing: should be connected either to the
 * RTS UART pin or to U_CFG_TEST_PIN_UART_B_RTS if that is
 * connected; on Windows this simply serves as a "disable/enable"
 * CTS flow control flag, negative for disable, else enable.
 */
#ifndef U_CFG_TEST_PIN_UART_A_CTS
# define U_CFG_TEST_PIN_UART_A_CTS   0
#endif

/** Macro to return the CTS pin for UART A: on some
 * platforms this is not a simple define.
 */
#define U_CFG_TEST_PIN_UART_A_CTS_GET U_CFG_TEST_PIN_UART_A_CTS

/** RTS pin for UART testing: should be connected connected either
 * to the CTS UART pin or to U_CFG_TEST_PIN_UART_B_CTS if that is
 * connected; on Windows this simply serves as a "disable/enable" RTS
 * flow control flag, negative for disable, else enable.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RTS
# define U_CFG_TEST_PIN_UART_A_RTS   0
#endif

/** Macro to return the RTS pin for UART A: on some
 * platforms this is not a simple define.
 */
#define U_CFG_TEST_PIN_UART_A_RTS_GET U_CFG_TEST_PIN_UART_A_RTS

/** Tx pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_RXD.  This is not relevant for Windows and
 * so is set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_TXD
# define U_CFG_TEST_PIN_UART_B_TXD   -1
#endif

/** Rx pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_TXD.  This is not relevant for Windows and
 * so is set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RXD
# define U_CFG_TEST_PIN_UART_B_RXD   -1
#endif

/** CTS pin for dual-UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_RTS; on Windows this simply serves as a
 * "disable/enable" CTS flow control flag, negative for disable,
 * else enable.
 */
#ifndef U_CFG_TEST_PIN_UART_B_CTS
# define U_CFG_TEST_PIN_UART_B_CTS   0
#endif

/** RTS pin for UART testing: if present should be connected to
 * U_CFG_TEST_PIN_UART_A_CTS; on Windows this simply serves as a
 * "disable/enable" RTS flow control flag, negative for disable,
 * else enable.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RTS
# define U_CFG_TEST_PIN_UART_B_RTS   0
#endif

/** Reset pin for a GNSS module, not relevant on Windows
 * since it is only used for testing of I2C, which Windows doesn't
 * have.
 */
#ifndef U_CFG_TEST_PIN_GNSS_RESET_N
# define U_CFG_TEST_PIN_GNSS_RESET_N   -1
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: DEBUG RELATED
 * -------------------------------------------------------------- */

/** When this is set to 1 the inactivity detector will be enabled
 * that will check if there is no call to uPortLog() within a certain
 * time.
 */
#ifndef U_CFG_TEST_ENABLE_INACTIVITY_DETECTOR
# define U_CFG_TEST_ENABLE_INACTIVITY_DETECTOR  1
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_H_

// End of file
