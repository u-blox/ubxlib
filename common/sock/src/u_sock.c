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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the common, network-independent portion
 * of the sockets API.  This includes re-entrancy, error checking,
 * checking of socket state, handling of blocking and socket select
 * for TCP server operation.
 *
 * This implementation expects to call on underlying cell/wifi
 * APIs for the functions listed below, where "Xxx" could be Cell
 * or Wifi (and in future BLE).  The format of the calls to these
 * functions are deliberately left loose to accommodate variations
 * in implementation but the forms below are the simplest ones to
 * integrate with.
 *
 * In all cases the value of devHandle will be taken from the
 * appropriate range in u_network_handle.h and will have been brought
 * up before any socket operation is conducted.
 *
 * All of the required types are defined in u_sock.h and, if security
 * is required, u_security_tls.h.
 *
 * In all cases an error from cell/wifi must be indicated by returning
 * a negative error value taken from the errno's listed in
 * u_sock_errno.h: for instance -U_SOCK_EPERM or -U_SOCK_EAGAIN -11.
 * This implementation then negates these values and writes them
 * to errno as is usual for a BSD sockets API before returning
 * a value of -1 to the user to indicate that an error has
 * occurred.  Zero, in this case U_SOCK_ENONE, remains the
 * indication of success (and nothing is written to errno in this
 * case).
 *
 * Initialise sockets layer (mandatory):
 *
 * int32_t uXxxSockInit();
 *
 * Will be called before any other socket function is called,
 * can be used to setup up global resources etc. for the
 * entire socket layer.  If the socket layer is already
 * initialised when this function is called the function
 * must return U_SOCK_ENONE without making any changes.
 *
 * Initialise sockets instance (mandatory):
 *
 * int32_t uXxxSockInitInstance(uDeviceHandle_t devHandle);
 *
 * Will be called before any other socket function is
 * called on the given network instance, can be used to
 * setup up resources specific to that instance.  If the
 * instance is already initialised when this function
 * is called the function must return U_SOCK_ENONE without
 * making any changes.
 *
 * Create socket (mandatory):
 *
 * int32_t uXxxSockCreate(uDeviceHandle_t devHandle,
 *                        uSockType_t type,
 *                        uSockProtocol_t protocol);
 *
 * Returns a sockHandle that identifies the socket that has
 * been created for all future calls (or negative error code).
 * The type and protocol parameters are checked before this
 * function is called.
 *
 * Set blocking (mandatory):
 *
 * void uXxxSockBlockingSet(uDeviceHandle_t devHandle,
 *                          int32_t sockHandle,
 *                          bool isBlocking);
 *
 * Since blocking is handled at this level, the underlying
 * socket must always be non-blocking and this function
 * will be called to effect that.
 *
 * Close (mandatory):
 *
 * int32_t uXxxSockClose(uDeviceHandle_t devHandle,
 *                       int32_t sockHandle,
 *                       void (pCallback) (int32_t,
 *                                         int32_t));
 *
 * This call should block until the socket is closed.
 * If socket closure will take many seconds, e.g. due
 * to strict adherence to TCP socket closing rules,
 * the pCallback parameter should be taken and be
 * called, with the first parameter being devHandle
 * and the second parameter sockHandle, when the socket
 * is finally closed.  The callback should only be
 * called once.
 *
 * DNS look-up (recommended):
 *
 * int32_t uXxxSockGetHostByName(uDeviceHandle_t devHandle,
 *                               const char *pHostName,
 *                               uSockIpAddress_t *pHostIpAddress);
 *
 * Get local address of socket (recommended):
 *
 * int32_t uXxxSockGetLocalAddress(uDeviceHandle_t devHandle,
 *                                 int32_t sockHandle,
 *                                 uSockAddress_t *pLocalAddress);
 *
 * Note that there is NO requirement for a
 * uXxxSockGetRemoteAddress() function since this layer
 * remembers the remote address and always provides that
 * remote address with the UDP send-to function.  Hence there
 * is also no requirement on the underlying socket layer
 * to remember the remote address.
 *
 * Connect to a server (required if TCP is supported):
 *
 * int32_t uXxxSockConnect(uDeviceHandle_t devHandle,
 *                         int32_t sockHandle,
 *                         const uSockAddress_t *pRemoteAddress);
 *
 * This function will only be called on a socket in state
 * U_SOCK_STATE_CREATED.
 *
 * Deinitialise (optional):
 *
 * void uXxxSockDeinit();
 *
 * Will be called after the socket layer has shut down,
 * can be used to release global resources etc.  Does NOT
 * need to close sockets or the like, this layer will
 * ensure that those calls have already been made.
 *
 * Cleanup (optional):
 *
 * void uXxxSockCleanup(uDeviceHandle_t devHandle);
 *
 * Where present this may be called after a socket or sockets
 * have been closed.  Can be useful to allow freeing of
 * resources associated with the given network instance.
 *
 * Set option (optional):
 *
 * int32_t uXxxSockOptionSet(uDeviceHandle_t devHandle,
 *                           int32_t sockHandle,
 *                           int32_t level,
 *                           int32_t option,
 *                           const void *pOptionValue,
 *                           size_t optionValueLength);
 *
 * The value of level shall be taken from U_SOCK_OPT_LEVEL_xxx
 * and the values if option from U_SOCK_OPT_xxx. pOptionValue
 * points to the value to set and optionValueLength is its
 * length in bytes but usually the options are either a single
 * int32_t (for an integer value), so length 4, or two int32_t
 * values (i.e. uSockLinger_t), so length 8, or a timeval
 * struct also consisting of two int32_t's.  No checking
 * is performed on the level, option, pOptionValue or
 * optionValueLength parameters, it is entirely up to the
 * implementation to do this and return sensible error values
 * (e.g. -U_SOCK_EINVAL).  All options are passed transparently
 * through except for U_SOCK_OPT_RCVTIMEO which is handled
 * here in the u_sock layer (since blocking is handled here).
 *
 * Get option (optional):
 *
 * int32_t uXxxSockOptionGet(uDeviceHandle_t devHandle,
 *                           int32_t sockHandle,
 *                           int32_t level,
 *                           int32_t option,
 *                           void *pOptionValue,
 *                           size_t *pOptionValueLength);
 *
 * As uXxxSockOptionSet() but with pOptionValue being
 * populated with the result.  However, pOptionValue may be NULL,
 * in which case the length of the option that _would_ have
 * been got should be returned in pOptionValueLength.  No
 * checking is performed on the level, option, pOptionValue
 * or optionValueLength parameters, it is entirely up to the
 * implementation to do this and return sensible error values
 * (e.g. -U_SOCK_EINVAL).  All options are passed transparently
 * through except for U_SOCK_OPT_RCVTIMEO which is handled
 * here in the u_sock layer (since blocking is handled here).
 *
 * Send-to, i.e. datagram, AKA UDP, data transmission
 * (optional):
 *
 * int32_t uXxxSockSendTo(uDeviceHandle_t devHandle,
 *                        int32_t sockHandle,
 *                        const uSockAddress_t *pRemoteAddress,
 *                        const void *pData, size_t dataSizeBytes);
 *
 * Returns the number of bytes sent or negative errno in the
 * usual way.  pRemoteAddress will always be provided.  If
 * dataSizeBytes cannot fit into the maximum permitted
 * size of a datagram for the given network no data shall
 * be sent and an error (e.g. -U_SOCK_EMSGSIZE) shall be
 * returned.  It is valid to use this call on a TCP socket.
 *
 * Receive-from, i.e. datagram, AKA UDP, data reception
 * (optional):
 *
 * int32_t uXxxSockReceiveFrom(uDeviceHandle_t devHandle,
 *                             int32_t sockHandle,
 *                             const uSockAddress_t *pRemoteAddress,
 *                             void *pData, size_t dataSizeBytes);
 *
 * Returns the number of bytes received or negative errno in the
 * usual way. pRemoteAddress may be NULL.  If dataSizeBytes is
 * insufficient to read in a whole received datagram any remainder
 * shall be discarded.  If no data is available, U_SOCK_EWOULDBLOCK
 * should be returned; this is different to the case where a zero
 * length datagram has been received (where zero should be returned).
 * It is valid to use this call on a TCP socket.
 *
 * Write, i.e. byte-oriented or streamed, AKA TCP, data
 * transmission over a connected socket (optional):
 *
 * int32_t uXxxSockWrite(uDeviceHandle_t devHandle,
 *                       int32_t sockHandle,
 *                       const void *pData, size_t dataSizeBytes);
 *
 * Will only be called on a TCP socket that is connected.
 * dataSizeBytes is not limited (except to INT_MAX): the
 * function should loop until no more bytes can be sent or
 * an error occurs.  Returns the number of bytes sent or
 * negative errno in the usual way.
 *
 * Read, i.e. byte-oriented or streamed, AKA TCP, data
 * reception over a connected socket (optional):
 *
 * int32_t uXxxSockRead(uDeviceHandle_t devHandle,
 *                      int32_t sockHandle,
 *                      void *pData, size_t dataSizeBytes);
 *
 * Will only be called on a TCP socket that is connected.
 * The function should loop until dataSizeBytes have been
 * filled, no more data is available or an error has occureed.
 * Returns the number of bytes received or negative errno in the
 * usual way.  If no data at all is available,
 * U_SOCK_EWOULDBLOCK should be returned.
 *
 * Register a callback on data being received (optional):
 *
 * void uXxxSockRegisterCallbackData(uDeviceHandle_t devHandle,
 *                                   int32_t sockHandle,
 *                                   void (pCallback) (int32_t,
 *                                                     int32_t));
 *
 * When new data is received pCallback should be called
 * with the first parameter being devHandle and the
 * second parameter sockHandle.  pCallback will be
 * set to NULL to remove an existing callback.
 *
 * Register a callback on a socket being closed, either
 * locally or by the remote host (optional):
 *
 * void uXxxSockRegisterCallbackClosed(uDeviceHandle_t devHandle,
 *                                     int32_t sockHandle,
 *                                     void (pCallback) (int32_t,
 *                                                       int32_t));
 *
 * When the socket is closed pCallback should be called
 * with the first parameter being devHandle and the
 * second parameter sockHandle.  pCallback will be set
 * to NULL to remove an existing callback.  The callback
 * should only be called once.
 *
 * Bind a socket to a local IP address for receiving
 * incoming TCP connections (required for TCP server only):
 *
 * int32_t uXxxSockBind(uDeviceHandle_t devHandle,
 *                      int32_t sockHandle,
 *                      const uSockAddress_t *pLocalAddress);
 *
 * Set listening mode (required for TCP server only):
 *
 * int32_t uXxxSockListen(uDeviceHandle_t devHandle,
 *                        int32_t sockHandle,
 *                        size_t backlog);
 *
 * Accept an incoming TCP connection (required for TCP
 * server only):
 *
 * int32_t uXxxSockAccept(uDeviceHandle_t devHandle,
 *                        int32_t sockHandle,
 *                        uSockAddress_t *pRemoteAddress);
 *
 * The return value is the sockHandle to be used with
 * the new connection from now on.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"        // For UCHAR_MAX, USHRT_MAX, INT_MAX
#include "errno.h"
#include "stdlib.h"        // strtol()
#include "stddef.h"        // NULL, size_t etc.
#include "stdint.h"        // int32_t etc.
#include "stdbool.h"
#include "string.h"        // strlen(), strchr(), strtol()
#include "stdio.h"         // snprintf()
#include "sys/time.h"      // mktime() and struct timeval in most cases

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_port_clib_platform_specific.h" /* struct timeval in some cases and
                                              integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_sock.h"
#include "u_sock_security.h"
#include "u_sock_errno.h"

#include "u_cell_sec_tls.h"
#include "u_cell_sock.h"
#include "u_wifi_sock.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SOCK_NUM_STATIC_SOCKETS
/** The number of statically allocated sockets.  When
 * more than this number of sockets are required to
 * be open simultaneously they will be allocated and
 * it is up to the user to call uSockCleanUp()
 * to release the memory occupied by closed allocated
 * sockets when done.
 */
# define U_SOCK_NUM_STATIC_SOCKETS     7
#endif

/** Increment a socket descriptor.
 */
#define U_SOCK_INC_DESCRIPTOR(d)  (d)++;         \
                                  if ((d) < 0) { \
                                      d = 0;     \
                                  }

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Socket state.
 */
typedef enum {
    U_SOCK_STATE_CREATED,   /**< Freshly created, unsullied. */
    U_SOCK_STATE_CONNECTED, /**< TCP connected or UDP has an address. */
    U_SOCK_STATE_SHUTDOWN_FOR_READ,  /**< Block all reads. */
    U_SOCK_STATE_SHUTDOWN_FOR_WRITE, /**< Block all writes. */
    U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE, /**< Block all reads and
                                               writes. */
    U_SOCK_STATE_CLOSING, /**< Block all reads and writes, waiting
                               for far end to complete closure, can be
                               tidied up. */
    U_SOCK_STATE_CLOSED   /**< Actually closed, cannot be found,
                               container may be re-used. */
} uSockState_t;

/** A socket.
 */
typedef struct {
    uSockType_t type;
    uSockProtocol_t protocol;
    uDeviceHandle_t devHandle;
    int32_t sockHandle; /**< This is the socket handle
                             that is returned by the
                             underlying socket layer and
                             is NOTHING TO DO with the socket
                             descriptor. */
    uSockState_t state;
    uSockAddress_t remoteAddress;
    int64_t receiveTimeoutMs;
    int32_t bytesSent;
    uSecurityTlsContext_t *pSecurityContext;
    void (*pDataCallback) (void *);
    void *pDataCallbackParameter;
    void (*pClosedCallback) (void *);
    void *pClosedCallbackParameter;
    bool blocking; // At end to optimise structure packing
} uSockSocket_t;

/** A socket container.
 */
typedef struct uSockContainer_t {
    struct uSockContainer_t *pPrevious;
    uSockDescriptor_t descriptor;
    uSockSocket_t socket;
    struct uSockContainer_t *pNext;
    bool isStatic; // At end to optimise structure packing
} uSockContainer_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Keep track of whether we're initialised or not.
 */
static bool gInitialised = false;

/** Mutex to protect the container list.
 */
static uPortMutexHandle_t gMutexContainer = NULL;

/** Mutex to protect just the callbacks in the container list.
 */
static uPortMutexHandle_t gMutexCallbacks = NULL;

/** Root of the socket container list.
 */
static uSockContainer_t *gpContainerListHead = NULL;

/** The next descriptor to use.
 */
static uSockDescriptor_t gNextDescriptor = 0;

/** Containers for statically allocated sockets.
 */
static uSockContainer_t gStaticContainers[U_SOCK_NUM_STATIC_SOCKETS];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Initialise.
static int32_t init()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal = U_SOCK_ENOMEM;
    uSockContainer_t **ppContainer = &gpContainerListHead;
    uSockContainer_t *pTmp = NULL;
    uSockContainer_t **ppPreviousNext = NULL;

    // The mutexes are set up once only
    if (gMutexContainer == NULL) {
        errorCode = uPortMutexCreate(&gMutexContainer);
    }
    if ((errorCode == 0) && (gMutexCallbacks == NULL)) {
        errorCode = uPortMutexCreate(&gMutexCallbacks);
    }

    if (errorCode == 0) {
        errnoLocal = U_SOCK_ENONE;
        if (!gInitialised) {
            // uXxxSockInit returns a negated value of errno
            // from the U_SOCK_Exxx list
            errnoLocal = uCellSockInit();
            if (errnoLocal == U_SOCK_ENONE) {
                errnoLocal = uWifiSockInit();
            }

            if (errnoLocal == U_SOCK_ENONE) {
                //  Link the static containers into the start of the container list
                for (size_t x = 0; x < sizeof(gStaticContainers) /
                     sizeof(gStaticContainers[0]); x++) {
                    *ppContainer = &gStaticContainers[x];
                    (*ppContainer)->isStatic = true;
                    (*ppContainer)->socket.state = U_SOCK_STATE_CLOSED;
                    (*ppContainer)->pNext = NULL;
                    if (ppPreviousNext != NULL) {
                        *ppPreviousNext = *ppContainer;
                    }
                    ppPreviousNext = &((*ppContainer)->pNext);
                    (*ppContainer)->pPrevious = pTmp;
                    pTmp = *ppContainer;
                    ppContainer = &((*ppContainer)->pNext);
                }

                gInitialised = true;
            }
        }
    }

    return errnoLocal;
}

// Deinitialise.
static void deinitButNotMutex()
{
    if (gInitialised) {
        // IMPORTANT: can't delete the mutexes here as we can't
        // know if anyone has hold of them.  They just have
        // to remain.

        uCellSockDeinit();
        uWifiSockDeinit();

        gInitialised = false;
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CONTAINER STUFF
 * -------------------------------------------------------------- */

// Find the socket container for the given descriptor.
// Will not find sockets in state CLOSED.
// This does NOT lock the mutex, you need to do that.
static uSockContainer_t *pContainerFindByDescriptor(uSockDescriptor_t descriptor)
{
    uSockContainer_t *pContainer = NULL;
    uSockContainer_t *pContainerThis = gpContainerListHead;

    while ((pContainerThis != NULL) &&
           (pContainer == NULL)) {
        if ((pContainerThis->descriptor == descriptor) &&
            (pContainerThis->socket.state != U_SOCK_STATE_CLOSED)) {
            pContainer = pContainerThis;
        }
        pContainerThis = pContainerThis->pNext;
    }

    return pContainer;
}

// Find the socket container for the given network handle
// and socket handle.  If sockHandle is less than zero,
// returns the first entry for the given devHandle.
// Will not find sockets in state CLOSED.
// This does NOT lock the mutex, you need to do that.
static uSockContainer_t *pContainerFindByDeviceHandle(uDeviceHandle_t devHandle,
                                                      int32_t sockHandle)
{
    uSockContainer_t *pContainer = NULL;
    uSockContainer_t *pContainerThis = gpContainerListHead;

    while ((pContainerThis != NULL) &&
           (pContainer == NULL)) {
        if ((pContainerThis->socket.devHandle == devHandle) &&
            ((pContainerThis->socket.sockHandle == sockHandle) ||
             (pContainerThis->socket.sockHandle < 0)) &&
            (pContainerThis->socket.state != U_SOCK_STATE_CLOSED)) {
            pContainer = pContainerThis;
        }
        pContainerThis = pContainerThis->pNext;
    }

    return pContainer;
}

// Determine the number of non-closed sockets.
// This does NOT lock the mutex, you need to do that.
static size_t numContainersInUse()
{
    uSockContainer_t *pContainer = gpContainerListHead;
    size_t numInUse = 0;

    while (pContainer != NULL) {
        if (pContainer->socket.state != U_SOCK_STATE_CLOSED) {
            numInUse++;
        }
        pContainer = pContainer->pNext;
    }

    return numInUse;
}

// Create a socket in a container with the given descriptor.
// This does NOT lock the mutex, you need to do that.
static uSockContainer_t *pSockContainerCreate(uSockDescriptor_t descriptor,
                                              uSockType_t type,
                                              uSockProtocol_t protocol)
{
    uSockContainer_t *pContainer = NULL;
    uSockContainer_t *pContainerPrevious = NULL;
    uSockContainer_t **ppContainerThis = &gpContainerListHead;

    // Traverse the list, stopping if there is a container
    // that holds a closed socket, which we could re-use
    while ((*ppContainerThis != NULL) && (pContainer == NULL)) {
        if ((*ppContainerThis)->socket.state == U_SOCK_STATE_CLOSED) {
            pContainer = *ppContainerThis;
        }
        pContainerPrevious = *ppContainerThis;
        ppContainerThis = &((*ppContainerThis)->pNext);
    }

    if (pContainer == NULL) {
        // Reached the end of the list and found no re-usable
        // containers, so allocate memory for the new container
        // and add it to the list
        pContainer = (uSockContainer_t *) pUPortMalloc(sizeof (*pContainer));
        if (pContainer != NULL) {
            pContainer->isStatic = false;
            pContainer->pPrevious = pContainerPrevious;
            pContainer->pNext = NULL;
            *ppContainerThis = pContainer;
        }
    }

    // Set up the new container and socket
    if (pContainer != NULL) {
        pContainer->descriptor = descriptor;
        memset(&(pContainer->socket), 0, sizeof(pContainer->socket));
        pContainer->socket.type = type;
        pContainer->socket.protocol = protocol;
        pContainer->socket.devHandle = NULL;
        pContainer->socket.sockHandle = -1;
        pContainer->socket.state = U_SOCK_STATE_CREATED;
        pContainer->socket.blocking = true;
        pContainer->socket.receiveTimeoutMs = U_SOCK_DEFAULT_RECEIVE_TIMEOUT_MS;
        pContainer->socket.pSecurityContext = NULL;
        pContainer->socket.pDataCallback = NULL;
        pContainer->socket.pDataCallbackParameter = NULL;
        pContainer->socket.pClosedCallback = NULL;
        pContainer->socket.pClosedCallbackParameter = NULL;
    }

    return pContainer;
}

// Free the container corresponding to the descriptor.
// Has no effect on static containers.
// This does NOT lock the mutex, you need to do that.
static bool containerFree(uSockDescriptor_t descriptor)
{
    uSockContainer_t **ppContainer = NULL;
    uSockContainer_t **ppContainerThis = &gpContainerListHead;
    bool success = false;

    while ((*ppContainerThis != NULL) &&
           (ppContainer == NULL)) {
        if ((*ppContainerThis)->descriptor == descriptor) {
            ppContainer = ppContainerThis;
        } else {
            ppContainerThis = &((*ppContainerThis)->pNext);
        }
    }

    if ((ppContainer != NULL) && (*ppContainer != NULL)) {
        if (!(*ppContainer)->isStatic) {
            // If we found it, and it wasn't static, free it
            // If there is a previous container, move its pNext
            if ((*ppContainer)->pPrevious != NULL) {
                (*ppContainer)->pPrevious->pNext = (*ppContainer)->pNext;
            }
            // If there is a next container, move its pPrevious
            if ((*ppContainer)->pNext != NULL) {
                (*ppContainer)->pNext->pPrevious = (*ppContainer)->pPrevious;
            }

            // Free the memory and NULL the pointer
            uPortFree(*ppContainer);
            *ppContainer = NULL;
        } else {
            // Nothing to do for a static container,
        }

        success = true;
    }

    return success;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CALLBACKS
 * -------------------------------------------------------------- */

// Callback for when local socket closures at the underlying
// cell/wifi socket layer happen asynchronously, either
// due to local closure or by the remote host
static void closedCallback(uDeviceHandle_t devHandle,
                           int32_t sockHandle)
{
    uSockContainer_t *pContainer;

    // Don't lock the container mutex here as this
    // needs to be callable while a send or receive is
    // in progress and that already has the mutex
    pContainer = pContainerFindByDeviceHandle(devHandle,
                                              sockHandle);
    if (pContainer != NULL) {
        // Mark the container as closed
        pContainer->socket.state = U_SOCK_STATE_CLOSED;
        U_PORT_MUTEX_LOCK(gMutexCallbacks);
        if (pContainer->socket.pClosedCallback != NULL) {
            pContainer->socket.pClosedCallback(pContainer->socket.pClosedCallbackParameter);
            pContainer->socket.pClosedCallback = NULL;
        }
        // We can now finally release any security
        // context
        uSecurityTlsRemove(pContainer->socket.pSecurityContext);
        pContainer->socket.pSecurityContext = NULL;
        U_PORT_MUTEX_UNLOCK(gMutexCallbacks);
    }
}

// Callback for when data has been received at the
// underlying cell/wifi socket layer.
static void dataCallback(uDeviceHandle_t devHandle,
                         int32_t sockHandle)
{
    uSockContainer_t *pContainer;

    // Don't lock the container mutex here as this
    // needs to be callable while a send or receive is
    // in progress and that already has the mutex
    pContainer = pContainerFindByDeviceHandle(devHandle,
                                              sockHandle);
    if (pContainer != NULL) {
        U_PORT_MUTEX_LOCK(gMutexCallbacks);
        if (pContainer->socket.pDataCallback != NULL) {
            pContainer->socket.pDataCallback(pContainer->socket.pDataCallbackParameter);
        }
        U_PORT_MUTEX_UNLOCK(gMutexCallbacks);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: ADDRESS CONVERSION
 * -------------------------------------------------------------- */

// Given a string, which may be an IP address or a
// domain name, return a pointer to the separator
// character for the port number part of it,
// or NULL if there is no port number
//lint -e{818} Suppress could be declared as pointing
// to const 'cos when called from pUSockDomainRemovePort()
// it can't.
static char *pAddressPortSeparator(char *pAddress)
{
    char *pColon = NULL;

    if (pAddress != NULL) {
        // If there's a square bracket at the start of
        // the domain string then we've been given an
        // IPV6 address with port number so move
        // the pointer to the closing square bracket
        if (*pAddress == '[') {
            pAddress = strchr(pAddress, ']');
        }
        if (pAddress != NULL) {
            // Check for a port number on the end
            pColon = strchr(pAddress, ':');
            if (pColon != NULL) {
                // Check if there are more colons in the
                // string: if so this is an IPV6 address
                // without a port number on the end
                if (strchr(pColon + 1, ':') != NULL) {
                    pColon = NULL;
                }
            }
        }
    }

    return pColon;
}

// Determine whether the given IP address string is IPV4.
static bool addressStringIsIpv4(const char *pAddressString)
{
    // If it's got a dot in it, must be IPV4
    return (strchr(pAddressString, '.') != NULL);
}

// Convert an IPV4 address string "xxx.yyy.www.zzz:65535" into
// a struct.
static bool ipv4StringToAddress(const char *pAddressString,
                                uSockAddress_t *pAddress)
{
    bool success = true;
    uint8_t digits[4];
    int32_t y;
    size_t z = 0;
    char *pTmp;
    const char *pColon = NULL;

    pAddress->ipAddress.type = U_SOCK_ADDRESS_TYPE_V4;
    pAddress->ipAddress.address.ipv4 = 0;
    pAddress->port = 0;

    // Get the numbers from the IP address part,
    // moving pAddressString along as we go
    for (size_t x = 0; (x < sizeof(digits) /
                        sizeof(digits[0])) &&
         success; x++) {
        y = strtol(pAddressString, &pTmp, 10);
        digits[x] = (uint8_t) y;
        success = (pTmp > pAddressString) &&
                  (y >= 0) && (y <= UCHAR_MAX) &&
                  ((*pTmp == '.') || (*pTmp == 0) ||
                   (*pTmp == ':'));
        if (*pTmp == ':') {
            pColon = pTmp;
        }
        pAddressString = pTmp;
        pAddressString++;
        z++;
    }

    if (success && (z == sizeof(digits) /
                    sizeof(digits[0]))) {
        // Got enough digits, now calculate the
        // IP address part in network-byte order
        pAddress->ipAddress.address.ipv4 = (((uint32_t) digits[0]) << 24) |
                                           (((uint32_t) digits[1]) << 16) |
                                           (((uint32_t) digits[2]) << 8)  |
                                           (((uint32_t) digits[3]) << 0);
        // Check the port number on the end
        if (pColon != NULL) {
            success = false;
            // Fill in the port number
            y = strtol(pColon + 1, NULL, 10);
            if (y <= (int32_t) USHRT_MAX) {
                pAddress->port = (uint16_t) y;
                success = true;
            }
        }
    }

    return success;
}

// Convert an IPV6 address string "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
// or "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:65535" into a struct.
static bool ipv6StringToAddress(const char *pAddressString,
                                uSockAddress_t *pAddress)
{
    bool success = true;
    uint16_t digits[8];
    bool hasPort = false;
    const char *pPortColon = NULL;
    char *pTmp;
    int32_t y;
    size_t z = 0;

    pAddress->ipAddress.type = U_SOCK_ADDRESS_TYPE_V6;
    memset(pAddress->ipAddress.address.ipv6, 0,
           sizeof(pAddress->ipAddress.address.ipv6));
    pAddress->port = 0;

    // See if there's a '[' on the start
    if ((strlen(pAddressString) > 0) && (*pAddressString == '[')) {
        hasPort = true;
        pAddressString++;
    }

    // Get the hex numbers from the IP address part,
    // moving pAddressString along and checking
    // for the colon before the port number as we go
    for (size_t x = 0; (x < sizeof(digits) /
                        sizeof(digits[0])) &&
         success; x++) {
        y = strtol(pAddressString, &pTmp, 16);
        digits[x] = (uint16_t) y;
        success = (pTmp > pAddressString) &&
                  (y >= 0) && (y <= (int32_t) USHRT_MAX) &&
                  ((*pTmp == ':') || (*pTmp == 0) ||
                   ((*pTmp == ']') && hasPort));
        if ((*pTmp == ']') && hasPort &&
            (*(pTmp + 1) == ':')) {
            pPortColon = pTmp + 1;
        }
        pAddressString = pTmp;
        pAddressString++;
        z++;
    }

    if (success && (z == sizeof(digits) /
                    sizeof(digits[0]))) {
        // Got enough digits, now slot the uint16_t's
        // into the array in network-byte order
        pAddress->ipAddress.address.ipv6[3] = (((uint32_t) digits[0]) << 16) | (digits[1]);
        pAddress->ipAddress.address.ipv6[2] = (((uint32_t) digits[2]) << 16) | (digits[3]);
        pAddress->ipAddress.address.ipv6[1] = (((uint32_t) digits[4]) << 16) | (digits[5]);
        pAddress->ipAddress.address.ipv6[0] = (((uint32_t) digits[6]) << 16) | (digits[7]);

        // Get the port number if there was one
        if (hasPort) {
            success = false;
            if (pPortColon != NULL) {
                // Fill in the port number
                y = strtol(pPortColon + 1, NULL, 10);
                if (y <= (int32_t) USHRT_MAX) {
                    pAddress->port = (uint16_t) y;
                    success = true;
                }
            }
        }
    }

    return success;
}

// Convert an IP address struct (i.e. without a port number) into a
// string, returning the length of the string.
static int32_t ipAddressToString(const uSockIpAddress_t *pIpAddress,
                                 char *pBuffer,
                                 size_t sizeBytes)
{
    int32_t stringLengthOrError = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    size_t thisLength;

    // Convert the address in network byte order (MSB first);
    switch (pIpAddress->type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            stringLengthOrError = snprintf(pBuffer, sizeBytes,
                                           "%u.%u.%u.%u",
                                           (unsigned int) ((pIpAddress->address.ipv4 >> 24) & 0xFF),
                                           (unsigned int) ((pIpAddress->address.ipv4 >> 16) & 0xFF),
                                           (unsigned int) ((pIpAddress->address.ipv4 >> 8)  & 0xFF),
                                           (unsigned int) ((pIpAddress->address.ipv4 >> 0)  & 0xFF));
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            stringLengthOrError = 0;
            for (int32_t x = 3; (x >= 0) && (stringLengthOrError >= 0); x--) {
                thisLength = snprintf(pBuffer, sizeBytes,
                                      "%x:%x",
                                      (unsigned int) ((pIpAddress->address.ipv6[x] >> 16) & 0xFFFF),
                                      (unsigned int) ((pIpAddress->address.ipv6[x] >> 0)  & 0xFFFF));
                if (x > 0) {
                    if (thisLength < sizeBytes) {
                        *(pBuffer + thisLength) = ':';
                        thisLength++;
                    } else {
                        stringLengthOrError = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    }
                }
                if (thisLength < sizeBytes) {
                    sizeBytes -= thisLength;
                    pBuffer += thisLength;
                    stringLengthOrError += (int32_t) thisLength;
                } else {
                    stringLengthOrError = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                }
            }
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
        //fall-through
        default:
            break;
    }

    return stringLengthOrError;
}

// Convert an address struct, which includes a port number,
// into a string, returning the length of the string.
static int32_t addressToString(const uSockAddress_t *pAddress,
                               bool includePortNumber,
                               char *pBuffer,
                               size_t sizeBytes)
{
    int32_t stringLengthOrError = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t thisLength;

    if (includePortNumber) {
        // If this is an IPV6 address, then start with a square bracket
        // to delineate the IP address part
        if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
            if (sizeBytes > 1) {
                *pBuffer = '[';
                stringLengthOrError++;
                sizeBytes--;
                pBuffer++;
            } else {
                stringLengthOrError =  (int32_t) U_ERROR_COMMON_NO_MEMORY;
            }
        }
        // Do the IP address part
        if (stringLengthOrError >= 0) {
            thisLength = ipAddressToString(&(pAddress->ipAddress),
                                           pBuffer, sizeBytes);
            if (thisLength >= 0) {
                sizeBytes -= thisLength;
                pBuffer += thisLength;
                stringLengthOrError += thisLength;
                // If this is an IPV6 address then close the square brackets
                if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
                    if (sizeBytes > 1) {
                        *pBuffer = ']';
                        stringLengthOrError++;
                        sizeBytes--;
                        pBuffer++;
                    } else {
                        stringLengthOrError =  (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    }
                }
            } else {
                stringLengthOrError =  (int32_t) U_ERROR_COMMON_NO_MEMORY;
            }
        }
        // Add the port number
        if (stringLengthOrError >= 0) {
            thisLength = snprintf(pBuffer, sizeBytes, ":%u",
                                  pAddress->port);
            if (thisLength < (int32_t) sizeBytes) {
                stringLengthOrError += thisLength;
            } else {
                stringLengthOrError =  (int32_t) U_ERROR_COMMON_NO_MEMORY;
            }
        }
    } else {
        // No port number required, just do the ipAddress part
        stringLengthOrError = ipAddressToString(&(pAddress->ipAddress),
                                                pBuffer, sizeBytes);
    }

    return (int32_t) stringLengthOrError;
}

// Print out a socket option for debug purposes.
//lint -esym(522, printSocketOption) Suppress "lacks side effects"
// when compiled out
static void printSocketOption(const void *pOptionValue,
                              size_t optionValueLength)
{
#if U_CFG_ENABLE_LOGGING
    int32_t y;

    uPortLog("[%d int32s] ", optionValueLength / sizeof(int32_t));
    if ((pOptionValue != NULL) && (optionValueLength > 0)) {
        // Print a series of int32_t's
        //lint -e{826} Suppress suspicious pointer warning
        for (size_t x = 0; x < optionValueLength / sizeof(int32_t); x++) {
            y = *((const int32_t *) (((const char *) pOptionValue) + (x * 4)));
            uPortLog("%d (0x%08x) ", y, y);
        }
    }
#else
    (void) pOptionValue;
    (void) optionValueLength;
#endif
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: RECEIVING
 * -------------------------------------------------------------- */

// Receive data on a socket, either UDP or TCP.
static int32_t receive(const uSockContainer_t *pContainer,
                       uSockAddress_t *pRemoteAddress,
                       void *pData, size_t dataSizeBytes)
{
    uDeviceHandle_t devHandle = pContainer->socket.devHandle;
    int32_t sockHandle = pContainer->socket.sockHandle;
    int32_t negErrnoOrSize = -U_SOCK_ENOSYS;
    int32_t startTimeMs = uPortGetTickTimeMs();
    int32_t devType = uDeviceGetDeviceType(devHandle);

    // Run around the loop until a packet of data turns up
    // or we time out or just once if we're non-blocking.
    do {
        if (pContainer->socket.protocol == U_SOCK_PROTOCOL_UDP) {
            // UDP style
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                negErrnoOrSize = uCellSockReceiveFrom(devHandle,
                                                      sockHandle,
                                                      pRemoteAddress,
                                                      pData,
                                                      dataSizeBytes);
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                negErrnoOrSize = uWifiSockReceiveFrom(devHandle,
                                                      sockHandle,
                                                      pRemoteAddress,
                                                      pData,
                                                      dataSizeBytes);
            }
        } else {
            // TCP style
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                negErrnoOrSize = uCellSockRead(devHandle,
                                               sockHandle,
                                               pData,
                                               dataSizeBytes);
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                negErrnoOrSize = uWifiSockRead(devHandle,
                                               sockHandle,
                                               pData,
                                               dataSizeBytes);
            }
        }
        if (negErrnoOrSize < 0) {
            // Yield for the poll interval
            uPortTaskBlock(U_SOCK_RECEIVE_POLL_INTERVAL_MS);
        }
    } while ((negErrnoOrSize < 0) &&
             (pContainer->socket.blocking) &&
             (uPortGetTickTimeMs() - startTimeMs <
              pContainer->socket.receiveTimeoutMs));

    return negErrnoOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CREATE/OPEN/CLOSE/CLEAN-UP
 * -------------------------------------------------------------- */

// Create a socket.
int32_t uSockCreate(uDeviceHandle_t devHandle, uSockType_t type,
                    uSockProtocol_t protocol)
{
    int32_t descriptorOrError = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uSockDescriptor_t descriptor = gNextDescriptor;
    int32_t sockHandle = -U_SOCK_ENOSYS;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {
        // Check parameters
        errnoLocal = U_SOCK_ENODEV;
        if (uDeviceIsValidInstance(U_DEVICE_INSTANCE(devHandle))) {
            errnoLocal = U_SOCK_EPROTONOSUPPORT;
            if (((type == U_SOCK_TYPE_STREAM) && (protocol == U_SOCK_PROTOCOL_TCP)) ||
                ((type == U_SOCK_TYPE_DGRAM) && (protocol == U_SOCK_PROTOCOL_UDP))) {
                errnoLocal = U_SOCK_ENONE;
            }
        }
    }

    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        errnoLocal = U_SOCK_ENOBUFS;
        if (numContainersInUse() < U_SOCK_MAX_NUM_SOCKETS) {
            // Find the next free descriptor
            descriptorOrError = (int32_t) U_ERROR_COMMON_BSD_ERROR;
            while (descriptorOrError < 0) {
                // Try the descriptor value, making sure
                // each time that it can't be found.
                if (pContainerFindByDescriptor(descriptor) == NULL) {
                    gNextDescriptor = descriptor;
                    U_SOCK_INC_DESCRIPTOR(gNextDescriptor);
                    // Found a free descriptor, now try to
                    // create the socket in a container
                    pContainer = pSockContainerCreate(descriptor,
                                                      type, protocol);
                    if (pContainer != NULL) {
                        descriptorOrError = (int32_t) descriptor;
                    } else {
                        errnoLocal = U_SOCK_ENOMEM;
                        uPortLog("U_SOCK: unable to allocate memory"
                                 " for socket.\n");
                        // Exit stage left
                        break;
                    }
                }
                U_SOCK_INC_DESCRIPTOR(descriptor);
            }

            if ((descriptorOrError >= 0) && (pContainer != NULL)) {
                int32_t devType = uDeviceGetDeviceType(devHandle);
                errnoLocal = U_SOCK_ENOSYS;
                if (pContainerFindByDeviceHandle(devHandle, -1) == NULL) {
                    // If this is the first time we have
                    // encountered this network layer,
                    // ask the underlying cell/wifi sockets
                    // layer to initialise it
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        errnoLocal = -uCellSockInitInstance(devHandle);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        errnoLocal = -uWifiSockInitInstance(devHandle);
                    }
                }
                // Get the underlying cell/wifi socket layer to
                // create the socket there. uXxxSockCreate() returns
                // a socket handle or a negated value of errno from
                // the U_SOCK_Exxx list
                if (errnoLocal == 0) {
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        sockHandle = uCellSockCreate(devHandle,
                                                     type, protocol);
                        // Setting non-blocking so that
                        // we do the blocking here instead.
                        // Since this has no return value
                        // we can do it at the same time
                        uCellSockBlockingSet(devHandle,
                                             sockHandle, false);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        sockHandle = uWifiSockCreate(devHandle,
                                                     type, protocol);
                        // TODO: Set blocking stuff
                    }

                    if (sockHandle >= 0) {
                        // All is good, no need to set descriptorOrError
                        // as it was already set above
                        pContainer->socket.sockHandle = sockHandle;
                        pContainer->socket.devHandle = devHandle;
                        pContainer->socket.bytesSent = 0;
                        uPortLog("U_SOCK: socket created, descriptor %d,"
                                 " network handle 0x%08x, socket handle %d.\n",
                                 descriptorOrError, devHandle, sockHandle);
                    } else {
                        // Set errno
                        errnoLocal = -sockHandle;
                        // Free the container once more
                        containerFree(descriptorOrError);
                        uPortLog("U_SOCK: underlying socket layer could not create"
                                 " socket (errno %d).\n", errnoLocal);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        descriptorOrError = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return descriptorOrError;
}

// Make an outgoing connection on the given socket.
int32_t uSockConnect(uSockDescriptor_t descriptor,
                     const uSockAddress_t *pRemoteAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;
#if U_CFG_ENABLE_LOGGING
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
#endif

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {
        errnoLocal = U_SOCK_EINVAL;
        // Check that the remote IP address is sensible
        if (pRemoteAddress != NULL) {

            U_PORT_MUTEX_LOCK(gMutexContainer);

            // Find the container
            pContainer = pContainerFindByDescriptor(descriptor);
            errnoLocal = U_SOCK_EBADF;
            if (pContainer != NULL) {
                errnoLocal = U_SOCK_EPERM;
                if (pContainer->socket.state == U_SOCK_STATE_CREATED) {
                    // We have found the container and it is
                    // in the right state, talk to the underlying
                    // cell/wifi socket layer to make the connection
                    // uXxxSockConnect() returns a negated value of errno
                    // from the U_SOCK_Exxx list
                    devHandle = pContainer->socket.devHandle;
                    sockHandle = pContainer->socket.sockHandle;
                    errnoLocal = U_SOCK_ENONE;
                    errorCode = -U_SOCK_ENOSYS;
                    uPortLog("U_SOCK: connecting socket to \"%.*s\"...\n",
                             addressToString(pRemoteAddress, true,
                                             buffer, sizeof(buffer)),
                             buffer);
                    int32_t devType = uDeviceGetDeviceType(devHandle);
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        errorCode = uCellSockConnect(devHandle,
                                                     sockHandle,
                                                     pRemoteAddress);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        errorCode = uWifiSockConnect(devHandle,
                                                     sockHandle,
                                                     pRemoteAddress);
                    }

                    if (errorCode == 0) {
                        // All is good
                        memcpy(&pContainer->socket.remoteAddress,
                               pRemoteAddress,
                               sizeof(pContainer->socket.remoteAddress));
                        pContainer->socket.state = U_SOCK_STATE_CONNECTED;
                        uPortLog("U_SOCK: socket with descriptor %d, network"
                                 " handle 0x%08x, socket handle %d, is "
                                 " connected to address \"%.*s\".\n",
                                 descriptor, devHandle, sockHandle,
                                 addressToString(&pContainer->socket.remoteAddress,
                                                 true, buffer,
                                                 sizeof(buffer)),
                                 buffer);
                    } else {
                        // Set errno
                        errnoLocal = -errorCode;
                        uPortLog("U_SOCK: underlying layer errno %d on"
                                 " address \"%.*s\", descriptor/"
                                 "network/socket %d/0x%08x/%d.\n", errnoLocal,
                                 addressToString(pRemoteAddress, true,
                                                 buffer, sizeof(buffer)),
                                 buffer, descriptor, devHandle,
                                 sockHandle);
                    }
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexContainer);
        }
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Close a socket.
int32_t uSockClose(uSockDescriptor_t descriptor)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;
    uSockState_t finalState = U_SOCK_STATE_CLOSED;
    void (*pAsyncClosedCallback) (uDeviceHandle_t, int32_t) = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            // We have found the container, talk to the underlying
            // cell/wifi socket layer to close the socket there.
            // If the underlying socket layer waits while it gets
            // the ack for the ack for the ack at TCP level then
            // give it a callback to call when it is done and
            // set finalState to U_SOCK_STATE_CLOSING to mark
            // the socket as closing, not closed.
            // uXxxSockClose() returns a negated value of errno
            // from the U_SOCK_Exxx list
            devHandle = pContainer->socket.devHandle;
            sockHandle = pContainer->socket.sockHandle;
            errnoLocal = U_SOCK_ENONE;
            errorCode = -U_SOCK_ENOSYS;
            int32_t devType = uDeviceGetDeviceType(devHandle);
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                // In the cellular case asynchronous TCP
                // socket closure is used in some cases.
                if (pContainer->socket.protocol == U_SOCK_PROTOCOL_TCP) {
                    finalState = U_SOCK_STATE_CLOSING;
                    pAsyncClosedCallback = closedCallback;
                }
                errorCode = uCellSockClose(devHandle,
                                           sockHandle,
                                           pAsyncClosedCallback);
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                errorCode = uWifiSockClose(devHandle,
                                           sockHandle,
                                           pAsyncClosedCallback);
            }
            if (errorCode == 0) {
                uPortLog("U_SOCK: socket with descriptor %d,"
                         " network handle 0x%08x, socket handle %d,"
                         " has been closed.\n",
                         descriptor, devHandle, sockHandle);
                if (pContainer->socket.state != U_SOCK_STATE_CLOSED) {
                    // Now mark the socket as closed (or closing).
                    // Socket is only freed by a call to
                    // uSockCleanUp() in order to ensure
                    // thread-safeness
                    // Do the check for closed state above first as it
                    // is possible for the uXxxSockClose() function
                    // to call the callback to close the socket
                    // immediately, before it returns.
                    if (finalState == U_SOCK_STATE_CLOSED) {
                        // There was no hanging around, call the
                        // callback directly
                        closedCallback(devHandle, sockHandle);
                    } else {
                        // Just set the state and the callback
                        // will sort actual closing out later
                        pContainer->socket.state = finalState;
                    }
                }
            } else {
                errnoLocal = -errorCode;
                uPortLog("U_SOCK: underlying socket layer returned"
                         " errno %d on closing descriptor %d,"
                         " network handle 0x%08x, socket handle %d.\n",
                         errnoLocal, descriptor, devHandle,
                         sockHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Free memory from any sockets that are no longer in use.
void uSockCleanUp()
{
    uSockContainer_t *pContainer = gpContainerListHead;
    uSockContainer_t *pTmp;
    size_t numNonClosedSockets = 0;
    uDeviceHandle_t devHandle;

    if (gInitialised) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Move through the list removing closed sockets
        while (pContainer != NULL) {
            if ((pContainer->socket.state == U_SOCK_STATE_CLOSED) ||
                (pContainer->socket.state == U_SOCK_STATE_CLOSING)) {
                if (!(pContainer->isStatic)) {
                    // If this socket is not static, uncouple it
                    // If there is a previous container, move its pNext
                    if (pContainer->pPrevious != NULL) {
                        pContainer->pPrevious->pNext = pContainer->pNext;
                    } else {
                        // If there is no previous container, must be
                        // at the start of the list so move the head
                        // pointer on instead
                        gpContainerListHead = pContainer->pNext;
                    }
                    // If there is a next container, move its pPrevious
                    if (pContainer->pNext != NULL) {
                        pContainer->pNext->pPrevious = pContainer->pPrevious;
                    }

                    // Remember the next pointer and the
                    // network handle
                    pTmp = pContainer->pNext;
                    devHandle = pContainer->socket.devHandle;

                    // Free the memory
                    uPortFree(pContainer);
                    // Move to the next entry
                    pContainer = pTmp;
                } else {
                    // Remember the network handle
                    devHandle = pContainer->socket.devHandle;
                    pContainer->socket.state = U_SOCK_STATE_CLOSED;
                    // Move on
                    pContainer = pContainer->pNext;
                }

                if (devHandle != NULL) {
                    int32_t devType = uDeviceGetDeviceType(devHandle);
                    // Call the clean-up function in the underlying
                    // socket layer, where present
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        uCellSockCleanup(devHandle);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        uWifiSockCleanup(devHandle);
                    }
                }
            } else {
                // Move on but count the number of non-closed sockets
                numNonClosedSockets++;
                pContainer = pContainer->pNext;
            }
        }

        // If everything has been closed, we can deinit();
        if (numNonClosedSockets == 0) {
            deinitButNotMutex();
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }
}

// Close all sockets and free resource.
void uSockDeinit()
{
    uSockContainer_t *pContainer = gpContainerListHead;
    uSockContainer_t *pTmp;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    if (gInitialised) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Move through the list closing and
        // removing sockets
        while (pContainer != NULL) {
            if ((pContainer->socket.state != U_SOCK_STATE_CLOSING) &&
                (pContainer->socket.state != U_SOCK_STATE_CLOSED)) {
                // Talk to the underlying socket layer
                // to close the socket: ignoring errors here
                // 'cos there's nothing we can do,
                // we're closin' dowwwn...
                devHandle = pContainer->socket.devHandle;
                sockHandle = pContainer->socket.sockHandle;
                int32_t devType = uDeviceGetDeviceType(devHandle);
                if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                    uCellSockClose(devHandle, sockHandle, NULL);
                } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                    uWifiSockClose(devHandle, sockHandle, NULL);
                }
            }

            if (!(pContainer->isStatic)) {
                // If this socket is not static, uncouple it
                // If there is a previous container, move its pNext
                if (pContainer->pPrevious != NULL) {
                    pContainer->pPrevious->pNext = pContainer->pNext;
                } else {
                    // If there is no previous container, must be
                    // at the start of the list so move the head
                    // pointer on instead
                    gpContainerListHead = pContainer->pNext;
                }
                // If there is a next container, move its pPrevious
                if (pContainer->pNext != NULL) {
                    pContainer->pNext->pPrevious = pContainer->pPrevious;
                }

                // Remember the next pointer
                pTmp = pContainer->pNext;

                // Free the memory
                uPortFree(pContainer);
                // Move to the next entry
                pContainer = pTmp;
            } else {
                pContainer->socket.state = U_SOCK_STATE_CLOSED;
                // Move on
                pContainer = pContainer->pNext;
            }
        }

        // We can now deinit();
        deinitButNotMutex();

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURE
 * -------------------------------------------------------------- */

// Set a socket to be blocking or non-blocking.
void uSockBlockingSet(uSockDescriptor_t descriptor, bool isBlocking)
{
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        pContainer = pContainerFindByDescriptor(descriptor);
        errnoLocal = U_SOCK_EBADF;
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_ENONE;
            pContainer->socket.blocking = isBlocking;
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
    }
}

// Get whether a socket is blocking or not.
bool uSockBlockingGet(uSockDescriptor_t descriptor)
{
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    bool isBlocking = false;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_ENONE;
            isBlocking = pContainer->socket.blocking;
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
    }

    return isBlocking;
}

// Set the options for the given socket.
int32_t uSockOptionSet(uSockDescriptor_t descriptor,
                       int32_t level, uint32_t option,
                       const void *pOptionValue,
                       size_t optionValueLength)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    uPortLog("U_SOCK: option set command %d:0x%04x called"
             " on descriptor %d with value ", option, level,
             descriptor);
    printSocketOption(pOptionValue, optionValueLength);
    uPortLog("\n");

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_EINVAL;
            // Check parameters
            if ((optionValueLength == 0) ||
                ((optionValueLength > 0) && (pOptionValue != NULL))) {
                if ((level == U_SOCK_OPT_LEVEL_SOCK) &&
                    (option == U_SOCK_OPT_RCVTIMEO)) {
                    // Receive timeout we set locally
                    if ((pOptionValue != NULL) &&
                        (optionValueLength == sizeof(struct timeval))) {
                        // All good
                        errnoLocal = U_SOCK_ENONE;
                        pContainer->socket.receiveTimeoutMs =
                            (((const struct timeval *) pOptionValue)->tv_usec / 1000) +
                            (((int64_t) ((const struct timeval *) pOptionValue)->tv_sec) * 1000);
                        uPortLog("U_SOCK: timeout for socket descriptor"
                                 " %d set to %d ms.\n", descriptor,
                                 (int32_t) pContainer->socket.receiveTimeoutMs);
                    } else {
                        uPortLog("U_SOCK: socket option %d:0x%04x"
                                 " could not be set to value ",
                                 option, level);
                        printSocketOption(pOptionValue, optionValueLength);
                        uPortLog("\n");
                    }
                } else {
                    // Otherwise talk to the underlying socket
                    // layer to set the socket option.
                    // uXxxSockOptionSet() returns a negated value
                    // of errno from the U_SOCK_Exxx list
                    devHandle = pContainer->socket.devHandle;
                    sockHandle = pContainer->socket.sockHandle;
                    errnoLocal = U_SOCK_ENONE;
                    errorCode = -U_SOCK_ENOSYS;
                    int32_t devType = uDeviceGetDeviceType(devHandle);
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        errorCode = uCellSockOptionSet(devHandle,
                                                       sockHandle,
                                                       level, option,
                                                       pOptionValue,
                                                       optionValueLength);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        errorCode = uWifiSockOptionSet(devHandle,
                                                       sockHandle,
                                                       level, option,
                                                       pOptionValue,
                                                       optionValueLength);
                    }

                    if (errorCode == 0) {
                        // All good
                        uPortLog("U_SOCK: socket option %d:0x%04x"
                                 " set to value ", option, level);
                    } else {
                        // Invalid argument
                        errnoLocal = -errorCode;
                        uPortLog("U_SOCK: errno %d when setting"
                                 " socket option %d:0x%04x to value ",
                                 errnoLocal, option, level);
                    }
                    printSocketOption(pOptionValue, optionValueLength);
                    uPortLog("by network handle 0x%08x, socket"
                             " handle %d.\n", devHandle,
                             sockHandle);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Get the options for the given socket.
int32_t uSockOptionGet(uSockDescriptor_t descriptor,
                       int32_t level, uint32_t option,
                       void *pOptionValue,
                       size_t *pOptionValueLength)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_EINVAL;
            // If there's an optionValue then there must be a length
            if ((pOptionValue == NULL) ||
                (pOptionValueLength != NULL)) {
                if ((level == U_SOCK_OPT_LEVEL_SOCK) &&
                    (option == U_SOCK_OPT_RCVTIMEO)) {
                    // Receive timeout we have locally
                    if (pOptionValueLength != NULL) {
                        if (pOptionValue != NULL) {
                            if (*pOptionValueLength >= sizeof(struct timeval)) {
                                errnoLocal = U_SOCK_ENONE;
                                // Return the answer
                                ((struct timeval *) pOptionValue)->tv_sec =
                                    (int32_t) (pContainer->socket.receiveTimeoutMs / 1000);
                                ((struct timeval *) pOptionValue)->tv_usec =
                                    (pContainer->socket.receiveTimeoutMs % 1000) * 1000;
                                *pOptionValueLength = sizeof(struct timeval);
                                uPortLog("U_SOCK: timeout for socket descriptor"
                                         " %d is %d ms.\n", descriptor,
                                         (int32_t) pContainer->socket.receiveTimeoutMs);
                            }
                        } else {
                            errnoLocal = U_SOCK_ENONE;
                            // Caller just wants to know the length required
                            *pOptionValueLength = sizeof(struct timeval);
                        }
                    }
                } else {
                    // Otherwise talk to the underlying socket layer
                    // to get the socket option.
                    // uXxxSockOptionGet() returns a negated value of
                    // errno from the U_SOCK_Exxx list.
                    devHandle = pContainer->socket.devHandle;
                    sockHandle = pContainer->socket.sockHandle;
                    errnoLocal = U_SOCK_ENONE;
                    errorCode = -U_SOCK_ENOSYS;
                    int32_t devType = uDeviceGetDeviceType(devHandle);
                    if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                        errorCode = uCellSockOptionGet(devHandle,
                                                       sockHandle,
                                                       level, option,
                                                       pOptionValue,
                                                       pOptionValueLength);
                    } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                        errorCode = uWifiSockOptionGet(devHandle,
                                                       sockHandle,
                                                       level, option,
                                                       pOptionValue,
                                                       pOptionValueLength);
                    }

                    if (errorCode == 0) {
                        // All good.
                        if (pOptionValue != NULL) {
                            uPortLog("U_SOCK: the value of option %d:0x%04x"
                                     " for socket descriptor %d is ", option,
                                     level, descriptor);
                            printSocketOption(pOptionValue, *pOptionValueLength);
                            uPortLog("according to network handle 0x%08x, socket"
                                     " handle %d.\n", devHandle, sockHandle);
                        }
                    } else {
                        // Set errno
                        errnoLocal = -errorCode;
                        uPortLog("U_SOCK: getting the value of option"
                                 " %d:0x%04x for socket descriptor %d from"
                                 " network handle 0x%08x, socket handle %d,"
                                 " returned errno %d.\n",
                                 option, level, descriptor, devHandle,
                                 sockHandle, errnoLocal);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Add security to the given socket.
int32_t uSockSecurity(uSockDescriptor_t descriptor,
                      const uSecurityTlsSettings_t *pSettings)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_ENONE;
            // Talk to the common security layer
            devHandle = pContainer->socket.devHandle;
            sockHandle = pContainer->socket.sockHandle;
            pContainer->socket.pSecurityContext = pUSecurityTlsAdd(devHandle,
                                                                   pSettings);
            if (pContainer->socket.pSecurityContext == NULL) {
                errnoLocal = U_SOCK_ENOMEM;
            } else if (pContainer->socket.pSecurityContext->errorCode != 0) {
                errorCode = pContainer->socket.pSecurityContext->errorCode;
                uSecurityTlsRemove(pContainer->socket.pSecurityContext);
                switch (errorCode) {
                    case U_ERROR_COMMON_INVALID_PARAMETER:
                        errnoLocal = U_SOCK_EINVAL;
                        break;
                    case U_ERROR_COMMON_NO_MEMORY:
                        errnoLocal = U_SOCK_ENOMEM;
                        break;
                    default:
                        errnoLocal = U_SOCK_EOPNOTSUPP;
                        break;
                }
            } else {
                int32_t devType = uDeviceGetDeviceType(devHandle);
                // We're good
                if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                    // In the cellular case the security
                    // profile has to be applied before connect
                    errnoLocal = -uCellSockSecure(devHandle,
                                                  sockHandle,
                                                  ((uCellSecTlsContext_t *) (pContainer->socket.pSecurityContext->pNetworkSpecific))->profileId);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Set a local port which will be used on the next uSockCreate().
int32_t uSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        errorCode = -U_SOCK_ENOSYS;
        int32_t devType = uDeviceGetDeviceType(devHandle);
        if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
            errorCode = uCellSockSetNextLocalPort(devHandle, port);
        } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
            errorCode = uWifiSockSetNextLocalPort(devHandle, port);
        }

        if (errorCode < 0) {
            // Set errno
            errnoLocal = -errorCode;
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: UDP ONLY
 * -------------------------------------------------------------- */

// Send a datagram to the given host.
int32_t uSockSendTo(uSockDescriptor_t descriptor,
                    const uSockAddress_t *pRemoteAddress,
                    const void *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            // Check address and state
            if (pRemoteAddress != NULL) {
                errnoLocal = U_SOCK_ENONE;
            } else {
                // If there is no remote address and the socket was
                // connected we must use the stored address
                if (pContainer->socket.state == U_SOCK_STATE_CONNECTED) {
                    pRemoteAddress = &(pContainer->socket.remoteAddress);
                    errnoLocal = U_SOCK_ENONE;
                } else {
                    if ((pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_WRITE) ||
                        (pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE)) {
                        // Socket is shut down
                        errnoLocal = U_SOCK_ESHUTDOWN;
                    } else if (pContainer->socket.state == U_SOCK_STATE_CLOSING) {
                        // I know connection isn't strictly relevant
                        // to UDP transmission but I can't see anything
                        // more appropriate to return
                        errnoLocal = U_SOCK_ENOTCONN;
                    } else {
                        // Destination address required?
                        errnoLocal = U_SOCK_EDESTADDRREQ;
                    }
                }
            }
            if ((errnoLocal == U_SOCK_ENONE) && (pRemoteAddress != NULL)) {
                errnoLocal = U_SOCK_EPROTOTYPE;
                // It is OK to send UDP packets on a TCP socket
                if ((pContainer->socket.protocol == U_SOCK_PROTOCOL_UDP) ||
                    (pContainer->socket.protocol == U_SOCK_PROTOCOL_TCP)) {
                    errnoLocal = U_SOCK_EINVAL;
                    if ((pData == NULL) && (dataSizeBytes > 0)) {
                        // Invalid argument
                    } else {
                        errnoLocal = U_SOCK_ENONE;
                        if ((pData != NULL) && (dataSizeBytes > 0)) {
                            // Talk to the underlying cell/wifi
                            // socket layer to send the datagram.
                            // uXxxSockSendTo() returns the number of
                            // bytes sent or a negated value of errno
                            // from the U_SOCK_Exxx list.
                            devHandle = pContainer->socket.devHandle;
                            sockHandle = pContainer->socket.sockHandle;
                            errorCodeOrSize = -U_SOCK_ENOSYS;
                            int32_t devType = uDeviceGetDeviceType(devHandle);
                            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                                errorCodeOrSize = uCellSockSendTo(devHandle,
                                                                  sockHandle,
                                                                  pRemoteAddress,
                                                                  pData,
                                                                  dataSizeBytes);
                                if (errorCodeOrSize > 0) {
                                    pContainer->socket.bytesSent += errorCodeOrSize;
                                }
                            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                                errorCodeOrSize = uWifiSockSendTo(devHandle,
                                                                  sockHandle,
                                                                  pRemoteAddress,
                                                                  pData,
                                                                  dataSizeBytes);
                                if (errorCodeOrSize > 0) {
                                    pContainer->socket.bytesSent += errorCodeOrSize;
                                }
                            }

                            if (errorCodeOrSize < 0) {
                                // Set errno
                                errnoLocal = -errorCodeOrSize;
                            }
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCodeOrSize;
}

int32_t uSockGetTotalBytesSent(uSockDescriptor_t descriptor)
{
    int32_t errorCodeOrTotalBytesSent = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uSockContainer_t *pContainer;

    pContainer = pContainerFindByDescriptor(descriptor);

    if (pContainer != NULL) {
        errorCodeOrTotalBytesSent = pContainer->socket.bytesSent;
    }

    return errorCodeOrTotalBytesSent;
}

// Receive a single datagram from the given host.
int32_t uSockReceiveFrom(uSockDescriptor_t descriptor,
                         uSockAddress_t *pRemoteAddress,
                         void *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_EPROTOTYPE;
            // It is OK to receive UDP-style on a TCP socket
            if ((pContainer->socket.protocol == U_SOCK_PROTOCOL_UDP) ||
                (pContainer->socket.protocol == U_SOCK_PROTOCOL_TCP)) {
                // I know connection isn't strictly relevant
                // to UDP but I can't see anything more
                // appropriate to return
                errnoLocal = U_SOCK_ENOTCONN;
                if (pContainer->socket.state != U_SOCK_STATE_CLOSING) {
                    errnoLocal = U_SOCK_ESHUTDOWN;
                    if ((pContainer->socket.state != U_SOCK_STATE_SHUTDOWN_FOR_READ) &&
                        (pContainer->socket.state != U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE)) {
                        errnoLocal = U_SOCK_EINVAL;
                        if ((pData == NULL) && (dataSizeBytes > 0)) {
                            // Invalid argument
                        } else {
                            errnoLocal = U_SOCK_ENONE;
                            if ((pData != NULL) && (dataSizeBytes != 0)) {
                                // Receive the datagram
                                errorCodeOrSize = receive(pContainer,
                                                          pRemoteAddress,
                                                          pData,
                                                          dataSizeBytes);
                                if (errorCodeOrSize < 0) {
                                    // Set errno
                                    errnoLocal = -errorCodeOrSize;
                                }
                            }
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: STREAM (TCP)
 * -------------------------------------------------------------- */

// Send data.
int32_t uSockWrite(uSockDescriptor_t descriptor,
                   const void *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_EPROTOTYPE;
            if (pContainer->socket.protocol == U_SOCK_PROTOCOL_TCP) {
                if (pContainer->socket.state == U_SOCK_STATE_CONNECTED) {
                    errnoLocal = U_SOCK_EINVAL;
                    if (((pData == NULL) && (dataSizeBytes > 0)) ||
                        (dataSizeBytes > INT_MAX)) {
                        // Invalid argument
                    } else {
                        errnoLocal = U_SOCK_ENONE;
                        if ((pData != NULL) && (dataSizeBytes != 0)) {
                            // Talk to the underlying cell/wifi
                            // socket layer to send the datagram.
                            // uXxxSockWrite() returns the number
                            // of bytes sent or a negated value of
                            // errno from the U_SOCK_Exxx list.
                            devHandle = pContainer->socket.devHandle;
                            sockHandle = pContainer->socket.sockHandle;
                            errorCodeOrSize = -U_SOCK_ENOSYS;
                            int32_t devType = uDeviceGetDeviceType(devHandle);
                            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                                errorCodeOrSize = uCellSockWrite(devHandle,
                                                                 sockHandle,
                                                                 pData,
                                                                 dataSizeBytes);
                                if (errorCodeOrSize > 0) {
                                    pContainer->socket.bytesSent += errorCodeOrSize;
                                }
                            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                                errorCodeOrSize = uWifiSockWrite(devHandle,
                                                                 sockHandle,
                                                                 pData,
                                                                 dataSizeBytes);
                                if (errorCodeOrSize > 0) {
                                    pContainer->socket.bytesSent += errorCodeOrSize;
                                }
                            }

                            if (errorCodeOrSize < 0) {
                                // Set errno
                                errnoLocal = -errorCodeOrSize;
                            }
                        }
                    }
                } else {
                    if ((pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_READ) ||
                        (pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE)) {
                        // Socket is shut down
                        errnoLocal = U_SOCK_ESHUTDOWN;
                    } else if (pContainer->socket.state == U_SOCK_STATE_CLOSING) {
                        // Not connected mate
                        errnoLocal = U_SOCK_ENOTCONN;
                    } else {
                        // No route to host?
                        errnoLocal = U_SOCK_EHOSTUNREACH;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCodeOrSize;
}

// Receive data.
int32_t uSockRead(uSockDescriptor_t descriptor,
                  void *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            errnoLocal = U_SOCK_EPROTOTYPE;
            if (pContainer->socket.protocol == U_SOCK_PROTOCOL_TCP) {
                if (pContainer->socket.state == U_SOCK_STATE_CONNECTED) {
                    errnoLocal = U_SOCK_EINVAL;
                    if (((pData == NULL) && (dataSizeBytes > 0)) ||
                        (dataSizeBytes > INT_MAX)) {
                        // Invalid argument
                    } else {
                        errnoLocal = U_SOCK_ENONE;
                        if ((pData != NULL) && (dataSizeBytes != 0)) {
                            // Receive the datagram
                            errorCodeOrSize = receive(pContainer,
                                                      NULL, pData,
                                                      dataSizeBytes);
                            if (errorCodeOrSize < 0) {
                                // Set errno
                                errnoLocal = -errorCodeOrSize;
                            }
                        }
                    }
                } else {
                    if ((pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_READ) ||
                        (pContainer->socket.state == U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE)) {
                        // Socket is shut down
                        errnoLocal = U_SOCK_ESHUTDOWN;
                    } else if (pContainer->socket.state == U_SOCK_STATE_CLOSING) {
                        // Not connected mate
                        errnoLocal = U_SOCK_ENOTCONN;
                    } else {
                        // No route to host?
                        errnoLocal = U_SOCK_EHOSTUNREACH;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCodeOrSize;
}

// Prepare a TCP socket for being closed.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockShutdown(uSockDescriptor_t descriptor,
                      uSockShutdown_t how)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            // Set the socket state
            switch (how) {
                case U_SOCK_SHUTDOWN_READ:
                    pContainer->socket.state = U_SOCK_STATE_SHUTDOWN_FOR_READ;
                    errnoLocal = U_SOCK_ENONE;
                    break;
                case U_SOCK_SHUTDOWN_WRITE:
                    pContainer->socket.state = U_SOCK_STATE_SHUTDOWN_FOR_WRITE;
                    errnoLocal = U_SOCK_ENONE;
                    break;
                case U_SOCK_SHUTDOWN_READ_WRITE:
                    pContainer->socket.state = U_SOCK_STATE_SHUTDOWN_FOR_READ_WRITE;
                    errnoLocal = U_SOCK_ENONE;
                    break;
                default:
                    errnoLocal = U_SOCK_EINVAL;
                    break;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: ASYNC
 * -------------------------------------------------------------- */

// Register a callback for incoming data.
void uSockRegisterCallbackData(uSockDescriptor_t descriptor,
                               void (*pCallback) (void *),
                               void *pCallbackParameter)
{
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {
            U_PORT_MUTEX_LOCK(gMutexCallbacks);

            // Talk to the underlying cell/wifi
            // socket layer to set the callback.
            devHandle = pContainer->socket.devHandle;
            sockHandle = pContainer->socket.sockHandle;
            errnoLocal = U_SOCK_ENOSYS;
            int32_t devType = uDeviceGetDeviceType(devHandle);
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                uCellSockRegisterCallbackData(devHandle,
                                              sockHandle,
                                              dataCallback);
                errnoLocal = U_SOCK_ENONE;
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                errnoLocal = -uWifiSockRegisterCallbackData(devHandle,
                                                            sockHandle,
                                                            dataCallback);
            }

            if (errnoLocal == U_SOCK_ENONE) {
                pContainer->socket.pDataCallback = pCallback;
                pContainer->socket.pDataCallbackParameter = pCallbackParameter;
            }

            U_PORT_MUTEX_UNLOCK(gMutexCallbacks);
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
    }
}

// Register a callback for remote socket closure.
void uSockRegisterCallbackClosed(uSockDescriptor_t descriptor,
                                 void (*pCallback) (void *),
                                 void *pCallbackParameter)
{
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {

        U_PORT_MUTEX_LOCK(gMutexContainer);

        // Find the container
        errnoLocal = U_SOCK_EBADF;
        pContainer = pContainerFindByDescriptor(descriptor);
        if (pContainer != NULL) {

            U_PORT_MUTEX_LOCK(gMutexCallbacks);

            // Talk to the underlying cell/wifi
            // socket layer to set the callback.
            devHandle = pContainer->socket.devHandle;
            sockHandle = pContainer->socket.sockHandle;
            errnoLocal = U_SOCK_ENOSYS;
            int32_t devType = uDeviceGetDeviceType(devHandle);
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                uCellSockRegisterCallbackClosed(devHandle,
                                                sockHandle,
                                                closedCallback);
                errnoLocal = U_SOCK_ENONE;
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                errnoLocal = -uWifiSockRegisterCallbackClosed(devHandle,
                                                              sockHandle,
                                                              closedCallback);
            }

            if (errnoLocal == U_SOCK_ENONE) {
                pContainer->socket.pClosedCallback = pCallback;
                pContainer->socket.pClosedCallbackParameter = pCallbackParameter;
            }

            U_PORT_MUTEX_UNLOCK(gMutexCallbacks);
        }

        U_PORT_MUTEX_UNLOCK(gMutexContainer);
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TCP INCOMING (TCP SERVER) ONLY
 * -------------------------------------------------------------- */

// Prepare a socket for receiving incoming TCP connections by
// binding it to an address.
int32_t uSockBind(uSockDescriptor_t descriptor,
                  const uSockAddress_t *pLocalAddress)
{
    // TODO
    (void) descriptor;
    (void) pLocalAddress;
    errno = U_SOCK_ENOSYS;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Set listening mode.
int32_t uSockListen(uSockDescriptor_t descriptor, size_t backlog)
{
    // TODO
    (void) descriptor;
    (void) backlog;
    errno = U_SOCK_ENOSYS;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Accept an incoming TCP connection on the given socket.
int32_t uSockAccept(uSockDescriptor_t descriptor,
                    uSockAddress_t *pRemoteAddress)
{
    // TODO
    (void) descriptor;
    (void) pRemoteAddress;
    errno = U_SOCK_ENOSYS;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Select: wait for one of a set of sockets to become unblocked.
int32_t uSockSelect(int32_t maxDescriptor,
                    uSockDescriptorSet_t *pReadDescriptorSet,
                    uSockDescriptorSet_t *pWriteDescriptoreSet,
                    uSockDescriptorSet_t *pExceptDescriptorSet,
                    int32_t timeMs)
{
    // TODO
    (void) maxDescriptor;
    (void) pReadDescriptorSet;
    (void) pWriteDescriptoreSet;
    (void) pExceptDescriptorSet;
    (void) timeMs;
    errno = U_SOCK_ENOSYS;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: FINDING ADDRESSES
 * -------------------------------------------------------------- */

// Get the address of the remote host connected to a given socket.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockGetRemoteAddress(uSockDescriptor_t descriptor,
                              uSockAddress_t *pRemoteAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {
        errnoLocal = U_SOCK_EINVAL;
        // Check parameters
        if (pRemoteAddress != NULL) {

            U_PORT_MUTEX_LOCK(gMutexContainer);

            // Find the container
            errnoLocal = U_SOCK_EBADF;
            pContainer = pContainerFindByDescriptor(descriptor);
            if (pContainer != NULL) {
                errnoLocal = U_SOCK_EHOSTUNREACH;
                if (pContainer->socket.state == U_SOCK_STATE_CONNECTED) {
                    memcpy(pRemoteAddress,
                           &(pContainer->socket.remoteAddress),
                           sizeof(*pRemoteAddress));
                    errnoLocal = U_SOCK_ENONE;
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexContainer);
        }
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Get the local address of the given socket.
int32_t uSockGetLocalAddress(uSockDescriptor_t descriptor,
                             uSockAddress_t *pLocalAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;
    uSockContainer_t *pContainer = NULL;
    uDeviceHandle_t devHandle;
    int32_t sockHandle;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {
        errnoLocal = U_SOCK_EINVAL;
        // Check parameters
        if (pLocalAddress != NULL) {

            U_PORT_MUTEX_LOCK(gMutexContainer);

            // Check that the descriptor is at least valid
            errnoLocal = U_SOCK_EBADF;
            pContainer = pContainerFindByDescriptor(descriptor);
            if (pContainer != NULL) {
                // Talk to the underlying cell/wifi
                // socket layer to get the local address.
                // uXxxSockGetLocalAddress() returns a negated
                // value from the U_SOCK_Exxx list.
                devHandle = pContainer->socket.devHandle;
                sockHandle = pContainer->socket.sockHandle;
                errnoLocal = U_SOCK_ENOSYS;
                int32_t devType = uDeviceGetDeviceType(devHandle);
                if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                    errnoLocal = -uCellSockGetLocalAddress(devHandle,
                                                           sockHandle,
                                                           pLocalAddress);
                } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                    errnoLocal = -uWifiSockGetLocalAddress(devHandle,
                                                           sockHandle,
                                                           pLocalAddress);
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexContainer);
        }
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

// Get the IP address of the given host name.
int32_t uSockGetHostByName(uDeviceHandle_t devHandle,
                           const char *pHostName,
                           uSockIpAddress_t *pHostIpAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    int32_t errnoLocal;

    errnoLocal = init();
    if (errnoLocal == U_SOCK_ENONE) {
        errnoLocal = U_SOCK_EINVAL;
        // Check parameters
        if ((pHostName != NULL) && (pHostIpAddress != NULL)) {

            U_PORT_MUTEX_LOCK(gMutexContainer);

            int32_t devType = uDeviceGetDeviceType(devHandle);

            // Talk to the underlying cell/wifi
            // socket layer to do the DNS look-up.
            // uXxxSockGetHostByName() returns a negated
            // value from the U_SOCK_Exxx list.
            errnoLocal = U_SOCK_ENOSYS;
            if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                errnoLocal = -uCellSockGetHostByName(devHandle,
                                                     pHostName,
                                                     pHostIpAddress);
            } else if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                errnoLocal = -uWifiSockGetHostByName(devHandle,
                                                     pHostName,
                                                     pHostIpAddress);
            }

            U_PORT_MUTEX_UNLOCK(gMutexContainer);
        }
    }

    if (errnoLocal != U_SOCK_ENONE) {
        // Write the errno
        errno = errnoLocal;
        errorCode = (int32_t) U_ERROR_COMMON_BSD_ERROR;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: ADDRESS CONVERSION
 * -------------------------------------------------------------- */

// Convert an IP address string into a struct.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockStringToAddress(const char *pAddressString,
                             uSockAddress_t *pAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // No need to call init(); here, this does not use the mutexes
    if ((pAddressString != NULL) && (pAddress != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
        if (addressStringIsIpv4(pAddressString)) {
            if (ipv4StringToAddress(pAddressString, pAddress)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        } else {
            if (ipv6StringToAddress(pAddressString, pAddress)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Convert an IP address struct into a string.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockIpAddressToString(const uSockIpAddress_t *pIpAddress,
                               char *pBuffer,
                               size_t sizeBytes)
{
    int32_t stringLengthOrError = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // No need to call init(); here, this does not use the mutexes
    if ((pIpAddress != NULL) && (pBuffer != NULL)) {
        stringLengthOrError = ipAddressToString(pIpAddress, pBuffer,
                                                sizeBytes);
    }

    return stringLengthOrError;
}

// Convert an address struct into a string.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockAddressToString(const uSockAddress_t *pAddress,
                             char *pBuffer,
                             size_t sizeBytes)
{
    int32_t stringLengthOrError = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // No need to call init(); here, this does not use the mutexes
    if ((pAddress != NULL) && (pBuffer != NULL)) {
        stringLengthOrError = addressToString(pAddress, true,
                                              pBuffer, sizeBytes);
    }

    return stringLengthOrError;
}

// Get the port number from a domain name.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
int32_t uSockDomainGetPort(char *pDomainString)
{
    int32_t port = -1;
    int32_t x;
    const char *pColon;

    pColon = pAddressPortSeparator(pDomainString);
    if (pColon != NULL) {
        x = strtol(pColon + 1, NULL, 10);
        if (x <= (int32_t) USHRT_MAX) {
            port = x;
        }
    }

    return port;
}

// Turn a domain name string into just the name part.
// Note: this does not need to reference the underlying
// cell/wifi socket layer.
char *pUSockDomainRemovePort(char *pDomainString)
{
    char *pColon;

    pColon = pAddressPortSeparator(pDomainString);
    if (pColon != NULL) {
        // Overwrite the colon with a NULL to remove it
        *pColon = '\0';
        if (*pDomainString == '[') {
            // If there was a '[' at the start of the
            // domain string then it is an IPV6 address
            // with a port number. In this case
            // we need to overwrite the closing ']' with
            // a NULL and set the return pointer to be
            // one beyond the '['.
            pColon--;
            *pColon = '\0';
            pDomainString++;
        }
    }

    return pDomainString;
}

// End of file
