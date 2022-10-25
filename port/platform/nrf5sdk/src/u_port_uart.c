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
 * @brief Implementation of the port UART API for the NRF52 platform.
 *
 * Note: in order to implement the API we require, where receipt
 * of data is signalled by an event queue and other things can
 * send to that same event queue, this code is implemented on top of
 * the nrf_uarte.h HAL and replaces the nrfx_uarte.h default driver
 * from Nordic.  It steals from the code in nrfx_uarte.c, Nordic's
 * implementation.
 *
 * So that users can continue to use the Nordic UARTE driver this
 * code uses only the UART port that the Nordic UARTE driver is NOT
 * using: for instance, to use UARTE1 in this driver then
 * NRFX_UARTE1_ENABLED should be set to 0 in sdk_config to free it
 * up.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"    // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_YIELD_MS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#include "u_port_uart.h"
#include "u_port_private.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "nrf.h"
#include "nrf_uarte.h"
#include "nrf_gpio.h"
#include "nrfx_ppi.h"
#include "nrfx_timer.h"
#include "nrf_delay.h"

#include "limits.h" // For INT_MAX
#include "string.h" // For memcpy()

/* Design note: it took ages to get this to work.
 * Rx DMA length is set to 1 byte because UART H/W must notify the
 * driver for every byte received. If Rx DMA length > 1, then
 * UARTE H/W will not report ENDRX until the entire buffer is filled.
 * But for our use case we want the readers to be notified for whatever
 * we received immediately.
 *
 * We don't read from the Rx DMA buff until we get the ENDRX event from the
 * UARTE H/W. ENDRX event guarantees that the data is copied to Rx DMA buffer
 *
 * The key is NEVER to stop the UARTE HW, Any attempt to stop and restart the
 * UARTE ends up with character loss.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The maximum number of UARTs supported, which is the range of the
// "uart" parameter on this platform
#ifndef NRFX_UARTE_ENABLED
// NRFX_UARTE is not enabled, we can have both
# define U_PORT_UART_MAX_NUM 2
#else
# if !NRFX_UARTE0_ENABLED && !NRFX_UARTE1_ENABLED
// NRFX_UARTE is enabled but neither UARTEx is enabled so we can
// have both
#  define U_PORT_UART_MAX_NUM 2
# else
#  if !NRFX_UARTE0_ENABLED || !NRFX_UARTE1_ENABLED
#    define U_PORT_UART_MAX_NUM 1
#  else
#    error No UARTs available, both are being used by the Nordic NRFX_UARTE driver; to use this code at least one of NRFX_UARTE0_ENABLED or NRFX_UARTE1_ENABLED must be set to 0.
#  endif
# endif
#endif
#define U_PORT_UART_TX_QUEUE_LENGTH 16
#define U_PORT_UART_RX_DMA_LENGTH   1
#define U_PORT_UART_TX_DMA_LENGTH   32
/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART.
 * ORDER IS IMPORTANT: a portion of this structure is
 * statically initialised.
 */
typedef struct {
    NRF_UARTE_Type *pReg;
    nrf_ppi_channel_t ppiChannel;
    bool hwfcSuspended;
    int32_t uartHandle;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
    size_t rxBufferSizeBytes;
    uint8_t rxDmaBuff;
    uint8_t dummyTxDma;
    uint8_t txDmaBuff[U_PORT_UART_TX_DMA_LENGTH];
    uint8_t *pRxBuff; /**< Also used as a marker that this UART is in use. */
    uint8_t *pTxBuff;
    uint32_t bufferRead;
    uint32_t bufferWrite;
    uint32_t txBuffLen;
    uint32_t txWritten;
    bool bufferFull;
    bool disableTxIrq;
    uPortSemaphoreHandle_t txSem;
    uPortQueueHandle_t txQueueHandle;
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
} uPortUartEvent_t;

typedef struct {
    int32_t handle;
    uint8_t *pData;
    uint32_t len;
} uartTxData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Mutex to protect UART data.
static uPortMutexHandle_t gMutex = NULL;

// UART data, where the UARTE register and the
// associated counter and PPI channel are the only
// ones initialised here.
// In this implementation uart and handle are synonymous,
// both are indexes into the gUartData array.
#if !NRFX_UARTE0_ENABLED && !NRFX_UARTE1_ENABLED
static uPortUartData_t gUartData[] = {
    {
        NRF_UARTE0,
    },
    {
        NRF_UARTE1,
    }
};
# else
#  if !NRFX_UARTE0_ENABLED
static uPortUartData_t gUartData[] = {NRF_UARTE0,
                                     };
#  else
static uPortUartData_t gUartData[] = {NRF_UARTE1,
                                     };
#  endif
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
// Retrieve total number of bytes received
static uint32_t uartGetRxdBytes(uPortUartData_t *pUartData)
{
    uint32_t rxdBytes;

    if (pUartData->bufferWrite == pUartData->bufferRead) {

        rxdBytes = pUartData->bufferFull ?
                   pUartData->rxBufferSizeBytes : 0;

    } else if (pUartData->bufferWrite < pUartData->bufferRead) {

        rxdBytes = (pUartData->rxBufferSizeBytes -
                    pUartData->bufferRead) +
                   pUartData->bufferWrite;
    } else {

        rxdBytes = pUartData->bufferWrite - pUartData->bufferRead;
    }

    return rxdBytes;
}

// Event handler, calls the user's event callback.
static void eventHandler(void *pParam, size_t paramLength)
{
    uPortUartEvent_t *pEvent = (uPortUartEvent_t *) pParam;

    (void) paramLength;

    // Don't need to worry about locking the mutex,
    // the close() function makes sure this event handler
    // exits cleanly and, in any case, the user callback
    // will want to be able to access functions in this
    // API which will need to lock the mutex.

    if ((pEvent->uartHandle >= 0) &&
        (pEvent->uartHandle < sizeof(gUartData) / sizeof(gUartData[0]))) {
        if (gUartData[pEvent->uartHandle].pEventCallback != NULL) {
            gUartData[pEvent->uartHandle].pEventCallback(pEvent->uartHandle,
                                                         pEvent->eventBitMap,
                                                         gUartData[pEvent->uartHandle].pEventCallbackParam);
        }
    }
}

// Close a UART instance
// Note: gMutex should be locked before this is called.
static void uartClose(int32_t handle)
{
    uint32_t pinRtsNrf;
    uint32_t pinCtsNrf;
    NRF_UARTE_Type *pReg;

    if ((handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
        if (gUartData[handle].pRxBuff != NULL) {
            pReg = gUartData[handle].pReg;

            // Disable Rx interrupts
            nrf_uarte_int_disable(pReg, NRF_UARTE_INT_ENDTX_MASK |
                                  NRF_UARTE_INT_TXSTOPPED_MASK |
                                  NRF_UARTE_INT_ENDRX_MASK);
            NRFX_IRQ_DISABLE(nrfx_get_irq_number((void *) (pReg)));

            // Make sure all transfers are finished before UARTE is
            // disabled to achieve the lowest power consumption
            nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_RXTO);
            nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPRX);
            nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_TXSTOPPED);
            nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPTX);
            while (!nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED) ||
                   !nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_RXTO)) {}

            // Disable the UARTE
            nrf_uarte_disable(pReg);

            // Put the pins back
            nrf_gpio_cfg_default(nrf_uarte_tx_pin_get(pReg));
            nrf_gpio_cfg_default(nrf_uarte_rx_pin_get(pReg));
            nrf_uarte_txrx_pins_disconnect(pReg);
            pinRtsNrf = nrf_uarte_rts_pin_get(pReg);
            pinCtsNrf = nrf_uarte_cts_pin_get(pReg);
            nrf_uarte_hwfc_pins_disconnect(pReg);
            if (pinCtsNrf != NRF_UARTE_PSEL_DISCONNECTED) {
                nrf_gpio_cfg_default(pinCtsNrf);
            }
            if (pinRtsNrf != NRF_UARTE_PSEL_DISCONNECTED) {
                nrf_gpio_cfg_default(pinRtsNrf);
            }

            // Remove the callback if there is one
            if (gUartData[handle].eventQueueHandle >= 0) {
                uPortEventQueueClose(gUartData[handle].eventQueueHandle);
            }
            gUartData[handle].eventQueueHandle = -1;

            // And finally free the allocated resources
            // and mark them NULL
            uPortFree(gUartData[handle].pRxBuff);
            gUartData[handle].pRxBuff = NULL;
            gUartData[handle].pTxBuff = NULL;
            gUartData[handle].bufferRead = 0;
            gUartData[handle].bufferWrite = 0;
            gUartData[handle].bufferFull = false;
            gUartData[handle].txWritten = 0;
            uPortSemaphoreDelete(gUartData[handle].txSem);
            uPortQueueDelete(gUartData[handle].txQueueHandle);
        }
    }
}
static void userNotify(uPortUartData_t *pUartData)
{

    if ((pUartData->eventQueueHandle >= 0) &&
        (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
        uPortUartEvent_t event;
        event.uartHandle = pUartData->uartHandle;
        event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
        uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                               &event, sizeof(event));
    }
}

static int32_t uartTxFifoFill(uPortUartData_t *pUartData, const uint8_t *pTxBuff, uint32_t len)
{
    NRF_UARTE_Type *pReg = pUartData->pReg;

    if (len > sizeof(pUartData->txDmaBuff)) {
        len = sizeof(pUartData->txDmaBuff);
    }

    memcpy(pUartData->txDmaBuff, pTxBuff, len);

    // Check if Tx has come to stop before starting another
    // transaction
    if (nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED)) {
        nrf_uarte_tx_buffer_set(pReg, pUartData->txDmaBuff, len);
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDTX);
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_TXSTOPPED);
        nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTTX);
    } else {
        len = 0;
    }

    return len;
}

static int32_t uartFifoRead(uPortUartData_t *pUartData, uint8_t *pRxBuff, uint32_t size)
{
    int32_t rxdCount = 0;
    NRF_UARTE_Type *pReg = pUartData->pReg;
    if (size > 0 && nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ENDRX)) {

        // Clear Rx interrupt
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDRX);

        // Copy the Rxd character from DMA buffer
        pRxBuff[rxdCount++] = (uint8_t)pUartData->rxDmaBuff;

        // Start Rx again
        nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTRX);
    }

    return rxdCount;
}

static void uartIrqHandler(uPortUartData_t *pUartData)
{
    NRF_UARTE_Type *pReg = pUartData->pReg;
    bool read = false;

    if (nrf_uarte_int_enable_check(pReg, NRF_UARTE_INT_ENDTX_MASK) &&
        nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ENDTX)) {
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDTX);
        nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPTX);
    }


    if (nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED)) {
        if (pUartData->disableTxIrq) {
            nrf_uarte_int_disable(pReg, NRF_UARTE_INT_TXSTOPPED_MASK);
            pUartData->disableTxIrq = false;
            return;
        }
    }

    if (nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ERROR)) {
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ERROR);
    }

    // Handle Rx
    if (nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ENDRX)) {

        if (pUartData->bufferFull == false) {
            // Start reading out the bytes from Rx DMA buff until we see no more ENDRX event
            while (uartFifoRead(pUartData, (pUartData->pRxBuff + pUartData->bufferWrite), 1) != 0) {

                pUartData->bufferWrite++;
                pUartData->bufferWrite %= pUartData->rxBufferSizeBytes;
                read = true;
                // Stop Rx interrupt when there is no more space
                // Rx interrupts will be renabled in the uPortUartRead
                if (pUartData->bufferWrite == pUartData->bufferRead) {
                    pUartData->bufferFull = true;
                    nrf_uarte_int_disable(pReg, NRF_UARTE_INT_ENDRX_MASK);
                    break;
                }
            }

            if (read) {
                //signal the user to read
                userNotify(pUartData);
            }
        }
    }

    // Handle Tx
    if (pUartData->disableTxIrq == false &&
        nrf_uarte_int_enable_check(pReg, NRF_UARTE_INT_TXSTOPPED_MASK) &&
        nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED)) {

        if (pUartData->pTxBuff == NULL) {
            uartTxData_t txData;
            if (uPortQueueReceiveIrq(pUartData->txQueueHandle, (void *)&txData) == U_ERROR_COMMON_SUCCESS) {
                pUartData->pTxBuff = txData.pData;
                pUartData->txBuffLen = txData.len;
            }
        }

        if (pUartData->pTxBuff == NULL) {
            pUartData->disableTxIrq = true;
            return;
        }

        if (pUartData->txBuffLen > pUartData->txWritten) {
            pUartData->txWritten += uartTxFifoFill(pUartData,
                                                   pUartData->pTxBuff + pUartData->txWritten,
                                                   pUartData->txBuffLen - pUartData->txWritten);
        } else {
            pUartData->pTxBuff = NULL;
            pUartData->txBuffLen = 0;
            pUartData->txWritten = 0;
            uPortSemaphoreGiveIrq(pUartData->txSem);
        }
    }
}

// Convert a baud rate into an NRF52840 baud rate.
static int32_t baudRateToNrfBaudRate(int32_t baudRate)
{
    int32_t baudRateNrf = -1;

    switch (baudRate) {
        case 1200:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud1200;
            break;
        case 2400:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud2400;
            break;
        case 9600:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud9600;
            break;
        case 14400:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud14400;
            break;
        case 19200:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud19200;
            break;
        case 28800:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud28800;
            break;
        case 31250:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud31250;
            break;
        case 38400:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud38400;
            break;
        case 56000:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud56000;
            break;
        case 57600:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud57600;
            break;
        case 76800:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud76800;
            break;
        case 115200:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud115200;
            break;
        case 230400:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud230400;
            break;
        case 250000:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud250000;
            break;
        case 460800:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud460800;
            break;
        case 921600:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud921600;
            break;
        case 1000000:
            baudRateNrf = UARTE_BAUDRATE_BAUDRATE_Baud1M;
            break;
        default:
            break;
    }

    return baudRateNrf;
}

// Derived from the NRFX function nrfx_get_irq_number()
__STATIC_INLINE IRQn_Type getIrqNumber(void const *pReg)
{
    return (IRQn_Type) (uint8_t)((uint32_t)(pReg) >> 12);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INTERRUPT HANDLERS
 * -------------------------------------------------------------- */

#if !NRFX_UARTE0_ENABLED
void nrfx_uarte_0_irq_handler(void)
{
    uartIrqHandler(&(gUartData[0]));
}
#endif

#if !NRFX_UARTE1_ENABLED
void nrfx_uarte_1_irq_handler(void)
{
    uartIrqHandler(&(gUartData[1]));
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the UART driver.
int32_t uPortUartInit()
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        for (size_t x = 0; x < sizeof(gUartData) / sizeof(gUartData[0]); x++) {
            gUartData[x].pRxBuff = NULL;
        }
    }

    return (int32_t) errorCode;
}

// Deinitialise the UART driver.
void uPortUartDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Close all the UART instances
        for (size_t x = 0; x < sizeof(gUartData) / sizeof(gUartData[0]); x++) {
            if (gUartData[x].pRxBuff != NULL) {
                uartClose(x);
            }
        }

        // Delete the mutex
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
    uErrorCode_t handleOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    int32_t baudRateNrf = baudRateToNrfBaudRate(baudRate);
    uint32_t pinCtsNrf = NRF_UARTE_PSEL_DISCONNECTED;
    uint32_t pinRtsNrf = NRF_UARTE_PSEL_DISCONNECTED;
    nrf_uarte_hwfc_t hwfc = NRF_UARTE_HWFC_DISABLED;
    NRF_UARTE_Type *pReg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) &&
            (uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (baudRateNrf > 0) && (pinRx >= 0) && (pinTx >= 0) &&
            (pReceiveBuffer == NULL) && (gUartData[uart].pRxBuff == NULL)) {

            handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
            pReg = gUartData[uart].pReg;

            if (uPortQueueCreate(U_PORT_UART_TX_QUEUE_LENGTH,
                                 sizeof(uartTxData_t),
                                 &gUartData[uart].txQueueHandle) == U_ERROR_COMMON_SUCCESS) {
                handleOrErrorCode = U_ERROR_COMMON_SUCCESS;

                // Malloc memory for the read buffer
                gUartData[uart].rxBufferSizeBytes = receiveBufferSizeBytes;
                gUartData[uart].pRxBuff = pUPortMalloc(gUartData[uart].rxBufferSizeBytes);
                gUartData[uart].pTxBuff = NULL;

                // Set up the rest of the UART data structure
                gUartData[uart].uartHandle = uart;
                gUartData[uart].hwfcSuspended = false;
                gUartData[uart].eventQueueHandle = -1;
                gUartData[uart].eventFilter = 0;
                gUartData[uart].pEventCallback = NULL;
                gUartData[uart].pEventCallbackParam = NULL;
                gUartData[uart].bufferRead = 0;
                gUartData[uart].bufferWrite = 0;
                gUartData[uart].bufferFull = false;
                uPortSemaphoreCreate(&gUartData[uart].txSem, 0, 1);
                nrf_uarte_disable(pReg);
                // Set baud rate
                nrf_uarte_baudrate_set(pReg, baudRateNrf);
                // Set Tx/Rx pins
                nrf_gpio_pin_set(pinTx);
                nrf_gpio_cfg_output(pinTx);
                nrf_uarte_txrx_pins_set(pReg, pinTx, pinRx);
                // Set flow control
                if (pinCts >= 0) {
                    pinCtsNrf = pinCts;
                    nrf_gpio_cfg_input(pinCtsNrf,
                                       NRF_GPIO_PIN_NOPULL);
                    hwfc = NRF_UARTE_HWFC_ENABLED;
                }
                if (pinRts >= 0) {
                    pinRtsNrf = pinRts;
                    nrf_gpio_pin_set(pinRtsNrf);
                    nrf_gpio_cfg_output(pinRtsNrf);
                    hwfc = NRF_UARTE_HWFC_ENABLED;
                }
                if (hwfc == NRF_UARTE_HWFC_ENABLED) {
                    nrf_uarte_hwfc_pins_set(pReg, pinRtsNrf, pinCtsNrf);
                }
                // Configure the UART
                nrf_uarte_configure(pReg, NRF_UARTE_PARITY_EXCLUDED, hwfc);
                // Enable the UART
                nrf_uarte_enable(pReg);
                // Clear flags, set Rx interrupt and buffer and let it go
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDRX);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDTX);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ERROR);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_RXSTARTED);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_TXSTOPPED);
                // Off we go
                nrf_uarte_rx_buffer_set(pReg,
                                        &gUartData[uart].rxDmaBuff,
                                        U_PORT_UART_RX_DMA_LENGTH);
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTRX);
                nrf_uarte_int_enable(pReg, NRF_UARTE_INT_ENDTX_MASK |
                                     NRF_UARTE_INT_TXSTOPPED_MASK |
                                     NRF_UARTE_INT_ENDRX_MASK);

                // Turn off Tx at the moment to save power,
                // This will be enabled when there is data to be Txed
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPTX);

                NRFX_IRQ_PRIORITY_SET(getIrqNumber((void *) pReg),
                                      NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY);
                NRFX_IRQ_ENABLE(getIrqNumber((void *) (pReg)));
                // Return the handle
                handleOrErrorCode = gUartData[uart].uartHandle;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) handleOrErrorCode;
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        uartClose(handle);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
            sizeOrErrorCode = uartGetRxdBytes(&gUartData[handle]);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    NRF_UARTE_Type *pReg;
    uint8_t *pRxBuff;
    uint32_t rxBuffSize;
    uint8_t *pTempBuff;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (sizeBytes > 0) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

            pReg = gUartData[handle].pReg;
            pRxBuff = gUartData[handle].pRxBuff;
            pTempBuff = (uint8_t *)pBuffer;
            rxBuffSize = gUartData[handle].rxBufferSizeBytes;
            // No of bytes available to read
            sizeOrErrorCode = uartGetRxdBytes(&gUartData[handle]);

            if (sizeOrErrorCode > 0) {

                // Set to no of bytes requested
                if (sizeOrErrorCode > sizeBytes) {
                    sizeOrErrorCode = sizeBytes;
                }

                for (uint32_t i = 0; i < sizeOrErrorCode; i++) {

                    *pTempBuff++ = pRxBuff[gUartData[handle].bufferRead];
                    gUartData[handle].bufferRead++;
                    gUartData[handle].bufferRead %= rxBuffSize;
                }

                // Reset buffer full condition and renable
                // Rx interrupts
                gUartData[handle].bufferFull = false;
                nrf_uarte_int_enable(pReg, NRF_UARTE_INT_ENDRX_MASK);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    NRF_UARTE_Type *pReg;
    uartTxData_t txData;
    uPortQueueHandle_t txQueueHandle;

    if (gMutex != NULL) {

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

            U_PORT_MUTEX_LOCK(gMutex);
            pReg = gUartData[handle].pReg;
            txQueueHandle = gUartData[handle].txQueueHandle;

            txData.handle = handle;
            txData.pData = (void *)pBuffer;
            txData.len = sizeBytes;
            // enqueue the buffer here and retrieve it when
            // the TXSTOPPED interrupt is triggered.
            uPortQueueSend(txQueueHandle, (void *)&txData);
            nrf_uarte_int_enable(pReg, NRF_UARTE_INT_TXSTOPPED_MASK);
            uPortSemaphoreTake(gUartData[handle].txSem);
            U_PORT_MUTEX_UNLOCK(gMutex);
            sizeOrErrorCode = sizeBytes;
        }

    }

    return (int32_t) sizeOrErrorCode;
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
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    char name[16];

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle < 0) &&
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
                gUartData[handle].eventQueueHandle = (int32_t) errorCode;
                gUartData[handle].pEventCallback = pFunction;
                gUartData[handle].pEventCallbackParam = pParam;
                gUartData[handle].eventFilter = filter;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    int32_t eventQueueHandle = -1;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle >= 0)) {
            // Save the eventQueueHandle and set all
            // the parameters to indicate that the
            // queue is closed
            eventQueueHandle = gUartData[handle].eventQueueHandle;
            gUartData[handle].eventQueueHandle = -1;
            gUartData[handle].pEventCallback = NULL;
            gUartData[handle].eventFilter = 0;
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

// Get the callback filter bit-mask.
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle >= 0)) {
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
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle >= 0) &&
            (filter != 0)) {
            gUartData[handle].eventFilter = filter;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Send an event to the callback.
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartEvent_t event;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            errorCode = uPortEventQueueSend(gUartData[handle].eventQueueHandle,
                                            &event, sizeof(event));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Send an event to the callback, but only if there's room on the queue.
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartEvent_t event;
    int32_t startTimeMs = uPortGetTickTimeMs();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (gUartData[handle].eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            do {
                // Push an event to event queue, IRQ version so as not to block
                errorCode = uPortEventQueueSendIrq(gUartData[handle].eventQueueHandle,
                                                   &event, sizeof(event));
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
            } while ((errorCode != 0) &&
                     (uPortGetTickTimeMs() - startTimeMs < delayMs));
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
            (gUartData[handle].eventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(gUartData[handle].eventQueueHandle);
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
            (gUartData[handle].eventQueueHandle >= 0)) {
            sizeOrErrorCode = uPortEventQueueStackMinFree(gUartData[handle].eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    bool rtsFlowControlIsEnabled = false;
    NRF_UARTE_Type *pReg;

    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        pReg = gUartData[handle].pReg;

        if (nrf_uarte_rts_pin_get(pReg) != NRF_UARTE_PSEL_DISCONNECTED) {
            rtsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    bool ctsFlowControlIsEnabled = false;
    NRF_UARTE_Type *pReg;

    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        pReg = gUartData[handle].pReg;

        if (nrf_uarte_cts_pin_get(pReg) != NRF_UARTE_PSEL_DISCONNECTED) {
            ctsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return ctsFlowControlIsEnabled;
}

// Suspend CTS flow control.
int32_t uPortUartCtsSuspend(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    NRF_UARTE_Type *pReg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (!gUartData[handle].hwfcSuspended) {
                pReg = gUartData[handle].pReg;
                if (nrf_uarte_cts_pin_get(pReg) != NRF_UARTE_PSEL_DISCONNECTED) {
                    // Note: this disables flow control in both directions since
                    // it is not possible to do so just for CTS using nRF5
                    nrf_uarte_configure(pReg, NRF_UARTE_PARITY_EXCLUDED,
                                        NRF_UARTE_HWFC_DISABLED);
                    gUartData[handle].hwfcSuspended = true;
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
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        if ((handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
            if (gUartData[handle].hwfcSuspended) {
                nrf_uarte_configure(gUartData[handle].pReg,
                                    NRF_UARTE_PARITY_EXCLUDED,
                                    NRF_UARTE_HWFC_ENABLED);
                gUartData[handle].hwfcSuspended = false;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// End of file
