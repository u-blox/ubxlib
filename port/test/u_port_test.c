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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Test for the port API: these should pass on all platforms.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "stdlib.h"    // rand()
#include "string.h"    // strtok() and strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_clib_platform_specific.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The queue length to create during testing.
 */
#define U_PORT_TEST_QUEUE_LENGTH 20

/** The size of each item on the queue during testing.
 */
#define U_PORT_TEST_QUEUE_ITEM_SIZE sizeof(int32_t)

/** The guard time for the OS test.
 */
#define U_PORT_TEST_OS_GUARD_DURATION_MS 2200

/** The task block duration to use in testing the
 * time for which a block lasts.  This needs to
 * be quite long as any error must be visible
 * in the test duration as measured by the
 * test system which is logging the test output.
 */
#define U_PORT_TEST_OS_BLOCK_TIME_MS 5000

/** Tolerance on block time.  Note that this needs
 * to be large enough to account for the tick coarseness
 * on all platforms.  For instance, on ESP32 the default
 * tick is 100 ms.
 */
#define U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS 150

/** The number of re-entrancy test tasks to run.
 */
#define U_PORT_TEST_OS_NUM_REENT_TASKS 3

/** Fill value for the heap.
 */
#define U_PORT_TEST_OS_MALLOC_FILL ((int32_t) 0xdeadbeef)

/** The amount of memory to malloc()ate during re-entrancy
 * testing.
  */
#define U_PORT_TEST_OS_MALLOC_SIZE_INTS ((int32_t) (1024 / sizeof(int32_t)))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) && \
    (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)
/** Type to hold the stuff that the UART test task needs to know
 * about.
 */
typedef struct {
    uPortMutexHandle_t runningMutexHandle;
    uPortQueueHandle_t uartQueueHandle;
    uPortQueueHandle_t controlQueueHandle;
} uartTestTaskData_t;

#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// OS test mutex handle.
static uPortMutexHandle_t gMutexHandle = NULL;

// OS test queue handle for data.
static uPortQueueHandle_t gQueueHandleData = NULL;

// OS test queue handle for control.
static uPortQueueHandle_t gQueueHandleControl = NULL;

// OS test task handle.
static uPortTaskHandle_t gTaskHandle = NULL;

// OS task parameter.
static char gTaskParameter[6];

// Stuff to send to the OS test task, must all be positive numbers.
static const int32_t gStuffToSend[] = {0, 100, 25, 3};

// Flag for re-entrancy testing, wait for start.
static bool gWaitForGo;

// Flag for re-entrancy testing, wait for delete.
static bool gWaitForStop;

#if (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) && \
    (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)

// The number of bytes correctly received during UART testing.
static int32_t gUartBytesReceived = 0;

// The data to send during UART testing.
static const char gUartTestData[] =  "_____0000:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0100:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0200:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0300:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0400:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0500:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0600:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0700:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0800:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789"
                                     "_____0900:0123456789012345678901234567890123456789"
                                     "01234567890123456789012345678901234567890123456789";

// A buffer to receive UART data into:
// deliberately a non-integer divisor of
// U_PORT_UART_RX_BUFFER_SIZE
// so that the buffers go "around the corner"
static char gUartBuffer[(U_PORT_UART_RX_BUFFER_SIZE / 2) +
                                                         (U_PORT_UART_RX_BUFFER_SIZE / 4)];

#endif // (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) &&
// (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// The test task for re-entrancy.
// The parameter is a pointer to an integer which, on arrival,
// is a unique non-zero index that the task can identify itself
// by and, on return, should be set to zero for success, negative
// for error.
static void osReentTask(void *pParameter)
{
    int32_t index = *((int32_t *) pParameter) & 0xFF;
    int32_t *pMem;
    int32_t *pTmp;
    int32_t returnCode = 0;
    char *pStr;
    char str[6];
    char buffer[32];
    uint32_t a = 0;
    int32_t b = 0;
    char c = '0';
    int32_t checkInt = ((U_PORT_TEST_OS_MALLOC_FILL) & ~0xFF) | index;
    int32_t mallocSizeInts = 1 + (rand() % (U_PORT_TEST_OS_MALLOC_SIZE_INTS - 1));

    // Wait for it...
    while (gWaitForGo) {
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Malloc a random amount of memory and fill it with
    // a known value unique to this task, yielding while
    // doing it so that others can get in and mess it up
    pMem = (int32_t *) malloc(mallocSizeInts * sizeof(int32_t));
    uPortTaskBlock(U_CFG_OS_YIELD_MS);
    if (pMem != NULL) {
        pTmp = pMem;
        for (int32_t x = 0; x < mallocSizeInts; x++) {
            *pTmp = checkInt;
            pTmp++;
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }

        // Copy the string into RAM so that strtok can
        // fiddle with it
        strncpy(str, "a,b,c", sizeof(str));
        // Do a strtok()
        strtok(str, ",");
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        strtok(NULL, ",");
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        pStr = strtok(NULL, ",");
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Do an snprintf() with parameters, which should
        // allocate memory.
        snprintf(buffer, sizeof(buffer), "%u %d %s", 4294967295U, (int) index, pStr);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Do a logging call with parameters, which will ultimately
        // call printf().  Note that this may not necessarily produce
        // "nice" output when logging from multiple tasks but it should
        // not corrupt memory
        uPortLog("U_PORT_TEST_OS_REENT_TASK_%d: %d \"%s\".\n", index, index, buffer);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Do an sscanf() of the parameters in the buffer
        // we wrote earlier
        if (sscanf(buffer, "%u %d %c", (unsigned int *) &a, (int *) &b, &c) == 3) {
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
            if (a != 4294967295UL) {
                returnCode = -3;
            }
            if (b != index) {
                returnCode = -4;
            }
            if (c != 'c') {
                returnCode = -5;
            }
        } else {
            returnCode = -2;
        }

        // Check that the memory we malloc()ed still contains
        // what we put there
        pTmp = pMem;
        for (int32_t x = 0; (returnCode == 0) &&
             (x < mallocSizeInts); x++) {
            if (*pTmp != checkInt) {
                returnCode = -6;
            }
            pTmp++;
        }

        // Free the memory again
        free(pMem);

        // Run around doing more malloc/compare/free with random
        // amounts of memory and yielding just to mix things up
        uPortLog("U_PORT_TEST_OS_REENT_TASK_%d: please wait while malloc()"
                 " is thrashed...\n", index);
        for (size_t x = 0; (returnCode == 0) && (x < 100); x++) {
            mallocSizeInts = 1 + (rand() % (U_PORT_TEST_OS_MALLOC_SIZE_INTS - 1));
            pMem = (int32_t *) malloc(mallocSizeInts * sizeof(int32_t));
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
            if (pMem != NULL) {
                pTmp = pMem;
                for (int32_t y = 0; y < mallocSizeInts; y++) {
                    *pTmp = checkInt;
                    pTmp++;
                }
                uPortTaskBlock(U_CFG_OS_YIELD_MS);
                pTmp = pMem;
                for (int32_t y = 0; (returnCode == 0) &&
                     (y < mallocSizeInts); y++) {
                    if (*pTmp != checkInt) {
                        returnCode = -8;
                    }
                    pTmp++;
                }
            } else {
                returnCode = -7;
            }
            free(pMem);
        }
    } else {
        returnCode = -1;
    }

    uPortLog("U_PORT_TEST_OS_REENT_TASK: instance %d done, returning %d.\n",
             index, returnCode);

    // Finally, set the parameter to the return code  to indicate done
    *((int32_t *) pParameter) = returnCode;

    // Wait for it...
    while (gWaitForStop) {
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // And delete ourselves
    uPortTaskDelete(NULL);
}

// The test task for OS stuff.
static void osTestTask(void *pParameters)
{
    int32_t queueItem = 0;
    int32_t index = 0;

    // Fill in the parameter
    strncpy(gTaskParameter, "Boo!", sizeof(gTaskParameter));

    // Pause here to let the task that spawned this one
    // run otherwise gTaskHandle won't have been populated.
    uPortTaskBlock(U_CFG_OS_YIELD_MS);

    uPortLog("U_PORT_TEST_OS_TASK: task with handle 0x%08x started,"
             " received parameter pointer 0x%08x containing string"
             " \"%s\".\n", (int) gTaskHandle, pParameters,
             (const char *) pParameters);
    U_PORT_TEST_ASSERT(strcmp((const char *) pParameters, gTaskParameter) == 0);

    U_PORT_TEST_ASSERT(uPortTaskIsThis(gTaskHandle));

    uPortLog("U_PORT_TEST_OS_TASK: task trying to lock the mutex.\n");
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) == 0);
    uPortLog("U_PORT_TEST_OS_TASK: unlocking it again.\n");
    U_PORT_TEST_ASSERT(uPortMutexUnlock(gMutexHandle) == 0);

    uPortLog("U_PORT_TEST_OS_TASK: locking it again (non-try version).\n");
    U_PORT_MUTEX_LOCK(gMutexHandle);

    U_PORT_TEST_ASSERT(gQueueHandleControl != NULL);
    U_PORT_TEST_ASSERT(gQueueHandleData != NULL);
    uPortLog("U_PORT_TEST_OS_TASK: task waiting on queue for data.\n");
    while (queueItem >= 0) {
        U_PORT_TEST_ASSERT(uPortQueueReceive(gQueueHandleData,
                                             &queueItem) == 0);
        uPortLog("U_PORT_TEST_OS_TASK: task received %d.\n", queueItem);
        if (queueItem >= 0) {
            uPortLog("                         item %d, expecting %d.\n",
                     index + 1, gStuffToSend[index]);
            U_PORT_TEST_ASSERT(gStuffToSend[index] == queueItem);
            index++;
        }
        uPortLog("U_PORT_TEST_OS_TASK: task checking control queue.\n");
        if (uPortQueueTryReceive(gQueueHandleControl, 100, &queueItem) == 0) {
            uPortLog("U_PORT_TEST_OS_TASK: task received %d on control"
                     " queue.\n", queueItem);
            U_PORT_TEST_ASSERT(queueItem == -1);
        }
    }

    uPortLog("U_PORT_TEST_OS_TASK: task exiting, unlocking mutex.\n");
    U_PORT_MUTEX_UNLOCK(gMutexHandle);

    uPortLog("U_PORT_TEST_OS_TASK: task deleting itself.\n");
    U_PORT_TEST_ASSERT(uPortTaskDelete(NULL) == 0);
}

// Function to send stuff to a queue.
static int32_t sendToQueue(uPortQueueHandle_t queueHandle,
                           int32_t thing)
{
    return uPortQueueSend(queueHandle, &thing);
}

// Function to send stuff to a queue using the IRQ version.
static int32_t sendToQueueIrq(uPortQueueHandle_t queueHandle,
                              int32_t thing)
{
    return uPortQueueSendIrq(queueHandle, &thing);
}

#if (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) && \
    (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)

// The test task for UART stuff.
static void uartTestTask(void *pParameters)
{
    uartTestTaskData_t *pUartTaskData = (uartTestTaskData_t *) pParameters;
    int32_t control = 0;
    int32_t receiveSize;
    int32_t dataSize;
    int32_t blockNumber = 0;
    size_t indexInBlock = 0;
    char *pReceive = gUartBuffer;

    U_PORT_MUTEX_LOCK(pUartTaskData->runningMutexHandle);

    uPortLog("U_PORT_TEST_UART_TASK: task started.\n");

    // Run until the control queue sends us a non-zero thing
    // or we spot an error
    while (control == 0) {
        // Wait for notification of UART data
        dataSize = uPortUartEventTryReceive(pUartTaskData->uartQueueHandle,
                                            1000);
        // Note: can't assert on uPortUartGetReceiveSize()
        // being larger than or equal to dataSize since dataSize
        // might be an old queued value.
        while (dataSize > 0) {
            receiveSize = uPortUartGetReceiveSize(U_CFG_TEST_UART_0);
            dataSize = uPortUartRead(U_CFG_TEST_UART_0,
                                     pReceive,
                                     gUartBuffer + sizeof(gUartBuffer) - pReceive);
            // dataSize will be smaller than receiveSize
            // if our data buffer is smaller than the UART receive
            // buffer but something might also have been received
            // between the two calls, making it larger.  Just
            // can't easily check uPortUartGetReceiveSize()
            // for accuracy, so instead do a range check here
            U_PORT_TEST_ASSERT(receiveSize >= 0);
            U_PORT_TEST_ASSERT(receiveSize <= U_PORT_UART_RX_BUFFER_SIZE);
            // Compare the data with the expected data
            for (int32_t x = 0; x < dataSize; x++) {
                if (gUartTestData[indexInBlock] == *pReceive) {
                    gUartBytesReceived++;
                    indexInBlock++;
                    // -1 below to omit gUartTestData string terminator
                    if (indexInBlock >= sizeof(gUartTestData) - 1) {
                        indexInBlock = 0;
                        blockNumber++;
                    }
                    pReceive++;
                    if (pReceive >= gUartBuffer + sizeof(gUartBuffer)) {
                        pReceive = gUartBuffer;
                    }
                } else {
                    control = -2;
                }
            }
        }

        // Check if there's anything for us on the control queue
        uPortQueueTryReceive(pUartTaskData->controlQueueHandle,
                             10, (void *) &control);
    }

    if (control == -2) {
        uPortLog("U_PORT_TEST_UART_TASK: error after %d character(s),"
                 " %d block(s).\n", gUartBytesReceived, blockNumber);
        uPortLog("U_PORT_TEST_UART_TASK: expected %c (0x%02x),"
                 " received %c (0x%02x).\n", gUartTestData[indexInBlock],
                 gUartTestData[indexInBlock], *pReceive, *pReceive);
    } else {
        uPortLog("U_PORT_TEST_UART_TASK: %d character(s), %d block(s)"
                 " received.\n", gUartBytesReceived, blockNumber);
    }

    U_PORT_MUTEX_UNLOCK(pUartTaskData->runningMutexHandle);

    uPortLog("U_PORT_TEST_UART_TASK: task ended.\n");

    U_PORT_TEST_ASSERT(uPortTaskDelete(NULL) == 0);
}

// Run a UART test at the given baud rate and with/without flow control.
static void runUartTest(int32_t size, int32_t speed, bool flowControlOn)
{
    uartTestTaskData_t uartTestTaskData;
    uPortTaskHandle_t uartTaskHandle;
    int32_t control;
    int32_t bytesToSend;
    int32_t bytesSent = 0;
    int32_t pinCts = -1;
    int32_t pinRts = -1;
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;

    gUartBytesReceived = 0;

    if (flowControlOn) {
        pinCts = U_CFG_TEST_PIN_UART_0_CTS;
        pinRts = U_CFG_TEST_PIN_UART_0_RTS;
    } else {
        // If we want to test with flow control off
        // but the flow control pins are actually
        // connected then they need to be set
        // to "get on with it"
#if (U_CFG_TEST_PIN_UART_0_CTS >= 0)
        // Make CTS an output pin and low
        U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_UART_0_CTS, 0) == 0);
        gpioConfig.pin = U_CFG_TEST_PIN_UART_0_CTS;
        gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
        U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
#endif
#if (U_CFG_TEST_PIN_UART_0_RTS >= 0)
        // Make RTS an output pin and low
        U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_UART_0_RTS, 0) == 0);
        gpioConfig.pin = U_CFG_TEST_PIN_UART_0_RTS;
        gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
        U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
#endif
    }

    uPortLog("U_PORT_TEST: testing UART loop-back, %d byte(s) at %d"
             " bits/s with flow control %s.\n", size, speed,
             ((pinCts >= 0) && (pinRts >= 0)) ? "on" : "off");

    U_PORT_TEST_ASSERT(uPortUartInit(U_CFG_TEST_PIN_UART_0_TXD,
                                     U_CFG_TEST_PIN_UART_0_RXD,
                                     pinCts, pinRts, speed,
                                     U_CFG_TEST_UART_0,
                                     &(uartTestTaskData.uartQueueHandle)) == 0);

    uPortLog("U_PORT_TEST: creating OS items to test UART...\n");

    // Create a mutex so that we can tell if the UART receive task is running
    U_PORT_TEST_ASSERT(uPortMutexCreate(&(uartTestTaskData.runningMutexHandle)) == 0);

    // Start a queue to control the UART receive task;
    // will send it -1 to exit
    U_PORT_TEST_ASSERT(uPortQueueCreate(5, sizeof(int32_t),
                                        &(uartTestTaskData.controlQueueHandle)) == 0);

    // Start the UART receive task
    U_PORT_TEST_ASSERT(uPortTaskCreate(uartTestTask, "uartTestTask",
                                       U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                       (void *) &uartTestTaskData,
                                       U_CFG_TEST_OS_TASK_PRIORITY,
                                       &uartTaskHandle) == 0);
    // Pause here to allow the task to start.
    uPortTaskBlock(100);

    // Send data over the UART N times, the UART receive task will check it
    while (bytesSent < size) {
        // -1 to omit gUartTestData string terminator
        bytesToSend = sizeof(gUartTestData) - 1;
        if (bytesToSend > size - bytesSent) {
            bytesToSend = size - bytesSent;
        }
        U_PORT_TEST_ASSERT(uPortUartWrite(U_CFG_TEST_UART_0,
                                          gUartTestData,
                                          bytesToSend) == bytesToSend);
        bytesSent += bytesToSend;
        uPortLog("U_PORT_TEST: %d byte(s) sent.\n", bytesSent);
    }

    // Wait long enough for everything to have been received
    uPortTaskBlock(1000);
    uPortLog("U_PORT_TEST: at end of test %d byte(s) sent, %d byte(s)"
             " received.\n", bytesSent, gUartBytesReceived);
    U_PORT_TEST_ASSERT(gUartBytesReceived == bytesSent);

    uPortLog("U_PORT_TEST: tidying up after UART test...\n");

    // Tell the UART Rx task to exit
    control = -1;
    U_PORT_TEST_ASSERT(uPortQueueSend(uartTestTaskData.controlQueueHandle,
                                      (void *) &control) == 0);
    // Wait for it to exit
    U_PORT_MUTEX_LOCK(uartTestTaskData.runningMutexHandle);
    U_PORT_MUTEX_UNLOCK(uartTestTaskData.runningMutexHandle);
    // Pause to allow it to be destroyed in the idle task
    uPortTaskBlock(100);

    // Tidy up the rest
    uPortQueueDelete(uartTestTaskData.controlQueueHandle);
    uPortMutexDelete(uartTestTaskData.runningMutexHandle);

    U_PORT_TEST_ASSERT(uPortUartDeinit(U_CFG_TEST_UART_0) == 0);
}

#endif // (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) &&
// (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then deinitialise the porting layer.
 */
U_PORT_TEST_FUNCTION("[port]", "portInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    uPortDeinit();
}

/** Test that the C stdlib functions are re-entrant.
 * Many platforms, specifically those built with GCC,
 * use newlib to provide the C stdlib functions.  Quite
 * a few of the library functions (e.g. printf(), malloc(),
 * strtok(), see full list here:
 * https://sourceware.org/newlib/libc.html#Reentrancy) are
 * NOT re-entrant, so cannot be used safely in an RTOS
 * world unless some hook functions provided by newlib
 * are connected to some hook functions provided by the
 * RTOS.  Pretty much all chipset vendors (see this link:
 * http://www.nadler.com/embedded/newlibAndFreeRTOS.html)
 * fail to do this in the code they provide.  We have
 * to do it.  This test is intended to check that we've
 * done it, though the problem may also show up in other
 * places.
 *
 * It is best if this check is run first in any automated
 * test run to avoid random crashes resulting from a
 * re-entrancy failure appearing elsewhere.  To do this the
 * define U_RUNNER_TOP_STR should be set to "port".
 * This is done by the u-blox test automation scripts.
 * Also, any heap checking that is available in the system
 * should be switched on.
 */
U_PORT_TEST_FUNCTION("[port]", "portRentrancy")
{
    bool finished = false;
    int32_t returnCode = 0;
    int32_t taskParameter[U_PORT_TEST_OS_NUM_REENT_TASKS];
    uPortTaskHandle_t taskHandle[U_PORT_TEST_OS_NUM_REENT_TASKS];
    int32_t stackMinFreeBytes;

    // Note: deliberately do NO printf()s until we have
    // set up the test scenario
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Set a flag which the tasks can wait on
    // before starting
    gWaitForGo = true;

    // Set a flag which the tasks can wait on
    // before stopping
    gWaitForStop = true;

    // Create a few tasks that wait on the flag
    // and then all try to call stdlib functions
    // that might cause memory issues at once.
    // The tasks are passed a (non-zero) index so that they
    // can identify themselves in malloc()s and
    // then indicate that they have finished by setting
    // the parameter to zero or less.
    for (size_t x = 0; (x < sizeof(taskHandle) /
                        sizeof(taskHandle[0])); x++) {
        taskParameter[x] = (int32_t) x + 1;
        U_PORT_TEST_ASSERT(uPortTaskCreate(osReentTask, "osReentTask",
                                           U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                           &(taskParameter[x]),
                                           U_CFG_TEST_OS_TASK_PRIORITY,
                                           &(taskHandle[x])) == 0);
    }

    // Let them run
    gWaitForGo = false;

    // Wait for everyone to finish, which is when
    // all parameters are zero or less
    while (!finished) {
        finished = true;
        for (size_t x = 0; finished &&
             (x < sizeof(taskParameter) /
              sizeof(taskParameter[0])); x++) {
            if (taskParameter[x] > 0) {
                finished = false;
            } else {
                if (taskParameter[x] < returnCode) {
                    returnCode = taskParameter[x];
                }
            }
        }
        uPortTaskBlock(100);
    }

    // Before stopping them, check their stack extents
    for (size_t x = 0; (x < sizeof(taskHandle) /
                        sizeof(taskHandle[0])); x++) {
        stackMinFreeBytes = uPortTaskStackMinFree(taskHandle[x]);
        uPortLog("U_PORT_TEST: test task %d had %d byte(s) free out of %d.\n",
                 x + 1, stackMinFreeBytes, U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    // Let them stop
    gWaitForStop = false;

    // Let the idle task tidy-away the tasks
    uPortTaskBlock(U_CFG_OS_YIELD_MS);

    // If the returnCode is 0 then that
    // is success.  If it is negative
    // than that then it indicates an error
    uPortLog("U_PORT_TEST: reentrancy task(s) returned %d.\n", returnCode);
    U_PORT_TEST_ASSERT(returnCode == 0);

    uPortDeinit();
}

/** Test all the normal OS stuff.
 */
U_PORT_TEST_FUNCTION("[port]", "portOs")
{
    int32_t errorCode;
    int64_t startTimeMs;
    int64_t timeNowMs;
    int32_t stackMinFreeBytes;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    startTimeMs = uPortGetTickTimeMs();
    uPortLog("U_PORT_TEST: tick time now is %d.\n",
             (int32_t) startTimeMs);

    uPortLog("U_PORT_TEST: creating a mutex...\n");
    errorCode = uPortMutexCreate(&gMutexHandle);
    uPortLog("                    returned error code %d, handle"
             " 0x%08x.\n", errorCode, gMutexHandle);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);

    uPortLog("U_PORT_TEST: creating a data queue...\n");
    errorCode = uPortQueueCreate(U_PORT_TEST_QUEUE_LENGTH,
                                 U_PORT_TEST_QUEUE_ITEM_SIZE,
                                 &gQueueHandleData);
    uPortLog("                    returned error code %d, handle"
             " 0x%08x.\n", errorCode, gQueueHandleData);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gQueueHandleData != NULL);

    uPortLog("U_PORT_TEST: creating a control queue...\n");
    errorCode = uPortQueueCreate(U_PORT_TEST_QUEUE_LENGTH,
                                 U_PORT_TEST_QUEUE_ITEM_SIZE,
                                 &gQueueHandleControl);
    uPortLog("                    returned error code %d, handle"
             " 0x%08x.\n", errorCode, gQueueHandleControl);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gQueueHandleControl != NULL);

    uPortLog("U_PORT_TEST: creating a test task with stack %d"
             " byte(s) and priority %d, passing it the pointer"
             " 0x%08x containing the string \"%s\"...\n",
             U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
             U_CFG_TEST_OS_TASK_PRIORITY,
             gTaskParameter, gTaskParameter);
    errorCode = uPortTaskCreate(osTestTask, "osTestTask",
                                U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                (void *) gTaskParameter,
                                U_CFG_TEST_OS_TASK_PRIORITY,
                                &gTaskHandle);
    uPortLog("                    returned error code %d, handle"
             " 0x%08x.\n", errorCode, gTaskHandle);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gTaskHandle != NULL);

    uPortLog("U_PORT_TEST: time now %d ms.\n", (int32_t) uPortGetTickTimeMs());
    // Pause to let the task print its opening messages
    uPortTaskBlock(1000);

    uPortLog("U_PORT_TEST: trying to lock the mutex, should fail...\n");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) != 0);

    uPortLog("U_PORT_TEST: sending stuff to task...\n");
    for (size_t x = 0; x < sizeof(gStuffToSend) /
         sizeof(gStuffToSend[0]); x++) {
        // If this is the last thing then queue up a -1 on the control
        // queue so that the test task exits after receiving the
        // last item on the data queue
        if (x == sizeof(gStuffToSend) / sizeof(gStuffToSend[0]) - 1) {
            stackMinFreeBytes = uPortTaskStackMinFree(gTaskHandle);
            uPortLog("U_PORT_TEST: test task had %d byte(s) free out of %d.\n",
                     stackMinFreeBytes, U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES);
            U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);

            uPortLog("U_PORT_TEST: sending -1 to terminate test task"
                     " control queue and waiting for it to stop...\n");
            sendToQueue(gQueueHandleControl, -1);
        }
        // Actually send the stuff by passing it to a function
        // where it will be placed on the stack so as to check
        // that the stuff is copied rather than referenced.
        // Use the IRQ version as well as the normal one.
        if (x & 1) {
            sendToQueue(gQueueHandleData, gStuffToSend[x]);
        } else {
            sendToQueueIrq(gQueueHandleData, gStuffToSend[x]);
        }
    }

    U_PORT_MUTEX_LOCK(gMutexHandle);
    // Yield to let it get the message
    uPortTaskBlock(U_CFG_OS_YIELD_MS);
    U_PORT_MUTEX_UNLOCK(gMutexHandle);
    uPortLog("U_PORT_TEST: task stopped.\n");

    // Pause to let the task print its final messages
    uPortTaskBlock(1000);

    uPortLog("U_PORT_TEST: deleting mutex...\n");
    uPortMutexDelete(gMutexHandle);

    uPortLog("U_PORT_TEST: deleting queues...\n");
    U_PORT_TEST_ASSERT(uPortQueueDelete(gQueueHandleControl) == 0);
    U_PORT_TEST_ASSERT(uPortQueueDelete(gQueueHandleData) == 0);

    timeNowMs = uPortGetTickTimeMs() - startTimeMs;
    uPortLog("U_PORT_TEST: according to uPortGetTickTimeMs()"
             " the test took %d ms.\n", (int32_t) timeNowMs);
    U_PORT_TEST_ASSERT((timeNowMs > 0) &&
                       (timeNowMs < U_PORT_TEST_OS_GUARD_DURATION_MS));

    uPortDeinit();
}

#if (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0)
/** Some ports, e.g. the Nordic one, use the tick time somewhat
 * differently when the UART is running so initialise that
 * here and re-measure time.  Of course, this is only testing
 * against its own time reference, for a proper test the log
 * should be checked for unusual variances in the time at which
 * the prints below are logged, and hence the longer time durations
 * used here so as to allow an error to appear.
 */
U_PORT_TEST_FUNCTION("[port]", "portOsExtended")
{
    int64_t startTimeMs;
    int64_t timeNowMs;
    int64_t timeDelta;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: running this test will take around"
             " %d second(s).\n",
             (U_PORT_TEST_OS_BLOCK_TIME_MS * 3) / 1000);

    startTimeMs = uPortGetTickTimeMs();
    uPortLog("U_PORT_TEST: tick time now is %d.\n",
             (int32_t) startTimeMs);

    uPortLog("U_PORT_TEST: waiting %d ms...\n",
             U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeNowMs = uPortGetTickTimeMs();
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    uPortLog("U_PORT_TEST: uPortTaskBlock(%d) blocked for"
             " %d ms.\n", U_PORT_TEST_OS_BLOCK_TIME_MS,
             (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    // Initialise the UART and re-measure time
    timeNowMs = uPortGetTickTimeMs();
    uPortLog("U_PORT_TEST: tick time now is %d.\n",
             (int32_t) timeNowMs);
    uPortLog("U_PORT_TEST: initialising UART...\n");
    U_PORT_TEST_ASSERT(uPortUartInit(U_CFG_TEST_PIN_UART_0_TXD,
                                     U_CFG_TEST_PIN_UART_0_RXD,
                                     U_CFG_TEST_PIN_UART_0_CTS,
                                     U_CFG_TEST_PIN_UART_0_RTS,
                                     U_CFG_TEST_BAUD_RATE,
                                     U_CFG_TEST_UART_0,
                                     &gQueueHandleData) == 0);
    uPortLog("U_PORT_TEST: waiting %d ms...\n",
             U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeNowMs = uPortGetTickTimeMs();
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    uPortLog("U_PORT_TEST: uPortTaskBlock(%d) blocked for"
             " %d ms.\n", U_PORT_TEST_OS_BLOCK_TIME_MS,
             (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    uPortLog("U_PORT_TEST: deinitialising UART...\n");
    timeNowMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortUartDeinit(U_CFG_TEST_UART_0) == 0);

    uPortLog("U_PORT_TEST: waiting %d ms...\n",
             U_PORT_TEST_OS_BLOCK_TIME_MS);
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    uPortLog("U_PORT_TEST: uPortTaskBlock(%d) blocked for"
             " %d ms.\n", U_PORT_TEST_OS_BLOCK_TIME_MS,
             (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    timeDelta = uPortGetTickTimeMs() - startTimeMs;
    uPortLog("U_PORT_TEST: according to uPortGetTickTimeMs()"
             " the test took %d second(s).\n", (int32_t) (timeDelta / 1000));
    uPortLog("U_PORT_TEST: *** IMPORTANT *** please visually check"
             " that the duration of this test as seen by the PC-side"
             " of the test system is also %d second(s).\n",
             (int32_t) (timeDelta / 1000));

    uPortDeinit();
}
#endif

/** Test: strtok_r since we have our own implementation on
 * some platforms.
 */
U_PORT_TEST_FUNCTION("[port]", "portStrtok_r")
{
    char *pSave;
    char buffer[8];

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: testing strtok_r...\n");

    buffer[sizeof(buffer) - 1] = 'x';

    strcpy(buffer, "abcabc");
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "b", &pSave), "a") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strcmp(strtok_r(NULL, "b", &pSave), "ca") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strcmp(strtok_r(NULL, "b", &pSave), "c") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strtok_r(NULL, "b", &pSave) == NULL);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');

    strcpy(buffer, "abcade");
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "a", &pSave), "bc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    strcpy(buffer, "abcade");
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "a", &pSave), "bc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strcmp(strtok_r(NULL, "a", &pSave), "de") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strtok_r(NULL, "a", &pSave) == NULL);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');

    strcpy(buffer, "abcabc");
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "d", &pSave), "abcabc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strtok_r(NULL, "d", &pSave) == NULL);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');

    uPortDeinit();
}

#if (U_CFG_TEST_PIN_A >= 0) && (U_CFG_TEST_PIN_B >= 0) && \
    (U_CFG_TEST_PIN_C >= 0)
/** Test GPIOs.
 */
U_PORT_TEST_FUNCTION("[port]", "portGpioRequiresSpecificWiring")
{
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: testing GPIOs.\n");
    uPortLog("U_PORT_TEST: pin A (%d, 0x%02x) will be used as an"
             " output and must be connected to pin B (%d, 0x%02x)"
             " via a 1k resistor.\n",
             U_CFG_TEST_PIN_A, U_CFG_TEST_PIN_A,
             U_CFG_TEST_PIN_B, U_CFG_TEST_PIN_B);
    uPortLog("U_PORT_TEST: pin B (%d, 0x%02x) will be used as an"
             " input and an open drain output.\n",
             U_CFG_TEST_PIN_B, U_CFG_TEST_PIN_B);
    uPortLog("U_PORT_TEST: pin C (%d, 0x%02x) will be used as"
             " an input and must be connected to pin B (%d,"
             " 0x%02x).\n",
             U_CFG_TEST_PIN_C, U_CFG_TEST_PIN_C,
             U_CFG_TEST_PIN_B, U_CFG_TEST_PIN_B);

    // Make pins B and C inputs, no pull
    gpioConfig.pin = U_CFG_TEST_PIN_B;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT;
    gpioConfig.pullMode =  U_PORT_GPIO_PULL_MODE_NONE;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
    gpioConfig.pin = U_CFG_TEST_PIN_C;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);

    // Set pin A high
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_A, 1) == 0);
    // Make it an output pin
    gpioConfig.pin = U_CFG_TEST_PIN_A;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
    // Let it settle
    uPortTaskBlock(U_CFG_OS_YIELD_MS);

    // Pins B and C should read 1
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_B) == 1);
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 1);

    // Set pin A low
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_A, 0) == 0);
    uPortTaskBlock(1);

    // Pins B and C should read 0
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_B) == 0);
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 0);

    // Make pin B an output, low, open drain
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_B, 0) == 0);
    gpioConfig.pin = U_CFG_TEST_PIN_B;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
    // Let it settle
    uPortTaskBlock(1);

    // Pin C should still read 0
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 0);

    // Set pin A high
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_A, 1) == 0);
    // Let it settle
    uPortTaskBlock(1);

    // Pin C should still read 0
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 0);

    // Set pin B high
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_B, 1) == 0);
    // Let it settle
    uPortTaskBlock(1);

    // Pin C should now read 1
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 1);

    // Make pin A an input/output pin
    gpioConfig.pin = U_CFG_TEST_PIN_A;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT_OUTPUT;
    gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_NORMAL;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);

    // Pin A should read 1
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_A) == 1);

    // Set pin A low
    U_PORT_TEST_ASSERT(uPortGpioSet(U_CFG_TEST_PIN_A, 0) == 0);
    // Let it settle
    uPortTaskBlock(1);

    // Pins A and C should read 0
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_A) == 0);
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 0);

    // Make pin B an input/output open-drain pin
    gpioConfig.pin = U_CFG_TEST_PIN_B;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT_OUTPUT;
    gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
    U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);

    // All pins should read 0
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_A) == 0);
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_B) == 0);
    U_PORT_TEST_ASSERT(uPortGpioGet(U_CFG_TEST_PIN_C) == 0);

    // Note: it is impossible to check pull up/down
    // of input pins reliably as boards have level shifters
    // and protection resistors between the board pins and the
    // chip pins that drown-out the effect of the pull up/down
    // inside the chip.
    // Also can't easily test drive strength and in any case
    // it is not supported on all platforms.

    uPortDeinit();
}
#endif

#if (U_CFG_TEST_PIN_UART_0_TXD >= 0) && (U_CFG_TEST_PIN_UART_0_RXD >= 0) && \
    (U_CFG_TEST_PIN_UART_1_TXD < 0) && (U_CFG_TEST_PIN_UART_1_RXD < 0)
/** Test UART.
 */
U_PORT_TEST_FUNCTION("[port]", "portUartRequiresSpecificWiring")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Run a UART test at 115,200
#if (U_CFG_TEST_PIN_UART_0_CTS >= 0) && (U_CFG_TEST_PIN_UART_0_RTS >= 0)
    // ...with flow control
    runUartTest(50000, 115200, true);
#endif
    // ...without flow control
    runUartTest(50000, 115200, false);

    uPortDeinit();
}
#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[port]", "portCleanUp")
{
    U_PORT_TEST_ASSERT(uPortUartDeinit(U_CFG_TEST_UART_0) == 0);
    uPortDeinit();
}

// End of file
