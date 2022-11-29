/*
 * Copyright 2019-2022 u-blox Ltd
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

/** @file
 * @brief Implementation of the port UART API for the sarar5ucpu platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_YIELD_MS

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"
#include "u_port_clib_platform_specific.h"

#include "ucpu_sdk_modem_uart.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** A macro to check that the respective event has arrived or not.
 */
#define U_PORT_UART_EVENT_CHECK(evenBitmap, event) ((evenBitmap) & (1 << event -1))

/** Size of UART read buffer. Data more than this size will not be
 * dropped or trimmed instead it will be available on next read call,
 * when next read buffer is placed to read data. Change to this value
 * will not impact any functionality it will work seamlessly.
 */
#define U_PORT_UART_READ_BUFFER_SIZE 2048

/** Timeout in milliseconds for UART operations. This timeout is with
 * in the range of u_at_client default timeout which is 8000 ms,
 * U_AT_CLIENT_DEFAULT_TIMEOUT_MS. The UART timeout will always be less
 * than the u_at_client timeouts.
 */
#define U_PORT_UART_TIMEOUT_MS 3000

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of UART context.
 */
typedef struct {
    int32_t uartHandle;              /**< Handle to modem UART interface. */
    bool markedForDeletion;          /**< If true this UART should NOT be used. UART
                                          is marked for delete when UART interface is
                                          closed or received detached event. */
    int32_t eventQueueHandle;        /**< Handle to event queue. */
    uint32_t eventFilter;            /**< A bit-mask to filter the events on which
                                          pEventCallback will be called. In our case
                                          only U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED
                                          bit-mask is supported, means call event callback
                                          with this bit-mask when data on UART is available
                                          to read. */
    void (*pEventCallback)(int32_t,
                           uint32_t,
                           void *);  /**< Event callback, the function to call
                                          when any data received on UART interface. */
    void *pEventCallbackParam;       /**< A parameter which will be passed to pEventCallback
                                          when it is called. */
} uPortUartContext_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle; /**< Handle to UART interface. */
    uint32_t eventBitMap; /**< The events bit-map, only eventBitMap type we support
                               at the moment is U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED. */
} uPortUartEvent_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to make UART operations thread safe.
 */
static uPortMutexHandle_t gModemUartMutex = NULL;

/** Buffer to read AT response. It is not a circular buffer.
 * Buffer is flushed when all data is read from it and than it is
 * placed for next read call. Data greater than this buffer size
 * will be available on next read call, when next read buffer is
 * placed to read data. This buffer is mutex protected.
 */
static uint8_t gModemUartReadBuffer[U_PORT_UART_READ_BUFFER_SIZE];

/** Pointer to the gModemUartReadBuffer.
 */
static uint8_t *gpModemUartReadBuffer = gModemUartReadBuffer;

/** Length of data received from UART interface.
 */
static uint32_t gModemUartReceiveBytes = 0;

/** Number of bytes written to UART interface.
 */
static int32_t gModemUartWriteBytes = 0;

/** Handle to timer. Modem UART interface is asynchronous and
 * we wait for event in while loop. The purpose of Timer is to
 * avoid the halt condition or exit the loop, when modem is stucked,
 * or modem takes time more than usual. The default timeout defined
 * is U_PORT_UART_TIMEOUT_MS.
 */
static uPortTimerHandle_t gModemUartTimerHandle = NULL;

/** Flag of timer expiration.
 */
static bool gModemUartTimerTimeout = false;

/** Handle to UART context.
 */
static uPortUartContext_t gModemUartContext;

/** UART event information.
 */
static uPortUartEvent_t gModemUartEvent;

/** UART event bitmap. Event bitmap is mapped to
 * ucpu_sdk_modem_uart_event_type_t, each bit
 * represents corresponding event i.e
 * bit 1 represends UCPU_MODEM_UART_EVENT_ATTACH_CNF event,
 * bit 2 represends UCPU_MODEM_UART_EVENT_DETACH_CNF event,
 * and so on.
 */
static uint32_t gModemUartEventBitmap = 0;

/** Flag to ensure that next read buffer is in place for
 * next data. When all data is read from gModemUartReadBuffer
 * then next read buffer is placed for next available data.
 */
static bool gModemUartReadBufferPlaced = false;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// UART event callback.
static void modemUartEventCallback(uint32_t eventType, uint32_t eventData,
                                   void *pParam)
{
    // Set event bitmap
    gModemUartEventBitmap |= (1 << eventType - 1);

    switch (eventType) {
        case UCPU_MODEM_UART_EVENT_READ_IND: {
            gModemUartReadBufferPlaced = false;
            gModemUartReceiveBytes = eventData;
            // Enqueue event callback
            uPortUartEventSend(gModemUartContext.uartHandle, U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
            break;
        }
        case UCPU_MODEM_UART_EVENT_WRITE_IND: {
            gModemUartWriteBytes = eventData;
            break;
        }
        case UCPU_MODEM_UART_EVENT_DETACH_CNF: {
            if (!gModemUartContext.markedForDeletion) {
                uPortUartDeinit();
            }
            break;
        }
        default: {
            break;
        }
    }
}

// UART timer callback.
static void modemUartTimerCallback(uPortTimerHandle_t timerHandle, void *pParameter)
{
    gModemUartTimerTimeout = true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the UART driver.
int32_t uPortUartInit()
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (gModemUartMutex == NULL) {
        // Reset the UART read buffer
        memset(gModemUartReadBuffer, 0, U_PORT_UART_READ_BUFFER_SIZE);
        gModemUartContext.markedForDeletion = false;
        gModemUartContext.eventQueueHandle = -1;
        gModemUartContext.uartHandle = -1;
        gModemUartContext.eventFilter = 0;
        gModemUartContext.pEventCallback = NULL;
        gModemUartContext.pEventCallbackParam = NULL;

        // Create mutex to protect UART interface
        errorCode = uPortMutexCreate(&gModemUartMutex);
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            // Create timer
            errorCode = uPortTimerCreate(&gModemUartTimerHandle,
                                         "Uart Timer", modemUartTimerCallback,
                                         NULL, U_PORT_UART_TIMEOUT_MS, false);
            if (errorCode < 0) {
                uPortLog("uPortUartInit() Error creating timer.\n");
                (void)uPortMutexDelete(gModemUartMutex);
            }
        } else {
            // Error creating mutex
            uPortLog("uPortUartInit() Error creating mutex.\n");
        }
    }

    return errorCode;
}

// Deinitialise the UART driver.
void uPortUartDeinit()
{
    if (gModemUartMutex != NULL) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);
        // First, mark UART instances for deletion
        gModemUartContext.markedForDeletion = true;
        gModemUartContext.pEventCallback = NULL;
        gModemUartContext.pEventCallbackParam = NULL;
        uPortTimerDelete(gModemUartTimerHandle);
        // Release the mutex so that deletion can occur
        U_PORT_MUTEX_UNLOCK(gModemUartMutex);

        // Delete the mutex
        U_PORT_MUTEX_LOCK(gModemUartMutex);
        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
        uPortMutexDelete(gModemUartMutex);
        gModemUartMutex = NULL;
        gModemUartTimerHandle = NULL;
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
    int32_t errorCode;

    if (gModemUartMutex != NULL) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) &&
            (pReceiveBuffer == NULL)) {

            // Reset all parameters
            memset(gModemUartReadBuffer, 0, U_PORT_UART_READ_BUFFER_SIZE);
            gModemUartContext.markedForDeletion = false;
            gModemUartContext.eventQueueHandle = -1;
            gModemUartContext.uartHandle = -1;
            gModemUartContext.eventFilter = 0;
            gModemUartContext.pEventCallback = NULL;
            gModemUartContext.pEventCallbackParam = NULL;

            handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
            // Open UART interface
            gModemUartContext.uartHandle = ucpu_sdk_modem_uart_open();
            if (gModemUartContext.uartHandle > 0) {
                // Reset event bitmap
                gModemUartEventBitmap = 0;
                gModemUartTimerTimeout = false;

                // Set event callback
                errorCode = ucpu_sdk_modem_uart_set_callback(gModemUartContext.uartHandle,
                                                             modemUartEventCallback,
                                                             NULL);
                if (errorCode == U_ERROR_COMMON_SUCCESS) {
                    // Start timer
                    errorCode = uPortTimerStart(gModemUartTimerHandle);
                    if (errorCode == U_ERROR_COMMON_SUCCESS) {
                        // Wait for UCPU_MODEM_UART_EVENT_ATTACH_CNF/UCPU_MODEM_UART_EVENT_OPEN_FAILURE or timeout
                        while (1) {
                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_ATTACH_CNF) ||
                                U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_OPEN_FAILURE) ||
                                gModemUartTimerTimeout) {
                                break;
                            }
                            uPortTaskBlock(100);
                        }
                        // Stop timer
                        errorCode == uPortTimerStop(gModemUartTimerHandle);
                        if (errorCode == U_ERROR_COMMON_SUCCESS) {
                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_ATTACH_CNF)) {
                                // Reset event bitmap
                                gModemUartEventBitmap = 0;
                                gModemUartTimerTimeout = false;

                                // Place read call to UART interface
                                errorCode = ucpu_sdk_modem_uart_read(gModemUartContext.uartHandle,
                                                                     gModemUartReadBuffer,
                                                                     U_PORT_UART_READ_BUFFER_SIZE);
                                if (errorCode == U_ERROR_COMMON_SUCCESS) {
                                    // Start timer
                                    errorCode = uPortTimerStart(gModemUartTimerHandle);
                                    if (errorCode == U_ERROR_COMMON_SUCCESS) {
                                        // Wait for UCPU_MODEM_UART_EVENT_EWOULD_BLOCK/UCPU_MODEM_UART_EVENT_READ_FAILURE/UCPU_MODEM_UART_EVENT_FAILURE or timeout
                                        while (1) {
                                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_EWOULD_BLOCK) ||
                                                U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_READ_FAILURE) ||
                                                U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE) ||
                                                gModemUartTimerTimeout) {
                                                break;
                                            }
                                            uPortTaskBlock(100);
                                        }
                                        // Stop timer
                                        errorCode == uPortTimerStop(gModemUartTimerHandle);
                                        if (errorCode == U_ERROR_COMMON_SUCCESS) {
                                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_EWOULD_BLOCK)) {
                                                gModemUartReadBufferPlaced = true;
                                                handleOrErrorCode = gModemUartContext.uartHandle;
                                            } else if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_READ_FAILURE) ||
                                                       U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE)) {
                                                // Error reading from UART interface
                                                handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                                                uPortLog("uPortUartOpen() Faild to read from UART interface.\n");
                                            } else {
                                                // Timeout reading from UART interface
                                                handleOrErrorCode = U_ERROR_COMMON_TIMEOUT;
                                                uPortLog("uPortUartOpen() Timeout reading from UART interface.\n");
                                            }
                                        } else {
                                            // Error stoping timer
                                            uPortLog("uPortUartOpen() Failed to stop timer.\n");
                                        }
                                    } else {
                                        // Error starting timer
                                        uPortLog("uPortUartOpen() Failed to start timer.\n");
                                    }
                                } else {
                                    // Error reading from UART interface
                                    uPortLog("uPortUartOpen() Error reading from UART interface.\n");
                                }
                            } else if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_OPEN_FAILURE)) {
                                // Failed to open UART interface
                                handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                                uPortLog("uPortUartOpen() Failed to open UART interface.\n");
                            } else {
                                // Timeout opening UART interface
                                handleOrErrorCode = U_ERROR_COMMON_TIMEOUT;
                                uPortLog("uPortUartOpen() Timeout opening UART interface.\n");
                            }
                        } else {
                            // Error stoping timer
                            uPortLog("uPortUartOpen() Failed to stop timer.\n");
                        }
                    } else {
                        // Error starting timer
                        uPortLog("uPortUartOpen() Failed to start timer.\n");
                    }
                } else {
                    // Error setting platform callback
                    uPortLog("uPortUartOpen() Error setting platform callback.\n");
                }
            } else {
                // Error opening UART interface
                uPortLog("uPortUartOpen() Error opening UART interface.\n");
            }
        } else {
            // Invalid parameter
            uPortLog("uPortUartOpen() Error invalid parameter.\n");
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return handleOrErrorCode;
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    int32_t errorCode;

    if ((handle > 0) &&
        (gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        // Mark the UART for deletion within the mutex
        gModemUartContext.markedForDeletion = true;
        gModemUartContext.pEventCallback = NULL;
        gModemUartContext.pEventCallbackParam = NULL;
        // Reset event bitmap
        gModemUartEventBitmap = 0;
        gModemUartTimerTimeout = false;

        ucpu_sdk_modem_uart_close(handle);

        // Start timer
        errorCode = uPortTimerStart(gModemUartTimerHandle);
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            // Wait for UCPU_MODEM_UART_EVENT_DETACH_CNF/UCPU_MODEM_UART_EVENT_FAILURE or timeout
            while (1) {
                if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_DETACH_CNF) ||
                    U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE) ||
                    gModemUartTimerTimeout) {
                    break;
                }
                uPortTaskBlock(100);
            }
            // Stop timer
            errorCode == uPortTimerStop(gModemUartTimerHandle);
            if (errorCode == U_ERROR_COMMON_SUCCESS) {
                if (!U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_DETACH_CNF)) {
                    // Error closing modem UART interface
                    uPortLog("uPortUartClose() Error closing UART interface.\n");
                }
            } else {
                // Error stoping timer
                uPortLog("uPortUartClose() Failed to stop timer.\n");
            }
        } else {
            // Error starting timer
            uPortLog("uPortUartClose() Failed to start timer.\n");
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    int32_t errorCodeOrSize = U_ERROR_COMMON_NOT_INITIALISED;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        errorCodeOrSize = gModemUartReceiveBytes;

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return errorCodeOrSize;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    int32_t errorCodeOrSize = U_ERROR_COMMON_NOT_INITIALISED;
    size_t thisSize = 0;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        errorCodeOrSize = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle > 0) &&
            (pBuffer != NULL) && (sizeBytes > 0)) {

            errorCodeOrSize = U_ERROR_COMMON_SUCCESS;
            if (gModemUartReceiveBytes > sizeBytes) {
                thisSize = sizeBytes;
            } else {
                thisSize = gModemUartReceiveBytes;
            }

            if (thisSize > 0) {
                // Copy data to buffer
                memcpy(pBuffer, gpModemUartReadBuffer, thisSize);
                gpModemUartReadBuffer += thisSize;
                gModemUartReceiveBytes -= thisSize;
                errorCodeOrSize = thisSize;
            }

            if ((gModemUartReceiveBytes == 0) && (gModemUartReadBufferPlaced == false)) {
                // Reset the UART read buffer
                memset(gModemUartReadBuffer, 0, U_PORT_UART_READ_BUFFER_SIZE);
                gpModemUartReadBuffer = gModemUartReadBuffer;
                // Reset event bitmap
                gModemUartEventBitmap = 0;
                gModemUartTimerTimeout = false;
                gModemUartReadBufferPlaced = true;

                // Read from modem UART interface. It is non-blocking call.
                // When data is available it will be notified via event and
                // data is copied to gModemUartReadBuffer
                errorCodeOrSize = ucpu_sdk_modem_uart_read(handle,
                                                           gModemUartReadBuffer,
                                                           U_PORT_UART_READ_BUFFER_SIZE);
                if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                    // Start timer
                    errorCodeOrSize = uPortTimerStart(gModemUartTimerHandle);
                    if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                        // Wait for UCPU_MODEM_UART_EVENT_EWOULD_BLOCK/UCPU_MODEM_UART_EVENT_READ_IND/UCPU_MODEM_UART_EVENT_FAILURE or timeout
                        while (1) {
                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_EWOULD_BLOCK) ||
                                U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_READ_FAILURE) ||
                                U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE) ||
                                gModemUartTimerTimeout) {
                                break;
                            }
                            uPortTaskBlock(100);
                        }
                        // Stop timer
                        errorCodeOrSize == uPortTimerStop(gModemUartTimerHandle);
                        if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                            if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_EWOULD_BLOCK)) {
                                // Number of bytes read from UART interface
                                errorCodeOrSize = thisSize;
                            } else if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_READ_FAILURE) ||
                                       U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE)) {
                                // Error reading from UART interface
                                errorCodeOrSize = U_ERROR_COMMON_PLATFORM;
                                uPortLog("uPortUartRead() Faild to read from UART interface.\n");
                            } else {
                                // Timeout reading from UART interface
                                errorCodeOrSize = U_ERROR_COMMON_TIMEOUT;
                                uPortLog("uPortUartRead() Timeout reading from UART interface.\n");
                            }
                        } else {
                            // Error stoping timer
                            uPortLog("uPortUartRead() Failed to stop timer.\n");
                        }
                    } else {
                        // Error starting timer
                        uPortLog("uPortUartRead() Failed to start timer.\n");
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return errorCodeOrSize;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t handle,
                       const void *pBuffer,
                       size_t sizeBytes)
{
    int32_t errorCodeOrSize = U_ERROR_COMMON_NOT_INITIALISED;
    size_t writeLen = 0;
    size_t offset = 0;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        gModemUartWriteBytes = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle > 0) &&
            (pBuffer != NULL)) {

            gModemUartWriteBytes = 0;
            // Reset event bitmap
            gModemUartEventBitmap = 0;
            gModemUartTimerTimeout = false;

            // Write to the modem UART interface. It is non-blocking call.
            // When data is written to UART interface it will be notified
            // via event in event callback
            errorCodeOrSize = ucpu_sdk_modem_uart_write(handle,
                                                        (void *) pBuffer,
                                                        sizeBytes);
            if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                // Start timer
                errorCodeOrSize = uPortTimerStart(gModemUartTimerHandle);
                if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                    // Wait for UCPU_MODEM_UART_EVENT_WRITE_IND/UCPU_MODEM_UART_EVENT_FAILURE or timeout
                    while (1) {
                        if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_WRITE_IND) ||
                            U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE) ||
                            gModemUartTimerTimeout) {
                            break;
                        }
                        uPortTaskBlock(100);
                    }
                    // Stop timer
                    errorCodeOrSize == uPortTimerStop(gModemUartTimerHandle);
                    if (errorCodeOrSize == U_ERROR_COMMON_SUCCESS) {
                        if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_WRITE_IND)) {
                            // Number of bytes written to UART interface
                            errorCodeOrSize = gModemUartWriteBytes;
                        } else if (U_PORT_UART_EVENT_CHECK(gModemUartEventBitmap, UCPU_MODEM_UART_EVENT_FAILURE)) {
                            // Error reading from UART interface
                            errorCodeOrSize = U_ERROR_COMMON_PLATFORM;
                            uPortLog("uPortUartWrite() Faild to write to UART interface.\n");
                        } else {
                            // Timeout reading from UART interface
                            errorCodeOrSize = U_ERROR_COMMON_TIMEOUT;
                            uPortLog("uPortUartWrite() Timeout writing to UART interface.\n");
                        }
                    } else {
                        // Error stoping timer
                        uPortLog("uPortUartWrite() Failed to stop timer.\n");
                    }
                } else {
                    // Error starting timer
                    uPortLog("uPortUartWrite() Failed to start timer.\n");
                }
            } else {
                // Error writing to UART interface
                uPortLog("uPortUartWrite() Failed to write to UART interface.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return errorCodeOrSize;
}

// Event handler, calls the user's event callback.
static void eventHandler(void *pParam, size_t paramLength)
{
    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion) &&
        (gModemUartContext.pEventCallback != NULL) &&
        (gModemUartContext.pEventCallbackParam != NULL) &&
        (gModemUartContext.eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {

        // Don't need to worry about locking the mutex here,
        // the user callback will be able to access functions in this
        // API which will lock the mutex before doing any action.

        // Call event callback
        gModemUartContext.pEventCallback(gModemUartEvent.uartHandle,
                                         gModemUartEvent.eventBitMap,
                                         gModemUartContext.pEventCallbackParam);
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
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle > 0) &&
            (filter != 0) && (pFunction != NULL)) {

            errorCode = U_ERROR_COMMON_PLATFORM;
            // Open event queue
            errorCode = uPortEventQueueOpen(eventHandler,
                                            "eventUart",
                                            sizeof(uint32_t),
                                            stackSizeBytes,
                                            priority,
                                            U_PORT_UART_EVENT_QUEUE_SIZE);
            if (errorCode >= 0) {
                gModemUartContext.pEventCallback = pFunction;
                gModemUartContext.pEventCallbackParam = pParam;
                gModemUartContext.eventQueueHandle = (int32_t) errorCode;
                gModemUartContext.eventFilter = filter;
                gModemUartEvent.uartHandle = handle;
                gModemUartEvent.eventBitMap = filter;
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else {
                uPortLog("uPortUartEventCallbackSet() Failed to open event queue = %d.\n", errorCode);
            }

            U_PORT_MUTEX_UNLOCK(gModemUartMutex);
        }
    }

    return errorCode;
}

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    int32_t eventQueueHandle = -1;
    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        if ((handle > 0) &&
            (gModemUartContext.eventQueueHandle >= 0)) {
            // Save the eventQueueHandle and set all
            // the parameters to indicate that the
            // queue is closed
            eventQueueHandle = gModemUartContext.eventQueueHandle;
            // Remove user event callback
            gModemUartContext.eventQueueHandle = -1;
            gModemUartContext.pEventCallback = NULL;
            gModemUartContext.pEventCallbackParam = NULL;
            gModemUartContext.eventFilter = 0;
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }
    // Now close the event queue
    // outside the gMutex lock.  Reason for this
    // is that the event task could be calling
    // back into here and we don't want it
    // blocked by us we'll get stuck.
    if (eventQueueHandle >= 0) {
        uPortEventQueueClose(eventQueueHandle);
    }
}

// Get the callback filter bit-mask.
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        if ((handle > 0) &&
            (gModemUartContext.eventQueueHandle >= 0)) {
            filter = gModemUartContext.eventFilter;
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return filter;
}

// Change the callback filter bit-mask.
int32_t uPortUartEventCallbackFilterSet(int32_t handle, uint32_t filter)
{
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle > 0) &&
            (filter != 0) &&
            (gModemUartContext.eventQueueHandle >= 0)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            gModemUartContext.eventFilter = filter;
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return errorCode;
}

// Send an event to the callback.
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

        if ((handle > 0) &&
            (gModemUartContext.eventQueueHandle >= 0) &&
            (gModemUartContext.pEventCallback != NULL) &&
            (gModemUartContext.pEventCallbackParam != NULL) &&
            (eventBitMap & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            errorCode = U_ERROR_COMMON_SUCCESS;

            // Push an event to event queue
            errorCode = uPortEventQueueSend(gModemUartContext.eventQueueHandle, NULL, 0);
        }
    }

    return errorCode;
}

// Send an event to the callback, non-blocking version.
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    int64_t startTime = uPortGetTickTimeMs();

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

        if ((handle > 0) &&
            (gModemUartContext.eventQueueHandle >= 0) &&
            (gModemUartContext.pEventCallback != NULL) &&
            (gModemUartContext.pEventCallbackParam != NULL) &&
            (eventBitMap & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {

            do {
                // Push an event to event queue, IRQ version so as not to block
                errorCode = uPortEventQueueSendIrq(gModemUartContext.eventQueueHandle,
                                                   NULL, 0);
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
            } while ((errorCode != 0) &&
                     (uPortGetTickTimeMs() - startTime < delayMs));
        }
    }

    return errorCode;
}

// Return true, if we're in an event callback.
bool uPortUartEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;

    if ((gModemUartMutex != NULL) &&
        (!gModemUartContext.markedForDeletion)) {
        U_PORT_MUTEX_LOCK(gModemUartMutex);

        if ((handle > 0) &&
            (gModemUartContext.eventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(gModemUartContext.eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gModemUartMutex);
    }

    return isEventCallback;
}

// Get the stack high watermark for the task on the event queue.
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    // Yet to implement
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

// Suspend CTS flow control.
int32_t uPortUartCtsSuspend(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;

    // Not valid in our case
    return sizeOrErrorCode;
}

// Resume CTS flow control.
void uPortUartCtsResume(int32_t handle)
{
    // Not valid in our case
}

// End of file
