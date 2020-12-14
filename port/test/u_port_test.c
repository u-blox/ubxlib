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
#include "u_port_crypto.h"
#include "u_port_event_queue.h"
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
#define U_PORT_TEST_OS_GUARD_DURATION_MS 2800

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

/** Number of interations for the event queue test.
 * Must be less than 256.
 */
#define U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS 100

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

/** Type to hold the stuff that the UART test task needs to know
 * about.
 */
typedef struct {
    size_t callCount;
    int32_t blockNumber;
    size_t indexInBlock;
    char *pReceive;
    size_t bytesReceived;
    int32_t errorCode;
} uartEventCallbackData_t;

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

// Handle for event queue callback max length
static int32_t gEventQueueMaxHandle;

// Error flag for event queue callback max length
static int32_t gEventQueueMaxErrorFlag;

// Counter for event queue callback max length
static int32_t gEventQueueMaxCounter;

// Handle for event queue callback min length
static int32_t gEventQueueMinHandle;

// Error flag for event queue callback min length
static int32_t gEventQueueMinErrorFlag;

// Counter for event queue callback min length
static int32_t gEventQueueMinCounter;

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

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
// U_CFG_TEST_UART_BUFFER_LENGTH_BYTES
// so that the buffers go "around the corner"
static char gUartBuffer[(U_CFG_TEST_UART_BUFFER_LENGTH_BYTES / 2) +
                                                                  (U_CFG_TEST_UART_BUFFER_LENGTH_BYTES / 4)];

#endif // (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

/** SHA256 test vector, input, RC4.55 from:
 * https://www.dlitz.net/crypto/shad256-test-vectors/
 */
static char const gSha256Input[] =
    "\xde\x18\x89\x41\xa3\x37\x5d\x3a\x8a\x06\x1e\x67\x57\x6e\x92\x6d"
    "\xc7\x1a\x7f\xa3\xf0\xcc\xeb\x97\x45\x2b\x4d\x32\x27\x96\x5f\x9e"
    "\xa8\xcc\x75\x07\x6d\x9f\xb9\xc5\x41\x7a\xa5\xcb\x30\xfc\x22\x19"
    "\x8b\x34\x98\x2d\xbb\x62\x9e";

/** SHA256 test vector, output, RC4.55 from:
 * https://www.dlitz.net/crypto/shad256-test-vectors/
 */
static const char gSha256Output[] =
    "\x03\x80\x51\xe9\xc3\x24\x39\x3b\xd1\xca\x19\x78\xdd\x09\x52\xc2"
    "\xaa\x37\x42\xca\x4f\x1b\xd5\xcd\x46\x11\xce\xa8\x38\x92\xd3\x82";

/** HMAC SHA256 test vector, key, test 1 from:
 * https://tools.ietf.org/html/rfc4231#page-3
 */
static const char gHmacSha256Key[] =
    "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
    "\x0b\x0b\x0b\x0b";

/** HMAC SHA256 test vector, input data, test 1 from:
 * https://tools.ietf.org/html/rfc4231#page-3
 */
static const char gHmacSha256Input[] = "\x48\x69\x20\x54\x68\x65\x72\x65";

/** HMAC SHA256 test vector, output data, test 1 from:
 * https://tools.ietf.org/html/rfc4231#page-3
 */
static const char gHmacSha256Output[] =
    "\xb0\x34\x4c\x61\xd8\xdb\x38\x53\x5c\xa8\xaf\xce\xaf\x0b\xf1\x2b"
    "\x88\x1d\xc2\x00\xc9\x83\x3d\xa7\x26\xe9\x37\x6c\x2e\x32\xcf\xf7";

/** AES CBC 128 test vector, key, test 1 from:
 * https://tools.ietf.org/html/rfc3602#page-6
 */
static const char gAes128CbcKey[] =
    "\x06\xa9\x21\x40\x36\xb8\xa1\x5b\x51\x2e\x03\xd5\x34\x12\x00\x06";

/** AES CBC 128 test vector, initial vector, test 1 from:
 * https://tools.ietf.org/html/rfc3602#page-6
 */
static const char gAes128CbcIV[] =
    "\x3d\xaf\xba\x42\x9d\x9e\xb4\x30\xb4\x22\xda\x80\x2c\x9f\xac\x41";

/** AES CBC 128 test vector, clear text, test 1 from:
 * https://tools.ietf.org/html/rfc3602#page-6
 */
static const char gAes128CbcClear[] = "Single block msg";

/** AES CBC 128 test vector, encrypted text, test 1 from:
 * https://tools.ietf.org/html/rfc3602#page-6
 */
static const char gAes128CbcEncrypted[] =
    "\xe3\x53\x77\x9c\x10\x79\xae\xb8\x27\x08\x94\x2d\xbe\x77\x18\x1a";

/** For tracking heap lost to memory  lost by the C library.
 */
static size_t gSystemHeapLost = 0;

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
//lint -esym(818, pParameters) Suppress "could be const"
// since this has to match the function signature
// exactly to avoid a compiler warning
static void osTestTask(void *pParameters)
{
    int32_t queueItem = 0;
    int32_t index = 0;
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;

    // Calling C library functions from a new task
    // allocates additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
#endif

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

#if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the first
    // printf()
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
#endif

    U_PORT_TEST_ASSERT(uPortTaskIsThis(gTaskHandle));

    uPortLog("U_PORT_TEST_OS_TASK: task trying to lock the mutex.\n");
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 500) == 0);
    uPortLog("U_PORT_TEST_OS_TASK: task trying to lock the mutex again, should fail!.\n");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) != 0);
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
            uPortLog("                     item %d, expecting %d.\n",
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

// An event queue function for max length parameter.
//lint -esym(818, pParam) Suppress "could be const"
// since this has to match the function signature
// exactly to avoid a compiler warning
static void eventQueueMaxFunction(void *pParam,
                                  size_t paramLength)
{
    uint8_t fill = 0xFF;

    if (gEventQueueMaxCounter <
        U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS) {
        // For U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS
        // we expect to receive paramLength of
        // U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES
        // containing the pattern 0xFF to 0 repeated
        // but with the last byte containing a counter
        // which increments from zero.
        if (paramLength != U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) {
            gEventQueueMaxErrorFlag = 1;
        }

        for (size_t x = 0; (gEventQueueMaxErrorFlag == 0) &&
             (x < paramLength - 1); x++) {
            if (*((uint8_t *) pParam + x) != fill) {
                gEventQueueMaxErrorFlag = 2;
            }
            fill--;
        }

        if (gEventQueueMaxErrorFlag == 0) {
            if (*((uint8_t *) pParam + paramLength - 1) !=
                (uint8_t) gEventQueueMaxCounter) {
                gEventQueueMaxErrorFlag = 3;
            }
        }
    } else {
        if (gEventQueueMaxCounter ==
            U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS) {
            // For one final bonus iteration we expect
            // pParam to be NULL and paramLength 0
            if (pParam != NULL) {
                gEventQueueMaxErrorFlag = 4;
            }
            if (paramLength != 0) {
                gEventQueueMaxErrorFlag = 5;
            }
        } else {
            // Anything else shouldn't happen
            gEventQueueMaxErrorFlag = 6;
        }
    }

    if (gEventQueueMaxErrorFlag == 0) {
        if (!uPortEventQueueIsTask(gEventQueueMaxHandle)) {
            // Not detecting that this is an event task
            gEventQueueMaxErrorFlag = 7;
        } else {
            if (uPortEventQueueIsTask(gEventQueueMinHandle)) {
                // Detecting that this is the wrong event task
                gEventQueueMaxErrorFlag = 8;
            }
        }
    }

    gEventQueueMaxCounter++;
}

// Event queue function for minimum length parameter.
//lint -esym(818, pParam) Suppress "could be const"
// since this has to match the function signature
// exactly to avoid a compiler warning
static void eventQueueMinFunction(void *pParam,
                                  size_t paramLength)
{
    if (gEventQueueMinCounter <
        U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS) {
        // For U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS
        // we expect to receive paramLength of 1 where
        // *pParam is a count of the number of times we've
        // been called.
        if (paramLength != 1) {
            gEventQueueMinErrorFlag = 1;
        }

        if (gEventQueueMinErrorFlag == 0) {
            if (*((uint8_t *) pParam) != (uint8_t) gEventQueueMinCounter) {
                gEventQueueMinErrorFlag = 2;
            }
        }
    } else {
        if (gEventQueueMinCounter ==
            U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS) {
            // For one final bonus iteration we expect
            // pParam to be NULL and paramLength 0
            if (pParam != NULL) {
                gEventQueueMinErrorFlag = 4;
            }
            if (paramLength != 0) {
                gEventQueueMinErrorFlag = 5;
            }
        } else {
            // Anything else shouldn't happen
            gEventQueueMinErrorFlag = 6;
        }
    }

    if (gEventQueueMinErrorFlag == 0) {
        if (!uPortEventQueueIsTask(gEventQueueMinHandle)) {
            // Not detecting that this is an event task
            gEventQueueMinErrorFlag = 7;
        } else {
            if (uPortEventQueueIsTask(gEventQueueMaxHandle)) {
                // Detecting that this is the wrong event task
                gEventQueueMinErrorFlag = 8;
            }
        }
    }

    gEventQueueMinCounter++;
}

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

// Callback that is called when data arrives at the UART
static void uartReceivedDataCallback(int32_t uartHandle,
                                     uint32_t filter,
                                     void *pParameters)
{
    int32_t dataSizeOrError;
    int32_t receiveSizeOrError;
    uartEventCallbackData_t *pEventCallbackData = (uartEventCallbackData_t *) pParameters;

    pEventCallbackData->callCount++;
    if (filter != U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        pEventCallbackData->errorCode = -1;
    } else {
        // Run until we spot an error or run out of data
        do {
            receiveSizeOrError = uPortUartGetReceiveSize(uartHandle);
            dataSizeOrError = uPortUartRead(uartHandle,
                                            pEventCallbackData->pReceive,
                                            gUartBuffer + sizeof(gUartBuffer) -
                                            pEventCallbackData->pReceive);
            if (dataSizeOrError < 0) {
                pEventCallbackData->errorCode = -2;
            }
            // dataSizeOrError will be smaller than receiveSizeOrError
            // if our data buffer is smaller than the UART receive
            // buffer but something might also have been received
            // between the two calls, making it larger.  Just
            // can't easily check uPortUartGetReceiveSize()
            // for accuracy, so instead do a range check here
            if (receiveSizeOrError < 0) {
                pEventCallbackData->errorCode = -3;
            }
            if (receiveSizeOrError > U_CFG_TEST_UART_BUFFER_LENGTH_BYTES) {
                pEventCallbackData->errorCode = -4;
            }
            // Compare the data with the expected data
            for (int32_t x = 0; (pEventCallbackData->errorCode == 0) &&
                 (x < dataSizeOrError); x++) {
                if (gUartTestData[pEventCallbackData->indexInBlock] ==
                    *(pEventCallbackData->pReceive)) {
                    pEventCallbackData->bytesReceived++;
                    pEventCallbackData->indexInBlock++;
                    // -1 below to omit gUartTestData string terminator
                    if (pEventCallbackData->indexInBlock >= sizeof(gUartTestData) - 1) {
                        pEventCallbackData->indexInBlock = 0;
                        pEventCallbackData->blockNumber++;
                    }
                    pEventCallbackData->pReceive++;
                    if (pEventCallbackData->pReceive >= gUartBuffer + sizeof(gUartBuffer)) {
                        pEventCallbackData->pReceive = gUartBuffer;
                    }
                } else {
                    pEventCallbackData->errorCode = -5;
                }
            }
        } while ((dataSizeOrError > 0) && (pEventCallbackData->errorCode == 0));
    }
}

// Run a UART test at the given baud rate and with/without flow control.
static void runUartTest(int32_t size, int32_t speed, bool flowControlOn)
{
    int32_t uartHandle;
    uartEventCallbackData_t eventCallbackData = {0};
    int32_t bytesToSend;
    int32_t bytesSent = 0;
    int32_t pinCts;
    int32_t pinRts;
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;
    int32_t stackMinFreeBytes;

    eventCallbackData.callCount = 0;
    eventCallbackData.pReceive = gUartBuffer;

    // Grab here the pins that would be passed to
    // uPortUartOpen(), not the _GET versions.  On
    // a platform where the pins are set at compile
    // time these values will be -1, ignored.
    pinCts = U_CFG_TEST_PIN_UART_A_CTS;
    pinRts = U_CFG_TEST_PIN_UART_A_RTS;

    // Print where the pins are actually connected, that's what
    // the user needs to know.  On a platform which can set
    // the pins at run-time the values will be the same
    // as the pinCts and pinRts values.
    uPortLog("U_PORT_TEST: UART CTS is on pin %d and RTS on"
             " pin %d", U_CFG_TEST_PIN_UART_A_CTS_GET,
             U_CFG_TEST_PIN_UART_A_RTS_GET);
    if (!flowControlOn) {
        uPortLog(" but we're going to ignore them for this"
                 " test.\n");
        // If we want to test with flow control off
        // but the flow control pins are actually
        // connected then they need to be set
        // to "get on with it"
        if (pinCts >= 0) {
            // Make CTS an output pin and low
            U_PORT_TEST_ASSERT(uPortGpioSet(pinCts, 0) == 0);
            gpioConfig.pin = pinCts;
            gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
            U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }
        if (pinRts >= 0) {
            // Make RTS an output pin and low
            U_PORT_TEST_ASSERT(uPortGpioSet(pinRts, 0) == 0);
            gpioConfig.pin = pinRts;
            gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
            U_PORT_TEST_ASSERT(uPortGpioConfig(&gpioConfig) == 0);
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }
        pinCts = -1;
        pinRts = -1;
    } else {
        uPortLog(".\n");
    }

    uPortLog("U_PORT_TEST: testing UART loop-back, %d byte(s) at %d"
             " bits/s with flow control %s.\n", size, speed,
             ((pinCts >= 0) && (pinRts >= 0)) ? "on" : "off");

    uPortLog("U_PORT_TEST: add a UART instance...\n");
    uartHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                               speed, NULL,
                               U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_TEST_PIN_UART_A_TXD,
                               U_CFG_TEST_PIN_UART_A_RXD,
                               pinCts, pinRts);
    U_PORT_TEST_ASSERT(uartHandle >= 0);

    uPortLog("U_PORT_TEST: add a UART event callback which"
             " will receive the data...\n");
    U_PORT_TEST_ASSERT(uPortUartEventCallbackSet(uartHandle,
                                                 (uint32_t) U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                 uartReceivedDataCallback,
                                                 (void *) &eventCallbackData,
                                                 U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                                 U_CFG_OS_APP_TASK_PRIORITY + 1) == 0);

    // Check that the callback is there
    U_PORT_TEST_ASSERT(uPortUartEventCallbackFilterGet(uartHandle) ==
                       U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);

    // Set the filter (there's only one so this isn't doing much,
    // but what can you do)
    U_PORT_TEST_ASSERT(uPortUartEventCallbackFilterSet(uartHandle,
                                                       (uint32_t) U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) ==
                       0);

    // Manually send an Rx event and check that it caused
    // the callback to be called
    U_PORT_TEST_ASSERT(eventCallbackData.callCount == 0);
    U_PORT_TEST_ASSERT(uPortUartEventSend(uartHandle,
                                          (uint32_t) U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) ==
                       0);
    U_PORT_TEST_ASSERT(eventCallbackData.callCount == 1);

    // Send data over the UART N times, the callback will check it
    while (bytesSent < size) {
        // -1 to omit gUartTestData string terminator
        bytesToSend = sizeof(gUartTestData) - 1;
        if (bytesToSend > size - bytesSent) {
            bytesToSend = size - bytesSent;
        }
        U_PORT_TEST_ASSERT(uPortUartWrite(uartHandle,
                                          gUartTestData,
                                          bytesToSend) == bytesToSend);
        bytesSent += bytesToSend;
        uPortLog("U_PORT_TEST: %d byte(s) sent.\n", bytesSent);
        // Yield so that the receive task has chance to do
        // its stuff.  This shouldn't really be necessary
        // but without it ESP32 seems to occasionally (1 in 20
        // or 30 runs) get stuck waiting for a transmit to
        // complete when flow control is on, suggesting that
        // it has been flow-controlled off due to the RX not
        // being serviced fast enough
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Wait long enough for everything to have been received
    uPortTaskBlock(1000);

    // Print out some useful stuff
    if (eventCallbackData.errorCode == -5) {
        uPortLog("U_PORT_TEST: error after %d character(s),"
                 " %d block(s).\n", eventCallbackData.bytesReceived,
                 eventCallbackData.blockNumber);
        uPortLog("U_PORT_TEST: expected %c (0x%02x), received"
                 " %c (0x%02x).\n",
                 gUartTestData[eventCallbackData.indexInBlock],
                 gUartTestData[eventCallbackData.indexInBlock],
                 eventCallbackData.pReceive, eventCallbackData.pReceive);
    } else if (eventCallbackData.errorCode < 0) {
        uPortLog("U_PORT_TEST: finished with error code %d"
                 " after correctly receiving %d byte(s).\n",
                 eventCallbackData.errorCode,
                 eventCallbackData.bytesReceived);
    }

    uPortLog("U_PORT_TEST: at end of test %d byte(s) sent, %d byte(s)"
             " received.\n", bytesSent, eventCallbackData.bytesReceived);
    U_PORT_TEST_ASSERT(eventCallbackData.bytesReceived == bytesSent);

    // Check the stack extent for the task on the end of the
    // event queue
    stackMinFreeBytes = uPortUartEventStackMinFree(uartHandle);
    uPortLog("U_PORT_TEST: UART event queue task had %d byte(s) free out of %d.\n",
             stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
    U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);

    uPortLog("U_PORT_TEST: tidying up after UART test...\n");
    uPortUartClose(uartHandle);
}

#endif // (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

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
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;
#endif

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    // The first time rand() is called the C library may
    // allocate memory, not something we can do anything
    // about, so call it once here to move that number
    // out of our sums.
    rand();
    heapUsed = uPortGetHeapFree();

    // Note: deliberately do NO printf()s until we have
    // set up the test scenario
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Set a flag which the tasks can wait on
    // before starting
    gWaitForGo = true;

    // Set a flag which the tasks can wait on
    // before stopping
    gWaitForStop = true;

#if U_CFG_OS_CLIB_LEAKS
    // Calling C library functions from new tasks will
    // allocate additional memory which, depending
    // on the OS/system, may not be recovered;
    // take account of that here.
    heapClibLoss = uPortGetHeapFree();
#endif
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
    uPortTaskBlock(U_CFG_OS_YIELD_MS + 1000);

#if U_CFG_OS_CLIB_LEAKS
    // Take account of any heap lost through the
    // library calls
    gSystemHeapLost += (size_t) (unsigned) (heapClibLoss - uPortGetHeapFree());
#endif

    // If the returnCode is 0 then that
    // is success.  If it is negative
    // that it indicates an error.
    uPortLog("U_PORT_TEST: reentrancy task(s) returned %d.\n", returnCode);
    U_PORT_TEST_ASSERT(returnCode == 0);

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Test all the normal OS stuff.
 */
U_PORT_TEST_FUNCTION("[port]", "portOs")
{
    int32_t errorCode;
    int64_t startTimeMs;
    int64_t timeNowMs;
    int32_t stackMinFreeBytes;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
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
    uPortLog("U_PORT_TEST: locking mutex, preventing task from executing\n");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) == 0);

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
    uPortTaskBlock(200);
    uPortLog("U_PORT_TEST: unlocking mutex, allowing task to execute\n");
    U_PORT_TEST_ASSERT(uPortMutexUnlock(gMutexHandle) == 0);;
    // Pause to let the task print its opening messages
    uPortTaskBlock(1200);

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

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

#if (U_CFG_TEST_UART_A >= 0)
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
    int32_t uartHandle;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
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
    uPortLog("U_PORT_TEST: add a UART instance...\n");
    uartHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                               U_CFG_TEST_BAUD_RATE,
                               NULL,
                               U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_TEST_PIN_UART_A_TXD,
                               U_CFG_TEST_PIN_UART_A_RXD,
                               U_CFG_TEST_PIN_UART_A_CTS,
                               U_CFG_TEST_PIN_UART_A_RTS);
    U_PORT_TEST_ASSERT(uartHandle >= 0);
    uPortLog("U_PORT_TEST: waiting %d ms...\n",
             U_PORT_TEST_OS_BLOCK_TIME_MS);
    //lint -e(838) Suppress previous value not used which
    // will occur if uPortLog() is compiled out
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
    uPortUartClose(uartHandle);

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

    //lint -esym(438, timeDelta) Suppress value not used, which
    // will occur if uPortLog() is compiled out
    timeDelta = uPortGetTickTimeMs() - startTimeMs;
    uPortLog("U_PORT_TEST: according to uPortGetTickTimeMs()"
             " the test took %d second(s).\n", (int32_t) (timeDelta / 1000));
    uPortLog("U_PORT_TEST: *** IMPORTANT *** please visually check"
             " that the duration of this test as seen by the PC-side"
             " of the test system is also %d second(s).\n",
             (int32_t) (timeDelta / 1000));

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}
#endif

/** Test event queues.
 */
U_PORT_TEST_FUNCTION("[port]", "portEventQueue")
{
    uint8_t *pParam;
    uint8_t fill;
    size_t x;
    int32_t stackMinFreeBytes;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // Reset error flags and counters
    gEventQueueMaxErrorFlag = 0;
    gEventQueueMaxCounter = 0;
    gEventQueueMinErrorFlag = 0;
    gEventQueueMinCounter = 0;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: opening two event queues...\n");
    // Open two event queues, one with the
    // maximum parameter length and one with just
    // a single byte, one with a name and one without
    gEventQueueMaxHandle = uPortEventQueueOpen(eventQueueMaxFunction, NULL,
                                               U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES,
                                               U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                               U_CFG_TEST_OS_TASK_PRIORITY,
                                               U_PORT_TEST_QUEUE_LENGTH);
    U_PORT_TEST_ASSERT(gEventQueueMaxHandle >= 0);
    gEventQueueMinHandle = uPortEventQueueOpen(eventQueueMinFunction, "blah", 1,
                                               U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                               U_CFG_TEST_OS_TASK_PRIORITY,
                                               U_PORT_TEST_QUEUE_LENGTH);
    U_PORT_TEST_ASSERT(gEventQueueMinHandle >= 0);

    // Generate a block with a known test pattern, 0xFF to 0 repeated.
    //lint -esym(613, pParam) Suppress possible use of NULL pointer: it is checked
    pParam = (uint8_t *) malloc(U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pParam != NULL);
    fill = 0xFF;
    for (x = 0; x < U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES; x++) {
        *(pParam + x) = fill;
        fill--;
    }

    uPortLog("U_PORT_TEST: sending to the two event queues %d"
             " time(s)...\n", U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);

    // Try to send too much to each queue, should fail
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           pParam,
                                           U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES + 1) < 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           pParam, 2) < 0);

    // Send the known test pattern N times to eventQueueMaxFunction
    // with the last byte overwritten with a counter, and just send
    // the counter to eventQueueMinFunction as its single byte
    // The receiving functions will set a flag if they find a
    // problem.
    // Use both the IRQ and non-IRQ versions of the call
    for (x = 0; (x < U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS) &&
         (gEventQueueMaxErrorFlag == 0) &&
         (gEventQueueMinErrorFlag == 0); x++) {
        *(pParam + U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES - 1) = (uint8_t) x;
        if (x & 1) {
            U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                                   (void *) pParam,
                                                   U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) == 0);
            U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                                   (void *) &x, 1) == 0);
        } else {
            U_PORT_TEST_ASSERT(uPortEventQueueSendIrq(gEventQueueMaxHandle,
                                                      (void *) pParam,
                                                      U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) == 0);
            U_PORT_TEST_ASSERT(uPortEventQueueSendIrq(gEventQueueMinHandle,
                                                      (void *) &x, 1) == 0);
        }
    }

    // Bonus iteration with NULL parameter
    //lint -esym(438, x) Suppress value not used, which
    // will occur if uPortLog() is compiled out
    x++;
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           NULL, 0) == 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           NULL, 0) == 0);

    if (gEventQueueMaxErrorFlag != 0) {
        uPortLog("U_PORT_TEST: event queue max length"
                 " failed on iteration %d with error %d.\n",
                 x, gEventQueueMaxErrorFlag);
    }
    if (gEventQueueMinErrorFlag != 0) {
        uPortLog("U_PORT_TEST: event queue min length"
                 " failed on iteration %d with error %d.\n",
                 x, gEventQueueMinErrorFlag);
    }

    U_PORT_TEST_ASSERT(gEventQueueMaxErrorFlag == 0);
    U_PORT_TEST_ASSERT(gEventQueueMaxCounter == U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);
    U_PORT_TEST_ASSERT(gEventQueueMaxErrorFlag == 0);
    U_PORT_TEST_ASSERT(gEventQueueMinCounter == U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);

    // Check that the uPortEventQueueIsTask() gives a negative
    // answer correctly
    U_PORT_TEST_ASSERT(!uPortEventQueueIsTask(gEventQueueMaxHandle));
    U_PORT_TEST_ASSERT(!uPortEventQueueIsTask(gEventQueueMinHandle));

    // Check stack usage of the tasks at the end of the event queues
    stackMinFreeBytes = uPortEventQueueStackMinFree(gEventQueueMinHandle);
    uPortLog("U_PORT_TEST: event queue min task had %d byte(s) free out of %d.\n",
             stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
    U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    stackMinFreeBytes = uPortEventQueueStackMinFree(gEventQueueMaxHandle);
    uPortLog("U_PORT_TEST: event queue max task had %d byte(s) free out of %d.\n",
             stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
    U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);

    uPortLog("U_PORT_TEST: closing the event queues...\n");
    U_PORT_TEST_ASSERT(uPortEventQueueClose(gEventQueueMaxHandle) == 0);
    U_PORT_TEST_ASSERT(uPortEventQueueClose(gEventQueueMinHandle) == 0);

    // Check that they are no longer available
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           pParam,
                                           U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) < 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           pParam, 1) < 0);

    // Free memory
    free(pParam);

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Test: strtok_r since we have our own implementation on
 * some platforms.
 */
U_PORT_TEST_FUNCTION("[port]", "portStrtok_r")
{
    char *pSave;
    char buffer[8];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: testing strtok_r...\n");

    buffer[sizeof(buffer) - 1] = 'x';

    strcpy(buffer, "abcabc");
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "b", &pSave), "a") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    //lint -e(668) Suppress possible passing of NULL pointer to strcmp,
    // it's a test, if it goes bang we've failed
    U_PORT_TEST_ASSERT(strcmp(strtok_r(NULL, "b", &pSave), "ca") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    //lint -e(668) Suppress possible passing of NULL pointer to strcmp,
    // it's a test, if it goes bang we've failed
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
    //lint -e(668) Suppress possible passing of NULL pointer to strcmp,
    // it's a test, if it goes bang we've failed
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

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

#if (U_CFG_TEST_PIN_A >= 0) && (U_CFG_TEST_PIN_B >= 0) && \
    (U_CFG_TEST_PIN_C >= 0)
/** Test GPIOs.
 */
U_PORT_TEST_FUNCTION("[port]", "portGpioRequiresSpecificWiring")
{
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
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
    uPortTaskBlock(1);

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

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}
#endif

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)
/** Test UART.
 */
U_PORT_TEST_FUNCTION("[port]", "portUartRequiresSpecificWiring")
{
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

#if ((U_CFG_TEST_PIN_UART_A_CTS < 0) && (U_CFG_TEST_PIN_UART_A_CTS_GET >=0)) || \
    ((U_CFG_TEST_PIN_UART_A_RTS < 0) && (U_CFG_TEST_PIN_UART_A_RTS_GET >=0))
    // If no CTS/RTS pin is set but the _GET macro returns an actual
    // pin then that means that the platform we're running on cannot
    // set the pins at run-time, only at compile-time; here we can only do
    // whatever those pins have been fixed to do, so run the test with
    // flow control only.
    runUartTest(50000, 115200, true);
#else
    // Either the platform can set pins at run-time or it can't and the
    // flow control pins are not connected so run UART test at 115,200
    // without flow control
    runUartTest(50000, 115200, false);
    if ((U_CFG_TEST_PIN_UART_A_CTS_GET >= 0) &&
        (U_CFG_TEST_PIN_UART_A_RTS_GET >= 0)) {
        // Must be on a platform where the pins can be set at run-time
        // and the flow control pins are connected so test with flow control
        runUartTest(50000, 115200, true);
    }
#endif

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}
#endif

/** Test crypto: not a rigorous test, more
 * a "hello world".
 */
U_PORT_TEST_FUNCTION("[port]", "portCrypto")
{
    char buffer[64];
    char iv[U_PORT_CRYPTO_AES128_INITIALISATION_VECTOR_LENGTH_BYTES];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // On some platforms (e.g. ESP32) the crypto libraries
    // allocate a semaphore when they are first called
    // which is never deleted.  To avoid that getting in their
    // way of our heap loss calculation, make a call to one
    // of the crypto functions here.
    uPortCryptoSha256(gSha256Input, sizeof(gSha256Input) - 1,
                      buffer);
    memset(buffer, 0, sizeof(buffer));

    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    uPortLog("U_PORT_TEST: testing SHA256...\n");

    U_PORT_TEST_ASSERT(uPortCryptoSha256(gSha256Input,
                                         sizeof(gSha256Input) - 1,
                                         buffer) == 0);
    U_PORT_TEST_ASSERT(memcmp(buffer, gSha256Output,
                              U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) == 0);

    uPortLog("U_PORT_TEST: testing HMAC SHA256...\n");
    U_PORT_TEST_ASSERT(uPortCryptoHmacSha256(gHmacSha256Key,
                                             sizeof(gHmacSha256Key) - 1,
                                             gHmacSha256Input,
                                             sizeof(gHmacSha256Input) - 1,
                                             buffer) == 0);
    U_PORT_TEST_ASSERT(memcmp(buffer, gHmacSha256Output,
                              U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) == 0);

    uPortLog("U_PORT_TEST: testing AES CBC 128...\n");
    memcpy(iv, gAes128CbcIV, sizeof(iv));
    U_PORT_TEST_ASSERT(uPortCryptoAes128CbcEncrypt(gAes128CbcKey,
                                                   sizeof(gAes128CbcKey) - 1,
                                                   iv, gAes128CbcClear,
                                                   sizeof(gAes128CbcClear) - 1,
                                                   buffer) == 0);
    U_PORT_TEST_ASSERT(memcmp(buffer, gAes128CbcEncrypted,
                              sizeof(gAes128CbcEncrypted) - 1) == 0);

    memcpy(iv, gAes128CbcIV, sizeof(iv));
    U_PORT_TEST_ASSERT(uPortCryptoAes128CbcDecrypt(gAes128CbcKey,
                                                   sizeof(gAes128CbcKey) - 1,
                                                   iv, gAes128CbcEncrypted,
                                                   sizeof(gAes128CbcEncrypted) - 1,
                                                   buffer) == 0);
    U_PORT_TEST_ASSERT(memcmp(buffer, gAes128CbcClear,
                              sizeof(gAes128CbcClear) - 1) == 0);

    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[port]", "portCleanUp")
{
    int32_t x;

    x = uPortTaskStackMinFree(NULL);
    uPortLog("U_PORT_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", x);
    U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_PORT_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
