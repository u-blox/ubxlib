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
 * @brief Implementation of the multiple-GNSS assistance API for GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MAX
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), memcmp(), memset()
#include "time.h"      // gmtime_r(), struct tm

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* In some cases gmtime_r(). */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_private.h"
#include "u_gnss_msg.h"
#include "u_gnss_msg_private.h"
#include "u_gnss_mga.h"

#include "u_lib_mga.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_MGA_ONLINE_HTTP_PREFIX
/** The prefix to put at the start of the GET request to the
 * AssistNow Online HTTP server.
 */
# define U_GNSS_MGA_ONLINE_HTTP_PREFIX "/GetOnlineData.ashx?"
#endif

#ifndef U_GNSS_MGA_OFFLINE_HTTP_PREFIX
/** The prefix to put at the start of the GET request to the
 * AssistNow Offline HTTP server.
 */
# define U_GNSS_MGA_OFFLINE_HTTP_PREFIX "/GetOfflineData.ashx?"
#endif

#ifndef U_GNSS_MGA_RESPONSE_MESSAGE_MAX_LENGTH_BYTES
/** The maximum length of UBX message coming back from the GNSS
 * device that libMga might be interested in.
 */
# define U_GNSS_MGA_RESPONSE_MESSAGE_MAX_LENGTH_BYTES 64
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A structure that is passed to readDeviceDatabaseCallback().
 */
typedef struct {
    int32_t errorCodeOrLength;
    int32_t totalMessages;
    int32_t totalLength;
    bool keepGoing;
    uGnssMgaDatabaseCallback_t *pCallback;
    void *pCallbackParam;
} uGnssMgaReadDeviceDatabase_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The possible MGA_DATA_TYPE_FLAGS supported by libMga,
 * MUST BE arranged in the same order as the uGnssSystem_t values
 * in gSystemBitMap and the two arrays MUST have the same
 * number of elements.
 */
static const MGA_DATA_TYPE_FLAGS gMgaDataTypeFlags[] = {MGA_GNSS_GPS,
                                                        MGA_GNSS_GLO,
                                                        MGA_GNSS_QZSS,
                                                        MGA_GNSS_GALILEO,
                                                        MGA_GNSS_BEIDOU
                                                       };

/** The possible uGnssSystem_t's supported by libMga, MUST BE
 * arranged in the same order as the MGA_DATA_TYPE_FLAGS values in
 * gMgaDataTypeFlags and the two arrays MUST have the same number of
 * elements.
 */
static const uGnssSystem_t gSystemBitMap[] = {U_GNSS_SYSTEM_GPS,
                                              U_GNSS_SYSTEM_GLONASS,
                                              U_GNSS_SYSTEM_QZSS,
                                              U_GNSS_SYSTEM_GALILEO,
                                              U_GNSS_SYSTEM_BEIDOU
                                             };

/** Table to turn a MGA_API_RESULT into a uErrorCode_t; all errors
 * outside this range should return U_ERROR_COMMON_UNKNOWN.
 */
static const uErrorCode_t gMgaApiResultToError[] = {
    U_ERROR_COMMON_SUCCESS,   // MGA_API_OK
    U_ERROR_COMMON_UNKNOWN,   // MGA_API_CANNOT_CONNECT: shouldn't happen as we don't use libMga that way
    U_ERROR_COMMON_UNKNOWN,   // MGA_API_CANNOT_GET_DATA: shouldn't happen as we don't use libMga that way
    U_ERROR_COMMON_NOT_INITIALISED, // MGA_API_CANNOT_INITIALIZE
    U_ERROR_COMMON_BUSY,      // MGA_API_ALREADY_RUNNING
    U_ERROR_COMMON_EMPTY,     // MGA_API_ALREADY_IDLE
    U_ERROR_COMMON_IGNORED,   // MGA_API_IGNORED_MSG
    U_ERROR_COMMON_BAD_DATA,  // MGA_API_BAD_DATA
    U_ERROR_COMMON_NO_MEMORY, // MGA_API_OUT_OF_MEMORY
    U_ERROR_COMMON_NOT_FOUND, // MGA_API_NO_MGA_INI_TIME
    U_ERROR_COMMON_EMPTY      // MGA_API_NO_DATA_TO_SEND
};


/** The body of the smallest UBX-MGA-FLASH-DATA message, used to erase it.
 */
static const char gUbxMgaFlashDataBodyErase[] = {0x01, // Message type
                                                 0x00, // Message version
                                                 0x00, // Sequence number
                                                 0x00,
                                                 0x00, // Payload size
                                                 0x00
                                                };

/** The values that an AssistNow Online data buffer _must_ begin with:
 * a UBX-MGA-INI message that sets the time, for example:
 *
 * b5 62 13 40 1800 10 00 0012e707060108312b008037553400000000000000002688
 *
 * ...so message class 0x13, message ID 0x40, length 24, type 0x10,
 * version 0x00 and then the rest of the message.
 */
static const char gAssistNowBufferStart[] = {0xb5, 0x62, 0x13, 0x40, 0x18, 0x00, 0x10, 0x00};

/** The number of "initial" bytes to send, without waiting for an
 * ack, versus the flow control type.
 */
static const int32_t gInitialBytes[] = {0,           // U_GNSS_MGA_FLOW_CONTROL_SIMPLE
                                        INT_MAX,     // U_GNSS_MGA_FLOW_CONTROL_WAIT
                                        U_GNSS_MGA_RX_BUFFER_SIZE_BYTES // U_GNSS_MGA_FLOW_CONTROL_SMART
                                       };

/* ----------------------------------------------------------------
* STATIC FUNCTIONS
* -------------------------------------------------------------- */

// Return a bitmap of MGA_DATA_TYPE_FLAGS given a bitmap of uGnssSystem_t.
static MGA_DATA_TYPE_FLAGS setGnssTypeFlags(uint32_t systemBitMap)
{
    MGA_DATA_TYPE_FLAGS flags = 0;

    for (size_t x = 0; x < sizeof(gSystemBitMap) / sizeof(gSystemBitMap[0]); x++) {
        if (systemBitMap & (1UL << gSystemBitMap[x])) {
            flags |= gMgaDataTypeFlags[x];
        }
    }

    return flags;
}

// Detect the type of AssistNow data that is in pBuffer, returning
// true if it is AssistNow Online data, else false.
static bool detectAssistNowType(const char *pBuffer, size_t size)
{
    bool onlineNotOffline = false;

    // Check the buffer against what we know it must begin with if it
    // contains AssistNow Online data
    if ((size >= sizeof(gAssistNowBufferStart)) &&
        (memcmp(pBuffer, gAssistNowBufferStart, sizeof(gAssistNowBufferStart)) == 0)) {
        onlineNotOffline = true;
    }

    return onlineNotOffline;
}

// Populate a time adjust structure from a UTC time.
static MgaTimeAdjust *pCreateTimeAdjust(int64_t timeUtcMilliseconds,
                                        int64_t timeUtcAccuracyMilliseconds,
                                        MgaTimeAdjust *pTimeAdjust)
{
    MgaTimeAdjust *pTimeAdjustReturned = NULL;
    struct tm structTm;
    time_t time;

    if (pTimeAdjust != NULL) {
        memset(pTimeAdjust, 0, sizeof(*pTimeAdjust));
        pTimeAdjust->mgaAdjustType = MGA_TIME_ADJUST_ABSOLUTE;
        if (timeUtcMilliseconds >= 0) {
            time = (time_t) (timeUtcMilliseconds / 1000);
            gmtime_r(&time, &structTm);
            // Literal calender year, so 2013 for 2013, rather than an offset
            pTimeAdjust->mgaYear = (UBX_U2) (structTm.tm_year + 1900);
            // Starts from 1 instead of 0
            pTimeAdjust->mgaMonth = (UBX_U1) (structTm.tm_mon + 1);
            pTimeAdjust->mgaDay = (UBX_U1) structTm.tm_mday;
            pTimeAdjust->mgaHour = (UBX_U1) structTm.tm_hour;
            pTimeAdjust->mgaMinute = (UBX_U1) structTm.tm_min;
            pTimeAdjust->mgaSecond = (UBX_U1) structTm.tm_sec;
            if (timeUtcAccuracyMilliseconds > 0) {
                pTimeAdjust->mgaAccuracyS = (UBX_U2) (timeUtcAccuracyMilliseconds / 1000);
                pTimeAdjust->mgaAccuracyMs = (UBX_U2) (timeUtcAccuracyMilliseconds % 1000);
            }
            pTimeAdjustReturned = pTimeAdjust;
        }
    }

    return pTimeAdjustReturned;
}

// Callback called as libMga writes to the GNSS device to indicate progress.
static void progressCallback(MGA_PROGRESS_EVENT_TYPE evtType, const void *pContext,
                             const void *pEvtInfo, UBX_I4 evtInfoSize)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uGnssPrivateInstance_t *pInstance = (uGnssPrivateInstance_t *) pContext;
    uGnssPrivateMga_t *pMga = pInstance->pMga;
    MgaMsgInfo *pMsgInfo = (MgaMsgInfo *) pEvtInfo;
    size_t blocksSent = 0;

    (void) evtInfoSize;

    switch (evtType) {
        case MGA_PROGRESS_EVT_START:
            pMga->blocksTotal = *((size_t *) pEvtInfo);
            break;
        case MGA_PROGRESS_EVT_MSG_SENT:
            blocksSent = pMsgInfo->sequenceNumber + 1;
            break;
        case MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED:
#if !U_CFG_OS_CLIB_LEAKS
            uPortLog("U_GNSS_MGA: message %d, transfer failed (%d).\n",
                     pMsgInfo->sequenceNumber + 1,
                     pMsgInfo->mgaFailedReason);
#endif
            pMga->transferInProgress = false;
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            break;
        case MGA_PROGRESS_EVT_TERMINATED:
            pMga->transferInProgress = false;
            switch (*((EVT_TERMINATION_REASON *) pEvtInfo)) {
                case TERMINATE_HOST_CANCEL:
                    errorCode = (int32_t) U_ERROR_COMMON_CANCELLED;
                    break;
                case TERMINATE_RECEIVER_NAK:
                    errorCode = (int32_t) U_GNSS_ERROR_NACK;
                    break;
                case TERMINATE_RECEIVER_NOT_RESPONDING:
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
                    break;
                case TERMINATE_PROTOCOL_ERROR:
                    errorCode = (int32_t) U_ERROR_COMMON_PROTOCOL_ERROR;
                    break;
                default:
                    errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                    break;
            }
            break;
        case MGA_PROGRESS_EVT_FINISH:
            pMga->transferInProgress = false;
            // Set the remembered error code to indicate success
            pMga->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            break;
        default:
            break;
    }

    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        // If the error code has "gone bad", remember it
        pMga->errorCode = errorCode;
    }

    if ((pMga->pProgressCallback != NULL) &&
        ((evtType == MGA_PROGRESS_EVT_MSG_SENT) || (errorCode !=  (int32_t) U_ERROR_COMMON_SUCCESS))) {
        if (!pMga->pProgressCallback(pInstance->gnssHandle,
                                     errorCode,
                                     pMga->blocksTotal, blocksSent,
                                     pMga->pProgressCallbackParam)) {
            // User has cancelled the transfer
            pMga->errorCode = (int32_t) U_ERROR_COMMON_CANCELLED;
            pMga->transferInProgress = false;
        }
    }
}

// Callback called by libMga to do the actual writing to the GNSS device.
static void writeDeviceCallback(const void *pContext, const UBX_U1 *pData, UBX_I4 iSize)
{
    uGnssPrivateInstance_t *pInstance = (uGnssPrivateInstance_t *) pContext;

    uGnssPrivateSendOnlyStreamRaw(pInstance, (const char *) pData, iSize);
}

// Callback called by the ubxlib message receive infrastructure when something
// arrives back from the GNSS device which libMga might need to know about.
static void readDeviceLibMgaCallback(uDeviceHandle_t gnssHandle,
                                     const uGnssMessageId_t *pMessageId,
                                     int32_t errorCodeOrLength,
                                     void *pCallbackParam)
{
    char buffer[U_GNSS_MGA_RESPONSE_MESSAGE_MAX_LENGTH_BYTES];

    (void) pMessageId;
    (void) pCallbackParam;

    if (errorCodeOrLength > 0) {
        if (errorCodeOrLength > U_GNSS_MGA_RESPONSE_MESSAGE_MAX_LENGTH_BYTES) {
            errorCodeOrLength = U_GNSS_MGA_RESPONSE_MESSAGE_MAX_LENGTH_BYTES;
        }
        errorCodeOrLength = uGnssMsgReceiveCallbackRead(gnssHandle, buffer,
                                                        errorCodeOrLength);
        if (errorCodeOrLength >= 0) {
            mgaProcessReceiverMessage((const UBX_U1 *) buffer, errorCodeOrLength);
        }
    }
}

// Enable acknowledgements for UBX-MGA messages.
static int32_t ubxMgaAckEnable(uGnssPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_NAVSPG_ACKAIDING_L,
                            .value = true
                           };
    // Enough room for the body of a UBX-CFG-NAVX5 message
    char message[40] = {0};

    if (pInstance != NULL) {
        if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
            // Use the CFG-VAL interface
            errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                  U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                  U_GNSS_CFG_VAL_LAYER_RAM);
        } else {
            // Ye olde way: with the UBX-CFG-NAVX5 message
            // Set the first bitmask, where bit 10 indicates that we
            // want to modify the Ack for Aiding flag.
            *((uint16_t *) (message + 2)) = uUbxProtocolUint16Encode(1ULL << 10);
            // Whether Ack for Aiding messages is on or off is at offset 17
            message[17] = 0x01;
            errorCode = uGnssPrivateSendUbxMessage(pInstance,
                                                   0x06, 0x23,
                                                   message, sizeof(message));
        }
    }

    return errorCode;
}

// Send a UBX-MGA message and wait for the ack.
static int32_t ubxMgaSendWaitAck(uGnssPrivateInstance_t *pInstance,
                                 int32_t messageClass,
                                 int32_t messageId,
                                 const char *pMessageBody,
                                 size_t messageBodyLengthBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t startTimeMs;
    // The UBX-MGA-ACK message ID
    uGnssPrivateMessageId_t ackMessageId = {.type = U_GNSS_PROTOCOL_UBX,
                                            .id.ubx = 0x1360
                                           };
    // Enough room for a UBX-MGA-ACK-DATA0 message, including overhead
    char buffer[8 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES] = {0};
    char *pBuffer = buffer;
    int32_t x;
    // 0 for "not acked", 1 for "nacked", 2 for "acked"
    size_t ackState = 0;

    if ((pInstance != NULL) && (pMessageBody != NULL)) {
        // Send the message
        errorCode = uGnssPrivateSendOnlyStreamUbxMessage(pInstance,
                                                         messageClass, messageId,
                                                         pMessageBody,
                                                         messageBodyLengthBytes);
        if (errorCode >= 0) {
            x = errorCode;
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            if (x == messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                // Wait for the UBX-MGA-ACK-DATA0 response for our message ID
                startTimeMs = uPortGetTickTimeMs();
                do {
                    x = uGnssPrivateReceiveStreamMessage(pInstance, &ackMessageId,
                                                         pInstance->ringBufferReadHandlePrivate,
                                                         &pBuffer, sizeof(buffer),
                                                         1000, NULL);
                    if (x == sizeof(buffer)) {
                        // Check the Ack
                        if ((buffer[1 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 0) && // Ack message version
                            ((int32_t) buffer[3 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == messageId)) { // Wanted ID
                            if (buffer[0 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 1) {
                                ackState = 2;
                            } else {
                                ackState = 1;
                            }
                        }
                    }
                } while ((ackState == 0) && (uPortGetTickTimeMs() - startTimeMs < pInstance->timeoutMs));
                if (ackState == 2) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else if (ackState == 1) {
                    errorCode = (int32_t) U_GNSS_ERROR_NACK;
                }
            }
        }
    }

    return errorCode;
}

// Given a pointer to the two-byte length field of a UBX message,
// return the length.
static int32_t ubxLength(const char *pBuffer, size_t size)
{
    int32_t length = (int32_t) U_ERROR_COMMON_BAD_DATA;

    if (size >= 2) {
        length = *(pBuffer) + (((uint32_t) * (pBuffer + 1)) << 8);
    }

    return length;

}

// Callback called by the ubxlib message receive infrastructure when readibg
// the navigation database from the GNSS device.
static void readDeviceDatabaseCallback(uDeviceHandle_t gnssHandle,
                                       const uGnssMessageId_t *pMessageId,
                                       int32_t errorCodeOrLength,
                                       void *pCallbackParam)
{
    uGnssMgaReadDeviceDatabase_t *pContext = (uGnssMgaReadDeviceDatabase_t *) pCallbackParam;
    // Enough room for the largest UBX-MGA-DBD and UBX-MGA-ACK messages, including overhead
    char buffer[U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES];
    int32_t id;
    int32_t x;

    (void) pCallbackParam;

    if (errorCodeOrLength > 0) {
        id = pMessageId->id.ubx & 0xFF;
        if (errorCodeOrLength > sizeof(buffer)) {
            errorCodeOrLength = sizeof(buffer);
        }
        if ((id == 0x80) || (id == 0x60)) {
            errorCodeOrLength = uGnssMsgReceiveCallbackRead(gnssHandle, buffer,
                                                            errorCodeOrLength);
            if (errorCodeOrLength >= 0) {
                if (id == 0x80) {
                    // A UBX-MGA-DBD message
                    pContext->totalMessages++;
                    pContext->totalLength += errorCodeOrLength + 2 - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
                    if (pContext->pCallback != NULL) {
                        // Pass the payload, plus the length indicator that precedes it,
                        // to the callback
                        pContext->keepGoing = pContext->pCallback(gnssHandle, buffer + 4,
                                                                  errorCodeOrLength + 2 - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES,
                                                                  pContext->pCallbackParam);
                        if (!pContext->keepGoing) {
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_CANCELLED;
                        }
                    }
                } else if (id == 0x60) {
                    // A UBX-MGA-ACK message, which ends the transfer:
                    // check that the number of messages, which is contained in
                    // the msgPayloadStart field, is as expected
                    if ((errorCodeOrLength >= 8 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) &&
                        (buffer[1 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 0) &&     // Message version
                        (buffer[3 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 0x80)) {  // -DBD message ID
                        // This is an ack/nack for our UBX-MGA-DBD message
                        pContext->keepGoing = false;
                        errorCodeOrLength = (int32_t) U_GNSS_ERROR_NACK;
                        if (buffer[0 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 1) {
                            // Ack
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_TRUNCATED;
                            x = uUbxProtocolUint32Decode(buffer + 4 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES);
                            if (x == pContext->totalMessages) {
                                // Number of messages is correct, let the user know we're done
                                errorCodeOrLength = pContext->totalLength;
                                if (pContext->pCallback != NULL) {
                                    pContext->pCallback(gnssHandle, NULL, 0, pContext->pCallbackParam);
                                }
#if !U_CFG_OS_CLIB_LEAKS
                            } else {
                                uPortLog("U_GNSS_MGA: %d UBX-MGA-DBD message(s) lost out of %d.\n",
                                         x - pContext->totalMessages, x);
#endif
                            }
                        } else {
                            // This is not documented but it appears that, at least on M10
                            // modules, if there is nothing to send back the module
                            // sends a NACK with an error code of 0xFF.
                            if ((pContext->totalLength == 0) &&
                                (buffer[2 + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES] == 0xFF)) {
                                // Count this as a successful return of nothing
                                errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
                                if (pContext->pCallback != NULL) {
                                    pContext->pCallback(gnssHandle, NULL, 0, pContext->pCallbackParam);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    pContext->errorCodeOrLength = errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Encode an AssistNow Online request body.
int32_t uGnssMgaOnlineRequestEncode(const uGnssMgaOnlineRequest_t *pRequest,
                                    char *pBuffer, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    MgaOnlineServerConfig serverConfig = {0};
    MGA_API_RESULT result;
    size_t prefixSize = 0;
    const uGnssMgaPos_t *pMgaPosFilter;

    if ((pRequest != NULL) && (pRequest->pTokenStr != NULL) &&
        ((pBuffer == NULL) || (size > 0))) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        serverConfig.strServerToken = pRequest->pTokenStr;
        // This field is a direct mapping
        serverConfig.dataTypeFlags = pRequest->dataTypeBitMap;
        serverConfig.gnssTypeFlags = setGnssTypeFlags(pRequest->systemBitMap);
        serverConfig.useFlags |= MGA_FLAGS_USE_LATENCY | MGA_FLAGS_USE_TIMEACC;
        pMgaPosFilter = pRequest->pMgaPosFilter;
        if (pMgaPosFilter != NULL) {
            serverConfig.useFlags |= MGA_FLAGS_USE_POSITION;
            serverConfig.bFilterOnPos = true;
            serverConfig.intX1e7Latitude = pMgaPosFilter->latitudeX1e7;
            serverConfig.intX1e7Longitude = pMgaPosFilter->longitudeX1e7;
            serverConfig.intX1e3Altitude = pMgaPosFilter->altitudeMillimetres;
            serverConfig.intX1e3Accuracy = pMgaPosFilter->radiusMillimetres;
        }
        serverConfig.intX1e3Latency = pRequest->latencyMilliseconds;
        serverConfig.intX1e3TimeAccuracy = pRequest->latencyAccuracyMilliseconds;

        if (pBuffer != NULL) {
            // Start with the prefix
            if (size >= sizeof(U_GNSS_MGA_ONLINE_HTTP_PREFIX)) {
                memcpy(pBuffer, U_GNSS_MGA_ONLINE_HTTP_PREFIX, sizeof(U_GNSS_MGA_ONLINE_HTTP_PREFIX));
                // -1 since the size consumed should not include the terminator
                prefixSize = sizeof(U_GNSS_MGA_ONLINE_HTTP_PREFIX) - 1;
                size -= prefixSize;
                pBuffer += prefixSize;
            }
        }
        if ((prefixSize > 0) || (pBuffer == NULL)) {
            // Continue if we were able to add the prefix or we're doing a NULL encode
            result = mgaBuildOnlineRequestParams(&serverConfig, pBuffer, size);
            if (result == MGA_API_OK) {
                errorCodeOrLength = (int32_t) (serverConfig.encodedMessageLength +
                                               (sizeof(U_GNSS_MGA_ONLINE_HTTP_PREFIX) - 1));
            }
        }
    }

    return (int32_t) errorCodeOrLength;
}

// Encode an AssistNow Offline request body.
int32_t uGnssMgaOfflineRequestEncode(const uGnssMgaOfflineRequest_t *pRequest,
                                     char *pBuffer, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    MgaOfflineServerConfig serverConfig = {0};
    size_t prefixSize = 0;
    MGA_API_RESULT result;

    if ((pRequest != NULL) && (pRequest->pTokenStr != NULL) &&
        ((pBuffer == NULL) || (size > 0))) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        serverConfig.strServerToken = pRequest->pTokenStr;
        serverConfig.gnssTypeFlags = setGnssTypeFlags(pRequest->systemBitMap);
        if (pRequest->almanacDataAlso) {
            // Get the almanac data as well as the long-term stuff
            serverConfig.almFlags = setGnssTypeFlags(pRequest->systemBitMap);
        }
        // Set period, which is in weeks, to zero and use the days field instead
        serverConfig.period = 0;
        serverConfig.resolution = pRequest->daysBetweenItems;
        serverConfig.numofdays = pRequest->periodDays;

        if (pBuffer != NULL) {
            // Start with the prefix
            if (size >= sizeof(U_GNSS_MGA_OFFLINE_HTTP_PREFIX)) {
                memcpy(pBuffer, U_GNSS_MGA_OFFLINE_HTTP_PREFIX, sizeof(U_GNSS_MGA_OFFLINE_HTTP_PREFIX));
                // -1 since the size consumed should not include the terminator
                prefixSize = sizeof(U_GNSS_MGA_OFFLINE_HTTP_PREFIX) - 1;
                size -= prefixSize;
                pBuffer += prefixSize;
            }
        }
        if ((prefixSize > 0) || (pBuffer == NULL)) {
            // Continue if we were able to add the prefix or we're doing a NULL encode
            result = mgaBuildOfflineRequestParams(&serverConfig, pBuffer, size);
            if (result == MGA_API_OK) {
                errorCodeOrLength = (int32_t) (serverConfig.encodedMessageLength +
                                               (sizeof(U_GNSS_MGA_OFFLINE_HTTP_PREFIX) - 1));
            }
        }
    }

    return (int32_t) errorCodeOrLength;
}

// Initialise the GNSS module with the approximate time.
int32_t uGnssMgaIniTimeSend(uDeviceHandle_t gnssHandle,
                            int64_t timeUtcNanoseconds,
                            int64_t timeUtcAccuracyNanoseconds,
                            uGnssMgaTimeReference_t *pReference)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    struct tm structTm;
    time_t time;
    // Enough room for the body of a UBX-MGA-INI-TIME_UTC message
    char message[24] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        // Values in pReference deliberately not checked; the module will do that
        if ((pInstance != NULL) && (timeUtcNanoseconds >= 0) &&
            (timeUtcAccuracyNanoseconds >= 0)) {
            time = timeUtcNanoseconds / 1000000000LL;
            if (gmtime_r(&time, &structTm) != NULL) {
                // Make sure that acks for aiding messages are enabled
                errorCode = ubxMgaAckEnable(pInstance);
                if (errorCode == 0) {
                    message[0] = 0x10; // Message type
                    message[1] = 0;    // Message version
                    if (pReference != NULL) {
                        message[2] = pReference->extInt & 0x0F;
                        if (pReference->fallingNotRising) {
                            message[2] |= 0x10;
                        }
                        if (pReference->lastNotNext) {
                            message[2] |= 0x20;
                        }
                    }
                    message[3] = 0x80; // Leap seconds unknown
                    *((uint16_t *) (message + 4)) = uUbxProtocolUint16Encode(structTm.tm_year + 1900); // Year
                    message[6] = structTm.tm_mon + 1; // Month starting at 1
                    message[7] = structTm.tm_mday; // Day starting at 1
                    message[8] = structTm.tm_hour; // Hour
                    message[9] = structTm.tm_min;  // Minute
                    message[10] = structTm.tm_sec; // Seconds
                    // Nanoseconds
                    *((uint32_t *) (message + 12)) = uUbxProtocolUint32Encode((int32_t) (timeUtcNanoseconds %
                                                                                         1000000000LL));
                    // Accuracy, seconds part
                    *((uint16_t *) (message + 16)) = uUbxProtocolUint16Encode((int16_t) (timeUtcAccuracyNanoseconds /
                                                                                         1000000000LL));
                    // Accuracy, nanoseconds part
                    *((uint32_t *) (message + 20)) = uUbxProtocolUint32Encode((int32_t) (timeUtcAccuracyNanoseconds %
                                                                                         1000000000LL));
                    // Send the UBX-MGA-INI-TIME_UTC message and wait for the ack
                    errorCode = ubxMgaSendWaitAck(pInstance, 0x13, 0x40, message, sizeof(message));
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Initialise the GNSS module with an approximate position.
int32_t uGnssMgaIniPosSend(uDeviceHandle_t gnssHandle,
                           const uGnssMgaPos_t *pMgaPos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Enough room for the body of a UBX-MGA-INI-POS_LLH message
    char message[20] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pMgaPos != NULL)) {
            // Make sure that acks for aiding messages are enabled
            errorCode = ubxMgaAckEnable(pInstance);
            if (errorCode == 0) {
                message[0] = 0x01; // Message type
                message[1] = 0;    // Message version
                *((uint32_t *) (message + 4)) = uUbxProtocolUint32Encode(pMgaPos->latitudeX1e7);
                *((uint32_t *) (message + 8)) = uUbxProtocolUint32Encode(pMgaPos->longitudeX1e7);
                *((uint32_t *) (message + 12)) = uUbxProtocolUint32Encode(pMgaPos->altitudeMillimetres / 10);
                *((uint32_t *) (message + 16)) = uUbxProtocolUint32Encode(pMgaPos->radiusMillimetres / 10);
                // Send the UBX-MGA-INI-POS_LLH message and wait for the ack
                errorCode = ubxMgaSendWaitAck(pInstance, 0x13, 0x40, message, sizeof(message));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Send the response from a u-blox assistance server to a GNSS module.
int32_t uGnssMgaResponseSend(uDeviceHandle_t gnssHandle,
                             int64_t timeUtcMilliseconds,
                             int64_t timeUtcAccuracyMilliseconds,
                             uGnssMgaSendOfflineOperation_t offlineOperation,
                             uGnssMgaFlowControl_t flowControl,
                             const char *pBuffer, size_t size,
                             uGnssMgaProgressCallback_t *pCallback,
                             void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMga_t *pMga;
    uGnssPrivateMessageId_t privateMessageId;
    int32_t readHandle;
    bool onlineNotOffline;
    int32_t protocolsOut = 0;
    MgaTimeAdjust timeAdjust;
    MgaTimeAdjust *pTimeAdjust = NULL;
    MgaFlowConfiguration flowConfiguration = {0};
    MgaEventInterface eventInterface = {0};
    struct tm structTm;
    time_t time;
    UBX_U1 *pBufferUbx = NULL;
    MGA_API_RESULT result;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pBuffer != NULL) && (size > 0) &&
            ((int32_t) flowControl >= 0) && (flowControl < U_GNSS_MGA_FLOW_CONTROL_MAX_NUM)) {
            if (timeUtcMilliseconds >= 0) {
                // Populate the time adjust structure, if present
                pTimeAdjust = pCreateTimeAdjust(timeUtcMilliseconds,
                                                timeUtcAccuracyMilliseconds,
                                                &timeAdjust);
            }
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Allocate memory for the stuff we need to hook off the instance
            pMga = (uGnssPrivateMga_t *) pUPortMalloc(sizeof(uGnssPrivateMga_t));
            if (pMga != NULL) {
                memset(pMga, 0, sizeof(*pMga));
                pInstance->pMga = pMga;
#ifndef U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE
                if ((flowControl != U_GNSS_MGA_FLOW_CONTROL_WAIT) ||
                    (offlineOperation == U_GNSS_MGA_SEND_OFFLINE_FLASH)) {
                    // On a best effort basis, if we are waiting for Acks,
                    // switch off NMEA messages while we do this as the
                    // message load on the interface may otherwise cause this
                    // process to take a very long time
                    protocolsOut = uGnssPrivateGetProtocolOut(pInstance);
                    if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                        uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, false);
                    }
                }
#endif
                //  Now employ libMga to do the rest
                pMga->transferInProgress = true;
                result = mgaInit();
                if (result == MGA_API_OK) {
                    pMga->errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                    // Grab an asynchronous receive slot so that we get the messages
                    // sent back from the GNSS device for libMga to process
                    privateMessageId.type = U_GNSS_PROTOCOL_UBX;
                    privateMessageId.id.ubx = U_GNSS_UBX_MESSAGE_ALL;
                    errorCode = uGnssMsgPrivateReceiveStart(pInstance, &privateMessageId,
                                                            readDeviceLibMgaCallback, pMga);
                    if (errorCode >= 0) {
                        readHandle = errorCode;
                        errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                        flowConfiguration.msgTimeOut = U_GNSS_MGA_MESSAGE_TIMEOUT_MS;
                        flowConfiguration.msgRetryCount = U_GNSS_MGA_MESSAGE_RETRIES;
                        flowConfiguration.mgaFlowControl = (MGA_FLOW_CONTROL_TYPE) flowControl;
                        flowConfiguration.mgaCfgVal = U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                                                         U_GNSS_PRIVATE_FEATURE_CFGVALXXX);
                        eventInterface.evtWriteDevice = writeDeviceCallback;
                        eventInterface.evtProgress = progressCallback;
                        pMga->pProgressCallback = pCallback;
                        pMga->pProgressCallbackParam = pCallbackParam;
                        result = mgaConfigure(&flowConfiguration, &eventInterface, (void *) pInstance);
                        if (result == MGA_API_OK) {
                            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                            result = mgaSessionStart();
                            if (result == MGA_API_OK) {
                                errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                                // Determine what kind of AssistNow this is and start the transfer
                                onlineNotOffline = detectAssistNowType(pBuffer, size);
                                if (onlineNotOffline || (offlineOperation != U_GNSS_MGA_SEND_OFFLINE_NONE)) {
                                    errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                                    if (onlineNotOffline) {
                                        result = mgaSessionSendOnlineData((UBX_U1 *) pBuffer, size, pTimeAdjust);
                                    } else {
                                        if (offlineOperation == U_GNSS_MGA_SEND_OFFLINE_FLASH) {
                                            result = mgaSessionSendOfflineToFlash((UBX_U1 *) pBuffer, size);
                                        } else {
                                            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                                            if (pTimeAdjust != NULL) {
                                                if (offlineOperation == U_GNSS_MGA_SEND_OFFLINE_TODAYS) {
                                                    // Filter by today
                                                    time = (time_t) (timeUtcMilliseconds / 1000);
                                                    gmtime_r(&time, &structTm);
                                                    result = mgaGetTodaysOfflineData(&structTm, (UBX_U1 *) pBuffer, size,
                                                                                     &pBufferUbx, (UBX_I4 *) &size);
                                                } else if (offlineOperation == U_GNSS_MGA_SEND_OFFLINE_ALMANAC) {
                                                    // Filter almanac data
                                                    result = mgaGetAlmOfflineData((UBX_U1 *) pBuffer, size,
                                                                                  &pBufferUbx, (UBX_I4 *) &size);
                                                }
                                                if (result == MGA_API_OK) {
                                                    if (pBufferUbx != NULL) {
                                                        pBuffer = (const char *) pBufferUbx;
                                                    }
                                                    errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                                                    result = mgaSessionSendOfflineData((UBX_U1 *) pBuffer, size,
                                                                                       pTimeAdjust, NULL);
                                                    if (pBufferUbx != NULL) {
                                                        // Free memory from filtering
                                                        uPortFree(pBufferUbx);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (result == MGA_API_OK) {
                                        // Need to wait for all of the transfers to complete
                                        while (pMga->transferInProgress) {
                                            mgaCheckForTimeOuts();
                                            uPortTaskBlock(U_GNSS_MGA_POLL_TIMER_MS);
                                        }
                                        errorCode = pMga->errorCode;
                                    } else {
                                        uPortLog("U_GNSS_MGA: libMga returned error %d.\n", result);
                                        if (result < sizeof(gMgaApiResultToError) / sizeof(gMgaApiResultToError[0])) {
                                            errorCode = (int32_t) gMgaApiResultToError[result];
                                        }
                                    }
                                }
                                mgaSessionStop();
                            }
                        }
                        uGnssMsgPrivateReceiveStop(pInstance, readHandle);
                        mgaDeinit();
                    }
                }
                if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                    // Restore NMEA messages, if we switched them off above
                    uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, true);
                }
                pInstance->pMga = NULL;
                uPortFree(pMga);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Erase the flash memory attached to a GNSS chip.
int32_t uGnssMgaErase(uDeviceHandle_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Enough room for the body of a UBX-MGA-FLASH-ACK message
    char message[6];

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Send the UBX-MGA-FLASH-DATA message and wait for the
            // UBX-MGA-FLASH-ACK response
            errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                          0x13, 0x21,
                                                          gUbxMgaFlashDataBodyErase,
                                                          sizeof(gUbxMgaFlashDataBodyErase),
                                                          message, sizeof(message));
            if (errorCode >= (int32_t) sizeof(message)) {
                errorCode = (int32_t) U_GNSS_ERROR_NACK;
                if ((message[0] == 0x03) && (message[1] == 0) && (message[2] == 0)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get whether AssistNow Autonomous operation is on or off.
bool uGnssMgaAutonomousIsOn(uDeviceHandle_t gnssHandle)
{
    bool onNotOff = false;
    uGnssPrivateInstance_t *pInstance;
    uint32_t keyId = U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L;
    uGnssCfgVal_t *pCfgVal = NULL;
    // Enough room for the body of a UBX-CFG-NAVX5 message
    char message[40];

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                // Use the CFG-VAL interface
                if (uGnssCfgPrivateValGetListAlloc(pInstance, &keyId, 1, &pCfgVal,
                                                   U_GNSS_CFG_VAL_LAYER_RAM) == 1) {
                    onNotOff = (bool) pCfgVal->value;
                    uPortFree(pCfgVal);
                }
            } else {
                // Ye olde way: poll for the UBX-CFG-NAVX5 message
                if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                      0x06, 0x23,
                                                      NULL, 0,
                                                      message,
                                                      sizeof(message)) >= sizeof(message)) {
                    // Whether AssistNow Autonomous is on or off is at offset 27
                    onNotOff = (message[27] != 0);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return onNotOff;
}

// Set AssistNow Autonomous operation on or off.
int32_t uGnssMgaSetAutonomous(uDeviceHandle_t gnssHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t x;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L};
    // Enough room for the body of a UBX-CFG-NAVX5 message
    char message[40] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                // Use the CFG-VAL interface
                cfgVal.value = onNotOff;
                errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                      U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                      U_GNSS_CFG_LAYERS_SET);
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // Ye olde way: with the UBX-CFG-NAVX5 message
                // Unfortunately the single mask bit for AssistNow Autonomous
                // requires us to change both the on/offness and the value
                // of the maximum acceptable orbit error, which we don't want
                // to change, so read the current value first
                x = uGnssPrivateSendReceiveUbxMessage(pInstance, 0x06, 0x23,
                                                      NULL, 0,
                                                      message, sizeof(message));
                if (x >= sizeof(message)) {
                    // Set the first bitmask, where bit 14 indicates that we
                    // want to modify the AssistNow Autonomous stuff.
                    *((uint16_t *) (message + 2)) = uUbxProtocolUint16Encode(1ULL << 14);
                    // Zero the second bitmask, just in case
                    *((uint32_t *) (message + 4)) = 0;
                    // Whether AssistNow Autonomous is on or off is at offset 27
                    message[27] = 0;
                    if (onNotOff) {
                        message[27] = 0x01;
                    }
                    // Send the modified UBX-CFG-NAVX5 message
                    errorCode =  uGnssPrivateSendUbxMessage(pInstance,
                                                            0x06, 0x23,
                                                            message,
                                                            sizeof(message));
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the assistance database from a GNSS device.
int32_t uGnssMgaGetDatabase(uDeviceHandle_t gnssHandle,
                            uGnssMgaDatabaseCallback_t *pCallback,
                            void *pCallbackParam)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    int32_t protocolsOut = 0;
    int32_t readHandle;
    int32_t startTimeMs;
    // The UBX-MGA message class/ID (to capture -DBD and -ACK)
    uGnssPrivateMessageId_t messageId = {.type = U_GNSS_PROTOCOL_UBX,
                                         .id.ubx = 0x1300 + U_GNSS_UBX_MESSAGE_ID_ALL
                                        };
    volatile uGnssMgaReadDeviceDatabase_t context = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Not supported for if there is an intermediate module
            if ((pInstance->transportType != U_GNSS_TRANSPORT_AT) &&
                (pInstance->intermediateHandle == NULL)) {
#ifndef U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE
                // On a best effort basis, switch off NMEA messages while
                // we do this as the message load on the interface may
                // otherwise cause this process to take a very long time
                protocolsOut = uGnssPrivateGetProtocolOut(pInstance);
                if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                    uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, false);
                }
#endif
                // Set up a reader to capture the navigation database responses
                context.keepGoing = true;
                context.pCallback = pCallback;
                context.pCallbackParam = pCallbackParam;
                errorCodeOrLength = uGnssMsgPrivateReceiveStart(pInstance, &messageId,
                                                                readDeviceDatabaseCallback,
                                                                (void *) &context);
                if (errorCodeOrLength >= 0) {
                    readHandle = errorCodeOrLength;
                    // Now poll for the database: the reader callback will call
                    // the user callback to store the data until done
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                    if (uGnssPrivateSendOnlyStreamUbxMessage(pInstance, 0x13, 0x80,
                                                             NULL, 0) == U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        startTimeMs = uPortGetTickTimeMs();
                        while (context.keepGoing && (context.errorCodeOrLength >= 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < U_GNSS_MGA_DATABASE_READ_TIMEOUT_MS)) {
                            uPortTaskBlock(250);
                        }
                        if (!context.keepGoing) {
                            errorCodeOrLength = context.errorCodeOrLength;
                        }
                        // Stop reading
                        uGnssMsgPrivateReceiveStop(pInstance, readHandle);
                        if ((errorCodeOrLength < 0) && (errorCodeOrLength != (int32_t) U_ERROR_COMMON_CANCELLED) &&
                            (pCallback != NULL)) {
                            // Let the user also know that we're done in the error case,
                            // provided the user wasn't the cause
                            pCallback(gnssHandle, NULL, 0, pCallbackParam);
                        }
                    } else {
                        // Stop reading in the error case
                        uGnssMsgPrivateReceiveStop(pInstance, readHandle);
                    }
                }

                if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                    // Restore NMEA messages, if we switched them off above
                    uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, true);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

// Set (restore) the assistance database to a GNSS device.
int32_t uGnssMgaSetDatabase(uDeviceHandle_t gnssHandle,
                            uGnssMgaFlowControl_t flowControl,
                            const char *pBuffer, size_t size,
                            uGnssMgaProgressCallback_t *pCallback,
                            void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    int32_t initialBytes;
    int32_t length;
    int32_t x = size;
    const char *pTmp = pBuffer;
    int32_t totalBlocks = 0;
    int32_t blocksSent = 0;
    int32_t protocolsOut = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pBuffer != NULL) && (size > 0) &&
            ((int32_t) flowControl >= 0) && (flowControl < U_GNSS_MGA_FLOW_CONTROL_MAX_NUM)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Not supported if there is an intermediate module
            if ((pInstance->transportType != U_GNSS_TRANSPORT_AT) &&
                (pInstance->intermediateHandle == NULL)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (flowControl != U_GNSS_MGA_FLOW_CONTROL_WAIT) {
                    // Enable acks if we need them; do this here as we don't want
                    // to get half way and then discover that we can't enable them
                    errorCode = ubxMgaAckEnable(pInstance);
#ifndef U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE
                    // On a best effort basis, if we are waiting for Acks,
                    // switch off NMEA messages while we do this as the
                    // message load on the interface may otherwise cause this
                    // process to take a very long time
                    protocolsOut = uGnssPrivateGetProtocolOut(pInstance);
                    if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                        uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, false);
                    }
#endif
                }
                if (errorCode == 0) {
                    // First, run through the buffer and see if it makes sense
                    while ((x > 2) && (errorCode >= 0)) { // 2 'cos there must be a length indicator
                        // Work out the length
                        length = ubxLength(pTmp, x);
                        if ((length >= 0) &&
                            (length <= U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES) &&
                            (x >= length + 2)) { // +2 to include the length bytes
                            // That length makes sense
                            x -= length + 2; // +2 to account for the length bytes
                            pTmp += length + 2;
                            totalBlocks++;
                        } else {
                            uPortLog("U_GNSS_MGA: %d byte(s), offset %d, bad length %d (max %d).\n",
                                     size, pTmp - pBuffer, length, U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES);
                            errorCode = (int32_t) U_ERROR_COMMON_BAD_DATA;
                        }
                    }
                    if ((errorCode == 0) && (totalBlocks > 0)) {
                        // Good, the data at pBuffer makes sense
                        // Run through up to initialBytes of the buffer in
                        // "fire and forget" mode
                        initialBytes = gInitialBytes[flowControl];
                        if (initialBytes > (int32_t) size) {
                            initialBytes = size;
                        }
                        while ((initialBytes > 0) && (size > 2) &&
                               (errorCode == 0)) { // 2 'cos there must be a length indicator
                            // Work out the length
                            length = ubxLength(pBuffer, size);
                            if ((int32_t) size >= length + 2) { // +2 to include the length bytes
                                initialBytes -= length;
                                if (initialBytes >= 0) {
                                    // Send the UBX-MGA-DBD message
                                    errorCode = uGnssPrivateSendOnlyStreamUbxMessage(pInstance,
                                                                                     0x13, 0x80,
                                                                                     pBuffer + 2, // +2 to skip the length bytes
                                                                                     length);
                                    if (errorCode >= 0) {
                                        if (errorCode == length + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                                            size -= length + 2; // +2 to account for the length bytes
                                            pBuffer += length + 2;
                                            blocksSent++;
                                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                            uPortTaskBlock(U_GNSS_MGA_INTER_MESSAGE_DELAY_MS);
                                        } else {
                                            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                                        }
                                    }
                                    if (pCallback != NULL) {
                                        pCallback(gnssHandle, errorCode, totalBlocks, blocksSent, pCallbackParam);
                                    }
                                }
                            }
                        }
                        // With that done we start waiting for acks
                        while ((size > 2) && (errorCode == 0)) { // 2 'cos there must be a length indicator
                            // Work out the length
                            length = ubxLength(pBuffer, size);
                            if ((int32_t) size >= length + 2) { // +2 to include the length bytes
                                // Send the UBX-MGA-DBD message and wait for the ack
                                errorCode = ubxMgaSendWaitAck(pInstance, 0x13, 0x80, pBuffer + 2, length);
                                if (errorCode == 0) {
                                    size -= length + 2; // +2 to account for the length bytes
                                    pBuffer += length + 2;
                                    blocksSent++;
                                }
                            }
                            if (pCallback != NULL) {
                                pCallback(gnssHandle, errorCode, totalBlocks, blocksSent, pCallbackParam);
                            }
                        }
                    }
                }

                if ((protocolsOut >= 0) && ((protocolsOut & (1ULL << U_GNSS_PROTOCOL_NMEA)) != 0)) {
                    // Restore NMEA messages, if we switched them off above
                    uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, true);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// End of file
