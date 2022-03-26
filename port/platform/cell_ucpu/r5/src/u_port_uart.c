/*
 * Copyright 2020 u-blox Ltd
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

/** @file
 * @brief Implementation of the port UART API for the sarar5ucpu platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "txm_module.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "ucpu_modem_api.h"
#include "u_port_event_queue.h"
#include "u_port_clib_platform_specific.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of UARTs supported, which is the range of the
 * "uart" parameter on this platform.
 */
#define U_PORT_UART_MAX_NUM 1

/** The maximum data size for AT command to write on uart in single
 * iteration.
 */
#define U_PORT_AT_MAX_DATA_SIZE (UCPU_MAX_AT_CMD_LENGTH - 64)

/** The maximum size of AT response buffer.
 */
#define UCPU_RESP_BUFFER_SIZE (UCPU_MAX_AT_RESP_LENGTH * 4)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART.
 */
typedef struct {
    int32_t uart;
    int32_t uartHandle;
    uPortMutexHandle_t queue; /**< Also used as a marker that this UART is in use. */
    bool markedForDeletion; /**< If true this UART should NOT be used. */
    uPortTaskHandle_t eventTaskHandle;
    uPortMutexHandle_t eventTaskRunningMutex;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
} uPortUartEvent_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect UART data.
 */
static uPortMutexHandle_t gMutex = NULL;

/** The UART data. Note that either uart or handle can be
 * used as an index into the array, they are synonymous on
 * this platform.
 */
static uPortUartData_t gUartData[U_PORT_UART_MAX_NUM];

/** Buffer to store AT response.
 */
static uint8_t respBuff[UCPU_RESP_BUFFER_SIZE] = {0};

/** Read pointer to response buffer.
 */
static uint8_t *pRespRead = respBuff;

/** Write pointer to response buffer.
 */
static uint8_t *pRespWrite = respBuff;

/** Uart event information.
 */
static uPortUartEvent_t uartEvent;

/** Flag to indicate event callback is in process.
 */
static bool inEventCallback = false;

/** Flag to handle buffer full condition.
 */
static bool aboutToFull = false;

/** The event queue for URCs.
 */
static int32_t gURCEventQueueHandle;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// AT command callback.
// pCallbackData:  Pointer to received AT response string.
// unUsed: Additional parameter which is not used in modem AT case.
static void atCommandCallback(uint32_t pCallbackData, uint32_t unUsed)
{
    uint32_t len = 0;
    st_ucpu_at_resp_t *pAtResp;
    uint32_t urcFlag;

    U_PORT_MUTEX_LOCK(gMutex);

    pAtResp = (st_ucpu_at_resp_t *) pCallbackData;
    urcFlag = pAtResp->urc_flag;
    len = pAtResp->resp_len;
    uPortLog("atCommandCallback() Port Layer type: %d len: %d data: %s.", urcFlag, pAtResp->resp_len,
             (char *) pAtResp->at_resp_buffer);
    if (pRespWrite >= pRespRead) {
        if (len <= (UCPU_RESP_BUFFER_SIZE - (pRespWrite - respBuff))) {
            memcpy(pRespWrite, pAtResp->at_resp_buffer, pAtResp->resp_len);
            pRespWrite = (uint8_t *)pRespWrite + len;
        } else {
            uint32_t tempLen = len - (UCPU_RESP_BUFFER_SIZE - (pRespWrite - respBuff));
            if ((pRespRead - respBuff) > tempLen) {
                uint32_t offset = 0;
                tempLen = UCPU_RESP_BUFFER_SIZE - (pRespWrite - respBuff);
                memcpy(pRespWrite, pAtResp->at_resp_buffer, tempLen); // Copy data at the end of buffer.
                offset = tempLen;
                pRespWrite = respBuff;

                tempLen = len - tempLen;
                memcpy(pRespWrite, (uint8_t *)pAtResp->at_resp_buffer + offset,
                       tempLen); // Copy remaining data at the start of buffer.
                pRespWrite = (uint8_t *)pRespWrite + tempLen;
                aboutToFull = true;
            } else {
                uPortLog("Overflow 1.");
                // Overflow.
                (void) pCallbackData;
            }
        }
    } else {
        if (len < (pRespRead - pRespWrite)) {
            memcpy(pRespWrite, pAtResp->at_resp_buffer, pAtResp->resp_len);
            pRespWrite = (uint8_t *)pRespWrite + len;
        } else {
            uPortLog("Overflow 2.");
            // Overflow.
            (void) pCallbackData;
        }
    }
    U_PORT_MUTEX_UNLOCK(gMutex);

    // Enqueue event callback
    if (urcFlag &&
        !gUartData[uartEvent.uartHandle].markedForDeletion &&
        (gUartData[uartEvent.uartHandle].pEventCallback != NULL) &&
        (gUartData[uartEvent.uartHandle].pEventCallbackParam != NULL) &&
        (gUartData[uartEvent.uartHandle].eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {

        int32_t errorCode = U_ERROR_COMMON_SUCCESS;

        errorCode = uPortEventQueueSend(gURCEventQueueHandle, NULL, 0);
        uPortLog("atCommandCallback() Enqueue in URC Event Queue: %d.", errorCode);
    }

    uPortLog("atCommandCallback() Data copy completed.");
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the UART driver.
int32_t uPortUartInit()
{
    int32_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        for (size_t x = 0; x < sizeof(gUartData) / sizeof(gUartData[0]); x++) {
            gUartData[x].queue = NULL;
            gUartData[x].markedForDeletion = false;
        }
    }

    return errorCode;
}

// Deinitialise the UART driver.
void uPortUartDeinit()
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        // First, mark all instances for deletion
        for (size_t x = 0; x < sizeof(gUartData) / sizeof(gUartData[0]); x++) {
            gUartData[x].markedForDeletion = true;
        }
        // Release the mutex so that deletion can occur
        U_PORT_MUTEX_UNLOCK(gMutex);

        // Delete the mutex
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open a UART instance.
int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t receiveBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts)
{
    int32_t handleOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) &&
            (uart < sizeof(gUartData) / sizeof(gUartData[0]))) {
            handleOrErrorCode = U_ERROR_COMMON_PLATFORM;

            gUartData[uart].markedForDeletion = false;
            gUartData[uart].eventTaskHandle = NULL;
            gUartData[uart].eventTaskRunningMutex = NULL;
            gUartData[uart].pEventCallback = NULL;
            gUartData[uart].pEventCallbackParam = NULL;
            gUartData[uart].eventFilter = 0;

            // Initialize AT layer
            handleOrErrorCode = ucpu_at_init((void *)atCommandCallback);
            uPortLog("AT Initialization - result = %u\r\n", handleOrErrorCode);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            // Mark the UART for deletion within the mutex
            gUartData[handle].markedForDeletion = true;
            gUartData[handle].pEventCallback = NULL;
            gUartData[handle].pEventCallbackParam = NULL;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            if (pRespWrite > pRespRead) {
                sizeOrErrorCode = pRespWrite - pRespRead;
            } else if (pRespWrite == pRespRead) {
                if (aboutToFull == true) {
                    sizeOrErrorCode = UCPU_RESP_BUFFER_SIZE;
                } else {
                    sizeOrErrorCode = 0;
                }
            } else {
                sizeOrErrorCode = UCPU_RESP_BUFFER_SIZE - (pRespRead - pRespWrite);
            }

            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    size_t thisSize;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (sizeBytes > 0)  &&
            !gUartData[handle].markedForDeletion) {
            sizeOrErrorCode = U_ERROR_COMMON_SUCCESS;
            if ((pRespRead == pRespWrite) && (aboutToFull == false)) {
                // No data.
            } else if (pRespWrite > pRespRead) {
                thisSize = pRespWrite - pRespRead;
                if (sizeBytes < thisSize) {
                    thisSize = sizeBytes;
                }
                memcpy(pBuffer, pRespRead, thisSize);
                pBuffer = (uint8_t *)pBuffer + thisSize;
                pRespRead = (uint8_t *)pRespRead + thisSize;
                sizeOrErrorCode = thisSize;
            } else {
                thisSize = UCPU_RESP_BUFFER_SIZE - (pRespRead - pRespWrite);
                if (sizeBytes < thisSize) {
                    thisSize = sizeBytes;
                }

                if (thisSize < (UCPU_RESP_BUFFER_SIZE - (pRespRead - respBuff))) {
                    memcpy(pBuffer, pRespRead, thisSize);
                    pBuffer = (uint8_t *)pBuffer + thisSize;
                    pRespRead = (uint8_t *)pRespRead + thisSize;
                    sizeOrErrorCode = thisSize;
                } else {
                    size_t tempSize;

                    tempSize = (UCPU_RESP_BUFFER_SIZE - (pRespRead - respBuff));
                    memcpy(pBuffer, pRespRead, tempSize); // Copy data from the end of buffer.
                    pBuffer = (uint8_t *)pBuffer + tempSize;
                    pRespRead = respBuff;

                    tempSize = thisSize - tempSize;
                    memcpy(pBuffer, pRespRead, tempSize); // Copy remaining data from the start of buffer.
                    pBuffer = (uint8_t *)pBuffer + tempSize;
                    pRespRead = (uint8_t *)pRespRead + tempSize;
                    sizeOrErrorCode = thisSize;
                    aboutToFull = false;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t handle,
                       const void *pBuffer,
                       size_t sizeBytes)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    size_t writeLen = 0;
    size_t offset = 0;
    static uint8_t atCommandStr[UCPU_MAX_AT_CMD_LENGTH];

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (handle >= 0) &&
            (!gUartData[handle].markedForDeletion)) {
            // Run around the loop until all packets of data send.
            do {
                if (sizeBytes >= U_PORT_AT_MAX_DATA_SIZE) {
                    writeLen = U_PORT_AT_MAX_DATA_SIZE;
                } else {
                    writeLen = sizeBytes;
                }

                memset((uint8_t *) atCommandStr, 0, UCPU_MAX_AT_CMD_LENGTH);
                memcpy(atCommandStr, (uint8_t *) pBuffer + offset, writeLen);
                ucpu_at_send_cmd((uint8_t *) atCommandStr, writeLen);
                sizeBytes -= writeLen;
                offset += writeLen;
            } while (sizeBytes > 0);

            sizeOrErrorCode = offset;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    uPortTaskBlock(25); // Little delay required to process data
    return sizeOrErrorCode;
}

// Callback for the urc event queue.
static void urcEventQueueCallback(void *pParameters, size_t paramLength)
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        // Call event callback
        if (!gUartData[uartEvent.uartHandle].markedForDeletion &&
            (gUartData[uartEvent.uartHandle].pEventCallback != NULL) &&
            (gUartData[uartEvent.uartHandle].pEventCallbackParam != NULL) &&
            (gUartData[uartEvent.uartHandle].eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            inEventCallback = true;
            uPortLog("urcEventQueueCallback() Invoking the URC Callback.");
            gUartData[uartEvent.uartHandle].pEventCallback(uartEvent.uartHandle,
                                                           uartEvent.eventBitMap,
                                                           gUartData[uartEvent.uartHandle].pEventCallbackParam);
            inEventCallback = false;
        }
        uPortLog("urcEventQueueCallback() Data copy completed.");

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set an event callback.
int32_t uPortUartEventCallbackSet(int32_t handle,
                                  uint32_t filter,
                                  void (*pFunction)(int32_t,
                                                    uint32_t,
                                                    void *),
                                  void *pParam,
                                  size_t stackSizeBytes,
                                  int32_t priority)
{
    int32_t errorCodeOrHandle = U_ERROR_COMMON_NOT_INITIALISED;

    if ((handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
        !gUartData[handle].markedForDeletion &&
        (gUartData[handle].eventTaskRunningMutex == NULL) &&
        (filter != 0) && (pFunction != NULL)) {

        if (gMutex != NULL) {
            U_PORT_MUTEX_LOCK(gMutex);

            // Create URC Thread
            gURCEventQueueHandle = uPortEventQueueOpen(urcEventQueueCallback,
                                                       "urcCallbacks",
                                                       sizeof(uint32_t),
                                                       stackSizeBytes,
                                                       priority,
                                                       10);
            if (gURCEventQueueHandle < 0) {
                uPortLog("uPortEventQueueOpen() failed for URC Event Queue %d\n", gURCEventQueueHandle);
                //assert(false);
            }

            gUartData[handle].pEventCallback = pFunction;
            gUartData[handle].pEventCallbackParam = pParam;
            gUartData[handle].eventFilter = filter;
            uartEvent.uartHandle = handle;
            uartEvent.eventBitMap = filter;
            errorCodeOrHandle = U_ERROR_COMMON_SUCCESS;

            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }
    return errorCodeOrHandle;
}

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            gUartData[handle].pEventCallback = NULL;
            gUartData[handle].pEventCallbackParam = NULL;
            gUartData[handle].eventFilter = 0;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Get the callback filter bit-mask.
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            filter = gUartData[handle].eventFilter;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return filter;
}

// Change the callback filter bit-mask.
int32_t uPortUartEventCallbackFilterSet(int32_t handle,
                                        uint32_t filter)
{
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (filter != 0)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            gUartData[handle].eventFilter = filter;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Send an event to the callback.
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartEvent_t event;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].pEventCallback != NULL) &&
            (gUartData[handle].pEventCallbackParam != NULL) &&
            (eventBitMap & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            gUartData[handle].pEventCallback(event.uartHandle,
                                             event.eventBitMap,
                                             gUartData[handle].pEventCallbackParam);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Return true if we're in an event callback.
bool uPortUartEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;

    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            isEventCallback = inEventCallback;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

// Get the stack high watermark for the task on the event queue.
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // yet to implement
    return sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    bool rtsFlowControlIsEnabled = false;

    // Not valid in our case
    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    bool ctsFlowControlIsEnabled = false;

    // Not valid in our case
    return ctsFlowControlIsEnabled;
}

int32_t uPortUartCtsSuspend(int32_t handle)
{
    return U_ERROR_COMMON_NOT_SUPPORTED;
}

void uPortUartCtsResume(int32_t handle)
{
    (void) handle;
}
// End of file
