/*
 * Copyright 2019-2022 u-blox Ltd
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

/* Only bring in #includes specifically related to the test framework */

/** @file
 * @brief Porting layer and configuration items passed in at application
 * level when executing tests on the sara5ucpu platform.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: UNITY RELATED
 * -------------------------------------------------------------- */

/** Macro to wrap a test assertion and map it to unity.
 */
#define U_PORT_TEST_ASSERT(condition) TEST_ASSERT(condition)
#define U_PORT_TEST_ASSERT_EQUAL(expected, actual) TEST_ASSERT_EQUAL(expected, actual)

/** Macro to wrap the definition of a test function and
 * map it to unity.
 */
#define U_PORT_TEST_FUNCTION(group, name) U_PORT_UNITY_TEST_FUNCTION(group,  \
                                                                     name)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HEAP RELATED
 * -------------------------------------------------------------- */

/** The minimum free heap space permitted, i.e. what's left for
 * user code.
 */
#define U_CFG_TEST_HEAP_MIN_FREE_BYTES (1024 * 64)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS RELATED
 * -------------------------------------------------------------- */

/** The stack size to use for the test task created during OS testing.
 */
#define U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES 1536

/** The task priority to use for the task created during OS
 * testing: make sure that the priority of the task RUNNING
 * the tests is lower than this. In ThreadX, as used on this
 * platform, low numbers indicate higher priority.
 */
#define U_CFG_TEST_OS_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 11)

/** The minimum free stack space permitted for the main task,
 * basically what's left as a margin for user code.
 */
#define U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES (1024 * 5)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HW RELATED
 * -------------------------------------------------------------- */

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_A
# define U_CFG_TEST_PIN_A            -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_B
# define U_CFG_TEST_PIN_B            -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_C
# define U_CFG_TEST_PIN_C            -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_UART_A
# define U_CFG_TEST_UART_A           -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_UART_B
# define U_CFG_TEST_UART_B           -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_BAUD_RATE
# define U_CFG_TEST_BAUD_RATE 115200
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_UART_BUFFER_LENGTH_BYTES
# define U_CFG_TEST_UART_BUFFER_LENGTH_BYTES 512
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_TXD
# define U_CFG_TEST_PIN_UART_A_TXD   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#define U_CFG_TEST_PIN_UART_A_TXD_GET U_CFG_TEST_PIN_UART_A_TXD

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RXD
# define U_CFG_TEST_PIN_UART_A_RXD   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#define U_CFG_TEST_PIN_UART_A_RXD_GET U_CFG_TEST_PIN_UART_A_RXD

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_CTS
# define U_CFG_TEST_PIN_UART_A_CTS   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#define U_CFG_TEST_PIN_UART_A_CTS_GET U_CFG_TEST_PIN_UART_A_CTS

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_A_RTS
# define U_CFG_TEST_PIN_UART_A_RTS   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#define U_CFG_TEST_PIN_UART_A_RTS_GET U_CFG_TEST_PIN_UART_A_RTS

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_TXD
# define U_CFG_TEST_PIN_UART_B_TXD   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RXD
# define U_CFG_TEST_PIN_UART_B_RXD   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_CTS
# define U_CFG_TEST_PIN_UART_B_CTS   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_UART_B_RTS
# define U_CFG_TEST_PIN_UART_B_RTS   -1
#endif

/** Required for compilation of ubxlib tests/examples but not
 * relevant to SARAR5UCPU and hence set to -1.
 */
#ifndef U_CFG_TEST_PIN_GNSS_RESET_N
# define U_CFG_TEST_PIN_GNSS_RESET_N   -1
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_H_

// End of file
