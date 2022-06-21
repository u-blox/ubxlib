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

#include "u_device_shared.h"

#include "u_network_shared.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"

#include "u_network.h"
#include "u_network_config_wifi.h"
#include "u_network_private_wifi.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC
# define U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC 5
#endif

//lint -esym(767, LOG_TAG) Suppress LOG_TAG defined differently in another module
//lint -esym(750, LOG_TAG) Suppress LOG_TAG not referenced
#define LOG_TAG "U_NETWORK_WIFI: "

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uPortQueueHandle_t getQueueHandle(uDeviceHandle_t devHandle)
{
    uPortQueueHandle_t queueHandle = NULL;
    uDeviceInstance_t *pInstance = U_DEVICE_INSTANCE(devHandle);

    for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                        sizeof(pInstance->networkData[0])); x++) {
        if (pInstance->networkData[x].networkType == U_NETWORK_TYPE_WIFI) {
            queueHandle = (uPortQueueHandle_t) pInstance->networkData[x].pContext;
            break;
        }
    }

    return queueHandle;
}

static void setQueueHandle(uDeviceHandle_t devHandle, uPortQueueHandle_t queueHandle)
{
    uDeviceInstance_t *pInstance = U_DEVICE_INSTANCE(devHandle);

    for (size_t x = 0; (x < sizeof(pInstance->networkData) /
                        sizeof(pInstance->networkData[0])); x++) {
        if (pInstance->networkData[x].networkType == U_NETWORK_TYPE_WIFI) {
            pInstance->networkData[x].pContext = queueHandle;
            break;
        }
    }
}

static void wifiConnectionCallback(uDeviceHandle_t devHandle,
                                   int32_t connId,
                                   int32_t status,
                                   int32_t channel,
                                   char *pBssid,
                                   int32_t disconnectReason,
                                   void *pCallbackParameter)
{
    uDeviceInstance_t *pInstance;
    uDeviceNetworkData_t *pNetworkData;
    uNetworkStatusCallbackData_t *pStatusCallbackData;
    bool isUp;
    uNetworkStatus_t networkStatus;
    uPortQueueHandle_t queueHandle = getQueueHandle(devHandle);

    if ((pCallbackParameter != NULL) && (queueHandle != NULL)) {
        // If pCallbackParameter is not NULL then we're in
        // the bring-up phase and need to pass the information
        // to the queue
        uStatusMessage_t msg = {
            .msgType = (status == U_WIFI_CON_STATUS_DISCONNECTED) ? U_MSG_WIFI_DISCONNECT : U_MSG_WIFI_CONNECT,
            .disconnectReason = disconnectReason,
            .netStatusMask = 0
        };
        uPortQueueSend(queueHandle, &msg);
    }

    // Note: can't lock the device API here since we may collide
    // with a network up/down call that will have already locked
    // it and then may, internally, be waiting on something to pass
    // up the event queue that we are currently blocking (since
    // the same event queue is used for most things).
    // We rely on the fact that the various network down calls
    // are well behaved and will not pull the rug out from under
    // one of their callbacks.
    if (uDeviceGetInstance(devHandle, &pInstance) == 0) {
        pNetworkData = pUNetworkGetNetworkData(pInstance, U_NETWORK_TYPE_WIFI);
        if (pNetworkData != NULL) {
            pStatusCallbackData = (uNetworkStatusCallbackData_t *) pNetworkData->pStatusCallbackData;
            if ((pStatusCallbackData != NULL) &&
                (pStatusCallbackData->pCallback)) {
                networkStatus.wifi.pBssid = NULL;
                isUp = (status == (int32_t) U_WIFI_CON_STATUS_CONNECTED);
                networkStatus.wifi.connId = connId;
                networkStatus.wifi.status = status;
                networkStatus.wifi.channel = channel;
                if (isUp) {
                    networkStatus.wifi.pBssid = pBssid;
                }
                networkStatus.wifi.disconnectReason = disconnectReason;
                pStatusCallbackData->pCallback(devHandle, U_NETWORK_TYPE_WIFI,
                                               isUp, &networkStatus,
                                               pStatusCallbackData->pCallbackParameter);
            }
        }
    }

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
        if ((disconnectReason < 0) || (disconnectReason >= 6)) {
            // For all other values use "Unknown"
            //lint -esym(438, disconnectReason)
            disconnectReason = 0;
        }
        uPortLog(LOG_TAG "Wifi connection lost connId: %d, reason: %d (%s)\n",
                 connId,
                 disconnectReason,
                 strDisconnectReason[disconnectReason]);
    }
    if (queueHandle == NULL) {
        uPortLog(LOG_TAG "[no-one to tell].\n");
    }
#endif
}

static void wifiNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      int32_t interfaceType,
                                      uint32_t statusMask,
                                      void *pCallbackParameter)
{
    (void)devHandle;
    (void)interfaceType;
    (void)pCallbackParameter;
    uPortQueueHandle_t queueHandle = getQueueHandle(devHandle);

#if !U_CFG_OS_CLIB_LEAKS
    uPortLog(LOG_TAG "Network status IPv4 %s, IPv6 %s\n",
             ((statusMask & U_WIFI_STATUS_MASK_IPV4_UP) > 0) ? "up" : "down",
             ((statusMask & U_WIFI_STATUS_MASK_IPV6_UP) > 0) ? "up" : "down");
    if (queueHandle == NULL) {
        uPortLog(LOG_TAG "[no-one to tell].\n");
    }
#endif

    uStatusMessage_t msg = {
        .msgType = U_MSG_NET_STATUS,
        .disconnectReason = 0,
        .netStatusMask = statusMask
    };
    if (queueHandle != NULL) {
        uPortQueueSend(queueHandle, &msg);
    }
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

// Bring a Wifi interface up or take it down.
int32_t uNetworkPrivateChangeStateWifi(uDeviceHandle_t devHandle,
                                       const uNetworkCfgWifi_t *pCfg,
                                       bool upNotDown)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    uDeviceInstance_t *pDevInstance;
    errorCode = uDeviceGetInstance(devHandle, &pDevInstance);
    if (errorCode != 0) {
        return errorCode;
    }
    if ((pCfg == NULL) || (pCfg->version != 0) || (pCfg->type != U_NETWORK_TYPE_WIFI)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    // Callback message queue, may or may not be initiated before
    uPortQueueHandle_t queueHandle = getQueueHandle(devHandle);
    if (upNotDown) {
        if (!queueHandle) {
            errorCode = uPortQueueCreate(2, sizeof(uStatusMessage_t), &queueHandle);
            if (errorCode == 0) {
                setQueueHandle(devHandle, queueHandle);
                // We pass the devHandle as the parameter to pass to
                // wifiConnectionCallback() as a kind of "flag" so that
                // it knows it is in the bring-up phase
                errorCode = uWifiSetConnectionStatusCallback(devHandle,
                                                             wifiConnectionCallback,
                                                             devHandle);
                if (errorCode == 0) {
                    errorCode = uWifiSetNetworkStatusCallback(devHandle,
                                                              wifiNetworkStatusCallback,
                                                              NULL);
                }
            }
        }
        if (errorCode == 0) {
            // Clear status queue since we are only interested in fresh messages
            statusQueueClear(queueHandle);

            errorCode = uWifiStationConnect(devHandle,
                                            pCfg->pSsid,
                                            (uWifiAuth_t)pCfg->authentication,
                                            pCfg->pPassPhrase);
            if (errorCode == 0) {
                // Wait until the network layer is up before return
                errorCode = statusQueueWaitForWifiConnected(queueHandle, 20);
                if (errorCode == 0) {
                    errorCode = statusQueueWaitForNetworkUp(queueHandle,
                                                            U_NETWORK_PRIVATE_WIFI_NETWORK_TIMEOUT_SEC);
                }
            }

            if (errorCode == (int32_t)U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID) {
                // If we already are connected to the SSID we return success
                errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            }

            if (errorCode != 0) {
                // Something went wrong, clean up.
                // Use a recursive call here for convenience.
                // This is without risk for a loop as the upNotDown
                // parameter is false and hence this line can not
                // be reached in the call.
                uNetworkPrivateChangeStateWifi(devHandle, pCfg, false);
            } else {
                // Set up the network status callback with a NULL parameter
                // so that it knows it is outside the bring-up phase
                uWifiSetConnectionStatusCallback(devHandle, wifiConnectionCallback, NULL);
            }

            uWifiSetNetworkStatusCallback(devHandle, NULL, NULL);
        }
    } else {
        // If queueHandle is NULL then the network was never brought up,
        // hence success
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (queueHandle) {
            statusQueueClear(queueHandle);

            // We pass the devHandle as the parameter to pass to
            // wifiConnectionCallback() as a kind of "flag" so that
            // it knows it is in the bring-up phase
            errorCode = uWifiSetConnectionStatusCallback(devHandle,
                                                         wifiConnectionCallback,
                                                         devHandle);
            if (errorCode == 0) {
                errorCode = uWifiStationDisconnect(devHandle);
                uPortLog("uWifiStationDisconnect: %d\n", errorCode);
            }

            if (errorCode == 0) {
                // Wait until the wifi have been disabled before return
                errorCode = statusQueueWaitForWifiDisabled(queueHandle, 5);
            }

            if (errorCode == (int32_t)U_WIFI_ERROR_ALREADY_DISCONNECTED) {
                // If we already are disconnected we return success
                errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
            }
            // We remove the callback and clear the queue handle
            // whether the above succeeds or not since the system
            // may be going down and we don't want a queue to be
            // left hanging; there is no other clean-up mechanism
            uWifiSetConnectionStatusCallback(devHandle, NULL, NULL);
            uPortQueueDelete(queueHandle);
            setQueueHandle(devHandle, NULL);
        }
    }

    return errorCode;
}
// Set a call-back to be called when the Wifi network status changes.
int32_t uNetworkSetStatusCallbackWifi(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    // Nothing to do: since uWifiSetConnectionStatusCallback() gets the
    // device handle it already has all it needs.
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// End of file
