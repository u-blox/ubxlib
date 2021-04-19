/*
 * Copyright 2020 u-blox Ltd
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

#ifndef _U_CELL_MQTT_H_
#define _U_CELL_MQTT_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the MQTT client API for cellular
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
/** The maximum length of an MQTT broker address string.
 */
# define U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES
/** The maximum length of an MQTT publish message in bytes,
 * if hex mode has to be used.
 */
# define U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES 512
#endif

#ifndef U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES
/** The maximum length of an MQTT publish message in bytes,
 * if hex mode has to be used.
 */
# define U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES 512
#endif

#ifndef U_CELL_MQTT_PUBLISH_BIN_MAX_LENGTH_BYTES
/** The maximum length of an MQTT publish message in bytes,
 * if binary mode can be used.
 */
# define U_CELL_MQTT_PUBLISH_BIN_MAX_LENGTH_BYTES 1024
#endif

#ifndef U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES
/** The maximum length of an MQTT topic used as a filter
 * or in a will message in bytes.
 */
# define U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES
/** The maximum length of an MQTT read topic in bytes.
 */
# define U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES 256
#endif

#ifndef U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES
/** The maximum length of an MQTT "will" message in
 * bytes.
 */
# define U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES 256
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MQTT QoS.  The values here should match those in uCellMqttQos_t.
 */
typedef enum {
    U_CELL_MQTT_QOS_AT_MOST_ONCE = 0,
    U_CELL_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_CELL_MQTT_QOS_EXACTLY_ONCE = 2,
    U_CELL_MQTT_QOS_MAX_NUM
} uCellMqttQos_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the cellular MQTT client.  If the client is already
 * initialised then this function returns immediately. The
 * module must be powered up for this function to work.
 * IMPORTANT: if you re-boot the cellular module after calling this
 * function you will lose all settings and must call uCellMqttDeinit()
 * followed by uCellMqttInit() to put them back again.
 *
 * @param cellHandle         the handle of the cellular instance
 *                           to be used.
 * @param pBrokerNameStr     the null-terminated string that gives
 *                           the name of the broker for this MQTT
 *                           session.  This may be a domain name,
 *                           or an IP address and may include a port
 *                           number.  NOTE: if a domain name is used
 *                           the module may immediately try to perform
 *                           a DNS look-up to establish the IP address
 *                           of the broker and hence you should ensure
 *                           that the module is connected beforehand.
 * @param pClientIdStr       the null-terminated string that
 *                           will be the client ID for this
 *                           MQTT session.  May be NULL, in
 *                           which case the driver will provide
 *                           a name.
 * @param pUserNameStr       the null-terminated string that is the
 *                           user name required by the MQTT broker.
 * @param pPasswordStr       the null-terminated string that is the
 *                           password required by the MQTT broker.
 * @param pKeepGoingCallback certain of the MQTT API functions
 *                           need to wait for the broker to respond
 *                           and this may take some time.  Specify
 *                           a callback function here which will be
 *                           called while the API is waiting.  While
 *                           the callback function returns true the
 *                           API will continue to wait until success
 *                           or U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
 *                           is reached.  If the callback function
 *                           returns false then the API will return.
 *                           Note that the thing the API was waiting for
 *                           may still succeed, this does not cancel
 *                           the operation, it simply stops waiting
 *                           for the response.  The callback function
 *                           may also be used to feed any application
 *                           watchdog timer that may be running.
 *                           May be NULL, in which case the
 *                           APIs will continue to wait until success
 *                           or U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS.
 * @param futureExpansion    in this version of this API this field
 *                           is ignored.
 * @return                   zero on success or negative error code on
 *                           failure.
 */
int32_t uCellMqttInit(int32_t cellHandle, const char *pBrokerNameStr,
                      const char *pClientIdStr, const char *pUserNameStr,
                      const char *pPasswordStr,
                      bool (*pKeepGoingCallback)(void),
                      bool futureExpansion);

/** Shut-down the given cellular MQTT client.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 */
void uCellMqttDeinit(int32_t cellHandle);

/** Get the current cellular MQTT client ID.
 *
 * @param cellHandle       the handle of the cellular instance to be used.
 * @param pClientIdStr     pointer to a place to put the client ID,
 *                         which will be null-terminated. Can only be
 *                         NULL if sizeBytes is zero.
 * @param sizeBytes        size of the storage at pClientIdStr,
 *                         including the terminator.
 * @return                 the number of bytes written to pClientIdStr,
 *                         not including the terminator (i.e. what
 *                         strlen() would return), or negative error code.
 */
int32_t uCellMqttGetClientId(int32_t cellHandle, char *pClientIdStr,
                             size_t sizeBytes);

/** Set the local port to use for the MQTT client.  If this is not
 * called the IANA assigned ports of 1883 for non-secure MQTT or 8883
 * for TLS secured MQTT will be used.
 * Note that SARA-R5 does not support setting the local port, the port
 * numbers will be pre-set based on whether security is on or off.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @param port       the port number.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetLocalPort(int32_t cellHandle, uint16_t port);

/** Get the local port used by the MQTT client.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the port number on success or negative error code.
 */
int32_t uCellMqttGetLocalPort(int32_t cellHandle);

/** Set the inactivity timeout used by the MQTT client.  If this
 * is not called then no inactivity timeout is used.  Note that for
 * SARA-R5 modules *setting* a value of 0 is not permitted, *leaving*
 * the value at the default of 0 is permitted.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 * @param seconds     the inactivity timeout in seconds.
 * @return            zero on success or negative error code.
 */
int32_t uCellMqttSetInactivityTimeout(int32_t cellHandle,
                                      size_t seconds);

/** Get the inactivity timeout used by the MQTT client.  Note that
 * zero means there is no inactivity timeout.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the inactivity timeout in seconds on success
 *                   or negative error code.
 */
int32_t uCellMqttGetInactivityTimeout(int32_t cellHandle);

/** Switch MQTT ping or "keep alive" on.  This will send an
 * MQTT ping message to the broker near the end of the
 * inactivity timeout to keep the connection alive.
 * If this is not called no such ping is sent.
 * IMPORTANT: a re-boot of the module will lose your setting.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetKeepAliveOn(int32_t cellHandle);

/** Switch MQTT ping or "keep alive" off. See
 * uMqttSetKeepAliveOn() for more details.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetKeepAliveOff(int32_t cellHandle);

/** Determine whether MQTT ping or "keep alive" is on or off.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if MQTT ping or "keep alive" is on, else
 *                   false.
 */
bool uCellMqttIsKeptAlive(int32_t cellHandle);

/** If this function returns successfully then
 * the topic subscriptions and message queue status
 * will be retained by both the client and the
 * broker across MQTT disconnects/connects.
 * Note that SARA-R5 does not support session retention.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetRetainOn(int32_t cellHandle);

/** Switch MQTT session retention off. See
 * uMqttSetSessionRetainOn() for more details.
 * IMPORTANT: a re-boot of the module will lose your
 * setting.
 * This is the default state.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetRetainOff(int32_t cellHandle);

/** Determine whether MQTT session retention is on or off.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if MQTT session retention is on else false.
 */
bool uCellMqttIsRetained(int32_t cellHandle);

/** Switch MQTT TLS security on.  By default MQTT TLS
 * security is off.  If you intend to switch security on don't
 * forget to specify the secure broker port number in the call
 * to uCellMqttInit() e.g. "mybroker.com:8883".
 * IMPORTANT: a re-boot of the module will lose your
 * setting.
 *
 * @param cellHandle        the handle of the cellular instance
 *                          to be used.
 * @param securityProfileId the security profile ID
 *                          containing the TLS security
 *                          parameters.  Specify -1
 *                          to let this be chosen
 *                          automatically.
 * @return                  zero on success or negative
 *                          error code.
 */
int32_t uCellMqttSetSecurityOn(int32_t cellHandle,
                               int32_t securityProfileId);

/** Switch MQTT TLS security off.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttSetSecurityOff(int32_t cellHandle);

/** Determine whether MQTT TLS security is on or off.
 *
 * @param cellHandle         the handle of the cellular instance
 *                           to be used.
 * @param pSecurityProfileId a pointer to a place to put
 *                           the security profile ID that
 *                           is being used for MQTT TLS
 *                           security; may be NULL.
 * @return                   true if MQTT TLS security is
 *                           on else false.
 */
bool uCellMqttIsSecured(int32_t cellHandle,
                        int32_t *pSecurityProfileId);

/** Set the MQTT "will" message that will be sent
 * by the broker on an uncommanded disconnect of the MQTT
 * client.  Note that SARA-R4 does not support "will"s.
 * IMPORTANT: a re-boot of the module will lose your
 * setting.
 *
 * @param cellHandle       the handle of the cellular instance to
 *                         be used.
 * @param pTopicNameStr    the null-terminated topic string
 *                         for the "will" message; may be NULL,
 *                         in which case the topic name string
 *                         will not be modified.
 * @param pMessage         a pointer to the "will" message;
 *                         the "will" message is not restricted
 *                         to ASCII values.  May be NULL,
 *                         in which case the message will not be
 *                         modified.
 * @param messageSizeBytes since pMessage may include binary
 *                         content, including NULLs, this
 *                         parameter specifies the length of
 *                         pMessage. If pMessage happens to
 *                         be an ASCII string this parameter
 *                         should be set to strlen(pMessage).
 *                         Ignored if pMessage is NULL.
 * @param qos              the MQTT QoS to use for the
 *                         "will" message.
 * @param retain           if true the "will" message will
 *                         be kept by the broker across
 *                         MQTT disconnects/connects, else
 *                         it will be cleared.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uCellMqttSetWill(int32_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain);

/** Get the MQTT "will" message that will be sent
 * by the broker on an uncommanded disconnect of the MQTT
 * client.  Note that SARA-R4 does not support "will"s.
 *
 * @param cellHandle          the handle of the cellular instance
 *                            to be used.
 * @param pTopicNameStr       a place to put the null-terminated
 *                            topic string used with the "will"
 *                            message; may be NULL.
 * @param topicNameSizeBytes  the number of bytes of storage
 *                            at pTopicNameStr.  Ignored if
 *                            pTopicNameStr is NULL.
 * @param pMessage            a place to put the "will" message;
 *                            may be NULL.
 * @param pMessageSizeBytes   on entry this should point to the
 *                            number of bytes of storage at
 *                            pMessage. On return, if pMessage
 *                            is not NULL, this will be updated
 *                            to the number of bytes written
 *                            to pMessage.  Must be non-NULL if
 *                            pMessage is not NULL.
 * @param pQos                a place to put the MQTT QoS that is
 *                            used for the "will" message. May
 *                            be NULL.
 * @param pRetain             a place to put the status of "will"
 *                            message retention. May be NULL.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uCellMqttGetWill(int32_t cellHandle, char *pTopicNameStr,
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
int32_t uCellMqttConnect(int32_t cellHandle);

/** Stop an MQTT session.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           zero on success or negative error code.
 */
int32_t uCellMqttDisconnect(int32_t cellHandle);

/** Determine whether an MQTT session is active or not.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           true if an MQTT session is active else false.
 */
bool uCellMqttIsConnected(int32_t cellHandle);

/** Publish an MQTT message. The pKeepGoingCallback()
 * function set during initialisation will be called while
 * this function is waiting for publish to complete.
 *
 * @param cellHandle       the handle of the cellular instance to
 *                         be used.
 * @param pTopicNameStr    the null-terminated topic string
 *                         for the message; cannot be NULL.
 * @param pMessage         a pointer to the message; the message
 *                         is not restricted to ASCII values.
 *                         Cannot be NULL.
 * @param messageSizeBytes since pMessage may include binary
 *                         content, including NULLs, this
 *                         parameter specifies the length of
 *                         pMessage. If pMessage happens to
 *                         be an ASCII string this parameter
 *                         should be set to strlen(pMessage).
 * @param qos              the MQTT QoS to use for this message.
 * @param retain           if true the message will be retained
 *                         by the broker across MQTT disconnects/
 *                         connects.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uCellMqttPublish(int32_t cellHandle, const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain);

/** Subscribe to an MQTT topic. The pKeepGoingCallback()
 * function set during initialisation will be called while
 * this function is waiting for a subscription to complete.
 *
 * @param cellHandle       the handle of the cellular instance
 *                         to be used.
 * @param pTopicFilterStr  the null-terminated topic string
 *                         to subscribe to; the wildcard '+'
 *                         may be used to specify "all"
 *                         at any one topic level and the
 *                         wildcard '#' may be used at the end
 *                         of the string to indicate "everything
 *                         from here on".  Cannot be NULL.
 * @param maxQos           the maximum MQTT message QoS to
 *                         for this subscription.
 * @return                 the QoS of the subscription else
 *                         negative error code.
 */
int32_t uCellMqttSubscribe(int32_t cellHandle,
                           const char *pTopicFilterStr,
                           uCellMqttQos_t maxQos);

/** Unsubscribe from an MQTT topic.
 *
 * @param cellHandle       the handle of the cellular instance to
 *                         be used.
 * @param pTopicFilterStr  the null-terminated topic string
 *                         to unsubscribe from; the wildcard '+'
 *                         may be used to specify "all"
 *                         at any one topic level and the
 *                         wildcard '#' may be used at the end
 *                         of the string to indicate "everything
 *                         from here on".  Cannot be NULL.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uCellMqttUnsubscribe(int32_t cellHandle,
                             const char *pTopicFilterStr);

/** Set a callback to be called when new messages are
 * available to be read.
 *
 * @param cellHandle      the handle of the cellular instance to
 *                        be used.
 * @param pCallback       the callback. The first parameter to
 *                        the callback will be filled in with
 *                        the number of messages available to
 *                        be read. The second parameter will be
 *                        pCallbackParam. Use NULL to deregister
 *                        a previous callback.
 * @param pCallbackParam  this value will be passed to pCallback
 *                        as the second parameter.
 * @return                zero on success else negative error
 *                        code.
 */
int32_t uCellMqttSetMessageCallback(int32_t cellHandle,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam);

/** Get the current number of unread messages.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           the number of unread messages or negative
 *                   error code.
 */
int32_t uCellMqttGetUnread(int32_t cellHandle);

/** Read an MQTT message.
 *
 * @param cellHandle          the handle of the cellular instance to
 *                            be used.
 * @param pTopicNameStr       a place to put the null-terminated
 *                            topic string of the message; cannot
 *                            be NULL.
 * @param topicNameSizeBytes  the number of bytes of storage
 *                            at pTopicNameStr.
 * @param pMessage            a place to put the message; may be NULL.
 * @param pMessageSizeBytes   on entry this should point to the
 *                            number of bytes of storage at
 *                            pMessage. On return, this will be
 *                            updated to the number of bytes written
 *                            to pMessage.  Ignored if pMessage is NULL.
 * @param pQos                a place to put the QoS of the message;
 *                            may be NULL.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uCellMqttMessageRead(int32_t cellHandle, char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage, size_t *pMessageSizeBytes,
                             uCellMqttQos_t *pQos);

/** Get the last MQTT error code.
 *
 * @param cellHandle the handle of the cellular instance to be used.
 * @return           an error code, the meaning of which is
 *                   utterly module specific.
 */
int32_t uCellMqttGetLastErrorCode(int32_t cellHandle);

/** Determine if MQTT is supported by the given cellHandle.
 *
 * @param cellHandle  the handle of the cellular instance to be used.
 */
bool uCellMqttIsSupported(int32_t cellHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_MQTT_H_

// End of file
