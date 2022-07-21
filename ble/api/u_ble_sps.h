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

#ifndef _U_BLE_SPS_H_
#define _U_BLE_SPS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_compiler.h"
#include "u_device.h"

/** \addtogroup _BLE
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that obtain data transfer
 * related commands for ble using the SPS protocol.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */
/** Invalid connection handle
 */
#define U_BLE_SPS_INVALID_HANDLE ((int32_t)(-1))

/** Size of receive buffer for a connected data channel
 *  When this buffer is full flow control will be invoked
 *  to stop the data flow from remote device, if enabled.
 */
#ifndef U_BLE_SPS_BUFFER_SIZE
#define U_BLE_SPS_BUFFER_SIZE 1024
#endif

/** Maximum number of simultaneous connections,
 *  server and client combined
 */
#ifndef U_BLE_SPS_MAX_CONNECTIONS
#define U_BLE_SPS_MAX_CONNECTIONS 8
#endif

/** Default timeout for data sending. Can be modified per
 *  connection with uBleSpsSetSendTimeout().
 */
#ifndef U_BLE_SPS_DEFAULT_SEND_TIMEOUT_MS
#define U_BLE_SPS_DEFAULT_SEND_TIMEOUT_MS 100
#endif

/** Default central scan interval
 */
#ifndef U_BLE_SPS_CONN_PARAM_SCAN_INT_DEFAULT
#define U_BLE_SPS_CONN_PARAM_SCAN_INT_DEFAULT 48
#endif

/** Default central scan window
 */
#ifndef U_BLE_SPS_CONN_PARAM_SCAN_WIN_DEFAULT
#define U_BLE_SPS_CONN_PARAM_SCAN_WIN_DEFAULT 48
#endif

/** Default timeout when creating connection from central
 */
#ifndef U_BLE_SPS_CONN_PARAM_TMO_DEFAULT
#define U_BLE_SPS_CONN_PARAM_TMO_DEFAULT 5000
#endif

/** Default minimum connection interval
 */
#ifndef U_BLE_SPS_CONN_PARAM_CONN_INT_MIN_DEFAULT
#define U_BLE_SPS_CONN_PARAM_CONN_INT_MIN_DEFAULT 24
#endif

/** Default maximum connection interval
 */
#ifndef U_BLE_SPS_CONN_PARAM_CONN_INT_MAX_DEFAULT
#define U_BLE_SPS_CONN_PARAM_CONN_INT_MAX_DEFAULT 30
#endif

/** Default connection latency
 */
#ifndef U_BLE_SPS_CONN_PARAM_CONN_LATENCY_DEFAULT
#define U_BLE_SPS_CONN_PARAM_CONN_LATENCY_DEFAULT 0
#endif

/** Default link loss timeout
 */
#ifndef U_BLE_SPS_CONN_PARAM_LINK_LOSS_TMO_DEFAULT
#define U_BLE_SPS_CONN_PARAM_LINK_LOSS_TMO_DEFAULT 2000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** SPS connection status.
 */
typedef enum {
    U_BLE_SPS_CONNECTED = 0,
    U_BLE_SPS_DISCONNECTED = 1,
} uBleConnectionStatus_t;

/** GATT service handles for SPS server.
 */
typedef struct {
    uint16_t     service;
    uint16_t     fifoValue;
    uint16_t     fifoCcc;
    uint16_t     creditsValue;
    uint16_t     creditsCcc;
} uBleSpsHandles_t;

/** Connection parameters.
 *
 *  @param scanInterval        scan interval (N*0.625 ms).
 *  @param scanWindow          scan window (N*0.625 ms).
 *  @param createConnectionTmo timeout before giving up if
 *                             remote device is not found in ms.
 *  @param connIntervalMin     connection interval (N*1.25 ms).
 *  @param connIntervalMax     connection interval (N*1.25 ms).
 *  @param connLatency         connection lantency, nbr of connection intervals.
 *  @param linkLossTimeout     link loss timeout in ms.
 */
typedef struct {
    // For central
    uint16_t scanInterval;
    uint16_t scanWindow;
    uint32_t createConnectionTmo;
    uint16_t connIntervalMin;
    uint16_t connIntervalMax;
    uint16_t connLatency;
    uint32_t linkLossTimeout;
} uBleSpsConnParams_t;

/** Connection status callback type.
 *
 * @param connHandle             connection handle (use to send disconnect).
 * @param[in] pAddress           BLE address.
 * @param status                 new status of connection, of #uBleConnectionStatus_t type.
 * @param channel                channel nbr, use to send data.
 * @param mtu                    max size of each packet.
 * @param[in] pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uBleSpsConnectionStatusCallback_t)(int32_t connHandle, char *pAddress,
                                                  int32_t status, int32_t channel, int32_t mtu,
                                                  void *pCallbackParameter);

/** Data callback type. Called to indicate that data is available for reading.
 *
 * @param channel                channel number.
 * @param[in] pCallbackParameter parameter pointer set when registering callback.
 */
typedef void (*uBleSpsAvailableCallback_t)(int32_t channel, void *pCallbackParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Sets the callback for connection events.
 * When a connected callback arrives, it is advisable to have a 50 ms delay
 * before data is sent on the connect.
 *
 * @param devHandle              the handle of the u-blox device.
 * @param[in] pCallback          callback function. Use NULL to deregister the callback.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       zero on success, on failure negative error code.
 */
int32_t uBleSpsSetCallbackConnectionStatus(uDeviceHandle_t devHandle,
                                           uBleSpsConnectionStatusCallback_t pCallback,
                                           void *pCallbackParameter);

/** Sets the callback for data available.
 *
 * @param devHandle              the handle of the u-blox device.
 * @param[in] pCallback          callback function. Use NULL to deregister the callback.
 * @param[in] pCallbackParameter parameter included with the callback.
 * @return                       Zero on success, on failure negative error code.
 */
int32_t uBleSpsSetDataAvailableCallback(uDeviceHandle_t devHandle,
                                        uBleSpsAvailableCallback_t pCallback,
                                        void *pCallbackParameter);

/** Create a SPS connection over BLE, this is the u-blox proprietary protocol for
 *  streaming data over BLE. Flow control is used.
 *
 * @note if the initiating side is peripheral it must also run an
 * SPS server which the central device then will connect to when this
 * function is called.
 *
 * @param devHandle       the handle of the u-blox device.
 * @param[in] pAddress    pointer to the address in 0012F398DD12p format,
 *                        must not be NULL.
 * @param[in] pConnParams pointer to connection parameters.
 *                        Use NULL for default values.
 * @return                zero on success, on failure negative error code.
 */
int32_t uBleSpsConnectSps(uDeviceHandle_t devHandle,
                          const char *pAddress,
                          const uBleSpsConnParams_t *pConnParams);

/** Disconnect the connection.
 * If data has been sent, it is advisable to have a 50 ms delay
 * before calling disconnect.
 *
 * @param devHandle  the handle of the u-blox device.
 * @param connHandle the connection handle from the connected event.
 * @return           zero on success, on failure negative error code.
 */
int32_t uBleSpsDisconnect(uDeviceHandle_t devHandle, int32_t connHandle);

/**
 *
 * @param devHandle  the handle of the u-blox device.
 * @param channel    channel to receive on, given in connection callback.
 * @param[out] pData pointer to the data buffer, must not be NULL.
 * @param length     size of receive buffer.
 *
 * @return           Number of bytes received, zero if no data is available,
 *                   on failure negative error code
 */
int32_t uBleSpsReceive(uDeviceHandle_t devHandle, int32_t channel, char *pData, int32_t length);

/** Send data
 *
 * @param devHandle the handle of the u-blox device.
 * @param channel   the channel to send on.
 * @param[in] pData pointer to the data, must not be NULL.
 * @param length    length of data to send, must not be 0.
 * @return          zero on success, on failure negative error code.
 */
int32_t uBleSpsSend(uDeviceHandle_t devHandle, int32_t channel, const char *pData, int32_t length);

/** Set timeout for data sending
 *
 * If sending of data takes more than this time uBleSpsSend() will stop sending data
 * and return. No error code will be given since uBleSpsSend() returns the number of bytes
 * actually written.
 *
 * @note this setting is per channel and thus has to be set after connecting.
 * #U_BLE_SPS_DEFAULT_SEND_TIMEOUT_MS will be used if timeout is not set
 *
 * @param devHandle the handle of the u-blox device.
 * @param channel   the channel to use this timeout on.
 * @param timeout   timeout in ms.
 *
 * @return          zero on success, on failure negative error code.
 */
int32_t uBleSpsSetSendTimeout(uDeviceHandle_t devHandle, int32_t channel, uint32_t timeout);

/** Get server handles for channel connection
 *
 * By reading the server handles for a connection
 * and preseting them before connecting to the same server next time,
 * the connection setup speed will improve significantly.
 * Read the server handles for a current connection using this function
 * and cache them for e.g. a bonded device for future use.
 *
 * @note This only works when the connecting side is central.
 *       If connecting side is peripheral it is up to the central
 *       device to cache server handles.
 *
 * @param devHandle     the handle of the u-blox device.
 * @param channel       the channel to read server handles on.
 * @param[out] pHandles pointer to struct with handles to write.
 *
 * @return              zero on success, on failure negative error code.
 */
int32_t uBleSpsGetSpsServerHandles(uDeviceHandle_t devHandle, int32_t channel,
                                   uBleSpsHandles_t *pHandles);

/** Preset server handles before conneting
 *
 * By reading the server handles for a connection
 * and preseting them before connecting to the same server next time,
 * the connection setup speed will improve significantly.
 * Preset cached server handles for a bonded device using this function
 * The preset values will be used on the next call to uBleSpsConnectSps().
 *
 * @note This only works when the connecting side is central.
 * If connecting side is peripheral it is up to the central device
 * to cache server handles.
 *
 * @param devHandle    the handle of the u-blox device.
 * @param[in] pHandles pointer to struct with handles.
 *
 * @return             zero on success, on failure negative error code.
 */
int32_t uBleSpsPresetSpsServerHandles(uDeviceHandle_t devHandle, const uBleSpsHandles_t *pHandles);

/** Disable flow control for next SPS connection
 *
 * Flow control is enabled by default. Flow control cannot be altered for
 * an ongoing connection.  Disabling flow control decreases connection setup
 * time and data overhead with the risk of losing data. If the received
 * amount of data during a connection is smaller than #U_BLE_SPS_BUFFER_SIZE
 * there is no risk of losing received data. The risk of losing sent data
 * depends on remote-side buffer sizes.
 *
 * Notice: If you use uBleSpsGetSpsServerHandles() to read server handles
 * you have to connect with flow control enabled since some of the server
 * handles are related to flow control.
 *
 * @param devHandle the handle of the u-blox device.
 *
 * @return          zero on success, on failure negative error code.
 */
int32_t uBleSpsDisableFlowCtrlOnNext(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_BLE_SPS_H_

// End of file
