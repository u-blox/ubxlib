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
#include "u_ble_context.h"

#include "u_cx_gatt_client.h"
#include "u_cx_gatt_server.h"
#include "u_cx_urc.h"

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

// Some convenience hex-bin routines with allocation
static bool hexToBin(const char *hexData, uint8_t **pBinData, int32_t *pBinDataLength)
{
    bool ok = false;
    *pBinDataLength = strlen(hexData) / 2;
    *pBinData = pUPortMalloc(*pBinDataLength);
    if (*pBinData != NULL) {
        ok = uCxAtUtilHexToBinary(hexData, *pBinData, *pBinDataLength) == *pBinDataLength;
        if (!ok) {
            uPortFree(*pBinData);
        }
    }
    return ok;
}

static bool binToHex(const uint8_t *pBinData, int32_t binDataLength, char **hexData)
{
    bool ok = false;
    size_t hexDataLength = binDataLength * 2 + 1;
    *hexData = pUPortMalloc(hexDataLength);
    if (*hexData != NULL) {
        ok = uCxAtUtilBinaryToHex(pBinData, binDataLength, *hexData, hexDataLength);
        if (!ok) {
            uPortFree(*hexData);
        }
    }
    return ok;
}

static void notificationCallback(struct uCxHandle *pUcxHandle, int32_t connHandle,
                                 int32_t valueHandle, uByteArray_t *pValue)
{
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    uBleDeviceState_t *pState = (uBleDeviceState_t *)pInstance->pBleContext;
    if ((pState != NULL) && (pState->notifyCallback != NULL)) {
        pState->notifyCallback(connHandle, valueHandle, pValue->pData, (uint8_t)pValue->length);
    }
}

static void writeCallback(struct uCxHandle *pUcxHandle, int32_t connHandle, int32_t valueHandle,
                          uByteArray_t *pValue, uOptions_t options)
{
    (void)options;
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    uBleDeviceState_t *pState = (uBleDeviceState_t *)pInstance->pBleContext;
    if ((pState != NULL) && (pState->writeCallback != NULL)) {
        pState->writeCallback(connHandle, valueHandle, pValue->pData, (uint8_t)pValue->length);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleGattDiscoverServices(uDeviceHandle_t devHandle, int32_t connHandle,
                                 uBleGattDiscoverServiceCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (cb != NULL)) {
        uCxGattClientDiscoverPrimaryServicesBegin(pUcxHandle, connHandle);
        uCxGattClientDiscoverPrimaryServices_t resp;
        while (uCxGattClientDiscoverPrimaryServicesGetNext(pUcxHandle, &resp)) {
            char *pUuid;
            if (binToHex(resp.uuid.pData, resp.uuid.length, &pUuid)) {
                cb(connHandle, resp.start_handle, resp.end_handle, pUuid);
                uPortFree(pUuid);
            }
        }
        errorCode = uCxEnd(pUcxHandle);
    }
    return errorCode;
}

int32_t uBleGattDiscoverChar(uDeviceHandle_t devHandle,
                             int32_t connHandle,
                             uBleGattDiscoverCharCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (cb != NULL)) {
        uCxGattClientDiscoverServiceCharsBegin(pUcxHandle, connHandle, 1, 65535);
        uCxGattClientDiscoverServiceChars_t resp;
        while (uCxGattClientDiscoverServiceCharsGetNext(pUcxHandle, &resp)) {
            char *pUuid;
            if (binToHex(resp.uuid.pData, resp.uuid.length, &pUuid)) {
                cb(connHandle, resp.attr_handle, resp.properties.pData[0], resp.value_handle, pUuid);
                uPortFree(pUuid);
            }
        }
        errorCode = uCxEnd(pUcxHandle);
    }
    return errorCode;
}

int32_t uBleGattEnableNotification(uDeviceHandle_t devHandle,
                                   int32_t connHandle, uint16_t valueHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxGattClientConfigWrite(pUcxHandle, connHandle,
                                             (int32_t)(valueHandle + 1), U_CONFIG_ENABLE_NOTIFICATIONS);
    }
    return errorCode;
}

int32_t uBleGattSetNotificationCallback(uDeviceHandle_t devHandle,
                                        uBleGattNotificationCallback_t cb)
{
    int32_t errorCode = uShortRangeLock();
    if (errorCode == 0) {
        int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
        uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pUcxHandle != NULL) && (pInstance != NULL)) {
            errorCode = checkCreateBleContext(pInstance);
            if (errorCode == 0) {
                ((uBleDeviceState_t *)(pInstance->pBleContext))->notifyCallback = cb;
                uCxGattClientRegisterNotification(pUcxHandle, notificationCallback);
                errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGattSetWriteCallback(uDeviceHandle_t devHandle,
                                 uBleGattWriteCallback_t cb)
{
    int32_t errorCode = uShortRangeLock();
    if (errorCode == 0) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
        uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pUcxHandle != NULL) && (pInstance != NULL)) {
            errorCode = checkCreateBleContext(pInstance);
            if (errorCode == 0) {
                ((uBleDeviceState_t *)(pInstance->pBleContext))->writeCallback = cb;
                uCxGattServerRegisterNotification(pUcxHandle, writeCallback);
                errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
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
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uByteArray_t resp;
        errorCode = uCxGattClientReadBegin(pUcxHandle, connHandle, valueHandle, &resp);
        if (errorCode == 0) {
            int32_t len = resp.length;
            if (len > valueLength) {
                len = valueLength;
            }
            memcpy(pValue, resp.pData, len);
        }
        errorCode = uCxEnd(pUcxHandle);
    }
    return errorCode;
}

int32_t uBleGattWriteValue(uDeviceHandle_t devHandle,
                           int32_t connHandle, uint16_t valueHandle,
                           const void *pValue, uint8_t valueLength,
                           bool waitResponse)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        if (waitResponse) {
            errorCode = uCxGattClientWrite(pUcxHandle, connHandle, valueHandle, pValue,
                                           valueLength);
        } else {
            errorCode = uCxGattClientWriteNoRsp(pUcxHandle, connHandle, valueHandle,
                                                pValue, valueLength);
        }
    }
    return errorCode;
}

int32_t uBleGattWriteNotifyValue(uDeviceHandle_t devHandle,
                                 int32_t connHandle, uint16_t valueHandle,
                                 const void *pValue, uint8_t valueLength)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxGattServerSendNotification(pUcxHandle, connHandle,
                                                  valueHandle, pValue, valueLength);
    }
    return errorCode;
}

int32_t uBleGattBeginAddService(uDeviceHandle_t devHandle,
                                const char *pUuid)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        int32_t servHandle;
        uint8_t *pUuidData;
        int32_t len;
        if (hexToBin(pUuid, &pUuidData, &len)) {
            errorCode = uCxGattServerServiceDefine(pUcxHandle, pUuidData, len, &servHandle);
            uPortFree(pUuidData);
        }
    }
    return errorCode;
}

int32_t uBleGattAddCharacteristic(uDeviceHandle_t devHandle,
                                  const char *pUuid, uint8_t properties,
                                  uint16_t *pValueHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        uint8_t *pUuidData;
        int32_t len;
        uCxGattServerHostCharDefine_t resp;
        if (hexToBin(pUuid, &pUuidData, &len)) {
            errorCode = uCxGattServerHostCharDefine(pUcxHandle, pUuidData, len,
                                                    &properties, 1, 1, 1, &resp);
            uPortFree(pUuidData);
            if (errorCode == 0) {
                *pValueHandle = resp.value_handle;
            }
        }
    }
    return errorCode;
}

int32_t uBleGattEndAddService(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxGattServerServiceActivate(pUcxHandle);
    }
    return errorCode;
}

#endif