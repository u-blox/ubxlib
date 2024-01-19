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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff. Anything required from the
 * platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the cellular PPP API: these should pass on all
 * platforms where CMUX is also supported. They are only compiled
 * if U_CFG_TEST_CELL_MODULE_TYPE is defined and U_CFG_TEST_DISABLE_MUX
 * is NOT defined.
 */

#if defined(U_CFG_TEST_CELL_MODULE_TYPE) && !defined(U_CFG_TEST_DISABLE_MUX)

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
#include "u_port_os.h"   // Required by u_cell_private.h
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
# include "u_port_i2c.h"
# include "u_port_spi.h"
#endif
#include "u_port_heap.h"
#include "u_port_event_queue.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#include "u_at_client.h"

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
# include "u_network.h"
# include "u_network_test_shared_cfg.h"
# include "u_location.h"
#endif

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_info.h"    // For uCellInfoGetModelStr()
#if U_CFG_APP_PIN_CELL_PWR_ON < 0
# include "u_cell_pwr.h"
#endif
#ifdef U_CELL_TEST_MUX_ALWAYS
# include "u_cell_mux.h"
#endif

#include "u_cell_ppp_shared.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
# include "u_gnss_module_type.h"
# include "u_gnss_type.h"
# include "u_gnss.h"         // uGnssSetUbxMessagePrint()
# include "u_gnss_pos.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_PPP_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_CELL_PPP_TEST_TIMEOUT_SECONDS
/** How long to wait for uCellPppOpen() to connect.
 */
# define U_CELL_PPP_TEST_TIMEOUT_SECONDS 60
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Handle.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** A small buffer to check that static buffers don't blow things
 * up.
 */
static char gBuffer[16];

/* ----------------------------------------------------------------
* STATIC FUNCTIONS
* -------------------------------------------------------------- */

// Callback function for the cellular connection process.
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Callback for received data; doesn't do much.
static void receiveDataCallback(uDeviceHandle_t cellHandle,
                                const char *pData,
                                size_t dataSize,
                                void *pCallbackParam)
{
    (void) cellHandle;
    (void) pData;
    (void) dataSize;
    (void) pCallbackParam;
}

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// Network-API level bring up, used when addressing the GNSS chip inside a cellular module
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    // Add the device for each network configuration
    // if not already added
    pList = pUNetworkTestListAlloc(uNetworkTestIsDeviceCell);
    if (pList == NULL) {
        U_TEST_PRINT_LINE("*** WARNING *** nothing to do.");
    }
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Bring up each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(*pTmp->pDevHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);
    }

    return pList;
}

// Convert a lat/long into a whole number and a bit-after-the-decimal-point
// that can be printed by a version of printf() that does not support
// floating point operations, returning the prefix (either "+" or "-").
// The result should be printed with printf() format specifiers
// %c%d.%07d, e.g. something like:
//
// int32_t whole;
// int32_t fraction;
//
// printf("%c%d.%07d/%c%d.%07d", latLongToBits(latitudeX1e7, &whole, &fraction),
//                               whole, fraction,
//                               latLongToBits(longitudeX1e7, &whole, &fraction),
//                               whole, fraction);
static char latLongToBits(int32_t thingX1e7,
                          int32_t *pWhole,
                          int32_t *pFraction)
{
    char prefix = '+';

    // Deal with the sign
    if (thingX1e7 < 0) {
        thingX1e7 = -thingX1e7;
        prefix = '-';
    }
    *pWhole = thingX1e7 / 10000000;
    *pFraction = thingX1e7 % 10000000;

    return prefix;
}

// Print lat/long location as a clickable link.
static void printLocation(int32_t latitudeX1e7, int32_t longitudeX1e7)
{
    char prefixLat;
    char prefixLong;
    int32_t wholeLat;
    int32_t wholeLong;
    int32_t fractionLat;
    int32_t fractionLong;

    prefixLat = latLongToBits(latitudeX1e7, &wholeLat, &fractionLat);
    prefixLong = latLongToBits(longitudeX1e7, &wholeLong, &fractionLong);
    uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
             prefixLat, wholeLat, fractionLat, prefixLong, wholeLong,
             fractionLong);
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

/* ----------------------------------------------------------------
* PUBLIC FUNCTIONS
* -------------------------------------------------------------- */

/** A very basic test of PPP operation indeed; most of the real
 * testing is done in the platform tests.
 */
U_PORT_TEST_FUNCTION("[cellPpp]", "cellPppBasic")
{
    int32_t resourceCount;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    char buffer[64];
    int32_t x;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Only run if PPP operation is supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_PPP)) {
        U_TEST_PRINT_LINE("testing PPP, first with no connection.");
        // First check before having connected: should return error
        gStopTimeMs = uPortGetTickTimeMs() + (U_CELL_PPP_TEST_TIMEOUT_SECONDS * 1000);
        x = uCellPppOpen(cellHandle, NULL, NULL, gBuffer,
                         sizeof(gBuffer), keepGoingCallback);
        U_TEST_PRINT_LINE("uCellPppOpen() returned %d.", x);
        U_PORT_TEST_ASSERT(x < 0);
        x = uCellPppTransmit(cellHandle, "dummy", 5);
        U_TEST_PRINT_LINE("uCellPppTransmit() returned %d.", x);
        U_PORT_TEST_ASSERT(x < 0);

        U_TEST_PRINT_LINE("now with a connection.");
        // Now connect
        gStopTimeMs = uPortGetTickTimeMs() +
                      (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
        x = uCellNetConnect(cellHandle, NULL,
#ifdef U_CELL_TEST_CFG_APN
                            U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
#else
                            NULL,
#endif
#ifdef U_CELL_TEST_CFG_USERNAME
                            U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
#else
                            NULL,
#endif
#ifdef U_CELL_TEST_CFG_PASSWORD
                            U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
#else
                            NULL,
#endif
                            keepGoingCallback);
        U_PORT_TEST_ASSERT (x == 0);

        gStopTimeMs = uPortGetTickTimeMs() + (U_CELL_PPP_TEST_TIMEOUT_SECONDS * 1000);
        x = uCellPppOpen(cellHandle, NULL, NULL, gBuffer,
                         sizeof(gBuffer), keepGoingCallback);
        U_TEST_PRINT_LINE("uCellPppOpen() returned %d.", x);
        U_PORT_TEST_ASSERT(x == 0);

        x = uCellPppTransmit(cellHandle, "dummy", 5);
        U_TEST_PRINT_LINE("uCellPppTransmit() returned %d.", x);
        U_PORT_TEST_ASSERT(x == 5);

        // Check that we can still do normal AT things
        memset(buffer, 0, sizeof(buffer));
        x = uCellInfoGetModelStr(cellHandle, buffer, sizeof(buffer));
        U_PORT_TEST_ASSERT((x > 0) && (x < sizeof(buffer) - 1) &&
                           (x == strlen(buffer)));

        U_TEST_PRINT_LINE("closing PPP (there will be a delay)...");
        U_PORT_TEST_ASSERT(uCellPppClose(cellHandle, true) == 0);

        // Disconnect
        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

    } else {
        U_TEST_PRINT_LINE("PPP is not supported, not testing it.");
        U_PORT_TEST_ASSERT(uCellPppOpen(cellHandle, receiveDataCallback,
                                        NULL, NULL,
                                        U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                        keepGoingCallback) < 0);
        U_PORT_TEST_ASSERT(uCellPppTransmit(cellHandle, "dummy", 5) < 0);
        U_PORT_TEST_ASSERT(uCellPppClose(cellHandle, false) < 0);
        uCellPppFree(cellHandle);
    }

    // Do the standard postamble, also powering the module down
    // as otherwise SARA-R5 can get upset since the PPP close
    // we do directly here is not coordinated with the underlying
    // PPP and so it probably won't have closed the module's PPP
    // connection up nicely.
    uCellTestPrivatePostamble(&gHandles, true);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

/** Test that GNSS access can run at the same time as PPP.
 */
U_PORT_TEST_FUNCTION("[cellPpp]", "cellPppWithGnss")
{
    uNetworkTestList_t *pList;
    const uCellPrivateModule_t *pModule;
    uDeviceHandle_t cellHandle = NULL;
    uDeviceHandle_t gnssHandle = NULL;
    int32_t resourceCount;
    int32_t x;
    uLocation_t location;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // Obtain the initial resource count
    resourceCount = uTestUtilGetDynamicResourceCount();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    // Don't check these for success as not all platforms support I2C or SPI
    uPortI2cInit();
    uPortSpiInit();
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Do the preamble to get all the networks up
    pList = pStdPreamble();

    // Find the cellular device and the GNSS network in the list
    for (uNetworkTestList_t *pTmp = pList; (pTmp != NULL) && (gnssHandle == NULL); pTmp = pTmp->pNext) {
        if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) {
            cellHandle = *pTmp->pDevHandle;
            if (pTmp->networkType == U_NETWORK_TYPE_GNSS) {
                gnssHandle = *pTmp->pDevHandle;
                U_TEST_PRINT_LINE("selected GNSS network via cellular device.");
            }
        }
    }

    if (gnssHandle != NULL) {
        U_PORT_TEST_ASSERT(cellHandle != NULL);

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssHandle, true);

        // Get the private module data as we need it for testing
        pModule = pUCellPrivateGetModule(cellHandle);
        U_PORT_TEST_ASSERT(pModule != NULL);
        //lint -esym(613, pModule) Suppress possible use of NULL pointer
        // for pModule from now on

        // Only run if PPP operation is supported
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_PPP)) {
            U_TEST_PRINT_LINE("testing PPP and GNSS at the same time.");
            x = uCellPppOpen(cellHandle, NULL, NULL, gBuffer, sizeof(gBuffer), NULL);
            U_TEST_PRINT_LINE("uCellPppOpen() returned %d.", x);
            U_PORT_TEST_ASSERT(x == 0);

            // Now get location
            x = uLocationGet(gnssHandle, U_LOCATION_TYPE_GNSS,
                             NULL, NULL, &location, NULL);
            U_TEST_PRINT_LINE("uLocationGet() returned %d.", x);
            U_PORT_TEST_ASSERT(x == 0);
            printLocation(location.latitudeX1e7, location.longitudeX1e7);

            x = uCellPppTransmit(cellHandle, "dummy", 5);
            U_TEST_PRINT_LINE("uCellPppTransmit() returned %d.", x);
            U_PORT_TEST_ASSERT(x == 5);

            U_TEST_PRINT_LINE("closing PPP (there will be a delay)...");
            U_PORT_TEST_ASSERT(uCellPppClose(cellHandle, true) == 0);
        }

        // Call PPP free this time
        uCellPppFree(cellHandle);

    } else {
        U_TEST_PRINT_LINE("*** WARNING *** not testing GNSS at the same time"
                          " as PPP since no GNSS device is attached via cellular.");
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing and powering off device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            x = uDeviceClose(*pTmp->pDevHandle, true);
            if (x != 0) {
                // Device has not responded to power off request, just
                // release resources
                x = uDeviceClose(*pTmp->pDevHandle, false);
            }
            U_PORT_TEST_ASSERT(x == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uPortEventQueueCleanUp();

    uDeviceDeinit();
    uPortSpiDeinit();
    uPortI2cDeinit();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellPpp]", "cellPppCleanUp")
{
    uPortEventQueueCleanUp();
    uCellTestPrivateCleanup(&gHandles);
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
#endif
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #if defined(U_CFG_TEST_CELL_MODULE_TYPE) && !defined(U_CFG_TEST_DISABLE_MUX)

// End of file
