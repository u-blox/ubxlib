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
 * mqtt communication using ubxlib.
 *
*/

#include <string.h>
#include <stdio.h>

#include "ubxlib.h"

#define BROKER_NAME "ubxlib.com"

// Change the line below based on which type of module you want to use.
// Then change all -1 values below to appropriate pin and settings values
// appropriate for your module connection.
#if 1
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = "tsiot",       // Thingstream SIM, use NULL for default
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
#else
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = -1
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
static const uNetworkCfgWifi_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_WIFI,
    .pSsid = "SSID",      // Wifi SSID - replace with your SSID
    .authentication = 2,  // WPA/WPA2/WPA3
    .pPassPhrase = "???"  // WPA passphrase - replace with yours
};
static const uNetworkType_t gNetworkType = U_NETWORK_TYPE_WIFI;
#endif

// Callback for unread message indications.
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    bool *pMessagesAvailable = (bool *)pParam;
    *pMessagesAvailable = true;
}

void main()
{
    // Remove the line below if you want the log printouts from ubxlib
    uPortLogOff();
    // Initiate ubxlib
    uPortInit();
    uDeviceInit();
    // And the U-blox module
    int32_t errorCode;
    uDeviceHandle_t deviceHandle;
    printf("\nInitiating the module...\n");
    errorCode = uDeviceOpen(&gDeviceCfg, &deviceHandle);
    if (errorCode == 0) {
        printf("Bringing up the network...\n");
        errorCode = uNetworkInterfaceUp(deviceHandle, gNetworkType, &gNetworkCfg);
        if (errorCode == 0) {
            uMqttClientContext_t *pContext = pUMqttClientOpen(deviceHandle, NULL);
            if (pContext != NULL) {
                uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
                volatile bool messagesAvailable = false;
                char topic[32];

                connection.pBrokerNameStr = BROKER_NAME;
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
                    if (uMqttClientSubscribe(pContext, topic,
                                             U_MQTT_QOS_EXACTLY_ONCE)) {
                        printf("----------------------------------------------\n");
                        printf("To view the mqtt messages from this device use:\n");
                        printf("mosquitto_sub -h %s -t %s -v\n", BROKER_NAME, topic);
                        printf("To send mqtt messages to this device use:\n");
                        printf("mosquitto_pub -h %s -t %s -m message\n", BROKER_NAME, topic);
                        printf("Send message \"exit\" to disconnect\n");
                        bool done = false;
                        int i = 0;
                        while (!done) {
                            char buffer[25];
                            if (messagesAvailable) {
                                char buffer[25];
                                while (uMqttClientGetUnread(pContext) > 0) {
                                    size_t cnt = sizeof(buffer) - 1;
                                    if (uMqttClientMessageRead(pContext, topic,
                                                               sizeof(topic),
                                                               buffer, &cnt,
                                                               NULL) == 0) {
                                        buffer[cnt] = 0;
                                        printf("Received message: %s\n", buffer);
                                        done = strstr(buffer, "exit");
                                    }
                                }
                                messagesAvailable = false;
                            } else {
                                snprintf(buffer, sizeof(buffer), "Hello #%d", ++i);
                                uMqttClientPublish(pContext, topic, buffer,
                                                   strlen(buffer),
                                                   U_MQTT_QOS_EXACTLY_ONCE,
                                                   false);
                            }
                            uPortTaskBlock(1000);
                        }
                    } else {
                        printf("* Failed to subscribe to topic: %s\n", topic);
                    }
                    uMqttClientDisconnect(pContext);
                } else {
                    printf("* Failed to connect to the mqtt broker\n");
                }

            } else {
                printf("* Failed to create mqtt instance !\n ");
            }

            printf("Closing down the network...\n");
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
