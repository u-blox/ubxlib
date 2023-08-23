/*
 * Copyright 2019-2023 u-blox
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
 * @brief Tests for the GNSS multiple-GNSS assistance (AKA AssistNow) API:
 * these should pass on all platforms that have a GNSS module connected to
 * them, an authentication token (U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN)
 * defined and U_CFG_TEST_GNSS_ASSIST_NOW defined.  Since the tests require
 * an HTTP connection to talk with the AssistNow service, one of either
 * U_CFG_TEST_CELL_MODULE_TYPE or U_CFG_TEST_SHORT_RANGE_MODULE_TYPE must
 * also be defined.
 *
 * Note: unlike the other GNSS tests, this opens devices using the device
 * and network APIs since, as well as GNSS, it needs to find at least one
 * HTTP(S) transport to do the communication with the AssistNow servers.
 *
 * Note: while we allow Wifi as well as cellular here, it is possible that
 * the length limitations on HTTP responses over the Wifi HTTP API mean
 * that all of the tests below would not pass.
 *
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
#include "string.h"    // strlen()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_at_client.h" // Required by u_gnss_private.h

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_i2c.h"
#include "u_port_spi.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_security.h"
#include "u_security_tls.h"

#include "u_http_client.h"

#include "u_cell_info.h"  // uCellInfoGetTimeUtc()
#include "u_cell_net.h"   // Required by u_cell_pwr.h
#include "u_cell_pwr.h"   // uCellPwrReboot()

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_mga.h"
#include "u_gnss_pwr.h"     // U_GNSS_RESET_TIME_SECONDS
#include "u_gnss_msg.h"     // uGnssMsgReceiveStatStreamLoss(), uGnssMsgSend()
#include "u_gnss_info.h"    // uGnssInfoGetCommunicationStats()
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_GNSS_MGA_TEST"

/** The string to put at the start of all prints from this test
 * that do not require any iterations on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration(s) version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and an iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_MGA_TEST_HTTP_SERVER_ONLINE
/** The full URL of the AssistNow Online server to use during testing.
 */
# define U_GNSS_MGA_TEST_HTTP_SERVER_ONLINE U_GNSS_MGA_HTTP_SERVER_ONLINE
#endif

#ifndef U_GNSS_MGA_TEST_HTTP_SERVER_OFFLINE
/** The full URL of the AssistNow Offline server to use during testing.
 */
# define U_GNSS_MGA_TEST_HTTP_SERVER_OFFLINE U_GNSS_MGA_HTTP_SERVER_OFFLINE
#endif

#ifndef U_GNSS_MGA_TEST_HTTP_BUFFER_OUT_LENGTH_BYTES
/** The maximum buffer size to encode an AssistNow request into.
 */
# define U_GNSS_MGA_TEST_HTTP_BUFFER_OUT_LENGTH_BYTES 256
#endif

#ifndef U_GNSS_MGA_TEST_HTTP_BUFFER_IN_LENGTH_BYTES
/** The maximum buffer size for the HTTP response.
 */
# define U_GNSS_MGA_TEST_HTTP_BUFFER_IN_LENGTH_BYTES (5 * 1024)
#endif

#ifndef U_GNSS_MGA_TEST_DATABASE_LENGTH_BYTES
/** Size of a buffer to hold the database from a GNSS device.
 */
# define U_GNSS_MGA_TEST_DATABASE_LENGTH_BYTES (10 * 1024)
#endif

#ifndef U_GNSS_MGA_TEST_MY_LOCATION
/** Location to filter AssistNow Online requests: set this to your
 * test system's location. */
# define U_GNSS_MGA_TEST_MY_LOCATION {522227359, /* lat */ 748165, /* long */ 83123, /* altitude */ 20000, /* radius */}
#endif

#ifndef U_GNSS_MGA_TEST_HTTP_GET_RETRIES
/** How many times to retry a HTTP GET request on failure,
 * which might be because we're crowding-out the server.
 */
# define U_GNSS_MGA_TEST_HTTP_GET_RETRIES 3
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Array of online/offline requests to test.
 */
typedef struct {
    bool isOnlineNotOffline;
    union {
        uGnssMgaOnlineRequest_t online;
        uGnssMgaOfflineRequest_t offline;
    };
    int32_t expectedOutcome;
    uGnssMgaSendOfflineOperation_t offlineOperation;
} uGnssMgaTest_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** A place to hook the database buffer.
 */
static char *gpDatabase = NULL;

/** A place to hook our outgoing HTTP buffer, the one to encode into.
 */
static char *gpHttpBufferOut = NULL;

/** A place to hook the HTTP response buffer.
 */
static char *gpHttpBufferIn = NULL;

/** Position to use when filtering AssistNow Online.
 */
static const uGnssMgaPos_t gMgaPosFilter = U_GNSS_MGA_TEST_MY_LOCATION;

# ifndef U_GNSS_MGA_TEST_DISABLE_DATABASE
/** A count of how many times databaseCallback() has been called.
 */
static size_t gDatabaseCalledCount = 0;

/** The names of the flow control types; must have the same number of
 * members as gFlowControlList and match the order.
 */
static const char *gpFlowControlNameList[] = {"no", "ack/nack", "smart"};

/** The types of flow control to use with the GNSS chip while downloading;
 * must have the same number of members as gpFlowControlNameList and match
 * the order.
*/
static const uGnssMgaFlowControl_t gFlowControlList[] = {U_GNSS_MGA_FLOW_CONTROL_WAIT,
                                                         U_GNSS_MGA_FLOW_CONTROL_SIMPLE,
                                                         U_GNSS_MGA_FLOW_CONTROL_SMART
                                                        };
# endif

# if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_ASSIST_NOW) && \
     (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

/** Array of requests to test: note that the figures in here are all
 * quite small as there is potentially a lot of data to download and
 * we're running on quite small memory MCUs here.
 */
static const uGnssMgaTest_t gRequestList[] = {
    // Assist Now Online request: this should get us just the time
    {
        .isOnlineNotOffline = true,
        .online = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            0, 0, NULL, 0, 0
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALL // This should be ignored
    },
    // Assist Now Online request: just ephemeris, just GPS
    {
        .isOnlineNotOffline = true,
        .online = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            1ULL << U_GNSS_MGA_DATA_TYPE_EPHEMERIS, 1ULL << U_GNSS_SYSTEM_GPS, NULL, 0, 0
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_TODAYS // This should be ignored
    },
    // Assist Now Offline request: all good, no filtering, minimum everything (at least one system must be specified)
    {
        .isOnlineNotOffline = false,
        .offline = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            false, (1ULL << U_GNSS_SYSTEM_GLONASS), 1, 1
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_FLASH
    },
    // Assist Now Offline request: all good, 2 days, max interval, can't cope with much more
    // data than this in the kind of HTTP buffer sizes we generally have available
    {
        .isOnlineNotOffline = false,
        .offline = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            false, (1ULL << U_GNSS_SYSTEM_GPS), // Different system type
            2, 3
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_TODAYS
    },
    // Assist Now Offline request: all good, with almanac, 2 days, max interval,
    // can't cope with much more data than this in the kind of HTTP buffer sizes we
    // generally have available
    {
        .isOnlineNotOffline = false,
        .offline = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            true, (1ULL << U_GNSS_SYSTEM_GPS), // Different system type
            2, 3
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALMANAC
    },
    // Assist Now Online request: all good
    {
        .isOnlineNotOffline = true,
        .online = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            (1ULL << U_GNSS_MGA_DATA_TYPE_ALMANAC), // Just almanac
            (1ULL << U_GNSS_SYSTEM_BEIDOU), // Different system type
            NULL, 0, 0
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_FLASH // This should be ignored
    },
    // Assist Now Online request: all good, filter on position, add latency
    {
        .isOnlineNotOffline = true,
        .online = {
            U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN),
            0, 0, &gMgaPosFilter, 2000, 4000
        },
        .expectedOutcome = 0,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALL // This should be ignored
    },
    // Assist Now Offline request: error case, no token
    {
        .isOnlineNotOffline = false,
        .offline = U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS,
        .expectedOutcome = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALL
    },
    // Assist Now Online request: error case, no token
    {
        .isOnlineNotOffline = true,
        .online = U_GNSS_MGA_ONLINE_REQUEST_DEFAULTS,
        .expectedOutcome = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER,
        .offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALL
    }
};

/** A place to hook the HTTP client contexts: one for AssistNow
 * Online and one for AssistNow Offline.
 */
static uHttpClientContext_t *gpHttpContext[2] = {0};

/** The amount of data pointed-to by gpHttpBufferIn.
 */
static size_t gHttpBufferInSize = 0;

/** The names of the offline operation types; must have the same number
 * of members as the valid values for uGnssMgaSendOfflineOperation_t.
 */
static const char *gpOfflineOperation[] = {"send everything", "write to flash", "send todays", "send almanac"};

/** The transport type as text: in some cases there is more than one
 * GNSS chip attached so it is useful to know which one we've selected.
 */
static const char *const gpTransportType[] = {"None",       // U_DEVICE_TRANSPORT_TYPE_NONE
                                              "UART",       // U_DEVICE_TRANSPORT_TYPE_UART
                                              "I2C",        // U_DEVICE_TRANSPORT_TYPE_I2C
                                              "SPI",        // U_DEVICE_TRANSPORT_TYPE_SPI
                                              "Virtual Serial" // U_DEVICE_TRANSPORT_TYPE_VIRTUAL_SERIAL
                                             };

# endif // #if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_MGA) &&
// (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

# if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_ASSIST_NOW) && \
     (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

// Print out binary.
static void printHex(const char *pHex, size_t length)
{
#if U_CFG_ENABLE_LOGGING
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pHex++;
        uPortLog("%02x", (unsigned char) c);
    }
#else
    (void) pStr;
    (void) length;
#endif
}

// Do this before every test to bring everything up.
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    // Add the device for each network configuration
    // if not already added
    pList = pUNetworkTestListAlloc(NULL);
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
            // For reasons I don't understand, SARA-R422 will flag an internal error
            // when we get to the HTTP part of test gnssMgaServer() unless it has
            // been freshly powered-on here; hence restart the cellular module
            if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) {
                U_PORT_TEST_ASSERT(uCellPwrReboot(*pTmp->pDevHandle, NULL) == 0);
            }
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

    // It is possible for HTTP client closure in an
    // underlying layer to have failed in a previous
    // test, leaving HTTP hanging so, just in case,
    // clear it up here
    for (size_t x = 0; x < sizeof(gpHttpContext) / sizeof(gpHttpContext[0]); x++) {
        if (gpHttpContext[x] != NULL) {
            uHttpClientClose(gpHttpContext[x]);
            gpHttpContext[x] = NULL;
        }
    }

    return pList;
}

# endif // #if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_MGA) &&
// (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

// Callback for progress when sending stuff to the GNSS device.
static bool progressCallback(uDeviceHandle_t devHandle,
                             int32_t errorCode,
                             size_t blocksTotal, size_t blocksSent,
                             void *pProgressCallbackParam)
{
    int32_t paramLocal;

    (void) devHandle;

    if (pProgressCallbackParam != NULL) {
        paramLocal = *((int32_t *) pProgressCallbackParam);
        if (paramLocal >= 0) {
            if (errorCode < 0) {
                paramLocal = errorCode;
            } else if (blocksTotal < blocksSent) {
                paramLocal = -1000000;
            }
        }
        if (paramLocal >= 0) {
            paramLocal++;
        }
        *((int32_t *) pProgressCallbackParam) = paramLocal;
    }

    return true;
}

# ifndef U_GNSS_MGA_TEST_DISABLE_DATABASE
// Callback for database reads.
static bool databaseCallback(uDeviceHandle_t devHandle,
                             const char *pBuffer, size_t size,
                             void *pDatabaseCallbackParam)
{
    bool keepGoing = true;
    int32_t paramLocal;

    (void) devHandle;

    if (pDatabaseCallbackParam != NULL) {
        paramLocal = *((int32_t *) pDatabaseCallbackParam);
        if (paramLocal >= 0) {
            if ((pBuffer == NULL) && (size != 0)) {
                paramLocal = -1;
            }
            if (size > U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES + 2) { // +2 for the length bytes
                paramLocal = -2;
            }
        }
        if (paramLocal >= 0) {
            if (paramLocal + size >= U_GNSS_MGA_TEST_DATABASE_LENGTH_BYTES) {
                keepGoing = false;
            } else if ((gpDatabase != NULL) && (size > 0) && (pBuffer != NULL)) {
                memcpy(gpDatabase + paramLocal, pBuffer, size);
            }
            paramLocal += size;
            gDatabaseCalledCount++;
        }
        *((int32_t *) pDatabaseCallbackParam) = paramLocal;
    }

    return keepGoing;
}
# endif // ifndef U_GNSS_MGA_TEST_DISABLE_DATABASE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test basic MGA things, ones that don't involve talking to a server.
 */
U_PORT_TEST_FUNCTION("[gnssMga]", "gnssMgaBasic")
{
    uDeviceHandle_t gnssDevHandle = NULL;
    int32_t heapUsed;
    int64_t timeUtc = 1685651437; // Chosen randomly
    int32_t callbackParameter;
    int32_t startTimeMs;
    uGnssMgaTimeReference_t timeReference = {U_GNSS_MGA_EXT_INT_0, true, true};
    int32_t y;
    int32_t z;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    uDeviceHandle_t intermediateHandle = NULL;
    // This means software reset, everything except the ephemeris data
    // (in order that there is something left in the navigation database)
    const char reset[] = {0xFE, 0xFF, 0x01, 0x00};
    // Enough room for a UBX-CFG-RST, with a body of reset[] and overheads
    char buffer[4 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES];
    uGnssCommunicationStats_t communicationStats;
    const char *pProtocolName;

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Allocate a buffer to hold the GNSS device database
    gpDatabase = (char *) pUPortMalloc(U_GNSS_MGA_TEST_DATABASE_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpDatabase != NULL);

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
    for (size_t w = 0; w < iterations; w++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[w]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[w], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssDevHandle = gHandles.gnssHandle;

        U_PORT_TEST_ASSERT(uGnssGetIntermediate(gnssDevHandle, &intermediateHandle) == 0);

        if (intermediateHandle == NULL) {
            // If not on Virtual Serial ('cos we shouldn't be resetting an
            // on-board-cellular GNSS chip), reset the GNSS chip here so that
            // the navigation database won't be huge; this improves the
            // stability of testing
            U_TEST_PRINT_LINE("reseting GNSS before starting.");
            U_PORT_TEST_ASSERT(uUbxProtocolEncode(0x06, 0x04, reset, sizeof(reset), buffer) ==  sizeof(buffer));
            if (uGnssMsgSend(gnssDevHandle, buffer, sizeof(buffer)) == sizeof(buffer)) {
                uPortTaskBlock(U_GNSS_RESET_TIME_SECONDS * 1000);
            }
        }

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssDevHandle, true);

        // Check that setting AssistNow Autonomous works
        y = uGnssMgaAutonomousIsOn(gnssDevHandle);
        U_TEST_PRINT_LINE("AssistNow Autonomous is initially %s.", y ? "on" : "off");
#ifndef U_GNSS_MGA_TEST_ASSIST_NOW_AUTONOMOUS_NOT_SUPPORTED
        U_PORT_TEST_ASSERT(uGnssMgaSetAutonomous(gnssDevHandle, !y) == 0);
        z = uGnssMgaAutonomousIsOn(gnssDevHandle);
        U_TEST_PRINT_LINE("AssistNow Autonomous is now %s.", z ? "on" : "off");
        U_PORT_TEST_ASSERT(z != y);
        // Put it back
        U_PORT_TEST_ASSERT(uGnssMgaSetAutonomous(gnssDevHandle, y) == 0);
        z = uGnssMgaAutonomousIsOn(gnssDevHandle);
        U_TEST_PRINT_LINE("AssistNow Autonomous is back to %s.", z ? "on" : "off");
        U_PORT_TEST_ASSERT(z == y);
#endif

        // And check that sending initialisation vales for time and position work
        U_PORT_TEST_ASSERT(uGnssMgaIniTimeSend(gnssDevHandle, -1, 0, NULL) < 0);
        U_PORT_TEST_ASSERT(uGnssMgaIniTimeSend(gnssDevHandle, 0, -1, NULL) < 0);
        y = uGnssMgaIniTimeSend(gnssDevHandle,
                                ((int64_t) timeUtc) * 1000000000LL,
                                60000000000LL, NULL);
        U_TEST_PRINT_LINE("sending initial time with NULL reference point returned %d.\n", y);
        if (transportTypes[w] != U_GNSS_TRANSPORT_AT) {
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            // Not supported on AT transport
            U_PORT_TEST_ASSERT(y < 0);
        }
        y = uGnssMgaIniTimeSend(gnssDevHandle,
                                ((int64_t) timeUtc) * 1000000000LL,
                                60000000000LL, &timeReference);
        U_TEST_PRINT_LINE("sending initial time with reference point returned %d.\n", y);
        if (transportTypes[w] != U_GNSS_TRANSPORT_AT) {
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            // Not supported on AT transport
            U_PORT_TEST_ASSERT(y < 0);
        }
        U_PORT_TEST_ASSERT(uGnssMgaIniPosSend(gnssDevHandle, NULL) < 0);
        y = uGnssMgaIniPosSend(gnssDevHandle, &gMgaPosFilter);
        U_TEST_PRINT_LINE("sending initial position returned %d.\n", y);
        if (transportTypes[w] != U_GNSS_TRANSPORT_AT) {
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            // Not supported on AT transport
            U_PORT_TEST_ASSERT(y < 0);
        }

        // Attempt to erase flash: I've seen this get a NACK when there
        // really is flash to be erased, so try a few times to be sure
        y = -1;
        for (size_t x = 0; (x < 5) && (y < 0); x++) {
            y = uGnssMgaErase(gnssDevHandle);
            U_TEST_PRINT_LINE("attempting to erase flash returned %d.", y);
            uPortTaskBlock(2500);
        }

# ifdef U_GNSS_MGA_TEST_HAS_FLASH
        if (transportTypes[w] != U_GNSS_TRANSPORT_AT) {
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            // Not supported on AT transport
            U_PORT_TEST_ASSERT(y < 0);
        }
# else
        U_PORT_TEST_ASSERT(y < 0);
# endif

# ifndef U_GNSS_MGA_TEST_DISABLE_DATABASE
        callbackParameter = 0;
        gDatabaseCalledCount = 0;
        startTimeMs = uPortGetTickTimeMs();
        if ((transportTypes[w] != U_GNSS_TRANSPORT_AT) && (intermediateHandle == NULL)) {
            U_TEST_PRINT_LINE("reading database from GNSS device.");
            z = uGnssMgaGetDatabase(gnssDevHandle, databaseCallback, &callbackParameter);
            U_TEST_PRINT_LINE("uGnssMgaGetDatabase() returned %d.", z);
            if (callbackParameter >= 0) {
                U_TEST_PRINT_LINE("database callback was called %d times, with a total"
                                  " of %d byte(s) in %d milliseconds.", gDatabaseCalledCount,
                                  callbackParameter, uPortGetTickTimeMs() - startTimeMs);
                U_PORT_TEST_ASSERT(z == callbackParameter);
            } else {
                U_TEST_PRINT_LINE("database callback returned error %d.", callbackParameter);
                U_PORT_TEST_ASSERT(false);
            }
            U_PORT_TEST_ASSERT(z <= U_GNSS_MGA_TEST_DATABASE_LENGTH_BYTES);
            U_PORT_TEST_ASSERT(z >= 0);

            if (callbackParameter > 0) {
                // Now write it back using all of the flow control types
                for (size_t x = 0; x < sizeof(gpFlowControlNameList) / sizeof(gpFlowControlNameList[0]); x++) {
                    U_TEST_PRINT_LINE_X("writing database to GNSS device using %s flow control.",
                                        x + 1, gpFlowControlNameList[x]);
                    callbackParameter = 0;
                    y = uGnssMgaSetDatabase(gnssDevHandle, gFlowControlList[x],
                                            gpDatabase, z, progressCallback, &callbackParameter);
                    if (callbackParameter >= 0) {
                        U_TEST_PRINT_LINE_X("progress callback was called %d time(s).",
                                            x + 1, callbackParameter);
                    } else {
                        U_TEST_PRINT_LINE_X("progress callback returned error %d.",
                                            x + 1, callbackParameter);
                    }
                    U_TEST_PRINT_LINE_X("uGnssMgaSetDatabase() returned %d.", x + 1, y);
                    if (((callbackParameter < 0) || (y == 0)) &&
                        (uGnssInfoGetCommunicationStats(gnssDevHandle, -1, &communicationStats) == 0)) {
                        // Obtain and print the message stats of the GNSS device
                        // in case the failure is because we have stressed it
                        U_TEST_PRINT_LINE_X("communications from the GNSS chip's perspective:", x + 1);
                        U_TEST_PRINT_LINE_X(" %d transmit byte(s) currently pending.", x + 1,
                                            communicationStats.txPendingBytes);
                        U_TEST_PRINT_LINE_X(" %d byte(s) ever transmitted.", x + 1, communicationStats.txBytes);
                        U_TEST_PRINT_LINE_X(" %d%% transmit buffer currently used.", x + 1,
                                            communicationStats.txPercentageUsage);
                        U_TEST_PRINT_LINE_X(" %d%% peak transmit buffer usage.", x + 1,
                                            communicationStats.txPeakPercentageUsage);
                        U_TEST_PRINT_LINE_X(" %d receive byte(s) currently pending.", x + 1,
                                            communicationStats.rxPendingBytes);
                        U_TEST_PRINT_LINE_X(" %d byte(s) ever received.", x + 1, communicationStats.rxBytes);
                        U_TEST_PRINT_LINE_X(" %d%% receive buffer currently used.", x + 1,
                                            communicationStats.rxPercentageUsage);
                        U_TEST_PRINT_LINE_X(" %d%% peak receive buffer usage.", x + 1,
                                            communicationStats.rxPeakPercentageUsage);
                        U_TEST_PRINT_LINE_X(" %d 100 ms interval(s) with receive overrun errors.", x + 1,
                                            communicationStats.rxOverrunErrors);
                        for (size_t a = 0; a < sizeof(communicationStats.rxNumMessages) /
                             sizeof(communicationStats.rxNumMessages[0]); a++) {
                            if (communicationStats.rxNumMessages[a] >= 0) {
                                pProtocolName = pGnssTestPrivateProtocolName((uGnssProtocol_t) a);
                                if (pProtocolName != NULL) {
                                    U_TEST_PRINT_LINE_X(" %d %s message(s) decoded.", x + 1,
                                                        communicationStats.rxNumMessages[a], pProtocolName);
                                } else {
                                    U_TEST_PRINT_LINE_X(" %d protocol %d message(s) decoded.", x + 1,
                                                        communicationStats.rxNumMessages[a], a);
                                }
                            }
                        }
                        U_TEST_PRINT_LINE_X(" %d receive byte(s) skipped.", x + 1,
                                            communicationStats.rxSkippedBytes);
                    }
                    U_PORT_TEST_ASSERT(callbackParameter >= 0);
                    U_PORT_TEST_ASSERT(y == 0);
                }
            } else {
                U_TEST_PRINT_LINE("*** WARNING *** not testing writing database as there is nothing to write.");
            }
        } else {
            // Not supported when connected via an intermediate module
            U_PORT_TEST_ASSERT(uGnssMgaGetDatabase(gnssDevHandle, databaseCallback, &callbackParameter) < 0);
            U_PORT_TEST_ASSERT(callbackParameter == 0);
            U_PORT_TEST_ASSERT(uGnssMgaSetDatabase(gnssDevHandle, U_GNSS_MGA_FLOW_CONTROL_WAIT,
                                                   gpDatabase, 0, progressCallback, &callbackParameter) < 0);
            U_PORT_TEST_ASSERT(callbackParameter == 0);
        }
# endif // #ifdef U_GNSS_MGA_TEST_DISABLE_DATABASE

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssDevHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Free database buffer
    uPortFree(gpDatabase);
    gpDatabase = NULL;

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

# if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_ASSIST_NOW) && \
     (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

/** Test MGA things that talk to a server.
 */
U_PORT_TEST_FUNCTION("[gnssMga]", "gnssMgaServer")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t httpDevHandle = NULL;
    uDeviceHandle_t gnssDevHandle = NULL;
    uHttpClientConnection_t httpConnectionOnline = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    uHttpClientConnection_t httpConnectionOffline = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t httpTlsSettingsOnline = U_SECURITY_TLS_SETTINGS_DEFAULT;
    uSecurityTlsSettings_t httpTlsSettingsOffline = U_SECURITY_TLS_SETTINGS_DEFAULT;
    uHttpClientContext_t *pHttpContext;
    int32_t httpStatusCode;
    int32_t heapUsed;
    uGnssMgaSendOfflineOperation_t offlineOperation;
    int64_t timeUtcMilliseconds = -1;
    int64_t timeUtc = -1;
    const uGnssMgaTest_t *pRequest;
    int32_t encodeResultNullBuffer;
    int32_t encodeResult;
    size_t flowControlIndex = 0;
    int32_t callbackParameter;
    uGnssMgaTimeReference_t timeReference = {U_GNSS_MGA_EXT_INT_0, true, true};
    int32_t y;

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);
    uNetworkTestCleanUp();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    // Don't check these for success as not all platforms support I2C or SPI
    uPortI2cInit();
    uPortSpiInit();
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Do the preamble to get all the networks up
    pList = pStdPreamble();

    // Set up HTTP buffers
    gpHttpBufferOut = (char *) pUPortMalloc(U_GNSS_MGA_TEST_HTTP_BUFFER_OUT_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpHttpBufferOut != NULL);
    gpHttpBufferIn = (char *) pUPortMalloc(U_GNSS_MGA_TEST_HTTP_BUFFER_IN_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpHttpBufferIn != NULL);

    // Find a bearer that supports HTTP(S) and open the connections we need
    httpConnectionOnline.pServerName = U_GNSS_MGA_TEST_HTTP_SERVER_ONLINE;
    httpConnectionOffline.pServerName = U_GNSS_MGA_TEST_HTTP_SERVER_OFFLINE;
    // The offline server requires the server name indication field to be set
    httpTlsSettingsOffline.pSni = U_GNSS_MGA_TEST_HTTP_SERVER_OFFLINE;
    for (uNetworkTestList_t *pTmp = pList; (pTmp != NULL) &&
         (httpDevHandle == NULL); pTmp = pTmp->pNext) {
        if ((pTmp->networkType == U_NETWORK_TYPE_CELL) || (pTmp->networkType == U_NETWORK_TYPE_WIFI)) {
            httpDevHandle = *pTmp->pDevHandle;
            U_TEST_PRINT_LINE("opening HTTPS connection to %s...", httpConnectionOnline.pServerName);
            gpHttpContext[0] = pUHttpClientOpen(httpDevHandle, &httpConnectionOnline, &httpTlsSettingsOnline);
            U_PORT_TEST_ASSERT(gpHttpContext[0] != NULL);
            U_TEST_PRINT_LINE("opening HTTPS connection to %s...", httpConnectionOffline.pServerName);
            gpHttpContext[1] = pUHttpClientOpen(httpDevHandle, &httpConnectionOffline, &httpTlsSettingsOffline);
            U_PORT_TEST_ASSERT(gpHttpContext[1] != NULL);
        }
    }

    // If there is a cellular device in the list, we can use it to obtain
    // the UTC time for adjustment purposes
    for (uNetworkTestList_t *pTmp = pList; (pTmp != NULL) && (timeUtc < 0); pTmp = pTmp->pNext) {
        if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) {
            timeUtc = uCellInfoGetTimeUtc(*pTmp->pDevHandle);
        }
    }
    // If none was found, still need to set something
    if (timeUtc < 0) {
        timeUtc = 1685651437;
    }

    U_PORT_TEST_ASSERT(httpDevHandle != NULL);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Find the GNSS device
    for (uNetworkTestList_t *pTmp = pList; (pTmp != NULL) &&
         (gnssDevHandle == NULL); pTmp = pTmp->pNext) {
        if (pTmp->networkType == U_NETWORK_TYPE_GNSS) {
            gnssDevHandle = *pTmp->pDevHandle;
            U_TEST_PRINT_LINE("selected GNSS network on %s device.",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            if (pTmp->pDeviceCfg->deviceType == U_DEVICE_TYPE_GNSS) {
                U_TEST_PRINT_LINE("GNSS device is connected to this MCU via %s.",
                                  gpTransportType[pTmp->pDeviceCfg->transportType]);
            }
        }
    }

    U_PORT_TEST_ASSERT(gnssDevHandle != NULL);

    // So that we can see what we're doing
    uGnssSetUbxMessagePrint(gnssDevHandle, true);

    // Encode each type, first with a NULL buffer and then with a real buffer,
    // then request the data and forward it to the GNSS device
    for (size_t x = 0; x < sizeof(gRequestList) / sizeof(gRequestList[0]); x++) {
        pRequest = &gRequestList[x];
        memset(gpHttpBufferOut, 0xAA, U_GNSS_MGA_TEST_HTTP_BUFFER_OUT_LENGTH_BYTES);
        if (pRequest->isOnlineNotOffline) {
            encodeResultNullBuffer = uGnssMgaOnlineRequestEncode(&(pRequest->online), NULL, 0);
        } else {
            encodeResultNullBuffer = uGnssMgaOfflineRequestEncode(&(pRequest->offline), NULL, 0);
        }
        U_TEST_PRINT_LINE_X("encoding %s request with a NULL buffer returned %d.", x + 1,
                            pRequest->isOnlineNotOffline ? "online" : "offline",
                            encodeResultNullBuffer);
        if (pRequest->expectedOutcome >= 0) {
            U_PORT_TEST_ASSERT(encodeResultNullBuffer > 0);
            // +2 because there will be a terminator which is not counted in the return value
            // plus another character we use to check that there has been no overrun
            U_PORT_TEST_ASSERT(encodeResultNullBuffer + 2 <= U_GNSS_MGA_TEST_HTTP_BUFFER_OUT_LENGTH_BYTES);
            // Now for real
            timeUtcMilliseconds = -1;
            if (pRequest->isOnlineNotOffline) {
                pHttpContext = gpHttpContext[0];
                // Add x % 2 on the end so that in every other case we can
                // check that the additional byte is untouched
                encodeResult = uGnssMgaOnlineRequestEncode(&(pRequest->online),
                                                           gpHttpBufferOut,
                                                           encodeResultNullBuffer + 1 + (x % 2));
            } else {
                timeUtcMilliseconds = timeUtc * 1000;
                pHttpContext = gpHttpContext[1];
                // This one gets the exact size
                encodeResult = uGnssMgaOfflineRequestEncode(&(pRequest->offline),
                                                            gpHttpBufferOut,
                                                            encodeResultNullBuffer + 1 + (x % 2));
            }
            U_TEST_PRINT_LINE_X("encoding same request with a real buffer returned %d.", x + 1,
                                encodeResult);
            U_PORT_TEST_ASSERT(encodeResultNullBuffer == encodeResult);
            U_PORT_TEST_ASSERT(encodeResult == strlen(gpHttpBufferOut));
            if (x % 2 > 0) {
                // We gave this one more byte of buffer; check that it is untouched
                U_PORT_TEST_ASSERT(*(gpHttpBufferOut + encodeResult + 1) == 0xAA);
            }
            if (encodeResult >= 0) {
                U_TEST_PRINT_LINE_X("\"%s\".", x + 1, gpHttpBufferOut);
                httpStatusCode = 0;
                for (size_t z = 0; (z < U_GNSS_MGA_TEST_HTTP_GET_RETRIES) && (httpStatusCode != 200); z++) {
                    U_TEST_PRINT_LINE_X("sending GET request, try %d...", x + 1, z + 1);
                    gHttpBufferInSize = U_GNSS_MGA_TEST_HTTP_BUFFER_IN_LENGTH_BYTES;
                    httpStatusCode = uHttpClientGetRequest(pHttpContext,
                                                           gpHttpBufferOut, gpHttpBufferIn,
                                                           &gHttpBufferInSize,
                                                           NULL);
                    if (httpStatusCode == 200) {
                        U_TEST_PRINT_LINE_X("%d byte(s) were returned:", x + 1,
                                            gHttpBufferInSize);
                        uPortLog(U_TEST_PREFIX_X, x + 1);
                        printHex(gpHttpBufferIn, gHttpBufferInSize);
                        uPortLog("\n");
                        offlineOperation = pRequest->offlineOperation;
# ifndef U_GNSS_MGA_TEST_HAS_FLASH
                        if (offlineOperation == U_GNSS_MGA_SEND_OFFLINE_FLASH) {
                            offlineOperation = U_GNSS_MGA_SEND_OFFLINE_ALL;
                        }
# endif
                        U_TEST_PRINT_LINE_X("sending %s data to GNSS with %s flow control,"
                                            " offline operation \"%s\"...", x + 1,
                                            pRequest->isOnlineNotOffline ? "online" : "offline",
                                            gpFlowControlNameList[flowControlIndex],
                                            gpOfflineOperation[offlineOperation]);
                        // Now send the data to the GNSS device, cycling
                        // around all of the flow control methods and storing
                        // in flash every other time
                        callbackParameter = 0;
                        y = uGnssMgaResponseSend(gnssDevHandle, timeUtcMilliseconds, 60000,
                                                 offlineOperation,
                                                 gFlowControlList[flowControlIndex],
                                                 gpHttpBufferIn, gHttpBufferInSize,
                                                 progressCallback,
                                                 &callbackParameter);
                        flowControlIndex++;
                        if (flowControlIndex >= sizeof(gFlowControlList) / sizeof(gFlowControlList[0])) {
                            flowControlIndex = 0;
                        }
                        if (callbackParameter >= 0) {
                            U_TEST_PRINT_LINE_X("progress callback was called %d time(s).",
                                                x + 1, callbackParameter);
                        } else {
                            U_TEST_PRINT_LINE_X("progress callback returned error %d.",
                                                x + 1, callbackParameter);
                        }
                        U_TEST_PRINT_LINE_X("final result was %d.", x + 1, y);
                        U_PORT_TEST_ASSERT(callbackParameter >= 0);
                        U_PORT_TEST_ASSERT(y == 0);

                    } else {
                        U_TEST_PRINT_LINE_X("HTTP status code was %d.", x + 1,
                                            httpStatusCode);
                        if (z < U_GNSS_MGA_TEST_HTTP_GET_RETRIES - 1) {
                            // We might be being told to back off, so wait quite a bit
                            U_TEST_PRINT_LINE_X("server doesn't like us, pausing for a while.", x + 1);
                            uPortTaskBlock(30000);
                        }
                    }
                }
                U_PORT_TEST_ASSERT((httpStatusCode == 200) && (gHttpBufferInSize > 0));
                // Wait between server requests to stop us being banned
                U_TEST_PRINT_LINE_X("pausing for a few seconds.", x + 1);
                uPortTaskBlock(5000);
            }
        } else {
            U_PORT_TEST_ASSERT(encodeResultNullBuffer == pRequest->expectedOutcome);
        }
    }

    // Free HTTP buffers
    uPortFree(gpHttpBufferIn);
    gpHttpBufferIn = NULL;
    uPortFree(gpHttpBufferOut);
    gpHttpBufferOut = NULL;

    // Check that we haven't dropped any incoming data
    y = uGnssMsgReceiveStatStreamLoss(gnssDevHandle);
    U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
    U_PORT_TEST_ASSERT(y == 0);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);

    U_TEST_PRINT_LINE("closing HTTPS connections...");
    for (size_t x = 0; x < sizeof(gpHttpContext) / sizeof(gpHttpContext[0]); x++) {
        uHttpClientClose(gpHttpContext[x]);
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortSpiDeinit();
    uPortI2cDeinit();
    uPortDeinit();
}

# endif // #if defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_MGA) &&
// (defined(U_CFG_TEST_CELL_MODULE_TYPE) || defined(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE))

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssMga]", "gnssMgaCleanUp")
{
    int32_t x;

    uPortFree(gpHttpBufferIn);
    uPortFree(gpHttpBufferOut);
    uPortFree(gpDatabase);

    uGnssTestPrivateCleanup(&gHandles);

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at the"
                          " end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #if defined(U_CFG_TEST_GNSS_MODULE_TYPE)

// End of file
