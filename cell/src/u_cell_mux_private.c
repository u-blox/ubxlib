/*
 * Copyright 2019-2024 u-blox
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
 * @brief Implementation of the encode/decode functions for 3GPP 27.010
 * CMUX support.  This functions are called by the u_cell_mux.h API
 * functions, they are not intended for use externally.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_compiler.h" // U_INLINE

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"

#include "u_ringbuffer.h"
#include "u_interface.h"

#include "u_at_client.h"

#include "u_device_serial.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_mux.h"
#include "u_cell_mux_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The CMUX frame boundary marker.
 */
#define U_CELL_MUX_PRIVATE_FRAME_MARKER 0xf9

/** Mask for the location of the command/response bit.
 */
#define U_CELL_MUX_PRIVATE_COMMAND_RESPONSE_BIT_MASK 0x02

/** Mask for the location of the poll/final bit.
 */
#define U_CELL_MUX_PRIVATE_POLL_FINAL_BIT_MASK 0x10

/** Mask for the location of the extension bit, both for address
 * and length.
 */
#define U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK 0x01

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold a linked list of [AT client URC prefix] strings.
 */
typedef struct uCellMuxStringList_t {
    char *pString;
    size_t stringLength;
    struct uCellMuxStringList_t *pNext;
} uCellMuxStringList_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table for FCS generation, reversed, 8-bit, poly 0x07.
 */
static const uint8_t gFcsTable[256] = {
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};

/** The valid frame types when decoding a frame.
 */
static const uCellMuxPrivateFrameType_t gFrameTypeDecode[] = {U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND,
                                                              U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND,
                                                              U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE,
                                                              U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE,
                                                              U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH,
                                                              U_CELL_MUX_PRIVATE_FRAME_TYPE_UI
                                                             };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Calculate the FCS for a frame sent by CMUX, ref. 3GPP 27.010 Annex B.
static uint8_t calculateFcs(const char *pBuffer, size_t length)
{
    const uint8_t *pOutput = (uint8_t *) pBuffer;
    uint8_t fcs = 0xFF;

    while (length > 0) {
        fcs = gFcsTable[fcs ^ *pOutput];
        pOutput++;
        length--;
    }

    return (0xFF - fcs);
}

// Return true if the frame type is a command when encoding a frame.
static bool isCommandEncode(uCellMuxPrivateFrameType_t type)
{
    return ((type == U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND) ||
            (type == U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND) ||
            (type == U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH) ||
            (type == U_CELL_MUX_PRIVATE_FRAME_TYPE_UI));
}

// Check if a frame type is valid when decoding.
static bool isValidTypeDecode(uCellMuxPrivateFrameType_t type)
{
    bool isValid = false;

    for (size_t x = 0; (x < sizeof(gFrameTypeDecode) / sizeof(gFrameTypeDecode[0])) &&
         !isValid; x++) {
        isValid = (type == gFrameTypeDecode[x]);
    }

    return isValid;
}

// Get the number of bytes available: if parseHandle is NULL then the
// buffer from pContext will be used as the source, else the ring-buffer
// will be used as the source.
U_INLINE static size_t bytesAvailable(uParseHandle_t parseHandle,
                                      uCellMuxPrivateParserContext_t *pContext)
{
    size_t sizeBytes;

    if (parseHandle == NULL) {
        sizeBytes = pContext->bufferSize - pContext->bufferIndex;
    } else {
        sizeBytes = uRingBufferBytesAvailableUnprotected(parseHandle);
    }

    return sizeBytes;
}

// Get the next byte: if parseHandle is NULL then the pContext buffer
// will be used as the source and pContext->bufferIndex will be advanced,
// else the ring-buffer will be used as the source.
U_INLINE static bool getByte(uParseHandle_t parseHandle,
                             uCellMuxPrivateParserContext_t *pContext,
                             uint8_t *pValue)
{
    bool success = false;

    if (parseHandle == NULL) {
        if (pContext->bufferIndex < pContext->bufferSize) {
            *pValue = *(pContext->pBuffer + pContext->bufferIndex);
            pContext->bufferIndex++;
            success = true;
        }
    } else {
        success = uRingBufferGetByteUnprotected(parseHandle, pValue);
    }

    return success;
}

// Get the discard size: if parseHandle is non-NULL then the ring
// buffer function will be called, else this will return 0 because
// that is always the right answer for the linear buffer case.
U_INLINE static size_t getDiscard(uParseHandle_t parseHandle)
{
    size_t discardBytes = 0;

    if (parseHandle != NULL) {
        discardBytes = uRingBufferBytesDiscardUnprotected(parseHandle);
    }

    return discardBytes;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: 3GPP 27.010 CMUX ENCODE/DECODE
 * -------------------------------------------------------------- */

// Encode a 3GPP 27.010 mux frame.
int32_t uCellMuxPrivateEncode(uint8_t address, uCellMuxPrivateFrameType_t type,
                              bool pollFinal, const char *pInformation,
                              size_t informationLengthBytes, char *pBuffer)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uint8_t *pOutput = (uint8_t *) pBuffer;
    size_t fcsLength;

    if ((pOutput != NULL) && (address <= U_CELL_MUX_PRIVATE_ADDRESS_MAX) &&
        ((pInformation != NULL) || (informationLengthBytes == 0)) &&
        (informationLengthBytes <= U_CELL_MUX_PRIVATE_INFORMATION_MAX_LENGTH_BYTES)) {
        // Write the opening flag
        *pOutput = U_CELL_MUX_PRIVATE_FRAME_MARKER;
        pOutput++;
        // Write the 6-bit address and C/R bit, ensuring
        // that the extension bit is set
        *pOutput = (uint8_t) (address << 2);
        if (isCommandEncode(type)) {
            *pOutput |= U_CELL_MUX_PRIVATE_COMMAND_RESPONSE_BIT_MASK;
        }
        *pOutput |= U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK;
        pOutput++;
        // Write the 8-bit control field with the poll/final bit
        *pOutput = (uint8_t) type;
        if (pollFinal) {
            *pOutput |= U_CELL_MUX_PRIVATE_POLL_FINAL_BIT_MASK;
        }
        pOutput++;
        // Write the first byte of the length, which is in
        // the upper 7 bits, with the Extension bit, bit 0,
        // at zero
        *pOutput = (uint8_t) (informationLengthBytes << 1);
        if (informationLengthBytes > 0x7F) {
            // Length is more than one byte will hold, so
            // leave the Extension bit at zero and write
            // a second length byte
            pOutput++;
            *pOutput = (uint8_t) (informationLengthBytes >> 7);
        } else {
            // Length fits in one byte, set the Extension
            // bit to 1 to signal this
            *pOutput |= U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK;
        }
        pOutput++;
        // FCS length is at least what we've written so far
        // minus the opening flag byte
        fcsLength = ((char *) pOutput) - pBuffer - 1;
        if (pInformation != NULL) {
            // Copy in the information field
            memcpy(pOutput, pInformation, informationLengthBytes);
            pOutput += informationLengthBytes;
        }
        // Add the FCS, calculated over everything but the
        // opening flag byte, and only including the information
        // field if this is NOT a UIH frame
        if (type != U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH) {
            fcsLength += informationLengthBytes;
        }
        *pOutput = calculateFcs(pBuffer + 1, fcsLength);
        pOutput++;
        // Write the closing flag
        *pOutput = U_CELL_MUX_PRIVATE_FRAME_MARKER;
        pOutput++;
        errorCodeOrSize = ((char *) pOutput) - pBuffer;
    }

    return errorCodeOrSize;
}

// Parse a buffer for a CMUX frame.
int32_t uCellMuxPrivateParseCmux(uParseHandle_t parseHandle, void *pUserParam)
{
    uCellMuxPrivateParserContext_t *pContextParser = (uCellMuxPrivateParserContext_t *) pUserParam;
    uint8_t x = 0;

    if (bytesAvailable(parseHandle, pContextParser) < U_CELL_MUX_PRIVATE_FRAME_MIN_LENGTH_BYTES) {
        return U_ERROR_COMMON_TIMEOUT;
    }
    getByte(parseHandle, pContextParser, &x);
    if (U_CELL_MUX_PRIVATE_FRAME_MARKER != x) {  // = 0xF9
        return U_ERROR_COMMON_NOT_FOUND;
    }
    uint8_t fcs = 0xFF;
    // Next should be address but we might have caught a closing
    // frame marker so accept an opening frame marker if there is one:
    // This would mess-up if we ever had an address of 62 (0xF9 >> 2)
    // but thankfully we never go that high
    getByte(parseHandle, pContextParser, &x);
    if (U_CELL_MUX_PRIVATE_FRAME_MARKER == x) {
        getByte(parseHandle, pContextParser, &x);
        // Re-check that we have the minimum length, since the check at
        // the start of this function would not have included the extra flag
        if (bytesAvailable(parseHandle, pContextParser) < U_CELL_MUX_PRIVATE_FRAME_MIN_LENGTH_BYTES - 1) {
            return U_ERROR_COMMON_TIMEOUT;
        }
    }
    uint8_t address = x >> 2;
    bool commandResponse = ((x & U_CELL_MUX_PRIVATE_COMMAND_RESPONSE_BIT_MASK) ==
                            U_CELL_MUX_PRIVATE_COMMAND_RESPONSE_BIT_MASK);
    if (((x & U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK) != U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK) ||
        !((pContextParser->address == U_CELL_MUX_PRIVATE_ADDRESS_ANY) ||
          (pContextParser->address == address))) {
        return U_ERROR_COMMON_NOT_FOUND;
    }
    fcs = gFcsTable[fcs ^ x];
    getByte(parseHandle, pContextParser, &x); // control
    uCellMuxPrivateFrameType_t type = x & ~U_CELL_MUX_PRIVATE_POLL_FINAL_BIT_MASK;
    bool pollFinal = ((x & U_CELL_MUX_PRIVATE_POLL_FINAL_BIT_MASK) != 0);
    if (!isValidTypeDecode(type)) {
        return U_ERROR_COMMON_NOT_FOUND;
    }
    fcs = gFcsTable[fcs ^ x];
    getByte(parseHandle, pContextParser, &x); // first byte of I-field length
    uint16_t informationLengthBytes = x >> 1;
    fcs = gFcsTable[fcs ^ x];
    if ((x & U_CELL_MUX_PRIVATE_EXTENSION_BIT_MASK) == 0) {
        getByte(parseHandle, pContextParser, &x); // second byte of I-field length
        informationLengthBytes += ((uint16_t) x) << 7;
        fcs = gFcsTable[fcs ^ x];
    }
    // +2 below for FCS and closing flag
    if (bytesAvailable(parseHandle, pContextParser) < (size_t) informationLengthBytes + 2) {
        return U_ERROR_COMMON_TIMEOUT;
    }
    for (size_t y = 0; y < informationLengthBytes; y++) {
        getByte(parseHandle, pContextParser, &x);
        if ((pContextParser->pInformation != NULL) && (y < pContextParser->informationLengthBytes)) {
            *(pContextParser->pInformation + y) = (char) x;
        }
        if (type != U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH) {
            fcs = gFcsTable[fcs ^ x];
        }
    }
    getByte(parseHandle, pContextParser, &x);
    // 0xCF is the reversed order of 11110011
    if (gFcsTable[fcs ^ x] != 0xCF) {
        return U_ERROR_COMMON_NOT_FOUND;
    }
    getByte(parseHandle, pContextParser, &x);
    if (U_CELL_MUX_PRIVATE_FRAME_MARKER != x) {  // = 0xF9
        return U_ERROR_COMMON_NOT_FOUND;
    }
    // We can only claim a decoded CMUX frame if
    // there was nothing that needed discarding first.
    if (getDiscard(parseHandle) == 0) {
        pContextParser->address = address;
        pContextParser->commandResponse = commandResponse;
        pContextParser->type = type;
        pContextParser->pollFinal = pollFinal;
        pContextParser->informationLengthBytes = informationLengthBytes;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Copy the settings of one AT client into another AT client.
int32_t uCellMuxPrivateCopyAtClient(uAtClientHandle_t atHandleSource,
                                    uAtClientHandle_t atHandleDestination)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    const char *pString = NULL;
    void (*pUrcHandler) (uAtClientHandle_t, void *) = NULL;
    int32_t (*pWakeUpHandler) (uAtClientHandle_t, void *) = NULL;
    void (*pTimeoutCallback) (uAtClientHandle_t, int32_t *) = NULL;
    void *pHandlerParam = NULL;
    int32_t a;
    int32_t b = 0;
    int32_t c = 0;
    bool d = false;
    uCellMuxStringList_t *pUrcPrefixRoot = NULL;
    uCellMuxStringList_t *pUrcPrefixTmp = NULL;
    uCellMuxStringList_t **ppUrcPrefix = &pUrcPrefixRoot;
    uCellMuxStringList_t **ppUrcPrefixTmp = NULL;

    // Remove all of the existing AT handlers in the destination AT handler
    // First grab the prefix strings into temporary storage
    for (int32_t x = uAtClientUrcHandlerGetFirst(atHandleDestination, &pString, &pUrcHandler,
                                                 &pHandlerParam);
         (x >= 0) && (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS);
         x = uAtClientUrcHandlerGetNext(atHandleDestination, &pString, &pUrcHandler, &pHandlerParam)) {
        *ppUrcPrefix = (uCellMuxStringList_t *) pUPortMalloc(sizeof(uCellMuxStringList_t));
        if (*ppUrcPrefix != NULL) {
            (*ppUrcPrefix)->pNext = NULL;
            (*ppUrcPrefix)->stringLength = strlen(pString);
            // +1 to allow for a null terminator
            (*ppUrcPrefix)->pString = (char *) pUPortMalloc((*ppUrcPrefix)->stringLength + 1);
            if ((*ppUrcPrefix)->pString != NULL) {
                strncpy((*ppUrcPrefix)->pString, pString, (*ppUrcPrefix)->stringLength + 1);
                if (ppUrcPrefixTmp != NULL) {
                    *ppUrcPrefixTmp = *ppUrcPrefix;
                }
                ppUrcPrefixTmp = &((*ppUrcPrefix)->pNext);
                ppUrcPrefix = &((*ppUrcPrefix)->pNext);
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            }
        } else {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        }
    }
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        //... then remove them, freeing the temporary storage as we go
        while (pUrcPrefixRoot != NULL) {
            uAtClientRemoveUrcHandler(atHandleDestination, pUrcPrefixRoot->pString);
            uPortFree(pUrcPrefixRoot->pString);
            pUrcPrefixTmp = pUrcPrefixRoot;
            pUrcPrefixRoot = pUrcPrefixRoot->pNext;
            uPortFree(pUrcPrefixTmp);
        }
        // Copy the URC handlers into the now-empty-of-URC-handlers destination AT handler
        for (int32_t x = uAtClientUrcHandlerGetFirst(atHandleSource, &pString, &pUrcHandler,
                                                     &pHandlerParam);
             (x >= 0) && (uAtClientSetUrcHandler(atHandleDestination, pString, pUrcHandler, pHandlerParam) == 0);
             x = uAtClientUrcHandlerGetNext(atHandleSource, &pString, &pUrcHandler, &pHandlerParam)) {
        }

        //  Copy the settings
        uAtClientDebugSet(atHandleDestination, uAtClientDebugGet(atHandleSource));
        uAtClientPrintAtSet(atHandleDestination, uAtClientPrintAtGet(atHandleSource));
        uAtClientTimeoutSet(atHandleDestination, uAtClientTimeoutGet(atHandleSource));
        uAtClientTimeoutUrcSet(atHandleDestination, uAtClientTimeoutUrcGet(atHandleSource));
        uAtClientReadRetryDelaySet(atHandleDestination, uAtClientReadRetryDelayGet(atHandleSource));
        uAtClientDelimiterSet(atHandleDestination, uAtClientDelimiterGet(atHandleSource));
        uAtClientDelaySet(atHandleDestination, uAtClientDelayGet(atHandleSource));
        a = uAtClientGetActivityPinSettings(atHandleSource, &b, &c, &d);
        uAtClientSetActivityPin(atHandleDestination, a, b, c, d);

        // Copy the time-out callback and the wake-up handler
        uAtClientTimeoutCallbackGet(atHandleSource, &pTimeoutCallback);
        uAtClientTimeoutCallbackSet(atHandleDestination, pTimeoutCallback);
        uAtClientGetWakeUpHandler(atHandleSource, &pWakeUpHandler, &pHandlerParam, &a);
        uAtClientSetWakeUpHandler(atHandleDestination, pWakeUpHandler, pHandlerParam, a);
    } else {
        // Clean-up on error
        while (pUrcPrefixRoot != NULL) {
            uPortFree(pUrcPrefixRoot->pString);
            pUrcPrefixTmp = pUrcPrefixRoot;
            pUrcPrefixRoot = pUrcPrefixRoot->pNext;
            uPortFree(pUrcPrefixTmp);
        }
    }

    return errorCode;
}

// Remove the CMUX context for the given cellular instance.
void uCellMuxPrivateRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellMuxPrivateContext_t *pContext;
    uCellMuxPrivateChannelContext_t *pChannelContext;

    if (uCellMuxPrivateDisable(pInstance) == 0) {
        pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
        if (pContext != NULL) {
            // Free memory
            for (int32_t x = 0; x  < (sizeof(pContext->pDeviceSerial) / sizeof(pContext->pDeviceSerial[0]));
                 x++) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(
                                      pContext->pDeviceSerial[x]);
                if (pChannelContext != NULL) {
                    uPortMutexDelete(pChannelContext->mutex);
                    uPortMutexDelete(pChannelContext->mutexUserDataWrite);
                    uPortMutexDelete(pChannelContext->mutexUserDataRead);
                    uDeviceSerialDelete(pContext->pDeviceSerial[x]);
                }
            }
            uRingBufferGiveReadHandle(&(pContext->ringBuffer), pContext->readHandle);
            uRingBufferDelete(&(pContext->ringBuffer));
            uPortEventQueueClose(pContext->eventQueueHandle);
            uPortFree(pInstance->pMuxContext);
            pInstance->pMuxContext = NULL;
#ifdef U_CELL_MUX_ENABLE_DEBUG
            uPortLog("U_CELL_CMUX: memory free'd.\n");
#endif
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THERE ARE MORE uCellMuxPrivateXxx() FUNCTIONS IN U_CELL_MUX.C
 * -------------------------------------------------------------- */

// End of file
