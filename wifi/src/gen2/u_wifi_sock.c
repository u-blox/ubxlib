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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the data API for ble.
 */
;
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

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
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
#include "u_short_range_cfg.h"
#include "u_wifi_module_type.h"
#include "u_wifi_sock.h"

#include "u_cx_urc.h"
#include "u_cx_socket.h"
#include "u_cx_wifi.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * ------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * ------------------------------------------------------------- */

typedef struct {
    uDeviceHandle_t devHandle;
    int32_t uCxSockHandle;
    int32_t sockHandle;
    uPortSemaphoreHandle_t semaphore;
    uSockProtocol_t protocol;
    int32_t localPort;
    uSockAddress_t remoteAddress;
    int32_t remoteSockHandle;
    bool dataAvailable;
    uWifiSockCallback_t pDataCallback;
    uWifiSockCallback_t pClosedCallback;
} uWifiSocket_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * ------------------------------------------------------------- */

static uPortMutexHandle_t gSocketsMutex = NULL;
static uWifiSocket_t *gSocketList[U_WIFI_SOCK_MAX_NUM_SOCKETS];

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* Workaround for WiFi captive portal. Used to control the accept()
   timeout for now. Will be removed once a full select() implementation
   is available.
*/
int32_t gUWifiSocketAcceptTimeoutS = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * ------------------------------------------------------------- */

static int32_t findFreeSocketHandle()
{
    // Find free slot
    int32_t sockHandle = -1;
    for (int32_t i = 0; i < U_WIFI_SOCK_MAX_NUM_SOCKETS; i++) {
        if (gSocketList[i] == NULL) {
            sockHandle = i;
            break;
        }
    }
    return sockHandle;
}

static uWifiSocket_t *getSocketByHandle(int32_t sockHandle)
{
    uWifiSocket_t *pSock = NULL;
    if (sockHandle >= 0 && sockHandle < U_WIFI_SOCK_MAX_NUM_SOCKETS) {
        pSock = gSocketList[sockHandle];
    }
    return pSock;
}

static uWifiSocket_t *getSocketByUcxHandle(int32_t uCxSockHandle)
{
    for (int32_t i = 0; i < U_WIFI_SOCK_MAX_NUM_SOCKETS; i++) {
        uWifiSocket_t *pSock = gSocketList[i];
        if ((pSock != NULL) && (pSock->uCxSockHandle == uCxSockHandle)) {
            return pSock;
        }
    }
    return NULL;
}

static void socketConnectCallback(struct uCxHandle *puCxHandle, int32_t uCxSockHandle)
{
    uWifiSocket_t *pSock = getSocketByUcxHandle(uCxSockHandle);
    if (pSock) {
        uPortSemaphoreGive(pSock->semaphore);
    }
}

static void socketDataCallback(struct uCxHandle *puCxHandle, int32_t uCxSockHandle,
                               int32_t number_bytes)
{
    (void)number_bytes;
    uWifiSocket_t *pSock = getSocketByUcxHandle(uCxSockHandle);
    if (pSock) {
        pSock->dataAvailable = true;
        if (pSock->pDataCallback != NULL) {
            pSock->pDataCallback(pSock->devHandle, pSock->sockHandle);
        }
    }
}

static void socketIncomingConnectCallback(struct uCxHandle *puCxHandle, int32_t uCxSockHandle,
                                          uSockIpAddress_t *pRemoteIp,
                                          int32_t listeningSocketHandle)
{
    uWifiSocket_t *pSock = getSocketByUcxHandle(uCxSockHandle);
    if (pSock) {
        uPortMutexLock(gSocketsMutex);
        int32_t incomingSockHandle = findFreeSocketHandle();
        if (incomingSockHandle >= 0) {
            uWifiSocket_t *pIncomingSock = pUPortMalloc(sizeof(uWifiSocket_t));
            if (pIncomingSock != NULL) {
                *pIncomingSock = *pSock;
                pIncomingSock->uCxSockHandle = listeningSocketHandle;
                pIncomingSock->sockHandle = incomingSockHandle;
                pIncomingSock->remoteSockHandle = -1;
                gSocketList[incomingSockHandle] = pIncomingSock;
                pSock->remoteAddress.ipAddress.address = pRemoteIp->address;
                pSock->remoteSockHandle = listeningSocketHandle;
                uPortSemaphoreGive(pSock->semaphore);
            }
        }
        uPortMutexLock(gSocketsMutex);
    }
}

static void socketClosedCallback(struct uCxHandle *puCxHandle, int32_t uCxSockHandle)
{
    uWifiSocket_t *pSock = getSocketByUcxHandle(uCxSockHandle);
    if (pSock) {
        if (pSock->pClosedCallback != NULL) {
            pSock->pClosedCallback(pSock->devHandle, pSock->sockHandle);
        }
    }
}

static uOption_t getIntOptionId(int32_t level, uint32_t option)
{
    if (level == U_SOCK_OPT_LEVEL_TCP) {
        switch (option) {
            case U_SOCK_OPT_TCP_NODELAY:
                return U_OPTION_NO_DELAY;
            case U_SOCK_OPT_TCP_KEEPIDLE:
                return U_OPTION_KEEP_IDLE;
            case U_SOCK_OPT_TCP_KEEPINTVL:
                return U_OPTION_KEEP_INTVL;
            case U_SOCK_OPT_TCP_KEEPCNT:
                return U_OPTION_KEEP_CNT;
            default:
                break;
        }
    }
    return (uOption_t) -1;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiSockPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * ------------------------------------------------------------- */

// Initialise the wifi sockets layer.
int32_t uWifiSockInit(void)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    if (gSocketsMutex == NULL) {
        errorCode = uPortMutexCreate(&gSocketsMutex);
    }
    return errorCode;
}

int32_t uWifiSockInitInstance(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uCxUrcRegisterSocketConnect(pUcxHandle, socketConnectCallback);
        uCxUrcRegisterSocketDataAvailable(pUcxHandle, socketDataCallback);
        uCxUrcRegisterSocketClosed(pUcxHandle, socketClosedCallback);
        uCxUrcRegisterSocketIncommingConnection(pUcxHandle,
                                                socketIncomingConnectCallback);
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiSockDeinitInstance(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uCxUrcRegisterSocketConnect(pUcxHandle, NULL);
        uCxUrcRegisterSocketDataAvailable(pUcxHandle, NULL);
        uCxUrcRegisterSocketClosed(pUcxHandle, NULL);
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

// Deinitialise the wifi sockets layer.
void uWifiSockDeinit()
{
    if (gSocketsMutex != NULL) {
        uPortMutexDelete(gSocketsMutex);
        gSocketsMutex = NULL;
    }
}

int32_t uWifiSockCreate(uDeviceHandle_t devHandle,
                        uSockType_t type,
                        uSockProtocol_t protocol)
{
    (void)type;
    uPortMutexLock(gSocketsMutex);
    int32_t sockHandle = findFreeSocketHandle();
    if (sockHandle == -1) {
        return (int32_t)U_ERROR_COMMON_NO_MEMORY;
    }
    int32_t errorCodeOrHandle = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if ((pInstance != NULL) && pUcxHandle != NULL) {
        errorCodeOrHandle = (int32_t)U_ERROR_COMMON_NO_MEMORY;
        uWifiSocket_t *pSocket = (uWifiSocket_t *)pUPortMalloc(sizeof(uWifiSocket_t));
        if (pSocket != NULL) {
            int32_t uCxsockHandle;
            errorCodeOrHandle = uCxSocketCreate1(pUcxHandle,
                                                 (uProtocol_t)protocol, &uCxsockHandle);
            if (errorCodeOrHandle >= 0) {
                memset((void *)pSocket, 0, sizeof(uWifiSocket_t));
                pSocket->remoteSockHandle = -1;
                pSocket->devHandle = devHandle;
                pSocket->uCxSockHandle = uCxsockHandle;
                pSocket->sockHandle = sockHandle;
                uPortSemaphoreCreate(&(pSocket->semaphore), 0, 1);
                pSocket->protocol = protocol;
                pSocket->localPort = -1;
                pSocket->localPort = pInstance->sockNextLocalPort;
                pInstance->sockNextLocalPort = -1;
                gSocketList[sockHandle] = pSocket;
                errorCodeOrHandle = sockHandle;
            } else {
                uPortFree(pSocket);
            }
        }
    }
    uPortMutexUnlock(gSocketsMutex);
    return errorCodeOrHandle;
}

int32_t uWifiSockConnect(uDeviceHandle_t devHandle,
                         int32_t sockHandle,
                         const uSockAddress_t *pRemoteAddress)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        char addrString[25];
        if (uSockAddressToString(pRemoteAddress, addrString, sizeof(addrString)) > 0) {
            char *pos = strrchr(addrString, ':');
            *pos = 0;
        }
        errorCode = uCxSocketConnect(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                     addrString, pRemoteAddress->port);
        if (errorCode >= 0 && pUWiFiSocket->protocol == U_SOCK_PROTOCOL_TCP) {
            errorCode = uPortSemaphoreTryTake(pUWiFiSocket->semaphore,
                                              U_SOCK_DEFAULT_RECEIVE_TIMEOUT_MS);
        }
    }
    return errorCode;
}

int32_t uWifiSockClose(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       uWifiSockCallback_t pCallback)
{
    (void)pCallback;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uPortMutexLock(gSocketsMutex);
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        errorCode = uCxSocketClose(pUcxHandle, pUWiFiSocket->uCxSockHandle);
        if (pCallback) {
            pCallback(pUWiFiSocket->devHandle, sockHandle);
        }
        if (pUWiFiSocket->pClosedCallback != NULL) {
            pUWiFiSocket->pClosedCallback(pUWiFiSocket->devHandle, pUWiFiSocket->sockHandle);
        }

        uPortSemaphoreDelete(pUWiFiSocket->semaphore);
        uPortFree(gSocketList[sockHandle]);
        gSocketList[sockHandle] = NULL;
    }
    uPortMutexUnlock(gSocketsMutex);
    return errorCode;
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
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        uCxSocketSetOption(pUcxHandle, pUWiFiSocket->uCxSockHandle, U_OPTION_BLOCK,
                           isBlocking ? 1 : 0);
    }
}

bool uWifiSockBlockingGet(uDeviceHandle_t devHandle,
                          int32_t sockHandle)
{
    bool isBlocking = false;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        int32_t value;
        isBlocking = (uCxSocketGetOption(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                         U_OPTION_BLOCK, &value) == 0) &&
                     value == 1;
    }
    return isBlocking;
}

int32_t uWifiSockOptionSet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           const void *pOptionValue,
                           size_t optionValueLength)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        uOption_t wifiOpt = getIntOptionId(level, option);
        if ((wifiOpt != (uOption_t) -1) && (optionValueLength == sizeof(int32_t))) {
            errorCode = uCxSocketSetOption(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                           wifiOpt, *((int32_t *)pOptionValue));
        }
    }
    return errorCode;
}

int32_t uWifiSockOptionGet(uDeviceHandle_t devHandle,
                           int32_t sockHandle,
                           int32_t level,
                           uint32_t option,
                           void *pOptionValue,
                           size_t *pOptionValueLength)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        uOption_t wifiOpt = getIntOptionId(level, option);
        if ((wifiOpt != (uOption_t) -1) && (*pOptionValueLength == sizeof(int32_t))) {
            int32_t value;
            errorCode = uCxSocketGetOption(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                           wifiOpt, &value);
            if (errorCode == 0) {
                *((int32_t *)pOptionValue) = value;
            }
        }
    }
    return errorCode;
}

int32_t uWifiSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);
    if ((pInstance != NULL) && (port != -1) && ((port >= 0) && (port <= UINT16_MAX))) {
        pInstance->sockNextLocalPort = port;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiSockWrite(uDeviceHandle_t devHandle,
                       int32_t sockHandle,
                       const void *pData, size_t dataSizeBytes)
{
    (void)dataSizeBytes;
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        errorCodeOrLength = uCxSocketWriteBinary(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                                 (uint8_t *)pData, dataSizeBytes);
    }
    return errorCodeOrLength;
}

int32_t uWifiSockRead(uDeviceHandle_t devHandle,
                      int32_t sockHandle,
                      void *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        // ucx limit
        dataSizeBytes = MIN(dataSizeBytes, 1000);
        errorCodeOrLength = uCxSocketReadBinary(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                                dataSizeBytes, pData);
    }
    if (errorCodeOrLength == 0) {
        // If there is no data available we must return U_SOCK_EWOULDBLOCK
        errorCodeOrLength = -U_SOCK_EWOULDBLOCK;
    }
    return errorCodeOrLength;
}

int32_t uWifiSockSendTo(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        const uSockAddress_t *pRemoteAddress,
                        const void *pData,
                        size_t dataSizeBytes)
{
    // *** UCX WORKAROUND FIX ***
    // Currently no corresponding ucx api. Use normal connect and write.
    int32_t errorCodeOrLength = uWifiSockConnect(devHandle, sockHandle, pRemoteAddress);
    if (errorCodeOrLength == 0) {
        uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
        if (pUWiFiSocket != NULL) {
            pUWiFiSocket->remoteAddress = *pRemoteAddress;    // Save for ReceiveFrom, see below
        }
        errorCodeOrLength = uWifiSockWrite(devHandle, sockHandle, pData, dataSizeBytes);
    }
    return errorCodeOrLength;
}

int32_t uWifiSockReceiveFrom(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             uSockAddress_t *pRemoteAddress,
                             void *pData, size_t dataSizeBytes)
{
    // *** UCX WORKAROUND FIX ***
    // The corresponding ucx ReceiveFrom only supports string transfer.
    // So therefor we use the common read function.
    // The data may be split up so loop as long as available, or timeout.
    // If there is no data available we must return U_SOCK_EWOULDBLOCK
    int32_t tot = 0;
    int32_t res;
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if (pUWiFiSocket != NULL) {
        if (!pUWiFiSocket->dataAvailable) {
            return -U_SOCK_EWOULDBLOCK;
        } else {
            pUWiFiSocket->dataAvailable = false;
        }
        if (pRemoteAddress != NULL) {
            // Have to assume address set in SendTo.
            *pRemoteAddress = pUWiFiSocket->remoteAddress;
        }
    }
    int32_t startTimeMs = uPortGetTickTimeMs();
    while (((uPortGetTickTimeMs() - startTimeMs) < 5000) && (dataSizeBytes > 0) &&
           ((res = uWifiSockRead(devHandle, sockHandle, pData, dataSizeBytes)) >= 0)) {
        tot += res;
        dataSizeBytes -= res;
        pData += res;
        uPortTaskBlock(1);
    }
    return tot > 0 ? tot : -U_SOCK_EWOULDBLOCK;
}

int32_t uWifiSockRegisterCallbackData(uDeviceHandle_t devHandle,
                                      int32_t sockHandle,
                                      uWifiSockCallback_t pCallback)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if (pUWiFiSocket != NULL) {
        pUWiFiSocket->pDataCallback = pCallback;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiSockRegisterCallbackClosed(uDeviceHandle_t devHandle,
                                        int32_t sockHandle,
                                        uWifiSockCallback_t pCallback)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if (pUWiFiSocket != NULL) {
        pUWiFiSocket->pClosedCallback = pCallback;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiSockGetHostByName(uDeviceHandle_t devHandle,
                               const char *pHostName,
                               uSockIpAddress_t *pHostIpAddress)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxSocketGetHostByName(pUcxHandle, pHostName, pHostIpAddress);
    }
    return errorCode;
}

int32_t uWifiSockGetLocalAddress(uDeviceHandle_t devHandle,
                                 int32_t sockHandle,
                                 uSockAddress_t *pLocalAddress)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uCxWifiStationGetNetworkStatus_t resp;
        errorCode = uCxWifiStationGetNetworkStatus(pUcxHandle, 0, &resp);
        if (errorCode == 0) {
            pLocalAddress->ipAddress = resp.status_val;
            pLocalAddress->port = 0;
        }
    }
    return errorCode;
}

int32_t uWifiSockBind(uDeviceHandle_t devHandle, int32_t sockHandle,
                      const uSockAddress_t *pLocalAddress)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if (pUWiFiSocket != NULL) {
        pUWiFiSocket->localPort = pLocalAddress->port;
        errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return errorCode;
}

int32_t uWifiSockListen(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        size_t backlog)
{
    (void)backlog;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if ((pUcxHandle != NULL) && (pUWiFiSocket != NULL)) {
        errorCode = uCxSocketListen(pUcxHandle, pUWiFiSocket->uCxSockHandle,
                                    pUWiFiSocket->localPort);
    }
    return errorCode;
}

int32_t uWifiSockAccept(uDeviceHandle_t devHandle,
                        int32_t sockHandle,
                        uSockAddress_t *pRemoteAddress)
{
    (void)devHandle;
    int32_t errorCodeOrHandle = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uWifiSocket_t *pUWiFiSocket = getSocketByHandle(sockHandle);
    if (pUWiFiSocket != NULL) {
        errorCodeOrHandle =
            uPortSemaphoreTryTake(pUWiFiSocket->semaphore, gUWifiSocketAcceptTimeoutS * 1000);
        if (errorCodeOrHandle == 0) {
            *pRemoteAddress = pUWiFiSocket->remoteAddress;
            errorCodeOrHandle = pUWiFiSocket->remoteSockHandle;
        }
    }
    return errorCodeOrHandle;
}

// End of file
