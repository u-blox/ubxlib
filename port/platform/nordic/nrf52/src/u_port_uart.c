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

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_os.h"
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
 * stream happens to pauuse. The key is NEVER to stop the UARTE HW,
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


// Length of DMA on NRF52840 HW.
// Note that the maximum length is 256 however
// the cost of starting a new DMA buffer is
// zero (since the pointer is double-buffered
// buffered in HW) so setting this to a smaller
// value so that the user can set
// U_PORT_UART_RX_BUFFER_SIZE to a smaller
// value and still have at least two buffers.
#ifndef U_PORT_UART_SUB_BUFFER_SIZE
# define U_PORT_UART_SUB_BUFFER_SIZE 128
#endif

// The number of sub-buffers.
#define U_PORT_UART_NUM_SUB_BUFFERS (U_PORT_UART_RX_BUFFER_SIZE / \
                                     U_PORT_UART_SUB_BUFFER_SIZE)

#if U_PORT_UART_NUM_SUB_BUFFERS < 2
# error Cannot accommodate two sub-buffers, either increase U_PORT_UART_RX_BUFFER_SIZE to a larger multiple of U_PORT_UART_SUB_BUFFER_SIZE or reduce U_PORT_UART_SUB_BUFFER_SIZE.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A UART event.  Since we only ever need to signal
 * size or error then on this platform the
 * uPortUartEventData_t can simply be an int32_t.
 */
typedef int32_t uPortUartEventData_t;

/** UART receive buffer structure, which can be used as a list.
 */
typedef struct uPortUartBuffer_t {
    char *pStart;
    struct uPortUartBuffer_t *pNext;
} uPortUartBuffer_t;

/** Structure of the things we need to keep track of per UART.
 */
typedef struct {
    NRF_UARTE_Type *pReg;
    nrfx_timer_t timer;
    nrf_ppi_channel_t ppiChannel;
    uPortMutexHandle_t mutex;
    uPortQueueHandle_t queue;
    char *pRxStart;
    uPortUartBuffer_t *pRxBufferWriteNext;
    char *pRxRead;
    size_t startRxByteCount;
    volatile size_t endRxByteCount;
    bool userNeedsNotify; /**< set this when all the data has
                           * been read and hence the user
                           * would like a notification
                           * when new data arrives. */
    uPortUartBuffer_t rxBufferList[U_PORT_UART_NUM_SUB_BUFFERS];
} uPortUartData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// UART data, where the UARTE register and the
// associated counter and PPI channel are the only
// ones initialised here.
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
    if (x > U_PORT_UART_RX_BUFFER_SIZE) {
        x = U_PORT_UART_RX_BUFFER_SIZE;
    }

    return x;
}

// Callback to be called when the receive check timer has expired.
// pParameter must be a pointer to CellularPortUartData_t.
static void rxCb(void *pParameter)
{
    uPortUartData_t *pUartData = (uPortUartData_t *) pParameter;
    size_t x;
    BaseType_t yield = false;

    x = uartGetRxBytes(pUartData);
    // If there is at least some data and the user needs to
    // be notified, let them know
    if ((x > 0) && pUartData->userNeedsNotify) {
        uPortUartEventData_t uartSizeOrError;
        uartSizeOrError = x;
        xQueueSendFromISR((QueueHandle_t) (pUartData->queue),
                          &uartSizeOrError, &yield);
        pUartData->userNeedsNotify = false;
    }

    // Required for FreeRTOS task scheduling to work
    if (yield) {
        taskYIELD();
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
                                (uint8_t *) (pUartData->pRxBufferWriteNext->pStart),
                                U_PORT_UART_SUB_BUFFER_SIZE);
        // Move the write next buffer pointer on
        pUartData->pRxBufferWriteNext = pUartData->pRxBufferWriteNext->pNext;
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
__STATIC_INLINE bool isGoodForDma(void const *pBuffer)
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

// Initialise a UARTE.
int32_t uPortUartInit(int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts,
                      int32_t baudRate,
                      int32_t uart,
                      uPortQueueHandle_t *pUartQueue)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t baudRateNrf = baudRateToNrfBaudRate(baudRate);
    uint32_t pinCtsNrf = NRF_UARTE_PSEL_DISCONNECTED;
    uint32_t pinRtsNrf = NRF_UARTE_PSEL_DISCONNECTED;
    nrf_uarte_hwfc_t hwfc = NRF_UARTE_HWFC_DISABLED;
    NRF_UARTE_Type *pReg;
    char *pRxBuffer = NULL;
    nrfx_timer_config_t timerConfig = NRFX_TIMER_DEFAULT_CONFIG;

    if ((pUartQueue != NULL) && (pinRx >= 0) && (pinTx >= 0) &&
        (uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
        (baudRateNrf >= 0)) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        if (gUartData[uart].mutex == NULL) {
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
            if (nrfx_prs_acquire(pReg, irq_handlers[pReg) != NRFX_SUCCESS) {
                  errorCode = U_ERROR_COMMON_PLATFORM;
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
                        errorCode = U_ERROR_COMMON_PLATFORM;
                    }
                } else {
                    errorCode = U_ERROR_COMMON_PLATFORM;
                }
            } else {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }

            if (errorCode == 0) {
            // Create the mutex
            errorCode = uPortMutexCreate(&(gUartData[uart].mutex));
                if (errorCode == 0) {

                    U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

                    errorCode = U_ERROR_COMMON_NO_MEMORY;

                    // Malloc memory for the read buffer
                    pRxBuffer = malloc(U_PORT_UART_RX_BUFFER_SIZE);
                    if (pRxBuffer != NULL) {
                        gUartData[uart].pRxStart = pRxBuffer;
                        // Set up the read pointer
                        gUartData[uart].pRxRead = pRxBuffer;
                        // Set up the buffer list for the DMA write process
                        for (size_t x = 0; x < sizeof(gUartData[uart].rxBufferList) /
                             sizeof(gUartData[uart].rxBufferList[0]); x++) {
                            gUartData[uart].rxBufferList[x].pStart = pRxBuffer;
                            // Set up the next pointers in a ring
                            if (x < (sizeof(gUartData[uart].rxBufferList) /
                                     sizeof(gUartData[uart].rxBufferList[0])) - 1) {
                                gUartData[uart].rxBufferList[x].pNext =  &(gUartData[uart].rxBufferList[x + 1]);
                            } else {
                                gUartData[uart].rxBufferList[x].pNext = &(gUartData[uart].rxBufferList[0]);
                            }
                            pRxBuffer += U_PORT_UART_SUB_BUFFER_SIZE;
                        }
                        // Set up the write buffer pointer etc.
                        gUartData[uart].pRxBufferWriteNext = &(gUartData[uart].rxBufferList[0]);
                        gUartData[uart].startRxByteCount = 0;
                        gUartData[uart].endRxByteCount = 0;
                        gUartData[uart].userNeedsNotify = true;

                        // Create the queue
                        errorCode = uPortQueueCreate(U_PORT_UART_EVENT_QUEUE_SIZE,
                                                     sizeof(uPortUartEventData_t),
                                                     pUartQueue);
                        if (errorCode == 0) {
                            gUartData[uart].queue = *pUartQueue;

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
                                                    (uint8_t *) (gUartData[uart].pRxBufferWriteNext->pStart),
                                                    U_PORT_UART_SUB_BUFFER_SIZE);
                            gUartData[uart].pRxBufferWriteNext = gUartData[uart].pRxBufferWriteNext->pNext;
                            nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTRX);
                            nrf_uarte_int_enable(pReg, NRF_UARTE_INT_ERROR_MASK     |
                                                 NRF_UARTE_INT_RXSTARTED_MASK);
                            NRFX_IRQ_PRIORITY_SET(getIrqNumber((void *) pReg),
                                                  NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY);
                            NRFX_IRQ_ENABLE(getIrqNumber((void *) (pReg)));

                            // Put the tick timer into UART mode and
                            // register the receive timout callback
                            uPortPrivateTickTimeUartMode();
                            uPortPrivateTickTimeSetInterruptCb(rxCb,
                                                               &(gUartData[uart]));
                        }
                    }

                    U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);

                    // If we failed to create the queue or get memory for the buffer,
                    // delete the mutex, free memory, put the uart's
                    // mutex back to NULL and disable the counter/timer, freeing
                    // the PPI channel if it was allocated.
                    if ((errorCode != 0) ||
                        (pRxBuffer == NULL)) {
                        uPortMutexDelete(gUartData[uart].mutex);
                        gUartData[uart].mutex = NULL;
                        free(pRxBuffer);
                        nrfx_timer_disable(&(gUartData[uart].timer));
                        nrfx_timer_uninit(&(gUartData[uart].timer));
                        if (gUartData[uart].ppiChannel != -1) {
                            nrfx_ppi_channel_disable(gUartData[uart].ppiChannel);
                            nrfx_ppi_channel_free(gUartData[uart].ppiChannel);
                            gUartData[uart].ppiChannel = -1;
                        }
                    }
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
    uint32_t pinRtsNrf;
    uint32_t pinCtsNrf;
    NRF_UARTE_Type *pReg;

    if (uart < sizeof(gUartData) / sizeof(gUartData[0])) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        if (gUartData[uart].mutex != NULL) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            pReg = gUartData[uart].pReg;

            // No need to lock the mutex, we need to delete it
            // and we're not allowed to delete a locked mutex.
            // The caller needs to make sure that no read/write
            // is in progress when this function is called.

            // Disable the counter/timer and associated PPI
            // channel.
            nrfx_timer_disable(&(gUartData[uart].timer));
            nrfx_timer_uninit(&(gUartData[uart].timer));
            nrfx_ppi_channel_disable(gUartData[uart].ppiChannel);
            nrfx_ppi_channel_free(gUartData[uart].ppiChannel);
            gUartData[uart].ppiChannel = -1;

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

            // Delete the queue
            uPortQueueDelete(gUartData[uart].queue);
            gUartData[uart].queue = NULL;
            // Free the buffer
            free(gUartData[uart].pRxStart);
            // Delete the mutex
            uPortMutexDelete(gUartData[uart].mutex);
            gUartData[uart].mutex = NULL;
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Push a UART event onto the UART event queue.
int32_t uPortUartEventSend(const uPortQueueHandle_t queueHandle,
                           int32_t sizeBytesOrError)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortUartEventData_t uartSizeOrError;

    if (queueHandle != NULL) {
        uartSizeOrError = sizeBytesOrError;
        errorCode = uPortQueueSend(queueHandle,
                                   (void *) &uartSizeOrError);
    }

    return errorCode;
}

// Receive a UART event, blocking until one turns up.
int32_t uPortUartEventReceive(const uPortQueueHandle_t queueHandle)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortUartEventData_t uartSizeOrError;

    if (queueHandle != NULL) {
        sizeOrErrorCode = U_ERROR_COMMON_PLATFORM;
        if (uPortQueueReceive(queueHandle,
                              &uartSizeOrError) == 0) {
            sizeOrErrorCode = uartSizeOrError;
        }
    }

    return sizeOrErrorCode;
}

// Receive a UART event with a timeout.
int32_t uPortUartEventTryReceive(const uPortQueueHandle_t queueHandle,
                                 int32_t waitMs)
{
    int32_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uPortUartEventData_t uartSizeOrError;

    if (queueHandle != NULL) {
        sizeOrErrorCode = U_ERROR_COMMON_TIMEOUT;
        if (uPortQueueTryReceive(queueHandle, waitMs,
                                 &uartSizeOrError) == 0) {
            sizeOrErrorCode = uartSizeOrError;
        }
    }

    return sizeOrErrorCode;
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t uart)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (uart < sizeof(gUartData) / sizeof(gUartData[0])) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gUartData[uart].mutex != NULL) {

            U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

            sizeOrErrorCode = uartGetRxBytes(&(gUartData[uart]));
            if (sizeOrErrorCode == 0) {
                gUartData[uart].userNeedsNotify = true;
            }

            U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);

        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t uart, char *pBuffer,
                      size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    size_t totalRead;
    size_t thisRead;

    if ((pBuffer != NULL) && (sizeBytes > 0) &&
        (uart < sizeof(gUartData) / sizeof(gUartData[0]))) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gUartData[uart].mutex != NULL) {

            U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

            // The user can't read more than
            // U_PORT_UART_RX_BUFFER_SIZE
            if (sizeBytes > U_PORT_UART_RX_BUFFER_SIZE) {
                sizeBytes = U_PORT_UART_RX_BUFFER_SIZE;
            }

            // Get the number of bytes available to read
            totalRead = uartGetRxBytes(&(gUartData[uart]));
            if (totalRead > sizeBytes) {
                totalRead = sizeBytes;
            }

            // Copy out from the read pointer onwards,
            // stopping at the end of the buffer or
            // totalRead, whichever comes first
            thisRead = gUartData[uart].pRxStart +
                       U_PORT_UART_RX_BUFFER_SIZE -
                       gUartData[uart].pRxRead;
            if (thisRead > totalRead) {
                thisRead = totalRead;
            }
            memcpy(pBuffer, gUartData[uart].pRxRead, thisRead);
            gUartData[uart].pRxRead += thisRead;
            pBuffer += thisRead;
            if (gUartData[uart].pRxRead >= gUartData[uart].pRxStart +
                U_PORT_UART_RX_BUFFER_SIZE) {
                gUartData[uart].pRxRead = gUartData[uart].pRxStart;
            }

            // Copy out any remainder
            if (thisRead < totalRead) {
                thisRead = totalRead - thisRead;
                memcpy(pBuffer, gUartData[uart].pRxRead, thisRead);
                gUartData[uart].pRxRead += thisRead;
            }

            // Update the starting number for the byte count
            gUartData[uart].startRxByteCount += totalRead;

            // Set the return value
            sizeOrErrorCode = totalRead;

            // Set the notify flag if we were unable
            // to read anything
            gUartData[uart].userNeedsNotify = false;
            if (sizeOrErrorCode == 0) {
                gUartData[uart].userNeedsNotify = true;
            }

            U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);
        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t uart, const char *pBuffer,
                       size_t sizeBytes)
{
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    const char *pTxBuffer = NULL;
    char *pTmpBuffer = NULL;
    NRF_UARTE_Type *pReg;

    if ((pBuffer != NULL) &&
        (uart < sizeof(gUartData) / sizeof(gUartData[0]))) {
        sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gUartData[uart].mutex != NULL) {

            U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

            sizeOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
            pReg = gUartData[uart].pReg;

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
                pTxBuffer = pBuffer;
            }

            if (pTxBuffer != NULL) {
                // Set up the flags
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_ENDTX);
                nrf_uarte_event_clear(pReg, NRF_UARTE_EVENT_TXSTOPPED);
                nrf_uarte_tx_buffer_set(pReg, (uint8_t const *) pTxBuffer,
                                        sizeBytes);
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STARTTX);

                // Wait for the transmission to complete
                while (!nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_ENDTX)) {}

                // Put UARTE into lowest power state.
                nrf_uarte_task_trigger(pReg, NRF_UARTE_TASK_STOPTX);
                while (!nrf_uarte_event_check(pReg, NRF_UARTE_EVENT_TXSTOPPED)) {}

                sizeOrErrorCode = sizeBytes;
            }

            // Free memory (it is valid C to free a NULL buffer)
            free(pTmpBuffer);

            U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);
        }
    }

    return (int32_t) sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortIsRtsFlowControlEnabled(int32_t uart)
{
    bool rtsFlowControlIsEnabled = false;
    NRF_UARTE_Type *pReg;

    if ((uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
        (gUartData[uart].mutex != NULL)) {

        U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

        pReg = gUartData[uart].pReg;

        if (nrf_uarte_rts_pin_get(pReg) != NRF_UARTE_PSEL_DISCONNECTED) {
            rtsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);
    }

    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortIsCtsFlowControlEnabled(int32_t uart)
{
    bool ctsFlowControlIsEnabled = false;
    NRF_UARTE_Type *pReg;

    if ((uart < sizeof(gUartData) / sizeof(gUartData[0])) &&
        (gUartData[uart].mutex != NULL)) {

        U_PORT_MUTEX_LOCK(gUartData[uart].mutex);

        pReg = gUartData[uart].pReg;

        if (nrf_uarte_cts_pin_get(pReg) != NRF_UARTE_PSEL_DISCONNECTED) {
            ctsFlowControlIsEnabled = true;
        }

        U_PORT_MUTEX_UNLOCK(gUartData[uart].mutex);
    }

    return ctsFlowControlIsEnabled;
}

// End of file
