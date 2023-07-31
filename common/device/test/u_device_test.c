/*
 * Copyright 2019-2023 u-blox
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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the device API(s).
 */


#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_assert.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"

#include "u_interface.h"
#include "u_device_serial.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_DEVICE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_DEVICE_TEST_SERIAL_SEND_SIZE_BYTES
/** The number of bytes to send in the serial device lookback test.
 */
# define U_DEVICE_TEST_SERIAL_SEND_SIZE_BYTES 10000
#endif

#ifndef U_DEVICE_TEST_SERIAL_TIME_TO_ARRIVE_MS
/** The amount of time to wait for the serial data looped-back
 * over a real UART port to arrive back, allowing laziness (e.g. on
 * a heavily loaded Windoze machine).
 */
# define U_DEVICE_TEST_SERIAL_TIME_TO_ARRIVE_MS 3000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

/** Context data for the serial device test, basically containing
 * all the non-virtual things.
 */
typedef struct {
    int32_t uartHandle;
    int32_t uart;
    int32_t baudRate;
    int32_t pinTx;
    int32_t pinRx;
    int32_t pinCts;
    int32_t pinRts;
    const char *pPrefix;
    // These only so that we can re-use the uPortUartEventCallback via
    // trampoline()
    struct uDeviceSerial_t *pDeviceSerial;
    void (*pEventCallback)(struct uDeviceSerial_t *pDeviceSerial, uint32_t eventBitMap, void *pParam);
    void *pEventCallbackParam;
} uDeviceTestSerialContext_t;

/** Type to hold the stuff that the UART test task needs to know about.
 */
typedef struct {
    size_t callCount;
    int32_t blockNumber;
    size_t indexInBlock;
    char *pReceive;
    size_t bytesReceived;
    int32_t errorCode;
    struct uDeviceSerial_t *pDeviceSerial;
} uDeviceTestSerialCallbackData_t;

#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

// The data to send during serial device testing.
static const char gSerialTestData[] =  "_____0000:0123456789012345678901234567890123456789"
                                       "01234567890123456789012345678901234567890123456789"
                                       "_____0100:0123456789012345678901234567890123456789"
                                       "01234567890123456789012345678901234567890123456789"
                                       "_____0200:0123456789012345678901234567890123456789"
                                       "01234567890123456789012345678901234567890123456789"
                                       "_____0300:0123456789012345678901234567890123456789"
                                       "01234567890123456789012345678901234567890123456789"
                                       "_____0400:0123456789012345678901234567890123456789"
                                       "01234567890123456789012345678901234567890123456789";

// A buffer to receive serial data into.
static char gSerialBuffer[U_CFG_TEST_UART_BUFFER_LENGTH_BYTES];

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: FOR THE SERIAL INTERFACE TEST
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

// Trampoline so that the function signature that uPortUartEventCallbackSet()
// uses (int32_t handle, uint32_t eventBitMap, void *pParam) can be employed
// with that which serialEventCallbackSet() uses (struct uDeviceSerial_t *pHandle,
// uint32 eventBitMap, void *pParam).
static void trampoline(int32_t handle, uint32_t eventBitMap, void *pParam)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *) pParam;

    (void) handle;

    if ((pContext != NULL) && (pContext->pEventCallback != NULL) &&
        (pContext->pDeviceSerial != NULL)) {
        pContext->pEventCallback(pContext->pDeviceSerial, eventBitMap,
                                 pContext->pEventCallbackParam);
    }
}

// Callback that is called when data arrives at the virtual serial device,
// code taken largely from uartReceivedDataCallback() over in u_port_test.c.
static void serialCallback(struct uDeviceSerial_t *pDeviceSerial,
                           uint32_t filter, void *pParameters)
{
    int32_t receiveSizeOrError;
    int32_t actualSizeOrError;
    uDeviceTestSerialCallbackData_t *pSerialCallbackData =
        (uDeviceTestSerialCallbackData_t *) pParameters;

    // In this test jig, because we are using the underlying uPortUart
    // physical UART code, we don't actually get pDeviceSerial back as the
    // first parameter at all, it is the UART handle of the physical UART;
    // to fix this we pass pDeviceSerial in via pSerialCallbackData and
    // copy the value in here.
    pDeviceSerial = pSerialCallbackData->pDeviceSerial;

    pSerialCallbackData->callCount++;
    if (filter != U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED) {
        pSerialCallbackData->errorCode = -1;
    } else {
        // Run until we spot an error or run out of data
        do {
            receiveSizeOrError = pDeviceSerial->getReceiveSize(pDeviceSerial);
            actualSizeOrError = 0;
            if (receiveSizeOrError > 0) {
                actualSizeOrError =  pDeviceSerial->read(pDeviceSerial,
                                                         pSerialCallbackData->pReceive,
                                                         gSerialBuffer + sizeof(gSerialBuffer) -
                                                         pSerialCallbackData->pReceive);
                if (actualSizeOrError < 0) {
                    pSerialCallbackData->errorCode = -2;
                }
                if (receiveSizeOrError < 0) {
                    pSerialCallbackData->errorCode = -3;
                }
                if (receiveSizeOrError > U_CFG_TEST_UART_BUFFER_LENGTH_BYTES) {
                    pSerialCallbackData->errorCode = -4;
                }
                if (actualSizeOrError > U_CFG_TEST_UART_BUFFER_LENGTH_BYTES) {
                    pSerialCallbackData->errorCode = -5;
                }
                // Compare the data with the expected data
                for (int32_t x = 0; (pSerialCallbackData->errorCode == 0) &&
                     (x < actualSizeOrError); x++) {
                    if (gSerialTestData[pSerialCallbackData->indexInBlock] ==
                        *(pSerialCallbackData->pReceive)) {
                        pSerialCallbackData->bytesReceived++;
                        pSerialCallbackData->indexInBlock++;
                        // -1 below to omit gSerialTestData string terminator
                        if (pSerialCallbackData->indexInBlock >= sizeof(gSerialTestData) - 1) {
                            pSerialCallbackData->indexInBlock = 0;
                            pSerialCallbackData->blockNumber++;
                        }
                        pSerialCallbackData->pReceive++;
                        if (pSerialCallbackData->pReceive >= gSerialBuffer + sizeof(gSerialBuffer)) {
                            pSerialCallbackData->pReceive = gSerialBuffer;
                        }
                    } else {
                        pSerialCallbackData->errorCode = -6;
                    }
                }
            }
        } while ((actualSizeOrError > 0) && (pSerialCallbackData->errorCode == 0));
    }
}

// Open a virtual serial device, mapped to a real one.
static int32_t serialOpen(struct uDeviceSerial_t *pDeviceSerial,
                          void *pReceiveBuffer, size_t receiveBufferSizeBytes)
{
    int32_t errorCode = -1;
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);

    if (pContext->pPrefix != NULL) {
        uPortUartPrefix(pContext->pPrefix);
    }
    pContext->uartHandle = uPortUartOpen(pContext->uart, pContext->baudRate,
                                         pReceiveBuffer, receiveBufferSizeBytes,
                                         pContext->pinTx, pContext->pinRx,
                                         pContext->pinCts, pContext->pinRts);
    if (pContext->uartHandle >= 0) {
        errorCode = 0;
    }

    return errorCode;
}

// Close a virtual serial device.
static void serialClose(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    uPortUartClose(pContext->uartHandle);
}

// Get the number of bytes waiting in the receive buffer of a
// virtual serial device.
static int32_t serialGetReceiveSize(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    return uPortUartGetReceiveSize(pContext->uartHandle);
}

// Read from the given virtual serial device.
static int32_t serialRead(struct uDeviceSerial_t *pDeviceSerial,
                          void *pBuffer, size_t sizeBytes)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    return uPortUartRead(pContext->uartHandle, pBuffer, sizeBytes);
}

// Write to the given virtual serial device.
static int32_t serialWrite(struct uDeviceSerial_t *pDeviceSerial,
                           const void *pBuffer, size_t sizeBytes)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    return uPortUartWrite(pContext->uartHandle, pBuffer, sizeBytes);
}

// Set an event callback on the virtual serial device.
static int32_t serialEventCallbackSet(struct uDeviceSerial_t *pDeviceSerial,
                                      uint32_t filter,
                                      void (*pFunction)(struct uDeviceSerial_t *,
                                                        uint32_t,
                                                        void *),
                                      void *pParam,
                                      size_t stackSizeBytes,
                                      int32_t priority)
{
    int32_t errorCode;
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);

    pContext->pEventCallback = pFunction;
    pContext->pEventCallbackParam = pParam;
    errorCode = uPortUartEventCallbackSet(pContext->uartHandle, filter,
                                          trampoline, pContext, stackSizeBytes,
                                          priority);
    if (errorCode != 0) {
        // Tidy up on error
        pContext->pEventCallback = NULL;
        pContext->pEventCallbackParam = NULL;
    }

    return errorCode;
}

// Remove a serial event callback.
static void serialEventCallbackRemove(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    uPortUartEventCallbackRemove(pContext->uartHandle);
    pContext->pEventCallback = NULL;
    pContext->pEventCallbackParam = NULL;
}

// Change the serial event callback filter bit-mask.
static int32_t serialEventCallbackFilterSet(struct uDeviceSerial_t *pDeviceSerial,
                                            uint32_t filter)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);
    return uPortUartEventCallbackFilterSet(pContext->uartHandle, filter);
}

// Populate the vector table.
static void interfaceSerialInit(struct uDeviceSerial_t *pDeviceSerial)
{
    uDeviceTestSerialContext_t *pContext = (uDeviceTestSerialContext_t *)
                                           pUInterfaceContext(pDeviceSerial);

    pDeviceSerial->open = serialOpen;
    pDeviceSerial->close = serialClose;
    pDeviceSerial->getReceiveSize = serialGetReceiveSize;
    pDeviceSerial->read = serialRead;
    pDeviceSerial->write = serialWrite;
    pDeviceSerial->eventCallbackSet = serialEventCallbackSet;
    pDeviceSerial->eventCallbackRemove = serialEventCallbackRemove;
    pDeviceSerial->eventCallbackFilterSet = serialEventCallbackFilterSet;

    pContext->uartHandle = -1;
    pContext->uart = U_CFG_TEST_UART_A;
    pContext->baudRate = 115200;
    pContext->pinTx = U_CFG_TEST_PIN_UART_A_TXD;
    pContext->pinRx = U_CFG_TEST_PIN_UART_A_RXD;
    pContext->pinCts = U_CFG_TEST_PIN_UART_A_CTS;
    pContext->pinRts = U_CFG_TEST_PIN_UART_A_RTS;
    pContext->pPrefix = NULL;
    pContext->pDeviceSerial = pDeviceSerial;
    pContext->pEventCallback = NULL;
    pContext->pEventCallbackParam = NULL;
}

#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

U_PORT_TEST_FUNCTION("[device]", "deviceSerial")
{
    int32_t heapUsed;
    uDeviceSerial_t *pDeviceSerial;
    uDeviceTestSerialCallbackData_t serialCallbackData = {0};
    int32_t bytesToSend;
    int32_t bytesSent = 0;

    serialCallbackData.callCount = 0;
    serialCallbackData.pReceive = gSerialBuffer;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_TEST_PRINT_LINE("testing virtual serial device.");

    uPortInit();

    // Create a virtual serial device and populate it
    // with functions which just call the real uPortUartXxx()
    // functions, and context data necessary to make those
    // functions work.
    pDeviceSerial = pUDeviceSerialCreate(interfaceSerialInit,
                                         sizeof(uDeviceTestSerialContext_t));
    U_PORT_TEST_ASSERT(pDeviceSerial != NULL);

    serialCallbackData.pDeviceSerial = pDeviceSerial;

    // Now run a UART test over the "virtual" serial device
    U_TEST_PRINT_LINE("running virtual serial using real UART...");
    U_PORT_TEST_ASSERT(pDeviceSerial->open(pDeviceSerial, NULL,
                                           U_CFG_TEST_UART_BUFFER_LENGTH_BYTES) == 0);

    // Set our event callback and filter
    U_PORT_TEST_ASSERT(pDeviceSerial->eventCallbackSet(pDeviceSerial,
                                                       (uint32_t) U_DEVICE_SERIAL_EVENT_BITMASK_DATA_RECEIVED,
                                                       serialCallback,
                                                       (void *) &serialCallbackData,
                                                       U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                                       U_CFG_OS_APP_TASK_PRIORITY + 1) == 0);

    // From here on is basically a copy of the latter half of
    // runUartTest() over in u_port_test.c

    // Send data over the serial device N times, the callback will check it
    while (bytesSent < U_DEVICE_TEST_SERIAL_SEND_SIZE_BYTES) {
        // -1 to omit the gSerialTestData string terminator
        bytesToSend = sizeof(gSerialTestData) - 1;
        if (bytesToSend > U_DEVICE_TEST_SERIAL_SEND_SIZE_BYTES - bytesSent) {
            bytesToSend = U_DEVICE_TEST_SERIAL_SEND_SIZE_BYTES - bytesSent;
        }
        U_PORT_TEST_ASSERT(pDeviceSerial->write(pDeviceSerial,
                                                gSerialTestData,
                                                bytesToSend) == bytesToSend);
        bytesSent += bytesToSend;
        U_TEST_PRINT_LINE("%d byte(s) sent.", bytesSent);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Wait long enough for everything to have been received
    uPortTaskBlock(U_DEVICE_TEST_SERIAL_TIME_TO_ARRIVE_MS);

    // Print out some useful stuff
    if (serialCallbackData.errorCode == -5) {
        U_TEST_PRINT_LINE("error after %d character(s), %d block(s).",
                          serialCallbackData.bytesReceived,
                          serialCallbackData.blockNumber);
        U_TEST_PRINT_LINE("expected %c (0x%02x), received %c (0x%02x).",
                          gSerialTestData[serialCallbackData.indexInBlock],
                          gSerialTestData[serialCallbackData.indexInBlock],
                          serialCallbackData.pReceive, serialCallbackData.pReceive);
    } else if (serialCallbackData.errorCode < 0) {
        U_TEST_PRINT_LINE("finished with error code %d after"
                          " correctly receiving %d byte(s).",
                          serialCallbackData.errorCode,
                          serialCallbackData.bytesReceived);
    }

    U_TEST_PRINT_LINE("at end of test %d byte(s) sent, %d byte(s) received.",
                      bytesSent, serialCallbackData.bytesReceived);
    U_PORT_TEST_ASSERT(serialCallbackData.bytesReceived == bytesSent);

    // Close the serial device
    pDeviceSerial->close(pDeviceSerial);

    // Delete the serial device instance
    uDeviceSerialDelete(pDeviceSerial);

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

#endif

// End of file