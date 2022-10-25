/*
 * Copyright 2019-2022 u-blox
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
 * @brief Implementation of the "general" API for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "u_cfg_sw.h"

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"

#include "u_hex_bin_convert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Suppress picky lint "not referenced"
//lint -esym(750, U_IFACE_TYPE_UNKNOWN)
//lint -esym(750, U_IFACE_TYPE_WIFI_AP)
//lint -esym(750, U_IFACE_TYPE_ETHERNET)
//lint -esym(750, U_IFACE_TYPE_PPP)
//lint -esym(750, U_IFACE_TYPE_BRIDGE)
//lint -esym(750, U_IFACE_TYPE_BT_PAN)
#define U_IFACE_TYPE_UNKNOWN  0
#define U_IFACE_TYPE_WIFI_STA 1
#define U_IFACE_TYPE_WIFI_AP  2
#define U_IFACE_TYPE_ETHERNET 3
#define U_IFACE_TYPE_PPP      4
#define U_IFACE_TYPE_BRIDGE   5
#define U_IFACE_TYPE_BT_PAN   6

//lint -esym(767, LOG_TAG) Suppress LOG_TAG defined differently in another module
//lint -esym(750, LOG_TAG) Suppress LOG_TAG not referenced
#define LOG_TAG "U_WIFI: "

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    uDeviceHandle_t devHandle;
    int32_t status;
    int32_t connId;
    int32_t channel;
    char bssid[U_WIFI_BSSID_SIZE];
    int32_t reason;
} uWifiConnection_t;

typedef struct {
    uDeviceHandle_t devHandle;
    int32_t interfaceId;
} uWifiworkEvent_t;

//lint -esym(749, uWifiStaCfgAction_t::STA_ACTION_RESET) Suppress not referenced
//lint -esym(749, uWifiStaCfgAction_t::STA_ACTION_LOAD) Suppress not referenced
//lint -esym(749, uWifiStaCfgAction_t::STA_ACTION_STORE) Suppress not referenced
typedef enum {
    STA_ACTION_RESET = 0,
    STA_ACTION_STORE = 1,
    STA_ACTION_LOAD = 2,
    STA_ACTION_ACTIVATE = 3,
    STA_ACTION_DEACTIVATE = 4
} uWifiStaCfgAction_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Helper function for getting the short range instance */
static inline int32_t getInstance(uDeviceHandle_t devHandle,
                                  uShortRangePrivateInstance_t **ppInstance)
{
    *ppInstance = pUShortRangePrivateGetInstance(devHandle);
    if (*ppInstance == NULL) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if ((*ppInstance)->mode != U_SHORT_RANGE_MODE_EDM) {
        return (int32_t) U_WIFI_ERROR_INVALID_MODE;
    }

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

/** Helper function for writing a Wifi station config integer value */
static int32_t writeWifiStaCfgInt(uAtClientHandle_t atHandle,
                                  int32_t cfgId,
                                  int32_t tag,
                                  int32_t value)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UWSC=");
    uAtClientWriteInt(atHandle, cfgId);
    uAtClientWriteInt(atHandle, tag);
    uAtClientWriteInt(atHandle, value);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

/** Helper function for writing a Wifi station config string value */
static int32_t writeWifiStaCfgStr(uAtClientHandle_t atHandle,
                                  int32_t cfgId,
                                  int32_t tag,
                                  const char *pValue)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UWSC=");
    uAtClientWriteInt(atHandle, cfgId);
    uAtClientWriteInt(atHandle, tag);
    uAtClientWriteString(atHandle, pValue, true);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

/** Helper function for reading a Wifi station status string value */
static int32_t readWifiStaStatusString(uAtClientHandle_t atHandle,
                                       int32_t statusId,
                                       char *pString,
                                       size_t lengthBytes)
{
    int32_t retValue;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UWSSTAT=");
    uAtClientWriteInt(atHandle, statusId);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UWSSTAT:");
    // Skip status_id
    uAtClientSkipParameters(atHandle, 1);
    retValue = uAtClientReadString(atHandle, pString, lengthBytes, false);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);
    if (errorCode < 0) {
        retValue = errorCode;
    }
    return retValue;
}

/** Helper function for reading a Wifi station status int value */
static int32_t readWifiStaStatusInt(uAtClientHandle_t atHandle,
                                    int32_t statusId)
{
    int32_t retValue;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UWSSTAT=");
    uAtClientWriteInt(atHandle, statusId);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UWSSTAT:");
    // Skip status_id
    uAtClientSkipParameters(atHandle, 1);
    retValue = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);
    if (errorCode < 0) {
        retValue = errorCode;
    }
    return retValue;
}

/** Helper function for triggering a Wifi station config action */
static int32_t writeWifiStaCfgAction(uAtClientHandle_t atHandle,
                                     int32_t cfgId,
                                     uWifiStaCfgAction_t action)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UWSCA=");
    uAtClientWriteInt(atHandle, cfgId);
    uAtClientWriteInt(atHandle, (int32_t) action);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

/** Helper function for reading an interface status integer */
static int32_t readIfaceStatusInt(uAtClientHandle_t atHandle,
                                  int32_t interfaceId,
                                  int32_t statusId)
{
    int32_t retValue;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UNSTAT=");
    uAtClientWriteInt(atHandle, interfaceId);
    uAtClientWriteInt(atHandle, statusId);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UNSTAT:");
    // Skip interface_id and status_id
    uAtClientSkipParameters(atHandle, 2);
    retValue = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);
    if (errorCode < 0) {
        retValue = errorCode;
    }
    return retValue;
}

/** Helper function for reading an interface status string */
static int32_t readIfaceStatusString(uAtClientHandle_t atHandle,
                                     int32_t interfaceId,
                                     int32_t statusId,
                                     char *pString,
                                     size_t lengthBytes)
{
    int32_t retValue;
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UNSTAT=");
    uAtClientWriteInt(atHandle, interfaceId);
    uAtClientWriteInt(atHandle, statusId);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UNSTAT:");
    // Skip interface_id and status_id
    uAtClientSkipParameters(atHandle, 2);
    retValue = uAtClientReadString(atHandle, pString, lengthBytes, false);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);
    if (errorCode < 0) {
        retValue = errorCode;
    }
    return retValue;
}

static void wifiConnectCallback(uAtClientHandle_t atHandle,
                                void *pParameter)
{
    volatile uWifiConnectionStatusCallback_t pCallback = NULL;
    volatile void *pCallbackParam = NULL;
    uWifiConnection_t *pStatus = (uWifiConnection_t *) pParameter;

    (void) atHandle;

    if (!pStatus) {
        return;
    }

    if (uShortRangeLock() == (int32_t)U_ERROR_COMMON_SUCCESS) {
        const uShortRangePrivateInstance_t *pInstance;
        pInstance = pUShortRangePrivateGetInstance(pStatus->devHandle);
        if (pInstance && pInstance->pWifiConnectionStatusCallback) {
            pCallback = pInstance->pWifiConnectionStatusCallback;
            pCallbackParam = pInstance->pWifiConnectionStatusCallbackParameter;
        }

        uShortRangeUnlock();

        if (pCallback) {
            //lint -e(1773) Suppress attempt to cast away volatile
            pCallback(pStatus->devHandle,
                      pStatus->connId,
                      pStatus->status,
                      pStatus->channel,
                      pStatus->bssid,
                      pStatus->reason,
                      (void *)pCallbackParam);
        }
    }

    uPortFree(pStatus);
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUWLE_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uDeviceHandle_t devHandle = (uDeviceHandle_t)pParameter;
    char bssid[U_WIFI_BSSID_SIZE];
    int32_t connId;
    int32_t channel;
    uWifiConnection_t *pStatus;
    connId = uAtClientReadInt(atHandle);
    (void)uAtClientReadString(atHandle, bssid, U_WIFI_BSSID_SIZE, false);
    channel = uAtClientReadInt(atHandle);

    pStatus = (uWifiConnection_t *) pUPortMalloc(sizeof(*pStatus));
    if (pStatus != NULL) {
        pStatus->devHandle = devHandle;
        pStatus->connId = connId;
        pStatus->status = U_WIFI_CON_STATUS_CONNECTED;
        pStatus->channel = channel;
        memcpy(pStatus->bssid, bssid, U_WIFI_BSSID_SIZE);
        pStatus->reason = 0;
        //lint -e(1773) Suppress attempt to cast away volatile
        if (uAtClientCallback(atHandle, wifiConnectCallback, pStatus) < 0) {
            uPortFree(pStatus);
        }
    }
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUWLD_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uDeviceHandle_t devHandle = (uDeviceHandle_t)pParameter;
    int32_t connId;
    int32_t reason;
    uWifiConnection_t *pStatus;
    connId = uAtClientReadInt(atHandle);
    reason = uAtClientReadInt(atHandle);

    pStatus = (uWifiConnection_t *) pUPortMalloc(sizeof(*pStatus));
    if (pStatus != NULL) {
        pStatus->devHandle = devHandle;
        pStatus->connId = connId;
        pStatus->status = U_WIFI_CON_STATUS_DISCONNECTED;
        pStatus->channel = 0;
        pStatus->bssid[0] = '\0';
        pStatus->reason = reason;
        if (uAtClientCallback(atHandle, wifiConnectCallback, pStatus) < 0) {
            uPortFree(pStatus);
        }
    }
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void networkStatusCallback(uAtClientHandle_t atHandle,
                                  void *pParameter)
{
    int32_t ifaceType = -1;
    uint32_t statusMask = 0;
    int32_t errorCode = -1;
    volatile uWifiNetworkStatusCallback_t pCallback = NULL;
    volatile void *pCallbackParam = NULL;
    const uWifiworkEvent_t *pEvt = (uWifiworkEvent_t *) pParameter;

    if (!pEvt) {
        return;
    }

    if (uShortRangeLock() != (int32_t)U_ERROR_COMMON_SUCCESS) {
        return;
    }

    const uShortRangePrivateInstance_t *pInstance;
    pInstance = pUShortRangePrivateGetInstance(pEvt->devHandle);
    if (pInstance && pInstance->pNetworkStatusCallback) {
        pCallback = pInstance->pNetworkStatusCallback;
        pCallbackParam = pInstance->pNetworkStatusCallbackParameter;
    }

    // Before we can call the callback we need to readout the actual network state
    if (pCallback) {
        // Read out the interface type
        ifaceType = readIfaceStatusInt(atHandle, pEvt->interfaceId, 2);
        // Ignore everything but Wifi STA for now
        if (ifaceType == U_IFACE_TYPE_WIFI_STA) {
            char ipV4Str[16] = "";
            // We are only interested if the IPv6 addr is valid or not. When it's invalid
            // it will just say "::". For this reason we only use a small readbuffer below:
            char ipV6Str[4] = "";

            errorCode = readIfaceStatusString(atHandle, pEvt->interfaceId, 103, ipV4Str, sizeof(ipV4Str));
            if (errorCode >= 0) {
                errorCode = readIfaceStatusString(atHandle, pEvt->interfaceId, 201, ipV6Str, sizeof(ipV6Str));
            }
            if (errorCode >= 0) {
                static const char invalidIpV4[] = "0.0.0.0";
                static const char invalidIpV6[] = "::";
                if (strcmp(ipV4Str, invalidIpV4) != 0) {
                    statusMask |= U_WIFI_STATUS_MASK_IPV4_UP;
                }
                if (strcmp(ipV6Str, invalidIpV6) != 0) {
                    statusMask |= U_WIFI_STATUS_MASK_IPV6_UP;
                }
            }
        }
    }

    // Important: Make sure we unlock the short range mutex before calling callback
    uShortRangeUnlock();

    if (errorCode >= 0 && pCallback) {
        //lint -e(1773) Suppress attempt to cast away volatile
        pCallback(pEvt->devHandle, ifaceType, statusMask, (void *)pCallbackParam);
    }

    uPortFree(pParameter);
}

static void UUNU_urc(uAtClientHandle_t atHandle,
                     void *pParameter)
{
    int32_t interfaceId;
    interfaceId = uAtClientReadInt(atHandle);
    if (interfaceId >= 0) {
        uWifiworkEvent_t *pEvt;
        pEvt = (uWifiworkEvent_t *) pUPortMalloc(sizeof(uWifiworkEvent_t));
        if (pEvt != NULL) {
            pEvt->devHandle = (uDeviceHandle_t)pParameter;
            pEvt->interfaceId = interfaceId;
            if (uAtClientCallback(atHandle, networkStatusCallback, pEvt) < 0) {
                uPortFree(pEvt);
            }
        }
    }
}

static void UUND_urc(uAtClientHandle_t atHandle,
                     void *pParameter)
{
    int32_t interfaceId;
    interfaceId = uAtClientReadInt(atHandle);
    if (interfaceId >= 0) {
        uWifiworkEvent_t *pEvt;
        pEvt = (uWifiworkEvent_t *) pUPortMalloc(sizeof(uWifiworkEvent_t));
        if (pEvt != NULL) {
            pEvt->devHandle = (uDeviceHandle_t)pParameter;
            pEvt->interfaceId = interfaceId;
            if (uAtClientCallback(atHandle, networkStatusCallback, pEvt) < 0) {
                uPortFree(pEvt);
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the wifi driver.
int32_t uWifiInit()
{
    return uShortRangeInit();
}

// Shut-down the wifi driver.
void uWifiDeinit()
{
    uShortRangeDeinit();
}

int32_t uWifiSetConnectionStatusCallback(uDeviceHandle_t devHandle,
                                         uWifiConnectionStatusCallback_t pCallback,
                                         void *pCallbackParameter)
{
    int32_t errorCode;
    uShortRangePrivateInstance_t *pInstance;

    errorCode = uShortRangeLock();
    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    errorCode = getInstance(devHandle, &pInstance);
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUWLE:");
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUWLD:");
        if (pCallback != NULL) {
            pInstance->pWifiConnectionStatusCallback = pCallback;
            pInstance->pWifiConnectionStatusCallbackParameter = pCallbackParameter;
            uAtClientSetUrcHandler(pInstance->atHandle, "+UUWLE:",
                                   UUWLE_urc, (void *)devHandle);
            uAtClientSetUrcHandler(pInstance->atHandle, "+UUWLD:",
                                   UUWLD_urc, (void *)devHandle);
        } else {
            pInstance->pWifiConnectionStatusCallback = NULL;
        }
    }

    uShortRangeUnlock();

    return errorCode;
}

int32_t uWifiSetNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      uWifiNetworkStatusCallback_t pCallback,
                                      void *pCallbackParameter)
{
    int32_t errorCode;
    uShortRangePrivateInstance_t *pInstance;

    errorCode = uShortRangeLock();
    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    errorCode = getInstance(devHandle, &pInstance);
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUNU:");
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUND:");
        if (pCallback != NULL) {
            pInstance->pNetworkStatusCallback = pCallback;
            pInstance->pNetworkStatusCallbackParameter = pCallbackParameter;
            uAtClientSetUrcHandler(pInstance->atHandle, "+UUNU:",
                                   UUNU_urc, (void *)devHandle);
            uAtClientSetUrcHandler(pInstance->atHandle, "+UUND:",
                                   UUND_urc, (void *)devHandle);
        } else {
            pInstance->pNetworkStatusCallback = NULL;
        }
    }

    uShortRangeUnlock();

    return errorCode;
}

int32_t uWifiStationConnect(uDeviceHandle_t devHandle, const char *pSsid,
                            uWifiAuth_t authentication,
                            const char *pPassPhrase)
{
    int32_t errorCode;
    uShortRangePrivateInstance_t *pInstance;

    errorCode = uShortRangeLock();
    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    errorCode = getInstance(devHandle, &pInstance);
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientHandle_t atHandle = pInstance->atHandle;

        // Read connection status
        int32_t conStatus = readWifiStaStatusInt(atHandle, 3);
        if (conStatus == 2) {
            // Wifi already connected. Check if the SSID is the same
            char ssid[32 + 1];
            errorCode = (int32_t) U_WIFI_ERROR_ALREADY_CONNECTED;
            int32_t tmp = readWifiStaStatusString(atHandle, 0, ssid, sizeof(ssid));
            if (tmp >= 0) {
                if (strcmp(ssid, pSsid) == 0) {
                    errorCode = (int32_t) U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID;
                }
            }
        }

        // Configure Wifi
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Set Wifi STA inactive on start up
            uPortLog(LOG_TAG "Activating wifi STA mode\n");
            errorCode = writeWifiStaCfgInt(atHandle, 0, 0, 0);
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Set SSID
            errorCode = writeWifiStaCfgStr(atHandle, 0, 2, pSsid);
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Set authentication
            errorCode = writeWifiStaCfgInt(atHandle, 0, 5, (int32_t)authentication);
        }
        if ((errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) &&
            (authentication != U_WIFI_AUTH_OPEN)) {
            // Set PSK/passphrase
            errorCode = writeWifiStaCfgStr(atHandle, 0, 8, pPassPhrase);
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Set IP mode to static IP
            errorCode = writeWifiStaCfgInt(atHandle, 0, 100, 2);
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Activate wifi
            errorCode = writeWifiStaCfgAction(atHandle, 0, STA_ACTION_ACTIVATE);
        }
    }

    uShortRangeUnlock();

    return errorCode;
}

int32_t uWifiStationDisconnect(uDeviceHandle_t devHandle)
{
    int32_t errorCode;
    uShortRangePrivateInstance_t *pInstance;

    errorCode = uShortRangeLock();
    if (errorCode != (int32_t)U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    errorCode = getInstance(devHandle, &pInstance);
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientHandle_t atHandle = pInstance->atHandle;
        // Read connection status
        int32_t conStatus = readWifiStaStatusInt(atHandle, 3);
        if (conStatus != 0) {
            uPortLog(LOG_TAG "De-activating wifi STA mode\n");
            errorCode = writeWifiStaCfgAction(atHandle, 0, STA_ACTION_DEACTIVATE);
        } else {
            // Wifi is already disabled
            errorCode = (int32_t) U_WIFI_ERROR_ALREADY_DISCONNECTED;
        }
    }

    uShortRangeUnlock();

    return errorCode;
}


int32_t uWifiStationScan(uDeviceHandle_t devHandle, const char *pSsid,
                         uWifiScanResultCallback_t pCallback)
{
    int32_t errorCode;
    uShortRangePrivateInstance_t *pInstance;

    errorCode = uShortRangeLock();
    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    errorCode = getInstance(devHandle, &pInstance);
    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientHandle_t atHandle = pInstance->atHandle;

        uAtClientLock(atHandle);
        // Since the scanning can take some time we release the short range lock here
        // This should be fine since we currently have the AT client lock instead
        uShortRangeUnlock();
        if (pSsid) {
            uAtClientCommandStart(atHandle, "AT+UWSCAN=");
            uAtClientWriteString(atHandle, pSsid, false);
        } else {
            uAtClientCommandStart(atHandle, "AT+UWSCAN");
        }
        uAtClientCommandStop(atHandle);

        uAtClientTimeoutSet(atHandle, 10000);

        // Handle the scan results
        // Loop until we get OK, ERROR or timeout
        while (uAtClientResponseStart(atHandle, "+UWSCAN:") == 0) {
            uWifiScanResult_t scanResult;
            int32_t result;
            char bssid[32];

            result = uAtClientReadString(atHandle, bssid, sizeof(bssid), false);
            if (result >= 0) {
                if (uHexToBin(bssid, result, (char *)scanResult.bssid) != result / 2) {
                    result = -1;
                }
            }
            if (result < 0) {
                uPortLog(LOG_TAG "Warning: Failed to parse BSSID");
            }
            scanResult.opMode = uAtClientReadInt(atHandle);
            result = uAtClientReadString(atHandle, scanResult.ssid, sizeof(scanResult.ssid), false);
            if (result < 0) {
                uPortLog(LOG_TAG "Warning: Failed to parse SSID");
            }
            scanResult.channel = uAtClientReadInt(atHandle);
            scanResult.rssi = uAtClientReadInt(atHandle);
            scanResult.authSuiteBitmask = uAtClientReadInt(atHandle);
            scanResult.uniCipherBitmask = (uint8_t)uAtClientReadInt(atHandle);
            scanResult.grpCipherBitmask = (uint8_t)uAtClientReadInt(atHandle);

            pCallback(devHandle, &scanResult);
        }

        errorCode = uAtClientUnlock(atHandle);
    } else {
        uShortRangeUnlock();
    }


    return errorCode;
}

// End of file
