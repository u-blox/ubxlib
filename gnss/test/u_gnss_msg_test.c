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
 * @brief Tests for the GNSS message API: these should pass on all
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

#include "stdlib.h"    // malloc()/free()
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
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"   // For uGnssCfgSetProtocolOut()
#include "u_gnss_info.h"  // For uGnssInfoGetFirmwareVersionStr() and uGnssInfoGetCommunicationStats()
#include "u_gnss_pos.h"   // For uGnssPosGetStart()
#include "u_gnss_msg.h"
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_MSG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES
/** The maximum size of message to exchange in the blocking test.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES 1024
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TIMEOUT_MS
/** How long to wait for a message receive to return in the blocking
 * test, fairly generous in case lots of NMEA messages are being sent.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TIMEOUT_MS 10000
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES
/** A sensible default buffer size for the message receive non-blocking
 * test.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES 1024
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TASK_THRESHOLD_BYTES
/** The minimum amount of stack we want reserved for the user in the
 * non-blocking message receive task.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TASK_THRESHOLD_BYTES 512
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX
/** The minimum number of ubx messages we expect each message receiver
 * to receive during the non-blocking test.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX 15
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_NMEA
/** The minimum number of NMEA messages we expect each message receiver
 * to receive during the non-blocking test.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_NMEA 300
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POS_DELAY_SECONDS
/** The number of seconds into the non-blocking test that we
 * trigger a position establishment request.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POS_DELAY_SECONDS 5
#endif

#ifndef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS
/** The time to wait between RRLP polls in the non-blocking test in seconds.
 */
# define U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold the data for a non-blocking message reader.
 */
typedef struct {
    int32_t asyncHandle;
    uGnssModuleType_t moduleType;
    char *pBuffer;
    uGnssMessageId_t messageId;
    size_t numReceived;
    size_t numDecodedMin;
    size_t numRead;
    size_t numDecoded;
    size_t numOutsize;
    bool stopped;
    size_t numWhenStopped;
    size_t numNotWanted;
    void *pNmeaComprehenderContext;
    bool nmeaSequenceHasBegun;
    size_t numNmeaSequence;
    size_t numNmeaBadSequence;
} uGnssMsgTestReceive_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int32_t gStopTimeMs;

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** A variable to track errors in the callbacks.
 */
static int32_t gCallbackErrorCode = 0;

#ifndef U_CFG_TEST_USING_NRF5SDK

/** Array of message receivers.
 */
static uGnssMsgTestReceive_t *gpMessageReceive[U_GNSS_MSG_RECEIVER_MAX_NUM] = {0};

/** Timestamp used by posCallback().
 */
static int32_t gPosStopTime = -1;

/** GNSS handle as seen by posCallback().
 */
static uDeviceHandle_t gGnssHandle = NULL;

/** Error code as seen by posCallback().
 */
static volatile int32_t gErrorCode;

/** Latitude as seen by posCallback().
 */
static int32_t gLatitudeX1e7 = INT_MIN;

/** Longitude as seen by posCallback().
 */
static int32_t gLongitudeX1e7 = INT_MIN;

/** Altitude as seen by posCallback().
 */
static int32_t gAltitudeMillimetres = INT_MIN;

/** Radius as seen by posCallback().
 */
static int32_t gRadiusMillimetres = INT_MIN;

/** Speed as seen by posCallback().
 */
static int32_t gSpeedMillimetresPerSecond = INT_MIN;

/** Number of satellites as seen by posCallback().
 */
static int32_t gSvs = 0;

/** Time as seen by posCallback().
 */
static int64_t gTimeUtc = LONG_MIN;

#endif // #ifndef U_CFG_TEST_USING_NRF5SDK 

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for certain cellular network processes.
static bool keepGoingCallback(uDeviceHandle_t gnssHandle)
{
    bool keepGoing = true;

    if (gnssHandle != gHandles.gnssHandle) {
        gCallbackErrorCode = 1;
    }

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Check that a UBX-MON-VER message is as expected.
static void checkMessageReceive(uGnssMessageId_t *pMessageId,
                                char *pBuffer, int32_t size,
                                uint16_t classAndIdExpected,
                                int32_t sizeExpected,
                                char *pBodyExpected)
{
    size_t y;
    char *pTmp;

    U_PORT_TEST_ASSERT(pBuffer != NULL);
    if (pMessageId != NULL) {
        U_TEST_PRINT_LINE("%d byte(s) returned with message class/ID 0x%04x.", size, pMessageId->id.ubx);
        U_PORT_TEST_ASSERT(pMessageId->type == U_GNSS_PROTOCOL_UBX);
        U_PORT_TEST_ASSERT(pMessageId->id.ubx == classAndIdExpected);
    }
    U_PORT_TEST_ASSERT(size == sizeExpected);
    U_PORT_TEST_ASSERT(*pBuffer == 0xb5);
    U_PORT_TEST_ASSERT(*(pBuffer + 1) == 0x62);
    U_PORT_TEST_ASSERT(*(pBuffer + 2) == 0x0a);
    U_PORT_TEST_ASSERT(*(pBuffer + 3) == 0x04);
    size = uUbxProtocolUint16Decode(pBuffer + 4);
    U_PORT_TEST_ASSERT(size == sizeExpected - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);

    // The string returned contains multiple lines separated by more than one
    // null terminator; try to print it nicely here.
    U_TEST_PRINT_LINE("GNSS chip version string is:");
    pTmp = pBuffer + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES;
    while (pTmp < pBuffer + sizeExpected - (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES -
                                            U_UBX_PROTOCOL_HEADER_LENGTH_BYTES)) {
        y = strlen(pTmp);
        if (y > 0) {
            U_TEST_PRINT_LINE("\"%s\".", pTmp);
            pTmp += y;
        } else {
            pTmp++;
        }
    }

    if (pBodyExpected != NULL) {
        // Check that the bodies are the same
        U_PORT_TEST_ASSERT(memcmp(pBodyExpected, pBuffer + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES,
                                  size - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) == 0);
    }
}

// NRF52, which we use NRF5SDK on, doesn't have enough heap for this test
#ifndef U_CFG_TEST_USING_NRF5SDK

// Callback for the non-blocking message receives.
static void messageReceiveCallback(uDeviceHandle_t gnssHandle,
                                   const uGnssMessageId_t *pMessageId,
                                   int32_t errorCodeOrLength,
                                   void *pCallbackParam)
{
    uGnssMsgTestReceive_t *pMsgReceive = (uGnssMsgTestReceive_t *) pCallbackParam;
    int32_t nmeaComprehenderErrorCode;

    if (gnssHandle != gHandles.gnssHandle) {
        gCallbackErrorCode = 1;
    }
    if (pMessageId == NULL) {
        gCallbackErrorCode = 2;
    }
    if (errorCodeOrLength < 0) {
        gCallbackErrorCode = 3;
    }
    if (pCallbackParam == NULL) {
        gCallbackErrorCode = 4;
    }

    if (pMsgReceive != NULL) {
        pMsgReceive->numReceived++;
        if (pMsgReceive->pBuffer == NULL) {
            gCallbackErrorCode = 5;
        }
        if ((pMessageId != NULL) && (pMessageId->type != pMsgReceive->messageId.type)) {
            pMsgReceive->numNotWanted++;
        }
        if ((errorCodeOrLength > 0) &&
            (errorCodeOrLength <= U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES)) {
            if (uGnssMsgReceiveCallbackRead(gnssHandle,
                                            pMsgReceive->pBuffer,
                                            errorCodeOrLength) == errorCodeOrLength) {
                pMsgReceive->numRead++;
                // Deliberately using U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES
                // rather than "size" here to check that uGnssMsgIsGood() ignores
                // any rubbish on the end
                if (uGnssMsgIsGood(pMsgReceive->pBuffer,
                                   U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES)) {
                    pMsgReceive->numDecoded++;
                    // NOTE: uGnssTestPrivateNmeaComprehender() currently only supports
                    // M9, hence this check
                    if ((pMsgReceive->messageId.type == U_GNSS_PROTOCOL_NMEA) &&
                        (pMsgReceive->moduleType == U_GNSS_MODULE_TYPE_M9)) {
#ifdef U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_PRINT
                        // It's often useful to see these messages but the load is
                        // heavy so we don't enable printing unless required
                        U_TEST_PRINT_LINE("%.*s", errorCodeOrLength - 2, pMsgReceive->pBuffer);
#endif
                        // This is an NMEA message, pass it to the comprehender
                        nmeaComprehenderErrorCode = uGnssTestPrivateNmeaComprehender(pMsgReceive->pBuffer,
                                                                                     errorCodeOrLength,
                                                                                     &(pMsgReceive->pNmeaComprehenderContext),
                                                                                     !U_CFG_OS_CLIB_LEAKS);
                        if (pMsgReceive->nmeaSequenceHasBegun) {
                            if (nmeaComprehenderErrorCode == (int32_t) U_ERROR_COMMON_NOT_FOUND) {
                                // NMEA sequence is not as expected
                                pMsgReceive->numNmeaBadSequence++;
                                pMsgReceive->nmeaSequenceHasBegun = false;
                            } else if (nmeaComprehenderErrorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                                // An NMEA sequence has been completed, well done
                                pMsgReceive->nmeaSequenceHasBegun = false;
                            }
                        } else {
                            if (nmeaComprehenderErrorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) {
                                // An NMEA sequence has started
                                pMsgReceive->nmeaSequenceHasBegun = true;
                                pMsgReceive->numNmeaSequence++;
                            }
                        }
                    }
                }
            }
        } else {
            // Not an error: some messages might just be too large
            pMsgReceive->numOutsize++;
        }
        if (pMsgReceive->stopped) {
            pMsgReceive->numWhenStopped++;
        }
    }
}

# ifdef U_GNSS_MSG_TEST_INCLUDE_A_POS
// Callback function for asynchronous position establishment API.
static void posCallback(uDeviceHandle_t gnssHandle,
                        int32_t errorCode,
                        int32_t latitudeX1e7,
                        int32_t longitudeX1e7,
                        int32_t altitudeMillimetres,
                        int32_t radiusMillimetres,
                        int32_t speedMillimetresPerSecond,
                        int32_t svs,
                        int64_t timeUtc)
{
    gPosStopTime = uPortGetTickTimeMs();
    gGnssHandle = gnssHandle;
    gErrorCode = errorCode;
    gLatitudeX1e7 = latitudeX1e7;
    gLongitudeX1e7 = longitudeX1e7;
    gAltitudeMillimetres = altitudeMillimetres;
    gRadiusMillimetres = radiusMillimetres;
    gSpeedMillimetresPerSecond = speedMillimetresPerSecond;
    gSvs = svs;
    gTimeUtc = timeUtc;
}
# endif // # ifdef U_GNSS_MSG_TEST_INCLUDE_A_POS

// Convert a lat/long into a whole number and a
// bit-after-the-decimal-point that can be printed
// without having to invoke floating point operations,
// returning the prefix (either "+" or "-").
// The result should be printed with printf() format
// specifiers %c%d.%07d, e.g. something like:
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

#endif // #ifndef U_CFG_TEST_USING_NRF5SDK 

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Exchange transparent messages with the GNSS chip, blocking form.
 */
U_PORT_TEST_FUNCTION("[gnssMsg]", "gnssMsgReceiveBlocking")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    char *pBuffer1;
    char *pBuffer2;
    char *pBuffer3;
    int32_t y;
    int32_t x;
    // Enough room to encode the poll for a UBX-MON-VER message
    char command[U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES];
    uGnssMessageId_t messageId;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types except U_GNSS_TRANSPORT_AT
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t w = 0; w < iterations; w++) {
        // Only do this for non-message-filtered transport since that is the worst case
        if ((transportTypes[w] == U_GNSS_TRANSPORT_UART) ||
            (transportTypes[w] == U_GNSS_TRANSPORT_I2C)) {
            // Do the standard preamble
            U_TEST_PRINT_LINE("testing on transport %s...",
                              pGnssTestPrivateTransportTypeName(transportTypes[w]));
            U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                        transportTypes[w], &gHandles, true,
                                                        U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                        U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
            gnssHandle = gHandles.gnssHandle;

            // Make sure NMEA is on
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, true) == 0);

            // So that we can see what we're doing
            uGnssSetUbxMessagePrint(gnssHandle, true);

            pBuffer1 = (char *) malloc(U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES);
            U_PORT_TEST_ASSERT(pBuffer1 != NULL);
            pBuffer2 = (char *) malloc(U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES);
            U_PORT_TEST_ASSERT(pBuffer2 != NULL);

            // Ask for the firmware version string in the normal way
            U_TEST_PRINT_LINE("getting the version string the normal way...");
            y = uGnssInfoGetFirmwareVersionStr(gnssHandle, pBuffer1,
                                               U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES);
            U_PORT_TEST_ASSERT(y > 0);

            // Now manually encode a request for the version string using the
            // message class and ID of the UBX-MON-VER command
            memset(pBuffer2, 0x66, U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES);
            x = uUbxProtocolEncode(0x0a, 0x04, NULL, 0, command);
            U_PORT_TEST_ASSERT(x == sizeof(command));
            U_TEST_PRINT_LINE("getting the version string using transparent API,"
                              " blocking call...");
            // Since we're going to use wild-card receive, need to flush the
            // buffer first to only pick up the message that is a response
            uGnssMsgReceiveFlush(gnssHandle, false);
            x = uGnssMsgSend(gnssHandle, command, sizeof(command));
            U_TEST_PRINT_LINE("%d byte(s) sent.", x);
            U_PORT_TEST_ASSERT(x == sizeof(command));

            U_TEST_PRINT_LINE("receiving response without message filter and with auto-buffer.");
            messageId.type = U_GNSS_PROTOCOL_UBX;
            messageId.id.ubx = (U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) | U_GNSS_UBX_MESSAGE_ID_ALL;
            gStopTimeMs = uPortGetTickTimeMs() + U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TIMEOUT_MS;
            pBuffer3 = NULL;
            gCallbackErrorCode = 0;
            x = uGnssMsgReceive(gnssHandle, &messageId, &pBuffer3, 0,
                                // +1000 in order to rely on keepGoingCallback
                                U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TIMEOUT_MS + 1000,
                                keepGoingCallback);
            U_TEST_PRINT_LINE("%d byte(s) received (including ubx protocol overhead).", x);
            checkMessageReceive(&messageId, pBuffer3, x, messageId.id.ubx,
                                y + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES, pBuffer1);
            U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

            U_TEST_PRINT_LINE("getting the version string using transparent API,"
                              " blocking call...");
            uGnssMsgReceiveFlush(gnssHandle, false);
            x = uGnssMsgSend(gnssHandle, command, sizeof(command));
            U_TEST_PRINT_LINE("%d byte(s) sent.", x);
            U_PORT_TEST_ASSERT(x == sizeof(command));

            U_TEST_PRINT_LINE("receiving response with message filter and buffer provided.");
            messageId.type = U_GNSS_PROTOCOL_UBX;
            messageId.id.ubx = 0x0a04;
            gCallbackErrorCode = 0;
            x = uGnssMsgReceive(gnssHandle, &messageId, &pBuffer2,
                                U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES,
                                U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TIMEOUT_MS, NULL);
            U_TEST_PRINT_LINE("%d byte(s) received (including ubx protocol overhead).", x);
            checkMessageReceive(&messageId, pBuffer2, x, messageId.id.ubx,
                                y + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES, pBuffer1);
            U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);
            for (y = x; x < U_GNSS_MSG_TEST_MESSAGE_RECEIVE_BUFFER_SIZE_BYTES; x++) {
                U_PORT_TEST_ASSERT(*(pBuffer2 + y) == 0x66);
            }

            // Free memory
            free(pBuffer3);
            free(pBuffer2);
            free(pBuffer1);

            y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
            U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
            U_PORT_TEST_ASSERT(y == 0);

            // Do the standard postamble.
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

// NRF52, which we use NRF5SDK on, doesn't have enough heap for this test
#ifndef U_CFG_TEST_USING_NRF5SDK

/** Read transparent messages with the GNSS chip, non-blocking form.
 */
U_PORT_TEST_FUNCTION("[gnssMsg]", "gnssMsgReceiveNonBlocking")
{
    uDeviceHandle_t gnssHandle;
    const uGnssPrivateModule_t *pModule;
    int32_t heapUsed;
    uGnssMsgTestReceive_t *pTmp;
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    bool bad = false;
    // Enough room to poll UBX-RXM-MEASX
    char command[U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES];
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];
    int32_t startTimeMs;
    int32_t posStartTime = -1;
    char prefix[2];
    int32_t whole[2];
    int32_t fraction[2];
    uGnssCommunicationStats_t communicationStats;
    const char *pProtocolName;

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types except U_GNSS_TRANSPORT_AT
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t w = 0; w < iterations; w++) {
        // Only do this for non-message-filtered transport since we need all
        // protocol types for a stress test
        if ((transportTypes[w] == U_GNSS_TRANSPORT_UART) ||
            (transportTypes[w] == U_GNSS_TRANSPORT_I2C)) {
            // Do the standard preamble
            U_TEST_PRINT_LINE("testing on transport %s...",
                              pGnssTestPrivateTransportTypeName(transportTypes[w]));
            U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                        transportTypes[w], &gHandles, true,
                                                        U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                        U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
            gnssHandle = gHandles.gnssHandle;

            pModule = pUGnssPrivateGetModule(gnssHandle);
            U_PORT_TEST_ASSERT(pModule != NULL);

            // Make sure NMEA is on
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, true) == 0);

            U_TEST_PRINT_LINE("running %d transparent non-blocking receives for ~%d second(s)...",
                              sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]),
                              U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS *
                              U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX);

            // Note that we don't switch on message printing here, just too much man

            // Do this twice - once asking for loads of nice long RRLP messages to add
            // stress, then a second time doing a position establishment in parallel
            for (size_t z = 0; z < 2; z++) {
# ifdef U_GNSS_MSG_TEST_INCLUDE_A_POS
                if (z == 0) {
                    U_TEST_PRINT_LINE("run %d, with nice long RRLP messages to decode.", z + 1);
                } else {
                    U_TEST_PRINT_LINE("run %d, with parallel position establishment.", z + 1);
                }
# else
                U_TEST_PRINT_LINE("run %d, with nice long RRLP messages to decode.", z + 1);
# endif
                // Allocate memory for all the transparent receivers
                for (size_t x = 0; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x++) {
                    gpMessageReceive[x] = (uGnssMsgTestReceive_t *) malloc(sizeof(uGnssMsgTestReceive_t));
                    U_PORT_TEST_ASSERT(gpMessageReceive[x] != NULL);
                    pTmp = gpMessageReceive[x];
                    memset(pTmp, 0, sizeof(*pTmp));
                    pTmp->pBuffer = (char *) malloc(U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_BUFFER_SIZE_BYTES);
                    U_PORT_TEST_ASSERT(pTmp->pBuffer != NULL);
                    // Ask for all message types in either protocol
                    pTmp->messageId.type = U_GNSS_PROTOCOL_NMEA; // pNmea left at NULL is "all"
                    pTmp->numDecodedMin = U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_NMEA;
                    if (x % 2) {
                        pTmp->messageId.type = U_GNSS_PROTOCOL_UBX;
                        pTmp->messageId.id.ubx = (U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) | U_GNSS_UBX_MESSAGE_ID_ALL;
                        pTmp->numDecodedMin = U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX;
                    }
                }

                // Hook them in, passing a pointer to the entry as the callback parameter
                gCallbackErrorCode = 0;
                for (size_t x = 0; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x++) {
                    pTmp = gpMessageReceive[x];
                    pTmp->asyncHandle = uGnssMsgReceiveStart(gnssHandle,
                                                             &(pTmp->messageId),
                                                             messageReceiveCallback,
                                                             (void *) pTmp);
                    pTmp->moduleType = pModule->moduleType;
                    U_PORT_TEST_ASSERT(pTmp->asyncHandle >= 0);
                }

                // Messages should now start arriving at our callback
                startTimeMs = uPortGetTickTimeMs();
                U_PORT_TEST_ASSERT(uUbxProtocolEncode(0x02, 0x14, NULL, 0, command) == sizeof(command));
                for (size_t x = 0; x < U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX; x++) {
# ifdef U_GNSS_MSG_TEST_INCLUDE_A_POS
                    if (z == 0) {
                        // Poll for RRLP (UBX-RXM-MEASX), the response to which can be quite long
                        U_TEST_PRINT_LINE("%3d polling for a ubx-format RRLP message in the mix.",
                                          U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS *
                                          (U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX - x));
                        U_PORT_TEST_ASSERT(uGnssMsgSend(gnssHandle, command,
                                                        sizeof(command)) == sizeof(command));
                    } else {
                        // Start a parallel position establishment part-way into the test
                        if ((posStartTime < 0) &&
                            ((uPortGetTickTimeMs() - startTimeMs) / 1000 >
                             U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POS_DELAY_SECONDS)) {
                            U_TEST_PRINT_LINE("started a position establishment in parallel, please wait...");
                            gErrorCode = 0xFFFFFFFF;
                            posStartTime = uPortGetTickTimeMs();
                            U_PORT_TEST_ASSERT(uGnssPosGetStart(gnssHandle, posCallback) == 0);
                        }
                    }
# else
                    (void) startTimeMs;
                    // Poll for RRLP (UBX-RXM-MEASX), the response to which can be quite long
                    U_TEST_PRINT_LINE("%3d polling for a ubx-format RRLP message in the mix.",
                                      U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS *
                                      (U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_MIN_UBX - x));
                    U_PORT_TEST_ASSERT(uGnssMsgSend(gnssHandle, command,
                                                    sizeof(command)) == sizeof(command));
# endif
                    uPortTaskBlock(U_GNSS_MSG_TEST_MESSAGE_RECEIVE_NON_BLOCKING_POLL_DELAY_SECONDS * 1000);
                }

                // Wait for all of those to come through
                U_TEST_PRINT_LINE("wait for it...");
                uPortTaskBlock(5000);

                // Now stop the odd ones
                for (size_t x = 1; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x += 2) {
                    pTmp = gpMessageReceive[x];
                    U_PORT_TEST_ASSERT(uGnssMsgReceiveStop(gnssHandle, pTmp->asyncHandle) == 0);
                    pTmp->stopped = true;
                }

                // A little more cooking...
                U_TEST_PRINT_LINE("just a little longer...");
                uPortTaskBlock(5000);

                // Record the stack extent of the transparent receive task and then
                // stop everything; not asserting here so that we can see what
                // the outcome of all the above was first
                a = uGnssMsgReceiveStackMinFree(gnssHandle);
                uPortTaskBlock(100);
                b = uGnssMsgReceiveStopAll(gnssHandle);
                uPortTaskBlock(100);
                c = uGnssMsgReceiveStatStreamLoss(gnssHandle);
                d = uGnssMsgReceiveStatReadLoss(gnssHandle);

                // Print the outcome prettilyish
                U_TEST_PRINT_LINE("run %d done, results are:", z + 1);
                U_TEST_PRINT_LINE("handle   received   read  decoded threshold  NMEA sequence  NMEA bad sequence  not wanted   outsized   when stopped");
                for (size_t x = 0; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x++) {
                    pTmp = gpMessageReceive[x];
                    uPortLog(U_TEST_PREFIX " %2d       ", pTmp->asyncHandle);
                    if (pTmp->numReceived > 0) {
                        uPortLog("%5d", pTmp->numReceived);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("    ");
                    if (pTmp->numRead > 0) {
                        uPortLog("%5d", pTmp->numRead);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("   ");
                    if (pTmp->numDecoded > 0) {
                        uPortLog("%5d", pTmp->numDecoded);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("   %5d       ", pTmp->numDecodedMin);
                    if (pTmp->numNmeaSequence > 0) {
                        uPortLog("%5d", pTmp->numNmeaSequence);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("         ");
                    if (pTmp->numNmeaBadSequence > 0) {
                        uPortLog("%5d", pTmp->numNmeaBadSequence);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("               ");
                    if (pTmp->numNotWanted > 0) {
                        uPortLog("%5d", pTmp->numNotWanted);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("       ");
                    if (pTmp->numOutsize > 0) {
                        uPortLog("%5d", pTmp->numOutsize);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("        ");
                    if (pTmp->numWhenStopped > 0) {
                        uPortLog("%5d", pTmp->numWhenStopped);
                    } else {
                        uPortLog("  -  ");
                    }
                    uPortLog("\n");
# ifdef U_GNSS_MSG_TEST_INCLUDE_A_POS
                    if (z == 0) {
                        if (pTmp->numDecoded + pTmp->numOutsize < pTmp->numDecodedMin) {
                            bad = true;
                        }
                    } else {
                        if (pTmp->numReceived < (size_t) (gPosStopTime >= 0)) {
                            bad = true;
                        }
                    }
# else
                    if (pTmp->numDecoded + pTmp->numOutsize < pTmp->numDecodedMin) {
                        bad = true;
                    }
# endif
                    // Can currently only check the NMEA sequence for M9
                    // modules, hence the check below
                    if ((pTmp->messageId.type != U_GNSS_PROTOCOL_UBX) &&
                        (pTmp->moduleType == U_GNSS_MODULE_TYPE_M9) &&
                        (pTmp->numNmeaSequence == 0)) {
                        bad = true;
                    }
                    if (pTmp->numNmeaBadSequence > 0) {
                        bad = true;
                    }
                    if (pTmp->numReceived + pTmp->numOutsize < pTmp->numRead) {
                        bad = true;
                    }
                    if (pTmp->numNotWanted > 0) {
                        bad = true;
                    }
                    if (pTmp->numWhenStopped > 0) {
                        bad = true;
                    }
                    // Such a burst of logging can overwhelm some platforms
                    // (e.g. NRF5SDK) so pause between prints so as not to lose stuff.
                    uPortTaskBlock(10);
                }
                U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", c);
                U_TEST_PRINT_LINE("%d byte(s) lost by the asynchronous read task during that test.", d);
                if (a != U_ERROR_COMMON_NOT_SUPPORTED) {
                    U_TEST_PRINT_LINE("the minimum stack of the callback task  was %d.", a);
                }
                U_TEST_PRINT_LINE("the callback error code was %d.", gCallbackErrorCode);

                // Now do the asserting
                U_PORT_TEST_ASSERT(!bad);
                U_PORT_TEST_ASSERT((a == U_ERROR_COMMON_NOT_SUPPORTED) ||
                                   (a >= U_GNSS_MSG_TEST_MESSAGE_RECEIVE_TASK_THRESHOLD_BYTES));
                U_PORT_TEST_ASSERT(b == 0);
                U_PORT_TEST_ASSERT(c == 0);
                U_PORT_TEST_ASSERT(d == 0);
                U_PORT_TEST_ASSERT(gCallbackErrorCode == 0);

                if (posStartTime >= 0) {
                    // Check the outcome of the parallel position establishment
                    if (gPosStopTime >= 0) {
                        U_TEST_PRINT_LINE("parallel position establishment reported after %d second(s).",
                                          (gPosStopTime - posStartTime) / 1000);
                        U_PORT_TEST_ASSERT(gGnssHandle == gnssHandle);
                        if (gErrorCode != 0xFFFFFFFF) {
                            U_TEST_PRINT_LINE("parallel position establishment returned error code %d.", gErrorCode);
                        }
                        U_PORT_TEST_ASSERT(gErrorCode == 0);

                        prefix[0] = latLongToBits(gLatitudeX1e7, &(whole[0]), &(fraction[0]));
                        prefix[1] = latLongToBits(gLongitudeX1e7, &(whole[1]), &(fraction[1]));
                        U_TEST_PRINT_LINE("location %c%d.%07d/%c%d.%07d (radius %d metre(s)), %d metre(s) high,"
                                          " moving at %d metre(s)/second, %d satellite(s) visible, time %d.",
                                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1],
                                          gRadiusMillimetres / 1000, gAltitudeMillimetres / 1000,
                                          gSpeedMillimetresPerSecond / 1000, gSvs, (int32_t) gTimeUtc);
                        U_TEST_PRINT_LINE("paste this into a browser https://maps.google.com/?q=%c%d.%07d,%c%d.%07d",
                                          prefix[0], whole[0], fraction[0], prefix[1], whole[1], fraction[1]);

                        U_PORT_TEST_ASSERT(gLatitudeX1e7 > INT_MIN);
                        U_PORT_TEST_ASSERT(gLongitudeX1e7 > INT_MIN);
                        // Don't test altitude as we may only have a 2D fix
                        U_PORT_TEST_ASSERT(gRadiusMillimetres > INT_MIN);
                        U_PORT_TEST_ASSERT(gSpeedMillimetresPerSecond > INT_MIN);
                        // Inertial fixes will be reported with no satellites, hence >= 0
                        U_PORT_TEST_ASSERT(gSvs >= 0);
                        U_PORT_TEST_ASSERT(gTimeUtc > 0);
                    } else {
                        U_TEST_PRINT_LINE("parallel position establishment did not complete though.");
                        U_PORT_TEST_ASSERT(false);
                    }
                }

                // Switch message printing on for this bit
                uGnssSetUbxMessagePrint(gnssHandle, true);

                a = uGnssInfoGetCommunicationStats(gnssHandle, -1, &communicationStats);
                if (a == 0) {
                    // Print the communication stats as seen by the GNSS chip
                    U_TEST_PRINT_LINE("communications from the GNSS chip's perspective:");
                    U_TEST_PRINT_LINE(" %d transmit byte(s) currently pending.", communicationStats.txPendingBytes);
                    U_TEST_PRINT_LINE(" %d byte(s) ever transmitted.", communicationStats.txBytes);
                    U_TEST_PRINT_LINE(" %d%% transmit buffer currently used.", communicationStats.txPercentageUsage);
                    U_TEST_PRINT_LINE(" %d%% peak transmit buffer usage.", communicationStats.txPeakPercentageUsage);
                    U_TEST_PRINT_LINE(" %d receive byte(s) currently pending.", communicationStats.rxPendingBytes);
                    U_TEST_PRINT_LINE(" %d byte(s) ever received.", communicationStats.rxBytes);
                    U_TEST_PRINT_LINE(" %d%% receive buffer currently used.", communicationStats.rxPercentageUsage);
                    U_TEST_PRINT_LINE(" %d%% peak receive buffer usage.", communicationStats.rxPeakPercentageUsage);
                    U_TEST_PRINT_LINE(" %d 100 ms interval(s) with receive overrun errors.",
                                      communicationStats.rxOverrunErrors);
                    for (size_t x = 0; x < sizeof(communicationStats.rxNumMessages) /
                         sizeof(communicationStats.rxNumMessages[0]); x++) {
                        if (communicationStats.rxNumMessages[x] >= 0) {
                            pProtocolName = pGnssTestPrivateProtocolName((uGnssProtocol_t) x);
                            if (pProtocolName != NULL) {
                                U_TEST_PRINT_LINE(" %d %s message(s) decoded.", communicationStats.rxNumMessages[x], pProtocolName);
                            } else {
                                U_TEST_PRINT_LINE(" %d protocol %d message(s) decoded.", communicationStats.rxNumMessages[x], x);
                            }
                        }
                    }
                    U_TEST_PRINT_LINE(" %d receive byte(s) skipped.", communicationStats.rxSkippedBytes);

                    // Assert on some of the above
                    U_PORT_TEST_ASSERT(communicationStats.txPeakPercentageUsage < 100);
                    U_PORT_TEST_ASSERT(communicationStats.rxPeakPercentageUsage < 100);
                    U_PORT_TEST_ASSERT(communicationStats.rxOverrunErrors == 0);
                    U_PORT_TEST_ASSERT(communicationStats.rxNumMessages[U_GNSS_PROTOCOL_UBX] > 0);
                } else {
                    U_TEST_PRINT_LINE("unable to check GNSS chip's view of communications state.");
                    U_PORT_TEST_ASSERT(a == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
                }

                // Switch message printing back off again
                uGnssSetUbxMessagePrint(gnssHandle, false);

                // Free memory
                for (size_t x = 0; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x++) {
                    free(gpMessageReceive[x]->pBuffer);
                    free(gpMessageReceive[x]->pNmeaComprehenderContext);
                    free(gpMessageReceive[x]);
                    gpMessageReceive[x] = NULL;
                }
            }

            // Do the standard postamble
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

#endif // U_CFG_TEST_USING_NRF5SDK 

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssMsg]", "gnssMsgCleanUp")
{
    int32_t x;

    uGnssTestPrivateCleanup(&gHandles);

#ifndef U_CFG_TEST_USING_NRF5SDK
    for (size_t x = 0; x < sizeof(gpMessageReceive) / sizeof(gpMessageReceive[0]); x++) {
        if (gpMessageReceive[x] != NULL) {
            free(gpMessageReceive[x]->pBuffer);
            free(gpMessageReceive[x]);
        }
    }
#endif // U_CFG_TEST_USING_NRF5SDK 

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
