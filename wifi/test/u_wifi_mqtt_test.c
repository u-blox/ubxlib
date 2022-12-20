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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for wifi MQTT These tests should pass on
 * platforms that has wifi module
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stdio.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // rand()

// Must always be included before u_short_range_test_selector.h
//lint -efile(766, u_wifi_module_type.h)
#include "u_wifi_module_type.h"

#include "u_short_range_test_selector.h"

#if U_SHORT_RANGE_TEST_WIFI()

#include "string.h"
#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_wifi.h"
#include "u_wifi_test_private.h"
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"
#include "u_security_credential.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_MQTT_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#define MQTT_PUBLISH_TOTAL_MSG_COUNT 4
#define MQTT_RETRY_COUNT 60

#ifndef U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES
/** Maximum topic length for reading.
 */
# define U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES 128
#endif

#ifndef U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES
/** Maximum length for reading a message from the broker.
 */
# define U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES 1024
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

uMqttClientContext_t *mqttClientCtx;

char uniqueClientId[U_SHORT_RANGE_SERIAL_NUMBER_LENGTH];

const uMqttClientConnection_t mqttUnsecuredConnection = {
    .pBrokerNameStr = "ubxlib.redirectme.net",
    .pUserNameStr = "test_user",
    .pPasswordStr = "test_passwd",
    .pClientIdStr = uniqueClientId,
    .localPort = 1883
};


const uMqttClientConnection_t mqttSecuredConnection = {
    .pBrokerNameStr = "ubxlib.redirectme.net",
    .pUserNameStr = "test_user",
    .pPasswordStr = "test_passwd",
    .pClientIdStr = uniqueClientId,
    .localPort = 8883,
    .keepAlive = true
};

uSecurityTlsSettings_t mqttTlsSettings = {

    .pRootCaCertificateName = "ubxlib.redirectme.crt",
    .pClientCertificateName = NULL,
    .pClientPrivateKeyName = NULL,
    .certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA
};

static const char *gpRootCaCert = "-----BEGIN CERTIFICATE-----\n"
                                  "MIIDoTCCAomgAwIBAgIUeGAKqrye/8CWhtXa3rmG8tYYYXMwDQYJKoZIhvcNAQEL\n"
                                  "BQAwYDELMAkGA1UEBhMCR0IxEzARBgNVBAgMClNvbWUtU3RhdGUxDzANBgNVBAoM\n"
                                  "BnUtYmxveDELMAkGA1UECwwCY2ExHjAcBgNVBAMMFXVieGxpYi5yZWRpcmVjdG1l\n"
                                  "Lm5ldDAeFw0yMjEyMTMxNDE2MDhaFw0yMzAxMTIxNDE2MDhaMGAxCzAJBgNVBAYT\n"
                                  "AkdCMRMwEQYDVQQIDApTb21lLVN0YXRlMQ8wDQYDVQQKDAZ1LWJsb3gxCzAJBgNV\n"
                                  "BAsMAmNhMR4wHAYDVQQDDBV1YnhsaWIucmVkaXJlY3RtZS5uZXQwggEiMA0GCSqG\n"
                                  "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCpjsAl6td3Vwj9V0clx7G21DkgVnFbkUQj\n"
                                  "jmJdb73AeWWg0h2bvpsaHxR5hZ8aGbdCqkbDMbYn7b6rSZV3JekY7njdd59nrHZK\n"
                                  "em7jnNNyQO07HXDQ8VqYlcq2HxwHqu1ZMzwGoWDHYa4stEmbV7N55KQcBeTS2OsX\n"
                                  "uIhEe2mh/y4ykDisp6/mtmAQhQORKAtlYNnTqWzoGTMFGenEySqIosSTcFbN/5DA\n"
                                  "3ZyGHD3lxWQs54rIjIKEOQz+BNKPH3udWY5/hir1qrO606Vy5PFEf72wr1mOifR/\n"
                                  "6qInudi3UWDSBh2m/C5ogkaLhl8ZD+8hoc78a8xlISPbHeGAHB1JAgMBAAGjUzBR\n"
                                  "MB0GA1UdDgQWBBROm6p7gjvxXXdzSe/9MvirKmT7kzAfBgNVHSMEGDAWgBROm6p7\n"
                                  "gjvxXXdzSe/9MvirKmT7kzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUA\n"
                                  "A4IBAQAFhcnOpEmXUShPdbZkjJ4FyU/toaJUkhue4WgHrvqMf1swL4w8EGB8ZhlC\n"
                                  "EDdLVhQD/bGAaZs2gw1KE4vvBY4Yf4466zWsEew97Fvo1HpML1ZP6wziHbqhnydL\n"
                                  "ddKLkKnANeq6rPpoi8JZi6+Ab4DujAyeY1+olwAzVxEf9NgRxGhymcbpdhIeXMAE\n"
                                  "PPJ5U+pJ8myUimmnXplH/N6uFkrVPJ02OXu3ZTvDc9aNlhD6qFV2sAmo0KL5F9N6\n"
                                  "v19eWFvYo7ep+Km8ThcuuAYjce5h/9ldp4BbTgED+1IXFFlI9Ym6s9XJpqvlibYs\n"
                                  "Nbqr5r24/tl8a9ZVc3oALcWkHFLD\n"
                                  "-----END CERTIFICATE-----\n";

//lint -e(843) Suppress warning "could be declared as const"
const char *testPublishMsg[MQTT_PUBLISH_TOTAL_MSG_COUNT] =  {"Hello test",
                                                             "aaaaaaaaaaaaaaaaaaa",
                                                             "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                                             "ccccccccccccccccccccccccccccccccccccccccccc"
                                                            };
static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

static const uint32_t gWifiStatusMaskAllUp = U_WIFI_STATUS_MASK_IPV4_UP |
                                             U_WIFI_STATUS_MASK_IPV6_UP;


static volatile bool mqttSessionDisconnected = false;
static volatile int32_t gWifiConnected = 0;
static volatile uint32_t gWifiStatusMask = 0;

static uShortRangeUartConfig_t uart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                        .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                        .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                        .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                        .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                        .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
                                      };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void setUniqueClientId(uDeviceHandle_t devHandle)
{
    int32_t len = uShortRangeGetSerialNumber(devHandle, uniqueClientId);
    if (len > 2) {
        if (uniqueClientId[0] == '"') {
            // Remove the quote characters
            memmove(uniqueClientId, uniqueClientId + 1, len - 1);
            uniqueClientId[len - 2] = 0;
        }

    } else {
        // Failed to get serial number, use a random number
        snprintf(uniqueClientId, sizeof(uniqueClientId), "%d", rand());
    }
}

static void wifiConnectionCallback(uDeviceHandle_t devHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    (void)devHandle;
    (void)connId;
    (void)channel;
    (void)pBssid;
    (void)disconnectReason;
    (void)pCallbackParameter;
    if (status == U_WIFI_CON_STATUS_CONNECTED) {
        U_TEST_PRINT_LINE("connected Wifi connId: %d, bssid: %s, channel: %d.",
                          connId, pBssid, channel);
        gWifiConnected = 1;
    } else {
#ifdef U_CFG_ENABLE_LOGGING
        //lint -esym(752, strDisconnectReason)
        static const char strDisconnectReason[6][20] = {
            "Unknown", "Remote Close", "Out of range",
            "Roaming", "Security problems", "Network disabled"
        };
        if ((disconnectReason < 0) || (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            //lint -esym(438, disconnectReason)
            disconnectReason = 0;
        }
        U_TEST_PRINT_LINE("wifi connection lost connId: %d, reason: %d (%s).",
                          connId, disconnectReason,
                          strDisconnectReason[disconnectReason]);
#endif
        gWifiConnected = 0;
    }
}
static void wifiNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)devHandle;
    (void)interfaceType;
    (void)statusMask;
    (void)pCallbackParameter;
    U_TEST_PRINT_LINE("network status IPv4 %s, IPv6 %s.",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
                      ((statusMask & U_WIFI_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");

    gWifiStatusMask = statusMask;
}

static void mqttSubscribeCb(int32_t unreadMsgCount, void *cbParam)
{
    (void)cbParam;
    //lint -e(715) suppress symbol not referenced
    U_TEST_PRINT_LINE("MQTT unread msg count = %d.", unreadMsgCount);
}

static void mqttDisconnectCb(int32_t status, void *cbParam)
{
    (void)cbParam;
    (void)status;
    mqttSessionDisconnected = true;
}


static int32_t mqttSubscribe(uMqttClientContext_t *mqttClientCtx,
                             const char *pTopicFilterStr,
                             uMqttQos_t maxQos)
{
    int32_t count = 0;
    int32_t err = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Retry until connection succeeds
    for (count = 0; ((count < MQTT_RETRY_COUNT) && (err < 0)); count++) {

        err = uMqttClientSubscribe(mqttClientCtx, pTopicFilterStr, maxQos);
        uPortTaskBlock(1000);
    }

    return err;
}

static int32_t mqttPublish(uMqttClientContext_t *mqttClientCtx,
                           const char *pTopicNameStr,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uMqttQos_t qos,
                           bool retain)
{

    int32_t count = 0;
    int32_t err = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Retry until connection succeeds
    for (count = 0; ((count < MQTT_RETRY_COUNT) &&
                     (err != (int32_t) U_ERROR_COMMON_SUCCESS)); count++) {

        err = uMqttClientPublish(mqttClientCtx,
                                 pTopicNameStr,
                                 pMessage,
                                 messageSizeBytes,
                                 qos,
                                 retain);
        uPortTaskBlock(1000);
    }

    return err;

}

static int32_t wifiMqttUnsubscribeTest(bool isSecuredConnection)
{
    size_t msgBufSz;
    uMqttQos_t qos = U_MQTT_QOS_AT_MOST_ONCE;
    int32_t err;

    char *pTopicOut1;
    char *pTopicIn;
    char *pMessageIn;
    int32_t topicId1;
    int32_t count;

    setUniqueClientId(gHandles.devHandle);

    // Malloc space to read messages and topics into
    pTopicOut1 = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut1 != NULL);

    pTopicIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicIn != NULL);

    //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
    pMessageIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pMessageIn != NULL);


    topicId1 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut1, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%u", (unsigned int) topicId1);

    mqttClientCtx = NULL;

    if (isSecuredConnection == false) {

        mqttClientCtx = pUMqttClientOpen(gHandles.devHandle, NULL);
        U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

        err = uMqttClientConnect(mqttClientCtx, &mqttUnsecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    } else {

        mqttClientCtx = pUMqttClientOpen(gHandles.devHandle, &mqttTlsSettings);
        U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

        err = uMqttClientConnect(mqttClientCtx, &mqttSecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    }


    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(mqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(mqttClientCtx, mqttSubscribeCb, (void *)mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(mqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = mqttSubscribe(mqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(mqttClientCtx,
                          pTopicOut1,
                          testPublishMsg[count],
                          strlen(testPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(mqttClientCtx) == MQTT_PUBLISH_TOTAL_MSG_COUNT);


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (uMqttClientGetTotalMessagesSent(mqttClientCtx) == uMqttClientGetUnread(mqttClientCtx)) {
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
            break;
        } else {
            err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            uPortTaskBlock(1000);
        }
    }

    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    while (uMqttClientGetUnread(mqttClientCtx) != 0) {

        msgBufSz = U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES;

        uMqttClientMessageRead(mqttClientCtx,
                               pTopicIn,
                               U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                               pMessageIn,
                               &msgBufSz,
                               &qos);

        U_TEST_PRINT_LINE("for topic %s msgBuf content %s msg size %d.",
                          pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(mqttClientCtx) ==
                       MQTT_PUBLISH_TOTAL_MSG_COUNT);

    err = uMqttClientUnsubscribe(mqttClientCtx, pTopicOut1);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(mqttClientCtx,
                          pTopicOut1,
                          testPublishMsg[count],
                          strlen(testPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    U_PORT_TEST_ASSERT(uMqttClientGetUnread(mqttClientCtx) == 0);

    mqttSessionDisconnected = false;
    err = uMqttClientDisconnect(mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);


    for (count = 0; ((count < MQTT_RETRY_COUNT) && !mqttSessionDisconnected); count++) {
        uPortTaskBlock(1000);
    }

    U_PORT_TEST_ASSERT(mqttSessionDisconnected == true);
    mqttSessionDisconnected = false;

    uMqttClientClose(mqttClientCtx);
    uPortFree(pTopicIn);
    uPortFree(pTopicOut1);
    uPortFree(pMessageIn);

    return err;
}

static int32_t wifiMqttPublishSubscribeTest(bool isSecuredConnection)
{

    size_t msgBufSz;
    uMqttQos_t qos = U_MQTT_QOS_AT_MOST_ONCE;
    int32_t err;

    char *pTopicOut1;
    char *pTopicOut2;
    char *pTopicIn;
    char *pMessageIn;
    int32_t topicId1;
    int32_t topicId2;
    int32_t count;

    setUniqueClientId(gHandles.devHandle);

    // Malloc space to read messages and topics into
    pTopicOut1 = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut1 != NULL);
    pTopicOut2 = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut2 != NULL);

    pTopicIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicIn != NULL);

    //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
    pMessageIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pMessageIn != NULL);


    topicId1 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut1, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%u", (unsigned int) topicId1);

    topicId2 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut2, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%u", (unsigned int) topicId2);

    mqttClientCtx = NULL;

    if (isSecuredConnection == false) {

        mqttClientCtx = pUMqttClientOpen(gHandles.devHandle, NULL);
        U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

        err = uMqttClientConnect(mqttClientCtx, &mqttUnsecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    } else {

        mqttClientCtx = pUMqttClientOpen(gHandles.devHandle, &mqttTlsSettings);
        U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

        err = uMqttClientConnect(mqttClientCtx, &mqttSecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    }
    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(mqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(mqttClientCtx, mqttSubscribeCb, (void *)mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(mqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = mqttSubscribe(mqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    err = mqttSubscribe(mqttClientCtx, pTopicOut2, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(mqttClientCtx,
                          pTopicOut1,
                          testPublishMsg[count],
                          strlen(testPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(mqttClientCtx,
                          pTopicOut2,
                          testPublishMsg[count],
                          strlen(testPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(mqttClientCtx) == (MQTT_PUBLISH_TOTAL_MSG_COUNT
                                                                          << 1));


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (uMqttClientGetTotalMessagesSent(mqttClientCtx) == uMqttClientGetUnread(mqttClientCtx)) {
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
            break;
        } else {
            err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            uPortTaskBlock(1000);
        }
    }

    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    while (uMqttClientGetUnread(mqttClientCtx) != 0) {

        msgBufSz = U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES;

        uMqttClientMessageRead(mqttClientCtx,
                               pTopicIn,
                               U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                               pMessageIn,
                               &msgBufSz,
                               &qos);

        U_TEST_PRINT_LINE("for topic %s msgBuf content %s msg size %d.",
                          pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(mqttClientCtx) ==
                       (MQTT_PUBLISH_TOTAL_MSG_COUNT << 1));

    err = uMqttClientDisconnect(mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (mqttSessionDisconnected) {
            break;
        } else {
            uPortTaskBlock(1000);
        }
    }
    U_PORT_TEST_ASSERT(mqttSessionDisconnected == true);
    mqttSessionDisconnected = false;

    uMqttClientClose(mqttClientCtx);
    uPortFree(pTopicIn);
    uPortFree(pTopicOut1);
    uPortFree(pTopicOut2);
    uPortFree(pMessageIn);

    return err;
}

static uWifiTestError_t startWifi(void)
{
    int32_t waitCtr = 0;
    //lint -e(438) suppress testError warning
    uWifiTestError_t testError = U_WIFI_TEST_ERROR_NONE;

    gWifiStatusMask = 0;
    gWifiConnected = 0;
    // Do the standard preamble
    //lint -e(40) suppress undeclared identifier 'U_CFG_TEST_SHORT_RANGE_MODULE_TYPE'
    if (0 != uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                      &uart,
                                      &gHandles)) {
        testError = U_WIFI_TEST_ERROR_PREAMBLE;
    }
    if (testError == U_WIFI_TEST_ERROR_NONE) {
        // Add unsolicited response cb for connection status
        uWifiSetConnectionStatusCallback(gHandles.devHandle,
                                         wifiConnectionCallback, NULL);
        // Add unsolicited response cb for IP status
        uWifiSetNetworkStatusCallback(gHandles.devHandle,
                                      wifiNetworkStatusCallback, NULL);

        // Connect to wifi network
        int32_t status;
        status = uWifiStationConnect(gHandles.devHandle,
                                     U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                     U_WIFI_AUTH_WPA_PSK,
                                     U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE));
        if (status == (int32_t) U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID) {
            gWifiConnected = true;
            gWifiStatusMask = gWifiStatusMaskAllUp;
        } else if (status != 0) {
            testError = U_WIFI_TEST_ERROR_CONNECT;
        }
    }
    //Wait for connection and IP events.
    //There could be multiple IP events depending on network comfiguration.
    while (!testError && (!gWifiConnected || (gWifiStatusMask != gWifiStatusMaskAllUp))) {
        if (waitCtr >= 15) {
            if (!gWifiConnected) {
                U_TEST_PRINT_LINE("unable to connect to WiFi network.");
                testError = U_WIFI_TEST_ERROR_CONNECTED;
            } else {
                U_TEST_PRINT_LINE("unable to retrieve IP address.");
                testError = U_WIFI_TEST_ERROR_IPRECV;
            }
            break;
        }

        uPortTaskBlock(1000);
        waitCtr++;
    }
    U_TEST_PRINT_LINE("wifi handle = %d.", gHandles.devHandle);

    return testError;
}

static void stopWifi(void)
{
    uWifiTestPrivatePostamble(&gHandles);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttPublishSubscribeTest")
{
    U_PORT_TEST_ASSERT_EQUAL(U_WIFI_TEST_ERROR_NONE, startWifi());
    wifiMqttPublishSubscribeTest(false);
    stopWifi();
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttUnsubscribeTest")
{
    U_PORT_TEST_ASSERT_EQUAL(U_WIFI_TEST_ERROR_NONE, startWifi());
    wifiMqttUnsubscribeTest(false);
    stopWifi();
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttSecuredPublishSubscribeTest")
{
    int32_t err;
    U_PORT_TEST_ASSERT_EQUAL(U_WIFI_TEST_ERROR_NONE, startWifi());
    err = uSecurityCredentialStore(gHandles.devHandle,
                                   U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                   mqttTlsSettings.pRootCaCertificateName,
                                   gpRootCaCert,
                                   strlen(gpRootCaCert),
                                   NULL,
                                   NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    wifiMqttPublishSubscribeTest(true);
    err = uSecurityCredentialRemove(gHandles.devHandle,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    mqttTlsSettings.pRootCaCertificateName);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    stopWifi();
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttSecuredUnsubscribeTest")
{
    int32_t err;
    U_PORT_TEST_ASSERT_EQUAL(U_WIFI_TEST_ERROR_NONE, startWifi());
    err = uSecurityCredentialStore(gHandles.devHandle,
                                   U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                   mqttTlsSettings.pRootCaCertificateName,
                                   gpRootCaCert,
                                   strlen(gpRootCaCert),
                                   NULL,
                                   NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    wifiMqttUnsubscribeTest(true);
    err = uSecurityCredentialRemove(gHandles.devHandle,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    mqttTlsSettings.pRootCaCertificateName);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    stopWifi();
}
#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
