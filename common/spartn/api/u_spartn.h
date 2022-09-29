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

#ifndef _U_SPARTN_H_
#define _U_SPARTN_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __spartn __SPARTN
 *  @{
 */

/** @file
 * @brief This header file defines the SPARTN API, providing some
 * utility functions for SPARTN format messages. Note that there
 * is NO NEED to employ these utilities for normal operation of
 * the Point Perfect service: SPARTN messages should be received,
 * either via MQTT or from a u-blox NEO-D9S L-band receiver, and
 * forwarded transparently to a u-blox high-precision GNSS chip,
 * such as the ZED-F9P, which decodes the SPARTN messages itself.
 *
 * Note: SPARTN messages received from a NEO-D9S receiver
 * are encapsulated inside UBX-format PMP messages as a stream, i.e.
 * there isn't one SPARTN message per PMP message, the SPARTN
 * mesages can be spread across PMP messages, starting in one
 * and ending in another.  In order to use these validation
 * functions the SPARTN messages must first be extracted from
 * the PMP messages.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum size of a SPARTN message.
 */
#define U_SPARTN_MESSAGE_LENGTH_MAX_BYTES (4 + 8 + 1024 + 64 + 4)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Detect a SPARTN message.  ONLY THE SPARTN HEADER (TF001 to TF015)
 * is decoded, sufficient to confirm that it is a SPARTN message and
 * calculate the length of that message, hence this can be used to
 * check if a message has "begun arriving" in a buffer; the message
 * may not yet be fully inside the buffer.  No message CRC check is
 * carried out, just a frame CRC check; if you want a message CRC
 * check on a complete message then please use uSpartnValidate()
 * instead.
 *
 * IMPORTANT: the CRC-4 check in the SPARTN header provides only
 * light protection: it is possible for random data to pass CRC
 * checking, hence you should ensure that, if the SPARTN message
 * has been received over L-band, the quality is sufficiently high.
 * Of course you should also call uSpartnValidate() on the message
 * when you have received all of it.
 *
 * @param[in] pBuffer       a pointer to a buffer of possible message
 *                          data.
 * @param bufferLengthBytes the amount of data at pBuffer.
 * @param[out] ppMessage    a pointer to a place to put a pointer to
 *                          the start of the detected SPARTN message;
 *                          may be NULL.
 * @return                  the length of the detected message or
 *                          negative error code.  All of the message,
 *                          TF001 to TF018, header/CRC etc. is included
 *                          in the count, hence this number may be
 *                          bigger than bufferLengthBytes.  If pBuffer
 *                          contains what looks like the start of a
 *                          header but not with enough data to either
 *                          verify it or determine the length of the
 *                          message, #U_ERROR_COMMON_TIMEOUT will be
 *                          returned.
 */
int32_t uSpartnDetect(const char *pBuffer, size_t bufferLengthBytes,
                      const char **ppMessage);

/** Validate a SPARTN message.  Call this function with a buffer and
 * it will return a pointer to the first valid SPARTN format message
 * it finds in the buffer.  A message CRC check will be conducted and,
 * on success, a pointer to the entire message, TF001 to TF018, still
 * encrypted, will be written to ppMessage.
 *
 * A good pattern for use of this function would be:
 *
 * ```
 * const char *pBufferStart = &(dataIn[0]);
 * size_t bufferLength = sizeof(dataIn);
 * const char *pBufferEnd = pBufferStart;
 * const char *pMessage;
 *
 * for (int32_t x = uSpartnValidate(pBufferStart, bufferLength,
 *                                  &pMessage);
 *      x > 0;
 *      x = uSpartnValidate(pBufferStart, bufferLength,
 *                          &pMessage)) {
 *
 *    printf("Message length %d byte(s).\n", x);
 *
 *    // Do something with the message here
 *
 *    bufferLength -= x;
 *    pBufferStart = pMessage + x;
 * }
 * ```
 *
 * @param[in] pBuffer           a pointer to a buffer of message data.
 * @param bufferLengthBytes     the amount of data at pBuffer.
 * @param[out] ppMessage        a pointer to a place to store a pointer
 *                              to the start of the SPARTN message; may
 *                              be NULL.
 * @return                      on success the number of bytes pointed-to
 *                              by ppMessage else negative error code.
 *                              If ppMessage is NULL then the number of
 *                              bytes that it would have pointed-to is
 *                              returned.  If pBuffer contains a partial
 *                              message #U_ERROR_COMMON_TIMEOUT will be
 *                              returned.
 */
int32_t uSpartnValidate(const char *pBuffer, size_t bufferLengthBytes,
                        const char **ppMessage);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SPARTN_H_

// End of file
