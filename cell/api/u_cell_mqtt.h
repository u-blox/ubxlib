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

#ifndef _U_CELL_MQTT_H_
#define _U_CELL_MQTT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the MQTT/MQTT-SN client API for cellular
 * modules.  These functions are NOT thread-safe and are NOT intended to be
 * called directly.  Instead, please use the common/mqtt_client API which
 * wraps the functions exposed here to handle error checking and
 * re-entrancy.
 * Note that the cellular MQTT API supports only a single MQTT instance,
 * hence the handles used throughout this API are the handle of the
 * cellular instance; no MQTT handle is required.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES
/** The maximum length of an MQTT broker address string; this does
 * NOT include room for a null terminator, any buffer should be
 * this length plus one.
 */
# define U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES
/** The maximum length of an MQTT publish message in bytes,
 * if hex mode has to be used.
 */
# define U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES 512
#endif

#ifndef U_CELL_MQTT_PUBLISH_BIN_MAX_LENGTH_BYTES
/** The maximum length of an MQTT publish message in bytes,
 * if binary mode can be used; this does not include room
 * for a null terminator, any buffer should be this length
 * plus one.
 */
# define U_CELL_MQTT_PUBLISH_BIN_MAX_LENGTH_BYTES 1024
#endif

#ifndef U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES
/** The maximum length of an MQTT topic used as a filter
 * or in a will message in bytes; this does NOT include
 * room for a null terminator, any buffer should be
 * this length plus one.
 */
# define U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES
/** The maximum length of an MQTT read topic in bytes;
 * this does NOT include room for a null terminator,
 * any buffer should be this length plus one.
 */
# define U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES
/** The maximum length of an MQTT "will" message in
 * bytes; this does NOT include room for a null
 * terminator, any buffer should be this length
 * plus one.
 */
# define U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_RETRIES_DEFAULT
/** The number of times to retry an MQTT operation if the
 * failure is due to radio conditions.
 */
# define U_CELL_MQTT_RETRIES_DEFAULT 2
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MQTT QoS.  The values here should match those in #uMqttQos_t.
 */
typedef enum {
    U_CELL_MQTT_QOS_AT_MOST_ONCE = 0,
    U_CELL_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_CELL_MQTT_QOS_EXACTLY_ONCE = 2,
    U_CELL_MQTT_QOS_MAX_NUM,
    U_CELL_MQTT_QOS_SEND_AND_FORGET = 3, /**< valid for MQTT-SN publish messages only. */
    U_CELL_MQTT_QOS_SN_PUBLISH_MAX_NUM
} uCellMqttQos_t;

/** The type of MQTT-SN topic name.  The values here
 * should match those in #uMqttSnTopicNameType_t.
 */
typedef enum {
    U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL = 0, /**< a two-byte ID, for example 0x0001, referring to a normal MQTT topic, for example "thing/this". */
    U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED = 1, /**< a pre-agreed two byte ID, for example 0x0100. */
    U_CELL_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT = 2,  /**< two alpha-numeric characters, for example "ab". */
    U_CELL_MQTT_SN_TOPIC_NAME_TYPE_MAX_NUM
} uCellMqttSnTopicNameType_t;

/** This type holds the two sorts of MQTT-SN topic name; a uint16_t
 * ID ( 0 to 65535) or a two-character name (for example "ab"). The
 * structure here MUST match #uMqttSnTopicName_t.
 */
typedef struct {
// *INDENT-OFF* (otherwise AStyle makes a mess of this)
    union {
        uint16_t id; /**< Populate this for the types #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL
                          or #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED. */
        // nameShort MUST be of length 2, as defined by the MQTT-SN specifications; the
        // code is written such that no terminating 0 is required in the storage here.
        char nameShort[2]; /**< Populate this for #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT;
                                nameShort must contain two ASCII characters, no terminator
                                is required. */
    } name;
    uCellMqttSnTopicNameType_t type; /**< If the id field is populated and was obtained
                                          through uCellMqttSnRegisterNormalTopic()
                                          or uCellMqttSnSubscribeNormalTopic() then set this to
                                          #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL.  If the id field
                                          is populated and is a predefined topic ID then set
                                          this to #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED.  If the
                                          nameShort field is populated, set this to
                                          #U_CELL_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT. */
// *INDENT-ON*
} uCellMqttSnTopicName_t;


/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT AND MQTT-SN
 * -------------------------------------------------------------- */

/** Initialise the cellular MQTT client.  If the client is already
 * initialised then this function returns immediately. The
 * module must be powered up for this function to work.
 * IMPORTANT: if you re-boot the cellular module after calling this
 * function you will lose all settings and must call uCellMqttDeinit()
 * followed by uCellMqttInit() to put them back again.
 *
 * @param cellHandle             the handle of the cellular instance
 *                               to be used.
 * @param[in] pBrokerNameStr     the null-terminated string that gives
 *                               the name of the broker for this MQTT
 *                               session.  This may be a domain name,
 *                               or an IP address and may include a port
 *                               number.  NOTE: if a domain name is used
 *                               the module may immediately try to perform
 *                               a DNS look-up to establish the IP address
 *                               of the broker and hence you should ensure
 *                               that the module is connected beforehand.
 * @param[in] pClientIdStr       the null-terminated string that
 *                               will be the client ID for this
 *                               MQTT session.  May be NULL, in
 *                               which case the driver will provide
 *                               a name.
 * @param[in] pUserNameStr       the null-terminated string that is the
 *                               user name required by the MQTT broker;
 *                               ignored for MQTT-SN.
 * @param[in] pPasswordStr       the null-terminated string that is the
 *                               password required by the MQTT broker;
 *                               ignored for MQTT-SN.
 * @param[in] pKeepGoingCallback certain of the MQTT API functions
 *                               need to wait for the broker to respond
 *                               and this may take some time.  Specify
 *                               a callback function here which will be
 *                               called while the API is waiting.  While
 *                               the callback function returns true the
 *                               API will continue to wait until success
 *                               or #U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
 *                               is reached.  If the callback function
 *                               returns false then the API will return.
 *                               Note that the thing the API was waiting for
 *                               may still succeed, this does not cancel
 *                               the operation, it simply stops waiting
 *                               for the response.  The callback function
 *                               may also be used to feed any application
 *                               watchdog timer that may be running.
 *                               May be NULL, in which case the
 *                               APIs will continue to wait until success
 *                               or #U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS.
 * @param mqttSn                 set to true if the connection is an MQTT-SN
 *                               connection to an MQTT-SN broker.
 * @return                       zero on success or negative error code on
 *                               failure.
 */
int32_t uCellMqttInit(uDeviceHandle_t cellHandle, const char *pBrokerNameStr,
                      const char *pClientIdStr, const char *pUserNameStr,
                      const char *pPasswordStr,
                      bool (*pKeepGoingCallback)(void),
                      bool mqttSn);

/** Shut-down the given cellular MQTT client.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 */
void uCellMqttDeinit(uDeviceHandle_t cellHandle);

/** Get the current cellular MQTT client ID.
 *
 * @param cellHandle         the handle of the cellular instance to be used.
 * @param[out] pClientIdStr  pointer to a place to put the client ID,
 *                           which will be null-terminated. Can only be
 *                           NULL if sizeBytes is zero.
 * @param sizeBytes          size of the storage at pClientIdStr,
 *                           including the terminator.
 * @return                   the number of bytes written to pClientIdStr,
 *                           not including the terminator (what strlen()
 *                           would return), or negative error code.
 */
int32_t uCellMqttGetClientId(uDeviceHandle_t cellHandle, char *pClientIdStr,
                             size_t sizeBytes);

/** Set the local port to use for the MQTT client.
 * Note that only SARA-R412M-02B supports setting the local port.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @param port       the port number.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetLocalPort(uDeviceHandle_t cellHandle, uint16_t port);

/** Get the local port used by the MQTT client.
 * Note that only SARA-R412M-02B supports setting the local port and,
 * that it does not support _reading_ the local port unless one has
 * been specifically set with uCellMqttSetLocalPort().
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the port number on success or negative error code.
 */
int32_t uCellMqttGetLocalPort(uDeviceHandle_t cellHandle);

/** Set the inactivity timeout used by the MQTT client.  If this
 * is not called then no inactivity timeout is used.  An inactivity
 * timeout value of 0 means no inactivity timeout.  The inactivity
 * timeout is applied at the moment the connection to the broker is
 * made.
 * Note that a very short inactivity timeout in conjunction with MQTT
 * "keep alive" is inadvisable; the MQTT pings sent near the end of
 * the inactivity timeout could cause heavy broker/network load and
 * high power consumption.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 * @param seconds     the inactivity timeout in seconds.
 * @return            zero on success or negative error code.
 */
int32_t uCellMqttSetInactivityTimeout(uDeviceHandle_t cellHandle,
                                      size_t seconds);

/** Get the inactivity timeout used by the MQTT client.  Note that
 * zero means there is no inactivity timeout.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the inactivity timeout in seconds on success
 *                   or negative error code.
 */
int32_t uCellMqttGetInactivityTimeout(uDeviceHandle_t cellHandle);

/** Switch MQTT ping or "keep alive" on.  This will send an
 * MQTT ping message to the broker near the end of the
 * inactivity timeout to keep the connection alive.
 * If this is not called no such ping is sent.  This must
 * be called after a connection has been made and is specific
 * to that connection, i.e. "keep alive" always begins off
 * for a connection and you must switch it on.  If the inactivity
 * timeout is zero then this function will return
 * #U_CELL_ERROR_NOT_ALLOWED.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetKeepAliveOn(uDeviceHandle_t cellHandle);

/** Switch MQTT ping or "keep alive" off. See
 * uMqttSetKeepAliveOn() for more details.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetKeepAliveOff(uDeviceHandle_t cellHandle);

/** Determine whether MQTT ping or "keep alive" is on or off.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if MQTT ping or "keep alive" is on, else
 *                   false.
 */
bool uCellMqttIsKeptAlive(uDeviceHandle_t cellHandle);

/** If this function returns successfully then
 * the topic subscriptions and message queue status
 * will be retained by both the client and the
 * broker across MQTT disconnects/connects.
 * Note that SARA-R5 does not support session retention.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetRetainOn(uDeviceHandle_t cellHandle);

/** Switch MQTT session retention off. See
 * uMqttSetSessionRetainOn() for more details.
 * IMPORTANT: a re-boot of the module will lose your
 * setting. Off is the default state.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetRetainOff(uDeviceHandle_t cellHandle);

/** Determine whether MQTT session retention is on or off.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if MQTT session retention is on else false.
 */
bool uCellMqttIsRetained(uDeviceHandle_t cellHandle);

/** Switch MQTT [D]TLS security on.  By default MQTT TLS security
 * (DTLS security for MQTT-SN) is off.  If you intend to switch
 * security on don't forget to specify the secure broker port number
 * in the call to uCellMqttInit() for example "mybroker.com:8883".
 * IMPORTANT: a re-boot of the module will lose your
 * setting.
 * Note that SARA-R4 modules do not support changing MQTT
 * TLS security mode once an MQTT session has been used
 * without powering the module down and up again.
 * Note that SARA-R4xxx-02B doesn't support MQTT TLS security.
 *
 * @param cellHandle        the handle of the cellular instance
 *                          to be used.
 * @param securityProfileId the security profile ID
 *                          containing the [D]TLS security
 *                          parameters.  Specify -1
 *                          to let this be chosen
 *                          automatically.
 * @return                  zero on success or negative
 *                          error code.
 */
int32_t uCellMqttSetSecurityOn(uDeviceHandle_t cellHandle,
                               int32_t securityProfileId);

/** Switch MQTT [D]TLS security off.
 * Note that SARA-R4 modules do not support switching
 * MQTT TLS security off again once it has been switched on
 * for an MQTT session without powering the module down and
 * up again.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetSecurityOff(uDeviceHandle_t cellHandle);

/** Determine whether MQTT [D]TLS security is on or off.
 *
 * @param cellHandle              the handle of the cellular instance
 *                                to be used.
 * @param[out] pSecurityProfileId a pointer to a place to put
 *                                the security profile ID that
 *                                is being used for MQTT [D]TLS
 *                                security; may be NULL.
 * @return                        true if MQTT [D]TLS security is
 *                                on else false.
 */
bool uCellMqttIsSecured(uDeviceHandle_t cellHandle,
                        int32_t *pSecurityProfileId);

/** Set the MQTT "will" message that will be sent
 * by the broker on an uncommanded disconnect of the MQTT
 * client.  Note that SARA-R4 does not support "will"s.
 * IMPORTANT: a re-boot of the module will lose your
 * setting.
 *
 * @param cellHandle         the handle of the cellular instance to
 *                           be used.
 * @param[in] pTopicNameStr  the null-terminated topic string
 *                           for the "will" message; may be NULL,
 *                           in which case the topic name string
 *                           will not be modified.
 * @param[in] pMessage       a pointer to the "will" message.  For
 *                           MQTT the "will" message is not
 *                           restricted to ASCII values while for
 *                           MQTT-SN it must be a null-terminated
 *                           ASCII string containing only printable
 *                           characters (isprint() returns true)
 *                           and no double quotation marks. May be
 *                           NULL, in which case the message will
 *                           not be modified.
 * @param messageSizeBytes   since pMessage may include binary
 *                           content, including NULLs, this
 *                           parameter specifies the length of
 *                           pMessage. If pMessage happens to
 *                           be an ASCII string this parameter
 *                           should be set to strlen(pMessage).
 *                           Ignored if pMessage is NULL.
 * @param qos                the MQTT QoS to use for the
 *                           "will" message.
 * @param retain             if true the "will" message will
 *                           be kept by the broker across
 *                           MQTT disconnects/connects, else
 *                           it will be cleared.
 * @return                   zero on success else negative error
 *                           code.
 */
int32_t uCellMqttSetWill(uDeviceHandle_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain);

/** Get the MQTT "will" message that will be sent
 * by the broker on an uncommanded disconnect of the MQTT
 * client.  Note that SARA-R4 does not support "will"s.
 *
 * @param cellHandle                the handle of the cellular instance
 *                                  to be used.
 * @param[out] pTopicNameStr        a place to put the null-terminated
 *                                  topic string used with the "will"
 *                                  message; may be NULL.
 * @param topicNameSizeBytes        the number of bytes of storage
 *                                  at pTopicNameStr.  Ignored if
 *                                  pTopicNameStr is NULL.
 * @param[out] pMessage             a place to put the "will" message;
 *                                  may be NULL.
 * @param[in,out] pMessageSizeBytes on entry this should point to the
 *                                  number of bytes of storage at
 *                                  pMessage. On return, if pMessage
 *                                  is not NULL, this will be updated
 *                                  to the number of bytes written
 *                                  to pMessage.  Must be non-NULL if
 *                                  pMessage is not NULL.
 * @param[out] pQos                 a place to put the MQTT QoS that is
 *                                  used for the "will" message. May
 *                                  be NULL.
 * @param pRetain                   a place to put the status of "will"
 *                                  message retention. May be NULL.
 * @return                          zero on success else negative error
 *                                  code.
 */
int32_t uCellMqttGetWill(uDeviceHandle_t cellHandle, char *pTopicNameStr,
                         size_t topicNameSizeBytes,
                         char *pMessage,
                         size_t *pMessageSizeBytes,
                         uCellMqttQos_t *pQos, bool *pRetain);

/** Start an MQTT session. The pKeepGoingCallback()
 * function set during initialisation will be called while
 * this function is waiting for a connection to be made.
 *
 * @param cellHandle    the handle of the cellular instance to
 *                      be used.
 * @return              zero on success or negative error code.
 */
int32_t uCellMqttConnect(uDeviceHandle_t cellHandle);

/** Stop an MQTT session.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttDisconnect(uDeviceHandle_t cellHandle);

/** Determine whether an MQTT session is active or not.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if an MQTT session is active else false.
 */
bool uCellMqttIsConnected(uDeviceHandle_t cellHandle);

/** Set a callback to be called when new messages are
 * available to be read.
 *
 * NOTE: it would be tempting to read a new unread message in your message
 * callback.  However, note that if your device has been out of coverage
 * while you are subscribed to an MQTT topic and then returns to coverage,
 * there could be a deluge of messages that land all at once.  And since
 * reading a message will cause the number of unread messages to change,
 * you will likely get two unread message indications after every read: one
 * indicating the count has gone up, since the messages are still arriving,
 * and another indicating the count has gone down, since you've just read
 * one.  Hence it is best if your MQTT message reads are carried out in
 * their own thread; this thread would begin reading when a non-zero
 * number of messages are available to read and continue to read messages
 * until there are no more.  This takes the load out of the call-back queue
 * and prevents multiple-triggering.
 *
 * @param cellHandle          the handle of the cellular instance to
 *                            be used.
 * @param[in] pCallback       the callback. The first parameter to
 *                            the callback will be filled in with
 *                            the number of messages available to
 *                            be read. The second parameter will be
 *                            pCallbackParam. Use NULL to deregister
 *                            a previous callback.
 * @param[in] pCallbackParam  this value will be passed to pCallback
 *                            as the second parameter.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uCellMqttSetMessageCallback(uDeviceHandle_t cellHandle,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam);

/** Get the current number of unread messages.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the number of unread messages or negative
 *                   error code.
 */
int32_t uCellMqttGetUnread(uDeviceHandle_t cellHandle);

/** Get the last MQTT error code.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           an error code, the meaning of which is
 *                   utterly module specific.
 */
int32_t uCellMqttGetLastErrorCode(uDeviceHandle_t cellHandle);

/** Set a callback to be called if the MQTT connection
 * is disconnected, either locally or by the broker.
 *
 * @param cellHandle         the handle of the cellular instance
 *                           to be used.
 * @param[in] pCallback      the callback. The first parameter is the
 *                           error code, as would be returned by
 *                           uCellMqttGetLastErrorCode(), the second
 *                           parameter is pCallbackParam. Use NULL to
 *                           deregister a previous callback.
 * @param[in] pCallbackParam this value will be passed to pCallback
 *                           as the second parameter.
 * @return                   zero on success else negative error
 *                           code.
 */
int32_t uCellMqttSetDisconnectCallback(uDeviceHandle_t cellHandle,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam);

/** Set the number of retries that the MQTT client will make for any
 * operation that fails due to the radio interface.  If this function
 * is not called #U_CELL_MQTT_RETRIES_DEFAULT will apply.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 * @param numRetries  the number of retries.
 */
void uCellMqttSetRetries(uDeviceHandle_t cellHandle, size_t numRetries);

/** Get the number of retries that the MQTT client will make for any
 * operation that fails due to the radio interface.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           on success, the number of retries, else negative
 *                   error code.
 */
int32_t uCellMqttGetRetries(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT ONLY
 * -------------------------------------------------------------- */

/** Determine if MQTT is supported by the given cellHandle.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 * @return            true if MQTT is supported, else false.
 */
bool uCellMqttIsSupported(uDeviceHandle_t cellHandle);

/** Publish an MQTT message. The pKeepGoingCallback()
 * function set during initialisation will be called while
 * this function is waiting for publish to complete.
 *
 * @param cellHandle        the handle of the cellular instance to
 *                          be used.
 * @param[in] pTopicNameStr the null-terminated topic string
 *                          for the message; cannot be NULL.
 * @param[in] pMessage      a pointer to the message; the message
 *                          is not restricted to ASCII values.
 *                          Cannot be NULL.
 * @param messageSizeBytes  since pMessage may include binary
 *                          content, including NULLs, this
 *                          parameter specifies the length of
 *                          pMessage. If pMessage happens to
 *                          be an ASCII string this parameter
 *                          should be set to strlen(pMessage).
 *                          The maximum message size varies with
 *                          module type: if binary entry is supported
 *                          or pMessage contains purely ASCII
 *                          printable characters (i.e. isprint()
 *                          returns true) then it is usually 1024
 *                          characters, else it will likely be
 *                          512 characters to allow for hex coding;
 *                          however on some modules (e.g. SARA-R410M-03B)
 *                          it can be as low as 256 characters.
 * @param qos               the MQTT QoS to use for this message.
 * @param retain            if true the message will be retained
 *                          by the broker across MQTT disconnects/
 *                          connects.
 * @return                  zero on success else negative error
 *                          code.
 */
int32_t uCellMqttPublish(uDeviceHandle_t cellHandle, const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain);

/** Subscribe to an MQTT topic. The pKeepGoingCallback()
 * function set during initialisation will be called while
 * this function is waiting for a subscription to complete.
 *
 * @param cellHandle           the handle of the cellular instance
 *                             to be used.
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to subscribe to; the wildcard '+'
 *                             may be used to specify "all"
 *                             at any one topic level and the
 *                             wildcard '#' may be used at the end
 *                             of the string to indicate "everything
 *                             from here on".  Cannot be NULL.
 * @param maxQos               the maximum MQTT message QoS to
 *                             for this subscription.
 * @return                     the QoS of the subscription else
 *                             negative error code.
 */
int32_t uCellMqttSubscribe(uDeviceHandle_t cellHandle,
                           const char *pTopicFilterStr,
                           uCellMqttQos_t maxQos);

/** Unsubscribe from an MQTT topic.
 *
 * @param cellHandle           the handle of the cellular instance to
 *                             be used.
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to unsubscribe from; the wildcard '+'
 *                             may be used to specify "all"
 *                             at any one topic level and the
 *                             wildcard '#' may be used at the end
 *                             of the string to indicate "everything
 *                             from here on".  Cannot be NULL.
 * @return                     zero on success else negative error
 *                             code.
 */
int32_t uCellMqttUnsubscribe(uDeviceHandle_t cellHandle,
                             const char *pTopicFilterStr);

/** Read an MQTT message.
 *
 * @param cellHandle                 the handle of the cellular instance to
 *                                   be used.
 * @param[out] pTopicNameStr         a place to put the null-terminated
 *                                   topic string of the message; cannot
 *                                   be NULL.
 * @param topicNameSizeBytes         the number of bytes of storage
 *                                   at pTopicNameStr.
 * @param[out] pMessage              a place to put the message; may be NULL.
 * @param[in,out] pMessageSizeBytes  on entry this should point to the
 *                                   number of bytes of storage at
 *                                   pMessage. On return, this will be
 *                                   updated to the number of bytes written
 *                                   to pMessage.  Ignored if pMessage is
 *                                   NULL.
 * @param[out] pQos                  a place to put the QoS of the message;
 *                                   may be NULL.
 * @return                           zero on success else negative error
 *                                   code.
 */
int32_t uCellMqttMessageRead(uDeviceHandle_t cellHandle, char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage, size_t *pMessageSizeBytes,
                             uCellMqttQos_t *pQos);

/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT-SN ONLY
 * -------------------------------------------------------------- */

/** Determine if MQTT-SN is supported by the given cellHandle.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 * @return            true if MQTT-SN is supported, else false.
 */
bool uCellMqttSnIsSupported(uDeviceHandle_t cellHandle);

/** MQTT-SN only: ask the MQTT-SN broker for an MQTT-SN topic name
 * for the given normal MQTT topic name; if you wish to publish to
 * a normal MQTT topic, for example "thing/this", using MQTT-SN, which
 * only transports a 16-bit topic ID, then you must register the
 * normal MQTT topic to obtain an MQTT-SN topic name for it.
 * Note: if you intend to subscribe to an MQTT topic as well as
 * publish to an MQTT topic you do NOT need to use this function:
 * instead use the pTopicName returned by
 * uCellMqttSnSubscribeNormalTopic().  This function does not need
 * to be used for MQTT-SN short topic names (e.g. "xy") because they
 * already fit into 16-bits.
 * Note that this does NOT subscribe to the topic, it just gets you
 * an ID, you need to call uCellMqttSnSubscribe() to do the subscribing.
 * Must be connected to an MQTT-SN broker for this to work.

 *
 * @param cellHandle         the handle of the cellular instance to
 *                           be used.
 * @param[in] pTopicNameStr  the null-terminated topic name string;
 *                           cannot be NULL.
 * @param[out] pTopicName    a place to put the MQTT-SN topic name;
 *                           cannot be NULL.
 * @return                   zero on success, else negative error code.
 */
int32_t uCellMqttSnRegisterNormalTopic(uDeviceHandle_t cellHandle,
                                       const char *pTopicNameStr,
                                       uCellMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: publish a message; this differs from uCellMqttPublish()
 * in that it uses an MQTT-SN topic name, which will be a predefined ID
 * or a short name or as returned by uCellMqttSnRegisterNormalTopic()/
 * uCellMqttSnSubscribeNormalTopic()).
 * Must be connected to an MQTT-SN broker for this to work.
 *
 * @param cellHandle          the handle of the cellular instance to
 *                            be used.
 * @param[in] pTopicName      the MQTT-SN topic name; cannot be NULL.
 * @param[in] pMessage        a pointer to the message; the message
 *                            is not restricted to ASCII values.
 *                            Cannot be NULL.
 * @param messageSizeBytes    since pMessage may include binary
 *                            content, including NULLs, this
 *                            parameter specifies the length of
 *                            pMessage. If pMessage happens to
 *                            be an ASCII string this parameter
 *                            should be set to strlen(pMessage).
 * @param qos                 the MQTT QoS to use for this message.
 * @param retain              if true the message will be kept
 *                            by the broker across MQTT disconnects/
 *                            connects, else it will be cleared.
 * @return                    zero on success else negative error code.
 */
int32_t uCellMqttSnPublish(uDeviceHandle_t cellHandle,
                           const uCellMqttSnTopicName_t *pTopicName,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uCellMqttQos_t qos, bool retain);

/** MQTT-SN only: subscribe to an MQTT-SN topic; this differs from
 * uMqttClientSubscribe() in that it takes an MQTT-SN topic name,
 * instead of a filter string, as the topic parameter.  Must be
 * connected to an MQTT-SN broker for this to work.
 *
 * @param cellHandle              the handle of the cellular instance
 *                                to be used.
 * @param[in] pTopicName          the MQTT topic name to subscribe to;
 *                                cannot be NULL.
 * @param maxQos                  the maximum QoS for this subscription.
 * @return                        the QoS of the subscription else
 *                                negative error code.
 */
int32_t uCellMqttSnSubscribe(uDeviceHandle_t cellHandle,
                             const uCellMqttSnTopicName_t *pTopicName,
                             uCellMqttQos_t maxQos);

/** MQTT-SN only: subscribe to a normal MQTT topic; this differs
 * from uCellMqttSubscribe() in that it can return pTopicName,
 * allowing MQTT-SN publish/read operations to be carried out on
 * a normal MQTT topic.  Must be connected to an MQTT-SN broker
 * for this to work.
 *
 * @param cellHandle              the handle of the cellular instance
 *                                to be used.
 * @param[in] pTopicFilterStr     the null-terminated topic string to
 *                                subscribe to; cannot be NULL.  The
 *                                 wildcard '+' may be used to specify
 *                                "all" at any one topic level and the
 *                                wildcard '#' may be used at the end
 *                                of the string to indicate "everything
 *                                from here on", but note that pTopicName
 *                                cannot not be populated if wild-cards
 *                                are used.
 * @param maxQos                  the maximum MQTT message QoS for this
 *                                subscription.
 * @param[out] pTopicName         a place to put the MQTT-SN topic ID that
 *                                can be used for publishing to this topic;
 *                                may be NULL.
 * @return                        the QoS of the subscription else negative
 *                                error code.
 */
int32_t uCellMqttSnSubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                        const char *pTopicFilterStr,
                                        uCellMqttQos_t maxQos,
                                        uCellMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: unsubscribe from an MQTT-SN topic; this differs from
 * uCellMqtttUnsubscribe() in that it takes an MQTT-SN topic name,
 * instead of a filter string, as the topic parameter.  Must be
 * connected to an MQTT-SN broker for this to work.
 *
 * @param cellHandle           the handle of the cellular instance to
 *                             be used.
 * @param[in] pTopicName       the MQTT-SN topic name to unsubscribe from;
 *                             cannot be NULL.
 * @return                     zero on success else negative error
 *                             code.
 */
int32_t uCellMqttSnUnsubscribe(uDeviceHandle_t cellHandle,
                               const uCellMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: unsubscribe from a normal MQTT topic.  Must be
 * connected to an MQTT-SN broker for this to work.
 *
 * @param cellHandle           the handle of the cellular instance to
 *                             be used.
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to unsubscribe from. The wildcard '+' may
 *                             be used to specify "all" at any one topic
 *                             level and the wildcard '#' may be used
 *                             at the end of the string to indicate
 *                             "everything from here on".  Cannot be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uCellMqttSnUnsubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                          const char *pTopicFilterStr);

/** MQTT-SN only: read a message, must be used to read messages when an
 * MQTT-SN connection is in place; it differs from uCellMqttMessageRead()
 * in that it uses an MQTT-SN topic name; if the message is actually an
 * MQTT message then the topic name will be populated with the MQTT-SN
 * topic name that you received when you called
 * uCellMqttSnSubscribeNormalTopic().
 * Must be connected to an MQTT-SN broker for this to work.
 *
 * @param cellHandle                the handle of the cellular instance to
 *                                  be used.
 * @param[out] pTopicName           a place to put the MQTT-SN topic name;
 *                                  cannot be NULL.
 * @param[out] pMessage             a place to put the message; may be NULL.
 * @param[in,out] pMessageSizeBytes on entry this should point to the
 *                                  number of bytes of storage at
 *                                  pMessage. On return, this will be
 *                                  updated to the number of bytes written
 *                                  to pMessage.  Ignored if pMessage is
 *                                  NULL.
 * @param[out] pQos                 a place to put the QoS of the message;
 *                                  may be NULL.
 * @return                          zero on success else negative error
 *                                  code.
 */
int32_t uCellMqttSnMessageRead(uDeviceHandle_t cellHandle,
                               uCellMqttSnTopicName_t *pTopicName,
                               char *pMessage, size_t *pMessageSizeBytes,
                               uCellMqttQos_t *pQos);

/** MQTT-SN only: update an existing MQTT "will" message that will
 * be sent by the broker on an uncommanded disconnect of the MQTT
 * client.  Note that while the form of this API requires a message
 * size for forward compatibility, the underlying AT interface for
 * this command ONLY works if pMessage is a null-terminated string
 * containing only printable characters (i.e. isprint() returns true)
 * and no double quotation marks.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle       the handle of the cellular instance to
 *                         be used.
 * @param[in] pMessage     a pointer to the "will" message;
 *                         must be a null terminated string, cannot
 *                         be NULL must contain only printable
 *                         characters (isprint() returns true)
 *                         and no double quotation marks.
 * @param messageSizeBytes provided for future compatiblity only,
 *                         please use strlen(pMessage).
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uCellMqttSnSetWillMessaage(uDeviceHandle_t cellHandle,
                                   const char *pMessage,
                                   size_t messageSizeBytes);

/** MQTT-SN only: update the parameters for an existing MQTT "will".
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle        the handle of the cellular instance to
 *                          be used.
 * @param[in] pTopicNameStr the null-terminated topic string
 *                          for the "will" message; cannot be NULL.
 * @param qos               the MQTT QoS to use for the "will"
 *                          message.
 * @param retain            if true the "will" message will be
 *                          kept by the broker across
 *                          MQTT disconnects/connects, else it
 *                          will be cleared.
 * @return                  zero on success else negative error
 *                          code.
 */
int32_t uCellMqttSnSetWillParameters(uDeviceHandle_t cellHandle,
                                     const char *pTopicNameStr,
                                     uCellMqttQos_t qos, bool retain);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_MQTT_H_

// End of file
