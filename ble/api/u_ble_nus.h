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

#ifndef _U_BLE_NUS_H_
#define _U_BLE_NUS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _BLE _Bluetooth Low Energy
 *  @{
 */

/** @file
 * @brief This header file defines interface to the Nordic Uart Service (NUS)
 * client and server. Minimal point to point implementation, only one connection
 * at the time is supported.
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

/** Peer write detection callback
 *  @param[in]  pValue      pointer to the data written by the peer.
 *  @param[in]  valueLength size of the data.
 */
typedef void (*uBleNusReceiveCallback_t)(uint8_t *pValue,
                                         uint8_t valueLength);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initiate a device as either NUS client or server.
 *  The BLE interface of the provided device must have been initiated
 *  before this call. In the case of NUS client the device must be in
 *  BLE central mode and when NUS server in BLE peripheral mode.
 *
 *  When client mode is wanted then the address of the peer server device
 *  must be provided. A connection to this device will be attempted and
 *  then a validation of the required NUS characteristics.
 *
 *  In server mode the NUS service and its characteristics will be created.
 *  In this case the device should have been set to advertisement mode before
 *  this call.
 *
 *  Once a connection has been established, any writes made from the peer will
 *  be sent to the callback and writes to the peer can be made via uBleNusWrite.
 *
 *  Please note that use of the functionality in this API is intended to be
 *  used exclusively and without any other BLE GAP or GATT functions being
 *  called at the same time. Only one connection at the time is allowed.
 *
 * Note: not all modules support NUS server mode (e.g. ODIN-W2 does not).
 *
 * @param[in] devHandle   the handle of the u-blox BLE device.
 * @param[in] pAddress    peer address when client mode, set to NULL for
 *                        server mode.
 * @param[in] cb          a callback routine for peer write detection.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleNusInit(uDeviceHandle_t devHandle,
                    const char *pAddress,
                    uBleNusReceiveCallback_t cb);

/** Write data to the peer
 *
 * @param[in]  pValue      pointer to the data to write.
 * @param[in]  valueLength size of the data.
 * @return                 zero on success, on failure negative error code.
 */
int32_t uBleNusWrite(const void *pValue, uint8_t valueLength);

/** Create advertisement data package with the NUS service UUID.
 *  This data can then be used for uBleGapAdvertiseStart.
 *  Needed when advertising for clients that does filtering on this UUID.
 *
 * @param[out] pAdvData       pointer to advertisement storage.
 * @param[in]  advDataSize    max size of the advertisement storage.
 * @return                    size of the created package, on failure negative error code.
 */
int32_t uBleNusSetAdvData(uint8_t *pAdvData, uint8_t advDataSize);

/** Close down possible NUS connection
 *
 * @return                    zero on success, on failure negative error code.
 */
int32_t uBleNusDeInit();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif  // _U_BLE_NUS_H_

// End of file
