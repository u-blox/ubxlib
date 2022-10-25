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

#ifndef _U_SOCK_H_
#define _U_SOCK_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h" // uDeviceHandle_t

/** \addtogroup sock Sockets
 *  @{
 */

/** @file
 * @brief This header file defines the sockets API. These functions are
 * thread-safe with the exception of uSockSetNextLocalPort().
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: DEFAULTS FOR IMPLEMENTATION MACROS
 * -------------------------------------------------------------- */

#ifndef U_SOCK_MAX_NUM_SOCKETS
/** A value for the maximum number of sockets that can be open
 * simultaneously is required by this API in order that if can
 * define #U_SOCK_DESCRIPTOR_SET_SIZE.  A limitation may also be
 * applied by the underlying implementation.
 */
# define U_SOCK_MAX_NUM_SOCKETS 7
#endif

#ifndef U_SOCK_DEFAULT_RECEIVE_TIMEOUT_MS
/** The default receive timeout for a socket in milliseconds.
 */
# define U_SOCK_DEFAULT_RECEIVE_TIMEOUT_MS 10000
#endif

#ifndef U_SOCK_RECEIVE_POLL_INTERVAL_MS
/** The interval at which this layer hits the underlying
 * network layer with a request for incoming data while
 * waiting for the uSockReceiveFrom() or uSockRead().
 * This also represents the minimum time these calls will
 * take in the non-blocking case.
 */
# define U_SOCK_RECEIVE_POLL_INTERVAL_MS 100
#endif

#ifndef U_SOCK_CLOSE_TIMEOUT_SECONDS
/** The time permitted for a socket to be closed in seconds.
 * This can be quite long when strictly adhering to the socket
 * closure rules for TCP sockets (when no asynchronous callback
 * is provided by the underlying socket layer).  The
 * SARA-R4 cellular module requires a timeout of more than 35
 * seconds.
 */
# define U_SOCK_CLOSE_TIMEOUT_SECONDS 60
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: SOCKET OPTIONS FOR SOCKET LEVEL (-1)
 * -------------------------------------------------------------- */

/** The level for socket options. The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_LEVEL_SOCK   0x0fff

/** Socket option: turn on debugging info recording.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_SO_DEBUG     0x0001

/** Socket option: socket has had listen(). The value matches
 * LWIP which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_ACCEPTCONN   0x0002

/** Socket option: allow local address re-use. The value
 * matches LWIP which matches the BSD sockets API (see
 * Stevens et al).
 */
#define U_SOCK_OPT_REUSEADDR    0x0004

/** Socket option: keep connections alive. The value matches
 * LWIP which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_KEEPALIVE    0x0008

/** Socket option: just use interface addresses.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_DONTROUTE    0x0010

/** Socket option: permit sending of broadcast messages.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_BROADCAST    0x0020

/** Socket option: linger on close if data present.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_LINGER       0x0080

/** Socket option: leave received OOB data in line.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_OOBINLINE    0x0100

/** Socket option: allow local address and port re-use.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_REUSEPORT    0x0200

/** Socket option: send buffer size. The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_SNDBUF       0x1001

/** Socket option: receive buffer size. The value matches
 * LWIP which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_RCVBUF       0x1002

/** Socket option: send low-water mark. The value matches
 * LWIP which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_SNDLOWAT     0x1003

/** Socket option: receive low-water mark. The value matches
 * LWIP which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_RCVLOWAT     0x1004

/** Socket option: send timeout. The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_SNDTIMEO     0x1005

/** Socket option: receive timeout.  The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_RCVTIMEO     0x1006

/** Socket option: get and then clear error status.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_ERROR        0x1007

/** Socket option: get socekt type.  The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_TYPE         0x1008

/** Socket option: connect timeout.  The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_CONTIMEO     0x1009

/** Socket option: don't create UDP checksum.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_NO_CHECK     0x100a

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: SOCKET OPTIONS FOR IP LEVEL (0)
 * -------------------------------------------------------------- */

/** The level for IP options. The value matches LWIP which
 * matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_LEVEL_IP     0

/** IP socket option: type of service. The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_IP_TOS       0x0001

/** IP socket option: time to live. The value matches LWIP
 * which matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_IP_TTL       0x0002

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: SOCKET OPTIONS FOR TCP LEVEL (6)
 * -------------------------------------------------------------- */

/** The level for TCP options. The value matches LWIP which
 * matches the BSD sockets API (see Stevens et al).
 */
#define U_SOCK_OPT_LEVEL_TCP     6

/** TCP socket option: turn off Nagle's algorithm.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_TCP_NODELAY  0x0001

/** TCP socket option: send keepidle probes when it is idle
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_TCP_KEEPIDLE 0x0002

/** TCP socket option: time in seconds between two successive
 * keepalive retransmissions.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_TCP_KEEPINTVL 0x0004

/** TCP socket option: the number of retransmissions to be
 * sent before disconnecting the remote end.
 * The value matches LWIP which matches the BSD sockets API
 * (see Stevens et al).
 */
#define U_SOCK_OPT_TCP_KEEPCNT  0x0005

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: MISC
 * -------------------------------------------------------------- */

/** The size that should be allowed for an address string,
 * which could be IPV6 and could include a port number; this
 * includes room for a null terminator.
 */
#define U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES 64

/** The maximum number of sockets that can be uSockSelect()ed
 * from.  Note that increasing this may increase stack usage
 * as applications normally declare their descriptor sets as
 * automatic variables.  A size of 256 will be 256 / 8 = 32 bytes.
 */
#define U_SOCK_DESCRIPTOR_SET_SIZE U_SOCK_MAX_NUM_SOCKETS

/** The default socket timeout in milliseconds.
 */
#define U_SOCK_RECEIVE_TIMEOUT_DEFAULT_MS 10000

/** Zero a file descriptor set.
 */
#define U_SOCK_FD_ZERO(pSet) memset(*(pSet), 0, sizeof(*(pSet)))

/** Set the bit corresponding to a given file descriptor in a set.
 */
#define U_SOCK_FD_SET(d, pSet) if (((d) >= 0) &&                          \
                                   ((d) < U_SOCK_DESCRIPTOR_SET_SIZE)) {  \
                                   (*(pSet))[(d) / 8] |= 1 << ((d) & 7);  \
                               }

/** Clear the bit corresponding to a given file descriptor in a set.
 */
#define U_SOCK_FD_CLR(d, pSet) if (((d) >= 0) &&                             \
                                   ((d) < U_SOCK_DESCRIPTOR_SET_SIZE)) {     \
                                   (*(pSet))[(d) / 8] &= ~(1 << ((d) & 7));  \
                               }

/** Determine if the bit corresponding to a given file descriptor is set.
 */
#define U_SOCK_FD_ISSET(d, pSet) if (((d) >= 0) &&                           \
                                     ((d) < U_SOCK_DESCRIPTOR_SET_SIZE)) {   \
                                     (*(pSet))[(d) / 8] & (1 << ((d) & 7));  \
                                 }

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Socket descriptor.
 */
typedef int32_t uSockDescriptor_t;

/** A socket descriptor set, for use with uSockSelect().
 */
typedef uint8_t uSockDescriptorSet_t[(U_SOCK_DESCRIPTOR_SET_SIZE + 7) / 8];

/** Supported socket types: the numbers match those of LWIP.
 */
typedef enum {
    U_SOCK_TYPE_NONE   = 0,
    U_SOCK_TYPE_STREAM = 1, //<! TCP.
    U_SOCK_TYPE_DGRAM  = 2  //<! UDP.
} uSockType_t;

/** Supported protocols: the numbers match those of LWIP.
 */
typedef enum {
    U_SOCK_PROTOCOL_TCP     = 6,
    U_SOCK_PROTOCOL_UDP     = 17
} uSockProtocol_t;

/** IP address type: the numbers match those of LWIP.
 */
typedef enum {
    U_SOCK_ADDRESS_TYPE_V4    = 0,
    U_SOCK_ADDRESS_TYPE_V6    = 6,
    U_SOCK_ADDRESS_TYPE_V4_V6 = 46
} uSockIpAddressType_t;

/** IP address (doesn't include port number).
 */
typedef struct {
    uSockIpAddressType_t type; //<! Do NOT use
    // U_SOCK_ADDRESS_TYPE_V4_V6
    // here!
    union {
        uint32_t ipv4;
        uint32_t ipv6[4];
    } address;
} uSockIpAddress_t;

/** Address (includes port number).
 */
typedef struct {
    uSockIpAddress_t ipAddress;
    uint16_t port;
} uSockAddress_t;

/** Socket shut-down types: the numbers match those of LWIP.
 */
typedef enum {
    U_SOCK_SHUTDOWN_READ = 0,
    U_SOCK_SHUTDOWN_WRITE = 1,
    U_SOCK_SHUTDOWN_READ_WRITE = 2
} uSockShutdown_t;

/** Struct to define the U_SOCK_OPT_LINGER socket option.
 * This struct matches that of LWIP.
 */
typedef struct {
    int32_t onNotOff;       //<! option on/off.
    int32_t lingerSeconds;  //<! linger time in seconds.
} uSockLinger_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: CREATE/OPEN/CLOSE/CLEAN-UP
 * -------------------------------------------------------------- */

/** Create a socket.  The local port number employed will be
 * assigned by the IP stack unless uSockSetNextLocalPort()
 * has been called.
 *
 * @param devHandle      the handle of the underlying network
 *                       layer to use, usually established by
 *                       a call to uDeviceOpen().
 * @param type           the type of socket to create.
 * @param protocol       the protocol that will run over the given
 *                       socket.
 * @return               the descriptor of the socket else negative
 *                       error code (and errno will also be set to
 *                       a value from u_sock_errno.h).
 */
int32_t uSockCreate(uDeviceHandle_t devHandle, uSockType_t type,
                    uSockProtocol_t protocol);

/** Make an outgoing connection on the given socket.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pRemoteAddress the address of the remote host to connect
 *                       to.
 * @return               zero on success else negative error code
 *                       (and errno will also be set to a value
 *                       from u_sock_errno.h).
 */
int32_t uSockConnect(uSockDescriptor_t descriptor,
                     const uSockAddress_t *pRemoteAddress);

/** Close a socket.  Note that a TCP socket should be shutdown
 * with a call to uSockShutdown() before it is closed. Note that
 * in some cases where TCP socket closure can take a considerable
 * time (due to strict adherence to the TCP protocol rules) the
 * socket may not be actually closed when this function returns,
 * it could be closed some 10's of seconds later.  If it is
 * important to know when the socket is closed, register a
 * call-back using uSockRegisterCallbackClosed() before calling
 * uSockClose().  Also note that closing the socket does NOT
 * free the memory it occupied, see uSockCleanUp() for that.
 *
 * @param descriptor the descriptor of the socket to be closed.
 * @return           zero on success else negative error code
 *                   (and errno will also be set to a value
 *                   from u_sock_errno.h).
 */
int32_t uSockClose(uSockDescriptor_t descriptor);

/** In order to maintain thread-safe operation, when a socket is
 * closed, either locally or by the remote host, it is only marked
 * as closed and the memory is retained, since some other thread
 * may be refering to it.  You should call this clean-up function
 * when you are sure that there is no socket activity, either
 * locally or from the remote host, in order to free memory
 * allocated for sockets.  A socket that is closed locally but
 * waiting for the far end to close WILL be cleaned-up by this
 * function and so no callback registered by
 * uSockRegisterCallbackClosed() will be triggered when the
 * remote server finally closes the connection.
 */
void uSockCleanUp();

/** Sockets has no initialisation function, it initialises
 * itself as required on any call.  If the application requires
 * sockets to be shut down in an organised way, with all sockets
 * closed locally, it can be done by calling this function.
 * It is different from uSockCleanUp() in that all sockets,
 * whatever their state, are closed locally.
 */
void uSockDeinit();

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURE
 * -------------------------------------------------------------- */

/** Set a socket to be blocking or non-blocking.
 *
 * @param descriptor the descriptor of the socket.
 * @param isBlocking set to true if the socket is to be blocking,
 *                   set to false if the socket is to be non-blocking.
 */
void uSockBlockingSet(uSockDescriptor_t descriptor, bool isBlocking);

/** Get whether a socket is blocking or not.
 *
 * @param descriptor the descriptor of the socket.
 * @return           true if the socket is blocking, else false.
 */
bool uSockBlockingGet(uSockDescriptor_t descriptor);

/** Set the options for the given socket.  This function obeys
 * the BSD socket conventions and hence, for instance, to set
 * the socket receive timeout one would pass in a level
 * of #U_SOCK_OPT_LEVEL_SOCK, and option value of
 * #U_SOCK_OPT_RCVTIMEO and then the option value would be
 * a pointer to a structure of type timeval.
 *
 * @param descriptor        the descriptor of the socket.
 * @param level             the option level
 *                          (see U_SOCK_OPT_LEVEL_xxx).
 * @param option            the option (see U_SOCK_OPT_xxx).
 * @param pOptionValue      a pointer to the option value to set.
 * @param optionValueLength the length of the data at pOptionValue.
 * @return                  zero on success else negative error code
 *                          (and errno will also be set to a value
 *                          from u_sock_errno.h).
 */
int32_t uSockOptionSet(uSockDescriptor_t descriptor,
                       int32_t level, uint32_t option,
                       const void *pOptionValue,
                       size_t optionValueLength);

/** Get the options for the given socket.
 *
 * @param descriptor         the descriptor of the socket.
 * @param level              the option level
 *                           (see U_SOCK_OPT_LEVEL_xxx).
 * @param option             the option (see U_SOCK_OPT_xxx).
 * @param pOptionValue       a pointer to a place to put the option value.
 *                           May be NULL in which case pOptionValueLength
 *                           still returns the length that would have
 *                           been written.
 * @param pOptionValueLength when called, the length of the space pointed
 *                           to by pOptionValue, on return the length
 *                           of data in bytes that would be written to
 *                           pOptionValue if it were not NULL.
 * @return                   zero on success else negative error code
 *                           (and errno will also be set to a value
 *                           from u_sock_errno.h).
 */
int32_t uSockOptionGet(uSockDescriptor_t descriptor,
                       int32_t level, uint32_t option,
                       void *pOptionValue,
                       size_t *pOptionValueLength);

/** Set a local port which will be used on the next uSockCreate(),
 * otherwise a local port will be chosen by the IP stack.  This is
 * useful if you, for instance, need to maintain a DTLS connection
 * across IP connections.  Once uSockCreate() has been called, the local
 * port will return to being IP-stack-assigned once more.  Obviously this
 * is not thread-safe, unless the caller makes it so through some form
 * of mutex protection at application level.  Specify -1 to cancel a
 * previously selected local port if you change your mind.
 * NOTE: not all module types support setting the local port (e.g.
 * cellular SARA-R4 doesn't).
 *
 * @param devHandle   the handle of the underlying network layer to
 *                    use, usually established by a call to uDeviceOpen().
 * @param port        the uint16_t port number or -1 to cancel a previous
 *                    uSockSetNextLocalPort() selection.
 * @return            zero on success else negative error code (and
 *                    errno will also be set to a value from
 *                    u_sock_errno.h).
 */
int32_t uSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port);

/* ----------------------------------------------------------------
 * FUNCTIONS: UDP ONLY
 * -------------------------------------------------------------- */

/** Send a datagram to the given host.  Note that some modules,
 * e.g. SARA-R422, do not allow datagrams to be sent over a connected
 * socket.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pRemoteAddress the address of the remote host to send to;
 *                       may be NULL in which case the address
 *                       from the uSockConnect() call
 *                       is used (and if no uSockConnect()
 *                       has been called on the socket this will
 *                       fail).
 * @param pData          the data to send, may be NULL, in which
 *                       case this function does nothing.
 * @param dataSizeBytes  the number of bytes of data to send;
 *                       must be zero if pData is NULL.
 * @return               on success the number of bytes sent else
 *                       negative error code (and errno will also
 *                       be set to a value from u_sock_errno.h).
 */
int32_t uSockSendTo(uSockDescriptor_t descriptor,
                    const uSockAddress_t *pRemoteAddress,
                    const void *pData, size_t dataSizeBytes);

/** Receive a single datagram from the given host.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pRemoteAddress a place to put the address of the remote
 *                       host from which the datagram was received;
 *                       may be NULL.
 * @param pData          a buffer in which to store the arriving
 *                       datagram.
 * @param dataSizeBytes  the number of bytes of storage available
 *                       at pData.  Each call receives a single
 *                       datagram of a maximum length that can
 *                       be retrieved using the #U_SOCK_OPT_RCVBUF
 *                       socket option; if dataSizeBytes is less
 *                       than this size the remainder will be thrown
 *                       away so, to ensure no loss, always
 *                       allocate a buffer of at least the value
 *                       returned by #U_SOCK_OPT_RCVBUF.
 * @return               on success the number of bytes received
 *                       else negative error code (and errno will
 *                       also be set to a value from u_sock_errno.h).
 */
int32_t uSockReceiveFrom(uSockDescriptor_t descriptor,
                         uSockAddress_t *pRemoteAddress,
                         void *pData, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: STREAM (TCP)
 * -------------------------------------------------------------- */

/** Send data.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pData          the data to send.
 * @param dataSizeBytes  the number of bytes of data to send.
 * @return               on success the number of bytes sent else
 *                       negative error code (and errno will also
 *                       be set to a value from u_sock_errno.h).
 */
int32_t uSockWrite(uSockDescriptor_t descriptor,
                   const void *pData, size_t dataSizeBytes);

/** Receive data.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pData          a buffer in which to store the arriving
 *                       data.
 * @param dataSizeBytes  the number of bytes of storage available
 *                       at pData.
 * @return               on success the number of bytes received
 *                       else negative error code (and errno will
 *                       also be set to a value from u_sock_errno.h).
 */
int32_t uSockRead(uSockDescriptor_t descriptor,
                  void *pData, size_t dataSizeBytes);

/** Prepare a TCP socket for being closed.
 * This is provided for BSD socket compatibility however
 * it may not be used under the hood other than to prevent
 * further reads/writes to the socket.
 *
 * @param descriptor the descriptor of the socket to be prepared.
 * @param how        what type of shutdown to perform.
 * @return           zero on success else negative error code (and
 *                   errno will also be set to a value from
 *                   u_sock_errno.h).
 */
int32_t uSockShutdown(uSockDescriptor_t descriptor,
                      uSockShutdown_t how);

/* ----------------------------------------------------------------
 * FUNCTIONS: ASYNC
 * -------------------------------------------------------------- */

/** Register a callback which will be called when incoming
 * data has arrived on a socket.  The stack size of the task
 * within which the callback is run is
 * #U_EDM_STREAM_TASK_STACK_SIZE_BYTES for Wi-Fi and
 * #U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES for cellular.
 *
 * IMPORTANT: don't spend long in your callback, i.e. don't call
 * directly back into this API (only do that via another task
 * or you risk mutex deadlocks). don't call things that will cause
 * any sort of processing load or might get stuck.
 *
 * @param descriptor         the descriptor of the socket.
 * @param pCallback          the function to call when data arrives,
 *                           use NULL to cancel a previously
 *                           registered callback.
 * @param pCallbackParameter parameter to be passed to the
 *                           pCallback function when it is called;
 *                           may be NULL.
 */
void uSockRegisterCallbackData(uSockDescriptor_t descriptor,
                               void (*pCallback) (void *),
                               void *pCallbackParameter);

/** Register a callback which will be called when a socket is
 * closed, either locally or by the remote host.  The stack size
 * and of the task within which the callback is run
 * is #U_EDM_STREAM_TASK_STACK_SIZE_BYTES for Wi-Fi and
 * #U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES for cellular.
 *
 * IMPORTANT: don't spend long in your callback, i.e. don't
 * call directly back into this API, don't call things that will
 * cause any sort of processing load or might get stuck.
 *
 * @param descriptor         the descriptor of the socket.
 * @param pCallback          the function to call, use NULL
 *                           to cancel a previously registered
 *                           callback.
 * @param pCallbackParameter parameter to be passed to the
 *                           pCallback function when it is
 *                           called; may be NULL.
 */
void uSockRegisterCallbackClosed(uSockDescriptor_t descriptor,
                                 void (*pCallback) (void *),
                                 void *pCallbackParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS: TCP INCOMING (TCP SERVER) ONLY
 * -------------------------------------------------------------- */

/** Prepare a socket for receiving incoming TCP connections by
 * binding it to an address.
 *
 * @param descriptor     the descriptor of the socket to be prepared.
 * @param pLocalAddress  the local IP address to bind to.
 * @return               zero on success else negative error code.
 */
int32_t uSockBind(uSockDescriptor_t descriptor,
                  const uSockAddress_t *pLocalAddress);

/** Set the given socket into listening mode for an incoming TCP
 * connection. The socket must have been bound to an address first.
 *
 * @param descriptor the descriptor of the socket to listen on.
 * @param backlog    the number of pending connections that can
 *                   be queued.
 * @return           zero on success else negative error code.
 */
int32_t uSockListen(uSockDescriptor_t descriptor, size_t backlog);

/** Accept an incoming TCP connection on the given socket.
 *
 * @param descriptor      the descriptor of the socket with the queued
 *                        incoming connection.
 * @param pRemoteAddress  a pointer to a place to put the address
 *                        of the thing from which the connection has
 *                        been accepted.
 * @return                the descriptor of the new socket connection
 *                        that must be used for TCP communication
 *                        with the thing from now on else negative
 *                        error code.
 */
int32_t uSockAccept(uSockDescriptor_t descriptor,
                    uSockAddress_t *pRemoteAddress);

/** Select: wait for one of a set of sockets to become unblocked.
 *
 * @param maxDescriptor         the highest numbered descriptor in the
 *                              sets that follow to select on + 1.
 * @param pReadDescriptorSet    the set of descriptors to check for
 *                              unblocking for a read operation. May
 *                              be NULL.
 * @param pWriteDescriptorSet   the set of descriptors to check for
 *                              unblocking for a write operation. May
 *                              be NULL.
 * @param pExceptDescriptorSet  the set of descriptors to check for
 *                              exceptional conditions. May be NULL.
 * @param timeMs                the timeout for the select operation
 *                              in milliseconds.
 * @return                      a positive value if an unblock
 *                              occurred, zero on timeout, negative
 *                              on any other error.  Use
 *                              #U_SOCK_FD_ISSET() to determine
 *                              which descriptor(s) were unblocked.
 */
int32_t uSockSelect(int32_t maxDescriptor,
                    uSockDescriptorSet_t *pReadDescriptorSet,
                    uSockDescriptorSet_t *pWriteDescriptorSet,
                    uSockDescriptorSet_t *pExceptDescriptorSet,
                    int32_t timeMs);


/** Get the number of bytes sent by the socket
 * @param descriptor    the descriptor of the socket to get the sent bytes
 *
 * @return              number of sent bytes or negative error code
 */

int32_t uSockGetTotalBytesSent(uSockDescriptor_t descriptor);

/* ----------------------------------------------------------------
 * FUNCTIONS: FINDING ADDRESSES
 * -------------------------------------------------------------- */

/** Get the address of the remote host connected to a given socket.
 *
 * @param descriptor     the descriptor of the socket.
 * @param pRemoteAddress a pointer to a place to put the address
 *                       of the remote end of the socket.
 * @return               zero on success else negative error code.
 */
int32_t uSockGetRemoteAddress(uSockDescriptor_t descriptor,
                              uSockAddress_t *pRemoteAddress);

/** Get the local address of the given socket.
 * IMPORTANT: the port number will be zero unless this is a TCP
 * server, i.e. it will be zero for any client connection.
 *
 * @param descriptor    the descriptor of the socket.
 * @param pLocalAddress a pointer to a place to put the address
 *                      of the local end of the socket.
 * @return              zero on success else negative error code.
 */
int32_t uSockGetLocalAddress(uSockDescriptor_t descriptor,
                             uSockAddress_t *pLocalAddress);

/** Get the IP address of the given host name.  If the host name
 * is already an IP address then the IP address is returned
 * straight away without any external action, hence this also
 * implements "get host by address".
 *
 * @param devHandle      the handle of the underlying network to
 *                       use for host name look-up.
 * @param pHostName      a string representing the host to search
 *                       for, for example "google.com" or "192.168.1.0".
 * @param pHostIpAddress a pointer to a place to put the IP address
 *                       of the host.  Set this to NULL to determine
 *                       if a host is there without bothering to
 *                       return the address.
 * @return               zero on success else negative error code
 *                       (and errno will also be set to a value from
 *                       u_sock_errno.h).
 */
int32_t uSockGetHostByName(uDeviceHandle_t devHandle, const char *pHostName,
                           uSockIpAddress_t *pHostIpAddress);


/* ----------------------------------------------------------------
 * FUNCTIONS: ADDRESS CONVERSION
 * -------------------------------------------------------------- */

/** Convert an IP address string into a struct.
 *
 * @param pAddressString the string to convert.  Both IPV4 and
 *                       IPV6 addresses are supported and a port
 *                       number may be included.  Note that the
 *                       IPV6 optimisation of removing a single
 *                       consecutive set of zero hextets in the
 *                       string by using "::" is NOT supported.
 *                       TODO: support it!
 *                       The string does not have to be null-
 *                       terminated, it may contain random crap
 *                       after the address; provided the first
 *                       N bytes form a valid address then the
 *                       conversion will succeed.
 * @param pAddress       a pointer to a place to put the address.
 * @return               zero on success else negative error code.
 */
int32_t uSockStringToAddress(const char *pAddressString,
                             uSockAddress_t *pAddress);

/** Convert an IP address struct (without a port number)
 * into a string.
 *
 * @param pIpAddress a pointer to the IP address to convert.
 * @param pBuffer    a buffer in which to place the string.
 *                   Allow #U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES
 *                   for a full IPV6 address and terminator.
 * @param sizeBytes  the amount of memory pointed to by
 *                   pBuffer.
 * @return           on success the length of the string, not
 *                   including the terminator (what strlen()
 *                   would return) else negative error code.
 */
int32_t uSockIpAddressToString(const uSockIpAddress_t *pIpAddress,
                               char *pBuffer, size_t sizeBytes);

/** Convert an address struct (with a port number) into a
 * string.
 *
 * @param pAddress   a pointer to the address to convert.
 * @param pBuffer    a buffer in which to place the string.
 *                   Allow #U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES
 *                   for a full IPV6 address with port number
 *                   and terminator.
 * @param sizeBytes  the amount of memory pointed to by pBuffer.
 * @return           on success the length of the string, not
 *                   including the terminator (what strlen()
 *                   would return) else negative error code.
 */
int32_t uSockAddressToString(const uSockAddress_t *pAddress,
                             char *pBuffer, size_t sizeBytes);

/** Get the port number from a domain name.
 *
 * @param pDomainString the null-terminated domain name.
 * @return              the port number or -1 if there
 *                      is no port number.
 */
int32_t uSockDomainGetPort(char *pDomainString);

/** Turn a domain name string into just the name part,
 * by removing the port off the end if it is present.
 * This is done by modifying pDomainString in place.
 * IMPORTANT: if the string that is passed in is an
 * IPV6 address with a port number then it will be of
 * the form "[0:1:2:3:4:a:b:c]:x".  In order to return
 * a valid IPV6 address, not only will the new
 * terminator be placed where the ']' is but the
 * pointer that is returned by this function will
 * be a pointer to the '0' instead of pDomainString.
 *
 * @param pDomainString the NULL terminated domain name.
 * @return              a pointer to the start of the
 *                      modified domain name.
 */
char *pUSockDomainRemovePort(char *pDomainString);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SOCK_H_

// End of file
