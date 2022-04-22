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

#include "u_wifi.h"

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
#define U_WIFI_NET_CON_STATUS_DISCONNECTED 0
#define U_WIFI_NET_CON_STATUS_CONNECTED    1

/** Wifi disconnect reason codes used by uWifiNetConnectionStatusCallback_t */
#define U_WIFI_NET_REASON_UNKNOWN          0
#define U_WIFI_NET_REASON_REMOTE_CLOSE     1
#define U_WIFI_NET_REASON_OUT_OF_RANGE     2
#define U_WIFI_NET_REASON_ROAMING          3
#define U_WIFI_NET_REASON_SECURITY_PROBLEM 4
#define U_WIFI_NET_REASON_NETWORK_DISABLED 5

/** Status bits used by uWifiNetIpStatusCallback_t */
#define U_WIFI_NET_STATUS_MASK_IPV4_UP     (1 << 0) /**< When this bit is set IPv4 network is up */
#define U_WIFI_NET_STATUS_MASK_IPV6_UP     (1 << 1) /**< When this bit is set IPv6 network is up */

/** uWifiNetScanResult_t values for .opMode */
#define U_WIFI_NET_OP_MODE_INFRASTRUCTURE  1
#define U_WIFI_NET_OP_MODE_ADHOC           2

/** uWifiNetScanResult_t values for .authSuiteBitmask */
#define U_WIFI_NET_AUTH_MASK_SHARED_SECRET (1 << 0)
#define U_WIFI_NET_AUTH_MASK_PSK           (1 << 1)
#define U_WIFI_NET_AUTH_MASK_EAP           (1 << 2)
#define U_WIFI_NET_AUTH_MASK_WPA           (1 << 3)
#define U_WIFI_NET_AUTH_MASK_WPA2          (1 << 4)
#define U_WIFI_NET_AUTH_MASK_WPA3          (1 << 5)

/** uWifiNetScanResult_t values for .uniCipherBitmask and .grpCipherBitmask */
#define U_WIFI_NET_CIPHER_MASK_WEP64       (1 << 0)
#define U_WIFI_NET_CIPHER_MASK_WEP128      (1 << 1)
#define U_WIFI_NET_CIPHER_MASK_TKIP        (1 << 2)
#define U_WIFI_NET_CIPHER_MASK_AES_CCMP    (1 << 3)
#define U_WIFI_NET_CIPHER_MASK_UNKNOWN     0xFF /**< This will be the value for modules that doesn't support cipher masks */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef enum {
    U_WIFI_NET_AUTH_OPEN = 1, /**< No authentication mode */
    U_WIFI_NET_AUTH_WPA_PSK = 2, /**< WPA/WPA2/WPA3 psk authentication mode. */
} uWifiNetAuth_t;

typedef struct {
    uint8_t bssid[U_WIFI_BSSID_SIZE]; /**< BSSID of the AP in binary format */
    char ssid[U_WIFI_SSID_SIZE];      /**< Null terminated SSID string */
    int32_t channel;            /**< WiFi channel number */
    int32_t opMode;             /**< Operation mode, see U_WIFI_OP_MODE_xxx defines for values */
    int32_t rssi;               /**< Received signal strength indication */
    uint32_t authSuiteBitmask;  /**< Authentication bitmask, see U_NET_WIFI_AUTH_MASK_xx defines for values */
    uint8_t uniCipherBitmask;   /**< Unicast cipher bitmask, see U_NET_WIFI_CIPHER_MASK_xx defines for values */
    uint8_t grpCipherBitmask;   /**< Group cipher bitmask, see U_NET_WIFI_CIPHER_MASK_xx defines for values */
} uWifiNetScanResult_t;


/** Scan result callback type
 *
 * This callback will be called once for each entry found.
 *
 * @param devHandle          the handle of the wifi instance.
 * @param pResult            the scan result.
 * @param pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uWifiNetScanResultCallback_t) (uDeviceHandle_t devHandle,
                                              uWifiNetScanResult_t *pResult);


/** Connection status callback type
 *
 * @param devHandle          the handle of the wifi instance.
 * @param connId             connection ID.
 * @param status             new status of connection. Please see U_WIFI_NET_CON_STATUS_xx.
 * @param channel            wifi channel.
 *                           Note: Only valid for U_WIFI_NET_STATUS_CONNECTED otherwise set to 0.
 * @param pBssid             remote AP BSSID as null terminated string.
 *                           Note: Only valid for U_WIFI_NET_STATUS_CONNECTED otherwise set to NULL.
 * @param disconnectReason   disconnect reason. Please see U_WIFI_NET_REASON_xx.
 *                           Note: Only valid for U_WIFI_NET_STATUS_DISCONNECTED otherwise set to 0.
 * @param pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uWifiNetConnectionStatusCallback_t) (uDeviceHandle_t devHandle,
                                                    int32_t connId,
                                                    int32_t status,
                                                    int32_t channel,
                                                    char *pBssid,
                                                    int32_t disconnectReason,
                                                    void *pCallbackParameter);


/** Network status callback type
 *
 * @param devHandle          the handle of the wifi instance.
 * @param interfaceType      interface type. Only 1: Wifi Station supported at the moment.
 * @param statusMask         bitmask indicating the new status. Please see defined bits
 *                           U_WIFI_NET_STATUS_MASK_xx.
 * @param pCallbackParameter Parameter pointer set when registering callback.
 */
typedef void (*uWifiNetNetworkStatusCallback_t) (uDeviceHandle_t devHandle,
                                                 int32_t interfaceType,
                                                 uint32_t statusMask,
                                                 void *pCallbackParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Connect to a Wifi access point
 *
 * @param devHandle        the handle of the wifi instance.
 * @param pSsid            the Service Set Identifier
 * @param authentication   the authentication type
 * @param[in] pPassPhrase  the Passphrase (8-63ASCII characters as a string) for WPA/WPA2/WPA3
 * @return                 zero on successful, else negative error code.
 *                         Note: There is no actual connection until the Wifi callback reports
 *                         connected.
 */
int32_t uWifiNetStationConnect(uDeviceHandle_t devHandle, const char *pSsid,
                               uWifiNetAuth_t authentication, const char *pPassPhrase);

/** Disconnect from Wifi access point
 *
 * @param devHandle   the handle of the wifi instance.
 * @return            zero on successful, else negative error code.
 *                    Note: The disconnection is not completed until the Wifi callback
 *                    reports disconnected.
 */
int32_t uWifiNetStationDisconnect(uDeviceHandle_t devHandle);

/** Set a callback for Wifi connection status.
  *
 * @param devHandle              the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiNetSetConnectionStatusCallback(uDeviceHandle_t devHandle,
                                            uWifiNetConnectionStatusCallback_t pCallback,
                                            void *pCallbackParameter);
/** Set a callback for network status.
 *
 * @param devHandle              the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiNetSetNetworkStatusCallback(uDeviceHandle_t devHandle,
                                         uWifiNetNetworkStatusCallback_t pCallback,
                                         void *pCallbackParameter);


/** Scan for SSIDs
 *
 * Please note that this function will block until the scan process is completed.
 * During this time pCallback will be called for each scan result entry found.
 *
 * @param devHandle        the handle of the wifi instance.
 * @param[in] pSsid        optional SSID to search for. Set to NULL to search for any SSID.
 * @param[in] pCallback    callback for handling a scan result entry.
 *                         IMPORTANT: The callback will be called while the AT lock is held.
 *                                    This means that you are not allowed to call other u-blox
 *                                    module APIs directly from this callback.
 * @return                 zero on successful, else negative error code.
 */
int32_t uWifiNetStationScan(uDeviceHandle_t devHandle, const char *pSsid,
                            uWifiNetScanResultCallback_t pCallback);

#ifdef __cplusplus
}
#endif

#endif // _U_WIFI_NET_H_

// End of file
