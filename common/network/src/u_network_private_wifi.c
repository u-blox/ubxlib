/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the Wifi portion of the network API.
 * The contents of this file aren't any more "private" than the
 * other sources files but the associated header file should be
 * private and this is simply named to match.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "u_cfg_sw.h"

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"
#include "u_wifi_net.h"

#include "u_network.h"
#include "u_network_private_short_range.h"
#include "u_network_config_wifi.h"
#include "u_network_private_wifi.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_WIFI_MAX_NUM
# define U_NETWORK_PRIVATE_WIFI_MAX_NUM 1
#endif

#ifndef U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC
# define U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC 5
#endif

//lint -esym(767, LOG_TAG) Suppress LOG_TAG defined differently in another module
//lint -esym(750, LOG_TAG) Suppress LOG_TAG not referenced
#define LOG_TAG "U_NETWORK_WIFI: "

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    int32_t netShoHandle;    /**< The handle returned by uNetworkAddShortRange(). */
    int32_t wifiHandle;      /**< The handle returned by uWifiAdd(). */
    uPortQueueHandle_t statusQueue; /**< Message queue used for wifi connection
                                         and network status events. */
} uNetworkPrivateWifiInstance_t;

typedef enum {
    U_MSG_WIFI_CONNECT,
    U_MSG_WIFI_DISCONNECT,
    U_MSG_NET_STATUS
} uStatusMessageType_t;

typedef struct {
    uStatusMessageType_t msgType;
    int32_t disconnectReason;
    uint32_t netStatusMask;
} uStatusMessage_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to keep track of the instances.
 */
static uNetworkPrivateWifiInstance_t gInstance[U_NETWORK_PRIVATE_WIFI_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uNetworkPrivateWifiInstance_t *pGetFree()
{
    uNetworkPrivateWifiInstance_t *pFree = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pFree == NULL); x++) {
        if (gInstance[x].netShoHandle < 0) {
            pFree = &(gInstance[x]);
        }
    }

    return pFree;
}

// Find the given instance in the list.
static uNetworkPrivateWifiInstance_t *pGetInstance(int32_t wifiHandle)
{
    uNetworkPrivateWifiInstance_t *pInstance = NULL;

    // Find the handle in the list
    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pInstance == NULL); x++) {
        if (gInstance[x].wifiHandle == wifiHandle) {
            pInstance = &(gInstance[x]);
        }
    }

    return pInstance;
}

static void clearInstance(uNetworkPrivateWifiInstance_t *pInstance)
{
    pInstance->wifiHandle = -1;
    pInstance->netShoHandle = -1;
    pInstance->statusQueue = NULL;
}

// Parse a int32_t value to uWifiNetAuth_t
// Returns 0 on success, -1 on failure
static int32_t parseAuthentication(int32_t value, uWifiNetAuth_t *pDstAuth)
{
    switch (value) {
        case (int32_t)U_SHORT_RANGE_WIFI_AUTH_OPEN:
            *pDstAuth = U_SHORT_RANGE_WIFI_AUTH_OPEN;
            break;

        case (int32_t)U_SHORT_RANGE_WIFI_AUTH_WPA_PSK:
            *pDstAuth = U_SHORT_RANGE_WIFI_AUTH_WPA_PSK;
            break;

        default:
            return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

static void wifiConnectionCallback(int32_t wifiHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    (void)wifiHandle;
    (void)connId;
    (void)channel;
    (void)pBssid;
    uNetworkPrivateWifiInstance_t *pInstance;
    pInstance = (uNetworkPrivateWifiInstance_t *)pCallbackParameter;

    uStatusMessage_t msg = {
        .msgType = (status == U_WIFI_CON_STATUS_DISCONNECTED) ? U_MSG_WIFI_DISCONNECT : U_MSG_WIFI_CONNECT,
        .disconnectReason = disconnectReason,
        .netStatusMask = 0
    };
    // We don't care if the queue gets full here
    (void)uPortQueueSend(pInstance->statusQueue, &msg);

#if defined(U_CFG_ENABLE_LOGGING) && !U_CFG_OS_CLIB_LEAKS
    if (status == U_WIFI_CON_STATUS_CONNECTED) {
        uPortLog(LOG_TAG "Wifi connected connId: %d, bssid: %s, channel: %d\n",
                 connId,
                 pBssid,
                 channel);
    } else {
        //lint -esym(752, strDisconnectReason)
        static const char strDisconnectReason[6][20] = {
            "Unknown", "Remote Close", "Out of range",
            "Roaming", "Security problems", "Network disabled"
        };
        if ((disconnectReason < 0) && (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            //lint -esym(438, disconnectReason)
            disconnectReason = 0;
        }
        uPortLog(LOG_TAG "Wifi connection lost connId: %d, reason: %d (%s)\n",
                 connId,
                 disconnectReason,
                 strDisconnectReason[disconnectReason]);
    }
#endif
}

static void wifiNetworkStatusCallback(int32_t wifiHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)wifiHandle;
    (void)interfaceType;
    uNetworkPrivateWifiInstance_t *pInstance;
    pInstance = (uNetworkPrivateWifiInstance_t *)pCallbackParameter;

#if !U_CFG_OS_CLIB_LEAKS
    uPortLog(LOG_TAG "Network status IPv4 %s, IPv6 %s\n",
             ((statusMask & U_WIFI_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
             ((statusMask & U_WIFI_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");
#endif

    uStatusMessage_t msg = {
        .msgType = U_MSG_NET_STATUS,
        .disconnectReason = 0,
        .netStatusMask = statusMask
    };
    // We don't care if the queue gets full here
    (void)uPortQueueSend(pInstance->statusQueue, &msg);
}

static inline void statusQueueClear(const uPortQueueHandle_t queueHandle)
{
    int32_t result;
    do {
        uStatusMessage_t msg;
        result = uPortQueueTryReceive(queueHandle, 0, &msg);
    } while (result == (int32_t) U_ERROR_COMMON_SUCCESS);
}

static inline int32_t statusQueueWaitForWifiDisabled(const uPortQueueHandle_t queueHandle,
                                                     int32_t timeoutSec)
{
    int32_t startTime = (int32_t)uPortGetTickTimeMs();
    while ((int32_t)uPortGetTickTimeMs() - startTime < timeoutSec * 1000) {
        uStatusMessage_t msg;
        int32_t errorCode = uPortQueueTryReceive(queueHandle, 1000, &msg);
        if ((errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) &&
            (msg.msgType == U_MSG_WIFI_DISCONNECT) &&
            (msg.disconnectReason == U_WIFI_REASON_NETWORK_DISABLED)) {
            return (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t) U_ERROR_COMMON_TIMEOUT;
}

static inline int32_t statusQueueWaitForWifiConnected(const uPortQueueHandle_t queueHandle,
                                                      int32_t timeoutSec)
{
    int32_t startTime = (int32_t)uPortGetTickTimeMs();
    while ((int32_t)uPortGetTickTimeMs() - startTime < timeoutSec * 1000) {
        uStatusMessage_t msg;
        int32_t errorCode = uPortQueueTryReceive(queueHandle, 1000, &msg);
        if ((errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) &&
            (msg.msgType == U_MSG_WIFI_CONNECT)) {
            return (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }
    return (int32_t) U_ERROR_COMMON_TIMEOUT;
}


static inline int32_t statusQueueWaitForNetworkUp(const uPortQueueHandle_t queueHandle,
                                                  int32_t timeoutSec)
{
    static const uint32_t desiredNetStatusMask =
        U_WIFI_STATUS_MASK_IPV4_UP | U_WIFI_STATUS_MASK_IPV6_UP;
    uint32_t lastNetStatusMask = 0;
    int32_t startTime = (int32_t)uPortGetTickTimeMs();
    while ((int32_t)uPortGetTickTimeMs() - startTime < timeoutSec * 1000) {
        uStatusMessage_t msg;
        int32_t errorCode = uPortQueueTryReceive(queueHandle, 1000, &msg);
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            switch (msg.msgType) {
                case U_MSG_NET_STATUS:
                    lastNetStatusMask = msg.netStatusMask;
                    if (msg.netStatusMask == desiredNetStatusMask) {
                        // We are done waiting!
                        return (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                    break;

                case U_MSG_WIFI_DISCONNECT:
                    if (msg.disconnectReason != U_WIFI_REASON_NETWORK_DISABLED) {
                        return (int32_t) U_ERROR_COMMON_TEMPORARY_FAILURE;
                    }
                    break;

                default:
                    // Ignore
                    break;
            }
        }
    }
    if ((lastNetStatusMask & desiredNetStatusMask) > 0) {
        // If one of the network protocol is up we
        // return without failure since this could
        // be only a missconfiguration
        uPortLog("Warning: A network protocol failed");
        return (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return (int32_t) U_ERROR_COMMON_TIMEOUT;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for Wifi.
int32_t uNetworkInitWifi(void)
{
    int32_t errorCode = uNetworkInitShortRange();
    if (errorCode >= 0) {
        errorCode = uWifiInit();
    }

    for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
        clearInstance(&gInstance[x]);
    }

    return errorCode;
}

// Deinitialise the Wifi network API.
void uNetworkDeinitWifi(void)
{
    uWifiDeinit();
    uNetworkDeinitShortRange();
}

// Add a Wifi network instance.
int32_t uNetworkAddWifi(const uNetworkConfigurationWifi_t *pConfiguration)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uNetworkPrivateWifiInstance_t *pInstance;
    const uShortRangeConfig_t shoConfig = {
        .module = pConfiguration->module,
        .uart = pConfiguration->uart,
        .pinTxd = pConfiguration->pinTxd,
        .pinRxd = pConfiguration->pinRxd,
        .pinCts = pConfiguration->pinCts,
        .pinRts = pConfiguration->pinRts
    };

    // Check that the module supports Wifi
    const uShortRangeModuleInfo_t *pModuleInfo;
    pModuleInfo = uShortRangeGetModuleInfo(pConfiguration->module);
    if (!pModuleInfo) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (!pModuleInfo->supportsWifi) {
        return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    }

    pInstance = pGetFree();
    if (pInstance != NULL) {
        errorCode = uNetworkAddShortRange(&shoConfig);
        pInstance->netShoHandle = errorCode;

        if (errorCode >= 0) {
            uAtClientHandle_t atHandle = uNetworkGetAtClientShortRange(pInstance->netShoHandle);
            errorCode = uWifiAdd((uWifiModuleType_t) pConfiguration->module, atHandle);
            pInstance->wifiHandle = errorCode;
        }

        if (errorCode >= 0) {
            errorCode = uPortQueueCreate(2, sizeof(uStatusMessage_t), &pInstance->statusQueue);
        }

        if (errorCode >= 0) {
            errorCode = uWifiNetSetConnectionStatusCallback(pInstance->wifiHandle,
                                                            wifiConnectionCallback,
                                                            pInstance);
        }

        if (errorCode >= 0) {
            errorCode = pInstance->wifiHandle;
        } else {
            // Something went wrong - cleanup...
            if (pInstance->statusQueue) {
                uPortQueueDelete(pInstance->statusQueue);
                pInstance->statusQueue = NULL;
            }

            if (pInstance->wifiHandle >= 0) {
                uWifiRemove(pInstance->wifiHandle);
            }

            if (pInstance->netShoHandle >= 0) {
                uNetworkRemoveShortRange(pInstance->netShoHandle);
            }
            clearInstance(pInstance);
        }
    }

    return errorCode;
}

// Remove a Wifi network instance.
int32_t uNetworkRemoveWifi(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateWifiInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL) {
        uWifiNetSetConnectionStatusCallback(pInstance->wifiHandle, NULL, NULL);
        uPortQueueDelete(pInstance->statusQueue);
        pInstance->statusQueue = NULL;
        uWifiRemove(pInstance->wifiHandle);
        uNetworkRemoveShortRange(pInstance->netShoHandle);
        clearInstance(pInstance);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Bring up the given Wifi network instance.
int32_t uNetworkUpWifi(int32_t handle,
                       const uNetworkConfigurationWifi_t *pConfiguration)
{
    int32_t errorCode;
    uWifiNetAuth_t auth;
    uNetworkPrivateWifiInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    errorCode = parseAuthentication(pConfiguration->authentication, &auth);
    if ((pInstance != NULL) && (pInstance->wifiHandle >= 0) &&
        (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS)) {

        // Clear status queue since we are only interrested in fresh messages
        statusQueueClear(pInstance->statusQueue);

        errorCode = uWifiNetSetNetworkStatusCallback(pInstance->wifiHandle,
                                                     wifiNetworkStatusCallback,
                                                     pInstance);

        if (errorCode >= 0) {
            errorCode = uWifiNetStationConnect(pInstance->wifiHandle,
                                               pConfiguration->pSsid,
                                               (uWifiNetAuth_t) pConfiguration->authentication,
                                               pConfiguration->pPassPhrase);
        }

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Wait until the network layer is up before return
            errorCode = statusQueueWaitForWifiConnected(pInstance->statusQueue, 20);
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                errorCode = statusQueueWaitForNetworkUp(pInstance->statusQueue,
                                                        U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC);
            }
            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                uNetworkDownWifi(handle, pConfiguration);
            }
        }

        if (errorCode == (int32_t) U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID) {
            // This is mainly used for test system:
            // If we already are connected to the SSID we return success
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        uWifiNetSetNetworkStatusCallback(pInstance->wifiHandle, NULL, NULL);
    }

    return errorCode;
}

// Take down the given Wifi network instance.
int32_t uNetworkDownWifi(int32_t handle,
                         const uNetworkConfigurationWifi_t *pConfiguration)
{
    (void) pConfiguration;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateWifiInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pGetInstance(handle);
    if (pInstance != NULL && pInstance->wifiHandle >= 0) {
        // Clear status queue since we are only interrested in fresh messages
        statusQueueClear(pInstance->statusQueue);

        errorCode = uWifiNetStationDisconnect(pInstance->wifiHandle);
        uPortLog("uWifiNetStationDisconnect: %d\n", errorCode);

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Wait until the wifi have been disabled before return
            errorCode = statusQueueWaitForWifiDisabled(pInstance->statusQueue, 5);
        }

        if (errorCode == (int32_t) U_WIFI_ERROR_ALREADY_DISCONNECTED) {
            // This is mainly used for test system:
            // If we already are disconnected we return success
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// End of file
