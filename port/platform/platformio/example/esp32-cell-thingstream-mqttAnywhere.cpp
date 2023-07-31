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

/*
 *
 * A simple demo application showing how to set up
 * a connection to the Thingstream MQTT-Anywhere service (MQTT-SN) using the ubxlib.
 * By: Jan-Ole Giebel
 *
*/
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <driver/uart.h> //For using UART_NUM_1, UART_NUM_2 and UART_NUM_*

#include "ubxlib.h"

#define BROKER_NAME "10.7.0.55:2442" // The Thingstream MQTT-Anywhere service (MQTT-SN gateway) IP address and port.

// Change all -1 values below to appropriate pin and settings values
// appropriate for your module connection.
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = "tsudp",       // Thingstream SIM: For the Thingstream MQTT-Anywhere service (MQTT-SN gateway) the APN must be set to "TSUDP". When using the Thingstream SIM for other 'internet' services, APN must be set to "TSIOT".
    .timeoutSeconds = 240  // Connection timeout in seconds
};

static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = -1,
            .pSimPinCode = NULL, /* SIM pin */
            .pinEnablePower = -1,
            .pinPwrOn = -1,
            .pinVInt = -1,
            .pinDtrPowerSaving = -1
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = -1,
            .baudRate = -1,
            .pinTxd = -1,
            .pinRxd = -1,
            .pinCts = -1,
            .pinRts = -1,
            .pPrefix = NULL // Relevant for Linux only
        },
    },
};

static const uNetworkType_t gNetworkType = U_NETWORK_TYPE_CELL;

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

void setup()
{
    //Start the serial communication, for the Debug-Output.
    Serial.begin(115200);
}

void loop()
{
    // Remove the line below if you want the log printouts from ubxlib
    uPortLogOff();
    // Initiate ubxlib
    uPortInit();
    uDeviceInit();
    // And the U-blox module
    int32_t errorCode;
    uDeviceHandle_t deviceHandle;
    uMqttSnTopicName_t topicName;
    bool done = false;
    int i = 0;
    uPortLog("\nInitiating the module...\n");
    errorCode = uDeviceOpen(&gDeviceCfg, &deviceHandle);
    uPortLog("Opened device with return code %d.\n", errorCode);
    if (errorCode == 0) {
        uPortLog("Bringing up the network...\n");
        errorCode = uNetworkInterfaceUp(deviceHandle, gNetworkType, &gNetworkCfg);
        if (errorCode == 0) {
            uMqttClientContext_t *pContext = pUMqttClientOpen(deviceHandle, NULL);
            if (pContext != NULL) {
                uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
                volatile bool messagesAvailable = false;
                char topic[32];

                connection.pBrokerNameStr = BROKER_NAME;
                connection.mqttSn = true;
                connection.pClientIdStr =
                    "your_device-identity"; //The device identity shown in the thingsteam dashboard.
                uPortLog("Connecting to MQTT broker \"%s\"...\n", BROKER_NAME);
                if (uMqttClientConnect(pContext, &connection) == 0) {
                    uMqttClientSetMessageCallback(pContext,
                                                  messageIndicationCallback,
                                                  (void *)&messagesAvailable);
                    // Get a unique topic name for this test
                    uSecurityGetSerialNumber(deviceHandle, topic);
                    if (topic[0] == '"') {
                        // Remove quotes
                        size_t len = strlen(topic);
                        memmove(topic, topic + 1, len);
                        topic[len - 2] = 0;
                    }
                    uPortLog("Subscribing to topic \"%s\"...\n", topic);
                    if (uMqttClientSnSubscribeNormalTopic(pContext, topic,
                                                          U_MQTT_QOS_EXACTLY_ONCE, &topicName)) {
                        uPortLog("----------------------------------------------\n");
                        uPortLog("To view the mqtt messages from this device use (Do not forget to add your thinstream MQTT-Client credentials!):\n");
                        uPortLog("mosquitto_sub -h %s -t %s -v\n", BROKER_NAME, topic);
                        uPortLog("To send mqtt messages to this device use:\n");
                        uPortLog("mosquitto_pub -h %s -t %s -m message\n", BROKER_NAME, topic);
                        uPortLog("Send message \"exit\" to disconnect\n");

                        uMqttClientSnRegisterNormalTopic(pContext, topic, &topicName);

                        while (!done) {
                            char buffer[50];
                            if (messagesAvailable) {
                                char buffer[50];

                                while (uMqttClientGetUnread(pContext) > 0) {
                                    size_t cnt = sizeof(buffer) - 1;
                                    if (uMqttClientSnMessageRead(pContext, &topicName,
                                                                 buffer, &cnt,
                                                                 NULL) == 0) {
                                        buffer[cnt] = 0;
                                        uPortLog("Received message: %s\n", buffer);
                                        done = strstr(buffer, "exit");
                                    }
                                }
                                messagesAvailable = false;
                            } else {
                                snprintf(buffer, sizeof(buffer), "Hello #%d", ++i);
                                uMqttClientSnPublish(pContext, &topicName, buffer,
                                                     strlen(buffer),
                                                     U_MQTT_QOS_EXACTLY_ONCE,
                                                     false);
                                uPortLog("Publishing \"%s\" to topic 0x%04x...\n", buffer, topicName.name.id);
                            }
                            uPortTaskBlock(5000);
                        }
                    } else {
                        uPortLog("* Failed to subscribe to topic: %s\n", topic);
                    }
                    uMqttClientDisconnect(pContext);
                } else {
                    uPortLog("* Failed to connect to the mqtt broker\n");
                }

            } else {
                uPortLog("* Failed to create mqtt instance !\n ");
            }

            uPortLog("Closing down the network...\n");
            uNetworkInterfaceDown(deviceHandle, gNetworkType);
        } else {
            printf("* Failed to bring up the network: %d\n", errorCode);
        }
        uDeviceClose(deviceHandle, true);
    } else {
        printf("* Failed to initiate the module: %d\n", errorCode);
    }

    printf("\n== All done ==\n");

    while (1) {
        uPortTaskBlock(1000);
    }
}
