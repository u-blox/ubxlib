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

#ifndef _U_UBX_PROTOCOL_H_
#define _U_UBX_PROTOCOL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __ubx-protocol __UBX Protocol
 *  @{
 */

/** @file
 * @brief This header file defines the UBX protocol API, intended to
 * encode/decode UBX format messages when communicating with a u-blox
 * GNSS module.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The length of the UBX protocol (header consisting of 0xB5,
 * 0x62, class, ID and two bytes of length).
 */
#define U_UBX_PROTOCOL_HEADER_LENGTH_BYTES 6

/** The overhead of the UBX protocol (header consisting of 0xB5,
 * 0x62, class, ID, two bytes of length and, at the end, two bytes
 * of CRC). Must be added to the encoded message length to obtain
 * the required encode buffer size.
 */
#define U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES (U_UBX_PROTOCOL_HEADER_LENGTH_BYTES + 2)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** The UBX message protocol is natively little endian, hence any
 * multi-byte values must be little-endian encoded.  Call this
 * function to confirm that your processor is little endian if you
 * intend to use multi-byte values in a message body; you must convert
 * them to little-endian form if it is not since this message codec
 * has no way of knowing what content you are sending. You can do this
 * with the uUbxProtocolUint16Encode() and uUbxProtocolUint32Encode()
 * functions provided and, likewise, decode received multi-byte values
 * from a message body with the uUbxProtocolUint16Decode() and
 * uUbxProtocolUint32Decode() functions provided. Of course, you can
 * always use this functions in any case, since they automatically
 * respect endianness, but you do not need to do so if your processor
 * is already little-endian.
 *
 * @return  true if the processor is little-endian, else false.
 */
bool uUbxProtocolIsLittleEndian();

/** Decode a uint16_t from a pointer to a little-endian uint16_t,
 * ensuring that the endianness of the decoded value is correct
 * for this processor.
 *
 * @param[in] pByte  a pointer to a uint16_t value to decode; cannot be NULL.
 * @return           the decoded uint16_t value, endianness respected.
 */
uint16_t uUbxProtocolUint16Decode(const char *pByte);

/** Decode a uint32_t from a pointer to a little-endian uint32_t,
 * ensuring that the endianness of the decoded value is correct
 * for this processor.
 *
 * @param[in] pByte  a pointer to a uint32_t value to decode; cannot be NULL.
 * @return           the decoded uint32_t value, endianness respected.
 */
uint32_t uUbxProtocolUint32Decode(const char *pByte);

/** Decode a uint64_t from a pointer to a little-endian uint64_t,
 * ensuring that the endianness of the decoded value is correct
 * for this processor.
 *
 * @param[in] pByte  a pointer to a uint64_t value to decode; cannot be NULL.
 * @return           the decoded uint64_t value, endianness respected.
 */
uint64_t uUbxProtocolUint64Decode(const char *pByte);

/** Encode the given uint16_t value with correct endianness for the UBX
 * protocol.
 *
 * @param uint16  the uint16_t value to encode.
 * @return        the encoded uint16_t value.
 */
uint16_t uUbxProtocolUint16Encode(uint16_t uint16);

/** Encode the given uint32_t value with correct endianness for the UBX
 * protocol.
 *
 * @param uint32  the uint32_t value to encode.
 * @return        the encoded uint32_t value.
 */
uint32_t uUbxProtocolUint32Encode(uint32_t uint32);

/** Encode the given uint64_t value with correct endianness for the UBX
 * protocol.
 *
 * @param uint64  the uint64_t value to encode.
 * @return        the encoded uint64_t value.
 */
uint64_t uUbxProtocolUint64Encode(uint64_t uint64);

/** Encode a UBX protocol message.
 *
 * @param messageClass            the UBX protocol message class.
 * @param messageId               the UBX protocol message ID.
 * @param[in] pMessageBody        the message body to be encoded,
 *                                may be NULL if the message has no
 *                                body.
 * @param messageBodyLengthBytes  the length of the message body,
 *                                must non-zero if pMessage is not
 *                                NULL.
 * @param[out] pBuffer            a buffer in which the encoded
 *                                message is to be stored; at least
 *                                messageLengthBytes +
 *                                #U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES
 *                                must be allowed.
 * @return                        on success the number of bytes written
 *                                to pBuffer, else negative error code.
 */
int32_t uUbxProtocolEncode(int32_t messageClass, int32_t messageId,
                           const char *pMessageBody, size_t messageBodyLengthBytes,
                           char *pBuffer);

/** Decode a UBX protocol message.  Call this function with a buffer
 * and it will return the first valid UBX format message it finds
 * in the buffer. ppBufferOut will be set to the first position in
 * the buffer after any message is found, or will point one byte
 * beyond the end of the buffer if no message or a partial message
 * is found.  Hence a good pattern for use of this function could be:
 *
 * ```
 * const char *pBufferStart = &(dataIn[0]);
 * size_t bufferLength = sizeof(dataIn);
 * const char *pBufferEnd = pBufferStart;
 * int32_t messageClass;
 * int32_t messageId;
 * char messageBody[128];
 *
 * for (int32_t x = uUbxProtocolDecode(pBufferStart, bufferLength,
 *                                     &messageClass, &messageId,
 *                                     messageBody, sizeof(messageBody),
 *                                     &pBufferEnd);
 *      x > 0;
 *      x = uUbxProtocolDecode(pBufferStart, bufferLength,
 *                             &messageClass, &messageId,
 *                             messageBody, sizeof(messageBody),
 *                             &pBufferEnd)) {
 *    printf("Message class 0x%02x, message ID 0x%02x, "
 *           " message body length %d byte(s).\n", messageClass,
 *           messageId, x);
 *    if (x > sizeof(messageBody)) {
 *        printf("Warning: message body is larger than storage"
 *               " buffer (only %d bytes).\n", sizeof(messageBody));
 *        x = sizeof(messageBody);
 *    }
 *
 *    // Handle the message here
 *
 *    bufferLength -= pBufferEnd - pBufferStart;
 *    pBufferStart = pBufferEnd;
 * }
 * ```
 *
 * @param[in] pBufferIn              a pointer to the message buffer to
 *                                   decode.
 * @param bufferLengthBytes          the amount of data at pBufferIn.
 * @param[out] pMessageClass         a pointer to somewhere to store the
 *                                   decoded UBX message class; may be NULL.
 * @param[out] pMessageId            a pointer to somewher to store the
 *                                   decoded UBX message ID; may be NULL.
 * @param[out] pMessageBody          a pointer to somewhere to store
 *                                   the decoded message body; it is safe
 *                                   to decode back into pBufferIn if you
 *                                   don't mind over-writing the message.
 *                                   May be NULL.
 * @param maxMessageBodyLengthBytes  the amount of storage at pMessageBody.
 * @param[out] ppBufferOut           a pointer to somewhere to store the
 *                                   buffer pointer after message decoding
 *                                   has been completed; may be NULL;
 * @return                           on success the number of message body
 *                                   bytes decoded, else negative error
 *                                   code.  If pMessageBody is NULL then
 *                                   the number of bytes that would have
 *                                   been decoded is returned, hence
 *                                   allowing the potential size of a
 *                                   decoded message to be determined in
 *                                   order to make a subsequent call with
 *                                   exactly the right buffer size or
 *                                   destination.  Note that the
 *                                   return value may be larger than
 *                                   maxMessageBodyLengthBytes, though only
 *                                   a maximum of maxMessageBodyLengthBytes
 *                                   will be written to pMessageBody.  If
 *                                   pBufferIn contains a partial message
 *                                   #U_ERROR_COMMON_TIMEOUT will be returned.
 */
int32_t uUbxProtocolDecode(const char *pBufferIn, size_t bufferLengthBytes,
                           int32_t *pMessageClass, int32_t *pMessageId,
                           char *pMessageBody, size_t maxMessageBodyLengthBytes,
                           const char **ppBufferOut);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_UBX_PROTOCOL_H_

// End of file
