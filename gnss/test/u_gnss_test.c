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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the GNSS "general" API: these should pass on all
 * platforms where one or preferably two UARTs are available.  No
 * GNSS module is actually used in this set of tests.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Streaming handle for one GNSS module (could be UART or I2C).
 */
static int32_t gStreamAHandle = -1;

/** The type of streaming handle A.
 */
static uGnssTransportType_t gTransportTypeA = U_GNSS_TRANSPORT_NONE;

/** UART handle for another GNSS module.
 */
static int32_t gUartBHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then de-initialise a GNSS chip.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uGnssInit() == 0);
    uGnssDeinit();
    uPortDeinit();
}

#if (U_CFG_TEST_UART_A >= 0) || (U_CFG_APP_GNSS_I2C >= 0)
/** Add a straaming GNSS instance, e.g. UART or I2C,
 * and remove it again.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssAddStream")
{
    uDeviceHandle_t gnssHandleA;
    uDeviceHandle_t dummyHandle;
    uGnssTransportHandle_t transportHandleA;
# if (U_CFG_TEST_UART_B >= 0)
    uDeviceHandle_t gnssHandleB;
    uGnssTransportHandle_t transportHandleB;
# endif
    uGnssTransportType_t transportType = U_GNSS_TRANSPORT_NONE;
    uGnssTransportHandle_t transportHandle;
    int32_t errorCode;
    int32_t heapUsed;
    bool printUbxMessagesDefault;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

# if (U_CFG_APP_GNSS_I2C >= 0)
    U_PORT_TEST_ASSERT(uPortI2cInit() == 0);

    gStreamAHandle = uPortI2cOpen(U_CFG_APP_GNSS_I2C,
                                  U_CFG_APP_PIN_GNSS_SDA,
                                  U_CFG_APP_PIN_GNSS_SCL,
                                  true);
    U_PORT_TEST_ASSERT(gStreamAHandle >= 0);
    gTransportTypeA = U_GNSS_TRANSPORT_UBX_I2C;
    transportHandleA.i2c = gStreamAHandle;
# else
    gStreamAHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                                   U_CFG_TEST_BAUD_RATE,
                                   NULL,
                                   U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                   U_CFG_TEST_PIN_UART_A_TXD,
                                   U_CFG_TEST_PIN_UART_A_RXD,
                                   U_CFG_TEST_PIN_UART_A_CTS,
                                   U_CFG_TEST_PIN_UART_A_RTS);
    U_PORT_TEST_ASSERT(gStreamAHandle >= 0);
    gTransportTypeA = U_GNSS_TRANSPORT_UBX_UART;
    transportHandleA.uart = gStreamAHandle;
# endif

    U_PORT_TEST_ASSERT(uGnssInit() == 0);

    U_TEST_PRINT_LINE("adding a GNSS instance on streaming port...");
    errorCode = uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                         gTransportTypeA, transportHandleA,
                         -1, false, &gnssHandleA);
    U_PORT_TEST_ASSERT_EQUAL((int32_t) U_ERROR_COMMON_SUCCESS, errorCode);
    transportHandle.uart = -1;
    transportHandle.i2c = -1;
    U_PORT_TEST_ASSERT(uGnssGetTransportHandle(gnssHandleA,
                                               &transportType,
                                               &transportHandle) == 0);
    switch (gTransportTypeA) {
        case U_GNSS_TRANSPORT_UBX_UART:
            U_PORT_TEST_ASSERT(transportType == U_GNSS_TRANSPORT_UBX_UART);
            U_PORT_TEST_ASSERT(transportHandle.uart == transportHandleA.uart);
            break;
        case U_GNSS_TRANSPORT_UBX_I2C:
            U_PORT_TEST_ASSERT(transportType == U_GNSS_TRANSPORT_UBX_I2C);
            U_PORT_TEST_ASSERT(transportHandle.i2c == transportHandleA.i2c);
            break;
        default:
            U_PORT_TEST_ASSERT(false);
            break;
    }

    printUbxMessagesDefault = uGnssGetUbxMessagePrint(gnssHandleA);
    uGnssSetUbxMessagePrint(gnssHandleA, !printUbxMessagesDefault);
    if (printUbxMessagesDefault) {
        U_PORT_TEST_ASSERT(!uGnssGetUbxMessagePrint(gnssHandleA));
    } else {
        U_PORT_TEST_ASSERT(uGnssGetUbxMessagePrint(gnssHandleA));
    }

    U_TEST_PRINT_LINE("adding another instance on the same streaming"
                      " port, should fail...");
# if (U_CFG_APP_GNSS_I2C >= 0)
    U_PORT_TEST_ASSERT(uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                                U_GNSS_TRANSPORT_UBX_I2C,
                                transportHandleA,
                                -1, false, &dummyHandle) < 0);
# else
    U_PORT_TEST_ASSERT(uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                                U_GNSS_TRANSPORT_UBX_UART,
                                transportHandleA,
                                -1, false, &dummyHandle) < 0);
# endif

# if (U_CFG_TEST_UART_B >= 0)
    // If we have a second UART port, add a second GNSS API on it
    gUartBHandle = uPortUartOpen(U_CFG_TEST_UART_B,
                                 U_CFG_TEST_BAUD_RATE,
                                 NULL,
                                 U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                 U_CFG_TEST_PIN_UART_B_TXD,
                                 U_CFG_TEST_PIN_UART_B_RXD,
                                 U_CFG_TEST_PIN_UART_B_CTS,
                                 U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gUartBHandle >= 0);
    transportHandleB.uart = gUartBHandle;

    U_TEST_PRINT_LINE("adding a GNSS instance on UART %d...", U_CFG_TEST_UART_B);
    errorCode = uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                         U_GNSS_TRANSPORT_UBX_UART,
                         transportHandleB,
                         -1, false, &gnssHandleB);
    U_PORT_TEST_ASSERT_EQUAL((int32_t) U_ERROR_COMMON_SUCCESS, errorCode);
    transportType = U_GNSS_TRANSPORT_NONE;
    transportHandle.uart = -1;
    U_PORT_TEST_ASSERT(uGnssGetTransportHandle(gnssHandleB,
                                               &transportType,
                                               &transportHandle) == 0);
    U_PORT_TEST_ASSERT(transportType == U_GNSS_TRANSPORT_UBX_UART);
    U_PORT_TEST_ASSERT(transportHandle.uart == transportHandleB.uart);
    if (printUbxMessagesDefault) {
        U_PORT_TEST_ASSERT(uGnssGetUbxMessagePrint(gnssHandleB));
    } else {
        U_PORT_TEST_ASSERT(!uGnssGetUbxMessagePrint(gnssHandleB));
    }

    U_TEST_PRINT_LINE("adding another instance on the same UART, should fail...");
    U_PORT_TEST_ASSERT(uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                                U_GNSS_TRANSPORT_UBX_UART,
                                transportHandleB, -1, false,
                                &dummyHandle) < 0);

    // Don't remove this one, let uGnssDeinit() do it
# endif

    U_TEST_PRINT_LINE("removing first GNSS instance...");
    uGnssRemove(gnssHandleA);

    U_TEST_PRINT_LINE("adding it again...");
    // Use NMEA this time for the sake of variety
    if (gTransportTypeA == U_GNSS_TRANSPORT_UBX_UART) {
        gTransportTypeA = U_GNSS_TRANSPORT_NMEA_UART;
    } else if (gTransportTypeA == U_GNSS_TRANSPORT_UBX_I2C) {
        gTransportTypeA = U_GNSS_TRANSPORT_NMEA_I2C;
    }
    errorCode = uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                         gTransportTypeA,
                         transportHandleA,
                         -1, false, &gnssHandleA);
    U_PORT_TEST_ASSERT_EQUAL((int32_t) U_ERROR_COMMON_SUCCESS, errorCode);
    transportType = U_GNSS_TRANSPORT_NONE;
    transportHandle.uart = -1;
    transportHandle.i2c = -1;
    U_PORT_TEST_ASSERT(uGnssGetTransportHandle(gnssHandleA,
                                               &transportType,
                                               &transportHandle) == 0);
    switch (gTransportTypeA) {
        case U_GNSS_TRANSPORT_NMEA_UART:
            U_PORT_TEST_ASSERT(transportType == U_GNSS_TRANSPORT_NMEA_UART);
            U_PORT_TEST_ASSERT(transportHandle.uart == transportHandleA.uart);
            break;
        case U_GNSS_TRANSPORT_NMEA_I2C:
            U_PORT_TEST_ASSERT(transportType == U_GNSS_TRANSPORT_NMEA_I2C);
            U_PORT_TEST_ASSERT(transportHandle.i2c == transportHandleA.i2c);
            break;
        default:
            U_PORT_TEST_ASSERT(false);
            break;
    }

    U_TEST_PRINT_LINE("deinitialising GNSS API...");
    uGnssDeinit();

    U_TEST_PRINT_LINE("removing stream...");
    switch (gTransportTypeA) {
        case U_GNSS_TRANSPORT_UBX_UART:
        //lint -fallthrough
        case U_GNSS_TRANSPORT_NMEA_UART:
            uPortUartClose(gStreamAHandle);
            break;
        case U_GNSS_TRANSPORT_UBX_I2C:
        //lint -fallthrough
        case U_GNSS_TRANSPORT_NMEA_I2C:
            uPortI2cClose(gStreamAHandle);
            break;
        default:
            break;
    }
    gStreamAHandle = -1;

# if (U_CFG_TEST_UART_B >= 0)
    uPortUartClose(gUartBHandle);
    gUartBHandle = -1;
# endif

    uPortI2cDeinit();
    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
# else
    (void) heapUsed;
# endif
}
#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssCleanUp")
{
    int32_t x;

    uGnssDeinit();
    if (gStreamAHandle >= 0) {
        switch (gTransportTypeA) {
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
                uPortUartClose(gStreamAHandle);
                break;
            case U_GNSS_TRANSPORT_UBX_I2C:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_I2C:
                uPortI2cClose(gStreamAHandle);
                break;
            default:
                break;
        }
    }
    if (gUartBHandle >= 0) {
        uPortUartClose(gUartBHandle);
    }

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortI2cDeinit();
    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
