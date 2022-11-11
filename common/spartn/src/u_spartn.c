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
 * @brief Implementation of the SPARTN message decode API.
 *
 * Here's a digest of the SPARTN frame, as defined at https://www.spartnformat.org/
 * ```
 *  +-----------------------+-----------------------------------------------+---------------+----------------+---------------+
 *  |      FRAME START      |                PAYLOAD DESCRIPTION            |    PAYLOAD    | AUTHENTICATION |  MESSAGE CRC  |
 *  |        32 bits        |                     48/64 bits                |               |                |               |
 *  |                       |         32/48 bits        |      16 bits      |               |                |               |
 *  |                       |                           |    ENCRYPT/AUTH   |               |                |               |
 *  | P8 T7 L10 E1 MCT2 FC4 | MST4 TT1 T16/32 SID7 PID4 | EID4 ESN6 AI3 AL3 | <= 1024 bytes |  8 to 64 bytes |  1 to 4 bytes |
 *  +-----------------------+-----------------------------------------------+---------------+----------------+---------------+
 * ```
 * FRAME START:
 *
 * - P8 (TF001):     8-bit fixed preamble = 0x73.
 * - T7 (TF002):     7-bit message type.
 * - L10 (TF003):    10-bit payload length in bytes (so the payload can be up to 1024 bytes).
 * - E1 (TF004):     1-bit flag; if 1 then the message is encrypted and authenticated, more fields are present.
 * - MCT2 (TF005):   2-bits indicating the message CRC type.
 * - FC4 (TF006):    4-bit frame check-sum calculated over all preceding bytes except P8.
 *
 * PAYLOAD DESCRIPTION:
 *
 * - MST4 (TF007):   4-bit message sub-type.
 * - TT1 (TF008):    1-bit GNSS time tag type (0 = 16-bit, 1 = 32-bit).
 * - T16/32 (TF009): 16 or 32-bit GNSS time tag.
 * - SID7 (TF010):   7-bit solution ID.
 * - PID4 (TF011):   4-bit solution processor ID.
 *
 * The ENCRYPT/AUTH parts of the payload decription are only present if E1 = 1:
 *
 * - EID4 (TF012):   4-bit encryption ID.
 * - ESN6 (TF013):   6-bit encryption sequence number.
 * - AI3 (TF014):    3-bit authentication indicator.
 * - AL3 (TF015):    3-bit authentication length.
 *
 * PAYLOAD (TF016): up to 1024 bytes in length.
 *
 * AUTHENTICATION (TF017): only present if E1 = 1, length, in the range 8 to 64 bytes,
 * given by AL3, computed over all preceding bytes except P8 and on an
 * already-encrypted payload.
 *
 * MESSAGE CRC (TF018): length 1 to 4 bytes, of type given by MC2, calculated
 * over all preceding bytes except P8.
 *
 * Note: the byte ordering, for each field, is MSB first, so for instance TF002 to
 * TF006 looks like this:
 *
 * Bytes transmitted:    |----------- 1 ----------------|------------ 2 ----------------|------------ 3 ----------------|
 * Bits transmitted:      0   1   2   3   4   5   6   7 | 8   9  10  11  12  13  14  15 |16  17  18  19  20  21  22  23 |
 * Belongs to:            <--------T7 (TF002)-----><----------L10 (TF003) ----------------><-E1><-MCT2-><----- FC4 ---->
 * Meaning:                MSB                 LSB   MSB                               LSB        M  L   MSB         LSB
 *
 * Example:  bytes       |           0x13               |           0x11                |            0xd1               |
 *           bits        |0   0   0   1   0   0   1   1 | 0   0   0   1   0   0   0   1 | 1   1   0   1   1   0   0   1 |
 *
 * ...would be interpreted as:
 *
 * T7 (TF002) = 0x09
 * L10 (TF003) = 0x223
 * E1 (TF004) = 0x01
 * MCT2 (TF005) = 0x01
 * FC4 (TF006) = 0x09
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_spartn.h"
#include "u_spartn_crc.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The minimum length of a SPARTN message header: FRAME START +
 * smallest PAYLOAD DESCRIPTION (i.e. 16-bit GNSS time tag and no
 * ENCRYPT/AUTH).
 */
#define U_SPARTN_HEADER_LENGTH_MIN_BYTES (4 + 4)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ---------- ------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Look for a SPARTN message header in a buffer and supply its position,
// plus the message CRC position and type.
static int32_t decodeHeader(const char *pBuffer, size_t bufferLengthBytes,
                            const char **ppMessage,
                            const char **ppMessageCrcStart,
                            uSpartnCrcType_t *pMessageCrcType)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    // Use a uint8_t pointer for maths, more certain of its behaviour than char
    const uint8_t *pInput = (const uint8_t *) pBuffer;
    const uint8_t *pMessage = NULL;
    uint8_t frameBuffer[4];
    size_t lengthHeader;
    size_t lengthBeyondHeader;
    size_t crcType;

    if (pInput != NULL) {
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        while ((sizeOrErrorCode < 0) && (sizeOrErrorCode != (int32_t) U_ERROR_COMMON_TIMEOUT) &&
               (bufferLengthBytes > 0)) {
            if (*pInput == 0x73) {
                // Potentially a FRAME START
                sizeOrErrorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                pMessage = pInput;
                if (bufferLengthBytes >= U_SPARTN_HEADER_LENGTH_MIN_BYTES) {
                    // Have enough data to work on the header; confirm that this
                    // is a FRAME START by doing a frame CRC check on it
                    // Copy everything from FRAME START except TF001 into a buffer
                    memcpy(&frameBuffer, pInput + 1, 3);
                    frameBuffer[3] = 0;

                    // frameBuffer now contains, in order of bit-arrival:
                    //
                    // bytes:    |      0     |     1     |      2      |     3     |
                    // contents: |<---T7---><-----L10------->E1-MCT2-FC4|           |
                    // meaning:  |M       L M |           |L    M L  M L|           |

                    // Remove the frame CRC that is in the lower four
                    // bits of byte 2, giving us 20 bits in the buffer with
                    // zero-fill elsewhere
                    frameBuffer[2] &= 0xf0;
                    // Compute the CRC-4 over 24 bits and check it against the frame CRC (TF006)
                    if (uSpartnCrc4((const char *) frameBuffer, 3) == (*(pInput + 3) & 0x0f)) {
                        lengthHeader = U_SPARTN_HEADER_LENGTH_MIN_BYTES;
                        // So far so good, now parse the PAYLOAD DESCRIPTION to work out
                        // how long it is; check if the TF008 (GNSS time tag type) bit is set
                        if (*(pInput + 4) & 0x08) {
                            // The GNSS time tag is 32 bits instead of 16, so account for that
                            lengthHeader += 2;
                        }
                        // Work out the length beyond the message header
                        // First the length of the payload from the 10-bit TF003 field,
                        // which is splattered across the three bytes of frameBuffer
                        lengthBeyondHeader = ((((size_t) frameBuffer[0]) & 0x01) << 9) +
                                             (((size_t) frameBuffer[1]) << 1) +
                                             ((((size_t) frameBuffer[2]) & 0x80) >> 7);
                        // Add the length of the message CRC by looking at
                        // the 2-bit message CRC type field (TF005).  Since we have
                        // 0: CRC-8, 1: CRC-16, 2: CRC-24, 3: CRC-32 it is easy
                        // to calculate
                        crcType = (frameBuffer[2] & 0x30) >> 4;
                        lengthBeyondHeader += crcType + 1;
                        if (pMessageCrcType != NULL) {
                            *pMessageCrcType = (uSpartnCrcType_t) crcType;
                        }
                        // Work out the additions as a consequence of encryption/authentication
                        // being switched on
                        if (frameBuffer[2] & 0x40) {
                            // TF004 is set, so we need the ENCRYPT/AUTH fields to work
                            // out the message length; see if they are in the buffer
                            if ((int32_t) bufferLengthBytes - (int32_t) lengthHeader >= 2) {
                                // The ENCRYPT/AUTH fields are in the buffer
                                lengthHeader += 2;
                                // To work out how big the AUTHENTICATION field is we
                                // need to check if the authentication indicator field
                                // (TF014) in PAYLOAD DESCRIPTION is greater than 1.
                                // This is in the final byte of the header so we
                                // can use lengthHeader, which is now pointing
                                // at the start of the payload, to index to it
                                if (((*(pInput + lengthHeader - 1) & 0x38) >> 3) > 1) {
                                    // AUTHENTICATION is present, find out how
                                    // big it is from the 3-bit authentication
                                    // length (TF015) at the beginning of the same
                                    // byte
                                    switch (*(pInput + lengthHeader - 1) & 0x07) {
                                        case 0: // 64 bits
                                            lengthBeyondHeader += 64 / 8;
                                            break;
                                        case 1: // 96 bits
                                            lengthBeyondHeader += 96 / 8;
                                            break;
                                        case 2: // 128 bits
                                            lengthBeyondHeader += 128 / 8;
                                            break;
                                        case 3: // 256 bits
                                            lengthBeyondHeader += 256 / 8;
                                            break;
                                        case 4: // 512 bits
                                            lengthBeyondHeader += 512 / 8;
                                            break;
                                        default:
                                            // Error case: not a supported message
                                            sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                                            lengthHeader = 0;
                                            break;
                                    }
                                }
                            } else {
                                // Might be a message but we don't yet have enough
                                // data to work out its length; set the length
                                // of the header to zero to flag this
                                lengthHeader = 0;
                            }
                        }
                        if (lengthHeader > 0) {
                            // We have a header length, so (a) there are no errors and (b)
                            // we have all the data we need to determine the message length,
                            // then we are done; otherwise sizeOrErrorCode is left at
                            // U_ERROR_COMMON_TIMEOUT (or U_ERROR_COMMON_NOT_FOUND if there
                            // was an error)
                            sizeOrErrorCode = (int32_t) (lengthHeader + lengthBeyondHeader);
                            if (ppMessageCrcStart != NULL) {
                                *ppMessageCrcStart = (const char *) pMessage + lengthHeader + lengthBeyondHeader - (crcType + 1);
                            }
                        }
                    } else {
                        // Not a SPARTN message
                        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                    }
                } else {
                    // Might be a SPARTN message but we don't yet have all of
                    // the header and hence can't work out the message
                    // length; leave sizeOrErrorCode at U_ERROR_COMMON_TIMEOUT
                    // so that the caller knows we need more data
                }
            }

            // Move along
            pInput++;
            bufferLengthBytes--;
        }
    }

    if ((sizeOrErrorCode >= 0) && (ppMessage != NULL)) {
        *ppMessage = (const char *) pMessage;
    }

    return sizeOrErrorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Look for a SPARTN message header in a buffer.
int32_t uSpartnDetect(const char *pBuffer, size_t bufferLengthBytes,
                      const char **ppMessage)
{
    return decodeHeader(pBuffer, bufferLengthBytes, ppMessage, NULL, NULL);
}

// Validate a SPARTN message.
int32_t uSpartnValidate(const char *pBuffer, size_t bufferLengthBytes,
                        const char **ppMessage)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    int32_t messageLength;
    const char *pMessage = NULL;
    const uint8_t *pMessageCrcStart = NULL;
    uSpartnCrcType_t messageCrcType = U_SPARTN_CRC_TYPE_NONE;
    size_t crcLength;
    uint32_t crcFromMessage;

    messageLength = decodeHeader(pBuffer, bufferLengthBytes, &pMessage,
                                 (const char **) &pMessageCrcStart, &messageCrcType);
    if (messageLength > 0) {
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (((int32_t) bufferLengthBytes - (pMessage - pBuffer) >= messageLength) &&
            (pMessageCrcStart != NULL)) {
            sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            // Got a header and enough room for the whole body
            // to be contained, let's see if the body is valid
            // The message CRC is over the whole message, except
            // the first byte, up to the start of the CRC and the
            // CRC value is MSB first like all the others
            crcLength = ((const char *) pMessageCrcStart) - pMessage - 1;
            switch (messageCrcType) {
                case U_SPARTN_CRC_TYPE_8:
                    crcFromMessage = *pMessageCrcStart;
                    if (uSpartnCrc8(pMessage + 1, crcLength) == crcFromMessage) {
                        sizeOrErrorCode = messageLength;
                    }
                    break;
                case U_SPARTN_CRC_TYPE_16:
                    crcFromMessage = (((uint32_t) * pMessageCrcStart) << 8) +
                                     (uint32_t) * (pMessageCrcStart + 1);
                    if (uSpartnCrc16(pMessage + 1, crcLength) == crcFromMessage) {
                        sizeOrErrorCode = messageLength;
                    }
                    break;
                case U_SPARTN_CRC_TYPE_24:
                    crcFromMessage = (((uint32_t) * pMessageCrcStart) << 16) +
                                     ((uint32_t) * (pMessageCrcStart + 1) << 8) +
                                     (uint32_t) * (pMessageCrcStart + 2);
                    if (uSpartnCrc24(pMessage + 1, crcLength) == crcFromMessage) {
                        sizeOrErrorCode = messageLength;
                    }
                    break;
                case U_SPARTN_CRC_TYPE_32:
                    crcFromMessage = (((uint32_t) * pMessageCrcStart) << 24) +
                                     ((uint32_t) * (pMessageCrcStart + 1) << 16) +
                                     ((uint32_t) * (pMessageCrcStart + 2) << 8) +
                                     (uint32_t) * (pMessageCrcStart + 3);
                    if (uSpartnCrc32(pMessage + 1, crcLength) == crcFromMessage) {
                        sizeOrErrorCode = messageLength;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if ((sizeOrErrorCode >= 0) && (ppMessage != NULL)) {
        *ppMessage = pMessage;
    }

    return sizeOrErrorCode;
}

// End of file
