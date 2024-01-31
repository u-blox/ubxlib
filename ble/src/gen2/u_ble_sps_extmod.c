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
 * @brief Implementation of the data API for ble.
 */

#ifndef U_CFG_BLE_MODULE_INTERNAL

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_cfg_sw.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"
#include "u_ble_sps.h"
#include "u_ble_private.h"
#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

#include "u_ble_gap.h"
#include "u_ble_gatt.h"
#include "u_ble_context.h"

#include "u_cx_urc.h"
#include "u_cx_bluetooth.h"
#include "u_cx_sps.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

// Some globals needed as uCx assumes a existing connection when
// connecting to a SPS server
static uPortSemaphoreHandle_t gSemaphore;
static uPortMutexHandle_t gMutex;

/* ----------------------------------------------------------------
 * EXPORTED VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void spsCommonCallback(struct uCxHandle *pUcxHandle, int32_t connHandle,
                              int32_t status)
{
    if ((pUcxHandle != NULL) && (pUcxHandle->pAtClient != NULL) &&
        (pUcxHandle->pAtClient->pConfig != NULL)) {
        uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
        uBleDeviceState_t *pState = pGetBleContext(pUcxHandle->pAtClient->pConfig->pContext);
        if ((pState != NULL) && (pInstance->pSpsConnectionCallback != NULL)) {
            pInstance->pSpsConnectionCallback(connHandle, pState->spsAddr, status, 0, pState->mtu,
                                              pInstance->pSpsConnectionCallbackParameter);
        }
    }
}

static void spsConnectCallback(struct uCxHandle *pUcxHandle, int32_t connHandle)
{
    spsCommonCallback(pUcxHandle, connHandle, (int32_t)U_SHORT_RANGE_EVENT_CONNECTED);
}

static void spsDisconnectCallback(struct uCxHandle *pUcxHandle, int32_t connHandle)
{
    spsCommonCallback(pUcxHandle, connHandle, (int32_t)U_SHORT_RANGE_EVENT_DISCONNECTED);
}

static void spsDataAvailableCallback(struct uCxHandle *pUcxHandle, int32_t connHandle,
                                     int32_t numberBytes)
{
    (void)pUcxHandle;
    (void)connHandle;
    (void)numberBytes;

    if ((pUcxHandle != NULL) && (pUcxHandle->pAtClient != NULL)) {
        uBleDeviceState_t *pState = pGetBleContext(pUcxHandle->pAtClient->pConfig->pContext);
        if (pState != NULL) {
            pState->spsDataAvailable = true;
        }
    }
}

static void bleConnectCallback(struct uCxHandle *pUcxHandle, int32_t connHandle,
                               uBtLeAddress_t *pBdAddr)
{
    if ((pUcxHandle != NULL) && (pUcxHandle->pAtClient != NULL)) {
        uBleDeviceState_t *pState = pGetBleContext(pUcxHandle->pAtClient->pConfig->pContext);
        if (pState != NULL) {
            pState->connHandle = connHandle;
            uCxBdAddressToString(pBdAddr, pState->spsAddr, sizeof(pState->spsAddr));
        }
    }
    uPortSemaphoreGive(gSemaphore);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleSpsSetCallbackConnectionStatus(uDeviceHandle_t devHandle,
                                           uBleSpsConnectionStatusCallback_t pCallback,
                                           void *pCallbackParameter)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    if (pInstance != NULL) {
        pInstance->pSpsConnectionCallback = pCallback;
        pInstance->pSpsConnectionCallbackParameter = pCallbackParameter;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uBleSpsConnectSps(uDeviceHandle_t devHandle,
                          const char *pAddress,
                          const uBleSpsConnParams_t *pConnParams)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (pInstance != NULL)) {
        uPortMutexLock(gMutex);
        uCxBluetoothRegisterConnect(pUcxHandle, bleConnectCallback);
        errorCode = uBleGapConnect(devHandle, pAddress);
        if (errorCode == 0) {
            errorCode = uPortSemaphoreTryTake(gSemaphore, 5000);
            if (errorCode == 0) {
                uCxSpsRegisterConnect(pUcxHandle, spsConnectCallback);
                uCxSpsRegisterDisconnect(pUcxHandle, spsDisconnectCallback);
                uCxSpsRegisterDataAvailable(pUcxHandle, spsDataAvailableCallback);
                if (pConnParams != NULL) {
                    // Setup the parameters currently available in uCx
                    errorCode = uCxBluetoothSetConnectionIntervalMin(pUcxHandle,
                                                                     pConnParams->connIntervalMin);
                    if (errorCode == 0) {
                        errorCode = uCxBluetoothSetConnectionIntervalMax(pUcxHandle,
                                                                         pConnParams->connIntervalMax);
                    }
                    if (errorCode == 0) {
                        errorCode = uCxBluetoothSetConnectionPeripheralLatency(
                                        pUcxHandle, pConnParams->connLatency);
                    }
                    if (errorCode == 0) {
                        errorCode = uCxBluetoothSetConnectionLinklossTimeout(pUcxHandle,
                                                                             pConnParams->linkLossTimeout);
                    }
                }
                if (errorCode == 0) {
                    uBleDeviceState_t *pState = (uBleDeviceState_t *)pInstance->pBleContext;
                    uCxBluetoothGetConnectionStatus_t resp;
                    resp.status_val = 20; // Best guess in case we fail
                    uCxBluetoothGetConnectionStatus(pUcxHandle, pState->connHandle,
                                                    U_PROPERTY_ID_MTU_SIZE, &resp);
                    pState->mtu = resp.status_val;
                    errorCode = uCxSpsConnect1(pUcxHandle, pState->connHandle);
                }
            }
        }
        uPortMutexUnlock(gMutex);
    }
    return errorCode;
}

int32_t uBleSpsDisconnect(uDeviceHandle_t devHandle, int32_t connHandle)
{
    return uBleGapDisconnect(devHandle, connHandle);
}

int32_t uBleSpsReceive(uDeviceHandle_t devHandle, int32_t channel, char *pData, int32_t length)
{
    (void)channel;
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uBleDeviceState_t *pState = pGetBleContext(pUShortRangePrivateGetInstance(devHandle));
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (pState != NULL)) {
        errorCodeOrLength = uCxSpsRead(pUcxHandle, pState->spsConnHandle, length, (uint8_t *)pData);
    }
    return errorCodeOrLength;
}

int32_t uBleSpsSend(uDeviceHandle_t devHandle, int32_t channel, const char *pData, int32_t length)
{
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    uBleDeviceState_t *pState = pGetBleContext(pInstance);
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (pInstance != NULL) && (pState != NULL)) {
        int32_t received = 0;
        do {
            errorCodeOrLength = uCxSpsWrite(pUcxHandle, pState->spsConnHandle,
                                            (uint8_t *)pData, MIN(1000, length - received));
            if (errorCodeOrLength > 0) {
                received += errorCodeOrLength;
                pData += errorCodeOrLength;
            }
        } while ((received < length) && (errorCodeOrLength > 0));
        if (errorCodeOrLength >= 0) {
            errorCodeOrLength = received;
        }
    }
    if (pState->spsDataAvailable) {
        pState->spsDataAvailable = false;
        if (pInstance->pBtDataAvailableCallback != NULL) {
            pInstance->pBtDataAvailableCallback(0, pInstance->pBtDataCallbackParameter);
        }
    }
    return errorCodeOrLength;
}

int32_t uBleSpsSetSendTimeout(uDeviceHandle_t devHandle, int32_t channel, uint32_t timeout)
{
    (void)devHandle;
    (void)channel;
    (void)timeout;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

int32_t uBleSpsSetDataAvailableCallback(uDeviceHandle_t devHandle,
                                        uBleSpsAvailableCallback_t pCallback,
                                        void *pCallbackParameter)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    if (pInstance != NULL) {
        pInstance->pBtDataAvailableCallback = pCallback;
        pInstance->pBtDataCallbackParameter = pCallbackParameter;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

void uBleSpsPrivateInit(void)
{
    if (gSemaphore == NULL) {
        uPortSemaphoreCreate(&gSemaphore, 0, 1);
    }
    if (gMutex == NULL) {
        uPortMutexCreate(&gMutex);
    }
}

void uBleSpsPrivateDeinit(void)
{
    if (gSemaphore != NULL) {
        uPortSemaphoreDelete(gSemaphore);
        gSemaphore = NULL;
    }
    if (gMutex != NULL) {
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

//lint -esym(818, pHandles) Suppress pHandles could be const, need to
// follow prototype
int32_t uBleSpsGetSpsServerHandles(uDeviceHandle_t devHandle, int32_t channel,
                                   uBleSpsHandles_t *pHandles)
{
    (void)channel;
    (void)devHandle;
    (void)pHandles;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uBleSpsPresetSpsServerHandles(uDeviceHandle_t devHandle, const uBleSpsHandles_t *pHandles)
{
    (void)devHandle;
    (void)pHandles;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uBleSpsDisableFlowCtrlOnNext(uDeviceHandle_t devHandle)
{
    (void)devHandle;
    return (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
}

#endif

// End of file
