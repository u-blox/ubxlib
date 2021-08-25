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

#ifndef _U_MQTT_CLIENT_H_
#define _U_MQTT_CLIENT_H_

/* This file breaks the usual inclusion rules: u_security_tls.h
 * is an internal API that this API hides, hence it is allowed to
 * be included here.
 */
#include "u_security_tls.h"

/** @file
 * @brief This header file defines the u-blox MQTT client API.  This
 * API is threadsafe except for the pUMqttClientOpen() and
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

/** The defaults for an MQTT connection, see uMqttClientConnection_t.
 * Whenever an instance of uMqttClientConnection_t is created it
 * should be assigned to this to ensure the correct default
 * settings. */
#define U_MQTT_CLIENT_CONNECTION_DEFAULT {NULL, NULL, NULL, NULL,  \
                                          -1, -1, false, false,    \
                                          NULL, NULL}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MQTT connection information.  Note that not all options
 * are supported by all modules and the maximum length of the
 * various string fields may differ between modules.
 * NOTE: if this structure is modified be sure to modify
 * U_MQTT_CLIENT_CONNECTION_DEFAULT to match.
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
                                            required by the MQTT broker. */
    const char *pPasswordStr;          /**< the null-terminated password
                                            required by the MQTT broker. */
    const char *pClientIdStr;          /**< the null-terminated client ID
                                            for this MQTT connection.
                                            May be NULL (the default),
                                            in which case the driver will
                                            provide a name. */
    int32_t localPort;                 /**< the local port number to be
                                            used by the MQTT client. Set
                                            to -1 (the default) to let the
                                            driver chose (in which case the
                                            IANA assigned ports of 1883 for
                                            non-secure  MQTT or 8883 for
                                            TLS secured MQTT will be used).
                                            The SARA-R5 cellular module does
                                            not support setting localPort,
                                            this value must be left at -1. */
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
                                            false. The SARA-R5 celllular module
                                            does not support retention. */
    const uMqttWill_t *pWill;          /**< a pointer to the MQTT "will"
                                            message that the broker will be
                                            asked to send on an uncommanded
                                            disconnect of the MQTT client;
                                            specify NULL for none (the default).
                                            "will"s are not supported on SARA-R4
                                            cellular modules. */
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
                                            or U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
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
                                            U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS
                                            have elapsed. */
} uMqttClientConnection_t;

/** MQTT context data, used internally by this code and
 * exposed here only so that it can be handed around by the
 * caller.  The contents and, umm, structure of this structure
 * may be changed without notice and should not be relied upon
 * by the caller.
 */
typedef struct {
    int32_t networkHandle;
    void *mutexHandle; /* No 'p' prefix as this should be treated as a handle,
                          not using actual type to avoid customer having to drag
                          more headers in for what is an internal structure. */
    uSecurityTlsContext_t *pSecurityContext;
    int32_t totalMessagesSent;      /* Total messages sent from MQTT client */
    int32_t totalMessagesReceived;  /* Total messages received by MQTT client */
} uMqttClientContext_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open an MQTT client session.  The module must be powered
 * up for this function to work.  IMPORTANT: if you re-boot the
 * module after calling this function you will lose all settings
 * and must call uMqttClientClose() followed by pUMqttClientOpen()
 * to put them back again.
 *
 * @param networkHandle        the handle of the instance to be used,
 *                             e.g. as returned by uNetworkAdd().
 * @param pSecurityTlsSettings a pointer to the security settings to
 *                             be applied, NULL for no security.
 *                             If this is non-NULL, don't forget to
 *                             specify the secure broker port number
 *                             in uMqttClientConnection_t when
 *                             calling uMqttClientConnect(), e.g.
 *                             setting pBrokerNameStr to something
 *                             like "mybroker.com:8883". Note that
 *                             some modules (e.g. SARA-R4xx-02B cellular
 *                             modules) do not support MQTT TLS
 *                             security.
 * @return                     a pointer to the internal MQTT context
 *                             structure used by this code or NULL on
 *                             failure (in which case
 *                             uMqttClientOpenResetLastError() can
 *                             be called to obtain an error code).
 */
uMqttClientContext_t *pUMqttClientOpen(int32_t networkHandle,
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
 * @param pContext   a pointer to the internal MQTT context
 *                   structure that was originally returned by
 *                   pUMqttClientOpen().
 */
void uMqttClientClose(uMqttClientContext_t *pContext);

/** Connect an MQTT session.  If pKeepGoingCallback()
 * inside pConnection is non-NULL then it will called while
 * this function is waiting for a connection to be made.
 *
 * @param pContext     a pointer to the internal MQTT context
 *                     structure that was originally returned by
 *                     pUMqttClientOpen().
 * @param pConnection  the connection information for this
 *                     session.
 * @return             zero on success or negative error code.
 */
int32_t uMqttClientConnect(const uMqttClientContext_t *pContext,
                           const uMqttClientConnection_t *pConnection);

/** Disconnect an MQTT session.
 *
 * @param pContext   a pointer to the internal MQTT context
 *                   structure that was originally returned by
 *                   pUMqttClientOpen().
 * @return           zero on success or negative error code.
 */
int32_t uMqttClientDisconnect(const uMqttClientContext_t *pContext);

/** Determine whether the given MQTT session is connected or not.
 *
 * @param pContext   a pointer to the internal MQTT context
 *                   structure that was originally returned by
 *                   pUMqttClientOpen().
 * @return           true if the MQTT session is connected else
 *                   false.
 */
bool uMqttClientIsConnected(const uMqttClientContext_t *pContext);

/** Publish an MQTT message. If pKeepGoingCallback() inside
 * the pConnection structure passed to uMqttClientConnect() was
 * non-NULL then it will called while this function is waiting
 * for publish to complete.
 *
 * @param pContext         a pointer to the internal MQTT context
 *                         structure that was originally returned
 *                         by pUMqttClientOpen().
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
 * @param retain           if true the message will be kept
 *                         by the broker across MQTT disconnects/
 *                         connects, else it will be cleared.
 * @return                 zero on success else negative error
 *                         code.
 */
int32_t uMqttClientPublish(uMqttClientContext_t *pContext,
                           const char *pTopicNameStr,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uMqttQos_t qos, bool retain);

/** Subscribe to an MQTT topic. If pKeepGoingCallback() inside
 * the pConnection structure passed to uMqttClientConnect() was non-NULL
 * it will be called while this function is waiting for a
 * subscription to complete.
 *
 * @param pContext         a pointer to the internal MQTT context
 *                         structure that was originally returned
 *                         by pUMqttClientOpen().
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
int32_t uMqttClientSubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr,
                             uMqttQos_t maxQos);

/** Unsubscribe from an MQTT topic.
 *
 * @param pContext         a pointer to the internal MQTT context
 *                         structure that was originally returned
 *                         by pUMqttClientOpen().
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
int32_t uMqttClientUnsubscribe(const uMqttClientContext_t *pContext,
                               const char *pTopicFilterStr);

/** Set a callback to be called when new messages are
 * available to be read.  The callback may then call
 * uMqttClientGetUnread() to read the messages.
 *
 * @param pContext        a pointer to the internal MQTT context
 *                        structure that was originally returned
 *                        by pUMqttClientOpen().
 * @param pCallback       the callback. The first parameter to
 *                        the callback will be filled in with
 *                        the number of messages available to
 *                        be read. The second parameter will be
 *                        pCallbackParam.  Use NULL to deregister
 *                        a previous callback.
 * @param pCallbackParam  this value will be passed to pCallback
 *                        as the second parameter.
 * @return                zero on success else negative error
 *                        code.
 */
int32_t uMqttClientSetMessageCallback(const uMqttClientContext_t *pContext,
                                      void (*pCallback) (int32_t, void *),
                                      void *pCallbackParam);

/** Get the current number of unread messages.
 *
 * @param pContext  a pointer to the internal MQTT context
 *                  structure that was originally returned
 *                  by pUMqttClientOpen().
 * @return          the number of unread messages or negative
 *                  error code.
 */
int32_t uMqttClientGetUnread(const uMqttClientContext_t *pContext);

/** Read an MQTT message.
 *
 * @param pContext            a pointer to the internal MQTT context
 *                            structure that was originally returned
 *                            by pUMqttClientOpen().
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
 *                            to pMessage.  Ignored if pMessge is NULL.
 * @param pQos                a place to put the QoS of the message;
 *                            may be NULL.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uMqttClientMessageRead(uMqttClientContext_t *pContext,
                               char *pTopicNameStr,
                               size_t topicNameSizeBytes,
                               char *pMessage,
                               size_t *pMessageSizeBytes,
                               uMqttQos_t *pQos);

/** Get the last MQTT client error code.
 *
 * @param pContext      a pointer to the internal MQTT context
 *                      structure that was originally returned
 *                      by pUMqttClientOpen().
 * @return              an error code, the meaning of which is
 *                      utterly module specific.
 */
int32_t uMqttClientGetLastErrorCode(const uMqttClientContext_t *pContext);

/** Get the total number of message sent by MQTT client
 *
 * @param pContext      a pointer to the internal MQTT context
 *
 * @return              total number of messages published,
 *                      or negative error code.
 */
int32_t uMqttClientGetTotalMessagesSent(const uMqttClientContext_t *pContext);

/** Get the total number of messages received and read by MQTT client
 *
 * @param pContext      a pointer to the internal MQTT context
 *
 * @return              total number of messages received and read,
 *                      or negative error code.
 */
int32_t uMqttClientGetTotalMessagesReceived(const uMqttClientContext_t *pContext);

#ifdef __cplusplus
}
#endif

#endif // _U_MQTT_CLIENT_H_

// End of file
