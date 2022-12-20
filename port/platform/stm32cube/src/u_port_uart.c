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
 * @brief Implementation of the port UART API for the STM32F4 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()

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

#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"

#include "cmsis_os.h"

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

#include "string.h" // for memcpy()

/* The code here was written using the really useful information
 * here:
 *
 * https://stm32f4-discovery.net/2017/07/stm32-tutorial-efficiently-receive-uart-data-using-dma/
 *
 * This code uses the LL API, as that tutorial does, and sticks
 * to it exactly, hence where the LL API has a series of
 * named functions rather than taking a parameter (e.g.
 * LL_DMA_ClearFlag_HT0(), LL_DMA_ClearFlag_HT1(), etc.)
 * the correct function is accessed through a jump table,
 * making it possible to use it in a parameterised manner
 * again.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The maximum number of UART HW blocks on an STM32F4.
#define U_PORT_MAX_NUM_UARTS 8

// The maximum number of DMA engines on an STM32F4.
#define U_PORT_MAX_NUM_DMA_ENGINES 2

// The maximum number of DMA streams on an STM32F4.
#define U_PORT_MAX_NUM_DMA_STREAMS 8

// Determine if the given DMA engine/stream interrupt is in use
#define U_PORT_DMA_INTERRUPT_IN_USE(x, y) (((U_CFG_HW_UART1_AVAILABLE != 0) && (U_CFG_HW_UART1_DMA_ENGINE == x) && (U_CFG_HW_UART1_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART2_AVAILABLE != 0) && (U_CFG_HW_UART2_DMA_ENGINE == x) && (U_CFG_HW_UART2_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART3_AVAILABLE != 0) && (U_CFG_HW_UART3_DMA_ENGINE == x) && (U_CFG_HW_UART3_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART4_AVAILABLE != 0) && (U_CFG_HW_UART4_DMA_ENGINE == x) && (U_CFG_HW_UART4_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART5_AVAILABLE != 0) && (U_CFG_HW_UART5_DMA_ENGINE == x) && (U_CFG_HW_UART5_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART6_AVAILABLE != 0) && (U_CFG_HW_UART6_DMA_ENGINE == x) && (U_CFG_HW_UART6_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART7_AVAILABLE != 0) && (U_CFG_HW_UART7_DMA_ENGINE == x) && (U_CFG_HW_UART7_DMA_STREAM == y)) || \
                                           ((U_CFG_HW_UART8_AVAILABLE != 0) && (U_CFG_HW_UART8_DMA_ENGINE == x) && (U_CFG_HW_UART8_DMA_STREAM == y)))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A UART event.  Since we only ever need to signal
 * size or error then on this platform the
 * uPortUartEventData_t can simply be an int32_t.
 */
typedef int32_t uPortUartEventData_t;

/** Structure of the constant data per UART.
 */
typedef struct {
    USART_TypeDef *pReg;
    uint32_t dmaEngine;
    uint32_t dmaStream;
    uint32_t dmaChannel;
    IRQn_Type irq;
} uPortUartConstData_t;

/** Structure of the data per UART.
 */
typedef struct uPortUartData_t {
    int32_t uart;
    int32_t uartHandle;
    bool ctsSuspended;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
    const uPortUartConstData_t *pConstData;
    bool rxBufferIsMalloced;
    size_t rxBufferSizeBytes;
    char *pRxBufferStart;
    char *pRxBufferRead;
    volatile char *pRxBufferWrite;
    struct uPortUartData_t *pNext;
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
} uPortUartEvent_t;

/** Function pointers for STM32Cube functions
 */
typedef void (*uClockEnFunc_t)(uint32_t);
typedef void (*uDmaFunc_t)(DMA_TypeDef *);
typedef uint32_t (*uDmaActiveFunc_t)(DMA_TypeDef *);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Root of the UART linked list.
static uPortUartData_t *gpUartDataHead = NULL;

// Mutex to protect the linked list.
static uPortMutexHandle_t gMutex = NULL;

// The next UART handle to use
static int32_t gNextHandle = 0;

// Get the bus enable function for the given UART/USART.
static const uClockEnFunc_t gLlApbClkEnable[] = {
    0, // This to avoid having to -1 all the time
    LL_APB2_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock,
    LL_APB2_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock,
    LL_APB1_GRP1_EnableClock
};

// Get the LL driver peripheral number for a given UART/USART.
static const uint32_t gLlApbGrpPeriphUart[] = {
    0, // This to avoid having to -1 all the time
    LL_APB2_GRP1_PERIPH_USART1,
    LL_APB1_GRP1_PERIPH_USART2,
    LL_APB1_GRP1_PERIPH_USART3,
    LL_APB1_GRP1_PERIPH_UART4,
    LL_APB1_GRP1_PERIPH_UART5,
    LL_APB2_GRP1_PERIPH_USART6,
    LL_APB1_GRP1_PERIPH_UART7,
    LL_APB1_GRP1_PERIPH_UART8
};

// Get the LL driver peripheral number for a given DMA engine.
static const uint32_t gLlApbGrpPeriphDma[] = {
    0, // This to avoid having to -1 all the time
    LL_AHB1_GRP1_PERIPH_DMA1,
    LL_AHB1_GRP1_PERIPH_DMA2
};

// Get the DMA base address for a given DMA engine
static DMA_TypeDef *const gpDmaReg[] =  {
    0,  // This to avoid having to -1 all the time
    DMA1,
    DMA2
};

// Get the alternate function required on a GPIO line for a given UART.
// Note: which function a GPIO line actually performs on that UART is
// hard coded in the chip; for instance see table 12 of the STM32F437 data sheet.
static const uint32_t gGpioAf[] = {
    0, // This to avoid having to -1 all the time
    LL_GPIO_AF_7,  // USART 1
    LL_GPIO_AF_7,  // USART 2
    LL_GPIO_AF_7,  // USART 3
    LL_GPIO_AF_8,  // UART 4
    LL_GPIO_AF_8,  // UART 5
    LL_GPIO_AF_8,  // USART 6
    LL_GPIO_AF_8,  // USART 7
    LL_GPIO_AF_8
}; // UART 8

// Table of stream IRQn for DMA engine 1
static const IRQn_Type gDma1StreamIrq[] = {
    DMA1_Stream0_IRQn,
    DMA1_Stream1_IRQn,
    DMA1_Stream2_IRQn,
    DMA1_Stream3_IRQn,
    DMA1_Stream4_IRQn,
    DMA1_Stream5_IRQn,
    DMA1_Stream6_IRQn,
    DMA1_Stream7_IRQn
};

// Table of stream IRQn for DMA engine 2
static const IRQn_Type gDma2StreamIrq[] = {
    DMA2_Stream0_IRQn,
    DMA2_Stream1_IRQn,
    DMA2_Stream2_IRQn,
    DMA2_Stream3_IRQn,
    DMA2_Stream4_IRQn,
    DMA2_Stream5_IRQn,
    DMA2_Stream6_IRQn,
    DMA2_Stream7_IRQn
};

// Table of DMAx_Stream_IRQn per DMA engine
static const IRQn_Type *gpDmaStreamIrq[] = {
    NULL, // This to avoid having to -1 all the time
    gDma1StreamIrq,
    gDma2StreamIrq
};

// Table of LL_DMA_CHANNEL_x per channel
static const int32_t gLlDmaChannel[] = {
    LL_DMA_CHANNEL_0,
    LL_DMA_CHANNEL_1,
    LL_DMA_CHANNEL_2,
    LL_DMA_CHANNEL_3,
    LL_DMA_CHANNEL_4,
    LL_DMA_CHANNEL_5,
    LL_DMA_CHANNEL_6,
    LL_DMA_CHANNEL_7
};

// Table of functions LL_DMA_ClearFlag_HTx(DMA_TypeDef *DMAx) for each stream.
static const uDmaFunc_t gpLlDmaClearFlagHt[]  = {
    LL_DMA_ClearFlag_HT0,
    LL_DMA_ClearFlag_HT1,
    LL_DMA_ClearFlag_HT2,
    LL_DMA_ClearFlag_HT3,
    LL_DMA_ClearFlag_HT4,
    LL_DMA_ClearFlag_HT5,
    LL_DMA_ClearFlag_HT6,
    LL_DMA_ClearFlag_HT7
};

// Table of functions LL_DMA_ClearFlag_TCx(DMA_TypeDef *DMAx) for each stream.
static const uDmaFunc_t gpLlDmaClearFlagTc[] = {
    LL_DMA_ClearFlag_TC0,
    LL_DMA_ClearFlag_TC1,
    LL_DMA_ClearFlag_TC2,
    LL_DMA_ClearFlag_TC3,
    LL_DMA_ClearFlag_TC4,
    LL_DMA_ClearFlag_TC5,
    LL_DMA_ClearFlag_TC6,
    LL_DMA_ClearFlag_TC7
};

// Table of functions LL_DMA_ClearFlag_TEx(DMA_TypeDef *DMAx) for each stream.
static uDmaFunc_t gpLlDmaClearFlagTe[] = {
    LL_DMA_ClearFlag_TE0,
    LL_DMA_ClearFlag_TE1,
    LL_DMA_ClearFlag_TE2,
    LL_DMA_ClearFlag_TE3,
    LL_DMA_ClearFlag_TE4,
    LL_DMA_ClearFlag_TE5,
    LL_DMA_ClearFlag_TE6,
    LL_DMA_ClearFlag_TE7
};

// Table of functions LL_DMA_ClearFlag_DMEx(DMA_TypeDef *DMAx) for each stream.
static uDmaFunc_t gpLlDmaClearFlagDme[] = {
    LL_DMA_ClearFlag_DME0,
    LL_DMA_ClearFlag_DME1,
    LL_DMA_ClearFlag_DME2,
    LL_DMA_ClearFlag_DME3,
    LL_DMA_ClearFlag_DME4,
    LL_DMA_ClearFlag_DME5,
    LL_DMA_ClearFlag_DME6,
    LL_DMA_ClearFlag_DME7
};

// Table of functions LL_DMA_ClearFlag_FEx(DMA_TypeDef *DMAx) for each stream.
static uDmaFunc_t gpLlDmaClearFlagFe[] = {
    LL_DMA_ClearFlag_FE0,
    LL_DMA_ClearFlag_FE1,
    LL_DMA_ClearFlag_FE2,
    LL_DMA_ClearFlag_FE3,
    LL_DMA_ClearFlag_FE4,
    LL_DMA_ClearFlag_FE5,
    LL_DMA_ClearFlag_FE6,
    LL_DMA_ClearFlag_FE7
};

// Table of functions LL_DMA_IsActiveFlag_HTx(DMA_TypeDef *DMAx) for each stream.
static const uDmaActiveFunc_t gpLlDmaIsActiveFlagHt[] = {
    LL_DMA_IsActiveFlag_HT0,
    LL_DMA_IsActiveFlag_HT1,
    LL_DMA_IsActiveFlag_HT2,
    LL_DMA_IsActiveFlag_HT3,
    LL_DMA_IsActiveFlag_HT4,
    LL_DMA_IsActiveFlag_HT5,
    LL_DMA_IsActiveFlag_HT6,
    LL_DMA_IsActiveFlag_HT7
};

// Table of functions LL_DMA_IsActiveFlag_TCx(DMA_TypeDef *DMAx) for each stream.
static const uDmaActiveFunc_t gpLlDmaIsActiveFlagTc[] = {
    LL_DMA_IsActiveFlag_TC0,
    LL_DMA_IsActiveFlag_TC1,
    LL_DMA_IsActiveFlag_TC2,
    LL_DMA_IsActiveFlag_TC3,
    LL_DMA_IsActiveFlag_TC4,
    LL_DMA_IsActiveFlag_TC5,
    LL_DMA_IsActiveFlag_TC6,
    LL_DMA_IsActiveFlag_TC7
};

// Table of the constant data per UART.
static const uPortUartConstData_t gUartCfg[] = {{}, // This to avoid having to -1 all the time
    {
        USART1,
        U_CFG_HW_UART1_DMA_ENGINE,
        U_CFG_HW_UART1_DMA_STREAM,
        U_CFG_HW_UART1_DMA_CHANNEL,
        USART1_IRQn
    },
    {
        USART2,
        U_CFG_HW_UART2_DMA_ENGINE,
        U_CFG_HW_UART2_DMA_STREAM,
        U_CFG_HW_UART2_DMA_CHANNEL,
        USART2_IRQn
    },
    {
        USART3,
        U_CFG_HW_UART3_DMA_ENGINE,
        U_CFG_HW_UART3_DMA_STREAM,
        U_CFG_HW_UART3_DMA_CHANNEL,
        USART3_IRQn
    },
    {
        UART4,
        U_CFG_HW_UART4_DMA_ENGINE,
        U_CFG_HW_UART4_DMA_STREAM,
        U_CFG_HW_UART4_DMA_CHANNEL,
        UART4_IRQn
    },
    {
        UART5,
        U_CFG_HW_UART5_DMA_ENGINE,
        U_CFG_HW_UART5_DMA_STREAM,
        U_CFG_HW_UART5_DMA_CHANNEL,
        UART5_IRQn
    },
    {
        USART6,
        U_CFG_HW_UART6_DMA_ENGINE,
        U_CFG_HW_UART6_DMA_STREAM,
        U_CFG_HW_UART6_DMA_CHANNEL,
        USART6_IRQn
    },
    {
        UART7,
        U_CFG_HW_UART7_DMA_ENGINE,
        U_CFG_HW_UART7_DMA_STREAM,
        U_CFG_HW_UART7_DMA_CHANNEL,
        UART7_IRQn
    },
    {
        UART8,
        U_CFG_HW_UART8_DMA_ENGINE,
        U_CFG_HW_UART8_DMA_STREAM,
        U_CFG_HW_UART8_DMA_CHANNEL,
        UART8_IRQn
    }
};

// Table to make it possible for UART interrupts to get to the UART data
// without having to trawl through a list.  +1 is for the usual reason.
static uPortUartData_t *gpUart[U_PORT_MAX_NUM_UARTS + 1] = {NULL};

// Table to make it possible for a DMA interrupt to
// get to the UART data.  +1 is for the usual reason.
static uPortUartData_t *gpDmaUart[U_PORT_MAX_NUM_DMA_ENGINES + 1][U_PORT_MAX_NUM_DMA_STREAMS] = {NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the next free handle.
static int32_t nextHandleGet()
{
    int32_t handle = gNextHandle;

    gNextHandle++;
    // Can't have a negative handle, that means fail
    if (gNextHandle < 0) {
        gNextHandle = 0;
    }

    return handle;
}

// Add a UART data structure to the list.
// The required memory is allocated.
// Note: gMutex should be locked before this is called.
static uPortUartData_t *pAddUart(uPortUartData_t *pUartData)
{
    uPortUartData_t **ppUartData = &gpUartDataHead;

    // Go to the end of the list
    while (*ppUartData != NULL) {
        ppUartData = &((*ppUartData)->pNext);
    }

    // Malloc memory for the item
    *ppUartData = (uPortUartData_t *) pUPortMalloc(sizeof(uPortUartData_t));
    if (*ppUartData != NULL) {
        // Copy the data in
        memcpy(*ppUartData, pUartData, sizeof(uPortUartData_t));
        (*ppUartData)->pNext = NULL;
        // Set the UART table up to point to it
        // so that the UART interrupt can find it
        gpUart[pUartData->uart] = *ppUartData;
        // And set the other table up so that the
        // DMA interrupt can find the UART data as well
        gpDmaUart[pUartData->pConstData->dmaEngine][pUartData->pConstData->dmaStream] = *ppUartData;
    }

    return *ppUartData;
}

// Find the UART data structure for a given handle.
// Note: gMutex should be locked before this is called.
static uPortUartData_t *pGetUartDataByHandle(int32_t handle)
{
    uPortUartData_t *pUartData = gpUartDataHead;
    bool found = false;

    while (!found && (pUartData != NULL)) {
        if (pUartData->uartHandle == handle) {
            found = true;
        } else {
            pUartData = pUartData->pNext;
        }
    }

    return pUartData;
}

// Find the UART data structure for a given UART.
// Note: gMutex should be locked before this is called.
static uPortUartData_t *pGetUartDataByUart(int32_t uart)
{
    uPortUartData_t *pUartData = gpUartDataHead;
    bool found = false;

    while (!found && (pUartData != NULL)) {
        if (pUartData->uart == uart) {
            found = true;
        } else {
            pUartData = pUartData->pNext;
        }
    }

    return pUartData;
}

// Remove a UART from the list.
// The memory occupied is free'ed.
// Note: gMutex should be locked before this is called.
static bool removeUart(uPortUartData_t *pUartData)
{
    uPortUartData_t *pList = gpUartDataHead;
    uPortUartData_t *pTmp = NULL;
    bool found = false;

    // Find it in the list
    while (!found && (pList != NULL)) {
        if (pList == pUartData) {
            found = true;
        } else {
            pTmp = pList;
            pList = pList->pNext;
        }
    }

    // Remove the item
    if (pList != NULL) {
        // Move the next pointer of the previous
        // entry on
        if (pTmp != NULL) {
            pTmp->pNext = pList->pNext;
        }
        // NULL the entries in the two tables
        gpUart[pList->uart] = NULL;
        gpDmaUart[pList->pConstData->dmaEngine][pList->pConstData->dmaStream] = NULL;
        // Set the new head pointer if it's the head and free memory
        if (pList == gpUartDataHead) {
            gpUartDataHead = pList->pNext;
        }
        uPortFree(pList);
    }

    return found;
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

    pUartData = pGetUartDataByHandle(pEvent->uartHandle);
    if ((pUartData != NULL) && (pUartData->pEventCallback != NULL)) {
        pUartData->pEventCallback(pEvent->uartHandle,
                                  pEvent->eventBitMap,
                                  pUartData->pEventCallbackParam);
    }
}

// Close a UART instance
// Note: gMutex should be locked before this is called.
static void uartClose(int32_t handle)
{
    uPortUartData_t *pUartData;
    USART_TypeDef *pUartReg;
    uint32_t dmaEngine;
    uint32_t dmaStream;

    pUartData = pGetUartDataByHandle(handle);
    if (pUartData != NULL) {

        pUartReg = gUartCfg[pUartData->uart].pReg;
        dmaEngine = gUartCfg[pUartData->uart].dmaEngine;
        dmaStream = gUartCfg[pUartData->uart].dmaStream;

        // Disable DMA and UART/USART interrupts
        NVIC_DisableIRQ(gpDmaStreamIrq[dmaEngine][dmaStream]);
        NVIC_DisableIRQ(gUartCfg[pUartData->uart].irq);

        // Disable DMA and USART, waiting for DMA to be
        // disabled first according to the note in
        // section 10.3.17 of ST's RM0090.
        LL_DMA_DisableStream(gpDmaReg[dmaEngine], dmaStream);
        while (LL_DMA_IsEnabledStream(gpDmaReg[dmaEngine], dmaStream)) {}
        LL_USART_Disable(pUartReg);
        LL_USART_DeInit(pUartReg);

        // Remove the callback if there is one
        if (pUartData->eventQueueHandle >= 0) {
            uPortEventQueueClose(pUartData->eventQueueHandle);
        }
        if (pUartData->rxBufferIsMalloced) {
            // Free the buffer
            uPortFree(pUartData->pRxBufferStart);
        }
        // And finally remove the UART from the list
        removeUart(pUartData);
    }
}

// Deal with data already received by the DMA; this
// code is run in INTERRUPT CONTEXT.
static inline void dataIrqHandler(uPortUartData_t *pUartData,
                                  char *pRxBufferWriteDma)
{
    uPortUartEventData_t uartSizeOrError = 0;

    // Work out how much new data there is
    if (pUartData->pRxBufferWrite < pRxBufferWriteDma) {
        // The current write pointer is behind the DMA write pointer,
        // the number of bytes received is simply the difference
        uartSizeOrError = pRxBufferWriteDma - pUartData->pRxBufferWrite;
    } else if (pUartData->pRxBufferWrite > pRxBufferWriteDma) {
        // The current write pointer is ahead of the DMA
        // write pointer, the number of bytes received
        // is up to the end of the buffer then wrap
        // around to the DMA write pointer pointer
        uartSizeOrError = (pUartData->pRxBufferStart +
                           pUartData->rxBufferSizeBytes -
                           pUartData->pRxBufferWrite) +
                          (pRxBufferWriteDma - pUartData->pRxBufferStart);
    }

    // Move the write pointer on
    pUartData->pRxBufferWrite += uartSizeOrError;
    if (pUartData->pRxBufferWrite >= pUartData->pRxBufferStart +
        pUartData->rxBufferSizeBytes) {
        pUartData->pRxBufferWrite = pUartData->pRxBufferWrite -
                                    pUartData->rxBufferSizeBytes;
    }

    // Let the user know
    if ((uartSizeOrError > 0) && (pUartData->eventQueueHandle >= 0) &&
        (pUartData->eventFilter & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
        uPortUartEvent_t event;
        event.uartHandle = pUartData->uartHandle;
        event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
        uPortEventQueueSendIrq(pUartData->eventQueueHandle,
                               &event, sizeof(event));
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INTERRUPT HANDLERS
 * -------------------------------------------------------------- */

// DMA interrupt handler
void dmaIrqHandler(uint32_t dmaEngine, uint32_t dmaStream)
{
    DMA_TypeDef *const pDmaReg = gpDmaReg[dmaEngine];
    uPortUartData_t *pUartData = NULL;

    // Check half-transfer complete interrupt
    if (LL_DMA_IsEnabledIT_HT(pDmaReg, dmaStream) &&
        gpLlDmaIsActiveFlagHt[dmaStream](pDmaReg)) {
        // Clear the flag
        gpLlDmaClearFlagHt[dmaStream](pDmaReg);
        pUartData = gpDmaUart[dmaEngine][dmaStream];
    }

    // Check transfer complete interrupt
    if (LL_DMA_IsEnabledIT_TC(pDmaReg, dmaStream) &&
        gpLlDmaIsActiveFlagTc[dmaStream](pDmaReg)) {
        // Clear the flag
        gpLlDmaClearFlagTc[dmaStream](pDmaReg);
        pUartData = gpDmaUart[dmaEngine][dmaStream];
    }

    if (pUartData != NULL) {
        char *pRxBufferWriteDma;

        // Stuff has arrived: how much?
        // Get the new DMA pointer
        // LL_DMA_GetDataLength() returns a value in the sense
        // of "number of bytes left to be transmitted", so for
        // an Rx DMA we have to subtract the number from
        // the Rx buffer size
        pRxBufferWriteDma = pUartData->pRxBufferStart +
                            pUartData->rxBufferSizeBytes -
                            LL_DMA_GetDataLength(pDmaReg, dmaStream);
        // Deal with the data
        dataIrqHandler(pUartData, pRxBufferWriteDma);
    }
}

// UART interrupt handler
void uartIrqHandler(uPortUartData_t *pUartData)
{
    const uPortUartConstData_t *pUartCfg = pUartData->pConstData;
    USART_TypeDef *const pUartReg = pUartCfg->pReg;

    // Check for IDLE line interrupt
    if (LL_USART_IsEnabledIT_IDLE(pUartReg) &&
        LL_USART_IsActiveFlag_IDLE(pUartReg)) {
        char *pRxBufferWriteDma;

        // Clear flag
        LL_USART_ClearFlag_IDLE(pUartReg);

        // Get the new DMA pointer
        // LL_DMA_GetDataLength() returns a value in the sense
        // of "number of bytes left to be transmitted", so for
        // an Rx DMA we have to subtract the number from
        // the Rx buffer size
        pRxBufferWriteDma = pUartData->pRxBufferStart +
                            pUartData->rxBufferSizeBytes -
                            LL_DMA_GetDataLength(gpDmaReg[pUartCfg->dmaEngine],
                                                 pUartCfg->dmaStream);
        // Deal with the data
        dataIrqHandler(pUartData, pRxBufferWriteDma);
    }
}

#if U_CFG_HW_UART1_AVAILABLE
// USART 1 interrupt handler.
void USART1_IRQHandler()
{
    if (gpUart[1] != NULL) {
        uartIrqHandler(gpUart[1]);
    }
}
#endif

#if U_CFG_HW_UART2_AVAILABLE
// USART 2 interrupt handler.
void USART2_IRQHandler()
{
    if (gpUart[2] != NULL) {
        uartIrqHandler(gpUart[2]);
    }
}
#endif

#if U_CFG_HW_UART3_AVAILABLE
// USART 3 interrupt handler.
void USART3_IRQHandler()
{
    if (gpUart[3] != NULL) {
        uartIrqHandler(gpUart[3]);
    }
}
#endif

#if U_CFG_HW_UART4_AVAILABLE
// UART 4 interrupt handler.
void UART4_IRQHandler()
{
    if (gpUart[4] != NULL) {
        uartIrqHandler(gpUart[4]);
    }
}
#endif

#if U_CFG_HW_UART5_AVAILABLE
// UART 5 interrupt handler.
void UART5_IRQHandler()
{
    if (gpUart[5] != NULL) {
        uartIrqHandler(gpUart[5]);
    }
}
#endif

#if U_CFG_HW_UART6_AVAILABLE
// USART 6 interrupt handler.
void USART6_IRQHandler()
{
    if (gpUart[6] != NULL) {
        uartIrqHandler(gpUart[6]);
    }
}
#endif

#if U_CFG_HW_UART7_AVAILABLE
// UART 7 interrupt handler.
void UART7_IRQHandler()
{
    if (gpUart[7] != NULL) {
        uartIrqHandler(gpUart[7]);
    }
}
#endif

#if U_CFG_HW_UART8_AVAILABLE
// UART 8 interrupt handler.
void UART8_IRQHandler()
{
    if (gpUart[8] != NULL) {
        uartIrqHandler(gpUart[8]);
    }
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 0)
void DMA1_Stream0_IRQHandler()
{
    dmaIrqHandler(1, 0);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 1)
void DMA1_Stream1_IRQHandler()
{
    dmaIrqHandler(1, 1);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 2)
void DMA1_Stream2_IRQHandler()
{
    dmaIrqHandler(1, 2);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 3)
void DMA1_Stream3_IRQHandler()
{
    dmaIrqHandler(1, 3);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 4)
void DMA1_Stream4_IRQHandler()
{
    dmaIrqHandler(1, 4);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 5)
void DMA1_Stream5_IRQHandler()
{
    dmaIrqHandler(1, 5);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 6)
void DMA1_Stream6_IRQHandler()
{
    dmaIrqHandler(1, 6);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(1, 7)
void DMA1_Stream7_IRQHandler()
{
    dmaIrqHandler(1, 7);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 0)
void DMA2_Stream0_IRQHandler()
{
    dmaIrqHandler(2, 0);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 1)
void DMA2_Stream1_IRQHandler()
{
    dmaIrqHandler(2, 1);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 2)
void DMA2_Stream2_IRQHandler()
{
    dmaIrqHandler(2, 2);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 3)
void DMA2_Stream3_IRQHandler()
{
    dmaIrqHandler(2, 3);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 4)
void DMA2_Stream4_IRQHandler()
{
    dmaIrqHandler(2, 4);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 5)
void DMA2_Stream5_IRQHandler()
{
    dmaIrqHandler(2, 5);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 6)
void DMA2_Stream6_IRQHandler()
{
    dmaIrqHandler(2, 6);
}
#endif

#if U_PORT_DMA_INTERRUPT_IN_USE(2, 7)
void DMA2_Stream7_IRQHandler()
{
    dmaIrqHandler(2, 7);
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
    }

    return (int32_t) errorCode;
}

// Deinitialise the UART driver.
void uPortUartDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Close all the UART instances
        while (gpUartDataHead != NULL) {
            uartClose(gpUartDataHead->uartHandle);
        }

        // Finally delete the mutex
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open a UART instance.
int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t rxBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts)
{
    uErrorCode_t handleOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    ErrorStatus platformError;
    uPortUartData_t uartData = {0};
    LL_USART_InitTypeDef usartInitStruct = {0};
    LL_GPIO_InitTypeDef gpioInitStruct = {0};
    USART_TypeDef *pUartReg;
    uint32_t dmaEngine;
    DMA_TypeDef *pDmaReg;
    uint32_t dmaStream;
    uint32_t dmaChannel;
    IRQn_Type uartIrq;
    IRQn_Type dmaIrq;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart > 0) && (uart <= U_PORT_MAX_NUM_UARTS) &&
            (baudRate >= 0) && (rxBufferSizeBytes > 0) &&
            (pinRx >= 0) && (pinTx >= 0)) {
            if (pGetUartDataByUart(uart) == NULL) {
                handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
                uartData.uart = uart;
                uartData.rxBufferIsMalloced = false;
                uartData.pRxBufferStart = (char *) pReceiveBuffer;
                if (uartData.pRxBufferStart == NULL) {
                    // Malloc memory for the read buffer
                    uartData.pRxBufferStart = (char *) pUPortMalloc(rxBufferSizeBytes);
                    uartData.rxBufferIsMalloced = true;
                }
                if (uartData.pRxBufferStart != NULL) {
                    uartData.rxBufferSizeBytes = rxBufferSizeBytes;
                    uartData.pConstData = &(gUartCfg[uart]);
                    uartData.pRxBufferRead = uartData.pRxBufferStart;
                    uartData.pRxBufferWrite = uartData.pRxBufferStart;
                    uartData.ctsSuspended = false;
                    uartData.eventQueueHandle = -1;

                    pUartReg = gUartCfg[uart].pReg;
                    dmaEngine = gUartCfg[uart].dmaEngine;
                    pDmaReg = gpDmaReg[dmaEngine];
                    dmaStream = gUartCfg[uart].dmaStream;
                    dmaChannel = gUartCfg[uart].dmaChannel;
                    uartIrq = gUartCfg[uart].irq;
                    dmaIrq = gpDmaStreamIrq[dmaEngine][dmaStream];

                    // Now do the platform stuff
                    handleOrErrorCode = U_ERROR_COMMON_PLATFORM;

                    // Enable clock to the UART/USART HW block
                    gLlApbClkEnable[uart](gLlApbGrpPeriphUart[uart]);

                    // Enable clock to the DMA HW block (all DMAs
                    // are on bus 1)
                    LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphDma[dmaEngine]);

                    // Configure the GPIOs
                    // Note, using the LL driver rather than our
                    // driver or the HAL driver here partly because
                    // the example code does that and also
                    // because we need to enable the alternate
                    // function for these pins.

                    // Enable clock to the registers for the Tx/Rx pins
                    uPortPrivateGpioEnableClock(pinTx);
                    uPortPrivateGpioEnableClock(pinRx);
                    // The Pin field is a bitmap so we can do Tx and Rx
                    // at the same time as they are always on the same port
                    gpioInitStruct.Pin = (1U << U_PORT_STM32F4_GPIO_PIN(pinTx)) |
                                         (1U << U_PORT_STM32F4_GPIO_PIN(pinRx));
                    gpioInitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
                    gpioInitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
                    // Output type doesn't matter, it is overridden by
                    // the alternate function
                    gpioInitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
                    gpioInitStruct.Pull = LL_GPIO_PULL_UP;
                    gpioInitStruct.Alternate = gGpioAf[uart];
                    platformError = LL_GPIO_Init(pUPortPrivateGpioGetReg(pinTx),
                                                 &gpioInitStruct);

                    //  Configure RTS if present
                    if ((pinRts >= 0) && (platformError == SUCCESS)) {
                        uPortPrivateGpioEnableClock(pinRts);
                        gpioInitStruct.Pin = 1U << U_PORT_STM32F4_GPIO_PIN(pinRts);
                        platformError = LL_GPIO_Init(pUPortPrivateGpioGetReg(pinRts),
                                                     &gpioInitStruct);
                    }

                    //  Configure CTS if present
                    if ((pinCts >= 0) && (platformError == SUCCESS)) {
                        uPortPrivateGpioEnableClock(pinCts);
                        gpioInitStruct.Pin = 1U << U_PORT_STM32F4_GPIO_PIN(pinCts);
                        // The u-blox C030-R412M board requires a pull-down here
                        gpioInitStruct.Pull = LL_GPIO_PULL_DOWN;
                        platformError = LL_GPIO_Init(pUPortPrivateGpioGetReg(pinCts),
                                                     &gpioInitStruct);
                    }

                    // Configure DMA
                    if (platformError == SUCCESS) {
                        // Set the channel on our DMA/Stream
                        LL_DMA_SetChannelSelection(pDmaReg, dmaStream,
                                                   gLlDmaChannel[dmaChannel]);
                        // Towards RAM
                        LL_DMA_SetDataTransferDirection(pDmaReg, dmaStream,
                                                        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
                        // Low priority
                        LL_DMA_SetStreamPriorityLevel(pDmaReg, dmaStream,
                                                      LL_DMA_PRIORITY_LOW);
                        // Circular
                        LL_DMA_SetMode(pDmaReg, dmaStream, LL_DMA_MODE_CIRCULAR);
                        // Byte-wise transfers from a fixed
                        // register in a peripheral to an
                        // incrementing location in memory
                        LL_DMA_SetPeriphIncMode(pDmaReg, dmaStream,
                                                LL_DMA_PERIPH_NOINCREMENT);
                        LL_DMA_SetMemoryIncMode(pDmaReg, dmaStream,
                                                LL_DMA_MEMORY_INCREMENT);
                        LL_DMA_SetPeriphSize(pDmaReg, dmaStream,
                                             LL_DMA_PDATAALIGN_BYTE);
                        LL_DMA_SetMemorySize(pDmaReg, dmaStream,
                                             LL_DMA_MDATAALIGN_BYTE);
                        // Not FIFO mode, whatever that is
                        LL_DMA_DisableFifoMode(pDmaReg, dmaStream);

                        // Attach the DMA to the UART at one end
                        LL_DMA_SetPeriphAddress(pDmaReg, dmaStream,
                                                (uint32_t) & (pUartReg->DR));

                        // ...and to the RAM buffer at the other end
                        LL_DMA_SetMemoryAddress(pDmaReg, dmaStream,
                                                (uint32_t) (uartData.pRxBufferStart));
                        LL_DMA_SetDataLength(pDmaReg, dmaStream,
                                             uartData.rxBufferSizeBytes);

                        // Clear all the DMA flags and the DMA pending IRQ from any previous
                        // session first, or an unexpected interrupt may result
                        gpLlDmaClearFlagHt[dmaStream](pDmaReg);
                        gpLlDmaClearFlagTc[dmaStream](pDmaReg);
                        gpLlDmaClearFlagTe[dmaStream](pDmaReg);
                        gpLlDmaClearFlagDme[dmaStream](pDmaReg);
                        gpLlDmaClearFlagFe[dmaStream](pDmaReg);
                        NVIC_ClearPendingIRQ(dmaIrq);

                        // Enable half full and transmit complete DMA interrupts
                        LL_DMA_EnableIT_HT(pDmaReg, dmaStream);
                        LL_DMA_EnableIT_TC(pDmaReg, dmaStream);

                        // Set DMA priority
                        NVIC_SetPriority(dmaIrq,
                                         NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));

                        // Go!
                        NVIC_EnableIRQ(dmaIrq);

                        // Initialise the UART/USART
                        usartInitStruct.BaudRate = baudRate;
                        usartInitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
                        usartInitStruct.StopBits = LL_USART_STOPBITS_1;
                        usartInitStruct.Parity = LL_USART_PARITY_NONE;
                        // Both transmit and received enabled
                        usartInitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
                        usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
                        if ((pinRts >= 0) || (pinCts >= 0)) {
                            if ((pinRts >= 0) && (pinCts >= 0)) {
                                usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
                            } else {
                                if (pinRts >= 0) {
                                    usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS;
                                } else {
                                    usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_CTS;
                                }
                            }
                        }
                        usartInitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
                        platformError = LL_USART_Init(pUartReg, &usartInitStruct);
                    }

                    // Connect it all together
                    if (platformError == SUCCESS) {
                        // Asynchronous UART/USART with DMA on the receive
                        // and include only the idle line interrupt,
                        // DMA does the rest
                        LL_USART_ConfigAsyncMode(pUartReg);
                        LL_USART_EnableDMAReq_RX(pUartReg);
                        LL_USART_EnableIT_IDLE(pUartReg);

                        // Enable the UART/USART interrupt
                        NVIC_SetPriority(uartIrq,
                                         NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                                             5, 1));
                        LL_USART_ClearFlag_IDLE(pUartReg);
                        NVIC_ClearPendingIRQ(uartIrq);
                        NVIC_EnableIRQ(uartIrq);

                        // Enable DMA and UART/USART
                        LL_DMA_EnableStream(pDmaReg, dmaStream);
                        LL_USART_Enable(pUartReg);
                    }

                    // Finally, add the UART to the list
                    if (platformError == SUCCESS) {
                        handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
                        uartData.uartHandle = nextHandleGet();
                        if (pAddUart(&uartData) != NULL) {
                            handleOrErrorCode = uartData.uartHandle;
                        }
                    }
                }

                // If we failed, clean up
                if (handleOrErrorCode < 0) {
                    if (uartData.rxBufferIsMalloced) {
                        uPortFree(uartData.pRxBufferStart);
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
    uPortUartData_t *pUartData;
    const volatile char *pRxBufferWrite;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            pRxBufferWrite = pUartData->pRxBufferWrite;
            sizeOrErrorCode = 0;
            if (pUartData->pRxBufferRead < pRxBufferWrite) {
                // Read pointer is behind write, bytes
                // received is simply the difference
                sizeOrErrorCode = pRxBufferWrite - pUartData->pRxBufferRead;
            } else if (pUartData->pRxBufferRead > pRxBufferWrite) {
                // Read pointer is ahead of write, bytes received
                // is from the read pointer up to the end of the buffer
                // then wrap around to the write pointer
                sizeOrErrorCode = (pUartData->pRxBufferStart +
                                   pUartData->rxBufferSizeBytes -
                                   pUartData->pRxBufferRead) +
                                  (pRxBufferWrite - pUartData->pRxBufferStart);
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
    uint8_t *pDataPtr = pBuffer;
    uErrorCode_t sizeOrErrorCode = U_ERROR_COMMON_NOT_INITIALISED;
    size_t thisSize;
    uPortUartData_t *pUartData;
    const volatile char *pRxBufferWrite;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            pRxBufferWrite = pUartData->pRxBufferWrite;
            if (pUartData->pRxBufferRead < pRxBufferWrite) {
                // Read pointer is behind write, just take as much
                // of the difference as the user allows
                sizeOrErrorCode = pRxBufferWrite - pUartData->pRxBufferRead;
                if (sizeOrErrorCode > sizeBytes) {
                    sizeOrErrorCode = sizeBytes;
                }
                memcpy(pDataPtr, pUartData->pRxBufferRead,
                       sizeOrErrorCode);
                // Move the pointer on
                pUartData->pRxBufferRead += sizeOrErrorCode;
            } else if (pUartData->pRxBufferRead > pRxBufferWrite) {
                // Read pointer is ahead of write, first take up to the
                // end of the buffer as far as the user allows
                thisSize = pUartData->pRxBufferStart +
                           pUartData->rxBufferSizeBytes -
                           pUartData->pRxBufferRead;
                if (thisSize > sizeBytes) {
                    thisSize = sizeBytes;
                }
                memcpy(pDataPtr, pUartData->pRxBufferRead, thisSize);
                pDataPtr = pDataPtr + thisSize;
                sizeBytes -= thisSize;
                sizeOrErrorCode = thisSize;
                // Move the read pointer on, wrapping as necessary
                pUartData->pRxBufferRead += thisSize;
                if (pUartData->pRxBufferRead >= pUartData->pRxBufferStart +
                    pUartData->rxBufferSizeBytes) {
                    pUartData->pRxBufferRead = pUartData->pRxBufferStart;
                }
                // If there is still room in the user buffer then
                // carry on taking up to the write pointer
                if (sizeBytes > 0) {
                    thisSize = pRxBufferWrite - pUartData->pRxBufferRead;
                    if (thisSize > sizeBytes) {
                        thisSize = sizeBytes;
                    }
                    memcpy(pDataPtr, pUartData->pRxBufferRead, thisSize);
                    sizeOrErrorCode += thisSize;
                    // Move the read pointer on
                    pUartData->pRxBufferRead += thisSize;
                }
            } else {
                sizeOrErrorCode = 0;
            }

        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t handle,
                       const void *pBuffer,
                       size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const uint8_t *pDataPtr = pBuffer;
    uPortUartData_t *pUartData;
    USART_TypeDef *pReg;
    bool txOk = true;
    int32_t startTimeMs;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            pReg = gUartCfg[pUartData->uart].pReg;
            // Do the blocking send
            sizeOrErrorCode = (int32_t) sizeBytes;
            startTimeMs = uPortGetTickTimeMs();
            while ((sizeBytes > 0) && (txOk)) {
                LL_USART_TransmitData8(pReg, *pDataPtr);
                // Hint when debugging: if your code stops dead here
                // it is because the CTS line of this MCU's UART HW
                // is floating high, stopping the UART from
                // transmitting once its buffer is full: either
                // the thing at the other end doesn't want data sent to
                // it or the CTS pin when configuring this UART
                // was wrong and it's not connected to the right
                // thing.
                while (!(txOk = LL_USART_IsActiveFlag_TXE(pReg)) &&
                       (uPortGetTickTimeMs() - startTimeMs < U_PORT_UART_WRITE_TIMEOUT_MS)) {}
                if (txOk) {
                    pDataPtr++;
                    sizeBytes--;
                }
            }
            // Wait for transmission to complete so that we don't
            // write over stuff the next time
            while (!LL_USART_IsActiveFlag_TC(pReg) &&
                   (uPortGetTickTimeMs() - startTimeMs < U_PORT_UART_WRITE_TIMEOUT_MS)) {}
            sizeOrErrorCode -= (int32_t) sizeBytes;
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
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    char name[16];

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) && (pUartData->eventQueueHandle < 0) &&
            (filter != 0) && (pFunction != NULL)) {
            // Open an event queue to eventHandler()
            // which will receive uPortUartEvent_t
            // and give it a useful name for debug purposes
            snprintf(name, sizeof(name), "eventUart_%d", (int) pUartData->uart);
            errorCode = uPortEventQueueOpen(eventHandler, name,
                                            sizeof(uPortUartEvent_t),
                                            stackSizeBytes,
                                            priority,
                                            U_PORT_UART_EVENT_QUEUE_SIZE);
            if (errorCode >= 0) {
                pUartData->eventQueueHandle = errorCode;
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

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    uPortUartData_t *pUartData;
    int32_t eventQueueHandle = -1;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) &&
            (pUartData->eventQueueHandle >= 0)) {
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
        // blocked by us we'll get stuck.
        if (eventQueueHandle >= 0) {
            uPortEventQueueClose(eventQueueHandle);
        }
    }
}

// Get the callback filter bit-mask.
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            if (pUartData->eventQueueHandle >= 0) {
                filter = pUartData->eventFilter;
            }
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) && (filter != 0) &&
            (pUartData->eventQueueHandle >= 0)) {
            pUartData->eventFilter = filter;
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
    uPortUartData_t *pUartData;
    uPortUartEvent_t event;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) &&
            (pUartData->eventQueueHandle >= 0) &&
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

// Send an event to the callback, but only if there's room on the queue.
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    uPortUartEvent_t event;
    int64_t startTime = uPortGetTickTimeMs();

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) &&
            (pUartData->eventQueueHandle >= 0) &&
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
                     (uPortGetTickTimeMs() - startTime < delayMs));
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Return true if we're in an event callback.
bool uPortUartEventIsCallback(int32_t handle)
{
    uPortUartData_t *pUartData;
    bool isEventCallback = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) &&
            (pUartData->eventQueueHandle >= 0)) {
            isEventCallback = uPortEventQueueIsTask(pUartData->eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventCallback;
}

// Get the stack high watermark for the task on the event queue.
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) &&
            (pUartData->eventQueueHandle >= 0)) {
            sizeOrErrorCode = uPortEventQueueStackMinFree(pUartData->eventQueueHandle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    uPortUartData_t *pUartData;
    bool rtsFlowControlIsEnabled = false;
    uint32_t flowControlStatus;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            flowControlStatus = LL_USART_GetHWFlowCtrl(gUartCfg[pUartData->uart].pReg);
            if ((flowControlStatus == LL_USART_HWCONTROL_RTS) ||
                (flowControlStatus == LL_USART_HWCONTROL_RTS_CTS)) {
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
    uPortUartData_t *pUartData;
    bool ctsFlowControlIsEnabled = false;
    uint32_t flowControlStatus;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            flowControlStatus = LL_USART_GetHWFlowCtrl(gUartCfg[pUartData->uart].pReg);
            if ((flowControlStatus == LL_USART_HWCONTROL_CTS) ||
                (flowControlStatus == LL_USART_HWCONTROL_RTS_CTS)) {
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
    uint32_t flowControlStatus;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pGetUartDataByHandle(handle);
        if (pUartData != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (!pUartData->ctsSuspended) {
                flowControlStatus = LL_USART_GetHWFlowCtrl(gUartCfg[pUartData->uart].pReg);
                if ((flowControlStatus == LL_USART_HWCONTROL_CTS) ||
                    (flowControlStatus == LL_USART_HWCONTROL_RTS_CTS)) {
                    LL_USART_DisableCTSHWFlowCtrl(gUartCfg[pUartData->uart].pReg);
                    pUartData->ctsSuspended = true;
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pGetUartDataByHandle(handle);
        if ((pUartData != NULL) && (pUartData->ctsSuspended)) {
            LL_USART_EnableCTSHWFlowCtrl(gUartCfg[pUartData->uart].pReg);
            pUartData->ctsSuspended = false;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// End of file
