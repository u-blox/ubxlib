/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the "general" API for short range modules.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // Required by u_at_client.h

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

#include "u_network_handle.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SHORT_RANGE_UART_READ_BUFFER
# define U_SHORT_RANGE_UART_READ_BUFFER 1000
#endif

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The next instance handle to use.
 */
static int32_t gNextInstanceHandle = 0;

// Macro magic for gStringToModule
#define U_YES true
#define U_NO  false
#define U_SHORT_RANGE_MODULE(_TYPE_NAME, _GMM_NAME, _BLE, _BT_CLASSIC, _WIFI) \
    { \
        .moduleType = U_SHORT_RANGE_MODULE_TYPE_##_TYPE_NAME, \
        .pName = _GMM_NAME, \
        .supportsBle = _BLE, \
        .supportsBtClassic = _BT_CLASSIC, \
        .supportsWifi = _WIFI, \
    },

static const uShortRangeModuleInfo_t gModuleInfo[] = {
    U_SHORT_RANGE_MODULE_LIST
};

static const size_t gModuleInfoCount = sizeof(gModuleInfo) / sizeof(gModuleInfo[0]);

#undef U_YES
#undef U_NO
#undef U_SHORT_RANGE_MODULE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a connHandle, or use -1 for a free spot
static int32_t findFreeConnection(const uShortRangePrivateInstance_t *pInstance, int32_t connHandle)
{
    int32_t i;

    for (i = 0; i < U_SHORT_RANGE_MAX_CONNECTIONS; i++) {
        if (pInstance->connections[i].connHandle == connHandle) {
            break;
        }
    }

    if (i == U_SHORT_RANGE_MAX_CONNECTIONS) {
        i = -1;
    }

    return i;
}

// Find a short range instance in the list by AT handle.
// gUShortRangePrivateMutex should be locked before this is called.
//lint -e{818} suppress "could be declared as pointing to const": atHandle is anonymous
static uShortRangePrivateInstance_t *pGetShortRangeInstanceAtHandle(uAtClientHandle_t atHandle)
{
    uShortRangePrivateInstance_t *pInstance = gpUShortRangePrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->atHandle != atHandle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Add a short range instance to the list.
// gUShortRangePrivateMutex should be locked before this is called.
// Note: doesn't copy it, just adds it.
static void addShortRangeInstance(uShortRangePrivateInstance_t *pInstance)
{
    pInstance->pNext = gpUShortRangePrivateInstanceList;
    gpUShortRangePrivateInstanceList = pInstance;
}

// Remove a short range instance from the list.
// gUShortRangePrivateMutex should be locked before this is called.
// Note: doesn't free it, the caller must do that.
static void removeShortRangeInstance(const uShortRangePrivateInstance_t *pInstance)
{
    uShortRangePrivateInstance_t *pCurrent;
    uShortRangePrivateInstance_t *pPrev = NULL;

    pCurrent = gpUShortRangePrivateInstanceList;
    while (pCurrent != NULL) {
        if (pInstance == pCurrent) {
            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                gpUShortRangePrivateInstanceList = pCurrent->pNext;
            }
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
}

//lint -e{818} suppress "could be declared as pointing to const": it is!
static void restarted(const uAtClientHandle_t atHandle,
                      void *pParameter)
{
    (void)atHandle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    pInstance->ticksLastRestart = uPortGetTickTimeMs();
    uPortLog("U_SHORT_RANGE: module restart detected\n");
}

static void dataCallback(int32_t streamHandle, uint32_t eventBitmask,
                         void *pParameters)
{
    (void)streamHandle;
    int32_t sizeOrError;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameters;

    size_t read = 0;

    if (eventBitmask == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        do {
            sizeOrError = uPortUartRead(pInstance->streamHandle,
                                        pInstance->pBuffer + read,
                                        U_SHORT_RANGE_UART_READ_BUFFER - read);
            if (sizeOrError > 0) {
                read += sizeOrError;
            }
        } while ((sizeOrError > 0) && (read <= U_SHORT_RANGE_UART_READ_BUFFER));

        if (pInstance->pDataCallback) {
            pInstance->pDataCallback(-1, read, pInstance->pBuffer, pInstance->pDataCallbackParameter);
        }
    }
}

static int32_t parseUudpcProtocol(int32_t value, uShortRangeIpProtocol_t *pProtocol)
{
    switch (value) {
        case 0:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_TCP;
            break;
        case 1:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_UDP;
            break;
        case 6:
            *pProtocol = U_SHORT_RANGE_IP_PROTOCOL_MQTT;
            break;
        default:
            return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

static int32_t parseUudpcProfile(int32_t value, uShortRangeBtProfile_t *pProfile)
{
    switch (value) {
        case 1:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_SPP;
            break;
        case 2:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_DUN;
            break;
        case 4:
            *pProfile = U_SHORT_RANGE_BT_PROFILE_SPS;
            break;
        default:
            return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

static int32_t parseBdAddr(const char *pStr, uint8_t *pDstAddr)
{
    // Parse string: "01A0F7101C08p"

    // Basic validation
    if ((strlen(pStr) != 13) || ((pStr[12] != 'r') && (pStr[12] != 'p'))) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    for (int i = 0; i < U_SHORT_RANGE_BT_ADDRESS_LENGTH; i++) {
        char buf[3];
//lint -save -e679
        memcpy(&buf[0], &pStr[i * 2], 2);
//lint -restore
        buf[2] = 0;
        pDstAddr[i] = (uint8_t)strtol(buf, NULL, 16);
    }

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}


static int32_t parseIpv4Addr(char *pStr, uint8_t *pDstIp)
{
    // Parse string: "192.168.0.1"
    char *end = pStr + strlen(pStr);
    for (int i = 0; i < 4; i++) {
        if (pStr >= end) {
            return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        }
        char *tok = strchr(pStr, '.');
        if (i < 3) {
            if (tok == NULL) {
                // Missing a '.'
                return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            }
            tok[0] = 0;
            pDstIp[i] = (uint8_t)atoi(pStr);
            pStr = &tok[1];
        } else {
            pDstIp[i] = (uint8_t)atoi(pStr);
        }
    }

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

static int32_t parseIpv6Addr(char *pStr, uint8_t *pDstIp)
{
    // Parse string: "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"

    // Basic validation
    if ((strlen(pStr) != 41) || (pStr[0] != '[') || (pStr[40] != ']')) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    char *ptr = &pStr[1];
    for (int i = 0; i < 8; i++) {
        // Validate separator
        if ((i < 7) && (ptr[4] != ':')) {
            return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        }
        ptr[4] = 0;
        uint32_t value = (uint32_t)strtol(ptr, NULL, 16);
//lint -save -e679
        pDstIp[i * 2] = (uint8_t)(value >> 8);
        pDstIp[(i * 2) + 1] = (uint8_t)(value & 0xFF);
//lint -restore
        ptr += 5;
    }

    return (int32_t)U_ERROR_COMMON_SUCCESS;
}

static int32_t parseUint16(int32_t value, uint16_t *pDst)
{
    if ((value > 0) && (value <= 0xFFFF)) {
        *pDst = (uint16_t)value;
        return (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    return (int32_t)U_ERROR_COMMON_UNKNOWN;
}


//+UUDPC:<peer_handle>,<type>,<profile>,<address>,<frame_size>
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUDPC_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    int32_t connHandle;
    int32_t type;

    connHandle = uAtClientReadInt(atHandle);
    type = uAtClientReadInt(atHandle);

    int32_t id = findFreeConnection(pInstance, -1);
    if (id < 0) {
        uPortLog("U_SHORT_RANGE: Out of connection entries\n");
        return;
    }

    pInstance->connections[id].connHandle = connHandle;
    // The type will be filled in next section, but default to "invalid"
    pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;

    if (type == U_SHORT_RANGE_UUDPC_TYPE_BT) {
        int32_t err = 0;
        char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];

        pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_BT;

        int profile = uAtClientReadInt(atHandle);
        (void)uAtClientReadString(atHandle, address, U_SHORT_RANGE_BT_ADDRESS_SIZE, false);
        int frameSize = uAtClientReadInt(atHandle);

        if (pInstance->pBtConnectionStatusCallback != NULL) {
            uShortRangeConnectDataBt_t conData;
            err |= parseUint16(frameSize, &conData.framesize);
            err |= parseBdAddr(address, conData.address);
            err |= parseUudpcProfile(profile, &conData.profile);
            if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                pInstance->pBtConnectionStatusCallback(pInstance->handle, connHandle,
                                                       U_SHORT_RANGE_EVENT_CONNECTED,
                                                       &conData,
                                                       pInstance->pBtConnectionStatusCallbackParameter);
            }
        }
    } else if (type == U_SHORT_RANGE_UUDPC_TYPE_IPv4 ||
               type == U_SHORT_RANGE_UUDPC_TYPE_IPv6) {
        int32_t err = 0;
        char buffer[64];
        uShortRangeConnectDataIp_t conData;
        uShortRangeIpProtocol_t protocol = U_SHORT_RANGE_IP_PROTOCOL_TCP; // Keep compiler happy

        if (type == U_SHORT_RANGE_UUDPC_TYPE_IPv4) {
            // Parse remaining IPv4 data:
            //  <protocol>,<local_ip>,<local_port>,<remote_ip>,<remote_port>
            // Example: "0,192.168.0.40,54282,142.250.74.100,80"
            conData.type = U_SHORT_RANGE_CONNECTION_IPv4;
            // Protocol
            int value = uAtClientReadInt(atHandle);
            err |= parseUudpcProtocol(value, &protocol);
            conData.ipv4.protocol = protocol;
            // Local IP
            err |= uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            err |= parseIpv4Addr(buffer, conData.ipv4.localAddress);
            // Local port
            value = uAtClientReadInt(atHandle);
            err |= parseUint16(value, &conData.ipv4.localPort);
            // Remote IP
            err |= uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            err |= parseIpv4Addr(buffer, conData.ipv4.remoteAddress);
            // Remote port
            value = uAtClientReadInt(atHandle);
            err |= parseUint16(value, &conData.ipv4.remotePort);
        } else {
            // Parse remaining IPv6 data:
            //  <protocol>,<local_ip>,<local_port>,<remote_ip>,<remote_port>
            // Example: "0,[2001:0db8:85a3:0000:0000:8a2e:0370:7334],54282,[2001:0db8:85a3:0000:0000:8a2e:0370:7334],80"
            conData.type = U_SHORT_RANGE_CONNECTION_IPv6;
            int value = uAtClientReadInt(atHandle);
            err |= parseUudpcProtocol(value, &protocol);
            conData.ipv6.protocol = protocol;
            // Local IP
            err |= uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            err |= parseIpv6Addr(buffer, conData.ipv6.localAddress);
            // Local port
            value = uAtClientReadInt(atHandle);
            err |= parseUint16(value, &conData.ipv6.localPort);
            // Remote IP
            err |= uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            err |= parseIpv6Addr(buffer, conData.ipv6.remoteAddress);
            // Remote port
            value = uAtClientReadInt(atHandle);
            err |= parseUint16(value, &conData.ipv6.remotePort);
        }

        if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
            // There was an error in the parsing
            uPortLog("U_SHORT_RANGE: Unable to parse UUDPC URC\n");
            return;
        }

        if ((protocol == U_SHORT_RANGE_IP_PROTOCOL_TCP) ||
            (protocol == U_SHORT_RANGE_IP_PROTOCOL_UDP)) {
            pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_IP;
            if (pInstance->pIpConnectionStatusCallback != NULL) {
                pInstance->pIpConnectionStatusCallback(pInstance->handle, connHandle,
                                                       U_SHORT_RANGE_EVENT_CONNECTED, &conData,
                                                       pInstance->pIpConnectionStatusCallbackParameter);
            }
        } else if (protocol == U_SHORT_RANGE_IP_PROTOCOL_MQTT) {
            pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_MQTT;
            if (pInstance->pMqttConnectionStatusCallback != NULL) {
                pInstance->pMqttConnectionStatusCallback(pInstance->handle, connHandle,
                                                         U_SHORT_RANGE_EVENT_CONNECTED, &conData,
                                                         pInstance->pMqttConnectionStatusCallbackParameter);
            }
        }
    }
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUDPD_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    int32_t connHandle;

    connHandle = uAtClientReadInt(atHandle);

    int32_t id = findFreeConnection(pInstance, connHandle);

    if (id != -1) {
        switch (pInstance->connections[id].type) {
            case U_SHORT_RANGE_CONNECTION_TYPE_BT:
                if (pInstance->pBtConnectionStatusCallback != NULL) {
                    pInstance->pBtConnectionStatusCallback(pInstance->handle, connHandle,
                                                           U_SHORT_RANGE_EVENT_DISCONNECTED, NULL,
                                                           pInstance->pBtConnectionStatusCallbackParameter);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_IP:
                if (pInstance->pIpConnectionStatusCallback != NULL) {
                    pInstance->pIpConnectionStatusCallback(pInstance->handle, connHandle,
                                                           U_SHORT_RANGE_EVENT_DISCONNECTED, NULL,
                                                           pInstance->pIpConnectionStatusCallbackParameter);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_MQTT:
                if (pInstance->pMqttConnectionStatusCallback != NULL) {
                    pInstance->pMqttConnectionStatusCallback(pInstance->handle, connHandle,
                                                             U_SHORT_RANGE_EVENT_DISCONNECTED, NULL,
                                                             pInstance->pMqttConnectionStatusCallbackParameter);
                }
                break;

            default:
                break;
        }

        pInstance->connections[id].connHandle = -1;
        pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
    }
}

static void exitDataMode(const uShortRangePrivateInstance_t *pInstance)
{
    const char escSeq[3] = {'+', '+', '+'};

    uPortTaskBlock(1100);
    if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
        if (uShortRangeEdmStreamAtWrite(pInstance->streamHandle, escSeq, 3) != 3) {
            uPortTaskBlock(1100);
            uShortRangeEdmStreamAtWrite(pInstance->streamHandle, escSeq, 3);
        }
    } else if (pInstance->mode == U_SHORT_RANGE_MODE_DATA) {
        if (uPortUartWrite(pInstance->streamHandle, escSeq, 3) != 3) {
            uPortTaskBlock(1100);
            uPortUartWrite(pInstance->streamHandle, escSeq, 3);
        }
    }

    uPortTaskBlock(1100);
}

static int32_t setEchoOff(const uAtClientHandle_t atHandle, uint8_t retries)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;

    for (uint8_t i = 0; i < retries; i++) {
        uAtClientDeviceError_t deviceError;
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, 2000);
        uAtClientCommandStart(atHandle, "ATE0");
        uAtClientCommandStopReadResponse(atHandle);
        uAtClientDeviceErrorGet(atHandle, &deviceError);
        errorCode = uAtClientUnlock(atHandle);

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            break;
        }
    }

    return errorCode;
}

static int32_t convert(const char *pStr)
{
    for (int32_t i = 0;  i < (int32_t)gModuleInfoCount;  ++i) {
        if (!strncmp (pStr, gModuleInfo[i].pName, strlen(gModuleInfo[i].pName))) {
            return gModuleInfo[i].moduleType;
        }
    }
    return U_SHORT_RANGE_MODULE_TYPE_INVALID;
}

static uShortRangeModuleType_t getModule(const uAtClientHandle_t atHandle)
{
    uShortRangeModuleType_t module = U_SHORT_RANGE_MODULE_TYPE_INVALID;

    char buffer[20];
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+GMM");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, NULL);
    int32_t bytesRead = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
    uAtClientResponseStop(atHandle);
    int32_t errorCode = uAtClientUnlock(atHandle);

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS &&
        bytesRead >= 7) {
        module = convert(buffer);
    }

    return module;
}

// This function is called whenever a connection callback is set or cleared
// The function will check if we need to set or clear the URC connection handlers
static void configureConnectionUrcHandlers(uShortRangePrivateInstance_t *pInstance)
{
    bool connectionCallbackSet =
        ((pInstance->pBtConnectionStatusCallback != NULL) ||
         (pInstance->pIpConnectionStatusCallback != NULL) ||
         (pInstance->pMqttConnectionStatusCallback != NULL));

    if (connectionCallbackSet && !pInstance->urcConHandlerSet) {
        uAtClientSetUrcHandler(pInstance->atHandle, "+UUDPC:",
                               UUDPC_urc, pInstance);
        uAtClientSetUrcHandler(pInstance->atHandle, "+UUDPD:",
                               UUDPD_urc, pInstance);
        pInstance->urcConHandlerSet = true;
    } else if (!connectionCallbackSet && pInstance->urcConHandlerSet) {
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUDPC:");
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUDPD:");
        pInstance->urcConHandlerSet = false;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gUShortRangePrivateMutex == NULL) {
        // Create the mutex that protects the linked list
        errorCode = uPortMutexCreate(&gUShortRangePrivateMutex);
    }

    return errorCode;
}

void uShortRangeDeinit()
{
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        // Remove all short range instances
        while (gpUShortRangePrivateInstanceList != NULL) {
            pInstance = gpUShortRangePrivateInstanceList;
            removeShortRangeInstance(pInstance);
            free(pInstance);
        }

        // Unlock the mutex so that we can delete it
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        uPortMutexDelete(gUShortRangePrivateMutex);
        gUShortRangePrivateMutex = NULL;
    }
}

int32_t uShortRangeLock()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = uPortMutexLock(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeUnlock()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = uPortMutexUnlock(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeAdd(uShortRangeModuleType_t moduleType,
                       uAtClientHandle_t atHandle)
{
    int32_t handleOrErrorCode;
    const uShortRangePrivateModule_t *pModule = NULL;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    // Check parameters
    for (size_t i = 0; i < gUShortRangePrivateModuleListSize; i++) {
        if (gUShortRangePrivateModuleList[i].moduleType == moduleType) {
            pModule = &gUShortRangePrivateModuleList[i];
            break;
        }
    }
    if ((uShortRangeGetModuleInfo(moduleType) == NULL) ||
        (atHandle == NULL) || (pModule == NULL)) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    // Check if there is already an instance for the AT client
    pInstance = pGetShortRangeInstanceAtHandle(atHandle);
    if (pInstance == NULL) {
        // Allocate memory for the instance
        pInstance = (uShortRangePrivateInstance_t *) malloc(sizeof(uShortRangePrivateInstance_t));
        if (pInstance != NULL) {
            int32_t streamHandle;
            uAtClientStream_t streamType;
            // Fill the values in
            memset(pInstance, 0, sizeof(*pInstance));
            // Find a free handle
            do {
                pInstance->handle = gNextInstanceHandle;
                gNextInstanceHandle++;
                if (gNextInstanceHandle > (int32_t) U_NETWORK_HANDLE_RANGE) {
                    gNextInstanceHandle = 0;
                }
            } while (pUShortRangePrivateGetInstance(pInstance->handle) != NULL);

            for (int32_t i = 0; i < U_SHORT_RANGE_MAX_CONNECTIONS; i++) {
                pInstance->connections[i].connHandle = -1;
                pInstance->connections[i].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
            }

            pInstance->atHandle = atHandle;
            pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;
            pInstance->startTimeMs = 500;
            pInstance->urcConHandlerSet = false;

            streamHandle = uAtClientStreamGet(atHandle, &streamType);
            pInstance->streamHandle = streamHandle;
            pInstance->streamType = streamType;

            if (pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM) {
                pInstance->mode = U_SHORT_RANGE_MODE_EDM;
            }

            pInstance->pModule = pModule;
            pInstance->pNext = NULL;

            uAtClientTimeoutSet(atHandle, pInstance->pModule->atTimeoutSeconds * 1000);
            uAtClientDelaySet(atHandle, pInstance->pModule->commandDelayMs);
            // ...and finally add it to the list
            addShortRangeInstance(pInstance);

            uAtClientSetUrcHandler(atHandle, "+STARTUP",
                                   restarted, pInstance);
            pInstance->ticksLastRestart = 0;
        }
    }

    if (pInstance) {
        pInstance->refCounter++;
        handleOrErrorCode = pInstance->handle;
    } else {
        handleOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    }

    return handleOrErrorCode;
}

void uShortRangeRemove(int32_t shortRangeHandle)
{
    uShortRangePrivateInstance_t *pInstance;

    if (shortRangeHandle != -1 && gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        // There may be several users of the instance so check the reference counter
        if ((pInstance != NULL) && (--pInstance->refCounter <= 0)) {
            removeShortRangeInstance(pInstance);
            uAtClientRemoveUrcHandler(pInstance->atHandle, "+STARTUP");
            free(pInstance);
        }
    }
}

int32_t uShortRangeSetIpConnectionStatusCallback(int32_t shortRangeHandle,
                                                 uShortRangeIpConnectionStatusCallback_t pCallback,
                                                 void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL && pCallback != NULL) {
            pInstance->pIpConnectionStatusCallback = pCallback;
            pInstance->pIpConnectionStatusCallbackParameter = pCallbackParameter;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else if (pInstance != NULL && pCallback == NULL) {
            pInstance->pIpConnectionStatusCallback = NULL;
            pInstance->pIpConnectionStatusCallbackParameter = NULL;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
        configureConnectionUrcHandlers(pInstance);
    }

    return errorCode;
}

int32_t uShortRangeSetBtConnectionStatusCallback(int32_t shortRangeHandle,
                                                 uShortRangeBtConnectionStatusCallback_t pCallback,
                                                 void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL && pCallback != NULL) {
            pInstance->pBtConnectionStatusCallback = pCallback;
            pInstance->pBtConnectionStatusCallbackParameter = pCallbackParameter;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else if (pInstance != NULL && pCallback == NULL) {
            pInstance->pBtConnectionStatusCallback = NULL;
            pInstance->pBtConnectionStatusCallbackParameter = NULL;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
        configureConnectionUrcHandlers(pInstance);
    }

    return errorCode;
}

int32_t uShortRangeSetMqttConnectionStatusCallback(int32_t shortRangeHandle,
                                                   uShortRangeIpConnectionStatusCallback_t pCallback,
                                                   void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL && pCallback != NULL) {
            pInstance->pMqttConnectionStatusCallback = pCallback;
            pInstance->pMqttConnectionStatusCallbackParameter = pCallbackParameter;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else if (pInstance != NULL && pCallback == NULL) {
            pInstance->pMqttConnectionStatusCallback = NULL;
            pInstance->pMqttConnectionStatusCallbackParameter = NULL;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
        configureConnectionUrcHandlers(pInstance);
    }

    return errorCode;
}

uShortRangeModuleType_t uShortRangeDetectModule(int32_t shortRangeHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    uShortRangeModuleType_t module = U_SHORT_RANGE_MODULE_TYPE_INVALID;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        if (pInstance != NULL) {
            int32_t errorCode;
            // If we think we are in data mode, leave it
            if (pInstance->mode == U_SHORT_RANGE_MODE_DATA) {
                exitDataMode(pInstance);
                pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;
            }

            // Sometimes the UART (both in and outgoing) needs to be cleaned
            // from garbage or stray characters so test twice;
            // Use ATE0 as we want the echo off anyway and it is a OK/ERROR only
            // response command.
            errorCode = setEchoOff(pInstance->atHandle, 2);

            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS &&
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                const char atCommand[] = "\r\nATO2\r\n";
                uShortRangeEdmStreamAtWrite(pInstance->streamHandle, atCommand, sizeof(atCommand));
                uPortTaskBlock(60);
                errorCode = setEchoOff(pInstance->atHandle, 2);

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    exitDataMode(pInstance);
                    // This is edm handler, not uart, need that somehow
                    uShortRangeEdmStreamAtWrite(pInstance->streamHandle, atCommand, sizeof(atCommand));
                    uPortTaskBlock(60);
                }

                errorCode = setEchoOff(pInstance->atHandle, 2);
            }

            // We want to be in command mode but the module might be in EDM mode, the
            // only way to exit is to restart the module. This will send the binary packet that
            // executes "AT+CPWROFF" on the module as a raw write on the UART. Response is
            // ignored and if it works it will result in a startup URC (also ignored here, but might
            // trigger something in user code).
            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS &&
                pInstance->mode == U_SHORT_RANGE_MODE_COMMAND) {
                const unsigned char atCommandRestart[] = { 0xAA, 0x00, 0x0D, 0x00, 0x44, 0x41, 0x54,
                                                           0x2B, 0x43, 0x50, 0x57, 0x52, 0x4F, 0x46,
                                                           0x46, 0x0D, 0x55
                                                         };
                const unsigned char atCommandReset[] = { 0xAA, 0x00, 0x0E, 0x00, 0x44, 0x41, 0x54,
                                                         0x2B, 0x55, 0x46, 0x41, 0x43, 0x54, 0x4F,
                                                         0x52, 0x59, 0x0D, 0x55
                                                       };
                uPortUartWrite(pInstance->streamHandle, atCommandRestart, sizeof(atCommandRestart));

                int64_t ticks = uPortGetTickTimeMs();

                while ((ticks > pInstance->ticksLastRestart) &&
                       (ticks + 2000 > uPortGetTickTimeMs())) {
                    uPortTaskBlock(100);
                }
                errorCode = setEchoOff(pInstance->atHandle, 2);

                // We want to be in command mode but the module might start up in data mode.
                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    exitDataMode(pInstance);
                    errorCode = setEchoOff(pInstance->atHandle, 2);
                }

                // Last resort, module is in EDM with startup mode EDM. As the setting and what is expected
                // are that off we just do a raw favtory reset of the module.
                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    uPortUartWrite(pInstance->streamHandle, atCommandReset, sizeof(atCommandReset));
                    uPortTaskBlock(50);
                    uPortUartWrite(pInstance->streamHandle, atCommandRestart, sizeof(atCommandRestart));

                    ticks = uPortGetTickTimeMs();

                    while ((ticks > pInstance->ticksLastRestart) ||
                           (ticks + 2000 > uPortGetTickTimeMs())) {
                        uPortTaskBlock(100);
                    }
                    errorCode = setEchoOff(pInstance->atHandle, 2);
                }
            }

            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                module = getModule(pInstance->atHandle);
            }
        }
    }

    return module;
}

int32_t uShortRangeAttention(int32_t shortRangeHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Sending AT\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }
    }

    return errorCode;
}

int32_t uShortRangeDataMode(int32_t shortRangeHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND) {
                atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Goto Data mode\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "ATO1");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);

                if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    uAtClientRemove(pInstance->atHandle);
                    pInstance->pBuffer = (char *)malloc(U_SHORT_RANGE_UART_READ_BUFFER);
                    errorCode = uPortUartEventCallbackSet(pInstance->streamHandle,
                                                          U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                          dataCallback, (void *) pInstance,
                                                          U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                                          U_AT_CLIENT_URC_TASK_PRIORITY);
                    if (errorCode !=  (int32_t)U_ERROR_COMMON_SUCCESS) {
                        free(pInstance->pBuffer);
                    } else {
                        pInstance->mode = U_SHORT_RANGE_MODE_DATA;
                    }
                }
            }
        }
    }

    return errorCode;
}

int32_t uShortRangeCommandMode(int32_t shortRangeHandle, uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            const char escSeq[3] = {'+', '+', '+'};

            uPortTaskBlock(1100);
            if (uPortUartWrite(pInstance->streamHandle, escSeq, 3) == 3) {
                uPortTaskBlock(1100);

                uPortUartEventCallbackRemove(pInstance->streamHandle);

                pInstance->atHandle = uAtClientAdd(pInstance->streamHandle, U_AT_CLIENT_STREAM_TYPE_UART,
                                                   NULL, U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
                *pAtHandle = pInstance->atHandle;
                atHandle = pInstance->atHandle;
                pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }
    }

    return errorCode;
}

// Get the handle of the AT client.
int32_t uShortRangeAtClientHandleGet(int32_t shortRangeHandle,
                                     uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        if ((pInstance != NULL) && (pAtHandle != NULL)) {
            *pAtHandle = pInstance->atHandle;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

const uShortRangeModuleInfo_t *uShortRangeGetModuleInfo(int32_t moduleType)
{
    for (int32_t i = 0; i < (int32_t)gModuleInfoCount; i++) {
        if (gModuleInfo[i].moduleType == moduleType) {
            return &gModuleInfo[i];
        }
    }
    return NULL;
}

int32_t uShortRangeGetShoHandle(int32_t networkHandle)
{
    int32_t shoHandle = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (U_NETWORK_HANDLE_IS_WIFI(networkHandle)) {

        if ((networkHandle >= (int32_t)U_NETWORK_HANDLE_WIFI_MIN) &&
            (networkHandle <= (int32_t)U_NETWORK_HANDLE_WIFI_MAX)) {

            shoHandle =  networkHandle - (int32_t)U_NETWORK_HANDLE_WIFI_MIN;

        }

    } else if (U_NETWORK_HANDLE_IS_BLE(networkHandle)) {

        if ((networkHandle >= (int32_t)U_NETWORK_HANDLE_BLE_MIN) &&
            (networkHandle <= (int32_t)U_NETWORK_HANDLE_BLE_MAX)) {

            shoHandle =  networkHandle - (int32_t)U_NETWORK_HANDLE_BLE_MIN;
        }

    }
    return shoHandle;
}


int32_t uShortRangeGetSerialNumber(int32_t shortRangeHandle, char *pSerialNumber)
{
    uAtClientHandle_t atHandle;
    uShortRangePrivateInstance_t *pInstance;
    int32_t readBytes;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t retryCount;

    pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);

    if ((pInstance != NULL) &&
        (pSerialNumber != NULL)) {

        atHandle = pInstance->atHandle;

        for (retryCount = 0; retryCount < 3; retryCount++) {
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CGSN");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, NULL);
            readBytes = uAtClientReadBytes(atHandle, pSerialNumber,
                                           U_SHORT_RANGE_SERIAL_NUMBER_LENGTH, false);
            uAtClientResponseStop(atHandle);
            err = uAtClientUnlock(atHandle);

            if (err == (int32_t)U_ERROR_COMMON_SUCCESS) {
                pSerialNumber[readBytes] = '\0';
                break;
            }
        }
    }

    return err;
}
// End of file
