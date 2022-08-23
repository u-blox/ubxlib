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
 * @brief Implementation of the u-blox security chip-to-chip
 * feature for cellular.  This functions are called by the
 * u_cell_sec.h API functions, they are not intended for use
 * externally.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), memcmp()
#include "stdlib.h"    // rand()

#include "u_cfg_sw.h"
#include "u_compiler.h" // for U_WEAK

#include "u_port_clib_platform_specific.h" // rand()
#include "u_port_debug.h"
#include "u_port_crypto.h"

#include "u_at_client.h"

#include "u_security.h"

#include "u_cell_sec_c2c.h"

// Note: the compilation flag U_CELL_SEC_C2C_DETAILED_DEBUG
// was used during early stage development against real
// modems. It is FAR too heavyweight to be used normally,
// and of course shouldn't be necessary, but it is retained
// here in anticipation of that corner case appearing...
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
#if U_CFG_ENABLE_LOGGING
#include "ctype.h"
#include "u_port_os.h"
#endif
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The chip to chip frame boundary marker.
 */
#define U_CELL_SEC_C2C_FRAME_MARKER 0xf9

// Check that an array of size U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES
// is big enough to hold an IV also
#if U_CELL_SEC_C2C_IV_LENGTH_BYTES > U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES
# error U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES must be at least as big as U_CELL_SEC_C2C_IV_LENGTH_BYTES since we size a local array below on U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES and it is used for both.
#endif

// Check that U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES is at least as
// big as U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES
#if U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES > U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES
# error U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES must be at least as big as U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES since a TE secret is temporarily written to the space a truncated MAC would occupy during V2 encoding.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Storage for the random IV value.
 */
static char gIv[U_CELL_SEC_C2C_IV_LENGTH_BYTES];

/** Table for FCS generation according to RFC 1662.
 */
static const uint16_t gFcsTable[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Calculate FCS according to RFC 1662.
static uint16_t fcsGenerate(const char *pBuffer, size_t bufferLength)
{
    uint16_t fcs = 0xffff;

    while (bufferLength > 0) {
        fcs = (fcs >> 8) ^ gFcsTable[(fcs ^ *pBuffer) & 0xff];
        pBuffer++;
        bufferLength--;
    }

    // The FCS is then complemented before it is used
    fcs ^= 0xFFFF;

    return fcs;
}

// Return the length of a buffer after packing by the
// given modulo according to RFC 5652 section 6.3.
static size_t paddedLength(size_t length, size_t padModulo)
{
    return length + ((char) (padModulo - (length % padModulo)));
}

// Pad a buffer to the given modulo according to RFC 5652
// section 6.3.
static size_t pad(char *pBuffer, size_t length,
                  size_t bufferLength, size_t padModulo)
{
    char fill = (char) (padModulo - (length % padModulo));

    do {
        *(pBuffer + length) = fill;
        length++;
    } while ((length % padModulo != 0) && (length < bufferLength));

    return length;
}

// Unpad a buffer that was padded according to RFC 5652
// section 6.3.
static size_t unpad(const char *pBuffer, size_t bufferLength)
{
    size_t fill;

    if (bufferLength > 0) {
        fill = *(pBuffer + bufferLength - 1);
        if (bufferLength >= fill) {
            bufferLength -= fill;
        }
    }

    return bufferLength;
}

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
#if U_CFG_ENABLE_LOGGING
// Print out text.
static void print(const char *pStr, size_t length)
{
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        if (!isprint((int32_t) c)) {
            // Print the hex
            uPortLog("[%02x]", c);
        } else {
            // Print the ASCII character
            uPortLog("%c", c);
        }
    }
}

// Print out binary.
static void printHex(const char *pStr, size_t length)
{
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        uPortLog("[%02x]", c);
    }
    (void) pStr;
    (void) length;
}
#endif

// On some platforms printing is line
// buffered so long strings will get lost unless
// they are chunked up: this function
// prints reasonable block sizes
//lint -esym(522, printBlock) Suppress "lacks side effects", which
// will be true if logging is compiled out
void printBlock(const char *pStr, size_t length,
                bool isBinary)
{
#if U_CFG_ENABLE_LOGGING
    int32_t x = (int32_t) length;
    int32_t y;

    while (x > 0) {
        uPortLog("\"");
        y = x;
        if (y > 32) {
            y = 32;
        }
        if (isBinary) {
            printHex(pStr, y);
        } else {
            print(pStr, y);
        }
        uPortLog("\"\n");
        // Don't overwhelm the poor debug output,
        // there there
        uPortTaskBlock(100);
        x -= y;
        pStr += y;
    }
#else
    (void) pStr;
    (void) length;
    (void) isBinary;
#endif
}
#endif

// Run chip to chip encode.
static size_t encode(const uCellSecC2cContext_t *pContext)
{
    size_t length = 0;
    uCellSecC2cContextTx_t *pTx = pContext->pTx;
    size_t x;
    uint16_t y;
    char ivOrMac[U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES];
    bool success = false;

    // Get the IV into a local variable
    memcpy(ivOrMac, pUCellSecC2cGetIv(), U_CELL_SEC_C2C_IV_LENGTH_BYTES);

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
    uPortLog("U_CELL_SEC_C2C_ENCODE: IV:\n");
    printBlock(ivOrMac, U_CELL_SEC_C2C_IV_LENGTH_BYTES, true);

    uPortLog("U_CELL_SEC_C2C_ENCODE: key:\n");
    printBlock(pContext->key, sizeof(pContext->key), true);

    uPortLog("U_CELL_SEC_C2C_ENCODE: HMAC key:\n");
    printBlock(pContext->hmacKey, sizeof(pContext->hmacKey), true);

    uPortLog("U_CELL_SEC_C2C_ENCODE: TE secret:\n");
    printBlock(pContext->teSecret, sizeof(pContext->teSecret), true);

    uPortLog("U_CELL_SEC_C2C_ENCODE: input text is (%d byte(s)):\n",
             pTx->txInLength);
    printBlock(pTx->txIn, pTx->txInLength, false);
#endif

    // Pad the input data as required
    pTx->txInLength = pad(pTx->txIn, pTx->txInLength, pTx->txInLimit,
                          U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES);

    // The frame looks like this:
    //  ---------------------------------------------
    // | F9 |    |    |   |   ...   |   |   |   | F9 |
    //      |  length |      body       |  CRC  |
    //  ---------------------------------------------
    // F9 is the frame marker, the two-byte length and
    // CRC fields are little-endian.  Length is of the
    // body only.

    // Add the opening frame marker
    pTx->txOut[0] = (char) U_CELL_SEC_C2C_FRAME_MARKER;

    // Encrypt the data
    if (pContext->isV2) {
        // In V2 the body is as follows:
        //
        //  -----------------------------------------------
        // |    IV    | encrypted padded  |  truncated MAC |
        // | 16 bytes |     user data     |     16 bytes   |
        //  -----------------------------------------------
        //
        // Note that the V2 body is also encoded in a similar
        // function over in u_cell_sec.c, used when
        // creating the C2C confirmation tag: I would have
        // separated this part out and had a single version but
        // the C2C confirmation tag was a late-breaking
        // change and I didn't want to pull this code to bits.

        // Length is the padded input length plus the IV length
        // plus a truncated MAC length.
        // Little endian, like the CRC
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        uPortLog("U_CELL_SEC_C2C_ENCODE: version 2.\n");
        uPortLog("U_CELL_SEC_C2C_ENCODE: padded input length is %d byte(s).\n", pTx->txInLength);
#endif
        x = pTx->txInLength + U_CELL_SEC_C2C_IV_LENGTH_BYTES +
            U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES;
        pTx->txOut[1] = (char) x;
        pTx->txOut[2] = (char) (x >> 8);
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        uPortLog("U_CELL_SEC_C2C_ENCODE: chunk length will be %d byte(s).\n", x);
#endif

        // Write IV into the output.
        // Then the encryption function can be pointed at the
        // local copy so that we can cheerfully overwrite it
        memcpy(pTx->txOut + 3, ivOrMac,
               U_CELL_SEC_C2C_IV_LENGTH_BYTES);
        x = U_CELL_SEC_C2C_IV_LENGTH_BYTES;
        // Encrypt the padded plain text into the
        // output buffer using the encryption key and the IV
        if (uPortCryptoAes128CbcEncrypt(pContext->key,
                                        sizeof(pContext->key),
                                        ivOrMac, pTx->txIn,
                                        pTx->txInLength,
                                        pTx->txOut + 3 + x) == 0) {
            x += pTx->txInLength;
            // Next we need to create a HMAC tag across the
            // encrypted text, the IV and the TE Secret.
            // The simplest way to do this is to copy
            // the TE Secret into the output buffer, perform
            // the calculation (putting the result into the
            // local variable ivOrMac) and then we overwrite
            // where it is in the buffer with the truncated MAC
            // (which is at least as big, as checked with
            // a #error above)
            memcpy(pTx->txOut + 3 + x, pContext->teSecret,
                   sizeof(pContext->teSecret));
            if (uPortCryptoHmacSha256(pContext->hmacKey,
                                      sizeof(pContext->hmacKey),
                                      pTx->txOut + 3,
                                      x + sizeof(pContext->teSecret),
                                      ivOrMac) == 0) {
                // Now copy the first 16 bytes of the
                // generated HMAC tag into the output,
                // overwriting the TE Secret
                memcpy(pTx->txOut + 3 + x,
                       ivOrMac, U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES);
                // Account for its length
                x += U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES;
                success = true;
            }
        }
    } else {
        // In V1 the body is as follows:
        //
        //  ---------------------------------------------
        // |  encrypted padded  |    MAC    |    IV      |
        // |      user data     | 32 bytes  |  16 bytes  |
        //  ---------------------------------------------
        //
        // Length is the padded input length plus the MAC length
        // plus the IV length.
        // Little endian, like the CRC
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        uPortLog("U_CELL_SEC_C2C_ENCODE: version 1.\n");
        uPortLog("U_CELL_SEC_C2C_ENCODE: input length will be %d byte(s).\n", pTx->txInLength);
#endif
        x = pTx->txInLength +
            U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES +
            U_CELL_SEC_C2C_IV_LENGTH_BYTES;
        pTx->txOut[1] = (char) x;
        pTx->txOut[2] = (char) (x >> 8);
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        uPortLog("U_CELL_SEC_C2C_ENCODE: chunk length will be %d byte(s).\n", x);
#endif
        // Create the MAC and put it on the end of
        // the padded plain text in the input buffer
        x = pTx->txInLength;
        if (uPortCryptoSha256(pTx->txIn, x, pTx->txIn + x) == 0) {
            x += U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES;
            // Write IV into its position in the output
            // then the encryption function is pointed at the
            // local copy so that if can cheerfully overwrite it
            memcpy(pTx->txOut + 3 + x, ivOrMac,
                   U_CELL_SEC_C2C_IV_LENGTH_BYTES);
            // Encrypt the padded plain text plus MAC into the
            // output buffer using the encryption key and the IV
            if (uPortCryptoAes128CbcEncrypt(pContext->key,
                                            sizeof(pContext->key),
                                            ivOrMac, pTx->txIn, x,
                                            pTx->txOut + 3) == 0) {
                // Now account for the length of the initial vector
                x += U_CELL_SEC_C2C_IV_LENGTH_BYTES;
                success = true;
            }
        }
    }

    if (success) {
        // Calculate the checksum over the length
        // and everything else up to here
        x += 2;
        y = fcsGenerate(pTx->txOut + 1, x);
        // Account for the opening marker
        x++;
        // Write in the checksum, little-endianly it says
        // in RFC 1662
        pTx->txOut[x] = (char) y;
        pTx->txOut[x + 1] = (char) (y >> 8);

        // Account for the checksum
        x += 2;
        // Finally add the closing marker
        pTx->txOut[x] = (char) U_CELL_SEC_C2C_FRAME_MARKER;
        x++;
        length = x;

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        uPortLog("U_CELL_SEC_C2C_ENCODE: output is (%d byte(s)):\n", length);
        printBlock(pTx->txOut, length, true);
#endif
    }

    return length;
}

// Run chip to chip decode.
static size_t decode(const uCellSecC2cContext_t *pContext)
{
    size_t length = 0;
    uCellSecC2cContextRx_t *pRx = pContext->pRx;
    size_t x = 0;
    size_t chunkLength;
    size_t chunkLengthLimit;
    uint16_t y;
    char *pData = pRx->pRxIn;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
    size_t z = 0;
#endif

    // Look for an opening frame marker
    // The frame looks like this:
    //  ---------------------------------------------
    // | F9 |    |    |   |   ...   |   |   |   | F9 |
    //      |  length |      body       |  CRC  |
    //  ---------------------------------------------
    // F9 is the frame marker, the two-byte length and
    // CRC fields are little-endian.  Length is of the
    // body only.

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
    uPortLog("U_CELL_SEC_C2C_DECODE: buffer is %d byte(s) long.\n",
             pRx->rxInLength);
#endif

    // We need to avoid acting on corrupt lengths (due to
    // frame boundaries being mis-detected on loss of data)
    // so work out what the maximum length is.
    if (pContext->isV2) {
        chunkLengthLimit = U_CELL_SEC_C2C_USER_MAX_RX_LENGTH_BYTES +
                           U_CELL_SEC_C2C_IV_LENGTH_BYTES +
                           U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES +
                           U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES;
    } else {
        chunkLengthLimit = U_CELL_SEC_C2C_USER_MAX_RX_LENGTH_BYTES +
                           U_CELL_SEC_C2C_IV_LENGTH_BYTES +
                           U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES +
                           U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES;
    }

    while ((x < pRx->rxInLength) &&
           (*pData != (char) U_CELL_SEC_C2C_FRAME_MARKER)) {
        pData++;
        x++;
    }

    if ((*pData == (char) U_CELL_SEC_C2C_FRAME_MARKER) &&
        ((pRx->rxInLength - x) > U_CELL_SEC_C2C_OVERHEAD_BYTES)) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        if (x > 0) {
            uPortLog("U_CELL_SEC_C2C_DECODE: frame marker found after %d"
                     " byte(s) were discarded:\n", x);
            printBlock(pRx->pRxIn, x, true);
        }
        uPortLog("U_CELL_SEC_C2C_DECODE: found a frame marker and"
                 " enough bytes following (%d) to potentially hold"
                 " a frame.\n", pRx->rxInLength - x);
#endif
        // Have a frame marker and at least a non-zero length frame
        // Grab the length, little endian
        pData++;
        // Cast in two stages to keep Lint happy
        chunkLength = ((size_t) (int32_t) * pData);
        pData++;
        chunkLength += ((size_t) (int32_t) * pData) << 8;
        pData++;

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
        z = chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES;
        if (z > pRx->rxInLength - x) {
            z = pRx->rxInLength - x;
        }
        uPortLog("U_CELL_SEC_C2C_DECODE: chunk is %d byte(s) (including"
                 " %d bytes of overhead) of which we have %d byte(s):\n",
                 chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES,
                 U_CELL_SEC_C2C_OVERHEAD_BYTES, z);
        printBlock(pData - 3, z, true);
        z = 0;
        if ((pRx->rxInLength - x) > chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES) {
            z = (pRx->rxInLength - x) - (chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES);
            uPortLog("U_CELL_SEC_C2C_DECODE: first 16 bytes of %d byte(s)"
                     " after chunk ends:\n", z);
            if (z > 16) {
                z = 16;
            }
            printBlock((pData - 3) + chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES, z, true);
        }
#endif
        // pData now points to the start of the
        // encrypted data, x is the number of bytes discarded
        // before we reach the frame marker
        if ((chunkLength >= U_CELL_SEC_C2C_IV_LENGTH_BYTES +
             U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES) &&
            (pRx->rxInLength - x >= chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES) &&
            (chunkLength <= chunkLengthLimit)) {
            // Length is sane, now calculate the
            // CRC, which is over the chunk length
            // plus the length value itself
            y = fcsGenerate(pData - 2, chunkLength + 2);
            // CRC is little-endian according to RFC 1662
            if ((*(pData + chunkLength) == (char) y) &&
                (*(pData + chunkLength + 1) == (char) (y >> 8))) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                uPortLog("U_CELL_SEC_C2C_DECODE: FCS is good.\n");
#endif
                if (pContext->isV2) {
                    // In V2 the body is as follows:
                    //
                    //  -----------------------------------------------
                    // |    IV    |  encrypted padded  | truncated MAC |
                    // | 16 bytes |     user data      |    16 bytes   |
                    //  -----------------------------------------------
                    //
                    // The CRC matches.  Now we want
                    // to compute the HMAC tag across the
                    // encrypted text (i.e. minus the
                    // HMAC tag that forms part of
                    // the payload) plus the TE Secret.
                    // To do this concatenate the two
                    // into rxOut (there is enough room
                    // to do so).
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                    uPortLog("U_CELL_SEC_C2C_DECODE: version 2.\n");

                    uPortLog("U_CELL_SEC_C2C_DECODE: key:\n");
                    printBlock(pContext->key, sizeof(pContext->key), true);

                    uPortLog("U_CELL_SEC_C2C_DECODE: HMAC key:\n");
                    printBlock(pContext->hmacKey, sizeof(pContext->hmacKey), true);

                    uPortLog("U_CELL_SEC_C2C_DECODE: TE secret:\n");
                    printBlock(pContext->teSecret, sizeof(pContext->teSecret), true);
#endif
                    x = chunkLength -
                        U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES;
                    memcpy(pRx->rxOut, pData, x);
                    memcpy(pRx->rxOut + x, pContext->teSecret,
                           sizeof(pContext->teSecret));
                    // Compute the HMAC SHA256 of this block
                    // using the HMAC tag as the key and put it
                    // on the end of rxOut as temporary storage.
                    if (uPortCryptoHmacSha256(pContext->hmacKey,
                                              sizeof(pContext->hmacKey),
                                              pRx->rxOut,
                                              x + sizeof(pContext->teSecret),
                                              pRx->rxOut +
                                              x + sizeof(pContext->teSecret)) == 0) {
                        // Compare the first 16 bytes of
                        // it with the truncated MAC we received.
                        if (memcmp(pData + x, pRx->rxOut + x +
                                   sizeof(pContext->teSecret),
                                   U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES) == 0) {
                            // The MAC's match, decrypt the contents
                            // into rxOut using the key and the IV from the
                            // incoming message.  This will cause
                            // the IV in the incoming message to
                            // be overwritten with a new value
                            // but we don't care about that.
                            x = chunkLength - (U_CELL_SEC_C2C_IV_LENGTH_BYTES +
                                               U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES);
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                            uPortLog("U_CELL_SEC_C2C_DECODE: MACs match.\n");
                            uPortLog("U_CELL_SEC_C2C_DECODE: IV:\n");
                            printBlock(pData, U_CELL_SEC_C2C_IV_LENGTH_BYTES, true);
#endif
                            if (uPortCryptoAes128CbcDecrypt(pContext->key,
                                                            sizeof(pContext->key),
                                                            pData, /* IV */
                                                            pData + U_CELL_SEC_C2C_IV_LENGTH_BYTES,
                                                            x,
                                                            pRx->rxOut) == 0) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                                uPortLog("U_CELL_SEC_C2C_DECODE: padded decrypted data:\n");
                                printBlock(pRx->rxOut, x, false);
#endif
                                // Unpad the now plain text
                                length = unpad(pRx->rxOut, x);
                                // Copy it back into the receive buffer
                                // and set the output pointer
                                memcpy(pRx->pRxIn, pRx->rxOut, length);
                                pRx->pRxOut = pRx->pRxIn;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                                uPortLog("U_CELL_SEC_C2C_DECODE: decrypted data:\n");
                                printBlock(pRx->rxOut, length, false);
#endif
                            }
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                        } else {
                            uPortLog("U_CELL_SEC_C2C_DECODE: truncated MAC mismatch.\n");
#endif
                        }
                    }
                } else {
                    // In V1 the body is as follows:
                    //
                    //  ---------------------------------------------
                    // |  encrypted padded  |    MAC    |    IV      |
                    // |      user data     | 32 bytes  |  16 bytes  |
                    //  ---------------------------------------------
                    //
                    // The CRC matches, decrypt the contents
                    // using the key and the IV from the
                    // incoming message.  This will cause
                    // the IV in the incoming message to
                    // be overwritten with a new value
                    // but we don't care about that.
                    x = chunkLength - U_CELL_SEC_C2C_IV_LENGTH_BYTES;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                    uPortLog("U_CELL_SEC_C2C_DECODE: version 1.\n");

                    uPortLog("U_CELL_SEC_C2C_DECODE: key:\n");
                    printBlock(pContext->key, sizeof(pContext->key), true);

                    uPortLog("U_CELL_SEC_C2C_DECODE: IV:\n");
                    printBlock(pData + x,
                               U_CELL_SEC_C2C_IV_LENGTH_BYTES, true);
#endif
                    if (uPortCryptoAes128CbcDecrypt(pContext->key,
                                                    sizeof(pContext->key),
                                                    pData + x, /* IV */
                                                    pData, x,
                                                    pRx->rxOut) == 0) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                        if (x >= U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) {
                            uPortLog("U_CELL_SEC_C2C_DECODE: padded decrypted data:\n");
                            printBlock(pRx->rxOut,
                                       x - U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES, false);
                            uPortLog("U_CELL_SEC_C2C_DECODE: decrypted MAC:\n");
                            printBlock(pRx->rxOut + (x - U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES),
                                       U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES, true);
                        } else {
                            uPortLog("U_CELL_SEC_C2C_DECODE: chunk is too short"
                                     " (%d byte(s)):\n", chunkLength);
                        }
#endif
                        // The decrypted data consists of the padded
                        // plain-text data plus the MAC on the end.
                        // Compute the SHA256 of the plain-text data
                        // (room is left to put it on the end of rxOut,
                        // after the end of the MAC since we no longer
                        // need the [over-written] IV) and then compare
                        // it with the MAC we received.
                        if (uPortCryptoSha256(pRx->rxOut,
                                              x - U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES,
                                              pRx->rxOut + x) == 0) {
                            if (memcmp(pRx->rxOut + x - U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES,
                                       pRx->rxOut + x,
                                       U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) == 0) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                                uPortLog("U_CELL_SEC_C2C_DECODE: MACs match.\n");
#endif
                                // The MAC's match, get the unpadded length
                                // of the plain-text data
                                length = unpad(pRx->rxOut,
                                               x - U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES);
                                // Copy it back into the receive buffer
                                // and set the output pointer
                                memcpy(pRx->pRxIn, pRx->rxOut, length);
                                pRx->pRxOut = pRx->pRxIn;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                                uPortLog("U_CELL_SEC_C2C_DECODE: %d byte(s) decrypted"
                                         " data:\n", length);
                                printBlock(pRx->rxOut, length, false);
#endif
                            } else {
                                uPortLog("U_CELL_SEC_C2C_DECODE: MAC mismatch.\n");
                            }
                        }
                    }
                }
            } else {
                uPortLog("U_CELL_SEC_C2C_DECODE: corrupt frame, FCS mismatch.\n");
            }

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
            z = 0;
#endif
            // Look for the closing frame marker
            pData += chunkLength + 2; // +2 for the FCS bytes
            x = pRx->rxInLength - (pData - pRx->pRxIn);
            while ((x < pRx->rxInLength) &&
                   (*pData != (char) U_CELL_SEC_C2C_FRAME_MARKER)) {
                pData++;
                x++;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                z++;
#endif
            }
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
            uPortLog("U_CELL_SEC_C2C_DECODE: discarded %d byte(s)"
                     " looking for a closing frame marker.\n", z);
#endif
            if ((x < pRx->rxInLength) &&
                (*pData == (char) U_CELL_SEC_C2C_FRAME_MARKER)) {
                pData++;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
            } else {
                uPortLog("U_CELL_SEC_C2C_DECODE: didn't find one though.\n");
#endif
            }
            // Set the input length that is left
            pRx->rxInLength -= pData - pRx->pRxIn;
        } else {
            if (chunkLength > chunkLengthLimit) {
                // Error recovery: the chunk length is bigger
                // than it can be, potentially a mis-detected
                // frame-start flag due to corrupt input data.
                // Search forward for a potential new frame
                // start flag and dump up to that.
                uPortLog("U_CELL_SEC_C2C_DECODE: corrupt frame,"
                         " chunk length %d is larger than the"
                         " maximum %d byte(s).\n",
                         chunkLength, chunkLengthLimit);
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                z = 0;
#endif
                x = pRx->rxInLength - (pData - pRx->pRxIn);
                while ((x < pRx->rxInLength) &&
                       (*pData != (char) U_CELL_SEC_C2C_FRAME_MARKER)) {
                    pData++;
                    x++;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                    z++;
#endif
                }
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                uPortLog("U_CELL_SEC_C2C_DECODE: dumped %d byte(s)"
                         " looking for a frame marker to move on to.\n", z);
#endif
                if ((x < pRx->rxInLength) &&
                    (*pData == (char) U_CELL_SEC_C2C_FRAME_MARKER)) {
                    // This could be a starting or an ending frame marker:
                    // if there's nothing beyond it or the next byte is another
                    // frame marker then it is very likely an ending one so
                    // discard it
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                    uPortLog("U_CELL_SEC_C2C_DECODE: found a frame marker.\n");
#endif
                    if ((x == pRx->rxInLength - 1) ||
                        ((x < pRx->rxInLength - 1) &&
                         (*(pData + 1) == (char) U_CELL_SEC_C2C_FRAME_MARKER))) {
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                        uPortLog("U_CELL_SEC_C2C_DECODE: it was likely a closing"
                                 " frame marker, moving beyond it..\n");
#endif
                        pData++;
                    }
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                } else {
                    uPortLog("U_CELL_SEC_C2C_DECODE: didn't find one though.\n");
#endif
                }
            } else {
                // Don't have enough data to constitute a frame,
                // set pData back to where it was
                pData = pRx->pRxIn;
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
                uPortLog("U_CELL_SEC_C2C_DECODE: only have"
                         " %d byte(s) in the buffer, not enough"
                         " for all of our %d byte chunk (including"
                         " overheads), another %d byte(s) still"
                         " needed.\n",
                         pRx->rxInLength - x,
                         chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES,
                         chunkLength + U_CELL_SEC_C2C_OVERHEAD_BYTES - (pRx->rxInLength - x));
#endif
            }
        }
#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
    } else {
        uPortLog("U_CELL_SEC_C2C_DECODE: either no frame marker or"
                 " not enough bytes to form a frame yet.\n");
#endif
    }

#ifdef U_CELL_SEC_C2C_DETAILED_DEBUG
    uPortLog("U_CELL_SEC_C2C_DECODE: %d byte(s) consumed, %d byte(s) left.\n",
             pData - pRx->pRxIn, pRx->rxInLength);
#endif
    // Set pRxIn to wherever we've ended up
    pRx->pRxIn = pData;

    return length;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Transmit intercept function.
const char *pUCellSecC2cInterceptTx(uAtClientHandle_t atHandle,
                                    const char **ppData,
                                    size_t *pLength,
                                    void *pParameter)
{
    const char *pData = NULL;
    size_t length = 0;
    size_t lengthLeftOver = 0;
    uCellSecC2cContext_t *pContext = (uCellSecC2cContext_t *) pParameter;
    uCellSecC2cContextTx_t *pTx;

    (void) atHandle;

    if ((pContext != NULL) && (pLength != NULL)) {
        pTx = pContext->pTx;
        length = *pLength;
        if ((ppData != NULL) && (length > 0)) {
            // There is data to collect, add it to our transmit
            // input buffer, taking into account how big our buffer
            // would become when padding is added.
            if (paddedLength(pTx->txInLength + length,
                             U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES) > pTx->txInLimit) {
                // If the padding would take us over, the length
                // we can fit in is the limit minus one byte,
                // since padding always adds at least one byte to
                // the input
                length = pTx->txInLimit - (pTx->txInLength + 1);
                lengthLeftOver = *pLength - length;
            }
            memcpy(pTx->txIn + pTx->txInLength, *ppData, length);
            pTx->txInLength += length;
            // Move the data pointer on so that the caller
            // can see how far we've got
            *ppData += length;
        }
        // Assume that there is nothing to transmit onwards
        *pLength = 0;
        if (((ppData == NULL) && (pTx->txInLength > 0)) ||
            (lengthLeftOver > 0)) {
            // Either we're out of room or we're being flushed
            // so perform an encode
            *pLength = encode(pContext);
            pData = pTx->txOut;
            pTx->txInLength = 0;
        }
    }

    return pData;
}

// Obtain a random string to use as initial value.
// This is a default implementation only, intended to be
// overridden by the application.
U_WEAK
const char *pUCellSecC2cGetIv()
{
    for (size_t x = 0; x < sizeof(gIv); x++) {
        gIv[x] = (char) rand();
    }

    return gIv;
}

// Receive intercept function.
char *pUCellSecC2cInterceptRx(uAtClientHandle_t atHandle,
                              char **ppData,
                              size_t *pLength,
                              void *pParameter)
{
    char *pData = NULL;
    uCellSecC2cContext_t *pContext = (uCellSecC2cContext_t *) pParameter;
    uCellSecC2cContextRx_t *pRx;

    (void) atHandle;

    if ((pContext != NULL) && (pLength != NULL)) {
        pRx = pContext->pRx;
        pRx->rxInLength = *pLength;
        if (pRx->rxInLength > 0) {
            // Set the input and output pointers
            pRx->pRxIn = *ppData;
            pRx->pRxOut = NULL;
            // Try to decode a frame
            *pLength = decode(pContext);
            // Set the return value
            pData = pRx->pRxOut;
            // Set the pointer to the
            // amount consumed
            *ppData = pRx->pRxIn;
        }
    }

    return pData;
}

// End of file
