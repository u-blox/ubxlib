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
 * @brief Implementation of the event queue API.  This will run on
 * any platform.
 *
 * Design note: the event queue entries are stored in a fixed length
 * table rather than a linked list.  This is deliberate: it allows
 * the  handle to be an index rather than a pointer (improved
 * protection) but, most importantly, means that no loop is required
 * to find a queue, ensuring the lowest possible latency so that
 * send-to-queue can safely be called from an interrupt.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_port_event_queue_private.h"
#include "u_port_event_queue.h"

#if defined(__NEWLIB__) && defined(_REENT_SMALL) && !_REENT_GLOBAL_STDIO_STREAMS
#include "u_cfg_sw.h"
#include "u_port_debug.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The info for an event queue.
 */
typedef struct uEventQueue_t {
    void (*pFunction)(void *, size_t); /** The function to be called. */
    int32_t handle;            /** Handle for this event queue. */
    uPortQueueHandle_t queue; /** Handle for the OS queue. */
    size_t paramMaxLengthBytes; /** Max length of an item on this OS queue. */
    uPortTaskHandle_t task; /** Handle for the OS task. */
    uPortMutexHandle_t taskRunningMutex; /** Mutex to determine if task has exited. */
} uEventQueue_t;

/** The control/size word, prefixed to the parameter block sent to
 * the queue. Negative values are a control word, else this is the
 * size of the parameter block which follows.
 */
typedef enum {
//lint -esym(749, uEventQueueControlOrSize_t::U_EVENT_CONTROL_FORCE_INT32) Suppress enum not referenced
    U_EVENT_CONTROL_FORCE_INT32 = 0x7FFFFFFF, /* Force this enum to always
                                               * be 32 bit so that it can
                                               * also be used as a size. */
    U_EVENT_CONTROL_NONE = 0,
    U_EVENT_CONTROL_EXIT_NOW = -1
} uEventQueueControlOrSize_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the table.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Jump table, allowing an event queue to be found
 * without the need for a loop.
 */
static uEventQueue_t *gpEventQueue[U_PORT_EVENT_QUEUE_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Run the user function.  This will be run multiple times in a
// task of its own.
static void eventQueueTask(void *pParam)
{
    uEventQueue_t *pEventQueue = (uEventQueue_t *) pParam;
    char param[U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES +
                                                               U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES];
    uEventQueueControlOrSize_t *pControlOrSize = (uEventQueueControlOrSize_t *)
                                                 & (param[0]);

    U_PORT_MUTEX_LOCK(pEventQueue->taskRunningMutex);
#if defined(__NEWLIB__) && defined(_REENT_SMALL) && \
    !defined(_REENT_GLOBAL_STDIO_STREAMS) && !defined(_UNBUF_STREAM_OPT)
    // This is a temporary workaround to prevent false memory leak failures
    // in our automated tests.
    // When _REENT_SMALL is enabled in newlib the allocation of the stdout
    // stream is delayed until it is needed. To prevent the delayed allocation
    // we just make an empty print here.
    //
    // TODO: REMOVE THIS WHEN #272 IS DONE
    //
    // Note: If this is enabled for ESP32 it will crash... (?)
    uPortLog("");
#endif

    *pControlOrSize = U_EVENT_CONTROL_NONE;
    // Continue until we're told to exit
    while (*pControlOrSize != U_EVENT_CONTROL_EXIT_NOW) {
        if (uPortQueueReceive(pEventQueue->queue, param) == 0) {
            // If this is not a control message, call the
            // user function with the parameter block,
            // skipping the "control or size" word at the
            // start and passing it in instead as the size
            // parameter
            if ((int32_t) *pControlOrSize >= 0) {
                if ((int32_t) *pControlOrSize > 0) {
                    pEventQueue->pFunction((void *) & (param[U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES]),
                                           // Cast in two stages to keep Lint happy
                                           (size_t) (int32_t) *pControlOrSize);
                } else {
                    pEventQueue->pFunction(NULL, 0);
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(pEventQueue->taskRunningMutex);

    // Delete ourself
    uPortTaskDelete(NULL);
}

// Get the next free event handle.
static int32_t nextEventHandleGet()
{
    int32_t handle = -1;

    for (size_t x = 0; (handle < 0) &&
         x < sizeof(gpEventQueue) / sizeof(gpEventQueue[0]);
         x++) {
        if (gpEventQueue[x] == NULL) {
            handle = (int32_t) x;
        }
    }

    return handle;
}

// Close an event queue.
// The mutex must be locked before this is called.
static int32_t eventQueueClose(uEventQueue_t *pEventQueue)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    void *pControl;

    // It would be nice to send just U_EVENT_CONTROL_EXIT_NOW
    // on its own here but, as address sanitizer points out,
    // the uPortQueueSend() function must copy the required
    // length for an item on the queue so it has to be
    // given that data size, hence we allocate the block,
    // put U_EVENT_CONTROL_EXIT_NOW at the start of it and
    // then free it once it is sent
    pControl = pUPortMalloc(pEventQueue->paramMaxLengthBytes +
                            U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES);

    if (pControl != NULL) {
        *((uEventQueueControlOrSize_t *) pControl) = U_EVENT_CONTROL_EXIT_NOW;
        // Get the task to exit, persisting until it is done
        while (uPortQueueSend(pEventQueue->queue, pControl) != 0) {
            uPortTaskBlock(10);
        }
        uPortFree(pControl);
        U_PORT_MUTEX_LOCK(pEventQueue->taskRunningMutex);
        U_PORT_MUTEX_UNLOCK(pEventQueue->taskRunningMutex);

        // Tidy up
        uPortMutexDelete(pEventQueue->taskRunningMutex);
        errorCode = uPortQueueDelete(pEventQueue->queue);

        // Pause here to allow the deletions
        // above to actually occur in the idle thread,
        // required by some RTOSs (e.g. FreeRTOS)
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Now remove it from the list and free it
        gpEventQueue[pEventQueue->handle] = NULL;
        uPortFree(pEventQueue);
    }

    return errorCode;
}

// Find an event queue's structure in the table.
// The mutex must be locked before this is called.
static inline uEventQueue_t *pEventQueueGet(int32_t handle)
{
    uEventQueue_t *pEventQueue = NULL;

    if ((handle >= 0) &&
        (handle < (int32_t) (sizeof(gpEventQueue) / sizeof(gpEventQueue[0])))) {
        pEventQueue = gpEventQueue[handle];
    }

    return pEventQueue;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: BUT ONES THAT SHOULD BE CALLED INTERNALLY ONLY
 * -------------------------------------------------------------- */

// Initialise event queues.
// Suppress Lint warnings about this not being called etc., Lint just
// can't see where it is being called from.
//lint -esym(759, uPortEventQueuePrivateInit)
//lint -esym(765, uPortEventQueuePrivateInit)
//lint -esym(714, uPortEventQueuePrivateInit)
int32_t uPortEventQueuePrivateInit(void)
{
    int32_t errorCode = 0;

    if (gMutex == NULL) {
        for (size_t x = 0;
             x < sizeof(gpEventQueue) / sizeof(gpEventQueue[0]);
             x++) {
            gpEventQueue[x] = NULL;
        }
        // Allocate the mutex to protect the table
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

// Deinitialise event queues.
// Suppress Lint warnings about this not being called etc., Lint just
// can't see where it is being called from.
//lint -esym(759, uPortEventQueuePrivateDeinit)
//lint -esym(765, uPortEventQueuePrivateDeinit)
//lint -esym(714, uPortEventQueuePrivateDeinit)
void uPortEventQueuePrivateDeinit(void)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Remove all the event queues
        for (size_t x = 0;
             x < sizeof(gpEventQueue) / sizeof(gpEventQueue[0]);
             x++) {
            if (gpEventQueue[x] != NULL) {
                U_ASSERT(eventQueueClose(gpEventQueue[x]) == 0);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        // Finally delete the mutex
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open an event queue.
int32_t uPortEventQueueOpen(void (*pFunction) (void *, size_t),
                            const char *pName,
                            size_t paramMaxLengthBytes,
                            size_t stackSizeBytes,
                            int32_t priority,
                            size_t queueLength)
{
    uEventQueue_t *pEventQueue = NULL;
    uErrorCode_t handleOrError = U_ERROR_COMMON_NOT_INITIALISED;
    int32_t handle;
    const char *pTaskName = "eventQueueTask";

    if (gMutex != NULL) {
        handleOrError = U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pFunction != NULL) &&
            (paramMaxLengthBytes <= U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) &&
            (stackSizeBytes >= U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES) &&
            (priority >= U_CFG_OS_PRIORITY_MIN) &&
            (priority <= U_CFG_OS_PRIORITY_MAX) &&
            (queueLength > 0)) {

            U_PORT_MUTEX_LOCK(gMutex);

            handleOrError = U_ERROR_COMMON_NO_MEMORY;
            // See if there's a free handle
            handle = nextEventHandleGet();
            if (handle >= 0) {
                // Malloc a structure to represent the event queue
                pEventQueue = (uEventQueue_t *) pUPortMalloc(sizeof(uEventQueue_t));
                if (pEventQueue != NULL) {
                    pEventQueue->pFunction = pFunction;
                    pEventQueue->paramMaxLengthBytes = paramMaxLengthBytes;
                    // Create the queue
                    handleOrError = (uErrorCode_t) uPortQueueCreate(queueLength,
                                                                    paramMaxLengthBytes +
                                                                    U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES,
                                                                    &(pEventQueue->queue));
                    if (handleOrError == U_ERROR_COMMON_SUCCESS) {
                        // Create the mutex for task running status
                        handleOrError = (uErrorCode_t) uPortMutexCreate(&(pEventQueue->taskRunningMutex));
                        if (handleOrError == U_ERROR_COMMON_SUCCESS) {
                            // Finally, create the task itself
                            if (pName != NULL) {
                                pTaskName = pName;
                            }
                            handleOrError = (uErrorCode_t) uPortTaskCreate(eventQueueTask,
                                                                           pTaskName,
                                                                           stackSizeBytes,
                                                                           (void *) pEventQueue,
                                                                           priority,
                                                                           &(pEventQueue->task));
                            if (handleOrError == U_ERROR_COMMON_SUCCESS) {
                                // Wait for the eventQueueTask to lock the mutex,
                                // which shows it is running
                                while (uPortMutexTryLock(pEventQueue->taskRunningMutex, 0) == 0) {
                                    uPortMutexUnlock(pEventQueue->taskRunningMutex);
                                    uPortTaskBlock(U_CFG_OS_YIELD_MS);
                                }
                                // Add the event queue structure to the list
                                pEventQueue->handle = handle;
                                gpEventQueue[handle] = pEventQueue;
                                // Return the handle
                                handleOrError = (uErrorCode_t) handle;
                            } else {
                                // Couldn't create the task, delete the
                                // mutex and queue and free the structure
                                uPortMutexDelete(pEventQueue->taskRunningMutex);
                                uPortQueueDelete(pEventQueue->queue);
                                uPortFree(pEventQueue);
                            }
                        } else {
                            // Couldn't create the mutex, delete the queue
                            // and free the structure
                            uPortQueueDelete(pEventQueue->queue);
                            uPortFree(pEventQueue);
                        }
                    } else {
                        // Couldn't create the queue, free the structure
                        uPortFree(pEventQueue);
                    }
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }

    return (int32_t) handleOrError;
}

// Send to an event queue.
int32_t uPortEventQueueSend(int32_t handle, const void *pParam,
                            size_t paramLengthBytes)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uEventQueue_t *pEventQueue;
    char *pBlock = NULL;
    uPortQueueHandle_t queue = NULL;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pEventQueue = pEventQueueGet(handle);
        if ((pEventQueue != NULL) &&
            (paramLengthBytes <= pEventQueue->paramMaxLengthBytes) &&
            ((pParam != NULL) || (paramLengthBytes == 0))) {
            queue = pEventQueue->queue;
            errorCode = U_ERROR_COMMON_NO_MEMORY;
            // We need to add the control word to the start, so pUPortMalloc
            // a block that is paramMaxLengthBytes (i.e. paramMaxLengthBytes
            // of the queue, not just the paramLengthBytes passed in, since
            // uPortQueueSend() will expect to copy the full length) plus
            // plus the control word length
            pBlock = (char *) pUPortMalloc(pEventQueue->paramMaxLengthBytes +
                                           U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES);
            if (pBlock != NULL) {
                // Copy in the control word, which is actually just
                // the size in this case
                //lint -e(826) Suppress area too small; the size of pBlock is always
                // at least U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES in size
                *((uEventQueueControlOrSize_t *) pBlock) = (uEventQueueControlOrSize_t) paramLengthBytes;
                if (pParam != NULL) {
                    // Copy in param
                    //lint -e{826} Suppress pointed-to area too small, we make sure it is OK above
                    memcpy(pBlock + U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES,
                           pParam, paramLengthBytes);
                }
            }
        }

        // We release the mutex before sending to the
        // queue since the send process may block (e.g.
        // if the queue is full) and we don't want
        // that to block the entire API
        U_PORT_MUTEX_UNLOCK(gMutex);

        if (pBlock != NULL) {
            if (queue != NULL) {
                // Send it off
                errorCode = (uErrorCode_t) uPortQueueSend(queue, pBlock);
            }
            // Free memory again
            uPortFree(pBlock);
        }
    }

    return (int32_t) errorCode;
}

// Send to an event queue from an interrupt.
int32_t uPortEventQueueSendIrq(int32_t handle, const void *pParam,
                               size_t paramLengthBytes)
{
#ifndef _WIN32
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    uEventQueue_t *pEventQueue;
    char block[paramLengthBytes +
                                U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES];

    if (gMutex != NULL) {
        // Can't lock the mutex, we're in an interrupt.
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pEventQueue = pEventQueueGet(handle);
        if ((pEventQueue != NULL) &&
            (paramLengthBytes <= pEventQueue->paramMaxLengthBytes) &&
            ((pParam != NULL) || (paramLengthBytes == 0))) {
            // Copy in the control word, which is actually just
            // the size in this case
            //lint -e(826) Suppress area too small; the size of pBlock is always
            // at least U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES in size
            uEventQueueControlOrSize_t *pControlOrSize = (uEventQueueControlOrSize_t *) block;
            *pControlOrSize = (uEventQueueControlOrSize_t) paramLengthBytes;
            if (pParam != NULL) {
                // Copy in param
                memcpy(block + U_PORT_EVENT_QUEUE_CONTROL_OR_SIZE_LENGTH_BYTES,
                       pParam, paramLengthBytes);
            }
            // Send it off
            errorCode = (uErrorCode_t) uPortQueueSendIrq(pEventQueue->queue,
                                                         block);
        }
    }
#else
    // It would have been nice to leave this function as-is for win32
    // and just let uPortQueueSendIrq() return an error ('cos the IRQ
    // functions are not supported for the Windows platform), however
    // MSVC, which we use on Windows, doesn't support dynamically sized
    // arrays and hence we'd need to switch to using a static buffer for
    // block, which is entirely unnecessary.  Hence this compiler switch.
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_SUPPORTED;

    (void) handle;
    (void) pParam;
    (void) paramLengthBytes;
#endif

    return (int32_t) errorCode;
}

// Return whether we're in the event queue's task.
bool uPortEventQueueIsTask(int32_t handle)
{
    uEventQueue_t *pEventQueue;
    bool isEventTask = false;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pEventQueue = pEventQueueGet(handle);
        if (pEventQueue != NULL) {
            isEventTask = uPortTaskIsThis(pEventQueue->task);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return isEventTask;
}

// Get the stack high watermark for an event queue's task.
int32_t uPortEventQueueStackMinFree(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uEventQueue_t *pEventQueue;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pEventQueue = pEventQueueGet(handle);
        if (pEventQueue != NULL) {
            sizeOrErrorCode = uPortTaskStackMinFree(pEventQueue->task);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return sizeOrErrorCode;
}

// Close an event queue.
int32_t uPortEventQueueClose(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uEventQueue_t *pEventQueue;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pEventQueue = pEventQueueGet(handle);
        if (pEventQueue != NULL) {
            errorCode = eventQueueClose(pEventQueue);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the number of entries free on the given event queue.
int32_t uPortEventQueueGetFree(int32_t handle)
{
    int32_t errorCodeOrFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uEventQueue_t *pEventQueue;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pEventQueue = pEventQueueGet(handle);
        if (pEventQueue != NULL) {
            errorCodeOrFree = uPortQueueGetFree(pEventQueue->queue);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrFree;
}

// End of file
