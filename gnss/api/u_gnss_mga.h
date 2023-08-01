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

#ifndef _U_GNSS_MGA_H_
#define _U_GNSS_MGA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines functions related to Multiple GNSS
 * Assistance, a u-blox feature which improves the time to first fix
 * (TTFF), which can otherwise be around 30 seconds even in good
 * conditions.
 *
 * To improve TTFF, this MCU can download a few sorts of information to
 * the GNSS module:
 *
 * a) ephemeris data: precise data concerning the orbits of satellites,
 *    valid for a few hours into the future, available from a u-blox
 *    assistance server.
 * b) almanac data: approximate information concerning the orbits of
 *    satellites, valid for up to a few weeks in the future, available
 *    from a u-blox assistance server,
 * c) the current time and approximate current position of the GNSS module,
 *    if not already available in the GNSS module through RTC/battery-backup.
 *
 * In addition, a standard precision u-blox GNSS module can estimate the
 * almanac data by itself (so no connection to a server is required) to
 * achieve an improved TTFF, the data being valid for longer then (a) but
 * shorter than (b), at a penalty of slightly increased power consumption
 * in the GNSS module; let's call this (d).
 *
 * The shortest TTFF (e.g. a few seconds) is achieved if (a) is available,
 * then (e.g. 10 seconds) if (b) is available, then if (d) is switched on.
 * Making (c) available will improve TTFF in all cases and should always
 * be provided first as the assistance information may not be usable without
 * the current time.  (b) is useful in cases where internet connectivity is
 * sporadic.
 *
 * Assistance data can be requested from the u-blox assistance server in
 * two modes:
 *
 * - AssistNow Online: provides the current time, ephemeris and optionally
 *   almanac data.
 * - AssistNow Offline: provides data for up to 5 weeks in advance, hence
 *   the amount of data can be large (e.g. 10 kbytes per week versus
 *   3.5 kbytes total for the online case).
 *
 * In both cases the response is in the form of UBX messages that can be
 * sent directly to the GNSS module.  In the offline case, the data can be
 * stored by this MCU or in flash memory connected to the GNSS module.
 * Communication with the u-blox AssistNow servers is via an HTTP GET request
 * from this MCU; the response will arrive in a single HTTP GET response.
 *
 * Finally, before a GNSS module is powered off, it is possible to read the
 * current assistance database such that it can be restored when the
 * module is powered on again (for the case where there is no flash storage
 * on the GNSS module or battery backup).
 *
 * IMPORTANT: if the GNSS module is connected via an intermediate (e.g. cellular)
 * u-blox module, all of the above can be carried out by the intermediate
 * module instead; no actions by this MCU are required and hence you do not
 * need this API: please use the uCellLoc API instead.
 *
 * IMPLEMENTATION NOTE: the AssistNow response consists of many binary
 * messages designed to be sent directly to the GNSS chip.  Forwarding the
 * messages to the GNSS device _will_ require a relatively large amount of
 * heap memory, as will holding the body of the complete single HTTP GET
 * response body from the server; if this causes a problem then please let
 * us know and we will look into optimisation measures.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The usual name of the AssistNow Online server.
 */
#define U_GNSS_MGA_HTTP_SERVER_ONLINE "online-live1.services.u-blox.com"

/** The usual name of the AssistNow Offline server.
 */
#define U_GNSS_MGA_HTTP_SERVER_OFFLINE "offline-live1.services.u-blox.com"

#ifndef U_GNSS_MGA_INTER_MESSAGE_DELAY_MS
/** A delay to add between messages sent to the GNSS module when
 * using uGnssMgaResponseSend() and uGnssMgaSetDatabase() with
 * #U_GNSS_MGA_FLOW_CONTROL_WAIT flow control and in the initial batch
 * of messages sent when using #U_GNSS_MGA_FLOW_CONTROL_SMART.
 */
# define U_GNSS_MGA_INTER_MESSAGE_DELAY_MS 10
#endif

#ifndef U_GNSS_MGA_MESSAGE_TIMEOUT_MS
/** How long to wait for an acknowledgement before a message is assumed
 * to be nacked by the GNSS device; used only by uGnssMgaResponseSend().
 */
# define U_GNSS_MGA_MESSAGE_TIMEOUT_MS 5000
#endif

#ifndef U_GNSS_MGA_MESSAGE_RETRIES
/** How many times to retry sending a message before it is considered
 * failed; used only by uGnssMgaResponseSend().
 */
# define U_GNSS_MGA_MESSAGE_RETRIES 3
#endif

#ifndef U_GNSS_MGA_POLL_TIMER_MS
/** How long to wait between polls for timed-out messages in milliseconds.
 */
# define U_GNSS_MGA_POLL_TIMER_MS 1000
#endif

#ifndef U_GNSS_MGA_DATABASE_READ_TIMEOUT_MS
/** How long to wait for a navigation database read to complete
 * in milliseconds.
 */
# define U_GNSS_MGA_DATABASE_READ_TIMEOUT_MS 30000
#endif

#ifndef U_GNSS_MGA_RX_BUFFER_SIZE_BYTES
/** The size of the GNSS chip's internal receive buffer, used when
 * employing smart flow control.
 */
# define U_GNSS_MGA_RX_BUFFER_SIZE_BYTES 1000
#endif

/** The maximum length of the payload of a UBX-MGA-DBD message; for
 * the avoidance of doubt, this does NOT include the two length
 * indicator bytes that precede it, i.e. the maximum length passed
 * to uGnssMgaDatabaseCallback_t is two more than this.
 *
 * Note: the GNSS interface manual says that this value will not be
 * greater than 164 bytes but, by experiment, the last value returned
 * by the GNSS device is sometimes larger: 184 and 248 bytes have
 * both been observed, for M10 and M9 respectively, hence we set the
 * larger limit here for safety sake.
 */
#define U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES 248

#ifndef U_GNSS_MGA_ONLINE_REQUEST_DEFAULTS
/** Default values for #uGnssMgaOnlineRequest_t.
 */
# define U_GNSS_MGA_ONLINE_REQUEST_DEFAULTS {NULL, 0, 0, NULL, 0, 0}
#endif

#ifndef U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS
/** Default values for #uGnssMgaOfflineRequest_t.
 */
# define U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS {NULL, false, 1UL << U_GNSS_SYSTEM_GPS, 1, 1}
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible data types to request for AssistNow Online; values
 * designed to be as encoded into the JSON message to the server.
 */
typedef enum {
    U_GNSS_MGA_DATA_TYPE_EPHEMERIS = 0,
    U_GNSS_MGA_DATA_TYPE_ALMANAC = 1,
    U_GNSS_MGA_DATA_TYPE_AUX = 2,
    U_GNSS_MGA_DATA_TYPE_POS = 3,
    U_GNSS_MGA_DATA_TYPE_MAX_NUM
} uGnssMgaDataType_t;

/** The reference signal to use for time-initialisation of a GNSS
 * module, values such that they can be used directly in a
 * UBX-MGA-INI-TIME_UTC message.
 */
typedef enum {
    U_GNSS_MGA_EXT_INT_NONE = 0,
    U_GNSS_MGA_EXT_INT_0 = 1,
    U_GNSS_MGA_EXT_INT_1 = 2
} uGnssMgaExtInt_t;

/** The possible flow control types, used by uGnssMgaResponseSend() and
 * uGnssMgaSetDatabase().  Developer note: these values are used internally
 * to index into an array.
 */
typedef enum {
    U_GNSS_MGA_FLOW_CONTROL_SIMPLE = 0,  /**< wait for an ACK for each message; reliable but slow. */
    U_GNSS_MGA_FLOW_CONTROL_WAIT = 1,    /**< wait for #U_GNSS_MGA_INTER_MESSAGE_DELAY_MS between
                                              messages; fast but may not be completely reliable. */
    U_GNSS_MGA_FLOW_CONTROL_SMART = 2,   /**< send a burst of messages that will fit into the GNSS
                                              chip's RX buffer with #U_GNSS_MGA_INTER_MESSAGE_DELAY_MS,
                                              then wait for ACKs; a compromise in terms of
                                              speed/reliability. */
    U_GNSS_MGA_FLOW_CONTROL_MAX_NUM
} uGnssMgaFlowControl_t;

/** The kind of "send" operation to do for AssistNow Offline data, used by uGnssMgaResponseSend().
 */
typedef enum {
    U_GNSS_MGA_SEND_OFFLINE_ALL,     /**< send all of the offline data to the GNSS device,
                                          no filtering. */
    U_GNSS_MGA_SEND_OFFLINE_FLASH,   /**< as #U_GNSS_MGA_SEND_OFFLINE_ALL but also ask
                                          the GNSS device to store the offline data in
                                          flash memory, in which case the GNSS module
                                          will be able to use it automatically at power-on
                                          without the need for uGnssMgaSetDatabase(); only
                                          useful if the GNSS device has [sufficient] spare
                                          flash memory available. */
    U_GNSS_MGA_SEND_OFFLINE_TODAYS,  /**< send just the offline data for today to the GNSS
                                          device, i.e. filter the data, where "today" is
                                          with reference to the timeUtcMilliseconds parameter
                                          passed to uGnssMgaResponseSend().  This is useful
                                          if you have downloaded many days of offline data
                                          and stored it in the MCU (for example if your GNSS device
                                          has no available flash storage) and you want to just
                                          provide the GNSS chip with the minimum necessary data.
                                          Note that the almanac data is ALSO sent, there is
                                          no need to do #U_GNSS_MGA_SEND_OFFLINE_ALMANAC
                                          as well. */
    U_GNSS_MGA_SEND_OFFLINE_ALMANAC, /**< send just almanac data to the GNSS device, for example
                                          filter the data; useful for a similar reason to
                                          #U_GNSS_MGA_SEND_OFFLINE_TODAYS. */
    U_GNSS_MGA_SEND_OFFLINE_MAX_NUM,
    U_GNSS_MGA_SEND_OFFLINE_NONE     /**< kind of a "null" entry that can be used if
                                          uGnssMgaResponseSend() is being used to send
                                          AssistNow _online_ data; not that you have to, since
                                          this parameter will be ignored then in any case. */
} uGnssMgaSendOfflineOperation_t;

/** The reference point for time-initialisation of a GNSS module.
 */
typedef struct {
    uGnssMgaExtInt_t extInt;
    bool fallingNotRising;
    bool lastNotNext;
}  uGnssMgaTimeReference_t;

/** The approximate position, used when initialising a GNSS module
 * (and optionally in an AssistNow Offline request).
 */
typedef struct {
    int32_t latitudeX1e7;
    int32_t longitudeX1e7;
    int32_t altitudeMillimetres;
    int32_t radiusMillimetres;
} uGnssMgaPos_t;

/** A structure that defines an AssistNow Online request.
 *
 * If this structure is modified, please also modify
 * #U_GNSS_MGA_ONLINE_REQUEST_DEFAULTS to match.
 */
typedef struct {
    const char *pTokenStr;               /**< a pointer to a null-terminated
                                              authentication token to encode;
                                              an evaluation token may be
                                              obtained from
                                              https://www.u-blox.com/en/assistnow-service-evaluation-token-request-form
                                              or from your Thingstream portal
                                              https://portal.thingstream.io/app/location-services.
                                              Cannot be NULL. */
    uint32_t dataTypeBitMap;             /**< a bit-map of the data types that
                                              are requested, chosen from
                                              #uGnssMgaDataType_t, where each
                                              data type is represented by its
                                              bit position; for example set
                                              bit 0 to one for ephemeris data. */
    uint32_t systemBitMap;               /**< a bit-map of the GNSS systems
                                              that data should be requested for,
                                              chosen from #uGnssSystem_t, where
                                              each system is represented by its
                                              bit-position (for example set bit
                                              0 to one for GPS).  Not all systems
                                              are supported (see the latest u-blox
                                              AssistNow service description for
                                              which are supported). If no systems
                                              are specified the time alone will be
                                              returned by the server. */
    const uGnssMgaPos_t *pMgaPosFilter;  /**< the approximate current position of
                                              the GNSS module; leave as NULL to not
                                              have the AssistNow Online request
                                              filtered on position. */
    int32_t latencyMilliseconds;         /**< the expected round-trip time for the
                                              AssistNow Online request in
                                              milliseconds; this is necessary so
                                              that the server can correct the
                                              absolute time which it sends back as
                                              the first message in the response. */
    int32_t latencyAccuracyMilliseconds; /**< the accuracy of latencyMilliseconds
                                              in milliseconds. */
} uGnssMgaOnlineRequest_t;

/** A structure that defines an AssistNow Offline request.
 *
 * If this structure is modified, please also modify
 * #U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS to match.
 */
typedef struct {
    const char *pTokenStr;        /**< a pointer to a null-terminated
                                       authentication token to encode;
                                       an evaluation token may be
                                       obtained from
                                       https://www.u-blox.com/en/assistnow-service-evaluation-token-request-form
                                       or from your Thingstream portal
                                       https://portal.thingstream.io/app/location-services.
                                       Cannot be NULL. */
    bool almanacDataAlso;         /**< if set to true then the almanac
                                       data that would be downloaded by
                                       AssistNow Online is also requested. */
    uint32_t systemBitMap;        /**< a bit-map of the GNSS systems
                                       that data should be requested for,
                                       chosen from #uGnssSystem_t, where
                                       each system is represented by its
                                       bit-position (for example set bit
                                       0 to one for GPS).  Not all systems
                                       are supported (see the latest u-blox
                                       AssistNow service description for
                                       which are supported). At least one
                                       system must be specified or the server
                                       will return an error. */
    int32_t periodDays;           /**< the number of days for which almanac
                                       data is required; note that the size
                                       of the response returned by the server
                                       may increase by between 5 and 10 kbytes
                                       per day requested. */
    int32_t daysBetweenItems;     /**< the number of days between items: 1 for
                                       every day, 2 for one every two days or
                                       3 for one every 3 days. */
} uGnssMgaOfflineRequest_t;

/** Callback that will be called while uGnssMgaResponseSend() or
 * uGnssMgaSetDatabase() is running.  Do NOT call into the GNSS API from
 * this callback as the API will already be locked and you will get stuck.
 *
 * @param devHandle               the device handle.
 * @param errorCode               zero if the transfer is continuing
 *                                successfully, else negative error code.
 * @param blocksTotal             the number of data blocks that must be
 *                                sent to the GNSS device.
 * @param blocksSent              the number of data blocks successfully
 *                                sent to the GNSS device so far.
 * @param[in,out] pCallbackParam  the pCallbackParam pointer that
 *                                was passed to uGnssMgaResponseSend()
 *                                or uGnssMgaSetDatabase().
 * @return                        true to continue with the transfer,
 *                                false to terminate it.
 */
typedef bool (uGnssMgaProgressCallback_t)(uDeviceHandle_t devHandle,
                                          int32_t errorCode,
                                          size_t blocksTotal, size_t blocksSent,
                                          void *pCallbackParam);

/** Callback that will be called by uGnssMgaGetDatabase() when the
 * navigation database is being read from the GNSS device.  Do NOT call
 * into the GNSS API from this callback as the API will already be
 * locked and you will get stuck.  It is important that this function
 * returns quickly as there is no way to flow-control the data arriving
 * from the GNSS chip.
 *
 * @param devHandle               the device handle.
 * @param pBuffer                 the buffer of data that must be stored,
 *                                contigiuously, with any previous data.
 * @param size                    the number of bytes at pBuffer.
 * @param[in,out] pCallbackParam  the pCallbackParam pointer that
 *                                was passed to uGnssMgaGetDatabase().
 * @return                        true to continue with the transfer,
 *                                false to terminate it.
 */
typedef bool (uGnssMgaDatabaseCallback_t) (uDeviceHandle_t devHandle,
                                           const char *pBuffer, size_t size,
                                           void *pCallbackParam);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Encode an AssistNow Online request body suitable for sending via
 * an HTTP GET request to a u-blox assistance server (for example
 * online-live1.services.u-blox.com).  This does NOT send the HTTP
 * request: you must do that, e.g. using the uHttpClient API.  This
 * function is designed such that the buffer size may be determined
 * by calling it with pBuffer set to NULL; a buffer of the exact size
 * may then be allocated and the function called once more with the
 * real buffer.
 *
 * IMPORTANT: the value returned by this function does NOT include
 * the null terminator, which WILL be added; i.e. you get back
 * strlen(pBuffer).  If you call this function with NULL for pBuffer
 * and then allocate a buffer, you must add one, e.g.:
 *
 * ```
 * char *pBuffer;
 * int32_t x = uGnssMgaOnlineRequestEncode(pRequest, NULL, 0);
 * if (x >= 0) {
 *     x++; // Add room for terminator
 *     pBuffer = malloc(x);
 *     if (pBuffer != NULL) {
 *         x = uGnssMgaOnlineRequestEncode(pRequest, pBuffer, x);
 *         ...
 *         free(pBuffer);
 *     }
 * }
 * ```
 *
 * When you have sent this encoded HTTP GET request to the u-blox
 * assistance server #U_GNSS_MGA_HTTP_SERVER_ONLINE, e.g. using the
 * uHttpClient API, and received the response, you may forward the
 * response body to the GNSS module by calling uGnssMgaResponseSend().
 *
 * Tip: if the service returns an HTTP error code you might look at the
 * returned string anyway as it may contain an explanation of what the
 * server doesn't like (e.g. an unsupported system type).  That, or send
 * the same query string from a browser and look at the response there.
 *
 * @param[in] pRequest  a pointer to the request to encode; for
 *                      forwards compatability, please ensure that this
 *                      is assigned to
 *                      #U_GNSS_MGA_ONLINE_REQUEST_DEFAULTS initially and
 *                      then modified as appropriate before being
 *                      passed here.  Cannot be NULL.
 * @param[out] pBuffer  the buffer to encode the request into; use NULL
 *                      to not perform the encode but instead simply
 *                      return the number of bytes encoded string; a null
 *                      terminator IS included.
 * @param size          the number of bytes of storage at pBuffer; must
 *                      be 0 if pBuffer is NULL.
 * @return              the number of bytes encoded, or that would be
 *                      encoded if pBuffer were not NULL, NOT including
 *                      the null terminator, else negative
 *                      error code.
 */
int32_t uGnssMgaOnlineRequestEncode(const uGnssMgaOnlineRequest_t *pRequest,
                                    char *pBuffer, size_t size);

/** Encode an AssistNow Offline request body suitable for sending via an
 * HTTP GET request to a u-blox assistance server (for example
 * offline-live1.services.u-blox.com).  This does NOT send the HTTP
 * request: you must do that, e.g. using the uHttpClient API.  This
 * function is designed such that the buffer size may be determined
 * by calling it with pBuffer set to NULL to obtain the length of
 * the string that would be encoded; a buffer of the exact size
 * may then be allocated and the function called once more with the
 * real buffer.
 *
 * IMPORTANT: the value returned by this function does NOT include
 * the null terminator, which WILL be added; i.e. you get back
 * strlen(pBuffer).  If you call this function with NULL for pBuffer
 * and then allocate a buffer, you must add one, e.g.:
 *
 * ```
 * char *pBuffer;
 * int32_t x = uGnssMgaOfflineRequestEncode(pRequest, NULL, 0);
 * if (x >= 0) {
 *     x++; // Add room for terminator
 *     pBuffer = malloc(x);
 *     if (pBuffer != NULL) {
 *         x = uGnssMgaOfflineRequestEncode(pRequest, pBuffer, x);
 *         ...
 *         free(pBuffer);
 *     }
 * }
 * ```
 *
 * TLS: when you send the HTTP request, the AssistNow Offline server
 * REQUIRES that the Server Name Indication (pSni) field is set [to the
 * same server name]; security will fail otherwise.
 *
 * When you have sent this encoded HTTP GET request to the u-blox
 * assistance server #U_GNSS_MGA_HTTP_SERVER_OFFLINE, e.g. using the
 * uHttpClient API, and received the response, you may forward the
 * response body to the GNSS module by calling uGnssMgaResponseSend().
 *
 * Tip: if the service returns an HTTP error code you might look at the
 * returned string anyway as it may contain an explanation of what the
 * server doesn't like (e.g. an unsupported system type).  That, or send
 * the same query string from a browser and look at the response there.
 *
 * @param[in] pRequest  a pointer to the request to encode; for
 *                      forwards compatability, please ensure that this
 *                      is assigned to
 *                      #U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS initially and
 *                      then modified as appropriate before being
 *                      passed here.  Cannot be NULL.
 * @param[out] pBuffer  the buffer to encode the request into; use NULL
 *                      to not perform the encode but instead simply
 *                      return the number of bytes that would be
 *                      required to store the encoded string; a null
 *                      terminator IS included.
 * @param size          the number of bytes of storage at pBuffer; must be
 *                      0 if pBuffer is NULL.
 * @return              the number of bytes encoded, or that would be
 *                      encoded if pBuffer were not NULL, NOT including
 *                      the null terminator, else negative error code.
 */
int32_t uGnssMgaOfflineRequestEncode(const uGnssMgaOfflineRequest_t *pRequest,
                                     char *pBuffer, size_t size);

/** Initialise the GNSS module with the approximate time.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()) or, even better, where GNSS
 * is connected via a cellular module, allow the cellular module to
 * do all the heavy lifting by using the uCellLoc API instead of this
 * one.
 *
 * @param gnssHandle                 the handle of the GNSS instance.
 * @param timeUtcNanoseconds         the current UTC Unix time, i.e. NOT
 *                                   including leap seconds, in nanoseconds.
 * @param timeUtcAccuracyNanoseconds the accuracy of timeUtcNanoseconds in
 *                                   nanoseconds.
 * @param[in] pReference             a pointer to the reference that the GNSS
 *                                   module should use for this time
 *                                   initialisation; use NULL for no
 *                                   particular timing reference.
 * @return                           zero on success else negative error code.
 */
int32_t uGnssMgaIniTimeSend(uDeviceHandle_t gnssHandle,
                            int64_t timeUtcNanoseconds,
                            int64_t timeUtcAccuracyNanoseconds,
                            uGnssMgaTimeReference_t *pReference);

/** Initialise the GNSS module with an approximate position.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()) or, even better, where GNSS
 * is connected via a cellular module, allow the cellular module to
 * do all the heavy lifting by using the uCellLoc API instead of this
 * one.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param[in] pMgaPos a pointer to the approximate position; cannot
 *                    be NULL.
 * @return            zero on success else negative error code.
 */
int32_t uGnssMgaIniPosSend(uDeviceHandle_t gnssHandle,
                           const uGnssMgaPos_t *pMgaPos);

/** Send the body of an HTTP GET response received from a u-blox
 * assistance server (as a result of an AssistNow Online or an
 * AssistNow Offline request) to a GNSS device.  The complete
 * HTTP GET response body must be sent; truncated responses will
 * likely be rejected by the GNSS device.
 *
 * Note: this uses one of the #U_GNSS_MSG_RECEIVER_MAX_NUM message
 * handles from the uGnssMsg API.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()) or, even better, where GNSS
 * is connected via a cellular module, allow the cellular module to
 * do all the heavy lifting by using the uCellLoc API instead of this
 * one.
 *
 * Note: if flow control is set to a value other than
 * #U_GNSS_MGA_FLOW_CONTROL_WAIT or if writing to flash is involved
 * (#U_GNSS_MGA_SEND_OFFLINE_FLASH) then, in order to speed up this process
 * when waiting for Acks, NMEA messages from the GNSS chip are temporarily
 * disabled.  You can disable the disabling by defining the conditional
 * compilation flag U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE.
 *
 * @param gnssHandle                     the handle of the GNSS instance.
 * @param timeUtcMilliseconds            the current UTC Unix time, NOT including
 *                                       leap seconds, in milliseconds: MUST be set if
 *                                       pBuffer contains a response to an AssistNow
 *                                       Offline request (that is not just being
 *                                       written to flash) for the data to be useful,
 *                                       unless you set the onlineDataAlso field of
 *                                       #uGnssMgaOfflineRequest_t to true (because
 *                                       in that case the server will have already added
 *                                       the current time to the front of the downloaded
 *                                       data); if this is set for an AssistNow Online
 *                                       request then the time received from the server
 *                                       is replaced with this time; use -1 if the time
 *                                       is not known.
 * @param timeUtcAccuracyMilliseconds    the accuracy of timeUtcMilliseconds in
 *                                       milliseconds.
 * @param offlineOperation               if pBuffer contains the response to an
 *                                       AssistNow Offline request then use this to chose
 *                                       the kind of operation to perform; ignored if pBuffer
 *                                       contains the response to an AssistNow Online
 *                                       request.
 * @param flowControl                    the type of flow control to use.
 * @param[in] pBuffer                    a buffer containing the body of an HTTP GET
 *                                       response from a u-blox assistance server;
 *                                       cannot be NULL.
 * @param size                           the amount of data at pBuffer; must be greater
 *                                       than zero.
 * @param[in] pCallback                  a function which will be called at regular
 *                                       intervals while sending the response, to
 *                                       track progress, and must return true for the
 *                                       transfer to continue; if false is returned
 *                                       then the transfer will be cancelled.  May
 *                                       be NULL.  Do NOT call into the GNSS API from
 *                                       this callback as the API will already be
 *                                       locked and you will get stuck.
 * @param[in,out] pCallbackParam         parameter that will be passed to pCallback as
 *                                       its last parameter.
 * @return                               zero on success else negative error code.
 */
int32_t uGnssMgaResponseSend(uDeviceHandle_t gnssHandle,
                             int64_t timeUtcMilliseconds,
                             int64_t timeUtcAccuracyMilliseconds,
                             uGnssMgaSendOfflineOperation_t offlineOperation,
                             uGnssMgaFlowControl_t flowControl,
                             const char *pBuffer, size_t size,
                             uGnssMgaProgressCallback_t *pCallback,
                             void *pCallbackParam);

/** Erase the flash memory attached to a GNSS chip in which the
 * assistance data is stored; normally there should be no reason
 * to use this since any new assistance data written to the GNSS
 * chip flash with uGnssMgaResponseSend() will overwrite whatever
 * is already there.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             zero on success else negative error code.
 */
int32_t uGnssMgaErase(uDeviceHandle_t gnssHandle);

/** Get whether AssistNow Autonomous operation is on or off.
 * Note that for M9 modules and later this can also be read
 * using the configuration key ID #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L.
 * If the GNSS chip is inside or is connected via a cellular module
 * then use uCellLocAssistNowAutonomousIsOn() instead (see u_cell_loc.h)
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            true if AssistNow Autonomous operation is
 *                    on, else false.
 */
bool uGnssMgaAutonomousIsOn(uDeviceHandle_t gnssHandle);

/** Set AssistNow Autonomous operation on or off.  Note that for M9
 * modules and later this can also be set/get using the configuration
 * key ID #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L.  Switching
 * AssistNow Automomous to ON is only supported on standard precision
 * GNSS devices.  If the GNSS chip is inside or is connected via a
 * cellular module then use uCellLocSetAssistNowAutonomous() instead
 * (see u_cell_loc.h).
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param onNotOff    true if AssistNow Autonomous should be on,
 *                    false if it is to be off.
 * @return            zero on success else negative error code.
 */
int32_t uGnssMgaSetAutonomous(uDeviceHandle_t gnssHandle, bool onNotOff);

/** Get the assistance database from a GNSS device.  Use this
 * function before powering-down the GNSS module if it has no battery
 * backup or flash memory attached and so is unable to retain its
 * state.  The data retrieved here can be restored to the GNSS module
 * with uGnssMgaSetDatabase() when the module is powered on once more,
 * reducing the time to first fix.
 *
 * When this is called the GNSS device will return its assistance
 * database in chunks of up to 166 bytes (a two-byte length indicator
 * followed by up to #U_GNSS_MGA_DBD_MESSAGE_PAYLOAD_LENGTH_MAX_BYTES
 * bytes of payload); pCallback will be called for each chunk returned.
 * pCallback must store the raw data (i.e. all up-to-166 bytes) in a
 * contigouous manner and return true on success, else false (in
 * which case this function will return and the remaining chunks
 * will be ignored).  Database retrieval will time-out if the final ack
 * is not received within #U_GNSS_MGA_DATABASE_READ_TIMEOUT_MS.
 *
 * IMPORTANT: the GNSS device may return several hundred messages of
 * size 50 to 90 bytes in VERY quick succession with no flow control
 * whatsoever.  Should a single character be lost the transfer will
 * fail: you must have a perfect transport and sufficient bandwidth
 * in your MCU to read everything without loss for this function to
 * work.
 *
 * Note: this uses one of the #U_GNSS_MSG_RECEIVER_MAX_NUM message
 * handles from the uGnssMsg API.
 *
 * Note: in the case where pDatabaseCallback returns false, this function
 * should not be called again until the application is sure that all of
 * the remaining chunks have been sent by the GNSS module (e.g. wait a
 * while or reset the GNSS module), else there will be confusion.
 *
 * Note: in order to speed up this process, NMEA messages from the GNSS
 * chip are temporarily disabled.  You can disable the disabling by
 * defining the conditional compilation flag
 * U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE.
 *
 * Note: not supported if the GNSS device is connected via an intermediate
 * e.g. cellular module; instead please use uCellLocSetAssistNowDatabaseSave().
 *
 * @param gnssHandle         the handle of the GNSS instance.
 * @param[in] pCallback      a callback function that receives
 *                           each chunk of the assistance database.
 *                           The first parameter is the GNSS handle,
 *                           the second a pointer to the chunk, the
 *                           third the size of the chunk, which can
 *                           be up to 166 bytes, and the last the
 *                           user parameter pCallbackParam.
 *                           The callback should copy the entire
 *                           buffer, adding it to any previous chunks
 *                           contiguously, and return true if it wants
 *                           to be called for further chunks, else
 *                           false.  When there are no more chunks to
 *                           be returned (either because the process
 *                           has completed or there has been an error)
 *                           pCallback will be called with
 *                           pBuffer set to NULL and size set to 0.
 * @param[in] pCallbackParam a user parameter which will be passed to
 *                           pCallback as its last parameter.
 * @return                   the total number of bytes transferred to
 *                           pCallback, else negative error
 *                           code; if an error code is returned, any
 *                           partial data passed to pCallback should
 *                           be discarded.
 */
int32_t uGnssMgaGetDatabase(uDeviceHandle_t gnssHandle,
                            uGnssMgaDatabaseCallback_t *pCallback,
                            void *pCallbackParam);

/** Set (restore) the assistance database to a GNSS device.  Use this
 * to write back to the GNSS device the information that was retrieved
 * from it using uGnssMgaGetDatabase().
 *
 * Note: if flow control is set to a value other than
 * #U_GNSS_MGA_FLOW_CONTROL_WAIT then, in order to speed up this process
 * when waiting for Acks, NMEA messages from the GNSS chip are temporarily
 * disabled.  You can disable the disabling by defining the conditional
 * compilation flag U_GNSS_MGA_DISABLE_NMEA_MESSAGE_DISABLE.
 *
 * Note: not supported if the GNSS device is connected via an intermediate
 * e.g. cellular module; instead please use uCellLocSetAssistNowDatabaseSave().
 *
 * @param gnssHandle              the handle of the GNSS instance.
 * @param flowControl             the type of flow control to use.
 * @param[in] pBuffer             a pointer to the start of a buffer
 *                                containing a contiguous and complete
 *                                set of chunks, each up to 166 bytes
 *                                long, retrieved using
 *                                uGnssMgaGetDatabase(); cannot by NULL.
 * @param size                    the amount of data at pBuffer; must
 *                                be greater than zero.
 * @param[in] pCallback           a function which will be called at regular
 *                                intervals while sending the response, to
 *                                track progress, and must return true for
 *                                the transfer to continue; if false is
 *                                returned then the transfer will be
 *                                cancelled.  May be NULL.  Do NOT call
 *                                into the GNSS API from this callback as the
 *                                API will already be locked and you will get
 *                                stuck.
 * @param[in,out] pCallbackParam  parameter that will be passed to pCallback
 *                                as its last parameter.
 * @return                        zero on success else negative error code.
 */
int32_t uGnssMgaSetDatabase(uDeviceHandle_t gnssHandle,
                            uGnssMgaFlowControl_t flowControl,
                            const char *pBuffer, size_t size,
                            uGnssMgaProgressCallback_t *pCallback,
                            void *pCallbackParam);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_MGA_H_

// End of file
