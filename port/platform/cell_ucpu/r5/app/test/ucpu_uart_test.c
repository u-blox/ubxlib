/*
 * Copyright 2019-2022 u-blox Cambourne Ltd
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

/** @brief This basic example demonstrates how to bring up a network
 * connection and then perform mqtt and socket operations in their respectively threads.
 *
 * The purpose of this test app to verify the UART implementation.
 */

#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_port.h"
#include "u_cfg_sw.h"
#include "u_port_debug.h"
#include "u_port_os.h"

// For the cellular module types
#include "u_cell_module_type.h"

// For the network API
#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"

// For the sockets API
#include "u_sock.h"

// For the security description
#include "u_security.h"

// For the MQTT client API
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

// For default values for U_CFG_APP_xxx
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "ucpu_sdk_modem_uart.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Thread priority
#define THREAD_PRIORITY 10

// MQTT thread stack size
#define MQTT_THREAD_STACK_SIZE 8*1024

// Socket thread stack size
#define SOCKET_THREAD_STACK_SIZE 8*1024

// Socket echo server URL
#define TCP_SERVER_NAME "ubxlib.redirectme.net"

// Socket echo server port
#define TCP_SERVER_PORT 5055

// MQTT broker URL
#define MQTT_BROKER_NAME "a2ccb1d45r4m3z-ats.iot.us-east-2.amazonaws.com:8883"

// Client id
#define MQTT_CLIENT_ID "357862090073448"

// Message size for MQTT and Socket operations
#define MEG_SIZE 64

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Cellular network configuration:

static uDeviceCfg_t gDeviceCfgCell = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CELL_MODULE_TYPE_SARA_R5,
            .pSimPinCode = NULL
            .pinEnablePower = -1,
            .pinPwrOn = -1,
            .pinVInt = -1
        }
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = 0,
            .baudRate = U_CELL_UART_BAUD_RATE,
            .pinTxd = -1,
            .pinRxd = -1,
            .pinCts = -1,
            .pinRts = -1
        }
    }
};

static uNetworkCfgCell_t gDeviceNetworkCfgCell = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = NULL,
    .timeoutSeconds = 240
};


// MQTT thread handle
static uPortTaskHandle_t mqttThreadHandle;

// Socket thread handle
static uPortTaskHandle_t socketThreadHandle;

// Device handle
uDeviceHandle_t gDeviceHandle;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print out an address structure.
static void printAddress(const uSockAddress_t *pAddress,
                         bool hasPort)
{
    switch (pAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            uPortLog("IPV4");
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            uPortLog("IPV6");
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
            uPortLog("IPV4V6");
            break;
        default:
            uPortLog("unknown type (%d)", pAddress->ipAddress.type);
            break;
    }

    uPortLog(" ");

    if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%u",
                     (pAddress->ipAddress.address.ipv4 >> (x * 8)) & 0xFF);
            if (x > 0) {
                uPortLog(".");
            }
        }
        if (hasPort) {
            uPortLog(":%u", pAddress->port);
        }
    } else if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
        if (hasPort) {
            uPortLog("[");
        }
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%x:%x", pAddress->ipAddress.address.ipv6[x] >> 16,
                     pAddress->ipAddress.address.ipv6[x] & 0xFFFF);
            if (x > 0) {
                uPortLog(":");
            }
        }
        if (hasPort) {
            uPortLog("]:%u", pAddress->port);
        }
    }
}

// Callback for unread message indications.
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    bool *pMessagesAvailable = (bool *) pParam;

    // It is important to keep stack usage in this callback
    // to a minimum.  If you want to do more than set a flag
    // (e.g. you want to call into another ubxlib API) then send
    // an event to one of your own tasks, where you have allocated
    // sufficient stack, and do those things there
    uPortLog("The broker says there are %d message(s) unread.\n", numUnread);
    *pMessagesAvailable = true;
}

// MQTT operations thread. This thread create
// MQTT client instence and connect with MQTT broker.
// Subsribe to topic and send/receive data in
// infinit loop on subsribed topic.
static void mqttThread(void *thread_input)
{
    uMqttClientContext_t *pContext = NULL;
    uMqttClientConnection_t connection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char topic[32] = "/357862090073448/TEST";
    const char message[MEG_SIZE] = "This is mqtt test application, sending data packet no: ";
    char pubMessage[MEG_SIZE];
    char readBuffer[MEG_SIZE];
    size_t readBufferSize;
    int32_t startTimeMs;
    uint32_t count = 0;
    uint32_t result = 0;
    volatile bool messagesAvailable = false;
    bool isConnectedToServer = false;

    // Set the URL for the connection
    connection.pBrokerNameStr = MQTT_BROKER_NAME;
    connection.pClientIdStr = MQTT_CLIENT_ID;

    // Certificate settings
    tlsSettings.certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_NONE;
    tlsSettings.pExpectedServerUrl = connection.pBrokerNameStr;
    tlsSettings.pSni = connection.pBrokerNameStr;
    tlsSettings.useDeviceCertificate = true;
    tlsSettings.includeCaCertificates = true;

    // Create an MQTT instance
    uPortLog("Open mqtt client instance.\n");
    pContext = pUMqttClientOpen(networkHandle, &tlsSettings);
    if (pContext != NULL) {

        connection.inactivityTimeoutSeconds = 3000;

        uPortLog("Connecting to MQTT broker \"%s\"...\n", connection.pBrokerNameStr);
        if (uMqttClientConnect(pContext, &connection) == 0) {

            // Set up a callback to be called when the broker
            // says there are new messages available
            result = uMqttClientSetMessageCallback(pContext,
                                                   messageIndicationCallback,
                                                   (void *) &messagesAvailable);

            // Subscribe to our topic on the broker
            uPortLog("Subscribing to topic \"%s\"...\n", topic);
            if (uMqttClientSubscribe(pContext, topic,
                                     U_MQTT_QOS_AT_LEAST_ONCE)) {
                isConnectedToServer = true;
            } else {
                uPortLog("Failed to subscribe topic.\n");
            }
        } else {
            uPortLog("Failed to connect to MQTT broker.\n");
        }
    } else {
        uPortLog("Failed to open mqtt client instance.\n");
    }

    while (1) {
        if (isConnectedToServer) {
            uPortLog("MQTT itteration count = %d\n", ++count);

            startTimeMs = uPortGetTickTimeMs();
            memset(pubMessage, 0, sizeof(pubMessage));
            snprintf(pubMessage, sizeof(pubMessage), "%s%d", message, count);

            // Publish our message to our topic on the
            // MQTT broker
            uPortLog("Publishing \"%s\" to topic \"%s\"...\n",
                     pubMessage, topic);

            if (uMqttClientPublish(pContext, topic, pubMessage,
                                   strlen(pubMessage),
                                   U_MQTT_QOS_AT_MOST_ONCE,
                                   false) == 0) {

                // Wait for us to be notified that our new
                // message is available on the broker
                while (!messagesAvailable &&
                       (uPortGetTickTimeMs() - startTimeMs < 20000)) {
                    uPortTaskBlock(1000);
                }

                // Read the new message from the broker
                while (uMqttClientGetUnread(pContext) > 0) {
                    memset(readBuffer, 0, sizeof(readBuffer));
                    readBufferSize = sizeof(readBuffer);
                    if (uMqttClientMessageRead(pContext, topic,
                                               sizeof(topic),
                                               readBuffer, &readBufferSize,
                                               NULL) == 0) {

                        uPortLog("New MQTT message in topic \"%s\" is %d"
                                 " character(s): \"%.*s\".\n", topic,
                                 readBufferSize, readBufferSize, readBuffer);
                    }
                }
            } else {
                uPortLog("Unable to publish our message \"%s\"!.\n",
                         pubMessage);
            }
        }

        uPortTaskBlock(2000);
    }
}

// Socket operations thread. This thread creates
// a TCP socket and connects with TCP echo server.
// Send and receive data in infinit loop.
static void socketThread(void *thread_input)
{
    int32_t sock;
    int32_t x = 0;
    int32_t count = 0;
    uSockAddress_t address;
    const char message[MEG_SIZE] = "This is TCP socket echo test, sending data...";
    size_t txSize;
    char rxBuffer[MEG_SIZE];
    size_t rxSize = 0;
    bool isConnectedToServer = false;

    // Get the server's IP address using
    // the network's DNS resolution facility
    uSockGetHostByName(networkHandle, TCP_SERVER_NAME,
                       &address.ipAddress);
    uPortLog("TCP server IP address is: ");
    printAddress(&address, false);
    address.port = TCP_SERVER_PORT;
    uPortLog("\n");

    // Create the socket on the network
    uPortLog("Creating TCP socket...\n");
    sock = uSockCreate(networkHandle,
                       U_SOCK_TYPE_STREAM,
                       U_SOCK_PROTOCOL_TCP);

    // Make a TCP connection to the server using
    // the socket
    if (uSockConnect(sock, &address) == 0) {
        isConnectedToServer = true;
        uPortLog(" Connected with TCP server.\n");
    } else {
        uPortLog("Unable to connect to TCP server!\n");
    }

    while (1) {
        if (isConnectedToServer) {
            uPortLog("Socket itteration count = %d.\n", ++count);

            // Send the data over the socket
            // and print the echo that comes back

            txSize = strlen(message);
            x = 0;
            while ((x >= 0) && (txSize > 0)) {
                x = uSockWrite(sock, message, txSize);
                if (x > 0) {
                    txSize -= x;
                }
            }

            // Reset buffer
            memset(rxBuffer, 0, sizeof(rxBuffer));
            rxSize = 0;
            uPortLog("socket sent %d byte(s) to echo server.\n", x);
            while ((x >= 0) && (rxSize == 0) && (rxSize < sizeof(rxBuffer))) {
                x = uSockRead(sock, rxBuffer + rxSize, sizeof(rxBuffer) - rxSize);
                if (x > 0) {
                    rxSize += x;
                }
            }
            if (rxSize > 0) {
                uPortLog("Received socket echo back (%d byte(s)): %s\n", rxSize, rxBuffer);
            } else {
                uPortLog("No data received from TCP server!\n");
            }
        }

        uPortTaskBlock(2000);
    }
}

// Initialize the application, create threads etc.
static void StartThreads(void)
{
    int32_t result;

    // Create MQTT thread
    result = uPortTaskCreate(mqttThread,
                             "MQTT thread",
                             MQTT_THREAD_STACK_SIZE,
                             NULL,
                             THREAD_PRIORITY,
                             &mqttThreadHandle);
    uPortLog("Create mqtt thread, result %d.\n", result);

    // Create socket thread
    result = uPortTaskCreate(socketThread,
                             "Socket thread",
                             SOCKET_THREAD_STACK_SIZE,
                             NULL,
                             THREAD_PRIORITY,
                             &socketThreadHandle);
    uPortLog("Create socket thread, result %d.\n", result);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[exampleUart]", "uartTestExample")
{
    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Add a device with network, in this case of type cell
    // since that's what we have configuration information
    // for above
    uDeviceOpen(gDeviceCfgCell, &gDeviceHandle);

    // Bring up the network
    uPortLog("Bringing up the network...\n");
    if ((gDeviceHandle > 0) &&
        (uNetworkInterfaceUp(gDeviceHandle, U_NETWORK_TYPE_CELL, gDeviceNetworkCfgCell) == 0)) {
        // Start threads
        StartThreads();
    } else {
        uPortLog("Unable to bring up the network!\n");
        // Calling these will also deallocate the network handle
        uDeviceClose(gDeviceHandle, false);
        uDeviceDeinit();
        uPortDeinit();
    }

    while (1) {
        uPortTaskBlock(1000);
    }
}

// End of file
