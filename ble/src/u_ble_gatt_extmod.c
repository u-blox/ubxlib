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
 * @brief Implementation of the GAP API for BLE.
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

#include "u_ble_gatt.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void notifyUrc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uBleGattNotificationCallback_t cb = (uBleGattNotificationCallback_t)pParameter;
    uint8_t connHandle = uAtClientReadInt(atHandle);
    uint16_t valueHandle = uAtClientReadInt(atHandle);
    uint8_t value[25];
    uint16_t valueSize = uAtClientReadHexData(atHandle, value, sizeof(value));
    if (valueSize > 0) {
        cb(connHandle, valueHandle, value, (uint8_t)valueSize);
    }
}

static void writeUrc(uAtClientHandle_t atHandle,
                     void *pParameter)
{
    uBleGattWriteCallback_t cb = (uBleGattWriteCallback_t)pParameter;
    uint8_t connHandle = uAtClientReadInt(atHandle);
    uint16_t valueHandle = uAtClientReadInt(atHandle);
    uint8_t value[25];
    uint16_t valueSize = uAtClientReadHexData(atHandle, value, sizeof(value));
    if (valueSize > 0) {
        cb(connHandle, valueHandle, value, (uint8_t)valueSize);
    }
}

static int32_t writeValue(uDeviceHandle_t devHandle, const char *pAtCom,
                          int32_t connHandle, uint16_t valueHandle,
                          const void *pValue, uint8_t valueLength)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, pAtCom);
        uAtClientWriteInt(atHandle, connHandle);
        uAtClientWriteInt(atHandle, valueHandle);
        uAtClientWriteHexData(atHandle, pValue, valueLength);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientErrorGet(atHandle);
        uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleGattDiscoverServices(uDeviceHandle_t devHandle,
                                 int32_t connHandle,
                                 uBleGattDiscoverServiceCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTGDP=");
        uAtClientWriteInt(atHandle, connHandle);
        uAtClientCommandStop(atHandle);
        bool ok = true;
        while (ok && uAtClientResponseStart(atHandle, "+UBTGDP:") == 0) {
            errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            int32_t respConnHandle = uAtClientReadInt(atHandle);
            if (respConnHandle == connHandle) {
                uint16_t startHandle = uAtClientReadInt(atHandle);
                uint16_t endHandle = uAtClientReadInt(atHandle);
                ok = uAtClientErrorGet(atHandle) == 0;
                char uuid[33];
                ok = ok && uAtClientReadString(atHandle,
                                               uuid,
                                               sizeof(uuid),
                                               false) >= 0;
                if (ok && cb) {
                    cb(connHandle, startHandle, endHandle, uuid);
                }
            }
            if (!ok) {
                errorCode = U_BLE_ERROR_TEMPORARY_FAILURE;
            }
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);

        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattDiscoverChar(uDeviceHandle_t devHandle,
                             int32_t connHandle,
                             uBleGattDiscoverCharCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTGDCS=");
        uAtClientWriteInt(atHandle, connHandle);
        uAtClientWriteInt(atHandle, 1);
        uAtClientWriteInt(atHandle, 65535);
        uAtClientCommandStop(atHandle);
        bool ok = true;
        while (ok && uAtClientResponseStart(atHandle, "+UBTGDCS:") == 0) {
            errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            int32_t respConnHandle = uAtClientReadInt(atHandle);
            if (respConnHandle == connHandle) {
                uint16_t attrHandle = uAtClientReadInt(atHandle);
                uint8_t prop;
                ok = uAtClientReadHexData(atHandle, &prop, 1) > 0;
                uint16_t valueHandle = uAtClientReadInt(atHandle);
                ok = ok && uAtClientErrorGet(atHandle) == 0;
                char uuid[33];
                ok = ok && uAtClientReadString(atHandle,
                                               uuid,
                                               sizeof(uuid),
                                               false) >= 0;
                if (ok && cb) {
                    cb(connHandle, attrHandle, prop, valueHandle, uuid);
                }
            }
            if (!ok) {
                errorCode = U_BLE_ERROR_TEMPORARY_FAILURE;
            }
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattEnableNotification(uDeviceHandle_t devHandle,
                                   int32_t connHandle, uint16_t valueHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTGWC=");
        uAtClientWriteInt(atHandle, connHandle);
        // Assume that the notification handle is +1
        uAtClientWriteInt(atHandle, valueHandle + 1);
        uAtClientWriteInt(atHandle, 1);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientErrorGet(atHandle);
        uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattSetNotificationCallback(uDeviceHandle_t devHandle,
                                        uBleGattNotificationCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        const char *pMatch = "+UUBTGN:";
        // Remove possible existing callback
        uAtClientRemoveUrcHandler(atHandle, pMatch);
        if (cb) {
            errorCode = uAtClientSetUrcHandler(atHandle, pMatch,
                                               notifyUrc, cb);
        }
        uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattSetWriteCallback(uDeviceHandle_t devHandle,
                                 uBleGattWriteCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER)) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                const char *pMatch = "+UUBTGRW:";
                // Remove possible existing callback
                uAtClientRemoveUrcHandler(atHandle, pMatch);
                if (cb) {
                    errorCode = uAtClientSetUrcHandler(atHandle, pMatch,
                                                       writeUrc, cb);
                }
                uAtClientUnlock(atHandle);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattReadValue(uDeviceHandle_t devHandle,
                          int32_t connHandle, uint16_t valueHandle,
                          uint8_t *pValue, uint8_t valueLength)
{
    int32_t errorOrSize = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTGR=");
        uAtClientWriteInt(atHandle, connHandle);
        uAtClientWriteInt(atHandle, valueHandle);
        uAtClientCommandStop(atHandle);
        errorOrSize = uAtClientResponseStart(atHandle, "+UBTGR:");
        if (errorOrSize == (int32_t)U_ERROR_COMMON_SUCCESS) {
            uAtClientReadInt(atHandle);
            uAtClientReadInt(atHandle);
            errorOrSize = uAtClientReadHexData(atHandle, pValue, valueLength);
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorOrSize;
}

int32_t uBleGattWriteValue(uDeviceHandle_t devHandle,
                           int32_t connHandle, uint16_t valueHandle,
                           const void *pValue, uint8_t valueLength,
                           bool waitResponse)
{
    const char *pAtCom = waitResponse ? "AT+UBTGW=" : "AT+UBTGWN=";
    return writeValue(devHandle, pAtCom, connHandle, valueHandle, pValue, valueLength);
}

int32_t uBleGattWriteNotifyValue(uDeviceHandle_t devHandle,
                                 int32_t connHandle, uint16_t valueHandle,
                                 const void *pValue, uint8_t valueLength)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER)) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTGSN=");
                uAtClientWriteInt(atHandle, connHandle);
                uAtClientWriteInt(atHandle, valueHandle);
                uAtClientWriteHexData(atHandle, pValue, valueLength);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientErrorGet(atHandle);
                uAtClientUnlock(atHandle);
            }
        }
        uShortRangeUnlock();
    }

    return errorCode;
}

int32_t uBleGattAddService(uDeviceHandle_t devHandle,
                           const char *pUuid)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER)) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTGSER=");
                uAtClientWriteString(atHandle, pUuid, false);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientErrorGet(atHandle);
                uAtClientUnlock(atHandle);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattAddCharacteristic(uDeviceHandle_t devHandle,
                                  const char *pUuid, uint8_t properties,
                                  uint16_t *pValueHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_SHORT_RANGE_PRIVATE_HAS(pInstance->pModule,
                                          U_SHORT_RANGE_PRIVATE_FEATURE_GATT_SERVER)) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTGCHA=");
                uAtClientWriteString(atHandle, pUuid, false);
                uAtClientWriteHexData(atHandle, &properties, 1);
                uAtClientWriteInt(atHandle, 1);
                uAtClientWriteInt(atHandle, 1);
                uAtClientCommandStop(atHandle);
                errorCode = uAtClientResponseStart(atHandle, "+UBTGCHA:");
                if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    *pValueHandle = uAtClientReadInt(atHandle);
                    uAtClientReadInt(atHandle);
                }
                uAtClientResponseStop(atHandle);
                uAtClientUnlock(atHandle);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

#endif