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
 * @brief Stuff private to the STM32 porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy()

#include "u_cfg_os_platform_specific.h"
#include "u_cfg_hw_platform_specific.h" // For U_CFG_HW_SWO_CLOCK_HZ
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_event_queue.h"

#ifdef U_PORT_STM32_PURE_CMSIS
# include "cmsis_os2.h"
#endif

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
# include "tx_api.h"
# include "tx_thread.h"
#else
# include "FreeRTOS.h"
# include "timers.h"
#endif

#ifdef STM32U575xx
# include "stm32u5xx_hal.h"
# include "stm32u5xx_ll_bus.h"
#else
# include "stm32f4xx_hal.h"
# include "stm32f4xx_ll_bus.h"
#endif

#include "u_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
/** Convert an RTOS tick to milliseconds, ThreadX case.
 */
# define TICKS_TO_MS(ticks)  (ticks * 1000 / TX_TIMER_TICKS_PER_SECOND)
#else
/** Convert an RTOS tick to milliseconds, FreeRTOS case.
 */
# define TICKS_TO_MS(ticks)  (ticks * 1000 / configTICK_RATE_HZ)
#endif

/** Address of The ITM Enable register.
 */
#define ITM_ENA   (*(volatile uint32_t *) 0xE0000E00)

/** Address of the ITM Trace Privilege register.
 */
#define ITM_TPR   (*(volatile uint32_t *) 0xE0000E40)

/** Address of the ITM Trace Control register.
 */
#define ITM_TCR   (*(volatile uint32_t *) 0xE0000E80)

/** Address of the ITM Lock Status register.
 */
#define ITM_LSR   (*(volatile uint32_t *) 0xE0000FB0)

/** Address of the Debug register.
 */
#define DHCSR     (*(volatile uint32_t *) 0xE000EDF0)

/** Address of another Debug register.
 */
#define DEMCR     (*(volatile uint32_t *) 0xE000EDFC)

/** Address of the Trace Unit Async Clock prescaler register.
 */
#define TPIU_ACPR (*(volatile uint32_t *) 0xE0040010)

/** Address of the Trace Unit Selected Pin Protocol register.
 */
#define TPIU_SPPR (*(volatile uint32_t *) 0xE00400F0)

/** Address of the DWT Control register.
 */
#define DWT_CTRL  (*(volatile uint32_t *) 0xE0001000)

/** Address of the Formattr And Flush Control register.
 */
#define FFCR      (*(volatile uint32_t *) 0xE0040304)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Define a timer, intended to be used as part of a linked-list.
 */
typedef struct uPortPrivateTimer_t {
    uPortTimerHandle_t handle;
    char name[U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES];
    pTimerCallback_t *pCallback;
    void *pCallbackParam;
#ifdef U_PORT_STM32_PURE_CMSIS
    uint32_t intervalMs;
#endif
    struct uPortPrivateTimer_t *pNext;
} uPortPrivateTimer_t;

/** Define a semaphore, intended to be used as part of a linked-list.
 */
typedef struct uPortPrivateSemaphore_t {
    uPortSemaphoreHandle_t handle;
    uint32_t limit;
    uint32_t count;
    struct uPortPrivateSemaphore_t *pNext;
} uPortPrivateSemaphore_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Counter to keep track of RTOS ticks: NOT static
// so that the u_exception_handler.c can update it.
int32_t gTickTimerRtosCount;

// Get the GPIOx address for a given GPIO port.
static GPIO_TypeDef *const gpGpioReg[] = {GPIOA,
                                          GPIOB,
                                          GPIOC,
                                          GPIOD,
                                          GPIOE,
                                          GPIOF,
                                          GPIOG,
                                          GPIOH,
                                          GPIOI,
#ifdef GPIOJ
                                          GPIOJ,
#endif
#ifdef GPIOK
                                          GPIOK,
#endif
                                         };

#ifdef STM32U575xx
// Get the LL driver peripheral number for a given GPIO port,
// all on AHB2 for STM32U5
static const int32_t gLlApbGrpPeriphGpioPort[] = {LL_AHB2_GRP1_PERIPH_GPIOA,
                                                  LL_AHB2_GRP1_PERIPH_GPIOB,
                                                  LL_AHB2_GRP1_PERIPH_GPIOC,
                                                  LL_AHB2_GRP1_PERIPH_GPIOD,
                                                  LL_AHB2_GRP1_PERIPH_GPIOE,
                                                  LL_AHB2_GRP1_PERIPH_GPIOF,
                                                  LL_AHB2_GRP1_PERIPH_GPIOG,
                                                  LL_AHB2_GRP1_PERIPH_GPIOH,
                                                  LL_AHB2_GRP1_PERIPH_GPIOI,
#ifdef GPIOJ
                                                  LL_AHB2_GRP1_PERIPH_GPIOJ,
#endif
#ifdef GPIOK
                                                  LL_AHB2_GRP1_PERIPH_GPIOK,
#endif
                                                 };
#else
// Get the LL driver peripheral number for a given GPIO port,
// all on AHB1 for STM32F4.
static const int32_t gLlApbGrpPeriphGpioPort[] = {LL_AHB1_GRP1_PERIPH_GPIOA,
                                                  LL_AHB1_GRP1_PERIPH_GPIOB,
                                                  LL_AHB1_GRP1_PERIPH_GPIOC,
                                                  LL_AHB1_GRP1_PERIPH_GPIOD,
                                                  LL_AHB1_GRP1_PERIPH_GPIOE,
                                                  LL_AHB1_GRP1_PERIPH_GPIOF,
                                                  LL_AHB1_GRP1_PERIPH_GPIOG,
                                                  LL_AHB1_GRP1_PERIPH_GPIOH,
                                                  LL_AHB1_GRP1_PERIPH_GPIOI,
#ifdef GPIOJ
                                                  LL_AHB1_GRP1_PERIPH_GPIOJ,
#endif
#ifdef GPIOK
                                                  LL_AHB1_GRP1_PERIPH_GPIOK,
#endif
                                                 };
#endif

/** Root of the linked list of timers.
 */
static uPortPrivateTimer_t *gpTimerList = NULL;

/** Mutex to protect the linked list of timers.
 */
static uPortMutexHandle_t gMutexForTimers = NULL;

/** Use an event queue to move the execution of the timer callback
 * outside of the FreeRTOS/CMSIS timer task.
 */
static int32_t gEventQueueHandle = -1;

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
/** A place to store the current pre-emption threshold of a CMSIS
 * task, used as part of the implementation of critical sections
 * when running CMSIS over ThreadX (since ThreadX does not support
 * stopping the scheduler).
 */
static int32_t gSavedPremptionThreshold = -1;

/** A place to store the current priority of a CMSIS task, used
 * as part of the implementation of critical sections when running
 * CMSIS over ThreadX (since ThreadX does not support stopping the
 * scheduler).
 */
static int32_t gSavedPriority = -1;
#endif

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
/** Root of the linked list of semaphores.
 */
static uPortPrivateSemaphore_t *gpSemaphoreList = NULL;

/** Mutex to protect the list of semaphores.
 */
static uPortMutexHandle_t gMutexForSemaphores = NULL;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** This code from https://wiki.segger.com/J-Link_SWO_Viewer.
 * It enables SWO so that logging continues if the target resets
 * without the external debug tool being aware.
 * This can be switched off by overriding U_CFG_HW_SWO_CLOCK_HZ to
 * -1, in which case the external debug tool will set it up instead.
 */
static void enableSwo()
{
#if U_CFG_HW_SWO_CLOCK_HZ >= 0

    uint32_t stimulusRegs;

    // Enable access to SWO registers
    DEMCR |= (1 << 24);
    ITM_LSR = 0xC5ACCE55;

    // Initially disable ITM and stimulus port
    // To make sure that nothing is transferred via SWO
    // when changing the SWO prescaler etc.
    stimulusRegs = ITM_ENA;
    stimulusRegs &= ~(1 << 0); // Disable stimulus port 0
    ITM_ENA = stimulusRegs;
    ITM_TCR = 0; // Disable ITM

    // Initialize SWO (prescaler, etc.)
#ifndef STM32U575xx
    TPIU_SPPR = 0x00000002; // Select NRZ mode
    TPIU_ACPR = (SystemCoreClock / U_CFG_HW_SWO_CLOCK_HZ) - 1;
#endif
    ITM_TPR = 0x00000000;
    DWT_CTRL = 0x400003FE;
#ifndef STM32U575xx
    FFCR = 0x00000100;
#endif

    // Enable ITM and stimulus port
    ITM_TCR = 0x1000D; // Enable ITM
    ITM_ENA = stimulusRegs | (1 << 0); // Enable stimulus port 0
#endif
}

// Find a timer entry in the list.
// gMutexForTimers should be locked before this is called.
static uPortPrivateTimer_t *pTimerFind(uPortTimerHandle_t handle)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;

    while ((pTimer != NULL) && (pTimer->handle != handle)) {
        pTimer = pTimer->pNext;
    }

    return pTimer;
}

// Remove an entry from the list.
// gMutexForTimers should be locked before this is called.
static void timerRemove(uPortTimerHandle_t handle)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;
    uPortPrivateTimer_t *pPrevious = NULL;

    // Find the entry in the list
    while ((pTimer != NULL) && (pTimer->handle != handle)) {
        pPrevious = pTimer;
        pTimer = pTimer->pNext;
    }
    if (pTimer != NULL) {
        // Remove the entry from the list
        if (pPrevious != NULL) {
            pPrevious->pNext = pTimer->pNext;
        } else {
            // Must be at head
            gpTimerList = pTimer->pNext;
        }
        // Free the entry
        uPortFree(pTimer);
    }
}

// The timer event handler, where pParam is a pointer
// to the timer handle.
static void timerEventHandler(void *pParam, size_t paramLength)
{
    uPortTimerHandle_t handle = *((uPortTimerHandle_t *) pParam);
    uPortPrivateTimer_t *pTimer;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;

    (void) paramLength;

    if (gMutexForTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexForTimers);

        pTimer = pTimerFind(handle);
        if (pTimer != NULL) {
            pCallback = pTimer->pCallback;
            pCallbackParam = pTimer->pCallbackParam;
        }

        U_PORT_MUTEX_UNLOCK(gMutexForTimers);

        // Call the callback outside the locks so that the
        // callback itself may call the timer API
        if (pCallback != NULL) {
            pCallback(handle, pCallbackParam);
        }
    }
}

// The timer expiry callback, called by FreeRTOS.
static void timerCallback(uPortTimerHandle_t handle)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    // In the pure CMSIS case we get a pointer to a pTimer
    // structure rather than a handle so we need to convert it
    uPortPrivateTimer_t *pTimer = (uPortPrivateTimer_t *) handle;
    if (pTimer != NULL) {
        handle = pTimer->handle;
    }
#endif

    if (gEventQueueHandle >= 0) {
        // Send an event to our event task with the timer
        // handle as the payload, IRQ version so as never
        // to block
        uPortEventQueueSendIrq(gEventQueueHandle,
                               // NOLINTNEXTLINE(bugprone-sizeof-expression)
                               &handle, sizeof(handle));
    }
}

#ifdef U_PORT_STM32_PURE_CMSIS
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
// Get a semaphore from the list; only required for the ThreadX
// under CMSIS case.
// Note: gMutexForSemaphores must be locked before this is called
static uPortPrivateSemaphore_t *pSemaphoreFind(uPortSemaphoreHandle_t handle)
{
    uPortPrivateSemaphore_t *pTmp = gpSemaphoreList;
    uPortPrivateSemaphore_t *pSemaphore = NULL;

    while ((pTmp != NULL) && (pSemaphore == NULL)) {
        if (pTmp->handle == handle) {
            pSemaphore = pTmp;
        }
        pTmp = pTmp->pNext;
    }

    return pSemaphore;
}
# endif

// Increment the count of a semaphore, returning
// true if incrementing is allowed.
static bool semaphoreInc(uPortSemaphoreHandle_t handle)
{
    // Incrementing is allowed if we do not
    // have a stored record for this semaphore
    // (the FreeRTOS case)
    bool allowed = true;
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
    uPortPrivateSemaphore_t *pSemaphore;

    U_PORT_MUTEX_LOCK(gMutexForSemaphores);

    pSemaphore = pSemaphoreFind(handle);
    if (pSemaphore != NULL) {
        if (pSemaphore->count < pSemaphore->limit) {
            pSemaphore->count++;
        } else {
            allowed = false;
        }
    }

    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
# else
    (void) handle;
# endif
    return allowed;
}

// Decrement the count of a semaphore.
static void semaphoreDec(uPortSemaphoreHandle_t handle)
{
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
    uPortPrivateSemaphore_t *pSemaphore;

    U_PORT_MUTEX_LOCK(gMutexForSemaphores);

    pSemaphore = pSemaphoreFind(handle);
    if ((pSemaphore != NULL) && (pSemaphore->count > 0)) {
        pSemaphore->count--;
    }

    U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
# else
    (void) handle;
# endif
}
#endif // #ifdef U_PORT_STM32_PURE_CMSIS

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: INIT
 * -------------------------------------------------------------- */

// Initalise the private stuff.
int32_t uPortPrivateInit()
{
    int32_t errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexForTimers == NULL) {
        errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutexForTimers);
    }
#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
    if ((errorCodeOrEventQueueHandle == 0) && (gMutexForSemaphores == NULL)) {
        errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutexForSemaphores);
    }
#endif
    if ((errorCodeOrEventQueueHandle == 0) && (gEventQueueHandle < 0)) {
        // We need an event queue to offload the callback execution
        // from the FreeRTOS/CMSIS timer task
        errorCodeOrEventQueueHandle = uPortEventQueueOpen(timerEventHandler, "timerEvent",
#ifdef U_PORT_STM32_PURE_CMSIS
                                                          sizeof(osTimerId_t),
#else
                                                          sizeof(TimerHandle_t),
#endif
                                                          U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES,
                                                          U_CFG_OS_TIMER_EVENT_TASK_PRIORITY,
                                                          U_CFG_OS_TIMER_EVENT_QUEUE_SIZE);
        gTickTimerRtosCount = 0;
        enableSwo();
    }
    if (errorCodeOrEventQueueHandle >= 0) {
        gEventQueueHandle = errorCodeOrEventQueueHandle;
        errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;
    } else {
        // Clean-up on error
#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
        if (gMutexForSemaphores != NULL) {
            uPortMutexDelete(gMutexForSemaphores);
        }
#endif
        if (gMutexForTimers != NULL) {
            uPortMutexDelete(gMutexForTimers);
        }
    }

    return errorCodeOrEventQueueHandle;
}

// Deinitialise the private stuff.
void uPortPrivateDeinit()
{
    if (gMutexForTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexForTimers);

        // Tidy away the timers
        while (gpTimerList != NULL) {
#ifdef U_PORT_STM32_PURE_CMSIS
            osTimerStop((osTimerId_t) gpTimerList->handle);
#else
            xTimerStop((TimerHandle_t) gpTimerList->handle,
                       (portTickType) portMAX_DELAY);
#endif
            timerRemove(gpTimerList->handle);
        }

        U_PORT_MUTEX_UNLOCK(gMutexForTimers);

        // Close the event queue outside the mutex as it could be calling
        // back into this API
        if (gEventQueueHandle >= 0) {
            uPortEventQueueClose(gEventQueueHandle);
            gEventQueueHandle = -1;
        }

        uPortMutexDelete(gMutexForTimers);
        gMutexForTimers = NULL;
    }

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
    // Tidy away any semaphores
    if (gMutexForSemaphores != NULL) {
        uPortPrivateSemaphore_t *pTmp = NULL;

        U_PORT_MUTEX_LOCK(gMutexForSemaphores);

        while (gpSemaphoreList != NULL) {
            osSemaphoreDelete((osSemaphoreId_t) gpSemaphoreList->handle);
            pTmp = gpSemaphoreList;
            uPortFree(gpSemaphoreList);
            gpSemaphoreList = pTmp->pNext;
        }

        U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);

        uPortMutexDelete(gMutexForSemaphores);
        gMutexForSemaphores = NULL;
    }
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: TIMERS
 * -------------------------------------------------------------- */

// Add a timer entry to the list.
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                const char *pName,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;
    const char *_pName = NULL;

    if (gMutexForTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexForTimers);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pHandle != NULL) {
            // Create an entry in the list
            pTimer = (uPortPrivateTimer_t *) pUPortMalloc(sizeof(uPortPrivateTimer_t));
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if (pTimer != NULL) {
                // Populate the entry
                pTimer->name[0] = 0;
                if (pName != NULL) {
                    strncpy(pTimer->name, pName, sizeof(pTimer->name));
                    // Ensure terminator
                    pTimer->name[sizeof(pTimer->name) - 1] = 0;
                    _pName = pTimer->name;
                }
                pTimer->pCallback = pCallback;
                pTimer->pCallbackParam = pCallbackParam;
#ifdef U_PORT_STM32_PURE_CMSIS
                pTimer->intervalMs = intervalMs;
                osTimerAttr_t attr = {0};
                if (pName != NULL) {
                    attr.name = _pName;
                }
                pTimer->handle = (uPortTimerHandle_t) osTimerNew(timerCallback,
                                                                 periodic ? osTimerPeriodic : osTimerOnce,
                                                                 pTimer, &attr);
#else
                pTimer->handle = (uPortTimerHandle_t) xTimerCreate(_pName,
                                                                   MS_TO_TICKS(intervalMs),
                                                                   periodic ? pdTRUE : pdFALSE,
                                                                   NULL,
                                                                   (void (*)(struct tmrTimerControl *)) timerCallback);
#endif
                if (pTimer->handle != NULL) {
                    // Add the timer to the front of the list
                    pTimer->pNext = gpTimerList;
                    gpTimerList = pTimer;
                    *pHandle = pTimer->handle;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Tidy up if the timer could not be created
                    uPortFree(pTimer);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexForTimers);
    }

    return errorCode;
}

// Start a CMSIS-based timer.
int32_t uPortPrivateTimerStartCmsis(const uPortTimerHandle_t handle)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexForTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexForTimers);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pTimer = pTimerFind((uPortTimerHandle_t) handle);
        if (pTimer != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            if (osTimerStart((osTimerId_t) handle, MS_TO_TICKS(pTimer->intervalMs)) == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexForTimers);
    }

    return errorCode;
#else
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Change a CMSIS-based timer.
int32_t uPortPrivateTimerChangeCmsis(const uPortTimerHandle_t handle,
                                     uint32_t intervalMs)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexForTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexForTimers);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pTimer = pTimerFind((uPortTimerHandle_t) handle);
        if (pTimer != NULL) {
            pTimer->intervalMs = intervalMs;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutexForTimers);
    }

    return errorCode;
#else
    (void) handle;
    (void) intervalMs;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Delete a timer entry from the list.
int32_t uPortPrivateTimerDelete(uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutexForTimers != NULL) {

        // Delete the timer in the RTOS, outside the mutex as it can block
        errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
#ifdef U_PORT_STM32_PURE_CMSIS
        if (osTimerDelete((osTimerId_t) handle) == 0) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
#else
        if (xTimerDelete((TimerHandle_t) handle,
                         (portTickType) portMAX_DELAY) == pdPASS) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
#endif

        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {

            U_PORT_MUTEX_LOCK(gMutexForTimers);

            timerRemove(handle);

            U_PORT_MUTEX_UNLOCK(gMutexForTimers);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: GET TIME TICK
 * -------------------------------------------------------------- */

// Get the current tick converted to a time in milliseconds.
int64_t uPortPrivateGetTickTimeMs()
{
    return TICKS_TO_MS(gTickTimerRtosCount);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: SEMAPHORES FOR CMSIS
 * -------------------------------------------------------------- */

// Create a semaphore, CMSIS case.
int32_t uPortPrivateSemaphoreCreateCmsis(uPortSemaphoreHandle_t *pSemaphoreHandle,
                                         uint32_t initialCount,
                                         uint32_t limit)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    // Note: the CMSIS API takes two parameters: max_count and initial_count,
    // where initial_count is the number of tokens available at the outset
    // and max_count is what it sounds like.
    // The ThreadX API takes only initial_count.
    // Our API has "limit" for max_count and initialCount as the number of
    // tokens available at the outset.
    if ((pSemaphoreHandle != NULL) && (limit != 0) && (initialCount <= limit)) {
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
        // ThreadX case
        uPortPrivateSemaphore_t *pSemaphore;
        errorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gMutexForSemaphores != NULL) {

            U_PORT_MUTEX_LOCK(gMutexForSemaphores);

            errorCode = U_ERROR_COMMON_NO_MEMORY;
            pSemaphore = (uPortPrivateSemaphore_t *) pUPortMalloc(sizeof(*pSemaphore));
            if (pSemaphore != NULL) {
                errorCode = U_ERROR_COMMON_PLATFORM;
                pSemaphore->limit = limit;
                pSemaphore->count = initialCount;
                *pSemaphoreHandle = (uPortSemaphoreHandle_t) osSemaphoreNew(limit,
                                                                            initialCount,
                                                                            NULL);
                if (*pSemaphoreHandle != NULL) {
                    pSemaphore->handle = *pSemaphoreHandle;
                    // Add to the front of the list
                    pSemaphore->pNext = gpSemaphoreList;
                    gpSemaphoreList = pSemaphore;
                    errorCode = U_ERROR_COMMON_SUCCESS;
                } else {
                    // Clean-up on error
                    uPortFree(pSemaphore);
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
        }
# else
        // Normal CMSIS case
        errorCode = U_ERROR_COMMON_PLATFORM;
        *pSemaphoreHandle = (uPortSemaphoreHandle_t) osSemaphoreNew(limit,
                                                                    initialCount,
                                                                    NULL);
# endif
        if (*pSemaphoreHandle != NULL) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
#else
    (void) pSemaphoreHandle;
    (void) initialCount;
    (void) limit;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Destroy a semaphore, CMSIS case.
int32_t uPortPrivateSemaphoreDeleteCmsis(const uPortSemaphoreHandle_t semaphoreHandle)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
        // ThreadX case
        uPortPrivateSemaphore_t *pSemaphore = gpSemaphoreList;
        uPortPrivateSemaphore_t *pPrevious = NULL;
        errorCode = U_ERROR_COMMON_NOT_INITIALISED;
        if (gMutexForSemaphores != NULL) {

            U_PORT_MUTEX_LOCK(gMutexForSemaphores);

            errorCode = U_ERROR_COMMON_SUCCESS;
            osSemaphoreDelete((osSemaphoreId_t) semaphoreHandle);
            // Remove it from the list
            while (pSemaphore != NULL) {
                if (pSemaphore->handle == semaphoreHandle) {
                    if (pPrevious == NULL) {
                        // At head of list
                        gpSemaphoreList = pSemaphore->pNext;
                    } else {
                        pPrevious->pNext = pSemaphore->pNext;
                    }
                    uPortFree(pSemaphore);
                }
                pPrevious = pSemaphore;
                pSemaphore = pSemaphore->pNext;
            }

            U_PORT_MUTEX_UNLOCK(gMutexForSemaphores);
        }
# else
        // Normal CMSIS case
        errorCode = U_ERROR_COMMON_SUCCESS;
        osSemaphoreDelete((osSemaphoreId_t) semaphoreHandle);
# endif
    }

    return (int32_t) errorCode;
#else
    (void) semaphoreHandle;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Take the given semaphore, CMSIS case.
int32_t uPortPrivateSemaphoreTakeCmsis(const uPortSemaphoreHandle_t semaphoreHandle)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        if (osSemaphoreAcquire((osSemaphoreId_t) semaphoreHandle,
                               osWaitForever) == osOK) {
            semaphoreDec(semaphoreHandle);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
#else
    (void) semaphoreHandle;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Try to take the given semaphore, CMSIS case.
int32_t uPortPrivateSemaphoreTryTakeCmsis(const uPortSemaphoreHandle_t semaphoreHandle,
                                          int32_t delayMs)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (semaphoreHandle != NULL) {
        errorCode = U_ERROR_COMMON_TIMEOUT;
        if (osSemaphoreAcquire((osSemaphoreId_t) semaphoreHandle,
                               MS_TO_TICKS(delayMs)) == osOK) {
            semaphoreDec(semaphoreHandle);
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return (int32_t) errorCode;
#else
    (void) semaphoreHandle;
    (void) delayMs;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Give a semaphore, CMSIS case.
int32_t uPortPrivateSemaphoreGiveCmsis(const uPortSemaphoreHandle_t semaphoreHandle)
{
#ifdef U_PORT_STM32_PURE_CMSIS
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    osStatus_t status;

    if (semaphoreHandle != NULL) {
        // Increment first to avoid race conditions and
        // return success if we have already given enough
        errorCode = U_ERROR_COMMON_SUCCESS;
        if (semaphoreInc(semaphoreHandle)) {
            errorCode = U_ERROR_COMMON_PLATFORM;
            status = osSemaphoreRelease((osSemaphoreId_t) semaphoreHandle);
            // FreeRTOS _does_ obey the semaphore limit but it will
            // return osErrorResource if we ask to release a semaphore
            // more than limit times, whereas our API expects success
            // in that case
            if ((status == osOK) || (status == osErrorResource)) {
                errorCode = U_ERROR_COMMON_SUCCESS;
            } else {
                semaphoreDec(semaphoreHandle);
            }
        }
    }

    return (int32_t) errorCode;
#else
    (void) semaphoreHandle;
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: CRITICAL SECTION FOR CMSIS
 * -------------------------------------------------------------- */

// Enter a critical section, CMSIS-wise.
int32_t uPortPrivateEnterCriticalCmsis()
{
#ifdef U_PORT_STM32_PURE_CMSIS
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
    // ThreadX does not support osKernelLock(),
    // Instead set the current thread to be top
    // priority, which in ThreadX speak is 0, and
    // change its preemption threshold to the same
    TX_THREAD *pThread = tx_thread_identify();

    if ((pThread != NULL) && (gSavedPremptionThreshold < 0) && (gSavedPriority < 0) &&
        (tx_thread_preemption_change(pThread, 0, (UINT *) &gSavedPremptionThreshold) == 0) &&
        (tx_thread_priority_change(pThread, 0, (UINT *) &gSavedPriority) == 0)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }
# else
    if (osKernelLock() == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }
# endif
    return errorCode;
#else
    return (int32_t) U_ERROR_COMMON_NOT_COMPILED;
#endif
}

// Leave a critical section, CMSIS-wise.
void uPortPrivateExitCriticalCmsis()
{
#ifdef U_PORT_STM32_PURE_CMSIS
# ifndef U_PORT_STM32_CMSIS_ON_FREERTOS
    TX_THREAD *pThread = tx_thread_identify();
    UINT dummy;

    if ((pThread != NULL) && (gSavedPremptionThreshold >= 0) && (gSavedPriority >= 0)) {
        tx_thread_priority_change(pThread, (UINT) gSavedPriority, &dummy);
        tx_thread_preemption_change(pThread, (UINT) gSavedPremptionThreshold, &dummy);
        gSavedPriority = -1;
        gSavedPremptionThreshold = -1;
    }
# else
    osKernelUnlock();
# endif
#endif
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: MISC
 * -------------------------------------------------------------- */

// Return the base address for a given GPIO pin.
GPIO_TypeDef *pUPortPrivateGpioGetReg(int32_t pin)
{
    int32_t port = U_PORT_STM32_GPIO_PORT(pin);

    U_ASSERT(port >= 0);
    U_ASSERT(port < sizeof(gpGpioReg) / sizeof(gpGpioReg[0]));

    return gpGpioReg[port];
}

// Enable the clock to the register of the given GPIO pin.
void uPortPrivateGpioEnableClock(int32_t pin)
{
    int32_t port = U_PORT_STM32_GPIO_PORT(pin);

    U_ASSERT(port >= 0);
    U_ASSERT(port < sizeof(gLlApbGrpPeriphGpioPort) /
             sizeof(gLlApbGrpPeriphGpioPort[0]));

#ifdef STM32U575xx
    // All on AHB2 for STM32U5
    LL_AHB2_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[port]);
#else
    // All on AHB1 for STM32F4
    LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[port]);
#endif
}

// End of file
