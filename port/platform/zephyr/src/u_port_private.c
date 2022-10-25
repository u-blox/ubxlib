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
 * @brief Stuff private to the Zephyr porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_private.h"
#include "u_port_event_queue.h"

#include "zephyr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Define a timer, intended to be used as part of a linked-list.
 */
typedef struct uPortPrivateTimer_t {
    struct k_timer *pKTimer; /**< this is used as the handle. */
    uint32_t intervalMs;
    bool periodic;
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

/** Array of timer structures; we do this as a fixed
 * array since, in the Zephyr API, the callback gets a pointer
 * to the timer structure itself.  If that structure were inside
 * the linked list then, should any timers expire after the list
 * had been modified, it could either go bang or end up with the
 * wrong timer.
 */
static struct k_timer gKTimer[U_CFG_OS_TIMER_MAX_NUM];

/** If a user creates and destroys timers dynamically from different
 * threads during the life of an application without making completely
 * sure that the timer expiry calls have not yet landed in any
 * cross-over case then it is technically possible for a kTimer
 * structure to have been re-allocated, resulting in the wrong callback
 * being called.  To combat this, keep a record of the next entry in
 * the gKTimer array that is potentially free and always start the search
 * for a new free entry from there, minimizing the chance that a recently
 * used gKTimer entry will be picked up again.
 */
static size_t gLastKTimerNext = 0;

/** Zephry timer callbacks are called inside ISRs so, in order to put them
 * into task space, we use an event queue.
 */
static int32_t gTimerEventQueueHandle = -1;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a free kernal timer structure
// gMutex should be locked before this is called.
static struct k_timer *pKTimerFindFree()
{
    struct k_timer *pKTimer = NULL;
    uPortPrivateTimer_t *pTimer;
    size_t x = 0;
    size_t i = gLastKTimerNext;

    // For each kernel timer structure in the gKTimer array,
    // check if it is reference by an entry in the linked list;
    // if one isn't then that's the weener.
    for (x = 0; (pKTimer == NULL) && (x < sizeof(gKTimer) / sizeof(gKTimer[0])); x++) {
        pTimer = gpTimerList;
        while ((pTimer != NULL) && (pTimer->pKTimer != &(gKTimer[i]))) {
            pTimer = pTimer->pNext;
        }
        if (pTimer == NULL) {
            pKTimer = &(gKTimer[i]);
            gLastKTimerNext = i + 1;
            if (gLastKTimerNext >= sizeof(gKTimer) / sizeof(gKTimer[0])) {
                gLastKTimerNext = 0;
            }
        } else {
            i++;
            if (i >= sizeof(gKTimer) / sizeof(gKTimer[0])) {
                i = 0;
            }
        }
    }

    return pKTimer;
}

// Find a timer entry in the list.
// gMutex should be locked before this is called.
static uPortPrivateTimer_t *pTimerFind(struct k_timer *pKTimer)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;

    while ((pTimer != NULL) && (pTimer->pKTimer != pKTimer)) {
        pTimer = pTimer->pNext;
    }

    return pTimer;
}

// Remove an entry from the list.
// gMutex should be locked before this is called.
static void timerRemove(struct k_timer *pKTimer)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;
    uPortPrivateTimer_t *pPrevious = NULL;

    // Find the entry in the list
    while ((pTimer != NULL) && (pTimer->pKTimer != pKTimer)) {
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

// The timer event handler, where the parameter is a pointer to a
// kTimer pointer.
static void timerEventHandler(void *pParam, size_t paramLength)
{
    struct k_timer *pKTimer = *((struct k_timer **) pParam);
    uPortPrivateTimer_t *pTimer;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;

    (void) paramLength;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pTimer = pTimerFind(pKTimer);
        if (pTimer != NULL) {
            pCallback = pTimer->pCallback;
            pCallbackParam = pTimer->pCallbackParam;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Call the callback outside the locks so that the
        // callback itself may call the timer API
        if (pCallback != NULL) {
            pCallback((uPortTimerHandle_t) pKTimer, pCallbackParam);
        }
    }
}

// The timer expiry callback, called by Zephyr from interrupt context.
static void timerCallbackInt(struct k_timer *pKTimer)
{
    if (gTimerEventQueueHandle >= 0) {
        // Send an event to our event task with the pointer
        // pKTimer as the payload
        uPortEventQueueSendIrq(gTimerEventQueueHandle,
                               &pKTimer, sizeof(pKTimer)); // NOLINT(bugprone-sizeof-expression)
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: MISC
 * -------------------------------------------------------------- */

// Initalise the private stuff.
int32_t uPortPrivateInit()
{
    int32_t errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutex);
        if (errorCodeOrEventQueueHandle == 0) {
            // We need an event queue as Zephyr's timer callback is called
            // in interrupt context and we need to get it into task context
            errorCodeOrEventQueueHandle = uPortEventQueueOpen(timerEventHandler, "timerEvent",
                                                              sizeof(struct k_timer *),
                                                              U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES,
                                                              U_CFG_OS_TIMER_EVENT_TASK_PRIORITY,
                                                              U_CFG_OS_TIMER_EVENT_QUEUE_SIZE);
            if (errorCodeOrEventQueueHandle >= 0) {
                gTimerEventQueueHandle = errorCodeOrEventQueueHandle;
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
            k_timer_stop((struct k_timer *) gpTimerList->pKTimer);
            timerRemove(gpTimerList->pKTimer);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Close the event queue outside the mutex as it could be calling
        // back into this API
        if (gTimerEventQueueHandle >= 0) {
            uPortEventQueueClose(gTimerEventQueueHandle);
            gTimerEventQueueHandle = -1;
        }

        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: TIMERS
 * -------------------------------------------------------------- */

// Add a timer entry to the list.
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pHandle != NULL) {
            // Create an entry in the list
            pTimer = (uPortPrivateTimer_t *) pUPortMalloc(sizeof(uPortPrivateTimer_t));
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if (pTimer != NULL) {
                // Find a free timer structure
                pTimer->pKTimer = pKTimerFindFree();
                if (pTimer->pKTimer != NULL) {
                    // Populate the entry
                    k_timer_init(pTimer->pKTimer, timerCallbackInt, NULL);
                    pTimer->intervalMs = intervalMs;
                    pTimer->periodic = periodic;
                    pTimer->pCallback = pCallback;
                    pTimer->pCallbackParam = pCallbackParam;
                    // Add the timer to the front of the list
                    pTimer->pNext = gpTimerList;
                    gpTimerList = pTimer;
                    *pHandle = (uPortTimerHandle_t) (pTimer->pKTimer);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Tidy up if a free timer could not be found
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

        // Stop the timer in the kernel, outside the mutex in case
        // the call blocks
        k_timer_stop((struct k_timer *) handle);

        U_PORT_MUTEX_LOCK(gMutex);

        timerRemove((struct k_timer *) handle);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Start a timer.
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer = NULL;
    k_timeout_t duration;
    k_timeout_t period = {0};

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pTimer = pTimerFind((struct k_timer *) handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            duration = K_MSEC(pTimer->intervalMs);
            if (pTimer->periodic) {
                period = duration;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Release the mutex before starting the timer
        // in case the OS call blocks
        if (pTimer != NULL) {
            k_timer_start((struct k_timer *) handle, duration, period);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Change a timer interval.
int32_t uPortPrivateTimerChange(const uPortTimerHandle_t handle,
                                uint32_t intervalMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pTimer = pTimerFind(handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            pTimer->intervalMs = intervalMs;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}


// End of file
