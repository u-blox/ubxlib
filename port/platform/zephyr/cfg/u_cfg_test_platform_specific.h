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

/* These inclusions required to get the UART CTS/RTS pin assignments
 * from the Zephyr device tree.
 * Note that the pin numbers used below should be those of the MCU:
 * if you are using an MCU inside a u-blox module the IO pin numbering
 * for the module is likely different to that from the MCU: check the
 * data sheet for the module to determine the mapping.
 */
#include "devicetree.h"
#include "u_runner.h"

/** @file
 * @brief Porting layer and configuration items passed in at application
 * level when executing tests on the zephyr platform.
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
 * user code.  Unfortunately Zephyr does not offer a way to measure
 * the minimum free heap space left so settings this to -1 for now.
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

#ifdef CONFIG_BOARD_UBX_EVKNINAB3_NRF52840
#include "u_cfg_test_platform_specific_ubx_evkninab3_nrf52840.h"
#endif

#ifdef CONFIG_BOARD_UBX_EVKNINAB4_NRF52833
#include "u_cfg_test_platform_specific_ubx_evkninab4_nrf52833.h"
#endif

#ifdef CONFIG_BOARD_NRF52840DK_NRF52840
#include "u_cfg_test_platform_specific_nrf52840dk_nrf52840.h"
#endif

#ifdef CONFIG_BOARD_UBX_EVKNORAB1_NRF5340_CPUAPP
#include "u_cfg_test_platform_specific_ubx_evknorab1_nrf5340.h"
#endif

#ifdef CONFIG_BOARD_NRF5340PDK_NRF5340_CPUAPP
#include "u_cfg_test_platform_specific_nrf5340pdk_nrf5340.h"
#endif

#ifdef CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP
#include "u_cfg_test_platform_specific_nrf5340dk_nrf5340.h"
#endif

#ifdef CONFIG_BOARD_SPARKFUN_ASSET_TRACKER_NRF52840
#include "u_cfg_test_platform_specific_sparkfun_asset_tracker_nrf52840.h"
#endif

#ifdef CONFIG_BOARD_NATIVE_POSIX
#include "u_cfg_test_platform_specific_native_posix.h"
#endif

#if defined(CONFIG_BOARD_UBX_EVKNORAB1_NRF5340_CPUAPP) || \
    defined(CONFIG_BOARD_NRF5340PDK_NRF5340_CPUAPP)    || \
    defined(CONFIG_BOARD_NRF5340DK_NRF5340_CPUAPP)
/** UART HW block for UART driver loopback testing on nRF53.
 */
# ifndef U_CFG_TEST_UART_A
#  define U_CFG_TEST_UART_A          2
# endif
#else
/** UART HW block for UART driver loopback testing on everything else.
 */
# ifndef U_CFG_TEST_UART_A
#  define U_CFG_TEST_UART_A          1
# endif
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
 * they are defined at compile-time by the chosen
 * .dts file either from inside the Zephyr device
 * tree (under the "boards" directory of Zephyr)
 * or from a custom board file under the "custom_boards"
 * directory in here.  A .overlay file can be found
 * in the "board" directory under "runner" which sets
 * the pins used during the UART port testing for ubxlib.
 * The values defined here are simply to satisfy
 * the UART port API and are otherwise ignored.
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
