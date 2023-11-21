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
 * @brief Test for the port API: these should pass on all platforms.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // rand()
#include "string.h"    // strlen() and strcmp()
#include "stdio.h"     // snprintf()
#include "time.h"      // time_t and struct tm
#include "u_compiler.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port_clib_platform_specific.h" /* strtok_r(), mktime(), gmtime_r() and
                                              integer stdio, must be included
                                              before the other port files if
                                              any print or scan function
                                              is used. */
#include "u_port_clib_mktime64.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_gpio.h"
//lint -esym(766, u_port_uart.h) Suppress not referenced, which will be the case if U_PORT_TEST_CHECK_TIME_TAKEN is defined
#include "u_port_uart.h"
#if (U_CFG_APP_GNSS_I2C >= 0)
# include "u_port_i2c.h"
#endif
#if (U_CFG_APP_GNSS_SPI >= 0)
# include "u_port_spi.h"
#endif
#if (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0)
# include "u_ubx_protocol.h"
#endif
#include "u_port_crypto.h"
#include "u_port_event_queue.h"
#include "u_error_common.h"

#include "u_assert.h"

#include "u_test_util_resource_check.h"

#ifdef CONFIG_IRQ_OFFLOAD // To test semaphore from ISR in zephyr
#include <version.h>
#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/irq_offload.h>
#else
#include <irq_offload.h>
#endif
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_PORT_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef _WIN32
/** Check time delays on all platforms except _WIN32: on _WIN32
 * the tests are run on the same machine as all of the compilation
 * processes etc. and hence any attempt to check real-timeness
 * is futile.
 */
# define U_PORT_TEST_CHECK_TIME_TAKEN
#endif

#if defined(_WIN32) || defined(__ZEPHYR__) || defined(EL_PRODUCT_THREADX)
/** On some OSes it is possible to delete a task from another task,
 * so we can check that.
 */
# define U_PORT_TEST_DELETE_OTHER_TASK
#endif

/** The queue length to create during testing.
 */
#define U_PORT_TEST_QUEUE_LENGTH 20

/** The size of each item on the queue during testing.
 */
#define U_PORT_TEST_QUEUE_ITEM_SIZE sizeof(int32_t)

/** The task block duration to use in testing the
 * time for which a block lasts.  This needs to
 * be quite long as any error must be visible
 * in the test duration as measured by the
 * test system which is logging the test output.
 */
#define U_PORT_TEST_OS_BLOCK_TIME_MS 5000

#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
/** The guard time for the OS test.
 */
# define U_PORT_TEST_OS_GUARD_DURATION_MS 7000

/** Tolerance on block time.  Note that this needs
 * to be large enough to account for the tick coarseness
 * on all platforms.  For instance, on ESP32 the default
 * tick is 10 ms.
 */
# define U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS 150
#else
/** On platform where we can't rely on timing, we
 * allow up to this long for the osTestTask to lock the
 * mutex which indicates that it is running.
  */
# define U_PORT_TEST_OS_TEST_TASK_WAIT_SECONDS 60
#endif // #ifdef U_PORT_TEST_CHECK_TIME_TAKEN

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)
# ifdef U_PORT_TEST_CHECK_TIME_TAKEN
/** The amount of time to wait for the UART-loopbacked
 * data to arrive back normally.
 */
#  define U_PORT_TEST_UART_TIME_TO_ARRIVE_MS 1000
# else
/** The amount of time to wait for the UART-loopbacked
 * data to arrive back when allowing laziness (e.g. on
 * a heavily loaded Windoze machine).
 */
#  define U_PORT_TEST_UART_TIME_TO_ARRIVE_MS 10000
# endif
#endif

#if (U_CFG_APP_GNSS_I2C >= 0)
# ifndef U_PORT_TEST_I2C_ADDRESS
/** The I2C address to use when testing, which is the
 * default I2C address of a u-blox GNSS device.
 */
#  define U_PORT_TEST_I2C_ADDRESS 0x42
# endif

/** The size of the fixed part of a UBX-MON_VER message.
 */
# define U_PORT_TEST_I2C_UBX_MON_VER_FIXED_LENGTH 40

# ifndef U_PORT_TEST_I2C_UBX_MON_VER_MAX_SEGMENTS
/** The maximum number of optional segments we can
 * receive in a UBX-MON_VER message.
 */
#  define U_PORT_TEST_I2C_UBX_MON_VER_MAX_SEGMENTS 10
# endif

# ifndef U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES
/** Buffer length for an I2C message, which is enough for
 * the fixed part of a UBX-MON-VER message plus
 * U_PORT_TEST_I2C_UBX_MON_VER_MAX_SEGMENTS segments
 */
#  define U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES (U_PORT_TEST_I2C_UBX_MON_VER_FIXED_LENGTH + (30 * U_PORT_TEST_I2C_UBX_MON_VER_MAX_SEGMENTS) + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES)
# endif

# ifndef U_PORT_TEST_I2C_RETRIES
/** The number of times to retry the I2C test if the
 * value we get back from the device at the far end
 * is not as expected.
 */
#  define U_PORT_TEST_I2C_RETRIES 3
# endif
#endif

#if (U_CFG_APP_GNSS_SPI >= 0)
# ifndef U_PORT_TEST_SPI_TRIES
/** How many attempts we make while waiting for the GNSS
 * chip to respond with an ack.  The limiting factor here
 * is the Linux case where the SPI input gets buffered
 * from very early on and so there can be a lot more
 * to process.
 */
#  define U_PORT_TEST_SPI_TRIES 100
# endif
#endif

/** The number of re-entrancy test tasks to run.
 */
#define U_PORT_TEST_OS_NUM_REENT_TASKS 3

/** Fill value for the heap.
 */
#define U_PORT_TEST_OS_MALLOC_FILL ((int32_t) 0xdeadbeef)

/** The amount of memory to pUPortMalloc()ate during re-entrancy
 * testing.
 */
#define U_PORT_TEST_OS_MALLOC_SIZE_INTS ((int32_t) (1024 / sizeof(int32_t)))

/** Number of interations for the event queue test.
 * Must be less than 256.
 */
#define U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS 100

/** The minimum item size for the event queue test: we used
 * to fix this at 1 however there are some OS's which, internally,
 * allocate space in words, hence it is 4 for greater compatibility.
 */
#define U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES 4

#ifndef U_PORT_MALLOC_LENGTH_BYTES
/** How much to allocate in the heap test; deliberately an odd size.
 */
# define U_PORT_MALLOC_LENGTH_BYTES 9
#endif

/** How long to wait to receive  a message on a queue in osTestTask.
 */
#define U_PORT_OS_TEST_TASK_TRY_RECEIVE_MS 10

#ifndef U_PORT_TEST_CRITICAL_SECTION_TEST_TASK_START_TIME_SECONDS
/** How long to wait for the critical section test task to start,
 * leaving plenty of time for Windows.
 */
# define U_PORT_TEST_CRITICAL_SECTION_TEST_TASK_START_TIME_SECONDS 10
#endif

#ifndef U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS
/** How long to wait to check that the critical section is no longer
 * in effect: needs to be large to allow for Windows slop and small
 * enough not to cause any platform-specific watchdog to fire on an
 * embedded target.
 */
# ifdef _WIN32
#  define U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS 5000
# else
#  define U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS 20
# endif
#endif

#ifndef U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS
/** How long to wait to check the critical section: needs to be
 * large to allow for Windows slop and small enough not to cause
 * any platform-specific watchdog to fire on an embedded target.
 */
# ifdef _WIN32
#  define U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS 5000
# else
#  define U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS U_CFG_OS_YIELD_MS
# endif
#endif

#ifndef U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_LOOPS
/** If time does not pass during a critical section (e.g. on
 * our STM32F4 port it does not) then we can't use
 * U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS so in those
 * cases we just have to busy-wait for this number of loops.
 * Question is, what should the value be?  It is obviously a
 * compromise between platforms/CPU-clock-rates, needs to be big
 * enough for at least one RTOS tick to have passed and not so
 * large as to trip-up any interrupt watchdog (ESP-IDF has one
 * of those).
 */
//lint -esym(750, U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_LOOPS) Suppress
// not referenced, which might be the case if we're on Windows
# define U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_LOOPS 1000000
#endif

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

/** What the UART test should do about flow control.
*/
typedef enum {
    U_PORT_TEST_UART_FLOW_CONTROL_IS_USED,
    U_PORT_TEST_UART_FLOW_CONTROL_IS_NOT_USED,
    U_PORT_TEST_UART_FLOW_CONTROL_UNKNOWN_AT_COMPILE_TIME
} uPortTestUartFlowControl_t;

#endif

/** Struct for mktime64() and gmtime_r() testing.
 */
typedef struct {
    struct tm timeStruct;
    int64_t time;
} uPortTestTimeData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// OS test mutex handle.
static uPortMutexHandle_t gMutexHandle = NULL;

// OS test semaphore handle.
static uPortSemaphoreHandle_t gSemaphoreHandle = NULL;

// OS test queue handle for data.
static uPortQueueHandle_t gQueueHandleData = NULL;

// OS test queue handle for control.
static uPortQueueHandle_t gQueueHandleControl = NULL;

// OS test task handle.
static uPortTaskHandle_t gTaskHandle = NULL;

// OS task parameter.
static char gTaskParameter[6];

// Flag to indicate that the OS test task is running.
static bool gOsTestTaskHasLockedMutex;

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

#if (U_CFG_APP_GNSS_I2C >= 0)
/** I2C handle, global so that we can tidy up on failure;
 * I2C buses can easily get stuck, it would seem.
 */
static int32_t gI2cHandle = -1;

/** Buffer for a message read over I2C.
 */
static char *gpI2cBuffer = NULL;
#endif

#if (U_CFG_APP_GNSS_SPI >= 0)
/** SPI handle, global so that we can tidy up on failure.
 */
static int32_t gSpiHandle = -1;
#endif

/** Data for mktime64() and gmtime_r() testing.
 */
static uPortTestTimeData_t timeTestData[] = {
    {{0,  0, 0,  1, 0,  70,  4,   0, 0}, 0},
    {{1,  0, 0,  1, 0,  70,  4,   0, 0}, 1},
    {{1,  1, 0,  1, 0,  70,  4,   0, 0}, 61},
    {{1,  1, 1,  1, 0,  70,  4,   0, 0}, 3661},
    {{1,  1, 1,  1, 1,  70,  0,  31, 0}, 2682061},
    {{1,  1, 1,  1, 1,  70,  0,  31, 0}, 2682061},
    {{1,  1, 1,  1, 1,  70,  0,  31, 0}, 2682061},
    {{1,  1, 1,  1, 1,  70,  0,  31, 0}, 2682061},
    {{1,  1, 1,  1, 1,  70,  0,  31, 1}, 2682061},
    {{61, 0, 0,  1, 0,  72,  6,   0, 0}, 63072061},  // Leap year
    {{61, 0, 0,  1, 0,  70,  4,   0, 0}, 61},
    {{61, 0, 0,  1, 0,  70,  4,   0, 0}, 61},
    {{0, 59, 0,  1, 0,  70,  4,   0, 0}, 3540},
    {{0,  0, 23, 1, 0,  70,  4,   0, 0}, 82800},
    {{0,  0, 0, 31, 0,  70,  6,  30, 0}, 2592000},
    {{0,  0, 0,  1, 11, 70,  2, 334, 0}, 28857600},
    {{0,  0, 0, 13, 0,  70,  2,  12, 0}, 1036800},
    {{0,  0, 0,  1, 0, 137,  4,   0, 0}, 2114380800LL},
    {{0,  0, 0,  1, 0, 150,  4,   0, 0}, 2524608000LL}
};

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

/** Timer parameter value array; must have the same number of
 * entries as gTimerHandle.
 */
static int32_t gTimerParameterValue[4] = {0};

/** Index into the gTimerParameterValue array.
 */
static size_t gTimerParameterIndex = 0;

/** Timer handle array; must have the same number of
 * entries as gTimerParameterValue.
 */
static uPortTimerHandle_t gTimerHandle[4] = {0};

/** A variable to use during critical section and heap testing.
 */
static uint32_t gVariable = 0;

/** A place to hook an allocated block during heap testing.
 */
static void *gpMalloc = NULL;

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
    char *pSaved;
    char str[6];
    char buffer[32];
    int32_t checkInt = ((U_PORT_TEST_OS_MALLOC_FILL) & ~0xFF) | index;
    int32_t mallocSizeInts = 1 + (rand() % (U_PORT_TEST_OS_MALLOC_SIZE_INTS - 1));

    // Wait for it...
    while (gWaitForGo) {
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Malloc a random amount of memory and fill it with
    // a known value unique to this task, yielding while
    // doing it so that others can get in and mess it up
    pMem = (int32_t *) pUPortMalloc(mallocSizeInts * sizeof(int32_t));
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

        // Do a strtok_r()
        strtok_r(str, ",", &pSaved);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        strtok_r(NULL, ",", &pSaved);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        pStr = strtok_r(NULL, ",", &pSaved);
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

        // Check what ended up in the buffer we wrote earlier
        pStr = strtok_r(buffer, " ", &pSaved);
        if (pStr != NULL) {
            // First should be "4294967295"
            if (strcmp(pStr, "4294967295") == 0) {
                // Next should be the index
                pStr = strtok_r(NULL, " ", &pSaved);
                if (pStr != NULL) {
                    if (strtol(pStr, NULL, 10) == index) {
                        // And finally, the single character 'c'
                        pStr = strtok_r(NULL, " ", &pSaved);
                        if (pStr != NULL) {
                            if (strlen(pStr) == 1) {
                                if (*pStr != 'c') {
                                    returnCode = -8;
                                }
                            } else {
                                returnCode = -7;
                            }
                        } else {
                            returnCode = -6;
                        }
                    } else {
                        returnCode = -5;
                    }
                } else {
                    returnCode = -4;
                }
            } else {
                returnCode = -3;
            }
        } else {
            returnCode = -2;
        }

        // Check that the memory we allocated still contains
        // what we put there
        pTmp = pMem;
        for (int32_t x = 0; (returnCode == 0) &&
             (x < mallocSizeInts); x++) {
            if (*pTmp != checkInt) {
                returnCode = -9;
            }
            pTmp++;
        }

        // Free the memory again
        uPortFree(pMem);

        // Run around doing more malloc/compare/free with random
        // amounts of memory and yielding just to mix things up
        uPortLog("U_PORT_TEST_OS_REENT_TASK_%d: please wait while pUPortMalloc()"
                 " is thrashed...\n", index);
        for (size_t x = 0; (returnCode == 0) && (x < 100); x++) {
            mallocSizeInts = 1 + (rand() % (U_PORT_TEST_OS_MALLOC_SIZE_INTS - 1));
            pMem = (int32_t *) pUPortMalloc(mallocSizeInts * sizeof(int32_t));
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
                        returnCode = -10;
                    }
                    pTmp++;
                }
            } else {
                returnCode = -11;
            }
            uPortFree(pMem);
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

#ifndef U_PORT_TEST_DELETE_OTHER_TASK
    // And delete ourselves
    uPortTaskDelete(NULL);
#endif
}

// The test task for OS stuff.
//lint -esym(818, pParameters) Suppress "could be const"
// since this has to match the function signature
// exactly to avoid a compiler warning
static void osTestTask(void *pParameters)
{
    int32_t queueItem = 0;
    int32_t index = 0;
    int32_t x = 0;
    int32_t y;
    uPortTaskHandle_t taskHandle = NULL;
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
             " \"%s\".\n", U_PTR_TO_INT32(gTaskHandle), pParameters,
             (const char *) pParameters);
    U_PORT_TEST_ASSERT(strcmp((const char *) pParameters, gTaskParameter) == 0);

    U_PORT_TEST_ASSERT(uPortTaskIsThis(gTaskHandle));
    U_PORT_TEST_ASSERT(uPortTaskGetHandle(NULL) < 0);
    U_PORT_TEST_ASSERT(uPortTaskGetHandle(&taskHandle) == 0);
    uPortLog("U_PORT_TEST_OS_TASK: uPortTaskGetHandle() returned 0x%08x\n", taskHandle);
    U_PORT_TEST_ASSERT(gTaskHandle == taskHandle);

#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    // Only do this if we can rely on timing, since if this
    // task doesn't run immediately rhe lock is given to
    // it the calling task's tryLock might succeed (which we
    // sometimes see on Windows)
    uPortLog("U_PORT_TEST_OS_TASK: task trying to lock the mutex.\n");
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 500) == 0);
    uPortLog("U_PORT_TEST_OS_TASK: task trying to lock the mutex again, should fail!.\n");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) != 0);
    uPortLog("U_PORT_TEST_OS_TASK: unlocking it again.\n");
    U_PORT_TEST_ASSERT(uPortMutexUnlock(gMutexHandle) == 0);
#endif

    uPortLog("U_PORT_TEST_OS_TASK: locking it again (non-try version).\n");
    U_PORT_MUTEX_LOCK(gMutexHandle);

    gOsTestTaskHasLockedMutex = true;

    U_PORT_TEST_ASSERT(gQueueHandleControl != NULL);
    U_PORT_TEST_ASSERT(gQueueHandleData != NULL);
    uPortLog("U_PORT_TEST_OS_TASK: task waiting on queue for data.\n");
    while (queueItem >= 0) {
        U_PORT_TEST_ASSERT(uPortQueueReceive(gQueueHandleData,
                                             &queueItem) == 0);
        uPortLog("U_PORT_TEST_OS_TASK: task received %d.\n", queueItem);
        if ((queueItem >= 0) &&
            (index < (int32_t) (sizeof(gStuffToSend) / sizeof(gStuffToSend[0])))) {
            uPortLog("                     item %d, expecting %d.\n",
                     index + 1, gStuffToSend[index]);
            U_PORT_TEST_ASSERT(gStuffToSend[index] == queueItem);
            index++;
        }
        x = 0;
        y = uPortQueuePeek(gQueueHandleControl, &x);
        U_PORT_TEST_ASSERT((y == 0) ||
                           (y == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
                           (y == (int32_t) U_ERROR_COMMON_TIMEOUT));

        if (uPortQueueTryReceive(gQueueHandleControl,
                                 U_PORT_OS_TEST_TASK_TRY_RECEIVE_MS,
                                 &queueItem) == 0) {
            uPortLog("U_PORT_TEST_OS_TASK: task received %d on control"
                     " queue.\n", queueItem);
            U_PORT_TEST_ASSERT(queueItem == -1);
            U_PORT_TEST_ASSERT((y < 0) || (x == queueItem));
        }
        uPortLog("U_PORT_TEST_OS_TASK: queueItem %d.\n", queueItem);
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
        // we expect to receive paramLength of
        // U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES where
        // *pParam is a count of the number of times we've
        // been called.
        if (paramLength != U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES) {
            gEventQueueMinErrorFlag = U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES;
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
    int32_t receiveSizeOrError;
    int32_t actualSizeOrError;
    uartEventCallbackData_t *pEventCallbackData = (uartEventCallbackData_t *) pParameters;

    pEventCallbackData->callCount++;
    if (filter != U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        pEventCallbackData->errorCode = -1;
    } else {
        // Run until we spot an error or run out of data
        do {
            receiveSizeOrError = uPortUartGetReceiveSize(uartHandle);
            // Since the initial part of the test is to send a manual
            // "there some data" message, even though there isn't any
            // in the buffer yet, we shouldn't go on to read the
            // data unless we know there really is some; otherwise
            // uPortUartRead() could get stuck, holding a mutex out
            // down in the porting layer and preventing us from doing
            // the uPortUartWrite() part of the test.  This sending
            // data and receiving it ourselves isn't a normal case,
            // it only occurs during testing.
            actualSizeOrError = 0;
            if (receiveSizeOrError > 0) {
                actualSizeOrError = uPortUartRead(uartHandle,
                                                  pEventCallbackData->pReceive,
                                                  gUartBuffer + sizeof(gUartBuffer) -
                                                  pEventCallbackData->pReceive);
                if (actualSizeOrError < 0) {
                    pEventCallbackData->errorCode = -2;
                }
                // actualSizeOrError will be smaller than receiveSizeOrError
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
                if (actualSizeOrError > U_CFG_TEST_UART_BUFFER_LENGTH_BYTES) {
                    pEventCallbackData->errorCode = -5;
                }
                // Compare the data with the expected data
                for (int32_t x = 0; (pEventCallbackData->errorCode == 0) &&
                     (x < actualSizeOrError); x++) {
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
                        pEventCallbackData->errorCode = -6;
                    }
                }
            }
        } while ((actualSizeOrError > 0) && (pEventCallbackData->errorCode == 0));
    }
}

// Run a UART test at the given baud rate and with/without flow control.
static void runUartTest(int32_t size, int32_t speed,
                        uPortTestUartFlowControl_t flowControl)
{
    int32_t uartHandle;
    uartEventCallbackData_t eventCallbackData = {0};
    int32_t bytesToSend;
    int32_t bytesSent = 0;
    int32_t pinCts;
    int32_t pinRts;
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;
    int32_t stackMinFreeBytes;
    int32_t x;
    const char *pFlowControl = "?";

    eventCallbackData.callCount = 0;
    eventCallbackData.pReceive = gUartBuffer;

    // Grab here the pins that would be passed to
    // uPortUartOpen(), not the _GET versions.  On
    // a platform where the pins are set at compile
    // time these values will be -1, ignored.
    pinCts = U_CFG_TEST_PIN_UART_A_CTS;
    pinRts = U_CFG_TEST_PIN_UART_A_RTS;

#if defined(U_CFG_TEST_PIN_UART_A_CTS_GET) && defined (U_CFG_TEST_PIN_UART_A_RTS_GET)
    // Print where the pins are actually connected, that's what
    // the user needs to know.  On a platform which can set
    // the pins at run-time the values will be the same
    // as the pinCts and pinRts values.
    uPortLog(U_TEST_PREFIX "UART CTS is on pin %d and RTS on"
             " pin %d", U_CFG_TEST_PIN_UART_A_CTS_GET,
             U_CFG_TEST_PIN_UART_A_RTS_GET);
#else
    uPortLog(U_TEST_PREFIX "UART CTS is on pin %d and RTS on"
             " pin %d", pinCts, pinRts);
#endif

    if (flowControl != U_PORT_TEST_UART_FLOW_CONTROL_UNKNOWN_AT_COMPILE_TIME) {
        uPortLog(" but we're going to ignore them for this test.\n");
        if (flowControl == U_PORT_TEST_UART_FLOW_CONTROL_IS_USED) {
            pFlowControl = "on";
        } else {
            pFlowControl = "off";
        }
        // If we want to test with flow control off
        // but the flow control pins are actually
        // connected then they need to be set
        // to "get on with it"
        if (pinCts >= 0) {
            // Make CTS an output pin and low
            x = uPortGpioSet(pinCts, 0);
            // On Windows GPIOs aren't supported but
            // pinCts is still used as a flow control
            // on/off indicator
            U_PORT_TEST_ASSERT((x == 0) ||
                               (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
            gpioConfig.pin = pinCts;
            gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
            x = uPortGpioConfig(&gpioConfig);
            U_PORT_TEST_ASSERT((x == 0) ||
                               (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }
        if (pinRts >= 0) {
            // Make RTS an output pin and low
            x = uPortGpioSet(pinRts, 0);
            // On Windows GPIOs aren't supported but
            // pinRts is still used as a flow control
            // on/off indicator
            U_PORT_TEST_ASSERT((x == 0) ||
                               (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
            gpioConfig.pin = pinRts;
            gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
            x = uPortGpioConfig(&gpioConfig);
            U_PORT_TEST_ASSERT((x == 0) ||
                               (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }
        pinCts = -1;
        pinRts = -1;
    } else {
        uPortLog(".\n");
    }

    U_TEST_PRINT_LINE("testing UART loop-back, %d byte(s) at %d"
                      " bits/s with flow control %s.", size, speed,
                      pFlowControl);

#ifdef U_CFG_TEST_UART_PREFIX
    U_PORT_TEST_ASSERT(uPortUartPrefix(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_UART_PREFIX)) == 0);
#endif

    U_TEST_PRINT_LINE("add a UART instance...");
    uartHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                               speed, NULL,
                               U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_TEST_PIN_UART_A_TXD,
                               U_CFG_TEST_PIN_UART_A_RXD,
                               pinCts, pinRts);
    U_PORT_TEST_ASSERT(uartHandle >= 0);

    U_TEST_PRINT_LINE("add a UART event callback which will receive the data...");
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

    // Can't easily check that the CTS suspend/resume functions work
    // and, in any case, they may not be supported so simply call them
    // both here, before the main body of the test, to check that they
    // don't crash anything and that the test works afterwards
    x = uPortUartCtsSuspend(uartHandle);
    U_PORT_TEST_ASSERT((x == 0) ||
                       (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
    uPortUartCtsResume(uartHandle);

    // Manually send an Rx event and check that it caused
    // the callback to be called
    U_PORT_TEST_ASSERT(eventCallbackData.callCount == 0);
    U_PORT_TEST_ASSERT(uPortUartEventSend(uartHandle,
                                          (uint32_t) U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) ==
                       0);

#ifndef U_PORT_TEST_CHECK_TIME_TAKEN
    // Some platforms (e.g. Windows) can be a little slow at this
    uPortTaskBlock(1000);
#endif

    U_PORT_TEST_ASSERT(eventCallbackData.callCount == 1);

    // Do the manual send again, this time with the "try" version,
    // where supported
    eventCallbackData.callCount = 0;
    x = uPortUartEventTrySend(uartHandle,
                              (uint32_t) U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                              0);
    if (x == 0) {
#ifndef U_PORT_TEST_CHECK_TIME_TAKEN
        // Some platforms can be a little slow at this
        uPortTaskBlock(1000);
#endif
        U_PORT_TEST_ASSERT(eventCallbackData.callCount == 1);
    } else {
        U_PORT_TEST_ASSERT((x == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
                           (x == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
    }

    // Send data over the UART N times, the callback will check it
    U_TEST_PRINT_LINE("sending data.");
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
        U_TEST_PRINT_LINE("%d byte(s) sent.", bytesSent);
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
    U_TEST_PRINT_LINE("waiting %d ms for everything to arrive back.",
                      U_PORT_TEST_UART_TIME_TO_ARRIVE_MS);
    uPortTaskBlock(U_PORT_TEST_UART_TIME_TO_ARRIVE_MS);

    // Print out some useful stuff
    if (eventCallbackData.errorCode == -5) {
        U_TEST_PRINT_LINE("error after %d character(s), %d block(s).",
                          eventCallbackData.bytesReceived,
                          eventCallbackData.blockNumber);
        U_TEST_PRINT_LINE("expected %c (0x%02x), received %c (0x%02x).",
                          gUartTestData[eventCallbackData.indexInBlock],
                          gUartTestData[eventCallbackData.indexInBlock],
                          eventCallbackData.pReceive, eventCallbackData.pReceive);
    } else if (eventCallbackData.errorCode < 0) {
        U_TEST_PRINT_LINE("finished with error code %d after"
                          " correctly receiving %d byte(s).",
                          eventCallbackData.errorCode,
                          eventCallbackData.bytesReceived);
    }

    U_TEST_PRINT_LINE("at end of test %d byte(s) sent, %d byte(s) received.",
                      bytesSent, eventCallbackData.bytesReceived);
    U_PORT_TEST_ASSERT(eventCallbackData.bytesReceived == bytesSent);

    // Check the stack extent for the task on the end of the
    // event queue
    stackMinFreeBytes = uPortUartEventStackMinFree(uartHandle);
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("UART event queue task had %d byte(s) free out of %d.",
                          stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    U_TEST_PRINT_LINE("tidying up after UART test...");
    uPortUartClose(uartHandle);
}

#endif // (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)

#if (U_CFG_APP_GNSS_SPI >= 0)

// Compare SPI fill words, doing it for the right word length.
static bool fillWordSame(uint16_t wordA, uint16_t wordB, size_t lengthBytes)
{
    bool isSame = false;

    switch (lengthBytes) {
        case 1:
            isSame = ((wordA & 0xFF) == (wordB & 0xFF));
            break;
        case 2:
            isSame = ((wordA & 0xFFFF) == (wordB & 0xFFFF));
            break;
        case 3:
            isSame = ((wordA & 0xFFFFFF) == (wordB & 0xFFFFFF));
            break;
        case 4:
            isSame = ((wordA & 0xFFFFFFFF) == (wordB & 0xFFFFFFFF));
            break;
        default:
            break;
    }

    return isSame;
}

#endif

// Timer callback
static void timerCallback(const uPortTimerHandle_t timerHandle, void *pParameter)
{
    //lint -e(507) Suppress size incompatibility, we know what we're doing
    int32_t parameter = U_PTR_TO_INT32(pParameter);

    (void) timerHandle;

    // Increment the gTimerParameterValue entry at index parameter
    if ((parameter >= 0) &&
        (parameter < (int32_t) (sizeof(gTimerParameterValue) / sizeof(gTimerParameterValue[0])))) {
        gTimerParameterValue[parameter]++;
    }
}

// The test task for critical sections: if it can lock
// gMutex it increments the uint32_t variable it was passed
// in pParameter in a loop, else it exits
static void criticalSectionTestTask(void *pParameter)
{
    volatile uint32_t *pVariable = (uint32_t *) pParameter;

    while (uPortMutexTryLock(gMutexHandle, 0) == 0) {
        uPortMutexUnlock(gMutexHandle);
        (*pVariable)++;
        uPortTaskBlock(10);
    }

    uPortTaskDelete(NULL);
}

#if (U_CFG_APP_GNSS_I2C >= 0) && !defined(U_PORT_TEST_DISABLE_I2C)
// Reset a GNSS chip attached via I2C
static bool gnssReset()
{
    bool resetDone = false;

# if defined(U_CFG_TEST_PIN_GNSS_RESET_N) && (U_CFG_TEST_PIN_GNSS_RESET_N >= 0)
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;

    // If there is a GNSS module attached that has a RESET_N line
    // wired to it then pull that line low to reset the GNSS module,
    // nice and clean
    U_TEST_PRINT_LINE("resetting GNSS module by toggling pin %d (0x%0x) low.",
                      U_CFG_TEST_PIN_GNSS_RESET_N, U_CFG_TEST_PIN_GNSS_RESET_N);
    // Make the pin an open-drain output, and low
    if (uPortGpioSet(U_CFG_TEST_PIN_GNSS_RESET_N, 0) == 0) {
        gpioConfig.pin = U_CFG_TEST_PIN_GNSS_RESET_N;
        gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
        gpioConfig.driveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
        uPortGpioConfig(&gpioConfig);
        // Leave it low for half a second and release
        uPortTaskBlock(500);
        uPortGpioSet(U_CFG_TEST_PIN_GNSS_RESET_N, 1);
        // Let the chip recover
        uPortTaskBlock(2000);
        resetDone = true;
    }
# endif

    return resetDone;
}
#endif

// An assert hook function.
static void assertFunction(const char *pFileStr, int32_t line)
{
    (void) pFileStr;
    (void) line;

    gVariable = 1;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then deinitialise the porting layer.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[port]", "portInitialisation")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    uPortDeinit();
    // Printed for information: asserting happens at the end
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
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
 * re-entrancy failure appearing elsewhere.
 */
U_PORT_TEST_FUNCTION("[port]", "portRentrancy")
{
    bool finished = false;
    int32_t returnCode = 0;
    int32_t taskParameter[U_PORT_TEST_OS_NUM_REENT_TASKS];
    uPortTaskHandle_t taskHandle[U_PORT_TEST_OS_NUM_REENT_TASKS];
    int32_t stackMinFreeBytes;
    int32_t resourceCount;
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapClibLoss;
#endif
    struct tm tmStruct = {0,  0, 0,  1, 0,  70,  0, 0, 0};

    // On ESP-IDF mktime grabs memory when it first runs.
    // This should be sorted by the preamble test running
    // but on ESP-IDF we use the ESP-IDF unit test
    // environment which doesn't allow us to guarantee that
    // the preamble runs first; might have to change that
    // but for the moment do this here to get it out of our
    // sums.
    mktime(&tmStruct);

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    resourceCount = uTestUtilGetDynamicResourceCount();

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
    // can identify themselves in mallocs and
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
        if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
            U_TEST_PRINT_LINE("test task %d had %d byte(s) free out of %d.",
                              x + 1, stackMinFreeBytes, U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES);
            U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
        }
    }

    // Let them stop
    gWaitForStop = false;

#ifdef U_PORT_TEST_DELETE_OTHER_TASK
    for (size_t x = 0; (x < sizeof(taskHandle) /
                        sizeof(taskHandle[0])); x++) {
        U_PORT_TEST_ASSERT(uPortTaskDelete(taskHandle[x]) == 0);
    }
#endif

    // Let the idle task tidy-away the tasks
    uPortTaskBlock(1000);

    // If the returnCode is 0 then that
    // is success.  If it is negative
    // that it indicates an error.
    U_TEST_PRINT_LINE("reentrancy task(s) returned %d.", returnCode);
    U_PORT_TEST_ASSERT(returnCode == 0);

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test all the normal OS stuff.
 */
U_PORT_TEST_FUNCTION("[port]", "portOs")
{
    int32_t errorCode;
//lint -esym(838, startTimeMs) Suppress value not used/ referenced
//lint -esym(438, startTimeMs, timeNowMs) will be the case if we're not
//lint -esym(550, startTimeMs, timeNowMs) measuring time delays
    int32_t startTimeMs;
    int32_t timeNowMs;
    int32_t stackMinFreeBytes;
    int32_t y = -1;
    int32_t z;
    uPortQueueHandle_t queueHandle = NULL;
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    startTimeMs = uPortGetTickTimeMs();
    U_TEST_PRINT_LINE("tick time now is %d.", (int32_t) startTimeMs);

    U_TEST_PRINT_LINE("creating a mutex...");
    errorCode = uPortMutexCreate(&gMutexHandle);
    uPortLog("             returned error code %d, handle"
             " 0x%08x.\n", errorCode, gMutexHandle);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);

    U_TEST_PRINT_LINE("creating a data queue...");
    errorCode = uPortQueueCreate(U_PORT_TEST_QUEUE_LENGTH,
                                 U_PORT_TEST_QUEUE_ITEM_SIZE,
                                 &gQueueHandleData);
    uPortLog("             returned error code %d, handle"
             " 0x%08x.\n", errorCode, gQueueHandleData);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gQueueHandleData != NULL);
    errorCode = uPortQueueGetFree(gQueueHandleData);
    U_TEST_PRINT_LINE("%d entries free on data queue.", errorCode);
    U_PORT_TEST_ASSERT((errorCode == U_PORT_TEST_QUEUE_LENGTH) ||
                       (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED));

    U_TEST_PRINT_LINE("creating a control queue...");
    errorCode = uPortQueueCreate(U_PORT_TEST_QUEUE_LENGTH,
                                 U_PORT_TEST_QUEUE_ITEM_SIZE,
                                 &gQueueHandleControl);
    uPortLog("             returned error code %d, handle"
             " 0x%08x.\n", errorCode, gQueueHandleControl);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gQueueHandleControl != NULL);
    errorCode = uPortQueueGetFree(gQueueHandleControl);
    U_TEST_PRINT_LINE("%d entries free on control queue.", errorCode);
    U_PORT_TEST_ASSERT((errorCode == U_PORT_TEST_QUEUE_LENGTH) ||
                       (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED));
    U_TEST_PRINT_LINE("locking mutex, preventing task from executing.");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) == 0);

    gOsTestTaskHasLockedMutex = false;
    U_TEST_PRINT_LINE("creating a test task with stack %d byte(s)"
                      " and priority %d, passing it the pointer"
                      " 0x%08x containing the string \"%s\"...",
                      U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                      U_CFG_TEST_OS_TASK_PRIORITY,
                      gTaskParameter, gTaskParameter);
    errorCode = uPortTaskCreate(osTestTask, "osTestTask",
                                U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                (void *) gTaskParameter,
                                U_CFG_TEST_OS_TASK_PRIORITY,
                                &gTaskHandle);
    uPortLog("             returned error code %d, handle"
             " 0x%08x.\n", errorCode, gTaskHandle);
    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gTaskHandle != NULL);

    U_TEST_PRINT_LINE("time now %d ms.", (int32_t) uPortGetTickTimeMs());
    uPortTaskBlock(200);
    U_TEST_PRINT_LINE("unlocking mutex, allowing task to execute.");
    U_PORT_TEST_ASSERT(uPortMutexUnlock(gMutexHandle) == 0);;

#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    // Pause to let the task print its opening messages
    uPortTaskBlock(1200);
#else
    // On platforms where we can't rely on timing (e.g. Windows),
    // wait for the osTestTask to set a flag to indicate that it
    // has locked the mutex
    for (size_t x = 0; !gOsTestTaskHasLockedMutex &&
         (x < U_PORT_TEST_OS_TEST_TASK_WAIT_SECONDS); x++) {
        uPortTaskBlock(1000);
    }
    U_PORT_TEST_ASSERT(gOsTestTaskHasLockedMutex);
#endif

    U_TEST_PRINT_LINE("trying to lock the mutex, should fail...");
    U_PORT_TEST_ASSERT(uPortMutexTryLock(gMutexHandle, 10) != 0);

    U_TEST_PRINT_LINE("sending stuff to task...");
    for (size_t x = 0; x < sizeof(gStuffToSend) / sizeof(gStuffToSend[0]); x++) {
        // If this is the last thing then queue up a -1 on the control
        // queue so that the test task exits after receiving the
        // last item on the data queue
        if (x == sizeof(gStuffToSend) / sizeof(gStuffToSend[0]) - 1) {
            uPortTaskBlock(1000);
            stackMinFreeBytes = uPortTaskStackMinFree(gTaskHandle);
            if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
                U_TEST_PRINT_LINE("test task had %d byte(s) free out of %d.",
                                  stackMinFreeBytes, U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES);
                U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
            }

            U_TEST_PRINT_LINE("sending -1 to terminate test task"
                              " control queue and waiting for it to stop...");
            sendToQueue(gQueueHandleControl, -1);
        }
        // Actually send the stuff by passing it to a function
        // where it will be placed on the stack so as to check
        // that the stuff is copied rather than referenced.
        // Use the IRQ version as well as the normal one.
        if (x & 1) {
            sendToQueue(gQueueHandleData, gStuffToSend[x]);
        } else {
            z = sendToQueueIrq(gQueueHandleData, gStuffToSend[x]);
            if (z == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
                sendToQueue(gQueueHandleData, gStuffToSend[x]);
            }
        }
    }

    U_PORT_MUTEX_LOCK(gMutexHandle);
    // Yield to let it get the message
    uPortTaskBlock(U_CFG_OS_YIELD_MS);
    U_PORT_MUTEX_UNLOCK(gMutexHandle);
    U_TEST_PRINT_LINE("task stopped.");

    // Pause to let the task print its final messages
    uPortTaskBlock(1000);

    U_TEST_PRINT_LINE("deleting mutex...");
    uPortMutexDelete(gMutexHandle);

    U_TEST_PRINT_LINE("deleting queues...");
    U_PORT_TEST_ASSERT(uPortQueueDelete(gQueueHandleControl) == 0);
    U_PORT_TEST_ASSERT(uPortQueueDelete(gQueueHandleData) == 0);

    // Create a queue to test peek with
    U_PORT_TEST_ASSERT(uPortQueueCreate(U_PORT_TEST_QUEUE_LENGTH,
                                        U_PORT_TEST_QUEUE_ITEM_SIZE,
                                        &queueHandle) == 0);
    sendToQueue(queueHandle, 0xFF);
    z = uPortQueuePeek(queueHandle, &y);
    U_TEST_PRINT_LINE("peeking queue returned %d.", z);
    if (z == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) {
        U_PORT_TEST_ASSERT(y == -1);
        U_TEST_PRINT_LINE("peek is not supported on this platform.");
    } else {
        U_PORT_TEST_ASSERT(z == 0);
        U_TEST_PRINT_LINE("found %d on queue.", y);
        U_PORT_TEST_ASSERT(uPortQueueReceive(queueHandle, &z) == 0);
        U_PORT_TEST_ASSERT(z == 0xFF);
        U_PORT_TEST_ASSERT(y == z);
    }
    U_PORT_TEST_ASSERT(uPortQueueDelete(queueHandle) == 0);

    timeNowMs = uPortGetTickTimeMs() - startTimeMs;
    U_TEST_PRINT_LINE("according to uPortGetTickTimeMs()"
                      " the test took %d ms.", (int32_t) timeNowMs);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    U_PORT_TEST_ASSERT((timeNowMs > 0) &&
                       (timeNowMs < U_PORT_TEST_OS_GUARD_DURATION_MS));
#endif

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

static void osTestTaskSemaphoreGive(void *pParameters)
{
    (void) pParameters;

    uPortTaskBlock(500);
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) == 0);
    uPortTaskDelete(NULL);
}

#if (!defined(_WIN32) && !defined(__linux__)) || defined(CONFIG_IRQ_OFFLOAD)
static void osTestTaskSemaphoreGiveFromIsr(const void *pParameters)
{
    (void) pParameters;

    U_PORT_TEST_ASSERT(uPortSemaphoreGiveIrq(gSemaphoreHandle) == 0);
}
#endif

U_PORT_TEST_FUNCTION("[port]", "portOsSemaphore")
{
    int32_t errorCode;
    int32_t startTimeTestMs;
    int32_t startTimeMs;
    int32_t timeNowMs;
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    startTimeTestMs = uPortGetTickTimeMs();
    U_TEST_PRINT_LINE("tick time now is %d.", (int32_t) startTimeTestMs);

    U_TEST_PRINT_LINE("initialize a semaphore with invalid max limit.");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0,
                                            0) == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

    U_TEST_PRINT_LINE("initialize a semaphore with invalid count.");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 2,
                                            1) == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

    U_TEST_PRINT_LINE("verify that the semaphore waits and timeouts with TryTake.");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0, 1) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);
    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle,
                                             500) == (int32_t)U_ERROR_COMMON_TIMEOUT);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    int64_t diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_TEST_PRINT_LINE("diffMs %d.", (int32_t)diffMs);
    U_PORT_TEST_ASSERT(diffMs > 250 && diffMs < 750);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    U_TEST_PRINT_LINE("verify that the semaphore waits with Take and is taken.");
    uPortLog("             by this thread after given by second thread.\n");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0, 1) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);
    errorCode = uPortTaskCreate(osTestTaskSemaphoreGive, "osTestTaskSemaphoreGive",
                                U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                (void *) gTaskParameter,
                                U_CFG_TEST_OS_TASK_PRIORITY,
                                &gTaskHandle);

    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gTaskHandle != NULL);
    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTake(gSemaphoreHandle) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs > 250 && diffMs < 750);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    U_TEST_PRINT_LINE("verify that the semaphore waits with TryTake and is taken.");
    uPortLog("             by this thread after given by second thread.\n");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0, 1) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);
    errorCode = uPortTaskCreate(osTestTaskSemaphoreGive, "osTestTaskSemaphoreGive",
                                U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                (void *) gTaskParameter,
                                U_CFG_TEST_OS_TASK_PRIORITY,
                                &gTaskHandle);

    U_PORT_TEST_ASSERT(errorCode == 0);
    U_PORT_TEST_ASSERT(gTaskHandle != NULL);
    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle, 5000) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs > 250 && diffMs < 750);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    U_TEST_PRINT_LINE("verify that +2 as initialCount works for TryTake.");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 2, 3) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);
    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle, 5000) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs < 250);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle, 5000) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs < 250);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle,
                                             500) == (int32_t)U_ERROR_COMMON_TIMEOUT);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs > 250 && diffMs < 750);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    U_TEST_PRINT_LINE("verify that +2 as limit works for TryTake.");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0, 2) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) == 0); // Internal count is 1
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) == 0); // Internal count is 2
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) ==
                       0); // Internal count shall not be increased
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) ==
                       0); // Internal count shall not be increased

    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle, 5000) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs < 250);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle, 5000) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs < 250);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gSemaphoreHandle,
                                             500) == (int32_t)U_ERROR_COMMON_TIMEOUT);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs > 250 && diffMs < 750);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    U_TEST_PRINT_LINE("verify that the semaphore waits with Take and is taken.");
    uPortLog("             by this thread after given from ISR.\n");
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gSemaphoreHandle, 0, 1) == 0);
    U_PORT_TEST_ASSERT(gSemaphoreHandle != NULL);

#ifdef CONFIG_IRQ_OFFLOAD // Only really tested for zephyr for now
    irq_offload(osTestTaskSemaphoreGiveFromIsr, NULL);
#else
# if !defined(_WIN32) && !defined(__linux__)
    osTestTaskSemaphoreGiveFromIsr(NULL);
# else
    // ISR not supported on Windows or native Linux, do the non-ISR version
    // to keep the test going
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gSemaphoreHandle) == 0);
# endif
#endif

    startTimeMs = uPortGetTickTimeMs();
    U_PORT_TEST_ASSERT(uPortSemaphoreTake(gSemaphoreHandle) == 0);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    diffMs = uPortGetTickTimeMs() - startTimeMs;
    U_PORT_TEST_ASSERT(diffMs < 250);
#endif
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gSemaphoreHandle) == 0);

    timeNowMs = uPortGetTickTimeMs() - startTimeTestMs;
    U_TEST_PRINT_LINE("according to uPortGetTickTimeMs() the test took %d ms.",
                      (int32_t) timeNowMs);
#ifdef U_PORT_TEST_CHECK_TIME_TAKEN
    U_PORT_TEST_ASSERT((timeNowMs > 0) &&
                       (timeNowMs < U_PORT_TEST_OS_GUARD_DURATION_MS));
#endif

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#if (U_CFG_TEST_UART_A >= 0) && defined(U_PORT_TEST_CHECK_TIME_TAKEN)
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
    int32_t startTimeMs;
    int32_t timeNowMs;
    int32_t timeDelta;
    int32_t uartHandle;
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("running this test will take around %d second(s).",
                      (U_PORT_TEST_OS_BLOCK_TIME_MS * 3) / 1000);

    startTimeMs = uPortGetTickTimeMs();
    U_TEST_PRINT_LINE("tick time now is %d.", (int32_t) startTimeMs);

    U_TEST_PRINT_LINE("waiting %d ms...", U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeNowMs = uPortGetTickTimeMs();
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    U_TEST_PRINT_LINE("uPortTaskBlock(%d) blocked for %d ms.",
                      U_PORT_TEST_OS_BLOCK_TIME_MS,
                      (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    // Initialise the UART and re-measure time
    timeNowMs = uPortGetTickTimeMs();
    U_TEST_PRINT_LINE("tick time now is %d.", (int32_t) timeNowMs);
    U_TEST_PRINT_LINE("add a UART instance...");
#ifdef U_CFG_TEST_UART_PREFIX
    U_PORT_TEST_ASSERT(uPortUartPrefix(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_UART_PREFIX)) == 0);
#endif
    uartHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                               U_CFG_TEST_BAUD_RATE,
                               NULL,
                               U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_TEST_PIN_UART_A_TXD,
                               U_CFG_TEST_PIN_UART_A_RXD,
                               U_CFG_TEST_PIN_UART_A_CTS,
                               U_CFG_TEST_PIN_UART_A_RTS);
    U_PORT_TEST_ASSERT(uartHandle >= 0);
    U_TEST_PRINT_LINE("waiting %d ms...", U_PORT_TEST_OS_BLOCK_TIME_MS);
    //lint -e(838) Suppress previous value not used which
    // will occur if uPortLog() is compiled out
    timeNowMs = uPortGetTickTimeMs();
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    U_TEST_PRINT_LINE("uPortTaskBlock(%d) blocked for %d ms.",
                      U_PORT_TEST_OS_BLOCK_TIME_MS,
                      (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    U_TEST_PRINT_LINE("deinitialising UART...");
    timeNowMs = uPortGetTickTimeMs();
    uPortUartClose(uartHandle);

    U_TEST_PRINT_LINE("waiting %d ms...", U_PORT_TEST_OS_BLOCK_TIME_MS);
    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);
    timeDelta = uPortGetTickTimeMs() - timeNowMs;
    U_TEST_PRINT_LINE("uPortTaskBlock(%d) blocked for %d ms.",
                      U_PORT_TEST_OS_BLOCK_TIME_MS,
                      (int32_t) (timeDelta));
    U_PORT_TEST_ASSERT((timeDelta >= U_PORT_TEST_OS_BLOCK_TIME_MS -
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS) &&
                       (timeDelta <= U_PORT_TEST_OS_BLOCK_TIME_MS +
                        U_PORT_TEST_OS_BLOCK_TIME_TOLERANCE_MS));

    //lint -esym(438, timeDelta) Suppress value not used, which
    // will occur if uPortLog() is compiled out
    timeDelta = uPortGetTickTimeMs() - startTimeMs;
    U_TEST_PRINT_LINE("according to uPortGetTickTimeMs() the test took"
                      " %d second(s).", (int32_t) (timeDelta / 1000));
    U_TEST_PRINT_LINE("***IMPORTANT*** please visually check that the"
                      " duration of this test as seen by the PC-side"
                      " of the test system is also %d second(s).",
                      (int32_t) (timeDelta / 1000));

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

#ifndef U_PORT_TEST_CHECK_TIME_TAKEN
/** If checking of time taken is NOT being done, at least
 * run uPortTaskBlock for a given time period so that
 * the user is able to visually check that it's not, for
 * instance, LESS than expected.
 */
U_PORT_TEST_FUNCTION("[port]", "portOsBlock")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("waiting %d ms...", U_PORT_TEST_OS_BLOCK_TIME_MS);

    uPortTaskBlock(U_PORT_TEST_OS_BLOCK_TIME_MS);

    U_TEST_PRINT_LINE("***IMPORTANT*** please visually check that the"
                      " duration of this test as seen by the PC-side"
                      " of the test is not less than %d second(s).",
                      U_PORT_TEST_OS_BLOCK_TIME_MS / 1000);

    uPortDeinit();
    // Printed for information: asserting happens at the end
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}
#endif

/** Test event queues.
 */
U_PORT_TEST_FUNCTION("[port]", "portEventQueue")
{
    uint8_t *pParam;
    uint8_t fill;
    size_t x;
    int32_t y;
    int32_t stackMinFreeBytes;
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Reset error flags and counters
    gEventQueueMaxErrorFlag = 0;
    gEventQueueMaxCounter = 0;
    gEventQueueMinErrorFlag = 0;
    gEventQueueMinCounter = 0;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("opening two event queues...");
    // Open two event queues, one with the
    // maximum parameter length and one with just
    // a single byte, one with a name and one without
    gEventQueueMaxHandle = uPortEventQueueOpen(eventQueueMaxFunction, NULL,
                                               U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES,
                                               U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                               U_CFG_TEST_OS_TASK_PRIORITY,
                                               U_PORT_TEST_QUEUE_LENGTH);
    U_PORT_TEST_ASSERT(gEventQueueMaxHandle >= 0);
    y = uPortEventQueueGetFree(gEventQueueMaxHandle);
    U_TEST_PRINT_LINE("%d entries free on \"event queue max\".", y);
    U_PORT_TEST_ASSERT((y == U_PORT_TEST_QUEUE_LENGTH) ||
                       (y == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED));
    gEventQueueMinHandle = uPortEventQueueOpen(eventQueueMinFunction, "blah",
                                               U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES,
                                               U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES,
                                               U_CFG_TEST_OS_TASK_PRIORITY,
                                               U_PORT_TEST_QUEUE_LENGTH);
    U_PORT_TEST_ASSERT(gEventQueueMinHandle >= 0);
    y = uPortEventQueueGetFree(gEventQueueMinHandle);
    U_TEST_PRINT_LINE("%d entries free on \"event queue min\".", y);
    U_PORT_TEST_ASSERT((y == U_PORT_TEST_QUEUE_LENGTH) ||
                       (y == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED));

    // Generate a block with a known test pattern, 0xFF to 0 repeated.
    //lint -esym(613, pParam) Suppress possible use of NULL pointer: it is checked
    pParam = (uint8_t *) pUPortMalloc(U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pParam != NULL);
    fill = 0xFF;
    for (x = 0; x < U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES; x++) {
        *(pParam + x) = fill;
        fill--;
    }

    U_TEST_PRINT_LINE("sending to the two event queues %d time(s)...",
                      U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);

    // Try to send too much to each queue, should fail
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           pParam,
                                           U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES + 1) < 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           pParam, U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES + 1) < 0);

    // Send the known test pattern N times to eventQueueMaxFunction
    // with the last byte overwritten with a counter, and just send
    // the counter to eventQueueMinFunction as its
    // U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES payload.
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
                                                   (void *) &x, U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES) == 0);
        } else {
            y = uPortEventQueueSendIrq(gEventQueueMaxHandle, (void *) pParam,
                                       U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES);
            if (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
                y = uPortEventQueueSend(gEventQueueMaxHandle, (void *) pParam,
                                        U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES);
            }
            U_PORT_TEST_ASSERT(y == 0);
            y = uPortEventQueueSendIrq(gEventQueueMinHandle, (void *) &x,
                                       U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES);
            if (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
                y = uPortEventQueueSend(gEventQueueMinHandle, (void *) &x,
                                        U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES);
            }
            U_PORT_TEST_ASSERT(y == 0);
        }
#if defined(CONFIG_ARCH_POSIX) || defined(__linux__)
        // Delay needed here since Zephyr 3. Reason unknown.
        uPortTaskBlock(1);
#endif
    }

    // Bonus iteration with NULL parameter
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           NULL, 0) == 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           NULL, 0) == 0);
#if defined(CONFIG_ARCH_POSIX) || defined(__linux__)
    // Delay needed here since Zephyr 3. Reason unknown.
    uPortTaskBlock(1);
#endif

#ifndef U_PORT_TEST_CHECK_TIME_TAKEN
    // Let everything get to its destination; can be a problem when
    // running on Windows as a platform if the machine in question
    // is heavily loaded (a Windows test agent often is)
    uPortTaskBlock(1000);
#endif

    if (gEventQueueMaxErrorFlag != 0) {
        U_TEST_PRINT_LINE("event queue max length failed on iteration"
                          " %d with error %d.", x,
                          gEventQueueMaxErrorFlag);
    }
    if (gEventQueueMinErrorFlag != 0) {
        U_TEST_PRINT_LINE("event queue min length failed on iteration"
                          " %d with error %d.", x,
                          gEventQueueMinErrorFlag);
    }

    U_TEST_PRINT_LINE("event queue min received %d message(s).",
                      gEventQueueMinCounter);
    U_TEST_PRINT_LINE("event queue max received %d message(s).",
                      gEventQueueMaxCounter);
    U_PORT_TEST_ASSERT(gEventQueueMaxErrorFlag == 0);
    U_PORT_TEST_ASSERT(gEventQueueMaxCounter == U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);
    U_PORT_TEST_ASSERT(gEventQueueMinErrorFlag == 0);
    U_PORT_TEST_ASSERT(gEventQueueMinCounter == U_PORT_TEST_OS_EVENT_QUEUE_ITERATIONS + 1);

    // Check that the uPortEventQueueIsTask() gives a negative
    // answer correctly
    U_PORT_TEST_ASSERT(!uPortEventQueueIsTask(gEventQueueMaxHandle));
    U_PORT_TEST_ASSERT(!uPortEventQueueIsTask(gEventQueueMinHandle));

    // Check stack usage of the tasks at the end of the event queues
    stackMinFreeBytes = uPortEventQueueStackMinFree(gEventQueueMinHandle);
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("event queue min task had %d byte(s) free out of %d.",
                          stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }
    stackMinFreeBytes = uPortEventQueueStackMinFree(gEventQueueMaxHandle);
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("event queue max task had %d byte(s) free out of %d.",
                          stackMinFreeBytes, U_PORT_EVENT_QUEUE_MIN_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    U_TEST_PRINT_LINE("closing the event queues...");
    U_PORT_TEST_ASSERT(uPortEventQueueClose(gEventQueueMaxHandle) == 0);
    uPortEventQueueCleanUp();
    U_PORT_TEST_ASSERT(uPortEventQueueClose(gEventQueueMinHandle) == 0);

    // Check that they are no longer available
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMaxHandle,
                                           pParam,
                                           U_PORT_EVENT_QUEUE_MAX_PARAM_LENGTH_BYTES) < 0);
    U_PORT_TEST_ASSERT(uPortEventQueueSend(gEventQueueMinHandle,
                                           pParam,
                                           U_PORT_TEST_OS_EVENT_QUEUE_PARAM_MIN_SIZE_BYTES) < 0);

    // Free memory
    uPortFree(pParam);

    uPortDeinit();

    // Give the RTOS idle task time to tidy-away the tasks
    uPortTaskBlock(1000);

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test heap API.
 *
 * NOTE: for this to work fully U_ASSERT_HOOK_FUNCTION_TEST_RETURN must be defined.
 */
U_PORT_TEST_FUNCTION("[port]", "portHeap")
{
    int32_t x;
    int32_t y;

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing heap allocation.");

    // Dump current heap with no prefix
    y = uPortHeapDump(NULL);
#ifdef U_CFG_HEAP_MONITOR
    U_PORT_TEST_ASSERT(y >= 0);
#else
    U_PORT_TEST_ASSERT(y == 0);
#endif
    gpMalloc = pUPortMalloc(U_PORT_MALLOC_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpMalloc != NULL);

    // Dump new heap extent, this time with a prefix
    x = uPortHeapDump(U_TEST_PREFIX);
#ifdef U_CFG_HEAP_MONITOR
    U_PORT_TEST_ASSERT(x == y + 1);
#else
    U_PORT_TEST_ASSERT(x == 0);
#endif

    // Register an assert function
    gVariable = 0;
    uAssertHookSet(assertFunction);

    // Free memory: should be fine
    uPortFree(gpMalloc);
    gpMalloc = NULL;
    U_PORT_TEST_ASSERT(gVariable == 0);

#if defined(U_CFG_HEAP_MONITOR) && defined(U_ASSERT_HOOK_FUNCTION_TEST_RETURN)
    U_TEST_PRINT_LINE("testing buffer overrun detection with assert hook.");

    // Malloc another block and corrupt it with an overrun
    gpMalloc = pUPortMalloc(U_PORT_MALLOC_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpMalloc != NULL);
    *(((char *) gpMalloc) + U_PORT_MALLOC_LENGTH_BYTES) = 0;
    uPortFree(gpMalloc);
    gpMalloc = NULL;
    U_PORT_TEST_ASSERT(gVariable == 1);

    // And again, no corruption this time
    gVariable = 0;
    gpMalloc = pUPortMalloc(U_PORT_MALLOC_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpMalloc != NULL);
    uPortFree(gpMalloc);
    gpMalloc = NULL;
    U_PORT_TEST_ASSERT(gVariable == 0);

    U_TEST_PRINT_LINE("testing buffer underrun detection with assert hook.");

    // Finally with an underrun
    gVariable = 0;
    gpMalloc = pUPortMalloc(U_PORT_MALLOC_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpMalloc != NULL);
    *(((char *) gpMalloc) - 1) = 0;
    uPortFree(gpMalloc);
    gpMalloc = NULL;
    U_PORT_TEST_ASSERT(gVariable == 1);
#endif

    U_TEST_PRINT_LINE("removing assert hook.");
    // Remove the assert hook
    uAssertHookSet(NULL);
    uPortDeinit();
    // Printed for information: asserting happens at the end
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

/** Test: strtok_r since we have our own implementation on
 * some platforms.
 */
U_PORT_TEST_FUNCTION("[port]", "portStrtok_r")
{
    char *pSave;
    char buffer[8];
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing strtok_r()...");

    buffer[sizeof(buffer) - 1] = 'x';

    // memcpy() rather than strncpy() as strncpy() would overwrite the 'x' with 0
    memcpy(buffer, "abcabc", 7);
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

    memcpy(buffer, "abcade", 7);
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "a", &pSave), "bc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    memcpy(buffer, "abcade", 7);
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "a", &pSave), "bc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    //lint -e(668) Suppress possible passing of NULL pointer to strcmp,
    // it's a test, if it goes bang we've failed
    U_PORT_TEST_ASSERT(strcmp(strtok_r(NULL, "a", &pSave), "de") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strtok_r(NULL, "a", &pSave) == NULL);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');

    memcpy(buffer, "abcabc", 7);
    U_PORT_TEST_ASSERT(strcmp(strtok_r(buffer, "d", &pSave), "abcabc") == 0);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');
    U_PORT_TEST_ASSERT(strtok_r(NULL, "d", &pSave) == NULL);
    U_PORT_TEST_ASSERT(buffer[sizeof(buffer) - 1] == 'x');

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test: mktime64().
 */
U_PORT_TEST_FUNCTION("[port]", "portMktime64")
{
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing mktime64()...");

    for (size_t x = 0; x < sizeof(timeTestData) /
         sizeof(timeTestData[0]); x++) {
        U_PORT_TEST_ASSERT(mktime64(&timeTestData[x].timeStruct) ==
                           timeTestData[x].time);
    }

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test: gmtime_r().
 */
U_PORT_TEST_FUNCTION("[port]", "portGmtime_r")
{
    int32_t resourceCount;
    struct tm timeStruct;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing gmtime_r()...");

    for (size_t x = 0; x < sizeof(timeTestData) / sizeof(timeTestData[0]); x++) {
        if (timeTestData[x].time <= INT_MAX) {
            memset(&timeStruct, 0xFF, sizeof(timeStruct));
            U_PORT_TEST_ASSERT(gmtime_r((time_t *) & (timeTestData[x].time), &timeStruct) == &timeStruct);
            U_PORT_TEST_ASSERT(timeStruct.tm_sec == timeTestData[x].timeStruct.tm_sec % 60);
            U_PORT_TEST_ASSERT(timeStruct.tm_min == timeTestData[x].timeStruct.tm_min +
                               (timeTestData[x].timeStruct.tm_sec / 60));
            U_PORT_TEST_ASSERT(timeStruct.tm_hour == timeTestData[x].timeStruct.tm_hour);
            U_PORT_TEST_ASSERT(timeStruct.tm_mday == timeTestData[x].timeStruct.tm_mday);
            U_PORT_TEST_ASSERT(timeStruct.tm_mon == timeTestData[x].timeStruct.tm_mon);
            U_PORT_TEST_ASSERT(timeStruct.tm_year == timeTestData[x].timeStruct.tm_year);
            U_PORT_TEST_ASSERT(timeStruct.tm_wday == timeTestData[x].timeStruct.tm_wday);
            U_PORT_TEST_ASSERT(timeStruct.tm_yday == timeTestData[x].timeStruct.tm_yday);
            U_PORT_TEST_ASSERT(timeStruct.tm_isdst == 0);
        }
    }

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test: uPortGetTimezoneOffsetSeconds().
 */
U_PORT_TEST_FUNCTION("[port]", "portGetTimezoneOffsetSeconds")
{
    int32_t resourceCount;
    struct tm utcTm;
    time_t utc = 0;
    time_t wrong;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing uPortGetTimezoneOffsetSeconds()...");

#if defined(WIN32) || defined (__linux__)
    // On systems that have proper time we must use proper time
    // as otherwise Daylight Saving Time can't be taken into account
    utc = time(NULL);
#endif

    // gmtime() gets us the UTC time in a struct tm *** STILL AS UTC ***
#ifdef WIN32
    // Windows doesn't have gmtime_r()
    struct tm *pUtcTm = gmtime(&utc);
    U_PORT_TEST_ASSERT(pUtcTm != NULL);
    utcTm = *pUtcTm;
#else
    U_PORT_TEST_ASSERT(gmtime_r(&utc, &utcTm) == &utcTm);
#endif
    // Setting daylight saving flag to -1 should cause mktime()
    // to decide whether DST is in effect by itself
    utcTm.tm_isdst = -1;
    // mktime assumes it is being given a *** LOCAL *** time and,
    // to return UTC, subtracts the timezone offset
    wrong = mktime(&utcTm);
    U_TEST_PRINT_LINE("UTC time %d and, given that, mktime() returns %d,"
                      " therefore timezone offset %d second(s),"
                      " uPortGetTimezoneOffsetSeconds() %d second(s).",
                      (int32_t) utc, (int32_t) wrong, (int32_t) (utc - wrong),
                      uPortGetTimezoneOffsetSeconds());
    U_PORT_TEST_ASSERT(uPortGetTimezoneOffsetSeconds() == utc - wrong);

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

#if (U_CFG_TEST_PIN_A >= 0) && (U_CFG_TEST_PIN_B >= 0) && \
    (U_CFG_TEST_PIN_C >= 0)
/** Test GPIOs.
 */
U_PORT_TEST_FUNCTION("[port]", "portGpioRequiresSpecificWiring")
{
    uPortGpioConfig_t gpioConfig = U_PORT_GPIO_CONFIG_DEFAULT;
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing GPIOs.");
    U_TEST_PRINT_LINE("pin A (%d, 0x%02x) will be used as an output"
                      " and must be connected to pin B (%d, 0x%02x)"
                      " via a 1k resistor.",
                      U_CFG_TEST_PIN_A, U_CFG_TEST_PIN_A,
                      U_CFG_TEST_PIN_B, U_CFG_TEST_PIN_B);
    U_TEST_PRINT_LINE("pin B (%d, 0x%02x) will be used as an input"
                      " and an open drain output.",
                      U_CFG_TEST_PIN_B, U_CFG_TEST_PIN_B);
    U_TEST_PRINT_LINE("pin C (%d, 0x%02x) will be used as an input"
                      " and must be connected to pin B (%d, 0x%02x).",
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

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B < 0)
/** Test UART.
 */
U_PORT_TEST_FUNCTION("[port]", "portUartRequiresSpecificWiring")
{
    int32_t resourceCount;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

#if defined(U_CFG_TEST_PIN_UART_A_CTS_GET) && defined (U_CFG_TEST_PIN_UART_A_RTS_GET)
# if ((U_CFG_TEST_PIN_UART_A_CTS < 0) && (U_CFG_TEST_PIN_UART_A_CTS_GET >=0)) || \
     ((U_CFG_TEST_PIN_UART_A_RTS < 0) && (U_CFG_TEST_PIN_UART_A_RTS_GET >=0))
    // If no CTS/RTS pin is set but the _GET macro returns an actual
    // pin then that means that the platform we're running on cannot
    // set the pins at run-time, only at compile-time; here we can only do
    // whatever those pins have been fixed to do, so run the test with
    // flow control only.
    runUartTest(50000, 115200, U_PORT_TEST_UART_FLOW_CONTROL_IS_USED);
# else
    // Either the platform can set pins at run-time or it can't and the
    // flow control pins are not connected so run UART test at 115,200
    // without flow control
    runUartTest(50000, 115200, U_PORT_TEST_UART_FLOW_CONTROL_IS_NOT_USED);
    // NOLINTNEXTLINE(misc-redundant-expression)
    if ((U_CFG_TEST_PIN_UART_A_CTS_GET >= 0) &&
        (U_CFG_TEST_PIN_UART_A_RTS_GET >= 0)) {
        // Must be on a platform where the pins can be set at run-time
        // and the flow control pins are connected so test with flow control
        runUartTest(50000, 115200, U_PORT_TEST_UART_FLOW_CONTROL_IS_USED);
        runUartTest(50000, 1000000, U_PORT_TEST_UART_FLOW_CONTROL_IS_USED);
    }
# endif
#else
    // If we cannot determine at compile time whether a CTS or RTS pin
    // is set then we can only do whatever the HW does and shouldn't
    // try the high data rate test
    runUartTest(50000, 115200,
                U_PORT_TEST_UART_FLOW_CONTROL_UNKNOWN_AT_COMPILE_TIME);
#endif

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

#if (U_CFG_APP_GNSS_I2C >= 0) && !defined(U_PORT_TEST_DISABLE_I2C)
/** Test I2C.
 */
U_PORT_TEST_FUNCTION("[port]", "portI2cRequiresSpecificWiring")
{
    int32_t y;
    int32_t z;
    int32_t messageClass = -1;
    int32_t messageId = -1;
    int32_t resourceCount;
    char *gpI2cBuffer;
    bool success = false;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Enough room for the fixed part of a UBX-MON-VER message
    // plus 10 segments
    gpI2cBuffer = (char *) pUPortMalloc(U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(gpI2cBuffer != NULL);
    // I2C can suffer issues due to pull-ups, speed, the responsiveness
    // of the device at the far end, etc., hence we allow retries of
    // this test if we do not read back the expected number of waiting
    // bytes from the device lower down in the test
    for (size_t x = 0; (x < U_PORT_TEST_I2C_RETRIES) && !success; x++) {
        // Reset the GNSS chip as otherwise it might have queued up
        // other data which would mess this test up
        if (gnssReset()) {

            U_TEST_PRINT_LINE("testing I2C, round %d, assuming a u-blox GNSS"
                              " device on the I2C bus at address 0x%02x.", x + 1,
                              U_PORT_TEST_I2C_ADDRESS);

            // Try to open an I2C instance without having initialised I2C, should fail
            U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA,
                                            U_CFG_APP_PIN_GNSS_SCL, true) < 0);
            // Now initialise I2C
            U_PORT_TEST_ASSERT(uPortI2cInit() == 0);
# if !defined(__ZEPHYR__) && !defined(__linux__)
            // Try to open an I2C instance without pins, should fail
            U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, -1, U_CFG_APP_PIN_GNSS_SCL, true) < 0);
            U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA, -1, true) < 0);
# endif
            // Try to open an I2C instance not as controller, should fail
            U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA,
                                            U_CFG_APP_PIN_GNSS_SCL, false) < 0);
            // Now do it properly
            gI2cHandle = uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA, U_CFG_APP_PIN_GNSS_SCL, true);
            U_PORT_TEST_ASSERT(gI2cHandle >= 0);

            // Note: no real way of testing uPortI2cAdopt() here since
            // it would require platform specific test code.

            // Close again and deinit I2C, using the bus-recovery version
            // (where supported) in case of previous test failures
            if (uPortI2cCloseRecoverBus(gI2cHandle) == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
                uPortI2cClose(gI2cHandle);
            }
            uPortI2cDeinit();
            // Try to open an I2C instance without having initialised I2C again, should fail
            U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA,
                                            U_CFG_APP_PIN_GNSS_SCL, true) < 0);

            // Initialise and open again
            uPortI2cInit();
            gI2cHandle = uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA, U_CFG_APP_PIN_GNSS_SCL, true);
            U_PORT_TEST_ASSERT(gI2cHandle >= 0);

            // Test getting and setting the clock rate

# if U_PORT_I2C_CLOCK_FREQUENCY_HERTZ == 400000
#  error This test needs updating: U_PORT_I2C_CLOCK_FREQUENCY_HERTZ is now 400,000!
# endif
            y = uPortI2cGetClock(gI2cHandle);
            if (y >= 0) {
                U_PORT_TEST_ASSERT(uPortI2cGetClock(gI2cHandle) == U_PORT_I2C_CLOCK_FREQUENCY_HERTZ);
                // All platforms support setting at least 400,000
                U_PORT_TEST_ASSERT(uPortI2cSetClock(gI2cHandle, 400000) == 0);
                U_PORT_TEST_ASSERT(uPortI2cGetClock(gI2cHandle) == 400000);
                // Close, re-open and check that we're back at the default clock rate
                uPortI2cClose(gI2cHandle);
                gI2cHandle = uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA, U_CFG_APP_PIN_GNSS_SCL, true);
                U_PORT_TEST_ASSERT(gI2cHandle >= 0);
                U_PORT_TEST_ASSERT(uPortI2cGetClock(gI2cHandle) == U_PORT_I2C_CLOCK_FREQUENCY_HERTZ);
            } else {
                U_PORT_TEST_ASSERT((y == U_ERROR_COMMON_NOT_SUPPORTED) || (y == U_ERROR_COMMON_NOT_IMPLEMENTED));
                U_TEST_PRINT_LINE("get of I2C clock not supported/implemented, not testing I2C clock API.");
            }

            // Test getting and setting the timeout
            y = uPortI2cGetTimeout(gI2cHandle);
            if (y > 0) {
                U_TEST_PRINT_LINE("I2C timeout is %d ms.", y);
#if defined(__XTENSA__) && !defined(CONFIG_IDF_TARGET_ESP32)
                // Need to use >= as the underlying timeout is the nearest power of 2
                U_PORT_TEST_ASSERT(y >= U_PORT_I2C_TIMEOUT_MILLISECONDS);
                // Need to multiply-up for a valid test since, for the ESP32x3 cases,
                // the timeout can only be a power of two of the underlying clock
                U_TEST_PRINT_LINE("setting I2C timeout to at least %d ms.", U_PORT_I2C_TIMEOUT_MILLISECONDS * 2);
                U_PORT_TEST_ASSERT(uPortI2cSetTimeout(gI2cHandle, U_PORT_I2C_TIMEOUT_MILLISECONDS * 2) == 0);
                y = uPortI2cGetTimeout(gI2cHandle);
                U_TEST_PRINT_LINE("I2C timeout is now %d ms.", y);
                U_PORT_TEST_ASSERT(y >= U_PORT_I2C_TIMEOUT_MILLISECONDS * 2);
#else
                // In all other cases, should be able to get an exact match
                // and just add one to see a difference
                U_PORT_TEST_ASSERT(y == U_PORT_I2C_TIMEOUT_MILLISECONDS);
                U_TEST_PRINT_LINE("setting I2C timeout to %d ms.", U_PORT_I2C_TIMEOUT_MILLISECONDS + 1);
                U_PORT_TEST_ASSERT(uPortI2cSetTimeout(gI2cHandle, U_PORT_I2C_TIMEOUT_MILLISECONDS + 1) == 0);
                y = uPortI2cGetTimeout(gI2cHandle);
                U_TEST_PRINT_LINE("I2C timeout is now %d ms.", y);
                U_PORT_TEST_ASSERT(y == U_PORT_I2C_TIMEOUT_MILLISECONDS + 1);
#endif
                // Close, re-open and check that we're back at the default timeout
                uPortI2cClose(gI2cHandle);
                gI2cHandle = uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA, U_CFG_APP_PIN_GNSS_SCL, true);
                U_PORT_TEST_ASSERT(gI2cHandle >= 0);
                y = uPortI2cGetTimeout(gI2cHandle);
                U_TEST_PRINT_LINE("I2C timeout is now back to %d ms.", y);
#if defined (__XTENSA__) && !defined(CONFIG_IDF_TARGET_ESP32)
                U_PORT_TEST_ASSERT(y < U_PORT_I2C_TIMEOUT_MILLISECONDS * 2);
#else
                U_PORT_TEST_ASSERT(y == U_PORT_I2C_TIMEOUT_MILLISECONDS);
#endif
            } else {
                U_PORT_TEST_ASSERT((y == U_ERROR_COMMON_NOT_SUPPORTED) || (y == U_ERROR_COMMON_NOT_IMPLEMENTED));
                U_TEST_PRINT_LINE("get of I2C timeout not supported/implemented, not testing I2C timeout.");
            }

# ifdef U_CFG_TEST_USING_ST32F4
            // There is errata for STM32F4:
            //
            // https://www.st.com/resource/en/errata_sheet/es0206-stm32f427437-and-stm32f429439-line-limitations-stmicroelectronics.pdf
            //
            // ...which suggests that using a 100 kHz I2C clock might not work in some circumstances, hence
            // switch to 400 kHz for these tests
            U_TEST_PRINT_LINE("STM32F4, switching to 400 kHz clock for greater reliability/compatibility...");
            U_PORT_TEST_ASSERT(uPortI2cSetClock(gI2cHandle, 400000) == 0);
# endif

            U_TEST_PRINT_LINE("talking to GNSS chip over I2C...");
            // Set gpI2cBuffer up to contain the REGSTREAM address, which is valid for all u-blox GNSS devices
            // and means that any I2C read from the GNSS chip will get the next byte it wants to stream at us
            *gpI2cBuffer = 0xFF;
            // First talk to an I2C address that is not present
            U_TEST_PRINT_LINE("deliberately using an invalid address (0x%02x).", U_PORT_TEST_I2C_ADDRESS - 1);
            U_PORT_TEST_ASSERT(uPortI2cControllerSend(gI2cHandle, U_PORT_TEST_I2C_ADDRESS - 1, NULL, 0,
                                                      false) < 0);
            U_PORT_TEST_ASSERT(uPortI2cControllerSendReceive(gI2cHandle, U_PORT_TEST_I2C_ADDRESS - 1,
                                                             gpI2cBuffer, 1, NULL, 0) < 0);

            // The following should do nothing and return success
            U_PORT_TEST_ASSERT(uPortI2cControllerSendReceive(gI2cHandle, U_PORT_TEST_I2C_ADDRESS - 1,
                                                             NULL, 0, NULL, 0) == 0);
            U_TEST_PRINT_LINE("now using the valid address (0x%02x).", U_PORT_TEST_I2C_ADDRESS);
# if !defined(U_CFG_TEST_USING_NRF5SDK) && !defined(__ZEPHYR__)
            // Now do a NULL send which will succeed only if the GNSS device is there;
            // note that the NRFX drivers used on NRF52 and NRF53 don't support sending
            // only the address, data must follow
            U_PORT_TEST_ASSERT(uPortI2cControllerSend(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, NULL, 0,
                                                      false) == 0);
# endif

            // Write to the REGSTREAM address on the GNSS device
            U_PORT_TEST_ASSERT(uPortI2cControllerSend(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, gpI2cBuffer, 1,
                                                      false) == 0);
            // Write a longer thing; UBX-MON-VER polls the GNSS device for a
            // 40 + n·* 30 byte UBX-MON-VER response containing a 40 byte fixed part, which
            // is all we captue here; message class 0x0a, message ID 0x04.
            memset(gpI2cBuffer, 0xFF, U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES);
            y = uUbxProtocolEncode(0x0a, 0x04, NULL, 0, gpI2cBuffer);
            // Send
            U_PORT_TEST_ASSERT(uPortI2cControllerSend(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, gpI2cBuffer, y,
                                                      false) == 0);
            // Wait for the GNSS chip to sort itself out
            uPortTaskBlock(100);
            // There should now be a 40 + n * 30 byte UBX-MON-VER message waiting for us.  The number of
            // bytes waiting for us is available by a read of register addresses 0xFD and 0xFE in
            // the GNSS chip.  The register address in the GNSS chip auto-increments, so sending
            // 0xFD, with no stop bit, and then a read request for two bytes should get us the
            // [big-endian] length
            *gpI2cBuffer = 0xFD;
            U_PORT_TEST_ASSERT(uPortI2cControllerSend(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, gpI2cBuffer, 1,
                                                      true) == 0);
            U_PORT_TEST_ASSERT(uPortI2cControllerSendReceive(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, NULL, 0,
                                                             gpI2cBuffer, 2) == 2);
            y = (int32_t) ((((uint32_t) * gpI2cBuffer) << 8) + (uint32_t) * (gpI2cBuffer + 1));
            U_TEST_PRINT_LINE("read of number of bytes waiting returned 0x%02x%02x (%d).", *gpI2cBuffer,
                              *(gpI2cBuffer + 1), y);
            if (y >= U_PORT_TEST_I2C_UBX_MON_VER_FIXED_LENGTH + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) {
                // With the register address auto-incremented to 0xFF we can now just read out the
                // UBX-MON-VER response
                if (y > U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES) {
                    y = U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES;
                }
                memset(gpI2cBuffer, 0xFF, U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES);
                z = uPortI2cControllerSendReceive(gI2cHandle, U_PORT_TEST_I2C_ADDRESS, NULL, 0,
                                                  gpI2cBuffer, y);
                U_TEST_PRINT_LINE("%d byte(s) retrieved.", z);
                U_PORT_TEST_ASSERT(z >= y);
                y = uUbxProtocolDecode(gpI2cBuffer, z, &messageClass, &messageId,
                                       gpI2cBuffer, U_PORT_TEST_I2C_BUFFER_LENGTH_BYTES, NULL);
                U_TEST_PRINT_LINE("%d byte(s) of message decoded (class 0x%02x, ID 0x%02x).", y, messageClass,
                                  messageId);
                // Can't be sure that the decoded length is the whole thing since other
                // message _could_ have landed after it
                U_PORT_TEST_ASSERT(y >= U_PORT_TEST_I2C_UBX_MON_VER_FIXED_LENGTH);
                U_PORT_TEST_ASSERT(messageClass == 0x0a);
                U_PORT_TEST_ASSERT(messageId == 0x04);

                uPortI2cClose(gI2cHandle);
                uPortI2cDeinit();
                U_PORT_TEST_ASSERT(uPortI2cGetClock(gI2cHandle) < 0);
                U_PORT_TEST_ASSERT(uPortI2cSetClock(gI2cHandle, U_PORT_I2C_CLOCK_FREQUENCY_HERTZ) < 0);
                U_PORT_TEST_ASSERT(uPortI2cGetTimeout(gI2cHandle) < 0);
                U_PORT_TEST_ASSERT(uPortI2cSetTimeout(gI2cHandle, U_PORT_I2C_TIMEOUT_MILLISECONDS) < 0);
                U_PORT_TEST_ASSERT(uPortI2cOpen(U_CFG_APP_GNSS_I2C, U_CFG_APP_PIN_GNSS_SDA,
                                                U_CFG_APP_PIN_GNSS_SCL, true) < 0);
                success = true;
            } else {
                uPortI2cClose(gI2cHandle);
                uPortI2cDeinit();
                U_TEST_PRINT_LINE("***WARNING*** retrying to get the right I2C response.");
                // Relax a little before the retry
                uPortTaskBlock(5000);
            }
        } else {
            U_TEST_PRINT_LINE("not testing I2C as we can't reset the GNSS chip.");
            // This is no reason to fail the test, we just can't run it
            success = true;
        }
    }

    uPortFree(gpI2cBuffer);
    gpI2cBuffer = NULL;

    U_PORT_TEST_ASSERT(success);

    // Now we're done
    uPortDeinit();
    gI2cHandle = -1;

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

#if (U_CFG_APP_GNSS_SPI >= 0)
/** Test SPI.
 */
U_PORT_TEST_FUNCTION("[port]", "portSpiRequiresSpecificWiring")
{
    // *INDENT-OFF* (otherwise AStyle makes a mess of this)
#ifdef U_CFG_TEST_GNSS_SPI_SELECT_INDEX
    uCommonSpiControllerDevice_t device = U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS(U_CFG_TEST_GNSS_SPI_SELECT_INDEX);
#else
    uCommonSpiControllerDevice_t device = U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS(U_CFG_APP_PIN_GNSS_SPI_SELECT);
#endif
    // *INDENT-ON*
    uCommonSpiControllerDevice_t tmp = {0};
    int32_t y = 0;
    int32_t messageClass = -1;
    int32_t messageId = -1;
    int32_t resourceCount;
    char buffer1[12]; // Enough room for the body of a UBX-MON-BATCH message
    char buffer2[(12 +
                  U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES) *
                 2]; // Enough room for the two UBX-MON-BATCH messages, allowing overlap
    char *pBuffer = buffer2;
    char buffer3[sizeof(uint64_t)];

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing SPI, assuming a u-blox GNSS device is connected to it.");

    // Try to open an SPI instance without having initialised SPI, should fail
    U_PORT_TEST_ASSERT(uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                                    U_CFG_APP_PIN_GNSS_SPI_MISO, U_CFG_APP_PIN_GNSS_SPI_CLK,
                                    true) < 0);

    // Now initialise SPI
    U_PORT_TEST_ASSERT(uPortSpiInit() == 0);
# if !defined(__ZEPHYR__) && !defined(__linux__)
    // Try to open an SPI instance without a valid combination of pins, should fail
    U_PORT_TEST_ASSERT(uPortSpiOpen(U_CFG_APP_GNSS_SPI, -1, -1,
                                    U_CFG_APP_PIN_GNSS_SPI_CLK, true) < 0);
    U_PORT_TEST_ASSERT(uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                                    U_CFG_APP_PIN_GNSS_SPI_MISO, -1, true) < 0);
# endif
    // Try to open an SPI instance not as controller, should fail
    U_PORT_TEST_ASSERT(uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                                    U_CFG_APP_PIN_GNSS_SPI_MISO, U_CFG_APP_PIN_GNSS_SPI_CLK,
                                    false) < 0);
    // Now do it properly
    gSpiHandle = uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                              U_CFG_APP_PIN_GNSS_SPI_MISO, U_CFG_APP_PIN_GNSS_SPI_CLK, true);
    U_PORT_TEST_ASSERT(gSpiHandle >= 0);

    // Close again and deinit SPI
    uPortSpiClose(gSpiHandle);
    uPortSpiDeinit();
    // Try to open an SPI instance without having initialised SPI again, should fail
    U_PORT_TEST_ASSERT(uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                                    U_CFG_APP_PIN_GNSS_SPI_MISO, U_CFG_APP_PIN_GNSS_SPI_CLK,
                                    true) < 0);

    // Initialise and open again
    U_PORT_TEST_ASSERT(uPortSpiInit() == 0);
    gSpiHandle = uPortSpiOpen(U_CFG_APP_GNSS_SPI, U_CFG_APP_PIN_GNSS_SPI_MOSI,
                              U_CFG_APP_PIN_GNSS_SPI_MISO, U_CFG_APP_PIN_GNSS_SPI_CLK, true);
    U_PORT_TEST_ASSERT(gSpiHandle >= 0);

    // Configure SPI for the M8/M9/M10 GNSS chip we will test against
    // Settings should all be left at defaults apart from chip select pin
    U_TEST_PRINT_LINE("chip select index is %d.", device.indexSelect);
    U_TEST_PRINT_LINE("chip select pin is %d (0x%02x).", device.pinSelect, device.pinSelect);
    U_PORT_TEST_ASSERT(uPortSpiControllerSetDevice(gSpiHandle, &device) == 0);
    // Get the settings back and check that they are all as expected; the
    // defaults are chosen to match the settings of platforms that do not
    // support one or more parameters
    U_PORT_TEST_ASSERT(uPortSpiControllerGetDevice(gSpiHandle, &tmp) == 0);
    U_PORT_TEST_ASSERT(tmp.pinSelect == U_CFG_APP_PIN_GNSS_SPI_SELECT);
    U_PORT_TEST_ASSERT(tmp.frequencyHertz <= device.frequencyHertz);
    U_PORT_TEST_ASSERT(tmp.mode == device.mode);
    U_PORT_TEST_ASSERT(tmp.wordSizeBytes == device.wordSizeBytes);
    U_PORT_TEST_ASSERT(tmp.lsbFirst == device.lsbFirst);
    U_PORT_TEST_ASSERT(tmp.startOffsetNanoseconds == device.startOffsetNanoseconds);
    U_PORT_TEST_ASSERT(tmp.stopOffsetNanoseconds == device.stopOffsetNanoseconds);
    U_PORT_TEST_ASSERT(tmp.sampleDelayNanoseconds == device.sampleDelayNanoseconds);
    U_PORT_TEST_ASSERT(fillWordSame(tmp.fillWord, device.fillWord, device.wordSizeBytes));

    U_TEST_PRINT_LINE("talking to GNSS chip over SPI...");
    // Write to the GNSS chip; polls for a UBX-MON-BATCH message which should
    // get back a 20 byte response containing a 12 byte body (see section 32.16
    // of the u-blox M8 receiver manual); message class 0x0a, message ID 0x32.
    memset(buffer1, 0, sizeof(buffer1));
    y = uUbxProtocolEncode(0x0a, 0x32, NULL, 0, buffer2);
    U_PORT_TEST_ASSERT(y == U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
    // There is no real use-case for a single word send/receive when talking
    // to a GNSS chip so, in order to do a word-test as well as a block-test,
    // we send the above in two segments: a "word" of three bytes, then the
    // second half of the buffer minus the three bytes.  If there's an
    // endianness mismatch between what we've requested of the SPI protocol
    // and this processor, we also need to perform pre-reversal of the bytes
    // we are sending as a single word, since uPortSpiControllerSendReceiveWord()
    // will be helpfully byte-reversing them for us.
    U_TEST_PRINT_LINE("this MCU is %s-endian and we are sending"
                      " %s first.", U_PORT_IS_LITTLE_ENDIAN ? "little" : "big",
                      tmp.lsbFirst ? "LSB" : "MSB");
    if (tmp.lsbFirst != U_PORT_IS_LITTLE_ENDIAN) {
        U_TEST_PRINT_LINE("pre-reversing word to compensate for endiannness conversion.");
        for (size_t x = 0; x < 3; x++) {
            buffer3[x] = buffer2[(3 - 1) - x];
        }
    } else {
        memcpy(buffer3, buffer2, 3);
    }
    uPortSpiControllerSendReceiveWord(gSpiHandle, *((uint64_t *) &buffer3), 3);
    U_PORT_TEST_ASSERT(uPortSpiControllerSendReceiveBlock(gSpiHandle,
                                                          &(buffer2[3]),
                                                          y - 3,
                                                          NULL, 0) == 0);

    // There should now be a 20 byte response waiting for us, which we will
    // receive into buffer2 and decode into buffer1; question is, when
    // does it start to arrive?
    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer2, 0, sizeof(buffer2));
    // In order to test both single word and block receives, we do a short
    // block receive, a word send/receive, then another block receive for
    // the remainder of the buffer and after this we try to decode the
    // ack message.
    // Wait a tiny amount first to make sure the answer is ready and so should
    // definitely be spread across the three function calls.
    uPortTaskBlock(10);
    // We're looking for messageClass 0x0a and message ID 0x32
    y = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    for (size_t x = 0; ((messageClass != 0x0a) || (messageId != 0x32) || (y < 0)) &&
         (x < U_PORT_TEST_SPI_TRIES); x++) {
        // 4 chosen here because receiving 4 bytes is a special case for ESP32
        y = 4;
        U_PORT_TEST_ASSERT(uPortSpiControllerSendReceiveBlock(gSpiHandle, NULL, 0,
                                                              pBuffer, y) == y);
        pBuffer += y;
        // Only a two-byte word here, rather than three, just for the sake
        // of variety
        *((uint64_t *) buffer3) = uPortSpiControllerSendReceiveWord(gSpiHandle, 0xFFFF, 2);
        if (tmp.lsbFirst != U_PORT_IS_LITTLE_ENDIAN) {
            *pBuffer = buffer3[1];
            pBuffer++;
            *pBuffer = buffer3[0];
        } else {
            *pBuffer = buffer3[0];
            pBuffer++;
            *pBuffer = buffer3[1];
        }
        pBuffer++;
        y = (sizeof(buffer2) - (pBuffer - buffer2)) / 2;
        U_PORT_TEST_ASSERT(uPortSpiControllerSendReceiveBlock(gSpiHandle, NULL, 0,
                                                              pBuffer, y) == y);
        pBuffer += y;
        y = uUbxProtocolDecode(buffer2, pBuffer - buffer2, &messageClass, &messageId,
                               buffer1, sizeof(buffer1), (const char **) &pBuffer);
        if (y == (int32_t) U_ERROR_COMMON_NOT_FOUND) {
            // Got nothing at all, reset the buffer pointer for the next try
            pBuffer = buffer2;
        }
    }

    U_PORT_TEST_ASSERT(messageClass == 0x0a);
    U_PORT_TEST_ASSERT(messageId == 0x32);
    // The body of the response is 12 bytes long
    U_PORT_TEST_ASSERT(y == 12);

    // Deinit SPI
    uPortSpiClose(gSpiHandle);
    uPortSpiDeinit();

    // Now we're done
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}
#endif

/** Test crypto: not a rigorous test, more a "hello world".
 */
U_PORT_TEST_FUNCTION("[port]", "portCrypto")
{
    char buffer[64];
    char iv[U_PORT_CRYPTO_AES128_INITIALISATION_VECTOR_LENGTH_BYTES];
    int32_t resourceCount;
    int32_t x;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    memset(buffer, 0, sizeof(buffer));

    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing SHA256...");
    x = uPortCryptoSha256(gSha256Input,
                          sizeof(gSha256Input) - 1,
                          buffer);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_PORT_TEST_ASSERT(x == (int32_t) U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(memcmp(buffer, gSha256Output,
                                  U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) == 0);
    } else {
        U_TEST_PRINT_LINE("SHA256 not supported.");
    }

    U_TEST_PRINT_LINE("testing HMAC SHA256...");
    x = uPortCryptoHmacSha256(gHmacSha256Key,
                              sizeof(gHmacSha256Key) - 1,
                              gHmacSha256Input,
                              sizeof(gHmacSha256Input) - 1,
                              buffer);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_PORT_TEST_ASSERT(x == (int32_t) U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(memcmp(buffer, gHmacSha256Output,
                                  U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES) == 0);
    } else {
        U_TEST_PRINT_LINE("HMAC SHA256 not supported.");
    }

    U_TEST_PRINT_LINE("testing AES CBC 128...");
    memcpy(iv, gAes128CbcIV, sizeof(iv));
    x = uPortCryptoAes128CbcEncrypt(gAes128CbcKey,
                                    sizeof(gAes128CbcKey) - 1,
                                    iv, gAes128CbcClear,
                                    sizeof(gAes128CbcClear) - 1,
                                    buffer);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_PORT_TEST_ASSERT(x == (int32_t) U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(memcmp(buffer, gAes128CbcEncrypted,
                                  sizeof(gAes128CbcEncrypted) - 1) == 0);
    } else {
        U_TEST_PRINT_LINE("AES CBC 128 encryption not supported.");
    }

    memcpy(iv, gAes128CbcIV, sizeof(iv));
    x = uPortCryptoAes128CbcDecrypt(gAes128CbcKey,
                                    sizeof(gAes128CbcKey) - 1,
                                    iv, gAes128CbcEncrypted,
                                    sizeof(gAes128CbcEncrypted) - 1,
                                    buffer);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_PORT_TEST_ASSERT(x == (int32_t) U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(memcmp(buffer, gAes128CbcClear,
                                  sizeof(gAes128CbcClear) - 1) == 0);
    } else {
        U_TEST_PRINT_LINE("AES CBC 128 decryption not supported.");
    }

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test timers.
 */
U_PORT_TEST_FUNCTION("[port]", "portTimers")
{
    int32_t resourceCount;
    int32_t y;
    int64_t startTime;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing timers...");

    // Create the first timer,
    y = uPortTimerCreate(&gTimerHandle[gTimerParameterIndex],
                         NULL, timerCallback, (void *) gTimerParameterIndex,
                         1000, false);
    U_PORT_TEST_ASSERT((y == 0) || (y == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED));
    if (y == 0) {
        // Delete it again, without having started it
        U_PORT_TEST_ASSERT(uPortTimerDelete(gTimerHandle[gTimerParameterIndex]) == 0);
        // It should not have expired
        U_PORT_TEST_ASSERT(gTimerParameterValue[gTimerParameterIndex] == 0);

        // Now create a second one shot timer with a name this time
        gTimerParameterIndex++;
        U_PORT_TEST_ASSERT(uPortTimerCreate(&gTimerHandle[gTimerParameterIndex],
                                            "timer 2", timerCallback,
                                            (void *) gTimerParameterIndex,
                                            1000, false) == 0);

        // Start it
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[gTimerParameterIndex]) == 0);
        // Stop it
        U_PORT_TEST_ASSERT(uPortTimerStop(gTimerHandle[gTimerParameterIndex]) == 0);
        // It should not have expired
        U_PORT_TEST_ASSERT(gTimerParameterValue[gTimerParameterIndex] == 0);

        // Create a third one-shot timer that we will actually let expire
        // this time
        gTimerParameterIndex++;
        U_PORT_TEST_ASSERT(uPortTimerCreate(&gTimerHandle[gTimerParameterIndex],
                                            "timer 3", timerCallback,
                                            (void *) gTimerParameterIndex,
                                            1000, false) == 0);
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[gTimerParameterIndex]) == 0);

        // Create a fourth timer, this time periodic and of a shorter duration
        // than the above
        gTimerParameterIndex++;
        U_PORT_TEST_ASSERT(uPortTimerCreate(&gTimerHandle[gTimerParameterIndex],
                                            "timer 4", timerCallback,
                                            (void *) gTimerParameterIndex,
                                            300, true) == 0);
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[gTimerParameterIndex]) == 0);

        // The periodic timer should expire three times in the time that the
        // one-shot timer expires
        // Note: this test deliberately allows for slop in the actual timer
        // values however their relative values should still be correct
        startTime = uPortGetTickTimeMs();
        while ((gTimerParameterValue[2] == 0) &&
               (uPortGetTickTimeMs() - startTime < 10000)) {
            uPortTaskBlock(100);
        }
        U_PORT_TEST_ASSERT((gTimerParameterValue[2] == 1) && (gTimerParameterValue[3] == 3));

        // Stop the periodic timer, make the expiry longer
        // than the one-shot was, and restart both of them
        U_PORT_TEST_ASSERT(uPortTimerStop(gTimerHandle[3]) == 0);
        U_PORT_TEST_ASSERT(uPortTimerChange(gTimerHandle[3], 1200) == 0);
        // Deliberately start both timers twice to ensure
        // that a started timer can be started again successfully
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[2]) == 0);
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[2]) == 0);
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[3]) == 0);
        U_PORT_TEST_ASSERT(uPortTimerStart(gTimerHandle[3]) == 0);
        // Wait for the periodic timer to expire one more time
        startTime = uPortGetTickTimeMs();
        while ((gTimerParameterValue[3] < 4) &&
               (uPortGetTickTimeMs() - startTime < 5000)) {
            uPortTaskBlock(100);
        }
        U_PORT_TEST_ASSERT(gTimerParameterValue[3] == 4);

        // Stop the one-shot timer, which should have expired now
        U_PORT_TEST_ASSERT(uPortTimerStop(gTimerHandle[2]) == 0);
        // Delete the periodic timer without stopping it
        U_PORT_TEST_ASSERT(uPortTimerDelete(gTimerHandle[3]) == 0);
        // Delete the one-shot timer
        U_PORT_TEST_ASSERT(uPortTimerDelete(gTimerHandle[2]) == 0);
        // Delete the second timer we created, which is still hanging around
        U_PORT_TEST_ASSERT(uPortTimerDelete(gTimerHandle[1]) == 0);

        // Wait for the deletions to occur and allow some
        // time also to test if any timers expire more than
        // they should
        uPortTaskBlock(1000);

        // Do a final check of all of the gTimerParameterValues:
        U_TEST_PRINT_LINE("at the end of the timer test:");
        for (size_t x = 0; x < sizeof(gTimerParameterValue) / sizeof(gTimerParameterValue[0]); x++) {
            U_TEST_PRINT_LINE("timer %d expired %d time(s).", x + 1,
                              gTimerParameterValue[x]);
        }
        // The first two never expired, the one-shot timer should
        // have expired twice and the periodic timer four times
        U_PORT_TEST_ASSERT(gTimerParameterValue[0] == 0);
        U_PORT_TEST_ASSERT(gTimerParameterValue[1] == 0);
        U_PORT_TEST_ASSERT(gTimerParameterValue[2] == 2);
        U_PORT_TEST_ASSERT(gTimerParameterValue[3] == 4);

    } else {
        U_TEST_PRINT_LINE("timers are not supported.");
    }

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test critical sections.
 */
U_PORT_TEST_FUNCTION("[port]", "portCriticalSection")
{
    int32_t errorCode;
    int32_t resourceCount;
    uint32_t y;
    int32_t startTimeMs;
    int32_t errorFlag = 0x00;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing critical sections, may take up to %d second(s)...",
                      ((U_PORT_TEST_CRITICAL_SECTION_TEST_TASK_START_TIME_SECONDS * 1000) +
                       (U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS * 2)) / 1000);

    // Create the mutex that allows us to synchronise with the critical
    // section test task
    U_PORT_TEST_ASSERT(uPortMutexCreate(&gMutexHandle) == 0);
    U_PORT_TEST_ASSERT(gMutexHandle != NULL);

    // Create the task
    U_PORT_TEST_ASSERT(uPortTaskCreate(criticalSectionTestTask, "critTestTask",
                                       U_CFG_TEST_OS_TASK_STACK_SIZE_BYTES,
                                       (void *) &gVariable,
                                       U_CFG_TEST_OS_TASK_PRIORITY,
                                       &gTaskHandle) == 0);
    U_PORT_TEST_ASSERT(gTaskHandle != NULL);

    // The task should lock the mutex and begin incrementing the
    // variable we pointed it at
    for (size_t x = 0; (gVariable == 0) &&
         (x < U_PORT_TEST_CRITICAL_SECTION_TEST_TASK_START_TIME_SECONDS); x++) {
        uPortTaskBlock(1000);
    }
    U_PORT_TEST_ASSERT(gVariable > 0);

    // Start the critical section
    startTimeMs = uPortGetTickTimeMs();
    (void)startTimeMs; // Suppress value not being read (it is for Windows)
    errorCode = uPortEnterCritical();
    // Note: don't assert inside here as we don't want to leave this test
    // with the critical section active, instead just set errorFlag to indicate
    // an error that we can assert on once we've left the critical section
    if (!((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED))) {
        errorFlag |= 0x01;
    }
    if (errorCode == 0) {
        // With the critical section running, check that the variable doesn't change
        y = gVariable;
#ifndef _WIN32
        // We can't call task block in here, and we can't guarantee that
        // uPortGetTickTimeMs() will advance, so just busy-wait for a long time
        for (size_t volatile z = 0; z < U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_LOOPS; z++) {}
#else
        // On Windoze we can use the tick and we need to in order that we wait a nice
        // long time to _prove_ that the critical section has worked
        //lint -e{441, 550} Suppress loop variable not used in 2nd part of for()
        for (size_t x = 0; (gVariable == y) &&
             (uPortGetTickTimeMs() - startTimeMs <
              U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS); x++) {
            uPortTaskBlock(100);
        }
#endif
        if (gVariable != y) {
            errorFlag |= 0x02;
        }

        // Leave the critical section
        uPortExitCritical();

        // Now check the error flag
        U_TEST_PRINT_LINE("error flag is 0x%08x.", errorFlag);
        U_PORT_TEST_ASSERT(errorFlag == 0);

        // gVariable should start changing again
        startTimeMs = uPortGetTickTimeMs();
        //lint -e{441, 550} Suppress loop variable not used in 2nd part of for()
        for (size_t x = 0; (gVariable == y) &&
             (uPortGetTickTimeMs() - startTimeMs <
              U_PORT_TEST_CRITICAL_SECTION_TEST_WAIT_TIME_MS); x++) {
            uPortTaskBlock(10);
        }
        U_PORT_TEST_ASSERT(gVariable != y);
    } else {
        U_TEST_PRINT_LINE("critical sections not implemented on this platform,"
                          " so not testing them.");
    }

    // Lock the mutex, which should cause the critical section test task to exit
    U_PORT_TEST_ASSERT(uPortMutexLock(gMutexHandle) == 0);
    // Allow time for the idle task to clean up the task
    uPortTaskBlock(1000);
    // Now it can be deleted
    U_PORT_TEST_ASSERT(uPortMutexUnlock(gMutexHandle) == 0);
    uPortMutexDelete(gMutexHandle);
    gMutexHandle = NULL;

    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[port]", "portCleanUp")
{

#if (U_CFG_APP_GNSS_I2C >= 0)
    if (gI2cHandle >= 0) {
        // Do bus recovery where possible so as not to upset
        // any subsequent tests that use I2C
        if (uPortI2cCloseRecoverBus(gI2cHandle) == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
            uPortI2cClose(gI2cHandle);
        }
    }
    uPortI2cDeinit();
    uPortFree(gpI2cBuffer);
#endif

#if (U_CFG_APP_GNSS_SPI >= 0)
    if (gSpiHandle >= 0) {
        // Clean up SPI
        uPortSpiClose(gSpiHandle);
    }
    uPortSpiDeinit();
#endif

    uPortFree(gpMalloc);

    uPortDeinit();

    U_PORT_TEST_ASSERT(uTestUtilResourceCheck(U_TEST_PREFIX,
                                              U_TEST_UTIL_RESOURCE_CHECK_ERROR_MARKER,
                                              true));
}

// End of file
