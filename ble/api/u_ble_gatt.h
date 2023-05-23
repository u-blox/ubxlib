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

#ifndef _U_BLE_GATT_H_
#define _U_BLE_GATT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _BLE _Bluetooth Low Energy
 * @{
 */

/** @file
 * @brief This header file defines the general BLE GATT APIs.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Callback from service discovery of a connected BLE device when in
 *  central mode.
 * @param[in]  connHandle  corresponding connection handle.
 * @param[in]  startHandle first handle of the service.
 * @param[in]  endHandle   last handle of the service.
 * @param[in]  pUuid       pointer to a string with the service UUID.
 */
typedef void (*uBleGattDiscoverServiceCallback_t)(uint8_t connHandle,
                                                  uint16_t startHandle,
                                                  uint16_t endHandle,
                                                  char *pUuid);

/** Callback from characteristics discovery of a connected BLE device when in
 *  central mode.
 * @param[in]  connHandle  corresponding connection handle.
 * @param[in]  attrHandle  characteristic attribute handle.
 * @param[in]  properties  characteristic properties mask.
 * @param[in]  valueHandle characteristic value handle.
 * @param[in]  pUuid       pointer to a string with the characteristic UUID.
 */
typedef void (*uBleGattDiscoverCharCallback_t)(uint8_t connHandle, uint16_t attrHandle,
                                               uint8_t properties, uint16_t valueHandle,
                                               char *pUuid);

/** Callback when notification sent to a characteristic when in central mode.
 * @param[in]  connHandle  corresponding connection handle.
 * @param[in]  valueHandle characteristic value handle.
 * @param[in]  pValue      pointer to notification data.
 * @param[in]  valueLength size of the notification data.
 */
typedef void (*uBleGattNotificationCallback_t)(uint8_t connHandle,
                                               uint16_t valueHandle,
                                               uint8_t *pValue,
                                               uint8_t valueLength);

/** Callback when write is made to characteristic in peripheral mode.
 * @param[in]  connHandle  corresponding connection handle.
 * @param[in]  valueHandle characteristic value handle.
 * @param[in]  pValue      pointer to notification data.
 * @param[in]  valueLength size of the notification data.
 */
typedef void (*uBleGattWriteCallback_t)(uint8_t connHandle,
                                        uint16_t valueHandle,
                                        uint8_t *pValue,
                                        uint8_t valueLength);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/* Central (client) role GATT functions */

/** Do a enumeration (discovery) of all services in a connected peripheral
 *  when in central mode. The supplied callback will be called for each
 *  service found.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] connHandle  the connection handle retrieved from uBleGapConnect().
 * @param[in] cb          a callback routine for the found services.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattDiscoverServices(uDeviceHandle_t devHandle,
                                 int32_t connHandle,
                                 uBleGattDiscoverServiceCallback_t cb);

/** Do a enumeration (discovery) of all characteristics in a connected peripheral
 *  when in central mode. The supplied callback will be called for each
 *  characteristic found.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] connHandle  the connection handle retrieved from uBleGapConnect().
 * @param[in] cb          a callback routine for the found characteristics.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattDiscoverChar(uDeviceHandle_t devHandle,
                             int32_t connHandle,
                             uBleGattDiscoverCharCallback_t cb);

/** Set callback for peer writes when in central mode.
 *
 * Note: not all modules support this (e.g. ODIN-W2 does not).
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] cb          a callback routine for write data.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattSetWriteCallback(uDeviceHandle_t devHandle,
                                 uBleGattWriteCallback_t cb);


/* Peripheral (server) role GATT functions */

/** Add a server service when in peripheral mode
 *
 * Note: not all modules support this (e.g. ODIN-W2 does not).
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] pUuid       pointer to a string with the service UUID.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattAddService(uDeviceHandle_t devHandle,
                           const char *pUuid);

/** Add a server characteristic when in peripheral mode
 *
 * Note: not all modules support this (e.g. ODIN-W2 does not).
 *
 * @param[in]  devHandle    the handle of the u-blox BLE device.
 * @param[in]  pUuid        pointer to a string with the characteristic UUID.
 * @param[in]  properties   characteristic properties mask.
 * @param[out] pValueHandle pointer to a variable to receive the created
 *                          characteristic handle.
 * @return                  zero on success, on failure negative error code.
 */
int32_t uBleGattAddCharacteristic(uDeviceHandle_t devHandle,
                                  const char *pUuid, uint8_t properties,
                                  uint16_t *pValueHandle);

/** Set callback for peer notification writes when in peripheral mode.
 *
 * Note: not all modules support this (e.g. ODIN-W2 does not).
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] cb          a callback routine for write data.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattSetNotificationCallback(uDeviceHandle_t devHandle,
                                        uBleGattNotificationCallback_t cb);

/** Enable notifications on a connected GATT server value handle when in
 *  peripheral mode. The supplied callback will be called when notifications
 *  occur. Note that the supplied handle must have notification property.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] connHandle  the connection handle retrieved from uBleGapConnect.
 * @param[in] valueHandle value handle.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleGattEnableNotification(uDeviceHandle_t devHandle,
                                   int32_t connHandle, uint16_t valueHandle);


/* Common read/write GATT functions */

/** Read data from a supplied characteristics value handle.
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] connHandle  the connection handle retrieved from uBleGapConnect().
 * @param[in] valueHandle value handle.
 * @param[in] pValue      pointer to memory where to store the data.
 * @param[in] valueLength maximum size of the data.
 * @return                number of bytes read on success, on failure negative
 *                        error code.
 */
int32_t uBleGattReadValue(uDeviceHandle_t devHandle,
                          int32_t connHandle, uint16_t valueHandle,
                          uint8_t *pValue, uint8_t valueLength);

/** Write data to a supplied characteristics value handle.
 *
 * @param[in] devHandle    the handle of the u-blox BLE device.
 * @param[in] connHandle   the connection handle retrieved from uBleGapConnect().
 * @param[in] valueHandle  value handle.
 * @param[in] pValue       pointer to data to be written.
 * @param[in] valueLength  size of the data.
 * @param[in] waitResponse wait for write confirmation from the peer.
 * @return                 zero on success, on failure negative error code.
 */
int32_t uBleGattWriteValue(uDeviceHandle_t devHandle,
                           int32_t connHandle, uint16_t valueHandle,
                           const void *pValue, uint8_t valueLength,
                           bool waitResponse);

/** Write data with notification to a supplied characteristics value handle.
 *
 * Note: not all modules support this (e.g. ODIN-W2 does not).
 *
 * @param[in] devHandle    the handle of the u-blox BLE device.
 * @param[in] connHandle   the connection handle retrieved from uBleGapConnect().
 * @param[in] valueHandle  value handle.
 * @param[in] pValue       pointer to data to be written.
 * @param[in] valueLength  size of the data.
 * @return                 zero on success, on failure negative error code.
 */
int32_t uBleGattWriteNotifyValue(uDeviceHandle_t devHandle,
                                 int32_t connHandle, uint16_t valueHandle,
                                 const void *pValue, uint8_t valueLength);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif  // _U_BLE_GATT_H_

// End of file
