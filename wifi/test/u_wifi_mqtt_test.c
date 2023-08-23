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
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

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

uMqttClientContext_t *gpMqttClientCtx;

char gUniqueClientId[U_SHORT_RANGE_SERIAL_NUMBER_LENGTH];

const uMqttClientConnection_t gMqttUnsecuredConnection = {
    .pBrokerNameStr = "ubxlib.com",
    .pUserNameStr = "test_user",
    .pPasswordStr = "test_passwd",
    .pClientIdStr = gUniqueClientId,
    .localPort = 1883
};


const uMqttClientConnection_t gMqttSecuredConnection = {
    .pBrokerNameStr = "ubxlib.com",
    .pUserNameStr = "test_user",
    .pPasswordStr = "test_passwd",
    .pClientIdStr = gUniqueClientId,
    .localPort = 8883,
    .keepAlive = true
};

uSecurityTlsSettings_t gMqttTlsSettings = {

    .pRootCaCertificateName = "ubxlib.com.crt",
    .pClientCertificateName = NULL,
    .pClientPrivateKeyName = NULL,
    .certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA
};

static const char *gpRootCaCert = "-----BEGIN CERTIFICATE-----\n"
                                  "MIID5zCCAs+gAwIBAgIUTEgSJDC+dF9XeSzXYRE0TQRYRVowDQYJKoZIhvcNAQEL\n"
                                  "BQAwgYExCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJXQTEQMA4GA1UEBwwHVGhhbHdp\n"
                                  "bDEPMA0GA1UECgwGdS1ibG94MQswCQYDVQQLDAJjYTETMBEGA1UEAwwKdWJ4bGli\n"
                                  "LmNvbTEgMB4GCSqGSIb3DQEJARYRdWJ4bGliQHUtYmxveC5jb20wIBcNMjMwNzMx\n"
                                  "MjIzNDQ5WhgPMjEyMzA3MDcyMjM0NDlaMIGBMQswCQYDVQQGEwJVUzELMAkGA1UE\n"
                                  "CAwCV0ExEDAOBgNVBAcMB1RoYWx3aWwxDzANBgNVBAoMBnUtYmxveDELMAkGA1UE\n"
                                  "CwwCY2ExEzARBgNVBAMMCnVieGxpYi5jb20xIDAeBgkqhkiG9w0BCQEWEXVieGxp\n"
                                  "YkB1LWJsb3guY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAq1PQ\n"
                                  "xqnk7vuxexFticwr5wqR+n6cdQCJW37UGAml6fWQ4opyuZo+tUWLMrZJjS/wa/jK\n"
                                  "j+JVxvlOrVehdrnce5kpFWHj8Iz85QrGE21FsS2IpV7yVBIcBdC6LpC8CwNmffBg\n"
                                  "gzjMRdHiqVpz8761E82mp1iTt1BqrBGYrBKmk4H2QPkpJf2ymaff0Jm4Zf8m1Ozq\n"
                                  "JWj27jxre3hy8yi0PLRI5+bC7sdUNheZc43mwfLua2hFyzh046VoZ/ZIOKuiBqjG\n"
                                  "uy4Yo+diIBzneiAMXNimMO0STCfTeiUpNtfLuvDhLkYkx7FEcRWq5Navz3DyzwG/\n"
                                  "iI3Dtj13MmpW9RwkcwIDAQABo1MwUTAdBgNVHQ4EFgQUID2tRSRpiIhPRNWo0RVY\n"
                                  "zB/2S28wHwYDVR0jBBgwFoAUID2tRSRpiIhPRNWo0RVYzB/2S28wDwYDVR0TAQH/\n"
                                  "BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAaTC+6oIEQLJMMiIfzT4M5SAnU0jg\n"
                                  "YrZdzqcnX434uFIV6FlXKHs/tUuKFXA/grjj4XZuxtpkxdTvDW5lOwC59tBD4ATS\n"
                                  "YDY+nUC4EMwv2aoFUrHV6zTUayqJHE2mNK+OuVc2OUVXUb7CEDbJJsECrPCDIIH+\n"
                                  "LqqmRbvEypT0IRGp8a/vWI1XRdqf1x6MRvvIt+ju1kTe8/8WFVgu+sf1sO9JPYAL\n"
                                  "rDiCkDLChTkFxDHAWQjl1ZE9qS01/a9rXLohdSPknIxuJn/TE/xF1tik/x7sJgNI\n"
                                  "0OTAHWp66P43CopVUKJy5uprBYxkPwKICiB5/sY9ukH+gqLx2jRhwX4y6A==\n"
                                  "-----END CERTIFICATE-----\n";

//lint -e(843) Suppress warning "could be declared as const"
const char *gTestPublishMsg[MQTT_PUBLISH_TOTAL_MSG_COUNT] =  {"Hello test",
                                                              "aaaaaaaaaaaaaaaaaaa",
                                                              "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                                              "ccccccccccccccccccccccccccccccccccccccccccc"
                                                             };
static uWifiTestPrivate_t gHandles = { -1, -1, NULL, NULL };

static volatile bool gMqttSessionDisconnected = false;

static uShortRangeUartConfig_t gUart = { .uartPort = U_CFG_APP_SHORT_RANGE_UART,
                                         .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
                                         .pinTx = U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                         .pinRx = U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                         .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                         .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,
#ifdef U_CFG_APP_UART_PREFIX // Relevant for Linux only
                                         .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX)
#else
                                         .pPrefix = NULL
#endif
                                       };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void setUniqueClientId(uDeviceHandle_t devHandle)
{
    int32_t len = uShortRangeGetSerialNumber(devHandle, gUniqueClientId);
    if (len > 2) {
        if (gUniqueClientId[0] == '"') {
            // Remove the quote characters
            memmove(gUniqueClientId, gUniqueClientId + 1, len - 1);
            gUniqueClientId[len - 2] = 0;
        }

    } else {
        // Failed to get serial number, use a random number
        snprintf(gUniqueClientId, sizeof(gUniqueClientId), "%d", rand());
    }
}

static void mqttSubscribeCb(int32_t unreadMsgCount, void *pCbParam)
{
    (void)pCbParam;
    //lint -e(715) suppress symbol not referenced
    U_TEST_PRINT_LINE("MQTT unread msg count = %d.", unreadMsgCount);
}

static void mqttDisconnectCb(int32_t status, void *pCbParam)
{
    (void)pCbParam;
    (void)status;
    gMqttSessionDisconnected = true;
}


static int32_t mqttSubscribe(uMqttClientContext_t *gpMqttClientCtx,
                             const char *pTopicFilterStr,
                             uMqttQos_t maxQos)
{
    int32_t count = 0;
    int32_t err = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Retry until connection succeeds
    for (count = 0; ((count < MQTT_RETRY_COUNT) && (err < 0)); count++) {

        err = uMqttClientSubscribe(gpMqttClientCtx, pTopicFilterStr, maxQos);
        uPortTaskBlock(1000);
    }

    return err;
}

static int32_t mqttPublish(uMqttClientContext_t *gpMqttClientCtx,
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

        err = uMqttClientPublish(gpMqttClientCtx,
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

    gpMqttClientCtx = NULL;

    if (isSecuredConnection == false) {

        gpMqttClientCtx = pUMqttClientOpen(gHandles.devHandle, NULL);
        U_PORT_TEST_ASSERT(gpMqttClientCtx != NULL);

        err = uMqttClientConnect(gpMqttClientCtx, &gMqttUnsecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    } else {

        gpMqttClientCtx = pUMqttClientOpen(gHandles.devHandle, &gMqttTlsSettings);
        U_PORT_TEST_ASSERT(gpMqttClientCtx != NULL);

        err = uMqttClientConnect(gpMqttClientCtx, &gMqttSecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    }


    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(gpMqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(gpMqttClientCtx, mqttSubscribeCb, (void *)gpMqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(gpMqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = mqttSubscribe(gpMqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(gpMqttClientCtx,
                          pTopicOut1,
                          gTestPublishMsg[count],
                          strlen(gTestPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(gpMqttClientCtx) ==
                       MQTT_PUBLISH_TOTAL_MSG_COUNT);


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (uMqttClientGetTotalMessagesSent(gpMqttClientCtx) == uMqttClientGetUnread(gpMqttClientCtx)) {
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
            break;
        } else {
            err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            uPortTaskBlock(1000);
        }
    }

    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    while (uMqttClientGetUnread(gpMqttClientCtx) != 0) {

        msgBufSz = U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES;

        uMqttClientMessageRead(gpMqttClientCtx,
                               pTopicIn,
                               U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                               pMessageIn,
                               &msgBufSz,
                               &qos);

        U_TEST_PRINT_LINE("for topic %s msgBuf content %s msg size %d.",
                          pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(gpMqttClientCtx) ==
                       MQTT_PUBLISH_TOTAL_MSG_COUNT);

    err = uMqttClientUnsubscribe(gpMqttClientCtx, pTopicOut1);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(gpMqttClientCtx,
                          pTopicOut1,
                          gTestPublishMsg[count],
                          strlen(gTestPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttClientCtx) == 0);

    gMqttSessionDisconnected = false;
    err = uMqttClientDisconnect(gpMqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);


    for (count = 0; ((count < MQTT_RETRY_COUNT) && !gMqttSessionDisconnected); count++) {
        uPortTaskBlock(1000);
    }

    U_PORT_TEST_ASSERT(gMqttSessionDisconnected == true);
    gMqttSessionDisconnected = false;

    uMqttClientClose(gpMqttClientCtx);
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

    gpMqttClientCtx = NULL;

    if (isSecuredConnection == false) {

        gpMqttClientCtx = pUMqttClientOpen(gHandles.devHandle, NULL);
        U_PORT_TEST_ASSERT(gpMqttClientCtx != NULL);

        err = uMqttClientConnect(gpMqttClientCtx, &gMqttUnsecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    } else {

        gpMqttClientCtx = pUMqttClientOpen(gHandles.devHandle, &gMqttTlsSettings);
        U_PORT_TEST_ASSERT(gpMqttClientCtx != NULL);

        err = uMqttClientConnect(gpMqttClientCtx, &gMqttSecuredConnection);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    }
    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(gpMqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(gpMqttClientCtx, mqttSubscribeCb, (void *)gpMqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(gpMqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = mqttSubscribe(gpMqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    err = mqttSubscribe(gpMqttClientCtx, pTopicOut2, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(gpMqttClientCtx,
                          pTopicOut1,
                          gTestPublishMsg[count],
                          strlen(gTestPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = mqttPublish(gpMqttClientCtx,
                          pTopicOut2,
                          gTestPublishMsg[count],
                          strlen(gTestPublishMsg[count]),
                          qos,
                          false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(gpMqttClientCtx) == (MQTT_PUBLISH_TOTAL_MSG_COUNT
                                                                            << 1));


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (uMqttClientGetTotalMessagesSent(gpMqttClientCtx) == uMqttClientGetUnread(gpMqttClientCtx)) {
            err = (int32_t)U_ERROR_COMMON_SUCCESS;
            break;
        } else {
            err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            uPortTaskBlock(1000);
        }
    }

    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    while (uMqttClientGetUnread(gpMqttClientCtx) != 0) {

        msgBufSz = U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES;

        uMqttClientMessageRead(gpMqttClientCtx,
                               pTopicIn,
                               U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                               pMessageIn,
                               &msgBufSz,
                               &qos);

        U_TEST_PRINT_LINE("for topic %s msgBuf content %s msg size %d.",
                          pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(gpMqttClientCtx) ==
                       (MQTT_PUBLISH_TOTAL_MSG_COUNT << 1));

    err = uMqttClientDisconnect(gpMqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);


    for (count = 0; count < MQTT_RETRY_COUNT; count++) {

        if (gMqttSessionDisconnected) {
            break;
        } else {
            uPortTaskBlock(1000);
        }
    }
    U_PORT_TEST_ASSERT(gMqttSessionDisconnected == true);
    gMqttSessionDisconnected = false;

    uMqttClientClose(gpMqttClientCtx);
    uPortFree(pTopicIn);
    uPortFree(pTopicOut1);
    uPortFree(pTopicOut2);
    uPortFree(pMessageIn);

    return err;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttPublishSubscribeTest")
{
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);
    wifiMqttPublishSubscribeTest(false);
    uWifiTestPrivatePostamble(&gHandles);
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttUnsubscribeTest")
{
    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);
    wifiMqttUnsubscribeTest(false);
    uWifiTestPrivatePostamble(&gHandles);
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttSecuredPublishSubscribeTest")
{
    int32_t err;

    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);
    err = uSecurityCredentialStore(gHandles.devHandle,
                                   U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                   gMqttTlsSettings.pRootCaCertificateName,
                                   gpRootCaCert,
                                   strlen(gpRootCaCert),
                                   NULL,
                                   NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    wifiMqttPublishSubscribeTest(true);
    err = uSecurityCredentialRemove(gHandles.devHandle,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    gMqttTlsSettings.pRootCaCertificateName);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    uWifiTestPrivatePostamble(&gHandles);
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttSecuredUnsubscribeTest")
{
    int32_t err;

    U_PORT_TEST_ASSERT(uWifiTestPrivatePreamble((uWifiModuleType_t) U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                                &gUart, &gHandles) == 0);
    U_PORT_TEST_ASSERT(uWifiTestPrivateConnect(&gHandles) == U_WIFI_TEST_ERROR_NONE);
    err = uSecurityCredentialStore(gHandles.devHandle,
                                   U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                   gMqttTlsSettings.pRootCaCertificateName,
                                   gpRootCaCert,
                                   strlen(gpRootCaCert),
                                   NULL,
                                   NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    wifiMqttUnsubscribeTest(true);
    err = uSecurityCredentialRemove(gHandles.devHandle,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    gMqttTlsSettings.pRootCaCertificateName);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    uWifiTestPrivatePostamble(&gHandles);
}
#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
