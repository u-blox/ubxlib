/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicawifi law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_WIFI_H_
#define _U_WIFI_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _wifi _Wifi
 *  @{
 */

/** @file
 * @brief This header file defines the general wifi APIs,
 * basically initialise and deinitialise.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_WIFI_BSSID_SIZE 6        /**< binary BSSID size. */
#define U_WIFI_SSID_SIZE (32 + 1)  /**< null-terminated SSID string size. */

/** Wifi connection status codes used by #uWifiConnectionStatusCallback_t */
#define U_WIFI_CON_STATUS_DISCONNECTED 0
#define U_WIFI_CON_STATUS_CONNECTED    1

/** Wifi disconnect reason codes used by #uWifiConnectionStatusCallback_t */
#define U_WIFI_REASON_UNKNOWN          0
#define U_WIFI_REASON_REMOTE_CLOSE     1
#define U_WIFI_REASON_OUT_OF_RANGE     2
#define U_WIFI_REASON_ROAMING          3
#define U_WIFI_REASON_SECURITY_PROBLEM 4
#define U_WIFI_REASON_NETWORK_DISABLED 5

/** Status bits used by #uWifiNetworkStatusCallback_t */
#define U_WIFI_STATUS_MASK_IPV4_UP     (1 << 0) /**< When this bit is set IPv4 network is up */
#define U_WIFI_STATUS_MASK_IPV6_UP     (1 << 1) /**< When this bit is set IPv6 network is up */

/** #uWifiScanResult_t values for .opMode */
#define U_WIFI_OP_MODE_INFRASTRUCTURE  1
#define U_WIFI_OP_MODE_ADHOC           2

/** #uWifiScanResult_t values for .authSuiteBitmask */
#define U_WIFI_AUTH_MASK_SHARED_SECRET (1 << 0)
#define U_WIFI_AUTH_MASK_PSK           (1 << 1)
#define U_WIFI_AUTH_MASK_EAP           (1 << 2)
#define U_WIFI_AUTH_MASK_WPA           (1 << 3)
#define U_WIFI_AUTH_MASK_WPA2          (1 << 4)
#define U_WIFI_AUTH_MASK_WPA3          (1 << 5)

/** #uWifiScanResult_t values for .uniCipherBitmask and .grpCipherBitmask */
#define U_WIFI_CIPHER_MASK_WEP64       (1 << 0)
#define U_WIFI_CIPHER_MASK_WEP128      (1 << 1)
#define U_WIFI_CIPHER_MASK_TKIP        (1 << 2)
#define U_WIFI_CIPHER_MASK_AES_CCMP    (1 << 3)
#define U_WIFI_CIPHER_MASK_UNKNOWN     0xFF /**< This will be the value for modules that doesn't support cipher masks */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to wifi.
 */
typedef enum {
    U_WIFI_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< Force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_WIFI_ERROR_AT = U_ERROR_WIFI_MAX,      /**< -512 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_NOT_CONFIGURED = U_ERROR_WIFI_MAX - 1, /**< -511 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_NOT_FOUND = U_ERROR_WIFI_MAX - 2,  /**< -510 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_INVALID_MODE = U_ERROR_WIFI_MAX - 3,  /**< -509 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_TEMPORARY_FAILURE = U_ERROR_WIFI_MAX - 4,  /**< -508 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_CONNECTED = U_ERROR_WIFI_MAX - 5,  /**< -507 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_CONNECTED_TO_SSID = U_ERROR_WIFI_MAX - 6,  /**< -506 if #U_ERROR_BASE is 0. */
    U_WIFI_ERROR_ALREADY_DISCONNECTED = U_ERROR_WIFI_MAX - 7  /**< -505 if #U_ERROR_BASE is 0. */
} uWifiErrorCode_t;

typedef enum {
    U_WIFI_AUTH_OPEN = 1,          /**< no authentication mode. */
    U_WIFI_AUTH_WPA_PSK = 2,       /**< WPA/WPA2/WPA3 psk authentication mode. */
    U_WIFI_AUTH_WPA2_WPA3_PSK = 6, /**< WPA2/WPA3 psk authentication mode. */
    U_WIFI_AUTH_WPA3_PSK = 7,      /**< WPA3 psk authentication mode. */
} uWifiAuth_t;

typedef struct {
    uint8_t bssid[U_WIFI_BSSID_SIZE]; /**< BSSID of the AP in binary format. */
    char ssid[U_WIFI_SSID_SIZE];      /**< null-terminated SSID string. */
    int32_t channel;            /**< WiFi channel number. */
    int32_t opMode;             /**< operation mode, see U_WIFI_OP_MODE_xxx defines for values. */
    int32_t rssi;               /**< received signal strength indication. */
    uint32_t authSuiteBitmask;  /**< authentication bitmask, see U_WIFI_AUTH_MASK_xx defines for values. */
    uint8_t uniCipherBitmask;   /**< unicast cipher bitmask, see U_WIFI_CIPHER_MASK_xx defines for values. */
    uint8_t grpCipherBitmask;   /**< group cipher bitmask, see U_WIFI_CIPHER_MASK_xx defines for values. */
} uWifiScanResult_t;


/** Scan result callback type.
 *
 * This callback will be called once for each entry found.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param[out] pResult  the scan result.
 */
typedef void (*uWifiScanResultCallback_t) (uDeviceHandle_t devHandle,
                                           uWifiScanResult_t *pResult);


/** Connection status callback type.
 *
 * @param devHandle              the handle of the wifi instance.
 * @param connId                 connection ID.
 * @param status                 new status of connection. Please see U_WIFI_CON_STATUS_xx.
 * @param channel                wifi channel.
 *                               Note: only valid for #U_WIFI_CON_STATUS_CONNECTED otherwise set to 0.
 * @param[in] pBssid             remote AP BSSID as null terminated string.
 *                               Note: only valid for #U_WIFI_CON_STATUS_CONNECTED otherwise set to NULL.
 * @param disconnectReason       disconnect reason. Please see U_WIFI_REASON_xx.
 *                               Note: only valid for #U_WIFI_CON_STATUS_DISCONNECTED otherwise set to 0.
 * @param[in] pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uWifiConnectionStatusCallback_t) (uDeviceHandle_t devHandle,
                                                 int32_t connId,
                                                 int32_t status,
                                                 int32_t channel,
                                                 char *pBssid,
                                                 int32_t disconnectReason,
                                                 void *pCallbackParameter);


/** Network status callback type.
 *
 * @param devHandle              the handle of the wifi instance.
 * @param interfaceType          interface type. Only 1: Wifi Station supported at the moment.
 * @param statusMask             bitmask indicating the new status. Please see defined bits
 *                               U_WIFI_STATUS_MASK_xx.
 * @param[in] pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uWifiNetworkStatusCallback_t) (uDeviceHandle_t devHandle,
                                              int32_t interfaceType,
                                              uint32_t statusMask,
                                              void *pCallbackParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise wifi.  If the driver is already initialised then this
 * function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uWifiInit();

/** Shut-down wifi.  All instances will be removed internally
 * with calls to uWifiRemove().
 */
void uWifiDeinit();

/** Connect to a Wifi access point
 *
 * @param devHandle        the handle of the wifi instance.
 * @param[in] pSsid        the Service Set Identifier
 * @param authentication   the authentication type
 * @param[in] pPassPhrase  the passphrase (8-63 ASCII characters as a string) for WPA/WPA2/WPA3
 * @return                 zero on successful, else negative error code.
 *                         Note: there is no actual connection until the Wifi callback reports
 *                         connected.
 */
int32_t uWifiStationConnect(uDeviceHandle_t devHandle, const char *pSsid,
                            uWifiAuth_t authentication, const char *pPassPhrase);

/** Disconnect from Wifi access point
 *
 * @param devHandle the handle of the wifi instance.
 * @return          zero on successful, else negative error code.
 *                  Note: the disconnection is not completed until the Wifi callback
 *                  reports disconnected.
 */
int32_t uWifiStationDisconnect(uDeviceHandle_t devHandle);

/** Set a callback for Wifi connection status.
  *
 * @param devHandle              the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiSetConnectionStatusCallback(uDeviceHandle_t devHandle,
                                         uWifiConnectionStatusCallback_t pCallback,
                                         void *pCallbackParameter);
/** Set a callback for network status.
 *
 * @param devHandle              the handle of the short range instance.
 * @param[in] pCallback          callback function.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uWifiSetNetworkStatusCallback(uDeviceHandle_t devHandle,
                                      uWifiNetworkStatusCallback_t pCallback,
                                      void *pCallbackParameter);


/** Scan for SSIDs
 *
 * Please note that this function will block until the scan process is completed.
 * During this time pCallback will be called for each scan result entry found.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param[in] pSsid     optional SSID to search for. Set to NULL to search for any SSID.
 * @param[in] pCallback callback for handling a scan result entry.
 *                      IMPORTANT: the callback will be called while the AT lock is held
 *                      hence you are not allowed to call other u-blox
 *                      module APIs directly from this callback.
 * @return              zero on successful, else negative error code.
 */
int32_t uWifiStationScan(uDeviceHandle_t devHandle, const char *pSsid,
                         uWifiScanResultCallback_t pCallback);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_H_

// End of file
