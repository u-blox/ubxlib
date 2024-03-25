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

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef enum { BLE_ROLE_ANY,
               BLE_ROLE_CENTRAL,
               BLE_ROLE_PERIPHERAL
             } bleRoleCheck_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uint32_t msToTick(uint32_t ms)
{
    return ms * 1000 / 625;
}

static int32_t setUrcHandler(uAtClientHandle_t atHandle, const char *pPrefix,
                             void (*pHandler)(uAtClientHandle_t, void *), void *pHandlerParam)
{
    // Remove possible existing URC and add new one if specified
    int32_t errorCode = 0;
    uAtClientRemoveUrcHandler(atHandle, pPrefix);
    if (pHandler != NULL) {
        errorCode = uAtClientSetUrcHandler(atHandle, pPrefix, pHandler, pHandlerParam);
    }
    return errorCode;
}

static bool validateBle(uShortRangePrivateInstance_t *pInstance, bleRoleCheck_t roleCheck)
{
    // Validate that the device has BLE enabled, has the correct role and that
    // sps server is not enabled
    bool ok = false;
    if (pInstance) {
        uDeviceNetworkData_t *pNetworkData =
            pUNetworkGetNetworkData(pInstance->devHandle, U_NETWORK_TYPE_BLE);
        if (pNetworkData) {
            uNetworkCfgBle_t *pCfgBle = (uNetworkCfgBle_t *)pNetworkData->pCfg;
            ok = pCfgBle != NULL &&
                 !pCfgBle->spsServer &&
                 pInstance->atHandle != NULL;
            if (ok && roleCheck != BLE_ROLE_ANY) {
                int32_t currRole = uBlePrivateGetRole(pInstance->devHandle);
                if (roleCheck == BLE_ROLE_CENTRAL) {
                    ok = currRole == U_BLE_CFG_ROLE_CENTRAL ||
                         currRole == U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL;
                } else {
                    ok = currRole == U_BLE_CFG_ROLE_PERIPHERAL ||
                         currRole == U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL;
                }
            }
        }
    }
    return ok;
}

static void connectUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapConnectCallback_t cb = (uBleGapConnectCallback_t)pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    int32_t connHandle = uAtClientReadInt(atHandle);
    (void)uAtClientReadInt(atHandle);
    uAtClientReadString(atHandle, address, sizeof(address), false);
    cb(connHandle, address, true);
}

static void disconnectUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapConnectCallback_t cb = (uBleGapConnectCallback_t)pParameter;
    int32_t connHandle = uAtClientReadInt(atHandle);
    cb(connHandle, NULL, false);
}

// Use common connect urc callback using the provided application callback as parameter
static int32_t setConnectUrc(uAtClientHandle_t atHandle, uBleGapConnectCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    if (cb) {
        errorCode = setUrcHandler(atHandle, "+UUBTACLC:", connectUrc, cb);
        if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
            errorCode = setUrcHandler(atHandle, "+UUBTACLD:", disconnectUrc, cb);
        }
    }
    return errorCode;
}

static void phyUpdateUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapPhyUpdateCallback_t cb = (uBleGapPhyUpdateCallback_t)pParameter;
    int32_t connHandle = uAtClientReadInt(atHandle);
    int32_t status = uAtClientReadInt(atHandle);
    int32_t txPhy = uAtClientReadInt(atHandle);
    int32_t rxPhy = uAtClientReadInt(atHandle);
    cb(connHandle, status, txPhy, rxPhy);
}

static void bleBondCompleteUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapBondCompleteCallback_t cb = (uBleGapBondCompleteCallback_t)pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    uAtClientReadString(atHandle, address, sizeof(address), false);
    int32_t status = uAtClientReadInt(atHandle);
    cb(address, status);
}

static void bleBondConfirmCUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapBondConfirmCallback_t cb = (uBleGapBondConfirmCallback_t)pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    uAtClientReadString(atHandle, address, sizeof(address), false);
    int32_t numVal = uAtClientReadInt(atHandle);
    cb(address, numVal);
}

static void bleBondPassKeyRequestUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapBondPasskeyRequestCallback_t cb = (uBleGapBondPasskeyRequestCallback_t)pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    uAtClientReadString(atHandle, address, sizeof(address), false);
    cb(address);
}

static void bleBondPassKeyEnterUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uBleGapBondPasskeyEntryCallback_t cb = (uBleGapBondPasskeyEntryCallback_t)pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    uAtClientReadString(atHandle, address, sizeof(address), false);
    int32_t passKey = uAtClientReadInt(atHandle);
    cb(address, passKey);
}

static bool setBleConfig(const uAtClientHandle_t atHandle, int32_t parameter, uint64_t value)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UBTLECFG=");
    uAtClientWriteInt(atHandle, parameter);
    uAtClientWriteUint64(atHandle, value);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle) == 0;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleGapGetMac(uDeviceHandle_t devHandle, char *pMac)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_TEMPORARY_FAILURE;
        if (pInstance != NULL &&
            pInstance->atHandle != NULL) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMLA=1");
            uAtClientCommandStop(atHandle);
            if (uAtClientResponseStart(atHandle, "+UMLA:") == 0) {
                uAtClientReadString(atHandle,
                                    pMac,
                                    U_SHORT_RANGE_BT_ADDRESS_SIZE,
                                    false);
                uAtClientResponseStop(atHandle);
            }
            errorCode = uAtClientUnlock(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapSetPairable(uDeviceHandle_t devHandle, bool isPairable)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTPM=");
        uAtClientWriteInt(atHandle, isPairable ? 2 : 1);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleSetBondParameters(uDeviceHandle_t devHandle, int32_t ioCapabilities,
                              int32_t bondSecurity,
                              uBleGapBondConfirmCallback_t confirmCb,
                              uBleGapBondPasskeyRequestCallback_t passKeyRequestCb,
                              uBleGapBondPasskeyEntryCallback_t passKeyEntryCb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        // Try to map the never UCX concept parameters to the old ones.
        int32_t secMode = 2;
        if (bondSecurity == U_BT_LE_BOND_NO_SEC) {
            secMode = 1;
        } else if (bondSecurity == U_BT_LE_BOND_UNAUTH) {
            secMode = 2;
        } else if (bondSecurity >= U_BT_LE_BOND_AUTH) {
            if (ioCapabilities == U_BT_LE_IO_DISP_ONLY) {
                secMode = 3;
            } else if (ioCapabilities == U_BT_LE_IO_DISP_YES_NO) {
                secMode = 4;
            } else if (ioCapabilities == U_BT_LE_IO_KEYB_ONLY) {
                secMode = 5;
            }
        }
        uAtClientCommandStart(atHandle, "AT+UBTSM=");
        uAtClientWriteInt(atHandle, secMode);
        uAtClientWriteInt(atHandle, 0);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode == 0) {
            errorCode = setUrcHandler(atHandle, "+UUBTUC:", bleBondConfirmCUrc, confirmCb);
            if (errorCode == 0) {
                errorCode = setUrcHandler(atHandle, "+UUBTUPE:", bleBondPassKeyRequestUrc,
                                          passKeyRequestCb);
            }
            if (errorCode == 0) {
                errorCode = setUrcHandler(atHandle, "+UUBTUPD:", bleBondPassKeyEnterUrc,
                                          passKeyEntryCb);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapBond(uDeviceHandle_t devHandle, const char *pAddress,
                    uBleGapBondCompleteCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == 0) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_CENTRAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            errorCode = setUrcHandler(atHandle, "+UUBTB:", bleBondCompleteUrc, cb);
            if (errorCode == 0) {
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTB=");
                uAtClientWriteString(atHandle, pAddress, false);
                uAtClientWriteInt(atHandle, 1);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapRemoveBond(uDeviceHandle_t devHandle, const char *pAddress)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == 0) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pAddress == NULL) {
            pAddress = "FFFFFFFFFFFF";
        }
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientRemoveUrcHandler(atHandle, "+UUBTB:");
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTUB=");
        uAtClientWriteString(atHandle, pAddress, false);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapBondConfirm(uDeviceHandle_t devHandle, bool confirm, const char *pAddress)
{
    // This will be called from URC callback so don't call uShortRangeLock here.
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    pInstance = pUShortRangePrivateGetInstance(devHandle);
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientCommandStart(atHandle, "AT+UBTUC=");
    uAtClientWriteString(atHandle, pAddress, false);
    uAtClientWriteInt(atHandle, confirm ? 1 : 0);
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientErrorGet(atHandle);
    return errorCode;
}

int32_t uBleGapBondEnterPasskey(uDeviceHandle_t devHandle, bool confirm, const char *pAddress,
                                int32_t passkey)
{
    // Respond to passkey request.
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == 0) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        uAtClientHandle_t atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UBTUPE=");
        uAtClientWriteString(atHandle, pAddress, false);
        uAtClientWriteInt(atHandle, confirm ? 1 : 0);
        uAtClientWriteInt(atHandle, passkey);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapSetConnectCallback(uDeviceHandle_t devHandle, uBleGapConnectCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (validateBle(pInstance, BLE_ROLE_ANY)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            errorCode = setConnectUrc(atHandle, cb);
            uAtClientUnlock(atHandle);
        } else {
            errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapScan(uDeviceHandle_t devHandle,
                    uBleGapDiscoveryType_t discType,
                    bool activeScan,
                    uint32_t timeousMs,
                    uBleGapScanCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_CENTRAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            // Set the timeout for the AT response and thereby the scan timeout
            uAtClientTimeoutSet(atHandle, timeousMs + 500);

            // Start the scan
            uAtClientCommandStart(atHandle, "AT+UBTD=");
            uAtClientWriteInt(atHandle, discType);
            uAtClientWriteInt(atHandle, activeScan ? 1 : 2);
            uAtClientWriteInt(atHandle, timeousMs);
            uAtClientCommandStop(atHandle);

            // Get the responses synchronously
            uBleScanResult_t result;
            bool ok = true;
            bool keepGoing = true;
            while (keepGoing && uAtClientResponseStart(atHandle, "+UBTD:") == 0) {
                errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
                ok = uAtClientReadString(atHandle,
                                         result.address,
                                         sizeof(result.address),
                                         false) == (sizeof(result.address) - 1);
                result.rssi = uAtClientReadInt(atHandle);
                ok = ok && uAtClientErrorGet(atHandle) == 0;
                ok = ok && uAtClientReadString(atHandle,
                                               result.name,
                                               sizeof(result.name),
                                               false) >= 0;
                int32_t x = uAtClientReadInt(atHandle);
                ok = ok && (x >= 0);
                result.dataType = (uint8_t) x;
                ok = ok && (result.dataType == 1 || result.dataType == 2);
                if (ok) {
                    ok = false;
                    x = uAtClientReadHexData(atHandle, result.data, sizeof(result.data));
                    if (x >= 0) {
                        result.dataLength = (uint8_t) x;
                        ok = true;
                    }
                }
                if (ok && cb) {
                    keepGoing = cb(&result);
                }
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapSetConnectParams(uDeviceHandle_t devHandle, uBleGapConnectConfig_t *pConfig)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, U_BLE_CFG_ROLE_CENTRAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            bool ok = setBleConfig(atHandle, 4, msToTick(pConfig->connIntervalMinMs));
            ok = ok && setBleConfig(atHandle, 5, msToTick(pConfig->connIntervalMaxMs));
            ok = ok && setBleConfig(atHandle, 6, pConfig->connLatency);
            ok = ok && setBleConfig(atHandle, 7, msToTick(pConfig->linkLossTimeoutMs));
            ok = ok && setBleConfig(atHandle, 8, msToTick(pConfig->connCreateTimeoutMs));
            ok = ok && setBleConfig(atHandle, 9, msToTick(pConfig->scanIntervalMs));
            ok = ok && setBleConfig(atHandle, 10, msToTick(pConfig->scanWindowMs));
            ok = ok && setBleConfig(atHandle, 27, pConfig->preferredTxPhy);
            ok = ok && setBleConfig(atHandle, 28, pConfig->preferredRxPhy);
            errorCode = ok ? 0 : uAtClientErrorGet(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapConnect(uDeviceHandle_t devHandle, const char *pAddress)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_CENTRAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UBTACLC=");
            uAtClientWriteString(atHandle, pAddress, false);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapRequestPhyChange(uDeviceHandle_t devHandle, int32_t connHandle,
                                int32_t txPhy, int32_t rxPhy, uBleGapPhyUpdateCallback_t cb)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, U_BLE_CFG_ROLE_CENTRAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            setUrcHandler(atHandle, "+UUBTLEPHYU:", phyUpdateUrc, cb);
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UBTLEPHYR=");
            uAtClientWriteInt(atHandle, connHandle);
            uAtClientWriteInt(atHandle, txPhy);
            uAtClientWriteInt(atHandle, rxPhy);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapDisconnect(uDeviceHandle_t devHandle, int32_t connHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_ANY)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UBTACLD=");
            uAtClientWriteInt(atHandle, connHandle);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientErrorGet(atHandle);
            uAtClientUnlock(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapSetAdvData(const char *pName,
                          const uint8_t *pManufData, uint8_t manufDataSize,
                          uint8_t *pAdvData, uint8_t advDataSize)
{
    if (!pName && !pManufData) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    uint8_t nameSize = pName ? (uint8_t)(strlen(pName) + 2) : 0; /* string length, tag and the string */
    manufDataSize = pManufData ? manufDataSize + 2 : 0;
    int32_t totSize = nameSize + manufDataSize;
    if (totSize > advDataSize) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (pName) {
        nameSize--;
        *(pAdvData++) = nameSize--;
        *(pAdvData++) = U_BT_DATA_NAME_COMPLETE;
        memcpy(pAdvData, pName, nameSize);
        pAdvData += nameSize;
    }
    if (pManufData) {
        manufDataSize--;
        *(pAdvData++) = manufDataSize--;
        *(pAdvData++) = U_BT_DATA_MANUFACTURER_DATA;
        memcpy(pAdvData, pManufData, manufDataSize);
    }
    return totSize;
}

int32_t uBleGapAdvertiseStart(uDeviceHandle_t devHandle, const uBleGapAdvConfig_t *pConfig)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_PERIPHERAL)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            bool ok;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UBTCM=");
            uAtClientWriteInt(atHandle, pConfig->connectable ? 2 : 1);
            uAtClientCommandStopReadResponse(atHandle);
            ok = uAtClientUnlock(atHandle) == 0;
            ok = ok && setBleConfig(atHandle, 1, msToTick(pConfig->minIntervalMs));
            ok = ok && setBleConfig(atHandle, 2, msToTick(pConfig->maxIntervalMs));
            if (ok) {
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTCFG=");
                uAtClientWriteInt(atHandle, 2);
                uAtClientWriteUint64(atHandle, msToTick(pConfig->maxClients));
                uAtClientCommandStopReadResponse(atHandle);
                ok = uAtClientUnlock(atHandle) == 0;
            }
            if (ok && pConfig->pAdvData) {
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTAD=");
                uAtClientWriteHexData(atHandle, pConfig->pAdvData, pConfig->advDataLength);
                uAtClientCommandStopReadResponse(atHandle);
                ok = uAtClientUnlock(atHandle) == 0;
            }
            if (ok && pConfig->pRespData) {
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTSD=");
                uAtClientWriteHexData(atHandle, pConfig->pRespData, pConfig->respDataLength);
                uAtClientCommandStopReadResponse(atHandle);
                ok = uAtClientUnlock(atHandle) == 0;
            }
            errorCode = ok ? 0 : uAtClientErrorGet(atHandle);
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uBleGapAdvertiseStop(uDeviceHandle_t devHandle)
{
    // Not implemented for now due to currently not available in u-connect
    (void)devHandle;
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uBleGapReset(uDeviceHandle_t devHandle)
{
    // Restart the module to restore device to default settings
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t)U_BLE_ERROR_INVALID_MODE;
        if (validateBle(pInstance, BLE_ROLE_ANY)) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UFACTORY");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientCommandStart(atHandle, "AT+CPWROFF");
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            // Wait for restart to complete
            if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                uPortTaskBlock(5000);
                uAtClientFlush(atHandle);
            }
        }
        uShortRangeUnlock();
    }
    return errorCode;
}
#endif