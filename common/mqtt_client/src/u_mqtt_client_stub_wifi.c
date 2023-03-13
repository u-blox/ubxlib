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
 * @brief Stubs to allow the MQTT Client API to be compiled without Wi-Fi;
 * if you call a Wi-Fi API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when Wi-Fi is not included in the build.
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
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"
#include "u_wifi_mqtt.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uWifiMqttInit(uDeviceHandle_t devHandle, void **ppMqttSession)
{
    (void) devHandle;
    (void) ppMqttSession;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uWifiMqttClose(uMqttClientContext_t *pContext)
{
    (void) pContext;
}

U_WEAK int32_t uWifiMqttConnect(const uMqttClientContext_t *pContext,
                                const uMqttClientConnection_t *pConnection)
{
    (void) pContext;
    (void) pConnection;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttDisconnect(const uMqttClientContext_t *pContext)
{
    (void) pContext;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uWifiMqttIsConnected(const uMqttClientContext_t *pContext)
{
    (void) pContext;
    return false;
}

U_WEAK int32_t uWifiMqttSetMessageCallback(const uMqttClientContext_t *pContext,
                                           void (*pCallback) (int32_t, void *),
                                           void *pCallbackParam)
{
    (void) pContext;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttGetUnread(const uMqttClientContext_t *pContext)
{
    (void) pContext;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttSetDisconnectCallback(const uMqttClientContext_t *pContext,
                                              void (*pCallback) (int32_t, void *),
                                              void *pCallbackParam)
{
    (void) pContext;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttPublish(const uMqttClientContext_t *pContext,
                                const char *pTopicNameStr,
                                const char *pMessage,
                                size_t messageSizeBytes,
                                uMqttQos_t qos,
                                bool retain)
{
    (void) pContext;
    (void) pTopicNameStr;
    (void) pMessage;
    (void) messageSizeBytes;
    (void) qos;
    (void) retain;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttSubscribe(const uMqttClientContext_t *pContext,
                                  const char *pTopicFilterStr,
                                  uMqttQos_t maxQos)
{
    (void) pContext;
    (void) pTopicFilterStr;
    (void) maxQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttUnsubscribe(const uMqttClientContext_t *pContext,
                                    const char *pTopicFilterStr)
{
    (void) pContext;
    (void) pTopicFilterStr;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiMqttMessageRead(const uMqttClientContext_t *pContext,
                                    char *pTopicNameStr,
                                    size_t topicNameSizeBytes,
                                    char *pMessage,
                                    size_t *pMessageSizeBytes,
                                    uMqttQos_t *pQos)
{
    (void) pContext;
    (void) pTopicNameStr;
    (void) topicNameSizeBytes;
    (void) pMessage;
    (void) pMessageSizeBytes;
    (void) pQos;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
