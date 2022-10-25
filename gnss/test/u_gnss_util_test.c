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
 * @brief Tests for the GNSS utilities API: these should pass on all
 * platforms that have a GNSS module connected to them.  They
 * are only compiled if U_CFG_TEST_GNSS_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_info.h"  // For uGnssInfoGetFirmwareVersionStr()
#include "u_gnss_util.h"
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_UTIL_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES
/** The maximum size of a version string.
 */
# define U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES 1024
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Exchange transparent UBX message with the GNSS chip.
 */
U_PORT_TEST_FUNCTION("[gnssUtil]", "gnssUtilTransparent")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    char *pBuffer1;
    char *pBuffer2;
    char *pTmp;
    int32_t y;
    int32_t x;
    size_t z;
    // Enough room to encode the poll for a UBX-MON-VER message
    char command[U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES];
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t w = 0; w < iterations; w++) {
        // Can't do this when the NMEA stream is active, just too messy
        if ((transportTypes[w] != U_GNSS_TRANSPORT_NMEA_UART) &&
            (transportTypes[w] != U_GNSS_TRANSPORT_NMEA_I2C)) {
            // Do the standard preamble
            U_TEST_PRINT_LINE("testing on transport %s...",
                              pGnssTestPrivateTransportTypeName(transportTypes[w]));
            U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                        transportTypes[w], &gHandles, true,
                                                        U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                        U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
            gnssHandle = gHandles.gnssHandle;

            // So that we can see what we're doing
            uGnssSetUbxMessagePrint(gnssHandle, true);

            pBuffer1 = (char *) pUPortMalloc(U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES);
            U_PORT_TEST_ASSERT(pBuffer1 != NULL);
            pBuffer2 = (char *) pUPortMalloc(U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES);
            U_PORT_TEST_ASSERT(pBuffer2 != NULL);

            // Ask for the firmware version string in the normal way
            U_TEST_PRINT_LINE("getting the version string the normal way...");
            y = uGnssInfoGetFirmwareVersionStr(gnssHandle, pBuffer1,
                                               U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES);
            U_PORT_TEST_ASSERT(y > 0);

            // Now manually encode a request for the version string using the
            // message class and ID of the UBX-MON-VER command
            memset(pBuffer2, 0x66, U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES);
            x = uUbxProtocolEncode(0x0a, 0x04, NULL, 0, command);
            U_PORT_TEST_ASSERT(x == sizeof(command));
            U_TEST_PRINT_LINE("getting the version string using transparent API...");
            x = uGnssUtilUbxTransparentSendReceive(gnssHandle,
                                                   command, sizeof(command),
                                                   pBuffer2,
                                                   U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES);
            U_TEST_PRINT_LINE("%d byte(s) returned.", x);
            U_PORT_TEST_ASSERT(x == y + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
            for (z = x + 1; x < U_GNSS_UTIL_TEST_VERSION_SIZE_MAX_BYTES; x++) {
                U_PORT_TEST_ASSERT(*(pBuffer2 + z) == 0x66);
            }
            U_PORT_TEST_ASSERT(*(pBuffer2) == 0xb5);
            U_PORT_TEST_ASSERT(*(pBuffer2 + 1) == 0x62);
            U_PORT_TEST_ASSERT(*(pBuffer2 + 2) == 0x0a);
            U_PORT_TEST_ASSERT(*(pBuffer2 + 3) == 0x04);
            x = uUbxProtocolUint16Decode(pBuffer2 + 4);
            U_PORT_TEST_ASSERT(x == y);

            // The string returned contains multiple lines separated by more than one
            // null terminator; try to print it nicely here.
            U_TEST_PRINT_LINE("GNSS chip version string is:");
            pTmp = pBuffer2 + 6;  // Skip 0xb5 0x62, message class/ID and length bytes
            while (pTmp < pBuffer2 + y) {
                z = strlen(pTmp);
                if (z > 0) {
                    U_TEST_PRINT_LINE("\"%s\".", pTmp);
                    pTmp += z;
                } else {
                    pTmp++;
                }
            }

            // Check that the bodies are the same
            U_PORT_TEST_ASSERT(memcmp(pBuffer1, pBuffer2 + 6,
                                      x - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) == 0);

            // Repeat but ignore the body this time
            x = uUbxProtocolEncode(0x0a, 0x04, NULL, 0, command);
            U_PORT_TEST_ASSERT(x == sizeof(command));
            U_TEST_PRINT_LINE("get version string and ignore response with the transparent API...");
            x = uGnssUtilUbxTransparentSendReceive(gnssHandle,
                                                   command, sizeof(command),
                                                   NULL, 0);
            U_TEST_PRINT_LINE("%d byte(s) returned.", x);
            U_PORT_TEST_ASSERT(x == 0);

            // Free memory
            uPortFree(pBuffer2);
            uPortFree(pBuffer1);

            // Do the standard postamble, but this time power the
            // module off as otherwise the response to the last
            // version string request will still be sitting in the
            // GNSS chip's buffer when the next test starts.
            uGnssTestPrivatePostamble(&gHandles, true);
        }
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssUtil]", "gnssUtilCleanUp")
{
    int32_t x;

    uGnssTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
