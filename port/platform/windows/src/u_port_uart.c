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
 * @brief Implementation of the port UART (i.e. COM port) API on Windows.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()

#include "windows.h"

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"
#include "u_port_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_UART_MAX_COM_PORT_NAME_BUFFER_LENGTH
/** The size of buffer required to contain a COM port name string,
 * of the form "\\.\COMxxx". This length INCLUDES the terminator.
 */
# define U_PORT_UART_MAX_COM_PORT_NAME_BUFFER_LENGTH 12
#endif

#ifndef U_PORT_UART_READ_TIMEOUT_MS
/** The read timeout to set on the COM ports.
 */
# define U_PORT_UART_READ_TIMEOUT_MS 50
#endif

#ifndef U_PORT_UART_TIMER_POLL_TIME_MS
/** Poll every 10 milliseconds to catch anything we might have missed.
 */
# define U_PORT_UART_TIMER_POLL_TIME_MS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per UART in
 * a linked list.
 */
typedef struct uPortUartData_t {
    int32_t uartHandle;
    bool markedForDeletion;
    char nameStr[U_PORT_UART_MAX_COM_PORT_NAME_BUFFER_LENGTH];
    HANDLE windowsUartHandle;
    HANDLE waitCommEventThreadHandle;
    HANDLE waitCommEventThreadReadyHandle;
    HANDLE waitCommEventThreadTerminateHandle;
    bool rxBufferIsMalloced;
    size_t rxBufferSizeBytes;
    char *pRxBufferStart;
    volatile char *pRxBufferRead;
    volatile char *pRxBufferWrite;
    bool ctsFlowControlSuspended;
    int32_t eventQueueHandle;
    uint32_t eventFilter;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
    struct uPortUartData_t *pNext;
} uPortUartData_t;

/** Structure describing an event.
 */
typedef struct {
    int32_t uartHandle;
    uint32_t eventBitMap;
    void (*pEventCallback)(int32_t, uint32_t, void *);
    void *pEventCallbackParam;
} uPortUartEvent_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect UART data.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Root of linked list of UART data.
 */
static uPortUartData_t *gpUartListRoot = NULL;

/** The next UART handle to use.
 */
static int32_t gUartHandleNext = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a UART in the list by handle.
// gMutex should be locked before this is called.
static uPortUartData_t *pUartGetByHandle(int32_t handle)
{
    uPortUartData_t *pTmp = gpUartListRoot;

    while ((pTmp != NULL) && (pTmp->uartHandle != handle)) {
        pTmp = pTmp->pNext;
    }

    return pTmp;
}

// Find a UART in the list by name.
// gMutex should be locked before this is called.
static uPortUartData_t *pUartGetByName(const char *pNameStr)
{
    uPortUartData_t *pTmp = gpUartListRoot;

    while ((pTmp != NULL) &&
           (strncmp(pTmp->nameStr, pNameStr, sizeof(pTmp->nameStr)) != 0)) {
        pTmp = pTmp->pNext;
    }

    return pTmp;
}

// Add a UART to the list, populating its UART handle and returning a
// pointer to it.
// gMutex should be locked before this is called.
static uPortUartData_t *pUartAdd()
{
    uPortUartData_t *pTmp;
    bool success = true;
    int32_t x;

    pTmp = (uPortUartData_t *) pUPortMalloc(sizeof(uPortUartData_t));
    if (pTmp != NULL) {
        memset(pTmp, 0, sizeof(*pTmp));
        pTmp->eventQueueHandle = -1;
        pTmp->uartHandle = -1;
        pTmp->windowsUartHandle = INVALID_HANDLE_VALUE;
        pTmp->waitCommEventThreadHandle = INVALID_HANDLE_VALUE;
        pTmp->waitCommEventThreadReadyHandle = INVALID_HANDLE_VALUE;
        pTmp->waitCommEventThreadTerminateHandle = INVALID_HANDLE_VALUE;
        pTmp->pNext = NULL;
        // Get the next UART handle
        x = gUartHandleNext;
        while ((pUartGetByHandle(gUartHandleNext) != NULL) && success) {
            gUartHandleNext++;
            if (gUartHandleNext < 0) {
                gUartHandleNext = 0;
            }
            if (gUartHandleNext == x) {
                // Looped
                success = false;
            }
        }
        if (success) {
            pTmp->uartHandle = gUartHandleNext;
            pTmp->pNext = gpUartListRoot;
            gpUartListRoot = pTmp;
        } else {
            // Clean up
            uPortFree(pTmp);
            pTmp = NULL;
        }
    }

    return pTmp;
}

// Remove a pipe from the list of pipes.
// gMutex should be locked before this is called.
static void uartRemove(const uPortUartData_t *pUartData)
{
    uPortUartData_t *pTmp = gpUartListRoot;
    uPortUartData_t *pPrevious = NULL;

    while (pTmp != NULL) {
        if (pTmp == pUartData) {
            if (pPrevious == NULL) {
                // At head
                gpUartListRoot = pTmp->pNext;
            } else {
                pPrevious->pNext = pTmp->pNext;
            }
            uPortFree(pTmp);
            // Force exit
            pTmp = NULL;
        } else {
            pPrevious = pTmp;
            pTmp = pTmp->pNext;
        }
    }
}

// Close a UART.
// !!! gMutex should NOT be locked when this is called !!!
static void uartCloseRequiresMutex(uPortUartData_t *pUartData)
{
    // Set the terminate event and wait for the waitCommEvent
    // thread to exit
    SetEvent(pUartData->waitCommEventThreadTerminateHandle);
    WaitForSingleObject(pUartData->waitCommEventThreadHandle, INFINITE);
    // Close the ready and terminate events
    CloseHandle(pUartData->waitCommEventThreadReadyHandle);
    CloseHandle(pUartData->waitCommEventThreadTerminateHandle);
    // Remove the callback if there is one
    if (pUartData->eventQueueHandle >= 0) {
        uPortEventQueueClose(pUartData->eventQueueHandle);
    }

    // Now lock the mutex for the remaining bits
    U_PORT_MUTEX_LOCK(gMutex);

    if (pUartData->rxBufferIsMalloced) {
        // Free the buffer
        uPortFree(pUartData->pRxBufferStart);
    }
    // Remove the UART itself
    CloseHandle(pUartData->windowsUartHandle);
    // And then take it out of the list
    uartRemove(pUartData);

    U_PORT_MUTEX_UNLOCK(gMutex);
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

    if (pEvent->pEventCallback != NULL) {
        pEvent->pEventCallback(pEvent->uartHandle,
                               pEvent->eventBitMap,
                               pEvent->pEventCallbackParam);
    }
}

// Handle a UART event, called by waitCommEventThread().
// Returns the system error code that the attempt to read the
// UART results in, usually either:
// ERROR_SUCCESS (0)
// ERROR_TIMEOUT (1460)
static DWORD handleThreadUartEvent(uPortUartData_t *pUartData,
                                   DWORD uartEvent)
{
    uPortUartEvent_t event;
    OVERLAPPED overlap;
    DWORD bytesRead;
    DWORD totalSize = 0;
    DWORD lastErrorCode = -1;
    const volatile char *pRxBufferRead;
    int32_t spaceAvailable;

    if (uartEvent & EV_RXCHAR) {
        // There's received data, go get it
        memset(&overlap, 0, sizeof(overlap));
        overlap.hEvent = CreateEvent(NULL, true, false, NULL);
        if (overlap.hEvent != INVALID_HANDLE_VALUE) {
            do {
                // Work out how much linear space we have
                // free in the buffer
                pRxBufferRead = pUartData->pRxBufferRead;
                if (pUartData->pRxBufferWrite >= pRxBufferRead) {
                    //        |              rxBufferSizeBytes          |
                    //        |---------------|-----------|----- X -----|
                    //        ^               ^           ^
                    //        |               |           |
                    // pRxBufferStart pRxBufferRead pRxBufferWrite
                    //
                    // Write pointer is at or ahead of the read pointer,
                    // bytes available, X, are from the write pointer
                    // up to the end of the buffer but we also need to
                    // make sure that wouldn't cause the pointers to
                    // catch up
                    spaceAvailable = pUartData->pRxBufferStart +
                                     (pUartData->rxBufferSizeBytes) -
                                     pUartData->pRxBufferWrite;
                    if ((spaceAvailable > 0) &&
                        (pRxBufferRead == pUartData->pRxBufferStart)) {
                        spaceAvailable--;
                    }
                } else {
                    //        |              rxBufferSizeBytes          |
                    //        |---------------|-----X-----|-------------|
                    //        ^               ^           ^
                    //        |               |           |
                    // pRxBufferStart pRxBufferWrite pRxBufferRead
                    //
                    // Write pointer is behind read, bytes available, X, is
                    // simply the difference, -1 so that they don't catch up
                    spaceAvailable = (pRxBufferRead - pUartData->pRxBufferWrite) - 1;
                }

                // Now read up to that amount of data or until
                // we hit the read COMM timeout
                bytesRead = 0;
                lastErrorCode = ERROR_SUCCESS;
                if (!ReadFile(pUartData->windowsUartHandle,
                              pUartData->pRxBufferWrite,
                              spaceAvailable, &bytesRead, &overlap)) {
                    lastErrorCode = GetLastError();
                    if (lastErrorCode == ERROR_IO_PENDING) {
                        GetOverlappedResult(pUartData->windowsUartHandle,
                                            &overlap, &bytesRead, true);
                        lastErrorCode = GetLastError();
                    }
                }
                // Move the write pointer on
                pUartData->pRxBufferWrite += bytesRead;
                totalSize += bytesRead;
                if (pUartData->pRxBufferWrite >= pUartData->pRxBufferStart +
                    pUartData->rxBufferSizeBytes) {
                    pUartData->pRxBufferWrite = pUartData->pRxBufferStart;
                }
                // Keep reading while there is stuff to read
            } while (bytesRead > 0);
        } else {
            lastErrorCode = GetLastError() ;
        }

        CloseHandle(overlap.hEvent);
    }

    if ((totalSize > 0) &&
        (pUartData->eventQueueHandle >= 0)) {
        // Call the user callback
        event.uartHandle = pUartData->uartHandle;
        event.eventBitMap = U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED;
        event.pEventCallback = pUartData->pEventCallback;
        event.pEventCallbackParam = pUartData->pEventCallbackParam;
        uPortEventQueueSend(pUartData->eventQueueHandle, &event, sizeof(event));
    }

    return lastErrorCode;
}

// Event thread used by all UARTs for COM events.
static int32_t waitCommEventThread(void *pParam)
{
    uPortUartData_t *pUartData;
    OVERLAPPED overlap;
    int32_t uartHandle;
    HANDLE windowsUartHandle = INVALID_HANDLE_VALUE;
    HANDLE windowsTimerHandle = INVALID_HANDLE_VALUE;
    HANDLE readyHandle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER timerDueTime;
    HANDLE eventHandles[3];
    DWORD uartEvent;
    DWORD x;
    bool keepGoing = true;

    if (gMutex != NULL) {
        // The parameter passed to us is actually the UART (non-windows) handle
        uartHandle = (int32_t) pParam;
        for (size_t x = 0; x < sizeof(eventHandles) / sizeof(eventHandles[0]); x++) {
            eventHandles[x] = INVALID_HANDLE_VALUE;
        }

        pUartData = pUartGetByHandle(uartHandle);
        if (pUartData != NULL) {
            // First item in the array is the event thread terminate event,
            // don't want to miss that
            eventHandles[0] = pUartData->waitCommEventThreadTerminateHandle;
            // Need the ready handle
            readyHandle = pUartData->waitCommEventThreadReadyHandle;
            // Also need the Windows handle of the UART
            windowsUartHandle = pUartData->windowsUartHandle;
        }

        // Create an event to capture stuff going-on on the serial port
        // The event is used by WaitCommEvent()
        eventHandles[1] = CreateEvent(NULL, true, false, NULL);
        if (eventHandles[1] != INVALID_HANDLE_VALUE) {
            // Put the event into the overlap structure so that it can
            // be passed to WaitCommEvent()
            memset(&overlap, 0, sizeof(overlap));
            overlap.hEvent = eventHandles[1];
            // Finally, last in the array, add a timer that we can
            // use to periodically poll the UART for received data
            // in case we were unable to process any events (e.g.
            // if our  buffer was full at the time).
            eventHandles[2] = CreateWaitableTimer(NULL, false, NULL);
            if (eventHandles[2] != INVALID_HANDLE_VALUE) {
                // Start the periodic timer, a relative timeout value
                // that is in units of 100 nanosecond intervals in the future
                timerDueTime.QuadPart = -U_PORT_UART_TIMER_POLL_TIME_MS * 10;
                if (SetWaitableTimer(eventHandles[2], &timerDueTime,
                                     U_PORT_UART_TIMER_POLL_TIME_MS, NULL, NULL, 0)) {
                    // Now we can wait for events on those handles
                    SetEvent(readyHandle);
                    while (keepGoing) {
                        if (WaitCommEvent(windowsUartHandle, &uartEvent, &overlap)) {
                            // An event has already occurred, handle it
                            // However, I have seen this return that there is a character
                            // in the buffer and then any attempt to read the buffer result
                            // in timeout, which means the code gets stuck here because
                            // it never processes any of the events in the section below.
                            // Hence if handleThreadUartEvent() doesn't return
                            // ERROR_SUCCESS then we move to waiting on events
                            x = handleThreadUartEvent(pUartData, uartEvent);
                        } else {
                            x = GetLastError();
                        }
                        if (x != ERROR_SUCCESS) {
                            // WaitCommEvent will now signal us with the
                            // eventHandles[1] flag when something has happened
                            // while a terminate event might arrive on eventHandles[0]
                            // and the periodic poll timer on eventHandles[2]
                            x = WaitForMultipleObjects(sizeof(eventHandles) / sizeof(eventHandles[0]),
                                                       eventHandles,
                                                       false, INFINITE);
                            switch (x) {
                                case WAIT_OBJECT_0 + 0:
                                    // Terminate thread was signalled
                                    keepGoing = false;
                                    break;
                                case WAIT_OBJECT_0 + 1:
                                    // UART event was signaled, go get it
                                    // Have to provide x but it is not used
                                    if (GetOverlappedResult(windowsUartHandle, &overlap, &x, true)) {
                                        handleThreadUartEvent(pUartData, uartEvent);
                                    }
                                    break;
                                case WAIT_OBJECT_0 + 2:
                                    // Periodic timer has gone off, do a read
                                    handleThreadUartEvent(pUartData, EV_RXCHAR);
                                    break;
                                default:
                                    // Ignore and continue
                                    break;
                            }
                        }
                    }

                    // Stop the periodic timer
                    CancelWaitableTimer(eventHandles[2]);
                }
            }
        }

        // Close the waitable timer handle and the UART event handle
        CloseHandle(eventHandles[2]);
        CloseHandle(eventHandles[1]);
    }

    ExitThread(0);
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
    uPortUartData_t *pTmp = gpUartListRoot;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // First, mark all instances for deletion
        while (pTmp != NULL) {
            pTmp->markedForDeletion = true;
            pTmp = pTmp->pNext;
        }

        // Release the mutex so that deletion can occur
        U_PORT_MUTEX_UNLOCK(gMutex);

        // Now close all the UART instances
        while (gpUartListRoot != NULL) {
            uartCloseRequiresMutex(gpUartListRoot);
        }

        // Delete the mutex
        U_PORT_MUTEX_LOCK(gMutex);
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
    uPortUartData_t *pUartData;
    char nameStr[U_PORT_UART_MAX_COM_PORT_NAME_BUFFER_LENGTH];
    DCB dcb;
    COMMTIMEOUTS timeouts;

    // TX/RX pins are managed by Windows
    (void) pinTx;
    (void) pinRx;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        snprintf(nameStr, sizeof(nameStr), "\\\\.\\COM%d", uart);
        handleOrErrorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        if ((uart >= 0) && (baudRate > 0) && (pUartGetByName(nameStr) == NULL) &&
            (receiveBufferSizeBytes > 0)) {
            handleOrErrorCode = U_ERROR_COMMON_NO_MEMORY;
            pUartData = pUartAdd();
            if (pUartData != NULL) {
                pUartData->markedForDeletion = false;
                pUartData->pRxBufferStart = (char *) pReceiveBuffer;
                if (pUartData->pRxBufferStart == NULL) {
                    // Malloc memory for the read buffer
                    pUartData->pRxBufferStart = (char *) pUPortMalloc(receiveBufferSizeBytes);
                    pUartData->rxBufferIsMalloced = true;
                }
                if (pUartData->pRxBufferStart != NULL) {
                    pUartData->rxBufferSizeBytes = receiveBufferSizeBytes;
                    pUartData->pRxBufferRead = pUartData->pRxBufferStart;
                    pUartData->pRxBufferWrite = pUartData->pRxBufferStart;
                    // Now do the platform stuff
                    handleOrErrorCode = U_ERROR_COMMON_PLATFORM;
                    strncpy(pUartData->nameStr, nameStr, sizeof(pUartData->nameStr));
                    pUartData->windowsUartHandle = CreateFile(pUartData->nameStr,
                                                              GENERIC_READ | GENERIC_WRITE,
                                                              0, NULL, OPEN_EXISTING,
                                                              FILE_FLAG_OVERLAPPED, NULL);
                    if (pUartData->windowsUartHandle != INVALID_HANDLE_VALUE) {
                        // Now configure it
                        memset(&dcb, 0, sizeof(dcb));
                        dcb.DCBlength = sizeof(DCB);
                        // Retrieve current settings.
                        if (GetCommState(pUartData->windowsUartHandle, &dcb)) {
                            // Set it up as required
                            dcb.BaudRate = baudRate;
                            dcb.ByteSize = 8;             //  data size, xmit and rcv
                            dcb.Parity = NOPARITY;        //  parity bit
                            dcb.StopBits = ONESTOPBIT;    //  stop bit
                            dcb.fDtrControl = DTR_CONTROL_DISABLE;
                            dcb.fOutxCtsFlow = 0;
                            // On windows the CTS pin is simply a flag
                            // indicating whether CTS flow control should be on
                            if (pinCts >= 0) {
                                dcb.fOutxCtsFlow = 1;
                            }
                            dcb.fRtsControl = RTS_CONTROL_ENABLE;
                            // Likewise, the RTS pin is simply a flag
                            // indicating whether RTS flow control should be on,
                            // but in this case we default-it to "enable" in
                            // order to make sure that, if flow control
                            // pin is actually connected through to the module,
                            // it is able to send us data
                            if (pinRts >= 0) {
                                dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
                            }
                            if (SetCommState(pUartData->windowsUartHandle, &dcb)) {
                                // Set the timeouts: no timeout in the write case,
                                // i.e. write is blocking, read timeout is set to
                                // U_PORT_UART_READ_TIMEOUT_MS
                                memset (&timeouts, 0, sizeof(timeouts));
                                timeouts.ReadIntervalTimeout = MAXDWORD ;
                                timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
                                timeouts.ReadTotalTimeoutConstant = U_PORT_UART_READ_TIMEOUT_MS;
                                if (SetCommTimeouts(pUartData->windowsUartHandle, &timeouts)) {
                                    // Create an event that lets us know the WaitCom event
                                    // thread is ready
                                    pUartData->waitCommEventThreadReadyHandle = CreateEvent(NULL, true,
                                                                                            false, NULL);
                                    if (pUartData->waitCommEventThreadReadyHandle != INVALID_HANDLE_VALUE) {
                                        // Create an event that can terminate our WaitCom event thread
                                        pUartData->waitCommEventThreadTerminateHandle = CreateEvent(NULL,
                                                                                                    true,
                                                                                                    false,
                                                                                                    NULL);
                                        if (pUartData->waitCommEventThreadTerminateHandle != INVALID_HANDLE_VALUE) {
                                            // ...then create the WaitCom thread
                                            pUartData->waitCommEventThreadHandle = CreateThread(NULL, 0,
                                                                                                (LPTHREAD_START_ROUTINE) waitCommEventThread,
                                                                                                (PVOID) pUartData->uartHandle,
                                                                                                0, NULL);
                                            if (pUartData->waitCommEventThreadHandle != INVALID_HANDLE_VALUE) {
                                                // Now mask-in the receive event flag as that's the only
                                                // one we support/care about
                                                if (SetCommMask(pUartData->windowsUartHandle, EV_RXCHAR)) {
                                                    // Done!
                                                    handleOrErrorCode = pUartData->uartHandle;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (handleOrErrorCode < 0) {
                    // Clean up
                    if (pUartData->waitCommEventThreadHandle != INVALID_HANDLE_VALUE) {
                        SetEvent(pUartData->waitCommEventThreadTerminateHandle);
                        WaitForSingleObject(pUartData->waitCommEventThreadHandle, INFINITE);
                    }
                    CloseHandle(pUartData->waitCommEventThreadReadyHandle);
                    CloseHandle(pUartData->waitCommEventThreadTerminateHandle);
                    CloseHandle(pUartData->windowsUartHandle);
                    if (pUartData->rxBufferIsMalloced) {
                        uPortFree(pUartData->pRxBufferStart);
                    }
                    uartRemove(pUartData);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    if (handleOrErrorCode >= 0) {
        // Wait for the WaitCom thread to be ready before continuing
        WaitForSingleObject(pUartData->waitCommEventThreadReadyHandle, INFINITE);
    }

    return (int32_t) handleOrErrorCode;
}

// Close a UART instance.
void uPortUartClose(int32_t handle)
{
    uPortUartData_t *pUartData = NULL;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            // Mark the UART for deletion within the mutex
            pUartData->markedForDeletion = true;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);

        if (pUartData != NULL) {
            // Actually delete the UART outside the mutex
            uartCloseRequiresMutex(pUartData);
        }
    }
}

// Get the number of bytes waiting in the receive buffer.
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortUartData_t *pUartData;
    const volatile char *pRxBufferWrite;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
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

    return sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    size_t thisSize;
    uPortUartData_t *pUartData;
    const volatile char *pRxBufferWrite;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
        if ((pBuffer != NULL) && (sizeBytes > 0) &&
            (pUartData != NULL) && !pUartData->markedForDeletion) {
            sizeOrErrorCode = 0;
            pRxBufferWrite = pUartData->pRxBufferWrite;
            if (pUartData->pRxBufferRead < pRxBufferWrite) {
                // Read pointer is behind write, just take as much
                // of the difference as the user allows
                sizeOrErrorCode = pRxBufferWrite - pUartData->pRxBufferRead;
                if (sizeOrErrorCode > (int32_t) sizeBytes) {
                    sizeOrErrorCode = sizeBytes;
                }
                memcpy(pBuffer, pUartData->pRxBufferRead,
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
                memcpy(pBuffer, pUartData->pRxBufferRead, thisSize);
                pBuffer = (char *) pBuffer + thisSize;
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
                    memcpy(pBuffer, pUartData->pRxBufferRead, thisSize);
                    pBuffer = (char *) pBuffer + thisSize;
                    sizeBytes -= thisSize;
                    sizeOrErrorCode += thisSize;
                    // Move the read pointer on
                    pUartData->pRxBufferRead += thisSize;
                }
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
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    OVERLAPPED overlap;
    DWORD bytesWritten = 0;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
        if ((pBuffer != NULL) && (sizeBytes > 0) &&
            (pUartData != NULL) && !pUartData->markedForDeletion) {
            sizeOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            memset(&overlap, 0, sizeof(overlap));
            overlap.hEvent = CreateEvent(NULL, true, false, NULL);
            if (overlap.hEvent != INVALID_HANDLE_VALUE) {
                if (WriteFile(pUartData->windowsUartHandle, pBuffer,
                              sizeBytes, &bytesWritten, &overlap) ||
                    ((GetLastError() == ERROR_IO_PENDING) &&
                     GetOverlappedResult(pUartData->windowsUartHandle,
                                         &overlap, &bytesWritten, true))) {
                    sizeOrErrorCode = (int32_t) bytesWritten;
                }
            }

            CloseHandle(overlap.hEvent);
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
        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
            (filter != 0) && (pFunction != NULL)) {
            // Open an event queue to eventHandler()
            // which will receive uPortUartEvent_t
            // and give it a useful name for debug purposes
            snprintf(name, sizeof(name), "eventCOM%d", (int) handle);
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
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
    uPortUartData_t *pUartData;
    uPortUartEvent_t event;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion &&
            (pUartData->eventQueueHandle >= 0) &&
            // The only event we support right now
            (eventBitMap == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
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

    // Not supported on Windows since uPortEventQueueSendIrq() is
    // not supported.

    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Return true if we're in an event callback.
bool uPortUartEventIsCallback(int32_t handle)
{
    bool isEventCallback = false;
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
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
    uPortUartData_t *pUartData;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
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
    uPortUartData_t *pUartData;
    DCB dcb;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            memset(&dcb, 0, sizeof(dcb));
            dcb.DCBlength = sizeof(DCB);
            // Retrieve settings.
            if (GetCommState(pUartData->windowsUartHandle, &dcb) &&
                (dcb.fRtsControl == RTS_CONTROL_HANDSHAKE)) {
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
    bool ctsFlowControlIsEnabled = false;
    uPortUartData_t *pUartData = NULL;
    DCB dcb;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && !pUartData->markedForDeletion) {
            memset(&dcb, 0, sizeof(dcb));
            dcb.DCBlength = sizeof(DCB);
            // Retrieve settings.
            if (GetCommState(pUartData->windowsUartHandle, &dcb) &&
                (dcb.fOutxCtsFlow != 0)) {
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
    uPortUartData_t *pUartData = NULL;
    DCB dcb;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pUartData = pUartGetByHandle(handle);
        if (pUartData != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (!pUartData->ctsFlowControlSuspended) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                memset(&dcb, 0, sizeof(dcb));
                dcb.DCBlength = sizeof(DCB);
                // Retrieve settings
                if (GetCommState(pUartData->windowsUartHandle, &dcb)) {
                    if (dcb.fOutxCtsFlow == 0) {
                        // Nothing to to
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        // Switch CTS off
                        dcb.fOutxCtsFlow = 0;
                        if (SetCommState(pUartData->windowsUartHandle, &dcb)) {
                            pUartData->ctsFlowControlSuspended = true;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
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
    uPortUartData_t *pUartData = NULL;
    DCB dcb;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pUartData = pUartGetByHandle(handle);
        if ((pUartData != NULL) && (pUartData->ctsFlowControlSuspended)) {
            memset(&dcb, 0, sizeof(dcb));
            dcb.DCBlength = sizeof(DCB);
            // Retrieve settings
            if (GetCommState(pUartData->windowsUartHandle, &dcb)) {
                if (dcb.fOutxCtsFlow == 0) {
                    dcb.fOutxCtsFlow = 1;
                    if (SetCommState(pUartData->windowsUartHandle, &dcb)) {
                        pUartData->ctsFlowControlSuspended = false;
                    }
                } else {
                    pUartData->ctsFlowControlSuspended = false;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// End of file
