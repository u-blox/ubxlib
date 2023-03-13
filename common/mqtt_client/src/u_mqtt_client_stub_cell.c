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
 * @brief Stubs to allow the MQTT Client API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when cellular is not included in the build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_device.h"
#include "u_cell_mqtt.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uCellMqttInit(uDeviceHandle_t cellHandle, const char *pBrokerNameStr,
                             const char *pClientIdStr, const char *pUserNameStr,
                             const char *pPasswordStr,
                             bool (*pKeepGoingCallback)(void),
                             bool mqttSn)
{
    (void) cellHandle;
    (void) pBrokerNameStr;
    (void) pClientIdStr;
    (void) pUserNameStr;
    (void) pPasswordStr;
    (void) pKeepGoingCallback;
    (void) mqttSn;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uCellMqttDeinit(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
}

U_WEAK int32_t uCellMqttSetLocalPort(uDeviceHandle_t cellHandle, uint16_t port)
{
    (void) cellHandle;
    (void) port;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetInactivityTimeout(uDeviceHandle_t cellHandle,
                                             size_t seconds)
{
    (void) cellHandle;
    (void) seconds;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetKeepAliveOn(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetRetainOn(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetSecurityOn(uDeviceHandle_t cellHandle,
                                      int32_t securityProfileId)
{
    (void) cellHandle;
    (void) securityProfileId;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetWill(uDeviceHandle_t cellHandle,
                                const char *pTopicNameStr,
                                const char *pMessage,
                                size_t messageSizeBytes,
                                uCellMqttQos_t qos, bool retain)
{
    (void) cellHandle;
    (void) pTopicNameStr;
    (void) pMessage;
    (void) messageSizeBytes;
    (void) qos;
    (void) retain;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttConnect(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttDisconnect(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellMqttIsConnected(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellMqttSetMessageCallback(uDeviceHandle_t cellHandle,
                                           void (*pCallback) (int32_t, void *),
                                           void *pCallbackParam)
{
    (void) cellHandle;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttGetUnread(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttGetLastErrorCode(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSetDisconnectCallback(uDeviceHandle_t cellHandle,
                                              void (*pCallback) (int32_t, void *),
                                              void *pCallbackParam)
{
    (void) cellHandle;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellMqttIsSupported(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellMqttPublish(uDeviceHandle_t cellHandle,
                                const char *pTopicNameStr,
                                const char *pMessage,
                                size_t messageSizeBytes,
                                uCellMqttQos_t qos, bool retain)
{
    (void) cellHandle;
    (void) pTopicNameStr;
    (void) pMessage;
    (void) messageSizeBytes;
    (void) qos;
    (void) retain;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSubscribe(uDeviceHandle_t cellHandle,
                                  const char *pTopicFilterStr,
                                  uCellMqttQos_t maxQos)
{
    (void) cellHandle;
    (void) pTopicFilterStr;
    (void) maxQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttUnsubscribe(uDeviceHandle_t cellHandle,
                                    const char *pTopicFilterStr)
{
    (void) cellHandle;
    (void) pTopicFilterStr;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttMessageRead(uDeviceHandle_t cellHandle,
                                    char *pTopicNameStr,
                                    size_t topicNameSizeBytes,
                                    char *pMessage, size_t *pMessageSizeBytes,
                                    uCellMqttQos_t *pQos)
{
    (void) cellHandle;
    (void) pTopicNameStr;
    (void) topicNameSizeBytes;
    (void) pMessage;
    (void) pMessageSizeBytes;
    (void) pQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uCellMqttSnIsSupported(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return false;
}

U_WEAK int32_t uCellMqttSnRegisterNormalTopic(uDeviceHandle_t cellHandle,
                                              const char *pTopicNameStr,
                                              uCellMqttSnTopicName_t *pTopicName)
{
    (void) cellHandle;
    (void) pTopicNameStr;
    (void) pTopicName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnPublish(uDeviceHandle_t cellHandle,
                                  const uCellMqttSnTopicName_t *pTopicName,
                                  const char *pMessage,
                                  size_t messageSizeBytes,
                                  uCellMqttQos_t qos, bool retain)
{
    (void) cellHandle;
    (void) pTopicName;
    (void) pMessage;
    (void) messageSizeBytes;
    (void) qos;
    (void) retain;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnSubscribe(uDeviceHandle_t cellHandle,
                                    const uCellMqttSnTopicName_t *pTopicName,
                                    uCellMqttQos_t maxQos)
{
    (void) cellHandle;
    (void) pTopicName;
    (void) maxQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnSubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                               const char *pTopicFilterStr,
                                               uCellMqttQos_t maxQos,
                                               uCellMqttSnTopicName_t *pTopicName)
{
    (void) cellHandle;
    (void) pTopicFilterStr;
    (void) maxQos;
    (void) pTopicName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnUnsubscribe(uDeviceHandle_t cellHandle,
                                      const uCellMqttSnTopicName_t *pTopicName)
{
    (void) cellHandle;
    (void) pTopicName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnUnsubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                                 const char *pTopicFilterStr)
{
    (void) cellHandle;
    (void) pTopicFilterStr;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnMessageRead(uDeviceHandle_t cellHandle,
                                      uCellMqttSnTopicName_t *pTopicName,
                                      char *pMessage, size_t *pMessageSizeBytes,
                                      uCellMqttQos_t *pQos)
{
    (void) cellHandle;
    (void) pTopicName;
    (void) pMessage;
    (void) pMessageSizeBytes;
    (void) pQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnSetWillMessaage(uDeviceHandle_t cellHandle,
                                          const char *pMessage,
                                          size_t messageSizeBytes)
{
    (void) cellHandle;
    (void) pMessage;
    (void) messageSizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellMqttSnSetWillParameters(uDeviceHandle_t cellHandle,
                                            const char *pTopicNameStr,
                                            uCellMqttQos_t qos, bool retain)
{
    (void) cellHandle;
    (void) pTopicNameStr;
    (void) qos;
    (void) retain;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
