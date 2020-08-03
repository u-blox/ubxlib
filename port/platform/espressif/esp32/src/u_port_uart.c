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
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "driver/uart.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The maximum number of UARTs supported, which is the range of the
// "uart" parameter on this platform
#define U_PORT_UART_MAX_NUM 3

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Mutexes to protect the UART hardware.
static uPortMutexHandle_t gMutex[U_PORT_UART_MAX_NUM] = {NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

//** Initialise a UART.
int32_t uPortUartInit(int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts,
                      int32_t baudRate,
                      int32_t uart,
                      uPortQueueHandle_t *pUartQueue)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uart_config_t config;
    esp_err_t espError;

    if ((pUartQueue != NULL) && (pinRx >= 0) && (pinTx >= 0) &&
        (uart < sizeof(gMutex) / sizeof(gMutex[0]))) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        if (gMutex[uart] == NULL) {
            errorCode = uPortMutexCreate(&(gMutex[uart]));
            if (errorCode == 0) {
                errorCode = U_ERROR_COMMON_PLATFORM;

                U_PORT_MUTEX_LOCK(gMutex[uart]);

                // Set the things that won't change
                config.data_bits = UART_DATA_8_BITS,
                config.stop_bits = UART_STOP_BITS_1,
                config.parity    = UART_PARITY_DISABLE,
                config.use_ref_tick = false;

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
                                                           U_PORT_UART_RX_BUFFER_SIZE,
                                                           0, /* Blocking transmit */
                                                           U_PORT_UART_EVENT_QUEUE_SIZE,
                                                           (QueueHandle_t *) pUartQueue,
                                                           0);
                            if (espError == ESP_OK) {
                                errorCode = U_ERROR_COMMON_SUCCESS;
                            }
                        }
                    }
                }

                U_PORT_MUTEX_UNLOCK(gMutex[uart]);

                // If we failed to initialise the UART,
                // delete the mutex and put the uart's gMutex[]
                // state back to NULL
                if (errorCode != U_ERROR_COMMON_SUCCESS) {
                    uPortMutexDelete(gMutex[uart]);
                    gMutex[uart] = NULL;
                }
            }
        }
    }

    return (int32_t) errorCode;
}

// Shutdown a UART.
int32_t uPortUartDeinit(int32_t uart)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    esp_err_t espError;

    if (uart < sizeof(gMutex) / sizeof(gMutex[0])) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        if (gMutex[uart] != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;

            // No need to lock the mutex, we need to delete it
            // and we're not allowed to delete a locked mutex.
            // The caller needs to make sure that no read/write
            // is in progress when this function is called.
            espError = uart_driver_delete(uart);
            if (espError == ESP_OK) {
                uPortMutexDelete(gMutex[uart]);
                gMutex[uart] = NULL;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return (int32_t) errorCode;
}

// Push an invalid UART event onto the UART event queue.
int32_t uPortUartEventSend(const uPortQueueHandle_t queueHandle,
                           int32_t sizeBytesOrError)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uart_event_t uartEvent;

    if (queueHandle != NULL) {
        // On this platform uartEvent.size is of type
        // size_t so an error is signalled
        // by setting uartEvent.type to an
        // illegal value
        uartEvent.type = UART_EVENT_MAX;
        uartEvent.size = 0;
        if (sizeBytesOrError >= 0) {
            uartEvent.type = UART_DATA;
            uartEvent.size = sizeBytesOrError;
        }
        errorCode = uPortQueueSend(queueHandle, (void *) &uartEvent);
    }

    return errorCode;
}

// Receive a UART event, blocking until one turns up.
int32_t uPortUartEventReceive(const uPortQueueHandle_t queueHandle)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uart_event_t uartEvent;

    if (queueHandle != NULL) {
        sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;
        if (uPortQueueReceive(queueHandle, &uartEvent) == 0) {
            sizeOrErrorCode = U_ERROR_COMMON_UNKNOWN;
            if (uartEvent.type < UART_EVENT_MAX) {
                if (uartEvent.type == UART_DATA) {
                    sizeOrErrorCode = uartEvent.size;
                } else {
                    sizeOrErrorCode = 0;
                }
            }
        }
    }

    return sizeOrErrorCode;
}

// Receive a UART event with a timeout.
int32_t uPortUartEventTryReceive(const uPortQueueHandle_t queueHandle,
                                 int32_t waitMs)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uart_event_t uartEvent;

    if (queueHandle != NULL) {
        sizeOrErrorCode = U_ERROR_COMMON_TIMEOUT;
        if (uPortQueueTryReceive(queueHandle, waitMs, &uartEvent) == 0) {
            sizeOrErrorCode = U_ERROR_COMMON_UNKNOWN;
            if (uartEvent.type < UART_EVENT_MAX) {
                if (uartEvent.type == UART_DATA) {
                    sizeOrErrorCode = uartEvent.size;
                } else {
                    sizeOrErrorCode = 0;
                }
            }
        }
    }

    return sizeOrErrorCode;
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t uart)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    size_t receiveSize;

    if (uart < sizeof(gMutex) / sizeof(gMutex[0])) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gMutex[uart] != NULL) {
            sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;

            U_PORT_MUTEX_LOCK(gMutex[uart]);

            // Will get back either size or -1
            if (uart_get_buffered_data_len(uart, &(receiveSize)) == 0) {
                sizeOrErrorCode = receiveSize;
            }

            U_PORT_MUTEX_UNLOCK(gMutex[uart]);
        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t uart, char *pBuffer,
                      size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pBuffer != NULL) && (sizeBytes > 0) &&
        (uart < sizeof(gMutex) / sizeof(gMutex[0]))) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gMutex[uart] != NULL) {

            U_PORT_MUTEX_LOCK(gMutex[uart]);

            // Will get back either size or -1
            sizeOrErrorCode = uart_read_bytes(uart, (uint8_t *) pBuffer, sizeBytes, 0);
            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;
            }

            U_PORT_MUTEX_UNLOCK(gMutex[uart]);
        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t uart,
                       const char *pBuffer,
                       size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pBuffer != NULL) &&
        (uart < sizeof(gMutex) / sizeof(gMutex[0]))) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gMutex[uart] != NULL) {

            U_PORT_MUTEX_LOCK(gMutex[uart]);

            // Will get back either size or -1
            sizeOrErrorCode = uart_write_bytes(uart, (const char *) pBuffer, sizeBytes);
            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;
            }

            U_PORT_MUTEX_UNLOCK(gMutex[uart]);
        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortIsRtsFlowControlEnabled(int32_t uart)
{
    bool rtsFlowControlIsEnabled = false;
    uart_hw_flowcontrol_t flowControl;

    if ((uart < sizeof(gMutex) / sizeof(gMutex[0])) &&
        (gMutex[uart] != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex[uart]);

        if (uart_get_hw_flow_ctrl(uart, &flowControl) == ESP_OK) {
            if ((flowControl == UART_HW_FLOWCTRL_RTS) ||
                (flowControl == UART_HW_FLOWCTRL_CTS_RTS)) {
                rtsFlowControlIsEnabled = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex[uart]);
    }

    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortIsCtsFlowControlEnabled(int32_t uart)
{
    bool ctsFlowControlIsEnabled = false;
    uart_hw_flowcontrol_t flowControl;

    if ((uart < sizeof(gMutex) / sizeof(gMutex[0])) &&
        (gMutex[uart] != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex[uart]);

        if (uart_get_hw_flow_ctrl(uart, &flowControl) == ESP_OK) {
            if ((flowControl == UART_HW_FLOWCTRL_CTS) ||
                (flowControl == UART_HW_FLOWCTRL_CTS_RTS)) {
                ctsFlowControlIsEnabled = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex[uart]);
    }

    return ctsFlowControlIsEnabled;
}

// End of file
