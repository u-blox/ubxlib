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

/* NOTE TO IMPLEMENTERS: this is a simple implementation focussed
 * purely on deleting the FPLMN list.  If we get more interest in
 * SIM stuff then it would be worth rejigging it to offer a
 * generic +CSIM/+CRSM interface as a public API; but only if
 * there is interest.
 */

/** @file
 * @brief Implementation of the SIM API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MAX
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_hex_bin_convert.h"

#include "u_port.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_heap.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_file.h"
#include "u_cell_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The ID of the FPLMN list entry on the SIM.
 */
#define U_CELL_CSIM_FILE_ID_FPLMN 0x6f7b

/** The size of the FPLMN field on the SIM: for a 2G SIM this is
 * 4 * 3 bytes, however for a 3G SIM the fields are 5 bytes big
 * instead of 3 bytes big.
 */
#define U_CELL_CSIM_FPLMN_SIZE_BYTES_MAX 20

/** The size of the FPLMN field on a 2G SIM: 4 * 3 bytes.
 */
#define U_CELL_CSIM_FPLMN_SIZE_BYTES_2G 12

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible command types for AT+CRSM.
 */
typedef enum {
    U_CELL_SIM_COMMAND_READ_BINARY = 176,
    U_CELL_SIM_COMMAND_READ_RECORD = 178,
    U_CELL_SIM_COMMAND_GET_RESPONSE = 192,
    U_CELL_SIM_COMMAND_RETRIEVE_DATA = 203,
    U_CELL_SIM_COMMAND_UPDATE_BINARY = 214,
    U_CELL_SIM_COMMAND_SET_DATA = 219,
    U_CELL_SIM_COMMAND_UPDATE_RECORD = 220,
    U_CELL_SIM_COMMAND_STATUS = 242
} uCellSimCommand_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Do an AT+CRSM operation: pBinaryIn is the input BINARY data
// (so NOT a hex string, the binary data that would be encoded in
// such a string) and may be NULL.  The return value is the length
// of the BINARY data written to pBinaryOut, or which _would_ have
// been written to pBinaryOut if it were not NULL.
// The main response parameters are placed into pSw1 and pSw2.
static int32_t crsm(uAtClientHandle_t atHandle,
                    uCellSimCommand_t command, int32_t fileId,
                    int32_t p1, int32_t p2, int32_t p3,
                    const char *pBinaryIn, size_t binaryLengthIn,
                    int32_t *pSw1, int32_t *pSw2,
                    char *pBinaryOut, size_t binaryLengthOutMax)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t x;
    int32_t y;
    int32_t z;
    char *pHex = NULL;
    char *pReadBuffer = NULL;

    if (((pBinaryIn != NULL) && (binaryLengthIn > 0)) ||
        ((pBinaryOut != NULL) && (binaryLengthOutMax > 0))) {
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Get memory for doing HEX strings
        x = binaryLengthIn;
        if ((int32_t) binaryLengthOutMax > x) {
            x = binaryLengthOutMax;
        }
        pHex = pUPortMalloc((x * 2) + 1);  // +1 for null terminator
        if (pHex != NULL) {
            // Write the BINARY data from pBinaryIn as a hex string
            x = uBinToHex(pBinaryIn, binaryLengthIn, pHex);
            // Add a terminator
            *(pHex + x) = 0;
            errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    if (errorCodeOrSize == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CRSM=");
        uAtClientWriteInt(atHandle, command);
        uAtClientWriteInt(atHandle, fileId);
        uAtClientWriteInt(atHandle, p1);
        uAtClientWriteInt(atHandle, p2);
        uAtClientWriteInt(atHandle, p3);
        if (pBinaryIn != NULL) {
            uAtClientWriteString(atHandle, pHex, true);
        }
        uAtClientCommandStop(atHandle);
        // The response should be +CRSM: SW1, SW2[, "hex string"]
        uAtClientResponseStart(atHandle, "+CRSM:");
        // Read SW1 and SW2 and the hex string, if present
        x = uAtClientReadInt(atHandle);
        y = uAtClientReadInt(atHandle);
        // Leave the buffer passed to the AT client read
        // function as NULL if we've not been given an
        // output buffer; this way the AT client read
        // function will return the number of characters
        // in the hex string and throw them away
        if (pBinaryOut != NULL) {
            pReadBuffer = pHex;
        }
        if (pReadBuffer == NULL) {
            // Don't limit the read length if we're throwing it away;
            // strange calculation in order to end up with INT_MAX
            // going into uAtClientReadString() just below
            binaryLengthOutMax = (INT_MAX / 2) - 1;
        }
        z = uAtClientReadString(atHandle, pReadBuffer,
                                (binaryLengthOutMax * 2) + 1, false);
        uAtClientResponseStop(atHandle);
        errorCodeOrSize = uAtClientUnlock(atHandle);
        if (errorCodeOrSize == 0) {
            if (pSw1 != NULL) {
                *pSw1 = x;
            }
            if (pSw2 != NULL) {
                *pSw2 = y;
            }
            if (z > 0) {
                if (pBinaryOut != NULL) {
                    // Write the hex string from pHex to
                    // BINARY in pBinaryOut
                    uHexToBin(pHex, z, pBinaryOut);
                }
            }
            errorCodeOrSize = z >> 2;
        }
    }

    uPortFree(pHex);

    return errorCodeOrSize;
}

// Parse the response to a CRSM command.
static int32_t crsmParseResponse(int32_t sw1, int32_t sw2)
{
    // Set a nice obvious error code
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PROTOCOL_ERROR;

    (void) sw2;

    // 0x90 in SW1 is success for both 2G and 3G SIM cards but
    // the "success after retrying internally" codes are
    // different: 0x92 for 2G, 0x63 for 3G
    if ((sw1 == 0x90) || (sw1 == 0x92) || (sw1 == 0x63)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Determine the length of the FPLMN field on the SIM.
static int32_t crsmGetFplmnLength(uAtClientHandle_t atHandle)
{
    int32_t errorCodeOrSize;
    int32_t sw1 = 0;
    int32_t sw2 = 0;

    // Attempt to read the maximum size of FPLMN
    errorCodeOrSize = crsm(atHandle, U_CELL_SIM_COMMAND_READ_BINARY,
                           U_CELL_CSIM_FILE_ID_FPLMN, 0, 0,
                           U_CELL_CSIM_FPLMN_SIZE_BYTES_MAX,
                           NULL, 0, &sw1, &sw2, NULL, 0);
    if (errorCodeOrSize >= 0) {
        // Parse the response
        if ((crsmParseResponse(sw1, sw2) == 0) && (errorCodeOrSize > 0)) {
            // The number of bytes is the response length, we're done
        } else if (sw1 == 0x67) {
            // 0x67 means "wrong length"; trying to read the
            // maximum length has failed so we have to assume
            // the smaller 2G length
            errorCodeOrSize = U_CELL_CSIM_FPLMN_SIZE_BYTES_2G;
        }
    } else {
        // Some modules (e.g. LENA-R8) return "+CME ERROR: parameters
        // are invalid" so, when there is an AT error, we just have
        // to assume the shorter 2G length again
        errorCodeOrSize = U_CELL_CSIM_FPLMN_SIZE_BYTES_2G;
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Delete the FPLMN list from the SIM.
int32_t uCellSimFplmnListDelete(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t sw1 = 0;
    int32_t sw2 = 0;
    size_t length;
    // Enough room for the binary data to write
    char buffer[U_CELL_CSIM_FPLMN_SIZE_BYTES_MAX];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            // Note: knife-and-forking this for now, which is to
            // write 0xFFFFFF or 0xFFFFFFFFFF (depending on whether
            // we're talking to a 2G or a 3G SIM) to the
            // [up to four] entries that comprise the FPLMN list

            // First, find out how long the FPLMN field is, by reading it
            errorCode = crsmGetFplmnLength(atHandle);
            if (errorCode >= 0) {
                length = errorCode;
                if (length > sizeof(buffer)) {
                    length = sizeof(buffer);
                }
                // Fill buffer with the binary data, 0xFF to do the overwriting
                for (int32_t x = 0; x < (int32_t) length; x++) {
                    buffer[x] = 0xFF;
                }
                // Now do the write to delete the FPLMN data
                errorCode = crsm(atHandle, U_CELL_SIM_COMMAND_UPDATE_BINARY,
                                 U_CELL_CSIM_FILE_ID_FPLMN, 0, 0, length,
                                 buffer, length, &sw1, &sw2, NULL, 0);
                if (errorCode == 0) {
                    // Parse the response
                    errorCode = crsmParseResponse(sw1, sw2);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
