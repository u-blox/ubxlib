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

#ifndef _U_WIFI_NET_H_
#define _U_WIFI_NET_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the APIs that obtain data transfer
 * related commands for ble using the sps protocol.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Wifi connection status codes used by uWifiNetConnectionStatusCallback_t */
#define U_WIFI_CON_STATUS_DISCONNECTED 0
#define U_WIFI_CON_STATUS_CONNECTED    1

/** Wifi disconnect reason codes used by uWifiNetConnectionStatusCallback_t */
#define U_WIFI_REASON_UNKNOWN          0
#define U_WIFI_REASON_REMOTE_CLOSE     1
#define U_WIFI_REASON_OUT_OF_RANGE     2
#define U_WIFI_REASON_ROAMING          3
#define U_WIFI_REASON_SECURITY_PROBLEM 4
#define U_WIFI_REASON_NETWORK_DISABLED 5

/** Status bits used by uWifiNetIpStatusCallback_t */
#define U_WIFI_STATUS_MASK_IPV4_UP 0x01 /**< When this bit is set IPv4 network is up */
#define U_WIFI_STATUS_MASK_IPV6_UP 0x02 /**< When this bit is set IPv6 network is up */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef enum {
    U_SHORT_RANGE_WIFI_AUTH_OPEN = 1, /**< No authentication mode */
    U_SHORT_RANGE_WIFI_AUTH_WPA_PSK = 2, /**< WPA/WPA2/WPA3 psk authentication mode. */
} uWifiNetAuth_t;


/** Connection status callback type
 *
 * @param wifiHandle         the handle of the wifi instance.
 * @param connId             connection ID.
 * @param status             new status of connection. Please see U_WIFI_CON_STATUS_xx.
 * @param channel            wifi channel.
 *                           Note: Only valid for U_WIFI_STATUS_CONNECTED otherwise set to 0.
 * @param pBssid             remote AP BSSID as null terminated string.
 *                           Note: Only valid for U_WIFI_STATUS_CONNECTED otherwise set to NULL.
 * @param disconnectReason   disconnect reason. Please see U_WIFI_REASON_xx.
 *                           Note: Only valid for U_WIFI_STATUS_DISCONNECTED otherwise set to 0.
 * @param pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uWifiNetConnectionStatusCallback_t) (int32_t wifiHandle,
                                                    int32_t connId,
                                                    int32_t status,
                                                    int32_t channel,
                                                    char *pBssid,
                                                    int32_t disconnectReason,
                                                    void *pCallbackParameter);


/** Network status callback type
 *
 * @param wifiHandle         the handle of the wifi instance.
 * @param interfaceType      interface type. Only 1: Wifi Station supported at the moment.
 * @param statusMask         bitmask indicating the new status. Please see defined bits
 *                           U_WIFI_STATUS_MASK_xx.
 * @param pCallbackParameter Parameter pointer set when registering callback.
 */
typedef void (*uWifiNetNetworkStatusCallback_t) (int32_t wifiHandle,
                                                 int32_t interfaceType,
                                                 uint32_t statusMask,
                                                 void *pCallbackParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Connect to a Wifi access point
 *
 * @param wifiHandle       the handle of the wifi instance.
 * @param pSsid            the Service Set Identifier
 * @param authentication   the authentication type
 * @param[in] pPassPhrase  the Passphrase (8-63ASCII characters as a string) for WPA/WPA2/WPA3
 * @return                 zero on successful, else negative error code.
 *                         Note: There is no actual connection until the Wifi callback reports
 *                         connected.
 */
int32_t uWifiNetStationConnect(int32_t wifiHandle, const char *pSsid, uWifiNetAuth_t authentication,
                               const char *pPassPhrase);

/** Disconnect from Wifi access point
 *
 * @param wifiHandle  the handle of the wifi instance.
 * @return            zero on successful, else negative error code.
 *                    Note: The disconnection is not completed until the Wifi callback
 *                    reports disconnected.
 */
int32_t uWifiNetStationDisconnect(int32_t wifiHandle);

/** Set a callback for Wifi connection status.
  *
 * @param wifiHandle             the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiNetSetConnectionStatusCallback(int32_t wifiHandle,
                                            uWifiNetConnectionStatusCallback_t pCallback,
                                            void *pCallbackParameter);
/** Set a callback for network status.
  *
 * @param wifiHandle             the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiNetSetNetworkStatusCallback(int32_t wifiHandle,
                                         uWifiNetNetworkStatusCallback_t pCallback,
                                         void *pCallbackParameter);




#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_NET_H_

// End of file
