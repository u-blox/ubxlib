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

/** BLE radio modes (PHY) */
#define U_BT_LE_PHY_DONT_CARE 0 /**< Let other side decide */
#define U_BT_LE_PHY_1_MPBS    1 /**< 1 Mbps */
#define U_BT_LE_PHY_2_MBPS    2 /**< 2 Mbps */
#define U_BT_LE_PHY_CODED     4 /**< Coded PHY (when supported by the module) */

/** BLE device I/O capabilities for bonding */
#define U_BT_LE_IO_NONE        0 /**< No input and no output */
#define U_BT_LE_IO_DISP_ONLY   1 /**< Display only */
#define U_BT_LE_IO_DISP_YES_NO 2 /**< Display yes/no */
#define U_BT_LE_IO_KEYB_ONLY   3 /**< Keyboard only */
#define U_BT_LE_IO_KEYB_DISP   4 /**< Keyboard and display */

/** BLE bonding security modes */
#define U_BT_LE_BOND_NO_SEC    0 /**< Security disabled */
#define U_BT_LE_BOND_UNAUTH    1 /**< Allow unauthenticated bonding (Just Works) */
#define U_BT_LE_BOND_AUTH      2 /**< Only allow authenticated bonding */
#define U_BT_LE_BOND_AUTH_ENCR 3 /**< Only allow authenticated bonding with
                                      encrypted Bluetooth link. Fallback to
                                      simple bonding if the remote side does not
                                      support secure connections */
#define U_BT_LE_BOND_STRICT    4 /**< Only allow authenticated bonding with encrypted
                                      Bluetooth link. Strictly uses secure connections */

/** BLE bonding error codes */
#define U_BT_LE_BOND_ERR_SUCCESS  0 /**< Bonding procedure succeeded */
#define U_BT_LE_BOND_ERR_TIMEOUT  1 /**< Bonding procedure failed due to timeout */
#define U_BT_LE_BOND_ERR_FAILED   2 /**< Bonding failed because of authentication or
                                         pairing failed. */
#define U_BT_LE_BOND_ERR_WEAK     3 /**< Bonding failed because the protection against
                                         Man-In-The-Middle attack could not be guaranteed,
                                         the generated link key was too weak */

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
 *  @param      connHandle  connection handle identifying the peer.
 *                          Must later be used for uBleGapDisconnect and
 *                          in subsequent call to related gatt functions.
 *  @param[in]  pAddress    peer address.
 *  @param[in]  connected   True for connect and false for disconnect.
 */
typedef void (*uBleGapConnectCallback_t)(int32_t connHandle, char *pAddress, bool connected);

/** BLE advertisement configuration parameters.
*/
typedef struct {
    uint32_t minIntervalMs; /**< Advertising interval minimum in ms. */
    uint32_t maxIntervalMs; /**< Advertising interval maximum in ms. */
    bool connectable;       /**< Peripheral connectable. */
    uint8_t maxClients;     /**< Maximum number of connected clients. */
    uint8_t *pAdvData;      /**< Advertisement data. */
    uint8_t advDataLength;  /**< Advertisement data size. */
    uint8_t *pRespData;     /**< Advertisement response data. */
    uint8_t respDataLength; /**< Advertisement response data size. */
} uBleGapAdvConfig_t;

/** BLE connection configuration settings.
*/
typedef struct {
    uint16_t scanIntervalMs;         /**< Scan interval in ms. */
    uint16_t scanWindowMs;           /**< Scan window in ms. */
    uint32_t connCreateTimeoutMs;    /**< Timeout for connection in ms. */
    uint16_t connIntervalMinMs;      /**< Connection interval minimum in ms. */
    uint16_t connIntervalMaxMs;      /**< Connection interval maximum in ms. */
    uint16_t connLatency;            /**< Connection peripheral latency. */
    uint32_t linkLossTimeoutMs;      /**< Connection link loss timeout in ms. */
    uint16_t preferredTxPhy;         /**< Preferred transmitter PHY. */
    uint16_t preferredRxPhy;         /**< Preferred receiver PHY. */
} uBleGapConnectConfig_t;

/** PHY update callback, the result of a call to uBleGapRequestPhyChange().
 *  @param      connHandle  connection handle identifying the peer.
 *  @param      status      Bluetooth status code.
 *  @param      txPhy       transmitter PHY.
 *  @param      rxPhy       receiver PHY.
 */
typedef void (*uBleGapPhyUpdateCallback_t)(int32_t connHandle, int32_t status,
                                           int32_t txPhy, int32_t rxPhy);

/** Bonding callback when I/O capability set to #U_BT_LE_IO_DISP_YES_NO.
 *  Confirm or deny by calling uBleGapBondConfirm().
 *  @param[in]  pAddress      mac address of the bonding remote.
 *  @param      numericValue  numeric value to confirm.
 */
typedef void (*uBleGapBondConfirmCallback_t)(const char *pAddress, int32_t numericValue);

/** Bonding callback when I/O capability set to #U_BT_LE_IO_KEYB_ONLY.
 *  The passkey show on the remote device must be returned by calling uBleGapBondEnterPasskey().
 *  @param[in]  pAddress   mac address of the bonding remote.
 */
typedef void (*uBleGapBondPasskeyRequestCallback_t)(const char *pAddress);

/** Bonding callback when I/O capability set to #U_BT_LE_IO_DISP_ONLY.
 *  The passkey provided must be entered on the remote device.
 *  @param[in]  pAddress   mac address of the bonding remote.
 *  @param      passkey    passkey to be shown.
 */
typedef void (*uBleGapBondPasskeyEntryCallback_t)(const char *pAddress, int32_t passkey);

/** Callback indicating the completion result of a bonding started by uBleGapBond().
 *  @param[in]  pAddress   mac address of the bonding remote.
 *  @param      status     value from one of the U_BT_LE_BOND_ERR_x constants.
 */
typedef void (*uBleGapBondCompleteCallback_t)(const char *pAddress, int32_t status);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the mac address of the BLE device.
 *
 * @param      devHandle  the handle of the u-blox BLE device.
 * @param[out] pMac       pointer to a string for the address.
 *                        Must at least be of size #U_SHORT_RANGE_BT_ADDRESS_SIZE
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapGetMac(uDeviceHandle_t devHandle, char *pMac);

/** Enable or disable pairing mode. The default mode if this function is not called
 *  is that the the device is pairable.
 *
 * @param      devHandle  the handle of the u-blox BLE device.
 * @param      isPairable set to true to enable pairing.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapSetPairable(uDeviceHandle_t devHandle, bool isPairable);

/** Configure bonding security. If this function is not called before any bonding attempts
 *  the security level will be "Security disabled".
 *
 * @param      devHandle        the handle of the u-blox BLE device.
 * @param      ioCapabilities   current device I/O capabilities.
 * @param      bondSecurity     bonding security level.
 * @param[in]  confirmCb        function to be called when bonding requires confirmation.
 *                              Can be set to NULL when not applicable.
 * @param[in]  passKeyRequestCb function to be called when bonding requires passkey entry
 *                              on the remote.device. Can be set to NULL when not applicable.
 * @param[in]  passKeyEntryCb   function to be called when bonding requires passkey entry
 *                              from the local device. Can be set to NULL when not applicable.
 * @return                      zero on success, on failure negative error code.
 */
int32_t uBleSetBondParameters(uDeviceHandle_t devHandle,
                              int32_t ioCapabilities,
                              int32_t bondSecurity,
                              uBleGapBondConfirmCallback_t confirmCb,
                              uBleGapBondPasskeyRequestCallback_t passKeyRequestCb,
                              uBleGapBondPasskeyEntryCallback_t passKeyEntryCb);

/** Request bonding with a peripheral when in central mode.
 *
 * @param      devHandle       the handle of the u-blox BLE device.
 * @param[in]  pAddress        mac address of the peripheral.
 * @param[in]  cb              callback when bonding has completed.
 * @return                     zero on success, on failure negative error code.
 */
int32_t uBleGapBond(uDeviceHandle_t devHandle, const char *pAddress,
                    uBleGapBondCompleteCallback_t cb);

/** Remove bonding from this device.
 *
 * @param      devHandle       the handle of the u-blox BLE device.
 * @param[in]  pAddress        mac address of the bonded device,
 *                             can be set to NULL to remove all bonded devices.
 * @return                     zero on success, on failure negative error code.
 */
int32_t uBleGapRemoveBond(uDeviceHandle_t devHandle, const char *pAddress);

/** Confirm or deny bonding from a central. This function shoule be called after that the
 *  function "yesNoCallback" earlier specified in uBleSetBondParameters() has been called
 *  (and only then).
 *
 * @param      devHandle       the handle of the u-blox BLE device.
 * @param      confirm         set to true to confirm or false to deny.
 * @param[in]  pAddress        mac address of the central requesting a bond.
 * @return                     zero on success, on failure negative error code.
 */
int32_t uBleGapBondConfirm(uDeviceHandle_t devHandle, bool confirm, const char *pAddress);

/** Confirm or deny bonding by specifying a passkey. This function should be called after
 *  that the function "confirmCb" earlier specified in uBleSetBondParameters() has been
 *  called (and only then).
 *
 * @param      devHandle       the handle of the u-blox BLE device.
 * @param      confirm         set to true to confirm or false to deny.
 * @param[in]  pAddress        mac address of the central requesting a bond.
 * @param      passkey         passkey used to confirm bonding, ignored if confirm is false.
 * @return                     zero on success, on failure negative error code.
 */
int32_t uBleGapBondEnterPasskey(uDeviceHandle_t devHandle, bool confirm,
                                const char *pAddress, int32_t passkey);

/** Set callback for connection events. These can occur both when in central and
 *  peripheral mode.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
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
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param     discType    type of scan to perform.
 * @param     activeScan  active or passive scan.
 * @param     timeousMs   total time interval in milliseconds used for the scan.
 * @param[in] cb          a callback routine for the found devices.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapScan(uDeviceHandle_t devHandle, uBleGapDiscoveryType_t discType, bool activeScan,
                    uint32_t timeousMs, uBleGapScanCallback_t cb);

/** Set the connection configuration parameters to be used for the next call to uBleGapConnect.
 *  If not called then the module's default ones will be used.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param[in] pConfig     connection configuration parameters.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapSetConnectParams(uDeviceHandle_t devHandle, uBleGapConnectConfig_t *pConfig);

/** Try connecting to another peripheral BLE device.
 *  If a connection callback has been set via uBleGapSetConnectCallback() then
 *  this will be called when the connection has been completed.
 *  Requires the BLE device to be in central mode.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param[in] pAddress    mac address of the peripheral.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapConnect(uDeviceHandle_t devHandle, const char *pAddress);

/** Request a new PHY configuration for an existing BLE connection.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param     connHandle  the connection handle received when connection was made.
 * @param     txPhy       transmitter PHY.
 * @param     rxPhy       receiver PHY.
 * @param[in] cb          callback for the result of the request.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapRequestPhyChange(uDeviceHandle_t devHandle, int32_t connHandle,
                                int32_t txPhy, int32_t rxPhy, uBleGapPhyUpdateCallback_t cb);

/** Start to disconnect a connected peripheral BLE device.
 *  If a connection callback has been set via uBleGapSetConnectCallback() then
 *  this will be called when the devices have completed the disconnect.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param     connHandle  the connection handle received when connection was made.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapDisconnect(uDeviceHandle_t devHandle, int32_t connHandle);

/** Convenience routine for creating advertisement data with a full name
 *  or manufacturer data or both. This data can then be used for uBleGapAdvertiseStart().
 *  Set possible unused parameter to NULL.
 *
 * @param[in]  pName          name string, or NULL.
 * @param[in]  pManufData     pointer to manufacturer data or NULL.
 * @param      manufDataSize  size of manufacturer data.
 * @param[out] pAdvData       pointer to advertisement storage.
 * @param      advDataSize    max size of the advertisement storage.
 * @return                    size of the created package, on failure negative error code.
 */
int32_t uBleGapSetAdvData(const char *pName, const uint8_t *pManufData, uint8_t manufDataSize,
                          uint8_t *pAdvData, uint8_t advDataSize);

/** Start BLE advertisement using the the specified configuration.
 *  Requires the BLE device to be in peripheral mode.
 *  If the device is set to connectable and a connection callback has been set
 *  via uBleGapSetConnectCallback(), then this will be called if a central connects.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @param[in] pConfig     advertisement configuration.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapAdvertiseStart(uDeviceHandle_t devHandle, const uBleGapAdvConfig_t *pConfig);

/** Stop ongoing BLE advertisement.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapAdvertiseStop(uDeviceHandle_t devHandle);

/** Reset all GAP settings on the BLE device to factory values.
 *
 * @param     devHandle   the handle of the u-blox BLE device.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGapReset(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif  // _U_BLE_GAP_H_

// End of file
