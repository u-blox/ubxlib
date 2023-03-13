/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_BLE_GAP_H_
#define _U_BLE_GAP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _BLE _Bluetooth Low Energy
 *  @{
 */

/** @file
 * @brief This header file defines the general BLE GAP APIs in ubxlib.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Advertisement data type definitions */
#define U_BT_DATA_FLAGS 0x01                  /**< AD flags */
#define U_BT_DATA_UUID16_SOME 0x02            /**< 16-bit UUID, more available */
#define U_BT_DATA_UUID16_ALL 0x03             /**< 16-bit UUID, all listed */
#define U_BT_DATA_UUID32_SOME 0x04            /**< 32-bit UUID, more available */
#define U_BT_DATA_UUID32_ALL 0x05             /**< 32-bit UUID, all listed */
#define U_BT_DATA_UUID128_SOME 0x06           /**< 128-bit UUID, more available */
#define U_BT_DATA_UUID128_ALL 0x07            /**< 128-bit UUID, all listed */
#define U_BT_DATA_NAME_SHORTENED 0x08         /**< Shortened name */
#define U_BT_DATA_NAME_COMPLETE 0x09          /**< Complete name */
#define U_BT_DATA_TX_POWER 0x0a               /**< Tx Power */
#define U_BT_DATA_SM_TK_VALUE 0x10            /**< Security Manager TK Value */
#define U_BT_DATA_SM_OOB_FLAGS 0x11           /**< Security Manager OOB Flags */
#define U_BT_DATA_SOLICIT16 0x14              /**< Solicit UUIDs, 16-bit */
#define U_BT_DATA_SOLICIT128 0x15             /**< Solicit UUIDs, 128-bit */
#define U_BT_DATA_SVC_DATA16 0x16             /**< Service data, 16-bit UUID */
#define U_BT_DATA_GAP_APPEARANCE 0x19         /**< GAP appearance */
#define U_BT_DATA_LE_BT_DEVICE_ADDRESS 0x1b   /**< LE Bluetooth Device Address */
#define U_BT_DATA_LE_ROLE 0x1c                /**< LE Role */
#define U_BT_DATA_SOLICIT32 0x1f              /**< Solicit UUIDs, 32-bit */
#define U_BT_DATA_SVC_DATA32 0x20             /**< Service data, 32-bit UUID */
#define U_BT_DATA_SVC_DATA128 0x21            /**< Service data, 128-bit UUID */
#define U_BT_DATA_LE_SC_CONFIRM_VALUE 0x22    /**< LE SC Confirmation Value */
#define U_BT_DATA_LE_SC_RANDOM_VALUE 0x23     /**< LE SC Random Value */
#define U_BT_DATA_URI 0x24                    /**< URI */
#define U_BT_DATA_LE_SUPPORTED_FEATURES 0x27  /**< LE Supported Features */
#define U_BT_DATA_CHANNEL_MAP_UPDATE_IND 0x28 /**< Channel Map Update Indication */
#define U_BT_DATA_MESH_PROV 0x29              /**< Mesh Provisioning PDU */
#define U_BT_DATA_MESH_MESSAGE 0x2a           /**< Mesh Networking PDU */
#define U_BT_DATA_MESH_BEACON 0x2b            /**< Mesh Beacon */
#define U_BT_DATA_BIG_INFO 0x2c               /**< BIGInfo */
#define U_BT_DATA_BROADCAST_CODE 0x2d         /**< Broadcast Code */
#define U_BT_DATA_CSIS_RSI 0x2e               /**< CSIS Random Set ID type */

#define U_BT_DATA_MANUFACTURER_DATA 0xff /**< Manufacturer Specific Data */

#define U_BT_LE_AD_LIMITED 0x01  /**< Limited Discoverable */
#define U_BT_LE_AD_GENERAL 0x02  /**< General Discoverable */
#define U_BT_LE_AD_NO_BREDR 0x04 /**< BR/EDR not supported */

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
/** BLE scanning types
 */
typedef enum {
    U_BLE_GAP_SCAN_DISCOVER_ALL_ONCE = 2,
    U_BLE_GAP_SCAN_DISCOVER_LIMITED_ONCE,
    U_BLE_GAP_SCAN_DISCOVER_ALL,
    U_BLE_GAP_SCAN_DISCOVER_WHITELISTED
} uBleGapDiscoveryType_t;

/** Result from a BLE scan
 */
typedef struct {
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE]; /**< Peer mac address */
    int rssi;           /**< Peer rssi value*/
    char name[31];      /**< Possible name field extracted from the advertisement data*/
    uint8_t dataType;   /**< Advertisement data type */
    uint8_t dataLength; /**< Advertisement data size */
    uint8_t data[31];   /**< Complete advertisement data*/
} uBleScanResult_t;

/** BLE scan result callback.
 *  @param[in]  pScanResult  information about a found device.
 *  @return                  true if the scan should continue or
 *                           false to stop it before the timeout.
 */
typedef bool (*uBleGapScanCallback_t)(uBleScanResult_t *pScanResult);

/** Connect/disconnect callback for central and peripheral.
 *  @param[in]  connHandle  connection handle identifying the peer.
 *                          Must later be used for uBleGapDisconnect and
 *                          in subsequent call to related gatt functions.
 *  @param[in]  pAddress    peer address.
 *  @param[in]  connected   True for connect and false for disconnect.
 */
typedef void (*uBleGapConnectCallback_t)(int32_t connHandle, char *pAddress, bool connected);

/** BLE advertisement configuration parameters.
*/
typedef struct {
    uint32_t minIntervalMs; /**< Advertising interval minimum in ms */
    uint32_t maxIntervalMs; /**< Advertising interval maximum in ms */
    bool connectable;       /**< Peripheral connectable */
    uint8_t maxClients;     /**< Maximum number of connected clients */
    uint8_t *pAdvData;      /**< Advertisement data */
    uint8_t advDataLength;  /**< Advertisement data size */
    uint8_t *pRespData;     /**< Advertisement response data */
    uint8_t respDataLength; /**< Advertisement response data size */
} uBleGapAdvConfig_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the mac address of the BLE device.
 *
 * @param[in]  devHandle  the handle of the u-blox BLE device.
 * @param[out] pMac       pointer to a string for the address.
 *                        Must at least be of size #U_SHORT_RANGE_BT_ADDRESS_SIZE
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapGetMac(uDeviceHandle_t devHandle, char *pMac);

/** Set callback for connection events. These can occur both when in central and
 *  peripheral mode.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] cb          the callback routine, set to NULL to remove the callback.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapSetConnectCallback(uDeviceHandle_t devHandle, uBleGapConnectCallback_t cb);

/** Do a synchronous (blocking) scan for advertising BLE devices during
 *  the specified time interval. For each found device the provided
 *  callback will be called with the corresponding advertisement data.
 *  Please note that this is a blocking call so no other BLE operations
 *  can be performed in the callback.
 *  Requires the BLE device to be in central mode.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] discType    type of scan to perform.
 * @param[in] activeScan  active or passive scan.
 * @param[in] timeousMs   total time interval in milliseconds used for the scan.
 * @param[in] cb          a callback routine for the found devices.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapScan(uDeviceHandle_t devHandle,
                    uBleGapDiscoveryType_t discType,
                    bool activeScan,
                    uint32_t timeousMs,
                    uBleGapScanCallback_t cb);

/** Try connecting to another peripheral BLE device.
 *  If a connection callback has been set via uBleGapSetConnectCallback() then
 *  this will be called when the connection has been completed.
 *  Requires the BLE device to be in central mode.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] pAddress    mac address of the peripheral.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapConnect(uDeviceHandle_t devHandle, const char *pAddress);

/** Start to disconnect a connected peripheral BLE device.
 *  If a connection callback has been set via uBleGapSetConnectCallback() then
 *  this will be called when the devices have completed the disconnect.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] connHandle  the connection handle received when connection was made.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapDisconnect(uDeviceHandle_t devHandle, int32_t connHandle);

/** Convenience routine for creating advertisement data with a full name
 *  or manufacturer data or both. This data can then be used for uBleGapAdvertiseStart.
 *  Set possible unused parameter to NULL.
 *
 * @param[in]  pName          name string, or NULL.
 * @param[in]  pManufData     pointer to manufacturer data or NULL.
 * @param[in]  manufDataSize  size of manufacturer data.
 * @param[out] pAdvData       pointer to advertisement storage.
 * @param[in]  advDataSize    max size of the advertisement storage.
 * @return                    size of the created package, on failure negative error code.
 */
int32_t uBleGapSetAdvData(const char *pName,
                          const uint8_t *pManufData, uint8_t manufDataSize,
                          uint8_t *pAdvData, uint8_t advDataSize);

/** Start BLE advertisement using the the specified configuration.
 *  Requires the BLE device to be in peripheral mode.
 *  If the device is set to connectable and a connection callback has been set
 *  via uBleGapSetConnectCallback, then this will be called if a central connects.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] pConfig     advertisement configuration.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapAdvertiseStart(uDeviceHandle_t devHandle, const uBleGapAdvConfig_t *pConfig);

/** Stop ongoing BLE advertisement.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapAdvertiseStop(uDeviceHandle_t devHandle);

/** Reset all GAP settings on the BLE device to factory values.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapReset(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif  // _U_BLE_GAP_H_

// End of file
