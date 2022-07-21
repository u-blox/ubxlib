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

#ifndef _U_WIFI_MQTT_H_
#define _U_WIFI_MQTT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the MQTT APIs for WiFi.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef U_WIFI_MQTT_BUFFER_SIZE
#define U_WIFI_MQTT_BUFFER_SIZE 4096
#endif

#ifndef U_WIFI_MQTT_WRITE_TIMEOUT_MS
#define U_WIFI_MQTT_WRITE_TIMEOUT_MS 500
#endif

/** The maximum number of connections that can be open at one time.
 */
#define U_WIFI_MQTT_MAX_NUM_CONNECTIONS 7


typedef enum {
    U_WIFI_MQTT_QOS_AT_MOST_ONCE = 0,
    U_WIFI_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_WIFI_MQTT_QOS_EXACTLY_ONCE = 2,
    U_WIFI_MQTT_QOS_MAX_NUM
} uWifiMqttQos_t;

/* ----------------------------------------------------------------
 * FUNCTIONS:
 * -------------------------------------------------------------- */

/** Initialise the WiFi MQTT client. If the client is already
 *  initialised then this function returns #U_ERROR_COMMON_SUCCESS
 *
 *  @param devHandle          the handle of the wifi instance to be used.
 *  @param[out] ppMqttSession pointer to MQTT session will be allocated and returned.
 *  @return                   zero on success or negative error code
 */
int32_t uWifiMqttInit(uDeviceHandle_t devHandle, void **ppMqttSession);

/** Allocate a new MQTT session
 *
 * @param[in,out] pContext client context returned by pUMqttClientOpen().
 * @param[in] pConnection  connection information for this session.
 * @return                 zero on success or negative error code
 */
int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                         const uMqttClientConnection_t *pConnection);

/** Disconnect from MQTT broker
 *
 * @param[in] pContext client context returned by pUMqttClientOpen().
 * @return             zero on success or negative error code.
 */
int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext);

/** Publish topic on connected MQTT session.
 *
 * @param[in] pContext      client context returned by pUMqttClientOpen().
 * @param[in] pTopicNameStr pointer to topic string.
 * @param[in] pMessage      pointer to message buffer that need to be published.
 * @param messageSizeBytes  size of the message buffer.
 * @param qos               qos of the message.
 * @param retain            set to true if the message need to be retained by the broker
 *                          between connect and disconnect.
 * @return                  zero on success or negative error code.
 */
int32_t uWifiMqttPublish(const uMqttClientContext_t *pContext,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uMqttQos_t qos,
                         bool retain);

/** Set a callback to be called when new messages are
 * available to be read.  The callback may then call
 * uMqttClientGetUnread() to read the messages.
 *
 * @param[in] pContext       client context returned by pUMqttClientOpen().
 * @param[in] pCallback      the callback. The first parameter to
 *                           the callback will be filled in with
 *                           the number of messages available to
 *                           be read. The second parameter will be
 *                           pCallbackParam.  Use NULL to deregister
 *                           a previous callback.
 * @param[in] pCallbackParam this value will be passed to pCallback
 *                           as the second parameter.
 * @return                   zero on success else negative error
 *                           code.
 */
int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam);

/** Set a callback to be called if the MQTT client disconnects
 * from the boker. WiFi MQTT client triggers disconnect callback
 * Error code will be set to #U_ERROR_COMMON_TIMEOUT,
 * when the connection to broker fails during connection initiation.
 * Error code will be set to #U_ERROR_COMMON_SUCCESS,
 * when the disconnection initiated by the user using
 * uWifiMqttClose() or uWifiMqttDisconnect()
 *
 * @param[in] pContext        client context returned by pUMqttClientOpen().
 * @param[in] pCallback       the callback. The first parameter is the
 *                            error code,second parameter is pCallbackParam.
 * @param[in] pCallbackParam  this value will be passed to pCallback.
 * @return                    zero on success else negative error
 *                            code.
 */
int32_t uWifiMqttSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam);

/** Subscribe topic on connected MQTT session.
 *
 * @param[in] pContext        client context returned by pUMqttClientOpen().
 * @param[in] pTopicFilterStr pointer to topic string that need to be subscribed
 * @param maxQos              qos of the message.
 * @return                    zero on success or negative error code.
 */
int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                           const char *pTopicFilterStr,
                           uMqttQos_t maxQos);

/** Unsubscribe from topic on connected MQTT session.
 *
 * @param[in] pContext        client context returned by pUMqttClientOpen().
 * @param[in] pTopicFilterStr pointer to topic string that need to be unsubscribed.
 * @return                    zero on success or negative error code.
 */
int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                             const char *pTopicFilterStr);

/** Close connected MQTT session. This API will disconnect from the broker if connected
 * followed by releasing all the resources associated to that particular session
 *
 * @param[in] pContext client context returned by pUMqttClientOpen().
 */
void uWifiMqttClose(uMqttClientContext_t *pContext);

/** Get total number of unread messages in a given MQTT session.
 *
 * @param[in] pContext client context returned by pUMqttClientOpen().
 * @return             zero on success or negative error code.
 */
int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext);

/** Read messages and their corresponding topics for a given MQTT session.
 *
 * @param[in] pContext           client context returned by pUMqttClientOpen().
 * @param[out] pTopicNameStr     user should provide empty buffer of topicNameSizeBytes.
 * @param topicNameSizeBytes     topicNameSizeBytes should be >= minimum length of topic string.
 * @param[out] pMessage          user should provide empty buffer of size pMessageSizeBytes.
 * @param[out] pMessageSizeBytes pMessageSizeBytes should be >= minimum length of topic buffer.
 * @param pQos                   retrieve the QOS of the message.
 * @return                       zero on success or negative error code.
 *
 */
int32_t uWifiMqttMessageRead(const uMqttClientContext_t *pContext,
                             char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage,
                             size_t *pMessageSizeBytes,
                             uMqttQos_t *pQos);

/** Check if we are connected to the given MQTT session.
 *
 * @param[in] pContext            client context returned by pUMqttClientOpen().
 * @return                    true/false.
 */
bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext);


#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_MQTT_H_

// End of file
