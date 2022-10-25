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
 * @brief Implementation of the data API for ble.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "stdio.h"     // snprintf()
#include "limits.h"    // UINT16_MAX

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_os.h"
#include "u_cfg_sw.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"

#include "u_sock_errno.h"
#include "u_sock.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

#include "u_wifi_module_type.h"
#include "u_wifi_sock.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */

#define U_WIFI_MAX_INSTANCE_COUNT 2

/* ----------------------------------------------------------------
 * TYPES
 * ------------------------------------------------------------- */

typedef enum {
    WIFI_INT_OPT_INVALID = -1,
    WIFI_INT_OPT_TCP_NODELAY = 0,
    WIFI_INT_OPT_TCP_KEEPIDLE,
    WIFI_INT_OPT_TCP_KEEPINTVL,
    WIFI_INT_OPT_TCP_KEEPCNT,

    /* Sentinel */
    WIFI_INT_OPT_MAX
} WifiIntOptId_t;

typedef struct {
    int32_t sockHandle; /**< The handle of the socket instance
                             -1 if this socket is not in use. */
    uDeviceHandle_t devHandle; /**< The u-blox device handle.
                             -1 if this socket is not in use. */
    int32_t connHandle; /**< The connection handle that the wifi module
                                   uses for the socket instance.
                                   -1 if this socket is not in use. */
    int32_t edmChannel; /**< The EDM stream channel. */
    uPortSemaphoreHandle_t semaphore;
    uSockType_t type;
    uSockProtocol_t protocol;
    bool connected;
    bool connecting;
    bool closing;
    uSockAddress_t remoteAddress;
    int32_t localPort;
    uShortRangePbufList_t *pTcpRxBuff;
    uShortRangePktList_t udpPktList;
    int32_t intOpts[WIFI_INT_OPT_MAX];
    uWifiSockCallback_t pAsyncClosedCallback; /**< Set to NULL if socket is not in use. */
    uWifiSockCallback_t pDataCallback; /**< Set to NULL if socket is not in use. */
    uWifiSockCallback_t pClosedCallback; /**< Set to NULL if socket is not in use. */
} uWifiSockSocket_t;

typedef enum {
    U_PING_STATUS_WAITING = 0,
    U_PING_STATUS_IP_RECEIVED,
    U_PING_STATUS_ERROR
} uPingStatus_t;

typedef struct {
    volatile uPingStatus_t status;
    volatile uSockAddress_t resultSockAddress;
    uPortSemaphoreHandle_t semaphore;
} uPingContext_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * ------------------------------------------------------------- */
// Keep track of whether we're initialised or not.
static bool gInitialised = false;

/** A list of uDevice handles for the instances
 *  Each time uWifiSockInitInstance() is called the corresponding
 *  device handle will be added to this list. We need this in order
 *  to de-initialize each instance when user calls uWifiSockDeinit()
 */
static uDeviceHandle_t gInstanceDeviceHandleList[U_WIFI_MAX_INSTANCE_COUNT];

/** The sockets: a nice simple array, nothing fancy.
 */
static uWifiSockSocket_t gSockets[U_WIFI_SOCK_MAX_NUM_SOCKETS];
static uPingContext_t gPingContext;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */


/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ------------------------------------------------------------- */

static void freeSocket(uWifiSockSocket_t *pSock)
{
    if (pSock != NULL) {
        pSock->sockHandle = -1;
        if (pSock->semaphore != NULL) {
            uPortSemaphoreDelete(pSock->semaphore);
            pSock->semaphore = NULL;
        }
    }
}

static uWifiSockSocket_t *pAllocateSocket(uDeviceHandle_t devHandle)
{
    bool outOfMemory = false;
    uWifiSockSocket_t *pSock = NULL;

    for (int32_t index = 0; index < U_WIFI_SOCK_MAX_NUM_SOCKETS; index++) {
        if (gSockets[index].sockHandle == -1) {
            int32_t tmp;
            pSock = &(gSockets[index]);
            pSock->sockHandle = index;
            pSock->devHandle = devHandle;
            pSock->semaphore = NULL;
            tmp = uPortSemaphoreCreate(&(pSock->semaphore), 0, 1);
            if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
                outOfMemory = true;
                break;
            }
            if (pSock->protocol == U_SOCK_PROTOCOL_TCP) {
                pSock->pTcpRxBuff = NULL;
            } else if (pSock->protocol == U_SOCK_PROTOCOL_UDP) {
                memset((void *)(&pSock->udpPktList), 0, sizeof(uShortRangePktList_t));
            }
            break;
        }
    }

    if (outOfMemory) {
        freeSocket(pSock);
        pSock = NULL;
    }

    return pSock;
}

static void freeAllSockets(void)
{
    for (int32_t index = 0; index < U_WIFI_SOCK_MAX_NUM_SOCKETS; index++) {
        freeSocket(&(gSockets[index]));
    }
}

static inline WifiIntOptId_t getIntOptionId(int32_t level, uint32_t option)
{
    if (level == U_SOCK_OPT_LEVEL_TCP) {
        switch (option) {
            case U_SOCK_OPT_TCP_NODELAY:
                return WIFI_INT_OPT_TCP_NODELAY;
            case U_SOCK_OPT_TCP_KEEPIDLE:
                return WIFI_INT_OPT_TCP_KEEPIDLE;
            case U_SOCK_OPT_TCP_KEEPINTVL:
                return WIFI_INT_OPT_TCP_KEEPINTVL;
            case U_SOCK_OPT_TCP_KEEPCNT:
                return WIFI_INT_OPT_TCP_KEEPCNT;
            default:
                break;
        }
    }
    return WIFI_INT_OPT_INVALID;
}

static inline int32_t compareSockAddr(const uSockAddress_t *pAddr1,
                                      const uSockAddress_t *pAddr2)
{
    int32_t ret;
    ret = memcmp(&(pAddr1->port), &(pAddr2->port), sizeof(pAddr1->port));
    if (ret == 0) {
        ret = memcmp(&(pAddr1->ipAddress.type),
                     &(pAddr2->ipAddress.type),
                     sizeof(pAddr1->ipAddress.type));
    }
    if (ret == 0) {
        if (pAddr1->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
            ret = memcmp(&(pAddr1->ipAddress.address.ipv4),
                         &(pAddr2->ipAddress.address.ipv4),
                         sizeof(pAddr1->ipAddress.address.ipv4));
        } else {
            ret = memcmp(&(pAddr1->ipAddress.address.ipv6[0]),
                         &(pAddr2->ipAddress.address.ipv6[0]),
                         sizeof(pAddr1->ipAddress.address.ipv6));
        }
    }
    return ret;
}

static int32_t validateSockAddress(const uSockAddress_t *pRemoteAddress)
{
    switch (pRemoteAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            // Check that address is not 0.0.0.0
            if (pRemoteAddress->ipAddress.address.ipv4 == 0) {
                return -U_SOCK_EINVAL;
            }
            break;
        case U_SOCK_ADDRESS_TYPE_V6: {
            // Check that address is not all zeroes
            bool allZero = true;
            for (int i = 0; (i < 4) && allZero; i++) {
                if (pRemoteAddress->ipAddress.address.ipv6[i] != 0) {
                    allZero = false;
                }
            }
            if (allZero) {
                return -U_SOCK_EINVAL;
            }
            break;
        }
        default:
            return -U_SOCK_EINVAL;
    }

    if (pRemoteAddress->port == 0) {
        return -U_SOCK_EINVAL;
    }

    return U_SOCK_ENONE;
}

static uWifiSockSocket_t *pFindConnectingSocketByRemoteAddress(uDeviceHandle_t devHandle,
                                                               const uSockAddress_t *pRemoteAddr)
{
    uWifiSockSocket_t *pSock = NULL;

    for (int32_t index = 0; index < U_WIFI_SOCK_MAX_NUM_SOCKETS; index++) {
        if ((gSockets[index].sockHandle == index) &&      // is active socket
            (gSockets[index].connecting) &&               // is connecting
            (gSockets[index].devHandle == devHandle) && // correct instance
            (compareSockAddr(pRemoteAddr,
                             &gSockets[index].remoteAddress) == 0)) { // correct remote socket address
            pSock = &(gSockets[index]);
            break;
        }
    }

    return pSock;
}

static uWifiSockSocket_t *pFindSocketByEdmChannel(uDeviceHandle_t devHandle, int32_t edmChannel)
{
    uWifiSockSocket_t *pSock = NULL;

    for (int32_t index = 0; index < U_WIFI_SOCK_MAX_NUM_SOCKETS; index++) {
        if (gSockets[index].sockHandle == index &&      // is active socket
            gSockets[index].devHandle == devHandle && // correct instance
            gSockets[index].edmChannel == edmChannel) { // correct edm channel
            pSock = &(gSockets[index]);
            break;
        }
    }

    return pSock;
}

static inline int32_t getInstance(uDeviceHandle_t devHandle,
                                  uShortRangePrivateInstance_t **ppInstance)
{
    if (!gInitialised) {
        return -U_SOCK_EFAULT;
    }

    *ppInstance = pUShortRangePrivateGetInstance(devHandle);
    if (*ppInstance == NULL) {
        return -U_SOCK_EINVAL;
    }

    if ((*ppInstance)->mode != U_SHORT_RANGE_MODE_EDM) {
        return -U_SOCK_EIO;
    }

    return U_SOCK_ENONE;
}

static inline int32_t getInstanceAndSocket(uDeviceHandle_t devHandle, int32_t sockHandle,
                                           uShortRangePrivateInstance_t **ppInstance,
                                           uWifiSockSocket_t **ppSock)
{
    int32_t errnoLocal;

    *ppSock = NULL;
    errnoLocal = getInstance(devHandle, ppInstance);
    if (errnoLocal != U_SOCK_ENONE) {
        return errnoLocal;
    }

    errnoLocal = -U_SOCK_EBADFD;
    if ((sockHandle >= 0) &&
        (sockHandle < U_WIFI_SOCK_MAX_NUM_SOCKETS) &&
        (gSockets[sockHandle].sockHandle == sockHandle) &&
        (gSockets[sockHandle].devHandle == devHandle)) {

        *ppSock = &(gSockets[sockHandle]);
        errnoLocal = U_SOCK_ENONE;
    }

    return errnoLocal;
}

// Get a socket option that has an integer as a
// parameter
static int32_t getOptionInt(uWifiSockSocket_t *pSock,
                            WifiIntOptId_t option,
                            void *pOptionValue,
                            size_t *pOptionValueLength)
{
    if ((pOptionValueLength != NULL) && (pOptionValue == NULL)) {
        // Caller just wants to know the length required
        *pOptionValueLength = sizeof(int32_t);
        return U_SOCK_ENONE;
    }

    if ((pOptionValueLength == NULL) || (pOptionValue == NULL) ||
        (*pOptionValueLength < sizeof(int32_t))) {
        return -U_SOCK_EINVAL;
    }

    *((int32_t *)pOptionValue) = pSock->intOpts[option];

    return U_SOCK_ENONE;
}

// Set a socket option that has an integer as a parameter
static int32_t setOptionInt(uWifiSockSocket_t *pSock,
                            WifiIntOptId_t option,
                            const void *pOptionValue,
                            size_t optionValueLength)
{
    if ((pOptionValue == NULL) || (optionValueLength < sizeof(int32_t))) {
        return -U_SOCK_EINVAL;
    }

    pSock->intOpts[option] = *((const int32_t *)pOptionValue);

    return U_SOCK_ENONE;
}

// Convert a short range IP struct to uSockAddress structs
static void convertToSockAddress(const uShortRangeConnectDataIp_t *pShoAddr,
                                 uint16_t *pLocalPort,
                                 uSockAddress_t *pRemoteSockAddr)
{
    if (pShoAddr->type == U_SHORT_RANGE_CONNECTION_IPv4) {
        pRemoteSockAddr->port = pShoAddr->ipv4.remotePort;
        pRemoteSockAddr->ipAddress.type = U_SOCK_ADDRESS_TYPE_V4;
        pRemoteSockAddr->ipAddress.address.ipv4 = 0;
        *pLocalPort = pShoAddr->ipv4.localPort;
        for (int i = 0; i < 4; i++) {
            pRemoteSockAddr->ipAddress.address.ipv4 |=
                ((uint32_t)pShoAddr->ipv4.remoteAddress[i]) << (8 * (3 - i));
        }
    } else {
        pRemoteSockAddr->port = pShoAddr->ipv6.remotePort;
        pRemoteSockAddr->ipAddress.type = U_SOCK_ADDRESS_TYPE_V6;
        *pLocalPort = pShoAddr->ipv6.localPort;
        for (int i = 0; i < 4; i++) {
            pRemoteSockAddr->ipAddress.address.ipv6[i] = 0;
            for (int j = 0; j < 4; j++) {
                //lint -e{679}
                pRemoteSockAddr->ipAddress.address.ipv6[i] |=
                    ((uint32_t)pShoAddr->ipv6.remoteAddress[(i * 4) + j]) << (8 * (3 - j));
            }
        }
    }
}

//lint -e{818} suppress "address could be declared as pointing to const":
// need to follow function signature
static void edmIpConnectionCallback(int32_t edmHandle,
                                    int32_t edmChannel,
                                    uShortRangeConnectionEventType_t eventType,
                                    const uShortRangeConnectDataIp_t *pConnectData,
                                    void *pCallbackParameter)
{
    (void)edmHandle;
    volatile uDeviceHandle_t devHandle;
    volatile int32_t sockHandle = -1;
    volatile uWifiSockCallback_t pUserClosedCb = NULL;
    volatile uWifiSockCallback_t pUserAsyncClosedCb = NULL;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pCallbackParameter;
    // Basic validation
    if (pInstance == NULL || pInstance->atHandle == NULL) {
        return;
    }

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        uPortLog("U_WIFI_SOCK: ERROR failed to take lock\n");
    }

    devHandle = pInstance->devHandle;

    switch (eventType) {
        case U_SHORT_RANGE_EVENT_CONNECTED: {
            uSockAddress_t remoteAddr;
            uint16_t localPort;
            convertToSockAddress(pConnectData,
                                 &localPort,
                                 &remoteAddr);
            pSock = pFindConnectingSocketByRemoteAddress(devHandle, &remoteAddr);
            if (pSock) {
                pSock->edmChannel = edmChannel;
                pSock->connected = true;
                pSock->localPort = localPort;
            }
            break;
        }

        case U_SHORT_RANGE_EVENT_DISCONNECTED: {
            pSock = pFindSocketByEdmChannel(devHandle, edmChannel);
            if (pSock && pSock->connected) {
                sockHandle = pSock->sockHandle;
                pSock->connected = false;
                pUserClosedCb = pSock->pClosedCallback;
                pUserAsyncClosedCb = pSock->pAsyncClosedCallback;
                if (pSock->closing) {
                    // User has called close()
                    freeSocket(pSock);
                }
            }
            break;
        }

        default:
            break;
    }

    if (pSock) {
        uPortSemaphoreGive(pSock->semaphore);
    }

    uShortRangeUnlock();

    // Call the user callbacka after the mutex has been unlocked
    if (pUserClosedCb) {
        pUserClosedCb(devHandle, sockHandle);
    }
    if (pUserAsyncClosedCb) {
        pUserAsyncClosedCb(devHandle, sockHandle);
    }
}

static void edmIpDataCallback(int32_t edmHandle, int32_t edmChannel,
                              uShortRangePbufList_t *pBufList,
                              void *pCallbackParameter)
{
    (void)edmHandle;
    volatile uDeviceHandle_t devHandle;
    volatile int32_t sockHandle = -1;
    volatile uWifiSockCallback_t pUserDataCb = NULL;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pCallbackParameter;
    // Basic validation
    if (pInstance == NULL || pInstance->atHandle == NULL) {
        return;
    }

    U_ASSERT( uShortRangeLock() == (int32_t) U_ERROR_COMMON_SUCCESS );

    devHandle = pInstance->devHandle;
    uWifiSockSocket_t *pSock = pFindSocketByEdmChannel(devHandle, edmChannel);
    if (pSock) {
        sockHandle = pSock->sockHandle;
        if (pSock->protocol == U_SOCK_PROTOCOL_UDP) {

            if (uShortRangePktListAppend(&pSock->udpPktList,
                                         pBufList) != (int32_t)U_ERROR_COMMON_SUCCESS) {
                uPortLog("U_WIFI_SOCK: UDP pkt insert failed\n");
                uShortRangePbufListFree(pBufList);
            }
        } else {

            if (pSock->pTcpRxBuff == NULL) {
                pSock->pTcpRxBuff = pBufList;
            } else {
                uShortRangePbufListMerge(pSock->pTcpRxBuff, pBufList);
            }
        }

        // Schedule user data callback
        pUserDataCb = pSock->pDataCallback;
    }

    uShortRangeUnlock();

    // Call the user callback after the mutex has been unlocked
    if (pUserDataCb) {
        pUserDataCb(devHandle, sockHandle);
    }
}

static void UUPING_urc(uAtClientHandle_t atHandle,
                       void *pParameter)
{
    char ipStr[64];
    volatile uPingContext_t *pPingCtx = (volatile uPingContext_t *)pParameter;

    // default to error
    pPingCtx->status = U_PING_STATUS_ERROR;
    // retry_num
    uAtClientReadInt(atHandle);
    // p_size
    uAtClientReadInt(atHandle);
    // remote_hostname
    uAtClientReadString(atHandle, NULL, 256, false);
    // remote_ip
    if (uAtClientReadString(atHandle, ipStr, sizeof(ipStr), false) > 0) {
        int32_t tmp;
        // Use a temporary output variable to avoid "Attempt to cast away volatile" lint issue
        uSockAddress_t tmpAddr;
        tmp = uSockStringToAddress(ipStr, &tmpAddr);
        pPingCtx->resultSockAddress = tmpAddr;
        if (tmp == (int32_t) U_ERROR_COMMON_SUCCESS) {
            pPingCtx->status = U_PING_STATUS_IP_RECEIVED;
        }
    }
    // ttl
    uAtClientReadInt(atHandle);
    // rtt
    uAtClientReadInt(atHandle);

    uPortSemaphoreGive(pPingCtx->semaphore);
}

static void UUPINGER_urc(uAtClientHandle_t atHandle,
                         void *pParameter)
{
    (void)atHandle;
    volatile uPingContext_t *pPingCtx = (volatile uPingContext_t *)pParameter;

    // we received an error
    pPingCtx->status = U_PING_STATUS_ERROR;
    uPortSemaphoreGive(pPingCtx->semaphore);
}

static int32_t connectPeer(uAtClientHandle_t atHandle,
                           const uPortSemaphoreHandle_t semaphoreHandle,
                           const char *pProtocolStr,
                           const uSockAddress_t *pAddress,
                           const char *pFlagStr)
{
    int32_t errnoLocal = U_SOCK_ENONE;
    int32_t tmp;
    char ipAddrStr[64];

    tmp = uSockIpAddressToString(&pAddress->ipAddress,
                                 ipAddrStr, sizeof(ipAddrStr));
    if (tmp <= 0) {
        errnoLocal = tmp;
    }

    if (errnoLocal == U_SOCK_ENONE) {
        char portStr[16];

        snprintf(portStr, sizeof(portStr), ":%d", pAddress->port);

        // Make sure the semaphore is taken
        // it could be given by a disconnect event earlier
        uPortSemaphoreTryTake(semaphoreHandle, 0);

        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDCP=");
        uAtClientWritePartialString(atHandle, true, pProtocolStr);
        uAtClientWritePartialString(atHandle, false, "://");
        uAtClientWritePartialString(atHandle, false, ipAddrStr);
        uAtClientWritePartialString(atHandle, false, portStr);
        if (pFlagStr) {
            uAtClientWritePartialString(atHandle, false, "/?");
            uAtClientWritePartialString(atHandle, false, pFlagStr);
        }
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UDCP:");
        errnoLocal = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        tmp = uAtClientUnlock(atHandle);

        if (tmp == (int32_t) U_ERROR_COMMON_SUCCESS) {
            if (uPortSemaphoreTryTake(semaphoreHandle, 5000) != 0) {
                errnoLocal = -U_SOCK_ETIMEDOUT;
            }
        } else {
            errnoLocal = -U_SOCK_EIO;
        }
    }

    return errnoLocal;
}

static int32_t closePeer(uAtClientHandle_t atHandle,
                         int32_t connHandle)
{
    int32_t errnoLocal = U_SOCK_ENONE;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UDCPC=");
    uAtClientWriteInt(atHandle, connHandle);
    uAtClientCommandStopReadResponse(atHandle);
    if (uAtClientUnlock(atHandle) != (int32_t) U_ERROR_COMMON_SUCCESS) {
        errnoLocal = -U_SOCK_EIO;
    }

    return errnoLocal;
}

int32_t deinitInstance(uDeviceHandle_t devHandle)
{
    int32_t errnoLocal;
    uShortRangePrivateInstance_t *pInstance = NULL;

    // First check that uWifiSockInitInstance has been called
    // and that the instance is not already de-initialized
    errnoLocal = -U_SOCK_EINVAL;
    for (int i = 0; i < U_WIFI_MAX_INSTANCE_COUNT; i++) {
        if (gInstanceDeviceHandleList[i] == devHandle) {
            gInstanceDeviceHandleList[i] = NULL;
            errnoLocal = U_SOCK_ENONE;
            break;
        }
    }

    if (errnoLocal == U_SOCK_ENONE) {
        errnoLocal = getInstance(devHandle, &pInstance);
    }

    if ((errnoLocal == U_SOCK_ENONE) && pInstance) {
        int32_t shortRangeEC;
        shortRangeEC = uShortRangeEdmStreamIpEventCallbackSet(pInstance->streamHandle,
                                                              NULL,
                                                              NULL);

        if (shortRangeEC == (int32_t) U_ERROR_COMMON_SUCCESS) {
            shortRangeEC = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                                    U_SHORT_RANGE_CONNECTION_TYPE_IP,
                                                                    NULL,
                                                                    NULL);
        }

        if (shortRangeEC != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_ENOSR;
        }
    }

    return errnoLocal;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * ------------------------------------------------------------- */

// Initialise the wifi sockets layer.
int32_t uWifiSockInit(void)
{
    int32_t errnoLocal = U_SOCK_ENONE;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    if (!gInitialised) {
        int32_t tmp = uPortSemaphoreCreate(&gPingContext.semaphore, 0, 1);
        if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_ENOMEM;
        }
        for (int i = 0; i < U_WIFI_MAX_INSTANCE_COUNT; i++) {
            gInstanceDeviceHandleList[i] = NULL;
        }
        if (errnoLocal == U_SOCK_ENONE) {
            freeAllSockets();
            gInitialised = true;
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockInitInstance(uDeviceHandle_t devHandle)
{
    int32_t errnoLocal;
    bool alreadyInit = false;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    // Check that the instance isn't already initilized
    errnoLocal = U_SOCK_ENONE;
    for (int i = 0; i < U_WIFI_MAX_INSTANCE_COUNT; i++) {
        if (gInstanceDeviceHandleList[i] == devHandle) {
            alreadyInit = true;
            break;
        }
    }
    if (!alreadyInit) {
        // Try to add the wifi handle to the instance list
        errnoLocal = -U_SOCK_ENOMEM;
        for (int i = 0; i < U_WIFI_MAX_INSTANCE_COUNT; i++) {
            if (gInstanceDeviceHandleList[i] == NULL) {
                errnoLocal = U_SOCK_ENONE;
                gInstanceDeviceHandleList[i] = devHandle;
                break;
            }
        }

        if (errnoLocal == U_SOCK_ENONE) {
            errnoLocal = getInstance(devHandle, &pInstance);
        }
        if ((errnoLocal == U_SOCK_ENONE) && pInstance) {
            if (pInstance->devHandle == NULL) {
                pInstance->devHandle = devHandle;
            }
        }
        if ((errnoLocal == U_SOCK_ENONE) && pInstance) {
            int32_t shortRangeEC;
            shortRangeEC = uShortRangeEdmStreamIpEventCallbackSet(pInstance->streamHandle,
                                                                  edmIpConnectionCallback,
                                                                  pInstance);

            if (shortRangeEC == (int32_t) U_ERROR_COMMON_SUCCESS) {
                shortRangeEC = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle,
                                                                        U_SHORT_RANGE_CONNECTION_TYPE_IP,
                                                                        edmIpDataCallback,
                                                                        pInstance);
            }

            if (shortRangeEC != (int32_t) U_ERROR_COMMON_SUCCESS) {
                errnoLocal = -U_SOCK_ENOSR;
            }
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockDeinitInstance(uDeviceHandle_t devHandle)
{
    int32_t errnoLocal;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }
    errnoLocal = deinitInstance(devHandle);

    uShortRangeUnlock();

    return errnoLocal;
}

// Deinitialise the wifi sockets layer.
void uWifiSockDeinit()
{
    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        uPortLog("U_WIFI_SOCK: ERROR - Failed to take lock\n");
        return;
    }

    if (gInitialised) {
        for (int i = 0; i < U_WIFI_MAX_INSTANCE_COUNT; i++) {
            if (gInstanceDeviceHandleList[i] != NULL) {
                deinitInstance(gInstanceDeviceHandleList[i]);
            }
        }

        freeAllSockets();
        uPortSemaphoreDelete(gPingContext.semaphore);
        // Nothing more to do, URCs will have been
        // removed on close
        gInitialised = false;
    }

    uShortRangeUnlock();
}

int32_t uWifiSockCreate(uDeviceHandle_t devHandle,
                        uSockType_t type,
                        uSockProtocol_t protocol)
{
    int32_t sockHandle = -U_SOCK_ENOMEM;
    uShortRangePrivateInstance_t *pInstance = NULL;
    uWifiSockSocket_t *pSock;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    // Find the instance
    sockHandle = getInstance(devHandle, &pInstance);
    if (sockHandle == U_SOCK_ENONE) {
        pSock = pAllocateSocket(devHandle);
        if (pSock != NULL) {
            pSock->type = type;
            pSock->protocol = protocol;
            pSock->connected = false;
            pSock->closing = false;
            pSock->edmChannel = -1;
            pSock->connHandle = -1;
            memset(&pSock->remoteAddress, 0, sizeof(pSock->remoteAddress));
            pSock->localPort = pInstance->sockNextLocalPort;
            pInstance->sockNextLocalPort = -1;
            memset(pSock->intOpts, 0, sizeof(pSock->intOpts));
            sockHandle = pSock->sockHandle;
        }
    }

    uShortRangeUnlock();

    return sockHandle;
}

int32_t uWifiSockConnect(uDeviceHandle_t devHandle,
                         int32_t sockHandle,
                         const uSockAddress_t *pRemoteAddress)
{
    int32_t errnoLocal;
    bool udpAndConnected = false;
    uShortRangePrivateInstance_t *pInstance = NULL;
    uWifiSockSocket_t *pSock = NULL;
    int32_t x = 0;

    errnoLocal = validateSockAddress(pRemoteAddress);
    if (errnoLocal != U_SOCK_ENONE) {
        return errnoLocal;
    }

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        pSock->remoteAddress = *pRemoteAddress;
        pSock->connecting = true;
        // This is probably a very rare case but if user first calls sendTo()
        // and later on connect() it is expected that this should succeed.
        // This is what happens in the sockBasicUdp test.
        udpAndConnected = ((pSock->protocol == U_SOCK_PROTOCOL_UDP) &&
                           pSock->connected &&
                           (compareSockAddr(pRemoteAddress, &pSock->remoteAddress) == 0));
    }

    if (errnoLocal == U_SOCK_ENONE && !udpAndConnected) {
        char flagStr[96] = {0};
        volatile uAtClientHandle_t atHandle = pInstance->atHandle;
        volatile uPortSemaphoreHandle_t connectionSem = pSock->semaphore;

        errnoLocal = U_SOCK_ENOMEM;
        if (pSock->localPort >= 0) {
            x = snprintf(flagStr, sizeof(flagStr), "local_port=%d&",
                         (int)pSock->localPort);
        }

        if (x >= 0) {
            errnoLocal = U_SOCK_ENONE;
            snprintf(flagStr + x, sizeof(flagStr) - x, "flush_tx=%d&keepalive=%d+%d+%d",
                     (int)pSock->intOpts[WIFI_INT_OPT_TCP_NODELAY],
                     (int)pSock->intOpts[WIFI_INT_OPT_TCP_KEEPIDLE],
                     (int)pSock->intOpts[WIFI_INT_OPT_TCP_KEEPINTVL],
                     (int)pSock->intOpts[WIFI_INT_OPT_TCP_KEEPCNT]);

            // We need to release the lock during connection phase
            uShortRangeUnlock();

            int32_t conPeerResult;
            if (pSock->protocol == U_SOCK_PROTOCOL_TCP) {
                conPeerResult = connectPeer(atHandle, connectionSem, "tcp", pRemoteAddress, flagStr);
            } else {
                conPeerResult = connectPeer(atHandle, connectionSem, "udp", pRemoteAddress, NULL);
            }

            // Reclaim the lock so we can continue working with the socket
            if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
                return -U_SOCK_EIO;
            }

            // Make sure the socket is still valid
            if (pSock->sockHandle != sockHandle) {
                errnoLocal = -U_SOCK_EIO;
            }

            if (conPeerResult >= 0) {
                if (errnoLocal == U_SOCK_ENONE) {
                    pSock->connHandle = conPeerResult;
                    // The connection attempt is finished but it might have failed
                    if (!pSock->connected) {
                        errnoLocal = -U_SOCK_ECONNREFUSED;
                    } else if (pSock->edmChannel < 0) { // Make sure we got the EDM channel
                        errnoLocal = -U_SOCK_EUNATCH;
                    }
                }
                // On failure make sure that the peer is closed
                if ((errnoLocal != U_SOCK_ENONE) && (pSock->connHandle >= 0)) {
                    closePeer(pInstance->atHandle, pSock->connHandle);
                    // Update socket state
                    pSock->connHandle = -1;
                    pSock->edmChannel = -1;
                }
            } else {
                errnoLocal = conPeerResult;
            }
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockClose(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       uWifiSockCallback_t pCallback)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        pSock->pAsyncClosedCallback = pCallback;
        if (!pSock->closing) {
            pSock->closing = true;
            if (pSock->connected) {
                volatile uAtClientHandle_t atHandle = pInstance->atHandle;
                volatile int32_t connHandle = pSock->connHandle;

                // We need to release the lock during disconnection phase
                uShortRangeUnlock();

                errnoLocal = closePeer(atHandle, connHandle);

                // Reclaim the lock so we can continue working with the socket
                if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    return -U_SOCK_EIO;
                }
            } else {
                // Peer is already disconnected so deallocate socket
                freeSocket(pSock);
            }
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

void uWifiSockCleanup(uDeviceHandle_t devHandle)
{
    // Not supported - do nothing
    (void)devHandle;
}

void uWifiSockBlockingSet(uDeviceHandle_t devHandle,
                          int32_t sockHandle,
                          bool isBlocking)
{
    // Not supported - do nothing
    (void)devHandle;
    (void)sockHandle;
    (void)isBlocking;
}

bool uWifiSockBlockingGet(uDeviceHandle_t devHandle,
                          int32_t sockHandle)
{
    // Not supported
    (void)devHandle;
    (void)sockHandle;
    return false;
}

int32_t uWifiSockOptionSet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           const void *pOptionValue,
                           size_t optionValueLength)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        WifiIntOptId_t wifiOpt = getIntOptionId(level, option);

        errnoLocal = -U_SOCK_EINVAL;
        if (wifiOpt != WIFI_INT_OPT_INVALID) {
            errnoLocal = setOptionInt(pSock, wifiOpt, pOptionValue, optionValueLength);
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockOptionGet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           void *pOptionValue,
                           size_t *pOptionValueLength)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        WifiIntOptId_t wifiOpt = getIntOptionId(level, option);

        errnoLocal = -U_SOCK_EINVAL;
        if (wifiOpt != WIFI_INT_OPT_INVALID) {
            errnoLocal = getOptionInt(pSock, wifiOpt, pOptionValue, pOptionValueLength);
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port)
{
    int32_t errnoLocal;
    uShortRangePrivateInstance_t *pInstance = NULL;

    // Find the instance
    errnoLocal = getInstance(devHandle, &pInstance);
    if ((errnoLocal == U_SOCK_ENONE) &&
        ((port == -1) || ((port >= 0) && (port <= UINT16_MAX)))) {
        pInstance->sockNextLocalPort = port;
    }

    return errnoLocal;
}

int32_t uWifiSockWrite(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       const void *pData, size_t dataSizeBytes)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if ((dataSizeBytes == 0) || (pData == NULL)) {
        return -U_SOCK_EINVAL;
    }

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);

    // We only support Write for TCP sockets
    if ((errnoLocal == U_SOCK_ENONE) && (pSock->protocol != U_SOCK_PROTOCOL_TCP)) {
        errnoLocal = -U_SOCK_EOPNOTSUPP;
    }

    if (errnoLocal == U_SOCK_ENONE) {
        // Make sure we got the EDM channel
        if (pSock->edmChannel < 0) {
            errnoLocal = -U_SOCK_EUNATCH;
        }
    }
    if (errnoLocal == U_SOCK_ENONE) {
        int32_t shortRangeEC = uShortRangeEdmStreamWrite(pInstance->streamHandle,
                                                         pSock->edmChannel,
                                                         pData, dataSizeBytes,
                                                         U_WIFI_SOCK_WRITE_TIMEOUT_MS);
        if (shortRangeEC >= 0) {
            errnoLocal = shortRangeEC;
        } else {
            errnoLocal = U_SOCK_ECOMM;
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockRead(uDeviceHandle_t devHandle,
                      int32_t sockHandle,
                      void *pData, size_t dataSizeBytes)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;
    uShortRangePbufList_t *pList;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);

    // We only support Read for TCP sockets
    if ((errnoLocal == U_SOCK_ENONE) && (pSock->protocol != U_SOCK_PROTOCOL_TCP)) {
        errnoLocal = -U_SOCK_EOPNOTSUPP;
    }

    if (errnoLocal == U_SOCK_ENONE) {
        pList = pSock->pTcpRxBuff;
        errnoLocal = (int32_t)uShortRangePbufListConsumeData(pList, (char *)pData, dataSizeBytes);
        if (errnoLocal == 0) {
            // If there are no data available we must return U_SOCK_EWOULDBLOCK
            errnoLocal = -U_SOCK_EWOULDBLOCK;
        }

        if ((pList != NULL) && (pList->totalLen == 0)) {
            uShortRangePbufListFree(pList);
            pSock->pTcpRxBuff = NULL;
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockSendTo(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        const uSockAddress_t *pRemoteAddress,
                        const void *pData,
                        size_t dataSizeBytes)
{
    int32_t errnoLocal;
    uShortRangePrivateInstance_t *pInstance = NULL;
    uWifiSockSocket_t *pSock = NULL;

    errnoLocal = validateSockAddress(pRemoteAddress);
    if (errnoLocal != U_SOCK_ENONE) {
        return errnoLocal;
    }

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);

    // We only support SendTo for UDP sockets
    if ((errnoLocal == U_SOCK_ENONE) && (pSock->protocol != U_SOCK_PROTOCOL_UDP)) {
        errnoLocal = -U_SOCK_EOPNOTSUPP;
    }

    if (errnoLocal == U_SOCK_ENONE) {
        // Check if there are already a peer or if we need to setup a new one
        if (pSock->connHandle < 0) {
            char flagStr[32] = {0};
            char *pFlagStr = NULL;
            volatile uAtClientHandle_t atHandle = pInstance->atHandle;
            volatile uPortSemaphoreHandle_t connectionSem = pSock->semaphore;
            pSock->remoteAddress = *pRemoteAddress;
            pSock->connecting = true;

            if (pSock->localPort >= 0) {
                snprintf(flagStr, sizeof(flagStr), "local_port=%d",
                         (int)pSock->localPort);
                pFlagStr = flagStr;
            }

            // We need to release the lock during connection phase
            uShortRangeUnlock();

            int32_t conPeerResult = connectPeer(atHandle, connectionSem, "udp", pRemoteAddress, pFlagStr);

            // Reclaim the lock so we can continue working with the socket
            if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
                return -U_SOCK_EIO;
            }

            // Make sure the socket is still valid
            if (pSock->sockHandle == sockHandle) {
                pSock->connecting = false;
                if (conPeerResult >= 0) {
                    pSock->connHandle = conPeerResult;
                    // The connection attempt is finished but it might have failed
                    if (!pSock->connected) {
                        errnoLocal = -U_SOCK_ECONNREFUSED;
                    } else if (pSock->edmChannel < 0) { // Make sure we got the EDM channel
                        errnoLocal = -U_SOCK_EUNATCH;
                    }

                } else {
                    errnoLocal = conPeerResult;
                }
                // On failure make sure that the peer is closed
                if ((errnoLocal != U_SOCK_ENONE) && (pSock->connHandle >= 0)) {
                    closePeer(pInstance->atHandle, pSock->connHandle);
                    // Update socket state
                    pSock->connHandle = -1;
                    pSock->edmChannel = -1;
                }
            } else {
                errnoLocal = -U_SOCK_EIO;
            }
        } else {
            // We already have a peer. Make sure the caller remote address matches.
            if (compareSockAddr(&pSock->remoteAddress, pRemoteAddress) != 0) {
                errnoLocal = -U_SOCK_EADDRNOTAVAIL;
            }
        }
    }

    // Write the data
    if (errnoLocal == U_SOCK_ENONE) {
        int32_t shortRangeEC = uShortRangeEdmStreamWrite(pInstance->streamHandle,
                                                         pSock->edmChannel,
                                                         pData, dataSizeBytes,
                                                         U_WIFI_SOCK_WRITE_TIMEOUT_MS);
        if (shortRangeEC >= 0) {
            errnoLocal = shortRangeEC;
        } else {
            errnoLocal = U_SOCK_ECOMM;
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockReceiveFrom(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             uSockAddress_t *pRemoteAddress,
                             void *pData, size_t dataSizeBytes)
{
    int32_t errnoLocal;
    uShortRangePrivateInstance_t *pInstance = NULL;
    uWifiSockSocket_t *pSock = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);

    if ((errnoLocal == U_SOCK_ENONE) && (pSock->connHandle < 0)) {
        // uWifiSockSendTo must have been called first in order to setup the peer
        errnoLocal = -U_SOCK_EUNATCH;
    }

    // We only support ReceiveFrom for UDP sockets
    if ((errnoLocal == U_SOCK_ENONE) && (pSock->protocol != U_SOCK_PROTOCOL_UDP)) {
        errnoLocal = -U_SOCK_EOPNOTSUPP;
    }

    // Read the data
    if (errnoLocal == U_SOCK_ENONE) {

        errnoLocal = uShortRangePktListConsumePacket(&pSock->udpPktList, (char *)pData, &dataSizeBytes,
                                                     NULL);

        if ((errnoLocal == (int32_t)U_ERROR_COMMON_NO_MEMORY) ||
            (errnoLocal == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER)) {
            errnoLocal = -U_SOCK_EWOULDBLOCK;
        } else if (errnoLocal == (int32_t)U_ERROR_COMMON_TEMPORARY_FAILURE) {
            errnoLocal = -U_SOCK_EMSGSIZE;
        } else if (errnoLocal == (int32_t)U_ERROR_COMMON_SUCCESS) {
            errnoLocal = (int32_t)dataSizeBytes;
        }

        if (pRemoteAddress) {
            // At the moment we only receive packets from the address from first
            // call to uWifiSockSendTo()
            *pRemoteAddress = pSock->remoteAddress;
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}


int32_t uWifiSockRegisterCallbackData(uDeviceHandle_t devHandle,
                                      int32_t sockHandle,
                                      uWifiSockCallback_t pCallback)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        pSock->pDataCallback = pCallback;
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockRegisterCallbackClosed(uDeviceHandle_t devHandle,
                                        int32_t sockHandle,
                                        uWifiSockCallback_t pCallback)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        pSock->pClosedCallback = pCallback;
    }

    uShortRangeUnlock();

    return errnoLocal;
}

int32_t uWifiSockGetHostByName(uDeviceHandle_t devHandle,
                               const char *pHostName,
                               uSockIpAddress_t *pHostIpAddress)
{
    int32_t tmp;
    int32_t errnoLocal;
    uAtClientHandle_t atHandle = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstance(devHandle, &pInstance);
    if (errnoLocal == U_SOCK_ENONE) {
        atHandle = pInstance->atHandle;
    }

    // Register the ping URCs
    if (errnoLocal == U_SOCK_ENONE) {
        tmp = uAtClientSetUrcHandler(pInstance->atHandle, "+UUPING:",
                                     UUPING_urc, (void *)&gPingContext);
        if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_ENOMEM;
        }
    }
    if (errnoLocal == U_SOCK_ENONE) {
        tmp = uAtClientSetUrcHandler(pInstance->atHandle, "+UUPINGER:",
                                     UUPINGER_urc, (void *)&gPingContext);
        if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_ENOMEM;
        }
    }

    // Send UPING AT command
    if (errnoLocal == U_SOCK_ENONE) {
        // Make sure the semaphore is cleared before we start
        uPortSemaphoreTryTake(gPingContext.semaphore, 0);
        gPingContext.status = U_PING_STATUS_WAITING;

        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPING=");
        // remote_host
        uAtClientWriteString(atHandle, pHostName, false);
        // retry_num
        uAtClientWriteInt(atHandle, 1);
        // p_size
        uAtClientWriteInt(atHandle, 64);
        // timeout
        uAtClientWriteInt(atHandle, 10);
        uAtClientCommandStopReadResponse(atHandle);
        tmp = uAtClientUnlock(atHandle);
        if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_EIO;
        }
    }

    // Wait for response
    if (errnoLocal == U_SOCK_ENONE) {
        tmp = uPortSemaphoreTryTake(gPingContext.semaphore, 5000);
        if (tmp == (int32_t) U_ERROR_COMMON_SUCCESS) {
            if (gPingContext.status == U_PING_STATUS_IP_RECEIVED) {
                *pHostIpAddress = gPingContext.resultSockAddress.ipAddress;
            } else {
                errnoLocal = -U_SOCK_EHOSTUNREACH;
            }
        } else {
            errnoLocal = -U_SOCK_ETIMEDOUT;
        }
    }

    // Regardless if there is an error or not we unregister the URC
    if (pInstance) {
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUPING:");
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUPINGER:");
    }

    uShortRangeUnlock();

    return errnoLocal;
}


int32_t uWifiSockGetLocalAddress(uDeviceHandle_t devHandle,
                                 int32_t sockHandle,
                                 uSockAddress_t *pLocalAddress)
{
    int32_t errnoLocal;
    uWifiSockSocket_t *pSock = NULL;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (uShortRangeLock() != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return -U_SOCK_EIO;
    }

    errnoLocal = getInstanceAndSocket(devHandle, sockHandle, &pInstance, &pSock);
    if (errnoLocal == U_SOCK_ENONE) {
        char ipStr[64];
        int32_t tmp;
        int32_t status_id = 101; // Local IPv4 address
        uAtClientHandle_t atHandle = pInstance->atHandle;

        if (pSock->remoteAddress.ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
            status_id = 201; // Local IPv6 address
        }

        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UNSTAT=");
        uAtClientWriteInt(atHandle, 0);
        uAtClientWriteInt(atHandle, status_id);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UNSTAT:");
        // Skip configuration_id and param_tag
        uAtClientSkipParameters(atHandle, 2);
        tmp = uAtClientReadString(atHandle, ipStr, sizeof(ipStr), false);
        if (tmp < (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_EIO;
        }
        uAtClientResponseStop(atHandle);
        tmp = uAtClientUnlock(atHandle);
        if (tmp != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errnoLocal = -U_SOCK_EIO;
        }

        if (errnoLocal == U_SOCK_ENONE) {
            // TODO: The port number will be set to 0 for now.
            // We can retrieve the local port from pSock, but
            // this value will not be valid until a connection
            // has been opened.
            tmp = uSockStringToAddress(ipStr, pLocalAddress);
            if (tmp != U_SOCK_ENONE) {
                // If we receive a IP that cannot be parsed it is most
                // likely because the network is down.
                errnoLocal = U_SOCK_ENETDOWN;
            }
        }
    }

    uShortRangeUnlock();

    return errnoLocal;
}

// End of file
