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

#ifndef _U_GNSS_UTIL_H_
#define _U_GNSS_UTIL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_ringbuffer.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the utility functions of the GNSS API.
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

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Send a command of your choosing to the GNSS chip and, optionally,
 * wait for the response; THIS FUNCTION SHOULD ONLY BE USED IF
 * YOUR GNSS CHIP IS CONNECTED VIA AN INTERMEDIATE (e.g. CELLULAR)
 * MODULE.  While it _will_ work for the directly-connected case
 * for sending, for the receive direction, for the UART/I2C
 * transport case, it internally pulls data directly from the UART/I2C
 * port layer rather than going via the ring buffer and hence could
 * end up reading and potentially discarding data that other bits
 * of this system probably wanted.  So if your GNSS device is connected
 * directly to this MCU using a streaming transport (e.g. UART or I2C)
 * you should use uGnssMsgSend() in conjunction with uGnssMsgReceive()
 * or uGnssMsgReceiveStart() instead.
 *
 * You must encode the message correctly (e.g. using the encode/decode
 * functions of the UBX protocol API if you are using UBX format).
 * Since this code knows nothing of the outgoing message format,
 * it can know nothing of the response format either, so pResponse
 * is just the stream of output from the GNSS chip that came after
 * the sent message.  It is up to you to pick the exact response
 * message out of the stream and parse it; if you are only using UBX
 * format messages you may like to call uGnssCfgSetProtocolOut() to
 * filter out NMEA messages.
 *
 * IMPORTANT: when the GNSS chip is connected via an intermediate
 * [e.g. cellular] module (i.e. you are using #U_GNSS_TRANSPORT_AT)
 * then responses will only be returned by this function if UBX FORMAT
 * is used; that is why this function has "Ubx" in the name. However,
 * the message contents are not touched by this code and hence could
 * be anything at all *except* that in the case of the AT transport
 * the intermediate AT (e.g. cellular) module will overwrite the
 * last two bytes of the message with a UBX message checksum.
 * Hence, you _can_ send a non-UBX-format message transparently
 * to the GNSS chip with this function when using
 * #U_GNSS_TRANSPORT_AT but be sure to add two dummy bytes
 * to the outgoing message buffer.
 *
 * It is planned, in future, to make transport via an intermediate
 * cellular module work in the same way as the UART and I2C streaming
 * interfaces (by implementing support for 3GPP TS 27.010 +CMUX in this
 * code), at which point this function will be deprecated.
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param[in] pCommand           the command to send; may be NULL.
 * @param commandLengthBytes     the amount of data at pCommand; must
 *                               be non-zero if pCommand is non-NULL.
 * @param[out] pResponse         a pointer to somewhere to store the
 *                               response, if one is expected; may
 *                               be NULL.
 * @param maxResponseLengthBytes the amount of storage at pResponse;
 *                               must be non-zero if pResponse is non-NULL.
 * @return                       on success the number of bytes copied
 *                               into pResponse (zero if pResponse is
 *                               NULL), else negative error code.
 */
int32_t uGnssUtilUbxTransparentSendReceive(uDeviceHandle_t gnssHandle,
                                           const char *pCommand,
                                           size_t commandLengthBytes,
                                           char *pResponse,
                                           size_t maxResponseLengthBytes);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_UTIL_H_

// End of file
