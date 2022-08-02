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
 * @brief Implementation of functions that are private to GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memmove(), strstr()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"
#include "u_port_debug.h"

#include "u_hex_bin_convert.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_device_shared.h"

#include "u_network_shared.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES
/** The length of a temporary buffer to read the raw serial port
 * data into and then search within it for ubx messages, filtering
 * them out from NMEA stuff. Since we have no way of telling
 * how much other stuff there is in the buffer, we create one
 * of twice the expected message length so that we can cope with
 * a wanted message of maximal length starting right at the end.
 * This is also the amount we need to store a hex-encoded ubx-format
 * message when receiving responses over an AT interface.
 */
# define U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES ((U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES + \
                                                U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) * 2)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A ubx message.
 */
typedef struct {
    int32_t cls;
    int32_t id;
    char *pBody;
    size_t bodyMaxLengthBytes;
} uGnssPrivateUbxMessage_t;

/* ----------------------------------------------------------------
 * VARIABLES THAT ARE SHARED THROUGHOUT THE GNSS IMPLEMENTATION
 * -------------------------------------------------------------- */

/** Root for the linked list of instances.
 */
uGnssPrivateInstance_t *gpUGnssPrivateInstanceList = NULL;

/** Mutex to protect the linked list.
 */
uPortMutexHandle_t gUGnssPrivateMutex = NULL;

/** The characteristics of the modules supported by this driver,
 * compiled into the driver.  Order is important: uGnssModuleType_t
 * is used to index into this array.
 */
const uGnssPrivateModule_t gUGnssPrivateModuleList[] = {
    {
        U_GNSS_MODULE_TYPE_M8, 0 /* features */
    },
    {
        U_GNSS_MODULE_TYPE_M9, 0 /* features */
    }
};

/** Number of items in the gUGnssPrivateModuleList array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUGnssPrivateModuleListSize = sizeof(gUGnssPrivateModuleList) /
                                           sizeof(gUGnssPrivateModuleList[0]);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table to convert a GNSS transport type into a streaming transport type.
 */
const uGnssPrivateStreamType_t gGnssPrivateTransportTypeToStream[] = {
    U_GNSS_PRIVATE_STREAM_TYPE_NONE, // U_GNSS_TRANSPORT_NONE
    U_GNSS_PRIVATE_STREAM_TYPE_UART, // U_GNSS_TRANSPORT_UBX_UART
    U_GNSS_PRIVATE_STREAM_TYPE_NONE, // U_GNSS_TRANSPORT_UBX_AT
    U_GNSS_PRIVATE_STREAM_TYPE_UART, // U_GNSS_TRANSPORT_NMEA_UART
    U_GNSS_PRIVATE_STREAM_TYPE_I2C,  // U_GNSS_TRANSPORT_UBX_I2C
    U_GNSS_PRIVATE_STREAM_TYPE_I2C   // U_GNSS_TRANSPORT_NMEA_I2C
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Send a ubx format message over UART or I2C.
static int32_t sendUbxMessageStream(int32_t streamHandle,
                                    uGnssPrivateStreamType_t streamType,
                                    const char *pMessage,
                                    size_t messageLengthBytes, bool printIt)
{
    int32_t errorCodeOrSentLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    switch (streamType) {
        case U_GNSS_PRIVATE_STREAM_TYPE_UART:
            errorCodeOrSentLength = uPortUartWrite(streamHandle, pMessage, messageLengthBytes);
            break;
        case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
            errorCodeOrSentLength = uPortI2cControllerSend(streamHandle, U_GNSS_I2C_ADDRESS,
                                                           pMessage, messageLengthBytes, false);
            if (errorCodeOrSentLength == 0) {
                errorCodeOrSentLength = messageLengthBytes;
            }
            break;
        default:
            break;
    }

    if (printIt && (errorCodeOrSentLength == messageLengthBytes)) {
        uPortLog("U_GNSS: sent ubx command");
        uGnssPrivatePrintBuffer(pMessage, messageLengthBytes);
        uPortLog(".\n");
    }

    return errorCodeOrSentLength;
}

// Receive a ubx format message over UART or I2C.
// The class and ID fields of pResponse, if present,
// should be set to the message class and ID of the expected response,
// so that other random ubx messages can be filtered out; set to -1
// for "don't care"
// Note that this message handling is good for the request-response
// style usage of the ubx protocol employed here, where we are looking
// for a particular response and are happy to throw away anything else
// that happens to be read into the buffer beyond that.  Should we,
// in the future, expect to handle asynchronous ubx messages coming from
// the GNSS chip this would need to be revisited with some form of
// rolling buffer mechanism in which store any residual stuff from the
// uPortUartRead()/whatever() after the wanted message.
static int32_t receiveUbxMessageStream(int32_t streamHandle,
                                       uGnssPrivateStreamType_t streamType,
                                       uGnssPrivateUbxMessage_t *pResponse,
                                       int32_t timeoutMs, bool printIt)
{
    int32_t errorCodeOrResponseBodyLength = 0;
    int64_t startTime;
    int32_t x = -1;
    int32_t y;
    int32_t cls;
    int32_t id;
    int32_t bytesKept = 0;
    int32_t receiveSize;
    char *pBuffer = NULL;
    char *pTmpStart;
    char *pTmpEnd;

    if ((pResponse->pBody != NULL) && (pResponse->bodyMaxLengthBytes > 0)) {
        // We need a temporary buffer to read the raw serial port
        // response into and then search within it for ubx messages,
        // filtering them out from NMEA stuff. Since we have no way of telling
        // how much other stuff there is in the buffer, we create one
        // of twice the expected message length so that we can cope with
        // a wanted message of maximal length starting right at the end
        pBuffer = (char *) malloc(U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
            startTime = uPortGetTickTimeMs();
            // Wait for something to start coming back
            while ((errorCodeOrResponseBodyLength < 0) &&
                   (uPortGetTickTimeMs() - startTime < timeoutMs)) {
                if (uGnssPrivateStreamGetReceiveSize(streamHandle, streamType) > 0) {
                    // Got something, read what we can in and check if
                    // it contains the ubx message we want (could also
                    // be NMEA stuff or unwanted ubx messages)
                    while (((receiveSize = uGnssPrivateStreamGetReceiveSize(streamHandle, streamType)) > 0) &&
                           (bytesKept < U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES) &&
                           (errorCodeOrResponseBodyLength < 0)) {
                        if (receiveSize > U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES - bytesKept) {
                            receiveSize = U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES - bytesKept;
                        }
                        // Read the response into pBuffer
                        switch (streamType) {
                            case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                                x = uPortUartRead(streamHandle, pBuffer + bytesKept,
                                                  (size_t) (unsigned) (U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES - bytesKept));
                                break;
                            case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                                x = uPortI2cControllerSendReceive(streamHandle, U_GNSS_I2C_ADDRESS,
                                                                  NULL, 0, pBuffer + bytesKept,
                                                                  (size_t) receiveSize);
                                break;
                            default:
                                break;
                        }
                        y = 0;
                        pTmpStart = pBuffer;
                        pTmpEnd = pBuffer;
                        if (x > 0) {
                            y = x + bytesKept;
                        }
                        while (y > 0) {
                            // Try to decode the new + kept buffer contents to see if
                            // the ubx message we want is now contained within it
                            errorCodeOrResponseBodyLength = uUbxProtocolDecode(pTmpStart, y,
                                                                               &cls, &id,
                                                                               pResponse->pBody,
                                                                               pResponse->bodyMaxLengthBytes,
                                                                               //lint -e(1773) Suppress attempt to cast
                                                                               // away const: I'm doing exactly the opposite...
                                                                               (const char **) &pTmpEnd);
                            if (errorCodeOrResponseBodyLength >= 0) {
                                if (errorCodeOrResponseBodyLength > (int32_t) pResponse->bodyMaxLengthBytes) {
                                    errorCodeOrResponseBodyLength = (int32_t) pResponse->bodyMaxLengthBytes;
                                }
                                if (printIt) {
                                    uPortLog("U_GNSS: decoded ubx response 0x%02x 0x%02x", cls, id);
                                    if (errorCodeOrResponseBodyLength > 0) {
                                        uPortLog(":");
                                        uGnssPrivatePrintBuffer(pResponse->pBody, errorCodeOrResponseBodyLength);
                                    }
                                    uPortLog(" [body %d byte(s)].\n", errorCodeOrResponseBodyLength);
                                }
                                if (((pResponse->cls >= 0) && (cls != pResponse->cls)) ||
                                    ((pResponse->id >= 0) && (id != pResponse->id))) {
                                    // Not what we were waiting for, try
                                    // to decode more from this buffer-full
                                    y -= (int32_t) (pTmpEnd - pTmpStart);
                                    pTmpStart = pTmpEnd;
                                    errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
                                } else {
                                    // Let the caller know the message
                                    // class/ID that came back
                                    pResponse->cls = cls;
                                    pResponse->id = id;
                                    // Exit the decode loop
                                    y = 0;
                                }
                            } else {
                                // Not a ubx message, or not a complete one anyway
                                if (errorCodeOrResponseBodyLength == (int32_t) U_ERROR_COMMON_TIMEOUT) {
                                    // We've got back a "timeout" response
                                    // from uUbxProtocolDecode(): a ubx format message
                                    // has begun but we need more data to complete it.
                                    //
                                    // pBuffer looks like this:
                                    //
                                    //     ---------------------------------------------
                                    //    |  NMEA stuff   |  partial ubx message |
                                    //     ---------------------------------------------
                                    //    |                         |---- x -----|
                                    //    ^                         ^            ^
                                    // pBuffer                  bytesKept       pTmp
                                    //
                                    // Keep what we've got in the buffer but if we've got
                                    // more than the maximal length of response then get
                                    // rid of the excess to stop the buffer clogging up
                                    bytesKept = y;
                                    if (bytesKept > U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES +
                                        U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                                        bytesKept -= U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES +
                                                     U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
                                        memmove(pBuffer, pBuffer + U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES +
                                                U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES,
                                                bytesKept);
                                    }
                                } else {
                                    // Any other response means we've got nothing,
                                    // so lose anything that has been kept
                                    bytesKept = 0;
                                }

                                // Done with this buffer-full
                                y = 0;
                            }
                        }
                    }
                } else {
                    // Relax a little
                    uPortTaskBlock(100);
                }
            }

            // Free memory
            free(pBuffer);
        }
    }

    return errorCodeOrResponseBodyLength;
}

// Send a ubx format message over an AT interface and receive
// the response.  No matching of message ID or class for
// the response is performed as it is not possible to get other
// responses when using an AT command.
static int32_t sendReceiveUbxMessageAt(const uAtClientHandle_t atHandle,
                                       const char *pSend,
                                       size_t sendLengthBytes,
                                       uGnssPrivateUbxMessage_t *pResponse,
                                       int32_t timeoutMs,
                                       bool printIt)
{
    int32_t errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    int32_t x;
    size_t bytesToSend;
    char *pBuffer;
    int32_t bytesRead;
    bool atPrintOn = uAtClientPrintAtGet(atHandle);
    bool atDebugPrintOn = uAtClientDebugGet(atHandle);

    U_ASSERT(pResponse != NULL);

    // Need a buffer to hex encode the message into
    // and receive the response into
    x = (int32_t) (sendLengthBytes * 2) + 1; // +1 for terminator
    if (x < U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES + 1) {
        x = U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES + 1;
    }
    pBuffer = (char *) malloc(x);
    if (pBuffer != NULL) {
        errorCodeOrResponseBodyLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
        bytesToSend = uBinToHex(pSend, sendLengthBytes, pBuffer);
        if (!printIt) {
            // Switch off the AT command printing if we've been
            // told not to print stuff; particularly important
            // on platforms where the C library leaks memory
            // when called from dynamically created tasks and this
            // is being called for the GNSS asynchronous API
            uAtClientPrintAtSet(atHandle, false);
            uAtClientDebugSet(atHandle, false);
        }
        // Add terminator
        *(pBuffer + bytesToSend) = 0;
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, timeoutMs);
        uAtClientCommandStart(atHandle, "AT+UGUBX=");
        uAtClientWriteString(atHandle, pBuffer, true);
        // Read the response
        uAtClientCommandStop(atHandle);
        if (printIt) {
            uPortLog("U_GNSS: sent ubx command");
            uGnssPrivatePrintBuffer(pSend, sendLengthBytes);
            uPortLog(".\n");
        }
        uAtClientResponseStart(atHandle, "+UGUBX:");
        // Read the hex-coded response back into pBuffer
        bytesRead = uAtClientReadString(atHandle, pBuffer, x, false);
        uAtClientResponseStop(atHandle);
        if ((uAtClientUnlock(atHandle) == 0) && (bytesRead >= 0)) {
            // Decode the hex into the same buffer
            x = (int32_t) uHexToBin(pBuffer, bytesRead, pBuffer);
            if (x > 0) {
                // Decode the ubx message into pResponse
                errorCodeOrResponseBodyLength = uUbxProtocolDecode(pBuffer, x,
                                                                   &(pResponse->cls),
                                                                   &(pResponse->id),
                                                                   pResponse->pBody,
                                                                   pResponse->bodyMaxLengthBytes,
                                                                   NULL);
                if (errorCodeOrResponseBodyLength >= 0) {
                    if (errorCodeOrResponseBodyLength > (int32_t) pResponse->bodyMaxLengthBytes) {
                        errorCodeOrResponseBodyLength = (int32_t) pResponse->bodyMaxLengthBytes;
                    }
                    if (printIt) {
                        uPortLog("U_GNSS: decoded ubx response 0x%02x 0x%02x",
                                 pResponse->cls, pResponse->id);
                        if (errorCodeOrResponseBodyLength > 0) {
                            uPortLog(":");
                            uGnssPrivatePrintBuffer(pResponse->pBody, errorCodeOrResponseBodyLength);
                        }
                        uPortLog(" [body %d byte(s)].\n", errorCodeOrResponseBodyLength);
                    }
                }
            }
        }

        uAtClientPrintAtSet(atHandle, atPrintOn);
        uAtClientDebugSet(atHandle, atDebugPrintOn);

        // Free memory
        free(pBuffer);
    }

    return errorCodeOrResponseBodyLength;
}

// Send a ubx format message to the GNSS module and receive
// the response.
static int32_t sendReceiveUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                     int32_t messageClass,
                                     int32_t messageId,
                                     const char *pMessageBody,
                                     size_t messageBodyLengthBytes,
                                     uGnssPrivateUbxMessage_t *pResponse)
{
    int32_t errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t bytesToSend = 0;
    char *pBuffer;

    if ((pInstance != NULL) &&
        (((pMessageBody == NULL) && (messageBodyLengthBytes == 0)) ||
         (messageBodyLengthBytes > 0)) &&
        ((pResponse->bodyMaxLengthBytes == 0) || (pResponse->pBody != NULL))) {
        errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Allocate a buffer big enough to encode the outgoing message
        pBuffer = (char *) malloc(messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCodeOrResponseBodyLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
            bytesToSend = uUbxProtocolEncode(messageClass, messageId,
                                             pMessageBody, messageBodyLengthBytes,
                                             pBuffer);
            if (bytesToSend > 0) {

                U_PORT_MUTEX_LOCK(pInstance->transportMutex);

                switch (pInstance->transportType) {
                    case U_GNSS_TRANSPORT_UBX_UART:
                    //lint -fallthrough
                    case U_GNSS_TRANSPORT_NMEA_UART:
                        errorCodeOrResponseBodyLength = sendUbxMessageStream(pInstance->transportHandle.uart,
                                                                             U_GNSS_PRIVATE_STREAM_TYPE_UART,
                                                                             pBuffer, bytesToSend,
                                                                             pInstance->printUbxMessages);
                        if (errorCodeOrResponseBodyLength >= 0) {
                            errorCodeOrResponseBodyLength = receiveUbxMessageStream(pInstance->transportHandle.uart,
                                                                                    U_GNSS_PRIVATE_STREAM_TYPE_UART,
                                                                                    pResponse, pInstance->timeoutMs,
                                                                                    pInstance->printUbxMessages);
                        }
                        break;
                    case U_GNSS_TRANSPORT_UBX_I2C:
                    //lint -fallthrough
                    case U_GNSS_TRANSPORT_NMEA_I2C:
                        errorCodeOrResponseBodyLength = sendUbxMessageStream(pInstance->transportHandle.i2c,
                                                                             U_GNSS_PRIVATE_STREAM_TYPE_I2C,
                                                                             pBuffer, bytesToSend,
                                                                             pInstance->printUbxMessages);
                        if (errorCodeOrResponseBodyLength >= 0) {
                            errorCodeOrResponseBodyLength = receiveUbxMessageStream(pInstance->transportHandle.i2c,
                                                                                    U_GNSS_PRIVATE_STREAM_TYPE_I2C,
                                                                                    pResponse, pInstance->timeoutMs,
                                                                                    pInstance->printUbxMessages);
                        }
                        break;
                    case U_GNSS_TRANSPORT_UBX_AT:
                        //lint -e{1773} Suppress attempt to cast away const: I'm not!
                        errorCodeOrResponseBodyLength = sendReceiveUbxMessageAt((const uAtClientHandle_t)
                                                                                pInstance->transportHandle.pAt,
                                                                                pBuffer, bytesToSend,
                                                                                pResponse, pInstance->timeoutMs,
                                                                                pInstance->printUbxMessages);
                        break;
                    default:
                        break;
                }

                U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);
            }

            // Free memory
            free(pBuffer);
        }
    }

    return errorCodeOrResponseBodyLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS
 * -------------------------------------------------------------- */

// Find a GNSS instance in the list by instance handle.
uGnssPrivateInstance_t *pUGnssPrivateGetInstance(uDeviceHandle_t handle)
{
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;
    uDeviceHandle_t gnssHandle = uNetworkGetDeviceHandle(handle,
                                                         U_NETWORK_TYPE_GNSS);

    if (gnssHandle == NULL) {
        // If the network function returned nothing then the handle
        // we were given wasn't obtained through the network API,
        // just use what we were given
        gnssHandle = handle;
    }
    while ((pInstance != NULL) && (pInstance->gnssHandle != gnssHandle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Get the module characteristics for a given instance.
const uGnssPrivateModule_t *pUGnssPrivateGetModule(uDeviceHandle_t gnssHandle)
{
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;
    const uGnssPrivateModule_t *pModule = NULL;

    while ((pInstance != NULL) && (pInstance->gnssHandle != gnssHandle)) {
        pInstance = pInstance->pNext;
    }

    if (pInstance != NULL) {
        pModule = pInstance->pModule;
    }

    return pModule;
}

// Print a ubx message in hex.
//lint -esym(522, uGnssPrivatePrintBuffer) Suppress "lacks side effects"
// when compiled out
void uGnssPrivatePrintBuffer(const char *pBuffer,
                             size_t bufferLengthBytes)
{
#if U_CFG_ENABLE_LOGGING
    for (size_t x = 0; x < bufferLengthBytes; x++) {
        uPortLog(" %02x", *pBuffer);
        pBuffer++;
    }
#else
    (void) pBuffer;
    (void) bufferLengthBytes;
#endif
}

// Get the streaming transport type from a given GNSS transport type.
int32_t uGnssPrivateGetStreamType(uGnssTransportType_t transportType)
{
    int32_t errorCodeOrStreamType = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((transportType >= 0) &&
        (transportType < sizeof(gGnssPrivateTransportTypeToStream) / sizeof (
             gGnssPrivateTransportTypeToStream[0]))) {
        errorCodeOrStreamType = (int32_t) gGnssPrivateTransportTypeToStream[transportType];
    }

    return errorCodeOrStreamType;
}

// Get the number of bytes waiting for us when using a streaming transport.
int32_t uGnssPrivateStreamGetReceiveSize(int32_t streamHandle,
                                         uGnssPrivateStreamType_t streamType)
{
    int32_t errorCodeOrReceiveSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char buffer[2];

    switch (streamType) {
        case U_GNSS_PRIVATE_STREAM_TYPE_UART:
            errorCodeOrReceiveSize = uPortUartGetReceiveSize(streamHandle);
            break;
        case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
            // The number of bytes waiting for us is available by a read of
            // I2C register addresses 0xFD and 0xFE in the GNSS chip.
            // The register address in the GNSS chip auto-increments, so sending
            // 0xFD, with no stop bit, and then a read request for two bytes
            // should get us the [big-endian] length
            buffer[0] = 0xFD;
            errorCodeOrReceiveSize = uPortI2cControllerSend(streamHandle, U_GNSS_I2C_ADDRESS,
                                                            buffer, 1, true);
            if (errorCodeOrReceiveSize == 0) {
                errorCodeOrReceiveSize = uPortI2cControllerSendReceive(streamHandle, U_GNSS_I2C_ADDRESS,
                                                                       NULL, 0, buffer, sizeof(buffer));
                if (errorCodeOrReceiveSize == sizeof(buffer)) {
                    errorCodeOrReceiveSize = (int32_t) ((((uint32_t) buffer[0]) << 8) + (uint32_t) buffer[1]);
                }
            }
            break;
        default:
            break;
    }

    return errorCodeOrReceiveSize;
}

// Send a ubx format message over UART or I2C.
int32_t uGnssPrivateSendOnlyStreamUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                             int32_t messageClass,
                                             int32_t messageId,
                                             const char *pMessageBody,
                                             size_t messageBodyLengthBytes)
{
    int32_t errorCodeOrSentLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t transportTypeStream;
    int32_t streamHandle = -1;
    int32_t bytesToSend = 0;
    char *pBuffer;

    if (pInstance != NULL) {
        transportTypeStream = uGnssPrivateGetStreamType(pInstance->transportType);
        if ((transportTypeStream >= 0) &&
            (((pMessageBody == NULL) && (messageBodyLengthBytes == 0)) ||
             (messageBodyLengthBytes > 0))) {
            errorCodeOrSentLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;

            // Allocate a buffer big enough to encode the outgoing message
            pBuffer = (char *) malloc(messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
            if (pBuffer != NULL) {
                bytesToSend = uUbxProtocolEncode(messageClass, messageId,
                                                 pMessageBody, messageBodyLengthBytes,
                                                 pBuffer);

                U_PORT_MUTEX_LOCK(pInstance->transportMutex);

                switch (transportTypeStream) {
                    case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                        streamHandle = pInstance->transportHandle.uart;
                        break;
                    case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                        streamHandle = pInstance->transportHandle.i2c;
                        break;
                    default:
                        break;
                }

                errorCodeOrSentLength = sendUbxMessageStream(streamHandle,
                                                             (uGnssPrivateStreamType_t) transportTypeStream,
                                                             pBuffer, bytesToSend,
                                                             pInstance->printUbxMessages);

                U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);

                // Free memory
                free(pBuffer);
            }
        }
    }

    return errorCodeOrSentLength;
}

// Send a message that has no acknowledgement and check that it was received.
int32_t uGnssPrivateSendOnlyCheckStreamUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                                  int32_t messageClass,
                                                  int32_t messageId,
                                                  const char *pMessageBody,
                                                  size_t messageBodyLengthBytes)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t userMessageSentLength;
    // Message buffer for the 120-byte UBX-MON-MSGPP message
    char message[120] = {0};
    uint64_t y;

    if ((pInstance != NULL) &&
        (uGnssPrivateGetStreamType(pInstance->transportType) >= 0)) {
        // Send UBX-MON-MSGPP to get the number of messages received
        errorCodeOrLength = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                              0x0a, 0x06,
                                                              NULL, 0,
                                                              message,
                                                              sizeof(message));
        if (errorCodeOrLength == sizeof(message)) {
            // Derive the number of messages received on the port
            y = uUbxProtocolUint64Decode(message + ((size_t) (unsigned) pInstance->portNumber * 16));
            // Now send the message
            errorCodeOrLength = uGnssPrivateSendOnlyStreamUbxMessage(pInstance, messageClass, messageId,
                                                                     pMessageBody, messageBodyLengthBytes);
            if (errorCodeOrLength == messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                userMessageSentLength = errorCodeOrLength;
                // Get the number of received messages again
                errorCodeOrLength = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                      0x0a, 0x06,
                                                                      NULL, 0,
                                                                      message,
                                                                      sizeof(message));
                if (errorCodeOrLength == sizeof(message)) {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                    y = uUbxProtocolUint64Decode(message + ((size_t) (unsigned) pInstance->portNumber * 16)) - y;
                    // Should be two: UBX-MON-MSGPP and then the send done by
                    // uGnssPrivateSendReceiveUbxMessage().
                    if (y == 2) {
                        errorCodeOrLength = userMessageSentLength;
                    }
                }
            }
        }
    }

    return errorCodeOrLength;
}

// Receive a ubx format message over UART.
// Receive a ubx format message over UART or I2C.
int32_t uGnssPrivateReceiveOnlyStreamUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                                int32_t messageClass,
                                                int32_t messageId,
                                                char *pMessageBody,
                                                size_t maxBodyLengthBytes)
{
    int32_t errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t transportTypeStream;
    int32_t streamHandle = -1;
    uGnssPrivateUbxMessage_t response;

    if (pInstance != NULL) {
        transportTypeStream = uGnssPrivateGetStreamType(pInstance->transportType);
        if ((transportTypeStream >= 0) &&
            (((pMessageBody == NULL) && (maxBodyLengthBytes == 0)) ||
             (maxBodyLengthBytes > 0))) {
            // Fill the response structure in with the message class
            // and ID we expect to get back and the buffer passed in.
            response.cls = messageClass;
            response.id = messageId;
            response.pBody = pMessageBody;
            response.bodyMaxLengthBytes = maxBodyLengthBytes;

            U_PORT_MUTEX_LOCK(pInstance->transportMutex);

            switch (transportTypeStream) {
                case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                    streamHandle = pInstance->transportHandle.uart;
                    break;
                case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                    streamHandle = pInstance->transportHandle.i2c;
                    break;
                default:
                    break;
            }

            errorCodeOrResponseBodyLength = receiveUbxMessageStream(streamHandle,
                                                                    (uGnssPrivateStreamType_t) transportTypeStream,
                                                                    &response, pInstance->timeoutMs,
                                                                    pInstance->printUbxMessages);

            U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);
        }
    }

    return errorCodeOrResponseBodyLength;
}

// Send a ubx format message to the GNSS module and receive
// a response back.
int32_t uGnssPrivateSendReceiveUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                          int32_t messageClass,
                                          int32_t messageId,
                                          const char *pMessageBody,
                                          size_t messageBodyLengthBytes,
                                          char *pResponseBody,
                                          size_t maxResponseBodyLengthBytes)
{
    uGnssPrivateUbxMessage_t response;

    // Fill the response structure in with the message class
    // and ID we expect to get back and the buffer passed in.
    response.cls = messageClass;
    response.id = messageId;
    response.pBody = pResponseBody;
    response.bodyMaxLengthBytes = maxResponseBodyLengthBytes;

    return sendReceiveUbxMessage(pInstance, messageClass, messageId,
                                 pMessageBody, messageBodyLengthBytes,
                                 &response);
}

// Send a ubx format message to the GNSS module that only has an
// Ack response and check that it is Acked.
int32_t uGnssPrivateSendUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                   int32_t messageClass,
                                   int32_t messageId,
                                   const char *pMessageBody,
                                   size_t messageBodyLengthBytes)
{
    int32_t errorCode;
    uGnssPrivateUbxMessage_t response;
    char ackBody[2];

    // Fill the response structure in with the message class
    // and ID we expect to get back and the buffer passed in.
    response.cls = 0x05;
    response.id = -1;
    response.pBody = ackBody;
    response.bodyMaxLengthBytes = sizeof(ackBody);

    errorCode = sendReceiveUbxMessage(pInstance, messageClass, messageId,
                                      pMessageBody, messageBodyLengthBytes,
                                      &response);
    if ((errorCode == 2) && (response.cls == 0x05) &&
        (*(response.pBody) == (char) messageClass) &&
        (*(response.pBody + 1) == (char) messageId)) {
        errorCode = (int32_t) U_GNSS_ERROR_NACK;
        if (response.id == 0x01) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    } else {
        errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
    }

    return errorCode;
}

// Shut down and free memory from a running pos task.
void uGnssPrivateCleanUpPosTask(uGnssPrivateInstance_t *pInstance)
{
    if (pInstance->posTaskFlags & U_GNSS_POS_TASK_FLAG_HAS_RUN) {
        // Make the pos task exit if it is running
        pInstance->posTaskFlags &= ~(U_GNSS_POS_TASK_FLAG_KEEP_GOING);
        // Wait for the task to exit
        U_PORT_MUTEX_LOCK(pInstance->posMutex);
        U_PORT_MUTEX_UNLOCK(pInstance->posMutex);
        // Free the mutex
        uPortMutexDelete(pInstance->posMutex);
        pInstance->posMutex = NULL;
        // Only now clear all of the flags so that it is safe
        // to start again
        pInstance->posTaskFlags = 0;
    }
}

// Check whether the GNSS chip is on-board the cellular module.
bool uGnssPrivateIsInsideCell(const uGnssPrivateInstance_t *pInstance)
{
    bool isInside = false;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;
    char buffer[64]; // Enough for the ATI response

    if (pInstance != NULL) {
        atHandle = pInstance->transportHandle.pAt;
        switch (pInstance->transportType) {
            case U_GNSS_TRANSPORT_UBX_AT:
                // Simplest way to check is to send ATI and see if
                // it includes an "M8"
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "ATI");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, NULL);
                bytesRead = uAtClientReadBytes(atHandle, buffer,
                                               sizeof(buffer) - 1, false);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (bytesRead > 0)) {
                    // Add a terminator
                    buffer[bytesRead] = 0;
                    if (strstr("M8", buffer) != NULL) {
                        isInside = true;
                    }
                }
                break;
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UBX_I2C:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_I2C:
            //lint -fallthrough
            default:
                break;
        }
    }

    return isInside;
}

// End of file
