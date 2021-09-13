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
 * @brief Implementation of the port UART API for the NRF52 platform.
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

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_debug.h"
#include "u_port.h"
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

#include "limits.h" // For INT_MAX
#include "stdlib.h" // For malloc()/free()
#include "string.h" // For memcpy()

/* Note: in order to implement the API we require, where receipt
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
 *
 * Design note: it took ages to get this to work.  The issue
 * is with handling continuous reception that has gaps, i.e. running
 * DMA and also having a timer of some sort to push up to the
 * application any data left in a buffer when the incoming data
 * stream happens to pause. The key is NEVER to stop the UARTE HW,
 * to always have the ENDRX event shorted to a STARTRX task with at
 * least two buffers.  Any attempt to stop and restart
 * the UARTE ends up with character loss; believe me I've tried them
 * all.
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

// The maximum length of DMA for a UARTE
#define U_PORT_UART_MAX_DMA_LENGTH_BYTES 256

// The minimum viable sub-buffer length,
// should be greater than 4 'cos the UART has
// a buffer about that big.
#define U_PORT_UART_MIN_DMA_LENGTH_BYTES 10

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART.
 * ORDER IS IMPORTANT: a portion of this structure is
 * statically initialised.
 */
typedef struct {
    NRF_UARTE_Type *pReg;
    nrfx_timer_t timer;
    nrf_ppi_channel_t ppiChannel;
    int32_t uartHandle;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
    bool rxBufferIsMalloced;
    size_t rxBufferSizeBytes;
    size_t rxSubBufferSizeBytes;
    char *pRxStart; /**< Also used as a marker that this UART is in use. */
    char *pRxBufferWriteNext;
    char *pRxRead;
    size_t startRxByteCount;
    volatile size_t endRxByteCount;
    bool userNeedsNotify; /**< set this when all the data has
                           * been read and hence the user
                           * would like a notification
                           * when new data arrives. */
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
        NRFX_TIMER_INSTANCE(U_CFG_HW_UART_COUNTER_INSTANCE_0),
        -1
    },
    {
        NRF_UARTE1,
        NRFX_TIMER_INSTANCE(U_CFG_HW_UART_COUNTER_INSTANCE_1),
        -1
    }
};
# else
#  if !NRFX_UARTE0_ENABLED
static uPortUartData_t gUartData[] = {NRF_UARTE0,
                                      NRFX_TIMER_INSTANCE(U_CFG_HW_UART_COUNTER_INSTANCE_0),
                                      -1
                                      };
#  else
static uPortUartData_t gUartData[] = {NRF_UARTE1,
                                      NRFX_TIMER_INSTANCE(U_CFG_HW_UART_COUNTER_INSTANCE_1),
                                      -1
                                      };
#  endif
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set up the next receive sub-buffer
static inline void setNextSubBuffer(uPortUartData_t *pUartData)
{
    char **ppNext = &(pUartData->pRxBufferWriteNext);

    *ppNext = *ppNext + pUartData->rxSubBufferSizeBytes;
    if (*ppNext >= pUartData->pRxStart + pUartData->rxBufferSizeBytes) {
        *ppNext = pUartData->pRxStart;
    }
}

// Get the number of received bytes waiting in the buffer.
// Note: this may be called from interrupt context.
static size_t uartGetRxBytes(uPortUartData_t *pUartData)
{
    size_t x;

    // Read the amount of received data from the timer/counter
    // on CC channel 0
    pUartData->endRxByteCount = nrfx_timer_capture(&(pUartData->timer), 0);
    if (pUartData->endRxByteCount >= pUartData->startRxByteCount) {
        x = pUartData->endRxByteCount - pUartData->startRxByteCount;
    } else {
        // Wrapped
        x = INT_MAX - pUartData->startRxByteCount +
            pUartData->endRxByteCount;
    }
    if (x > pUartData->rxBufferSizeBytes) {
        x = pUartData->rxBufferSizeBytes;
    }

    return x;
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
        if (gUartData[handle].pRxStart != NULL) {
            pReg = gUartData[handle].pReg;

            // Disable the counter/timer and associated PPI
            // channel.
            nrfx_timer_disable(&(gUartData[handle].timer));
            nrfx_timer_uninit(&(gUartData[handle].timer));
            nrfx_ppi_channel_disable(gUartData[handle].ppiChannel);
            nrfx_ppi_channel_free(gUartData[handle].ppiChannel);
            gUartData[handle].ppiChannel = -1;

            // Disable Rx interrupts
            nrf_uarte_int_disable(pReg, NRF_UARTE_INT_ERROR_MASK     |
                                  NRF_UARTE_INT_RXSTARTED_MASK);
            NRFX_IRQ_DISABLE(nrfx_get_irq_number((void *) (pReg)));

            // Deregister the timer callback and
            // return the tick timer to normal mode
            uPortPrivateTickTimeSetInterruptCb(NULL, NULL);
            uPortPrivateTickTimeNormalMode();

            // Make sure all transfers are finished before UARTE is
            // disabled to achieve the lowest power consumption
            nrf_uarte_shorts_disable(pReg, NRF_UARTE_SHORT_ENDRX_STARTRX);
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

            // And finally free and mark as NULL the buffer
            if (gUartData[handle].rxBufferIsMalloced) {
                free(gUartData[handle].pRxStart);
            }
            gUartData[handle].pRxStart = NULL;
        }
    }
}

// Callback to be called when the receive check timer has expired.
// pParameter must be a pointer to uPortUartData_t.
static void rxCb(void *pParameter)
{
    uPortUartData_t *pUartData = (uPortUartData_t *) pParameter;
    size_t x;

    x = uartGetRxBytes(pUartData);
    // If there is at least some data and the user needs to
    // be notified, let them know
    if ((x > 0) && pUartData->userNeedsNotify) {
        if ((pUartData->eventQueueHandle >= 0) &&
            (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            uPortUartEvent_t event;
            event.uartHandle = pUartData->uartHandle;
            event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
            uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                                   &event, sizeof(event));
        }
        pUartData->userNeedsNotify = false;
    }
}

// The interrupt handler: only handles Rx events as Tx is blocking.
static void rxIrqHandler(uPortUartData_t *pUartData)
{
    NRF_UARTE_Type *pReg = pUartData->pReg;

    if (nrf_uarte_event_check(pReg,
                              NRF_UARTE_EVENT_RXSTARTED)) {
        // An RX has started so it's OK to update the buffer
        // pointer registers in the hardware for the one that
        // will follow after this one has ended as the
        // Rx buffer register is double-buffered in HW.
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_RXSTARTED);
        nrf_uarte_rx_buffer_set(pReg,
                                (uint8_t *) (pUartData->pRxBufferWriteNext),
                                pUartData->rxSubBufferSizeBytes);
        // Move the write next buffer pointer on
        setNextSubBuffer(pUartData);
    } else if (nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ERROR)) {
        // Clear any errors: no need to do anything, they
        // have no effect upon reception
        nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ERROR);
        nrf_uarte_errorsrc_get_and_clear(pReg);
    }
}

// Dummy counter event handler, required by
// nrfx_timer_init().
static void counterEventHandler(nrf_timer_event_t eventType,
                                void *pContext)
{
    (void) eventType;
    (void) pContext;
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

// Derived from the NRFX functions nrfx_is_in_ram() and
// nrfx_is_word_aligned(), check if a buffer pointer is good
// for DMA
__STATIC_INLINE bool isGoodForDma(const void *pBuffer)
{
    return (((((uint32_t) pBuffer) & 0x3u) == 0u) &&
            ((((uint32_t) pBuffer) & 0xE0000000u) == 0x20000000u));
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
    rxIrqHandler(&(gUartData[0]));
}
#endif

#if !NRFX_UARTE1_ENABLED
void nrfx_uarte_1_irq_handler(void)
{
    rxIrqHandler(&(gUartData[1]));
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
            gUartData[x].pRxStart = NULL;
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
            if (gUartData[x].pRxStart != NULL) {
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
    size_t subBufferSize = U_PORT_UART_MAX_DMA_LENGTH_BYTES;
    size_t numSubBuffers;
    nrfx_timer_config_t timerConfig = NRFX_TIMER_DEFAULT_CONFIG;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) &&
            (uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
            (baudRateNrf > 0) && (pinRx >= 0) && (pinTx >= 0) &&
            ((pReceiveBuffer == NULL) || isGoodForDma(pReceiveBuffer)) &&
            (gUartData[uart].pRxStart == NULL)) {

            // There must be at least two sub-buffers and each buffer
            // cannot be larger than U_PORT_UART_MAX_DMA_LENGTH_BYTES
            numSubBuffers = receiveBufferSizeBytes / subBufferSize;
            if (numSubBuffers < 2) {
                numSubBuffers = 2;
                subBufferSize = receiveBufferSizeBytes / numSubBuffers;
            }
            if (subBufferSize >= U_PORT_UART_MIN_DMA_LENGTH_BYTES) {
                handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                gUartData[uart].rxSubBufferSizeBytes = subBufferSize;
                gUartData[uart].rxBufferSizeBytes = subBufferSize * numSubBuffers;
                pReg = gUartData[uart].pReg;

#if NRFX_PRS_ENABLED
                static nrfx_irq_handler_t const irq_handlers[NRFX_UARTE_ENABLED_COUNT] = {
# if !NRFX_UARTE0_ENABLED
                    nrfx_uarte_0_irq_handler,
# endif
# if !NRFX_UARTE1_ENABLED
                    nrfx_uarte_1_irq_handler,
# endif
                };

                if (nrfx_prs_acquire(pReg, irq_handlers[pReg]) != NRFX_SUCCESS) {
                    handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                }
#endif

                // Set up a counter/timer as a counter to count
                // received characters.  This is required because
                // the DMA doesn't let you know until it's done.
                // This is done first because it can return error
                // codes and there's no point in continuing without
                // it.
                timerConfig.mode = NRF_TIMER_MODE_COUNTER;
                // Has to be 32 bit for overflow to work correctly
                timerConfig.bit_width = NRF_TIMER_BIT_WIDTH_32;
                if (nrfx_timer_init(&(gUartData[uart].timer),
                                    &timerConfig,
                                    counterEventHandler) == NRFX_SUCCESS) {
                    handleOrErrorCode = U_ERROR_COMMON_SUCCESS;
                    // Attach the timer/counter to the RXDRDY event
                    // of the UARTE using PPI
                    if (nrfx_ppi_channel_alloc(&(gUartData[uart].ppiChannel)) == NRFX_SUCCESS) {
                        if ((nrfx_ppi_channel_assign(gUartData[uart].ppiChannel,
                                                     nrf_uarte_event_address_get(pReg,
                                                                                 NRF_UARTE_EVENT_RXDRDY),
                                                     nrfx_timer_task_address_get(&(gUartData[uart].timer),
                                                                                 NRF_TIMER_TASK_COUNT)) != NRFX_SUCCESS) ||
                            (nrfx_ppi_channel_enable(gUartData[uart].ppiChannel) != NRFX_SUCCESS)) {
                            nrfx_ppi_channel_free(gUartData[uart].ppiChannel);
                            gUartData[uart].ppiChannel = -1;
                            handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                        }
                    } else {
                        handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                    }

                    if (handleOrErrorCode == U_ERROR_COMMON_SUCCESS) {
                        handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
                        gUartData[uart].rxBufferIsMalloced = false;
                        gUartData[uart].pRxStart = pReceiveBuffer;
                        if (gUartData[uart].pRxStart == NULL) {
                            // Malloc memory for the read buffer
                            gUartData[uart].pRxStart = malloc(gUartData[uart].rxBufferSizeBytes);
                            gUartData[uart].rxBufferIsMalloced = true;
                        }
                        if (gUartData[uart].pRxStart != NULL) {
                            // Set up the read pointer
                            gUartData[uart].pRxRead = gUartData[uart].pRxStart;
                            // Set up the rest of the UART data structure
                            gUartData[uart].uartHandle = uart;
                            gUartData[uart].pRxBufferWriteNext = gUartData[uart].pRxStart;
                            gUartData[uart].startRxByteCount = 0;
                            gUartData[uart].endRxByteCount = 0;
                            gUartData[uart].eventQueueHandle = -1;
                            gUartData[uart].eventFilter = 0;
                            gUartData[uart].pEventCallback = NULL;
                            gUartData[uart].pEventCallbackParam = NULL;
                            gUartData[uart].userNeedsNotify = true;

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

                            // Let the end of one RX trigger the next immediately
                            nrf_uarte_shorts_enable(pReg, NRF_UARTE_SHORT_ENDRX_STARTRX);

                            // Enable and clear the counter/timer that is counting
                            // received characters.
                            nrfx_timer_enable(&(gUartData[uart].timer));
                            nrfx_timer_clear(&(gUartData[uart].timer));

                            // Off we go
                            nrf_uarte_rx_buffer_set(pReg,
                                                    (uint8_t *) (gUartData[uart].pRxBufferWriteNext),
                                                    gUartData[uart].rxSubBufferSizeBytes);
                            setNextSubBuffer(&(gUartData[uart]));
                            nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTRX);
                            nrf_uarte_int_enable(pReg, NRF_UARTE_INT_ERROR_MASK  |
                                                 NRF_UARTE_INT_RXSTARTED_MASK);
                            NRFX_IRQ_PRIORITY_SET(getIrqNumber((void *) pReg),
                                                  NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY);
                            NRFX_IRQ_ENABLE(getIrqNumber((void *) (pReg)));

                            // Put the tick timer into UART mode and
                            // register the receive timout callback
                            uPortPrivateTickTimeUartMode();
                            uPortPrivateTickTimeSetInterruptCb(rxCb,
                                                               &(gUartData[uart]));
                            // Return the handle
                            handleOrErrorCode = gUartData[uart].uartHandle;
                        } else {
                            // Tidy up if we failed to allocate memory
                            nrfx_timer_disable(&(gUartData[uart].timer));
                            nrfx_timer_uninit(&(gUartData[uart].timer));
                            nrfx_ppi_channel_disable(gUartData[uart].ppiChannel);
                            nrfx_ppi_channel_free(gUartData[uart].ppiChannel);
                            gUartData[uart].ppiChannel = -1;
                        }
                    }
                }
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

            sizeOrErrorCode = uartGetRxBytes(&(gUartData[handle]));
            if (sizeOrErrorCode == 0) {
                gUartData[handle].userNeedsNotify = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    size_t totalRead;
    size_t thisRead;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (sizeBytes > 0) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {
            sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;

            // The user can't read more than
            // rxBufferSizeBytes
            if (sizeBytes > gUartData[handle].rxBufferSizeBytes) {
                sizeBytes = gUartData[handle].rxBufferSizeBytes;
            }

            // Before reading anything we re-enable RX events from UART
            gUartData[handle].userNeedsNotify = true;

            // Get the number of bytes available to read
            totalRead = uartGetRxBytes(&(gUartData[handle]));
            if (totalRead > sizeBytes) {
                totalRead = sizeBytes;
            }

            // Copy out from the read pointer onwards,
            // stopping at the end of the buffer or
            // totalRead, whichever comes first
            thisRead = gUartData[handle].pRxStart +
                       gUartData[handle].rxBufferSizeBytes -
                       gUartData[handle].pRxRead;
            if (thisRead > totalRead) {
                thisRead = totalRead;
            }
            memcpy(pBuffer, gUartData[handle].pRxRead, thisRead);
            pBuffer = (char *) pBuffer + thisRead;
            gUartData[handle].pRxRead += thisRead;
            if (gUartData[handle].pRxRead >= gUartData[handle].pRxStart +
                gUartData[handle].rxBufferSizeBytes) {
                gUartData[handle].pRxRead = gUartData[handle].pRxStart;
            }

            // Copy out any remainder
            if (thisRead < totalRead) {
                thisRead = totalRead - thisRead;
                memcpy(pBuffer, gUartData[handle].pRxRead, thisRead);
                gUartData[handle].pRxRead += thisRead;
            }

            // Update the starting number for the byte count
            gUartData[handle].startRxByteCount += totalRead;

            // Set the return value
            sizeOrErrorCode = totalRead;
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
    const char *pTxBuffer = NULL;
    char *pTmpBuffer = NULL;
    NRF_UARTE_Type *pReg;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pBuffer != NULL) && (handle >= 0) &&
            (handle < sizeof(gUartData) / sizeof(gUartData[0]))) {

            sizeOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
            pReg = gUartData[handle].pReg;

            // If the provided buffer is not good for
            // DMA (e.g. if it's in flash) then copy
            // it to somewhere that is
            if (!isGoodForDma(pBuffer)) {
                pTmpBuffer = malloc(sizeBytes);
                if (pTmpBuffer != NULL) {
                    memcpy(pTmpBuffer, pBuffer, sizeBytes);
                    pTxBuffer = pTmpBuffer;
                }
            } else {
                pTxBuffer = (const char *) pBuffer;
            }

            if (pTxBuffer != NULL) {
                // Set up the flags
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDTX);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_TXSTOPPED);
                nrf_uarte_tx_buffer_set(pReg, (uint8_t const *) pTxBuffer,
                                        sizeBytes);
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTTX);

                // Wait for the transmission to complete
                // Hint when debugging: if your code stops dead here
                // it is because the CTS line of this MCU's UART HW
                // is floating high, stopping the UART from
                // transmitting once its buffer is full: either
                // the thing at the other end doesn't want data sent to
                // it or the CTS pin when configuring this UART
                // was wrong and it's not connected to the right
                // thing.
                while (!nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ENDTX)) {}

                // Put UARTE into lowest power state.
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPTX);
                while (!nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED)) {}

                sizeOrErrorCode = sizeBytes;
            }

            // Free memory (it is valid C to free a NULL buffer)
            free(pTmpBuffer);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
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

// End of file
