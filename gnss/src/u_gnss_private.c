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
 * IMPORTANT: this code is changing a lot at the moment as we move
 * towards a more generic, streamed, approach - beware!
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memmove(), strstr()
#include "ctype.h"

#include "u_cfg_os_platform_specific.h"
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
#include "u_gnss_msg.h"
#include "u_gnss_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_AT_BUFFER_LENGTH_BYTES
/** The length of a temporary buffer store a hex-encoded ubx-format
 * message when receiving responses over an AT interface.
 */
# define U_GNSS_AT_BUFFER_LENGTH_BYTES ((U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES + \
                                         U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) * 2)
#endif

/** The maximum length of an NMEA message/sentence, including the
 * $ on the front and the CR/LF on the end.  Note that a buffer
 * of size twice this is put on the stack in
 * uGnssPrivateStreamDecodeRingBuffer() and hence it cannot be
 * made much bigger; not that there's a need to 'cos it's fixed
 * by the NMEA standard.
  */
#define U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES 82

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a received ubx-format message.
 */
typedef struct {
    int32_t cls;
    int32_t id;
    char **ppBody;   /**< a pointer to a pointer that the received
                          message body will be written to. If *ppBody is NULL
                          memory will be allocated.  If ppBody is NULL
                          then the response is not captured (but this
                          structure may still be used for cls/id matching). */
    size_t bodySize; /**< the number of bytes of storage at *ppBody;
                          must be zero if ppBody is NULL or
                          *ppBody is NULL.  If non-zero it MUST be
                          large enough to fit the body in or the
                          CRC calculation will fail. */
} uGnssPrivateUbxReceiveMessage_t;

/** Track state of UBX message decode matching.
 */
typedef enum {
    U_GNSS_PRIVATE_UBX_MATCH_NULL = 0,                    /**< no message yet detected. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_B5 = 1,      /**< got the 0xb5. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_62 = 2,      /**< got the 0x62. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_CLASS = 3,       /**< got the message class. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_ID = 4,          /**< got the message ID. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_LENGTH_BYTE_LOWER = 5,   /**< got the first byte of the length. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER = 6,              /**< got all of the message header. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_CLASS = 7,  /**< in a NACK, got the message class. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_ID = 8,     /**< in a NACK, got the message class. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_CRC_BYTE_1 = 9,          /**< got the first byte of the CRC. */
    U_GNSS_PRIVATE_UBX_MATCH_GOT_WHOLE_MESSAGE = 10,      /**< got a whole message. */
    U_GNSS_PRIVATE_UBX_MATCH_MAX_NUM
} uGnssPrivateUbxMatch_t;

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
        U_GNSS_MODULE_TYPE_M9,
        ((1UL << (int32_t) U_GNSS_PRIVATE_FEATURE_CFGVALXXX) /* features */
        )
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
    U_GNSS_PRIVATE_STREAM_TYPE_UART, // U_GNSS_TRANSPORT_UART
    U_GNSS_PRIVATE_STREAM_TYPE_NONE, // U_GNSS_TRANSPORT_AT
    U_GNSS_PRIVATE_STREAM_TYPE_I2C,  // U_GNSS_TRANSPORT_I2C
    U_GNSS_PRIVATE_STREAM_TYPE_UART, // U_GNSS_TRANSPORT_UBX_UART
    U_GNSS_PRIVATE_STREAM_TYPE_I2C   // U_GNSS_TRANSPORT_UBX_I2C
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MESSAGE RELATED
 * -------------------------------------------------------------- */

// Find the header of a ubx-format message in the given buffer,
// returning the number of bytes in the entire message (header
// and check-sum etc. included).  If a matching message is found
// pDiscard (which cannot be NULL) will be populated with the distance
// into pBuffer that the message begins; if a matching message is NOT
// found pDiscard will be populated with the amount of data that can be
// discarded; NOTE that this could be MORE than "size", since for a
// non-matching message we can discard the length of body + CRC that is
// to come.
// On entry pMessageClassAndId should contain the required message class
// (most significant byte) and ID (least significant byte), wildcards
// permitted, on exit this will be populated with the message class
// ID found. If a partial header is found U_ERROR_COMMON_TIMEOUT
// will be returned.
// Under some circumstance it is useful to check, in addition, for
// a NACK message for the given message class and ID landing at the
// same time.  Where this is the case checkNack should be set; if
// a NACK is found the error code will be U_GNSS_ERROR_NACK and, for
// this case, we do check the CRC.
// See also uGnssPrivateDecodeNmea().
static int32_t matchUbxMessageHeader(const char *pBuffer, size_t size,
                                     uint16_t *pMessageClassAndId,
                                     size_t *pDiscard,
                                     bool checkNack)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    const uint8_t *pInput = (const uint8_t *) pBuffer;
    size_t x;
    uGnssPrivateUbxMatch_t match = U_GNSS_PRIVATE_UBX_MATCH_NULL;
    int32_t ca = 0;
    int32_t cb = 0;
    bool updateCrc = false;
    bool idMatch = false;
    bool gotNack = false;
    uint16_t messageBodyLength = 0;
    uint8_t messageClass = U_GNSS_UBX_MESSAGE_CLASS_ALL;
    uint8_t messageId = U_GNSS_UBX_MESSAGE_ID_ALL;
    uint16_t nackMessageClassAndId = 0;
    uint8_t firstCrcByte;

    U_ASSERT(pDiscard != NULL);

    *pDiscard = 0;
    if (pMessageClassAndId != NULL) {
        messageClass = (uint8_t) (*pMessageClassAndId >> 8);
        messageId = (uint8_t) (*pMessageClassAndId & 0xff);
    }
    // Normally we only want the header; for the NACK case we
    // want the whole message and will CRC check it
    for (x = 0; (x < size) &&
         ((!gotNack && (match < U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER)) ||
          (gotNack && (match < U_GNSS_PRIVATE_UBX_MATCH_GOT_WHOLE_MESSAGE))); x++) {
        switch (match) {
            case U_GNSS_PRIVATE_UBX_MATCH_NULL:
                if (*pInput == 0xb5) {
                    // Got first byte of header
                    // We can always discard the stuff up to the point where the
                    // potential message began
                    *pDiscard = (const char *) pInput - pBuffer;
                    match = U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_B5;
                }
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_B5:
                match = U_GNSS_PRIVATE_UBX_MATCH_NULL;
                if (*pInput == 0x62) {
                    // Got second byte of header
                    match = U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_62;
                }
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER_BYTE_62:
                // Got message class, store it
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_CLASS;
                if ((messageClass == U_GNSS_UBX_MESSAGE_CLASS_ALL) ||
                    (messageClass == *pInput)) {
                    messageClass = *pInput;
                    idMatch = true;
                }
                if (checkNack && (*pInput == 0x05)) {
                    gotNack = true;
                    // If this is a nack then we need to check the
                    // CRC as we need the two bytes of body
                    ca = 0;
                    cb = 0;
                    updateCrc = true;
                }
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_CLASS:
                // Got message ID, store it
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_ID;
                if ((messageId == U_GNSS_UBX_MESSAGE_ID_ALL) ||
                    (messageId == *pInput)) {
                    messageId = *pInput;
                } else {
                    idMatch = false;
                }
                if (*pInput != 0x00) {
                    gotNack = false;
                }
                updateCrc = gotNack;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_MESSAGE_ID:
                // Got first byte of length, store it
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_LENGTH_BYTE_LOWER;
                messageBodyLength = *pInput;
                updateCrc = gotNack;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_LENGTH_BYTE_LOWER:
                // Got second byte of length, add it to the first
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER;
                messageBodyLength += ((size_t) *pInput) << 8; // *NOPAD*
                if (messageBodyLength != 2) {
                    // NACKs must have a body length of 2
                    gotNack = false;
                }
                updateCrc = gotNack;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER:
                // Must be in a NACK, grab the class of the NACKed' message from the body
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_CLASS;
                nackMessageClassAndId = ((uint16_t) *pInput) << 8; // *NOPAD*
                updateCrc = gotNack;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_CLASS:
                // Grab the ID of the NACKed message from the body
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_ID;
                nackMessageClassAndId |= *pInput; // *NOPAD*
                updateCrc = gotNack;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_NACK_MESSAGE_ID:
                // That's it for the NACK message body, grab the first CRC byte
                match = U_GNSS_PRIVATE_UBX_MATCH_GOT_CRC_BYTE_1;
                firstCrcByte = *pInput;
                break;
            case U_GNSS_PRIVATE_UBX_MATCH_GOT_CRC_BYTE_1:
                // Whole CRC, est arrivee, check it
                match = U_GNSS_PRIVATE_UBX_MATCH_NULL;
                if (((uint8_t) ca == firstCrcByte) && ((uint8_t) cb == *pInput)) {
                    match = U_GNSS_PRIVATE_UBX_MATCH_GOT_WHOLE_MESSAGE;
                }
                break;
            default:
                match = U_GNSS_PRIVATE_UBX_MATCH_NULL;
                break;
        }

        if (updateCrc) {
            ca += *pInput;
            cb += ca;
            updateCrc = false;
        }

        // Next byte
        pInput++;
    }

    if (match != U_GNSS_PRIVATE_UBX_MATCH_NULL) {
        // We got some parts of the message overhead, so
        // could be a message
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (match >= U_GNSS_PRIVATE_UBX_MATCH_GOT_HEADER) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            if (idMatch) {
                // Got a matching message, populate the message class/ID,
                // and return the whole message length, including header
                // and CRC
                if (pMessageClassAndId != NULL) {
                    *pMessageClassAndId = (((uint16_t) messageClass) << 8) | messageId;
                }
                errorCodeOrLength = messageBodyLength + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
            } else if (checkNack && gotNack && (match == U_GNSS_PRIVATE_UBX_MATCH_GOT_WHOLE_MESSAGE) &&
                       (pMessageClassAndId != NULL) && (*pMessageClassAndId == nackMessageClassAndId)) {
                // We were interested in NACK messages, we've captured a whole one with
                // correct CRC, and the message class and ID stored in the body of the
                // NACK message matches what we're looking for; we've been NACKed
                errorCodeOrLength = U_GNSS_ERROR_NACK;
                // We can now discard the whole NACK message, add to pDiscard the
                // length of the unwanted message body plus overhead
                *pDiscard += messageBodyLength + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
            } else {
                // Not an ID match
                *pDiscard += messageBodyLength + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
            }
        }
    } else {
        // Nothing; put into pDiscard all that we've processed
        *pDiscard = size;
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: STREAMING TRANSPORT ONLY
 * -------------------------------------------------------------- */

// Read or peek-at the data in the internal ring buffer.
static int32_t streamGetFromRingBuffer(uGnssPrivateInstance_t *pInstance,
                                       int32_t readHandle,
                                       char *pBuffer, size_t size,
                                       size_t offset,
                                       int32_t maxTimeMs,
                                       bool andRemove)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    size_t receiveSize;
    size_t totalSize = 0;
    int32_t x;
    size_t leftToRead = size;
    int32_t startTimeMs;

    if (pInstance != NULL) {
        startTimeMs = uPortGetTickTimeMs();
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
        while ((leftToRead > 0) &&
               (uPortGetTickTimeMs() - startTimeMs < maxTimeMs)) {
            if (andRemove) {
                receiveSize = (int32_t) uRingBufferReadHandle(&(pInstance->ringBuffer),
                                                              readHandle,
                                                              pBuffer, leftToRead);
            } else {
                receiveSize = (int32_t) uRingBufferPeekHandle(&(pInstance->ringBuffer),
                                                              readHandle,
                                                              pBuffer, leftToRead,
                                                              offset);
                offset += receiveSize;
            }
            leftToRead -= receiveSize;
            totalSize += receiveSize;
            if (pBuffer != NULL) {
                pBuffer += receiveSize;
            }
            if (receiveSize == 0) {
                // Just pull what's already there in, otherwise
                // we could flood the ring-buffer with data when
                // we're not actually reading it out, just peeking
                x = uGnssPrivateStreamFillRingBuffer(pInstance, 0, 0);
                if (x < 0) {
                    errorCodeOrLength = x;
                }
            }
        }
        if (totalSize > 0) {
            errorCodeOrLength = (int32_t) totalSize;
        }
    }

    return errorCodeOrLength;
}

// Send a message over UART or I2C.
static int32_t sendMessageStream(int32_t streamHandle,
                                 uGnssPrivateStreamType_t streamType,
                                 uint16_t i2cAddress,
                                 const char *pMessage,
                                 size_t messageLengthBytes, bool printIt)
{
    int32_t errorCodeOrSentLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    switch (streamType) {
        case U_GNSS_PRIVATE_STREAM_TYPE_UART:
            errorCodeOrSentLength = uPortUartWrite(streamHandle, pMessage, messageLengthBytes);
            break;
        case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
            errorCodeOrSentLength = uPortI2cControllerSend(streamHandle, i2cAddress,
                                                           pMessage, messageLengthBytes, false);
            if (errorCodeOrSentLength == 0) {
                errorCodeOrSentLength = messageLengthBytes;
            }
            break;
        default:
            break;
    }

    if (printIt && (errorCodeOrSentLength == messageLengthBytes)) {
        uPortLog("U_GNSS: sent command");
        uGnssPrivatePrintBuffer(pMessage, messageLengthBytes);
        uPortLog(".\n");
    }

    return errorCodeOrSentLength;
}

// Receive a ubx format message over UART or I2C.
// On entry pResponse should be set to the message class and ID of the
// expected response, wild cards permitted.  On success it will
// be set to the message ID received and the ubx message body length
// will be returned.
static int32_t receiveUbxMessageStream(uGnssPrivateInstance_t *pInstance,
                                       uGnssPrivateUbxReceiveMessage_t *pResponse,
                                       int32_t timeoutMs, bool printIt)
{
    int32_t errorCodeOrLength = 0;            // Deliberate choice to return 0 if pResponse
    uGnssPrivateMessageId_t privateMessageId; // indicates that no response is required
    char *pBuffer = NULL;

    if ((pInstance != NULL) && (pResponse != NULL) && (pResponse->ppBody != NULL)) {
        // Convert uGnssPrivateUbxReceiveMessage_t into uGnssPrivateMessageId_t
        privateMessageId.type = U_GNSS_PROTOCOL_UBX;
        privateMessageId.id.ubx = (U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) | U_GNSS_UBX_MESSAGE_ID_ALL;
        if (pResponse->cls >= 0) {
            privateMessageId.id.ubx = (privateMessageId.id.ubx & 0x00ff) | (((uint16_t) pResponse->cls) << 8);
        }
        if (pResponse->id >= 0) {
            privateMessageId.id.ubx = (privateMessageId.id.ubx & 0xff00) | pResponse->id;
        }
        // Now wait for the message, allowing a buffer to be allocated by
        // the message receive function
        errorCodeOrLength = uGnssPrivateReceiveStreamMessage(pInstance,
                                                             &privateMessageId,
                                                             pInstance->ringBufferReadHandlePrivate,
                                                             &pBuffer, 0,
                                                             timeoutMs, NULL);
        if (errorCodeOrLength >= U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
            // Convert uGnssPrivateMessageId_t into uGnssPrivateUbxReceiveMessage_t
            pResponse->cls = privateMessageId.id.ubx >> 8;
            pResponse->id = privateMessageId.id.ubx & 0xFF;
            // Check the message is good
            if (uGnssMsgIsGood(pBuffer, errorCodeOrLength)) {
                // Remove the protocol overhead from the length, we just want the body
                errorCodeOrLength -= U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
                // Copy the body of the message into the response
                if (*(pResponse->ppBody) == NULL) {
                    *(pResponse->ppBody) = (char *) malloc(errorCodeOrLength);
                } else {
                    if (errorCodeOrLength > (int32_t) pResponse->bodySize) {
                        errorCodeOrLength = (int32_t) pResponse->bodySize;
                    }
                }
                if (*(pResponse->ppBody) != NULL) {
                    memcpy(*(pResponse->ppBody), pBuffer + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES, errorCodeOrLength);
                    if (printIt) {
                        uPortLog("U_GNSS: decoded ubx response 0x%02x 0x%02x",
                                 privateMessageId.id.ubx >> 8, privateMessageId.id.ubx & 0xff);
                        if (errorCodeOrLength > 0) {
                            uPortLog(":");
                            uGnssPrivatePrintBuffer(*(pResponse->ppBody), errorCodeOrLength);
                        }
                        uPortLog(" [body %d byte(s)].\n", errorCodeOrLength);
                    }
                } else {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                }
            } else {
                // We assume here that this really was the message
                // we were after, but corrupted, hence no point
                // in waiting any longer
                errorCodeOrLength = (int32_t) U_GNSS_ERROR_CRC;
                if (printIt) {
                    uPortLog("U_GNSS: CRC error.\n");
                }
            }
        } else if (printIt && (errorCodeOrLength == (int32_t) U_GNSS_ERROR_NACK)) {
            uPortLog("U_GNSS: got Nack for 0x%02x 0x%02x.\n",
                     pResponse->cls, pResponse->id);
        }

        // Free memory (it is legal C to free a NULL pointer)
        free(pBuffer);
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: AT TRANSPORT ONLY
 * -------------------------------------------------------------- */

// Send a ubx format message over an AT interface and receive
// the response.  No matching of message ID or class for
// the response is performed as it is not possible to get other
// responses when using an AT command.
static int32_t sendReceiveUbxMessageAt(const uAtClientHandle_t atHandle,
                                       const char *pSend,
                                       size_t sendLengthBytes,
                                       uGnssPrivateUbxReceiveMessage_t *pResponse,
                                       int32_t timeoutMs,
                                       bool printIt)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    int32_t x;
    size_t bytesToSend;
    char *pBuffer;
    bool bufferReuse = false;
    int32_t bytesRead;
    size_t captureSize;
    int32_t clsNack = 0x05;
    int32_t idNack = 0x00;
    char ackBody[2] = {0};
    bool atPrintOn = uAtClientPrintAtGet(atHandle);
    bool atDebugPrintOn = uAtClientDebugGet(atHandle);

    U_ASSERT(pResponse != NULL);

    // Need a buffer to hex encode the message into
    // and receive the response into
    x = (int32_t) (sendLengthBytes * 2) + 1; // +1 for terminator
    if (x < U_GNSS_AT_BUFFER_LENGTH_BYTES + 1) {
        x = U_GNSS_AT_BUFFER_LENGTH_BYTES + 1;
    }
    pBuffer = (char *) malloc(x);
    if (pBuffer != NULL) {
        errorCodeOrLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
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
        if ((uAtClientUnlock(atHandle) == 0) && (bytesRead >= 0) &&
            (pResponse->ppBody != NULL)) {
            // Decode the hex into the same buffer
            x = (int32_t) uHexToBin(pBuffer, bytesRead, pBuffer);
            if (x > 0) {
                // Deal with the output buffer
                captureSize = x;
                if (*(pResponse->ppBody) != NULL) {
                    if (captureSize > pResponse->bodySize) {
                        captureSize = pResponse->bodySize;
                    }
                } else {
                    // We can just re-use the buffer we already have
                    *(pResponse->ppBody) = pBuffer;
                    bufferReuse = true;
                }
                errorCodeOrLength = (int32_t) captureSize;
                if (captureSize > 0) {
                    // First check if we received a NACK
                    if ((uUbxProtocolDecode(pBuffer, x, &clsNack, &idNack,
                                            ackBody, sizeof(ackBody),
                                            NULL) == 2) &&
                        (ackBody[0] == pResponse->cls) &&
                        (ackBody[1] == pResponse->id)) {
                        // We got a NACK for the message class
                        // and ID we are monitoring
                        errorCodeOrLength = (int32_t) U_GNSS_ERROR_NACK;
                    } else {
                        // No NACK, we can decode the message body, noting
                        // that it is safe to decode back into the same buffer
                        errorCodeOrLength = uUbxProtocolDecode(pBuffer, x,
                                                               &(pResponse->cls),
                                                               &(pResponse->id),
                                                               *(pResponse->ppBody),
                                                               captureSize, NULL);
                        if (errorCodeOrLength > (int32_t) captureSize) {
                            errorCodeOrLength = (int32_t) captureSize;
                        }
                    }
                }
                if (printIt) {
                    if (errorCodeOrLength >= 0) {
                        uPortLog("U_GNSS: decoded ubx response 0x%02x 0x%02x",
                                 pResponse->cls, pResponse->id);
                        if (errorCodeOrLength > 0) {
                            uPortLog(":");
                            uGnssPrivatePrintBuffer(*(pResponse->ppBody), errorCodeOrLength);
                        }
                        uPortLog(" [body %d byte(s)].\n", errorCodeOrLength);
                    } else if (errorCodeOrLength == (int32_t) U_GNSS_ERROR_NACK) {
                        uPortLog("U_GNSS: got Nack for 0x%02x 0x%02x.\n",
                                 pResponse->cls, pResponse->id);
                    }
                }
            }
        }

        uAtClientPrintAtSet(atHandle, atPrintOn);
        uAtClientDebugSet(atHandle, atDebugPrintOn);

        if (!bufferReuse) {
            free(pBuffer);
        }
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: ANY TRANSPORT
 * -------------------------------------------------------------- */

// Send a ubx format message to the GNSS module and receive
// the response.
static int32_t sendReceiveUbxMessage(uGnssPrivateInstance_t *pInstance,
                                     int32_t messageClass,
                                     int32_t messageId,
                                     const char *pMessageBody,
                                     size_t messageBodyLengthBytes,
                                     uGnssPrivateUbxReceiveMessage_t *pResponse)
{
    int32_t errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t bytesToSend = 0;
    char *pBuffer;

    if ((pInstance != NULL) &&
        (((pMessageBody == NULL) && (messageBodyLengthBytes == 0)) ||
         (messageBodyLengthBytes > 0)) &&
        ((pResponse->bodySize == 0) || (pResponse->ppBody != NULL))) {
        errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Allocate a buffer big enough to encode the outgoing message
        pBuffer = (char *) malloc(messageBodyLengthBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCodeOrResponseLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
            bytesToSend = uUbxProtocolEncode(messageClass, messageId,
                                             pMessageBody, messageBodyLengthBytes,
                                             pBuffer);
            if (bytesToSend > 0) {

                U_PORT_MUTEX_LOCK(pInstance->transportMutex);

                if ((pResponse != NULL) && (pResponse->ppBody != NULL) &&
                    (uGnssPrivateGetStreamType(pInstance->transportType) >= 0)) {
                    // For a streaming transport, if we're going to wait for
                    // a response, make sure that any historical data is
                    // cleared from our handle in the ring buffer so that
                    // we don't pick it up instead and lock our read
                    // pointer before we do the send so that we are sure
                    // we won't lose the response
                    uGnssPrivateStreamFillRingBuffer(pInstance, 0, 0);
                    uRingBufferLockReadHandle(&(pInstance->ringBuffer),
                                              pInstance->ringBufferReadHandlePrivate);
                    uRingBufferFlushHandle(&(pInstance->ringBuffer),
                                           pInstance->ringBufferReadHandlePrivate);
                }
                switch (pInstance->transportType) {
                    case U_GNSS_TRANSPORT_UART:
                    //lint -fallthrough
                    case U_GNSS_TRANSPORT_UBX_UART:
                        errorCodeOrResponseLength = sendMessageStream(pInstance->transportHandle.uart,
                                                                      U_GNSS_PRIVATE_STREAM_TYPE_UART,
                                                                      pInstance->i2cAddress,
                                                                      pBuffer, bytesToSend,
                                                                      pInstance->printUbxMessages);
                        if (errorCodeOrResponseLength >= 0) {
                            errorCodeOrResponseLength = receiveUbxMessageStream(pInstance, pResponse,
                                                                                pInstance->timeoutMs,
                                                                                pInstance->printUbxMessages);
                        }
                        break;
                    case U_GNSS_TRANSPORT_I2C:
                    //lint -fallthrough
                    case U_GNSS_TRANSPORT_UBX_I2C:
                        errorCodeOrResponseLength = sendMessageStream(pInstance->transportHandle.i2c,
                                                                      U_GNSS_PRIVATE_STREAM_TYPE_I2C,
                                                                      pInstance->i2cAddress,
                                                                      pBuffer, bytesToSend,
                                                                      pInstance->printUbxMessages);
                        if (errorCodeOrResponseLength >= 0) {
                            errorCodeOrResponseLength = receiveUbxMessageStream(pInstance, pResponse,
                                                                                pInstance->timeoutMs,
                                                                                pInstance->printUbxMessages);
                        }
                        break;
                    case U_GNSS_TRANSPORT_AT:
                        //lint -e{1773} Suppress attempt to cast away const: I'm not!
                        errorCodeOrResponseLength = sendReceiveUbxMessageAt((const uAtClientHandle_t)
                                                                            pInstance->transportHandle.pAt,
                                                                            pBuffer, bytesToSend,
                                                                            pResponse, pInstance->timeoutMs,
                                                                            pInstance->printUbxMessages);
                        break;
                    default:
                        break;
                }

                // Make sure the read handle is always unlocked afterwards
                uRingBufferUnlockReadHandle(&(pInstance->ringBuffer), pInstance->ringBufferReadHandlePrivate);

                U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);
            }

            // Free memory
            free(pBuffer);
        }
    }

    return errorCodeOrResponseLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS: MISC
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

// Set the protocol type output by the GNSS chip.
int32_t uGnssPrivateSetProtocolOut(uGnssPrivateInstance_t *pInstance,
                                   uGnssProtocol_t protocol,
                                   bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    // Message buffer for the 120-byte UBX-MON-MSGPP message
    char message[120] = {0};
    uint16_t mask = 0;
    uint64_t x;

    if ((pInstance != NULL) &&
        (pInstance->transportType != U_GNSS_TRANSPORT_AT) &&
        (onNotOff || ((protocol != U_GNSS_PROTOCOL_ALL) &&
                      (protocol != U_GNSS_PROTOCOL_UBX)))) {
        errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
        // Normally we would send the UBX-CFG-PRT message
        // by calling uGnssPrivateSendUbxMessage() which
        // would wait for an ack.  However, in this particular
        // case, the other parameters in the message are
        // serial port settings and, even though we are not
        // changing them, the returned UBX-ACK-ACK message
        // is often corrupted as a result.
        // The workaround is to avoid waiting for the ack by
        // using uGnssPrivateSendReceiveUbxMessage() with
        // an empty response buffer but, before we do that,
        // we send UBX-MON-MSGPP to determine the number of
        // messages received by the GNSS chip on the UART port
        // and then we check it again afterwards to be sure that
        // our UBX-CFG-PRT messages really were received.
        if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                              0x0a, 0x06,
                                              NULL, 0,
                                              message,
                                              sizeof(message)) == sizeof(message)) {
            // Get the number of messages received on the port
            x = uUbxProtocolUint64Decode(message + ((size_t) (unsigned) pInstance->portNumber * 16));
            // Now poll the GNSS chip for UBX-CFG-PRT to get the
            // existing configuration for the port we are connected on
            message[0] = (char) pInstance->portNumber;
            if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                  0x06, 0x00,
                                                  message, 1,
                                                  message, 20) == 20) {
                // Offsets 14 and 15 contain the output protocol bit-map
                mask = uUbxProtocolUint16Decode((const char *) & (message[14]));
                if (protocol == U_GNSS_PROTOCOL_ALL) {
                    mask = 0xFFFF; // Everything out
                } else {
                    if (onNotOff) {
                        mask |= 1 << protocol;
                    } else {
                        mask &= ~(1 << protocol);
                    }
                }
                *((uint16_t *) & (message[14])) = uUbxProtocolUint16Encode(mask);
                // Send the message and don't wait for response or ack
                errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                              0x06, 0x00,
                                                              message, 20,
                                                              NULL, 0);
                // Skip any serial port perturbance at the far end
                uPortTaskBlock(100);
                // Get the number of received messages again
                if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                      0x0a, 0x06,
                                                      NULL, 0,
                                                      message,
                                                      sizeof(message)) == sizeof(message)) {
                    x = uUbxProtocolUint64Decode(message + ((size_t) (unsigned) pInstance->portNumber * 16)) - x;
                    // Should be three: UBX-MON-MSGPP, the poll for UBX-CFG-PRT
                    // and then the UBX-CFG-PRT setting command itself.
                    if (x == 3) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            }
        }
    }

    return errorCode;
}

// Get the protocol types output by the GNSS chip.
int32_t uGnssPrivateGetProtocolOut(uGnssPrivateInstance_t *pInstance)
{
    int32_t errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    // Message buffer for the 20-byte UBX-CFG-PRT message
    char message[20] = {0};

    if ((pInstance != NULL) && (pInstance->transportType != U_GNSS_TRANSPORT_AT)) {
        errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_PLATFORM;
        // Poll the GNSS chip with UBX-CFG-PRT
        message[0] = (char) pInstance->portNumber;
        if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                              0x06, 0x00,
                                              message, 1,
                                              message,
                                              sizeof(message)) == sizeof(message)) {
            // Offsets 14 and 15 contain the output protocol bit-map
            errorCodeOrBitMap = (int32_t) uUbxProtocolUint16Decode((const char *) & (message[14]));
            if (errorCodeOrBitMap < 0) {
                // Don't expect to have the top-bit set so flag an error
                errorCodeOrBitMap = U_ERROR_COMMON_PLATFORM;
            }
        }
    }

    return errorCodeOrBitMap;
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
        if (pInstance->transportType == U_GNSS_TRANSPORT_AT) {
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
        }
    }

    return isInside;
}

// Stop the asynchronous message receive task.
void uGnssPrivateStopMsgReceive(uGnssPrivateInstance_t *pInstance)
{
    char queueItem[U_GNSS_MSG_RECEIVE_TASK_QUEUE_ITEM_SIZE_BYTES];
    uGnssPrivateMsgReceive_t *pMsgReceive;
    uGnssPrivateMsgReader_t *pNext;

    if ((pInstance != NULL) && (pInstance->pMsgReceive != NULL)) {
        pMsgReceive = pInstance->pMsgReceive;

        // Sending the task anything will cause it to exit
        uPortQueueSend(pMsgReceive->taskExitQueueHandle, queueItem);
        U_PORT_MUTEX_LOCK(pMsgReceive->taskRunningMutexHandle);
        U_PORT_MUTEX_UNLOCK(pMsgReceive->taskRunningMutexHandle);
        // Wait for the task to actually exit: the STM32F4 platform
        // needs this additional delay for some reason or it stalls here
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Free all the readers; no need to lock the reader mutex since
        // we've shut the task down
        while (pMsgReceive->pReaderList != NULL) {
            pNext = pMsgReceive->pReaderList->pNext;
            free(pMsgReceive->pReaderList);
            pMsgReceive->pReaderList = pNext;
        }

        // Free all OS resources
        uPortTaskDelete(pMsgReceive->taskHandle);
        uPortMutexDelete(pMsgReceive->taskRunningMutexHandle);
        uPortQueueDelete(pMsgReceive->taskExitQueueHandle);
        uPortMutexDelete(pMsgReceive->readerMutexHandle);

        // Pause here to allow the deletions
        // to actually occur in the idle thread,
        // required by some RTOSs (e.g. FreeRTOS)
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Free the temporary buffer
        free(pMsgReceive->pTemporaryBuffer);

        // Give the ring buffer handle back
        uRingBufferGiveReadHandle(&(pInstance->ringBuffer),
                                  pMsgReceive->ringBufferReadHandle);

        // Add it's done
        free(pInstance->pMsgReceive);
        pInstance->pMsgReceive = NULL;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS: MESSAGE RELATED
 * -------------------------------------------------------------- */

// Convert a public message ID to a private message ID.
int32_t uGnssPrivateMessageIdToPrivate(const uGnssMessageId_t *pMessageId,
                                       uGnssPrivateMessageId_t *pPrivateMessageId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pMessageId != NULL) && (pPrivateMessageId != NULL)) {
        pPrivateMessageId->type = pMessageId->type;
        switch (pMessageId->type) {
            case U_GNSS_PROTOCOL_UBX:
                pPrivateMessageId->id.ubx = pMessageId->id.ubx;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                break;
            case U_GNSS_PROTOCOL_NMEA:
                pPrivateMessageId->id.nmea[0] = 0;
                if (pMessageId->id.pNmea != NULL) {
                    strncpy(pPrivateMessageId->id.nmea, pMessageId->id.pNmea,
                            sizeof(pPrivateMessageId->id.nmea));
                }
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                break;
            default:
                break;
        }
    }

    return errorCode;
}

// Convert a private message ID to a public message ID.
int32_t uGnssPrivateMessageIdToPublic(const uGnssPrivateMessageId_t *pPrivateMessageId,
                                      uGnssMessageId_t *pMessageId, char *pNmea)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pPrivateMessageId != NULL) && (pMessageId != NULL) &&
        ((pPrivateMessageId->type != U_GNSS_PROTOCOL_NMEA) || (pNmea != NULL))) {
        pMessageId->type = pPrivateMessageId->type;
        switch (pPrivateMessageId->type) {
            case U_GNSS_PROTOCOL_UBX:
                pMessageId->id.ubx = pPrivateMessageId->id.ubx;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                break;
            case U_GNSS_PROTOCOL_NMEA:
                strncpy(pNmea, pPrivateMessageId->id.nmea,
                        U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1);
                // Ensure a terminator
                *(pNmea + U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS) = 0;
                pMessageId->id.pNmea = pNmea;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                break;
            default:
                break;
        }
    }

    return errorCode;
}

// Return true if the given private message ID is wanted.
bool uGnssPrivateMessageIdIsWanted(uGnssPrivateMessageId_t *pMessageId,
                                   uGnssPrivateMessageId_t *pMessageIdWanted)
{
    bool isWanted = false;

    if (pMessageIdWanted->type == U_GNSS_PROTOCOL_ALL) {
        isWanted = true;
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_NMEA) &&
               (pMessageId->type == U_GNSS_PROTOCOL_NMEA)) {
        isWanted = (strstr(pMessageId->id.nmea,
                           pMessageIdWanted->id.nmea) == pMessageId->id.nmea);
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_UBX) &&
               (pMessageId->type == U_GNSS_PROTOCOL_UBX)) {
        isWanted = ((pMessageIdWanted->id.ubx == ((U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) |
                                                  U_GNSS_UBX_MESSAGE_ID_ALL)) ||
                    (pMessageIdWanted->id.ubx == pMessageId->id.ubx));
    }

    return isWanted;
}

// Find a valid, matching, NMEA-format message in a buffer.
int32_t uGnssPrivateDecodeNmea(const char *pBuffer, size_t size,
                               char *pMessageId, size_t *pDiscard,
                               uGnssPrivateMessageDecodeState_t *pSavedState)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    const char *pInput = pBuffer;
    size_t i = 0;
    size_t x;
    const char *pMessageStart = pBuffer;
    uGnssPrivateMessageNmeaDecodeState_t state = {0};

    if (pDiscard != NULL) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        *pDiscard = 0;
        if ((pSavedState != NULL) && (pSavedState->type == U_GNSS_PROTOCOL_NMEA)) {
            // Fill in the state from the passed-in state
            state = pSavedState->saved.nmea;
            pInput += state.startOffset;
            i = state.startOffset;
        }

        // NMEA messages begin wih $, then comes the ID, which ends with a
        // comma, then the message body (which cannot contain CRLF) and
        // finally CRLF; match takes us through this:
        while ((i < size) && (state.match < U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_MESSAGE)) {
            if (*pInput == '$') {
                // If we get a dollar at any time we must be
                // at the start of a new sentence, so reset
                memset(state.talkerSentenceIdBuffer, 0, sizeof(state.talkerSentenceIdBuffer));
                pMessageStart = pInput;
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_DOLLAR;
                state.checkSum = 0;
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_DOLLAR) {
                // After the dollar we start the checkSum
                state.checkSum ^= (uint8_t) *pInput; // *NOPAD*
                if (*pInput != ',') {
                    x = pInput - pMessageStart - 1;
                    // -1 to always leave a null terminator
                    if (x < sizeof(state.talkerSentenceIdBuffer) - 1) {
                        // Save this character of the talker/sentence
                        state.talkerSentenceIdBuffer[pInput - pMessageStart - 1] = *pInput;
                    } else {
                        // Too much man
                        state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
                    }
                } else {
                    // End of the talker/sentence ID,
                    // see if it is what we're after
                    state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID;
                    if ((pMessageId != NULL) &&
                        (strstr(state.talkerSentenceIdBuffer, pMessageId) != state.talkerSentenceIdBuffer)) {
                        // Nope, wait for a new sentence to start
                        state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
                    }
                }
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID) {
                // Need a '*' to mark the start of the check-sum field
                if (*pInput == '*') {
                    state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_STAR;
                } else {
                    // Just continue the check-sum
                    state.checkSum ^= (uint8_t) *pInput; // *NOPAD*
                }
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_STAR) {
                // Got the first character of the two-digit hex-coded check-sum field
                state.hexCheckSumFromMessage[0] = *pInput;
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_CS_1;
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_CS_1) {
                // Got the second character of the two-digit hex-coded check-sum field
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
                state.hexCheckSumFromMessage[1] = *pInput;
                // See if it matches
                x = 0;
                uHexToBin(state.hexCheckSumFromMessage, sizeof(state.hexCheckSumFromMessage), (char *) &x);
                if (state.checkSum == (uint8_t) x) {
                    state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_VALID_CS;
                }
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_VALID_CS) {
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
                if (*pInput == '\r') {
                    state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_CR;
                }
            } else if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_ID_AND_CR) {
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
                if (*pInput == '\n') {
                    // Yes, got the final LF in a matching talker/sentence, done it!
                    state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_MESSAGE;
                }
            }

            i++;
            pInput++;
            if ((state.match > U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL) &&
                (pInput - pMessageStart > U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES)) {
                // Message has become too long: bail
                state.match = U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL;
            }
        }

        if (state.match > U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_NULL) {
            // We got some parts of the message overhead, so
            // store the offset for next time
            state.startOffset = pInput - pMessageStart;
            // Discard up to the start of the message
            *pDiscard = pMessageStart - pBuffer;
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
            if (state.match == U_GNSS_PRIVATE_MESSAGE_NMEA_MATCH_GOT_MATCHING_MESSAGE) {
                // Got a complete matching message, write the sentence/talker
                // ID back to pMessageId and return the message length
                if (pMessageId != NULL) {
                    memcpy(pMessageId, state.talkerSentenceIdBuffer,
                           sizeof(state.talkerSentenceIdBuffer));
                }
                errorCodeOrLength = pInput - pMessageStart;
                // Reset the state
                memset(&state, 0, sizeof(state));
            }
        } else {
            // Nuffin: populate pDiscard with all we've found
            *pDiscard = size;
            // Set the state back to defaults
            memset(&state, 0, sizeof(state));
        }

        if (pSavedState != NULL) {
            // Save the state
            pSavedState->type = U_GNSS_PROTOCOL_NMEA;
            pSavedState->saved.nmea = state;
        }
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS: STREAMING TRANSPORT ONLY
 * -------------------------------------------------------------- */

// Get the streaming transport type from a given GNSS transport type.
int32_t uGnssPrivateGetStreamType(uGnssTransportType_t transportType)
{
    int32_t errorCodeOrStreamType = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((transportType >= 0) &&
        (transportType < sizeof(gGnssPrivateTransportTypeToStream) /
         sizeof(gGnssPrivateTransportTypeToStream[0]))) {
        errorCodeOrStreamType = (int32_t) gGnssPrivateTransportTypeToStream[transportType];
    }

    return errorCodeOrStreamType;
}

// Get the number of bytes waiting for us when using a streaming transport.
// IMPORTANT: this function should not do anything that has "global"
// effect on the instance data since it is called by
// uGnssPrivateStreamFillRingBuffer() which may be called at any time by
// the message receive task over in u_gnss_msg.c
int32_t uGnssPrivateStreamGetReceiveSize(int32_t streamHandle,
                                         uGnssPrivateStreamType_t streamType,
                                         uint16_t i2cAddress)
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
            errorCodeOrReceiveSize = uPortI2cControllerSend(streamHandle, i2cAddress,
                                                            buffer, 1, true);
            if (errorCodeOrReceiveSize == 0) {
                errorCodeOrReceiveSize = uPortI2cControllerSendReceive(streamHandle, i2cAddress,
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

// Find the given message ID in the ring buffer.
// IMPORTANT: this function should not do anything that has "global"
// effect on the instance data since it is called by
// uGnssPrivateStreamFillRingBuffer() which may be called at any time by
// the message receive task over in u_gnss_msg.c
int32_t uGnssPrivateStreamDecodeRingBuffer(uGnssPrivateInstance_t *pInstance,
                                           int32_t readHandle,
                                           uGnssPrivateMessageId_t *pPrivateMessageId,
                                           size_t *pDiscard,
                                           uGnssPrivateMessageDecodeState_t *pSavedState)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char buffer[U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES * 2];  // * 2 so that we are more likely
    size_t receiveSize;                                      // to fit in a whole NMEA sentence
    size_t discardSize = 0;                                  // slightly on the large-side for the stack
    uGnssProtocol_t protocolFound = pPrivateMessageId->type;
    char nmeaStr[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1] = {0};
    uint16_t ubxId = (U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) | U_GNSS_UBX_MESSAGE_ID_ALL;
    char *pPrintDiscard = NULL;

    if ((pInstance != NULL) && (pPrivateMessageId != NULL) && (pDiscard != NULL)) {
        *pDiscard = 0;
        // Prepare the ID
        switch (pPrivateMessageId->type) {
            case U_GNSS_PROTOCOL_UBX:
                ubxId = pPrivateMessageId->id.ubx;
                break;
            case U_GNSS_PROTOCOL_NMEA:
                strncpy(nmeaStr, pPrivateMessageId->id.nmea,
                        U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1);
                // Ensure a terminator on the NMEA ID string
                nmeaStr[sizeof(nmeaStr) - 1] = 0;
                break;
            case U_GNSS_PROTOCOL_ALL:
            //lint fallthrough
            default:
                break;
        }

        do {
            // Fill our local buffer from the ring buffer but using a peek
            // so as not to move the read pointer on
            receiveSize = uRingBufferPeekHandle(&(pInstance->ringBuffer), readHandle,
                                                buffer, sizeof(buffer), 0);
            // Take a peek at a chunk, putting it into our temporary buffer
            switch (pPrivateMessageId->type) {
                case U_GNSS_PROTOCOL_UBX:
                    // See if there is a ubx message protocol header in there
                    errorCodeOrLength = matchUbxMessageHeader(buffer, receiveSize,
                                                              &ubxId, &discardSize,
                                                              true);
                    break;
                case U_GNSS_PROTOCOL_NMEA:
                    // See if there is an NMEA protocol message in there;
                    // a complete message in this case since the NMEA
                    // protocol has no length indicator in the header,
                    // we have to play "hunt the CRLF"
                    errorCodeOrLength = uGnssPrivateDecodeNmea(buffer, receiveSize,
                                                               nmeaStr, &discardSize,
                                                               pSavedState);
                    break;
                case U_GNSS_PROTOCOL_ALL:
                    // Since an NMEA message is all ASCII and the header
                    // of a ubx one is definitely not ASCII and is shorter
                    // than an NMEA message, we can reliably check for a
                    // ubx protocol header first
                    protocolFound = U_GNSS_PROTOCOL_UBX;
                    errorCodeOrLength = matchUbxMessageHeader(buffer, receiveSize,
                                                              &ubxId, &discardSize,
                                                              true);
                    if ((errorCodeOrLength > 0) && (discardSize > 0)) {
                        // Check if there's an NMEA protocol message hiding
                        // in the part of the buffer we are going to discard
                        size_t discardSizeNmea = 0;
                        int32_t errorCodeOrLengthNmea = uGnssPrivateDecodeNmea(buffer, receiveSize,
                                                                               nmeaStr, &discardSizeNmea,
                                                                               pSavedState);
                        if ((errorCodeOrLengthNmea > 0) && (discardSizeNmea < discardSize)) {
                            protocolFound = U_GNSS_PROTOCOL_NMEA;
                            discardSize = discardSizeNmea;
                            errorCodeOrLength = errorCodeOrLengthNmea;
                        }
                    }

                    if ((errorCodeOrLength < 0) &&
                        (errorCodeOrLength != U_ERROR_COMMON_TIMEOUT) &&
                        (errorCodeOrLength != U_GNSS_ERROR_NACK)) {
                        protocolFound = U_GNSS_PROTOCOL_NMEA;
                        errorCodeOrLength = uGnssPrivateDecodeNmea(buffer, receiveSize,
                                                                   nmeaStr, &discardSize,
                                                                   pSavedState);
                    }
                    break;
                default:
                    break;
            }
            // Discard from the ring buffer, populating *pDiscard
            // with any amount left over to be discarded by the caller
#ifdef U_GNSS_PRIVATE_PRINT_STREAM_RING_BUFFER_DISCARD
            if (discardSize > 0) {
                pPrintDiscard = (char *) malloc(discardSize);
            }
#endif
            *pDiscard += discardSize - uRingBufferReadHandle(&(pInstance->ringBuffer),
                                                             readHandle, pPrintDiscard, discardSize);
            if (pPrintDiscard != NULL) {
                uPortLog("U_GNSS_PRIVATE_DISCARD: ");
                uGnssPrivatePrintBuffer(pPrintDiscard, discardSize);
                uPortLog("\n");
                free(pPrintDiscard);
            }

            // Drop out of the loop if we succeed or if we received
            // a NACK for a ubx-format message we were looking for
            // or we are no longer discarding anything or if we need
            // the caller to discard stuff for us
        } while ((errorCodeOrLength < 0) && (errorCodeOrLength != U_GNSS_ERROR_NACK) &&
                 (discardSize > 0) && (*pDiscard == 0));

        if (errorCodeOrLength >= 0) {
            // Set the returned ID
            pPrivateMessageId->type = protocolFound;
            switch (pPrivateMessageId->type) {
                case U_GNSS_PROTOCOL_UBX:
                    pPrivateMessageId->id.ubx = ubxId;
                    break;
                case U_GNSS_PROTOCOL_NMEA:
                    strncpy(pPrivateMessageId->id.nmea, nmeaStr, sizeof(pPrivateMessageId->id.nmea));
                    break;
                case U_GNSS_PROTOCOL_ALL:
                //lint fallthrough
                default:
                    break;
            }
        }
    }

    return errorCodeOrLength;
}

// Fill the internal ring buffer with data from the GNSS chip.
// IMPORTANT: this function should not do anything that has "global"
// effect on the instance data since it may be called at any time
// by the message receive task over in u_gnss_msg.c.
int32_t uGnssPrivateStreamFillRingBuffer(uGnssPrivateInstance_t *pInstance,
                                         int32_t timeoutMs, int32_t maxTimeMs)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t startTimeMs;
    int32_t streamType;
    int32_t streamHandle = -1;
    int32_t receiveSize;
    int32_t totalReceiveSize = 0;
    int32_t ringBufferAvailableSize;
    char *pTemporaryBuffer;

    if (pInstance != NULL) {
        pTemporaryBuffer = pInstance->pTemporaryBuffer;
        if ((pInstance->pMsgReceive != NULL) &&
            uPortTaskIsThis(pInstance->pMsgReceive->taskHandle)) {
            // If we're being called from the message receive task,
            // which does not lock gUGnssPrivateMutex, we use its
            // temporary buffer in order to avoid clashes with
            // the main application task
            pTemporaryBuffer = pInstance->pMsgReceive->pTemporaryBuffer;
        }
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        streamType = uGnssPrivateGetStreamType(pInstance->transportType);
        switch (streamType) {
            case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                streamHandle = pInstance->transportHandle.uart;
                break;
            case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                streamHandle = pInstance->transportHandle.i2c;
                break;
            default:
                break;
        }
        if (streamHandle >= 0) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
            startTimeMs = uPortGetTickTimeMs();
            // This is constructed as a do()/while() so that
            // it always has one go even with a zero timeout
            do {
                receiveSize = uGnssPrivateStreamGetReceiveSize(streamHandle,
                                                               (uGnssPrivateStreamType_t) streamType,
                                                               pInstance->i2cAddress);
                // Don't try to read in more than uRingBufferForceAdd()
                // can put into the ring buffer
                ringBufferAvailableSize = uRingBufferAvailableSizeMax(&(pInstance->ringBuffer));
                if (receiveSize > ringBufferAvailableSize) {
                    receiveSize = ringBufferAvailableSize;
                }
                if (receiveSize > 0) {
                    // Read into a temporary buffer
                    if (receiveSize > U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES) {
                        receiveSize = U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES;
                    }
                    switch (streamType) {
                        case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                            // For UART we ask for as much data as we can, it will just
                            // bring in more if more has arrived between the "receive
                            // size" call above and now
                            receiveSize = uPortUartRead(streamHandle, pTemporaryBuffer,
                                                        U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES);
                            break;
                        case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                            // For I2C we need to ask for the amount we know is there since
                            // the I2C buffer is effectively on the GNSS chip and I2C drivers
                            // often don't say how much they've read, just giving us back
                            // the number we asked for on a successful read
                            receiveSize = uPortI2cControllerSendReceive(streamHandle,
                                                                        pInstance->i2cAddress,
                                                                        NULL, 0,
                                                                        pTemporaryBuffer,
                                                                        receiveSize);
                            break;
                        default:
                            break;
                    }
                    if (receiveSize >= 0) {
                        totalReceiveSize += receiveSize;
                        errorCodeOrLength = totalReceiveSize;
                        // Now stuff this into the ring buffer; we use a forced
                        // add: it is up to this MCU to keep up, we don't want
                        // to block data from the GNSS chip, after all it has
                        // no UART flow control lines that we can stop it with
                        if (!uRingBufferForceAdd(&(pInstance->ringBuffer),
                                                 pTemporaryBuffer, receiveSize)) {
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        }
                    } else {
                        // Error case
                        errorCodeOrLength = receiveSize;
                    }
                }
                if ((totalReceiveSize == 0) && (ringBufferAvailableSize > 0) && (timeoutMs > 0)) {
                    // Relax while we're waiting for data to start arriving
                    uPortTaskBlock(10);
                }
                // Exit if we get an error (that is not a timeout), or if we were given zero time,
                // or if there is no room in the ring-buffer for more data, or if we've
                // received nothing and hit the timeout, or if we are not still receiving stuff
                // or were given a maximum time and have exceeded it
            } while (((errorCodeOrLength == (int32_t) U_ERROR_COMMON_TIMEOUT) || (errorCodeOrLength >= 0)) &&
                     (timeoutMs > 0) && (ringBufferAvailableSize > 0) &&
                     // The first condition below is the "not yet received anything case", guarded by timeoutMs
                     // the second condition below is when we're receiving stuff, guarded by maxTimeMs
                     (((totalReceiveSize == 0) && (uPortGetTickTimeMs() - startTimeMs < timeoutMs)) ||
                      ((receiveSize > 0) && ((maxTimeMs == 0) || (uPortGetTickTimeMs() - startTimeMs < maxTimeMs)))));
        }
    }

    if (totalReceiveSize > 0) {
        errorCodeOrLength = totalReceiveSize;
    }

    return errorCodeOrLength;
}

// Read data from the internal ring buffer into the given linear buffer.
int32_t uGnssPrivateStreamReadRingBuffer(uGnssPrivateInstance_t *pInstance,
                                         int32_t readHandle,
                                         char *pBuffer, size_t size,
                                         int32_t maxTimeMs)
{
    return streamGetFromRingBuffer(pInstance, readHandle, pBuffer, size,
                                   0, maxTimeMs, true);
}

// Take a peek at the data in the internal ring buffer.
int32_t uGnssPrivateStreamPeekRingBuffer(uGnssPrivateInstance_t *pInstance,
                                         int32_t readHandle,
                                         char *pBuffer, size_t size,
                                         size_t offset,
                                         int32_t maxTimeMs)
{
    return streamGetFromRingBuffer(pInstance, readHandle, pBuffer, size,
                                   offset, maxTimeMs, false);
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

                errorCodeOrSentLength = sendMessageStream(streamHandle,
                                                          (uGnssPrivateStreamType_t) transportTypeStream,
                                                          pInstance->i2cAddress,
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
int32_t uGnssPrivateSendOnlyCheckStreamUbxMessage(uGnssPrivateInstance_t *pInstance,
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

// Receive an arbitrary message over UART or I2C.
int32_t uGnssPrivateReceiveStreamMessage(uGnssPrivateInstance_t *pInstance,
                                         uGnssPrivateMessageId_t *pPrivateMessageId,
                                         int32_t readHandle,
                                         char **ppBuffer, size_t size,
                                         int32_t timeoutMs,
                                         bool (*pKeepGoingCallback)(uDeviceHandle_t gnssHandle))
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t receiveSize;
    size_t discardSize = 0;
    int32_t startTimeMs;
    int32_t x = timeoutMs > U_GNSS_RING_BUFFER_MIN_FILL_TIME_MS * 10 ? timeoutMs / 10 :
                U_GNSS_RING_BUFFER_MIN_FILL_TIME_MS;
    int32_t y;
    uGnssPrivateMessageDecodeState_t state;

    if ((pInstance != NULL) && (pPrivateMessageId != NULL) &&
        (ppBuffer != NULL) && ((*ppBuffer == NULL) || (size > 0))) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
        startTimeMs = uPortGetTickTimeMs();
        U_GNSS_PRIVATE_MESSAGE_DECODE_STATE_DEFAULT(&state);
        // Lock our read pointer while we look for stuff
        uRingBufferLockReadHandle(&(pInstance->ringBuffer), readHandle);
        // This is constructed as a do()/while() so that it always has one go
        // even with a zero timeout
        do {
            // Try to pull some more data in
            uGnssPrivateStreamFillRingBuffer(pInstance, x, x);
            // Get the number of bytes waiting for us in the ring buffer
            receiveSize = uRingBufferDataSizeHandle(&(pInstance->ringBuffer),
                                                    readHandle);
            if (receiveSize < 0) {
                errorCodeOrLength = receiveSize;
            } else if (receiveSize > 0) {
                // Deal with any discard from a previous run around this loop
                discardSize -= uRingBufferReadHandle(&(pInstance->ringBuffer),
                                                     readHandle,
                                                     NULL, discardSize);
                if (discardSize == 0) {
                    // Attempt to decode a message/message header from the ring buffer
                    errorCodeOrLength = uGnssPrivateStreamDecodeRingBuffer(pInstance,
                                                                           readHandle,
                                                                           pPrivateMessageId,
                                                                           &discardSize,
                                                                           &state);
                    if (errorCodeOrLength > 0) {
                        if (*ppBuffer == NULL) {
                            // The caller didn't give us any memory; allocate the right
                            // amount; the caller must free this memory
                            *ppBuffer = malloc(errorCodeOrLength);
                        } else {
                            // If the user gave us a buffer, limit the size
                            if (errorCodeOrLength > (int32_t) size) {
                                discardSize += errorCodeOrLength - size;
                                errorCodeOrLength = size;
                            }
                        }
                        if (*ppBuffer != NULL) {
                            // Now read the message data into the buffer,
                            // which will move our read pointer on
                            y = timeoutMs - (uPortGetTickTimeMs() - startTimeMs);
                            if (y < U_GNSS_RING_BUFFER_MIN_FILL_TIME_MS) {
                                // Make sure we give ourselves time to read the messsage out
                                y = U_GNSS_RING_BUFFER_MIN_FILL_TIME_MS;
                            }
                            errorCodeOrLength = uGnssPrivateStreamReadRingBuffer(pInstance,
                                                                                 readHandle,
                                                                                 *ppBuffer,
                                                                                 errorCodeOrLength, y);
                        } else {
                            discardSize = errorCodeOrLength;
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        }
                    }
                }
            }
            // Continue to loop while we've not received anything (provided
            // there hasn't been a NACK for the ubx-format message we were looking
            // for and we haven't run out of memory) or still need to discard things,
            // but always checking the guard time/callback.
        } while ((((errorCodeOrLength < 0) && (errorCodeOrLength != (int32_t) U_GNSS_ERROR_NACK) &&
                   (errorCodeOrLength != (int32_t) U_ERROR_COMMON_NO_MEMORY)) ||
                  (discardSize > 0)) &&
                 (uPortGetTickTimeMs() - startTimeMs < timeoutMs) &&
                 ((pKeepGoingCallback == NULL) || pKeepGoingCallback(pInstance->gnssHandle)));

        // Read pointer can be unlocked now
        uRingBufferUnlockReadHandle(&(pInstance->ringBuffer), readHandle);
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS: ANY TRANSPORT
 * -------------------------------------------------------------- */

// Send a ubx format message and receive a response of known length.
int32_t uGnssPrivateSendReceiveUbxMessage(uGnssPrivateInstance_t *pInstance,
                                          int32_t messageClass,
                                          int32_t messageId,
                                          const char *pMessageBody,
                                          size_t messageBodyLengthBytes,
                                          char *pResponseBody,
                                          size_t maxResponseBodyLengthBytes)
{
    uGnssPrivateUbxReceiveMessage_t response;

    // Fill the response structure in with the message class
    // and ID we expect to get back and the buffer passed in.
    response.cls = messageClass;
    response.id = messageId;
    response.ppBody = NULL;
    response.bodySize = 0;
    if (pResponseBody != NULL) {
        response.ppBody = &pResponseBody;
        response.bodySize = maxResponseBodyLengthBytes;
    }

    return sendReceiveUbxMessage(pInstance, messageClass, messageId,
                                 pMessageBody, messageBodyLengthBytes,
                                 &response);
}

// Send a ubx format message and receive a response of unknown length.
int32_t uGnssPrivateSendReceiveUbxMessageAlloc(uGnssPrivateInstance_t *pInstance,
                                               int32_t messageClass,
                                               int32_t messageId,
                                               const char *pMessageBody,
                                               size_t messageBodyLengthBytes,
                                               char **ppResponseBody)
{
    uGnssPrivateUbxReceiveMessage_t response;

    // Fill the response structure in with the message class
    // and ID we expect to get back
    response.cls = messageClass;
    response.id = messageId;
    response.ppBody = ppResponseBody;
    response.bodySize = 0;

    return sendReceiveUbxMessage(pInstance, messageClass, messageId,
                                 pMessageBody, messageBodyLengthBytes,
                                 &response);
}

// Send a ubx format message to the GNSS module that only has an
// Ack response and check that it is Acked.
int32_t uGnssPrivateSendUbxMessage(uGnssPrivateInstance_t *pInstance,
                                   int32_t messageClass,
                                   int32_t messageId,
                                   const char *pMessageBody,
                                   size_t messageBodyLengthBytes)
{
    int32_t errorCode;
    uGnssPrivateUbxReceiveMessage_t response;
    char ackBody[2] = {0};
    char *pBody = &(ackBody[0]);

    // Fill the response structure in with the message class
    // and ID we expect to get back and the buffer passed in.
    response.cls = 0x05;
    response.id = -1;
    response.ppBody = &pBody;
    response.bodySize = sizeof(ackBody);

    errorCode = sendReceiveUbxMessage(pInstance, messageClass, messageId,
                                      pMessageBody, messageBodyLengthBytes,
                                      &response);
    if ((errorCode == 2) && (response.cls == 0x05) &&
        (ackBody[0] == (char) messageClass) &&
        (ackBody[1] == (char) messageId)) {
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
