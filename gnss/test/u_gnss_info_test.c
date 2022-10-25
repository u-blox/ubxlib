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
 * @brief Tests for the GNSS info API: these should pass on all
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

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_info.h"
#include "u_gnss_cfg.h" // uGnssCfgSetUtcStandard()
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_INFO_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES
/** The maximum size of a version string we test.
 */
# define U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES 1024
#endif

#ifndef U_GNSS_TEST_MIN_UTC_TIME
/** A minimum value for UTC time to test against (21 July 2021 13:40:36).
 */
# define U_GNSS_TEST_MIN_UTC_TIME 1626874836
#endif

#ifndef U_GNSS_TIME_TEST_TIMEOUT_SECONDS
/** The timeout on establishing UTC time.
 */
#define U_GNSS_TIME_TEST_TIMEOUT_SECONDS 180
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

/** Pull static info from a GNSS chip.
 */
U_PORT_TEST_FUNCTION("[gnssInfo]", "gnssInfoStatic")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    char *pBuffer;
    int32_t y;
    size_t z;
    char *pTmp;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t w = 0; w < iterations; w++) {
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

        pBuffer = (char *) pUPortMalloc(U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        U_PORT_TEST_ASSERT(pBuffer != NULL);
        // Ask for firmware version string with insufficient storage
        memset(pBuffer, 0x66, U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        y = uGnssInfoGetFirmwareVersionStr(gnssHandle, pBuffer, 0);
        U_PORT_TEST_ASSERT(y == 0);
        for (size_t x = 0; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }
        y = uGnssInfoGetFirmwareVersionStr(gnssHandle, pBuffer, 1);
        U_PORT_TEST_ASSERT(y == 0);
        U_PORT_TEST_ASSERT(*pBuffer == 0);
        for (size_t x = 1; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }

        // Now with hopefully sufficient storage
        y = uGnssInfoGetFirmwareVersionStr(gnssHandle, pBuffer,
                                           U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        U_PORT_TEST_ASSERT(y > 0);
        U_PORT_TEST_ASSERT(y < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        for (size_t x = y + 1; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }
        // The string returned contains multiple lines separated by more than one
        // null terminator; try to print it nicely here.
        U_TEST_PRINT_LINE("GNSS chip version string is:");
        pTmp = pBuffer;
        while (pTmp < pBuffer + y) {
            z = strlen(pTmp);
            if (z > 0) {
                U_TEST_PRINT_LINE("\"%s\".", pTmp);
                pTmp += z;
            } else {
                pTmp++;
            }
        }

        // Ask for the chip ID string with insufficient storage
        memset(pBuffer, 0x66, U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        y = uGnssInfoGetIdStr(gnssHandle, pBuffer, 0);
        U_PORT_TEST_ASSERT(y == 0);
        for (size_t x = 0; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }
        y = uGnssInfoGetIdStr(gnssHandle, pBuffer, 1);
        U_PORT_TEST_ASSERT(y == 0);
        U_PORT_TEST_ASSERT(*pBuffer == 0);
        for (size_t x = 1; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }

        // Now with hopefully sufficient storage
        y = uGnssInfoGetIdStr(gnssHandle, pBuffer, U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        U_PORT_TEST_ASSERT(y > 0);
        U_PORT_TEST_ASSERT(y < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES);
        uPortLog(U_TEST_PREFIX "GNSS chip ID string is 0x");
        for (size_t x = 0; x < (size_t) y; x++) {
            uPortLog("%02x", *(pBuffer + x));
        }
        uPortLog(".\n");
        for (size_t x = y + 1; x < U_GNSS_INFO_TEST_VERSION_SIZE_MAX_BYTES; x++) {
            U_PORT_TEST_ASSERT(*(pBuffer + x) == 0x66);
        }

        uGnssVersionType_t version;
        y = uGnssInfoGetVersions(gnssHandle, &version);
        U_PORT_TEST_ASSERT(y == 0);
        U_PORT_TEST_ASSERT(strlen(version.ver) > 0);
        U_PORT_TEST_ASSERT(strlen(version.hw) > 0);
        uPortLog(U_TEST_PREFIX "GNSS ver:\"%s\" hw:\"%s\" rom:\"%s\" fw:\"%s\" prot:\"%s\" mod:\"%s\"\n",
                 version.ver, version.hw,
                 version.rom, version.fw,
                 version.prot, version.mod);

        // Free memory
        uPortFree(pBuffer);

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Read time from GNSS.
 */
U_PORT_TEST_FUNCTION("[gnssInfo]", "gnssInfoTime")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    int64_t y = -1;
    int32_t startTimeMs;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t w = 0; w < iterations; w++) {
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

        // Set the UTC time standard to automatic, just in case
        U_TEST_PRINT_LINE("setting UTC standard to automatic..\n");
        U_PORT_TEST_ASSERT(uGnssCfgSetUtcStandard(gnssHandle, U_GNSS_UTC_STANDARD_AUTOMATIC) == 0);

        // Ask for time, allowing a few tries in case the GNSS receiver
        // has not yet found time
        U_TEST_PRINT_LINE("waiting up to %d second(s) to establish UTC time...",
                          U_GNSS_TIME_TEST_TIMEOUT_SECONDS);
        startTimeMs = uPortGetTickTimeMs();
        while ((y < 0) &&
               (uPortGetTickTimeMs() - startTimeMs < (U_GNSS_TIME_TEST_TIMEOUT_SECONDS * 1000))) {
            y = uGnssInfoGetTimeUtc(gnssHandle);
        }
        if (y > 0) {
            U_TEST_PRINT_LINE("UTC time according to GNSS is %d (took %d second(s)"
                              " to establish).", (int32_t) y,
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000);
        } else {
            U_TEST_PRINT_LINE("could not get UTC time from GNSS after %d second(s) (%d).",
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs) / 1000,
                              (int32_t) y);
        }
        U_PORT_TEST_ASSERT(y > U_GNSS_TEST_MIN_UTC_TIME);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
