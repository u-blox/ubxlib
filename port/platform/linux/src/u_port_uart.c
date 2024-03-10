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
 * @brief Implementation of the port UART (i.e. COM port) API on Linux.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"

#include "fcntl.h"
#include "termios.h"
#include "unistd.h"
#include "sys/select.h"
#include "pthread.h"  // threadId
#include "sys/ioctl.h"
#include "sys/param.h"
#include "u_error_common.h"
#include "u_linked_list.h"

#include "u_cfg_os_platform_specific.h"
#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_UART_READ_WAIT_MS
/** How long to wait when there is nothing to read from the UART.
 */
# define U_PORT_UART_READ_WAIT_MS 10
#endif

#ifndef U_PORT_UART_START_STOP_WAIT_MS
/** How long to wait for a UART, mostly the read task, to start up
 * and shut down.
 */
# define U_PORT_UART_START_STOP_WAIT_MS (U_PORT_UART_READ_WAIT_MS * 10)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct uPortUartData_t {
    int32_t id;
    int uartFd;
    bool markedForDeletion;
    uPortTaskHandle_t rxTask;
    uPortMutexHandle_t mutex;
    bool bufferAllocated;
    char *pBuffer;
    size_t bufferSize;
    size_t readPos;
    size_t writePos;
    bool bufferFull;
    bool hwHandshake;
    bool handshakeSuspended;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
} uPortUartEvent_t;

/** Structure to hold a UART name prefix along with the thread
 * ID that set it; used to ensure thread-safety between calls
 * to uPortUartPrefix() and uPortUartOpen().
 */
typedef struct {
    char str[U_PORT_UART_MAX_PREFIX_LENGTH + 1]; // +1 for terminator
    pthread_t threadId;
} uPortUartPrefix_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect UART data.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Root of linked list of UART data.
 */
static uLinkedList_t *gpUartList = NULL;

/** Root of linked list of UART prefixes.
 */
static uLinkedList_t *gpUartPrefixList = NULL;

/** Variable to keep track of the number of UARTs open.
 */
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Event handler, calls the user's event callback.
static void eventHandler(void *pParam, size_t paramLength)
{
    uPortUartEvent_t *pEvent = (uPortUartEvent_t *) pParam;
    (void) paramLength;
    if (pEvent->pEventCallback != NULL) {
        pEvent->pEventCallback(pEvent->uartHandle,
                               pEvent->eventBitMap,
                               pEvent->pEventCallbackParam);
    }
}

// Task handling incoming uart data
static void readTask(void *pParam)
{
    uPortUartData_t *p = (uPortUartData_t *)pParam;
    // Need a brief pause before calling select after open.
    uPortTaskBlock(10);
    while (!p->markedForDeletion) {
        // Initiate a set for select operation.
        fd_set set;
        FD_ZERO(&set);
        FD_SET(p->uartFd, &set);
        // Select timeout
        struct timeval tv = {0};
        tv.tv_usec = U_CFG_OS_YIELD_MS * 1000;
        // Wait for input
        int res = select(p->uartFd + 1, &set, NULL, NULL, &tv);
        if (res > 0) {
            // Input is available.
            int available;
            do {
                available = 0;
                ioctl(p->uartFd, FIONREAD, &available);
                size_t cnt = 0;
                size_t tot = 0;
                // Sample readPos
                size_t readPos = p->readPos;
                U_PORT_MUTEX_LOCK(p->mutex);
                if (p->writePos >= readPos) {
                    // Write pos ahead of read. Use the remaining area in the
                    // buffer first.
                    cnt = MIN(available, p->bufferSize - p->writePos);
                    cnt = read(p->uartFd, p->pBuffer + p->writePos, cnt);
                    if (cnt > 0) {
                        available -= cnt;
                        p->writePos = (p->writePos + cnt) % p->bufferSize;
                    }
                    tot = cnt;
                }
                if ((available > 0) && (p->writePos < readPos)) {
                    // Read pos ahead of write.
                    cnt = MIN(available, readPos - p->writePos);
                    cnt = read(p->uartFd, p->pBuffer + p->writePos, cnt);
                    if (cnt > 0) {
                        available -= cnt;
                        p->writePos = (p->writePos + cnt) % p->bufferSize;
                        // We might have filled up the buffer, in which case we must wait below.
                        p->bufferFull = p->writePos == readPos;
                    }
                    tot += cnt;
                }
                U_PORT_MUTEX_UNLOCK(p->mutex);
                if ((tot > 0) &&
                    (p->eventQueueHandle >= 0)) {
                    // Call the user callback
                    uPortUartEvent_t event;
                    event.uartHandle = p->uartFd;
                    event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
                    event.pEventCallback = p->pEventCallback;
                    event.pEventCallbackParam = p->pEventCallbackParam;
                    uPortEventQueueSend(p->eventQueueHandle, &event, sizeof(event));
                }
                while (p->bufferFull && !p->markedForDeletion) {
                    // Buffer is full, wait for consumption.
                    uPortTaskBlock(U_PORT_UART_READ_WAIT_MS);
                }
            } while ((available > 0) && !p->markedForDeletion);
        }
    }
}

static uPortUartPrefix_t *pFindPrefix(pthread_t threadId)
{
    uLinkedList_t *p = gpUartPrefixList;
    while (p != NULL) {
        uPortUartPrefix_t *pUartPrefix = (uPortUartPrefix_t *)(p->p);
        if (pUartPrefix->threadId == threadId) {
            return pUartPrefix;
        }
        p = p->pNext;
    }
    return NULL;
}

static uPortUartData_t *pFindUart(int32_t handle)
{
    uLinkedList_t *p = gpUartList;
    while (p != NULL) {
        uPortUartData_t *pUart = (uPortUartData_t *)(p->p);
        if (pUart->uartFd == handle) {
            return pUart;
        }
        p = p->pNext;
    }
    return NULL;
}

static uPortUartData_t *pFindUartById(int32_t id)
{
    uLinkedList_t *p = gpUartList;
    while (p != NULL) {
        uPortUartData_t *pUart = (uPortUartData_t *)(p->p);
        if (pUart->id == id) {
            return pUart;
        }
        p = p->pNext;
    }
    return NULL;
}

static void disposeUartData(uPortUartData_t *p)
{
    if (p != NULL) {
        uLinkedListRemove(&gpUartList, p);
        if (p->rxTask != NULL) {
            uPortTaskDelete(p->rxTask);
            // Wait for the task to exit before
            // we pull the structures out from under it
            uPortTaskBlock(U_PORT_UART_START_STOP_WAIT_MS);
        }
        if (p->eventQueueHandle >= 0) {
            uPortEventQueueClose(p->eventQueueHandle);
        }
        if (p->uartFd >= 0) {
            close(p->uartFd);
        }
        if (p->pBuffer != NULL && p->bufferAllocated) {
            uPortFree(p->pBuffer);
        }
        if (p->mutex != NULL) {
            uPortMutexDelete(p->mutex);
        }
        uPortFree(p);
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
    }
}

static uint32_t suspendResumeUartHwHandshake(int32_t handle, bool suspendNotResume)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    bool enabled = false;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            struct termios options;
            tcgetattr(pUartData->uartFd, &options);
            cfmakeraw(&options);
            enabled = (options.c_cflag & CRTSCTS) == CRTSCTS;
            if (enabled) {
                if (suspendNotResume) {
                    // HW handshake was enabled and we want to suspend it
                    options.c_cflag &= ~CRTSCTS;
                    if (tcsetattr(pUartData->uartFd, TCSANOW, &options) == 0) {
                        pUartData->handshakeSuspended = true;
                    } else {
                        errorCode = U_ERROR_COMMON_PLATFORM;
                    }
                }
            } else {
                if (pUartData->handshakeSuspended && !suspendNotResume) {
                    // HW handshake isn't enabled, has been suspended,
                    // and the caller would like to resume it
                    options.c_cflag |= CRTSCTS;
                    if (tcsetattr(pUartData->uartFd, TCSANOW, &options) == 0) {
                        pUartData->handshakeSuspended = false;
                    } else {
                        errorCode = U_ERROR_COMMON_PLATFORM;
                    }
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t)errorCode;
}

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

        // First, mark all instances for deletion
        uLinkedList_t *pList = gpUartList;
        while (pList != NULL) {
            uPortUartData_t *pUart = (uPortUartData_t *)(pList->p);
            pUart->markedForDeletion = true;
            pList = pList->pNext;
        }

        // Remove any UART prefixes
        while (gpUartPrefixList != NULL) {
            uPortUartPrefix_t *pUartPrefix = (uPortUartPrefix_t *)(gpUartPrefixList->p);
            uPortFree(pUartPrefix);
            uLinkedListRemove(&gpUartPrefixList, pUartPrefix);
        }

        // Release the mutex so that deletion can occur
        U_PORT_MUTEX_UNLOCK(gMutex);

        // Now remove all existing uarts
        while (gpUartList != NULL) {
            uPortUartData_t *pUart = (uPortUartData_t *)(gpUartList->p);
            disposeUartData(pUart);
        }
        // Delete the mutex
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

int32_t uPortUartPrefix(const char *pPrefix)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if ((pPrefix != NULL) && (strlen(pPrefix) <= U_PORT_UART_MAX_PREFIX_LENGTH)) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        uPortUartPrefix_t *pUartPrefix = pUPortMalloc(sizeof(uPortUartPrefix_t));
        if (pUartPrefix != NULL) {
            U_PORT_MUTEX_LOCK(gMutex);
            // Remove any existing prefixes for this thread ID
            pthread_t threadId = pthread_self();
            uPortUartPrefix_t *p;
            while ((p = pFindPrefix(threadId)) != NULL) {
                uPortFree(p);
                uLinkedListRemove(&gpUartPrefixList, p);
            }
            // Add the new one
            strncpy(pUartPrefix->str, pPrefix, sizeof(pUartPrefix->str));
            pUartPrefix->threadId = threadId;
            uLinkedListAdd(&gpUartPrefixList, (void *)pUartPrefix);
            errorCode = U_ERROR_COMMON_SUCCESS;
            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }
    return (int32_t)errorCode;
}

// Open a UART instance.
#define FAIL(code)                  \
    {                               \
        disposeUartData(pUartData); \
        return (int32_t)code;       \
    }
int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t bufferSize,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts)
{
    if (gMutex == NULL) {
        return U_ERROR_COMMON_NOT_INITIALISED;
    }
    if (pFindUartById(uart) != NULL) {
        return (int32_t)U_ERROR_COMMON_BUSY;
    }
    uPortUartData_t *pUartData = pUPortMalloc(sizeof(uPortUartData_t));
    if (pUartData == NULL) {
        return U_ERROR_COMMON_NO_MEMORY;
    }
    memset(pUartData, 0, sizeof(uPortUartData_t));
    pUartData->uartFd = -1;
    pUartData->id = -1;
    pUartData->eventQueueHandle = -1;
    if ((pinTx >= 0) || (pinRx >= 0)) {
        FAIL(U_ERROR_COMMON_INVALID_PARAMETER);
    }
    speed_t speed;
    if (baudRate == 9600) {
        speed = B9600;
    } else if (baudRate == 19200) {
        speed = B19200;
    } else if (baudRate == 38400) {
        speed = B38400;
    } else if (baudRate == 57600) {
        speed = B57600;
    } else if (baudRate == 115200) {
        speed = B115200;
    } else if (baudRate == 230400) {
        speed = B230400;
    } else if (baudRate == 460800) {
        speed = B460800;
    } else if (baudRate == 921600) {
        speed = B921600;
    } else {
        FAIL(U_ERROR_COMMON_INVALID_PARAMETER);
    }
    char prefix[U_PORT_UART_MAX_PREFIX_LENGTH + 1]; // +1 for terminator
    char portName[U_PORT_UART_MAX_PREFIX_LENGTH + 16]; // +16 for terminator and uart index
    U_PORT_MUTEX_LOCK(gMutex);
    uPortUartPrefix_t *pUartPrefix = pFindPrefix(pthread_self());
    if (pUartPrefix != NULL) {
        strncpy(prefix, pUartPrefix->str, sizeof(prefix));
    } else {
        strncpy(prefix, U_PORT_UART_PREFIX, sizeof(prefix));
    }
    if (uart >= 0) {
        snprintf(portName, sizeof(portName), "%s%d", prefix, uart);
    } else {
        snprintf(portName, sizeof(portName), "%s", prefix);
    }
    pUartData->uartFd = open(portName, O_RDWR | (O_NOCTTY & ~O_NDELAY));
    U_PORT_MUTEX_UNLOCK(gMutex);
    if (pUartData->uartFd < 0) {
        FAIL(U_ERROR_COMMON_PLATFORM);
    }
    struct termios options;
    tcgetattr(pUartData->uartFd, &options);
    cfmakeraw(&options);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    // Let pin definition control hardware handshake
    pUartData->hwHandshake = pinCts >= 0 || pinRts >= 0;
    if (pUartData->hwHandshake) {
        options.c_cflag |= CRTSCTS;
    } else {
        options.c_cflag &= ~CRTSCTS;
    }
    // Set timed read with 100 ms timeout
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    if (tcsetattr(pUartData->uartFd, TCSANOW, &options) == 0) {
        tcflush(pUartData->uartFd, TCIOFLUSH);
    } else {
        FAIL(U_ERROR_COMMON_PLATFORM);
    }

    if (pReceiveBuffer == NULL) {
        pUartData->pBuffer = pUPortMalloc(bufferSize);
        if (pUartData->pBuffer == NULL) {
            FAIL(U_ERROR_COMMON_NO_MEMORY);
        }
        pUartData->bufferAllocated = true;
    } else {
        pUartData->pBuffer = pReceiveBuffer;
    }
    pUartData->bufferSize = bufferSize;
    pUartData->readPos = 0;
    pUartData->writePos = 0;
    pUartData->bufferFull = false;
    if (uPortMutexCreate(&(pUartData->mutex)) != 0) {
        FAIL(U_ERROR_COMMON_NO_MEMORY);
    }
    if (uPortTaskCreate(readTask, "", 4 * 1024, pUartData, U_CFG_OS_PRIORITY_MAX - 5,
                        &(pUartData->rxTask)) != 0) {
        FAIL(U_ERROR_COMMON_PLATFORM);
    }
    // Wait for the read task to start
    uPortTaskBlock(U_PORT_UART_START_STOP_WAIT_MS);
    U_PORT_MUTEX_LOCK(gMutex);
    uLinkedListAdd(&gpUartList, (void *)pUartData);
    U_PORT_MUTEX_UNLOCK(gMutex);
    U_ATOMIC_INCREMENT(&gResourceAllocCount);
    pUartData->id = uart;
    return (int32_t)(pUartData->uartFd);
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    if (gMutex != NULL) {

        uPortUartData_t *pUartData = pFindUart(handle);

        U_PORT_MUTEX_LOCK(gMutex);

        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            // Mark the UART for deletion within the mutex
            pUartData->markedForDeletion = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        if (pUartData != NULL) {
            // Actually delete the UART outside the mutex
            disposeUartData(pUartData);
        }
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        sizeOrErrorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
        U_PORT_MUTEX_LOCK(gMutex);
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            if (pUartData->bufferFull) {
                sizeOrErrorCode = pUartData->bufferSize;
            } else if (pUartData->readPos <= pUartData->writePos) {
                // Read pointer is behind write, bytes
                // received is simply the difference
                sizeOrErrorCode = pUartData->writePos - pUartData->readPos;
            } else {
                // Read pointer is ahead of write, bytes received
                // is from the read pointer up to the end of the buffer
                // then wrap around to the write pointer
                sizeOrErrorCode = (int32_t)((pUartData->bufferSize - pUartData->readPos) + pUartData->writePos);
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pBuffer != NULL) && (sizeBytes > 0) &&
            (pUartData != NULL) && !pUartData->markedForDeletion) {
            sizeOrErrorCode = 0;
            U_PORT_MUTEX_LOCK(pUartData->mutex);
            if (pUartData->readPos < pUartData->writePos) {
                // Read pointer is behind write, just take as much
                // of the difference as the user allows
                sizeOrErrorCode = MIN(pUartData->writePos - pUartData->readPos, (int32_t)sizeBytes);
                memcpy(pBuffer, pUartData->pBuffer + pUartData->readPos, sizeOrErrorCode);
                // Move the pointer on
                pUartData->readPos += sizeOrErrorCode;
            } else if ((pUartData->readPos > pUartData->writePos) || pUartData->bufferFull) {
                // Read pointer is ahead of write or buffer is full, first take up to the
                // end of the buffer as far as the user allows
                size_t cnt = MIN(pUartData->bufferSize - pUartData->readPos, (int32_t)sizeBytes);
                memcpy(pBuffer, pUartData->pBuffer + pUartData->readPos, cnt);
                pBuffer = (char *) pBuffer + cnt;
                sizeBytes -= cnt;
                sizeOrErrorCode = (int32_t)cnt;
                // Move the read pointer on, wrapping as necessary
                pUartData->readPos = (pUartData->readPos + cnt) % pUartData->bufferSize;
                // If there is still room in the user buffer then
                // carry on taking up to the write pointer
                if (sizeBytes > 0) {
                    cnt = MIN(pUartData->writePos, sizeBytes);
                    memcpy(pBuffer, pUartData->pBuffer, cnt);
                    sizeOrErrorCode += cnt;
                    // Move the read pointer on
                    pUartData->readPos += cnt;
                }
            }
            if (pUartData->bufferFull && (sizeOrErrorCode > 0)) {
                // Release possible waiting read task.
                pUartData->bufferFull = false;
            }
            U_PORT_MUTEX_UNLOCK(pUartData->mutex);
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pBuffer != NULL) && (sizeBytes > 0) &&
            (pUartData != NULL) && !pUartData->markedForDeletion) {
            sizeOrErrorCode = write(pUartData->uartFd, pBuffer, sizeBytes);
            if (sizeOrErrorCode < 0) {
                sizeOrErrorCode = (int32_t)U_ERROR_COMMON_PLATFORM;
            }
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
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
            (filter != 0) && (pFunction != NULL)) {
            // Open an event queue to eventHandler()
            // which will receive uPortUartEvent_t
            // and give it a useful name for debug purposes
            char name[20];
            snprintf(name, sizeof(name), "eventUart%d", (int)handle);
            errorCode = uPortEventQueueOpen(eventHandler, name,
                                            sizeof(uPortUartEvent_t),
                                            stackSizeBytes,
                                            priority,
                                            U_PORT_UART_EVENT_QUEUE_SIZE);
            if (errorCode >= 0) {
                pUartData->eventQueueHandle = (int32_t) errorCode;
                pUartData->eventFilter = filter;
                pUartData->pEventCallback = pFunction;
                pUartData->pEventCallbackParam = pParam;
                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) errorCode;
}

// Remove an event callback.
void uPortUartEventCallbackRemove(int32_t handle)
{
    int32_t eventQueueHandle = -1;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
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

// Get the callback filter bit-mask.
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    uint32_t filter = 0;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            filter = pUartData->eventFilter;
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
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((filter != 0) && (pUartData != NULL) &&
            !pUartData->markedForDeletion) {
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
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
            (pUartData->eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
            uPortUartEvent_t event;
            event.uartHandle = handle;
            event.eventBitMap = eventBitMap;
            event.pEventCallback = pUartData->pEventCallback;
            event.pEventCallbackParam = pUartData->pEventCallbackParam;
            errorCode = uPortEventQueueSend(pUartData->eventQueueHandle,
                                            &event, sizeof(event));
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t) errorCode;
}

// Send an event to the callback, non-blocking version.
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    (void) handle;
    (void) eventBitMap;
    (void) delayMs;
    // Not supported on Linux since uPortEventQueueSendIrq() is
    // not supported.
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Return true if we're in an event callback.
bool uPortUartEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
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
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
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
    bool rtsFlowControlIsEnabled = false;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        uPortUartData_t *pUartData = pFindUart(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            rtsFlowControlIsEnabled = pUartData->hwHandshake || pUartData->handshakeSuspended;
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    return uPortUartIsRtsFlowControlEnabled(handle);
}

// Suspend CTS flow control.
int32_t uPortUartCtsSuspend(int32_t handle)
{
    return suspendResumeUartHwHandshake(handle, false);
}

// Resume CTS flow control.
void uPortUartCtsResume(int32_t handle)
{
    suspendResumeUartHwHandshake(handle, true);
}

// Get the number of UART interfaces currently open.
int32_t uPortUartResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
