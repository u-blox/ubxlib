/*
 * Copyright 2020 u-blox
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
#include "stdlib.h"    // malloc(), free()

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
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_wifi_net.h"
#include "u_wifi_test_private.h"
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
#define MQTT_PUBLISH_TOTAL_MSG_COUNT 4
#define MQTT_WAIT_COUNT 15

//lint -esym(767, LOG_TAG) Suppress LOG_TAG defined differently in another module
//lint -esym(750, LOG_TAG) Suppress LOG_TAG not referenced
#define LOG_TAG "U_WIFI_MQTT_TEST: "

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

const uMqttClientConnection_t mqttConnection = {
    .pBrokerNameStr = "broker.hivemq.com",
    .localPort = 1883,
    .pClientIdStr = "test_client_id",
    .pUserNameStr = "test_user",
    .pPasswordStr = "test_passwd"
};

//lint -e(843) Suppress warning "could be declared as const"
const char *testPublishMsg[MQTT_PUBLISH_TOTAL_MSG_COUNT] =  {"Hello test",
                                                             "aaaaaaaaaaaaaaaaaaa",
                                                             "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                                             "ccccccccccccccccccccccccccccccccccccccccccc"
                                                            };
static uWifiTestPrivate_t gHandles = { -1, -1, NULL, -1 };

static const uint32_t gNetStatusMaskAllUp = U_WIFI_NET_STATUS_MASK_IPV4_UP |
                                            U_WIFI_NET_STATUS_MASK_IPV6_UP;


static volatile bool mqttSessionDisconnected = false;
static volatile int32_t gWifiConnected = 0;
static volatile uint32_t gNetStatusMask = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void wifiConnectionCallback(int32_t wifiHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    (void)wifiHandle;
    (void)connId;
    (void)channel;
    (void)pBssid;
    (void)disconnectReason;
    (void)pCallbackParameter;
    if (status == U_WIFI_NET_CON_STATUS_CONNECTED) {
        uPortLog(LOG_TAG "Connected Wifi connId: %d, bssid: %s, channel: %d\n",
                 connId,
                 pBssid,
                 channel);
        gWifiConnected = 1;
    } else {
#ifdef U_CFG_ENABLE_LOGGING
        //lint -esym(752, strDisconnectReason)
        static const char strDisconnectReason[6][20] = {
            "Unknown", "Remote Close", "Out of range",
            "Roaming", "Security problems", "Network disabled"
        };
        if ((disconnectReason < 0) && (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            //lint -esym(438, disconnectReason)
            disconnectReason = 0;
        }
        uPortLog(LOG_TAG "Wifi connection lost connId: %d, reason: %d (%s)\n",
                 connId,
                 disconnectReason,
                 strDisconnectReason[disconnectReason]);
#endif
        gWifiConnected = 0;
    }
}
static void wifiNetworkStatusCallback(int32_t wifiHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)wifiHandle;
    (void)interfaceType;
    (void)statusMask;
    (void)pCallbackParameter;
    uPortLog(LOG_TAG "Network status IPv4 %s, IPv6 %s\n",
             ((statusMask & U_WIFI_NET_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
             ((statusMask & U_WIFI_NET_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");

    gNetStatusMask = statusMask;
}

static void mqttSubscribeCb(int32_t unreadMsgCount, void *cbParam)
{
    (void)cbParam;
    //lint -e(715) suppress symbol not referenced
    uPortLog(LOG_TAG "MQTT unread msg count = %d\n", unreadMsgCount);
}

static void mqttDisconnectCb(int32_t status, void *cbParam)
{
    (void)cbParam;
    (void)status;
    mqttSessionDisconnected = true;
}

static int32_t wifiMqttUnsubscribeTest(void)
{
    size_t msgBufSz;
    uMqttQos_t qos = U_MQTT_QOS_AT_MOST_ONCE;
    int32_t err;

    char *pTopicOut1;
    char *pTopicIn;
    char *pMessageIn;
    int32_t topicId1;
    int32_t count;

    // Malloc space to read messages and topics into
    pTopicOut1 = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut1 != NULL);

    pTopicIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicIn != NULL);

    //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
    pMessageIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pMessageIn != NULL);


    topicId1 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut1, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%d", topicId1);

    mqttClientCtx = NULL;

    mqttClientCtx = pUMqttClientOpen(gHandles.wifiHandle, NULL);
    U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

    err = uMqttClientConnect(mqttClientCtx, &mqttConnection);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(mqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(mqttClientCtx, mqttSubscribeCb, (void *)mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(mqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSubscribe(mqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = uMqttClientPublish(mqttClientCtx,
                                 pTopicOut1,
                                 testPublishMsg[count],
                                 strlen(testPublishMsg[count]),
                                 qos,
                                 false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(mqttClientCtx) == MQTT_PUBLISH_TOTAL_MSG_COUNT);


    for (count = 0; count < MQTT_WAIT_COUNT; count++) {

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

        uPortLog(LOG_TAG "For topic %s msgBuf content %s msg size %d\n", pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(mqttClientCtx) ==
                       MQTT_PUBLISH_TOTAL_MSG_COUNT);

    err = uMqttClientUnsubscribe(mqttClientCtx, pTopicOut1);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = uMqttClientPublish(mqttClientCtx,
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


    for (count = 0; ((count < MQTT_WAIT_COUNT) || !mqttSessionDisconnected); count++) {
        uPortTaskBlock(1000);
    }

    U_PORT_TEST_ASSERT(mqttSessionDisconnected == true);
    mqttSessionDisconnected = false;

    uMqttClientClose(mqttClientCtx);
    free(pTopicIn);
    free(pTopicOut1);
    free(pMessageIn);

    return err;
}

static int32_t wifiMqttPublishSubscribeTest(void)
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

    // Malloc space to read messages and topics into
    pTopicOut1 = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut1 != NULL);
    pTopicOut2 = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicOut2 != NULL);

    pTopicIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pTopicIn != NULL);

    //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
    pMessageIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pMessageIn != NULL);


    topicId1 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut1, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%d", topicId1);

    topicId2 = rand();
    // Make a unique topic name to stop different boards colliding
    snprintf(pTopicOut2, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
             "ubx_test/%d", topicId2);

    mqttClientCtx = NULL;

    mqttClientCtx = pUMqttClientOpen(gHandles.wifiHandle, NULL);
    U_PORT_TEST_ASSERT(mqttClientCtx != NULL);

    err = uMqttClientConnect(mqttClientCtx, &mqttConnection);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    //lint -e(731) suppress boolean argument to equal / not equal
    U_PORT_TEST_ASSERT(uMqttClientIsConnected(mqttClientCtx) == true);

    err = uMqttClientSetMessageCallback(mqttClientCtx, mqttSubscribeCb, (void *)mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSetDisconnectCallback(mqttClientCtx, mqttDisconnectCb, NULL);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);

    err = uMqttClientSubscribe(mqttClientCtx, pTopicOut1, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    err = uMqttClientSubscribe(mqttClientCtx, pTopicOut2, qos);
    U_PORT_TEST_ASSERT(err == (int32_t)qos);

    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = uMqttClientPublish(mqttClientCtx,
                                 pTopicOut1,
                                 testPublishMsg[count],
                                 strlen(testPublishMsg[count]),
                                 qos,
                                 false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }


    for (count = 0; count < MQTT_PUBLISH_TOTAL_MSG_COUNT; count++) {

        err = uMqttClientPublish(mqttClientCtx,
                                 pTopicOut2,
                                 testPublishMsg[count],
                                 strlen(testPublishMsg[count]),
                                 qos,
                                 false);
        U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(mqttClientCtx) == (MQTT_PUBLISH_TOTAL_MSG_COUNT
                                                                          << 1));


    for (count = 0; count < MQTT_WAIT_COUNT; count++) {

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

        uPortLog(LOG_TAG "For topic %s msgBuf content %s msg size %d\n", pTopicIn, pMessageIn, msgBufSz);
    }
    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(mqttClientCtx) ==
                       (MQTT_PUBLISH_TOTAL_MSG_COUNT << 1));

    err = uMqttClientDisconnect(mqttClientCtx);
    U_PORT_TEST_ASSERT(err == (int32_t)U_ERROR_COMMON_SUCCESS);


    for (count = 0; count < MQTT_WAIT_COUNT; count++) {

        if (mqttSessionDisconnected) {
            break;
        } else {
            uPortTaskBlock(1000);
        }
    }
    U_PORT_TEST_ASSERT(mqttSessionDisconnected == true);
    mqttSessionDisconnected = false;

    uMqttClientClose(mqttClientCtx);
    free(pTopicIn);
    free(pTopicOut1);
    free(pTopicOut2);
    free(pMessageIn);

    return err;
}

static void startWifi(void)
{
    int32_t waitCtr = 0;
    //lint -e(438) suppress testError warning
    uWifiTestError_t testError = U_WIFI_TEST_ERROR_NONE;

    gNetStatusMask = 0;
    gWifiConnected = 0;
    // Do the standard preamble
    //lint -e(40) suppress undeclared identifier 'U_CFG_TEST_SHORT_RANGE_MODULE_TYPE'
    if (0 != uWifiTestPrivatePreamble(U_CFG_TEST_SHORT_RANGE_MODULE_TYPE,
                                      &gHandles)) {
        testError = U_WIFI_TEST_ERROR_PREAMBLE;
    }
    if (testError == U_WIFI_TEST_ERROR_NONE) {
        // Add unsolicited response cb for connection status
        uWifiNetSetConnectionStatusCallback(gHandles.wifiHandle,
                                            wifiConnectionCallback, NULL);
        // Add unsolicited response cb for IP status
        uWifiNetSetNetworkStatusCallback(gHandles.wifiHandle,
                                         wifiNetworkStatusCallback, NULL);

        // Connect to wifi network
        if (0 != uWifiNetStationConnect(gHandles.wifiHandle,
                                        U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID),
                                        U_WIFI_NET_AUTH_WPA_PSK,
                                        U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE))) {

            testError = U_WIFI_TEST_ERROR_CONNECT;
        }
    }
    //Wait for connection and IP events.
    //There could be multiple IP events depending on network comfiguration.
    while (!testError && (!gWifiConnected || (gNetStatusMask != gNetStatusMaskAllUp))) {
        if (waitCtr >= 15) {
            if (!gWifiConnected) {
                uPortLog(LOG_TAG "Unable to connect to WifiNetwork\n");
                testError = U_WIFI_TEST_ERROR_CONNECTED;
            } else {
                uPortLog(LOG_TAG "Unable to retrieve IP address\n");
                testError = U_WIFI_TEST_ERROR_IPRECV;
            }
            break;
        }

        uPortTaskBlock(1000);
        waitCtr++;
    }
    uPortLog(LOG_TAG "wifi handle = %d\n", gHandles.wifiHandle);

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
    startWifi();
    wifiMqttPublishSubscribeTest();
    stopWifi();
}

U_PORT_TEST_FUNCTION("[wifiMqtt]", "wifiMqttUnsubscribeTest")
{
    startWifi();
    wifiMqttUnsubscribeTest();
    stopWifi();
}
#endif // U_SHORT_RANGE_TEST_WIFI()

// End of file
