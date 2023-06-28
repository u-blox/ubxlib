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
 * Simple BLE SPS example using ubxlib
 *
 * This example implements a SPS echo server to which a client can
 * connect and send data and then get that data echoed back.
 * A typical client can be the "U-blox Bluetooth Low Energy"
 * application available for Android and IOS.
 *
 */

#include <stdio.h>

#include "ubxlib.h"

// Change all -1 values below to appropriate pin and settings values
// appropriate for your module connection.
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

static const uNetworkCfgBle_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_BLE,
    .role = U_BLE_CFG_ROLE_PERIPHERAL,
    .spsServer = true
};


static void connectionCallback(int32_t connHandle, char *address, int32_t status,
                               int32_t channel, int32_t mtu, void *pParameters)
{
    if (status == (int32_t)U_BLE_SPS_CONNECTED) {
        printf("Connected to: %s\n", address);
    } else if (status == (int32_t)U_BLE_SPS_DISCONNECTED) {
        if (connHandle != U_BLE_SPS_INVALID_HANDLE) {
            printf("Diconnected\n");
        } else {
            printf("* Connection attempt failed\n");
        }
    }
}

static void dataAvailableCallback(int32_t channel, void *pParameters)
{
    char buffer[100];
    int32_t length;
    uDeviceHandle_t *pDeviceHandle = (uDeviceHandle_t *)pParameters;
    do {
        length = uBleSpsReceive(*pDeviceHandle, channel, buffer, sizeof(buffer) - 1);
        if (length > 0) {
            buffer[length] = 0;
            printf("Received: %s\n", buffer);
            // Echo the received data
            uBleSpsSend(*pDeviceHandle, channel, buffer, length);
        }
    } while (length > 0);
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
        printf("Bringing up the ble network...\n");
        errorCode = uNetworkInterfaceUp(deviceHandle, gNetworkCfg.type, &gNetworkCfg);
        if (errorCode == 0) {
            uBleSpsSetCallbackConnectionStatus(deviceHandle,
                                               connectionCallback,
                                               &deviceHandle);
            uBleSpsSetDataAvailableCallback(deviceHandle,
                                            dataAvailableCallback,
                                            &deviceHandle);
            printf("\n== Start a SPS client e.g. in a phone ==\n\n");
            printf("Waiting for connections...\n");
            while (1) {
                uPortTaskBlock(1000);
            }
        } else {
            printf("* Failed to bring up the network: %d\n", errorCode);
        }
        uDeviceClose(deviceHandle, true);
    } else {
        printf("* Failed to initiate the module: %d\n", errorCode);
    }

    while (1) {
        uPortTaskBlock(1000);
    }
}
