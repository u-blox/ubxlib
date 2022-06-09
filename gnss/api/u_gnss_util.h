/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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

/** Send a ubx command of your choosing to the GNSS chip and,
 * optionally, wait for the response.  You must encode the
 * message correctly (e.g. using the encode/decode functions
 * of the ubx API).  If you are expecting a response then it
 * is up to you to parse that response; in particular, if you
 * are sending a ubx format message and the transport type is
 * U_GNSS_TRANSPORT_NMEA_UART then you will need to pick the
 * ubx message out from any NMEA data that happens to be
 * emitted by the GNSS chip at around the same time.  Given
 * the asynchronous nature of NMEA transmission you may
 * prefer to set the transport type to U_GNSS_TRANSPORT_UBX_UART.
 *
 * Note: the message contents are not touched by this code and
 * hence could be anything at all *except* that in the case of
 * the AT transport the intermediate AT (e.g. cellular) module
 * will overwrite the last two bytes of the message with a
 * ubx message checksum.  Hence, to send a non-ubx message
 * transparently to the GNSS chip in this case, you should add
 * two dummy bytes to the message.
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param pCommand               the command to send; may be NULL.
 * @param commandLengthBytes     the amount of data at pCommand; must
 *                               be non-zero if pCommand is non-NULL.
 * @param pResponse              a pointer to somewhere to store the
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

#endif // _U_GNSS_UTIL_H_

// End of file
