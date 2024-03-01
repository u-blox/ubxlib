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

/** @file
 * @brief Implementation of the port UART API for the Zephyr platform.
 * Note that the UART behaviour is quite different between the embedded
 * target and the Linux/Posix versions: this is because the Zephyr
 * Linux/Posix platform does not support the interrupt-driven UART API;
 * interrupts are supported, just not that UART API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#else
#include <zephyr/types.h>
#include <kernel.h>
#include <device.h>
#include <drivers/uart.h>
#endif

#include <soc.h>

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_YIELD_MS
#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"

#include "u_timeout.h"

#include "u_linked_list.h"

#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#include "u_port_uart.h"

#include "string.h" // For memcpy()

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_ZEPHYR_PORT_UART_INTERRUPT_DRIVEN_NUM_NOPS
/** The number of NOPS to execute at the start of uartCb(),
 * see explanation at the top of that function.
 */
#define U_ZEPHYR_PORT_UART_INTERRUPT_DRIVEN_NUM_NOPS 50
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART.
 */
typedef struct {
    int32_t uart;
    const struct device *pDevice;
    struct uart_config config;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
    char *pBuffer;
    size_t receiveBufferSizeBytes;
    uint32_t bufferRead;
    int32_t bufferWrite;
    bool bufferFull;
    struct k_timer rxTimer;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    struct uartData_t *pTxData;
    struct k_fifo fifoTxData;
    uint32_t txWritten;
    struct k_sem txSem;
#else
    struct k_timer pollTimer;
#endif
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
} uPortUartEvent_t;

struct uartData_t {
    int32_t handle;
    char *pData;
    uint16_t len;
} uartData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Mutex to protect UART data.
static uPortMutexHandle_t gMutex = NULL;

// Root for the linked-list of UART instances, a list of
// pointers to uPortUartData_t
static uLinkedList_t *gpUartData = NULL;

// Variable to keep track of the number of UARTs open.
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the Zephyr device pointer for the given UART number.
static const struct device *pGetDevice(int32_t uart)
{
    const struct device *pDevice = NULL;

    switch (uart) {
#ifdef CONFIG_SERIAL
# if KERNEL_VERSION_MAJOR < 3
        case 0:
            pDevice = device_get_binding("UART_0");
            break;
        case 1:
            pDevice = device_get_binding("UART_1");
            break;
        case 2:
            pDevice = device_get_binding("UART_2");
            break;
        case 3:
            pDevice = device_get_binding("UART_3");
            break;
        case 4:
            pDevice = device_get_binding("UART_4");
            break;
        case 5:
            pDevice = device_get_binding("UART_5");
            break;
        case 6:
            pDevice = device_get_binding("UART_6");
            break;
        case 7:
            pDevice = device_get_binding("UART_7");
            break;
# else
        case 0:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart0);
            break;
        case 1:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart1);
            break;
        case 2:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart2);
            break;
        case 3:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart3);
            break;
        case 4:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart4);
            break;
        case 5:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart5);
            break;
        case 6:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart6);
            break;
        case 7:
            pDevice = U_DEVICE_DT_GET_OR_NULL(uart7);
            break;
# endif
#endif
        default:
            break;
    }

    return pDevice;
}

// Get the uart data for the given handle, or NULL if none.
static uPortUartData_t *pGetUartData(int32_t handle)
{
    uPortUartData_t *pUartData = NULL;
    uLinkedList_t *pList = gpUartData;

    while ((pList != NULL) && (pUartData == NULL)) {
        if (((uPortUartData_t *) (pList->p))->uart == handle) {
            pUartData = (uPortUartData_t *) pList->p;
        }
        pList = pList->pNext;
    }

    return pUartData;
}

// Event handler, calls the user's event callback.
static void eventHandler(void *pParam, size_t paramLength)
{
    uPortUartEvent_t *pEvent = (uPortUartEvent_t *) pParam;
    uPortUartData_t *pUartData;

    (void) paramLength;

    // Don't need to worry about locking the mutex,
    // the close() function makes sure this event handler
    // exits cleanly and, in any case, the user callback
    // will want to be able to access functions in this
    // API which will need to lock the mutex.

    pUartData = pGetUartData(pEvent->uartHandle);
    if ((pUartData != NULL) && (pUartData->pEventCallback != NULL)) {
        pUartData->pEventCallback(pEvent->uartHandle,
                                  pEvent->eventBitMap,
                                  pUartData->pEventCallbackParam);
    }
}

// Close a UART instance
// Note: gMutex should be locked before this is called.
static void uartClose(uPortUartData_t *pUartData)
{
    k_free(pUartData->pBuffer);
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    uart_irq_rx_disable(pUartData->pDevice);
    uart_irq_tx_disable(pUartData->pDevice);
#else
    k_timer_stop(&pUartData->pollTimer);
#endif
    uLinkedListRemove(&gpUartData, pUartData);
    k_free(pUartData);
    U_ATOMIC_DECREMENT(&gResourceAllocCount);
}

static void rxTimer(struct k_timer *pTimerId)
{
    uPortUartData_t *pUartData = (uPortUartData_t *)(pTimerId->user_data);

    if ((pUartData != NULL) &&
        (pUartData->eventQueueHandle >= 0) &&
        (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
        uPortUartEvent_t event;
        event.uartHandle = pUartData->uart;
        event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
        uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                               &event, sizeof(event));
    }
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
// uartCb called by the interrupt-based UART driver.
static void uartCb(const struct device *pDevice, void *pUserData)
{
    uPortUartData_t *pUartData = (uPortUartData_t *)(pUserData);

    if (pUartData != NULL) {
        // Note: we used to have code at the start of this function which
        // found the UART instance in an array of stored UART data.  When
        // we updated the array to be a linked-list instead, we changed
        // this function to receive a pointer to the UART as the user
        // parameter; this seemed like an efficiency improvement.
        // However, with this done UART transfers started failing; turns
        // out the delay introduced by the look-up code was serving a
        // purpose: a delay is required just here or the interrupt does not
        // work correctly.
        // We do this with NOPS rather than k_busy_wait() as we really don't
        // want to waste even one microsecond in an interrupt.
        for (size_t x = 0; x < U_ZEPHYR_PORT_UART_INTERRUPT_DRIVEN_NUM_NOPS; x++) {
            __asm volatile ("nop");
        }
        uart_irq_update(pDevice);
        if (uart_irq_rx_ready(pDevice)) {
            bool read = false;
            if (!pUartData->bufferFull) {
                while (uart_fifo_read(pDevice, (pUartData->pBuffer + pUartData->bufferWrite), 1) != 0) {
                    pUartData->bufferWrite++;
                    pUartData->bufferWrite %= pUartData->receiveBufferSizeBytes;
                    read = true;

                    if (pUartData->bufferWrite == pUartData->bufferRead) {
                        pUartData->bufferFull = true;
                        uart_irq_rx_disable(pDevice);
                        k_timer_stop(&(pUartData->rxTimer));
                        if ((pUartData->eventQueueHandle >= 0) &&
                            (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
                            uPortUartEvent_t event;
                            event.uartHandle = pUartData->uart;
                            event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
                            uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                                                   &event, sizeof(event));
                        }
                        break;
                    }
                }

                if (read) {
                    k_timer_start(&(pUartData->rxTimer), K_MSEC(1), K_NO_WAIT);
                }
            }
        }

        if (uart_irq_tx_ready(pDevice)) {
            if (pUartData->pTxData == NULL) {
                pUartData->pTxData = k_fifo_get(&(pUartData->fifoTxData), K_NO_WAIT);
                pUartData->txWritten = 0;
            }

            if (!pUartData->pTxData) {
                uart_irq_tx_disable(pDevice);
                return;
            }

            if (pUartData->pTxData->len > pUartData->txWritten) {

                pUartData->txWritten += uart_fifo_fill(pDevice,
                                                       pUartData->pTxData->pData + pUartData->txWritten,
                                                       pUartData->pTxData->len - pUartData->txWritten);

            } else {
                pUartData->pTxData = NULL;
                pUartData->txWritten = 0;
                k_sem_give(&(pUartData->txSem));

                if (k_fifo_is_empty(&(pUartData->fifoTxData))) {
                    uart_irq_tx_disable(pDevice);
                }
            }
        }
    }
}

#else

// Polled receive for when an interrupt-driven UART driver
// is not available (though note that pollTimer is still
// run in interrupt context, just not that of the UART, that of
// the timer code instead).
// Note: this is not intended to be efficient, just as similar
// as possible to the interrupt driven case.  It is used on the
// Linux/Posix platform for development/test work only.
static void pollTimer(struct k_timer *pTimerId)
{
    uPortUartData_t *pUartData = (uPortUartData_t *)(pTimerId->user_data);
    bool read = false;

    if (pUartData != NULL) {
        while (!pUartData->bufferFull &&
               (uart_poll_in(pUartData->pDevice,
                             pUartData->pBuffer + pUartData->bufferWrite) == 0)) {
            pUartData->bufferWrite++;
            pUartData->bufferWrite %= pUartData->receiveBufferSizeBytes;
            read = true;
            if (pUartData->bufferWrite == pUartData->bufferRead) {
                pUartData->bufferFull = true;
                k_timer_stop(&(pUartData->rxTimer));
                if ((pUartData->eventQueueHandle >= 0) &&
                    (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
                    uPortUartEvent_t event;
                    event.uartHandle = pUartData->uart;
                    event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
                    uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                                           &event, sizeof(event));
                }
            }
        }

        if (read) {
            k_timer_start(&(pUartData->rxTimer), K_MSEC(1), K_NO_WAIT);
        }
    }
}

#endif // #ifdef CONFIG_UART_INTERRUPT_DRIVEN

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uPortUartInit()
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return (int32_t)errorCode;
}

void uPortUartDeinit()
{
    uLinkedList_t *pListNext;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        while (gpUartData != NULL) {
            pListNext = gpUartData->pNext;
            uartClose((uPortUartData_t *) gpUartData->p);
            gpUartData = pListNext;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

int32_t uPortUartPrefix(const char *pPrefix)
{
    (void)pPrefix;
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t receiveBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts)
{
    uErrorCode_t handleOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    const struct device *pDevice;
    uPortUartData_t *pUartData;

    // Note that the pins passed into this function must be set
    // to -1 since the Zephyr platform used on NRF53 does not
    // permit the pin assignments to be set at run-time, only at
    // compile-time.  To obtain the real values for your peripheral
    // pin assignments take a look at the macros U_CFG_TEST_PIN_UART_A_xxx_GET
    // (e.g. U_CFG_TEST_PIN_UART_A_TXD_GET) in the file
    // `u_cfg_test_plaform_specific.h` for this platform, which
    // demonstrate a mechanism for doing this.

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) && (pReceiveBuffer == NULL) &&
            (pinTx < 0) && (pinRx < 0) && (pinCts < 0) && (pinRts < 0)) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            pUartData = pGetUartData(uart);
            pDevice = pGetDevice(uart);
            if ((pUartData == NULL) && (pDevice != NULL)) {
                handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
                pUartData = (uPortUartData_t *) k_malloc(sizeof(*pUartData));
                if (pUartData != NULL) {
                    memset(pUartData, 0, sizeof(*pUartData));
                    pUartData->pBuffer = k_malloc(receiveBufferSizeBytes);
                    if (pUartData->pBuffer != NULL) {
                        if (uLinkedListAdd(&gpUartData, pUartData)) {
                            pUartData->uart = uart;
                            pUartData->pDevice = pDevice;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
                            k_sem_init(&(pUartData->txSem), 0, 1);
                            k_fifo_init(&(pUartData->fifoTxData));
#endif
                            pUartData->receiveBufferSizeBytes = receiveBufferSizeBytes;
                            pUartData->eventQueueHandle = -1;
                            k_timer_init(&(pUartData->rxTimer), rxTimer, NULL);
                            k_timer_user_data_set(&(pUartData->rxTimer), (void *)pUartData);

                            uart_config_get(pUartData->pDevice, &(pUartData->config));
                            // Flow control is set in the .overlay file
                            // by including the line:
                            //     hw-flow-control;
                            // in the definition of the relevant UART
                            // so all we need to configure here is the
                            // baud rate as everything else is good at the
                            // default values (8N1).
                            pUartData->config.baudrate = baudRate;
                            uart_configure(pUartData->pDevice, &(pUartData->config));
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
                            uart_irq_callback_user_data_set(pUartData->pDevice, uartCb, (void *)pUartData);
                            uart_irq_rx_enable(pUartData->pDevice);
#else
                            k_timer_init(&(pUartData->pollTimer), pollTimer, NULL);
                            k_timer_user_data_set(&(pUartData->pollTimer), (void *)pUartData);
                            k_timer_start(&(pUartData->pollTimer), K_MSEC(1), K_MSEC(1));
#endif
                            U_ATOMIC_INCREMENT(&gResourceAllocCount);
                            handleOrErrorCode = uart;
                        } else {
                            // Clean up on error
                            k_free(pUartData->pBuffer);
                            k_free(pUartData);
                        }
                    } else {
                        // Clean up on error
                        k_free(pUartData);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) handleOrErrorCode;
}

void uPortUartClose(int32_t handle)
{
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if (pUartData != NULL) {
            uartClose(pUartData);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

int32_t uPortUartGetReceiveSize(int32_t handle)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if (pUartData != NULL) {
            if (pUartData->bufferFull) {
                sizeOrErrorCode = pUartData->receiveBufferSizeBytes;
            } else {
                sizeOrErrorCode = pUartData->bufferWrite - pUartData->bufferRead;

                if (sizeOrErrorCode < 0) {
                    sizeOrErrorCode = (pUartData->receiveBufferSizeBytes - pUartData->bufferRead) +
                                      pUartData->bufferWrite;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pBuffer != NULL) && (sizeBytes > 0)) {
            if (pUartData->bufferWrite == pUartData->bufferRead &&
                pUartData->bufferFull == false) {
                sizeOrErrorCode = 0;
            } else {
                int32_t toRead = 0;
                if (pUartData->bufferFull) {
                    sizeOrErrorCode = pUartData->receiveBufferSizeBytes;
                } else {
                    sizeOrErrorCode = pUartData->bufferWrite - pUartData->bufferRead;

                    if (sizeOrErrorCode == 0) {
                        sizeOrErrorCode = pUartData->receiveBufferSizeBytes;
                    } else if (sizeOrErrorCode < 0) {
                        sizeOrErrorCode = (pUartData->receiveBufferSizeBytes - pUartData->bufferRead) +
                                          pUartData->bufferWrite;
                    }
                }

                if (sizeOrErrorCode > sizeBytes) {
                    sizeOrErrorCode = sizeBytes;
                }

                toRead = sizeOrErrorCode;

                uint32_t read = MIN(toRead, pUartData->receiveBufferSizeBytes -
                                    pUartData->bufferRead);
                memcpy(pBuffer, pUartData->pBuffer + pUartData->bufferRead, read);
                toRead -= read;
                pUartData->bufferRead += read;
                pUartData->bufferRead %= pUartData->receiveBufferSizeBytes;

                if (toRead > 0) {
                    memcpy((char *)pBuffer + read, pUartData->pBuffer, toRead);
                    pUartData->bufferRead += toRead;
                }

                pUartData->bufferFull = false;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
                uart_irq_rx_enable(pUartData->pDevice);
#endif
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pBuffer != NULL) && (sizeBytes > 0) &&
            (sizeBytes < UINT16_MAX)) {
            errorCode = sizeBytes;
            // Hint when debugging: if your code stops dead here
            // it is because the CTS line of this MCU's UART HW
            // is floating high, stopping the UART from
            // transmitting once its buffer is full: either
            // the thing at the other end doesn't want data sent to
            // it or the CTS pin when configuring this UART
            // was wrong and it's not connected to the right
            // thing.
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
            struct uartData_t data;
            data.handle = handle;
            data.pData = (void *)pBuffer;
            data.len = (uint16_t) sizeBytes;

            k_fifo_put(&(pUartData->fifoTxData), &data);
            uart_irq_tx_enable(pUartData->pDevice);
            // UART write is async to wait here to make this function synchronous
            k_sem_take(&(pUartData->txSem), K_FOREVER);
#else
            // When we have no interrupts we can block right here
            const unsigned char *pBufferUnsignedChar = (const unsigned char *) pBuffer;
            while (sizeBytes > 0) {
                uart_poll_out(pUartData->pDevice, *pBufferUnsignedChar);
                pBufferUnsignedChar++;
                sizeBytes--;
            }
#endif
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) errorCode;
}

int32_t uPortUartEventCallbackSet(int32_t handle,
                                  uint32_t filter,
                                  void (*pFunction)(int32_t,
                                                    uint32_t,
                                                    void *),
                                  void *pParam,
                                  size_t stackSizeBytes,
                                  int32_t priority)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    char name[16];

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle < 0) &&
            (filter != 0) && (pFunction != NULL)) {
            // Open an event queue to eventHandler()
            // which will receive uPortUartEvent_t
            // and give it a useful name for debug purposes
            snprintf(name, sizeof(name), "eventUart_%d", (int) handle);
            errorCode = uPortEventQueueOpen(eventHandler, name,
                                            sizeof(uPortUartEvent_t),
                                            stackSizeBytes,
                                            priority,
                                            U_PORT_UART_EVENT_QUEUE_SIZE);
            if (errorCode >= 0) {
                pUartData->eventQueueHandle = (int32_t) errorCode;
                pUartData->eventQueueHandle = (int32_t) errorCode;
                pUartData->pEventCallback = pFunction;
                pUartData->pEventCallbackParam = pParam;
                pUartData->eventFilter = filter;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

void uPortUartEventCallbackRemove(int32_t handle)
{
    int32_t eventQueueHandle = -1;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0)) {
            // Save the eventQueueHandle and set all
            // the parameters to indicate that the
            // queue is closed
            eventQueueHandle = pUartData->eventQueueHandle;
            pUartData->eventQueueHandle = -1;
            pUartData->pEventCallback = NULL;
            pUartData->eventFilter = 0;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Now close the event queue
        // outside the gMutex lock.  Reason for this
        // is that the event task could be calling
        // back into here and we don't want it
        // blocked by us or we'll get stuck.
        if (eventQueueHandle >= 0) {
            uPortEventQueueClose(eventQueueHandle);
        }
    }
}

uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0)) {
            filter = pUartData->eventFilter;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return filter;
}

int32_t uPortUartEventCallbackFilterSet(int32_t handle,
                                        uint32_t filter)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0) &&
            (filter != 0)) {
            pUartData->eventFilter = filter;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    uPortUartEvent_t event;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            errorCode = uPortEventQueueSend(pUartData->eventQueueHandle,
                                            &event, sizeof(event));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    uPortUartEvent_t event;
    uTimeoutStart_t timeoutStart = uTimeoutStart();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            do {
                // Push an event to event queue, IRQ version so as not to block
                errorCode = uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                                                   &event, sizeof(event));
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
            } while ((errorCode != 0) &&
                     !uTimeoutExpiredMs(timeoutStart, delayMs));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

bool uPortUartEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(pUartData->eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

int32_t uPortUartEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle >= 0)) {
            sizeOrErrorCode = uPortEventQueueStackMinFree(pUartData->eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    bool rtsFlowControlIsEnabled = false;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) &&
            (pUartData->config.flow_ctrl == UART_CFG_FLOW_CTRL_RTS_CTS)) {
            rtsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return rtsFlowControlIsEnabled;
}

bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    bool ctsFlowControlIsEnabled = false;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartData(handle);
        if ((pUartData != NULL) &&
            (pUartData->config.flow_ctrl == UART_CFG_FLOW_CTRL_RTS_CTS)) {
            ctsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return ctsFlowControlIsEnabled;
}

// Suspend CTS flow control.
int32_t uPortUartCtsSuspend(int32_t handle)
{
    (void) handle;

    // On the Zephyr platform HW handshaking is controlled
    // statically by the UART configuration structure at
    // compile time and hence it is not possible to suspend
    // CTS operation
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Resume CTS flow control.
void uPortUartCtsResume(int32_t handle)
{
    (void) handle;
}

// Get the number of UART interfaces currently open.
int32_t uPortUartResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
