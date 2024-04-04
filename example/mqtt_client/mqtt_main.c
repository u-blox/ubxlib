/*
 * Copyright 2019-2024 u-blox
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

/** @brief This example demonstrates how to use the common MQTT API
 * to talk to an MQTT broker on the public internet using a u-blox
 * module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

// For U_SHORT_RANGE_TEST_WIFI()
#include "u_short_range_test_selector.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
# include "u_wifi_test_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// MQTT broker URL: there is no port number on the end of this URL,
// and hence, conventionally, it does not include TLS security.  You
// may make a secure [D]TLS connection on broker.emqx.io instead
// by editing this code to add [D]TLS security (see below) and
// changing MY_BROKER_NAME to have ":8883" on the end.
#define MY_BROKER_NAME "ubxlib.com"

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

// ZEPHYR USERS may prefer to set the device and network
// configuration from their device tree, rather than in this C
// code: see /port/platform/zephyr/README.md for instructions on
// how to do that.

// Below is the module configuration
// When U_CFG_TEST_CELL_MODULE_TYPE is set this example will setup a cellular
// link using uNetworkConfigurationCell_t.
// When U_CFG_TEST_SHORT_RANGE_MODULE_TYPE is set this example will instead use
// uNetworkConfigurationWifi_t config to setup a Wifi connection.

#if U_SHORT_RANGE_TEST_WIFI()

// Set U_CFG_TEST_SHORT_RANGE_MODULE_TYPE to your module type,
// chosen from the values in common/short_range/api/u_short_range_module_type.h

// DEVICE i.e. module/chip configuration: in this case a short-range
// module connected via UART
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,  // Use -1 if on Zephyr
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS,  // Use -1 if on Zephyr
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        },
    },
};
// NETWORK configuration for Wi-Fi
static const uNetworkCfgWifi_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_WIFI,
    .pSsid = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID), /* Wifi SSID - replace with your SSID */
    .authentication = U_WIFI_TEST_CFG_AUTHENTICATION, /* Authentication mode (see uWifiAuth_t in wifi/api/u_wifi.h) */
    .pPassPhrase = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE) /* WPA2 passphrase */
};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_WIFI;

#elif defined(U_CFG_TEST_CELL_MODULE_TYPE)

// Cellular configuration.
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different to that of the MCU: check
// the data sheet for the module to determine the mapping.

// DEVICE i.e. module/chip configuration: in this case a cellular
// module connected via UART
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = NULL, /* SIM pin */
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT,
            .pinDtrPowerSaving = U_CFG_APP_PIN_CELL_DTR
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_CELL_UART,
            .baudRate = U_CELL_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_CELL_TXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinRxd = U_CFG_APP_PIN_CELL_RXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinCts = U_CFG_APP_PIN_CELL_CTS,  // Use -1 if on Zephyr
            .pinRts = U_CFG_APP_PIN_CELL_RTS,  // Use -1 if on Zephyr
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        },
    },
};
// NETWORK configuration for cellular
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
    .timeoutSeconds = 240 /* Connection timeout in seconds */
    // There are five additional fields here which we do NOT set,
    // we allow the compiler to set them to 0 and all will be fine.
    // The fields are:
    //
    // - "pKeepGoingCallback": you may set this field to a function
    //   of the form "bool keepGoingCallback(uDeviceHandle_t devHandle)",
    //   e.g.:
    //
    //   .pKeepGoingCallback = keepGoingCallback;
    //
    //   ...and your function will be called periodically during an
    //   abortable network operation such as connect/disconnect;
    //   if it returns true the operation will continue else it
    //   will be aborted, allowing you immediate control.  If this
    //   field is set, timeoutSeconds will be ignored.
    //
    // - "pUsername" and "pPassword": if you are required to set a
    //   user name and password to go with the APN value that you
    //   were given by your service provider, set them here.
    //
    // - "authenticationMode": if you MUST give a user name and
    //   password and your cellular module does NOT support figuring
    //   out the authentication mode automatically (e.g. SARA-R4xx,
    //   LARA-R6 and LENA-R8 do not) then you must populate this field
    //   with the authentication mode that should be used, see
    //   #uCellNetAuthenticationMode_t in u_cell_net.h; there is no
    //   harm in populating this field even if the module _does_ support
    //   figuring out the authentication mode automatically but
    //   you ONLY NEED TO WORRY ABOUT IT if you were given that user
    //   name and password with the APN (which is thankfully not usual).
    //
    // - "pMccMnc": ONLY required if you wish to connect to a specific
    //   MCC/MNC rather than to the best available network; should point
    //   to the null-terminated string giving the MCC and MNC of the PLMN
    //   to use (for example "23410").
};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_CELL;
#else
// No module available - set some dummy values to make test system happy
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_CELL;
#endif

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
    uDeviceHandle_t devHandle = NULL;
    uMqttClientContext_t *pContext = NULL;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char topic[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    const char message[] = "The quick brown fox jumps over the lazy dog";
    char buffer[64];
    size_t bufferSize;
    volatile bool messagesAvailable = false;
    uTimeoutStart_t timeoutStart;
    int32_t returnCode;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    if (returnCode == 0) {
        // Bring up the network interface
        uPortLog("Bringing up the network...\n");
        if (uNetworkInterfaceUp(devHandle, gNetType,
                                &gNetworkCfg) == 0) {

            // Do things using the network, for
            // example connect to an MQTT broker
            // and publish/subscribe to topics
            // as follows

            // Create an MQTT instance.  Here we
            // are using a non-secure MQTT connection
            // and hence the [D]TLS parameter is NULL.
            // If you have edited MY_BROKER_NAME above
            // to connect on the ":8883" secure port
            // then you must change the [D]TLS parameter
            // to be &tlsSettings, which will apply the
            // default [D]TLS security settings. You may
            // change the [D]TLS security settings
            // structure to, for instance, add certificate
            // checking: see the sockets TLS example for
            // how to do that.
            pContext = pUMqttClientOpen(devHandle, NULL);
            if (pContext != NULL) {
                // Set the URL for the connection; everything
                // else can be left at defaults for the
                // public ubxlib.com broker
                connection.pBrokerNameStr = MY_BROKER_NAME;

                // If you wish to use MQTT-SN instead of MQTT,
                // and your broker supports it, you would set:
                // connection.mqttSn = true;

                // If you wish to use the Thingstream
                // MQTT service, you would set the following
                // values in the uMqttClientConnection_t
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
                    uSecurityGetSerialNumber(devHandle, topic);

                    // Subscribe to our topic on the broker
                    uPortLog("Subscribing to topic \"%s\"...\n", topic);
                    // If you were using MQTT-SN, you would call
                    // uMqttClientSnSubscribeNormalTopic() instead and
                    // capture the returned MQTT-SN topic name for use with
                    // uMqttClientSnPublish() a few lines below
                    // Note: >= in this case since the function
                    // returns the QOS of the subscription, which
                    // can be 0, 1 or 2
                    if (uMqttClientSubscribe(pContext, topic,
                                             U_MQTT_QOS_EXACTLY_ONCE) >= 0) {

                        // Publish our message to our topic on the
                        // MQTT broker
                        uPortLog("Publishing \"%s\" to topic \"%s\"...\n",
                                 message, topic);
                        timeoutStart = uTimeoutStart();
                        // If you were using MQTT-SN, you would call
                        // uMqttClientSnPublish() instead and pass it
                        // the MQTT-SN topic name returned by
                        // uMqttClientSnSubscribeNormalTopic()
                        if (uMqttClientPublish(pContext, topic, message,
                                               sizeof(message) - 1,
                                               U_MQTT_QOS_EXACTLY_ONCE,
                                               false) == 0) {

                            // Wait for us to be notified that our new
                            // message is available on the broker
                            while (!messagesAvailable &&
                                   !uTimeoutExpiredSeconds(timeoutStart, 10)) {
                                uPortTaskBlock(1000);
                            }

                            // Read the new message from the broker
                            while (uMqttClientGetUnread(pContext) > 0) {
                                bufferSize = sizeof(buffer);
                                // If you were using MQTT-SN, you would call
                                // uMqttClientSnMessageRead() instead and, rather
                                // than passing it the buffer "topic", you
                                // would pass it a pointer to a variable of
                                // type uMqttSnTopicName_t
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

            // Note: since devHandle is a cellular
            // or wifi handle, any of the `cell` or `wifi`
            // API calls could be made here using it.

            // Shut down MQTT
            uMqttClientClose(pContext);

            // When finished with the network layer
            uPortLog("Taking down network...\n");
            uNetworkInterfaceDown(devHandle, gNetType);
        } else {
            uPortLog("Unable to bring up the network!\n");
        }

        // Close the device
        // Note: we don't power the device down here in order
        // to speed up testing; you may prefer to power it off
        // by setting the second parameter to true.
        uDeviceClose(devHandle, false);

    } else {
        uPortLog("Unable to bring up the device!\n");
    }

    // Tidy up
    uDeviceDeinit();
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
