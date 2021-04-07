/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the ubx message encode/decode API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_ubx.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Encode a ubx format message.
int32_t uUbxEncode(int32_t messageClass, int32_t messageId,
                   const char *pMessage, size_t messageLengthBytes,
                   char *pBuffer)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char *pWrite = pBuffer;
    int32_t ca = 0;
    int32_t cb = 0;

    if ((((pMessage == NULL) && (messageLengthBytes == 0)) || (messageLengthBytes > 0)) &&
        (pBuffer != NULL) &&
        (messageLengthBytes >= U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES)) {

        // Complete the header
        *pWrite++ = (char) 0xb5;
        *pWrite++ = 0x62;
        *pWrite++ = (char) messageClass;
        *pWrite++ = (char) messageId;
        *pWrite++ = (char) (messageLengthBytes & 0xFF);
        *pWrite++ = (char) (messageLengthBytes >> 8);

        if (pMessage != NULL) {
            // Copy in the message body
            memcpy (pWrite, pMessage, messageLengthBytes);
            pWrite += messageLengthBytes;
        }

        // Work out the CRC over the variable elements of the
        // header and the body
        pBuffer += 2;
        for (size_t x = 0; x < messageLengthBytes + 4; x++) {
            ca += *pBuffer;
            cb += ca;
            pBuffer++;
        }

        // Write in the CRC
        *pWrite++ = (char) (ca & 0xFF);
        *pWrite = (char) (cb & 0xFF);

        errorCodeOrLength = (int32_t) (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES + messageLengthBytes);
    }

    return errorCodeOrLength;
}

// Decode a ubx format message.
int32_t uUbxDecode(const char *pBufferIn, size_t bufferLengthBytes,
                   int32_t *pMessageClass, int32_t *pMessageId,
                   char *pMessage, size_t maxMessageLengthBytes,
                   const char **ppBufferOut)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    int32_t overheadByteCount = 0;
    size_t expectedMessageByteCount = 0;
    size_t messageByteCount = 0;
    int32_t ca = 0;
    int32_t cb = 0;

    for (size_t x = 0; (x < bufferLengthBytes) &&
         (overheadByteCount <= U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES); x++) {
        //lint -e{650} Suppress warning about 0xb5 being out of range for char
        if ((overheadByteCount == 0) && (*pBufferIn == 0xb5)) {
            // Got first byte of header, increment count
            overheadByteCount++;
        } else if ((overheadByteCount == 1) && (*pBufferIn == 0x62)) {
            // Got second byte of header, increment count
            overheadByteCount++;
        } else if (overheadByteCount == 2) {
            // Got message class, store it, start CRC
            // calculation and increment count
            if (pMessageClass != NULL) {
                *pMessageClass = *pBufferIn;
            }
            ca = *pBufferIn;
            cb = ca;
            overheadByteCount++;
        } else if (overheadByteCount == 3) {
            // Got message ID, store it, add to CRC and
            // increment count
            if (pMessageId != NULL) {
                *pMessageId = *pBufferIn;
            }
            ca += *pBufferIn;
            cb += ca;
            overheadByteCount++;
        } else if (overheadByteCount == 4) {
            // Got first byte of length, store it, add to
            // CRC and increment count
            expectedMessageByteCount = *pBufferIn;
            ca += *pBufferIn;
            cb += ca;
            overheadByteCount++;
        } else if (overheadByteCount == 5) {
            // Got second byte of length, add it to the first,
            // add to CRC, increment count and reset the
            // message byte count ready for the body to come next.
            // Cast twice to keep Lint happy
            expectedMessageByteCount += ((unsigned) (int32_t) * pBufferIn) << 8;
            ca += *pBufferIn;
            cb += ca;
            overheadByteCount++;
            messageByteCount = 0;
        } else if ((overheadByteCount == 6) &&
                   (messageByteCount < expectedMessageByteCount)) {
            // Store the next byte of the message and
            // update CRC
            if ((pMessage != NULL) && (messageByteCount < maxMessageLengthBytes)) {
                *pMessage++ = *pBufferIn;
            }
            ca += *pBufferIn;
            cb += ca;
            messageByteCount++;
        } else if ((overheadByteCount == 7) &&
                   (messageByteCount == expectedMessageByteCount)) {
            // First byte of CRC, check it
            ca &= 0xFF;
            if (ca == *pBufferIn) {
                overheadByteCount++;
            } else {
                // Not a valid message, start again
                overheadByteCount = 0;
            }
        } else if (overheadByteCount == 8) {
            // Second byte of CRC, check it
            cb &= 0xFF;
            if (cb != *pBufferIn) {
                overheadByteCount++;
            } else {
                // Not a valid message, start again
                overheadByteCount = 0;
            }
        } else {
            // Not a valid message, start again
            overheadByteCount = 0;
        }

        // Next byte
        pBufferIn++;
    }

    if (overheadByteCount > 0) {
        // We got some parts of the message overhead, so
        // could be a message
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (overheadByteCount > U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
            // We got all the overhead bytes, this is a complete message
            sizeOrErrorCode = (int32_t) messageByteCount;
        }
    }

    if (ppBufferOut != NULL) {
        *ppBufferOut = pBufferIn;
    }

    return sizeOrErrorCode;
}

// End of file
