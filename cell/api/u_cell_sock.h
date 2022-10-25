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

#ifndef _U_CELL_SOCK_H_
#define _U_CELL_SOCK_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the sockets APIs for cellular.
 * These functions are NOT thread-safe and are NOT intended to be
 * called directly.  Instead, please use the common/sock API which
 * wraps the functions exposed here to handle error/state checking
 * and re-entrancy.
 * Note that this socket implementation is always non-blocking,
 * the common/sock API provides blocking behaviour.
 * The functions in here are different to those in the rest of
 * the cellular API in that they return a negated value from the
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
 * single TCP segment sent to the cellular module (defined by the
 * cellular module AT interface).  Note the if hex mode is
  set (using uCellSockHexModeOn()) then the number is halved.
 */
#define U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES 1024

#ifndef U_CELL_SOCK_TCP_RETRY_LIMIT
/** The number of times to retry sending TCP data:
 * if the module is accepting less than
 * #U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES each time,
 * helps to prevent lock-ups.
 */
# define U_CELL_SOCK_TCP_RETRY_LIMIT 3
#endif

/** The maximum number of sockets that can be open at one time.
 */
#define U_CELL_SOCK_MAX_NUM_SOCKETS 7

#ifndef U_CELL_SOCK_CONNECT_TIMEOUT_SECONDS
/** The amount of time allowed to connect a socket.
 */
# define U_CELL_SOCK_CONNECT_TIMEOUT_SECONDS 30
#endif

#ifndef U_CELL_SOCK_DNS_LOOKUP_TIME_SECONDS
/** The amount of time allowed to perform a DNS look-up.
 */
# define U_CELL_SOCK_DNS_LOOKUP_TIME_SECONDS 60
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: INIT/DEINIT
 * -------------------------------------------------------------- */

/** Initialise the cellular sockets layer.  Must be called before
 * this sockets layer is used.  If this sockets layer is already
 * initialised then success is returned without any action being
 * taken.
 *
 * @return  zero on success else negated value of U_SOCK_Exxx
 *          from u_sock_errno.h.
 */
int32_t uCellSockInit();

/** Initialise the cellular instance.  Must be called before
 * any other calls are made on the given instance.  If the
 * instance is already initialised then success is returned
 * without any action being taken.
 *
 * @return  zero on success else negated value of U_SOCK_Exxx
 *          from u_sock_errno.h.
 */
int32_t uCellSockInitInstance(uDeviceHandle_t cellHandle);

/** Deinitialise the cellular sockets layer.  Should be called
 * when the cellular sockets layer is finished with.  May be called
 * multiple times with no ill effects. Does not close sockets,
 * you must do that.
 */
void uCellSockDeinit();

/* ----------------------------------------------------------------
 * FUNCTIONS: CREATE/OPEN/CLOSE/CLEAN-UP
 * -------------------------------------------------------------- */

/** Create a socket.  The local port number employed will be
 * assigned by the IP stack unless uCellSockSetNextLocalPort()
 * has been called.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param type        the type of socket to create.
 * @param protocol    the protocol that will run over the given
 *                    socket.
 * @return            the socket handle on success else
 *                    negated value of U_SOCK_Exxx from
 *                    u_sock_errno.h.
 */
int32_t uCellSockCreate(uDeviceHandle_t cellHandle,
                        uSockType_t type,
                        uSockProtocol_t protocol);

/** Connect to a server.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param sockHandle         the handle of the socket.
 * @param[in] pRemoteAddress the address of the server to
 *                           connect to, possibly established
 *                           via a call to uCellSockGetHostByName(),
 *                           including port number.
 * @return                   zero on success else negated
 *                           value of U_SOCK_Exxx from
 *                           u_sock_errno.h.
 */
int32_t uCellSockConnect(uDeviceHandle_t cellHandle,
                         int32_t sockHandle,
                         const uSockAddress_t *pRemoteAddress);

/** Close a socket.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param sockHandle     the handle of the socket.
 * @param[in] pCallback  sometimes closure of a TCP socket
 *                       can take many seconds due to the
 *                       requirement to wait for remaining
 *                       data and acknowledgements. This
 *                       call will block while closure is
 *                       completed unless pCallback is non-
 *                       NULL, in which case this function
 *                       will return and will call pCallback,
 *                       with the first parameter the cellHandle
 *                       and the second parameter the sockHandle,
 *                       when the socket is eventually closed.
 * @return               zero on success else negated
 *                       value of U_SOCK_Exxx from
 *                       u_sock_errno.h.
 */
int32_t uCellSockClose(uDeviceHandle_t cellHandle,
                       int32_t sockHandle,
                       void (*pCallback) (uDeviceHandle_t,
                                          int32_t));

/** Clean-up.  This function should be called when
 * there is no socket activity, either locally or from
 * the remote host, in order to free memory occupied
 * by closed sockets.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellSockCleanup(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURE
 * -------------------------------------------------------------- */

/** Set a socket to be blocking or non-blocking. This function
 * is provided for compatibility purposes only: this socket
 * implementation is always non-blocking.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @param isBlocking  true to set the socket to be
 *                    blocking, else false.
 */
void uCellSockBlockingSet(uDeviceHandle_t cellHandle,
                          int32_t sockHandle,
                          bool isBlocking);

/** Get whether a socket is blocking or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @return            true if the socket is blocking,
 *                    else false.
 */
bool uCellSockBlockingGet(uDeviceHandle_t cellHandle,
                          int32_t sockHandle);

/** Set socket option.  This function obeys
 * the BSD socket conventions and hence, for instance, to set
 * the socket receive timeout one would pass in a level
 * of #U_SOCK_OPT_LEVEL_SOCK, and option value of
 * #U_SOCK_OPT_RCVTIMEO and then the option value would be
 * a pointer to a structure of type timeval.
 *
 * @param cellHandle        the handle of the cellular instance.
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
int32_t uCellSockOptionSet(uDeviceHandle_t cellHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           const void *pOptionValue,
                           size_t optionValueLength);

/** Get socket option.
 *
 * @param cellHandle                 the handle of the cellular instance.
 * @param sockHandle                 the handle of the socket.
 * @param level                      the option level (see
 *                                   U_SOCK_OPT_LEVEL_xxx in u_sock.h).
 * @param option                     the option (see U_SOCK_OPT_xxx in u_sock.h).
 * @param[out] pOptionValue          a pointer to a place to put the option
 *                                   value. May be NULL in which case
 *                                   pOptionValueLength still returns the
 *                                   length that would have been written.
 * @param[in,out] pOptionValueLength when called, the length of the space
 *                                   pointed to by pOptionValue, on return
 *                                   the length of data in bytes that would
 *                                   be written in pOptionValue if it were
 *                                   not NULL.
 * @return                           zero on success else negated
 *                                   value of U_SOCK_Exxx from
 *                                   u_sock_errno.h.
 */
int32_t uCellSockOptionGet(uDeviceHandle_t cellHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           void *pOptionValue,
                           size_t *pOptionValueLength);

/** Apply a security profile to a socket.  The security
 * profile should have been configured as required (see
 * the uCellSecTls API) and this should be called before
 * uCellSockConnect() or uCellSockSendTo() is called.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @param profileId   the security profile ID to use.
 * @return            zero on success else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockSecure(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        int32_t profileId);

/** Switch on use of hex mode on the underlying AT
 * interface.  This is useful if your application sends
 * many short (e.g. 50 byte) messages in rapid succession.
 * When run in the default binary mode this driver has to
 * wait for a prompt character to arrive from the module
 * and then transmit the binary message and, though the
 * message is of optimal length (the hex mode message is
 * obviously twice as long) the per-packet latencies can
 * be 50 to 100 ms per packet, larger than the time it would
 * take to transmit the twice-as-long hex-coded message for
 * short message lengths, though it should be noted that
 * the code will allocate a temporary buffer in which to
 * store the hex encoded message. In hex mode the maximum
 * length of a datagram is halved.
 * Note that this will apply to ALL sockets but you may
 * switch modes at any time, e.g. you might do so based
 * on message size.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockHexModeOn(uDeviceHandle_t cellHandle);

/** Switch back to the default mode of sending packets
 * in binary form on the underlying AT interface.
 * Note that this will apply to ALL sockets.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockHexModeOff(uDeviceHandle_t cellHandle);

/** Determine whether hex mode (or conversely binary mode)
 * is in used on the underlying AT interface.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if hex mode is on, else false.
 */
bool uCellSockHexModeIsOn(uDeviceHandle_t cellHandle);

/** Set a local port which will be used on the next
 * uCellSockCreate(), otherwise the local port will be
 * chosen by the IP stack.  Once uCellSockCreate() has
 * been called, the local port will return to being
 * IP-stack-assigned once more. Obviously this is not
 * thread-safe, unless the caller makes it so through some
 * form of mutex protection at application level.  Specify
 * -1 to cancel a previously selected local port if you
 * change your mind.
 * NOTE: not all module types support setting the local
 * port (e.g. SARA-R4 doesn't).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param port        the uint16_t port number or -1 to
 *                    cancel a previous
 *                    uCellSockSetNextLocalPort() selection.
 * @return            zero on success else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockSetNextLocalPort(uDeviceHandle_t cellHandle,
                                  int32_t port);

/* ----------------------------------------------------------------
 * FUNCTIONS: UDP ONLY
 * -------------------------------------------------------------- */

/** Send a datagram.  The maximum length of datagram
 * that can be transmitted is #U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES
 * (or half this if hex mode is on): if dataSizeBytes is longer than
 * this nothing will be transmitted and an error will be returned.
 * Note that not all modules support use of uCellSockSendTo() on a
 * connected socket (e.g. SARA-R422 does not).
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param sockHandle         the handle of the socket.
 * @param[in] pRemoteAddress the address of the server to
 *                           send the datagram to, possibly
 *                           established via a call to
 *                           uCellSockGetHostByName(),
 *                           plus port number.  Cannot be NULL.
 * @param[in] pData          the data to send, may be NULL, in which
 *                           case this function does nothing.
 * @param dataSizeBytes      the number of bytes of data to send;
 *                           must be zero if pData is NULL.
 * @return                   the number of bytes sent on
 *                           success else negated value
 *                           of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockSendTo(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        const uSockAddress_t *pRemoteAddress,
                        const void *pData, size_t dataSizeBytes);

/** Receive a datagram.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param sockHandle          the handle of the socket.
 * @param[out] pRemoteAddress a place to put the address of the remote
 *                            host from which the datagram was received;
 *                            may be NULL.
 * @param[out] pData          a buffer in which to store the arriving
 *                            datagram.
 * @param dataSizeBytes       the number of bytes of storage available
 *                            at pData.  Each call receives a single
 *                            datagram of up to
 *                            #U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES (or
 *                            half that if in hex mode); if dataSizeBytes
 *                            is less than this the remainder will be
 *                            thrown away.  To ensure no loss always
 *                            allocate a buffer of at least
 *                            #U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES  (or
 *                            half that if in hex mode).
 * @return                    the number of bytes received else negated
 *                            value of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockReceiveFrom(uDeviceHandle_t cellHandle,
                             int32_t sockHandle,
                             uSockAddress_t *pRemoteAddress,
                             void *pData, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: STREAM (TCP)
 * -------------------------------------------------------------- */

/** Send bytes over a connected socket.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param sockHandle     the handle of the socket.
 * @param[in] pData      the data to send, may be NULL, in which
 *                       case this function does nothing.
 * @param dataSizeBytes  the number of bytes of data to send;
 *                       must be zero if pData is NULL.
 * @return               the number of bytes sent on
 *                       success else negated value
 *                       of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockWrite(uDeviceHandle_t cellHandle,
                       int32_t sockHandle,
                       const void *pData, size_t dataSizeBytes);

/** Receive bytes on a connected socket.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param sockHandle     the handle of the socket.
 * @param[out] pData     a buffer in which to store the received
 *                       bytes.
 * @param dataSizeBytes  the number of bytes of storage available
 *                       at pData.
 * @return               the number of bytes received else negated
 *                       value of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockRead(uDeviceHandle_t cellHandle,
                      int32_t sockHandle,
                      void *pData, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: ASYNC
 * -------------------------------------------------------------- */

/** Register a callback on data being received.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pCallback the callback to be called, or
 *                      NULL to cancel a previous callback.
 *                      The first parameter passed to the
 *                      callback will be cellHandle, the
 *                      second sockHandle.
 */
void uCellSockRegisterCallbackData(uDeviceHandle_t cellHandle,
                                   int32_t sockHandle,
                                   void (*pCallback) (uDeviceHandle_t,
                                                      int32_t));

/** Register a callback on a socket being closed.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param sockHandle    the handle of the socket.
 * @param[in] pCallback the callback to be called, or
 *                      NULL to cancel a previous callback.
 *                      The first parameter passed to the
 *                      callback will be cellHandle, the
 *                      second sockHandle.
 */
void uCellSockRegisterCallbackClosed(uDeviceHandle_t cellHandle,
                                     int32_t sockHandle,
                                     void (*pCallback) (uDeviceHandle_t,
                                                        int32_t));

/* ----------------------------------------------------------------
 * FUNCTIONS: TCP INCOMING (TCP SERVER) ONLY
 * -------------------------------------------------------------- */

/** Bind a socket to a local address for receiving
 * incoming TCP connections (required for a TCP server only).
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param sockHandle        the handle of the socket.
 * @param[in] pLocalAddress the local address to bind to.
 * @return                  zero on success else negated
 *                          value of U_SOCK_Exxx from
 *                          u_sock_errno.h.
 */
int32_t uCellSockBind(uDeviceHandle_t cellHandle,
                      int32_t sockHandle,
                      const uSockAddress_t *pLocalAddress);

/** Set listening mode (required for TCP server only).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @param backlog     the number of pending connections that
 *                    can be queued.
 * @return            zero on success else negated value of
 *                    U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockListen(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        size_t backlog);

/** Accept an incoming TCP connection (required for TCP
 * server only).
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param sockHandle          the handle of the socket.
 * @param[out] pRemoteAddress a pointer to a place to put the
 *                            address of the thing from which the
 *                            connection has been accepted.
 * @return                    the sockHandle to be used with this
 *                            connection from now on else negated
 *                            value of U_SOCK_Exxx from
 *                            u_sock_errno.h.
 */
int32_t uCellSockAccept(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        uSockAddress_t *pRemoteAddress);

/* ----------------------------------------------------------------
 * FUNCTIONS: FINDING ADDRESSES
 * -------------------------------------------------------------- */

/** Perform a DNS look-up.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param[in] pHostName       the host name to look up, for example
 *                            "google.com".
 * @param[out] pHostIpAddress a place to put the IP address
 *                            of the host.
 * @return                    zero on success else negated
 *                            value of U_SOCK_Exxx from
 *                            u_sock_errno.h.
 */
int32_t uCellSockGetHostByName(uDeviceHandle_t cellHandle,
                               const char *pHostName,
                               uSockIpAddress_t *pHostIpAddress);

/** Get the local address of a socket.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param sockHandle         the handle of the socket.
 * @param[out] pLocalAddress a place to put the local IP address
 *                           of the socket.
 * @return                   zero on success else negated
 *                           value of U_SOCK_Exxx from
 *                           u_sock_errno.h.
 */
int32_t uCellSockGetLocalAddress(uDeviceHandle_t cellHandle,
                                 int32_t sockHandle,
                                 uSockAddress_t *pLocalAddress);

/* ----------------------------------------------------------------
 * FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

/** Get the last error on the given socket.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @return            if a positive value then this is the last error
 *                    code (refer to the "Internal TCP/UDP/IP stack
 *                    class error codes" appendix of the AT manual
 *                    for your module to determine what this means);
 *                    if a negative value then the last error code
 *                    could not be obtained.
 */
int32_t uCellSockGetLastError(uDeviceHandle_t cellHandle,
                              int32_t sockHandle);

/** Get the number of bytes sent on the given socket.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @return            the number of bytes sent, else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockGetBytesSent(uDeviceHandle_t cellHandle,
                              int32_t sockHandle);

/** Get the number of bytes received on the given socket.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param sockHandle  the handle of the socket.
 * @return            the number of bytes received, else negated value
 *                    of U_SOCK_Exxx from u_sock_errno.h.
 */
int32_t uCellSockGetBytesReceived(uDeviceHandle_t cellHandle,
                                  int32_t sockHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_SOCK_H_

// End of file
