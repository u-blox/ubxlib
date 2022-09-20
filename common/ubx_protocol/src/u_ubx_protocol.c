/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the UBX protocol message encode/decode API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_ubx_protocol.h"

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

// Wot it says it does.
bool uUbxProtocolIsLittleEndian()
{
    int32_t x = 1;

    return (*((char *) (&x)) == 1);
}

// Return a uint16_t from a pointer to a little-endian uint16_t.
uint16_t uUbxProtocolUint16Decode(const char *pByte)
{
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    const uint8_t *pInput = (const uint8_t *) pByte;
    uint16_t retValue;

    retValue  = *pInput;
    // Cast twice to keep Lint happy
    retValue += (uint16_t) (((uint16_t) *(pInput + 1)) << 8); // *NOPAD*

    return  retValue;
}

// Return a uint32_t from a pointer to a little-endian uint32_t.
uint32_t uUbxProtocolUint32Decode(const char *pByte)
{
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    const uint8_t *pInput = (const uint8_t *) pByte;
    uint32_t retValue;

    retValue  = *pInput;
    // Cast twice to keep Lint happy
    retValue += ((uint32_t) *(pInput + 1)) << 8;  // *NOPAD*
    retValue += ((uint32_t) *(pInput + 2)) << 16; // *NOPAD*
    retValue += ((uint32_t) *(pInput + 3)) << 24; // *NOPAD*

    return retValue;
}

// Return a uint64_t from a pointer to a little-endian uint64_t.
uint64_t uUbxProtocolUint64Decode(const char *pByte)
{
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    const uint8_t *pInput = (const uint8_t *) pByte;
    uint64_t retValue;

    retValue  = *pInput;
    // Cast twice to keep Lint happy
    retValue += ((uint64_t) *(pInput + 1)) << 8;  // *NOPAD*
    retValue += ((uint64_t) *(pInput + 2)) << 16; // *NOPAD*
    retValue += ((uint64_t) *(pInput + 3)) << 24; // *NOPAD*
    retValue += ((uint64_t) *(pInput + 4)) << 32; // *NOPAD*
    retValue += ((uint64_t) *(pInput + 5)) << 40; // *NOPAD*
    retValue += ((uint64_t) *(pInput + 6)) << 48; // *NOPAD*
    retValue += ((uint64_t) *(pInput + 7)) << 56; // *NOPAD*

    return retValue;
}

// Return a little-endian uint16_t from the given uint16_t.
uint16_t uUbxProtocolUint16Encode(uint16_t uint16)
{
    uint16_t retValue = uint16;

    if (!uUbxProtocolIsLittleEndian()) {
        retValue  = (uint16 & 0xFF00) >> 8;
        retValue += (uint16 & 0x00FF) << 8;
    }

    return retValue;
}

// Return a little-endian uint32_t from the given uint32_t.
uint32_t uUbxProtocolUint32Encode(uint32_t uint32)
{
    uint32_t retValue = uint32;

    if (!uUbxProtocolIsLittleEndian()) {
        retValue  = (uint32 & 0xFF000000) >> 24;
        retValue += (uint32 & 0x00FF0000) >> 8;
        retValue += (uint32 & 0x0000FF00) << 8;
        retValue += (uint32 & 0x000000FF) << 24;
    }

    return  retValue;
}

// Return a little-endian uint64_t from the given uint64_t.
uint64_t uUbxProtocolUint64Encode(uint64_t uint64)
{
    uint64_t retValue = uint64;

    if (!uUbxProtocolIsLittleEndian()) {
        retValue  = (uint64 & 0xFF00000000000000) >> 56;
        retValue += (uint64 & 0x00FF000000000000) >> 40;
        retValue += (uint64 & 0x0000FF0000000000) >> 24;
        retValue += (uint64 & 0x000000FF00000000) >> 8;
        retValue += (uint64 & 0x00000000FF000000) << 8;
        retValue += (uint64 & 0x0000000000FF0000) << 24;
        retValue += (uint64 & 0x000000000000FF00) << 40;
        retValue += (uint64 & 0x00000000000000FF) << 56;
    }

    return  retValue;
}

// Encode a UBX protocol message.
int32_t uUbxProtocolEncode(int32_t messageClass, int32_t messageId,
                           const char *pMessage, size_t messageBodyLengthBytes,
                           char *pBuffer)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    uint8_t *pWrite = (uint8_t *) pBuffer;
    int32_t ca = 0;
    int32_t cb = 0;

    if (((messageBodyLengthBytes == 0) || (pMessage != NULL)) &&
        (pBuffer != NULL)) {

        // Complete the header
        *pWrite++ = 0xb5;
        *pWrite++ = 0x62;
        *pWrite++ = (uint8_t) messageClass;
        *pWrite++ = (uint8_t) messageId;
        *pWrite++ = (uint8_t) (messageBodyLengthBytes & (uint8_t) 0xff);
        *pWrite++ = (uint8_t) (messageBodyLengthBytes >> 8);

        if (pMessage != NULL) {
            // Copy in the message body
            memcpy(pWrite, pMessage, messageBodyLengthBytes);
            pWrite += messageBodyLengthBytes;
        }

        // Work out the CRC over the variable elements of the
        // header and the body
        pBuffer += 2;
        for (size_t x = 0; x < messageBodyLengthBytes + 4; x++) {
            ca += (uint8_t) *pBuffer; // *NOPAD*
            cb += ca;
            pBuffer++;
        }

        // Write in the CRC
        *pWrite++ = (uint8_t) (ca & (uint8_t) 0xff);
        *pWrite = (uint8_t) (cb & (uint8_t) 0xff);

        errorCodeOrLength = (int32_t) (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES + messageBodyLengthBytes);
    }

    return errorCodeOrLength;
}

// Decode a UBX protocol message.
int32_t uUbxProtocolDecode(const char *pBufferIn, size_t bufferLengthBytes,
                           int32_t *pMessageClass, int32_t *pMessageId,
                           char *pMessage, size_t maxMessageLengthBytes,
                           const char **ppBufferOut)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    const uint8_t *pInput = (const uint8_t *) pBufferIn;
    int32_t overheadByteCount = 0;
    bool updateCrc = false;
    size_t expectedMessageByteCount = 0;
    size_t messageByteCount = 0;
    int32_t ca = 0;
    int32_t cb = 0;

    for (size_t x = 0; (x < bufferLengthBytes) &&
         (overheadByteCount < U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES); x++) {
        switch (overheadByteCount) {
            case 0:
                //lint -e{650} Suppress warning about 0xb5 being out of range for char
                if (*pInput == 0xb5) {
                    // Got first byte of header, increment count
                    overheadByteCount++;
                }
                break;
            case 1:
                if (*pInput == 0x62) {
                    // Got second byte of header, increment count
                    overheadByteCount++;
                } else {
                    // Not a valid message, start again
                    overheadByteCount = 0;
                }
                break;
            case 2:
                // Got message class, store it, start CRC
                // calculation and increment count
                if (pMessageClass != NULL) {
                    *pMessageClass = *pInput;
                }
                ca = 0;
                cb = 0;
                updateCrc = true;
                overheadByteCount++;
                break;
            case 3:
                // Got message ID, store it, update CRC and
                // increment count
                if (pMessageId != NULL) {
                    *pMessageId = *pInput;
                }
                updateCrc = true;
                overheadByteCount++;
                break;
            case 4:
                // Got first byte of length, store it, update
                // CRC and increment count
                expectedMessageByteCount = *pInput;
                updateCrc = true;
                overheadByteCount++;
                break;
            case 5:
                // Got second byte of length, add it to the first,
                // updat CRC, increment count and reset the
                // message byte count ready for the body to come next.
                // Cast twice to keep Lint happy
                expectedMessageByteCount += ((size_t) *pInput) << 8; // *NOPAD*
                messageByteCount = 0;
                updateCrc = true;
                overheadByteCount++;
                break;
            case 6:
                if (messageByteCount < expectedMessageByteCount) {
                    // Store the next byte of the message and
                    // update CRC
                    if ((pMessage != NULL) && (messageByteCount < maxMessageLengthBytes)) {
                        *pMessage++ = (char) *pInput; // *NOPAD*
                    }
                    updateCrc = true;
                    messageByteCount++;
                } else {
                    // First byte of CRC, check it
                    ca &= 0xff;
                    if ((uint8_t) ca == *pInput) {
                        overheadByteCount++;
                    } else {
                        // Not a valid message, start again
                        overheadByteCount = 0;
                    }
                }
                break;
            case 7:
                // Second byte of CRC, check it
                cb &= 0xff;
                if ((uint8_t) cb == *pInput) {
                    overheadByteCount++;
                } else {
                    // Not a valid message, start again
                    overheadByteCount = 0;
                }
                break;
            default:
                overheadByteCount = 0;
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

    if (overheadByteCount > 0) {
        // We got some parts of the message overhead, so
        // could be a message
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (overheadByteCount == U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
            // We got all the overhead bytes, this is a complete message
            sizeOrErrorCode = (int32_t) messageByteCount;
        }
    }

    if (ppBufferOut != NULL) {
        *ppBufferOut =  (const char *) pInput;
    }

    return sizeOrErrorCode;
}

// End of file
