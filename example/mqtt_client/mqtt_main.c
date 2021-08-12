/*
 * Copyright 2020 u-blox Cambourne Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @brief This example demonstrates how to use the common MQTT API
 * to talk to an MQTT broker on the public internet using a u-blox
 * module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

// Required by ubxlib
#include "u_port.h"
#include "u_port_os.h"

// The next two lines will cause uPortLog() output
// to be sent to ubxlib's chosen trace output.
// Comment them out to send the uPortLog() output
// to print() instead.
#include "u_cfg_sw.h"
#include "u_port_debug.h"

// For default values for U_CFG_APP_xxx
#include "u_cfg_app_platform_specific.h"

// For the cellular module types
#include "u_cell_module_type.h"

// For the network API
#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"

// For the security description
#include "u_security.h"

// For the MQTT client API
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// MQTT broker URL: there is no port number on the end of this URL,
// and hence, conventionally, it does not include TLS security.  You
// may make a secure TLS connection on test.mosquitto.org instead
// by editing this code to add TLS security (see below) and changing
// MY_BROKER_NAME to have ":8883" on the end.
#define MY_BROKER_NAME "test.mosquitto.org"

#ifndef U_CFG_ENABLE_LOGGING
# define uPortLog(format, ...)  print(format, ##__VA_ARGS__)
#endif

// For u-blox internal testing only
#ifdef U_PORT_TEST_ASSERT
# define EXAMPLE_FINAL_STATE(x) U_PORT_TEST_ASSERT(x);
#else
# define EXAMPLE_FINAL_STATE(x)
#endif

#ifndef U_PORT_TEST_FUNCTION
# error if you are not using the unit test framework to run this code you must ensure that the platform clocks/RTOS are set up and either define U_PORT_TEST_FUNCTION yourself or replace it as necessary.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Cellular network configuration:
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
static const uNetworkConfigurationCell_t gConfigCell = {U_NETWORK_TYPE_CELL,
                                                        U_CFG_TEST_CELL_MODULE_TYPE,
                                                        NULL, /* SIM pin */
                                                        NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
                                                        240, /* Connection timeout in seconds */
                                                        U_CFG_APP_CELL_UART,
                                                        /* Note that the pin numbers
                                                           that follow are those of the MCU:
                                                           if you are using an MCU inside
                                                           a u-blox module the IO pin numbering
                                                           for the module is likely different
                                                           to that from the MCU: check the data
                                                           sheet for the module to determine
                                                           the mapping. */
                                                        U_CFG_APP_PIN_CELL_TXD,
                                                        U_CFG_APP_PIN_CELL_RXD,
                                                        U_CFG_APP_PIN_CELL_CTS,
                                                        U_CFG_APP_PIN_CELL_RTS,
                                                        U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                                        U_CFG_APP_PIN_CELL_PWR_ON,
                                                        U_CFG_APP_PIN_CELL_VINT
                                                       };
#else
static const uNetworkConfigurationCell_t gConfigCell = {U_NETWORK_TYPE_NONE};
#endif

// TODO: Wifi network configuration.
// static const uNetworkConfigurationWifi_t gConfigWifi = {U_NETWORK_TYPE_NONE};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback for unread message indications.
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    bool *pMessagesAvailable = (bool *) pParam;

    // It is important to keep stack usage in this callback
    // to a minimum.  If you want to do more than set a flag
    // (e.g. you want to call into another ubxlib API) then send
    // an event to one of your own tasks, where you have allocated
    // sufficient stack, and do those things there.
    uPortLog("The broker says there are %d message(s) unread.\n", numUnread);
    *pMessagesAvailable = true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleMqttClient")
{
    int32_t networkHandle;
    uMqttClientContext_t *pContext = NULL;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char topic[32];
    const char message[] = "The quick brown fox jumps over the lazy dog";
    char buffer[64];
    size_t bufferSize;
    volatile bool messagesAvailable = false;
    int64_t startTimeMs;

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance, in this case of type cell
    // since that's what we have configuration information
    // for above.
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_CELL,
                                (void *) &gConfigCell);
    uPortLog("Added network with handle %d.\n", networkHandle);

    // Bring up the network layer
    uPortLog("Bringing up the network...\n");
    if (uNetworkUp(networkHandle) == 0) {

        // Do things using the network, for
        // example connect to an MQTT broker
        // and publish/subscribe to topics
        // as follows

        // Create an MQTT instance.  Here we
        // are using a non-secure MQTT connection
        // and hence the TLS parameter is NULL.
        // If you have edited MY_BROKER_NAME above
        // to connect on the ":8883" secure port
        // then you must change the TLS parameter
        // to be &tlsSettings, which will apply the
        // default TLS security settings. You may
        // change the TLS security settings
        // structure to, for instance, add certificate
        // checking: see the sockets TLS example for
        // how to do that.
        pContext = pUMqttClientOpen(networkHandle, NULL);
        if (pContext != NULL) {
            // Set the URL for the connection; everything
            // else can be left at defaults for the
            // public test.mosquitto.org broker
            connection.pBrokerNameStr = MY_BROKER_NAME;

            // If you wish to use the Thingstream
            // MQTT Now service, you would set the
            // following values in the uMqttClientConnection_t
            // structure instead:
            //
            // pBrokerNameStr to "mqtt.thingstream.io"
            // pClientIdStr to the Thingstream Client ID of your thing, something like "device:521b5a33-2374-4547-8edc-50743c144509"
            // pUserNameStr to the Thingstream username of your thing, something like "WF592TTWUQ18512KLU6L"
            // pPasswordStr to the Thingstream password of your thing, something like "nsd8hsK/NSDFdgdblfmbQVXbx7jeZ/8vnsiltgty"

            // Connect to the MQTT broker
            uPortLog("Connecting to MQTT broker \"%s\"...\n", MY_BROKER_NAME);
            if (uMqttClientConnect(pContext, &connection) == 0) {

                // Set up a callback to be called when the broker
                // says there are new messages available
                uMqttClientSetMessageCallback(pContext,
                                              messageIndicationCallback,
                                              (void *) &messagesAvailable);

                // In order to create a unique topic name on the
                // public server that we can publish and subscribe
                // to in this example code, we make the topic name
                // the serial number of the module
                uSecurityGetSerialNumber(networkHandle, topic);

                // Subscribe to our topic on the broker
                uPortLog("Subscribing to topic \"%s\"...\n", topic);
                if (uMqttClientSubscribe(pContext, topic,
                                         U_MQTT_QOS_EXACTLY_ONCE)) {

                    // Publish our message to our topic on the
                    // MQTT broker
                    uPortLog("Publishing \"%s\" to topic \"%s\"...\n",
                             message, topic);
                    startTimeMs = uPortGetTickTimeMs();
                    if (uMqttClientPublish(pContext, topic, message,
                                           sizeof(message) - 1,
                                           U_MQTT_QOS_EXACTLY_ONCE,
                                           false) == 0) {

                        // Wait for us to be notified that our new
                        // message is available on the broker
                        while (!messagesAvailable &&
                               (uPortGetTickTimeMs() < startTimeMs + 10000)) {
                            uPortTaskBlock(1000);
                        }

                        // Read the new message from the broker
                        while (uMqttClientGetUnread(pContext) > 0) {
                            bufferSize = sizeof(buffer);
                            if (uMqttClientMessageRead(pContext, topic,
                                                       sizeof(topic),
                                                       buffer, &bufferSize,
                                                       NULL) == 0) {
                                uPortLog("New message in topic \"%s\" is %d"
                                         " character(s): \"%.*s\".\n", topic,
                                         bufferSize, bufferSize, buffer);
                            }
                        }
                    } else {
                        uPortLog("Unable to publish our message \"%s\"!\n",
                                 message);
                    }
                } else {
                    uPortLog("Unable to subscribe to topic \"%s\"!\n", topic);
                }

                // Disconnect from the MQTT broker
                uMqttClientDisconnect(pContext);

            } else {
                uPortLog("Unable to connect to MQTT broker \"%s\"!\n", MY_BROKER_NAME);
            }
        } else {
            uPortLog("Unable to create MQTT instance!\n");
        }

        // Note: since networkHandle is a cellular
        // handle any of the `cell` API calls
        // could be made here using it.
        // If the configuration used were Wifi
        // then the `wifi` API calls could be
        // used

        // Shut down MQTT
        uMqttClientClose(pContext);

        // When finished with the network layer
        uPortLog("Taking down network...\n");
        uNetworkDown(networkHandle);
    } else {
        uPortLog("Unable to bring up the network!\n");
    }

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    uPortLog("Done.\n");

    // Stop the compiler warning about tlsSettings being unused
    (void) tlsSettings;

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((pContext == NULL) || messagesAvailable);
#endif
}

// End of file
