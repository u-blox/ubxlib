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

#ifndef _U_WIFI_SOCK_H_
#define _U_WIFI_SOCK_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the sockets APIs for wifi.
 * These functions are NOT thread-safe and are NOT intended to be
 * called directly.  Instead, please use the common/sock API which
 * wraps the functions exposed here to handle error/state checking
 * and re-entrancy.
 * Note that this socket implementation is always non-blocking,
 * the common/sock API provides blocking behaviour.
 * The functions in here are different to those in the rest of
 * the wifi API in that they return a negated value from the
 * errno values in u_sock_errno.h (e.g. -#U_SOCK_ENOMEM) instead
 * of a value from u_error_common.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum size of a datagram and the maximum size of a
 * single TCP segment sent to the wifi module.
 */
#define U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES 1024

#ifndef U_WIFI_SOCK_TCP_RETRY_LIMIT
/** The number of times to retry sending TCP data:
 * if the module is accepting less than
 * #U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES each time,
 * helps to prevent lock-ups.
 */
# define U_WIFI_SOCK_TCP_RETRY_LIMIT 3
#endif

/** The maximum number of sockets that can be open at one time.
 */
#define U_WIFI_SOCK_MAX_NUM_SOCKETS 7

/** The maximum number of connections that can be open at one time.
 */
#define U_WIFI_SOCK_MAX_NUM_CONNECTIONS 7

#ifndef U_WIFI_SOCK_CONNECT_TIMEOUT_SECONDS
/** The amount of time allowed to connect a socket.
 */
# define U_WIFI_SOCK_CONNECT_TIMEOUT_SECONDS 30
#endif

#ifndef U_WIFI_SOCK_DNS_LOOKUP_TIME_SECONDS
/** The amount of time allowed to perform a DNS look-up.
 */
# define U_WIFI_SOCK_DNS_LOOKUP_TIME_SECONDS 60
#endif


/** Size of receive buffer for a connected data channel
 */
#ifndef U_WIFI_SOCK_BUFFER_SIZE
#define U_WIFI_SOCK_BUFFER_SIZE 2048
#endif

#ifndef U_WIFI_SOCK_WRITE_TIMEOUT_MS
#define U_WIFI_SOCK_WRITE_TIMEOUT_MS 500
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef void (*uWifiSockCallback_t)(uDeviceHandle_t devHandle, int32_t sockHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: INIT/DEINIT
 * -------------------------------------------------------------- */

/** Initialise the wifi sockets layer.  Must be called before
 * this sockets layer is used.  If this sockets layer is already
 * initialised then success is returned without any action being
 * taken.
 *
 * @return  zero on success else negated value of U_SOCK_Exxx
 *          from u_sock_errno.h.
 */
int32_t uWifiSockInit(void);

/** Deinitialise the wifi sockets layer.  Should be called
 * when the wifi sockets layer is finished with.  May be called
 * multiple times with no ill effects. Does not close sockets,
 * you must do that.
 */
void uWifiSockDeinit();

/** Initialise the wifi instance.  Must be called before
 * any other calls are made on the given instance.
 *
 * @return  zero on success else negated value of U_SOCK_Exxx
 *          from u_sock_errno.h.
 */
int32_t uWifiSockInitInstance(uDeviceHandle_t devHandle);

/** Deinitialise the wifi instance.  Must be called before
 * uWifiSockDeinit().
 */
int32_t uWifiSockDeinitInstance(uDeviceHandle_t devHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CREATE/OPEN/CLOSE/CLEAN-UP
 * -------------------------------------------------------------- */

/** Create a socket.  The local port number employed will be
 * assigned by the IP stack unless uWifiSockSetNextLocalPort()
 * has been called.
 *
 * @param devHandle the handle of the wifi instance.
 * @param type      the type of socket to create.
 * @param protocol  the protocol that will run over the given
 *                  socket.
 * @return          the socket handle on success else
 *                  negated value of U_SOCK_Exxx from
 *                  u_sock_errno.h.
 */
int32_t uWifiSockCreate(uDeviceHandle_t devHandle,
                        uSockType_t type,
                        uSockProtocol_t protocol);

/** Connect to a server by IP address
 *
 * @param devHandle          the handle of the wifi instance.
 * @param sockHandle         the handle of the socket.
 * @param[in] pRemoteAddress the address of the server to
 *                           connect to, possibly established
 *                           via a call to uWifiSockGetHostByName(),
 *                           including port number.
 * @return                   zero on success else negated
 *                           value of U_SOCK_Exxx from
 *                           u_sock_errno.h.
 */
int32_t uWifiSockConnect(uDeviceHandle_t devHandle,
                         int32_t sockHandle,
                         const uSockAddress_t *pRemoteAddress);

/** Close a socket.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pCallback sometimes closure of a TCP socket
 *                      can take many seconds due to the
 *                      requirement to wait for remaining
 *                      data and acknowledgements. This
 *                      call will block while closure is
 *                      completed unless pCallback is non-
 *                      NULL, in which case this function
 *                      will return and will call pCallback,
 *                      with the first parameter the devHandle
 *                      and the second parameter the sockHandle,
 *                      when the socket is eventually closed.
 * @return              zero on success else negated
 *                      value of U_SOCK_Exxx from
 *                      u_sock_errno.h.
 */
int32_t uWifiSockClose(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       uWifiSockCallback_t pCallback);

/** Clean-up.  This function should be called when
 * there is no socket activity, either locally or from
 * the remote host, in order to free memory occupied
 * by closed sockets.
 *
 * @param devHandle the handle of the wifi instance.
 */
void uWifiSockCleanup(uDeviceHandle_t devHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURE
 * -------------------------------------------------------------- */

/** Set a socket to be blocking or non-blocking. This function
 * is provided for compatibility purposes only: this socket
 * implementation is always non-blocking.
 *
 * @param devHandle  the handle of the wifi instance.
 * @param sockHandle the handle of the socket.
 * @param isBlocking true to set the socket to be
 *                   blocking, else false.
 */
void uWifiSockBlockingSet(uDeviceHandle_t devHandle,
                          int32_t sockHandle,
                          bool isBlocking);

/** Get whether a socket is blocking or not.
 *
 * @param devHandle  the handle of the wifi instance.
 * @param sockHandle the handle of the socket.
 * @return           true if the socket is blocking,
 *                   else false.
 */
bool uWifiSockBlockingGet(uDeviceHandle_t devHandle,
                          int32_t sockHandle);

/** Set socket option.  This function obeys
 * the BSD socket conventions and hence, for instance, to set
 * the socket receive timeout one would pass in a level
 * of #U_SOCK_OPT_LEVEL_SOCK, and option value of
 * #U_SOCK_OPT_RCVTIMEO and then the option value would be
 * a pointer to a structure of type timeval.
 *
 * @param devHandle         the handle of the wifi instance.
 * @param sockHandle        the handle of the socket.
 * @param level             the option level
 *                          (see U_SOCK_OPT_LEVEL_xxx in u_sock.h).
 * @param option            the option (see U_SOCK_OPT_xxx in u_sock.h).
 * @param[in] pOptionValue  a pointer to the option value to set.
 * @param optionValueLength the length of the data at pOptionValue.
 * @return                  zero on success else negated
 *                          value of U_SOCK_Exxx from
 *                          u_sock_errno.h.
 */
int32_t uWifiSockOptionSet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           const void *pOptionValue,
                           size_t optionValueLength);

/** Get socket option.
 *
 * @param devHandle          the handle of the wifi instance.
 * @param sockHandle         the handle of the socket.
 * @param level              the option level (see
 *                           U_SOCK_OPT_LEVEL_xxx in u_sock.h).
 * @param option             the option (see U_SOCK_OPT_xxx in u_sock.h).
 * @param[out] pOptionValue  a pointer to a place to put the option
 *                           value. May be NULL in which case
 *                           pOptionValueLength still returns the
 *                           length that would have been written.
 * @param[in,out] pOptionValueLength when called, the length of the space
 *                           pointed to by pOptionValue, on return
 *                           the length of data in bytes that would
 *                           be written in pOptionValue if it were
 *                           not NULL.
 * @return                   zero on success else negated
 *                           value of U_SOCK_Exxx from
 *                           u_sock_errno.h.
 */
int32_t uWifiSockOptionGet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           void *pOptionValue,
                           size_t *pOptionValueLength);

/** Set a local port which will be used on the next uWifiSockCreate(),
 * otherwise a local port will be chosen by the IP stack.  Once
 * uWifiSockCreate() has been called, the local port will return to
 * being IP-stack-assigned once more.  Obviously this is not
 * thread-safe, unless the caller makes it so through some form of
 * mutex protection at application level.  Specify -1 to cancel a
 * previously selected local port if you change your mind.
 *
 * @param devHandle   the handle of the wifi instance.
 * @param port        the uint16_t port number or -1 to cancel a previous
 *                    uWifiSockSetNextLocalPort() selection.
 * @return            zero on success else negative error code (and
 *                    errno will also be set to a value from
 *                    u_sock_errno.h).
 */
int32_t uWifiSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port);

/* ----------------------------------------------------------------
 * FUNCTIONS: UDP ONLY
 * -------------------------------------------------------------- */

/** Send a datagram to IP address.
 *
 * The maximum length of datagram that can be transmitted is
 * #U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES; if dataSizeBytes is
 * longer than this nothing will be transmitted and an error
 * will be returned.
 *
 * @param devHandle          the handle of the wifi instance.
 * @param sockHandle         the handle of the socket.
 * @param[in] pRemoteAddress the address of the server to
 *                           send the datagram to, possibly
 *                           established via a call to
 *                           uWifiSockGetHostByName(),
 *                           plus port number.  Cannot be NULL.
 * @param[in] pData          the data to send, may be NULL, in which
 *                           case this function does nothing.
 * @param dataSizeBytes      the number of bytes of data to send;
 *                           must be zero if pData is NULL.
 * @return                   the number of bytes sent on
 *                           success else negated value
 *                           of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uWifiSockSendTo(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        const uSockAddress_t *pRemoteAddress,
                        const void *pData,
                        size_t dataSizeBytes);

/** Receive a datagram from IP address.
 *
 *  NOTE: Short range modules have very limited UDP support and can
 *        only receive packets from one IP adress that have been
 *        previously setup. This means that either uWifiSockSendTo()
 *        or uWifiSockConnect() must have been called before using
 *        this function.
 *
 * @param devHandle           the handle of the wifi instance.
 * @param sockHandle          the handle of the socket.
 * @param[out] pRemoteAddress a place to put the address of the remote
 *                            host from which the datagram was received;
 *                            may be NULL.
 * @param[in] pData           a buffer in which to store the arriving
 *                            datagram.
 * @param dataSizeBytes       the number of bytes of storage available
 *                            at pData.  Each call receives a single
 *                            datagram of up to
 *                            #U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES; if
 *                            dataSizeBytes is less than this the
 *                            remainder will be thrown away.  To ensure
 *                            no loss always allocate a buffer of
 *                            at least #U_WIFI_SOCK_MAX_SEGMENT_SIZE_BYTES.
 * @return                    the number of bytes received else negated
 *                            value of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uWifiSockReceiveFrom(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             uSockAddress_t *pRemoteAddress,
                             void *pData, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: STREAM (TCP)
 * -------------------------------------------------------------- */

/** Send bytes over a connected socket.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pData     the data to send, may be NULL, in which
 *                      case this function does nothing.
 * @param dataSizeBytes the number of bytes of data to send;
 *                      must be zero if pData is NULL.
 * @return              the number of bytes sent on
 *                      success else negated value
 *                      of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uWifiSockWrite(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       const void *pData, size_t dataSizeBytes);

/** Receive bytes on a connected socket.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param sockHandle    the handle of the socket.
 * @param[out] pData    a buffer in which to store the received
 *                      bytes.
 * @param dataSizeBytes the number of bytes of storage available
 *                      at pData.
 * @return              the number of bytes received else negated
 *                      value of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uWifiSockRead(uDeviceHandle_t devHandle,
                      int32_t sockHandle,
                      void *pData, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: ASYNC
 * -------------------------------------------------------------- */

/** Register a callback on data being received.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pCallback the callback to be called, or
 *                      NULL to cancel a previous callback.
 *                      The first parameter passed to the
 *                      callback will be devHandle, the
 *                      second sockHandle.
 * @return              zero on success else negated
 *                      value of U_SOCK_Exxx from
 *                      u_sock_errno.h.
 */
int32_t uWifiSockRegisterCallbackData(uDeviceHandle_t devHandle,
                                      int32_t sockHandle,
                                      uWifiSockCallback_t pCallback);

/** Register a callback on a socket being closed.
 *
 * @param devHandle     the handle of the wifi instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pCallback the callback to be called, or
 *                      NULL to cancel a previous callback.
 *                      The first parameter passed to the
 *                      callback will be devHandle, the
 *                      second sockHandle.
 * @return              zero on success else negated
 *                      value of U_SOCK_Exxx from
 *                      u_sock_errno.h.
 */
int32_t uWifiSockRegisterCallbackClosed(uDeviceHandle_t devHandle,
                                        int32_t sockHandle,
                                        uWifiSockCallback_t pCallback);

/* ----------------------------------------------------------------
 * FUNCTIONS: TCP INCOMING (TCP SERVER) ONLY - Not implemeneted yet!
 * -------------------------------------------------------------- */

/** Bind a socket to a local address for receiving
 * incoming TCP connections (required for a TCP server only).
 *
 * NOTE: Not implemented
 *
 * @param devHandle         the handle of the wifi instance.
 * @param sockHandle        the handle of the socket.
 * @param[in] pLocalAddress the local address to bind to.
 * @return                  zero on success else negated
 *                          value of U_SOCK_Exxx from
 *                          u_sock_errno.h.
 */
int32_t uWifiSockBind(uDeviceHandle_t devHandle,
                      int32_t sockHandle,
                      const uSockAddress_t *pLocalAddress);

/** Set listening mode (required for TCP server only).
 *
 * NOTE: Not implemented
 *
 * @param devHandle  the handle of the wifi instance.
 * @param sockHandle the handle of the socket.
 * @param backlog    the number of pending connections that
 *                   can be queued.
 * @return           zero on success else negated value of
 *                   U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uWifiSockListen(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        size_t backlog);

/** Accept an incoming TCP connection (required for TCP
 * server only).
 *
 * NOTE: Not implemented
 *
 * @param devHandle           the handle of the wifi instance.
 * @param sockHandle          the handle of the socket.
 * @param[out] pRemoteAddress a pointer to a place to put the
 *                            address of the thing from which the
 *                            connection has been accepted.
 * @return                    the sockHandle to be used with this
 *                            connection from now on else negated
 *                            value of U_SOCK_Exxx from
 *                            u_sock_errno.h.
 */
int32_t uWifiSockAccept(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        uSockAddress_t *pRemoteAddress);

/* ----------------------------------------------------------------
 * FUNCTIONS: FINDING ADDRESSES
 * -------------------------------------------------------------- */

/** Perform a DNS look-up.
 *
 * @param devHandle           the handle of the wifi instance.
 * @param[in] pHostName       the host name to look up, e.g.
 *                            "google.com".
 * @param[out] pHostIpAddress a place to put the IP address
 *                            of the host.
 * @return                    zero on success else negated
 *                            value of U_SOCK_Exxx from
 *                            u_sock_errno.h.
 */
int32_t uWifiSockGetHostByName(uDeviceHandle_t devHandle,
                               const char *pHostName,
                               uSockIpAddress_t *pHostIpAddress);

/** Get the local address of a socket.
 *
 * @param devHandle          the handle of the wifi instance.
 * @param sockHandle         the handle of the socket.
 * @param[out] pLocalAddress a place to put the local IP address
 *                           of the socket.
 * @return                   zero on success else negated
 *                           value of U_SOCK_Exxx from
 *                           u_sock_errno.h.
 */
int32_t uWifiSockGetLocalAddress(uDeviceHandle_t devHandle,
                                 int32_t sockHandle,
                                 uSockAddress_t *pLocalAddress);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_SOCK_H_

// End of file
