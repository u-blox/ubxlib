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
 * @brief Wrapped UART creation function.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_interface.h"
#include "u_device.h"

#include "u_port.h"
#include "u_port_uart.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Context data required if we are to create a virtual version
 * of a physical serial port.
 */
typedef struct {
    int32_t uartHandle;
    uDeviceCfgUart_t cfgUart;
    // These so that we can use the uPortUartEventCallback via trampoline()
    struct uDeviceSerial_t *pDeviceSerial;
    void (*pEventCallback)(struct uDeviceSerial_t *pDeviceSerial, uint32_t eventBitMap, void *pParam);
    void *pEventCallbackParam;
} uDeviceSerialWrappedUartContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: IMPLEMENTATIONS OF THE SERIAL DEVICE FUNCTIONS
 * -------------------------------------------------------------- */

// Trampoline so that the function signature that uPortUartEventCallbackSet()
// uses (int32_t handle, uint32_t eventBitMap, void *pParam) can be employed
// with that which serialEventCallbackSet() uses (struct uDeviceSerial_t *pHandle,
// uint32 eventBitMap, void *pParam).
static void trampoline(int32_t handle, uint32_t eventBitMap, void *pParam)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *) pParam;

    (void) handle;

    if ((pContext != NULL) && (pContext->pEventCallback != NULL) &&
        (pContext->pDeviceSerial != NULL)) {
        pContext->pEventCallback(pContext->pDeviceSerial, eventBitMap,
                                 pContext->pEventCallbackParam);
    }
}

// Open a virtual serial device, mapped to a real one.
static int32_t serialWrappedUartOpen(struct uDeviceSerial_t *pDeviceSerial,
                                     void *pReceiveBuffer, size_t receiveBufferSizeBytes)
{
    int32_t errorCode;
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    uDeviceCfgUart_t *pCfgUart = &(pContext->cfgUart);

    if (pCfgUart->pPrefix != NULL) {
        uPortUartPrefix(pCfgUart->pPrefix);
    }
    errorCode = uPortUartOpen(pCfgUart->uart, pCfgUart->baudRate,
                              pReceiveBuffer, receiveBufferSizeBytes,
                              pCfgUart->pinTxd, pCfgUart->pinRxd,
                              pCfgUart->pinCts, pCfgUart->pinRts);
    if (errorCode >= 0) {
        pContext->uartHandle = errorCode;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Close a virtual serial device, mapped to a real one.
static void serialWrappedUartClose(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    uPortUartClose(pContext->uartHandle);
}

// Get the number of bytes waiting in the receive buffer of a
// real serial device.
static int32_t serialWrappedUartGetReceiveSize(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartGetReceiveSize(pContext->uartHandle);
}

// Read from the given virtual serial device, mapped to a real one.
static int32_t serialWrappedUartRead(struct uDeviceSerial_t *pDeviceSerial,
                                     void *pBuffer, size_t sizeBytes)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartRead(pContext->uartHandle, pBuffer, sizeBytes);
}

// Write to the given virtual serial device, mapped to a real one.
static int32_t serialWrappedUartWrite(struct uDeviceSerial_t *pDeviceSerial,
                                      const void *pBuffer, size_t sizeBytes)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartWrite(pContext->uartHandle, pBuffer, sizeBytes);
}

// Set an event callback on the virtual serial device, mapped to a real one.
static int32_t serialWrappedUartEventCallbackSet(struct uDeviceSerial_t *pDeviceSerial,
                                                 uint32_t filter,
                                                 void (*pFunction)(struct uDeviceSerial_t *,
                                                                   uint32_t,
                                                                   void *),
                                                 void *pParam,
                                                 size_t stackSizeBytes,
                                                 int32_t priority)
{
    int32_t errorCode;
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);

    pContext->pEventCallback = pFunction;
    pContext->pEventCallbackParam = pParam;
    errorCode = uPortUartEventCallbackSet(pContext->uartHandle, filter,
                                          trampoline, pContext, stackSizeBytes,
                                          priority);
    if (errorCode != 0) {
        // Tidy up on error
        pContext->pEventCallback = NULL;
        pContext->pEventCallbackParam = NULL;
    }

    return errorCode;
}

// Remove a wrapped UART serial event callback.
static void serialWrappedUartEventCallbackRemove(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    uPortUartEventCallbackRemove(pContext->uartHandle);
    pContext->pEventCallback = NULL;
    pContext->pEventCallbackParam = NULL;
}

// Get the serial event callback filter bit-mask for a wrapped UART serial device.
static uint32_t serialWrappedUartEventCallbackFilterGet(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventCallbackFilterGet(pContext->uartHandle);
}

// Change the serial event callback filter bit-mask for a wrapped UART serial device.
static int32_t serialWrappedUartEventCallbackFilterSet(struct uDeviceSerial_t *pDeviceSerial,
                                                       uint32_t filter)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventCallbackFilterSet(pContext->uartHandle, filter);
}

// Send a UART event to a wrapped UART serial device.
static int32_t serialWrappedUartEventSend(struct uDeviceSerial_t *pDeviceSerial,
                                          uint32_t eventBitMap)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventSend(pContext->uartHandle, eventBitMap);
}

// Try to send a UART event to a wrapped UART serial device.
static int32_t serialWrappedUartEventTrySend(struct uDeviceSerial_t *pDeviceSerial,
                                             uint32_t eventBitMap, int32_t delayMs)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventTrySend(pContext->uartHandle, eventBitMap, delayMs);
}

// Determine if we are in the event callback task of a wrapped UART serial device.
static bool serialWrappedUartEventIsCallback(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventIsCallback(pContext->uartHandle);
}

// Get the minimum free stack of the callback of a wrapped UART serial device.
static int32_t serialWrappedUartEventStackMinFree(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartEventStackMinFree(pContext->uartHandle);
}

// Determine if RTS flow control is enabled on a wrapped UART serial device.
static bool serialWrappedUartIsRtsFlowControlEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartIsRtsFlowControlEnabled(pContext->uartHandle);
}

// Determine if CTS flow control is enabled on a wrapped UART serial device.
static bool serialWrappedUartIsCtsFlowControlEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartIsCtsFlowControlEnabled(pContext->uartHandle);
}

// Suspend CTS for a wrapped UART serial device.
static int32_t serialWrappedUartCtsSuspend(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    return uPortUartCtsSuspend(pContext->uartHandle);
}

// Resume CTS for a wrapped UART serial device.
static void serialWrappedUartCtsResume(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);
    uPortUartCtsResume(pContext->uartHandle);
}

// Set or clear discard on flow control for a wrapped UART serial device.
static int32_t serialWrappedUartDiscardOnOverflow(struct uDeviceSerial_t *pDeviceSerial,
                                                  bool onNotOff)
{
    // Not supported on a physical UART
    (void) pDeviceSerial;
    (void) onNotOff;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Determine if discard on flow control is enabled for a wrapped UART serial device.
static bool serialWrappedUartIsDiscardOnOverflowEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    // Never suppoted on a physical UART
    (void) pDeviceSerial;
    return false;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: INITIALISATION
 * -------------------------------------------------------------- */

// Initialisation callback for pUInterfaceCreate().
static void init(uInterfaceTable_t pInterfaceTable, void *pInitParam)
{
    uDeviceSerial_t *pDeviceSerial = (uDeviceSerial_t *) pInterfaceTable;
    uDeviceSerialWrappedUartContext_t *pContext = (uDeviceSerialWrappedUartContext_t *)
                                                  pUInterfaceContext(pDeviceSerial);

    pDeviceSerial->open = serialWrappedUartOpen;
    pDeviceSerial->close = serialWrappedUartClose;
    pDeviceSerial->getReceiveSize = serialWrappedUartGetReceiveSize;
    pDeviceSerial->read = serialWrappedUartRead;
    pDeviceSerial->write = serialWrappedUartWrite;
    pDeviceSerial->eventCallbackSet = serialWrappedUartEventCallbackSet;
    pDeviceSerial->eventCallbackRemove = serialWrappedUartEventCallbackRemove;
    pDeviceSerial->eventCallbackFilterGet = serialWrappedUartEventCallbackFilterGet;
    pDeviceSerial->eventCallbackFilterSet = serialWrappedUartEventCallbackFilterSet;
    pDeviceSerial->eventSend = serialWrappedUartEventSend;
    pDeviceSerial->eventTrySend = serialWrappedUartEventTrySend;
    pDeviceSerial->eventIsCallback = serialWrappedUartEventIsCallback;
    pDeviceSerial->eventStackMinFree = serialWrappedUartEventStackMinFree;
    pDeviceSerial->isRtsFlowControlEnabled = serialWrappedUartIsRtsFlowControlEnabled;
    pDeviceSerial->isCtsFlowControlEnabled = serialWrappedUartIsCtsFlowControlEnabled;
    pDeviceSerial->ctsSuspend = serialWrappedUartCtsSuspend;
    pDeviceSerial->ctsResume = serialWrappedUartCtsResume;
    pDeviceSerial->discardOnOverflow = serialWrappedUartDiscardOnOverflow;
    pDeviceSerial->isDiscardOnOverflowEnabled = serialWrappedUartIsDiscardOnOverflowEnabled;

    *pContext = *((uDeviceSerialWrappedUartContext_t *) pInitParam);
    pContext->pDeviceSerial = pDeviceSerial;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Create a serial interface that wraps a physical UART.
uDeviceSerial_t *pDeviceSerialCreateWrappedUart(const uDeviceCfgUart_t *pCfgUart)
{
    uDeviceSerial_t *pDeviceSerial = NULL;
    uDeviceSerialWrappedUartContext_t context = {0};

    if (pCfgUart != NULL) {
        context.cfgUart = *pCfgUart;
        context.uartHandle = -1;
        pDeviceSerial = (uDeviceSerial_t *) pUInterfaceCreate(sizeof(uDeviceSerial_t),
                                                              sizeof(uDeviceSerialWrappedUartContext_t),
                                                              U_DEVICE_SERIAL_VERSION,
                                                              init, (void *) &context, NULL);
    }

    return pDeviceSerial;
}

// End of file
