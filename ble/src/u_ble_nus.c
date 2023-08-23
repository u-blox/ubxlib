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
 * @brief Implementation of the Nordic Uart Service (NUS) client and server.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
#include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stdbool.h"
#include "stddef.h"  // NULL, size_t etc.
#include "stdint.h"  // int32_t etc.
#include "stdio.h"   // snprintf()
#include "stdlib.h"  // strol(), atoi(), strol(), strtof()
#include "string.h"  // memset(), strncpy(), strtok_r(), strtol()
#include "u_error_common.h"
#include "u_at_client.h"
#include "u_ble.h"
#include "u_ble_cfg.h"
#include "u_ble_extmod_private.h"
#include "u_ble_gap.h"
#include "u_ble_gatt.h"
#include "u_ble_private.h"
#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_short_range.h"
#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range_private.h"
#include "u_network.h"
#include "u_network_shared.h"
#include "u_network_config_ble.h"
#include "u_hex_bin_convert.h"

#include "u_ble_nus.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define NUS_SERVICE_UUID "6E400001B5A3F393E0A9E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002B5A3F393E0A9E50E24DCCA9E"
#define NUS_TX_CHAR_UUID "6E400003B5A3F393E0A9E50E24DCCA9E"

#define VALIDATE(f) if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) { errorCode = f; }

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static bool gIsServer;
static uDeviceHandle_t gDeviceHandle;
static volatile int32_t gConnectState;

static int32_t gConnHandle;
static uint16_t gRxHandle, gTxHandle;
static uBleNusReceiveCallback_t gReceiveCallback;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void connectCallback(int32_t connHandle, char *pAddress, bool connected)
{
    (void)pAddress;
    if (connected) {
        gConnHandle = connHandle;
        gConnectState = 1;
    } else {
        gConnectState = -1;
    }
}

static void discoverCharacteristcs(uint8_t connHandle, uint16_t attrHandle,
                                   uint8_t properties, uint16_t valueHandle,
                                   char *pUuid)
{
    (void)connHandle;
    (void)attrHandle;
    (void)properties;
    if (strncmp(pUuid, NUS_RX_CHAR_UUID, strlen(NUS_RX_CHAR_UUID)) == 0) {
        gRxHandle = valueHandle;
    }
    if (strncmp(pUuid, NUS_TX_CHAR_UUID, strlen(NUS_TX_CHAR_UUID)) == 0) {
        gTxHandle = valueHandle;
    }
}

static void receiveCallback(uint8_t connHandle,
                            uint16_t valueHandle,
                            uint8_t *pValue,
                            uint8_t valueSize)
{
    (void)connHandle;
    if ((valueHandle == gRxHandle && gIsServer) ||
        (valueHandle == gTxHandle && !gIsServer)) {
        gReceiveCallback(pValue, valueSize);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleNusInit(uDeviceHandle_t devHandle,
                    const char *pAddress,
                    uBleNusReceiveCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    gDeviceHandle = devHandle;
    uBleGapSetConnectCallback(gDeviceHandle, connectCallback);
    gReceiveCallback = cb;
    gIsServer = pAddress == NULL;
    if (gIsServer) {
        // Define the NUS service and characteristics
        errorCode = uBleGattAddService(gDeviceHandle, NUS_SERVICE_UUID);
        if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
            errorCode = uBleGattAddCharacteristic(gDeviceHandle, NUS_RX_CHAR_UUID, 0x0C, &gRxHandle);
        }
        if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
            errorCode = uBleGattAddCharacteristic(gDeviceHandle, NUS_TX_CHAR_UUID, 0x10, &gTxHandle);
        }
        if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
            // Detect client writes
            errorCode = uBleGattSetWriteCallback(gDeviceHandle, receiveCallback);
        }
    } else {
        gRxHandle = 0;
        gTxHandle = 0;
        // Connect and discover the characteristics handles
        errorCode = uBleGapConnect(gDeviceHandle, pAddress);
        if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
            gConnectState = 0;
            // Wait for connection, uConnect handles timeout by sending disconnect event
            while (gConnectState == 0) {
                uPortTaskBlock(500);
            }
            if (gConnectState == 1) {
                uBleGattDiscoverChar(gDeviceHandle, gConnHandle, discoverCharacteristcs);
                if (gRxHandle != 0 && gTxHandle != 0) {
                    // Detect server writes
                    errorCode = uBleGattEnableNotification(gDeviceHandle, gConnHandle, gTxHandle);
                    if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                        errorCode = uBleGattSetNotificationCallback(gDeviceHandle, receiveCallback);
                    }
                    if (errorCode != (int32_t)U_ERROR_COMMON_SUCCESS) {
                        uBleGapDisconnect(gDeviceHandle, gConnHandle);
                    }
                } else {
                    errorCode = (int32_t)U_BLE_ERROR_NOT_FOUND;
                }
            } else {
                errorCode = (int32_t)U_BLE_ERROR_NOT_FOUND;
            }
        }
    }
    return errorCode;
}

int32_t uBleNusDeInit()
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    if (gConnectState == 1) {
        errorCode = uBleGapDisconnect(gDeviceHandle, gConnHandle);
    }
    return errorCode;
}

int32_t uBleNusWrite(const void *pValue, uint8_t valueLength)
{
    if (gIsServer) {
        return uBleGattWriteNotifyValue(gDeviceHandle, gConnHandle, gTxHandle, pValue, valueLength);
    } else {
        return uBleGattWriteValue(gDeviceHandle, gConnHandle, gRxHandle, pValue, valueLength, true);
    }
}

int32_t uBleNusSetAdvData(uint8_t *pAdvData, uint8_t advDataSize)
{
    uint8_t size = (uint8_t)(strlen(NUS_SERVICE_UUID) / 2);
    if (!pAdvData || !advDataSize || advDataSize < (size + 2)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    *(pAdvData++) = size + 1;
    *(pAdvData++) = U_BT_DATA_UUID128_ALL;
    uHexToBin(NUS_SERVICE_UUID, strlen(NUS_SERVICE_UUID), (char *)pAdvData);
    // Reverse order
    for (int32_t i = 0; i < size / 2; i++) {
        uint8_t temp = pAdvData[i];
        uint8_t pos = size - i - 1;
        pAdvData[i] = pAdvData[pos];
        pAdvData[pos] = temp;
    }
    return size + 2;
}

#endif