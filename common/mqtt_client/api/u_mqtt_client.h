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

#ifndef _U_MQTT_CLIENT_H_
#define _U_MQTT_CLIENT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_security_tls.h"
#include "u_device.h"
#include "u_mqtt_common.h"

/** \addtogroup MQTT-Client MQTT Client
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox MQTT and MQTT-SN client
 * API. This API is threadsafe except for the pUMqttClientOpen() and
 * uMqttClientClose() functions, which should not be called
 * simultaneously with themselves or any other MQTT client API function.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
/** The maximum amount of time to wait for a response from the
 * MQTT broker in seconds.
 */
# define U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS 30
#endif

/** The defaults for an MQTT connection, see #uMqttClientConnection_t.
 * Whenever an instance of uMqttClientConnection_t is created it
 * should be assigned to this to ensure the correct default
 * settings. */
#define U_MQTT_CLIENT_CONNECTION_DEFAULT {NULL, NULL, NULL, NULL,  \
                                          -1, -1, false, false,    \
                                          NULL, NULL, false, 0}

/** The number of bytes required to store a short MQTT-SN topic name,
 * which will be of the form "xy", two characters plus a null terminator.
 */
#define U_MQTT_CLIENT_SN_TOPIC_NAME_SHORT_LENGTH_BYTES 3

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MQTT connection information.  Note that not all options
 * are supported by all modules and the maximum length of the
 * various string fields may differ between modules.
 * NOTE: if this structure is modified be sure to modify
 * #U_MQTT_CLIENT_CONNECTION_DEFAULT to match.
 */
typedef struct {
    const char *pBrokerNameStr;        /**< the null-terminated name
                                            of the broker for the MQTT
                                            connection.  This may be a
                                            domain name, or an IP
                                            address and may include a
                                            port number.
                                            NOTE: if a domain name is
                                            used the module may
                                            immediately try to perform
                                            a DNS look-up to establish
                                            the IP address of the broker
                                            and hence you should ensure
                                            that the module is
                                            connected beforehand. */
    const char *pUserNameStr;          /**< the null-terminated user name
                                            required by the MQTT broker;
                                            ignored for MQTT-SN. */
    const char *pPasswordStr;          /**< the null-terminated password
                                            required by the MQTT broker;
                                            ignored for MQTT-SN. */
    const char *pClientIdStr;          /**< the null-terminated client ID
                                            for this MQTT connection.
                                            May be NULL (the default),
                                            in which case the driver will
                                            provide a name. */
    int32_t localPort;                 /**< the local port number to be
                                            used by the MQTT client. Set
                                            to -1 (the default) to let the
                                            driver chose.  Note that only
                                            SARA-R412M-02B supports setting
                                            localPort; for all other modules
                                            this value must be left at -1.
                                            This parameter is nothing to do
                                            with the remote port number on the
                                            destination server you wish to
                                            connect to; that is specified in
                                            pBrokerNameStr e.g. mybroker.com:247
                                            to connect to the given remote
                                            server on port 247. */
    int32_t inactivityTimeoutSeconds;  /**< the inactivity timeout used by
                                            the MQTT client.  Set to -1 for
                                            no inactivity timeout (which is
                                            the default). Note that for SARA-R5
                                            cellular modules *setting* a value
                                            of 0 is not permitted, *leaving*
                                            the value at the default of 0 is
                                            permitted. */
    bool keepAlive;                    /**< whether MQTT ping or "keep alive"
                                            is on or off.  If this is true
                                            then an MQTT ping message will be
                                            sent to the broker near the end of
                                            the inactivity timeout to keep the
                                            connection alive.  Defaults to
                                            false. */
    bool retain;                       /**< if set to true then the topic
                                            subscriptions and message queue
                                            status will be kept by both the
                                            client and the broker across MQTT
                                            disconnects/connects. Defaults to
                                            false. The SARA-R5 cellular module
                                            does not support retention. */
    uMqttWill_t *pWill;                /**< a pointer to the MQTT "will"
                                            message that the broker will be
                                            asked to send on an uncommanded
                                            disconnect of the MQTT client;
                                            specify NULL for none (the default).
                                            "will"s are not supported on SARA-R4
                                            cellular modules.
                                            Note: not const because the "will"
                                            data can be updated when the connection
                                            is MQTT-SN. */
    bool (*pKeepGoingCallback) (void); /**< certain of the MQTT API functions
                                            need to wait for the broker to
                                            respond and this may take some
                                            time.  Specify a callback function
                                            here which will be called while this
                                            API is waiting.  pKeepGoingCallback
                                            may be called at any time until the
                                            MQTT sessions is ended.  While the
                                            callback function returns true the
                                            API will continue to wait until success
                                            or #U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
                                            is reached.  If the callback function
                                            returns false then the API will
                                            return. Note that the thing the API
                                            was waiting for may still succeed,
                                            this does not cancel the operation,
                                            it simply stops waiting for the
                                            response.  The callback function may
                                            also be used to feed any application
                                            watchdog timer that may be running.
                                            May be NULL (the default), in which
                                            case the APIs will continue to wait
                                            until success or
                                            #U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
                                            have elapsed. */
    bool mqttSn;                       /**< set to true to use MQTT-SN, else the
                                            connection will be MQTT (the default). */
    int32_t radius;                    /**< applicable to MQTT-SN only; not
                                            currently supported by any u-blox
                                            modules. */
} uMqttClientConnection_t;

/** MQTT context data, used internally by this code and
 * exposed here only so that it can be handed around by the
 * caller.  The contents and, umm, structure of this structure
 * may be changed without notice and should not be relied upon
 * by the caller.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    void *mutexHandle; /* No 'p' prefix as this should be treated as a handle,
                          not using actual type to avoid customer having to drag
                          more headers in for what is an internal structure. */
    void *pPriv; /* Underlying MQTT implementation shall use this void pointer
                    to hold the reference to the internal data structures */
    uSecurityTlsContext_t *pSecurityContext;
    int32_t totalMessagesSent;      /* Total messages sent from MQTT client */
    int32_t totalMessagesReceived;  /* Total messages received by MQTT client */
} uMqttClientContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT AND MQTT-SN
 * -------------------------------------------------------------- */

/** Open an MQTT client session.  The module must be powered
 * up for this function to work.  IMPORTANT: if you re-boot the
 * module after calling this function you will lose all settings
 * and must call uMqttClientClose() followed by pUMqttClientOpen()
 * to put them back again.
 *
 * @param devHandle                the device handle to be used,
 *                                 for example obtained using uDeviceOpen().
 * @param[in] pSecurityTlsSettings a pointer to the security settings to
 *                                 be applied, NULL for no security.
 *                                 If this is non-NULL, don't forget to
 *                                 specify the secure broker port number
 *                                 in uMqttClientConnection_t when
 *                                 calling uMqttClientConnect(), e.g.
 *                                 setting pBrokerNameStr to something
 *                                 like "mybroker.com:8883". Note that
 *                                 some modules (e.g. SARA-R4xx-02B cellular
 *                                 modules) do not support MQTT TLS
 *                                 security.
 * @return                         a pointer to the internal MQTT context
 *                                 structure used by this code or NULL on
 *                                 failure (in which case
 *                                 uMqttClientOpenResetLastError() can
 *                                 be called to obtain an error code).
 */
uMqttClientContext_t *pUMqttClientOpen(uDeviceHandle_t devHandle,
                                       const uSecurityTlsSettings_t *pSecurityTlsSettings);

/** If pUMqttClientOpen() returned NULL this function can be
 * called to find out why.  That error code is reset to "success"
 * by calling this function.
 *
 * @return the last error code from a call to pUMqttClientOpen().
 */
int32_t uMqttClientOpenResetLastError();

/** Close the given MQTT client session.  If the session is
 * connected it will be disconnected first.
 *
 * @param[in] pContext   a pointer to the internal MQTT context
 *                       structure that was originally returned by
 *                       pUMqttClientOpen().
 */
void uMqttClientClose(uMqttClientContext_t *pContext);

/** Connect an MQTT session.  If pKeepGoingCallback()
 * inside pConnection is non-NULL then it will called while this
 * function is waiting for a connection to be made; this function
 * works for both MQTT and MQTT-SN however see also
 * uMqttClientSnConnect().
 *
 * @param[in] pContext     a pointer to the internal MQTT context
 *                         structure that was originally returned by
 *                         pUMqttClientOpen().
 * @param[in] pConnection  the connection information for this
 *                         session.
 * @return                 zero on success or negative error code.
 */
int32_t uMqttClientConnect(uMqttClientContext_t *pContext,
                           const uMqttClientConnection_t *pConnection);

/** Disconnect an MQTT session.
 *
 * @param[in] pContext   a pointer to the internal MQTT context
 *                       structure that was originally returned by
 *                       pUMqttClientOpen().
 * @return               zero on success or negative error code.
 */
int32_t uMqttClientDisconnect(const uMqttClientContext_t *pContext);

/** Determine whether the given MQTT session is connected or not.
 *
 * @param[in] pContext   a pointer to the internal MQTT context
 *                       structure that was originally returned by
 *                       pUMqttClientOpen().
 * @return               true if the MQTT session is connected else
 *                       false.
 */
bool uMqttClientIsConnected(const uMqttClientContext_t *pContext);

/** Set a callback to be called when new messages are available to
 * be read.  The callback may then call uMqttClientGetUnread() to get
 * the number of unread messages.
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
 * @param[in] pContext        a pointer to the internal MQTT context
 *                            structure that was originally returned
 *                            by pUMqttClientOpen().
 * @param[in] pCallback       the callback. The first parameter to
 *                            the callback will be filled in with
 *                            the number of messages available to
 *                            be read. The second parameter will be
 *                            pCallbackParam.  Use NULL to deregister
 *                            a previous callback.
 * @param[in] pCallbackParam  this value will be passed to pCallback
 *                            as the second parameter.
 * @return                    zero on success else negative error code.
 */
int32_t uMqttClientSetMessageCallback(const uMqttClientContext_t *pContext,
                                      void (*pCallback) (int32_t, void *),
                                      void *pCallbackParam);

/** Get the current number of unread messages.
 *
 * @param[in] pContext  a pointer to the internal MQTT context
 *                      structure that was originally returned
 *                      by pUMqttClientOpen().
 * @return              the number of unread messages or negative
 *                      error code.
 */
int32_t uMqttClientGetUnread(const uMqttClientContext_t *pContext);

/** Get the last MQTT client error code.
 *
 * @param[in] pContext  a pointer to the internal MQTT context
 *                      structure that was originally returned
 *                      by pUMqttClientOpen().
 * @return              an error code, the meaning of which is
 *                      utterly module specific.
 */
int32_t uMqttClientGetLastErrorCode(const uMqttClientContext_t *pContext);

/** Get the total number of message sent by the MQTT client.
 *
 * @param[in] pContext  a pointer to the internal MQTT context.
 * @return              total number of messages published,
 *                      or negative error code.
 */
int32_t uMqttClientGetTotalMessagesSent(const uMqttClientContext_t *pContext);

/** Get the total number of messages received and read by the MQTT client.
 *
 * @param[in] pContext  a pointer to the internal MQTT context.
 * @return              total number of messages received and read,
 *                      or negative error code.
 */
int32_t uMqttClientGetTotalMessagesReceived(const uMqttClientContext_t *pContext);

/** Set a callback to be called if the broker drops the MQTT
 * connection.
 *
 * @param[in] pContext       a pointer to the internal MQTT context
 *                           structure that was originally returned
 *                           by pUMqttClientOpen().
 * @param[in] pCallback      the callback. The first parameter is the
 *                           error code, as would be returned by
 *                           uCellMqttClientGetLastErrorCode(), the
 *                           second parameter is pCallbackParam. Use
 *                           NULL to deregister a previous callback.
 * @param[in] pCallbackParam this value will be passed to pCallback.
 * @return                   zero on success else negative error code.
 */
int32_t uMqttClientSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                         void (*pCallback) (int32_t, void *),
                                         void *pCallbackParam);

/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT ONLY
 * -------------------------------------------------------------- */

/** MQTT only: publish an MQTT message. If pKeepGoingCallback()
 * inside the pConnection structure passed to uMqttClientConnect() was
 * non-NULL then it will called while this function is waiting for the
 * publish to complete.
 *
 * @param[in] pContext      a pointer to the internal MQTT context
 *                          structure that was originally returned
 *                          by pUMqttClientOpen().
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
 * @param qos               the MQTT QoS to use for this message.
 * @param retain            if true the message will be kept
 *                          by the broker across MQTT disconnects/
 *                          connects, else it will be cleared.
 * @return                  zero on success else negative error code.
 */
int32_t uMqttClientPublish(uMqttClientContext_t *pContext,
                           const char *pTopicNameStr,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uMqttQos_t qos, bool retain);

/** MQTT only: subscribe to an MQTT topic. If pKeepGoingCallback()
 * inside the pConnection structure passed to uMqttClientConnect()
 * was non-NULL it will be called while this function is waiting
 * for a subscription to complete.
 *
 * @param[in] pContext         a pointer to the internal MQTT context
 *                             structure that was originally returned
 *                             by pUMqttClientOpen().
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to subscribe to; the wildcard '+'
 *                             may be used to specify "all"
 *                             at any one topic level and the
 *                             wildcard '#' may be used at the end
 *                             of the string to indicate "everything
 *                             from here on".  Cannot be NULL.
 * @param maxQos               the maximum MQTT message QoS to
 *                             for this subscription.
 * @return                     the QoS of the subscription else negative
 *                             error code.
 */
int32_t uMqttClientSubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr,
                             uMqttQos_t maxQos);

/** MQTT only: unsubscribe from an MQTT topic.
 *
 * @param[in] pContext         a pointer to the internal MQTT context
 *                             structure that was originally returned
 *                             by pUMqttClientOpen().
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to unsubscribe from; the wildcard '+'
 *                             may be used to specify "all"
 *                             at any one topic level and the
 *                             wildcard '#' may be used at the end
 *                             of the string to indicate "everything
 *                             from here on".  Cannot be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uMqttClientUnsubscribe(const uMqttClientContext_t *pContext,
                               const char *pTopicFilterStr);

/** MQTT only: read an MQTT message.
 *
 * @param[in] pContext              a pointer to the internal MQTT context
 *                                  structure that was originally returned
 *                                  by pUMqttClientOpen().
 * @param[out] pTopicNameStr        a place to put the null-terminated
 *                                  topic string of the message; cannot
 *                                  be NULL.
 * @param topicNameSizeBytes        the number of bytes of storage at
 *                                  pTopicNameStr.
 * @param[out] pMessage             a place to put the message; may be NULL.
 * @param[in,out] pMessageSizeBytes on entry this should point to the
 *                                  number of bytes of storage at
 *                                  pMessage. On return, this will be
 *                                  updated to the number of bytes written
 *                                  to pMessage.  Ignored if pMessge is NULL.
 * @param[out] pQos                 a place to put the QoS of the message;
 *                                  may be NULL.
 * @return                          zero on success else negative error code.
 */
int32_t uMqttClientMessageRead(uMqttClientContext_t *pContext,
                               char *pTopicNameStr,
                               size_t topicNameSizeBytes,
                               char *pMessage,
                               size_t *pMessageSizeBytes,
                               uMqttQos_t *pQos);

/* ----------------------------------------------------------------
 * FUNCTIONS: MQTT-SN ONLY
 * -------------------------------------------------------------- */

/** Determine if MQTT-SN is supported.
 *
 * @param[in] pContext a pointer to the internal MQTT context
 *                     structure that was originally returned
 *                     by pUMqttClientOpen().
 * @return             true if MQTT-SN is supported, else false.
 */
bool uMqttClientSnIsSupported(const uMqttClientContext_t *pContext);

/** This may seem a bit peculiar.  uMqttClientSnConnect() performs
 * the same function as uMqttClientConnect(), however it gives
 * the option of doing all of the connection setup but NOT actually
 * performing the connection.  This is useful if you only wish to
 * call uMqttClientSnPublish() with uMqttQos_t set to
 * #U_MQTT_QOS_SEND_AND_FORGET; that will work WITHOUT a
 * connection to the MQTT-SN broker, saving you time and money.
 * Of course, to use a different MQTT QoS, or to subscribe to topics
 * on the broker etc. doNotConnect must be set to false (or you
 * may just use uMqttClientConnect() as normal).
 *
 * @param[in] pContext     a pointer to the internal MQTT context
 *                         structure that was originally returned by
 *                         pUMqttClientOpen().
 * @param[in] pConnection  the connection information for this
 *                         session.
 * @param doNotConnect     if set to true then all of the connection
 *                         parameters will be applied, locally, but
 *                         there will be no communication with the
 *                         MQTT-SN broker, no connection will be made;
 *                         if set to false this function is identical
 *                         in operation to uMqttClientConnect().
 * @return                 zero on success or negative error code.
 */
int32_t uMqttClientSnConnect(uMqttClientContext_t *pContext,
                             const uMqttClientConnection_t *pConnection,
                             bool doNotConnect);

/** Convenience function to populate an MQTT-SN topic name with
 * a predefined MQTT-SN topic ID.
 *
 * @param topicId         the predefined MQTT-SN topic ID.
 * @param[out] pTopicName a pointer to the MQTT-SN topic name to populate;
 *                        cannot be NULL.
 * @return                zero on success, else negative error code.
 */
int32_t uMqttClientSnSetTopicIdPredefined(uint16_t topicId,
                                          uMqttSnTopicName_t *pTopicName);

/** Convenience function to populate an MQTT-SN topic name with
* an MQTT-SN short topic name string.
 *
 * @param[in]  pTopicNameShortStr a pointer to the short topic name
 *                                string; cannot be NULL, must be
 *                                a null-terminated string that is
 *                                exactly two characters long, for
 *                                example "xy"; single character short
 *                                names are not permitted.
 * @param[out] pTopicName         a pointer to the MQTT-SN topic name to
 *                                populate; cannot be NULL.
 * @return                        zero on success, else negative error code.
 */
int32_t uMqttClientSnSetTopicNameShort(const char *pTopicNameShortStr,
                                       uMqttSnTopicName_t *pTopicName);

/** Convenience function to get the type of an MQTT-SN topic name.
 *
 * @param[in] pTopicName the MQTT-SN topic name; cannot be NULL.
 * @return               the MQTT-SN topic name type.
 */
uMqttSnTopicNameType_t uMqttClientSnGetTopicNameType(const uMqttSnTopicName_t *pTopicName);

/** Convenience function to get the ID from an MQTT-SN topic name.
 *
 * @param[in] pTopicName the MQTT-SN topic name; cannot be NULL.
 * @return               the topic ID or negative error code if
 *                       pTopicName does not contain a topic ID.
 */
int32_t uMqttClientSnGetTopicId(const uMqttSnTopicName_t *pTopicName);

/** Convenience function to get the short name from an MQTT-SN
 * topic name.
 *
 * @param[in]  pTopicName         the MQTT-SN topic name; cannot be NULL.
 * @param[out] pTopicNameShortStr a place to put the short name string; must
 *                                be a buffer of length at least
 *                                #U_MQTT_CLIENT_SN_TOPIC_NAME_SHORT_LENGTH_BYTES.
 *                                A null-terminator will be added. Cannot be NULL.
 * @return                        if pTopicName contained a short name,
 *                                the number of bytes copied to
 *                                pTopicNameShortStr is returned, else
 *                                negative error code.
 */
int32_t uMqttClientSnGetTopicNameShort(const uMqttSnTopicName_t *pTopicName,
                                       char *pTopicNameShortStr);

/** MQTT-SN only: ask the MQTT-SN broker for an MQTT-SN topic name
 * for the given normal MQTT topic name; if you wish to publish to
 * a normal MQTT topic, for example "thing/this", using MQTT-SN,
 * which only has a 16-bit topic field, then you must register the
 * normal MQTT topic to obtain an MQTT-SN topic ID for it.
 * Note: if you intend to subscribe to an MQTT topic as well as
 * publish to an MQTT topic you do NOT need to use this function:
 * instead use the pTopicName returned by
 * uMqttClientSnSubscribeNormalTopic().
 * Note: this function should not be used for MQTT-SN short topic
 * names (e.g. "xy") because they already fit into 16-bits; just use
 * uMqttClientSnSetTopicNameShort() to create the topic name and
 * use it with uMqttClientSnSubscribe().
 * Note that this does NOT subscribe to the topic, it just gets you
 * an ID, you need to call uMqttClientSnSubscribe() to do the
 * subscribing.
 * Must be connected to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext       a pointer to the internal MQTT context.
 * @param[in] pTopicNameStr  the null-terminated topic name string;
 *                           cannot be NULL.
 * @param[out] pTopicName    a place to put the MQTT-SN topic name;
 *                           cannot be NULL.
 * @return                   zero on success, else negative error code.
 */
int32_t uMqttClientSnRegisterNormalTopic(const uMqttClientContext_t *pContext,
                                         const char *pTopicNameStr,
                                         uMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: publish a message; this differs from uMqttClientPublish()
 * in that it uses an MQTT-SN topic name, either created with
 * uMqttClientSnSetTopicIdPredefined()/ uMqttClientSnSetTopicNameShort()
 * or as returned by uMqttClientSnRegisterNormalTopic()/
 * uMqttClientSnSubscribeNormalTopic()).
 * Must be connected to an MQTT-SN broker for this to work.
 * If pKeepGoingCallback() inside the pConnection structure passed to
 * uMqttClientConnect() was non-NULL then it will called while this
 * function is waiting for the publish to complete.
 *
 * @param[in] pContext           a pointer to the internal MQTT context
 *                               structure that was originally returned
 *                               by pUMqttClientOpen().
 * @param[in] pTopicName         the MQTT-SN topic name; cannot be NULL.
 * @param[in] pMessage           a pointer to the message; the message
 *                               is not restricted to ASCII values.
 *                               Cannot be NULL.
 * @param messageSizeBytes       since pMessage may include binary
 *                               content, including NULLs, this
 *                               parameter specifies the length of
 *                               pMessage. If pMessage happens to
 *                               be an ASCII string this parameter
 *                               should be set to strlen(pMessage).
 * @param qos                    the MQTT QoS to use for this message.
 * @param retain                 if true the message will be kept
 *                               by the broker across MQTT disconnects/
 *                               connects, else it will be cleared.
 * @return                       zero on success else negative error code.
 */
int32_t uMqttClientSnPublish(uMqttClientContext_t *pContext,
                             const uMqttSnTopicName_t *pTopicName,
                             const char *pMessage,
                             size_t messageSizeBytes,
                             uMqttQos_t qos, bool retain);

/** MQTT-SN only: subscribe to an MQTT-SN topic; this differs from
 * uMqttClientSubscribe() in that it takes an MQTT-SN topic name,
 * instead of a filter string, as the topic parameter.  To subscribe
 * to an MQTT topic, e.g. "bibble/blah", use
 * uMqttClientSnSubscribeNormalTopic() instead.  Must be connected to
 * an MQTT-SN broker for this to work.  If pKeepGoingCallback()
 * inside the pConnection structure passed to uMqttClientConnect()
 * was non-NULL it will be called while this function is waiting
 * for a subscription to complete.
 *
 * @param[in] pContext     a pointer to the internal MQTT context
 *                         structure that was originally returned
 *                         by pUMqttClientOpen().
 * @param[in] pTopicName   the MQTT topic name to subscribe to;
 *                         cannot be NULL.
 * @param maxQos           the maximum MQTT message QoS for this
 *                         subscription.
 * @return                 the QoS of the subscription else negative
 *                         error code.
 */
int32_t uMqttClientSnSubscribe(const uMqttClientContext_t *pContext,
                               const uMqttSnTopicName_t *pTopicName,
                               uMqttQos_t maxQos);

/** MQTT-SN only: subscribe to a normal MQTT topic; this differs
 * from uMqttClientSubscribe() in that it can return pTopicName,
 * allowing MQTT-SN publish/read operations to be carried out on
 * a normal MQTT topic.  Must be connected to an MQTT-SN broker for
 * this to work.  If pKeepGoingCallback() inside the pConnection
 * structure passed to uMqttClientConnect() was non-NULL it will
 * be called while this function is waiting for a subscription to
 * complete.
 *
 * @param[in] pContext            a pointer to the internal MQTT context
 *                                structure that was originally returned
 *                                by pUMqttClientOpen().
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
int32_t uMqttClientSnSubscribeNormalTopic(const uMqttClientContext_t *pContext,
                                          const char *pTopicFilterStr,
                                          uMqttQos_t maxQos,
                                          uMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: unsubscribe from an MQTT-SN topic; this differs from
 * uMqttClientSubscribe() in that it takes an MQTT-SN topic name,
 * instead of a filter string, as the topic parameter.  To unsubscribe
 * from an MQTT topic, e.g. "other/thing", use
 * uMqttClientSnUnsubscribeNormalTopic() instead.  Must be connected
 * to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext         a pointer to the internal MQTT context
 *                             structure that was originally returned
 *                             by pUMqttClientOpen().
 * @param[in] pTopicName       the MQTT-SN topic name to unsubscribe from;
 *                             cannot be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uMqttClientSnUnsubscribe(const uMqttClientContext_t *pContext,
                                 const uMqttSnTopicName_t *pTopicName);

/** MQTT-SN only: unsubscribe from a normal MQTT topic.  Must be
 * connected to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext         a pointer to the internal MQTT context
 *                             structure that was originally returned
 *                             by pUMqttClientOpen().
 * @param[in] pTopicFilterStr  the null-terminated topic string
 *                             to unsubscribe from. The wildcard '+' may
 *                             be used to specify "all" at any one topic
 *                             level and the wildcard '#' may be used
 *                             at the end of the string to indicate
 *                             "everything from here on".  Cannot be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uMqttClientSnUnsubscribeNormalTopic(const uMqttClientContext_t *pContext,
                                            const char *pTopicFilterStr);

/** MQTT-SN only: read a message, must be used to read messages when
 * an MQTT-SN connection is in place; it differs from
 * uMqttClientMessageRead() in that it uses an MQTT-SN topic name;
 * if the message is from a normal MQTT topic then the topic name will
 * be populated with the MQTT-SN topic ID that you received when
 * you called uMqttClientSnSubscribeNormalTopic().
 * Must be connected to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext              a pointer to the internal MQTT context
 *                                  structure that was originally returned
 *                                  by pUMqttClientOpen().
 * @param[out] pTopicName           a place to put the MQTT-SN topic name;
 *                                  cannot be NULL.
 * @param[out] pMessage             a place to put the message; may be NULL.
 * @param[in,out] pMessageSizeBytes on entry this should point to the
 *                                  number of bytes of storage at
 *                                  pMessage. On return, this will be
 *                                  updated to the number of bytes written
 *                                  to pMessage.  Ignored if pMessge is NULL.
 * @param[out] pQos                 a place to put the QoS of the message;
 *                                  may be NULL.
 * @return                          zero on success else negative error code.
 */
int32_t uMqttClientSnMessageRead(uMqttClientContext_t *pContext,
                                 uMqttSnTopicName_t *pTopicName,
                                 char *pMessage,
                                 size_t *pMessageSizeBytes,
                                 uMqttQos_t *pQos);

/** MQTT-SN only: notify the MQTT-SN broker that the "will" message
 * has been updated.  Call this if you change the "will" message that
 * was in the "will" structure pointed-to by the pWill parameter of the
 * pConnection structure passed to uMqttClientConnect().  Note that the
 * underlying AT interface for this command ONLY works if the "will"
 * message is a null-terminated ASCII string containing printable
 * characters (i.e. isprint() returns true) and no double quotation
 * marks (").  Note also that you cannot delete an existing or create
 * a new "will" message with this mechanism, you can only modify the one
 * that was pointed-to by the pWill parameter of the pConnection
 * structure; you may, of course, set the "will" message to be an empty
 * string (""), though what effect that has will depend upon your MQTT-SN
 * broker.  Must be connected to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext a pointer to the internal MQTT context.
 * @return             zero on success else negative error code.
 */
int32_t uMqttClientSnWillMessageUpdate(const uMqttClientContext_t *pContext);

/** MQTT-SN only: notify the MQTT-SN broker that the topic, QOS or
 * retention parameters of the "will" message have been updated.
 * Call this if you change any of those parameters in the structure that
 * was pointed-to by the pWill member of the pConnection structure that
 * was passed to uMqttClientConnect().  Note that if a change is made to
 * the "will" message then uMqttClientSnWillMessageUpdate() must [also] be
 * called.  Must be connected to an MQTT-SN broker for this to work.
 *
 * @param[in] pContext a pointer to the internal MQTT context.
 * @return             zero on success else negative error code.
 */
int32_t uMqttClientSnWillParametersUpdate(const uMqttClientContext_t *pContext);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_MQTT_CLIENT_H_

// End of file
