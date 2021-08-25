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

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strcmp(), memcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_security_tls.h"
#include "u_security.h"     // For uSecurityGetSerialNumber()

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_MQTT_CLIENT_TEST_MQTT_BROKER_URL
/** Server to use for MQTT client testing, non secure.
 */
# define U_MQTT_CLIENT_TEST_MQTT_BROKER_URL  broker.emqx.io
#endif

#ifndef U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL
/** Server to use for MQTT client testing: must support TLS
 * security.
 */
# define U_MQTT_CLIENT_TEST_MQTT_SECURE_BROKER_URL  broker.emqx.io:8883
#endif

#ifndef U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES
/** Maximum topic length for reading.
 */
# define U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES 128
#endif

#ifndef U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES
/** Maximum length for publishing a message to the broker;
 * this number should be 512 or 1024 but the limit on
 * SARA_R412M_02B is lower, hence this choice.
 */
# define U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES 128
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

/** Data to send over MQTT.
 */
static const char gSendData[] =  "______000:0123456789012345678901234567890123456789"
                                 "______050:0123456789012345678901234567890123456789";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular networkConnect process.
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Do this before every test to ensure there is a usable network.
static void stdPreamble()
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type if its not already been added
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if ((gUNetworkTestCfg[x].handle < 0) &&
            U_NETWORK_TEST_TYPE_HAS_MQTT(gUNetworkTestCfg[x].type)) {
            if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
                uPortLog("U_MQTT_CLIENT_TEST: adding %s network...\n",
                         gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                         gUNetworkTestCfg[x].pConfiguration);
                U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
            }
        }
    }

    // It is possible for MQTT client closure in an
    // underlying layer to have failed in a previous
    // test, leaving QMTT hanging, so just in case,
    // clear it up here
    if (gpMqttContextA != NULL) {
        uMqttClientClose(gpMqttContextA);
        gpMqttContextA = NULL;
    }
}

// Callback for unread message indications
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    int32_t *pNumUnread = (int32_t *) pParam;

#if !U_CFG_OS_CLIB_LEAKS
    // Only print stuff if the C library isn't going to leak
    uPortLog("U_MQTT_CLIENT_TEST: messageIndicationCallback() called.\n");
    uPortLog("U_MQTT_CLIENT_TEST: %d message(s) unread.\n", numUnread);
#endif

    *pNumUnread = numUnread;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test MQTT connectivity with deliberately minimal option set,
 * unsecure.
 * TODO: limits checking.
 * TODO: when we have a public u-blox MQTT test server, do some more
 * thrash tests.
 */
U_PORT_TEST_FUNCTION("[mqttClient]", "mqttClient")
{
    int32_t networkHandle;
    int32_t heapUsed;
    int32_t heapXxxSecurityInitLoss = 0;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    int32_t y;
    int32_t z;
    size_t s;
    int32_t numUnread = 0;
    int64_t startTimeMs;
    char *pTopicOut;
    char *pTopicIn;
    char *pMessageOut;
    char *pMessageIn;
    uMqttQos_t qos;

    // Do the standard preamble, which in this case
    // only adds the networks, doesn't bring them up,
    // since SARA-R4 will not connect with a different
    // security mode without being taken down first
    stdPreamble();

    // Repeat for all bearers
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        networkHandle = gUNetworkTestCfg[x].handle;
        if (networkHandle >= 0) {
            // Get the initial-ish heap
            heapUsed = uPortGetHeapFree();

            // Get a unique number we can use to stop parallel
            // tests colliding at the MQTT broker
            U_PORT_TEST_ASSERT(uSecurityGetSerialNumber(networkHandle,
                                                        gSerialNumber) > 0);

            // Malloc space to read messages and topics into
            pTopicOut = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
            U_PORT_TEST_ASSERT(pTopicOut != NULL);
            pTopicIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES);
            U_PORT_TEST_ASSERT(pTopicIn != NULL);
            pMessageOut = (char *) malloc(U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES);
            U_PORT_TEST_ASSERT(pMessageOut != NULL);
            //lint -esym(613, pMessageOut) Suppress possible use of NULL pointer in future
            pMessageIn = (char *) malloc(U_MQTT_CLIENT_TEST_READ_MESSAGE_MAX_LENGTH_BYTES);
            U_PORT_TEST_ASSERT(pMessageIn != NULL);

            // Do the entire sequence twice, once without TLS security
            // and once with TLS security, taking the network down between
            // attempts because SARA-R4 cellular modules do not support
            // changing security mode without power-cycling the module
            for (size_t run = 0; run < 2; run++) {
                uPortLog("U_MQTT_CLIENT_TEST: bringing up %s...\n",
                         gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                U_PORT_TEST_ASSERT(uNetworkUp(gUNetworkTestCfg[x].handle) == 0);

                // Make a unique topic name to stop different boards colliding
                snprintf(pTopicOut, U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                         "ubx_test/%s", gSerialNumber);

                // Open an MQTT client
                if (run == 0) {
                    uPortLog("U_MQTT_CLIENT_TEST: opening MQTT client...\n");
                    gpMqttContextA = pUMqttClientOpen(networkHandle, NULL);
                } else {
                    uPortLog("U_MQTT_CLIENT_TEST: opening MQTT client, now with a"
                             " TLS connection...\n");
                    // Creating a secure connection may use heap in the underlying
                    // network layer which will be reclaimed when the
                    // network layer is closed but we don't do that here
                    // to save time so need to allow for it in the heap loss
                    // calculation
                    heapXxxSecurityInitLoss += uPortGetHeapFree();
                    gpMqttContextA = pUMqttClientOpen(networkHandle, &tlsSettings);
                    heapXxxSecurityInitLoss -= uPortGetHeapFree();
                }

                if (gpMqttContextA != NULL) {
                    y = uMqttClientOpenResetLastError();
                    uPortLog("U_MQTT_CLIENT_TEST: opening MQTT client"
                             " returned %d.\n", y);
                    U_PORT_TEST_ASSERT(y == 0);
                    U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));

                    if (run == 0) {
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
                    uPortLog("U_MQTT_CLIENT_TEST: connecting to \"%s\"...\n",
                             connection.pBrokerNameStr);
                    startTimeMs = uPortGetTickTimeMs();
                    gStopTimeMs = startTimeMs +
                                  (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    y = uMqttClientConnect(gpMqttContextA, &connection);
                    z = uMqttClientOpenResetLastError();
                    if (y == 0) {
                        uPortLog("U_MQTT_CLIENT_TEST: connect successful after %d ms.\n",
                                 (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                        U_PORT_TEST_ASSERT(z == 0);
                        // Note: can't check the return value here as it is
                        // utterly module specific, only really checking that it
                        // doesn't bring the roof down
                        uMqttClientGetLastErrorCode(gpMqttContextA);
                        U_PORT_TEST_ASSERT(uMqttClientIsConnected(gpMqttContextA));

                        // Set the message indication callback
                        U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                         messageIndicationCallback,
                                                                         &numUnread) == 0);

                        uPortLog("U_MQTT_CLIENT_TEST: subscribing to topic \"%s\"...\n", pTopicOut);
                        startTimeMs = uPortGetTickTimeMs();
                        gStopTimeMs = startTimeMs +
                                      (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                        y = uMqttClientSubscribe(gpMqttContextA, pTopicOut, U_MQTT_QOS_EXACTLY_ONCE);
                        if (y >= 0) {
                            uPortLog("U_MQTT_CLIENT_TEST: subscribe successful after %d ms, QoS %d.\n",
                                     (int32_t) (uPortGetTickTimeMs() - startTimeMs), y);
                        } else {
                            uPortLog("U_MQTT_CLIENT_TEST: subscribe returned error %d after %d ms,"
                                     " module error %d.\n",
                                     y, (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                     uMqttClientGetLastErrorCode(gpMqttContextA));
                            //lint -e(506, 774) Suppress constant value Boolean
                            U_PORT_TEST_ASSERT(false);
                        }

                        // There may be unread messages sitting on the server from a previous test run,
                        // read them off here.
                        z = uMqttClientGetUnread(gpMqttContextA);
                        while ((y = uMqttClientGetUnread(gpMqttContextA)) > 0) {
                            uPortLog("U_MQTT_CLIENT_TEST: reading existing unread message %d of %d.\n",
                                     y, z);
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

                        uPortLog("U_MQTT_CLIENT_TEST: publishing %d byte(s) to topic \"%s\"...\n",
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
                            uPortLog("U_MQTT_CLIENT_TEST: publish successful after %d ms.\n",
                                     (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                            // We've just sent a message
                            U_PORT_TEST_ASSERT(uMqttClientGetTotalMessagesSent(gpMqttContextA) > 0);
                        } else {
                            uPortLog("U_MQTT_CLIENT_TEST: publish returned error %d after %d ms, module"
                                     " error %d.\n",
                                     y, (int32_t) (uPortGetTickTimeMs() - startTimeMs),
                                     uMqttClientGetLastErrorCode(gpMqttContextA));
                            //lint -e(506, 774) Suppress constant value Boolean
                            U_PORT_TEST_ASSERT(false);
                        }

                        uPortLog("U_MQTT_CLIENT_TEST: waiting for an unread message indication...\n");
                        startTimeMs = uPortGetTickTimeMs();
                        while ((numUnread == 0) &&
                               (uPortGetTickTimeMs() < startTimeMs +
                                (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000))) {
                            uPortTaskBlock(1000);
                        }

                        if (numUnread > 0) {
                            uPortLog("U_MQTT_CLIENT_TEST: %d message(s) unread.\n", numUnread);
                        } else {
                            uPortLog("U_MQTT_CLIENT_TEST: no messages unread after %d ms.\n",
                                     (int32_t) (uPortGetTickTimeMs() - startTimeMs));
                            //lint -e(506, 774) Suppress constant value Boolean
                            U_PORT_TEST_ASSERT(false);
                        }

                        U_PORT_TEST_ASSERT(numUnread == 1);
                        U_PORT_TEST_ASSERT(uMqttClientGetUnread(gpMqttContextA) == numUnread);

                        uPortLog("U_MQTT_CLIENT_TEST: reading the message...\n");
                        qos = U_MQTT_QOS_MAX_NUM;
                        s = U_MQTT_CLIENT_TEST_PUBLISH_MAX_LENGTH_BYTES;
                        U_PORT_TEST_ASSERT(uMqttClientMessageRead(gpMqttContextA,
                                                                  pTopicIn,
                                                                  U_MQTT_CLIENT_TEST_READ_TOPIC_MAX_LENGTH_BYTES,
                                                                  pMessageIn, &s,
                                                                  &qos) == 0);
                        uPortLog("U_MQTT_CLIENT_TEST: read %d byte(s).\n", s);
                        U_PORT_TEST_ASSERT(qos == U_MQTT_QOS_EXACTLY_ONCE);
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
                        uPortLog("U_MQTT_CLIENT_TEST: unsubscribing from topic \"%s\"...\n",
                                 pTopicOut);
                        gStopTimeMs = uPortGetTickTimeMs() +
                                      (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                        U_PORT_TEST_ASSERT(uMqttClientUnsubscribe(gpMqttContextA, pTopicOut) == 0);

                        // Remove the callback
                        U_PORT_TEST_ASSERT(uMqttClientSetMessageCallback(gpMqttContextA,
                                                                         NULL, NULL) == 0);

                        // Disconnect MQTT
                        uPortLog("U_MQTT_CLIENT_TEST: disconnecting from \"%s\"...\n",
                                 connection.pBrokerNameStr);
                        U_PORT_TEST_ASSERT(uMqttClientDisconnect(gpMqttContextA) == 0);
                        U_PORT_TEST_ASSERT(!uMqttClientIsConnected(gpMqttContextA));
                    } else {
                        if (run == 0) {
                            uPortLog("U_MQTT_CLIENT_TEST: connection failed after %d ms,"
                                     " with error %d, module error %d.\n",
                                     (int32_t) (uPortGetTickTimeMs() - startTimeMs), z,
                                     uMqttClientGetLastErrorCode(gpMqttContextA));
                            //lint -e(506, 774) Suppress constant value Boolean
                            U_PORT_TEST_ASSERT(false);
                        } else {
                            uPortLog("U_MQTT_CLIENT_TEST: MQTT security not supported.\n");
                        }
                    }

                    // Close the entire context
                    uMqttClientClose(gpMqttContextA);
                    gpMqttContextA = NULL;

                    uPortLog("U_MQTT_CLIENT_TEST: taking down %s...\n",
                             gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
                    U_PORT_TEST_ASSERT(uNetworkDown(networkHandle) == 0);
                }
            }

            // Free memory
            free(pMessageIn);
            free(pMessageOut);
            free(pTopicIn);
            free(pTopicOut);

            // Check for memory leaks
            heapUsed -= uPortGetHeapFree();
            uPortLog("U_MQTT_CLIENT_TEST: %d byte(s) were lost to security"
                     " initialisation; we have leaked %d byte(s).\n",
                     heapXxxSecurityInitLoss,
                     heapUsed - heapXxxSecurityInitLoss);
            U_PORT_TEST_ASSERT(heapUsed <= heapXxxSecurityInitLoss);
        } else {
            uPortLog("U_MQTT_CLIENT_TEST: MQTT not supported on %s,"
                     " skipping...\n", gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
        }
    }
}

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

    // The network test configuration is shared
    // between the network and sockets tests so
    // must reset the handles here in case the
    // sockets API tests are coming next.
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    uPortLog("U_MQTT_CLIENT_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", y);
    U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_MQTT_CLIENT_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
