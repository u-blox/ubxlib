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

#ifndef _U_NETWORK_H_
#define _U_NETWORK_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup network Network
 *  @{
 */

/** @file
 * @brief Functions for bringing up and down a network interface
 * on a device. These functions are thread-safe.
 *
 * The functions here should be used in conjunction with those in the
 * uDevice API in the following sequence.
 *
 * ```
 * uDeviceInit():           call this at start of day in order to make
 *                          the device API available.
 * uDeviceOpen():           call this with a pointer to a const structure
 *                          containing the physical configuration for the
 *                          device (module type, physical interface (UART
 *                          etc.), pins used, etc.): when the function
 *                          returns the module is powered-up and ready to
 *                          support a network.
 * uNetworkInterfaceUp():   call this with the device handle and a pointer
 *                          to a const structure containing the network
 *                          configuration (e.g. SSID in the case of Wifi,
 *                          APN in the case of cellular, etc.) when you
 *                          would like the network to connect; after this
 *                          is called you can send and receive stuff over
 *                          the network.
 * uNetworkInterfaceDown(): disconnect the network; the network remains
 *                          powered-up and may be reconfigured etc.: you
 *                          must call uNetworkInterfaceUp() to talk with
 *                          it again.
 * uDeviceClose():          call this to power the device down and clear
 *                          up any resources belonging to it; uDeviceOpen()
 *                          must be called to re-instantiate the device.
 * uDeviceDeinit():         call this at end of day in order to clear up any
 *                          resources owned by the device API.
 * ```
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

/** Network types.
 */
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_MAX_NUM)
//lint -estring(788, uNetworkType_t::U_NETWORK_TYPE_NONE)
//  Suppress not used within defaulted switch
typedef enum {
    U_NETWORK_TYPE_NONE,
    U_NETWORK_TYPE_BLE,
    U_NETWORK_TYPE_CELL,
    U_NETWORK_TYPE_WIFI,
    U_NETWORK_TYPE_GNSS,
    U_NETWORK_TYPE_MAX_NUM
} uNetworkType_t;

/** A version number for the network configuration structure. In
 * general you should allow the compiler to initialise any variable
 * of this type to zero and ignore it.  It is only set to a value
 * other than zero when variables in a new and extended version of
 * the structure it is a part of are being used, the version number
 * being employed by this code to detect that and, more importantly,
 * to adopt default values for any new elements when the version
 * number is STILL ZERO, maintaining backwards compatibility with
 * existing application code.  The structure this is a part of will
 * include instructions as to when a non-zero version number should
 * be set.
 */
typedef int32_t uNetworkCfgVersion_t;

/** Network status information for BLE.
 */
typedef struct {
    int32_t connHandle;  /**< connection handle (use to send disconnect). */
    char *pAddress; /**< BLE address. */
    int32_t status; /**< new status of connection; see #uBleConnectionStatus_t
                         in u_ble_sps.h. */
    int32_t channel; /**< channel nbr, use to send data. */
    int32_t mtu; /**< max size of each packet. */
} uNetworkStatusBle_t;

/** Network status information for cellular.
 */
typedef struct {
    int32_t domain; /**< the cellular domain; see #uCellNetRegDomain_t in u_cell_net.h. */
    int32_t status; /**< the status on that domain; see #uCellNetStatus_t in u_cell_net.h. */
} uNetworkStatusCell_t;

/** Network status information for Wi-Fi.
 */
typedef struct {
    int32_t connId;  /**< connection ID. */
    int32_t status; /**< new status of connection; see U_WIFI_CON_STATUS_xx in u_wifi.h. */
    int32_t channel; /**< Wi-Fi channel; only valid for status #U_WIFI_CON_STATUS_CONNECTED. */
    char *pBssid; /**< remote AP BSSID as a null terminated string. */
    int32_t disconnectReason; /**< disconnect reason; see U_WIFI_CON_STATUS_xx in u_wifi.h. */
} uNetworkStatusWifi_t;

/** The union of all network status types.
 */
typedef union {
    uNetworkStatusBle_t ble;
    uNetworkStatusCell_t cell;
    uNetworkStatusWifi_t wifi;
} uNetworkStatus_t;

/** Function signature for the network status callback.
 *
 * @param devHandle    the handle of the device.
 * @param netType      the network type that the status
 *                     applies to.
 * @param isUp         true if the network is up, else false.
 * @param[out] pStatus a pointer to a union containing
 *                     the detailed status information for
 *                     any network type; please pick the
 *                     correct union member for the value
 *                     of networkType, the BLE member
 *                     for #U_NETWORK_TYPE_BLE, the cell member
 *                     for #U_NETWORK_TYPE_CELL and the wifi
 *                     member for #U_NETWORK_TYPE_WIFI
 *                     (recalling that reporting of network
 *                     status is not relevant to GNSS).
 *                     IMPORTANT: the status information
 *                     should NOT be used outside the
 *                     callback function unless a copy
 *                     is taken.  For instance, to record
 *                     the address of a BLE peer for later
 *                     use, one would do this:
 * ```
 *                     char peerAddress[32];
 *                     void myNetworkStatusCallback(uDeviceHandle_t devHandle,
 *                                                  uNetworkType_t netType,
 *                                                  bool isUp,
 *                                                  uNetworkStatus_t *pStatus,
 *                                                  void *pParameter)
 *                    {
 *                        if ((netType == U_NETWORK_TYPE_BLE) && isUp &&
 *                            (pStatus != NULL)) {
 *                            strncpy(peerAddress, sizeof(peerAddress),
 *                                    pStatus->ble.pAddress);
 *                        }
 *                    ...
 * ```
 * @param[out] pParameter the value of pCallbackParameter as passed
 *                        to uNetworkSetStatusCallback().
 */
typedef void (*uNetworkStatusCallback_t) (uDeviceHandle_t devHandle,
                                          uNetworkType_t netType,
                                          bool isUp,
                                          uNetworkStatus_t *pStatus,
                                          void *pParameter);

/** Callback and parameter for network status.
 */
typedef struct {
    uNetworkStatusCallback_t pCallback;
    void *pCallbackParameter;
} uNetworkStatusCallbackData_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Bring up the given network interface on a device. If the network
 * is already up the implementation should return success without
 * doing anything.
 *
 * Note: for a Wi-Fi network, this function uses the
 * uWifiSetNetworkStatusCallback() and uWifiSetConnectionStatusCallback()
 * callbacks.
 *
 * @param devHandle        the handle of the device carrying the
 *                         network.
 * @param netType          which of the network interfaces to bring
 *                         up.
 * @param[in] pCfg         a pointer to the configuration
 *                         information for the given network
 *                         type.  This must be stored
 *                         statically, a true constant: the
 *                         contents are not copied by this
 *                         function. The configuration
 *                         structures are defined by this
 *                         API in the u_network_xxx.h header
 *                         files and have the name
 *                         uNetworkCfgXxx_t, where Xxx is
 *                         replaced by one of Cell, BLE or
 *                         Wifi.  The configuration is passed
 *                         transparently through to the given
 *                         API, hence the use of void *
 *                         here. The second entry in all of
 *                         these structures is of type
 *                         #uNetworkType_t to indicate the
 *                         type and allow cross-checking.
 *                         Can be set to NULL on subsequent calls
 *                         if the configuration is unchanged.
 * @return                 zero on success else negative error code.
 */
int32_t uNetworkInterfaceUp(uDeviceHandle_t devHandle, uNetworkType_t netType,
                            const void *pCfg);

/** Take down the given network interface on a device, disconnecting
 * it from any peer entity.  After this function returns
 * uNetworkInterfaceUp() must be called once more to ensure that the
 * network is brought back to a usable state.  If the network
 * is already down success will be returned.  If a network
 * status callback has been set with uNetworkSetStatusCallback(),
 * this will cancel it.
 *
 * Note: for a Wi-Fi network, this function uses the
 * uWifiSetConnectionStatusCallback() callback.
 *
 * @param devHandle the handle of the device that is carrying the
 *                  network.
 * @param netType   which of the module interfaces to take down.
 * @return          zero on success else negative error code.
 */
int32_t uNetworkInterfaceDown(uDeviceHandle_t devHandle, uNetworkType_t netType);

/** Enable or disable a callback which will be called when
 * the network status changes.  IMPORTANT: the actions that
 * might be taken by the application when a network has
 * gone down unexpectedly are different depending on the
 * underlying network type:
 *
 * BLE and Wi-Fi: if the isUp parameter passed to the callback
 *                is false, the network has dropped, it
 *                is up to the application to attempt to
 *                bring the network connection back up by
 *                calling uNetworkInterfaceUp() if it still
 *                needs it, along with any sockets or MQTT
 *                broker connection (which will also have been
 *                lost); see also the note below about how you
 *                should [not] go about this.
 *                Note also that this function uses the
 *                uWifiSetNetworkStatusCallback() and
 *                uBleSpsSetCallbackConnectionStatus() callbacks.
 *
 * Cellular:      if the isUp parameter passed to the callback
 *                is false then the cellular module will already
 *                be trying to regain service for you; you need
 *                do nothing, there is NO NEED to call
 *                uNetworkInterfaceUp() again.  Only when the
 *                callback is called ONCE MORE with isUp set to
 *                true do you need to take any action, which is
 *                to restore any sockets connection, or any MQTT
 *                broker connection, you may have had, since
 *                these will have been lost when cellular service
 *                was lost; see also the note below about how you
 *                should [not] go about this.
 *
 * GNSS:          this callback is not relevant to GNSS; an error
 *                will be returned.
 *
 * VERY IMPORTANT: you should NOT call any ubxlib APIs from the
 * callback, just set a flag or launch another task to perform
 * any required actions.  This is because the context that the
 * callback task is being run in is used, internally, by other
 * aspects of ubxlib, and so if you call back into ubxlib from
 * your callback task you are quite likely to get stuck.
 *
 * The callback will be called in a task with a stack of size
 * #U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES (see u_at_client.h).
 * Calling uNetworkInterfaceDown() will cancel the callback.
 *
 * @param devHandle               the handle of the device carrying
 *                                the network.
 * @param netType                 the network interface to apply
 *                                this callback to.
 * @param[in] pCallback           pointer to the function to
 *                                handle status changes; use NULL
 *                                to deactivate a previously
 *                                active network status
 *                                callback.
 * @param[in] pCallbackParameter  a pointer to be passed
 *                                to the callback as its
 *                                last parameter; may be NULL.
 * @return                        zero on success else negative
 *                                error code.
 */
int32_t uNetworkSetStatusCallback(uDeviceHandle_t devHandle,
                                  uNetworkType_t netType,
                                  uNetworkStatusCallback_t pCallback,
                                  void *pCallbackParameter);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_NETWORK_H_

// End of file
