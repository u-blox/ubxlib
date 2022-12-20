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
 * @brief Test for the u-blox MQTT client API: these should pass on
 * all platforms that include the appropriate communications hardware,
 * and will be run for all bearers for which the network API tests have
 * configuration information, i.e. cellular or BLE/Wifi for short range.
 * These tests use the network API and the test configuration information
 * from the network API to provide the communication path.
 * Note that no comprehensive testing of the MQTT configuration options
 * is carried out here, that is a matter for the testing of the
 * underlying API where the supported options for any given module are
 * known.  The tests here DELIBERATELY chose a minimal set of options
 * as support for all of them from all u-blox module types [that support
 * MQTT] is required.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strcmp(), memcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_security_tls.h"
#include "u_security.h"     // For uSecurityGetSerialNumber()

#include "u_mqtt_client.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_MQTT"

/** The string to put at the start of all MQTT prints from this test.
 */
#define U_TEST_PREFIX_MQTT U_TEST_PREFIX_BASE "_CLIENT_TEST: "

/** Print a whole line, with terminator, prefixed for the MQTT
 * tests in this test file.
 */
#define U_TEST_PRINT_LINE_MQTT(format, ...) uPortLog(U_TEST_PREFIX_MQTT format "\n", ##__VA_ARGS__)

/** The string to put at the start of all MQTT-SN prints from this test.
 */
#define U_TEST_PREFIX_MQTTSN U_TEST_PREFIX_BASE "SN_CLIENT_TEST: "

/** Print a whole line, with terminator, prefixed for the MQTT
 * tests in this test file.
 */
#define U_TEST_PRINT_LINE_MQTTSN(format, ...) uPortLog(U_TEST_PREFIX_MQTTSN format "\n", ##__VA_ARGS__)

#ifndef U_MQTT_CLIENT_TEST_MQTT_BROKER_URL
/** Server to use for MQTT client testing, non secure.
 */
//lint -esym(773, U_MQTT_CLIENT_TEST_MQTT_BROKER_URL) Suppress not fully
// bracketed, Lint is wary of the "-" in here but we can't have brackets
// around this since it is used directly.
# define U_MQTT_CLIENT_TEST_MQTT_BROKER_URL ubxlib.redirectme.net
#endif

#ifndef U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL
/** Server to use for MQTT client testing: must support [D]TLS
 * security.
 */
//lint -esym(773, U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL) Suppress not
// fully bracketed, Lint is wary of the "-" in here but we can't have
// brackets around this since it is used directly.
# define U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL ubxlib.redirectme.net:8883
#endif

#ifndef U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES
/** Maximum topic length for reading.
 */
# define U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES 126
#endif

#ifndef U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES
/** Maximum length for publishing a message to the broker;
 * this number should be 512 or 1024 but the limit on
 * SARA_R412M_02B is lower (at least on FW version M0.11.01,A.02.17),
 * hence this choice.
 */
# define U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES 126
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

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/**  The test MQTT context.
 */
static uMqttClientContext_t *gpMqttContextA = NULL;

/** A place to put the serial number of the module
 * which is used in the tests.
 */
static char gSerialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];

/** Data to send over MQTT; all printable characters.
 */
static const char gSendData[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789\"!#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

/** Flag to indicate that the disconnect callback
 * has been called.
 */
static bool gDisconnectCallbackCalled;

/** Keep track of the number of unread messages;
 * messageIndicationCallback gets a pointer to this.
 */
static int32_t gNumUnread;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular networkConnect process.
static bool keepGoingCallback(void)
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Do this before every test to ensure there is a usable network.
static uNetworkTestList_t *pStdPreamble(bool mqttSn)
{
    uNetworkTestList_t *pList;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Add the device for each network configuration
    // if not already added
    if (mqttSn) {
        pList = pUNetworkTestListAlloc(uNetworkTestHasMqttSn);
    } else {
        pList = pUNetworkTestListAlloc(uNetworkTestHasMqtt);
    }
    if (pList == NULL) {
        U_TEST_PRINT_LINE_MQTT("*** WARNING *** nothing to do.");
    }
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE_MQTT("adding device %s for network %s...",
                                   gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                                   gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // It is possible for MQTT client closure in an
    // underlying layer to have failed in a previous
    // test, leaving MQTT hanging, so just in case,
    // clear it up here
    if (gpMqttContextA != NULL) {
        uMqttClientClose(gpMqttContextA);
        gpMqttContextA = NULL;
    }

    return pList;
}

// Callback for unread message indications
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    int32_t *pNumUnread = (int32_t *) pParam;

#if !U_CFG_OS_CLIB_LEAKS
    // Only print stuff if the C library isn't going to leak
    U_TEST_PRINT_LINE_MQTT("messageIndicationCallback() called.");
    U_TEST_PRINT_LINE_MQTT("%d message(s) unread.", numUnread);
#endif

    *pNumUnread = numUnread;
}

// Callback for disconnects.
//lint -e{818} suppress "could be declared as pointing to const":
// need to follow the callback function signature.
static void disconnectCallback(int32_t errorCode, void *pParam)
{
    (void) pParam;

#if !U_CFG_OS_CLIB_LEAKS
    // Only print stuff if the C library isn't going to leak
    U_TEST_PRINT_LINE_MQTT("disconnectCallback() called.");
    U_TEST_PRINT_LINE_MQTT("last MQTT error code %d.", errorCode);
#else
    (void) errorCode;
#endif

    gDisconnectCallbackCalled = true;
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test MQTT connectivity with deliberately minimal option set.
 */
U_PORT_TEST_FUNCTION("[mqttClient]", "mqttClient")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t heapUsed;
    int32_t heapXxxSecurityInitLoss = 0;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    int32_t y;
    int32_t z;
    size_t s;
    int32_t startTimeMs;
    char *pTopicOut;
    char *pTopicIn;
    char *pMessageOut;
    char *pMessageIn;
    uMqttQos_t qos;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Do the standard preamble, which in this case
    // only adds the networks, doesn't bring them up,
    // since SARA-R4 will not connect with a different
    // security mode without being taken down first
    pList = pStdPreamble(false);

    // Repeat for all bearers
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;
        // Get the initial-ish heap
        heapUsed = uPortGetHeapFree();

        // Get a unique number we can use to stop parallel
        // tests colliding at the MQTT broker
        U_PORT_TEST_ASSERT(uSecurityGetSerialNumber(devHandle,
                                                    gSerialNumber) > 0);

        // Malloc space to read messages and topics into
        pTopicOut = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pTopicOut != NULL);
        pTopicIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pTopicIn != NULL);
        pMessageOut = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pMessageOut != NULL);
        //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
        pMessageIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pMessageIn != NULL);

        // Do the entire sequence twice, once without TLS security
        // and once with TLS security, taking the network down between
        // attempts because SARA-R4 cellular modules do not support
        // changing security mode without power-cycling the module
        for (size_t run = 0; run < 2; run++) {
            U_TEST_PRINT_LINE_MQTT("bringing up %s...",
                                   gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                                   pTmp->networkType,
                                                   pTmp->pNetworkCfg) == 0);
            // Make a unique topic name to stop different boards colliding
            snprintf(pTopicOut, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                     "ubx_test/%s", gSerialNumber);
            gNumUnread = 0;
            bool noTls = (run == 0) || (pTmp->networkType == U_NETWORK_TYPE_WIFI);
            // Open an MQTT client
            if (noTls) {
                U_TEST_PRINT_LINE_MQTT("opening MQTT client...");
                gpMqttContextA = pUMqttClientOpen(devHandle, NULL);
            } else {
                U_TEST_PRINT_LINE_MQTT("opening MQTT client, now with a TLS connection...");
                // Creating a secure connection may use heap in the underlying
                // network layer which will be reclaimed when the
                // network layer is closed but we don't do that here
                // to save time so need to allow for it in the heap loss
                // calculation
                heapXxxSecurityInitLoss += uPortGetHeapFree();
                gpMqttContextA = pUMqttClientOpen(devHandle, &tlsSettings);
                heapXxxSecurityInitLoss -= uPortGetHeapFree();
            }

            if (gpMqttContextA != NULL) {
                y = uMqttClientOpenResetLastError();
                U_TEST_PRINT_LINE_MQTT("opening MQTT client returned %d.", y);
                U_PORT_TEST_ASSERT(y == 0);
                // Set a disconnect callback
                gDisconnectCallbackCalled = false;
                uMqttClientSetDisconnectCallback(gpMqttContextA, disconnectCallback, NULL);
                U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);

                U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));

                if (noTls) {
                    connection.pBrokerNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_BROKER_URL);
#ifdef U_MQTT_CLIENT_TEST_MQTT_USERNAME
                    connection.pUserNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_USERNAME),
#endif
#ifdef U_MQTT_CLIENT_TEST_MQTT_PASSWORD
                    connection.pPasswordStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_PASSWORD),
#endif
                } else {
                    connection.pBrokerNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL);
#ifdef U_MQTT_CLIENT_TEST_MQTT_SECURE_USERNAME
                    connection.pUserNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_SECURE_USERNAME),
#endif
#ifdef U_MQTT_CLIENT_TEST_MQTT_SECURE_PASSWORD
                    connection.pPasswordStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_SECURE_PASSWORD),
#endif
                }
                connection.pKeepGoingCallback = keepGoingCallback;

                // Connect it
                U_TEST_PRINT_LINE_MQTT("connecting to \"%s\"...", connection.pBrokerNameStr);
                startTimeMs = uPortGetTickTimeMs();
                gStopTimeMs = startTimeMs +
                              (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                y = uMqttClientConnect(gpMqttContextA, &connection);
                z = uMqttClientOpenResetLastError();
                if (y == 0) {
                    U_TEST_PRINT_LINE_MQTT("connect successful after %d ms.",
                                           (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                    U_PORT_TEST_ASSERT(z == 0);
                    // Note: can't check the return value here as it is
                    // utterly module specific, only really checking that it
                    // doesn't bring the roof down
                    uMqttClientGetLastErrorCode(gpMqttContextA);
                    U_PORT_TEST_ASSERT(uMqttClientIsConnected(gpMqttContextA));
                    U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);

                    // Set the message indication callback
                    U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                     messageIndicationCallback,
                                                                     &gNumUnread) == 0);

                    U_TEST_PRINT_LINE_MQTT("subscribing to topic \"%s\"...", pTopicOut);
                    startTimeMs = uPortGetTickTimeMs();
                    gStopTimeMs = startTimeMs +
                                  (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    y = uMqttClientSubscribe(gpMqttContextA, pTopicOut, U_MQTT_QOS_EXACTLY_ONCE);
                    if (y >= 0) {
                        U_TEST_PRINT_LINE_MQTT("subscribe successful after %d ms, QoS %d.",
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs), y);
                    } else {
                        U_TEST_PRINT_LINE_MQTT("subscribe returned error %d after %d ms,"
                                               " module error %d.", y,
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                               uMqttClientGetLastErrorCode(gpMqttContextA));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    }

                    // There may be unread messages sitting on the server from a previous test run,
                    // read them off here.
                    z = uMqttClientGetUnread(gpMqttContextA);
                    while ((y = uMqttClientGetUnread(gpMqttContextA)) > 0) {
                        U_TEST_PRINT_LINE_MQTT("reading existing unread message %d of %d.", y, z);
                        U_PORT_TEST_ASSERT(uMqttClientMessageRead(gpMqttContextA,
                                                                  pTopicIn,
                                                                  U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                                                                  pMessageIn, &s,
                                                                  NULL) == 0);
                        //lint -e(668) Suppress possible use of NULL pointers,
                        // they are checked above
                        U_PORT_TEST_ASSERT(strcmp(pTopicIn, pTopicOut) == 0);
                    }

                    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == 0);

                    U_TEST_PRINT_LINE_MQTT("publishing %d byte(s) to topic \"%s\"...",
                                           U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES, pTopicOut);
                    startTimeMs = uPortGetTickTimeMs();
                    gStopTimeMs = startTimeMs +
                                  (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    // Fill in the outgoing message buffer with all possible things
                    s = 0;
                    y = U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES;
                    while (y > 0) {
                        z = sizeof(gSendData) - 1; // -1 to remove the terminator
                        if (z > y) {
                            z = y;
                        }
                        memcpy(pMessageOut + s, gSendData, z);
                        y -= z;
                        s += z;
                    }
                    y = uMqttClientPublish(gpMqttContextA, pTopicOut, pMessageOut,
                                           U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES,
                                           U_MQTT_QOS_EXACTLY_ONCE, false);
                    if (y == 0) {
                        U_TEST_PRINT_LINE_MQTT("publish successful after %d ms.",
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                        // We've just sent a message
                        U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(gpMqttContextA) > 0);
                    } else {
                        U_TEST_PRINT_LINE_MQTT("publish returned error %d after %d ms, module"
                                               " error %d.", y,
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                               uMqttClientGetLastErrorCode(gpMqttContextA));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    }

                    U_TEST_PRINT_LINE_MQTT("waiting for an unread message indication...");
                    startTimeMs = uPortGetTickTimeMs();
                    while ((gNumUnread == 0) &&
                           (uPortGetTickTimeMs() < startTimeMs +
                            (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000))) {
                        uPortTaskBlock(1000);
                    }

                    if (gNumUnread > 0) {
                        U_TEST_PRINT_LINE_MQTT("%d message(s) unread.", gNumUnread);
                    } else {
                        U_TEST_PRINT_LINE_MQTT("no messages unread after %d ms.",
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    }

                    U_PORT_TEST_ASSERT(gNumUnread == 1);
                    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == gNumUnread);

                    U_TEST_PRINT_LINE_MQTT("reading the message...");
                    qos = U_MQTT_QOS_MAX_NUM;
                    s = U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES;
                    U_PORT_TEST_ASSERT(uMqttClientMessageRead(gpMqttContextA,
                                                              pTopicIn,
                                                              U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                                                              pMessageIn, &s,
                                                              &qos) == 0);
                    U_TEST_PRINT_LINE_MQTT("read %d byte(s).", s);
                    if (pTmp->networkType != U_NETWORK_TYPE_WIFI) {
                        // Wifi doesn't support the qos parameter on read
                        U_PORT_TEST_ASSERT(qos == U_MQTT_QOS_EXACTLY_ONCE);
                    }
                    //lint -e(802) Suppress possible use of NULL pointers,
                    // they are checked above
                    U_PORT_TEST_ASSERT(strcmp(pTopicIn, pTopicOut) == 0);
                    U_PORT_TEST_ASSERT(s == U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES);
                    // Total message received must be non-zero
                    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(gpMqttContextA) > 0);
                    //lint -e(668, 802) Suppress possible use of NULL pointers,
                    // they are checked above
                    U_PORT_TEST_ASSERT(memcmp(pMessageIn, pMessageOut, s) == 0);

                    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == 0);

                    // Cancel the subscribe
                    U_TEST_PRINT_LINE_MQTT("unsubscribing from topic \"%s\"...", pTopicOut);
                    gStopTimeMs = uPortGetTickTimeMs() +
                                  (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    U_PORT_TEST_ASSERT(uMqttClientUnsubscribe(gpMqttContextA, pTopicOut) == 0);

                    // Remove the callback
                    U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                     NULL, NULL) == 0);

                    // Disconnect MQTT
                    U_TEST_PRINT_LINE_MQTT("disconnecting from \"%s\"...", connection.pBrokerNameStr);
                    U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);
                    U_PORT_TEST_ASSERT(uMqttClientDisconnect(gpMqttContextA) == 0);
                    U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));
                    uPortTaskBlock(U_CFG_OS_YIELD_MS);
                    if (pTmp->networkType != U_NETWORK_TYPE_CELL) {
                        // Cellular only calls the disconnect callback when
                        // dropped unexpectedly
                        U_PORT_TEST_ASSERT(gDisconnectCallbackCalled);
                    }
                } else {
                    if (noTls) {
                        U_TEST_PRINT_LINE_MQTT("connection failed after %d ms,"
                                               " with error %d, module error %d.",
                                               (int32_t) (uPortGetTickTimeMs() - startTimeMs), z,
                                               uMqttClientGetLastErrorCode(gpMqttContextA));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    } else {
                        U_TEST_PRINT_LINE_MQTT("MQTT security not supported.");
                    }
                }

                // Close the entire context
                uMqttClientClose(gpMqttContextA);
                gpMqttContextA = NULL;
            }
            U_TEST_PRINT_LINE_MQTT("taking down %s...",
                                   gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(devHandle,
                                                     pTmp->networkType) == 0);
        }

        // Free memory
        uPortFree(pMessageIn);
        uPortFree(pMessageOut);
        uPortFree(pTopicIn);
        uPortFree(pTopicOut);

        // Check for memory leaks
        heapUsed -= uPortGetHeapFree();
        U_TEST_PRINT_LINE_MQTT("%d byte(s) were lost to security initialisation;"
                               " we have leaked %d byte(s).", heapXxxSecurityInitLoss,
                               heapUsed - heapXxxSecurityInitLoss);
        U_PORT_TEST_ASSERT(heapUsed <= heapXxxSecurityInitLoss);
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE_MQTT("closing device %s...",
                                   gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();
}

#ifndef U_CFG_TEST_MQTT_CLIENT_SN_DISABLE_CONNECTIVITY_TEST

/** Test MQTT-SN connectivity.
 */
U_PORT_TEST_FUNCTION("[mqttClient]", "mqttClientSn")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle = NULL;
    int32_t heapUsed;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    int32_t y;
    int32_t z;
    size_t s;
    int32_t startTimeMs;
    char *pTopicNameOutMqtt;
    uMqttSnTopicName_t topicNameOut;
    uMqttSnTopicName_t topicNameIn;
    char *pMessageOut;
    char *pMessageIn;
    uMqttQos_t qos;
    char topicNameShortStr[U_MQTT_CLIENT_SN_TOPIC_NAME_SHORT_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    connection.mqttSn = true;

    // Bring up devices supporting MQTT-SN
    pList = pStdPreamble(true);

    // Repeat for all bearers
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;
        // Get the initial-ish heap
        heapUsed = uPortGetHeapFree();

        U_TEST_PRINT_LINE_MQTTSN("bringing up %s...",
                                 gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);

        // Get a unique number we can use to stop parallel
        // tests colliding at the MQTT-SN broker
        U_PORT_TEST_ASSERT(uSecurityGetSerialNumber(devHandle,
                                                    gSerialNumber) > 0);

        // Malloc space to read messages and topics into
        pTopicNameOutMqtt = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pTopicNameOutMqtt != NULL);
        pMessageOut = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pMessageOut != NULL);
        //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
        pMessageIn = (char *) pUPortMalloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(pMessageIn != NULL);

        // NOTE: would run the following in a loop, the second iteration doing DTLS
        // testing, however the Paho MQTT-SN Gateway version we are using for DTLS
        // testing becomes unresponsive after the first MQTT-SN session is closed,
        // requiring a restart of the service, hence regression testing is not viable.

        // Make a unique topic name to stop different boards colliding
        snprintf(pTopicNameOutMqtt, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                 "ubx_test/%s", gSerialNumber);
        gNumUnread = 0;
        // Open an MQTT-SN client
        U_TEST_PRINT_LINE_MQTTSN("opening MQTT-SN client...");
        gpMqttContextA = pUMqttClientOpen(devHandle, NULL);

        if ((gpMqttContextA != NULL) && (uMqttClientSnIsSupported(gpMqttContextA))) {
            y = uMqttClientOpenResetLastError();
            U_TEST_PRINT_LINE_MQTTSN("opening MQTT-SN client returned %d.", y);
            U_PORT_TEST_ASSERT(y == 0);
            // Set a disconnect callback
            gDisconnectCallbackCalled = false;
            uMqttClientSetDisconnectCallback(gpMqttContextA, disconnectCallback, NULL);
            U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);

            U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));

            connection.pBrokerNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_BROKER_URL);
#ifdef U_MQTT_CLIENT_TEST_MQTT_USERNAME
            connection.pUserNameStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_USERNAME),
#endif
#ifdef U_MQTT_CLIENT_TEST_MQTT_PASSWORD
            connection.pPasswordStr = U_PORT_STRINGIFY_QUOTED(U_MQTT_CLIENT_TEST_MQTT_PASSWORD),
#endif
            connection.pKeepGoingCallback = keepGoingCallback;

            // Connect it
            U_TEST_PRINT_LINE_MQTTSN("connecting to \"%s\"...", connection.pBrokerNameStr);
            startTimeMs = uPortGetTickTimeMs();
            gStopTimeMs = startTimeMs +
                          (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
            y = uMqttClientConnect(gpMqttContextA, &connection);
            z = uMqttClientOpenResetLastError();
            if (y == 0) {
                U_TEST_PRINT_LINE_MQTTSN("connect successful after %d ms.",
                                         (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                U_PORT_TEST_ASSERT(z == 0);
                // Note: can't check the return value here as it is
                // utterly module specific, only really checking that it
                // doesn't bring the roof down
                uMqttClientGetLastErrorCode(gpMqttContextA);
                U_PORT_TEST_ASSERT(uMqttClientIsConnected(gpMqttContextA));
                U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);

                // Set the message indication callback
                U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                 messageIndicationCallback,
                                                                 &gNumUnread) == 0);

                U_TEST_PRINT_LINE_MQTTSN("subscribing to MQTT topic \"%s\"...", pTopicNameOutMqtt);
                startTimeMs = uPortGetTickTimeMs();
                gStopTimeMs = startTimeMs +
                              (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                memset(&topicNameOut, 0xFF, sizeof(topicNameOut));
                y = uMqttClientSnSubscribeNormalTopic(gpMqttContextA, pTopicNameOutMqtt,
                                                      U_MQTT_QOS_EXACTLY_ONCE, &topicNameOut);
                if (y >= 0) {
                    U_TEST_PRINT_LINE_MQTTSN("subscribe successful after %d ms, topic ID \"%d\","
                                             " QoS %d.", (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                             topicNameOut.name.id, y);
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameOut) ==
                                       U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL);
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicId(&topicNameOut) >= 0);
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameShort(&topicNameOut, topicNameShortStr) < 0);
                } else {
                    U_TEST_PRINT_LINE_MQTTSN("subscribe returned error %d after %d ms,"
                                             " module error %d.", y,
                                             (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                             uMqttClientGetLastErrorCode(gpMqttContextA));
                    //lint -e(506, 774) Suppress constant value Boolean
                    U_PORT_TEST_ASSERT(false);
                }

                // There may be unread messages sitting on the server from a previous test run,
                // read them off here.
                z = uMqttClientGetUnread(gpMqttContextA);
                while ((y = uMqttClientGetUnread(gpMqttContextA)) > 0) {
                    U_TEST_PRINT_LINE_MQTTSN("reading existing unread message %d of %d.", y, z);
                    U_PORT_TEST_ASSERT(uMqttClientSnMessageRead(gpMqttContextA, &topicNameIn,
                                                                pMessageIn, &s, NULL) == 0);
                }

                U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == 0);

                // Do this twice, once with the topic ID returned by uMqttClientSnSubscribe()
                // above and a second time with one returned by uMqttClientSnRegisterNormalTopic()
                for (size_t idRun = 0; idRun < 2; idRun++) {
                    U_TEST_PRINT_LINE_MQTTSN("publishing %d byte(s) to topic \"%d\"...",
                                             U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES,
                                             topicNameOut.name.id);
                    startTimeMs = uPortGetTickTimeMs();
                    gStopTimeMs = startTimeMs +
                                  (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    // Fill in the outgoing message buffer with all possible things
                    s = 0;
                    y = U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES;
                    while (y > 0) {
                        z = sizeof(gSendData) - 1; // -1 to remove the terminator
                        if (z > y) {
                            z = y;
                        }
                        memcpy(pMessageOut + s, gSendData, z);
                        y -= z;
                        s += z;
                    }
                    y = uMqttClientSnPublish(gpMqttContextA, &topicNameOut,
                                             pMessageOut, U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES,
                                             U_MQTT_QOS_EXACTLY_ONCE, false);
                    if (y == 0) {
                        U_TEST_PRINT_LINE_MQTTSN("publish successful after %d ms.",
                                                 (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                        // We've just sent a message
                        U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(gpMqttContextA) > 0);
                    } else {
                        U_TEST_PRINT_LINE_MQTTSN("publish returned error %d after %d ms, module"
                                                 " error %d.", y,
                                                 (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                                 uMqttClientGetLastErrorCode(gpMqttContextA));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    }

                    U_TEST_PRINT_LINE_MQTTSN("waiting for an unread message indication...");
                    startTimeMs = uPortGetTickTimeMs();
                    while ((gNumUnread == 0) &&
                           (uPortGetTickTimeMs() - startTimeMs <
                            (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000))) {
                        uPortTaskBlock(1000);
                    }

                    if (gNumUnread > 0) {
                        U_TEST_PRINT_LINE_MQTTSN("%d message(s) unread.", gNumUnread);
                    } else {
                        U_TEST_PRINT_LINE_MQTTSN("no messages unread after %d ms.",
                                                 (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                        //lint -e(506, 774) Suppress constant value Boolean
                        U_PORT_TEST_ASSERT(false);
                    }

                    U_PORT_TEST_ASSERT(gNumUnread == 1);
                    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == gNumUnread);

                    U_TEST_PRINT_LINE_MQTTSN("reading the message...");
                    memset(&topicNameIn, 0xFF, sizeof(topicNameIn));
                    qos = U_MQTT_QOS_MAX_NUM;
                    s = U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES;
                    U_PORT_TEST_ASSERT(uMqttClientSnMessageRead(gpMqttContextA, &topicNameIn,
                                                                pMessageIn, &s, &qos) == 0);
                    U_TEST_PRINT_LINE_MQTTSN("read %d byte(s).", s);
                    U_PORT_TEST_ASSERT(qos == U_MQTT_QOS_EXACTLY_ONCE);
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameIn) ==
                                       U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL);
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicId(&topicNameIn) == uMqttClientSnGetTopicId(&topicNameOut));
                    U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameShort(&topicNameIn, topicNameShortStr) < 0);
                    U_PORT_TEST_ASSERT(s == U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES);
                    // Total message received must be non-zero
                    U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesReceived(gpMqttContextA) > 0);
                    //lint -e(668, 802) Suppress possible use of NULL pointers,
                    // they are checked above
                    U_PORT_TEST_ASSERT(memcmp(pMessageIn, pMessageOut, s) == 0);

                    U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == 0);

                    if (idRun == 0) {
                        // Now register an ID for the same topic for use on the next turn
                        // around this loop
                        U_TEST_PRINT_LINE_MQTTSN("registering MQTT topic \"%s\"...", pTopicNameOutMqtt);
                        memset(&topicNameOut, 0xFF, sizeof(topicNameOut));
                        U_PORT_TEST_ASSERT(uMqttClientSnRegisterNormalTopic(gpMqttContextA, pTopicNameOutMqtt,
                                                                            &topicNameOut) == 0);
                        U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameOut) ==
                                           U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL);
                        U_PORT_TEST_ASSERT(uMqttClientSnGetTopicId(&topicNameOut) >= 0);
                        U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameShort(&topicNameOut, topicNameShortStr) < 0);
                    }
                }

                // Cancel the subscribe
                U_TEST_PRINT_LINE_MQTTSN("unsubscribing from topic \"%d\"...", topicNameOut.name.id);
                gStopTimeMs = uPortGetTickTimeMs() +
                              (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                U_PORT_TEST_ASSERT(uMqttClientSnUnsubscribeNormalTopic(gpMqttContextA, pTopicNameOutMqtt) == 0);

                // Remove the callback
                U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                 NULL, NULL) == 0);

                // The above has tested publish/read in a nice organised way but
                // has only tested subscription to MQTT-style topics.  Since the
                // MQTT-SN-style topics don't have a "directory" type structure to
                // them, we can't really test publish/read since the various test
                // units we run in parallel would collide.  However, we can test
                // the act of subscribing and unsubscribing, which should be enough
                // since the publish/read functions are 99% similar anyway

                // Test predefined topic ID
                U_TEST_PRINT_LINE_MQTTSN("testing predefined topic IDs...");
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicIdPredefined(1, NULL) < 0);
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicIdPredefined(65535, &topicNameOut) == 0);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameOut) ==
                                   U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicId(&topicNameOut) == 65535);
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicIdPredefined(1, &topicNameOut) == 0);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameOut) ==
                                   U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicId(&topicNameOut) == 1);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameShort(&topicNameOut, topicNameShortStr) < 0);
                // Unfortunately the Paho MQTT-SN Gateway we use for testing dies
                // with a segmentation fault if you try to subscribe to a predefined
                // topic ID, so we don't do that.

                // Test short topic name
                U_TEST_PRINT_LINE_MQTTSN("testing short topic names...");
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicNameShort("ab", NULL) < 0);
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicNameShort("a", &topicNameOut) < 0);
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicNameShort("abc", &topicNameOut) < 0);

                memset(topicNameShortStr, 'a', sizeof(topicNameShortStr));
                *(topicNameShortStr + sizeof(topicNameShortStr) - 1) = 0;
                U_PORT_TEST_ASSERT(uMqttClientSnSetTopicNameShort("xy", &topicNameOut) == 0);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameType(&topicNameOut) ==
                                   U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT);
                U_PORT_TEST_ASSERT(uMqttClientSnGetTopicNameShort(&topicNameOut, topicNameShortStr) == 2);
                U_PORT_TEST_ASSERT(strlen(topicNameShortStr) == 2);
                U_PORT_TEST_ASSERT(strcmp(topicNameShortStr, "xy") == 0);
                // And again, unfortunately the Paho MQTT-SN Gateway we use for testing
                // doesn't seem to be able to accept subscriptions reliably, leading
                // to random AT timeout failures if we subscribe to a short name here
                // So we don't do that either.  Need a better test peer.

                // Disconnect MQTT
                U_TEST_PRINT_LINE_MQTTSN("disconnecting from \"%s\"...", connection.pBrokerNameStr);
                U_PORT_TEST_ASSERT(!gDisconnectCallbackCalled);
                U_PORT_TEST_ASSERT(uMqttClientDisconnect(gpMqttContextA) == 0);
                U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
                if (pTmp->networkType != U_NETWORK_TYPE_CELL) {
                    // Cellular only calls the disconnect callback when
                    // dropped unexpectedly
                    U_PORT_TEST_ASSERT(gDisconnectCallbackCalled);
                }
            } else {
                U_TEST_PRINT_LINE_MQTTSN("connection failed after %d ms,"
                                         " with error %d, module error %d.",
                                         (int32_t) (uPortGetTickTimeMs() - startTimeMs), z,
                                         uMqttClientGetLastErrorCode(gpMqttContextA));
                //lint -e(506, 774) Suppress constant value Boolean
                U_PORT_TEST_ASSERT(false);
            }

            // Close the entire context
            uMqttClientClose(gpMqttContextA);
            gpMqttContextA = NULL;
        } else {
            U_TEST_PRINT_LINE_MQTTSN("MQTT-SN not supported.");
            if (gpMqttContextA != NULL) {
                uMqttClientClose(gpMqttContextA);
                gpMqttContextA = NULL;
            }
        }

        // Free memory
        uPortFree(pMessageIn);
        uPortFree(pMessageOut);
        uPortFree(pTopicNameOutMqtt);

        U_TEST_PRINT_LINE_MQTTSN("taking down cellular network...");
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(devHandle,
                                                 pTmp->networkType) == 0);

        // Check for memory leaks
        heapUsed -= uPortGetHeapFree();
        U_TEST_PRINT_LINE_MQTTSN("we have leaked %d byte(s).", heapUsed);
        U_PORT_TEST_ASSERT(heapUsed <= 0);
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE_MQTTSN("closing device %s...",
                                     gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();
}

#endif // #ifndef U_CFG_TEST_MQTT_CLIENT_SN_DISABLE_CONNECTIVITY_TEST

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[mqttClient]", "mqttClientCleanUp")
{
    int32_t y;

    if (gpMqttContextA != NULL) {
        uMqttClientClose(gpMqttContextA);
        gpMqttContextA = NULL;
    }

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    uDeviceDeinit();

    y = uPortTaskStackMinFree(NULL);
    if (y != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE_MQTTSN("main task stack had a minimum of %d"
                                 " byte(s) free at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        U_TEST_PRINT_LINE_MQTTSN("heap had a minimum of %d byte(s) free"
                                 " at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
