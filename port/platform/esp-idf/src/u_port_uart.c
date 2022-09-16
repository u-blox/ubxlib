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

/** @file
 * @brief Implementation of the port UART API for the ESP32 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "driver/uart.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of UARTs supported, which is the range of the
 * "uart" parameter on this platform.
 */
#define U_PORT_UART_MAX_NUM 3

/** Define a minimum task stack size for this port (since we're
 * not using the event queue here).
 */
#define U_PORT_UART_EVENT_MIN_TASK_STACK_SIZE_BYTES 768

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART.
 */
typedef struct {
    QueueHandle_t queue; /**< Also used as a marker that this UART is in use. */
    bool markedForDeletion; /**< If true this UART should NOT be used. */
    bool ctsSuspended;
    uPortTaskHandle_t eventTaskHandle;
    uPortMutexHandle_t eventTaskRunningMutex;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
} uPortUartData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect UART data.
 */
static uPortMutexHandle_t gMutex = NULL;

/** The UART data.  Note that either uart or handle can be
 * used as an index into the array, they are synonymous on
 * this platform.
 */
static uPortUartData_t gUartData[U_PORT_UART_MAX_NUM];

/** Convert an ESP32 event into one of our bit-map events.
 * Only UART_DATA supported at the moment.
 */
static uint32_t gEsp32EventToEvent[] = { U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED /* UART_DATA */};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get one of our UART events from an ESP32 UART event.
static uint32_t getEventFromEsp32Event(uart_event_type_t esp32Event)
{
    uint32_t event = 0;

    if ((esp32Event >= 0) &&
        (esp32Event < sizeof(gEsp32EventToEvent) / sizeof(gEsp32EventToEvent[0]))) {
        event = gEsp32EventToEvent[esp32Event];
    }

    return event;
}

// Get the event task and its associated OS thingies
// to exit
// Note: gMutex should !!!NOT!!! be locked before this is called.
static void deleteEventTaskRequiresMutex(int32_t handle)
{
    uart_event_t uartEvent;

    if (gUartData[handle].eventTaskRunningMutex != NULL) {
        // If there is an event task running, get it to
        // exit by sending it an illegal event type.
        // Note that the task may have called back into
        // this driver and be about to wait on gMutex,
        // which we already have, leading to it getting
        // stuck.  Hence the requirement NOT to have a lock
        // on gMutex in this task by the time it gets to here.
        uartEvent.type = UART_EVENT_MAX;
        uartEvent.size = 0;
        uPortQueueSend((const uPortQueueHandle_t) gUartData[handle].queue,
                       (void *) &uartEvent);
        // Make sure the message gets there, in case the task
        // is running at a lower priority than us
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        U_PORT_MUTEX_LOCK(gUartData[handle].eventTaskRunningMutex);
        U_PORT_MUTEX_UNLOCK(gUartData[handle].eventTaskRunningMutex);
        // Delete the mutex
        uPortMutexDelete(gUartData[handle].eventTaskRunningMutex);
        gUartData[handle].eventTaskRunningMutex = NULL;
    }
}

// Close a UART instance
// Note: gMutex should !!!NOT!!! be locked before this is called.
static void uartCloseRequiresMutex(int32_t handle)
{
    if ((handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
        (gUartData[handle].queue != NULL)) {

        // Delete the event task, if there is one
        deleteEventTaskRequiresMutex(handle);

        // Shut down the driver, which will delete the queue
        uart_driver_delete(handle);

        // Set queue to NULL to mark this as free
        gUartData[handle].queue = NULL;
    }
}

// Event handler.  If an event callback is registered for a UART
// this will be run in a task of its own for that UART.
static void eventTask(void *pParam)
{
    uart_event_t event;
    int32_t handle = (int32_t) pParam;
    uint32_t eventBitMask;

    U_PORT_MUTEX_LOCK(gUartData[handle].eventTaskRunningMutex);

    do {
        if (uPortQueueReceive((const uPortQueueHandle_t) gUartData[handle].queue,
                              &event) == 0) {
            // Check if it is in the filter
            eventBitMask = getEventFromEsp32Event(event.type);
            if (eventBitMask & gUartData[handle].eventFilter) {
                // Call the callback
                if (gUartData[handle].pEventCallback != NULL) {
                    gUartData[handle].pEventCallback(handle, eventBitMask,
                                                     gUartData[handle].pEventCallbackParam);
                }
            }
        }
    } while (event.type < UART_EVENT_MAX);

    U_PORT_MUTEX_UNLOCK(gUartData[handle].eventTaskRunningMutex);

    // Delete ourself
    uPortTaskDelete(NULL);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the UART driver.
int32_t uPortUartInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

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
            if (gUartData[x].queue != NULL) {
                gUartData[x].markedForDeletion = true;
            }
        }

        // Release the mutex so that deletion can occur
        U_PORT_MUTEX_UNLOCK(gMutex);

        for (size_t x = 0; x < sizeof(gUartData) / sizeof(gUartData[0]); x++) {
            if (gUartData[x].markedForDeletion) {
                uartCloseRequiresMutex(x);
                gUartData[x].markedForDeletion = false;
            }
        }

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

    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uart_config_t config;
    esp_err_t espError;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) &&
            (uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (baudRate > 0) && (pReceiveBuffer == NULL) &&
            (receiveBufferSizeBytes > 0) &&
            (pinRx >= 0) && (pinTx >= 0) &&
            (gUartData[uart].queue == NULL)) {

            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

            gUartData[uart].markedForDeletion = false;
            gUartData[uart].ctsSuspended = false;
            gUartData[uart].eventTaskHandle = NULL;
            gUartData[uart].eventTaskRunningMutex = NULL;
            gUartData[uart].pEventCallback = NULL;
            gUartData[uart].pEventCallbackParam = NULL;
            gUartData[uart].eventFilter = 0;

            // Set the things that won't change
            config.data_bits  = UART_DATA_8_BITS;
            config.stop_bits  = UART_STOP_BITS_1;
            config.parity     = UART_PARITY_DISABLE;
#if SOC_UART_SUPPORT_REF_TICK
            config.source_clk = UART_SCLK_REF_TICK;
#elif SOC_UART_SUPPORT_XTAL_CLK
            config.source_clk = UART_SCLK_XTAL;
#else
            config.source_clk = UART_SCLK_APB;
#endif

            // Set the baud rate
            config.baud_rate = baudRate;

            // Set flow control
            config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
            config.rx_flow_ctrl_thresh = U_CFG_HW_CELLULAR_RTS_THRESHOLD;
            if ((pinCts >= 0) && (pinRts >= 0)) {
                config.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
            } else if (pinCts >= 0) {
                config.flow_ctrl = UART_HW_FLOWCTRL_CTS;
            } else if (pinRts >= 0) {
                config.flow_ctrl = UART_HW_FLOWCTRL_RTS;
            }

            // Do the UART configuration
            espError = uart_param_config(uart, &config);
            if (espError == ESP_OK) {
                // Set up the UART pins
                if (pinCts < 0) {
                    pinCts = UART_PIN_NO_CHANGE;
                }
                if (pinRts < 0) {
                    pinRts = UART_PIN_NO_CHANGE;
                }

                espError = uart_set_pin(uart, pinTx, pinRx,
                                        pinRts, pinCts);
                if (espError == ESP_OK) {
                    // Switch off SW flow control
                    espError = uart_set_sw_flow_ctrl(uart, false, 0, 0);
                    if (espError == ESP_OK) {
                        // Install the driver
                        espError = uart_driver_install(uart,
                                                       receiveBufferSizeBytes,
                                                       0, /* Blocking transmit */
                                                       U_PORT_UART_EVENT_QUEUE_SIZE,
                                                       &gUartData[uart].queue,
                                                       0);
                        if (espError == ESP_OK) {
                            handleOrErrorCode = uart;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    bool closeIt = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].queue != NULL) &&
            !gUartData[handle].markedForDeletion) {
            // Mark the UART for deletion within the
            // mutex
            gUartData[handle].markedForDeletion = true;
            closeIt = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        if (closeIt) {
            // Actually delete the UART outside the mutex
            uartCloseRequiresMutex(handle);
            gUartData[handle].markedForDeletion = false;
        }
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    size_t receiveSize;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {
            sizeOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

            // Will get back either size or -1
            if (uart_get_buffered_data_len(handle, &receiveSize) == 0) {
                sizeOrErrorCode = receiveSize;
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
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (sizeBytes > 0) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {

            // Will get back either size or -1
            sizeOrErrorCode = uart_read_bytes(handle,
                                              (uint8_t *) pBuffer,
                                              sizeBytes, 0);
            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
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
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion) {

            // Will get back either size or -1
            // Hint when debugging: if your code stops dead here
            // it is because the CTS line of this MCU's UART HW
            // is floating high, stopping the UART from
            // transmitting once its buffer is full: either
            // the thing at the other end doesn't want data sent to
            // it or the CTS pin when configuring this UART
            // was wrong and it's not connected to the right
            // thing.
            sizeOrErrorCode = uart_write_bytes(handle,
                                               (const char *) pBuffer,
                                               sizeBytes);
            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex == NULL) &&
            (filter != 0) && (pFunction != NULL) &&
            (stackSizeBytes >= U_PORT_UART_EVENT_MIN_TASK_STACK_SIZE_BYTES) &&
            (priority >= U_CFG_OS_PRIORITY_MIN) &&
            (priority <= U_CFG_OS_PRIORITY_MAX)) {

            gUartData[handle].pEventCallback = pFunction;
            gUartData[handle].pEventCallbackParam = pParam;
            gUartData[handle].eventFilter = filter;

            // On this platform we already have a queue,
            // so rather than using the event queue we
            // instantiate a task to read from the queue
            // and a mutex to manage the task
            errorCode = uPortMutexCreate(&(gUartData[handle].eventTaskRunningMutex));
            if (errorCode == 0) {
                errorCode = uPortTaskCreate(eventTask,
                                            "eventTask",
                                            stackSizeBytes,
                                            (void *) handle,
                                            priority,
                                            &(gUartData[handle].eventTaskHandle));
                if (errorCode == 0) {
                    // Pause to allow the task to run
                    uPortTaskBlock(U_CFG_OS_YIELD_MS);
                } else {
                    // Couldn't create the task, delete the
                    // mutex
                    uPortMutexDelete(gUartData[handle].eventTaskRunningMutex);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    bool removeIt = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)) {
            removeIt = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        if (removeIt) {
            // Delete the event task and it's associated gubbins
            deleteEventTaskRequiresMutex(handle);
        }
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
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)) {
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
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)  &&
            (filter != 0)) {
            gUartData[handle].eventFilter = filter;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Send an event to the callback.
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uart_event_t event;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // The eventBitMap needs to be translated into the
        // event types known to the ESP32 platform (not a
        // bitmap unfortunately) as they send to the queue
        // also.  The only eventBitMap type we support at the
        // moment is U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED
        // which translates to UART_DATA.
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)  &&
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.type = UART_DATA;
            event.size = 0;
            errorCode = uPortQueueSend((const uPortQueueHandle_t) gUartData[handle].queue,
                                       (void *) &event);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Send an event to the callback, but only if there's room on the queue.
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uart_event_t event;
    int64_t startTime = uPortGetTickTimeMs();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // The eventBitMap needs to be translated into the
        // event types known to the ESP32 platform (not a
        // bitmap unfortunately) as they send to the queue
        // also.  The only eventBitMap type we support at the
        // moment is U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED
        // which translates to UART_DATA.
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)  &&
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.type = UART_DATA;
            event.size = 0;
            do {
                // Push an event to event queue, IRQ version so as not to block
                errorCode = uPortQueueSendIrq((const uPortQueueHandle_t) gUartData[handle].queue,
                                              (void *) &event);
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
            } while ((errorCode != 0) &&
                     (uPortGetTickTimeMs() - startTime < delayMs));
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
            (gUartData[handle].eventTaskRunningMutex != NULL) &&
            !gUartData[handle].markedForDeletion) {
            isEventCallback = uPortTaskIsThis(gUartData[handle].eventTaskHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

// Get the stack high watermark for the task on the event queue.
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            !gUartData[handle].markedForDeletion &&
            (gUartData[handle].eventTaskRunningMutex != NULL)) {
            sizeOrErrorCode = uPortTaskStackMinFree(gUartData[handle].eventTaskHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    bool rtsFlowControlIsEnabled = false;
    uart_hw_flowcontrol_t flowControl;

    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        if (uart_get_hw_flow_ctrl(handle, &flowControl) == ESP_OK) {
            if ((flowControl == UART_HW_FLOWCTRL_RTS) ||
                (flowControl == UART_HW_FLOWCTRL_CTS_RTS)) {
                rtsFlowControlIsEnabled = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    bool ctsFlowControlIsEnabled = false;
    uart_hw_flowcontrol_t flowControl;

    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        if (uart_get_hw_flow_ctrl(handle, &flowControl) == ESP_OK) {
            if ((flowControl == UART_HW_FLOWCTRL_CTS) ||
                (flowControl == UART_HW_FLOWCTRL_CTS_RTS)) {
                ctsFlowControlIsEnabled = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return ctsFlowControlIsEnabled;
}

// Suspend CTS flow control.
int32_t uPortUartCtsSuspend(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uart_hw_flowcontrol_t flowCtrl;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (!gUartData[handle].ctsSuspended) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (uart_get_hw_flow_ctrl(handle, &flowCtrl) == ESP_OK) {
                    switch (flowCtrl) {
                        case UART_HW_FLOWCTRL_CTS:
                            if (uart_set_hw_flow_ctrl(handle, UART_HW_FLOWCTRL_DISABLE,
                                                      U_CFG_HW_CELLULAR_RTS_THRESHOLD) == ESP_OK) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                gUartData[handle].ctsSuspended = true;
                            }
                            break;
                        case UART_HW_FLOWCTRL_CTS_RTS:
                            if (uart_set_hw_flow_ctrl(handle, UART_HW_FLOWCTRL_RTS,
                                                      U_CFG_HW_CELLULAR_RTS_THRESHOLD) == ESP_OK) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                gUartData[handle].ctsSuspended = true;
                            }
                            break;
                        case UART_HW_FLOWCTRL_DISABLE:
                        case UART_HW_FLOWCTRL_RTS:
                        default:
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            break;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Resume CTS flow control.
void uPortUartCtsResume(int32_t handle)
{
    uart_hw_flowcontrol_t flowCtrl;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            gUartData[handle].ctsSuspended) {
            if (uart_get_hw_flow_ctrl(handle, &flowCtrl) == ESP_OK) {
                switch (flowCtrl) {
                    case UART_HW_FLOWCTRL_DISABLE:
                        uart_set_hw_flow_ctrl(handle, UART_HW_FLOWCTRL_CTS,
                                              U_CFG_HW_CELLULAR_RTS_THRESHOLD);
                        break;
                    case UART_HW_FLOWCTRL_RTS:
                        uart_set_hw_flow_ctrl(handle, UART_HW_FLOWCTRL_CTS_RTS,
                                              U_CFG_HW_CELLULAR_RTS_THRESHOLD);
                        break;
                    case UART_HW_FLOWCTRL_CTS:
                    case UART_HW_FLOWCTRL_CTS_RTS:
                    default:
                        break;
                }
                gUartData[handle].ctsSuspended = false;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// End of file
