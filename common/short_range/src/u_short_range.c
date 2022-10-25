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
 * @brief Implementation of the "general" API for short range modules.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strtol(), atoi()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "stdio.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // Required by u_at_client.h

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

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

// Remove a short range instance from the list and free it.
// gUShortRangePrivateMutex should be locked before this is called.
static void removeShortRangeInstance(uShortRangePrivateInstance_t *pInstance)
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

    uPortFree(pInstance);
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

static int32_t uShortRangeAdd(uShortRangeModuleType_t moduleType,
                              uAtClientHandle_t atHandle,
                              int32_t uartHandle,
                              uDeviceHandle_t *pDevHandle)
{
    int32_t handleOrErrorCode;
    const uShortRangePrivateModule_t *pModule = NULL;
    uShortRangePrivateInstance_t *pInstance;
    uDeviceInstance_t *pDevInstance;

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
    pDevInstance = pUDeviceCreateInstance(U_DEVICE_TYPE_SHORT_RANGE);
    if (pDevInstance == NULL) {
        return (int32_t) U_ERROR_COMMON_NO_MEMORY;
    }

    // Check if there is already an instance for the AT client
    pInstance = pGetShortRangeInstanceAtHandle(atHandle);
    if (pInstance == NULL) {
        // Allocate memory for the instance
        pInstance = (uShortRangePrivateInstance_t *) pUPortMalloc(sizeof(uShortRangePrivateInstance_t));
        if (pInstance != NULL) {
            int32_t streamHandle;
            uAtClientStream_t streamType;
            // Fill the values in
            memset(pInstance, 0, sizeof(*pInstance));

            for (int32_t i = 0; i < U_SHORT_RANGE_MAX_CONNECTIONS; i++) {
                pInstance->connections[i].connHandle = -1;
                pInstance->connections[i].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
            }

            pInstance->atHandle = atHandle;
            pInstance->mode = U_SHORT_RANGE_MODE_EDM;
            pInstance->startTimeMs = 500;
            pInstance->urcConHandlerSet = false;
            pInstance->sockNextLocalPort = -1;
            pInstance->uartHandle = uartHandle;

            streamHandle = uAtClientStreamGet(atHandle, &streamType);
            pInstance->streamHandle = streamHandle;
            pInstance->streamType = streamType;

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
        handleOrErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        pDevInstance->pContext = (void *)pInstance;
        //lint -e740 Disable Unusual pointer cast
        *pDevHandle = (uDeviceHandle_t)pDevInstance;
    } else {
        handleOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    }

    return handleOrErrorCode;
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
                pInstance->pBtConnectionStatusCallback(pInstance->devHandle, connHandle,
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
                pInstance->pIpConnectionStatusCallback(pInstance->devHandle, connHandle,
                                                       U_SHORT_RANGE_EVENT_CONNECTED, &conData,
                                                       pInstance->pIpConnectionStatusCallbackParameter);
            }
        } else if (protocol == U_SHORT_RANGE_IP_PROTOCOL_MQTT) {
            pInstance->connections[id].type = U_SHORT_RANGE_CONNECTION_TYPE_MQTT;
            if (pInstance->pMqttConnectionStatusCallback != NULL) {
                pInstance->pMqttConnectionStatusCallback(pInstance->devHandle, connHandle,
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
                    pInstance->pBtConnectionStatusCallback(pInstance->devHandle, connHandle,
                                                           U_SHORT_RANGE_EVENT_DISCONNECTED, NULL,
                                                           pInstance->pBtConnectionStatusCallbackParameter);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_IP:
                if (pInstance->pIpConnectionStatusCallback != NULL) {
                    pInstance->pIpConnectionStatusCallback(pInstance->devHandle, connHandle,
                                                           U_SHORT_RANGE_EVENT_DISCONNECTED, NULL,
                                                           pInstance->pIpConnectionStatusCallbackParameter);
                }
                break;

            case U_SHORT_RANGE_CONNECTION_TYPE_MQTT:
                if (pInstance->pMqttConnectionStatusCallback != NULL) {
                    pInstance->pMqttConnectionStatusCallback(pInstance->devHandle, connHandle,
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

// Wrapper function to execute at commands in edm mode
static int32_t executeAtCommand(const uAtClientHandle_t atHandle, uint8_t retries,
                                const char *command)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;

    for (uint8_t i = 0; i < retries; i++) {
        uAtClientDeviceError_t deviceError;
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, 2000);
        uAtClientCommandStart(atHandle, command);
        uAtClientCommandStopReadResponse(atHandle);
        uAtClientDeviceErrorGet(atHandle, &deviceError);
        errorCode = uAtClientUnlock(atHandle);

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            break;
        }
    }

    return errorCode;
}

// Attempt to enter EDM mode
static int32_t enterEDM(const uShortRangePrivateInstance_t *pInstance)
{
    const char atCommandEnterEDM[] = "\r\nATO2\r\n";
    int32_t errorCode;
    //We assume first we are in at mode, send command blindly to enter EDM mode
    uShortRangeEdmStreamAtWrite(pInstance->streamHandle, atCommandEnterEDM, sizeof(atCommandEnterEDM));

    // Echo off
    errorCode = executeAtCommand(pInstance->atHandle, 4, "ATE0");

    return errorCode;
}

static int32_t restartModuleHelper(const uShortRangePrivateInstance_t *pInstance)
{
    int32_t errorCode;

    errorCode = enterEDM(pInstance);

    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        return errorCode;
    }

    // Reboot
    errorCode = executeAtCommand(pInstance->atHandle, 1, "AT+CPWROFF");

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        // Until we have proper startup detection just block the task a bit since module
        // module startup validation can take some time
        uPortTaskBlock(3500);
        errorCode = enterEDM(pInstance);
    }

    return errorCode;
}

// Reboot and enter edm
static int32_t restartModuleAndEnterEDM(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_UNKNOWN;
    uShortRangePrivateInstance_t *pInstance;

    pInstance = pUShortRangePrivateGetInstance(devHandle);

    if (pInstance != NULL) {
        errorCode = restartModuleHelper(pInstance);

        // Try to restart again
        if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
            errorCode = restartModuleHelper(pInstance);
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

int32_t uShortRangeOpenUart(uShortRangeModuleType_t moduleType,
                            const uShortRangeUartConfig_t *pUartConfig,
                            bool restart, uDeviceHandle_t *pDevHandle)
{
    int32_t uartHandle = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    int32_t edmStreamHandle = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    uAtClientHandle_t atClientHandle = NULL;
    int32_t handleOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    // TODO: once we allow multiple edm streams this should be removed
    if (gpUShortRangePrivateInstanceList != NULL) {
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_INTERNAL;
    }

    if ((moduleType <= U_SHORT_RANGE_MODULE_TYPE_INTERNAL) ||
        (pUartConfig == NULL)) {
        return handleOrErrorCode;
    }

    handleOrErrorCode = uPortUartOpen(pUartConfig->uartPort,
                                      pUartConfig->baudRate,
                                      NULL,
                                      U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES,
                                      pUartConfig->pinTx,
                                      pUartConfig->pinRx,
                                      pUartConfig->pinCts,
                                      pUartConfig->pinRts);

    if (handleOrErrorCode < (int32_t) U_ERROR_COMMON_SUCCESS) {
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_UART;
    }

    //lint -e(838) Suppress previously assigned value has not been used
    uartHandle = handleOrErrorCode;
    handleOrErrorCode = uShortRangeEdmStreamInit();

    if (handleOrErrorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        uPortUartClose(uartHandle);
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_EDM;
    }

    handleOrErrorCode = uShortRangeEdmStreamOpen(uartHandle);

    if (handleOrErrorCode < (int32_t) U_ERROR_COMMON_SUCCESS) {
        uShortRangeEdmStreamDeinit();
        uPortUartClose(uartHandle);
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_EDM;
    }

    //lint -e(838) Suppress previously assigned value has not been used
    edmStreamHandle = handleOrErrorCode;
    //lint -e(838) Suppress previously assigned value has not been used
    atClientHandle = uAtClientAdd(edmStreamHandle,
                                  U_AT_CLIENT_STREAM_TYPE_EDM,
                                  NULL,
                                  U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);

    if (atClientHandle == NULL) {
        uShortRangeEdmStreamClose(edmStreamHandle);
        uShortRangeEdmStreamDeinit();
        uPortUartClose(uartHandle);
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_ATCLIENT;
    }

    // Set printing/debugging of AT commands, the user
    // can always switch printing off in u_cfg_sw.h.
    uAtClientPrintAtSet(atClientHandle, true);
    uAtClientDebugSet(atClientHandle, true);

    handleOrErrorCode = uShortRangeAdd(moduleType,
                                       atClientHandle,
                                       uartHandle,
                                       pDevHandle);

    if (handleOrErrorCode < (int32_t) U_ERROR_COMMON_SUCCESS) {
        uAtClientRemove(atClientHandle);
        uShortRangeEdmStreamClose(edmStreamHandle);
        uShortRangeEdmStreamDeinit();
        uPortUartClose(uartHandle);
        return (int32_t) U_SHORT_RANGE_ERROR_INIT_INTERNAL;
    }

    uShortRangeEdmStreamSetAtHandle(edmStreamHandle, atClientHandle);

    if (restart) {
        if (restartModuleAndEnterEDM(*pDevHandle) != (int32_t) U_ERROR_COMMON_SUCCESS) {
            uShortRangeClose(*pDevHandle);
            return (int32_t)U_SHORT_RANGE_ERROR_INIT_INTERNAL;
        }
    } else {
        if (moduleType != uShortRangeDetectModule(*pDevHandle)) {
            // Failed - wait a bit and try once more
            uPortTaskBlock(100);
            if (moduleType != uShortRangeDetectModule(*pDevHandle)) {
                uShortRangeClose(*pDevHandle);
                return (int32_t)U_SHORT_RANGE_ERROR_INIT_INTERNAL;
            }
        }
    }

    if (moduleType != getModule(atClientHandle)) {
        uShortRangeClose(*pDevHandle);
        return (int32_t)U_SHORT_RANGE_ERROR_INIT_INTERNAL;
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

void uShortRangeClose(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex == NULL) {
        uPortLog("Failed to close short range, uShortRange is uninitialized\n");
        return;
    }

    pInstance = pUShortRangePrivateGetInstance(devHandle);

    if (pInstance != NULL) {
        uAtClientIgnoreAsync(pInstance->atHandle);
        uShortRangeEdmStreamClose(pInstance->streamHandle);
        uShortRangeEdmStreamDeinit();
        uAtClientRemoveUrcHandler(pInstance->atHandle, "+STARTUP");
        uAtClientRemove(pInstance->atHandle);
        uPortUartClose(pInstance->uartHandle);
        removeShortRangeInstance(pInstance);
        uDeviceDestroyInstance(U_DEVICE_INSTANCE(devHandle));
    }
}

int32_t uShortRangeSetIpConnectionStatusCallback(uDeviceHandle_t devHandle,
                                                 uShortRangeIpConnectionStatusCallback_t pCallback,
                                                 void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pCallback != NULL) {

                pInstance->pIpConnectionStatusCallback = pCallback;
                pInstance->pIpConnectionStatusCallbackParameter = pCallbackParameter;
            } else {
                pInstance->pIpConnectionStatusCallback = NULL;
                pInstance->pIpConnectionStatusCallbackParameter = NULL;
            }
            configureConnectionUrcHandlers(pInstance);
        }
    }

    return errorCode;
}

int32_t uShortRangeSetBtConnectionStatusCallback(uDeviceHandle_t devHandle,
                                                 uShortRangeBtConnectionStatusCallback_t pCallback,
                                                 void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pCallback != NULL) {
                pInstance->pBtConnectionStatusCallback = pCallback;
                pInstance->pBtConnectionStatusCallbackParameter = pCallbackParameter;
            } else {
                pInstance->pBtConnectionStatusCallback = NULL;
                pInstance->pBtConnectionStatusCallbackParameter = NULL;
            }
            configureConnectionUrcHandlers(pInstance);
        }
    }

    return errorCode;
}

int32_t uShortRangeSetMqttConnectionStatusCallback(uDeviceHandle_t devHandle,
                                                   uShortRangeIpConnectionStatusCallback_t pCallback,
                                                   void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pCallback != NULL) {

                pInstance->pMqttConnectionStatusCallback = pCallback;
                pInstance->pMqttConnectionStatusCallbackParameter = pCallbackParameter;
            } else {
                pInstance->pMqttConnectionStatusCallback = NULL;
                pInstance->pMqttConnectionStatusCallbackParameter = NULL;
            }
            configureConnectionUrcHandlers(pInstance);
        }
    }

    return errorCode;
}

uShortRangeModuleType_t uShortRangeDetectModule(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    uShortRangeModuleType_t module = U_SHORT_RANGE_MODULE_TYPE_INVALID;
    int32_t errorCode;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if (pInstance != NULL) {
            errorCode = enterEDM(pInstance);
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                module = getModule(pInstance->atHandle);
            }
        }
    }

    return module;
}

int32_t uShortRangeAttention(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
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

// Get the handle of the AT client.
int32_t uShortRangeAtClientHandleGet(uDeviceHandle_t devHandle,
                                     uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(devHandle);
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

int32_t uShortRangeGetSerialNumber(uDeviceHandle_t devHandle, char *pSerialNumber)
{
    uAtClientHandle_t atHandle;
    uShortRangePrivateInstance_t *pInstance;
    int32_t readBytes;
    int32_t err = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t retryCount;

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    pInstance = pUShortRangePrivateGetInstance(devHandle);

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
                err = readBytes;
                break;
            }
        }
    }

    return err;
}

int32_t uShortRangeGetEdmStreamHandle(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    pInstance = pUShortRangePrivateGetInstance(devHandle);

    if ((pInstance != NULL) &&
        (pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM)) {

        errorCode = pInstance->streamHandle;
    }

    return errorCode;
}

int32_t uShortRangeGetUartHandle(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    pInstance = pUShortRangePrivateGetInstance(devHandle);

    if ((pInstance != NULL) &&
        (pInstance->uartHandle >= (int32_t) U_ERROR_COMMON_SUCCESS)) {

        errorCode = pInstance->uartHandle;
    }

    return errorCode;
}

int32_t uShortRangeSetBaudrate(uDeviceHandle_t *pDevHandle,
                               const uShortRangeUartConfig_t *pUartConfig)
{
    uShortRangePrivateInstance_t *pInstance;
    uShortRangeModuleType_t moduleType;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
    char atBuffer[48];

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    }

    pInstance = pUShortRangePrivateGetInstance(*pDevHandle);

    if ((pInstance != NULL) &&
        (pInstance->atHandle != NULL)) {
        snprintf(atBuffer, sizeof(atBuffer), "AT+UMRS=%d,1,8,1,1", (int)pUartConfig->baudRate);
        errorCode = executeAtCommand(pInstance->atHandle, 1, atBuffer);

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // NINA-Bx requires a delay of 1 sec after changing baudrate
            uPortTaskBlock(1000);
            moduleType = pInstance->pModule->moduleType;
            uShortRangeClose(*pDevHandle);
            errorCode = uShortRangeOpenUart(moduleType, pUartConfig, false, pDevHandle);
        }
    }

    return errorCode;
}

// Configure GPIO
int32_t uShortRangeGpioConfig(uDeviceHandle_t devHandle, int32_t gpioId,
                              bool isOutput, int32_t level)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pInstance != NULL) && ((int32_t) gpioId >= 0)) {
            atHandle = pInstance->atHandle;
            uAtClientCommandStart(atHandle, "AT+UGPIOC=");
            // Write GPIO ID.
            uAtClientWriteInt(atHandle, (int32_t) gpioId);
            // Write GPIO direction.
            uAtClientWriteInt(atHandle, isOutput ? 0 : 1);
            if (isOutput) {
                // Write initial output value
                uAtClientWriteInt(atHandle, level);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Set GPIO
int32_t uShortRangeGpioSet(uDeviceHandle_t devHandle, int32_t gpioId,
                           int32_t level)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        pInstance = pUShortRangePrivateGetInstance(devHandle);
        if ((pInstance != NULL) && ((int32_t) gpioId >= 0)) {
            atHandle = pInstance->atHandle;

            uAtClientCommandStart(atHandle, "AT+UGPIOW=");
            // Write GPIO ID
            uAtClientWriteInt(atHandle, (int32_t) gpioId);
            // Write output level
            uAtClientWriteInt(atHandle, level);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

int32_t uShortRangeResetToDefaultSettings(int32_t pinResetToDefaults)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortGpioConfig_t gpioConfig;

    if (gUShortRangePrivateMutex == NULL) {
        return errorCode;
    }

    U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
    gpioConfig.pin = pinResetToDefaults;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    errorCode = uPortGpioConfig(&gpioConfig);
    uPortGpioSet(pinResetToDefaults, 0); //assert

    //initiate reset sequence
    if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
        uPortTaskBlock(1200); // 1s silence
        for (int32_t count = 0; count < 5; count++) { // 5 transfers from deassert to assert
            uPortTaskBlock(40);
            uPortLog("U_SHORT_RANGE: setting module DSR to state 1 (deasserted)...\n");
            uPortGpioSet(pinResetToDefaults, 1); //deassert
            uPortTaskBlock(40);
            uPortLog("U_SHORT_RANGE: setting module DSR to state 0 (asserted)...\n");
            uPortGpioSet(pinResetToDefaults, 0); //assert
        }
        uPortTaskBlock(1200); // 1s silence
    }
    return errorCode;
}

// End of file
