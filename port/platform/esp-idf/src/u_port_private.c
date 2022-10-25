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
 * @brief Stuff private to the ESP32 porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy()

#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_private.h"
#include "u_port_event_queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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
    struct uPortPrivateTimer_t *pNext;
} uPortPrivateTimer_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list of timers.
 */
static uPortPrivateTimer_t *gpTimerList = NULL;

/** Mutex to protect the linked list of timers.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Use an event queue to move the execution of the timer callback
 * outside of the FreeRTOS timer task.
 */
static int32_t gEventQueueHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a timer entry in the list.
// gMutex should be locked before this is called.
static uPortPrivateTimer_t *pTimerFind(uPortTimerHandle_t handle)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;

    while ((pTimer != NULL) && (pTimer->handle != handle)) {
        pTimer = pTimer->pNext;
    }

    return pTimer;
}

// Remove an entry from the list.
// gMutex should be locked before this is called.
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
    TimerHandle_t handle = *((TimerHandle_t *) pParam);
    uPortPrivateTimer_t *pTimer;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;

    (void) paramLength;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pTimer = pTimerFind((uPortTimerHandle_t) handle);
        if (pTimer != NULL) {
            pCallback = pTimer->pCallback;
            pCallbackParam = pTimer->pCallbackParam;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Call the callback outside the locks so that the
        // callback itself may call the timer API
        if (pCallback != NULL) {
            pCallback((uPortTimerHandle_t) handle, pCallbackParam);
        }
    }
}

// The timer expiry callback, called by FreeRTOS.
static void timerCallback(TimerHandle_t handle)
{
    if (gEventQueueHandle >= 0) {
        // Send an event to our event task with the timer
        // handle as the payload, IRQ version so as never
        // to block
        uPortEventQueueSendIrq(gEventQueueHandle,
                               &handle, sizeof(handle));
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT
 * -------------------------------------------------------------- */

// Initialise the private stuff.
int32_t uPortPrivateInit()
{
    int32_t errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutex);
        if ((errorCodeOrEventQueueHandle == 0) && (gEventQueueHandle < 0)) {
            // We need an event queue to offload the callback execution
            // from the FreeRTOS timer task
            errorCodeOrEventQueueHandle = uPortEventQueueOpen(timerEventHandler, "timerEvent",
                                                              sizeof(TimerHandle_t),
                                                              U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES,
                                                              U_CFG_OS_TIMER_EVENT_TASK_PRIORITY,
                                                              U_CFG_OS_TIMER_EVENT_QUEUE_SIZE);
            if (errorCodeOrEventQueueHandle >= 0) {
                gEventQueueHandle = errorCodeOrEventQueueHandle;
                errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCodeOrEventQueueHandle;
}

// Deinitialise the private stuff.
void uPortPrivateDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Tidy away the timers
        while (gpTimerList != NULL) {
            xTimerStop((TimerHandle_t) gpTimerList->handle,
                       (TickType_t) portMAX_DELAY);
            timerRemove(gpTimerList->handle);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Close the event queue outside the mutex as it could be calling
        // back into this API
        if (gEventQueueHandle >= 0) {
            uPortEventQueueClose(gEventQueueHandle);
            gEventQueueHandle = -1;
        }

        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

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

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

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
                pTimer->handle = (uPortTimerHandle_t) xTimerCreate(_pName,
                                                                   intervalMs / portTICK_PERIOD_MS,
                                                                   periodic ? pdTRUE : pdFALSE,
                                                                   NULL, timerCallback);
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

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Remove a timer entry from the list.
int32_t uPortPrivateTimerDelete(uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        // Delete the timer in the RTOS, outside the mutex as it can block
        errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
        if (xTimerDelete((TimerHandle_t) handle,
                         (TickType_t) portMAX_DELAY) == pdPASS) {

            U_PORT_MUTEX_LOCK(gMutex);

            timerRemove(handle);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }

    return errorCode;
}

// End of file
