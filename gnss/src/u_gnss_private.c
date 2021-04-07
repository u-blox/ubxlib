/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
#include "string.h"    // memmove()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_ubx.h"

#include "u_at_client.h"

#include "u_gnss_types.h"
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
 * message when receiving responsed over an AT interface.
 */
# define U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES ((U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES + \
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
    size_t bodyLengthBytes;
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
 * compiled into the driver.
 */
//lint -esym(552, gUGnssPrivateModuleList) Suppress not accessed: this
// is for future expansion
const uGnssPrivateModule_t gUGnssPrivateModuleList[] = {
    {
        U_GNSS_MODULE_TYPE_M8, 0 /* features */
        // Since there are currently no optional GNSS features
        // zero is given for featureBitmap.  When optional
        // features are added the zero entry would be replaced
        // with something like:
        // ((1UL << (int32_t) U_GNSS_PRIVATE_FEATURE_GREAT_THING_1) |
        //  (1UL << (int32_t) U_GNSS_PRIVATE_FEATURE_GREAT_THING_2))
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

/** For binary/hex conversion.
 */
static const char gHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
                           };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert a buffer into the ASCII hex equivalent, returning the
// number of bytes (not hex digits) encoded.  No terminator is
// added to the converted buffer.
static size_t binToHex(const char *pBin, size_t binLength, char *pHex)
{
    for (size_t x = 0; x < binLength; x++) {
        *pHex = gHex[((unsigned char) * pBin) >> 4];
        pHex++;
        *pHex = gHex[*pBin & 0x0f];
        pHex++;
        pBin++;
    }

    return binLength * 2;
}

// Convert a buffer of ASCII hex into the binary equivalent,
// returning the number of bytes encoded.  This is coded such
// that a hex message can be decoded back into the same buffer
// it arrived in.  No terminator is added to the converted
// buffer.
static size_t hexToBin(const char *pHex, size_t hexLengthBytes, char *pBin)
{
    bool success = true;
    size_t length;
    char z[2];

    for (length = 0; (length < hexLengthBytes / 2) && success; length++) {
        z[0] = *pHex - '0';
        pHex++;
        z[1] = *pHex - '0';
        pHex++;
        for (size_t y = 0; (y < sizeof(z)) && success; y++) {
            if (z[y] > 9) {
                // Must be A to F or a to f
                z[y] -= 'A' - '0';
                z[y] += 10;
            }
            if (z[y] > 15) {
                // Must be a to f
                z[y] -= 'a' - 'A';
            }
            // Cast here to shut-up a warning under ESP-IDF
            // which appears to have chars as unsigned and
            // hence thinks the first condition is always true
            success = ((signed char) z[y] >= 0) && (z[y] <= 15);
        }
        if (success) {
            *pBin = (char) (((z[0] & 0x0f) << 4) | z[1]);
            pBin++;
        }
    }

    return length;
}

// Send a ubx format message over UART and, optionally, receive
// the response.  The class and ID fields of pResponse, if present,
// should be set to the message class and ID of the expected response,
// so that other random ubx messages can be filtered out; set to -1
// for "don't care".
static int32_t sendReceiveUbxMessageUart(int32_t uartHandle, const char *pSend,
                                         size_t sendLengthBytes,
                                         uGnssPrivateUbxMessage_t *pResponse,
                                         int32_t timeoutMs)
{
    int32_t errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_SUCCESS;
    int64_t startTime;
    int32_t x = 0;
    int32_t y;
    int32_t cls;
    int32_t id;
    int32_t bytesKept = 0;
    char *pBuffer = NULL;
    char *pTmpStart;
    char *pTmpEnd;

    if (pResponse != NULL) {
        errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // The caller wants a response.  We need a temporary
        // buffer to read the raw serial port response into
        // and then search within it for ubx messages, filtering
        // them out from NMEA stuff. Since we have no way of telling
        // how much other stuff there is in the buffer, we create one
        // of twice the expected message length so that we can cope with
        // a wanted message of maximal length starting right at the end
        pBuffer = (char *) malloc(U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    if (errorCodeOrResponseBodyLength == (int32_t) U_ERROR_COMMON_SUCCESS) {
        errorCodeOrResponseBodyLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
        if (uPortUartWrite(uartHandle, pSend, sendLengthBytes) ==
            sendLengthBytes) {
            errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pBuffer != NULL) {
                //lint -esym(613, pResponse) Suppress possible use of NULL
                // pointer, it is checked above, if somewhat convolutedly
                errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
                // The caller wants the response
                startTime = uPortGetTickTimeMs();
                // Wait for something to start coming back
                while ((errorCodeOrResponseBodyLength < 0) &&
                       (uPortGetTickTimeMs() < startTime + timeoutMs)) {
                    if (uPortUartGetReceiveSize(uartHandle) > 0) {
                        // Got something, read that we can in and check if
                        // it contains the ubx message we want (could also
                        // be NMEA stuff or unwanted ubx messages)
                        while ((uPortUartGetReceiveSize(uartHandle) > 0) &&
                               (bytesKept < U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES) &&
                               (errorCodeOrResponseBodyLength < 0)) {
                            // Read the response into pBuffer
                            x = uPortUartRead(uartHandle, pBuffer + bytesKept,
                                              (size_t) (unsigned) (U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES - bytesKept));
                            y = 0;
                            pTmpStart = pBuffer;
                            pTmpEnd = pBuffer;
                            if (x > 0) {
                                y = x + bytesKept;
                            }
                            while (y > 0) {
                                // Try to decode the new + kept buffer contents to see if
                                // the ubx message we want is now contained within it
                                errorCodeOrResponseBodyLength = uUbxDecode(pTmpStart, y,
                                                                           &cls, &id,
                                                                           pResponse->pBody,
                                                                           pResponse->bodyLengthBytes,
                                                                           //lint -e(1773) Suppress attempt to cast
                                                                           // away const: I'm doing exactly the opposite...
                                                                           (const char **) &pTmpEnd);
                                if (errorCodeOrResponseBodyLength >= 0) {
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
                                        // from uUbxDecode(): a ubx format message
                                        // has begun but we need to get for more data
                                        // to complete it.
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
                                        if (bytesKept > U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES +
                                            U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                                            bytesKept -= U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES +
                                                         U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
                                            memmove(pBuffer, pBuffer + U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES +
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
            }
        }
    }

    // Free memory; it is valid C to free a NULL pointer
    free(pBuffer);

    return errorCodeOrResponseBodyLength;
}

// Send a ubx format message over an AT interface and, optionally,
// receive the response.  No matching of message ID or class for
// the response is performed as it is not possible to get other
// responses when using an AT command.
static int32_t sendReceiveUbxMessageAt(const uAtClientHandle_t atHandle,
                                       const char *pSend,
                                       size_t sendLengthBytes,
                                       uGnssPrivateUbxMessage_t *pResponse,
                                       int32_t timeoutMs)
{
    int32_t errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    int32_t x;
    size_t bytesToSend;
    char *pBuffer;
    char *pTmp = NULL;
    int32_t bytesRead;

    // Need a buffer to hex encode the message into
    // and receive the response into
    x = (int32_t) (sendLengthBytes * 2) + 1; // +1 for terminator
    if (x < U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES + 1) {
        x = U_GNSS_TEMPORARY_BUFFER_LENGTH_BYTES + 1;
    }
    pBuffer = (char *) malloc(x);
    if (pBuffer != NULL) {
        errorCodeOrResponseBodyLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
        bytesToSend = binToHex(pSend, sendLengthBytes, pBuffer);
        // Add terminator
        *(pBuffer + bytesToSend) = 0;
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, timeoutMs);
        uAtClientCommandStart(atHandle, "AT+UGUBX=");
        uAtClientWriteString(atHandle, pBuffer, true);
        // Read the response
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UGUBX:");
        if (pResponse != NULL) {
            pTmp = pBuffer;
        }
        // Read the hex-coded response back into pTmp
        // (which may be NULL to throw the response away)
        bytesRead = uAtClientReadString(atHandle, pTmp, x, false);
        uAtClientResponseStop(atHandle);
        if ((uAtClientUnlock(atHandle) == 0) && (bytesRead >= 0)) {
            errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pTmp != NULL) {
                // Decode the hex into the same buffer
                x = (int32_t) hexToBin(pTmp, bytesRead, pTmp);
                if (x > 0) {
                    // Decode the ubx message into pResponse
                    errorCodeOrResponseBodyLength = uUbxDecode(pTmp, x,
                                                               &(pResponse->cls),
                                                               &(pResponse->id),
                                                               pResponse->pBody,
                                                               pResponse->bodyLengthBytes,
                                                               NULL);
                }
            }
        }

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
        (((pResponse->pBody == NULL) && (pResponse->bodyLengthBytes == 0)) ||
         (pResponse->bodyLengthBytes > 0))) {
        errorCodeOrResponseBodyLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Allocate a buffer big enough to encode the outgoing message
        pBuffer = (char *) malloc(messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCodeOrResponseBodyLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
            bytesToSend = uUbxEncode(messageClass, messageId,
                                     pMessageBody, messageBodyLengthBytes,
                                     pBuffer);
            if (bytesToSend > 0) {
                switch (pInstance->transportType) {
                    case U_GNSS_TRANSPORT_UBX_UART:
                    //lint -fallthrough
                    case U_GNSS_TRANSPORT_NMEA_UART:
                        errorCodeOrResponseBodyLength = sendReceiveUbxMessageUart(pInstance->transportHandle.uart,
                                                                                  pBuffer, bytesToSend,
                                                                                  pResponse, pInstance->timeoutMs);
                        break;
                    case U_GNSS_TRANSPORT_UBX_AT:
                        //lint -e{1773} Suppress attempt to cast away const: I'm not!
                        errorCodeOrResponseBodyLength = sendReceiveUbxMessageAt((const uAtClientHandle_t)
                                                                                pInstance->transportHandle.pAt,
                                                                                pBuffer, bytesToSend,
                                                                                pResponse, pInstance->timeoutMs);
                        break;
                    default:
                        break;
                }
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
uGnssPrivateInstance_t *pUGnssPrivateGetInstance(int32_t handle)
{
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->handle != handle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Get the module characteristics for a given instance.
const uGnssPrivateModule_t *pUGnssPrivateGetModule(int32_t handle)
{
    uGnssPrivateInstance_t *pInstance = gpUGnssPrivateInstanceList;
    const uGnssPrivateModule_t *pModule = NULL;

    while ((pInstance != NULL) && (pInstance->handle != handle)) {
        pInstance = pInstance->pNext;
    }

    if (pInstance != NULL) {
        pModule = pInstance->pModule;
    }

    return pModule;
}

// Send a ubx format message to the GNSS module and receive
// the response.
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
    response.bodyLengthBytes = maxResponseBodyLengthBytes;

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
    response.bodyLengthBytes = sizeof(ackBody);

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

// End of file
