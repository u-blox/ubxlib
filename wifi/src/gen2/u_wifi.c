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
 * @brief Implementation of the "general" API for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "u_cfg_sw.h"

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_cfg.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"
#include "u_wifi_cfg.h"

#include "u_hex_bin_convert.h"

// The headers below are necessary to work around an Espressif linker problem, see uWifiInit()
#include "u_network.h"
#include "u_network_config_wifi.h"
#include "u_network_private_wifi.h"
#include "u_sock.h"
#include "u_wifi_sock.h"     // For uWifiSockPrivateLink()
#include "u_mqtt_common.h"
#include "u_mqtt_client.h"
#include "u_wifi_mqtt.h"     // For uWifiMqttPrivateLink()
#include "u_wifi_http_private.h"   // For uWifiHttpPrivateLink()
#include "u_wifi_loc_private.h"    // For uWifiLocPrivateLink()

#include "u_cx_system.h"
#include "u_cx_urc.h"
#include "u_cx_wifi.h"
#include "u_cx_log.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define WLAN_HANDLE (0)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void WiFiLinkUpCallback(struct uCxHandle *pUcxHandle, int32_t wlan_handle,
                               uMacAddress_t *pBssid, int32_t channel)
{
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    if (pInstance != NULL) {
        pInstance->wifiState = 1;
        if (pInstance->pWifiConnectionStatusCallback) {
            char bssidStr[U_MAC_STRING_MAX_LENGTH_BYTES];
            uCxMacAddressToString(pBssid, bssidStr, sizeof(bssidStr));
            pInstance->pWifiConnectionStatusCallback(
                pInstance->devHandle, wlan_handle, U_WIFI_CON_STATUS_CONNECTED, channel, bssidStr,
                0, pInstance->pWifiConnectionStatusCallbackParameter);
        }
    }
}

static void WiFiLinkDownCallback(struct uCxHandle *pUcxHandle, int32_t wlan_handle, int32_t reason)
{
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    if (pInstance != NULL && (uShortRangeLock() == 0)) {
        // Try to map ucx reasons to old uconnect ones
        int32_t uWifiReason = (int32_t)U_WIFI_REASON_NETWORK_DISABLED;
        if ((reason == 15) || (reason == 2)) {
            uWifiReason = U_WIFI_REASON_SECURITY_PROBLEM;
        } else if ((reason == 0) && (pInstance->wifiState == 0)) {
            // *** UCX WORKAROUND FIX ***
            // Ucx reason is 0 both for out_of_range and manual close
            uWifiReason = U_WIFI_REASON_OUT_OF_RANGE;
        }
        pInstance->wifiState = 0;
        uWifiConnectionStatusCallback_t pCb = pInstance->pWifiConnectionStatusCallback;
        void *pCbParam = pInstance->pWifiConnectionStatusCallbackParameter;
        uShortRangeUnlock();

        if (pCb) {
            pCb(pInstance->devHandle, wlan_handle, U_WIFI_CON_STATUS_DISCONNECTED, 0, "",
                uWifiReason, pCbParam);
        }
    }
}

static void wiFiUpCallback(struct uCxHandle *pUcxHandle)
{
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    if (pInstance->pNetworkStatusCallback) {
        pInstance->pNetworkStatusCallback(pInstance->devHandle, 1,
                                          U_WIFI_STATUS_MASK_IPV4_UP | U_WIFI_STATUS_MASK_IPV6_UP,
                                          pInstance->pNetworkStatusCallbackParameter);
    }
}

static void wiFiDownCallback(struct uCxHandle *pUcxHandle)
{
    uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
    if (pInstance->pNetworkStatusCallback) {
        pInstance->pNetworkStatusCallback(pInstance->devHandle, 1, 0,
                                          pInstance->pNetworkStatusCallbackParameter);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the wifi driver.
int32_t uWifiInit()
{
    // Workaround for Espressif linker missing out files that
    // only contain functions which also have weak alternatives
    // (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899)
    // Basically any file that might end up containing only functions
    // that also have WEAK linked counterparts will be lost, so we need
    // to add a dummy function in those files and call it from somewhere
    // that will always be present in the build, which for Wifi we
    // choose to be here
    uNetworkPrivateWifiLink();
    uWifiSockPrivateLink();
    uWifiMqttPrivateLink();
    uWifiHttpPrivateLink();
    uWifiLocPrivateLink();

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
    int32_t errorCode = uShortRangeLock();
    if (errorCode == 0) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
        uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pUcxHandle != NULL) && (pInstance != NULL)) {
            errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            uCxWifiRegisterLinkUp(pUcxHandle, WiFiLinkUpCallback);
            uCxWifiRegisterLinkDown(pUcxHandle, WiFiLinkDownCallback);
            pInstance->pWifiConnectionStatusCallback = pCallback;
            pInstance->pWifiConnectionStatusCallbackParameter = pCallbackParameter;
        }
        uShortRangeUnlock();
    }
    return errorCode;
}

int32_t uWifiSetNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      uWifiNetworkStatusCallback_t pCallback,
                                      void *pCallbackParameter)
{
    int32_t errorCode = uShortRangeLock();
    if (errorCode == 0) {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
        uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pUcxHandle != NULL) && (pInstance != NULL)) {
            errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            uCxWifiRegisterStationNetworkUp(pUcxHandle, wiFiUpCallback);
            uCxWifiRegisterStationNetworkDown(pUcxHandle, wiFiDownCallback);
            uCxWifiRegisterApNetworkUp(pUcxHandle, wiFiUpCallback);
            uCxWifiRegisterApNetworkDown(pUcxHandle, wiFiUpCallback);
            pInstance->pNetworkStatusCallback = pCallback;
            pInstance->pNetworkStatusCallbackParameter = pCallbackParameter;
            uShortRangeUnlock();
        }
    }
    return errorCode;
}

int32_t uWifiStationConnect(uDeviceHandle_t devHandle, const char *pSsid,
                            uWifiAuth_t authentication,
                            const char *pPassPhrase)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
        if (authentication == U_WIFI_AUTH_OPEN) {
            errorCode = uCxWifiStationSetSecurityOpen(pUcxHandle, WLAN_HANDLE);
        } else {
            if (pSsid != NULL) {
                uCxWifiStationStatus_t resp;
                resp.type = U_CX_WIFI_STATION_STATUS_RSP_TYPE_WIFI_STATUS_ID_STR;
                if (uCxWifiStationStatusBegin(pUcxHandle, U_WIFI_STATUS_ID_SSID, &resp) &&
                    (resp.rspWifiStatusIdStr.ssid[0] != 0)) {
                    // Already connected, check if same ssid
                    if (strcmp(resp.rspWifiStatusIdStr.ssid, pSsid) == 0) {
                        errorCode = (int32_t)U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID;
                    } else {
                        errorCode = (int32_t)U_WIFI_ERROR_ALREADY_CONNECTED;
                    }
                    uCxEnd(pUcxHandle);
                } else {
                    errorCode = uCxEnd(pUcxHandle);
                }
                if (errorCode == 0) {
                    errorCode = uCxWifiStationSetConnectionParams(pUcxHandle, WLAN_HANDLE, pSsid);
                }
            }
            if ((pPassPhrase != NULL) && (errorCode == 0)) {
                errorCode = uCxWifiStationSetSecurityWpa(pUcxHandle, WLAN_HANDLE, pPassPhrase,
                                                         U_WPA_THRESHOLD_WPA2);
            }
        }
        if (errorCode == 0) {
            errorCode = uCxWifiStationConnect(pUcxHandle, WLAN_HANDLE);
        }
    }
    return errorCode;
}

int32_t uWifiStationDisconnect(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uShortRangePrivateInstance_t *pInstance = pUcxHandle->pAtClient->pConfig->pContext;
        if ((pInstance != NULL) && (pInstance->wifiState == 0)) {
            errorCode = U_WIFI_ERROR_ALREADY_DISCONNECTED;
        } else {
            errorCode = uCxWifiStationDisconnect(pUcxHandle);
            // *** UCX WORKAROUND FIX ***
            // Delay seems needed here
            uPortTaskBlock(5000);
        }
    }
    return errorCode;
}

int32_t uWifiSetHostName(uDeviceHandle_t devHandle, const char *pHostName)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxWifiSetHostname(pUcxHandle, pHostName);
    }
    return errorCode;
}

int32_t uWifiStationStoreConfig(uDeviceHandle_t devHandle, bool erase)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
        if (erase) {
            errorCode = uCxSystemDefaultSettings(pUcxHandle);
        }
        if (errorCode == 0) {
            errorCode = uCxSystemStoreConfiguration(pUcxHandle);
        }
    }
    return errorCode;
}

bool uWifiStationHasStoredConfig(uDeviceHandle_t devHandle)
{
    bool has = false;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        const char *pSsid;
        int32_t errorCode =
            uCxWifiStationGetConnectionParamsBegin(pUcxHandle, WLAN_HANDLE, &pSsid);
        has = errorCode == 0 && strlen(pSsid) > 0;
        uCxEnd(pUcxHandle);
    }
    return has;
}

int32_t uWifiAccessPointStart(uDeviceHandle_t devHandle,
                              const char *pSsid,
                              uWifiAuth_t authentication,
                              const char *pPassPhrase,
                              const char *pIpAddress)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
        if (pSsid != NULL) {
            errorCode = uCxWifiApSetConnectionParams1(pUcxHandle, pSsid);
        }
        if (errorCode == 0) {
            if (authentication == U_WIFI_AUTH_OPEN) {
                errorCode = uCxWifiApSetSecurityOpen(pUcxHandle);
            } else {
                if ((pPassPhrase != NULL) && (errorCode == 0)) {
                    errorCode = uCxWifiApSetSecurityWpa2(pUcxHandle, pPassPhrase, U_WPA_THRESHOLD_WPA2);
                }
            }
            if (errorCode == 0) {
                errorCode = uCxWifiApActivate(pUcxHandle);
            }
        }
    }
    return errorCode;
}

int32_t uWifiAccessPointStop(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxWifiApDeactivate(pUcxHandle);
    }
    return errorCode;
}

int32_t uWifiAccessPointStoreConfig(uDeviceHandle_t devHandle, bool erase)
{
    return uWifiStationStoreConfig(devHandle, erase);
}

bool uWifiAccessPointHasStoredConfig(uDeviceHandle_t devHandle)
{
    bool has = false;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uCxWifiApGetConnectionParams_t params;
        int32_t errorCode = uCxWifiApGetConnectionParamsBegin(pUcxHandle, &params);
        has = errorCode == 0 && strlen(params.ssid) > 0;
    }
    return has;
}

int32_t uWifiStationScan(uDeviceHandle_t devHandle, const char *pSsid,
                         uWifiScanResultCallback_t pCallback)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pUcxHandle != NULL) && (pCallback != NULL)) {
        uCxWifiStationScanDefaultBegin(pUcxHandle);
        uCxWifiStationScanDefault_t uCxresult;
        uWifiScanResult_t result;
        while (uCxWifiStationScanDefaultGetNext(pUcxHandle, &uCxresult)) {
            result.authSuiteBitmask = uCxresult.authentication_suites;
            memcpy(result.bssid, uCxresult.bssid.address, sizeof(result.bssid));
            result.channel = uCxresult.channel;
            result.grpCipherBitmask = uCxresult.group_ciphers;
            result.opMode = U_WIFI_OP_MODE_INFRASTRUCTURE; // Only one available for now
            result.rssi = uCxresult.rssi;
            memset(result.ssid, 0, sizeof(result.ssid));
            strncpy(result.ssid, uCxresult.ssid, sizeof(result.ssid) - 1);
            if ((pSsid == NULL) || (strcmp(pSsid, result.ssid) == 0)) {
                pCallback(devHandle, &result);
            }
        }
        errorCode = uCxEnd(pUcxHandle);
    }
    return errorCode;
}

// End of file
