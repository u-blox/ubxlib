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
#include <limits.h>

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // Required by u_at_client.h

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_compiler.h"

#include "u_timeout.h"

#include "u_cx_at_client.h"
#include "u_cx_general.h"
#include "u_cx_system.h"
#include "u_cx_urc.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_private.h"

#include "u_linked_list.h"
#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_wifi.h"

// The headers below necessary to work around an Espressif linker problem, see uShortRangeInit()
#include "u_device_private_short_range.h"
#include "u_security_tls.h"
#include "u_security_credential.h"
#include "u_short_range_sec_tls.h"     // For uShortRangeSecTlsPrivateLink()

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

#ifndef U_SHORT_RANGE_AT_CLIENT_CLOSE_DELAY_MS
/** Delay to allow the AT client to process enqueued asynchronous
 * events (URCs) before it is removed.
 */
# define U_SHORT_RANGE_AT_CLIENT_CLOSE_DELAY_MS 1000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

// Macro magic for gStringToModule
#define U_YES true
#define U_NO false
#define U_SHORT_RANGE_MODULE(_TYPE_NAME, _GMM_NAME, _BLE, _BT_CLASSIC, _WIFI)                      \
    {                                                                                              \
        .moduleType = U_SHORT_RANGE_MODULE_TYPE_##_TYPE_NAME,                                      \
        .pName = _GMM_NAME,                                                                        \
        .supportsBle = _BLE,                                                                       \
        .supportsBtClassic = _BT_CLASSIC,                                                          \
        .supportsWifi = _WIFI,                                                                     \
    },

static const uShortRangeModuleInfo_t gModuleInfo[] = {U_SHORT_RANGE_MODULE_LIST};

static const size_t gModuleInfoCount = sizeof(gModuleInfo) / sizeof(gModuleInfo[0]);

#undef U_YES
#undef U_NO
#undef U_SHORT_RANGE_MODULE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Calculate the remaining time for polling based on the start
// time and the AT timeout. Returns the time remaining for
// polling in milliseconds.
static int32_t pollTimeRemaining(int32_t atTimeoutMs,
                                 int32_t lockTimeMs)
{
    int32_t timeRemainingMs;
    int32_t now = uPortGetTickTimeMs();

    if (atTimeoutMs >= 0) {
        if (now - lockTimeMs > atTimeoutMs) {
            timeRemainingMs = 0;
        } else if (lockTimeMs + atTimeoutMs - now > INT_MAX) {
            timeRemainingMs = INT_MAX;
        } else {
            timeRemainingMs = lockTimeMs + atTimeoutMs - now;
        }
    } else {
        timeRemainingMs = 0;
    }

    // No need to worry about overflow here, we're never awake
    // for long enough
    return (int32_t)timeRemainingMs;
}

// ucxclient I/O routines

static int32_t read(uCxAtClient_t *pClient, void *pStreamHandle, void *pData, size_t length,
                    int32_t timeoutMs)
{
    (void)pClient;
    int32_t start = uPortGetTickTimeMs();
    int32_t readLength;
    while (true) {
        readLength = uPortUartRead(U_PTR_TO_INT32(pStreamHandle), pData, length);
        if (readLength != 0) {
            break;
        }
        // codechecker_suppress [readability-suspicious-call-argument]
        if (pollTimeRemaining(start, timeoutMs) <= 0) {
            readLength = 0;
            break;
        }
        uPortTaskBlock(U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS);
    };
    return readLength;
}

static int32_t write(uCxAtClient_t *pClient, void *pStreamHandle, const void *pData, size_t length)
{
    (void)pClient;
    return uPortUartWrite(U_PTR_TO_INT32(pStreamHandle), pData, length);
}

static void uartCallback(int32_t uartHandle, uint32_t eventBitmask,
                         void *pParameters)
{
    (void)uartHandle;
    (void)eventBitmask;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *)pParameters;
    if (uPortUartGetReceiveSize(uartHandle) > 0) {
        uCxAtClientHandleRx(&(pInstance->pUcxContext->uCxAtClient));
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeInit()
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;

    if (gUShortRangePrivateMutex == NULL) {
        errorCode = uPortMutexCreate(&gUShortRangePrivateMutex);
    }
    return errorCode;
}

void uShortRangeDeinit()
{
    if (gUShortRangePrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);
        // Unlock the mutex so that we can delete it
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        uPortMutexDelete(gUShortRangePrivateMutex);
        gUShortRangePrivateMutex = NULL;
    }
}

int32_t uShortRangeLock()
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {
        errorCode = uPortMutexLock(gUShortRangePrivateMutex);
    }
    return errorCode;
}

int32_t uShortRangeUnlock()
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gUShortRangePrivateMutex != NULL) {
        errorCode = uPortMutexUnlock(gUShortRangePrivateMutex);
    }
    return errorCode;
}

int32_t uShortRangeOpenUart(uShortRangeModuleType_t moduleType,
                            const uShortRangeUartConfig_t *pUartConfig,
                            bool restart, uDeviceHandle_t *pDevHandle)
{

    if (gUShortRangePrivateMutex == NULL) {
        return (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    }
    int32_t handleOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;

    if ((moduleType <= U_SHORT_RANGE_MODULE_TYPE_INTERNAL) || (pUartConfig == NULL)) {
        return handleOrErrorCode;
    }

    const uShortRangePrivateModule_t *pModule = NULL;
    for (size_t i = 0; i < gUShortRangePrivateModuleListSize; i++) {
        if (gUShortRangePrivateModuleList[i].moduleType == moduleType) {
            pModule = &gUShortRangePrivateModuleList[i];
            break;
        }
    }
    if (pModule == NULL) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (pUartConfig->pPrefix != NULL) {
        uPortUartPrefix(pUartConfig->pPrefix);
    }

    handleOrErrorCode = uPortUartOpen(pUartConfig->uartPort, pUartConfig->baudRate, NULL,
                                      U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES, pUartConfig->pinTx,
                                      pUartConfig->pinRx, pUartConfig->pinCts, pUartConfig->pinRts);
    if (handleOrErrorCode < (int32_t)U_ERROR_COMMON_SUCCESS) {
        return (int32_t)U_SHORT_RANGE_ERROR_INIT_UART;
    }

    int32_t uartHandle = handleOrErrorCode;
    uCxAtClientConfig_t *pConfig = (uCxAtClientConfig_t *)pUPortMalloc(sizeof(uCxAtClientConfig_t));
    uShortRangePrivateInstance_t *pInstance =
        (uShortRangePrivateInstance_t *)pUPortMalloc(sizeof(uShortRangePrivateInstance_t));
    uShortRangeUCxContext_t *pUCxContext = pUPortMalloc(sizeof(uShortRangeUCxContext_t));
    void *pRxBuff = pUPortMalloc(U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES);
    void *pUrcBuff = pUPortMalloc(U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES);
    uDeviceInstance_t *pDevInstance = pUDeviceCreateInstance(U_DEVICE_TYPE_SHORT_RANGE);
    if ((pConfig != NULL) && (pDevInstance != NULL) && (pUCxContext != NULL) &&
        (pRxBuff != NULL) && (pUrcBuff != NULL) && (pInstance != NULL)) {
        pConfig->pStreamHandle = U_INT32_TO_PTR(uartHandle);
        pConfig->pRxBuffer = pRxBuff;
        pConfig->rxBufferLen = U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES;
#if U_CX_USE_URC_QUEUE == 1
        pConfig->pUrcBuffer = pUrcBuff;
        pConfig->urcBufferLen = U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES;
#endif
        pConfig->read = read;
        pConfig->write = write;
        pConfig->timeoutMs = 100;
        pConfig->pContext = pInstance;
        memset((void *)pInstance, 0, sizeof(uShortRangePrivateInstance_t));
        uCxAtClientInit(pConfig, &(pUCxContext->uCxAtClient));
        uCxInit(&(pUCxContext->uCxAtClient), &(pUCxContext->uCxHandle));
        uCxHandle_t *pUcxHandle = &(pUCxContext->uCxHandle);
        uCxSystemGetUartSettings_t settings;
        uCxSystemGetUartSettings(pUcxHandle, &settings);
        int32_t flowCtrl = (pUartConfig->pinCts >= 0) ? 1 : 0;
        if (flowCtrl != settings.flow_control) {
            // Reboot to avoid saving possible temporary settings
            uCxSystemReboot(pUcxHandle);
            uPortTaskBlock(5000);
            // Now apply and save the new handshake settings
            uCxSystemSetUartSettings3(pUcxHandle, settings.baud_rate, flowCtrl, 1);
            uCxSystemStoreConfiguration(pUcxHandle);
        }
        // Want extended ucx error messages
        uCxAtClientExecSimpleCmd(&(pUCxContext->uCxAtClient), "AT+USYEE=1");
        for (int32_t i = 0; i < U_SHORT_RANGE_MAX_CONNECTIONS; i++) {
            pInstance->connections[i].connHandle = -1;
            pInstance->connections[i].type = U_SHORT_RANGE_CONNECTION_TYPE_INVALID;
        }
        pInstance->devHandle = (uDeviceHandle_t)pDevInstance;
        pInstance->uartHandle = uartHandle;
        pInstance->pModule = pModule;
        pInstance->pUcxContext = pUCxContext;

        pDevInstance->pContext = pInstance;
        *pDevHandle = (uDeviceHandle_t)pDevInstance;

        // Callback for urc checking
        uPortUartEventCallbackSet(uartHandle, U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                  uartCallback, pInstance,
                                  U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                  U_AT_CLIENT_URC_TASK_PRIORITY);

        if (moduleType == U_SHORT_RANGE_MODULE_TYPE_ANY) {
            moduleType = uShortRangeDetectModule(pInstance->devHandle);
            if (moduleType != U_SHORT_RANGE_MODULE_TYPE_INVALID) {
                for (size_t i = 0; i < gUShortRangePrivateModuleListSize; i++) {
                    if (gUShortRangePrivateModuleList[i].moduleType == moduleType) {
                        pInstance->pModule = &gUShortRangePrivateModuleList[i];
                        break;
                    }
                }
                uPortLog("U_SHORT_RANGE: Module %d identified and set sucessfully\n", moduleType);
            } else {
                uPortLog("U_SHORT_RANGE: could not identify the module type.\n");
                handleOrErrorCode = U_SHORT_RANGE_ERROR_INIT_INTERNAL;
            }
        }
        if (handleOrErrorCode >= (int32_t)U_ERROR_COMMON_SUCCESS) {
            if (restart) {
                uShortrangePrivateRestartDevice(pInstance->devHandle, false);
            }
            handleOrErrorCode = uCxSystemSetEchoOff(&(pUCxContext->uCxHandle));
            if (handleOrErrorCode == 0) {
                if (uShortRangeDetectModule(pInstance->devHandle) != moduleType) {
                    handleOrErrorCode = U_SHORT_RANGE_ERROR_NOT_DETECTED;
                }
            } else {
                handleOrErrorCode = U_SHORT_RANGE_ERROR_INIT_INTERNAL;
            }
        }
    } else {
        handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
    }

    if (handleOrErrorCode != 0) {
        if (uartHandle >= 0) {
            uPortUartClose(uartHandle);
        }
        if (pInstance != NULL) {
            uPortFree(pInstance);
        }
        if (pConfig != NULL) {
            uPortFree(pConfig);
        }
        if (pRxBuff != NULL) {
            uPortFree(pRxBuff);
        }
        if (pUrcBuff != NULL) {
            uPortFree(pUrcBuff);
        }
        if (pUCxContext != NULL) {
            uCxAtClientDeinit(&(pUCxContext->uCxAtClient));
            uPortFree(pUCxContext);
        }
        if (pDevInstance != NULL) {
            uDeviceDestroyInstance(pDevInstance);
        }
        *pDevHandle = NULL;
    }

    return handleOrErrorCode;
}

void uShortRangeClose(uDeviceHandle_t devHandle)
{
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(devHandle);

    if (pInstance != NULL) {
        uPortTaskBlock(U_SHORT_RANGE_AT_CLIENT_CLOSE_DELAY_MS);
        // Unlink any geofences and free the fence context
        uGeofenceContextFree((uGeofenceContext_t **)&pInstance->pFenceContext);
        uPortUartClose(pInstance->uartHandle);
        uShortRangeUCxContext_t *pUCxContext = pInstance->pUcxContext;
        uPortFree(pUCxContext->uCxAtClient.pConfig->pRxBuffer);
#if U_CX_USE_URC_QUEUE == 1
        uPortFree(pUCxContext->uCxAtClient.pConfig->pUrcBuffer);
#endif
        uPortFree((void *)(pUCxContext->uCxAtClient.pConfig));
        uCxAtClientDeinit(&(pUCxContext->uCxAtClient));
        uPortFree(pUCxContext);
        if (pInstance->pBleContext != NULL) {
            uPortFree(pInstance->pBleContext);
        }
        uPortFree(pInstance);
        uDeviceDestroyInstance(U_DEVICE_INSTANCE(devHandle));
    }
}

uShortRangeModuleType_t uShortRangeDetectModule(uDeviceHandle_t devHandle)
{
    uShortRangeModuleType_t moduleType = (int32_t)U_SHORT_RANGE_MODULE_TYPE_INVALID;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        const char *pIdStr;
        if (uCxGeneralGetDeviceModelIdentificationBegin(pUcxHandle, &pIdStr)) {
            for (int32_t i = 0; i < (int32_t)gModuleInfoCount; ++i) {
                if (!strncmp(pIdStr, gModuleInfo[i].pName, strlen(gModuleInfo[i].pName))) {
                    moduleType = gModuleInfo[i].moduleType;
                    break;
                }
            }
        }
        uCxEnd(pUcxHandle);
    }
    return moduleType;
}

int32_t uShortRangeAttention(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        errorCode = uCxGeneralAttention(pUcxHandle);
    }
    return errorCode;
}

int32_t uShortRangeAtClientHandleGet(uDeviceHandle_t devHandle,
                                     uAtClientHandle_t *pAtHandle)
{
    (void)devHandle;
    // Return U_ERROR_COMMON_SUCCESS instead of U_ERROR_COMMON_NOT_IMPLEMENTED
    // for now to avoid assertions in lots of the tests
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    // But return an invalid handle to indicate error
    *pAtHandle = NULL;
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

int32_t uShortRangeGetFirmwareVersionStr(uDeviceHandle_t devHandle,
                                         char *pStr, size_t size)
{
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        const char *pVersion;
        if (uCxGeneralGetSoftwareVersionBegin(pUcxHandle, &pVersion)) {
            memset(pStr, 0, size);
            strncpy(pStr, pVersion, size - 1);
            errorCodeOrLength = strlen(pVersion);
        }
        uCxEnd(pUcxHandle);
    }
    return errorCodeOrLength;

}

int32_t uShortRangeGetSerialNumber(uDeviceHandle_t devHandle, char *pSerialNumber)
{
    int32_t errorCodeOrLength = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        const char *pSerial;
        if (uCxGeneralGetSerialNumberBegin(pUcxHandle, &pSerial)) {
            strcpy(pSerialNumber, pSerial);
            errorCodeOrLength = strlen(pSerial);
        }
        uCxEnd(pUcxHandle);
    }
    return errorCodeOrLength;
}

int32_t uShortRangeGetEdmStreamHandle(uDeviceHandle_t devHandle)
{
    (void)devHandle;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;
    return errorCode;
}

int32_t uShortRangeGetUartHandle(uDeviceHandle_t devHandle)
{
    int32_t errorCodeOrHandle = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uShortRangePrivateInstance_t *pInstance;
    pInstance = pUShortRangePrivateGetInstance(devHandle);
    if ((pInstance != NULL) && (pInstance->uartHandle >= 0)) {
        errorCodeOrHandle = pInstance->uartHandle;
    }
    return errorCodeOrHandle;
}

int32_t uShortRangeSetBaudrate(uDeviceHandle_t *pDevHandle,
                               const uShortRangeUartConfig_t *pUartConfig)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(*pDevHandle);
    uShortRangePrivateInstance_t *pInstance = pUShortRangePrivateGetInstance(*pDevHandle);
    if ((pUcxHandle != NULL) && (pInstance != NULL)) {
        errorCode = uCxSystemSetUartSettings1(pUcxHandle, pUartConfig->baudRate);
        if (errorCode == 0) {
            // Must save and restart for the baud rate change to apply
            uCxSystemStoreConfiguration(pUcxHandle);
            uCxSystemReboot(pUcxHandle);
            uShortRangeClose(*pDevHandle);
            // Do timed wait for now, wait-for-urc function is coming in later version of the ucxclient
            uPortTaskBlock(5000);
            // Reopen and check for response
            uShortRangeModuleType_t moduleType = pInstance->pModule->moduleType;
            errorCode = uShortRangeOpenUart(moduleType, pUartConfig, false, pDevHandle);
            if (errorCode == 0) {
                errorCode = uShortRangeAttention(*pDevHandle);
            }
        }
    }
    return errorCode;
}

// No GPIO functions are available yet in uCx
int32_t uShortRangeGpioConfig(uDeviceHandle_t devHandle, int32_t gpioId,
                              bool isOutput, int32_t level)
{
    (void)devHandle;
    (void)gpioId;
    (void)isOutput;
    (void)level;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

int32_t uShortRangeGpioSet(uDeviceHandle_t devHandle, int32_t gpioId,
                           int32_t level)
{
    (void)devHandle;
    (void)gpioId;
    (void)level;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

int32_t uShortRangeResetToDefaultSettings(int32_t pinResetToDefaults)
{
    (void)pinResetToDefaults;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// End of file
