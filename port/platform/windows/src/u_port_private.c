/*
 * Copyright 2020 u-blox
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
 * @brief Stuff private to the Windows porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stdlib.h"    // For malloc()/free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "windows.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The lowest value that a queue handle can have; we avoid 0 since
 * there may be checks for NULL-ness floating around that it
 * would be prudent to avoid.
 */
#define U_PORT_PRIVATE_QUEUE_HANDLE_MIN 1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold a queue as part of a linked list.
 */
typedef struct uPortPrivateQueue_t {
    int32_t queueHandle;
    size_t itemSizeBytes;
    size_t bufferSizeBytes;
    char *pBuffer;
    char *pWrite;
    char *pRead;
    uPortSemaphoreHandle_t writeSemaphore;
    uPortSemaphoreHandle_t readSemaphore;
    uPortSemaphoreHandle_t accessSemaphore;
    struct uPortPrivateQueue_t *pNext;
} uPortPrivateQueue_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the linked list of queues.
 */
static uPortMutexHandle_t gMutexQueue = NULL;

/** A hook for the linked list of queues.
 */
static uPortPrivateQueue_t *gpQueueRoot = NULL;

/** The next queue handle to use.
 */
static int32_t gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;

/** Convert a local task priority value into a Windows one.
 */
static const int32_t localToWinPriority[] = {-2,  // 0
                                             -2,  // 1
                                             -2,  // 2
                                             -2,  // 3
                                             -1,  // 4
                                             -1,  // 5
                                             -1,  // 6
                                             -1,  // 7
                                             0,   // 8
                                             0,   // 9
                                             0,   // 10
                                             0,   // 11
                                             1,   // 12
                                             1,   // 13
                                             1,   // 14
                                             1    // 15
                                             };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a queue in the list by handle (i.e. index) and return
// a pointer to it.
// gMutexQueue should be locked before this is called.
static uPortPrivateQueue_t *pQueueFind(int32_t handle)
{
    uPortPrivateQueue_t *pTmp = gpQueueRoot;
    uPortPrivateQueue_t *pFound = NULL;

    while ((pTmp != NULL) && (pFound == NULL)) {
        if (pTmp->queueHandle == handle) {
            pFound = pTmp;
        } else {
            pTmp = pTmp->pNext;
        }
    }

    return pFound;
}

// Return a copy of a queue from the list.
// gMutexQueue should be locked before this is called.
int32_t queueGetCopy(int32_t handle, uPortPrivateQueue_t *pQueue)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pFound;

    pFound = pQueueFind(handle);
    if (pFound != NULL) {
        *pQueue = *pFound;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Write data to the queue and increment pointers.
// gMutexQueue should be locked before this is called.
int32_t queueWrite(int32_t handle, const char *pData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pQueue;

    pQueue = pQueueFind(handle);
    if (pQueue != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        // Copy in the data and advance the write pointer
        memcpy(pQueue->pWrite, pData, pQueue->itemSizeBytes);
        pQueue->pWrite += pQueue->itemSizeBytes;
        if (pQueue->pWrite >= pQueue->pBuffer + pQueue->bufferSizeBytes) {
            pQueue->pWrite = pQueue->pBuffer;
        }
    }

    return errorCode;
}

// Read data from the queue and increment pointers.
// gMutexQueue should be locked before this is called.
int32_t queueRead(int32_t handle, char *pData)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uPortPrivateQueue_t *pQueue;

    pQueue = pQueueFind(handle);
    if (pQueue != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (pData != NULL) {
            // Copy out the data
            memcpy(pData, pQueue->pRead, pQueue->itemSizeBytes);
        }
        // Advance the read pointer
        pQueue->pRead +=  pQueue->itemSizeBytes;
        if (pQueue->pRead >=  pQueue->pBuffer +  pQueue->bufferSizeBytes) {
            pQueue->pRead =  pQueue->pBuffer;
        }
    }

    return errorCode;
}

// Free memory held by a queue and the queue entry itself.
// gMutexQueue should be locked before this is called.
void queueFree(uPortPrivateQueue_t *pQueue)
{
    uPortSemaphoreDelete(pQueue->accessSemaphore);
    uPortSemaphoreDelete(pQueue->writeSemaphore);
    uPortSemaphoreDelete(pQueue->readSemaphore);
    // It is valid C to free a NULL buffer
    free(pQueue->pBuffer);
    free(pQueue);
}

// Remove a queue from the list, freeing memory.
// gMutexQueue should be locked before this is called.
void queueRemove(int32_t handle)
{
    uPortPrivateQueue_t *pTmp = gpQueueRoot;
    uPortPrivateQueue_t *pPrevious = NULL;

    while (pTmp != NULL) {
        if (pTmp->queueHandle == handle) {
            if (pPrevious == NULL) {
                // At head
                gpQueueRoot = pTmp->pNext;
            } else {
                pPrevious->pNext = pTmp->pNext;
            }
            // Free memory
            queueFree(pTmp);
            // Force exit
            pTmp = NULL;
        } else {
            pPrevious = pTmp;
            pTmp = pTmp->pNext;
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, MISC
 * -------------------------------------------------------------- */

// Initialise the private bits of the porting layer.
int32_t uPortPrivateInit(void)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexQueue == NULL) {
        errorCode = uPortMutexCreate(&gMutexQueue);
    }

    return errorCode;
}

// Deinitialise the private bits of the porting layer.
void uPortPrivateDeinit(void)
{
    if (gMutexQueue != NULL) {
        U_PORT_MUTEX_LOCK(gMutexQueue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);
        uPortMutexDelete(gMutexQueue);
        gMutexQueue = NULL;
    }
}

// Convert to Windows thread priority.
int32_t uPortPrivateTaskPriorityConvert(int32_t priority)
{
    if (priority < 0) {
        priority = 0;
    }
    if (priority > sizeof(localToWinPriority) - 1) {
        priority = sizeof(localToWinPriority) - 1;
    }

    return localToWinPriority[priority];
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT, QUEUES
 * -------------------------------------------------------------- */

/* Note: this implementation may seem more complex than it needs to
 * be!  Reasoning goes like this:
 *
 * 1.  We need a global mutex to protect the linked list of queues.
 * 2.  However, we obviously can't use that global mutex to protect
 *     _usage_ of any one queue, as holding the lock during the blocking
 *     read() would prevent a write() occurring.
 * 3.  So we take a copy of the queue entry while we work on it.
 * 4.  We employ a "write" semaphore whose maximum value is the maximum
 *     number of items in the queue, this way we can take() that
 *     semaphore to know that there is space to write to.
 * 5.  To actually do a write(), manipulating the pointers, we need to
 *     lock the global mutex again, to prevent the pointers being
 *     accessed by another write or disappearing from under us should
 *     the queue be closed.
 * 6.  However, how do we know that won't have happened between taking
 *     the "write" semaphore and obtaining the global mutex lock? To
 *     cover this, each queue also has an "access" semaphore.
 * 7.  When we have taken the "write" semaphore we take the "access"
 *     semaphore and then we can lock the global mutex.
 * 8.  Now of course, the queue may have been closed in the time
 *     between taking the "access" semaphore and locking the global
 *     mutex.  So, once we get inside the global mutex lock, we
 *     give() the "access" semaphore: if that semaphore has been
 *     vapourised by the queue being closed, give()ing it will return
 *     an error and we know not to go writing things to pointers that
 *     no longer exist.
 * 9.  We write the data into the queue and advance pointers.
 * 10. We indicate that something has been written by give()ing a
 *     "read" semaphore.
 * 11. The global mutex is released, job done.
 * 12. The read() process is similar, this time waiting on the "read"
 *     semaphore (which was give()n by the write() function) and
 *     give()ing the "write" semaphore.
 */

// Add a queue to the list, returning its handle.
int32_t uPortPrivateQueueAdd(size_t itemSizeBytes, size_t maxNumItems)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t x;
    size_t bufferSizeBytes = itemSizeBytes * maxNumItems;
    uPortPrivateQueue_t *pNew;
    bool success = true;

    if (gMutexQueue != NULL) {
        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;

        U_PORT_MUTEX_LOCK(gMutexQueue);

        // Find a unique handle
        x = gNextQueueHandle;
        while ((pQueueFind(gNextQueueHandle) != NULL) && success) {
            gNextQueueHandle++;
            if (gNextQueueHandle < U_PORT_PRIVATE_QUEUE_HANDLE_MIN) {
                gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;
            }
            if (gNextQueueHandle == x) {
                // Wrapped
                success = false;
            }
        }

        if (success) {
            // Allocate memory for the queue
            pNew = (uPortPrivateQueue_t *) malloc(sizeof(uPortPrivateQueue_t));
            if (pNew != NULL) {
                pNew->writeSemaphore = INVALID_HANDLE_VALUE;
                pNew->readSemaphore = INVALID_HANDLE_VALUE;
                pNew->accessSemaphore = INVALID_HANDLE_VALUE;
                pNew->pBuffer = NULL;
                // Create the semaphores
                // The write and read semaphores are created with the number of items
                // in the queue, write starting at maxNumItems and read starting at 0 items
                if ((uPortSemaphoreCreate(&pNew->writeSemaphore, maxNumItems, maxNumItems) == 0) &&
                    (uPortSemaphoreCreate(&pNew->readSemaphore, 0, maxNumItems) == 0) &&
                    (uPortSemaphoreCreate(&pNew->accessSemaphore, 1, 1) == 0)) {
                    // Allocate memory for the buffer
                    pNew->pBuffer = (char *) malloc(bufferSizeBytes);
                    if (pNew->pBuffer != NULL) {
                        // Populate the queue entry and add it to the front
                        // of the list
                        pNew->queueHandle = gNextQueueHandle;
                        pNew->itemSizeBytes = itemSizeBytes;
                        pNew->bufferSizeBytes = bufferSizeBytes;
                        pNew->pWrite = pNew->pBuffer;
                        pNew->pRead = pNew->pBuffer;
                        pNew->pNext = gpQueueRoot;
                        gpQueueRoot = pNew;
                        errorCodeOrHandle = pNew->queueHandle;
                        gNextQueueHandle++;
                        if (gNextQueueHandle < U_PORT_PRIVATE_QUEUE_HANDLE_MIN) {
                            gNextQueueHandle = U_PORT_PRIVATE_QUEUE_HANDLE_MIN;
                        }
                    }
                }
                if (errorCodeOrHandle < 0) {
                    // If we couldn't get memory for something, tidy up
                    queueFree(pNew);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexQueue);
    }

    return errorCodeOrHandle;
}

// Write a block of data to the given queue.
int32_t uPortPrivateQueueWrite(int32_t handle, const char *pData)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {
        errorCode =  (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pData != NULL) {
            // Get a copy of the queue while within the locks,
            // then release them so that the entire system
            // doesn't jam up while we wait for space to be available
            U_PORT_MUTEX_LOCK(gMutexQueue);
            errorCode = queueGetCopy(handle, &queue);
            U_PORT_MUTEX_UNLOCK(gMutexQueue);

            if (errorCode == 0) {
                // Wait for space to be available
                errorCode = uPortSemaphoreTake(queue.writeSemaphore);
                if (errorCode == 0) {
                    // Space is available, wait for the access semaphore
                    // to read it
                    errorCode = uPortSemaphoreTake(queue.accessSemaphore);
                    if (errorCode == 0) {

                        // While within the locks give back the access
                        // semaphore, write the data and potentially give back
                        // the space available semaphore
                        U_PORT_MUTEX_LOCK(gMutexQueue);

                        // NOTHING WITHIN THIS LOCK must block

                        // If the queue was closed or some such between the
                        // time we unlocked and relocked the mutex then the
                        // following line should return an error
                        errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                        if (errorCode == 0) {
                            // Now actually write the data and increment pointers
                            errorCode = queueWrite(queue.queueHandle, pData);
                            if (errorCode == 0) {
                                // Data is now available for reading
                                errorCode = uPortSemaphoreGive(queue.readSemaphore);
                            }
                        }

                        U_PORT_MUTEX_UNLOCK(gMutexQueue);
                    }
                }
            }
        }
    }

    return errorCode;
}

// Read a block of data from the given queue.
int32_t uPortPrivateQueueRead(int32_t handle, char *pData, int32_t waitMs)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;
    int64_t timeUsed;
    int64_t waitMs64 = (int64_t) waitMs;

    if (gMutexQueue != NULL) {

        // Get a copy of the queue while within the locks,
        // then release them so that the entire system
        // doesn't jam up while we wait for data to be available
        U_PORT_MUTEX_LOCK(gMutexQueue);
        errorCode = queueGetCopy(handle, &queue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);

        if (errorCode == 0) {
            // Wait for data to be available
            if (waitMs < 0) {
                errorCode = uPortSemaphoreTake(queue.readSemaphore);
            } else {
                timeUsed = uPortGetTickTimeMs();
                errorCode = uPortSemaphoreTryTake(queue.readSemaphore, waitMs);
            }
            if (errorCode == 0) {
                // Data is available, wait for the access semaphore
                // to read it
                if (waitMs < 0) {
                    errorCode = uPortSemaphoreTake(queue.accessSemaphore);
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    // Compensate for the time already spent waiting
                    timeUsed = uPortGetTickTimeMs() - timeUsed;
                    if (timeUsed > 0) { // Handle wrap in tick time
                        waitMs64 -= timeUsed;
                    }
                    if ((waitMs64 >= 0) && (waitMs64 <= INT_MAX)) {
                        errorCode = uPortSemaphoreTryTake(queue.accessSemaphore, (int32_t) waitMs64);
                    }
                }
                if (errorCode == 0) {

                    // While within the locks give back the access
                    // semaphore, read the data and potentially give back
                    // the data available semaphore
                    U_PORT_MUTEX_LOCK(gMutexQueue);

                    // NOTHING WITHIN THIS LOCK must block

                    // If the queue was closed or some such between the
                    // time we unlocked and relocked the mutex then the
                    // following line should return an error
                    errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                    if (errorCode == 0) {
                        // Now actually read the data and increment pointers
                        errorCode = queueRead(queue.queueHandle, pData);
                        if (errorCode == 0) {
                            // Space is now available for writing
                            errorCode = uPortSemaphoreGive(queue.writeSemaphore);
                        }
                    }

                    U_PORT_MUTEX_UNLOCK(gMutexQueue);
                } else {
                    // If we didn't get the access semaphore, just give the
                    // data available semaphore back
                    uPortSemaphoreGive(queue.readSemaphore);
                }
            }
        }
    }

    return errorCode;
}

// Peek the given queue.
int32_t uPortPrivateQueuePeek(int32_t handle, char *pData)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {
        errorCode =  (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pData != NULL) {

            U_PORT_MUTEX_LOCK(gMutexQueue);

            // Since there are no blocking calls here we can do
            // it all in one go within the mutex lock

            // Get a copy of the queue
            errorCode = queueGetCopy(handle, &queue);
            if (errorCode == 0) {
                // See if there is any data available right now, i.e.
                // with zero wait time
                errorCode = uPortSemaphoreTryTake(queue.readSemaphore, 0);
                if (errorCode == 0) {
                    // There is data available: copy it out and give the
                    // data available semaphore back.  No need to wait
                    // for the access semaphore as we have the mutex
                    // locked; no-one else can have got in to modify
                    // anything
                    memcpy(pData, queue.pRead, queue.itemSizeBytes);
                    errorCode = uPortSemaphoreGive(queue.readSemaphore);
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutexQueue);
        }
    }

    return errorCode;
}

// Get the number of free spaces in the given queue.
int32_t uPortPrivateQueueGetFree(int32_t handle)
{
    int32_t errorCodeOrFree =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {

        U_PORT_MUTEX_LOCK(gMutexQueue);

        // Since there are no blocking calls here we can do
        // it all in one go within the mutex lock

        // Get a copy of the queue
        errorCodeOrFree = queueGetCopy(handle, &queue);
        if (errorCodeOrFree == 0) {
            if (queue.pWrite == queue.pRead) {
                // There are two cases where the read and write pointers are equal:
                // when the queue is completely free and when it is completely full.
                // The difference is that in the full case there are no write
                // semaphores left so nothing can be written.
                // Hence if we can take the write semaphore then the queue is
                // completely free
                if (uPortSemaphoreTryTake(queue.writeSemaphore, 0) == 0) {
                    uPortSemaphoreGive(queue.writeSemaphore);
                    errorCodeOrFree = (int32_t) (queue.bufferSizeBytes / queue.itemSizeBytes);
                }
            } else if (queue.pWrite > queue.pRead) {
                errorCodeOrFree = (int32_t) ((queue.bufferSizeBytes - (queue.pWrite - queue.pRead)) /
                                             queue.itemSizeBytes);
            } else {
                errorCodeOrFree = (int32_t) ((queue.bufferSizeBytes - (queue.pRead - queue.pWrite)) /
                                             queue.itemSizeBytes);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexQueue);
    }

    return errorCodeOrFree;
}

// Remove a queue from the list.
int32_t uPortPrivateQueueRemove(int32_t handle)
{
    int32_t errorCode =  (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateQueue_t queue;

    if (gMutexQueue != NULL) {

        // Get a copy of the queue while within the locks,
        // then release them so that the entire system
        // doesn't jam up while we wait for the closing
        // semaphore
        U_PORT_MUTEX_LOCK(gMutexQueue);
        errorCode = queueGetCopy(handle, &queue);
        U_PORT_MUTEX_UNLOCK(gMutexQueue);

        if (errorCode == 0) {
            // Wait for the buffer access semaphore so that
            // we don't collide with a read or write call
            errorCode = uPortSemaphoreTake(queue.accessSemaphore);
            if (errorCode == 0) {

                // While within the locks give back the buffer access
                // semaphore and remove the queue

                U_PORT_MUTEX_LOCK(gMutexQueue);

                // NOTHING WITHIN THESE LOCKS must block

                // Just in case anything is waiting on either of the
                // read or write semaphores, give them all up.
                for (size_t x = 0; x < (queue.bufferSizeBytes / queue.itemSizeBytes); x++) {
                    uPortSemaphoreGive(queue.readSemaphore);
                    uPortSemaphoreGive(queue.writeSemaphore);
                }

                // Now release the access semaphore
                errorCode = uPortSemaphoreGive(queue.accessSemaphore);
                if (errorCode == 0) {
                    // Finally remove the queue
                    queueRemove(queue.queueHandle);
                }

                U_PORT_MUTEX_UNLOCK(gMutexQueue);
            }
        }
    }

    return errorCode;
}

// End of file
