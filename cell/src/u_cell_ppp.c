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
 * @brief Implementation of the PPP interface for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strlen()
#include "stdio.h"     // snprintf()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"
#include "u_port_ppp.h"

#include "u_interface.h"
#include "u_ringbuffer.h"

#include "u_at_client.h"

#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_pwr.h"
#include "u_cell_pwr_private.h"
#include "u_cell_mux.h"
#include "u_cell_mux_private.h"

#include "u_cell_ppp_shared.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_PPP_DIAL_STRING
/** The string that performs a PPP dial-up, intended to have the.
 * %d and %s replaced with the PDP context ID and the AT command
 * send delimiter respectively.
 */
# define U_CELL_PPP_DIAL_STRING "ATD*99***%d#%s"
#endif

#ifndef U_CELL_PPP_DIAL_RESPONSE_STRING
/** The string that indicates PPP has connected, sent by the module
 * in response to #U_CELL_PPP_DIAL_STRING.
 * Note: deliberately omits the end as some modules (e.g. SARA-R4)
 * respond with things like "\r\n CONNECT 150000000\r\n".
 */
# define U_CELL_PPP_DIAL_RESPONSE_STRING "\r\nCONNECT"
#endif

#ifndef U_CELL_PPP_DIAL_RESPONSE_STRING_LENGTH
/** The length of #U_CELL_PPP_DIAL_RESPONSE_STRING, not including
 * the null terminator.
 */
# define U_CELL_PPP_DIAL_RESPONSE_STRING_LENGTH 9
#endif

#ifndef U_CELL_PPP_ERROR_STRING
/** The "ERROR" string on the PPP interface when operated in
 * command mode (i.e. an AT interface), may be sent at any time.
 */
# define U_CELL_PPP_ERROR_STRING "\r\nERROR\r\n"
#endif

#ifndef U_CELL_PPP_ERROR_STRING_LENGTH
/** The length of #U_CELL_PPP_ERROR_STRING.
 */
# define U_CELL_PPP_ERROR_STRING_LENGTH 9
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The context data for PPP operation.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    uDeviceSerial_t *pDeviceSerial;
    int32_t contextId;
    uPortPppReceiveCallback_t *pReceiveCallback;
    void *pReceiveCallbackParam;
    char *pReceiveBuffer;
    size_t receiveBufferSize;
    bool receiveBufferIsMalloced;
    bool muxAlreadyEnabled;
    bool uartSleepWakeOnDataWasEnabled;
} uCellPppContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** An LCP terminate request, used to shut down PPP.
 */
static const char gLcpTerminateReq[] = {0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21, 0x7d, 0x25,
                                        0x7d, 0x22, 0x7d, 0x20, 0x7d, 0x30, 0x55, 0x73,
                                        0x65, 0x72, 0x20, 0x72, 0x65, 0x71, 0x75, 0x65,
                                        0x73, 0x74, 0x53, 0x33, 0x7e
                                       };

/** The start, at least, of an LCP terminate ack.
 */
static const char gLcpTerminateAck[] = {0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21, 0x7d, 0x26};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Check if the given buffer contains the given buffer; returns
// the number of characters matched: if this is non-zero but less
// than the length of pBufferWanted then the caller should move
// *pStartMatchOffset characters out of the buffer but keep the rest
// and call this again with any additions.
static int32_t bufferContains(const char *pBuffer, size_t size,
                              const char *pBufferWanted, size_t bufferLength,
                              size_t *pStartMatchOffset)
{
    size_t count = 0;

    *pStartMatchOffset = 0;
    if (pBufferWanted != NULL) {
        for (size_t x = 0; (x < size) && (count < bufferLength); x++) {
            if (*pBuffer == *(pBufferWanted + count)) {
                count++;
            } else {
                count = 0;
                if (*pBuffer == *pBufferWanted) {
                    count = 1;
                } else {
                    (*pStartMatchOffset)++;
                }
            }
            pBuffer++;
        }
    }

    return count;
}

// Print out a buffer of sent or received characters nicely.
static void printBuffer(const char *pBuffer, size_t size)
{
    for (size_t x = 0; x < size; x++) {
        if (!isprint((int32_t) *pBuffer)) {
            // Print the hex
            uPortLog("[%02x]", (unsigned char) *pBuffer);
        } else {
            // Print the ASCII character
            uPortLog("%c", *pBuffer);
        }
        pBuffer++;
    }
}

// A very minimal AT send/receive function to avoid having
// to use the full AT parser on the PPP channel.
static int32_t sendExpect(uCellPppContext_t *pContext,
                          const char *pSend, size_t sendLength,
                          const char *pResponse, size_t responseLength,
                          bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle),
                          size_t timeoutSeconds)
{
    int32_t errorCode = 0;
    uDeviceSerial_t *pDeviceSerial = pContext->pDeviceSerial;
    uDeviceHandle_t cellHandle = pContext->cellHandle;
    char buffer[64];
    int32_t timeoutMs = timeoutSeconds * 1000;
    int32_t startTimeMs;
    int32_t x = 0;
    int32_t y = 0;
    size_t startMatchOffset;

    if (pSend != NULL) {
        errorCode = pDeviceSerial->write(pDeviceSerial, pSend, sendLength);
    }
    if (errorCode == sendLength) {
        if (sendLength > 0) {
            uPortLog("U_CELL_PPP: sent ");
            printBuffer(pSend, sendLength);
            uPortLog("\n");
        }
        if (pResponse != NULL) {
            // Wait for a response to come back
            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
            startTimeMs = uPortGetTickTimeMs();
            while ((errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                   (uPortGetTickTimeMs() - startTimeMs < timeoutMs) &&
                   ((pKeepGoingCallback == NULL) || (pKeepGoingCallback(cellHandle)))) {
                x = pDeviceSerial->read(pDeviceSerial, buffer + y, sizeof(buffer) - y);
                if (x > 0) {
                    x += y;
                    uPortLog("U_CELL_PPP: received ");
                    printBuffer(buffer, x);
                    uPortLog("\n");
                    startMatchOffset = 0;
                    y = bufferContains(buffer, x, pResponse,
                                       responseLength,
                                       &startMatchOffset);
                    if (y == (int32_t) responseLength) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else if (y == 0) {
                        startMatchOffset = 0;
                        y = bufferContains(buffer, x, U_CELL_PPP_ERROR_STRING,
                                           U_CELL_PPP_ERROR_STRING_LENGTH,
                                           &startMatchOffset);
                        if (y == U_CELL_PPP_ERROR_STRING_LENGTH) {
                            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                        }
                    }
                    // Keep startMatchOffset characters, in case there was
                    // a partial match, moved down to the start of the buffer
                    memmove(buffer, buffer + startMatchOffset,
                            sizeof(buffer) - startMatchOffset);
                } else {
                    // Wait a little while for more to arrive
                    uPortTaskBlock(100);
                }
            }
        }
    } else {
        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    }

    return errorCode;
}

// Make the PPP connection over the AT interface.
static int32_t connectPpp(uCellPppContext_t *pContext,
                          bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    char buffer[16]; // Enough room for "ATD*99***x#\r"
    int32_t x;

    x = snprintf(buffer, sizeof(buffer), U_CELL_PPP_DIAL_STRING,
                 (int) pContext->contextId, U_AT_CLIENT_COMMAND_DELIMITER);
    if (x < sizeof(buffer)) {
        errorCode = sendExpect(pContext, buffer, x, U_CELL_PPP_DIAL_RESPONSE_STRING,
                               U_CELL_PPP_DIAL_RESPONSE_STRING_LENGTH, pKeepGoingCallback,
                               U_CELL_PPP_DIAL_TIMEOUT_SECONDS);
    }

    return errorCode;
}

// Close the PPP interface.
static void closePpp(uCellPrivateInstance_t *pInstance,
                     bool pppTerminateRequired)
{
    uCellPppContext_t *pContext;
    uDeviceSerial_t *pDeviceSerial;

    if (pInstance != NULL) {
        pContext = (uCellPppContext_t *) pInstance->pPppContext;
        // Note: we don't free the context or any allocated receive
        // buffer to ensure thread-safety of the callback
        if (pContext != NULL) {
            if (pContext->pDeviceSerial != NULL) {
                pDeviceSerial = pContext->pDeviceSerial;
                if (pppTerminateRequired) {
                    // Remove the callback so that we get the
                    // responses from now on
                    pDeviceSerial->eventCallbackRemove(pDeviceSerial);
                    // Send an LCP message which should terminate the
                    // PPP link
                    if (sendExpect(pContext, gLcpTerminateReq, sizeof(gLcpTerminateReq),
                                   gLcpTerminateAck, sizeof(gLcpTerminateAck),
                                   NULL, U_CELL_PPP_HANG_UP_TIMEOUT_SECONDS) != 0) {
                        uPortLog("U_CELL_PPP: *** WARNING *** PPP did not terminate, it may"
                                 " not connect next time, you may wish to reboot the module.\n");
                        pInstance->rebootIsRequired = true;
                    }
                }
                // Remove the multiplexer channel
                uCellMuxPrivateCloseChannel((uCellMuxPrivateContext_t *) pInstance->pMuxContext,
                                            U_CELL_MUX_PRIVATE_CHANNEL_ID_PPP);
                pContext->pDeviceSerial = NULL;
            }
            if (!pContext->muxAlreadyEnabled) {
                // Disable the multiplexer if one was in use
                // and it was us who started it
                uCellMuxPrivateDisable(pInstance);
            }
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_U201) {
                // SARA-U201 needs a little rest just here
                // or the next AT command may stall
                uPortTaskBlock(1000);
            }
            // Re-enable UART sleep if we had switched it off
            if (pContext->uartSleepWakeOnDataWasEnabled) {
                uCellPwrPrivateEnableUartSleep(pInstance);
            }
        }
    }
}

// Callback for data received over the PPP CMUX channel.
static void callback(uDeviceSerial_t *pDeviceSerial,
                     uint32_t eventBitmask, void *pParameters)
{
    uCellPppContext_t *pContext = (uCellPppContext_t *) pParameters;
    int32_t bytesRead;

    if ((eventBitmask & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) &&
        (pContext != NULL) && (pContext->pReceiveBuffer != NULL)) {
        bytesRead = pDeviceSerial->read(pDeviceSerial,
                                        pContext->pReceiveBuffer,
                                        pContext->receiveBufferSize);
        if ((bytesRead > 0) && (pContext->pReceiveCallback != NULL)) {
            pContext->pReceiveCallback(pContext->cellHandle,
                                       pContext->pReceiveBuffer, bytesRead,
                                       pContext->pReceiveCallbackParam);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO CELLULAR
 * -------------------------------------------------------------- */

// Free context.
void uCellPppPrivateRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellPppContext_t *pContext;

    if ((pInstance != NULL) && (pInstance->pPppContext != NULL)) {
        closePpp(pInstance, false);
        pContext = (uCellPppContext_t *) pInstance->pPppContext;
        if (pContext->receiveBufferIsMalloced) {
            // The receive buffer was malloc'ed, free it now
            uPortFree(pContext->pReceiveBuffer);
        }
        uPortFree(pContext);
        pInstance->pPppContext = NULL;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open the PPP interface of a cellular module.
int32_t uCellPppOpen(uDeviceHandle_t cellHandle,
                     uPortPppReceiveCallback_t *pReceiveCallback,
                     void *pReceiveCallbackParam,
                     char *pReceiveData, size_t receiveDataSize,
                     bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPppContext_t *pContext;
    uDeviceSerial_t *pDeviceSerial;
    bool pppTerminateRequired = false;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_PPP)) {
                // No point even trying if we're not on the network
                errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
                if (uCellPrivateIsRegistered(pInstance)) {
                    pContext = (uCellPppContext_t *) pInstance->pPppContext;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    if (pContext == NULL) {
                        // Allocate memory for the context
                        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        pContext = (uCellPppContext_t *) pUPortMalloc(sizeof(*pContext));
                        if (pContext != NULL) {
                            memset(pContext, 0, sizeof(*pContext));
                            pContext->cellHandle = cellHandle;
                            pContext->contextId = U_CELL_NET_CONTEXT_ID;
                            if (pInstance->pModule->pppContextId >= 0) {
                                pContext->contextId = pInstance->pModule->pppContextId;
                            }
                        }
                    }
                    if ((pContext != NULL) && (pContext->pDeviceSerial == NULL)) {
                        // Have a context and the serial device for PPP is not set up
                        if (pContext->receiveBufferIsMalloced) {
                            // There is a previously malloc'ed buffer: free it now
                            uPortFree(pContext->pReceiveBuffer);
                            pContext->receiveBufferIsMalloced = false;
                        }
                        pContext->pReceiveBuffer = pReceiveData;
                        pContext->receiveBufferSize = receiveDataSize;
                        if ((pReceiveCallback != NULL) && (pContext->pReceiveBuffer == NULL)) {
                            // Allocate memory for the receive data buffer
                            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                            pContext->pReceiveBuffer = (char *) pUPortMalloc(receiveDataSize);
                            if (pContext->pReceiveBuffer != NULL) {
                                pContext->receiveBufferIsMalloced = true;
                            }
                        }
                        if ((pContext != NULL) &&
                            ((pReceiveCallback == NULL) || (pContext->pReceiveBuffer != NULL))) {
                            // Have a context and either don't need a receive buffer or we have one
                            pInstance->pPppContext = pContext;
                            pContext->pReceiveCallback = pReceiveCallback;
                            pContext->pReceiveCallbackParam = pReceiveCallbackParam;
                            // Determine if CMUX and "wake-up on UART data line" UART power saving
                            // are already enabled
                            pContext->muxAlreadyEnabled = uCellMuxPrivateIsEnabled(pInstance);
                            pContext->uartSleepWakeOnDataWasEnabled = uCellPwrPrivateUartSleepIsEnabled(pInstance);
                            if (uCellPwrPrivateGetDtrPowerSavingPin(pInstance) >= 0) {
                                pContext->uartSleepWakeOnDataWasEnabled = false;
                            }
                            // Enable CMUX
                            errorCode = uCellMuxPrivateEnable(pInstance);
                            if (errorCode == 0) {
                                // Add the PPP channel
                                errorCode = uCellMuxPrivateAddChannel(pInstance,
                                                                      U_CELL_MUX_PRIVATE_CHANNEL_ID_PPP,
                                                                      &(pContext->pDeviceSerial));
                            }
                            if (errorCode == 0) {
                                // If we're on wake-up-on-data UART power saving and CMUX, switch
                                // UART power saving off, just in case
                                errorCode = uCellPwrPrivateDisableUartSleep(pInstance);
                            }
                            if (errorCode == 0) {
                                pDeviceSerial = pContext->pDeviceSerial;
                                // We now have a second serial interface to
                                // the module: do a PPP dial-up on it.  Could
                                // attach an AT handler to it but that would be
                                // an overhead in terms of RAM that we can do
                                // without, instead just send the dial-up string
                                // and wait for the response
                                uPortTaskBlock(1000);
                                errorCode = connectPpp(pContext, pKeepGoingCallback);
                                if ((errorCode == 0) && (pReceiveCallback != NULL)) {
                                    pppTerminateRequired = (errorCode == 0);
                                    // Note: the priority and stack size parameters
                                    // to eventCallbackSet() are ignored, hence use of -1
                                    errorCode = pDeviceSerial->eventCallbackSet(pDeviceSerial,
                                                                                U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED,
                                                                                callback, pContext,
                                                                                -1, -1);
                                }
                            }
                            if (errorCode < 0) {
                                // Tidy up on error
                                closePpp(pInstance, pppTerminateRequired);
                            }
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Determine if PPP is up and running.
bool uCellPppIsOpen(uDeviceHandle_t cellHandle)
{
    bool isRunning = false;
    uCellPrivateInstance_t *pInstance;
    uCellPppContext_t *pContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pPppContext != NULL)) {
            pContext = (uCellPppContext_t *) pInstance->pPppContext;
            isRunning = pContext->pDeviceSerial != NULL;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isRunning;
}

// Close the PPP interface of a cellular module.
int32_t uCellPppClose(uDeviceHandle_t cellHandle,
                      bool pppTerminateRequired)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_PPP)) {
                closePpp(pInstance, pppTerminateRequired);
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Transmit a buffer of data over the PPP interface.
int32_t uCellPppTransmit(uDeviceHandle_t cellHandle,
                         const char *pData, size_t dataSize)
{
    int32_t errorCodeOrBytesSent = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPppContext_t *pContext;
    uDeviceSerial_t *pDeviceSerial;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCodeOrBytesSent = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrBytesSent = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_PPP)) {
                errorCodeOrBytesSent = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                pContext = (uCellPppContext_t *) pInstance->pPppContext;
                if ((pContext != NULL) && (pContext->pDeviceSerial != NULL)) {
                    pDeviceSerial = pContext->pDeviceSerial;
                    errorCodeOrBytesSent = pDeviceSerial->write(pDeviceSerial,
                                                                pData, dataSize);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrBytesSent;
}

// Free memory.
void uCellPppFree(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                       U_CELL_PRIVATE_FEATURE_PPP))) {
            uCellPppPrivateRemoveContext(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

// End of file
