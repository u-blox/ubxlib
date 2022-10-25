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
 * @brief Implementation of the sockets API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), memcmp(), strlen()
#include "limits.h"    // UINT16_MAX

#include "u_cfg_sw.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_hex_bin_convert.h"

#include "u_sock_errno.h"
#include "u_sock.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell_net.h"
#include "u_cell_private.h"
#include "u_cell_sock.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Cross check address sizes.
 */
#if U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES < U_CELL_NET_IP_ADDRESS_SIZE
# error U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES must be at least as big as U_CELL_NET_IP_ADDRESS_SIZE
#endif

/** The value to use for socket-level options when talking to the
 * module (-1 as an int16_t).
 */
#define U_CELL_SOCK_OPT_LEVEL_SOCK_INT16 65535

#ifndef U_CELL_SOCK_DNS_SHOULD_RETRY_MS
/** I have seen DNS queries return ERROR very quickly, likely
 * because the module is busy doing something and can't service
 * the request.  This is the time window within that might
 * happen: if it returns at least this quickly with an error
 * then it is worth trying again.
 */
# define U_CELL_SOCK_DNS_SHOULD_RETRY_MS 2000
#endif

#ifndef U_CELL_SOCK_SECURE_DELAY_MILLISECONDS
/** I have seen secure socket operations fail if the
 * secured socket is used too quickly after security
 * has been applied, so wait this long before returning
 * after a security profile has been applied.
 */
#define U_CELL_SOCK_SECURE_DELAY_MILLISECONDS 250
#endif

#ifndef U_CELL_SOCK_SARA_R422_DNS_DELAY_MILLISECONDS
/** The gap to leave between being connected to the network
 * and performing a DNS look-up for a SARA-R422 module. If
 * you do a DNS look-up immediately after connecting then
 * SARA-R422 gets a bit upset.
 */
#define U_CELL_SOCK_SARA_R422_DNS_DELAY_MILLISECONDS 500
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A cellular socket.
 */
typedef struct {
    int32_t sockHandle; /**< The handle of the socket instance.
                             -1 if this socket is not in use. */
    uDeviceHandle_t cellHandle; /**< The handle of the cellular instance.
                             -1 if this socket is not in use. */
    uAtClientHandle_t atHandle; /**< The AT client handle for this instance.
                                     NULL if this socket is not in use. */
    int32_t sockHandleModule; /**< The handle that the cellular module
                                   uses for the socket instance.
                                   -1 if this socket is not in use. */
    volatile int32_t pendingBytes;
    void (*pAsyncClosedCallback) (uDeviceHandle_t, int32_t); /**< Set to NULL
                                                          if socket is
                                                          not in use. */
    void (*pDataCallback) (uDeviceHandle_t, int32_t); /**< Set to NULL if
                                                   socket is not
                                                   in use. */
    void (*pClosedCallback) (uDeviceHandle_t, int32_t); /**< Set to NULL
                                                     if socket is
                                                     not in use. */
} uCellSockSocket_t;

/** Definition of a URC handler.
 */
typedef struct {
    const char *pPrefix;
    void (*pHandler) (uAtClientHandle_t, void *);
} uCellSockUrcHandler_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Keep track of whether we're initialised or not.
static bool gInitialised = false;

/** The next socket handle to use.
 */
static int32_t gNextSockHandle = 0;

/** The sockets: a nice simple array, nothing fancy.
 */
static uCellSockSocket_t gSockets[U_CELL_SOCK_MAX_NUM_SOCKETS];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: LIST MANAGEMENT
 * -------------------------------------------------------------- */

// Find the entry for the given socket handle.
static uCellSockSocket_t *pFindBySockHandle(int32_t sockHandle)
{
    uCellSockSocket_t *pSock = NULL;

    for (size_t x = 0; (x < sizeof(gSockets) / sizeof(gSockets[0])) &&
         (pSock == NULL); x++) {
        if (gSockets[x].sockHandle == sockHandle) {
            pSock = &(gSockets[x]);
        }
    }

    return pSock;
}

// Find the entry for the given module socket handle.
//lint -e{818} suppress "could be declared as pointing to const": it is!
static uCellSockSocket_t *pFindBySockHandleModule(const uAtClientHandle_t atHandle,
                                                  int32_t sockHandleModule)
{
    uCellSockSocket_t *pSock = NULL;

    for (size_t x = 0; (x < sizeof(gSockets) / sizeof(gSockets[0])) &&
         (pSock == NULL); x++) {
        if ((gSockets[x].sockHandle >= 0) &&
            (gSockets[x].atHandle == atHandle) &&
            (gSockets[x].sockHandleModule == sockHandleModule)) {
            pSock = &(gSockets[x]);
        }
    }

    return pSock;
}

// Do AT+USOER, for debug purposes.
static void doUsoer(uAtClientHandle_t atHandle)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+USOER");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+USOER:");
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
}

// Create a socket entry in the list.
static uCellSockSocket_t *pSockCreate(int32_t sockHandle,
                                      uDeviceHandle_t cellHandle,
                                      uAtClientHandle_t atHandle)
{
    uCellSockSocket_t *pSock = NULL;

    // Find an empty entry in the list
    for (size_t x = 0; (x < sizeof(gSockets) / sizeof(gSockets[0])) &&
         (pSock == NULL); x++) {
        if (gSockets[x].sockHandle < 0) {
            pSock = &(gSockets[x]);
        }
    }

    // Set it up
    if (pSock != NULL) {
        pSock->sockHandle = sockHandle;
        pSock->cellHandle = cellHandle;
        pSock->atHandle = atHandle;
        pSock->sockHandleModule = -1;
        pSock->pendingBytes = 0;
        pSock->pAsyncClosedCallback = NULL;
        pSock->pDataCallback = NULL;
        pSock->pClosedCallback = NULL;
    }

    return pSock;
}

// Free an entry in the list.
static void sockFree(int32_t sockHandle)
{
    uCellSockSocket_t *pSock = NULL;

    for (size_t x = 0; (x < sizeof(gSockets) / sizeof(gSockets[0])) &&
         (pSock == NULL); x++) {
        if (gSockets[x].sockHandle == sockHandle) {
            pSock = &(gSockets[x]);
            pSock->sockHandle = -1;
            pSock->cellHandle = NULL;
            pSock->atHandle = NULL;
            pSock->sockHandleModule = -1;
            pSock->pendingBytes = 0;
            pSock->pAsyncClosedCallback = NULL;
            pSock->pDataCallback = NULL;
            pSock->pClosedCallback = NULL;
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: URC AND RELATED FUNCTIONS
 * -------------------------------------------------------------- */

// Callback trampoline for pending data.
static void dataCallback(const uAtClientHandle_t atHandle,
                         void *pParameter)
{
    //lint -e(507) Suppress size incompatibility: the compiler
    // we use for Lint checking is 64 bit so has 8 byte pointers
    // and Lint doesn't like them being used to carry 4 byte integers
    int32_t sockHandle = (int32_t) pParameter;
    uCellSockSocket_t *pSocket;

    (void) atHandle;

    if (sockHandle >= 0) {
        // Find the entry
        pSocket = pFindBySockHandle(sockHandle);
        if ((pSocket != NULL) && (pSocket->pDataCallback != NULL)) {
            pSocket->pDataCallback(pSocket->cellHandle,
                                   sockHandle);
        }
    }
}

// Callback trampoline for connection closed.
static void closedCallback(const uAtClientHandle_t atHandle,
                           void *pParameter)
{
    //lint -e(507) Suppress size incompatibility: the compiler
    // we use for Lint checking is 64 bit so has 8 byte pointers
    // and Lint doesn't like them being used to carry 4 byte integers
    int32_t sockHandle = (int32_t) pParameter;
    uCellSockSocket_t *pSocket;

    (void) atHandle;

    if (sockHandle >= 0) {
        // Find the entry
        pSocket = pFindBySockHandle(sockHandle);
        if (pSocket != NULL) {
            if (pSocket->pClosedCallback != NULL) {
                pSocket->pClosedCallback(pSocket->cellHandle,
                                         sockHandle);
                // Socket is now closed, can lose the callback
                pSocket->pClosedCallback = NULL;
            }
            if (pSocket->pAsyncClosedCallback != NULL) {
                pSocket->pAsyncClosedCallback(pSocket->cellHandle,
                                              sockHandle);
                // Socket is now closed, lose the
                // async closure callback
                pSocket->pAsyncClosedCallback = NULL;
            }

            // Free the entry
            sockFree(pSocket->sockHandle);
        }
    }
}

// Socket Read/Read-From URC.
static void UUSORD_UUSORF_urc(const uAtClientHandle_t atHandle,
                              void *pUnused)
{
    int32_t sockHandleModule;
    int32_t dataSizeBytes;
    uCellSockSocket_t *pSocket = NULL;

    (void) pUnused;

    // +UUSORx: <socket>,<length>
    sockHandleModule = uAtClientReadInt(atHandle);
    dataSizeBytes = uAtClientReadInt(atHandle);

    if (sockHandleModule >= 0) {
        // Find the entry
        pSocket = pFindBySockHandleModule(atHandle,
                                          sockHandleModule);
        if (pSocket != NULL) {
            // Call the user call-back via the trampoline
            if ((dataSizeBytes > 0) &&
                (pSocket->pDataCallback != NULL)) {
                uAtClientCallback(atHandle,
                                  dataCallback,
                                  (void *) (pSocket->sockHandle));
            }
            pSocket->pendingBytes = dataSizeBytes;
        }
    }
}

// Callback for Socket Close URC.
static void UUSOCL_urc(const uAtClientHandle_t atHandle,
                       void *pUnused)
{
    int32_t sockHandleModule;
    uCellSockSocket_t *pSocket = NULL;

    (void) pUnused;

    // +UUSOCL: <socket>
    sockHandleModule = uAtClientReadInt(atHandle);
    if (sockHandleModule >= 0) {
        // Find the entry
        pSocket = pFindBySockHandleModule(atHandle,
                                          sockHandleModule);
        if (pSocket != NULL) {
            if (pSocket->pClosedCallback != NULL) {
                uAtClientCallback(atHandle,
                                  closedCallback,
                                  (void *) (pSocket->sockHandle));
            }
        }
    }
}

/* ----------------------------------------------------------------
 * MORE VARIABLES
 * -------------------------------------------------------------- */

/** A table of the URC handlers to make set-up easier.
 */
static const uCellSockUrcHandler_t gUrcHandlers[] = {
    {"+UUSORD:", UUSORD_UUSORF_urc},
    {"+UUSORF:", UUSORD_UUSORF_urc},
    {"+UUSOCL:", UUSOCL_urc}
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SOCKET OPTIONS
 * -------------------------------------------------------------- */

// Set a socket option that has an integer as a parameter
// returning a (non-negated) value of U_SOCK_Exxx.
static int32_t setOptionInt(const uCellSockSocket_t *pSocket,
                            int32_t level,
                            uint32_t option,
                            const void *pOptionValue,
                            size_t optionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uAtClientHandle_t atHandle = pSocket->atHandle;

    if ((pOptionValue != NULL) &&
        (optionValueLength >= sizeof(int32_t))) {
        if (level == U_SOCK_OPT_LEVEL_SOCK) {
            level = U_CELL_SOCK_OPT_LEVEL_SOCK_INT16;
        }
        // Pass the option transparently through
        // to the module which can decide whether
        // it likes it or not
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+USOSO=");
        uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
        uAtClientWriteInt(atHandle, level);
        uAtClientWriteInt(atHandle, (int32_t) option);
        uAtClientWriteInt(atHandle, *((const int32_t *) pOptionValue));
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            // All good
            errnoLocal = U_SOCK_ENONE;
        } else {
            // Got an AT interace error, see
            // what the module's socket error
            // number has to say for debug purposes
            doUsoer(atHandle);
        }
    }

    return errnoLocal;
}

// Get a socket option that has an integer as a
// parameter returning a (non-negated) value of
// U_SOCK_Exxx.
static int32_t getOptionInt(const uCellSockSocket_t *pSocket,
                            int32_t level,
                            uint32_t option,
                            void *pOptionValue,
                            size_t *pOptionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uAtClientHandle_t atHandle = pSocket->atHandle;
    int32_t x;

    if (pOptionValueLength != NULL) {
        if (pOptionValue != NULL) {
            if (*pOptionValueLength >= sizeof(int32_t)) {
                // Get the answer
                if (level == U_SOCK_OPT_LEVEL_SOCK) {
                    level = U_CELL_SOCK_OPT_LEVEL_SOCK_INT16;
                }
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USOGO=");
                uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                uAtClientWriteInt(atHandle, level);
                uAtClientWriteInt(atHandle, (int32_t) option);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USOGO:");
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (x >= 0)) {
                    // All good
                    errnoLocal = U_SOCK_ENONE;
                    *((int32_t *) pOptionValue)  = x;
                    *pOptionValueLength = sizeof(int32_t);
                } else {
                    // Got an AT interace error, see
                    // what the module's socket error
                    // number has to say for debug purposes
                    doUsoer(atHandle);
                }
            }
        } else {
            errnoLocal = U_SOCK_ENONE;
            // Caller just wants to know the length required
            *pOptionValueLength = sizeof(int32_t);
        }
    }

    return errnoLocal;
}

// Set the linger socket option, returning a
// (non-negated) value of U_SOCK_Exxx.
static int32_t setOptionLinger(const uCellSockSocket_t *pSocket,
                               const void *pOptionValue,
                               size_t optionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uAtClientHandle_t atHandle = pSocket->atHandle;
    int32_t x;

    if ((pOptionValue != NULL) &&
        (optionValueLength >= sizeof(int32_t))) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+USOSO=");
        uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
        uAtClientWriteInt(atHandle, U_CELL_SOCK_OPT_LEVEL_SOCK_INT16);
        uAtClientWriteInt(atHandle, U_SOCK_OPT_LINGER);
        x = ((const uSockLinger_t *) pOptionValue)->onNotOff;
        uAtClientWriteInt(atHandle, x);
        if (x == 1) {
            uAtClientWriteInt(atHandle,
                              ((const uSockLinger_t *) pOptionValue)->lingerSeconds);
        }
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            // All good
            errnoLocal = U_SOCK_ENONE;
        } else {
            // Got an AT interace error, see
            // what the module's socket error
            // number has to say for debug purposes
            doUsoer(atHandle);
        }
    }

    return errnoLocal;
}

// Get the linger socket option, returning a (non-negated)
// value of U_SOCK_Exxx.
static int32_t getOptionLinger(const uCellSockSocket_t *pSocket,
                               void *pOptionValue,
                               size_t *pOptionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uAtClientHandle_t atHandle = pSocket->atHandle;
    int32_t x;
    int32_t y = -1;

    if (pOptionValueLength != NULL) {
        if (pOptionValue != NULL) {
            if (*pOptionValueLength >= sizeof(int32_t)) {
                // Get the answer
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USOGO=");
                uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                uAtClientWriteInt(atHandle, U_CELL_SOCK_OPT_LEVEL_SOCK_INT16);
                uAtClientWriteInt(atHandle, U_SOCK_OPT_LINGER);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USOGO:");
                x = uAtClientReadInt(atHandle);
                // Second parameter is only relevant if
                // the first is 1
                if (x == 1) {
                    y = uAtClientReadInt(atHandle);
                }
                uAtClientResponseStop(atHandle);
                if (uAtClientUnlock(atHandle) == 0) {
                    errnoLocal = U_SOCK_EIO;
                    if (x == 0) {
                        // All good
                        errnoLocal = U_SOCK_ENONE;
                        ((uSockLinger_t *) pOptionValue)->onNotOff = x;
                        *pOptionValueLength = sizeof(uSockLinger_t);
                    } else if ((x == 1) && (y >= 0)) {
                        // If x is 1, y must be present
                        errnoLocal = U_SOCK_ENONE;
                        ((uSockLinger_t *) pOptionValue)->onNotOff = x;
                        ((uSockLinger_t *) pOptionValue)->lingerSeconds = y;
                        *pOptionValueLength = sizeof(uSockLinger_t);
                    }
                } else {
                    // Got an AT interace error, see
                    // what the module's socket error
                    // number has to say for debug purposes
                    doUsoer(atHandle);
                }
            }
        } else {
            errnoLocal = U_SOCK_ENONE;
            // Caller just wants to know the length required
            *pOptionValueLength = sizeof(uSockLinger_t);
        }
    }

    return errnoLocal;
}

// Set hex mode on the underlying AT interface on or off.
int32_t setHexMode(uDeviceHandle_t cellHandle, bool hexModeOnNotOff)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t state = 0;

    if (hexModeOnNotOff) {
        state = 1;
    }

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        // Set hex mode
        errnoLocal = U_SOCK_EIO;
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDCONF=");
        uAtClientWriteInt(atHandle, 1);
        uAtClientWriteInt(atHandle, state);
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            pInstance->socketsHexMode = (state == 1);
            errnoLocal = U_SOCK_ENONE;
        }
    }

    return -errnoLocal;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Do AT+USOCTL for an operation with an integer return value.
static int32_t doUsoctl(uDeviceHandle_t cellHandle, int32_t sockHandle,
                        int32_t operation)
{
    int32_t negErrnoLocallOrValue = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uCellSockSocket_t *pSocket;
    uAtClientHandle_t atHandle;
    int32_t x;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                negErrnoLocallOrValue = -U_SOCK_EIO;
                // Do USOCTL 1 to get the last error code
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USOCTL=");
                uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                uAtClientWriteInt(atHandle, operation);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USOCTL:");
                // Skip the first two integers, which
                // are just the socket ID and our operation number
                // coming back
                uAtClientSkipParameters(atHandle, 2);
                // Now read the integer we actually want
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (x >= 0)) {
                    negErrnoLocallOrValue = x;
                }
            }
        }
    }

    return negErrnoLocallOrValue;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INIT/DEINIT
 * -------------------------------------------------------------- */

// Initialise the cellular sockets layer.
int32_t uCellSockInit()
{
    uCellSockSocket_t *pSock = NULL;

    if (!gInitialised) {

        // Clear the list
        for (size_t x = 0; (x < sizeof(gSockets) / sizeof(gSockets[0])); x++) {
            pSock = &(gSockets[x]);
            pSock->cellHandle = NULL;
            pSock->sockHandle = -1;
            pSock->sockHandleModule = -1;
            pSock->pendingBytes = 0;
            pSock->pDataCallback = NULL;
            pSock->pClosedCallback = NULL;
        }

        gInitialised = true;
    }

    return U_SOCK_ENONE;
}

// Initialise the cellular sockets instance.
int32_t uCellSockInitInstance(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    int32_t errnoLocal = U_SOCK_EINVAL;

    if (gInitialised) {
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errnoLocal = U_SOCK_ENONE;
            // Set up the URCs
            for (size_t x = 0; (x < sizeof(gUrcHandlers) /
                                sizeof(gUrcHandlers[0])) &&
                 (errnoLocal == U_SOCK_ENONE); x++) {
                if (uAtClientSetUrcHandler(pInstance->atHandle,
                                           gUrcHandlers[x].pPrefix,
                                           gUrcHandlers[x].pHandler,
                                           NULL) != 0) {
                    errnoLocal = U_SOCK_ENOMEM;
                }
            }
        }
    }

    return -errnoLocal;
}

// Deinitialise the cellular sockets layer.
void uCellSockDeinit()
{
    if (gInitialised) {
        // Nothing to do, URCs will have been
        // removed on close
        gInitialised = false;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CREATE/OPEN/CLOSE/CLEAN-UP
 * -------------------------------------------------------------- */

// Create a socket.
int32_t uCellSockCreate(uDeviceHandle_t cellHandle,
                        uSockType_t type,
                        uSockProtocol_t protocol)
{
    int32_t negErrnoLocal = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;

    (void) type;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        negErrnoLocal = -U_SOCK_ENOBUFS;
        atHandle = pInstance->atHandle;
        // Create the entry
        pSocket = pSockCreate(gNextSockHandle, cellHandle, atHandle);
        gNextSockHandle++;
        if (gNextSockHandle < 0) {
            gNextSockHandle = 0;
        }
        if (pSocket != NULL) {
            // Create the socket in the cellular module
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+USOCR=");
            // Protocol is 6 for TCP or 17 for UDP
            uAtClientWriteInt(atHandle, (int32_t) protocol);
            // User-specified local port number
            if (pInstance->sockNextLocalPort >= 0) {
                uAtClientWriteInt(atHandle, pInstance->sockNextLocalPort);
                pInstance->sockNextLocalPort = -1;
            }
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+USOCR:");
            pSocket->sockHandleModule = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                // All good
                negErrnoLocal = pSocket->sockHandle;
            } else {
                // Free the socket again
                sockFree(pSocket->sockHandle);
                // See what the module's socket error
                // number has to say for debug purposes
                doUsoer(atHandle);
            }
        }
    }

    return negErrnoLocal;
}

// Connect to a server.
int32_t uCellSockConnect(uDeviceHandle_t cellHandle,
                         int32_t sockHandle,
                         const uSockAddress_t *pRemoteAddress)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uAtClientDeviceError_t deviceError;
    uCellSockSocket_t *pSocket;
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
    char *pRemoteIpAddress;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if ((pSocket != NULL) &&
                (uSockAddressToString(pRemoteAddress, buffer,
                                      sizeof(buffer)) > 0)) {
                pRemoteIpAddress = pUSockDomainRemovePort(buffer);
                errnoLocal = U_SOCK_EHOSTUNREACH;
                // Connect the socket through the cellular module
                // If have seen modules return ERROR to this
                // immediately so try a few times
                deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_ERROR;
                for (size_t x = 3; (x > 0) &&
                     (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
                     x--) {
                    uAtClientLock(atHandle);
                    // Leave a little longer to connect
                    uAtClientTimeoutSet(atHandle,
                                        U_CELL_SOCK_CONNECT_TIMEOUT_SECONDS * 1000);
                    uAtClientCommandStart(atHandle, "AT+USOCO=");
                    // Write module socket handle
                    uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                    // Write IP address
                    uAtClientWriteString(atHandle, pRemoteIpAddress, true);
                    // Write port number
                    uAtClientWriteInt(atHandle, pRemoteAddress->port);
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientDeviceErrorGet(atHandle, &deviceError);
                    if (uAtClientUnlock(atHandle) == 0) {
                        // All good
                        errnoLocal = U_SOCK_ENONE;
                    }
                    if (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) {
                        // Got an AT interace error, see
                        // what the module's socket error
                        // number has to say for debug purposes
                        doUsoer(atHandle);
                        uPortTaskBlock(1000);
                    }
                }
            }
        }
    }

    return -errnoLocal;
}

// Close a socket.
int32_t uCellSockClose(uDeviceHandle_t cellHandle,
                       int32_t sockHandle,
                       void (*pCallback) (uDeviceHandle_t,
                                          int32_t))
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;
    uAtClientDeviceError_t deviceError;
    int32_t atError = -1;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                errnoLocal = U_SOCK_EIO;
                // Close the socket through the cellular module
                // If have seen modules return ERROR to this
                // immediately so try a few times
                deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_ERROR;
                for (size_t x = 3; (x > 0) &&
                     (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
                     x--) {
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle,
                                        U_SOCK_CLOSE_TIMEOUT_SECONDS * 1000);
                    uAtClientCommandStart(atHandle, "AT+USOCL=");
                    // Write module socket handle
                    uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                    if (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                                            U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)) {
                        // Asynchronous closure not supported
                        pCallback = NULL;
                    }
                    if (pCallback != NULL) {
                        // If a callback was given and the module
                        // supports asynchronous socket closure then
                        // request it
                        uAtClientWriteInt(atHandle, 1);
                    }
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientDeviceErrorGet(atHandle, &deviceError);
                    atError = uAtClientUnlock(atHandle);
                    if (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) {
                        uPortTaskBlock(1000);
                    }
                }

                if (atError == 0) {
                    // All good
                    errnoLocal = U_SOCK_ENONE;
                    pSocket->pAsyncClosedCallback = pCallback;
                    if (pCallback == NULL) {
                        // If no callback was given, or one
                        // was given and the the module
                        // doesn't support asynchronous closure,
                        // call the trampoline from here
                        uAtClientCallback(atHandle, closedCallback,
                                          (void *) sockHandle);
                    }
                } else {
                    // Got an AT interace error, see
                    // what the module's socket error
                    // number has to say for debug purposes
                    doUsoer(atHandle);
                }
            }
        }
    }

    return -errnoLocal;
}

// Clean-up.
void uCellSockCleanup(uDeviceHandle_t cellHandle)
{
    // Nothing to do
    (void) cellHandle;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURE
 * -------------------------------------------------------------- */

// Set a socket to be blocking or non-blocking.
void uCellSockBlockingSet(uDeviceHandle_t cellHandle,
                          int32_t sockHandle,
                          bool isBlocking)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) isBlocking;
    // Nothing to do: always non-blocking
}

// Get whether a socket is blocking or not.
bool uCellSockBlockingGet(uDeviceHandle_t cellHandle,
                          int32_t sockHandle)
{
    (void) cellHandle;
    (void) sockHandle;
    // Always non-blocking.
    return false;
}

// Set socket option.
int32_t uCellSockOptionSet(uDeviceHandle_t cellHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           const void *pOptionValue,
                           size_t optionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uCellSockSocket_t *pSocket;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                if ((optionValueLength == 0) ||
                    ((optionValueLength > 0) && (pOptionValue != NULL))) {
                    switch (level) {
                        case U_SOCK_OPT_LEVEL_SOCK:
                            switch (option) {
                                // The supported options which
                                // have an integer as a parameter
                                case U_SOCK_OPT_REUSEADDR:
                                case U_SOCK_OPT_KEEPALIVE:
                                case U_SOCK_OPT_BROADCAST:
                                case U_SOCK_OPT_REUSEPORT:
                                    errnoLocal = setOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              optionValueLength);
                                    break;
                                // The linger option which has
                                // uSockLinger_t as its parameter
                                case U_SOCK_OPT_LINGER:
                                    errnoLocal = setOptionLinger(pSocket, pOptionValue,
                                                                 optionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case U_SOCK_OPT_LEVEL_IP:
                            switch (option) {
                                // The supported options, both of
                                // which have an integer as a
                                // parameter
                                case U_SOCK_OPT_IP_TOS:
                                case U_SOCK_OPT_IP_TTL:
                                    errnoLocal = setOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              optionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case U_SOCK_OPT_LEVEL_TCP:
                            switch (option) {
                                // The supported options, both of
                                // which have an integer as a
                                // parameter
                                case U_SOCK_OPT_TCP_NODELAY:
                                case U_SOCK_OPT_TCP_KEEPIDLE:
                                    errnoLocal = setOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              optionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }

    return -errnoLocal;
}

// Get socket option.
int32_t uCellSockOptionGet(uDeviceHandle_t cellHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           void *pOptionValue,
                           size_t *pOptionValueLength)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uCellSockSocket_t *pSocket;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                // If there's an optionValue then there must be a length
                if ((pOptionValue == NULL) ||
                    (pOptionValueLength != NULL)) {
                    switch (level) {
                        case U_SOCK_OPT_LEVEL_SOCK:
                            switch (option) {
                                // The supported options which
                                // have an integer as a parameter
                                case U_SOCK_OPT_REUSEADDR:
                                case U_SOCK_OPT_KEEPALIVE:
                                case U_SOCK_OPT_BROADCAST:
                                case U_SOCK_OPT_REUSEPORT:
                                    errnoLocal = getOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              pOptionValueLength);
                                    break;
                                // The linger option which has
                                // uSockLinger_t as its parameter
                                case U_SOCK_OPT_LINGER:
                                    errnoLocal = getOptionLinger(pSocket, pOptionValue,
                                                                 pOptionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case U_SOCK_OPT_LEVEL_IP:
                            switch (option) {
                                // The supported options, both of
                                // which have an integer as a
                                // parameter
                                case U_SOCK_OPT_IP_TOS:
                                case U_SOCK_OPT_IP_TTL:
                                    errnoLocal = getOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              pOptionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case U_SOCK_OPT_LEVEL_TCP:
                            switch (option) {
                                // The supported options, both of
                                // which have an integer as a
                                // parameter
                                case U_SOCK_OPT_TCP_NODELAY:
                                case U_SOCK_OPT_TCP_KEEPIDLE:
                                    errnoLocal = getOptionInt(pSocket, level,
                                                              option, pOptionValue,
                                                              pOptionValueLength);
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }

    return -errnoLocal;
}

// Apply a security profile to a socket.
int32_t uCellSockSecure(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        int32_t profileId)
{
    int32_t negErrnoLocal = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                // Apply the profile in the cellular module
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USOSEC=");
                // Write module socket handle
                uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                // Enable security
                uAtClientWriteInt(atHandle, 1);
                // Write the profile ID
                uAtClientWriteInt(atHandle, profileId);
                uAtClientCommandStopReadResponse(atHandle);
                if (uAtClientUnlock(atHandle) == 0) {
                    negErrnoLocal = U_SOCK_ENONE;
                    uPortTaskBlock(U_CELL_SOCK_SECURE_DELAY_MILLISECONDS);
                } else {
                    // Got an AT interace error, see
                    // what the module's socket error
                    // number has to say for debug purposes
                    doUsoer(atHandle);
                }
            }
        }
    }

    return negErrnoLocal;
}

// Switch on hex mode.
int32_t uCellSockHexModeOn(uDeviceHandle_t cellHandle)
{
    return setHexMode(cellHandle, true);
}

// Switch off hex mode.
int32_t uCellSockHexModeOff(uDeviceHandle_t cellHandle)
{
    return setHexMode(cellHandle, false);
}

// Determine whether hex mode is on or off.
bool uCellSockHexModeIsOn(uDeviceHandle_t cellHandle)
{
    bool hexModeIsOn = false;
    uCellPrivateInstance_t *pInstance;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        hexModeIsOn = pInstance->socketsHexMode;
    }

    return hexModeIsOn;
}

// Set a local port for the next uCellSockCreate().
int32_t uCellSockSetNextLocalPort(uDeviceHandle_t cellHandle,
                                  int32_t port)
{
    int32_t negErrnoLocal = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if ((pInstance != NULL) &&
        ((port == -1) || ((port >= 0) && (port <= UINT16_MAX)))) {
        negErrnoLocal =  -U_SOCK_ENOSYS;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_SOCK_SET_LOCAL_PORT)) {
            negErrnoLocal = U_SOCK_ENONE;
            pInstance->sockNextLocalPort = port;
        }
    }

    return negErrnoLocal;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: UDP ONLY
 * -------------------------------------------------------------- */

// Send a datagram.
int32_t uCellSockSendTo(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        const uSockAddress_t *pRemoteAddress,
                        const void *pData, size_t dataSizeBytes)
{
    int32_t negErrnoLocalOrSize = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
    char *pRemoteIpAddress;
    size_t dataLengthMax = U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES;
    int32_t sentSize = 0;
    size_t x;
    bool written = false;
    char *pHexBuffer = NULL;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        if (pInstance->socketsHexMode) {
            dataLengthMax /= 2;
        }
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                negErrnoLocalOrSize = -U_SOCK_EDESTADDRREQ;
                if (uSockAddressToString(pRemoteAddress, buffer,
                                         sizeof(buffer)) > 0) {
                    pRemoteIpAddress = pUSockDomainRemovePort(buffer);
                    if (pRemoteIpAddress != NULL) {
                        negErrnoLocalOrSize = -U_SOCK_EMSGSIZE;
                        if (dataSizeBytes <= dataLengthMax) {
                            if (pInstance->socketsHexMode) {
                                negErrnoLocalOrSize = -U_SOCK_ENOMEM;
                                pHexBuffer = (char *) pUPortMalloc(dataSizeBytes * 2 + 1);  // +1 for terminator
                                if (pHexBuffer != NULL) {
                                    // Make the hex-coded null terminated string
                                    x = uBinToHex((const char *) pData, dataSizeBytes, pHexBuffer);
                                    *(pHexBuffer + x) = 0;
                                }
                            }
                            if (!pInstance->socketsHexMode || (pHexBuffer != NULL)) {
                                negErrnoLocalOrSize = -U_SOCK_EIO;
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+USOST=");
                                // Write module socket handle
                                uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                                // Write IP address
                                uAtClientWriteString(atHandle, pRemoteIpAddress, true);
                                // Write port number
                                uAtClientWriteInt(atHandle, pRemoteAddress->port);
                                // Number of bytes to follow
                                uAtClientWriteInt(atHandle, (int32_t) dataSizeBytes);
                                if (pHexBuffer) {
                                    // Send the hex mode data as a string
                                    uAtClientWriteString(atHandle, pHexBuffer, true);
                                    uAtClientCommandStop(atHandle);
                                    // Free the buffer
                                    uPortFree(pHexBuffer);
                                    written = true;
                                } else {
                                    // Not in hex mode, wait for the prompt
                                    uAtClientCommandStop(atHandle);
                                    if (uAtClientWaitCharacter(atHandle, '@') == 0) {
                                        // Wait for it...
                                        uPortTaskBlock(50);
                                        // Send the binary data
                                        uAtClientWriteBytes(atHandle, (const char *) pData,
                                                            dataSizeBytes, true);
                                        written = true;
                                    }
                                }
                                if (written) {
                                    // Grab the response
                                    uAtClientResponseStart(atHandle, "+USOST:");
                                    // Skip the socket ID
                                    uAtClientSkipParameters(atHandle, 1);
                                    // Bytes sent
                                    sentSize = uAtClientReadInt(atHandle);
                                    uAtClientResponseStop(atHandle);
                                    if ((uAtClientUnlock(atHandle) == 0) &&
                                        (sentSize >= 0)) {
                                        // All is good, probably
                                        negErrnoLocalOrSize = sentSize;
                                    }
                                } else {
                                    uAtClientUnlock(atHandle);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return negErrnoLocalOrSize;
}

// Receive a datagram.
int32_t uCellSockReceiveFrom(uDeviceHandle_t cellHandle,
                             int32_t sockHandle,
                             uSockAddress_t *pRemoteAddress,
                             void *pData, size_t dataSizeBytes)
{
    int32_t negErrnoLocalOrSize = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;
    int32_t dataLengthMax = U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES;
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
    int32_t x;
    int32_t port = -1;
    int32_t receivedSize = -1;
    int32_t readLength;
    char *pHexBuffer = NULL;

    buffer[0] = 0;  // In case of slip-ups

    // Note: the real maximum length of UDP packet we can receive
    // comes from fitting all of the following into one buffer:
    //
    // +USORF: xx,"max.len.ip.address.ipv4.or.ipv6",yyyyy,wwww,"the_data"\r\n
    //
    // where xx is the handle, max.len.ip.address.ipv4.or.ipv6 is NSAPI_IP_SIZE,
    // yyyyy is the port number (max 65536), wwww is the length of the data and
    // the_data is binary data. I make that 29 + 48 + len(the_data),
    // so the overhead is 77 bytes.

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        if (pInstance->socketsHexMode) {
            dataLengthMax /= 2;
        }
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                negErrnoLocalOrSize = -U_SOCK_EWOULDBLOCK;
                if (pSocket->pendingBytes == 0) {
                    // If the URC has not filled in pendingBytes,
                    // ask the module directly if there is anything
                    // to read
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USORF=");
                    uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                    // Zero bytes to read, just want to know the number
                    // of bytes waiting
                    uAtClientWriteInt(atHandle, 0);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+USORF:");
                    // Skip the socket ID
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the amount of data
                    x = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    // Update pending bytes here, before
                    // unlocking, as otherwise a data callback
                    // triggered by a URC could be sitting waiting
                    // to grab the AT lock and jump in before
                    // pending bytes has been updated, leading it
                    // back into here again, etc, etc.
                    if (x > 0) {
                        pSocket->pendingBytes = x;
                        // DON'T call the user data callback here:
                        // we already have the AT interface locked
                        // and a user might try to call back into
                        // here which would result in deadlock.
                        // They will get their received data, there
                        // is no need to worry.
                    }
                    uAtClientUnlock(atHandle);
                }
                if (pSocket->pendingBytes > 0) {
                    // In the UDP case we HAVE to read the number
                    // of bytes pending as this will be the size
                    // of the next UDP packet in the module and the
                    // module can only deliver whole UDP packets.
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USORF=");
                    uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                    // Number of bytes to read
                    uAtClientWriteInt(atHandle, dataLengthMax);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+USORF:");
                    // Skip the socket ID
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the IP address
                    uAtClientReadString(atHandle, buffer,
                                        sizeof(buffer), false);
                    // Read the port
                    port = uAtClientReadInt(atHandle);
                    // Read the amount of data
                    receivedSize = uAtClientReadInt(atHandle);
                    if (receivedSize > dataLengthMax) {
                        receivedSize = dataLengthMax;
                    }
                    if ((int32_t) dataSizeBytes > receivedSize) {
                        dataSizeBytes = receivedSize;
                    }
                    if (receivedSize > 0) {
                        if (pInstance->socketsHexMode) {
                            // In hex mode we need a buffer to dump
                            // the hex into and then we can decode it
                            negErrnoLocalOrSize = -U_SOCK_ENOMEM;
                            //lint -e{647} Suppress suspicious truncation
                            pHexBuffer = (char *) pUPortMalloc(receivedSize * 2 + 1);  // +1 for terminator
                        }
                        if (!pInstance->socketsHexMode || (pHexBuffer != NULL)) {
                            if (pHexBuffer != NULL) {
                                // In hex mode we can read in the whole string
                                //lint -e{647} Suppress suspicious truncation
                                readLength = uAtClientReadString(atHandle, pHexBuffer,
                                                                 receivedSize * 2 + 1, false);
                                if (readLength > 0) {
                                    x = (int32_t) dataSizeBytes * 2;
                                    if (readLength > x) {
                                        readLength = x;
                                    }
                                    uHexToBin(pHexBuffer, readLength, (char *) pData);
                                }
                                // Free memory
                                uPortFree(pHexBuffer);
                            } else {
                                // Binary mode, don't stop for anything!
                                uAtClientIgnoreStopTag(atHandle);
                                // Get the leading quote mark out of the way
                                uAtClientReadBytes(atHandle, NULL, 1, true);
                                // Now read out all the actual data,
                                // first the bit we want
                                uAtClientReadBytes(atHandle, (char *) pData,
                                                   dataSizeBytes, true);
                                if (receivedSize > (int32_t) dataSizeBytes) {
                                    //...and then the rest poured away to NULL
                                    uAtClientReadBytes(atHandle, NULL,
                                                       receivedSize -
                                                       dataSizeBytes, true);
                                }
                                // Make sure to wait for the stop tag before
                                // we finish
                                uAtClientRestoreStopTag(atHandle);
                            }
                        }
                    }
                    uAtClientResponseStop(atHandle);
                    // BEFORE unlocking, work out what's happened.
                    // This is to prevent a URC being processed that
                    // may indicate data left and over-write pendingBytes
                    // while we're also writing to it.
                    if ((uAtClientErrorGet(atHandle) == 0) &&
                        (receivedSize >= 0)) {
                        // Must use what +USORF returns here as it may be less
                        // or more than we asked for and also may be
                        // more than pendingBytes, depending on how
                        // the URCs landed
                        // This update of pendingBytes will be overwritten
                        // by the URC but we have to do something here
                        // 'cos we don't get a URC to tell us when pendingBytes
                        // has gone to zero.
                        if (receivedSize > pSocket->pendingBytes) {
                            pSocket->pendingBytes = 0;
                        } else {
                            pSocket->pendingBytes -= receivedSize;
                        }
                        negErrnoLocalOrSize = receivedSize;
                    }
                    uAtClientUnlock(atHandle);
                }
            }
        }
    }

    if ((negErrnoLocalOrSize >= 0) && (pRemoteAddress != NULL) && (port >= 0)) {
        if (uSockStringToAddress(buffer, pRemoteAddress) == 0) {
            pRemoteAddress->port = (uint16_t) port;
        } else {
            // If we can't decode the remote address this becomes
            // an error, can't go receiving things from servers
            // we know not who they are
            negErrnoLocalOrSize = -U_SOCK_EIO;
        }
    }

    return negErrnoLocalOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: STREAM (TCP)
 * -------------------------------------------------------------- */

// Send bytes over a connected socket.
int32_t uCellSockWrite(uDeviceHandle_t cellHandle,
                       int32_t sockHandle,
                       const void *pData, size_t dataSizeBytes)
{
    int32_t negErrnoLocalOrSize = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;
    int32_t leftToSendSize = (int32_t) dataSizeBytes;
    int32_t sentSize = 0;
    int32_t dataOffset = 0;
    int32_t thisSendSize = U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES;
    size_t x = 0;
    bool written = true;
    char *pHexBuffer = NULL;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        if (pInstance->socketsHexMode) {
            thisSendSize /= 2;
            negErrnoLocalOrSize = -U_SOCK_ENOMEM;
            pHexBuffer = (char *)pUPortMalloc(thisSendSize * 2 + 1); // +1 for terminator
        }
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                if (!pInstance->socketsHexMode || (pHexBuffer != NULL)) {
                    negErrnoLocalOrSize = U_SOCK_ENONE;
                    x = 0;
                    while ((leftToSendSize > 0) &&
                           (negErrnoLocalOrSize == U_SOCK_ENONE) &&
                           (x < U_CELL_SOCK_TCP_RETRY_LIMIT) &&
                           written) {
                        if (leftToSendSize < thisSendSize) {
                            thisSendSize = leftToSendSize;
                        }
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+USOWR=");
                        // Write module socket handle
                        uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                        // Number of bytes to follow
                        uAtClientWriteInt(atHandle, (int32_t) thisSendSize);
                        written = false;
                        if (pHexBuffer) {
                            // Make the hex-coded null terminated string
                            uBinToHex((const char *) pData + dataOffset,
                                      thisSendSize, pHexBuffer);
                            pHexBuffer[thisSendSize * 2] = 0;
                            // Send the hex mode data as a string
                            //lint -e(679) Suppress suspicious truncation
                            uAtClientWriteString(atHandle, pHexBuffer, true);
                            uAtClientCommandStop(atHandle);
                            written = true;
                        } else {
                            uAtClientCommandStop(atHandle);
                            // Wait for the prompt
                            if (uAtClientWaitCharacter(atHandle, '@') == 0) {
                                // Wait for it...
                                uPortTaskBlock(50);
                                // Go!
                                uAtClientWriteBytes(atHandle,
                                                    (const char *) pData + dataOffset,
                                                    thisSendSize, true);
                                written = true;
                            }
                        }
                        if (written) {
                            // Grab the response
                            uAtClientResponseStart(atHandle, "+USOWR:");
                            // Skip the socket ID
                            uAtClientSkipParameters(atHandle, 1);
                            // Bytes sent
                            sentSize = uAtClientReadInt(atHandle);
                            uAtClientResponseStop(atHandle);
                            if (uAtClientUnlock(atHandle) == 0) {
                                dataOffset += sentSize;
                                leftToSendSize -= sentSize;
                                // Technically, it should be OK to
                                // send fewer bytes than asked for,
                                // however if this happens a lot we'll
                                // get stuck, which isn't desirable,
                                // so use the loop counter to avoid that
                                if (sentSize < thisSendSize) {
                                    x++;
                                }
                            } else {
                                negErrnoLocalOrSize = -U_SOCK_EIO;
                                // Got an AT interface error, see
                                // what the module's socket error
                                // number has to say for debug purposes
                                doUsoer(atHandle);
                            }
                        } else {
                            negErrnoLocalOrSize = -U_SOCK_EIO;
                            uAtClientUnlock(atHandle);
                        }
                    }
                }
            }
        }
        // Free the buffer
        uPortFree(pHexBuffer);
    }

    if (negErrnoLocalOrSize == U_SOCK_ENONE) {
        // All is good
        negErrnoLocalOrSize = ((int32_t) dataSizeBytes) - leftToSendSize;
    }

    return negErrnoLocalOrSize;
}

// Receive bytes on a connected socket.
int32_t uCellSockRead(uDeviceHandle_t cellHandle,
                      int32_t sockHandle,
                      void *pData, size_t dataSizeBytes)
{
    int32_t negErrnoLocalOrSize = -U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSockSocket_t *pSocket;
    int32_t dataLengthMax = U_CELL_SOCK_MAX_SEGMENT_SIZE_BYTES;
    int32_t x = -1;
    int32_t thisWantedReceiveSize;
    int32_t thisActualReceiveSize;
    int32_t totalReceivedSize = 0;
    int32_t readLength;
    char *pHexBuffer = NULL;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        if (pInstance->socketsHexMode) {
            dataLengthMax /= 2;
        }
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                negErrnoLocalOrSize = -U_SOCK_EWOULDBLOCK;
                if (pSocket->pendingBytes == 0) {
                    // If the URC has not filled in pendingBytes,
                    // ask the module directly if there is anything
                    // to read
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USORD=");
                    uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                    // Zero bytes to read, just want to know the number
                    // of bytes waiting
                    uAtClientWriteInt(atHandle, 0);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+USORD:");
                    // Skip the socket ID
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the amount of data
                    x = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    // Update pending bytes here, before
                    // unlocking, as otherwise a data callback
                    // triggered by a URC could be sitting waiting
                    // to grab the AT lock and jump in before
                    // pending bytes has been updated, leading it
                    // back into here again, etc, etc.
                    if (x > 0) {
                        pSocket->pendingBytes = x;
                        // DON'T call the user data callback here:
                        // we already have the AT interface locked
                        // and a user might try to call back into
                        // here which would result in deadlock.
                        // They will get their received data, there
                        // is no need to worry.
                    }
                    uAtClientUnlock(atHandle);
                }
                if (pSocket->pendingBytes > 0) {
                    negErrnoLocalOrSize = U_SOCK_ENONE;
                    // Run around the loop until we run out of
                    // pending data or room in the buffer
                    while ((dataSizeBytes > 0) &&
                           (pSocket->pendingBytes > 0) &&
                           (negErrnoLocalOrSize == U_SOCK_ENONE)) {
                        thisWantedReceiveSize = dataLengthMax;
                        if (thisWantedReceiveSize > (int32_t) dataSizeBytes) {
                            thisWantedReceiveSize = (int32_t) dataSizeBytes;
                        }
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+USORD=");
                        uAtClientWriteInt(atHandle, pSocket->sockHandleModule);
                        // Number of bytes to read
                        uAtClientWriteInt(atHandle, thisWantedReceiveSize);
                        uAtClientCommandStop(atHandle);
                        uAtClientResponseStart(atHandle, "+USORD:");
                        // Skip the socket ID
                        uAtClientSkipParameters(atHandle, 1);
                        // Read the amount of data
                        thisActualReceiveSize = uAtClientReadInt(atHandle);
                        if (thisActualReceiveSize > (int32_t) dataSizeBytes) {
                            thisActualReceiveSize = (int32_t) dataSizeBytes;
                        }
                        if (thisActualReceiveSize > 0) {
                            if (pInstance->socketsHexMode) {
                                // In hex mode we need a buffer to dump
                                // the hex into and then we can decode it
                                negErrnoLocalOrSize = -U_SOCK_ENOMEM;
                                //lint -e{647} Suppress suspicious truncation
                                pHexBuffer = (char *) pUPortMalloc(thisActualReceiveSize * 2 + 1);  // +1 for terminator
                            }
                            if (!pInstance->socketsHexMode || (pHexBuffer != NULL)) {
                                negErrnoLocalOrSize = U_SOCK_ENONE;
                                if (pHexBuffer != NULL) {
                                    // In hex mode we can read in the whole string
                                    //lint -e{647} Suppress suspicious truncation
                                    readLength = uAtClientReadString(atHandle, pHexBuffer,
                                                                     thisActualReceiveSize * 2 + 1,
                                                                     false);
                                    if (readLength > 0) {
                                        x = ((int32_t) dataSizeBytes) * 2;
                                        if (readLength > x) {
                                            readLength = x;
                                        }
                                        uHexToBin(pHexBuffer, readLength,
                                                  (char *) pData + totalReceivedSize);
                                    }
                                    // Free memory
                                    uPortFree(pHexBuffer);
                                } else {
                                    // Binary mode, don't stop for anything!
                                    uAtClientIgnoreStopTag(atHandle);
                                    // Get the leading quote mark out of the way
                                    uAtClientReadBytes(atHandle, NULL, 1, true);
                                    // Now read out the available data
                                    uAtClientReadBytes(atHandle,
                                                       (char *) pData +
                                                       totalReceivedSize,
                                                       thisActualReceiveSize, true);
                                    // Make sure we wait for the stop tag before
                                    // going around again
                                    uAtClientRestoreStopTag(atHandle);
                                }
                            }
                        }
                        uAtClientResponseStop(atHandle);
                        // BEFORE unlocking, work out what's happened.
                        // This is to prevent a URC being processed that
                        // may indicate data left and over-write pendingBytes
                        // while we're also writing to it.
                        if ((uAtClientErrorGet(atHandle) == 0) &&
                            (thisActualReceiveSize >= 0)) {
                            // Must use what +USORD returns here as it may be less
                            // or more than we asked for and also may be
                            // more than pendingBytes, depending on how
                            // the URCs landed
                            // This update of pendingBytes will be overwritten
                            // by the URC but we have to do something here
                            // 'cos we don't get a URC to tell us when pendingBytes
                            // has gone to zero.
                            if (thisActualReceiveSize > pSocket->pendingBytes) {
                                pSocket->pendingBytes = 0;
                            } else {
                                pSocket->pendingBytes -= thisActualReceiveSize;
                            }
                            totalReceivedSize += thisActualReceiveSize;
                            dataSizeBytes -= thisActualReceiveSize;
                        } else {
                            negErrnoLocalOrSize = -U_SOCK_EIO;
                        }
                        uAtClientUnlock(atHandle);
                    }
                }
            }
        }
    }

    if (totalReceivedSize > 0) {
        negErrnoLocalOrSize = totalReceivedSize;
    }

    return negErrnoLocalOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: ASYNC
 * -------------------------------------------------------------- */

// Register a callback on data being received.
void uCellSockRegisterCallbackData(uDeviceHandle_t cellHandle,
                                   int32_t sockHandle,
                                   void (*pCallback) (uDeviceHandle_t,
                                                      int32_t))
{
    uCellPrivateInstance_t *pInstance;
    uCellSockSocket_t *pSocket;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                // Set the callback
                pSocket->pDataCallback = pCallback;
            }
        }
    }
}

// Register a callback on a socket being closed.
void uCellSockRegisterCallbackClosed(uDeviceHandle_t cellHandle,
                                     int32_t sockHandle,
                                     void (*pCallback) (uDeviceHandle_t,
                                                        int32_t))
{
    uCellPrivateInstance_t *pInstance;
    uCellSockSocket_t *pSocket;

    // Find the instance
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if (pInstance != NULL) {
        // Find the entry
        if (sockHandle >= 0) {
            pSocket = pFindBySockHandle(sockHandle);
            if (pSocket != NULL) {
                // Set the callback
                pSocket->pClosedCallback = pCallback;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TCP INCOMING (TCP SERVER) ONLY
 * -------------------------------------------------------------- */

// Bind a socket to a local address.
int32_t uCellSockBind(uDeviceHandle_t cellHandle,
                      int32_t sockHandle,
                      const uSockAddress_t *pLocalAddress)
{
    // The firewalls of cellular networks do not
    // generally allow incoming TCP connections
    // and hence this function is not implemented

    (void) cellHandle;
    (void) sockHandle;
    (void) pLocalAddress;

    return -U_SOCK_ENOSYS;
}

// Set listening mode.
int32_t uCellSockListen(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        size_t backlog)
{
    // The firewalls of cellular networks do not
    // generally allow incoming TCP connections
    // and hence this function is not implemented

    (void) cellHandle;
    (void) sockHandle;
    (void) backlog;

    return -U_SOCK_ENOSYS;
}

// Accept an incoming TCP connection.
int32_t uCellSockAccept(uDeviceHandle_t cellHandle,
                        int32_t sockHandle,
                        uSockAddress_t *pRemoteAddress)
{
    // The firewalls of cellular networks do not
    // generally allow incoming TCP connections
    // and hence this function is not implemented

    (void) cellHandle;
    (void) sockHandle;
    (void) pRemoteAddress;

    return -U_SOCK_ENOSYS;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: FINDING ADDRESSES
 * -------------------------------------------------------------- */

// Perform a DNS look-up.
int32_t uCellSockGetHostByName(uDeviceHandle_t cellHandle,
                               const char *pHostName,
                               uSockIpAddress_t *pHostIpAddress)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    int32_t atError = -1;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t bytesRead = 0;
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];
    uSockAddress_t address;
    int32_t startTimeMs;

    memset(&address, 0, sizeof(address));
    buffer[0] = 0;
    pInstance = pUCellPrivateGetInstance(cellHandle);
    if ((pInstance != NULL) && (pHostName != NULL)) {
        uPortLog("U_CELL_SOCK: looking up IP address of \"%s\".\n",
                 pHostName);
        errnoLocal = U_SOCK_ENXIO;
        // I have seen modules return ERROR very
        // quickly here when they are likely busy
        // doing something else and can't service
        // the request.  Hence, if we get an
        // ERROR in a short time-frame, wait a little
        // and try again
        startTimeMs = uPortGetTickTimeMs();
        while ((atError < 0) &&
               (uPortGetTickTimeMs() - startTimeMs <
                U_CELL_SOCK_DNS_SHOULD_RETRY_MS)) {
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) {
                // SARA-R422 can get upset if UDNSRN is sent very quickly
                // after a connection is made so we add a short delay here
                while (uPortGetTickTimeMs() - pInstance->connectedAtMs <
                       U_CELL_SOCK_SARA_R422_DNS_DELAY_MILLISECONDS) {
                    uPortTaskBlock(100);
                }
            }
            atHandle = pInstance->atHandle;

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CGDCONT?");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientUnlock(atHandle);

            uAtClientLock(atHandle);
            // Needs more time
            uAtClientTimeoutSet(atHandle,
                                U_CELL_SOCK_DNS_LOOKUP_TIME_SECONDS * 1000);
            uAtClientCommandStart(atHandle, "AT+UDNSRN=");
            uAtClientWriteInt(atHandle, 0);
            uAtClientWriteString(atHandle, pHostName, true);
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UDNSRN:");
            bytesRead = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
            uAtClientResponseStop(atHandle);
            atError = uAtClientUnlock(atHandle);
            if (atError < 0) {
                // Got an AT interace error, see
                // what the module's socket error
                // number has to say for debug purposes
                doUsoer(atHandle);
                uPortTaskBlock(U_CELL_SOCK_DNS_SHOULD_RETRY_MS / 2);
            }
        }

        if ((atError == 0) && (bytesRead >= 0)) {
            errnoLocal = U_SOCK_ENONE;
            // All is good
            uPortLog("U_CELL_SOCK: found it at \"%.*s\".\n",
                     bytesRead, buffer);
            if (pHostIpAddress != NULL) {
                errnoLocal = U_SOCK_ENXIO;
                // Convert to struct
                if (uSockStringToAddress(buffer,
                                         &address) == 0) {
                    errnoLocal = U_SOCK_ENONE;
                    memcpy(pHostIpAddress, &(address.ipAddress),
                           sizeof(*pHostIpAddress));
                }
            }
        } else {
            uPortLog("U_CELL_SOCK: host not found.\n");
        }
    }

    return -errnoLocal;
}

// Get the local address of a socket.
int32_t uCellSockGetLocalAddress(uDeviceHandle_t cellHandle,
                                 int32_t sockHandle,
                                 uSockAddress_t *pLocalAddress)
{
    int32_t errnoLocal = U_SOCK_EINVAL;
    uCellPrivateInstance_t *pInstance;
    char buffer[U_SOCK_ADDRESS_STRING_MAX_LENGTH_BYTES];

    (void) sockHandle;

    pInstance = pUCellPrivateGetInstance(cellHandle);
    if ((pInstance != NULL) && (pLocalAddress != NULL)) {
        // IP address is that of cellular, for all sockets.
        // uCellNetGetIpAddressStr() returns a positive size
        // on success
        errnoLocal = U_SOCK_ENETDOWN;
        if ((uCellNetGetIpAddressStr(pInstance->cellHandle, buffer) > 0) &&
            (uSockStringToAddress(buffer,
                                  pLocalAddress) == 0)) {
            // TODO: set port number to zero for now but
            // if we implement TCP server then the port
            // number should probably be socket-specific
            // and represent the port the socket is bound to.
            pLocalAddress->port = 0;
            errnoLocal = U_SOCK_ENONE;
        }
    }

    return -errnoLocal;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

// Get the last error code for the given socket.
int32_t uCellSockGetLastError(uDeviceHandle_t cellHandle,
                              int32_t sockHandle)
{
    // Do USOCTL 1 to return the last error code
    return doUsoctl(cellHandle, sockHandle, 1);
}

// Get the number of bytes sent on the given socket
int32_t uCellSockGetBytesSent(uDeviceHandle_t cellHandle,
                              int32_t sockHandle)
{
    // Do USOCTL 2 to return the number of bytes sent
    return doUsoctl(cellHandle, sockHandle, 2);
}

// Get the number of bytes received on the given socket
int32_t uCellSockGetBytesReceived(uDeviceHandle_t cellHandle,
                                  int32_t sockHandle)
{
    // Do USOCTL 3 to return the number of bytes received
    return doUsoctl(cellHandle, sockHandle, 3);
}

// End of file
