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

#ifndef _U_GNSS_DEC_H_
#define _U_GNSS_DEC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_gnss_dec_ubx_nav_pvt.h"
#include "u_gnss_dec_ubx_nav_hpposllh.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines an API to decode messages
 * from a GNSS chip.  Only a useful subset of messages are
 * supported.  Use this if you wish to access the detailed contents
 * of, for instance, a UBX-NAV-PVT message, or if you wish
 * to obtain high precision position from a HPG GNSS device
 * by requesting it to emit the UBX-NAV-HPPOSLLH message.
 *
 * The functions are thread-safe with the exception of
 * uGnssDecSetCallback().
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Union of all decodable message structures.
 */
typedef union {
    uGnssDecUbxNavPvt_t           ubxNavPvt;      /**< UBX-NAV-PVT. */
    uGnssDecUbxNavHpposllh_t      ubxNavHpposllh; /**< UBX-NAV-HPPOSLLH. */
} uGnssDecUnion_t;

/** The result of attempting to decode a message, returned by
 * pUGnssDecAlloc().
 */
typedef struct {
    int32_t errorCode;   /**< the outcome of message decoding:
                              zero on complete success, for example
                              #U_ERROR_COMMON_UNKNOWN if the message
                              was not known to this code,
                              # U_ERROR_COMMON_NOT_SUPPORTED if
                              the message was known but decoding
                              of it is not supported,
                              #U_ERROR_COMMON_TRUNCATED if the message
                              was incomplete, #U_ERROR_COMMON_BAD_DATA
                              if a decode was made (so pBody will be
                              non-NULL) but one or more fields
                              were out of range or an expected field
                              was not present,
                              #U_ERROR_COMMON_NO_MEMORY if no memory
                              could be allocated for the message body. */
    uGnssMessageId_t id; /**< the message ID; the type field will be
                              set to #U_GNSS_PROTOCOL_UNKNOWN if the
                              ID could not be determined. */
    char nmea[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1]; /** a place
                                                                    to store
                                                                    the NMEA
                                                                    message ID
                                                                    if id
                                                                    happens to
                                                                    be NMEA. */
    uGnssDecUnion_t *pBody; /**< a pointer to the decoded message body,
                                 NULL if the message body could not
                                 be decoded. */
} uGnssDec_t;

/** Callback that can be hooked into pUGnssDecAlloc() by
 * uGnssDecSetCallback() to decode message types that are not
 * known to this code.
 *
 * @param[in,out] pId             a pointer to the message ID, which
 *                                will have already been decoded for
 *                                #U_GNSS_PROTOCOL_UBX, #U_GNSS_PROTOCOL_NMEA
 *                                and #U_GNSS_PROTOCOL_RTCM, will
 *                                never be NULL; in the unlikely case
 *                                that the protocol type is set to
 *                                #U_GNSS_PROTOCOL_UNKNOWN and the
 *                                callback _is_ able to decode the
 *                                mesage it should populate the
 *                                fields with the truth as far as
 *                                it is concerned.
 *                                Note: if the callback decodes
 *                                an NMEA message and populates
 *                                this field with the NMEA ID, the
 *                                string that pNmea ends up pointing-to
 *                                MUST be a true constant, not from
 *                                allocated memory or on the stack
 *                                (unless the caller has devised a
 *                                scheme to provide non-transient
 *                                storage by clever use of
 *                                pCallbackParam).
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
 * @param[in,out] pCallbackParam  the pCallbackParam pointer that
 *                                was passed to uGnssDecSetCallback().
 * @return                        zero on a successful decode, else
 *                                negative error code, preferably
 *                                from the set suggested for the
 *                                errorCode field of #uGnssDec_t.
 */
typedef int32_t (uGnssDecFunction_t) (uGnssMessageId_t *pId,
                                      const char *pBuffer,
                                      size_t size,
                                      uGnssDecUnion_t **ppBody,
                                      void *pCallbackParam);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Decode a message buffer received from a GNSS device, for example
 * from uGnssMsgReceive() or the callback of uGnssMsgReceiveStart().
 * The message must be well formed, must begin at the start of pBuffer
 * and must include all headers; no checking of checksums etc. on the
 * end of a known message is performed, hence they may be omitted.
 *
 * Currently only a very limited set of messages (actually just
 * UBX-NAV-PVT and UBX-NAV-HPPOSLLH, the latter useful if you wish
 * to use a high precision GNSS (HPG) device to its full extent) are
 * supported; see the top of the file u_gnss_dec.c for instructions
 * on how to add more decoders, or use uGnssDecSetCallback() to
 * hook-in your own decoders at run-time.
 *
 * If only a partial decode is possible then the errorCode field of
 * the returned structure will be negative but the protocol type
 * and a message ID may _still_ have been decoded; check for
 * the id.type field of the returned structure being something other
 * than #U_GNSS_PROTOCOL_UNKNOWN.
 *
 * IMPORTANT: this function will *always* allocate memory for the
 * returned message structure, even in a fail case; it is up to the
 * caller to uGnssDecFree() the pointer when done (and it is always
 * safe to do so, even if the pointer is NULL).
 *
 * @param[in] pBuffer     the buffer containing the message to be
 *                        decoded; cannot be NULL.
 * @param size            the amount of data at pBuffer.
 * @return                on success a pointer to the decoded
 *                        message, else NULL.
 */
uGnssDec_t *pUGnssDecAlloc(const char *pBuffer, size_t size);

/** Free the memory returned by pUGnssDecAlloc().
 *
 * @param[in] pDec the pointer returned by pUGnssDecAlloc(); may
 *                 be NULL.
 */
void uGnssDecFree(uGnssDec_t *pDec);

/** Get the list of message IDs that pUGnssDecAlloc() can decode;
 * does not include any added by uGnssDecSetCallback().
 *
 * @param[out] ppIdList  a pointer to a place to put the pointer
 *                       to the message ID list; may be NULL just
 *                       to obtain the number of IDs.
 * @return               the number of items at *ppIdList, or which
 *                       would be at *ppIdList if it were not NULL.
 */
int32_t uGnssDecGetIdList(const uGnssMessageId_t **ppIdList);

/** If you wish to decode a message type that is not known
 * by this code then you may use this to hook your own decoder
 * onto the end of pUGnssDecAlloc(); a single, global callback.
 * uGnssDecSetCallback() should not be called while
 * pUGnssDecAlloc() may be acting.
 *
 * Note that the callback is called only after the built-in
 * decoders have all failed to work, hence it cannot override
 * them.
 *
 * @param[in] pCallback      your message decode callback, use
 *                           NULL to remove an existing callback.
 * @param[in] pCallbackParam will be passed to pCallback as its
 *                           last parameter; may be NULL.
 */
void uGnssDecSetCallback(uGnssDecFunction_t *pCallback,
                         void *pCallbackParam);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_DEC_H_

// End of file
