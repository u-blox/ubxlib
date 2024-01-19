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
 * @brief Implementation of the functions that enable and disable
 * 3GPP 27.010 CMUX operation.
 */

/* DESIGN NOTE: the data flow goes something like this:
 *
 * 1.  CMUX-multiplexed frames are read from the UART into a
 *     control buffer.
 * 2.  This data is also pushed into a ring buffer.
 * 3.  With one control buffer's worth of data read, the control
 *     buffer is parsed for CMUX frames on channel 0: this is so
 *     that any flow control information is handled independently
 *     of the user data.
 * 4.  Then the ring-buffer is parsed for non-channel-0 [i.e. user]
 *     CMUX frames and the information fields of these frames are
 *     copied into the data buffers of the individual channels.  If
 *     there is no room for the information-field data in the buffers
 *     then, assuming that CTS flow control is NOT enabled (if it is
 *     enabled then any overflow-data is simply discarded), a "stall" is
 *     indicated; the data is left in the ring-buffer and the far end
 *     is sent a flow-control-off.
 * 5.  When user data is read from the virtual serial port, if we had
 *     flow-controlled-off the far end then it is flow-controlled-on
 *     again and decoding of any existing data in the buffers is
 *     re-triggered.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()
#include "ctype.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_YIELD_MS

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_port_heap.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"

#include "u_interface.h"
#include "u_ringbuffer.h"

#include "u_at_client.h"

#include "u_device_shared.h"

#include "u_gnss_shared.h" // For uGnssUpdateAtHandle()

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_mux.h"
#include "u_cell_mux_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_MUX_SABM_TIMEOUT_MS
/** How long to wait for SABM to be agreed with the module (i.e. for
 * UA to come back for it).
 */
# define U_CELL_MUX_SABM_TIMEOUT_MS 5000
#endif

#ifndef U_CELL_MUX_DISC_TIMEOUT_MS
/** How long to wait for DISC to be agreed with the module (i.e. for
 * UA or DM to come back for it).
 */
# define U_CELL_MUX_DISC_TIMEOUT_MS 5000
#endif

#ifndef U_CELL_MUX_WRITE_TIMEOUT_MS
/** Guard time for writes to a CMUX channel: just re-use the guard
 * time for writing to a UART port.
  */
# define U_CELL_MUX_WRITE_TIMEOUT_MS U_PORT_UART_WRITE_TIMEOUT_MS
#endif

/** Macro to check that a CMUX channel is open.
 */
#define U_CELL_MUX_IS_OPEN(state) ((state) == U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN)

#ifndef U_CELL_MUX_SHORT_INFO_LENGTH_BYTES
/** The short information field carried around by uCellMuxUserFrame_t.
 */
# define U_CELL_MUX_SHORT_INFO_LENGTH_BYTES 10
#endif

#ifndef U_CELL_MUX_CALLBACK_TASK_STACK_SIZE_BYTES
/** The stack size for the task in which any serial device
 * callbacks are triggered: use the AT client URC task stack
 * size as that is most definitely going to be one of the callees.
 */
# define U_CELL_MUX_CALLBACK_TASK_STACK_SIZE_BYTES U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES
#endif

#ifndef U_CELL_MUX_CALLBACK_TASK_PRIORITY
/** The priority of the task in which any callbacks triggered via
 * the serial devices will run: the same as the AT client URC callback.
 */
# define U_CELL_MUX_CALLBACK_TASK_PRIORITY U_AT_CLIENT_URC_TASK_PRIORITY
#endif

#ifndef U_CELL_MUX_CALLBACK_QUEUE_LENGTH
/** The maximum length of the common callback queue for the serial devices.
 * Each item in the queue will be sizeof(uCellMuxEventTrampoline_t) bytes big.
 */
# define U_CELL_MUX_CALLBACK_QUEUE_LENGTH 20
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of the "user" parts of a CMUX frame with a short
 * information field, used by sendCommandCheckResponse().
 */
typedef struct {
    uCellMuxPrivateFrameType_t type;
    char information[U_CELL_MUX_SHORT_INFO_LENGTH_BYTES];
    size_t informationLengthBytes;
} uCellMuxUserFrame_t;

/** Structure to hold a serial event callback on the event queue.
 */
typedef struct {
    uCellMuxPrivateContext_t *pContext;
    int32_t channel;
    uint32_t eventBitMap;
} uCellMuxEventTrampoline_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The close-down command, sent in the information field of a UIH
 * frame.
  */
static const char gMuxCldCommand[] = {0xc3, 0x01};

/** The close-down response, received in the information field of
 a UIH frame.
  */
static const char gMuxCldResponse[] = {0xc1, 0x01};

/** A multiplexer frame which a module will determine as "close
 * the multiplexer", can be sent to the module if it is thought
 * to not be sending normal AT commands because it is actually
 * in multiplexer mode.
 */
static const char gMuxCldCommandFrame[] = {0xf9, 0x03, 0xff, 0x05, 0xc3, 0x01, 0xe7, 0xf9};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: HELPER FUNCTIONS FOR VIRTUAL SERIAL PORT
 * -------------------------------------------------------------- */

// Event handler, common to all virtual serial ports.
static void eventHandler(void *pParam, size_t paramLength)
{
    uCellMuxEventTrampoline_t *pEventTrampoline = (uCellMuxEventTrampoline_t *) pParam;
    uCellMuxPrivateContext_t *pContext = pEventTrampoline->pContext;
    uDeviceSerial_t *pDeviceSerial;
    uCellMuxPrivateChannelContext_t *pChannelContext;
    uCellMuxPrivateEventCallback_t *pEventCallback;

    (void) paramLength;

    // It is deliberate that this function re-derives everything from the
    // main context since only the main context can be guaranteed to be still
    // around when this event eventually occurs
    if (pContext != NULL) {
        pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, (uint8_t) pEventTrampoline->channel);
        if (pDeviceSerial != NULL) {
            pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
            if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {
                pEventCallback = &(pChannelContext->eventCallback);
                if (pEventCallback->pFunction != NULL) {
                    pEventCallback->pFunction(pDeviceSerial,
                                              pEventTrampoline->eventBitMap,
                                              pEventCallback->pParam);
                }
            }
        }
    }
}

// Send an event, either through manual triggering of the serial device
// or through new data having arrived.  Set delayMs to less than zero for
// a normal send, zero or more for a try send (where supported).
static int32_t sendEvent(uCellMuxPrivateContext_t *pContext,
                         uCellMuxPrivateChannelContext_t *pChannelContext,
                         int32_t eventBitMap, int32_t delayMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateEventCallback_t *pEventCallback;
    uCellMuxEventTrampoline_t trampolineData;
    uint32_t eventCallbackFilter;
    int64_t startTime = uPortGetTickTimeMs();
    bool irqSupported;

    if ((pContext != NULL) && (pChannelContext != NULL) && !pChannelContext->markedForDeletion) {
        pEventCallback = &(pChannelContext->eventCallback);
        eventCallbackFilter = pEventCallback->filter;
        if ((pEventCallback->pFunction != NULL) && (eventCallbackFilter & eventBitMap)) {
            trampolineData.pContext = pContext;
            trampolineData.channel = pChannelContext->channel;
            trampolineData.eventBitMap = eventBitMap;
            if (delayMs < 0) {
                errorCode = uPortEventQueueSend(pContext->eventQueueHandle, &trampolineData,
                                                sizeof(trampolineData));
            } else {
                do {
                    errorCode = uPortEventQueueSendIrq(pContext->eventQueueHandle, &trampolineData,
                                                       sizeof(trampolineData));
                    uPortTaskBlock(U_CFG_OS_YIELD_MS);
                    irqSupported = (errorCode != (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) &&
                                   (errorCode != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED);
                } while (irqSupported && (uPortGetTickTimeMs() - startTime < delayMs));

                if (!irqSupported) {
                    // If IRQ is not supported, just gotta do the normal send
                    errorCode = uPortEventQueueSend(pContext->eventQueueHandle, &trampolineData,
                                                    sizeof(trampolineData));
                }
            }
        }
    }

    return errorCode;
}

// The innards of serialGetReceiveSize(), brought out separately
// here so that cmuxReceiveCallback() can use it.
static int32_t serialGetReceiveSizeInnards(struct uDeviceSerial_t *pDeviceSerial)
{
    int32_t size = 0;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);
    uCellMuxPrivateTraffic_t *pTraffic = &(pChannelContext->traffic);
    const char *pRxBufferWrite = pTraffic->pRxBufferWrite;

    if (pTraffic->pRxBufferRead < pRxBufferWrite) {
        // Read pointer is behind write, bytes
        // received is simply the difference
        size = pRxBufferWrite - pTraffic->pRxBufferRead;
    } else if (pTraffic->pRxBufferRead > pRxBufferWrite) {
        // Read pointer is ahead of write, bytes received
        // is from the read pointer up to the end of the buffer
        // then wrap around to the write pointer
        size = (pTraffic->pRxBufferStart + pTraffic->rxBufferSizeBytes - pTraffic->pRxBufferRead) +
               (pRxBufferWrite - pTraffic->pRxBufferStart);
    }

    return size;
}

// The innards of serialRead(), brough out separately here so that
// sendCommandCheckResponse() can do a read from inside the mutex lock.
static int32_t serialReadInnards(volatile uCellMuxPrivateTraffic_t *pTraffic,
                                 void *pBuffer, size_t sizeBytes)
{
    int32_t totalRead = 0;
    uint8_t *pDataPtr = pBuffer;
    size_t thisSize;
    const char *pRxBufferWrite;

    pRxBufferWrite = pTraffic->pRxBufferWrite;
    if (pTraffic->pRxBufferRead < pRxBufferWrite) {
        // Read pointer is behind write, just take as much
        // of the difference as the user allows
        totalRead = pRxBufferWrite - pTraffic->pRxBufferRead;
        if (totalRead > (int32_t) sizeBytes) {
            totalRead = (int32_t) sizeBytes;
        }
        memcpy(pDataPtr, pTraffic->pRxBufferRead, totalRead);
        // Move the pointer on
        pTraffic->pRxBufferRead += totalRead;
    } else if (pTraffic->pRxBufferRead > pRxBufferWrite) {
        // Read pointer is ahead of write, first take up to the
        // end of the buffer as far as the user allows
        thisSize = pTraffic->pRxBufferStart +
                   pTraffic->rxBufferSizeBytes -
                   pTraffic->pRxBufferRead;
        if (thisSize > sizeBytes) {
            thisSize = sizeBytes;
        }
        memcpy(pDataPtr, pTraffic->pRxBufferRead, thisSize);
        pDataPtr += thisSize;
        sizeBytes -= thisSize;
        totalRead = thisSize;
        // Move the read pointer on, wrapping as necessary
        pTraffic->pRxBufferRead += thisSize;
        if (pTraffic->pRxBufferRead >= pTraffic->pRxBufferStart +
            pTraffic->rxBufferSizeBytes) {
            pTraffic->pRxBufferRead = pTraffic->pRxBufferStart;
        }
        // If there is still room in the user buffer then
        // carry on taking up to the write pointer
        if (sizeBytes > 0) {
            thisSize = pRxBufferWrite - pTraffic->pRxBufferRead;
            if (thisSize > sizeBytes) {
                thisSize = sizeBytes;
            }
            memcpy(pDataPtr, pTraffic->pRxBufferRead, thisSize);
            totalRead += thisSize;
            // Move the read pointer on
            pTraffic->pRxBufferRead += thisSize;
        }
    }

    return totalRead;
}

// The innards of serialWrite(), brough out separately here so that
// controlChannelInformation() can respond to MSC commands.
static int32_t serialWriteInnards(struct uDeviceSerial_t *pDeviceSerial,
                                  const void *pBuffer, size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);
    uCellPrivateInstance_t *pInstance = pChannelContext->pContext->pInstance;
    char *pBufferEncoded;
    size_t chunkSize = U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES;
    size_t thisChunkSize;
    size_t sizeWritten = 0;
    int32_t thisLengthWritten;
    size_t lengthWritten;
    int32_t startTimeMs;
    bool activityPinIsSet = false;

    // Encode the CMUX frame in chunks of the maximum information
    // length using a temporary buffer
    if (chunkSize > sizeBytes) {
        chunkSize = sizeBytes;
    }
    pBufferEncoded = (char *) pUPortMalloc(chunkSize + U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES);
    if (pBufferEncoded != NULL) {
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (pInstance->pinDtrPowerSaving >= 0) {
            activityPinIsSet = true;
            uCellPrivateSetPinDtr(pInstance, true);
        }
        startTimeMs = uPortGetTickTimeMs();
        while ((sizeWritten < sizeBytes) && (sizeOrErrorCode >= 0) &&
               (uPortGetTickTimeMs() - startTimeMs < U_CELL_MUX_WRITE_TIMEOUT_MS)) {
            // Encode a chunk as UIH
            thisChunkSize = sizeBytes - sizeWritten;
            if (thisChunkSize > U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES) {
                thisChunkSize = U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES;
            }
            sizeOrErrorCode = uCellMuxPrivateEncode(pChannelContext->channel,
                                                    U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH,
                                                    false, ((const char *) pBuffer) + sizeWritten,
                                                    thisChunkSize, pBufferEncoded);
            if (sizeOrErrorCode >= 0) {
                lengthWritten = 0;
                while ((sizeOrErrorCode >= 0) && (lengthWritten < (size_t) sizeOrErrorCode) &&
                       (uPortGetTickTimeMs() - startTimeMs < U_CELL_MUX_WRITE_TIMEOUT_MS)) {
                    if (!pChannelContext->traffic.txIsFlowControlledOff) {
                        // Send the data
                        thisLengthWritten = uPortUartWrite(pChannelContext->pContext->underlyingStreamHandle,
                                                           pBufferEncoded + lengthWritten,
                                                           sizeOrErrorCode - lengthWritten);
                        if (thisLengthWritten >= 0) {
                            lengthWritten += thisLengthWritten;
                        } else {
                            sizeOrErrorCode = thisLengthWritten;
                        }
                    } else {
                        uPortTaskBlock(10);
                    }
                }
#ifdef U_CELL_MUX_ENABLE_USER_TX_DEBUG
                if (sizeOrErrorCode >= 0) {
                    // Note: don't normally need debug prints for user writes as they
                    // are not very interesting (the control stuff is printed separately)
                    // but if you _really_ need it you can enable the code here
                    uPortLog("U_CELL_CMUX_%d: sent %d byte(s): ", pChannelContext->channel,
                             lengthWritten);
                    for (size_t x = 0; x < lengthWritten; x++) {
                        char y = *(pBufferEncoded + x);
#ifndef U_CELL_MUX_HEX_DEBUG
                        if (isprint((int32_t) y)) {
                            uPortLog("%c", y);
                        } else {
#endif
                            uPortLog("[%02x]", y);
#ifndef U_CELL_MUX_HEX_DEBUG
                        }
#endif
                    }
                    uPortLog(".\n");
                }
#endif
                // Keep track of the amount of user information written
                sizeWritten += thisChunkSize;
            }
        }

        if (activityPinIsSet) {
            uCellPrivateSetPinDtr(pInstance, false);
        }

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        if (sizeWritten == sizeBytes) {
            sizeOrErrorCode = (int32_t) sizeBytes;
        }

        // Free memory
        uPortFree(pBufferEncoded);
    }

    return sizeOrErrorCode;
}

// Send flow control on or off for the given channel.
static int32_t sendFlowControl(uCellMuxPrivateContext_t *pContext,
                               uint8_t channel, bool stopNotGo)
{
    char buffer[4];
    uDeviceSerial_t *pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, 0);

    // Format of the MSC frame that sends flow control is as described
    // in controlChannelInformation()
    buffer[0] = 0xe3; // MSC command
    buffer[1] = 0x05; // 2 bytes in the information field, EA bit set
    buffer[2] = (char) ((channel << 2) | 0x03); // the channel
    buffer[3] = 0x8d;  // RTR (AKA CTS), RTC (AKA DTR), DV (data valid) and EA bits set
    if (stopNotGo) {
        buffer[3] |= 0x02;   // Flow control is set to "please Mr Modem, do not send to us"
    }

#ifdef U_CELL_MUX_ENABLE_DEBUG
    uPortLog("U_CELL_CMUX_%d: %s [%02x%02x%02x%02x].\n", channel,
             stopNotGo ? "STOP" : "START", buffer[0], buffer[1], buffer[2], buffer[3]);
#endif

    return serialWriteInnards(pDeviceSerial, buffer, sizeof(buffer));
}

// Send a CMUX command and check the response
static int32_t sendCommandCheckResponse(uDeviceSerial_t *pDeviceSerial,
                                        uCellMuxUserFrame_t *pFrameSend,
                                        uCellMuxUserFrame_t *pFrameCheck,
                                        int32_t timeoutMs)
{
    int32_t errorCode;
    volatile uCellMuxPrivateChannelContext_t *pChannelContext = (volatile
                                                                 uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
    volatile uCellMuxPrivateTraffic_t *pTraffic = &(pChannelContext->traffic);
    char buffer[U_CELL_MUX_PRIVATE_FRAME_OVERHEAD_MAX_BYTES + sizeof(pFrameSend->information)];
    char *pTmp;
    int32_t length;
    int32_t startTimeMs;

    // Flush out any existing information field data
    while (serialReadInnards(pTraffic, buffer, sizeof(buffer)) > 0) {}
    // Encode the command
    length = uCellMuxPrivateEncode(pChannelContext->channel, pFrameSend->type,
                                   true, pFrameSend->information,
                                   pFrameSend->informationLengthBytes,
                                   buffer);
    if (length >= 0) {
        pTraffic->wantedResponseFrameType = pFrameCheck->type;
        errorCode = uPortUartWrite(pChannelContext->pContext->underlyingStreamHandle, buffer, length);
        if (errorCode == length) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
            uPortLog("U_CELL_CMUX_%d: tx %d byte(s): ", pChannelContext->channel, errorCode);
            for (int32_t x = 0; x < errorCode; x++) {
                uPortLog("[%02x]", buffer[x]);
            }
            uPortLog(".\n");
#endif
            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
            // Wait for a response
            startTimeMs = uPortGetTickTimeMs();
            while ((pTraffic->wantedResponseFrameType != U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE) &&
                   (uPortGetTickTimeMs() - startTimeMs < timeoutMs)) {
                uPortTaskBlock(10);
            }
            if (pTraffic->wantedResponseFrameType == U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                if (pChannelContext->channel == 0) {
                    // For the control channel we need to print the frame type out
                    // here as the message is removed before it gets to cmuxDecode()
                    uPortLog("U_CELL_CMUX_%d: rx frame type 0x%02x.\n", pChannelContext->channel,
                             pFrameCheck->type);
                }
#endif
                if (pFrameCheck->informationLengthBytes > 0) {
                    // Need to look for the right information field contents also
                    length = serialReadInnards(pTraffic, buffer, sizeof(buffer));
                    pTmp = buffer;
                    while ((length >= (int32_t) pFrameCheck->informationLengthBytes) &&
                           (memcmp(pTmp, pFrameCheck->information,
                                   pFrameCheck->informationLengthBytes) != 0)) {
                        pTmp++;
                        length--;
                    }
                    if (length >= (int32_t) pFrameCheck->informationLengthBytes) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                        uPortLog("U_CELL_CMUX_%d: decoded I-field %d byte(s):",
                                 pChannelContext->channel,
                                 pFrameCheck->informationLengthBytes);
                        for (size_t x = 0; x < pFrameCheck->informationLengthBytes; x++) {
                            uPortLog(" %02x", *(pTmp + x));
                        }
                        uPortLog(".\n");
#endif
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            } else {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                uPortLog("U_CELL_CMUX_%d: no response.\n", pChannelContext->channel);
#endif
            }
        }
    } else {
        errorCode = length;
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: VIRTUAL SERIAL PORT
 * -------------------------------------------------------------- */

// Open a virtual serial interface on a CMUX channel.
static int32_t serialOpen(struct uDeviceSerial_t *pDeviceSerial,
                          void *pReceiveBuffer, size_t receiveBufferSizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    volatile uCellMuxPrivateChannelContext_t *pChannelContext = (volatile
                                                                 uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
    volatile uCellMuxPrivateTraffic_t *pTraffic;
    bool isMalloced = false;
    uCellMuxUserFrame_t frameSend = {0};
    uCellMuxUserFrame_t frameCheck = {0};

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        pTraffic = &(pChannelContext->traffic);
        errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
        if (pChannelContext->state == U_CELL_MUX_PRIVATE_CHANNEL_STATE_NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if ((pReceiveBuffer == NULL) && (receiveBufferSizeBytes > 0)) {
                pReceiveBuffer = pUPortMalloc(receiveBufferSizeBytes);
                isMalloced = true;
            }
            if ((pReceiveBuffer != NULL) || (receiveBufferSizeBytes == 0)) {
                // Encode SABM to the given channel and wait for the response
                frameSend.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND;
                frameCheck.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE;
                errorCode = sendCommandCheckResponse(pDeviceSerial, &frameSend, &frameCheck,
                                                     U_CELL_MUX_SABM_TIMEOUT_MS);
                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    // Need to send the module an MSC that tells it that
                    // our virtual flow control signal is set to "no flow control"
                    sendFlowControl(pChannelContext->pContext,
                                    pChannelContext->channel, false);
                    pTraffic->pRxBufferStart = (char *) pReceiveBuffer;
                    pTraffic->rxBufferSizeBytes = receiveBufferSizeBytes;
                    pTraffic->rxBufferIsMalloced = isMalloced;
                    pTraffic->pRxBufferWrite = pTraffic->pRxBufferStart;
                    pTraffic->pRxBufferRead = pTraffic->pRxBufferWrite;
                    pChannelContext->state = U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN;
                } else if (isMalloced) {
                    // Clean up on error
                    uPortFree(pReceiveBuffer);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Close a virtual serial interface on a CMUX channel.
static void serialClose(struct uDeviceSerial_t *pDeviceSerial)
{
    volatile uCellMuxPrivateChannelContext_t *pChannelContext = (volatile
                                                                 uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
    volatile uCellMuxPrivateTraffic_t *pTraffic;
    uCellMuxUserFrame_t frameSend = {0};
    uCellMuxUserFrame_t frameCheck = {0};
    size_t x = sizeof(gMuxCldCommand);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        // In order to close a channel we must look all mutexes
        uPortMutexLock(pChannelContext->mutexUserDataWrite);
        uPortMutexLock(pChannelContext->mutexUserDataRead);

        pTraffic = &(pChannelContext->traffic);
        if (pChannelContext->channel == 0) {
            // To close channel 0, the control channel, we send the CLD
            // command and wait for the CLD response
            if (x > sizeof(frameSend.information)) {
                x = sizeof(frameSend.information);
            }
            frameSend.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH;
            memcpy(frameSend.information, gMuxCldCommand, x);
            frameSend.informationLengthBytes = sizeof(gMuxCldCommand);
            frameCheck.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH;
            memcpy(frameCheck.information, gMuxCldResponse, x);
            frameCheck.informationLengthBytes = sizeof(gMuxCldResponse);
            sendCommandCheckResponse(pDeviceSerial, &frameSend, &frameCheck,
                                     U_CELL_MUX_DISC_TIMEOUT_MS);
        } else {
            // For any other channel, send DISC and wait for UA
            frameSend.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND;
            frameCheck.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE;
            sendCommandCheckResponse(pDeviceSerial, &frameSend, &frameCheck,
                                     U_CELL_MUX_DISC_TIMEOUT_MS);
        }

        pChannelContext->state = U_CELL_MUX_PRIVATE_CHANNEL_STATE_NULL;
        if (pTraffic->rxBufferIsMalloced) {
            uPortFree(pTraffic->pRxBufferStart);
        }
        pTraffic->pRxBufferStart = NULL;
        // Don't actually close channel to ensure thread-safety
        pChannelContext->markedForDeletion = true;

        uPortMutexUnlock(pChannelContext->mutexUserDataRead);
        uPortMutexUnlock(pChannelContext->mutexUserDataWrite);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }
}

// Get the number of bytes waiting in a CMUX receive buffer.
static int32_t serialGetReceiveSize(struct uDeviceSerial_t *pDeviceSerial)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutexUserDataRead);

        sizeOrErrorCode = serialGetReceiveSizeInnards(pDeviceSerial);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutexUserDataRead);
    }

    return sizeOrErrorCode;
}

// Read from the receive buffer of the CMUX channel.
static int32_t serialRead(struct uDeviceSerial_t *pDeviceSerial,
                          void *pBuffer, size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);
    uCellMuxPrivateTraffic_t *pTraffic;
    int32_t x;

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutexUserDataRead);

        if (pBuffer != NULL) {
            sizeOrErrorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
            if (U_CELL_MUX_IS_OPEN(pChannelContext->state)) {
                pTraffic = &(pChannelContext->traffic);
                sizeOrErrorCode = serialReadInnards(pTraffic, pBuffer, sizeBytes);
#if defined(U_CELL_MUX_ENABLE_DEBUG) || defined(U_CELL_MUX_ENABLE_USER_RX_DEBUG)
                if (sizeOrErrorCode > 0) {
                    uPortLog("U_CELL_CMUX_%d: app read %d byte(s).\n", pChannelContext->channel,
                             sizeOrErrorCode);
                }
#endif
#ifdef U_CELL_MUX_ENABLE_USER_RX_DEBUG
                // Don't normally need this however it may be useful when
                // debugging the behaviour of a destination that is out of
                // reach, e.g. inside the IP stack of a platform, channeled
                // via PPP
                if (sizeOrErrorCode > 0) {
                    uPortLog("U_CELL_CMUX_%d: ", pChannelContext->channel);
                    for (int32_t x = 0; x < sizeOrErrorCode; x++) {
                        char y = *((char *) pBuffer + x);
#ifndef U_CELL_MUX_HEX_DEBUG
                        if (isprint((int32_t) y)) {
                            uPortLog("%c", y);
                        } else {
#endif
                            uPortLog("[%02x]", y);
#ifndef U_CELL_MUX_HEX_DEBUG
                        }
#endif
                    }
                    uPortLog(".\n");
                }
#endif
                if (pTraffic->rxIsFlowControlledOff &&
                    (((pTraffic->rxBufferSizeBytes - serialGetReceiveSizeInnards(pDeviceSerial)) * 100) /
                     pTraffic->rxBufferSizeBytes > U_CELL_MUX_PRIVATE_RX_FLOW_ON_THRESHOLD_PERCENT)) {
                    sendFlowControl(pChannelContext->pContext, pChannelContext->channel, false);
                    // The rxIsFlowControlledOff flag gets reset down in
                    // controlChannelInformation() when the acknowledgement arrives
                    // Re-trigger decoding of any received data we didn't previously
                    // have room to process.  We do a try send if we can so that we don't
                    // get stuck: if there are already events in the queue then they
                    // will do the trick.
                    x = uPortUartEventTrySend(pChannelContext->pContext->underlyingStreamHandle,
                                              U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED, 0);
                    if ((x == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
                        (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
                        uPortUartEventSend(pChannelContext->pContext->underlyingStreamHandle,
                                           U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED);
                    }
#ifdef U_CELL_MUX_ENABLE_DEBUG
                    uPortLog("U_CELL_CMUX: decoding retriggered.\n");
#endif
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutexUserDataRead);
    }

    return sizeOrErrorCode;
}

// Write to the CMUX channel.
static int32_t serialWrite(struct uDeviceSerial_t *pDeviceSerial,
                           const void *pBuffer, size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutexUserDataWrite);

        sizeOrErrorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
        if (U_CELL_MUX_IS_OPEN(pChannelContext->state)) {
            sizeOrErrorCode = serialWriteInnards(pDeviceSerial, pBuffer, sizeBytes);
        }

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutexUserDataWrite);
    }

    return sizeOrErrorCode;
}

// Set an event callback on the virtual serial interface.
static int32_t serialEventCallbackSet(struct uDeviceSerial_t *pDeviceSerial,
                                      uint32_t filter,
                                      void (*pFunction)(struct uDeviceSerial_t *,
                                                        uint32_t,
                                                        void *),
                                      void *pParam,
                                      size_t stackSizeBytes,
                                      int32_t priority)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    (void) stackSizeBytes;
    (void) priority;

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        pChannelContext->eventCallback.pFunction = pFunction;
        pChannelContext->eventCallback.filter = filter;
        pChannelContext->eventCallback.pParam = pParam;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Remove a serial event callback.
static void serialEventCallbackRemove(struct uDeviceSerial_t *pDeviceSerial)
{
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        pChannelContext->eventCallback.pFunction = NULL;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }
}

// Get the serial event callback filter.
static uint32_t serialEventCallbackFilterGet(struct uDeviceSerial_t *pDeviceSerial)
{
    uint32_t filter = 0;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        filter = pChannelContext->eventCallback.filter;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return filter;
}

// Change the serial event callback filter bit-mask.
static int32_t serialEventCallbackFilterSet(struct uDeviceSerial_t *pDeviceSerial,
                                            uint32_t filter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion && (filter != 0)) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        pChannelContext->eventCallback.filter = filter;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Send an event to the serial event callback.
static int32_t serialEventSend(struct uDeviceSerial_t *pDeviceSerial,
                               uint32_t eventBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        errorCode = sendEvent(pChannelContext->pContext, pChannelContext, eventBitMap, -1);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Try to send an event to the serial event callback.
static int32_t serialEventTrySend(struct uDeviceSerial_t *pDeviceSerial,
                                  uint32_t eventBitMap, int32_t delayMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        errorCode = sendEvent(pChannelContext->pContext, pChannelContext, eventBitMap, delayMs);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Return whether we're in a callback or not.
static bool serialEventIsCallback(struct uDeviceSerial_t *pDeviceSerial)
{
    bool isCallback = false;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        isCallback = uPortEventQueueIsTask(pChannelContext->pContext->eventQueueHandle);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return isCallback;
}

// Return the minimum free callback-task stack.
static int32_t serialEventStackMinFree(struct uDeviceSerial_t *pDeviceSerial)
{
    int32_t errorCodeOrStackMinFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        errorCodeOrStackMinFree = uPortEventQueueStackMinFree(pChannelContext->pContext->eventQueueHandle);

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCodeOrStackMinFree;
}

// Return whether RTS flow control is enabled or not; it always is for CMUX.
static bool serialIsRtsFlowControlEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return true;
}

// Return whether CTS flow control is enabled or not; it always is for CMUX.
static bool serialIsCtsFlowControlEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return true;
}

// Suspend CTS (i.e. this MCU flow-controlling the far end off).
static int32_t serialCtsSuspend(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Resume CTS.
static void serialCtsResume(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
}

// Set whether discard on overflow is enabled or not.
static int32_t serialDiscardOnOverflow(struct uDeviceSerial_t *pDeviceSerial,
                                       bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        pChannelContext->traffic.discardOnOverflow = onNotOff;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return errorCode;
}

// Return whether discard on overflow is enabled or not.
static bool serialIsDiscardOnOverflowEnabled(struct uDeviceSerial_t *pDeviceSerial)
{
    bool isEnabled = false;
    uCellMuxPrivateChannelContext_t *pChannelContext = (uCellMuxPrivateChannelContext_t *)
                                                       pUInterfaceContext(pDeviceSerial);

    if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion) {

        U_PORT_MUTEX_LOCK(pChannelContext->mutex);

        isEnabled = pChannelContext->traffic.discardOnOverflow;

        U_PORT_MUTEX_UNLOCK(pChannelContext->mutex);
    }

    return isEnabled;
}

// Populate the vector table.
static void initSerialInterface(struct uDeviceSerial_t *pDeviceSerial)
{
    pDeviceSerial->open = serialOpen;
    pDeviceSerial->close = serialClose;
    pDeviceSerial->getReceiveSize = serialGetReceiveSize;
    pDeviceSerial->read = serialRead;
    pDeviceSerial->write = serialWrite;
    pDeviceSerial->eventCallbackSet = serialEventCallbackSet;
    pDeviceSerial->eventCallbackRemove = serialEventCallbackRemove;
    pDeviceSerial->eventCallbackFilterGet = serialEventCallbackFilterGet;
    pDeviceSerial->eventCallbackFilterSet = serialEventCallbackFilterSet;
    pDeviceSerial->eventSend = serialEventSend;
    pDeviceSerial->eventTrySend = serialEventTrySend;
    pDeviceSerial->eventIsCallback = serialEventIsCallback;
    pDeviceSerial->eventStackMinFree = serialEventStackMinFree;
    pDeviceSerial->isRtsFlowControlEnabled = serialIsRtsFlowControlEnabled;
    pDeviceSerial->isCtsFlowControlEnabled = serialIsCtsFlowControlEnabled;
    pDeviceSerial->ctsSuspend = serialCtsSuspend;
    pDeviceSerial->ctsResume = serialCtsResume;
    pDeviceSerial->discardOnOverflow = serialDiscardOnOverflow;
    pDeviceSerial->isDiscardOnOverflowEnabled = serialIsDiscardOnOverflowEnabled;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CHANNEL MANAGEMENT
 * -------------------------------------------------------------- */

// Get the channel to use for GNSS
static uint8_t getChannelGnss(const uCellPrivateInstance_t *pInstance)
{
    uint8_t channel = (uint8_t) pInstance->pModule->defaultMuxChannelGnss;

    if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        // For the SARA-R5 case the CMUX channel for GNSS is different
        // if we are exchanging AT commands on the AUX UART, which is
        // USIO variant 2.
        if (uCellPrivateGetActiveSerialInterface(pInstance) == 2) {
            channel = 3;
        }
    }

    return channel;
}

// Open a CMUX channel.
static int32_t openChannel(uCellMuxPrivateContext_t *pContext,
                           uint8_t channel, size_t receiveBufferSizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateChannelContext_t *pChannelContext = NULL;
    uDeviceSerial_t *pDeviceSerial;
    int32_t index = -1;

    if ((pContext != NULL) && (channel <= U_CELL_MUX_PRIVATE_CHANNEL_ID_MAX)) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, channel);
        if (pDeviceSerial == NULL) {
            // Find an unused entry in the list
            for (size_t x = 0;
                 (x < sizeof(pContext->pDeviceSerial) / sizeof(pContext->pDeviceSerial[0])) &&
                 (index < 0); x++) {
                if (pContext->pDeviceSerial[x] == NULL) {
                    index = x;
                } else {
                    pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(
                                          pContext->pDeviceSerial[x]);
                    if ((pChannelContext != NULL) && pChannelContext->markedForDeletion) {
                        uPortMutexDelete(pChannelContext->mutex);
                        uPortMutexDelete(pChannelContext->mutexUserDataWrite);
                        uPortMutexDelete(pChannelContext->mutexUserDataRead);
                        uDeviceSerialDelete(pContext->pDeviceSerial[x]);
                        index = x;
                    }
                }
            }
            if (index >= 0) {
                // Create the serial device
                pDeviceSerial = pUDeviceSerialCreate(initSerialInterface,
                                                     sizeof(uCellMuxPrivateChannelContext_t));
                if (pDeviceSerial != NULL) {
                    pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                    errorCode = uPortMutexCreate(&(pChannelContext->mutex));
                    if (errorCode == 0) {
                        errorCode = uPortMutexCreate(&(pChannelContext->mutexUserDataRead));
                    }
                    if (errorCode == 0) {
                        errorCode = uPortMutexCreate(&(pChannelContext->mutexUserDataWrite));
                    }
                    if (errorCode == 0) {
                        pContext->pDeviceSerial[index] = pDeviceSerial;
                    }  else {
                        // Clean up on error
                        if (pChannelContext->mutexUserDataWrite != NULL) {
                            uPortMutexDelete(pChannelContext->mutexUserDataWrite);
                            pChannelContext->mutexUserDataWrite = NULL;
                        }
                        if (pChannelContext->mutexUserDataRead != NULL) {
                            uPortMutexDelete(pChannelContext->mutexUserDataRead);
                            pChannelContext->mutexUserDataRead = NULL;
                        }
                        if (pChannelContext->mutex != NULL) {
                            uPortMutexDelete(pChannelContext->mutex);
                            pChannelContext->mutex = NULL;
                        }
                        uDeviceSerialDelete(pDeviceSerial);
                        pDeviceSerial = NULL;
                    }
                }
            }
        }
        if (pDeviceSerial != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (index >= 0) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                pChannelContext->pContext = pContext;
                pChannelContext->channel = channel;
                pChannelContext->markedForDeletion = false;
                memset(&(pChannelContext->traffic), 0, sizeof(pChannelContext->traffic));
                memset(&(pChannelContext->eventCallback), 0, sizeof(pChannelContext->eventCallback));
                errorCode = pDeviceSerial->open(pDeviceSerial, NULL, receiveBufferSizeBytes);
                // Don't clean up on error here - the serial device will be re-used if
                // the user tries again and this ensures thread-safety.
            }
        }
    }

    return errorCode;
}

// Handle an information field that arrives in a UI/UIH frame on the control channel.
static void controlChannelInformation(uCellMuxPrivateContext_t *pContext,
                                      uint8_t *pBuffer, size_t size)
{
    bool isCommand;
    uint8_t mscChannel;
    uDeviceSerial_t *pDeviceSerial;
    uCellMuxPrivateChannelContext_t *pChannelContext;

    // TODO: is it possible to get more than one message in the same I-frame?

    // The only thing we should get here is an MSC command or response, format:
    //
    // |--- command ---|-- length --|-- channel --|-- bitmap --|-- break --|
    // | 1110 00 C/R 1 | 0000 0xx1  |  xxxx xx11  | see below  |  ignored  |
    //
    // The MSC frame is most interesting if it is a command (the response is
    // just what we sent to it copied back to us), in which case C/R is 1.
    //
    // The xx bits in the length field are 10 (2) or 11 (3), depending on
    // whether the optional break byte is included.
    //
    // The xxxxxx bits in the channel field give the channel.
    //
    // We will ignore the break byte since it only has meaning for circuit-switched
    // data connections (i.e. "+++") which we do not support/expect.
    //
    // The bitmap field contains all of the control signals:
    //
    // Bit:   7     6     5     4     3     2     1     0
    //        DV    IC   ---  ---    RTR   RTC    FC    EA
    //
    // Of these, we only care about the FC (flow control) bit and we only care
    // about it when C/R is 1.  An FC of 1 means "do not send data".
#ifdef U_CELL_MUX_ENABLE_DEBUG
    uPortLog("U_CELL_CMUX_0: MSC in:");
    for (size_t x = 0; x < size; x++) {
        uPortLog(" %02x", *(pBuffer + x));
    }
    uPortLog(".\n");
#endif
    if ((size >= 4) && ((*pBuffer == 0xe1) || (*pBuffer == 0xe3)) && (*(pBuffer + 1) >= 5)) {
        isCommand = ((*pBuffer & 0x02) == 0x02);
        mscChannel = *(pBuffer + 2) >> 2;
        pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, mscChannel);
        pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
        if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion &&
            U_CELL_MUX_IS_OPEN(pChannelContext->state)) {
            if (isCommand) {
                pChannelContext->traffic.txIsFlowControlledOff = ((*(pBuffer + 3) & 0x02) == 0x02);
            } else {
                pChannelContext->traffic.rxIsFlowControlledOff = ((*(pBuffer + 3) & 0x02) == 0x02);
            }
        }
        if (isCommand) {
            // We must acknowledge this with an MSC frame sent on channel 0
            // with the same contents but with the C/R bit set to 0
            pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, 0);
            *pBuffer &= ~0x02;
#ifdef U_CELL_MUX_ENABLE_DEBUG
            uPortLog("U_CELL_CMUX_0: MSC out:");
            for (size_t x = 0; x < size; x++) {
                uPortLog(" %02x", *(pBuffer + x));
            }
            uPortLog(".\n");
#endif
            serialWriteInnards(pDeviceSerial, pBuffer, size);
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CMUX FRAME DECODING
 * -------------------------------------------------------------- */

// Decode the linear control buffer looking for control messages.
static void cmuxDecodeControl(uCellMuxPrivateContext_t *pContext)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateParserContext_t parserContext;
    uDeviceSerial_t *pDeviceSerial;
    uCellMuxPrivateChannelContext_t *pChannelContext;
    size_t x;

    if (pContext != NULL) {
        // Point the parser context at the holding buffer
        memset(&parserContext, 0, sizeof(parserContext));
        parserContext.pBuffer = pContext->holdingBuffer;
        parserContext.bufferSize = pContext->holdingBufferIndex;
        if (parserContext.bufferSize > sizeof(pContext->holdingBuffer)) {
            parserContext.bufferSize = sizeof(pContext->holdingBuffer);
        }
        parserContext.bufferIndex = 0;
        // Run through the buffer decoding control channel frames only,
        // discarding everything else; decode any information fields
        // into the information buffer
        parserContext.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE;
        parserContext.address = U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL;
        parserContext.pInformation = pContext->scratch;
        parserContext.informationLengthBytes = sizeof(pContext->scratch);
        while ((parserContext.bufferIndex < parserContext.bufferSize) &&
               (errorCode != (int32_t) U_ERROR_COMMON_TIMEOUT)) {
            errorCode = uCellMuxPrivateParseCmux(NULL, &parserContext);
            if (errorCode == 0) {
                pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, parserContext.address);
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                if ((pChannelContext != NULL) && (!pChannelContext->markedForDeletion)) {
                    // Check if the frame type was wanted
                    if (pChannelContext->traffic.wantedResponseFrameType == parserContext.type) {
                        pChannelContext->traffic.wantedResponseFrameType = U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE;
                    }
                    switch (parserContext.type) {
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE:
                        // Remote end has disconnected
                        //fall-through
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND:
                            // TODO: this means we're out of mux mode
                            pChannelContext->state = U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN_DISCONNECTED;
                            break;
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH:
                        //fall-through
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_UI:
                            // This must be MSC, the flow control stuff
                            if (parserContext.informationLengthBytes > sizeof(pContext->scratch)) {
                                parserContext.informationLengthBytes = sizeof(pContext->scratch);
                            }
                            controlChannelInformation(pContext, (uint8_t *) pContext->scratch,
                                                      parserContext.informationLengthBytes);
                            break;
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE:
                        // We will have remembered that we received one of these, that's good enough
                        //fall-through
                        case U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND:
                        // Shouldn't receive this - ignore it
                        //fall-through
                        default:
                            break;
                    }
                }
            }

            if (errorCode != (int32_t) U_ERROR_COMMON_TIMEOUT) {
                // If we've either found nothing or been successful, we can
                // shuffle that data out of the buffer (gotta keep it if
                // we've timed-out as we will hopefully have added to it
                // when we come back into here)
                x = parserContext.bufferSize - parserContext.bufferIndex;
                memmove(pContext->holdingBuffer,
                        pContext->holdingBuffer + parserContext.bufferIndex, x);
                parserContext.bufferSize -= parserContext.bufferIndex;
                pContext->holdingBufferIndex = x;
                parserContext.bufferIndex = 0;
            }
        }

        // Note: you'll find that we generally get here with
        // U_ERROR_COMMON_TIMEOUT as the result: this is because
        // there is user data in the buffer as well as control
        // data and the decoder can't be sure it won't turn out
        // to be control data when a little more arrives, so will
        // hang on to some of it, just a handful of bytes
    }
}

// Decode received CMUX frames, just the non-control-channel ones, from
// the ring buffer.
static void cmuxDecode(uCellMuxPrivateContext_t *pContext, uint32_t eventBitMap)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateParserContext_t parserContext;
    uDeviceSerial_t *pDeviceSerial;
    uCellMuxPrivateChannelContext_t *pChannelContext;
    uCellMuxPrivateTraffic_t *pTraffic;
    U_RING_BUFFER_PARSER_f parserList[] = {uCellMuxPrivateParseCmux, NULL};
    size_t offset;
    bool stalled = false;
    size_t bufferLength;
    size_t discardLength;
    size_t x;
    const char *pRxBufferRead;

    if (pContext != NULL) {
        // Try to decode new CMUX messages from the ring buffer
        errorCodeOrLength = 0;
        while ((errorCodeOrLength >= 0) && !stalled) {
            memset(&parserContext, 0, sizeof(parserContext));
            parserContext.type = U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE;
            parserContext.address = U_CELL_MUX_PRIVATE_ADDRESS_ANY;
            // Initial decode, which does NOT copy-out the information field
            // because we don't know if we have enough room in the buffers
            errorCodeOrLength = uRingBufferParseHandle(&(pContext->ringBuffer),
                                                       pContext->readHandle,
                                                       parserList, &parserContext);
            if (errorCodeOrLength > 0) {
                discardLength = 0;
                pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, parserContext.address);
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                if ((pChannelContext != NULL) &&
                    (parserContext.address != U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL)) {
                    pTraffic = &(pChannelContext->traffic);
                    if (!pChannelContext->markedForDeletion) {
                        // Check if the frame type was wanted
#ifdef U_CELL_MUX_ENABLE_DEBUG
                        uPortLog("U_CELL_CMUX_%d: rx frame type 0x%02x.\n", pChannelContext->channel,
                                 parserContext.type);
#endif
                        if (pChannelContext->traffic.wantedResponseFrameType == parserContext.type) {
                            pChannelContext->traffic.wantedResponseFrameType = U_CELL_MUX_PRIVATE_FRAME_TYPE_NONE;
                        }
                        switch (parserContext.type) {
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_DM_RESPONSE:
                            // Remote end has disconnected
                            //fall-through
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_DISC_COMMAND:
                                // TODO: this requires a UA response
                                pChannelContext->state = U_CELL_MUX_PRIVATE_CHANNEL_STATE_OPEN_DISCONNECTED;
                                break;
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_UIH:
                            //fall-through
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_UI:
                                if (pTraffic->rxBufferSizeBytes > 0) {
                                    // We have user information, work out how much we can cope with
                                    // -1 below to avoid pointer wrap
                                    bufferLength  = pTraffic->rxBufferSizeBytes - serialGetReceiveSizeInnards(pDeviceSerial) - 1;
                                    if (bufferLength > sizeof(pContext->scratch)) {
                                        bufferLength = sizeof(pContext->scratch);
                                    }
                                    if (parserContext.informationLengthBytes > bufferLength) {
                                        discardLength = parserContext.informationLengthBytes - bufferLength;
                                        parserContext.informationLengthBytes = bufferLength;
                                    }
                                    if ((discardLength == 0) || pTraffic->discardOnOverflow) {
                                        // Re-parse the buffer to actually get the information field
                                        parserContext.pInformation = pContext->scratch;
                                        uRingBufferParseHandle(&(pContext->ringBuffer),
                                                               pContext->readHandle,
                                                               parserList, &parserContext);
                                        if (parserContext.informationLengthBytes > bufferLength) {
                                            parserContext.informationLengthBytes = bufferLength;
                                        }
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                        uPortLog("U_CELL_CMUX_%d: writing %d byte(s) of decode I-field, buffer %d/%d.\n",
                                                 pChannelContext->channel,
                                                 parserContext.informationLengthBytes,
                                                 serialGetReceiveSizeInnards(pDeviceSerial),
                                                 pTraffic->rxBufferSizeBytes);
#endif
                                        //  Move the user's information-field bytes into the main buffer
                                        pRxBufferRead = pTraffic->pRxBufferRead;
                                        if (pTraffic->pRxBufferWrite >= pRxBufferRead) {
                                            // Write pointer is equal to or ahead of read,
                                            // start by adding up to the end of the buffer
                                            offset = pTraffic->pRxBufferStart +
                                                     pTraffic->rxBufferSizeBytes - pTraffic->pRxBufferWrite;
                                            if (offset > parserContext.informationLengthBytes) {
                                                offset = parserContext.informationLengthBytes;
                                            }
                                            memcpy(pTraffic->pRxBufferWrite, pContext->scratch, offset);
                                            parserContext.informationLengthBytes -= offset;
                                            // Move the write pointer on, wrapping as necessary
                                            pTraffic->pRxBufferWrite += offset;
                                            if (pTraffic->pRxBufferWrite >= pTraffic->pRxBufferStart +
                                                pTraffic->rxBufferSizeBytes) {
                                                pTraffic->pRxBufferWrite = pTraffic->pRxBufferStart;
                                            }
                                            // If there is still stuff to write, continue writing
                                            // up to just before the read pointer
                                            if (parserContext.informationLengthBytes > 0) {
                                                x = pRxBufferRead - pTraffic->pRxBufferWrite;
                                                if (x > 0) {
                                                    x--;
                                                }
                                                if (x > parserContext.informationLengthBytes) {
                                                    x = parserContext.informationLengthBytes;
                                                }
                                                memcpy(pTraffic->pRxBufferWrite, pContext->scratch + offset, x);
                                                pTraffic->pRxBufferWrite += x;
                                            }
                                        } else {
                                            // Write pointer is behind read, just write as much as we can
                                            x = pRxBufferRead - pTraffic->pRxBufferWrite;
                                            if (x > parserContext.informationLengthBytes) {
                                                x = parserContext.informationLengthBytes;
                                            }
                                            memcpy(pTraffic->pRxBufferWrite, pContext->scratch, x);
                                            pTraffic->pRxBufferWrite += x;
                                        }
                                        // Wrap the write pointer if necessary
                                        if (pTraffic->pRxBufferWrite >= pTraffic->pRxBufferStart +
                                            pTraffic->rxBufferSizeBytes) {
                                            pTraffic->pRxBufferWrite = pTraffic->pRxBufferStart;
                                        }
                                        // Having decoded what we can, do any discarding
                                        if (discardLength > 0) {
                                            uRingBufferReadHandle(&(pContext->ringBuffer), pContext->readHandle,
                                                                  NULL, discardLength);
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                            uPortLog("U_CELL_CMUX_%d: discarded %d byte(s) of I-field.\n",
                                                     pChannelContext->channel, discardLength);
#endif
                                        }
                                    } else {
                                        // Not enough room to decode more of the information field
                                        // on this channel, we are stalled
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                        uPortLog("U_CELL_CMUX: stalled.\n");
#endif
                                        stalled = true;
                                    }

                                    // After all that, check if the channel's receive buffer is
                                    // sufficiently full that we should flow control off this channel
                                    if (!pTraffic->rxIsFlowControlledOff &&
                                        (((pTraffic->rxBufferSizeBytes - serialGetReceiveSizeInnards(pDeviceSerial)) * 100) /
                                         pTraffic->rxBufferSizeBytes) < U_CELL_MUX_PRIVATE_RX_FLOW_OFF_THRESHOLD_PERCENT) {
                                        sendFlowControl(pContext, parserContext.address, true);
                                        pTraffic->rxIsFlowControlledOff = true;
                                    }

                                    // Call the  event callback a user may have set for this
                                    // virtual serial device so that they can move the data out
                                    // of the buffer ASAP, but don't hang around if the queue
                                    // is already full as the events in front of us in the queue
                                    // will do the trick.
                                    sendEvent(pContext, pChannelContext, eventBitMap, 0);
                                }
                                break;
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_UA_RESPONSE:
                            // We will have remembered that we received one of these, that's good enough
                            //fall-through
                            case U_CELL_MUX_PRIVATE_FRAME_TYPE_SABM_COMMAND:
                            // Shouldn't receive this - ignore it
                            //fall-through
                            default:
                                break;
                        }
                    }
                }

                if (!stalled) {
                    // Remove any suff we have not already discarded from the ring-buffer
                    // if we've processed it
                    uRingBufferReadHandle(&(pContext->ringBuffer), pContext->readHandle,
                                          NULL, errorCodeOrLength - discardLength);
                }
            }
        }

        // If there is still data in any of the channel buffers and there is an event
        // callback then call it again here, in case the application had become
        // stuck with no buffer space to pull it into and needs the hint that there is
        // still stuff down here.
        for (x = 0; x < sizeof(pContext->pDeviceSerial) / sizeof (pContext->pDeviceSerial[0]); x++) {
            pDeviceSerial = pContext->pDeviceSerial[x];
            if ((pDeviceSerial != NULL) && (serialGetReceiveSizeInnards(pDeviceSerial) > 0)) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                if ((pChannelContext->eventCallback.pFunction != NULL) && !pChannelContext->markedForDeletion) {
                    sendEvent(pContext, pChannelContext, U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED, 0);
                }
            }
        }
    }
}

// Callback that is called when an event (e.g. data arrival) occurs on the
// stream interface carrying CMUX frames.
static void cmuxReceiveCallback(const uAtClientStreamHandle_t *pStream,
                                uint32_t eventBitMap,
                                void *pParameters)
{
    int32_t receiveSizeOrError;
    size_t y;
    uCellMuxPrivateContext_t *pContext = (uCellMuxPrivateContext_t *) pParameters;

    // Note: this does NOT lock the mutex because it needs to be able
    // to handle flow-control and so can't be locked-out by write operations

    if ((pContext != NULL) && (pStream != NULL) && (pStream->type == U_AT_CLIENT_STREAM_TYPE_UART)) {
        if (eventBitMap & U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED) {
            // This is constructed as a do/while loop so that it always has
            // at least one go at decoding stuff that was previously stored in
            // the ring buffer
            do {
                // Read and parse in chunks
                // Note: a consequence of this check is that, in theory, unprocessed
                // user data stuck in the ring buffer _could_ prevent incoming
                // control information from being decoded.  The ring buffer is
                // deliberately large to prevent that happening, but just so's you know...
                y = uRingBufferAvailableSize(&(pContext->ringBuffer));
                if (y > sizeof(pContext->holdingBuffer) - pContext->holdingBufferIndex) {
                    y = sizeof(pContext->holdingBuffer) - pContext->holdingBufferIndex;
                }
                receiveSizeOrError = uPortUartGetReceiveSize(pStream->handle.int32);
                if (receiveSizeOrError > (int32_t) y) {
                    receiveSizeOrError = y;
                }

                if (receiveSizeOrError > 0) {
                    // Read the CMUX stream into the control buffer
                    receiveSizeOrError = uPortUartRead(pStream->handle.int32,
                                                       pContext->holdingBuffer + pContext->holdingBufferIndex,
                                                       receiveSizeOrError);
                }

                // Add the control buffer contents to the ring buffer
                if (receiveSizeOrError > 0) {
                    if  (!uRingBufferAdd(&(pContext->ringBuffer),
                                         pContext->holdingBuffer + pContext->holdingBufferIndex,
                                         receiveSizeOrError)) {
                        // Should never get here since we checked the available size above
                        receiveSizeOrError = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    } else {
                        pContext->holdingBufferIndex += receiveSizeOrError;
                    }
                }

#ifdef U_CELL_MUX_ENABLE_DEBUG
                // -1 below since we lose one byte in the ring buffer implementation
                uPortLog("U_CELL_CMUX: rx %d byte(s) (ctrl %d/%d, ring %d/%d).\n",
                         receiveSizeOrError,
                         pContext->holdingBufferIndex,
                         sizeof(pContext->holdingBuffer),
                         uRingBufferDataSizeHandle(&(pContext->ringBuffer), pContext->readHandle),
                         sizeof(pContext->linearBuffer) - 1);
#endif

                // Decode control and then data
                cmuxDecodeControl(pContext);
                cmuxDecode(pContext, eventBitMap);

            } while (receiveSizeOrError > 0);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO CELLULAR
 * -------------------------------------------------------------- */

// Enable multiplexer mode.  This involves a few steps:
//
// 1. Send the AT+CMUX command and wait for the OK.
// 2. Send SABM and wait for UA on CMUX channel 0.
// 3. If successful, create a virtual serial interface and an
//    AT client on CMUX channel 1, the AT channel, copy the current
//    state there and begin using it.
// 4. If not successful, unwind.
int32_t uCellMuxPrivateEnable(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    uAtClientStreamHandle_t stream = U_AT_CLIENT_STREAM_HANDLE_DEFAULTS;
    uCellMuxPrivateContext_t *pContext;
    uDeviceSerial_t *pDeviceSerial;
    int32_t cmeeMode = 2;
    char tempBuffer[32];

    if (pInstance != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule, U_CELL_PRIVATE_FEATURE_CMUX)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pMuxContext == NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Allocate memory for our CMUX context; this will be
                // deallocated only when the cellular instance is removed
                pInstance->pMuxContext = pUPortMalloc(sizeof(uCellMuxPrivateContext_t));
                if (pInstance->pMuxContext != NULL) {
                    pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
                    memset(pContext, 0, sizeof(*pContext));
                    // To save memory, we use a single event queue for all callbacks
                    // from the CMUX channels, re-using the AT client sizes
                    pContext->eventQueueHandle = uPortEventQueueOpen(eventHandler,
                                                                     "cmuxCallbacks",
                                                                     sizeof(uCellMuxEventTrampoline_t),
                                                                     U_CELL_MUX_CALLBACK_TASK_STACK_SIZE_BYTES,
                                                                     U_CELL_MUX_CALLBACK_TASK_PRIORITY,
                                                                     U_CELL_MUX_CALLBACK_QUEUE_LENGTH);
                    if (pContext->eventQueueHandle >= 0) {
                        if (uRingBufferCreateWithReadHandle(&(pContext->ringBuffer),
                                                            pContext->linearBuffer,
                                                            sizeof(pContext->linearBuffer), 1) == 0) {
                            uRingBufferSetReadRequiresHandle(&(pContext->ringBuffer), true);
                            pContext->readHandle = uRingBufferTakeReadHandle(&(pContext->ringBuffer));
                        } else {
                            // Clean up on error
                            uPortEventQueueClose(pContext->eventQueueHandle);
                            uPortFree(pInstance->pMuxContext);
                            pInstance->pMuxContext = NULL;
                        }
                    } else {
                        // Clean up on error
                        uPortFree(pInstance->pMuxContext);
                        pInstance->pMuxContext = NULL;
                    }
                }
            }
            if (pInstance->pMuxContext != NULL) {
                pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pContext->savedAtHandle == NULL) {
                    // Initialise the other parts of [an existing] context
                    pContext->pInstance = pInstance;
                    pContext->channelGnss = getChannelGnss(pInstance);
                    pContext->holdingBufferIndex = 0;
                    // Initiate CMUX
                    atHandle = pInstance->atHandle;
                    uAtClientLock(atHandle);
                    uAtClientStreamGetExt(atHandle, &stream);
                    uRingBufferFlushHandle(&(pContext->ringBuffer), pContext->readHandle);
                    pContext->underlyingStreamHandle = stream.handle.int32;
                    uAtClientCommandStart(atHandle, "AT+CMUX=");
                    // Only basic mode and only UIH frames are supported by any
                    // of the cellular modules we support
                    uAtClientWriteInt(atHandle, 0);
                    uAtClientWriteInt(atHandle, 0);
                    // As advised in the u-blox multiplexer document, port
                    // speed is left empty for max compatibility
                    uAtClientWriteString(atHandle, "", false);
                    // Set the information field length
                    uAtClientWriteInt(atHandle,
                                      U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES);
                    // Everything else is left at defaults for max compatibility
                    uAtClientCommandStopReadResponse(atHandle);
                    // Not unlocking here, just check for errors
                    errorCode = uAtClientErrorGet(atHandle);
                    if (errorCode == 0) {
                        // Leave the AT client locked to stop it reacting to stuff coming
                        // back over the UART, which will shortly become the MUX
                        // control channel and not an AT interface at all.
                        // Replace the URC handler of the existing AT client
                        // with our own so that we get the received data
                        // and can decode it
                        uAtClientUrcHandlerHijackExt(atHandle, cmuxReceiveCallback, pContext);
                        // Give the module a moment for the MUX switcheroo
                        uPortTaskBlock(U_CELL_MUX_PRIVATE_ENABLE_DISABLE_DELAY_MS);
                        // Open the control channel, channel 0; for this we need no
                        // data buffer, since it does not carry user data
                        pContext->savedAtHandle = atHandle;
                        errorCode = openChannel(pContext, U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL, 0);
                        if (errorCode == 0) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                            uPortLog("U_CELL_CMUX_0: control channel open.\n");
#endif
                            // Channel 0 is up, now we need channel 1, on which
                            // we will need a data buffer for the information field carrying the
                            // user data (i.e. AT commands)
                            errorCode = openChannel(pContext, U_CELL_MUX_PRIVATE_CHANNEL_ID_AT,
                                                    U_CELL_MUX_PRIVATE_VIRTUAL_SERIAL_BUFFER_LENGTH_BYTES);
                            if (errorCode == 0) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                uPortLog("U_CELL_CMUX_1: AT channel open, flushing stored URCs...\n");
#endif
                                pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, U_CELL_MUX_PRIVATE_CHANNEL_ID_AT);
                                // Some modules (e.g. SARA-R422) can have stored up loads of URCs
                                // which they like to emit over the new mux channel; flush these
                                // out here
                                uPortTaskBlock(500);
                                do {
                                    uPortTaskBlock(10);
                                } while (pDeviceSerial->read(pDeviceSerial, tempBuffer, sizeof(tempBuffer)) > 0);
                                // Create a copy of the current AT client on this serial port
                                stream.handle.pDeviceSerial = pDeviceSerial;
                                stream.type = U_AT_CLIENT_STREAM_TYPE_VIRTUAL_SERIAL;
                                atHandle = uAtClientAddExt(&stream, NULL, U_CELL_AT_BUFFER_LENGTH_BYTES);
                                if (atHandle != NULL) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                    uPortLog("U_CELL_CMUX: AT client added.\n");
#endif
                                    errorCode = uCellMuxPrivateCopyAtClient(pContext->savedAtHandle,
                                                                            atHandle);
                                    if (errorCode == 0) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                                        uPortLog("U_CELL_CMUX: existing AT client copied, CMUX is running.\n");
#endif
                                        // Now that we have everything, we set the AT handle
                                        // of our instance to the new AT handle, leaving the
                                        // old AT handle locked
                                        pInstance->atHandle = atHandle;
                                        // The setting of echo-off and AT+CMEE is port-specific,
                                        // so we need to set those here for the new port
#ifdef U_CFG_CELL_ENABLE_NUMERIC_ERROR
                                        cmeeMode = 1;
#endif
                                        uAtClientLock(atHandle);
                                        uAtClientCommandStart(atHandle, "ATE0");
                                        uAtClientCommandStopReadResponse(atHandle);
                                        uAtClientCommandStart(atHandle, "AT+CMEE=");
                                        uAtClientWriteInt(atHandle, cmeeMode);
                                        uAtClientCommandStopReadResponse(atHandle);
                                        errorCode = uAtClientUnlock(atHandle);
                                        if (errorCode == 0) {
                                            // Let GNSS update any AT handles it may hold
                                            uGnssUpdateAtHandle(pContext->savedAtHandle, atHandle);
                                        }
                                    } else {
                                        // Recover on error
                                        uAtClientRemove(atHandle);
                                        atHandle = pContext->savedAtHandle;
                                    }
                                } else {
                                    // Recover on error
                                    atHandle = pContext->savedAtHandle;
                                }
                            }
                        }
                    }
                    if (errorCode < 0) {
                        // Clean up and unlock the AT client on error
                        uCellMuxPrivateCloseChannel(pContext, U_CELL_MUX_PRIVATE_CHANNEL_ID_AT);
                        // Closing the control channel will take us out of CMUX mode
                        uCellMuxPrivateCloseChannel(pContext, U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL);
                        uAtClientUrcHandlerHijackExt(atHandle, NULL, NULL);
                        pContext->savedAtHandle = NULL;
                        uAtClientUnlock(atHandle);
                    }
                }
            }
        }
    }

    return errorCode;
}

// Determine if the multiplexer is currently enabled.
bool uCellMuxPrivateIsEnabled(uCellPrivateInstance_t *pInstance)
{
    bool isEnabled = false;
    uCellMuxPrivateContext_t *pContext;

    if ((pInstance != NULL) && (pInstance->pMuxContext != NULL)) {
        pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
        isEnabled = (pContext->savedAtHandle != NULL);
    }

    return isEnabled;
}

// Add a multiplexer channel.
int32_t uCellMuxPrivateAddChannel(uCellPrivateInstance_t *pInstance,
                                  int32_t channel,
                                  uDeviceSerial_t **ppDeviceSerial)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellMuxPrivateContext_t *pContext;

    if ((pInstance != NULL) && (ppDeviceSerial != NULL) &&
        (channel != U_CELL_MUX_PRIVATE_CHANNEL_ID_CONTROL) &&
        (channel != U_CELL_MUX_PRIVATE_CHANNEL_ID_AT) &&
        ((channel <= U_CELL_MUX_PRIVATE_ADDRESS_MAX) ||
         (channel == U_CELL_MUX_CHANNEL_ID_GNSS))) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
        if (pInstance->pMuxContext != NULL) {
            pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
            if (pContext->savedAtHandle != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (channel == U_CELL_MUX_CHANNEL_ID_GNSS) {
                    channel = pContext->channelGnss;
                }
                if (channel >= 0) {
                    errorCode = openChannel(pContext, (uint8_t) channel,
                                            U_CELL_MUX_PRIVATE_VIRTUAL_SERIAL_BUFFER_LENGTH_BYTES);
                    if (errorCode == 0) {
#ifdef U_CELL_MUX_ENABLE_DEBUG
                        uPortLog("U_CELL_CMUX_%d: channel added.\n", channel);
#endif
                        *ppDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, (uint8_t) channel);
                    }
                }
            }
        }
    }

    return errorCode;
}

// Disable multiplexer mode.  This involves a few steps:
//
// 1. Send DISC on the virtual serial interface of any currently
//    open channels and close the virtual serial interfaces;
//    do channel 0, the control interface, last and it will
//    end CMUX mode.
// 2. Move AT client operations back to the original AT client.
// 3. DO NOT free memory; only uCellMuxPrivateRemoveContext() does
//    that, to ensure thread-safety.
int32_t uCellMuxPrivateDisable(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    uCellMuxPrivateContext_t *pContext;
    uCellMuxPrivateChannelContext_t *pChannelContext;

    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (pInstance->pMuxContext != NULL) {
            pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
            // Start from the top, so that we do channel 0, which
            // will always be at index 0, last
            for (int32_t x = (sizeof(pContext->pDeviceSerial) / sizeof(pContext->pDeviceSerial[0])) - 1;
                 x >= 0; x--) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(
                                      pContext->pDeviceSerial[x]);
                if (pChannelContext != NULL) {
                    uCellMuxPrivateCloseChannel(pContext, pChannelContext->channel);
                }
            }
            if (pContext->savedAtHandle != NULL) {
                // Copy the settings of the AT handler on channel 1
                // back into the original one, in case they have changed
                errorCode = uCellMuxPrivateCopyAtClient(atHandle, pContext->savedAtHandle);
                // While we set the error code above, there's not a whole lot
                // we can do if this fails, so continue anyway; close the
                // AT handler that was on channel 1
                uAtClientIgnoreAsync(atHandle);
                uAtClientRemove(atHandle);
                // Unhijack the old AT handler and unlock it
                atHandle = pContext->savedAtHandle;
                uAtClientUrcHandlerHijackExt(atHandle, NULL, NULL);
                uAtClientUnlock(atHandle);
                // Let GNSS update any AT handles it may hold
                uGnssUpdateAtHandle(pInstance->atHandle, atHandle);
                pInstance->atHandle = atHandle;
                pContext->savedAtHandle = NULL;
#ifdef U_CELL_MUX_ENABLE_DEBUG
                uPortLog("U_CELL_CMUX: closed.\n");
#endif
            }
            // Give the module a moment for the MUX switcheroo
            uPortTaskBlock(U_CELL_MUX_PRIVATE_ENABLE_DISABLE_DELAY_MS);
        }
    }

    return (int32_t) errorCode;
}

// Get the serial device for the given channel.
uDeviceSerial_t *pUCellMuxPrivateGetDeviceSerial(uCellMuxPrivateContext_t *pContext,
                                                 uint8_t channel)
{
    uDeviceSerial_t *pDeviceSerial = NULL;
    uCellMuxPrivateChannelContext_t *pChannelContext;

    if ((pContext != NULL) && (channel <= U_CELL_MUX_PRIVATE_CHANNEL_ID_MAX)) {
        for (size_t x = 0;
             (x < sizeof(pContext->pDeviceSerial) / sizeof(pContext->pDeviceSerial[0])) &&
             (pDeviceSerial == NULL); x++) {
            if (pContext->pDeviceSerial[x] != NULL) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(
                                      pContext->pDeviceSerial[x]);
                if ((pChannelContext != NULL) && !pChannelContext->markedForDeletion &&
                    (pChannelContext->channel == channel)) {
                    pDeviceSerial = pContext->pDeviceSerial[x];
                }
            }
        }
    }

    return pDeviceSerial;
}

// Close a CMUX channel.
void uCellMuxPrivateCloseChannel(uCellMuxPrivateContext_t *pContext, uint8_t channel)
{
    uDeviceSerial_t *pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext, channel);

    if (pDeviceSerial != NULL) {
        pDeviceSerial->close(pDeviceSerial);
#ifdef U_CELL_MUX_ENABLE_DEBUG
        uPortLog("U_CELL_CMUX_%d: channel closed.\n", channel);
#endif
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uCellMuxPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Enable multiplexer mode.
int32_t uCellMuxEnable(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = uCellMuxPrivateEnable(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Determine if the multiplexer is currently enabled.
bool uCellMuxIsEnabled(uDeviceHandle_t cellHandle)
{
    bool isEnabled = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isEnabled = uCellMuxPrivateIsEnabled(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isEnabled;
}

// Add a multiplexer channel.
int32_t uCellMuxAddChannel(uDeviceHandle_t cellHandle,
                           int32_t channel,
                           uDeviceSerial_t **ppDeviceSerial)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = uCellMuxPrivateAddChannel(pInstance, channel, ppDeviceSerial);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the serial device handle for an open multiplexer channel.
uDeviceSerial_t *pUCellMuxChannelGetDeviceSerial(uDeviceHandle_t cellHandle,
                                                 int32_t channel)
{
    uDeviceSerial_t *pDeviceSerial = NULL;
    uCellPrivateInstance_t *pInstance;
    uCellMuxPrivateContext_t *pContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) &&
            ((channel <= U_CELL_MUX_PRIVATE_ADDRESS_MAX) ||
             (channel == U_CELL_MUX_CHANNEL_ID_GNSS))) {
            if (pInstance->pMuxContext != NULL) {
                pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
                if (pContext->savedAtHandle != NULL) {
                    if (channel == U_CELL_MUX_CHANNEL_ID_GNSS) {
                        channel = pContext->channelGnss;
                    }
                    pDeviceSerial = pUCellMuxPrivateGetDeviceSerial(pContext,
                                                                    (uint8_t) channel);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return pDeviceSerial;
}

// Remove a multiplexer channel.
int32_t uCellMuxRemoveChannel(uDeviceHandle_t cellHandle,
                              uDeviceSerial_t *pDeviceSerial)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellMuxPrivateContext_t *pContext;
    uCellMuxPrivateChannelContext_t *pChannelContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pDeviceSerial != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pMuxContext != NULL) {
                pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
                if (pContext->savedAtHandle != NULL) {
                    pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(pDeviceSerial);
                    uCellMuxPrivateCloseChannel(pContext, pChannelContext->channel);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Disable multiplexer mode.
int32_t uCellMuxDisable(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = uCellMuxPrivateDisable(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return (int32_t) errorCode;
}

// Free memory.
void uCellMuxFree(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    uCellMuxPrivateContext_t *pContext;
    uCellMuxPrivateChannelContext_t *pChannelContext;
    size_t inUseCount = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pMuxContext != NULL)) {
            pContext = (uCellMuxPrivateContext_t *) pInstance->pMuxContext;
            for (size_t x = 0; x < (sizeof(pContext->pDeviceSerial) / sizeof(pContext->pDeviceSerial[0]));
                 x++) {
                pChannelContext = (uCellMuxPrivateChannelContext_t *) pUInterfaceContext(
                                      pContext->pDeviceSerial[x]);
                if (pChannelContext != NULL) {
                    if (pChannelContext->markedForDeletion) {
                        uPortMutexDelete(pChannelContext->mutexUserDataWrite);
                        uPortMutexDelete(pChannelContext->mutexUserDataRead);
                        uPortMutexDelete(pChannelContext->mutex);
                        uDeviceSerialDelete(pContext->pDeviceSerial[x]);
                        pContext->pDeviceSerial[x] = NULL;
                    } else {
                        inUseCount++;
                    }
                }
            }
            if (inUseCount == 0) {
                uCellMuxPrivateRemoveContext(pInstance);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

}

// Abort multiplexer mode in the module.
int32_t uCellMuxModuleAbort(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uDeviceSerial_t *pDeviceSerial;
    uAtClientHandle_t atHandle;
    uAtClientStreamHandle_t stream = U_AT_CLIENT_STREAM_HANDLE_DEFAULTS;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientStreamGetExt(atHandle, &stream);
            switch (stream.type) {
                case U_AT_CLIENT_STREAM_TYPE_UART:
                    errorCode = uPortUartWrite(stream.handle.int32,
                                               gMuxCldCommandFrame,
                                               sizeof(gMuxCldCommandFrame));
                    if (errorCode == sizeof(gMuxCldCommandFrame)) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                    break;
                case U_AT_CLIENT_STREAM_TYPE_VIRTUAL_SERIAL:
                    pDeviceSerial = stream.handle.pDeviceSerial;
                    errorCode = pDeviceSerial->write(pDeviceSerial,
                                                     gMuxCldCommandFrame,
                                                     sizeof(gMuxCldCommandFrame));
                    if (errorCode == sizeof(gMuxCldCommandFrame)) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                    break;
                default:
                    break;
            }
            uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
