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

#ifndef _U_PORT_PPP_H_
#define _U_PORT_PPP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_sock.h"

/** \addtogroup __port __Port
 *  @{
 */

/** @file
 * @brief This header file defines functions that allow a PPP
 * interface of ubxlib to be connected into the IP stack of
 * a platform.
 *
 * IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT
 *
 * It is HIGHLY LIKELY that there are many settings you need
 * to get right in your platform configuration files for PPP
 * to work: please see the README.md in the relevant platform
 * directory for details.
 *
 * It is ALSO HIGHLY LIKELY that there are limitations as to
 * what each platform actually supports; these limitations
 * are documented in the same place.
 *
 * Please also note that the application NEVER needs to call
 * any of the functions defined here; they are purely called
 * from within ubxlib to connect a platform's PPP interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Suggested size of receive buffer to request if pReceiveData
 * passed to #uPortPppConnectCallback_t is NULL.
 */
#define U_PORT_PPP_RECEIVE_BUFFER_BYTES 1024

#ifndef U_PORT_PPP_SHUTDOWN_TIMEOUT_SECONDS
/** How long to wait for the IP stack that PPP is attached to to
 * shut down any connections that may be running over PPP down.
 */
# define U_PORT_PPP_SHUTDOWN_TIMEOUT_SECONDS 10
#endif

#ifndef U_PORT_PPP_DNS_PRIMARY_DEFAULT_STR
/** The primary DNS address to use if it is not possible to
 * read the primary DNS address from the module.  Use NULL
 * to provide no default.
 */
# define U_PORT_PPP_DNS_PRIMARY_DEFAULT_STR "8.8.8.8"
#endif

#ifndef U_PORT_PPP_DNS_SECONDARY_DEFAULT_STR
/** The secondary DNS address to use if it is not possible to
 * read the secondary DNS address from the module.  Use NULL
 * to provide no default.
 */
# define U_PORT_PPP_DNS_SECONDARY_DEFAULT_STR "8.8.4.4"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible authentication modes for the PPP connection.
 *
 * Note: there is also a #uCellNetAuthenticationMode_t enumeration
 * which is set to match this one.  If you make a change here you
 * may need to make a change there also.
 */
typedef enum {
    U_PORT_PPP_AUTHENTICATION_MODE_NONE = 0,
    U_PORT_PPP_AUTHENTICATION_MODE_PAP = 1,
    U_PORT_PPP_AUTHENTICATION_MODE_CHAP = 2,
    U_PORT_PPP_AUTHENTICATION_MODE_MAX_NUM
} uPortPppAuthenticationMode_t;

/** Callback to receive a buffer of data from the PPP interface of
 * a module.  This function may be hooked into the PPP API at the
 * bottom-end of a platform's IP stack to permit it to receive the
 * contents of PPP frames arriving from a module.  Any data at pData
 * should be handled by this function before it returns as it may be
 * overwritten afterwards.
 *
 * @param[in] pDevHandle         the #uDeviceHandle_t of the [for
 *                               example celular] instance that called
 *                               the callback; this is a void *
 *                               rather than a #uDeviceHandle_t here
 *                               in order to avoid dragging in all
 *                               of the uDevice types into the port
 *                               layer.
 * @param[in] pData              a pointer to the received data;
 *                               will not be NULL.
 * @param dataSize               the number of bytes of data at pData.
 * @param[in,out] pCallbackParam the callback parameter that was
 *                               passed to uPortPppAttach().
 */
typedef void (uPortPppReceiveCallback_t)(void *pDevHandle,
                                         const char *pData,
                                         size_t dataSize,
                                         void *pCallbackParam);

/** Callback that opens the PPP interface of a module. If the PPP
 * interface is already open this function should do nothing and
 * return success; uPortPppDetach() should be called first if you
 * would like to change the buffering arrangements, the callback
 * or its parameter.
 *
 * @param[in] pDevHandle                the #uDeviceHandle_t of the device
 *                                      on which the PPP channel is to be
 *                                      opened; this is a void *
 *                                      rather than a #uDeviceHandle_t here
 *                                      in order to avoid dragging in all
 *                                      of the uDevice types into the port
 *                                      layer.
 * @param[in] pReceiveCallback          the data reception callback; may be
 *                                      NULL if only data transmission is
 *                                      required.
 * @param[in,out] pReceiveCallbackParam a parameter that will be passed to
 *                                      pReceiveCallback as its last parameter;
 *                                      may be NULL, ignored if pReceiveCallback
 *                                      is NULL.
 * @param[in] pReceiveData              a pointer to a buffer for received
 *                                      data; may be NULL, in which case, if
 *                                      pReceiveCallback is non-NULL, this code
 *                                      will provide a receive buffer.
 * @param receiveDataSize               the amount of space at pReceiveData in
 *                                      bytes or, if pReceiveData is NULL, the
 *                                      receive buffer size that should be
 *                                      allocated by this function;
 *                                      #U_PORT_PPP_RECEIVE_BUFFER_BYTES is
 *                                      a sensible value.
 * @param[in] pKeepGoingCallback        a callback function that governs how
 *                                      long to wait for the PPP connection to
 *                                      open.  This function will be called
 *                                      once a second while waiting for the
 *                                      PPP connection to complete; the PPP
 *                                      open attempt will only continue while
 *                                      it returns true.  This allows the caller
 *                                      to terminate the connection attempt at
 *                                      their convenience. May be NULL, in
 *                                      which case the connection attempt will
 *                                      eventually time out on failure.
 * @return                              zero on success, else negative error
 *                                      code.
 */
typedef int32_t (uPortPppConnectCallback_t) (void *pDevHandle,
                                             uPortPppReceiveCallback_t *pReceiveCallback,
                                             void *pReceiveCallbackParam,
                                             char *pReceiveData, size_t receiveDataSize,
                                             bool (*pKeepGoingCallback) (void *pDevHandle));

/** Callback that closes the PPP interface of a module.  When this
 * function has returned the pReceiveCallback function passed to
 * #uPortPppConnectCallback_t will no longer be called and any
 * pReceiveData buffer passed to #uPortPppConnectCallback_t will
 * no longer be written-to.  If no PPP connection is open this
 * function will do nothing and return success.
 *
 * @param[in] pDevHandle        the #uDeviceHandle_t of the device on which
 *                              the PPP channel is to be closed; this is a
 *                              void * rather than a #uDeviceHandle_t here
 *                              in order to avoid dragging in all of the
 *                              uDevice types into the port layer.
 * @param pppTerminateRequired  set this to true if the PPP connection
 *                              should be terminated first or leave
 *                              as false if the PPP connection
 *                              has already been terminated by
 *                              the peer.
 * @return                      zero on success, else negative error code.
 */
typedef int32_t (uPortPppDisconnectCallback_t) (void *pDevHandle,
                                                bool pppTerminateRequired);

/** Callback to transmit data over the PPP interface of a module.
 * This may be integrated into a higher layer, e.g. the PPP
 * interface at the bottom of an IP stack of a platform, to permit
 * it to send PPP frames over a module.  #uPortPppConnectCallback_t must
 * have returned successfully for transmission to succeed.
 *
 * @param[in] pDevHandle the #uDeviceHandle_t of the device on which
 *                       the PPP channel is to be transmitted; this
 *                       is a void * rather than a #uDeviceHandle_t
 *                       here in order to avoid dragging in all of
 *                       the uDevice types into the port layer.
 * @param[in] pData      a pointer to the data to transmit; can only
 *                       be NULL if dataSize is zero.
 * @param dataSize       the number of bytes of data at pData.
 * @return               on success the number bytes transmitted,
 *                       which may be less than dataSize, else
 *                       negative error code.
 */
typedef int32_t (uPortPppTransmitCallback_t) (void *pDevHandle,
                                              const char *pData,
                                              size_t dataSize);

/* ----------------------------------------------------------------
 * FUNCTIONS:  WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

/** Workaround for Espressif linker missing out files that
 * only contain functions which also have weak alternatives
 * (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
 *
 * You can ignore this function.
 */
void uPortPppDefaultPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Attach a PPP interface to the bottom of the IP stack of a
 * platform.  This is called by a ubxlib layer (e.g. cellular)
 * when a device is powered-up that is able to support PPP.  This
 * function performs all of the logical connection with the platform
 * but it does NOT call any of the callback functions passed in, the
 * ones that interact with the [e.g. cellular] device; those are
 * simply stored for use when uPortPppConnect(), uPortPppReconnect(),
 * uPortPppDisconnect() or uPortPppDetach() are called.
 *
 * The application NEVER NEEDS to call this function; it is purely
 * for internal use within ubxlib.
 *
 * If the PPP interface is already attached this function will do
 * nothing and return success; to ensure that any new parameters
 * are adopted, uPortPppDetach() should be called first.
 *
 * If a PPP interface is not supported by the platform this function
 * does not need to be implemented: a weakly-linked implementation
 * will take over and return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * Note: this only attaches the PPP interface logically, the
 * interface cannot be used until uPortPppConnect() is called.
 *
 * @param[in] pDevHandle                the #uDeviceHandle_t of the device
 *                                      that is offering the PPP interface;
 *                                      this is a void * rather than a
 *                                      #uDeviceHandle_t here in order to
 *                                      avoid dragging in all of the uDevice
 *                                      types into the port layer.
 * @param[in] pConnectCallback          a callback that will open the PPP
 *                                      interface on the device; may be
 *                                      NULL if the PPP interface is transmit
 *                                      only and is always open.
 * @param[in] pDisconnectCallback       a callback that will close the PPP
 *                                      interface on the device; may be
 *                                      NULL if the PPP interface cannot
 *                                      be closed.
 * @param[in] pTransmitCallback         a callback that the platform may call
 *                                      to send PPP data over the PPP
 *                                      interface; may be NULL if is it not
 *                                      possible to transmit data over the
 *                                      PPP interface.
 * @return                              zero on success, else negative error code.
 */
int32_t uPortPppAttach(void *pDevHandle,
                       uPortPppConnectCallback_t *pConnectCallback,
                       uPortPppDisconnectCallback_t *pDisconnectCallback,
                       uPortPppTransmitCallback_t *pTransmitCallback);

/** Indicate that a PPP interface that was previously attached with
 * a call to uPortPppAttach() is now connected.  Internally
 * #uPortPppConnectCallback_t will be called.
 *
 * The application NEVER NEEDS to call this function; it is purely
 * for internal use within ubxlib.
 *
 * If a PPP interface is not supported by the platform this function
 * does not need to be implemented: a weakly-linked implementation
 * will take over and return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param[in] pDevHandle             the #uDeviceHandle_t of the device
 *                                   that is offering the PPP interface;
 *                                   this is a void * rather than a
 *                                   #uDeviceHandle_t here in order to
 *                                   avoid dragging in all of the uDevice
 *                                   types into the port layer.
 * @param[in] pIpAddress             the IP address, if already assigned,
 *                                   NULL if not; you do not need to provide
 *                                   this if you are sure that the PPP
 *                                   negotiation process will do so.
 * @param[in] pDnsIpAddressPrimary   the primary DNS address, if already
 *                                   known, NULL if not; currently only
 *                                   IPV4 addresses are supported.  You
 *                                   do not need to provide this if you
 *                                   are sure that the PPP negotiation
 *                                   process will do so.
 * @param[in] pDnsIpAddressSecondary the secondary DNS address, if already
 *                                   known, NULL if not; currently only
 *                                   IPV4 addresses are supported.  You do
 *                                   not need to provide this if you are
 *                                   sure that the PPP negotiation process
 *                                   will do so.
 * @param[in] pUsername              pointer to a string giving the user
 *                                   name for PPP authentication; should
 *                                   be set to NULL if no user name or
 *                                   password is required.  This value
 *                                   is currently IGNORED in the Zephyr
 *                                   case since the user name is hard-coded
 *                                   by Zephyr (see pap.c inside Zephyr).
 * @param[in] pPassword              pointer to a string giving the
 *                                   password for PPP authentication; must
 *                                   be non-NULL if pUsername is non-NULL,
 *                                   ignored if pUsername is NULL.  This
 *                                   value is currently IGNORED in the Zephyr
 *                                   case since the password is hard-coded
 *                                   by Zephyr (see pap.c inside Zephyr).
 * @param authenticationMode         the authentication mode, ignored if
 *                                   pUsername is NULL; ignored by Zephyr
 *                                   (PAP will be used if authentication is
 *                                   required).
 * @return                           zero on success, else negative error
 *                                   code.
 */
int32_t uPortPppConnect(void *pDevHandle,
                        uSockIpAddress_t *pIpAddress,
                        uSockIpAddress_t *pDnsIpAddressPrimary,
                        uSockIpAddress_t *pDnsIpAddressSecondary,
                        const char *pUsername,
                        const char *pPassword,
                        uPortPppAuthenticationMode_t authenticationMode);

/** Reconnect a PPP interface after it was lost due to, for instance,
 * a radio interface service loss.  Internally #uPortPppConnectCallback_t
 * will be called.
 *
 * The application NEVER NEEDS to call this function; it is purely
 * for internal use within ubxlib.
 *
 * If a PPP interface is not supported by the platform this function
 * does not need to be implemented: a weakly-linked implementation
 * will take over and return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param[in] pDevHandle             the #uDeviceHandle_t of the device
 *                                   that is offering the PPP interface;
 *                                   this is a void * rather than a
 *                                   #uDeviceHandle_t here in order to
 *                                   avoid dragging in all of the uDevice
 *                                   types into the port layer.
 * @param[in] pIpAddress             the IP address, if already assigned,
 *                                   NULL if not; you do not need to provide
 *                                   this if you are sure that the PPP
 *                                   negotiation process has done so.
 * @return                           zero on success, else negative error
 *                                   code.
 */
int32_t uPortPppReconnect(void *pDevHandle,
                          uSockIpAddress_t *pIpAddress);

/** Indicate that a PPP interface that was previously attached with
 * a call to uPortPppAttach() is going to be disconnected.  This
 * must be called by a ubxlib layer (e.g. cellular) that previous
 * called uPortPppConnect() _before_ that connection is brought down.
 * Internally it will cause #uPortPppDisconnectCallback_t to be called.
 *
 * The application NEVER NEEDS to call this function; it is purely
 * for internal use within ubxlib.
 *
 * When this function has returned, pReceiveCallback passed
 * to #uPortPppConnectCallback_t will no longer be called and any
 * pReceiveData buffer passed to #uPortPppConnectCallback_t will no
 * longer be written-to.
 *
 * If no PPP connection is open this function will do nothing.
 *
 * If a PPP interface is not supported by the platform this function
 * does not need to be implemented: a weakly-linked implementation
 * will take over and return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param[in] pDevHandle  the #uDeviceHandle_t of the device
 *                        that offered the PPP interface; this
 *                        is a void * rather than a #uDeviceHandle_t
 *                        in order to avoid dragging in all of the
 *                        uDevice types into the port layer.
 * @return                zero on success, else negative error code.
 */
int32_t uPortPppDisconnect(void *pDevHandle);

/** Detach a PPP interface from the bottom of a platform's IP stack.
 * #uPortPppDisconnectCallback_t will be called first.
 *
 * The application NEVER NEEDS to call this function; it is purely
 * for internal use within ubxlib.
 *
 * When this function has returned none of the callbacks passed to
 * uPortPppAttach() will be called any more.
 *
 * If no PPP connection is open this function will do nothing.
 *
 * If a PPP interface is not supported by the platform this function
 * does not need to be implemented: a weakly-linked implementation
 * will take over and return #U_ERROR_COMMON_NOT_SUPPORTED.
 *
 * @param[in] pDevHandle the #uDeviceHandle_t of the device that
 *                       originally called uPortPppAttach(); this
 *                       is a void * rather than a #uDeviceHandle_t
 *                       here in order to avoid dragging in all of
 *                       the uDevice types into the port layer.
 * @return               zero on success, else negative error code.
 */
int32_t uPortPppDetach(void *pDevHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_PPP_H_

// End of file
