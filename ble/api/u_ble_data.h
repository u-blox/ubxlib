/*
 * Copyright 2020 u-blox Ltd
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

#ifndef _U_BLE_DATA_H_
#define _U_BLE_DATA_H_

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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */


/** Sets the callback for connections events
 * When a connected callback arrives, it is advisable to have a 50 ms delay
 * before data is sent on the connect.
 *
 * @param bleHandle          the handle of the ble instance.
 * @param pCallback          callback function. Use NULL to deregister the
 *                           callback. Parameter order:
 *                           - connection handle (use to send disconnect)
 *                           - address
 *                           - type (connected == 0, disconnected == 1)
 *                           - channel (use to send data)
 *                           - mtu (max size of each packet)
 *                           - pCallbackParameter
 * @param pCallbackParameter parameter included with the callback.
 * @return                   zero on success, on failure negative error code.
 */
int32_t uBleDataSetCallbackConnectionStatus(int32_t bleHandle,
                                            void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *),
                                            void *pCallbackParameter);

/** Sets the callback for data events
 *
 * @param bleHandle   the handle of the ble instance.
 * @param pCallback   callback function. Use NULL to deregister the
 *                    callback. Parameter order:
 *                    - channel
 *                    - size
 *                    - data
 *                    - pCallbackParameter
 * @param pCallbackParameter parameter included with the callback.
 * @return            zero on success, on failure negative error code.
 */
int32_t uBleDataSetCallbackData(int32_t bleHandle,
                                void (*pCallback) (int32_t, size_t, char *, void *),
                                void *pCallbackParameter);

/** Create a sps connection over ble, this is the u-blox propriatary protocol for
 *  streaming data over ble. Flow control is used.
 *
 * @param bleHandle   the handle of the ble instance.
 * @param pAddress    pointer to the address in 0012F398DD12p format,
 *                    must not be NULL.
 * @return            zero on success, on failure negative error code.
 */
int32_t uBleDataConnectSps(int32_t bleHandle, const char *pAddress);

/** Disconnect the connection.
 * If data has been sent, it is advisable to have a 50 ms delay
 * before calling disconnect.
 *
 * @param bleHandle   the handle of the ble instance.
 * @param connHandle  the connection handle from the connected event.
 * @return            zero on success, on failure negative error code.
 */
int32_t uBleDataDisconnect(int32_t bleHandle, int32_t connHandle);

/** Send data
 *
 * @param bleHandle   the handle of the ble instance.
 * @param channel     the channel to send on.
 * @param pData       pointer to the data, must not be NULL.
 * @param length      length of data to send, must not be 0.
 * @return            zero on success, on failure negative error code.
 */
int32_t uBleDataSend(int32_t bleHandle, int32_t channel, const char *pData, int32_t length);

#ifdef __cplusplus
}
#endif

#endif // _U_BLE_DATA_H_

// End of file
