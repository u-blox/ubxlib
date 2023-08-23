/*
 * Copyright 2019-2023 u-blox
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
 * @brief This file contains the implementation of the uGnssDec
 * API, used for decoding a useful subset of messages from a GNSS
 * device.
 *
 * To add a new message to the set of message decoders:
 *
 * 1.  Create a .h file in the "api" directory which defines the
 * message; for example, if you were creating a decoder for the
 * UBX message UBX-XXX-YYY the header file would be named
 * u_gnss_dec_ubx_xxx_yyy.h.  Use the naming convention and sizes of
 * the GNSS device interface manual in your types, bring out any
 * bit-fields and enums properly, forming them in the way the
 * current UBX-NAV-PVT decoder does, and document them all well,
 * including units, to produce a good set of Doxygen documentation
 * so that the customer doesn't have to keep referring back to the
 * interface manual: see u_gnss_dec_ubx_nav_pvt.h for an example.
 * Make sure to follow the usual pattern for the header file gating
 * \#defines and the _MESSAGE_CLASS, _MESSAGE_ID and _BODY_MIN_LENGTH
 * macros.  You may also choose to define helper functions which
 * convert the elements of the structure as defined by the GNSS
 * device interface manual into more friendly structures.
 *
 * 2. \#include this new header file in u_gnss_dec.h, add it to
 * ubxlib.h and add the new message struct to the #uGnssDecUnion_t
 * in this file.
 *
 * 3. Create the static decode function for the message here,
 * following the naming pattern, e.g. for UBX-XXX-YYY the function
 * would be named ubxXxxYyyAlloc(); the function  must have the
 * function signature of #uGnssDecKnownFunction_t.
 *
 * 4. Add the static function to the gpFunctionList array and add
 * its message ID to the gIdList array, making sure to put it in the
 * same position in both.
 *
 * 5. If in step (1) you chose to include helper functions, add a
 * .c file in this src directory, of the same name as the .h file,
 * which implements the helper functions; see u_gnss_dec_ubx_nav_pvt.c
 * for an example.
 *
 * 6. Add at least one test vector for the function to the
 * gTestDataKnownSet array in u_gnss_dec_test.c, using the pattern
 * of gUbxNavPvt as an example, and a spot-test for each helper
 * function if there are any (again, see the handling of UBX-NAV-PVT
 * for an example).
 *
 * Obviously it would be possible to add NMEA messages, or RTCM messages,
 * in the same way, just replacing "ubx" with "nmea" or "rtcm", but note
 * that this code does not use NMEA or RTCM messages and we want to avoid
 * code bloat, hence the uGnssDecSetCallback() hook to allow a customer
 * to add their own decoders at run-time.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"
#include "u_gnss_msg.h"
#include "u_gnss_dec.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Private version of function that can be hooked into
 * pUGnssDecAlloc() by uGnssDecSetCallback() to decode message types
 * that _are_ known to this code.
 *
 * @param[in] pBuffer             the buffer pointer that was passed to
 *                                pUGnssDecAlloc().
 * @param size                    the number of bytes at pBuffer;
 *                                for a known protocol it _might_
 *                                be that any FCS/check-sum bytes
 *                                on the end have been removed
 *                                by the caller, hence the function
 *                                should not _require_ them to be
 *                                present in the count.
 * @param[out] ppBody             a pointer to a place to put the
 *                                decoded message body, which the
 *                                callback should allocate using
 *                                pUPortMalloc(); will never be NULL.
 * @return                        zero on a successful decode, else
 *                                negative error code, preferably
 *                                from the set suggested for the
 *                                errorCode field of #uGnssDec_t.
 */
typedef int32_t (uGnssDecKnownFunction_t) (const char *pBuffer,
                                           size_t size,
                                           uGnssDecUnion_t **ppBody);

/* ----------------------------------------------------------------
 * STATIC VARIABLES: MISC
 * -------------------------------------------------------------- */

/** A place to store the user callback.
 */
static uGnssDecFunction_t *gpCallback = NULL;

/** A place to store the parameter for the user callback.
 */
static void *gpCallbackParam = NULL;

/** The list of known message IDs; order is important,
 * MUST be in the same order as gpFunctionList (see further
 * down in this file) and both lists must contain the same number
 * of elements.
 */
static const uGnssMessageId_t gIdList[] = {
    {
        .type = U_GNSS_PROTOCOL_UBX,
        .id.ubx = U_GNSS_UBX_MESSAGE(U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_CLASS, U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_ID)
    },
    {
        .type = U_GNSS_PROTOCOL_UBX,
        .id.ubx = U_GNSS_UBX_MESSAGE(U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_CLASS, U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_ID)
    }
};

// MORE STATIC VARIABLES after the message decoders...

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MESSAGE DECODERS
 * -------------------------------------------------------------- */

// Decode a UBX-NAV-PVT message.
static int32_t ubxNavPvtAlloc(const char *pBuffer, size_t size,
                              uGnssDecUnion_t **ppBody)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TRUNCATED;
    uGnssDecUbxNavPvt_t *pBody;

    // No need to check pBuffer or ppBody for NULLity,
    // we will never give this function NULL for those.
    if (size >= U_UBX_PROTOCOL_HEADER_LENGTH_BYTES + U_GNSS_DEC_UBX_NAV_PVT_BODY_MIN_LENGTH) {
        // Move past the header so that we can use payload offsets
        // throughout, matching the offsets in the interface manual
        pBuffer += U_UBX_PROTOCOL_HEADER_LENGTH_BYTES;
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pBody = (uGnssDecUbxNavPvt_t *) pUPortMalloc(sizeof(uGnssDecUbxNavPvt_t));
        if (pBody != NULL) {
            memset(pBody, 0, sizeof(*pBody));
            // All good now, unless we hit a field we can't decode,
            // in which case we _could_ set U_ERROR_COMMON_BAD_DATA,
            // but, since this message will have been checked for
            // integrity before it gets here, it is better to trust
            // that the module emitted stuff correctly: it knows
            // more about this than we do
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pBody->iTOW = (int32_t) uUbxProtocolUint32Decode(pBuffer + 0);
            pBody->year = uUbxProtocolUint16Decode(pBuffer + 4);
            pBody->month = (uint8_t) *(pBuffer + 6); // *NOPAD* stop AStyle making * look like a multiply
            pBody->day = (uint8_t) *(pBuffer + 7); // *NOPAD*
            pBody->hour = (uint8_t) *(pBuffer + 8); // *NOPAD*
            pBody->min = (uint8_t) *(pBuffer + 9); // *NOPAD*
            pBody->sec = (uint8_t) *(pBuffer + 10); // *NOPAD*
            pBody->valid = (uint8_t) *(pBuffer + 11); // *NOPAD*
            pBody->tAcc = uUbxProtocolUint32Decode(pBuffer + 12);
            pBody->nano = (int32_t) uUbxProtocolUint32Decode(pBuffer + 16);
            pBody->fixType = (uGnssDecUbxNavPvtFixType_t) *(pBuffer + 20); // *NOPAD*
            pBody->flags = (uint8_t) *(pBuffer + 21); // *NOPAD*
            pBody->flags2 = (uint8_t) *(pBuffer + 22); // *NOPAD*
            pBody->numSV = (uint8_t) *(pBuffer + 23); // *NOPAD*
            pBody->lon = (int32_t) uUbxProtocolUint32Decode(pBuffer + 24);
            pBody->lat = (int32_t) uUbxProtocolUint32Decode(pBuffer + 28);
            pBody->height = (int32_t) uUbxProtocolUint32Decode(pBuffer + 32);
            pBody->hMSL = (int32_t) uUbxProtocolUint32Decode(pBuffer + 36);
            pBody->hAcc = uUbxProtocolUint32Decode(pBuffer + 40);
            pBody->vAcc = uUbxProtocolUint32Decode(pBuffer + 44);
            pBody->velN = (int32_t) uUbxProtocolUint32Decode(pBuffer + 48);
            pBody->velE = (int32_t) uUbxProtocolUint32Decode(pBuffer + 52);
            pBody->velD = (int32_t) uUbxProtocolUint32Decode(pBuffer + 56);
            pBody->gSpeed = (int32_t) uUbxProtocolUint32Decode(pBuffer + 60);
            pBody->headMot = (int32_t) uUbxProtocolUint32Decode(pBuffer + 64);
            pBody->sAcc = uUbxProtocolUint32Decode(pBuffer + 68);
            pBody->headAcc = uUbxProtocolUint32Decode(pBuffer + 72);
            pBody->pDOP = uUbxProtocolUint16Decode(pBuffer + 76);
            pBody->flags3 = uUbxProtocolUint16Decode(pBuffer + 78);
            // 4 reserved bytes here
            pBody->headVeh = (int32_t) uUbxProtocolUint32Decode(pBuffer + 84);
            pBody->magDec = (int16_t) uUbxProtocolUint16Decode(pBuffer + 88);
            pBody->magAcc = (int16_t) uUbxProtocolUint16Decode(pBuffer + 90);

            // Populate the pointer that was passed in
            *ppBody = (uGnssDecUnion_t *) pBody;
        }
    }

    return errorCode;
}

// Decode a UBX-NAV-HPPOSLLH message.
static int32_t ubxNavHpposllhAlloc(const char *pBuffer, size_t size,
                                   uGnssDecUnion_t **ppBody)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TRUNCATED;
    uGnssDecUbxNavHpposllh_t *pBody;

    // No need to check pBuffer or ppBody for NULLity,
    // we will never give this function NULL for those.
    if (size >= U_UBX_PROTOCOL_HEADER_LENGTH_BYTES + U_GNSS_DEC_UBX_NAV_HPPOSLLH_BODY_MIN_LENGTH) {
        // Move past the header so that we can use payload offsets
        // throughout, matching the offsets in the interface manual
        pBuffer += U_UBX_PROTOCOL_HEADER_LENGTH_BYTES;
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pBody = (uGnssDecUbxNavHpposllh_t *) pUPortMalloc(sizeof(uGnssDecUbxNavHpposllh_t));
        if (pBody != NULL) {
            memset(pBody, 0, sizeof(*pBody));
            // All good now, unless we hit a field we can't decode,
            // in which case we _could_ set U_ERROR_COMMON_BAD_DATA,
            // but, since this message will have been checked for
            // integrity before it gets here, it is better to trust
            // that the module emitted stuff correctly: it knows
            // more about this than we do
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pBody->version = (uint8_t) *(pBuffer + 0); // *NOPAD* stop AStyle making * look like a multiply
            // 2 reserved bytes here
            pBody->flags = (uint8_t) *(pBuffer + 3); // *NOPAD*
            pBody->iTOW = (int32_t) uUbxProtocolUint32Decode(pBuffer + 4);
            pBody->lon = (int32_t) uUbxProtocolUint32Decode(pBuffer + 8);
            pBody->lat = (int32_t) uUbxProtocolUint32Decode(pBuffer + 12);
            pBody->height = (int32_t) uUbxProtocolUint32Decode(pBuffer + 16);
            pBody->hMSL = (int32_t) uUbxProtocolUint32Decode(pBuffer + 20);
            pBody->lonHp = (int8_t) *(pBuffer + 24); // *NOPAD*
            pBody->latHp = (int8_t) *(pBuffer + 25); // *NOPAD*
            pBody->heightHp = (int8_t) *(pBuffer + 26); // *NOPAD*
            pBody->hMSLHp = (int8_t) *(pBuffer + 27); // *NOPAD*
            pBody->hAcc = uUbxProtocolUint32Decode(pBuffer + 28);
            pBody->vAcc = uUbxProtocolUint32Decode(pBuffer + 32);

            // Populate the pointer that was passed in
            *ppBody = (uGnssDecUnion_t *) pBody;
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC VARIABLES: MESSAGE DECODER LIST
 * -------------------------------------------------------------- */

/** A list of message decode functions; order is important,
 * MUST be in the same order as gIdList and both lists
 * must contain the same number of elements.
 */
static uGnssDecKnownFunction_t *gpFunctionList[] = {
    ubxNavPvtAlloc,
    ubxNavHpposllhAlloc
};

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Decode a message buffer received from a GNSS device.
uGnssDec_t *pUGnssDecAlloc(const char *pBuffer, size_t size)
{
    uGnssDec_t *pDec = NULL;
    uint8_t *pBufferUint8 = (uint8_t *) pBuffer; // To avoid problems with signed char compares
    uGnssDecKnownFunction_t *pFunction = NULL;
    size_t x;
    size_t y;

    pDec = (uGnssDec_t *) pUPortMalloc(sizeof(uGnssDec_t));
    if (pDec != NULL) {
        memset(pDec, 0, sizeof(*pDec));
        pDec->errorCode = (int32_t) U_ERROR_COMMON_EMPTY;
        pDec->id.type = U_GNSS_PROTOCOL_UNKNOWN;
        if ((pBufferUint8 != NULL) && (size > 0)) {
            // Determine the protocol type/message ID and make
            // sure the header is sound
            pDec->errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
            if ((*pBufferUint8 == 0xB5) && (size >= 1) && (*(pBufferUint8 + 1) == 0x62)) {
                // Likely a UBX message
                pBufferUint8 += 2;
                pDec->id.type = U_GNSS_PROTOCOL_UBX;
                pDec->errorCode = (int32_t) U_ERROR_COMMON_TRUNCATED;
                if (size >= U_UBX_PROTOCOL_HEADER_LENGTH_BYTES) {
                    // Grab the message class and message ID, check the length,
                    // allowing the checksum bytes to be omitted
                    pDec->id.id.ubx = U_GNSS_UBX_MESSAGE(*pBufferUint8, *(pBufferUint8 + 1));
                    pBufferUint8 += 2;
                    y = *pBufferUint8 + ((uint16_t) *(pBufferUint8 + 1) << 8); // *NOPAD*
                    if (size >= y + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES) {
                        pDec->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            } else if (*pBufferUint8 == '$') {
                // Likely an NMEA message
                pBufferUint8++;
                y = size - 1;
                pDec->id.type = U_GNSS_PROTOCOL_NMEA;
                pDec->errorCode = (int32_t) U_ERROR_COMMON_TRUNCATED;
                for (x = 0; (((*pBufferUint8 >= 'A') && (*pBufferUint8 <= 'Z')) ||
                             ((*pBufferUint8 >= '0') && (*pBufferUint8 <= '9'))) &&
                     (x < y) && (x < sizeof(pDec->nmea) - 1); x++) {
                    // Looking for up to U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS
                    // characters in the range 0-9, A-Z, followed by a comma
                    pDec->nmea[x] = *pBufferUint8;
                    pBufferUint8++;
                }
                if ((x < y) && (*pBufferUint8 == ',')) {
                    pDec->id.id.pNmea = pDec->nmea;
                    pDec->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
                // No need to add a terminator since we zeroed the structure to begin with
            } else if (*pBufferUint8 == 0xD3) {
                // Likely an RTCM message
                pBufferUint8++;
                pDec->id.type = U_GNSS_PROTOCOL_RTCM;
                pDec->errorCode = (int32_t) U_ERROR_COMMON_TRUNCATED;
                // Length is only in the first three bits of the first length byte,
                // the rest must be zero
                if ((size >= 1 /* D3 */ + 2 /* length */) &&
                    ((*pBufferUint8 & 0xFC) == 0)) {
                    y = ((uint16_t) (*pBufferUint8 & 0x03) << 8) + *(pBufferUint8 + 1);
                    pBufferUint8 += 2;
                    if (size >= 1 /* D3 */ + 2 /* length */ + 2 /* ID */) {
                        // Grab the ID from the next two bytes
                        pDec->id.id.rtcm = (*(pBufferUint8 + 1) >> 4) + (((uint16_t) *pBufferUint8) << 4); // *NOPAD*
                        if (size >= 1 /* D3 */ + 2 /* length */ + y /* length includes the message ID */ ) {
                            // Check the length, allowing the CRC bytes to be omitted
                            pDec->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
            }
            if (pDec->errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                // Got a known protocol, an ID and a valid length, see if we have
                // a decoder for this message ID
                pDec->errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                for (x = 0; (pFunction == NULL) && (x < sizeof(gIdList) / sizeof(gIdList[0])); x++) {
                    if (uGnssMsgIdIsWanted(&(pDec->id), (uGnssMessageId_t *) & (gIdList[x]))) {
                        pFunction = gpFunctionList[x];
                    }
                }
                if (pFunction != NULL) {
                    // Found a matching decoder, run it
                    pDec->errorCode = pFunction(pBuffer, size, &(pDec->pBody));
                }
            }
            if ((pDec->errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                (gpCallback != NULL)) {
                // Couldn't decode the message: let the user callback try
                pDec->errorCode = gpCallback(&(pDec->id), pBuffer, size, &(pDec->pBody), gpCallbackParam);
            }
        }
    }

    return pDec;
}

// Free the memory returned by pUGnssDecAlloc().
void uGnssDecFree(uGnssDec_t *pDec)
{
    if (pDec != NULL) {
        uPortFree(pDec->pBody);
        uPortFree(pDec);
    }
}

// Get the list of message IDs that pUGnssDecAlloc() can decode.
int32_t uGnssDecGetIdList(const uGnssMessageId_t **ppIdList)
{
    if (ppIdList != NULL) {
        *ppIdList = gIdList;
    }

    return sizeof(gIdList) / sizeof(gIdList[0]);
}

// Add a custom decoder.
void uGnssDecSetCallback(uGnssDecFunction_t *pCallback,
                         void *pCallbackParam)
{
    gpCallback = pCallback;
    gpCallbackParam = pCallbackParam;
}

// End of file
