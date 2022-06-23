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

#ifndef _U_CELL_SEC_C2C_H_
#define _U_CELL_SEC_C2C_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions for the u-blox security
 * chip-to-chip feature.  These functions are called from within the
 * u_cell_sec.h API, they are not intended for external use.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum transmit (to the module) size for the user
 * data in a chip to chip security chunk: this is a hard limit of
 * the C2C protocol. Should be a multiple of 16 bytes for maximal
 * efficiency.  It is worth noting that the amount of user data that
 * can be fitted into a chunk is always one less than this because
 * the RFC 5652 padding scheme always adds at least one byte to the
 * input data.
 */
#define U_CELL_SEC_C2C_USER_MAX_TX_LENGTH_BYTES 256

/** The maximum received (from the module) size for the
 * user data in a chip to chip security chunk.  This is dictated by
 * the largest portion of TCP/UDP data we ever ask for from the module
 * when running sockets, i.e. U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES
 * (see u_cell_sock.h), plus the overhead for the "+USORD:" or
 * "+USORF:" that precedes it, the surrounding quote marks and the
 * line-ending.  Should be a multiple of 16 bytes for maximal
 * efficiency. If this is increased it will also be necessary to
 * increase the size of U_CELL_AT_BUFFER_LENGTH_BYTES in cell.h
 * since a whole chunk must be read-in before it can be decoded.
 */
#define U_CELL_SEC_C2C_USER_MAX_RX_LENGTH_BYTES (1024 + 16) // +16 for the AT-string overheads

/** The chunk overhead for chip to chip security: start and
 * stop flags, 2-byte length and 2-byte CRC.
 */
#define U_CELL_SEC_C2C_OVERHEAD_BYTES 6

/** The length of the initial vector for chip to chip security.
 */
#define U_CELL_SEC_C2C_IV_LENGTH_BYTES 16

/** The maximum length of padding that may be added to the
 * plain-text input for the encryption algorithm to work.
 */
#define U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES 16

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold context data for the chip to chip
 * security operations in the MCU to module (transmit/encode)
 * direction.
 */
typedef struct {
    // Leave room for a generated MAC on the end of the input text
    char txIn[U_CELL_SEC_C2C_USER_MAX_TX_LENGTH_BYTES +
                                                      U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES];
    size_t txInLength;
    size_t txInLimit;
    char txOut[U_CELL_SEC_C2C_USER_MAX_TX_LENGTH_BYTES +
                                                       U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES +
                                                       U_CELL_SEC_C2C_IV_LENGTH_BYTES +
                                                       U_CELL_SEC_C2C_OVERHEAD_BYTES];
} uCellSecC2cContextTx_t;

/** Structure to hold context data for the chip to chip
 * security operations in the module to MCU (receive/decode)
 * direction.
 */
typedef struct {
    char *pRxIn;
    size_t rxInLength;
    // Times two to leave room for a generated MAC,
    // used during checking, on the end of the input
    // text
    char rxOut[U_CELL_SEC_C2C_USER_MAX_RX_LENGTH_BYTES +
                                                       U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES +
                                                       (U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES * 2)];
    char *pRxOut;
} uCellSecC2cContextRx_t;

/** Context data.
 */
typedef struct {
    bool isV2;
    char teSecret[U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES];
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmacKey[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];
    uCellSecC2cContextTx_t *pTx;
    uCellSecC2cContextRx_t *pRx;
} uCellSecC2cContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Transmit intercept function, suitable for hooking
 * into the AT stream with uAtClientStreamInterceptTx().
 *
 * @param atHandle  the handle of the AT client calling this
 *                  function (not used).
 * @param ppData    a pointer to the data pointer into the
 *                  buffer that is ready for transmission.
 *                  This function will move the pointer
 *                  forward to indicate how much of the
 *                  buffer it has processed.
 * @param pLength   a pointer to the length of the data.
 *                  When this function is called the value at
 *                  pLength should be the amount of data in
 *                  the buffer.  When this function returns
 *                  the value at pLength will be the amount
 *                  of data that can now be transmitted.
 * @param pContext  a pointer to uCellSecC2cContext_t that
 *                  has been populated appropriately.
 * @return          a pointer to the data that is now
 *                  ready for transmission, or NULL on error.
 *                  The amount of data at this pointer
 *                  is the value at pLength.
 */
const char *pUCellSecC2cInterceptTx(uAtClientHandle_t atHandle,
                                    const char **ppData,
                                    size_t *pLength,
                                    void *pContext);

/** Obtain a random string to use as the initial value
 * at each encryption in pUCellSecC2cIntercepTx().
 * This function is implemented with weak linkage,
 * it should be overridden by the application defining
 * a function of the same name which can provide better
 * randomness.
 *
 * @return a pointer to a buffer of length
 *         U_CELL_SEC_C2C_IV_LENGTH_BYTES containing
 *         random values which must:
 *
 *         a) remain present for the life of the chip to
 *            chip session,
 *         b) be updated with new random values every
 *            time this function is called.
 */
const char *pUCellSecC2cGetIv(void);

/** Receive intercept function, suitable for hooking
 * into the AT stream with uAtClientStreamInterceptRx().
 * This should be called repeatedly until it returns NULL,
 * a which point it has run out of frames to process and
 * ppData represents how far it has got into the buffer
 * it was passed.
 *
 * @param atHandle  the handle of the AT client calling this
 *                  function (not used).
 * @param ppData    a pointer to the data pointer for the
 *                  buffer of received data.
 *                  This function will move the pointer
 *                  forward to indicate how much of the
 *                  buffer it has processed.
 * @param pLength   a pointer to the length of the data.
 *                  When this function is called the value at
 *                  pLength should be the amount of data in
 *                  the buffer.  When this function returns
 *                  the value at pLength will be the amount
 *                  of data that can now be processed by
 *                  the AT client.
 * @param pContext  a pointer to uCellSecC2cContext_t that
 *                  has been populated appropriately.
 * @return          a pointer to the data that is now
 *                  ready to be processed by the AT client,
 *                  or NULL if there is none.
 *                  The amount of data at this pointer
 *                  is the value at pLength.
 */
char *pUCellSecC2cInterceptRx(uAtClientHandle_t atHandle,
                              char **ppData,
                              size_t *pLength,
                              void *pContext);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_SEC_C2C_H_

// End of file
