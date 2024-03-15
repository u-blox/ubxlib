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

/** @file
 * @brief This file allows a connection to be made from pppd to a
 * PPP interface inside ubxlib.  Such a PPP interface is provided by
 * a cellular module.
 *
 * See port/platform/linux/README.md for a description of how it works.
 *
 * It is only compiled if U_CFG_PPP_ENABLE is defined.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen(), memset()

#include "pthread.h"  // threadId
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "unistd.h"
#include "errno.h"
#include "sys/types.h"

#include "u_cfg_os_platform_specific.h" // U_CFG_OS_PRIORITY_MAX
#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_linked_list.h"

#include "u_sock.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_port_heap.h"
#include "u_port_ppp.h"
#include "u_port_ppp_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_PPP_LOCAL_DEVICE_NAME
/** The name of the device that is the PPP entity at the bottom
 * of the IP stack on this MCU, i.e. the Linux-end of the PPP
 * link that pppd will connect to.
 */
# define U_PORT_PPP_LOCAL_DEVICE_NAME "127.0.0.1:5000"
#endif

#ifndef U_PORT_PPP_CONNECT_TIMEOUT_SECONDS
/** How long to wait for PPP to connect.
 */
# define U_PORT_PPP_CONNECT_TIMEOUT_SECONDS 15
#endif

#ifndef U_PORT_PPP_DISCONNECT_TIMEOUT_SECONDS
/** How long to wait for PPP to disconnect.
 */
# define U_PORT_PPP_DISCONNECT_TIMEOUT_SECONDS 10
#endif

#ifndef U_PORT_PPP_TX_LOOP_GUARD
/** How many times around the transmit loop to allow if stuff
 * won't send.
 */
# define U_PORT_PPP_TX_LOOP_GUARD 1000
#endif

#ifndef U_PORT_PPP_TX_LOOP_DELAY_MS
/** How long to wait between transmit attempts in milliseconds
 * when the data to transmit won't go all at once.
 */
# define U_PORT_PPP_TX_LOOP_DELAY_MS 10
#endif

#ifndef U_PORT_PPP_SOCKET_TASK_STACK_SIZE_BYTES
/** The stack size for the callback that is listening for
 * the pppd connection locally and shipping data out from it.
 */
# define U_PORT_PPP_SOCKET_TASK_STACK_SIZE_BYTES (1024 * 5)
#endif

#ifndef U_PORT_PPP_SOCKET_TASK_PRIORITY
/** The priority of the task that is listening for the pppd
 * connection locally receiving data fro it, should
 * be relatively high (e.g. U_CFG_OS_PRIORITY_MAX - 5, which is
 * the same as the AT Client URC task).
 */
# define U_PORT_PPP_SOCKET_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)
#endif

#ifndef U_PORT_PPP_BUFFER_CACHE_SIZE
/** pppd has no way to tell this code that the link is up,
 * so we keep a small cache of the communications in both
 * directions that we can monitor to see what's going on.
 * IMPORTANT: this must be at least as big as
 * gPppEncapsulatedIpcpPacketStart[], gLcpTerminateReqPacket[],
 * gLcpTerminateAckPacket[] and gConnectionTerminatedString[] for
 * this code to work.
 */
# define U_PORT_PPP_BUFFER_CACHE_SIZE 64
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#ifdef U_CFG_PPP_ENABLE

/** A structure to contain a buffer, used for monitoring
 * communications between the PPP entities.
 */
typedef struct {
    char buffer[U_PORT_PPP_BUFFER_CACHE_SIZE];
    size_t size;
} uPortPppBufferCache_t;

/** Define a PPP interface.
 */
typedef struct {
    void *pDevHandle;
    int listeningSocket; // int type since this is a native socket
    int connectedSocket;
    uPortTaskHandle_t socketTaskHandle;
    uPortMutexHandle_t socketTaskMutex;
    bool socketTaskExit;
    uPortPppBufferCache_t fromModuleBufferCache;
    uPortPppBufferCache_t fromPppdBufferCache;
    bool dataTransferSuspended;
    uPortPppConnectCallback_t *pConnectCallback;
    uPortPppDisconnectCallback_t *pDisconnectCallback;
    uPortPppTransmitCallback_t *pTransmitCallback;
    bool pppRunning;
    bool ipConnected;
    bool waitingForModuleDisconnect;
} uPortPppInterface_t;

/** Structure to hold the name of the MCU-end PPP device;
 * used to ensure thread-safety between calls to
 * uPortPppSetLocalDeviceName() and uPortPppAttach().
 */
typedef struct {
    char name[U_PORT_PPP_LOCAL_DEVICE_NAME_LENGTH + 1]; // +1 for terminator
    pthread_t threadId;
} uPortPppLocalDevice_t;

#endif // #ifdef U_CFG_PPP_ENABLE

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_PPP_ENABLE

/** Root of the linked list of PPP entities.
 */
static uLinkedList_t *gpPppInterfaceList = NULL; /**< A list of uPortPppInterface_t. */

/** Root of linked list of local device names.
 */
static uLinkedList_t *gpPppLocalDeviceNameList = NULL; /**< A list of uPortPppLocalDevice_t. */

/** Mutex to protect the linked list of PPP entities.
 */
static uPortMutexHandle_t gMutex = NULL;

/** The bytes that represent the start of a PPP-encapsulated
 * IPCP packet.
 */
static const char gPppEncapsulatedIpcpPacketStart[] = {0x7e, 0x80, 0x21};

/** The bytes that represent a normal LCP Terminate-Req.
 */
static const char gLcpTerminateReqPacket[] = {0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21, 0x7d, 0x25,
                                              0x7d, 0x22, 0x7d, 0x20, 0x7d, 0x30, 0x55, 0x73,
                                              0x65, 0x72, 0x20, 0x72, 0x65, 0x71, 0x75, 0x65,
                                              0x73, 0x74, 0x53, 0x33, 0x7e
                                             };

/** The bytes that represent an LCP Terminate-Ack for
 * gLcpTerminateReqPacket[].
 */
static const char gLcpTerminateAckPacket[] = {0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21, 0x7d, 0x26,
                                              0x7d, 0x22, 0x7d, 0x20, 0x7d, 0x24, 0x94, 0x7d,
                                              0x2d, 0x7e
                                             };
/** The string that the cellular module sends in response
 * to an gLcpTerminateReqPacket[].
 */
static const char gConnectionTerminatedString[] = {'\r', '\n', 'N', 'O', ' ', 'C', 'A', 'R',
                                                   'R', 'I', 'E', 'R', '\r', '\n'
                                                  };

#endif // #ifdef U_CFG_PPP_ENABLE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_PPP_ENABLE

// Find the local device name set by the given thread.
static uPortPppLocalDevice_t *pFindLocalDeviceName(pthread_t threadId)
{
    uPortPppLocalDevice_t *pLocalDevice = NULL;
    uLinkedList_t *pList = gpPppLocalDeviceNameList;

    while ((pList != NULL) && (pLocalDevice == NULL)) {
        if (((uPortPppLocalDevice_t *) pList->p)->threadId == threadId) {
            pLocalDevice = (uPortPppLocalDevice_t *) pList->p;
        } else {
            pList = pList->pNext;
        }
    }

    return pLocalDevice;
}

// Find the PPP interface structure for the given handle.
static uPortPppInterface_t *pFindPppInterface(void *pDevHandle)
{
    uPortPppInterface_t *pPppInterface = NULL;
    uLinkedList_t *pList = gpPppInterfaceList;

    while ((pList != NULL) && (pPppInterface == NULL)) {
        if (((uPortPppInterface_t *) pList->p)->pDevHandle == pDevHandle) {
            pPppInterface = (uPortPppInterface_t *) pList->p;
        } else {
            pList = pList->pNext;
        }
    }

    return pPppInterface;
}

// Add the new pBuffer contents to pBufferCache and then determine
// if the buffer cache contains pBufferWanted.  If it does not but
// there is a partial match then the contents of pBufferCache are
// moved down to remove the uninteresting bits, else pBufferCache
// is cleared.
// IMPORTANT: since this removes the cached buffer contents based
// on whether there is a match or not it will ONLY WORK if a given
// cache buffer is searched for one set of wanted stuff at a time.
static bool bufferContains(uPortPppBufferCache_t *pBufferCache,
                           const char *pBuffer, size_t size,
                           const char *pBufferWanted, size_t bufferLength)
{
    size_t count = 0;
    size_t startOffset = 0;
    size_t x;

    if (pBufferCache != NULL) {
        if (pBuffer != NULL) {
            // Copy as much of the new data as we can into the buffer cache
            if (size > sizeof(pBufferCache->buffer) - pBufferCache->size) {
                size = sizeof(pBufferCache->buffer) - pBufferCache->size;
            }
            memcpy(pBufferCache->buffer + pBufferCache->size, pBuffer, size);
            pBufferCache->size += size;
        }
        if (pBufferWanted != NULL) {
            // Check for a match
            for (x = 0; (x < pBufferCache->size) && (count < bufferLength); x++) {
                if (pBufferCache->buffer[x] == *(pBufferWanted + count)) {
                    count++;
                } else {
                    count = 0;
                    if (pBufferCache->buffer[x] == *pBufferWanted) {
                        count = 1;
                    } else {
                        startOffset = x;
                    }
                }
            }
            if ((count > 0) && (count < bufferLength)) {
                // Partial match, move the contents of the cached buffer
                // down to remove the uninteresting bits
                x = pBufferCache->size - startOffset;
                memmove(pBufferCache->buffer, pBufferCache->buffer + startOffset, x);
                pBufferCache->size = x;
            } else {
                // Either a complete match or no match, clear the cache
                pBufferCache->size = 0;
            }
        }
    }

    return (count == bufferLength);
}

// Do a select on a socket with a timeout in milliseconds.
static bool socketSelect(int socket, int32_t timeoutMs)
{
    fd_set set;
    struct timeval timeout = {0};

    FD_ZERO(&set);
    FD_SET(socket, &set);
    timeout.tv_usec = timeoutMs * 1000;
    return (select(socket + 1, &set, NULL, NULL, &timeout) > 0);
}

// Terminate a PPP link.
static void terminateLink(uPortPppInterface_t *pPppInterface)
{
    char buffer[128];
    int32_t dataSize;
    int32_t sent;
    const char *pData;
    size_t retryCount = 0;
    int32_t startTimeMs;
    bool pppdConnected = (pPppInterface->connectedSocket >= 0);

    // First, suspend normal data transfer between the entities
    pPppInterface->dataTransferSuspended = true;

    // Start by terminating the cellular side
    if (pPppInterface->pTransmitCallback != NULL) {
        pPppInterface->waitingForModuleDisconnect = true;
        pData = gLcpTerminateReqPacket;
        dataSize = sizeof(gLcpTerminateReqPacket);
        while ((dataSize > 0) && (retryCount < U_PORT_PPP_TX_LOOP_GUARD)) {
            sent = pPppInterface->pTransmitCallback(pPppInterface->pDevHandle,
                                                    pData, dataSize);
            if (sent > 0) {
                dataSize -= sent;
                pData += sent;
            } else {
                retryCount++;
                uPortTaskBlock(U_PORT_PPP_TX_LOOP_DELAY_MS);
            }
        }
    }

    if (pPppInterface->connectedSocket >= 0) {
        // While we are waiting for a response (which will be
        // picked up by moduleDataCallback() by setting
        // pPppInterface->waitingForModuleDisconnect to false),
        // terminate pppd on the MCU-side
        pData = gLcpTerminateReqPacket;
        dataSize = sizeof(gLcpTerminateReqPacket);
        retryCount = 0;
        while ((dataSize > 0) && (retryCount < U_PORT_PPP_TX_LOOP_GUARD)) {
            // Note: send() is like write() but, when passed MSG_NOSIGNAL,
            // it returns an error if the far end has closed the socket,
            // rather than causing Linux to throw a signal 13 (SIGPIPE)
            // exception which the application would have to handle
            sent = send(pPppInterface->connectedSocket, pData, dataSize, MSG_NOSIGNAL);
            if (sent > 0) {
                dataSize -= sent;
                pData += sent;
            } else {
                retryCount++;
                uPortTaskBlock(U_PORT_PPP_TX_LOOP_DELAY_MS);
            }
        }
    }

    // Wait for the response from pppd on the MCU side, and
    // from the cellular side (via the waitingForModuleDisconnect flag)
    startTimeMs = uPortGetTickTimeMs();
    while ((pPppInterface->waitingForModuleDisconnect || pppdConnected) &&
           (uPortGetTickTimeMs() - startTimeMs < U_PORT_PPP_CONNECT_TIMEOUT_SECONDS * 1000)) {
        // Wait for data to arrive on the connected socket
        if (pppdConnected &&
            socketSelect(pPppInterface->connectedSocket, U_CFG_OS_YIELD_MS)) {
            // Read the data
            dataSize = read(pPppInterface->connectedSocket, buffer, sizeof(buffer));
            if ((dataSize > 0) &&
                bufferContains(&(pPppInterface->fromPppdBufferCache),
                               buffer, dataSize,
                               gLcpTerminateAckPacket,
                               sizeof(gLcpTerminateAckPacket))) {
                pppdConnected = false;
            }
        }
        uPortTaskBlock(250);
    }

    if (!pppdConnected && !pPppInterface->waitingForModuleDisconnect) {
        pPppInterface->ipConnected = false;
        pPppInterface->pppRunning = false;
    }

    // Give up waiting now whatever
    pPppInterface->waitingForModuleDisconnect = false;
}

// Callback for when data is received from the cellular side.
static void moduleDataCallback(void *pDevHandle, const char *pData,
                               size_t dataSize, void *pCallbackParam)
{
    uPortPppInterface_t *pPppInterface = (uPortPppInterface_t *) pCallbackParam;
    int32_t x = dataSize;
    const char *pTmp = pData;
    int32_t written = 0;
    size_t retryCount = 0;

    // Write the data to the connected socket, if there is one
    while (!pPppInterface->dataTransferSuspended && (x > 0) &&
           (written >= 0) && (retryCount < U_PORT_PPP_TX_LOOP_GUARD) &&
           (pPppInterface->connectedSocket >= 0)) {
        written = send(pPppInterface->connectedSocket, pTmp, x, MSG_NOSIGNAL);
        if (written > 0) {
            x -= written;
            pTmp += written;
        } else {
            retryCount++;
            uPortTaskBlock(U_PORT_PPP_TX_LOOP_DELAY_MS);
        }
    }
    // Note: the check below is performed even when data transfer
    // is suspended as we may still be expecting a disconnect
    if ((dataSize > 0) && pPppInterface->waitingForModuleDisconnect) {
        if (bufferContains(&(pPppInterface->fromModuleBufferCache),
                           pData, dataSize, gConnectionTerminatedString,
                           sizeof(gConnectionTerminatedString))) {
            pPppInterface->waitingForModuleDisconnect = false;
        }
    }
}

// Task to listen on a socket for a pppd connection and pull data from it.
static void socketTask(void *pParameters)
{
    uPortPppInterface_t *pPppInterface = (uPortPppInterface_t *) pParameters;
    char buffer[1024];  // Can be this big because we have allowed enough room on the stack
    char *pData;
    int32_t x;
    int32_t dataSize;
    int32_t written;
    size_t retryCount;

    // Lock the task mutex to indicate that we're running
    U_PORT_MUTEX_LOCK(pPppInterface->socketTaskMutex);

    // "1" here for just one connection at a time
    listen(pPppInterface->listeningSocket, 1);

    while (!pPppInterface->socketTaskExit) {
        // Wait for a connection using select with a timeout, don't block
        if (socketSelect(pPppInterface->listeningSocket, U_CFG_OS_YIELD_MS)) {
            // Got some data on the listening socket, accept the connection
            pPppInterface->connectedSocket = accept(pPppInterface->listeningSocket, NULL, NULL);
            if (pPppInterface->connectedSocket >= 0) {
                uPortLog("U_PORT_PPP: pppd has connected to socket.\n");
                while ((pPppInterface->connectedSocket >= 0) &&
                       !pPppInterface->socketTaskExit) {
                    // Wait for data to arrive on the connected socket
                    if (socketSelect(pPppInterface->connectedSocket, U_CFG_OS_YIELD_MS) &&
                        !pPppInterface->dataTransferSuspended) {
                        // Read the data
                        dataSize = read(pPppInterface->connectedSocket, buffer, sizeof(buffer));
                        if (dataSize > 0) {
                            if ((pPppInterface->pTransmitCallback != NULL) &&
                                !pPppInterface->dataTransferSuspended) {
                                // Write the data to the cellular module
                                retryCount = 0;
                                pData = buffer;
                                x = dataSize;
                                written = 0;
                                while (pPppInterface->pppRunning &&
                                       !pPppInterface->dataTransferSuspended &&
                                       (x > 0) && (written >= 0) && (retryCount < U_PORT_PPP_TX_LOOP_GUARD)) {
                                    written = pPppInterface->pTransmitCallback(pPppInterface->pDevHandle, pData, x);
                                    if (written > 0) {
                                        x -= written;
                                        pData += written;
                                    } else {
                                        retryCount++;
                                        uPortTaskBlock(U_PORT_PPP_TX_LOOP_DELAY_MS);
                                    }
                                }
                                if ((dataSize > 0) && !pPppInterface->ipConnected) {
                                    // If the connection is not already flagged as IP-connected,
                                    // check the buffer of data for the start of an encapsulated
                                    // IPCP frame, which indicates that we are done with the LCP
                                    // part, the only part that could fail: we are connected.
                                    pPppInterface->ipConnected = bufferContains(&(pPppInterface->fromPppdBufferCache),
                                                                                buffer, dataSize,
                                                                                gPppEncapsulatedIpcpPacketStart,
                                                                                sizeof(gPppEncapsulatedIpcpPacketStart));
                                }
                            }
                        } else if (dataSize == 0) {
                            // If select() indicated there was data and yet
                            // reading the data gives us nothing then this
                            // is the socket telling us that the far-end has
                            // closed it
                            close(pPppInterface->connectedSocket);
                            pPppInterface->connectedSocket = -1;
                        }
                    }
                }
                if (pPppInterface->socketTaskExit) {
                    // If we have been told to exit then close
                    // the connected socket on the way out
                    close(pPppInterface->connectedSocket);
                    pPppInterface->connectedSocket = -1;
                    uPortLog("U_PORT_PPP: pppd has been disconnected from socket.\n");
                } else {
                    uPortLog("U_PORT_PPP: pppd has disconnected from socket.\n");
                }
            }
        } else {
            uPortTaskBlock(250);
        }
    }
    close(pPppInterface->listeningSocket);
    uPortLog("U_PORT_PPP: no longer listening for pppd on socket.\n");

    // Unlock the task mutex to indicate we're done
    U_PORT_MUTEX_UNLOCK(pPppInterface->socketTaskMutex);

    uPortTaskDelete(NULL);
}

// Start a listening task on the address given.
static int32_t startSocketTask(uPortPppInterface_t *pPppInterface,
                               const char *pAddressString)
{
    int32_t errorCode;
    struct sockaddr_in socketAddress = {0};
    uSockAddress_t sockUbxlib;
    int reuse = 1;

    errorCode = uSockStringToAddress(pAddressString, &sockUbxlib);
    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Create a listening socket and bind the given address to it
        pPppInterface->connectedSocket = -1;
        pPppInterface->listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (pPppInterface->listeningSocket >= 0) {
            // Set SO_REUSEADDR (and, in some cases SO_REUSEPORT) so that we
            // can re-bind to the socket when we come back into here
            if (setsockopt(pPppInterface->listeningSocket, SOL_SOCKET, SO_REUSEADDR,
                           (const char *) &reuse, sizeof(reuse)) < 0) {
                // This is not fatal, it just means that the OS might prevent
                // us binding to the same address again if we come back into
                // here too quickly
                uPortLog("U_PORT_PPP: *** WARNING *** setting socket option SO_REUSEADDR"
                         " returned errno %d.\n", errno);
            }
#ifdef SO_REUSEPORT
            if (setsockopt(pPppInterface->listeningSocket, SOL_SOCKET, SO_REUSEPORT,
                           (const char *) &reuse, sizeof(reuse)) < 0) {
                // This is not fatal, it just means that the OS might prevent
                // us binding to the same address again if we come back into
                // here too quickly
                uPortLog("U_PORT_PPP: *** WARNING *** setting socket option SO_REUSEPORT"
                         " returned errno %d.\n", errno);
            }
#endif
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
            socketAddress.sin_family = AF_INET;
            if (sockUbxlib.ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
                socketAddress.sin_addr.s_addr = htonl(sockUbxlib.ipAddress.address.ipv4);
            } else {
                // TODO: find out how this copy should work for an IPV6 address
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            }
            socketAddress.sin_port = htons(sockUbxlib.port);
            if (bind(pPppInterface->listeningSocket,
                     (struct sockaddr *) &socketAddress,
                     sizeof(socketAddress)) == 0) {
                // Now kick off a task that will listen on that socket
                // and read data from anything that attaches to it
                errorCode = uPortMutexCreate(&(pPppInterface->socketTaskMutex));
                if (errorCode == 0) {
                    errorCode = uPortTaskCreate(socketTask,
                                                "pppSocketTask",
                                                U_PORT_PPP_SOCKET_TASK_STACK_SIZE_BYTES,
                                                pPppInterface,
                                                U_PORT_PPP_SOCKET_TASK_PRIORITY,
                                                &(pPppInterface->socketTaskHandle));
                    if (errorCode == 0) {
                        uPortLog("U_PORT_PPP: listening for pppd on socket %s.\n", pAddressString);
                    } else {
                        uPortMutexDelete(pPppInterface->socketTaskMutex);
                        close(pPppInterface->listeningSocket);
                    }
                } else {
                    close(pPppInterface->listeningSocket);
                }
            } else {
                uPortLog("U_PORT_PPP: *** WARNING *** bind() to \"%s\" returned errno %d.\n",
                         pAddressString, errno);
            }
        }
    }

    return errorCode;
}

// Stop the listening task.
static void stopSocketTask(uPortPppInterface_t *pPppInterface)
{
    // Set the flag to make the socket task exit
    pPppInterface->socketTaskExit = true;
    // Wait for the task to exit
    U_PORT_MUTEX_LOCK(pPppInterface->socketTaskMutex);
    U_PORT_MUTEX_UNLOCK(pPppInterface->socketTaskMutex);
    // Free the mutex
    uPortMutexDelete(pPppInterface->socketTaskMutex);
    pPppInterface->socketTaskMutex = NULL;
}

// Disconnect a PPP interface.
static void pppDisconnect(uPortPppInterface_t *pPppInterface)
{
    bool wasRunning = false;

    if (pPppInterface != NULL) {
        if (pPppInterface->pppRunning) {
            wasRunning = true;
            // We don't have control over pppd, can't tell it to
            // disconnect the PPP link, which is kinda vital,
            // so instead we take control of the link and terminate
            // both sides ourselves
            terminateLink(pPppInterface);
        }
        if (pPppInterface->pDisconnectCallback != NULL) {
            pPppInterface->pDisconnectCallback(pPppInterface->pDevHandle,
                                               pPppInterface->pppRunning);
        }
        pPppInterface->pppRunning = false;
        if (wasRunning) {
            uPortLog("U_PORT_PPP: socket disconnected from module (but pppd still connected to socket).\n");
        }
    }
}

#endif // #ifdef U_CFG_PPP_ENABLE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO THIS PORT LAYER
 * -------------------------------------------------------------- */

#ifdef U_CFG_PPP_ENABLE

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
    uLinkedList_t *pListNext;
    uPortPppInterface_t *pPppInterface;
    uPortPppLocalDevice_t *pLocalDevice;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Remove any local device names
        while (gpPppLocalDeviceNameList != NULL) {
            pLocalDevice = (uPortPppLocalDevice_t *) (gpPppLocalDeviceNameList->p);
            uPortFree(pLocalDevice);
            uLinkedListRemove(&gpPppLocalDeviceNameList, pLocalDevice);
        }

        // Now remove all PPP interfaces
        while (gpPppInterfaceList != NULL) {
            pPppInterface = (uPortPppInterface_t *) gpPppInterfaceList->p;
            pListNext = gpPppInterfaceList->pNext;
            uLinkedListRemove(&gpPppInterfaceList, pPppInterface);
            // Make sure we don't accidentally try to call the
            // transmit or down callbacks since the device handle
            // will have been destroyed by now
            pPppInterface->pTransmitCallback = NULL;
            pPppInterface->pDisconnectCallback = NULL;
            pppDisconnect(pPppInterface);
            stopSocketTask(pPppInterface);
            uPortFree(pPppInterface);
            gpPppInterfaceList = pListNext;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

#else

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
}

#endif // #ifdef U_CFG_PPP_ENABLE

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_PPP_ENABLE

// Set the name of the device that is the MCU-end PPP entity.
int32_t uPortPppSetLocalDeviceName(const char *pDevice)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uPortPppLocalDevice_t *pLocalDevice;
    pthread_t threadId;
    uPortPppLocalDevice_t *pTmp;

    if ((pDevice != NULL) && (strlen(pDevice) < sizeof(pLocalDevice->name))) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pLocalDevice = pUPortMalloc(sizeof(uPortPppLocalDevice_t));
        if (pLocalDevice != NULL) {

            U_PORT_MUTEX_LOCK(gMutex);

            // Remove any existing local device name for this
            // thread ID
            threadId = pthread_self();
            while ((pTmp = pFindLocalDeviceName(threadId)) != NULL) {
                uPortFree(pTmp);
                uLinkedListRemove(&gpPppLocalDeviceNameList, pTmp);
            }
            // Add the new one
            strncpy(pLocalDevice->name, pDevice, sizeof(pLocalDevice->name));
            pLocalDevice->threadId = threadId;
            if (uLinkedListAdd(&gpPppLocalDeviceNameList, (void *) pLocalDevice)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                uPortFree(pLocalDevice);
            }

            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }

    return errorCode;
}

// Attach a PPP interface to pppd.
int32_t uPortPppAttach(void *pDevHandle,
                       uPortPppConnectCallback_t *pConnectCallback,
                       uPortPppDisconnectCallback_t *pDisconnectCallback,
                       uPortPppTransmitCallback_t *pTransmitCallback)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;
    uPortPppLocalDevice_t *pLocalDevice;
    const char *pName = U_PORT_PPP_LOCAL_DEVICE_NAME;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pPppInterface = (uPortPppInterface_t *) pUPortMalloc(sizeof(*pPppInterface));
            if (pPppInterface != NULL) {
                memset(pPppInterface, 0, sizeof(*pPppInterface));
                // Get the pppd-end device name and start a task
                // which will open a socket listening on it and
                // receive data sent by it
                pLocalDevice = pFindLocalDeviceName(pthread_self());
                if (pLocalDevice != NULL) {
                    pName = pLocalDevice->name;
                }
                errorCode = startSocketTask(pPppInterface, pName);
                if (errorCode == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pPppInterface->pDevHandle = pDevHandle;
                    pPppInterface->pConnectCallback = pConnectCallback;
                    pPppInterface->pDisconnectCallback = pDisconnectCallback;
                    pPppInterface->pTransmitCallback = pTransmitCallback;
                    if (uLinkedListAdd(&gpPppInterfaceList, pPppInterface)) {
                        // Everything else is done in uPortPppConnect()
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        stopSocketTask(pPppInterface);
                        uPortFree(pPppInterface);
                    }
                } else {
                    uPortFree(pPppInterface);
                }
            }
        }

        if (errorCode < 0) {
            uPortLog("U_PORT_PPP: *** WARNING *** unable to attach PPP (%d).\n", errorCode);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Connect a PPP interface.
int32_t uPortPppConnect(void *pDevHandle,
                        uSockIpAddress_t *pIpAddress,
                        uSockIpAddress_t *pDnsIpAddressPrimary,
                        uSockIpAddress_t *pDnsIpAddressSecondary,
                        const char *pUsername,
                        const char *pPassword,
                        uPortPppAuthenticationMode_t authenticationMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;
    int32_t startTimeMs;

    // There is no way for this code to provide the authentication
    // parameters back to pppd, the user has to set them when
    // pppd is started
    (void) pUsername;
    (void) pPassword;
    (void) authenticationMode;

    // PPP negotiation will set these
    (void) pIpAddress;
    (void) pDnsIpAddressPrimary;
    (void) pDnsIpAddressSecondary;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            // In case we were previously connected and
            // then disconnected
            pPppInterface->dataTransferSuspended = false;
            pPppInterface->ipConnected = false;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pPppInterface->pConnectCallback != NULL) {
                errorCode = pPppInterface->pConnectCallback(pDevHandle,
                                                            moduleDataCallback,
                                                            pPppInterface, NULL,
                                                            U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                            NULL);
            }
            if (errorCode == 0) {
                pPppInterface->pppRunning = true;
                // Use a nice specific error message here, most likely to point
                // people at a PPP kinda problem
                errorCode = (int32_t) U_ERROR_COMMON_PROTOCOL_ERROR;
                // Wait for the IP connection to succeed
                startTimeMs = uPortGetTickTimeMs();
                while ((!pPppInterface->ipConnected) &&
                       (uPortGetTickTimeMs() - startTimeMs < U_PORT_PPP_CONNECT_TIMEOUT_SECONDS * 1000)) {
                    uPortTaskBlock(250);
                }
                if (pPppInterface->ipConnected) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    uPortLog("U_PORT_PPP: socket connected to module.\n");
                }
                if ((errorCode != 0) &&
                    (pPppInterface->pDisconnectCallback != NULL)) {
                    // Clean up on error
                    pPppInterface->pDisconnectCallback(pPppInterface->pDevHandle, false);
                    pPppInterface->pppRunning = false;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Reconnect a PPP interface.
int32_t uPortPppReconnect(void *pDevHandle,
                          uSockIpAddress_t *pIpAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;

    (void) pIpAddress;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        pPppInterface = pFindPppInterface(pDevHandle);
        if ((pPppInterface != NULL) && (pPppInterface->pppRunning)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pPppInterface->pConnectCallback != NULL) {
                errorCode = pPppInterface->pConnectCallback(pDevHandle,
                                                            moduleDataCallback,
                                                            pPppInterface, NULL,
                                                            U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                            NULL);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Disconnect a PPP interface.
int32_t uPortPppDisconnect(void *pDevHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            pppDisconnect(pPppInterface);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Detach a PPP interface from pppd.
int32_t uPortPppDetach(void *pDevHandle)
{
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            uLinkedListRemove(&gpPppInterfaceList, pPppInterface);
            pppDisconnect(pPppInterface);
            stopSocketTask(pPppInterface);
            uPortFree(pPppInterface);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

#endif // #ifdef U_CFG_PPP_ENABLE

// End of file
