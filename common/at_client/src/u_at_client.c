/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the AT client API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // For INT_MAX
#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), strcmp(), strcspn(), strspm()
#include "stdio.h"     // snprintf()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"
#include "u_short_range_pbuf.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** A macro to check that the guard, U_AT_CLIENT_MARKER, is present.
 */
#define U_AT_CLIENT_GUARD_CHECK_ONE(marker) ((*((marker) + 0) == 'D') && \
                                             (*((marker) + 1) == 'E') && \
                                             (*((marker) + 2) == 'A') && \
                                             (*((marker) + 3) == 'D') && \
                                             (*((marker) + 4) == 'B') && \
                                             (*((marker) + 5) == 'E') && \
                                             (*((marker) + 6) == 'E') && \
                                             (*((marker) + 7) == 'F') ? true : false)

/** Macro to check that the given buffer/struct has U_AT_CLIENT_MARKER
 * at either end.
 */
#define U_AT_CLIENT_GUARD_CHECK(pBufStruct) (U_AT_CLIENT_GUARD_CHECK_ONE(pBufStruct->mk0) &&                 \
                                             U_AT_CLIENT_GUARD_CHECK_ONE(((char *) (pBufStruct)) +           \
                                                                         sizeof(uAtClientReceiveBuffer_t) +  \
                                                                         pBufStruct->dataBufferSize))

/** The AT client OK string which marks the end of
 * an AT sequence.
 */
#define U_AT_CLIENT_OK                      "OK\r\n"

/** The length of U_AT_CLIENT_OK in bytes.
 */
#define U_AT_CLIENT_OK_LENGTH_BYTES         4

/** The error string which can mark the end of
 * an AT command sequence.
 */
#define U_AT_CLIENT_ERROR                   "ERROR\r\n"

/** The length of U_AT_CLIENT_ERROR in bytes.
 */
#define U_AT_CLIENT_ERROR_LENGTH_BYTES      7

/** The error string which can mark the end of
 * an AT command sequence if the use aborts it.
 */
#define U_AT_CLIENT_ABORTED                 "ABORTED\r\n"

/** The length of U_AT_CLIENT_ABORTED in bytes.
 */
#define U_AT_CLIENT_ABORTED_LENGTH_BYTES    9

/** The CME ERROR string which can mark the end of
 * an AT command sequence.
 */
#define U_AT_CLIENT_CME_ERROR               "+CME ERROR:"

/** The length of U_AT_CLIENT_CME_ERROR in bytes.
 */
#define U_AT_CLIENT_CME_ERROR_LENGTH_BYTES  11

/** The CMS ERROR string which can mark the end of
 * an AT command sequence.
 */
#define U_AT_CLIENT_CMS_ERROR               "+CMS ERROR:"

/** The length of U_AT_CLIENT_CMS_ERROR in bytes.
 */
#define U_AT_CLIENT_CMS_ERROR_LENGTH_BYTES  11

/** This should be set to at least the maximum length
 * of any of the OK, ERROR, CME ERROR and CMS ERROR
 * strings.
 */
#define U_AT_CLIENT_INITIAL_URC_LENGTH      64

/** The maximum length of prefix to expect in
 * an information response.
 */
#define U_AT_CLIENT_MAX_LENGTH_INFORMATION_RESPONSE_PREFIX 64

/** The maximum length of the callback queue.
 * Each item in the queue will be
 * sizeof(uAtClientCallback_t) bytes big.
 */
#define U_AT_CLIENT_CALLBACK_QUEUE_LENGTH 10

/** Guard for the URC task data receive loop to make
 * sure it can't be drowned by the incoming stream,
 * preventing control commands from getting in.
 */
#define U_AT_CLIENT_URC_DATA_LOOP_GUARD       100

/** Macro that returns the start of the data buffer.
 */
#define U_AT_CLIENT_DATA_BUFFER_PTR(pBufStruct) (((char *) (pBufStruct)) +          \
                                                 sizeof(uAtClientReceiveBuffer_t))

/** Macro to lock the client mutex: as well as the normal case of
 * locking pClient->mutex this has to deal with the situation where
 * we're in the wake-up handler.  In that case we need to block
 * all "normal" access but only allow the wake-up task to call
 * back into here.  If the waking-up task is active and the task
 * asking for the lock is not that task then, as well as waiting
 * on the normal mutex that task needs to be held back until the
 * wake-up handler has completed its work.
 * The task scheduler is suspended while the checks are performed
 * to prevent race conditions.
 * Note this means URCs will be held back during the time we are
 * doing the wake-up.
 */
#define U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient)   {                                                                         \
                                                    uPortMutexHandle_t _mutex = pClient->mutex;                            \
                                                    uPortTaskHandle_t _task;                                               \
                                                    uPortTaskGetHandle(&_task);                                            \
                                                    if (uPortEnterCritical() == 0) {                                       \
                                                        if ((pClient->pWakeUp != NULL) &&                                  \
                                                            (pClient->pWakeUp->wakeUpTask != NULL)) {                      \
                                                            if (_task == pClient->pWakeUp->wakeUpTask) {                   \
                                                                _mutex = pClient->pWakeUp->mutex;                          \
                                                                uPortExitCritical();                                       \
                                                            } else {                                                       \
                                                                uPortExitCritical();                                       \
                                                                uPortMutexLock(pClient->pWakeUp->inWakeUpHandlerMutex);    \
                                                                uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);  \
                                                            }                                                              \
                                                        } else {                                                           \
                                                            uPortExitCritical();                                           \
                                                        }                                                                  \
                                                    }                                                                      \
                                                    uPortMutexLock(_mutex);

/** Macro to unlock the client mutex: just uses the _mutex variable
 * that the U_AT_CLIENT_LOCK_CLIENT_MUTEX() macro set up.
 */
#define U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient)    uPortMutexUnlock(_mutex);  }

#ifndef U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS
/** When performing hysteresis of the activity pin, the interval to use for each
 * wait step; value in milliseconds.
 */
# define U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS 10
#endif

/** The mutex stack, used when locking the stream mutex, required
 * because when the wake-up handler is active there will be two
 * stream mutexes that may be locked: the normal one and the
 * wake-up one; we keep a stack of the locked stream mutex so that
 * we know which one to unlock.
 */
#define U_AT_CLIENT_MUTEX_STACK_MAX_SIZE 2

/** The starting magic number for an AT client: avoiding 0.
 */
#define U_AT_CLIENT_MAGIC_NUMBER_START 1

// Do some cross-checking
#if (U_AT_CLIENT_CALLBACK_TASK_PRIORITY >= U_AT_CLIENT_URC_TASK_PRIORITY)
# error U_AT_CLIENT_CALLBACK_TASK_PRIORITY must be less than U_AT_CLIENT_URC_TASK_PRIORITY
#endif

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
/** Macros for detailed debugging of buffering behaviour.
 * This one for use inside the bufferFill() function.
 */
# define LOG_BUFFER_FILL(place) logDebug(pClient, place,            \
                                         (int32_t) eventIsCallback, \
                                         pData, pDataIntercept,     \
                                         (int32_t) length,          \
                                         (int32_t) x, (int32_t) y,  \
                                         (int32_t) z, readLength)

/** Macros for detailed debugging of buffering behaviour.
 * This one for use in general.
 */
# define LOG(place) logDebug(pClient, place,                        \
                             -1, NULL, NULL, -1, -1, -1, -1, -1)

/** Macros for detailed debugging of buffering behaviour.
 * This one for use in general, with a condition.
 */
# define LOG_IF(cond, place) if (cond) {                               \
                                 logDebug(pClient, place,              \
                                          -1, NULL, NULL, -1, -1, -1,  \
                                          -1, -1);                     \
                             }
#else
# define LOG_BUFFER_FILL(place)
# define LOG(place)
# define LOG_IF(cond, place)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Scope for the parser.
 */
typedef enum {
    U_AT_CLIENT_SCOPE_NONE,
    /** the part of the response that
     * includes the information response
     * (+CMD1,+CMD2..) and ends with OK
     * or (CME)(CMS) ERROR.
     */
    U_AT_CLIENT_SCOPE_RESPONSE,
    /** the information response part of
     * the response,  starts with +CMD1
     * and ends with U_AT_CLIENT_CRLF.
     * The information response contains
     * parameters separated by commas
     * and there may be more than one.
     */
    U_AT_CLIENT_SCOPE_INFORMATION
} uAtClientScope_t;

/** The definition of a URC.
 */
typedef struct uAtClientUrc_t {
    const char *pPrefix;       /** The prefix for this URC, e.g. "+CEREG:". */
    size_t prefixLength;       /** The length of pPrefix. */
    void (*pHandler) (uAtClientHandle_t, void *); /** The handler to call if pPrefix is matched. */
    void *pHandlerParam;       /** The parameter to pass to pHandler. */
    struct uAtClientUrc_t *pNext;
} uAtClientUrc_t;

/** The definition of a tag.
 */
typedef struct {
    const char *pString; /** Pointer to the tag, one of "\r\n", "OK\r\n" and "ERROR\r\n". */
    size_t length; /** The number of characters at pString. */
} uAtClientTagDef_t;

/** Tracker for a tag.
 */
typedef struct {
    const uAtClientTagDef_t *pTagDef; /** Pointer to the tag definition */
    bool found;  /** Keep track of whether the tag has been found or not. */
} uAtClientTag_t;

/** The definition of a receive buffer.  This is only a partial
 * definition, the start of the receive buffer, and is overlaid
 * on the buffer memory that is either passed in or allocated
 * during the initialisation of an AT client.  Immediately beyond
 * it lies the variable length data buffer itself and beyond
 * that U_AT_CLIENT_MARKER_SIZE bytes of the closing marker.
 * Note: if you change this structure you will also need to
 * change U_AT_CLIENT_BUFFER_OVERHEAD_BYTES in u_at_client.h.
 * In order to avoid problems with structure packing and
 * the size calculation the structure must be a multiple of 4 bytes
 * in size; the simplest way to do this is to only put items that
 * are 4 or 8 bytes in size into it.
 */
typedef struct {
    size_t isMalloced;  /** Set to 1 to indicate that data buffer was malloced. */
    size_t dataBufferSize; /** The size of the data buffer which follows this. */
    size_t length;     /** The number of characters that may be read from the buffer. */
    size_t lengthBuffered;  /** The number of bytes in the buffer: may be larger
                                than length if there is an intercept function
                                active and it hasn't yet pocessed the extra
                                bytes into readable characters. */
    size_t readIndex;  /** The read start position for characters in the buffer. */
    char mk0[U_AT_CLIENT_MARKER_SIZE]; /** Opening marker. */
} uAtClientReceiveBuffer_t;

/** Blocking states for the bufferFill() function.
 */
typedef enum {
    U_AT_CLIENT_BLOCK_STATE_NOTHING_RECEIVED,
    U_AT_CLIENT_BLOCK_STATE_WAIT_FOR_MORE,
    U_AT_CLIENT_BLOCK_STATE_DO_NOT_BLOCK
} uAtClientBlockState_t;

/** A struct defining a callback plus its optional parameter.
 */
typedef struct {
    void (*pFunction) (uAtClientHandle_t, void *);
    uAtClientHandle_t atHandle;
    void *pParam;
    int32_t atClientMagicNumber;
} uAtClientCallback_t;

/** Struct defining a wake-up handler.
 */
typedef struct {
    int32_t (*pHandler) (uAtClientHandle_t, void *);
    void *pParam;
    uPortMutexHandle_t mutex;
    uPortMutexHandle_t streamMutex;
    uPortMutexHandle_t inWakeUpHandlerMutex;
    uPortTaskHandle_t wakeUpTask;
    int32_t inactivityTimeoutMs;
    int32_t atTimeoutSavedMs;
} uAtClientWakeUp_t;

/** Struct defining an activity pin.
 */
typedef struct {
    int32_t pin;
    int32_t readyMs;
    bool highIsOn;
    int32_t lastToggleTime;
    int32_t hysteresisMs;
} uAtClientActivityPin_t;

/** Struct defining a stack of mutexes.
 */
typedef struct {
    uPortMutexHandle_t stack[U_AT_CLIENT_MUTEX_STACK_MAX_SIZE];
    uPortMutexHandle_t *pNextFree;
} uAtClientMutexStack_t;

/** Definition of an AT client instance.
 */
typedef struct uAtClientInstance_t {
    int32_t magicNumber; /** The magic number that uniquely identifies this AT client. */
    int32_t streamHandle; /** The stream handle to use. */
    uAtClientStream_t streamType; /** The type of API that streamHandle applies to. */
    uPortMutexHandle_t mutex; /** Mutex for threadsafeness. */
    uPortMutexHandle_t streamMutex; /** Mutex for the data stream. */
    uPortMutexHandle_t urcPermittedMutex; /** Mutex that we can use to avoid trampling on a URC. */
    uAtClientReceiveBuffer_t *pReceiveBuffer; /** Pointer to the receive buffer structure. */
    bool debugOn; /** Whether general debug is on or off. */
    bool printAtOn; /** Whether printing of AT commands and responses is on or off. */
    int32_t atTimeoutMs; /** The current AT timeout in milliseconds. */
    int32_t atTimeoutSavedMs; /** The saved AT timeout in milliseconds. */
    int32_t numConsecutiveAtTimeouts; /** The number of consecutive AT timeouts. */
    /** Callback to call if numConsecutiveAtTimeouts > 0. */
    void (*pConsecutiveTimeoutsCallback) (uAtClientHandle_t, int32_t *);
    char delimiter; /** The delimiter used between parameters. */
    int32_t delayMs; /** The delay from ending one AT command to starting the next. */
    uErrorCode_t error; /** The current error status. */
    uAtClientDeviceError_t deviceError; /** The error reported by the AT server. */
    uAtClientScope_t scope; /** The scope, where we're at in the AT command. */
    uAtClientTag_t stopTag; /** The stop tag for the current scope. */
    uAtClientUrc_t *pUrcList; /** Linked-list anchor for URC handlers. */
    int32_t lastResponseStopMs; /** The time the last response ended in milliseconds. */
    int32_t lockTimeMs; /** The time when the stream was locked. */
    int32_t lastTxTimeMs; /** The time when the last transmit activity was carried out, set to -1 initially. */
    size_t urcMaxStringLength; /** The longest URC string to monitor for. */
    size_t maxRespLength; /** The max length of OK, (CME) (CMS) ERROR and URCs. */
    bool delimiterRequired; /** Is a delimiter to be inserted before the next parameter or not. */
    uAtClientMutexStack_t lockedStreamMutexStack; /** A place to store locked stream mutexes. */
    const char *(*pInterceptTx) (uAtClientHandle_t,
                                 const char **,
                                 size_t *,
                                 void *); /** Function that intercepts Tx data before it
                                              is given to the stream. */
    void *pInterceptTxContext; /** Context pointer that will be passed to pInterceptTx
                                   as its fourth parameter. */
    char *(*pInterceptRx) (uAtClientHandle_t,
                           char **, size_t *,
                           void *); /** Function that intercepts Rx data before it is
                                        processed by the AT client. */
    void *pInterceptRxContext; /** Context pointer that will be passed to pInterceptRx
                                   as its fourth parameter. */
    uAtClientWakeUp_t *pWakeUp; /** Pointer to a wake-up handler structure. */
    uAtClientActivityPin_t *pActivityPin; /** Pointer to an activity pin structure. */
    struct uAtClientInstance_t *pNext;
} uAtClientInstance_t;

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
/** Structure used for detailed debugging of the AT client
 * buffering behaviour, used in particular to debug the
 * intercept functions.
 * This struct should only contain multiples of int32_t,
 * no gaps as we memcmp() it.
 */
typedef struct {
    int32_t timeMs; /**< Must be first to fall outside our memcmp(). */
    size_t place; /**< Must be second to fall outside our memcmp(). */
    const uAtClientInstance_t *pClient;
    int32_t inUrc; /**< 1 for yes, 0 for no, -1 for don't know. */
    const char *pDataBufferStart; /**< from uAtClientReceiveBuffer_t. */
    size_t dataBufferSize; /**< from uAtClientReceiveBuffer_t. */
    size_t dataBufferLength; /**< from uAtClientReceiveBuffer_t. */
    size_t dataBufferLengthBuffered; /**< from uAtClientReceiveBuffer_t. */
    size_t dataBufferReadIndex; /**< from uAtClientReceiveBuffer_t. */
    const char *pData;  /**< from bufferFill(). */
    const char *pDataIntercept;  /**< from bufferFill(). */
    int32_t length;  /**< from bufferFill(). */
    int32_t x;  /**< from bufferFill(). */
    int32_t y;  /**< from bufferFill(). */
    int32_t z;  /**< from bufferFill(). */
    int32_t readLength;  /**< from bufferFill(). */
} uAtClientDetailedDebug_t;

#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root for the linked list of AT clients.
 */
static uAtClientInstance_t *gpAtClientList = NULL;

/** Mutex to protect the linked list and
 * other global operations.
 */
static uPortMutexHandle_t gMutex = NULL;

/** As well as the linked list of AT clients we keep a list of
 * the magic numbers related to each AT client as an array.  This
 * is so that we can mark an AT client as not reacting to
 * asynchronous events (by removing it from the array).
 * Note: we can't run through the linked list for this kind of
 * thing as that would require a lock on gMutex and the asynchronous
 * event may not be able to obtain such a lock.
 */
static int32_t gAtClientMagicNumberProcessAsync[U_AT_CLIENT_MAX_NUM] = {0};

/** The next AT client magic number to use.
 */
static int32_t gAtClientMagicNumberNext = U_AT_CLIENT_MAGIC_NUMBER_START;

/** Definition of an information stop tag.
 */
static const uAtClientTagDef_t gInformationStopTag = {U_AT_CLIENT_CRLF,
                                                      U_AT_CLIENT_CRLF_LENGTH_BYTES
                                                     };

/** Definition of a response stop tag.
 */
static const uAtClientTagDef_t gResponseStopTag = {U_AT_CLIENT_OK,
                                                   U_AT_CLIENT_OK_LENGTH_BYTES
                                                  };

/** Definition of no stop tag.
 */
static const uAtClientTagDef_t gNoStopTag = {"", 0};

/** The event queue for callbacks.
 */
static int32_t gEventQueueHandle;

/** Mutex to protect gEventQueueHandle.
 * Note: the reason for this being separate to gMutex is
 * because uAtClientCallback(), which needs to ensure that
 * gEventQueueHandle is good, can be called by a URC callback.
 * If a URC lands while we're in uAtClientResponseStart(),
 * the URC callback will be called directly from within
 * uAtClientResponseStart(), rather than by the separate
 * URC task.  Since uAtClientResponseStart() must lock
 * gMutex while it runs gMutex can't also be locked
 * by uAtClientCallback() so we need a separate
 * mutex for the protection of gEventQueueHandle.
 */
static uPortMutexHandle_t gMutexEventQueue = NULL;

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
/** Array for detailed debugging.
 */
static uAtClientDetailedDebug_t gDebug[1000];

/** Current position in gDebug.
 */
static size_t gDebugIndex = 0;

/** Whether detailed logging is on or not.
 */
static bool gDebugOn = false;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
// Log the detailed debug.
static void logDebug(const uAtClientInstance_t *pClient,
                     int32_t place, int32_t inUrc, const char *pData,
                     const char *pDataIntercept, int32_t length,
                     int32_t x, int32_t y, int32_t z,
                     int32_t readLength)
{
    uAtClientDetailedDebug_t *pDebug;

    if (gDebugOn && (gDebugIndex < sizeof(gDebug) / sizeof(gDebug[0]))) {
        pDebug = &(gDebug[gDebugIndex]);

        pDebug->timeMs = (int32_t) uPortGetTickTimeMs();
        pDebug->place = place;
        pDebug->pClient = pClient;
        pDebug->inUrc = inUrc;

        pDebug->pDataBufferStart = U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer);
        pDebug->dataBufferSize = pClient->pReceiveBuffer->dataBufferSize;
        pDebug->dataBufferLength = pClient->pReceiveBuffer->length;
        pDebug->dataBufferLengthBuffered = pClient->pReceiveBuffer->lengthBuffered;
        pDebug->dataBufferReadIndex = pClient->pReceiveBuffer->readIndex;

        pDebug->pData = pData;
        pDebug->pDataIntercept = pDataIntercept;
        pDebug->length = length;
        pDebug->x = x;
        pDebug->y = y;
        pDebug->z = z;
        pDebug->readLength = readLength;

        // Only keep it if it is different
        // bar the initial 32-bit timestamp
        // and 32-bit "place"
        if ((gDebugIndex == 0) ||
            (memcmp(((int32_t *) pDebug) + 2, ((int32_t *) & (gDebug[gDebugIndex - 1])) + 2,
                    sizeof(*pDebug) - (sizeof(int32_t) * 2)) != 0)) {
            gDebugIndex++;
        }
    }
}

// Print out the detailed debug log.
static void printLogDebug(const uAtClientDetailedDebug_t *pDebug,
                          size_t number)
{
    char c;

    for (size_t x = 0; x < number; x++) {
        uPortLog("U_AT_CLIENT_%d-%d: %4d %3d",
                 pDebug->pClient->streamType,
                 pDebug->pClient->streamHandle,
                 x, pDebug->place);
        c = ' ';
        if (pDebug->inUrc == 0) {
            c = 'U';
        } else if (pDebug->inUrc < 0) {
            c = '?';
        }
        uPortLog(" %c @ %8d:", c, pDebug->timeMs);
        uPortLog(" buffer 0x%08x (%d)  ri %d  l %d lb %d, ",
                 (int) pDebug->pDataBufferStart,
                 pDebug->dataBufferSize,
                 pDebug->dataBufferReadIndex,
                 pDebug->dataBufferLength,
                 pDebug->dataBufferLengthBuffered);
        uPortLog(" pD 0x%08x pDI 0x%08x l %d x %d y %d z %d rl %d.\n",
                 (int) pDebug->pData,
                 (int) pDebug->pDataIntercept,
                 pDebug->length,
                 pDebug->x, pDebug->y, pDebug->z,
                 pDebug->readLength);
        pDebug++;
    }
}
#endif

// Find an AT client instance in the list by stream handle.
// gMutex should be locked before this is called.
static uAtClientInstance_t *pGetAtClientInstance(int32_t streamHandle,
                                                 uAtClientStream_t streamType)
{
    uAtClientInstance_t *pClient = gpAtClientList;

    while ((pClient != NULL) &&
           !((pClient->streamType == streamType) && (pClient->streamHandle == streamHandle))) {
        pClient = pClient->pNext;
    }

    return pClient;
}

// Get the number of AT clients currently active;
// gMutex should be locked before this is called.
static size_t numAtClients()
{
    size_t numClients = 0;
    uAtClientInstance_t *pClient = gpAtClientList;

    while (pClient != NULL) {
        pClient = pClient->pNext;
        numClients++;
    }

    return numClients;
}

// Add an AT client instance to the list.
// gMutex should be locked before this is called.
// Note: doesn't copy it, just adds it.
static void addAtClientInstance(uAtClientInstance_t *pClient)
{
    bool done = false;

    // Populate the magic number
    pClient->magicNumber = gAtClientMagicNumberNext;
    gAtClientMagicNumberNext++;
    if (gAtClientMagicNumberNext < U_AT_CLIENT_MAGIC_NUMBER_START) {
        gAtClientMagicNumberNext = U_AT_CLIENT_MAGIC_NUMBER_START;
    }
    for (size_t x = 0; !done &&
         (x < sizeof(gAtClientMagicNumberProcessAsync) / sizeof(gAtClientMagicNumberProcessAsync[0])); x++) {
        if (gAtClientMagicNumberProcessAsync[x] == 0) {
            gAtClientMagicNumberProcessAsync[x] = pClient->magicNumber;
            done = true;
        }
    }
    U_ASSERT(done);

    // Add to the list
    pClient->pNext = gpAtClientList;
    gpAtClientList = pClient;
}

// Mark an AT client as not processing asynchronous data.
// gMutex should be locked before this is called.
static void ignoreAsync(const uAtClientInstance_t *pClient)
{
    bool done = false;

    for (size_t x = 0; !done &&
         (x < sizeof(gAtClientMagicNumberProcessAsync) / sizeof(gAtClientMagicNumberProcessAsync[0])); x++) {
        if (gAtClientMagicNumberProcessAsync[x] == pClient->magicNumber) {
            // Remove the magic number from the list
            gAtClientMagicNumberProcessAsync[x] = 0;
            done = true;
        }
    }
}

// Remove an AT client instance from the list.
// gMutex should be locked before this is called.
// Note: doesn't free it, the caller must do that.
static void removeAtClientInstance(const uAtClientInstance_t *pClient)
{
    uAtClientInstance_t *pCurrent;
    uAtClientInstance_t *pPrev = NULL;

    // Remove the AT client from the linked list
    pCurrent = gpAtClientList;
    while (pCurrent != NULL) {
        if (pClient == pCurrent) {
            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                gpAtClientList = pCurrent->pNext;
            }
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
}

// Remove an AT client.
// gMutex should be locked before this is called.
static void removeClient(uAtClientInstance_t *pClient)
{
    uAtClientUrc_t *pUrc;

    // Must not be in a wake-up handler
    U_ASSERT((pClient->pWakeUp == NULL) ||
             ((uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) == 0) &&
              // This just to unlock the mutex if the try succeeded
              (uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex) == 0)));

    // Avoid pulling the rug out from under a URC
    U_PORT_MUTEX_LOCK(pClient->urcPermittedMutex);

    // Lock the stream also, for safety
    U_PORT_MUTEX_LOCK(pClient->streamMutex);

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    // Remove it from the list
    removeAtClientInstance(pClient);

    // Mark the AT client as not processing asynchronous data
    ignoreAsync(pClient);

    // Remove the URC event handler, which may be running
    // asynchronous stuff and so has to be flushed and
    // closed before we mess with anything else
    switch (pClient->streamType) {
        case U_AT_CLIENT_STREAM_TYPE_UART:
            uPortUartEventCallbackRemove(pClient->streamHandle);
            break;
        case U_AT_CLIENT_STREAM_TYPE_EDM:
            uShortRangeEdmStreamAtCallbackRemove(pClient->streamHandle);
            break;
        default:
            break;
    }

    // Free any URC handlers it had.
    while (pClient->pUrcList != NULL) {
        pUrc = pClient->pUrcList;
        pClient->pUrcList = pUrc->pNext;
        uPortFree(pUrc);
    }

    // Remove any activity pin
    uPortFree(pClient->pActivityPin);

    // Free the receive buffer if it was allocated.
    if (pClient->pReceiveBuffer->isMalloced) {
        uPortFree(pClient->pReceiveBuffer);
    }

    // Unlock its main mutex so that we can delete it
    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
    uPortMutexDelete(pClient->mutex);

    // Remove any wake-up handler: this must be
    // done after the lock is lifted since that
    // may want to unlock pClient->pWakeUp->mutex
    if (pClient->pWakeUp != NULL) {
        U_PORT_MUTEX_LOCK(pClient->pWakeUp->inWakeUpHandlerMutex);
        U_PORT_MUTEX_UNLOCK(pClient->pWakeUp->inWakeUpHandlerMutex);
        uPortMutexDelete(pClient->pWakeUp->inWakeUpHandlerMutex);
        U_PORT_MUTEX_LOCK(pClient->pWakeUp->mutex);
        U_PORT_MUTEX_UNLOCK(pClient->pWakeUp->mutex);
        uPortMutexDelete(pClient->pWakeUp->mutex);
        U_PORT_MUTEX_LOCK(pClient->pWakeUp->streamMutex);
        U_PORT_MUTEX_UNLOCK(pClient->pWakeUp->streamMutex);
        uPortMutexDelete(pClient->pWakeUp->streamMutex);
        uPortFree(pClient->pWakeUp);
    }

    // Delete the stream mutex
    U_PORT_MUTEX_UNLOCK(pClient->streamMutex);
    uPortMutexDelete(pClient->streamMutex);

    // Delete the URC active mutex
    U_PORT_MUTEX_UNLOCK(pClient->urcPermittedMutex);
    uPortMutexDelete(pClient->urcPermittedMutex);

    // And finally free the client context.
    uPortFree(pClient);
}

// Check if an asynchronous event should be processed
// for the given AT client.
static bool processAsync(int32_t magicNumber)
{
    bool process = false;

    for (size_t x = 0; !process &&
         (x < sizeof(gAtClientMagicNumberProcessAsync) / sizeof(gAtClientMagicNumberProcessAsync[0])); x++) {
        if (gAtClientMagicNumberProcessAsync[x] == magicNumber) {
            process = true;
        }
    }

    return process;
}

// Initialse a mutex stack.
static void mutexStackInit(uAtClientMutexStack_t *pStack)
{
    if (pStack->pNextFree == NULL) {
        pStack->pNextFree = pStack->stack;
    }
}

// Push an entry to a stack of mutexes.
static void mutexStackPush(uAtClientMutexStack_t *pStack,
                           uPortMutexHandle_t mutex)
{
    // If uPortEnterCritical() is not implemented then
    // there must only ever be one entry in the stack so that
    // no thread-safety issues can occur
    uPortEnterCritical();
    // NOTE: these asserts are, necessarily and obviously,
    // within a critical section.  The default U_ASSERT handler
    // simply prints something out and, when that is done while
    // in a critical section, it may cause a subsequent assert:
    // for instance with newlib under GCC you will see an
    // assert going off in lock.c because newlib wants to
    // lock the stdout stream for the print.
    U_ASSERT(pStack->pNextFree >= pStack->stack);
    U_ASSERT(pStack->pNextFree < pStack->stack + (sizeof(pStack->stack) / sizeof(pStack->stack[0])));
    *(pStack->pNextFree) = mutex;
    (pStack->pNextFree)++;
    uPortExitCritical();
}

// Pop an entry from a stack of mutexes.
static uPortMutexHandle_t mutexStackPop(uAtClientMutexStack_t *pStack)
{
    uPortMutexHandle_t mutex = NULL;

    // If uPortEnterCritical() is not implemented then
    // there must only ever be one entry in the stack so that
    // no thread-safety issues can occur
    // Note: we allow this to be called "out of step" with
    // the push operation, i.e. it can return a value of
    // NULL if there is nothing to pop, it is up to the
    // caller to handle that case
    uPortEnterCritical();
    if (pStack->pNextFree > pStack->stack) {
        (pStack->pNextFree)--;
        mutex = *(pStack->pNextFree);
    }
    uPortExitCritical();

    return mutex;
}

// Lock an AT stream, returning the one that was locked.
static uPortMutexHandle_t streamLock(const uAtClientInstance_t *pClient)
{
    uPortMutexHandle_t streamMutex = pClient->streamMutex;
    uPortTaskHandle_t task;

    // Just like the U_AT_CLIENT_LOCK_CLIENT_MUTEX macro,
    // lock the normal streamMutex unless we have a
    // wake-up handler and it is active, in which case
    // lock the streamMutex of the wake-up context if
    // we are in that task; if we're NOT in that task
    // then wait for the inWakeUpHandlerMutex to be
    // released in order to be sure that everything
    // has gone back to normal before we do anything
    // (and use the normal streamMutex)
    uPortTaskGetHandle(&task);
    if (uPortEnterCritical() == 0) {
        if ((pClient->pWakeUp != NULL) && (pClient->pWakeUp->wakeUpTask != NULL)) {
            if (task == pClient->pWakeUp->wakeUpTask) {
                streamMutex = pClient->pWakeUp->streamMutex;
                uPortExitCritical();
            } else {
                uPortExitCritical();
                uPortMutexLock(pClient->pWakeUp->inWakeUpHandlerMutex);
                uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);
            }
        } else {
            uPortExitCritical();
        }
    }

    uPortMutexLock(streamMutex);

    return streamMutex;
}

// Try to lock an AT stream, returning the one that was locked or NULL.
static uPortMutexHandle_t streamTryLock(const uAtClientInstance_t *pClient,
                                        int32_t timeoutMs)
{
    uPortMutexHandle_t streamMutex = pClient->streamMutex;

    // Same logic as streamLock()
    if (uPortEnterCritical() == 0) {
        if ((pClient->pWakeUp != NULL) && (pClient->pWakeUp->wakeUpTask != NULL)) {
            if (uPortTaskIsThis(pClient->pWakeUp->wakeUpTask)) {
                streamMutex = pClient->pWakeUp->streamMutex;
                uPortExitCritical();
            } else {
                uPortExitCritical();
                // Note: we can't "try" this one, if we're not in the
                // wake-up task we really have to wait for it to exit
                uPortMutexLock(pClient->pWakeUp->inWakeUpHandlerMutex);
                uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);
            }
        } else {
            uPortExitCritical();
        }
    }

    if (uPortMutexTryLock(streamMutex, timeoutMs) < 0) {
        streamMutex = NULL;
    }

    return streamMutex;
}

// Find one character buffer inside another.
static const char *pMemStr(const char *pBuffer,
                           size_t bufferLength,
                           const char *pFind,
                           size_t findLength)
{
    const char *pPos = NULL;

    if (bufferLength >= findLength) {
        for (size_t x = 0; (pPos == NULL) &&
             (x < (bufferLength - findLength) + 1); x++) {
            if (memcmp(pBuffer + x, pFind, findLength) == 0) {
                pPos = pBuffer + x;
            }
        }
    }

    return pPos;
}

// Print out AT commands and responses.
static void printAt(const uAtClientInstance_t *pClient,
                    const char *pAt, size_t length)
{
    char c;

    if (pClient->printAtOn) {
        for (size_t x = 0; x < length; x++) {
            c = *pAt++;
            if (!isprint((int32_t) c)) {
#ifdef U_AT_CLIENT_PRINT_CONTROL_CHARACTERS
                uPortLog("[%02x]", (unsigned char) c);
#else
                if (c == '\r') {
                    // Convert \r\n into \n
                    uPortLog("%c", '\n');
                } else if (c == '\n') {
                    // Do nothing
                } else {
                    // Print the hex
                    uPortLog("[%02x]", (unsigned char) c);
                }
#endif
            } else {
                // Print the ASCII character
                uPortLog("%c", c);
            }
        }
    }
}

// Set error.
static void setError(uAtClientInstance_t *pClient,
                     uErrorCode_t error)
{
    if (error != U_ERROR_COMMON_SUCCESS) {
        if (pClient->debugOn) {
            uPortLog("U_AT_CLIENT_%d-%d: AT error %d.\n",
                     pClient->streamType, pClient->streamHandle,
                     error);
        }
    }
    pClient->error = error;
}

// Clear errors.
// gMutex should be locked before this is called.
static void clearError(uAtClientInstance_t *pClient)
{
    pClient->deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR;
    pClient->deviceError.code = 0;
    setError(pClient, U_ERROR_COMMON_SUCCESS);
}

// Increment the number of consecutive timeouts
// and call the callback if there is one
static void consecutiveTimeout(uAtClientInstance_t *pClient)
{
    uAtClientCallback_t cb;

    U_PORT_MUTEX_LOCK(gMutexEventQueue);

    pClient->numConsecutiveAtTimeouts++;
    if (pClient->pConsecutiveTimeoutsCallback != NULL) {
        // pConsecutiveTimeoutsCallback second parameter
        // is an int32_t pointer but of course the generic
        // callback function is a void pointer so
        // need to cast here
        cb.pFunction = (void (*) (uAtClientHandle_t, void *)) pClient->pConsecutiveTimeoutsCallback;
        cb.atHandle = (uAtClientHandle_t) pClient;
        cb.pParam = &(pClient->numConsecutiveAtTimeouts);
        cb.atClientMagicNumber = pClient->magicNumber;
        uPortEventQueueSend(gEventQueueHandle, &cb, sizeof(cb));
    }

    U_PORT_MUTEX_UNLOCK(gMutexEventQueue);
}

// Calculate the remaining time for polling based on the start
// time and the AT timeout. Returns the time remaining for
// polling in milliseconds.
static int32_t pollTimeRemaining(int32_t atTimeoutMs,
                                 int32_t lockTimeMs)
{
    int32_t timeRemainingMs;
    int32_t now = uPortGetTickTimeMs();

    if (atTimeoutMs >= 0) {
        if (now - lockTimeMs > atTimeoutMs) {
            timeRemainingMs = 0;
        } else if (lockTimeMs + atTimeoutMs - now > INT_MAX) {
            timeRemainingMs = INT_MAX;
        } else {
            timeRemainingMs = lockTimeMs + atTimeoutMs - now;
        }
    } else {
        timeRemainingMs = 0;
    }

    // No need to worry about overflow here, we're never awake
    // for long enough
    return (int32_t) timeRemainingMs;
}

// Zero the buffer.
// totalReset also clears out any buffered data that
// may be awaiting processing by a receive intercept
// function
static void bufferReset(const uAtClientInstance_t *pClient,
                        bool totalReset)
{
    uAtClientReceiveBuffer_t *pBuffer = pClient->pReceiveBuffer;

    LOG_IF(totalReset, 200);
    // If there is no receive intercept function then
    // the buffered data can be reset also
    if (totalReset || (pClient->pInterceptRx == NULL)) {
        pBuffer->lengthBuffered = 0;
    }

    if (pBuffer->lengthBuffered > 0) {
        LOG_IF(!totalReset, 201);
        if (pBuffer->length > pBuffer->lengthBuffered) {
            // This should never occur, but if it did
            // it would not be good so best be safe.
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: *** WARNING ***"
                         " lengthBuffered (%d) > length (%d).\n",
                         pBuffer->lengthBuffered, pBuffer->length);
            }
            pBuffer->length = pBuffer->lengthBuffered;
        }
        // If there is stuff buffered, which will be beyond
        // length, need to move that down when we reset
        memmove(((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t),
                ((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t) + pBuffer->length,
                pBuffer->lengthBuffered - pBuffer->length);
        U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pBuffer));
        pBuffer->lengthBuffered -= pBuffer->length;
    }
    pBuffer->readIndex = 0;
    pBuffer->length = 0;
}

// Set the read position to 0 and move the buffer's
// unread content to the beginning.
static void bufferRewind(const uAtClientInstance_t *pClient)
{
    uAtClientReceiveBuffer_t *pBuffer = pClient->pReceiveBuffer;

    LOG(100);
    if ((pBuffer->readIndex > 0) &&
        (pBuffer->length >= pBuffer->readIndex)) {
        if (pBuffer->lengthBuffered < pBuffer->readIndex) {
            // This should never occur, but if it did
            // it would not be good so best be safe.
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: *** WARNING ***"
                         " lengthBuffered (%d) < readIndex (%d).\n",
                         pBuffer->lengthBuffered, pBuffer->readIndex);
            }
            pBuffer->lengthBuffered = pBuffer->readIndex;
        }
        pBuffer->length -= pBuffer->readIndex;
        pBuffer->lengthBuffered -= pBuffer->readIndex;
        LOG(101);
        // Move what has not been read to the
        // beginning of the buffer
        memmove(((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t),
                ((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t) + pBuffer->readIndex,
                pBuffer->lengthBuffered);
        U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pBuffer));
        pBuffer->readIndex = 0;
        LOG(102);
    }
}

// Read from the UART interface in nice coherent lines.
static int32_t uartReadNoStutter(uAtClientInstance_t *pClient,
                                 uAtClientBlockState_t blockState,
                                 int32_t atTimeoutMs)
{
    int32_t readLength = 0;
    int32_t thisReadLength;
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    char *pBuffer = U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                    pReceiveBuffer->lengthBuffered;
    int32_t bufferSize = pReceiveBuffer->dataBufferSize -
                         pReceiveBuffer->lengthBuffered;

    // Retry the read until we're sure there's nothing
    do {
        thisReadLength = uPortUartRead(pClient->streamHandle,
                                       pBuffer, bufferSize);
        if (thisReadLength > 0) {
            readLength += thisReadLength;
            pBuffer += thisReadLength;
            bufferSize -= thisReadLength;
            if (blockState == U_AT_CLIENT_BLOCK_STATE_NOTHING_RECEIVED) {
                // Got something: now wait for more
                blockState = U_AT_CLIENT_BLOCK_STATE_WAIT_FOR_MORE;
                uPortTaskBlock(U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS);
            }
        } else {
            if (blockState == U_AT_CLIENT_BLOCK_STATE_WAIT_FOR_MORE) {
                // We were waiting for more but we have received nothing
                // so stop blocking now
                blockState = U_AT_CLIENT_BLOCK_STATE_DO_NOT_BLOCK;
            } else {
                uPortTaskBlock(U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS);
            }

        }
    } while ((bufferSize > 0) &&
             (blockState != U_AT_CLIENT_BLOCK_STATE_DO_NOT_BLOCK) &&
             (pollTimeRemaining(atTimeoutMs, pClient->lockTimeMs) > 0));

    return readLength;
}

// This is where data comes into the AT client.
// Read from the stream into the receive buffer.
// Returns true on a successful read or false on timeout.
static bool bufferFill(uAtClientInstance_t *pClient,
                       bool blocking)
{
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    int32_t atTimeoutMs = -1;
    int32_t readLength = 0;
    size_t x = 0;
    size_t y = 0;
    size_t z = 0;
    size_t length;
    bool eventIsCallback = false;
    char *pData = NULL;
    //lint -esym(838, pDataIntercept) Suppress initial value not used: it
    // is if detailed debugging is on
    char *pDataIntercept = NULL;
    uAtClientBlockState_t blockState = U_AT_CLIENT_BLOCK_STATE_DO_NOT_BLOCK;

    // Determine if we're in a callback or not
    switch (pClient->streamType) {
        case U_AT_CLIENT_STREAM_TYPE_UART:
            eventIsCallback = uPortUartEventIsCallback(pClient->streamHandle);
            break;
        case U_AT_CLIENT_STREAM_TYPE_EDM:
            eventIsCallback = uShortRangeEdmStreamAtEventIsCallback(pClient->streamHandle);
            break;
        default:
            break;
    }

    if (pReceiveBuffer->lengthBuffered < pReceiveBuffer->length) {
        // This should never occur, but if it did
        // it would not be good so best be safe.
        if (pClient->debugOn) {
            // Let the world know, even if we're in a callback,
            // as this is important.
            uPortLog("U_AT_CLIENT_%d-%d: *** WARNING ***"
                     " lengthBuffered (%d) < length (%d).\n",
                     pReceiveBuffer->lengthBuffered,
                     pReceiveBuffer->length);
        }
        pReceiveBuffer->lengthBuffered = pReceiveBuffer->length;
    }

    length = pReceiveBuffer->lengthBuffered - pReceiveBuffer->length;

    // The receive buffer looks like this:
    //
    // +--------+-------------+-------------------------------+
    // |  read  |    unread   |            buffered           |
    // +--------+-------------+-------------------------------+
    //      readIndex       length                       lengthBuffered
    //
    // Up to "length" is stuff that is AT command stuff or whatever
    // received from the UART, readIndex is how far into that has
    // been read off by the AT parsing code.  Normally "length" and
    // "lengthBuffered" are the same, they only differ if there is
    // an active intercept function, e.g. for C2C security; stuff
    // between "length" and lengthBuffered has not yet been
    // processed by the intercept function (e.g. it's just new or
    // there's not enough of it to form some sort of frame structure
    // that the intercept function needs).  The intercept function
    // reads the stuff between "length" and lengthBuffered at which
    // point it may make it available as normal stuff which this
    // function then copies down into the unread part of "length".

    LOG_BUFFER_FILL(1);

    // If we're blocking, set blockState as appropriate and
    // set the timeout value.
    if (blocking) {
        blockState = U_AT_CLIENT_BLOCK_STATE_NOTHING_RECEIVED;
        atTimeoutMs = pClient->atTimeoutMs;
        if (eventIsCallback) {
            // Short timeout if we're in a URC callback
            atTimeoutMs = U_AT_CLIENT_URC_TIMEOUT_MS;
        }
    }

    // Reset buffer if it has become full
    if (pReceiveBuffer->lengthBuffered == pReceiveBuffer->dataBufferSize) {
#if U_CFG_OS_CLIB_LEAKS
        // If the C library leaks then don't print
        // in a callback as it will leak
        if (!eventIsCallback) {
#endif
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: !!! overflow.\n",
                         pClient->streamType, pClient->streamHandle);
            }
            printAt(pClient, U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer),
                    pReceiveBuffer->length);
#if U_CFG_OS_CLIB_LEAKS
        }
#endif
        LOG_BUFFER_FILL(2);
        bufferReset(pClient, true);
    }

    // Set up the pointer for the intercept function,
    // if there is one
    pDataIntercept = U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                     pReceiveBuffer->length;
    LOG_BUFFER_FILL(3);
    // Do the read
    do {
        switch (pClient->streamType) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                readLength = uartReadNoStutter(pClient, blockState, atTimeoutMs);
                break;
            case U_AT_CLIENT_STREAM_TYPE_EDM:
                readLength = uShortRangeEdmStreamAtRead(pClient->streamHandle,
                                                        U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                                        pReceiveBuffer->length,
                                                        pReceiveBuffer->dataBufferSize -
                                                        pReceiveBuffer->length);
                break;
            default:
                break;
        }
        LOG_BUFFER_FILL(4);

        if (readLength > 0) {
            // lengthBuffered is advanced by the amount we have
            // read in; may not be the same as the amount of data
            // available in the buffer for the AT client as
            // there may be an intercept function in the way
            pReceiveBuffer->lengthBuffered += readLength;
            // length starts out as the amount of data that has not yet
            // been successfully processed by the intercept function
            length += readLength;
        }
        x = length;
        LOG_BUFFER_FILL(5);

        if ((pClient->pInterceptRx != NULL) && (length > 0)) {
            // There's an intercept function and either we've just
            // read some new data or there is some left in the buffer
            // to be processed from last time.  The length available
            // to the AT parser is now determined by the intercept
            // function
            readLength = 0;
            // Run around the loop until the intercept function has
            // nothing more to give
            do {
                LOG_BUFFER_FILL(6);
                pData = pClient->pInterceptRx((uAtClientHandle_t) pClient,
                                              &pDataIntercept, &length,
                                              pClient->pInterceptRxContext);
                // length is now the length of the data that has been PROCESSED
                // by the intercept function and is ready to be AT-parsed.
                LOG_BUFFER_FILL(7);
                U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pReceiveBuffer));

                // Safety check
                if (length > x) {
                    length = x;
                }

                if (pData != NULL) {
                    LOG_BUFFER_FILL(8);
                    // length is the amount of usable data but it may
                    // be somewhere further on in the buffer (as
                    // pointed to by pData) so copy everything down in
                    // the buffer to make it contiguous.
                    //
                    // In the picture of the buffer below, "read" is stuff
                    // the AT client has dealt with, "unread" is stuff that
                    // has been processed by the intercept function but the AT
                    // client hasn't looked at yet and "buffered" is stuff
                    // that the intercept function has yet to process. pData
                    // is somewhere inside "buffered", pointing to our new
                    // "length" of processed data.  The intercept function
                    // will have moved pDataIntercept to somewhere beyond the
                    // end of "length".
                    //
                    // +--------+-------------+-------------------------------+
                    // |  read  |    unread   |            buffered           |
                    // +--------+-------------+----------+--------------------+
                    //       pRB->rI   pRB->l + readLength                pRB->lB
                    //                        |----------------X--------------|
                    //                        |----------|- length -|
                    //                                 pData
                    //                        |----------+--------------|
                    //                                             pDataIntercept

                    // First, move the processed stuff, "length" from pData onwards,
                    // down to join the end of the "unread" section.
                    memmove(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                            pReceiveBuffer->length + readLength,
                            pData, length);
                    U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pReceiveBuffer));

                    // We now have:
                    //
                    // +--------+-------------+-------------------------------+
                    // |  read  |    unread   |            buffered           |
                    // +--------+-------------+-------- ----------------------+
                    //       pRB->rI   pRB->l + readLength                pRB->lB
                    //                        |----------------X--------------|
                    //                        |-length-|
                    //                        |-------------------------|
                    //                                             pDataIntercept
                    //                                                  |- Y -|
                    //                                 |------ Z -------|

                    // Now we want to move the remaining unprocessed stuff,
                    // from pDataIntercept up to lengthBuffered, down to join
                    // the end of "length".
                    // y is how much stuff there is to move.
                    if (pDataIntercept > (U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                          pReceiveBuffer->lengthBuffered)) {
                        // This should never occur, but if it did
                        // it would not be good so best be safe.
                        // No print here as it would likely overload
                        // things as we're in a loop
                        pDataIntercept = U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                         pReceiveBuffer->lengthBuffered;
                    }
                    y = (U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                         pReceiveBuffer->lengthBuffered) - pDataIntercept;
                    LOG_BUFFER_FILL(9);
                    // Move it
                    memmove(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                            pReceiveBuffer->length + readLength + length,
                            pDataIntercept, y);
                    U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pReceiveBuffer));
                    // Lastly, we need to adjust the things that were at or
                    // beyond pDataIntercept to take account of the move.
                    // z is how far things were moved
                    z = pDataIntercept -
                        ((U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                          pReceiveBuffer->length + readLength + length));
                    LOG_BUFFER_FILL(10);
                    // Adjust pDataIntercept down by z.
                    pDataIntercept -= z;
                    // lengthBuffered is reduced by z
                    pReceiveBuffer->lengthBuffered -= z;
                    // Add the length as determined by the
                    // intercept function to readLength
                    readLength += (int32_t) length;
                    // x, the length left to be processed by the intercept
                    // function, becomes y, as does "length" for the next
                    // run around the loop
                    x = y;
                    length = y;
                    LOG_BUFFER_FILL(11);
                } else {
                    LOG_BUFFER_FILL(12);
                    // The intercept function needs more data,
                    // put length back to where it was and ask for more
                    length = x;
                }
                LOG_BUFFER_FILL(13);
            } while (pData != NULL);
        }

        LOG_BUFFER_FILL(14);
        uPortTaskBlock(U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS);
    } while ((readLength == 0) &&
             (pollTimeRemaining(atTimeoutMs, pClient->lockTimeMs) > 0));

    LOG_BUFFER_FILL(15);
    if (readLength > 0) {
#if U_CFG_OS_CLIB_LEAKS
        // If the C library leaks then don't print
        // in a callback as it will leak
        if (!eventIsCallback) {
#endif
            printAt(pClient, U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                    pReceiveBuffer->length + pReceiveBuffer->readIndex,
                    readLength);
#if U_CFG_OS_CLIB_LEAKS
        }
#endif
        pReceiveBuffer->length += readLength;
        LOG_BUFFER_FILL(16);
    }

    U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pReceiveBuffer));

    return readLength > 0;
}

// Get a character from the receive buffer.
// Resets and re-fills the buffer if everything has been read,
// i.e. the receive position is equal to the received length.
// Returns the next character or -1 on failure and also
// sets the error flag.
static int32_t bufferReadChar(uAtClientInstance_t *pClient)
{
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    int32_t character = -1;

    // Note that we need to distinguish two cases here:
    // returning -1, i.e. 0xFFFFFFFF, and returning the
    // character 0xFF, i.e. 0x000000FF.  While this may
    // seem clear, the sign-extension behaviour on various
    // platforms makes it more interesting, hence the casting
    // below

    if (pReceiveBuffer->readIndex < pReceiveBuffer->length) {
        // Read from the buffer
        character = (unsigned char) * (U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                       pReceiveBuffer->readIndex);
        pReceiveBuffer->readIndex++;
    } else {
        // Everything has been read, try to bring more in
        bufferReset(pClient, false);
        if (bufferFill(pClient, true)) {
            // Read something, all good
            character = (unsigned char) * (U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                           pReceiveBuffer->readIndex);
            pReceiveBuffer->readIndex++;
            pClient->numConsecutiveAtTimeouts = 0;
        } else {
            // Timeout
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: timeout.\n",
                         pClient->streamType, pClient->streamHandle);
            }
            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            consecutiveTimeout(pClient);
        }
    }

    return character;
}

// Look for pString at the start of the current receive buffer
// without bringing more data into it, and if the string
// is there consume it.
static bool bufferMatch(const uAtClientInstance_t *pClient,
                        const char *pString, size_t length)
{
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    bool found = false;

    bufferRewind(pClient);

    if ((pReceiveBuffer->length - pReceiveBuffer->readIndex) >= length) {
        if (pString && (memcmp(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                               pReceiveBuffer->readIndex,
                               pString, length) == 0)) {
            // Consume the matching part
            pReceiveBuffer->readIndex += length;
            found = true;
        }
    }

    return found;
}

// Check if the current byte in the buffer matches
// character and, if so, consume it.
static bool consumeOneCharacter(uAtClientInstance_t *pClient,
                                char character, bool destructive)
{
    int32_t readCharacter = bufferReadChar(pClient);

    if ((readCharacter >= 0) && (((char) readCharacter) != character) &&
        !destructive) {
        // If we read something and it was not the wanted
        // character then, if we're not being destructive,
        // decrement the buffer index to "put it back"
        pClient->pReceiveBuffer->readIndex--;
    }

    return ((char) readCharacter) == character;
}

// Set scope.
static void setScope(uAtClientInstance_t *pClient,
                     uAtClientScope_t scope)
{
    uAtClientTag_t *pStopTag = &(pClient->stopTag);

    if (pClient->scope != scope) {
        pClient->scope = scope;
        pStopTag->found = false;
        switch (scope) {
            case U_AT_CLIENT_SCOPE_RESPONSE:
                pStopTag->pTagDef = &gResponseStopTag;
                break;
            case U_AT_CLIENT_SCOPE_INFORMATION:
                // Consume the space that should follow the
                // information response prefix, if it is
                // there
                consumeOneCharacter(pClient, ' ', false);
                pStopTag->pTagDef = &gInformationStopTag;
                break;
            case U_AT_CLIENT_SCOPE_NONE:
                pStopTag->pTagDef = &gNoStopTag;
                break;
            default:
                //lint -e506 Suppress constant value Boolean
                U_ASSERT(false);
                break;
        }
    }
}

// Consume characters until pString is found.
static bool consumeToString(uAtClientInstance_t *pClient,
                            const char *pString)
{
    size_t index = 0;
    size_t length = strlen(pString);
    int32_t character = 0;

    while ((character >= 0) &&
           (index < length)) {
        character = bufferReadChar(pClient);
        if (character >= 0) {
            if (character == *(pString + index)) {
                index++;
            } else {
                index = 0;
                if (character == *pString) {
                    index++;
                }
            }
        }
    }

    return index == length;
}

// Consume characters until the stop tag is found.
static bool consumeToStopTag(uAtClientInstance_t *pClient)
{
    bool found = true;

    if (!pClient->stopTag.found &&
        (pClient->error == U_ERROR_COMMON_SUCCESS)) {
        if (pClient->stopTag.pTagDef == &gNoStopTag) {
            // If there is no stop tag, consume everything
            // in the buffer
            bufferReset(pClient, false);
        } else {
            // Otherwise consume up to the stop tag
            found = consumeToString(pClient, pClient->stopTag.pTagDef->pString);
            if (!found) {
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                if (pClient->debugOn) {
                    uPortLog("U_AT_CLIENT_%d-%d: stop tag not found.\n",
                             pClient->streamType, pClient->streamHandle);
                }
            }
        }
    }

    return found;
}

// Consume up to the information response stop tag,
// i.e. CR/LF.  Set scope to response.
static void informationResponseStop(uAtClientInstance_t *pClient)
{
    if (consumeToStopTag(pClient)) {
        if (pClient->stopTag.pTagDef != &gNoStopTag) {
            // If we're not ignoring stop tags, set the
            // scope to response
            setScope(pClient, U_AT_CLIENT_SCOPE_RESPONSE);
        }
    }
}

// Iterate through URCs and check if one of them matches the current
// contents of the receive buffer. If a URC is matched, set the
// scope to information response and, after the URC's handler has
// returned, finish off the information response scope by consuming
// up to CR/LF.
static bool bufferMatchOneUrc(uAtClientInstance_t *pClient)
{
    size_t prefixLength = 0;
    bool found = false;
    int32_t now;
    uErrorCode_t savedError;

    bufferRewind(pClient);

    for (uAtClientUrc_t *pUrc = pClient->pUrcList;
         !found && (pUrc != NULL);
         pUrc = pUrc->pNext) {
        prefixLength = pUrc->prefixLength;
        if (pClient->pReceiveBuffer->length >= prefixLength) {
            if (bufferMatch(pClient, pUrc->pPrefix, prefixLength)) {
                setScope(pClient, U_AT_CLIENT_SCOPE_INFORMATION);
                now = uPortGetTickTimeMs();
                // Before heading off into URCness, save
                // the current error state and reset
                // it so that the URC doesn't suffer the error
                savedError = pClient->error;
                pClient->error = U_ERROR_COMMON_SUCCESS;
                if (processAsync(pClient->magicNumber) && pUrc->pHandler) {
                    pUrc->pHandler(pClient, pUrc->pHandlerParam);
                }
                informationResponseStop(pClient);
                // Put the error state back again
                pClient->error = savedError;
                // Put the error state back again
                // Add the amount of time spent in the URC
                // world to the start time
                pClient->lockTimeMs += uPortGetTickTimeMs() - now;
                found = true;
            }
        }
    }

    return found;
}

// Read a string parameter.
// The mutex should be locked before this is called.
static int32_t readString(uAtClientInstance_t *pClient,
                          char *pString,
                          size_t lengthBytes,
                          bool ignoreStopTag)
{
    uAtClientTag_t *pStopTag = &(pClient->stopTag);
    int32_t lengthRead = 0;
    int32_t matchPos = 0;
    bool delimiterFound = false;
    bool inQuotes = false;
    int32_t c;

    while (((lengthBytes == 0) || (lengthRead < ((int32_t) lengthBytes - 1) + matchPos)) &&
           (pClient->error == U_ERROR_COMMON_SUCCESS) &&
           !delimiterFound &&
           (ignoreStopTag || !pStopTag->found)) {
        c = bufferReadChar(pClient);
        if (c == -1) {
            // Error
            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
        } else if (!inQuotes && (c == pClient->delimiter)) {
            // Reached delimiter
            delimiterFound = true;
        } else if (c == '\"') {
            // Switch into or out of quotes
            matchPos = 0;
            inQuotes = !inQuotes;
        } else {
            if (!inQuotes && !ignoreStopTag &&
                (pStopTag->pTagDef->length > 0)) {
                // It could be a stop tag
                if (c == *(pStopTag->pTagDef->pString + matchPos)) {
                    matchPos++;
                } else {
                    // If it wasn't a stop tag, reset
                    // the match position and check again
                    // in case it is the start of a new stop tag
                    matchPos = 0;
                    if (c == *(pStopTag->pTagDef->pString)) {
                        matchPos++;
                    }
                }
                if (matchPos == (int32_t) pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                    // Remove tag from string if it was matched
                    lengthRead -= (int32_t) pStopTag->pTagDef->length - 1;
                }
            } else {
                // Not anything
                matchPos = 0;
            }
            if (!pStopTag->found) {
                if (pString != NULL) {
                    // Add the character to the string
                    *(pString + lengthRead) = (char) c;
                }
                lengthRead++;
            }
        }
    }

    if ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
        (lengthBytes > 0) && (pString != NULL)) {
        // Add the terminator
        *(pString + lengthRead) = '\0';
    }

    // Clear up any rubbish by consuming
    // to delimiter or stop tag
    if (!delimiterFound) {
        c = -1;
        while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
               (c != pClient->delimiter) &&
               !pStopTag->found) {
            c = bufferReadChar(pClient);
            if (c == -1) {
                setError(pClient,
                         U_ERROR_COMMON_DEVICE_ERROR);
            } else if (pStopTag->pTagDef->length > 0) {
                // It could be a stop tag
                if (c == *(pStopTag->pTagDef->pString + matchPos)) {
                    matchPos++;
                } else {
                    // If it wasn't a stop tag, reset
                    // the match position and check again
                    // in case it is the start of a new stop tag
                    matchPos = 0;
                    if (c == *(pStopTag->pTagDef->pString)) {
                        matchPos++;
                    }
                }
                if (matchPos == (int32_t) pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                }
            }
        }
    }

    if (pClient->error != U_ERROR_COMMON_SUCCESS) {
        lengthRead = -1;
    }

    return lengthRead;
}

// Read an integer.
// The mutex should be locked before this is called.
static int32_t readInt(uAtClientInstance_t *pClient)
{
    char buffer[32]; // Enough for an integer
    int32_t integerRead = -1;

    if ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
        !pClient->stopTag.found &&
        (readString(pClient, buffer,
                    sizeof(buffer), false) > 0)) {
        integerRead = strtol(buffer, NULL, 10);
    }

    return integerRead;
}

// Record an error sent from the AT server, i.e. ERROR
// or CMS ERROR or CME ERROR.
static void setDeviceError(uAtClientInstance_t *pClient,
                           uAtClientDeviceErrorType_t errorType)
{
    int32_t errorCode;

    pClient->deviceError.type = errorType;
    pClient->deviceError.code = 0;

    if ((errorType == U_AT_CLIENT_DEVICE_ERROR_TYPE_CMS) ||
        (errorType == U_AT_CLIENT_DEVICE_ERROR_TYPE_CME)) {

        setScope(pClient, U_AT_CLIENT_SCOPE_INFORMATION);
        errorCode = readInt(pClient);

        if (errorCode >= 0) {
            pClient->deviceError.code = errorCode;
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: CME/CMS error code %d.\n",
                         pClient->streamType, pClient->streamHandle,
                         errorCode);
            }
        }
    }

    setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
}

// Look for a device error string in the receive buffer
// and deal with it.
static bool deviceErrorInBuffer(uAtClientInstance_t *pClient)
{
    bool found;

    found = bufferMatch(pClient, U_AT_CLIENT_CME_ERROR,
                        U_AT_CLIENT_CME_ERROR_LENGTH_BYTES);
    if (found) {
        setDeviceError(pClient, U_AT_CLIENT_DEVICE_ERROR_TYPE_CME);
    } else {
        found = bufferMatch(pClient, U_AT_CLIENT_CMS_ERROR,
                            U_AT_CLIENT_CMS_ERROR_LENGTH_BYTES);
        if (found) {
            setDeviceError(pClient, U_AT_CLIENT_DEVICE_ERROR_TYPE_CMS);
        } else {
            found = bufferMatch(pClient, U_AT_CLIENT_ERROR,
                                U_AT_CLIENT_ERROR_LENGTH_BYTES);
            if (found) {
                setDeviceError(pClient,
                               U_AT_CLIENT_DEVICE_ERROR_TYPE_ERROR);
            } else {
                found = bufferMatch(pClient, U_AT_CLIENT_ABORTED,
                                    U_AT_CLIENT_ABORTED_LENGTH_BYTES);
                if (found) {
                    setDeviceError(pClient,
                                   U_AT_CLIENT_DEVICE_ERROR_TYPE_ABORTED);
                }
            }
        }
    }

    return found;
}

// Process an AT response by checking if the receive
// buffer contains the given prefix, a URC or OK/(CMS)(CME)ERROR,
// returning true if the prefix was matched.
static bool processResponse(uAtClientInstance_t *pClient,
                            const char *pPrefix, bool checkUrc)
{
    bool processingDone = false;
    bool prefixMatched = false;
    const char *pTmp;

    while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
           (!pClient->stopTag.found) &&
           !processingDone) {
        // Remove any CR/LF's at the start
        while (bufferMatch(pClient, U_AT_CLIENT_CRLF,
                           U_AT_CLIENT_CRLF_LENGTH_BYTES)) {}
        // Check for the end of the response, i.e. "OK"
        if (bufferMatch(pClient, gResponseStopTag.pString,
                        gResponseStopTag.length)) {
            setScope(pClient, U_AT_CLIENT_SCOPE_RESPONSE);
            pClient->stopTag.found = true;
        } else {
            // The response has not ended, check for an error
            if (!deviceErrorInBuffer(pClient)) {
                // No error, check for the prefix
                if ((pPrefix != NULL) && bufferMatch(pClient, pPrefix,
                                                     strlen(pPrefix))) {
                    prefixMatched = true;
                    processingDone = true;
                } else {
                    // No prefix match, check for a URC
                    if (checkUrc && bufferMatchOneUrc(pClient)) {
                        // Just loop again
                    } else {
                        // If no matches were found, see if there's
                        // a CR/LF in the buffer with some characters
                        // between it and where we are now to read
                        pTmp = pMemStr(U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer) +
                                       pClient->pReceiveBuffer->readIndex,
                                       pClient->pReceiveBuffer->length,
                                       U_AT_CLIENT_CRLF, U_AT_CLIENT_CRLF_LENGTH_BYTES);
                        if ((pTmp != NULL) &&
                            (pTmp - U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer)) > 0) {
                            // There is a CR/LF after some stuff
                            // to read and there was no prefix,
                            // so return now so that the caller
                            // can read the stuff
                            if (pPrefix == NULL) {
                                prefixMatched = true;
                                processingDone = true;
                            } else {
                                // Just consume up to CR/LF
                                consumeToString(pClient, U_AT_CLIENT_CRLF);
                            }
                        } else {
                            // We might still bufferMatch something,
                            // try to fill the buffer with more stuff
                            if (!bufferFill(pClient, true)) {
                                // If we don't get any data within
                                // the timeout, set an error to
                                // indicate the need for recovery
                                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                                consecutiveTimeout(pClient);
                            } else {
                                pClient->numConsecutiveAtTimeouts = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    return prefixMatched;
}

// Write data to the stream.
//
// Design note concerning the wake-up handler
// process below; first the needs:
// - the wake-up handler must be allowed to call back
//   into this AT interface, one level of recursion.
// - the wake-up handler must be allowed to launch
//   asynchronous callbacks that may also call into
//   this AT interface.
// - these asynchronous callbacks must be blocked
//   from doing AT things while the wake-up process
//   is occurring and then be allowed to continue
//   once the wake-up has been completed.
// Given those needs, the design here is: when wake-up
// is required inWakeUpHandlerMutex is locked and
// the current task ID saved before the wake-up function
// is called. The U_AT_CLIENT_LOCK_CLIENT_MUTEX that
// gates every AT client API call checks the current
// task ID against this saved task ID and, if it
// matches, it blocks against a separate wake-up mutex
// rather than the normal mutex.  If the task ID does
// not match then it _also_ blocks on inWakeUpHandlerMutex
// before proceeding, hence holding off processing until
// the wake-up process has completed.
static size_t write(uAtClientInstance_t *pClient,
                    const char *pData, size_t length,
                    bool andFlush)
{
    int32_t thisLengthWritten = 0;
    size_t lengthToWrite;
    const char *pDataStart = pData;
    const char *pDataToWrite = pData;
    int32_t savedLockTimeMs;
    int32_t wakeUpDurationMs = 0;
    uAtClientScope_t savedScope;
    uAtClientTag_t savedStopTag;
    bool savedDelimiterRequired;
    uAtClientDeviceError_t savedDeviceError;

    while (((pData < pDataStart + length) || andFlush) &&
           (pClient->error == U_ERROR_COMMON_SUCCESS)) {
        lengthToWrite = length - (pData - pDataStart);
        if ((pClient->pWakeUp != NULL) && (pClient->lastTxTimeMs >= 0) &&
            (uPortGetTickTimeMs() - pClient->lastTxTimeMs > pClient->pWakeUp->inactivityTimeoutMs) &&
            (uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) == 0)) {
            // We have a wake-up handler, the inactivity timeout
            // has expired and we've managed to lock the wake-up
            // handler mutex (if we aren't able to lock the wake-up
            // handler mutex  then we must already be in the wake-up
            // handler, having recursed, so can just continue); now
            // we need to call the wake-up handler function.
            // Set wakeUpTask to the current task handle so
            // that any future calls can be locked against the
            // separate pWakeUp->mutex if they come from the task
            // we're in at the moment, the one dealing with the wake-up
            uPortTaskGetHandle(&(pClient->pWakeUp->wakeUpTask));
            // The pClient->mutex will have been locked on the way
            // into here by U_AT_CLIENT_LOCK_CLIENT_MUTEX.
            // Remember the lock time and measure how long
            // waking-up takes in order to correct for it
            savedLockTimeMs = pClient->lockTimeMs;
            wakeUpDurationMs = uPortGetTickTimeMs();
            // Remember the dynamic things that the
            // wake-up handler might overwrite
            savedScope = pClient->scope;
            savedStopTag = pClient->stopTag;
            savedDelimiterRequired = pClient->delimiterRequired;
            savedDeviceError = pClient->deviceError;
            // Reset the scope, stopTag and delimiterRequired
            pClient->scope = U_AT_CLIENT_SCOPE_NONE;
            pClient->stopTag.pTagDef = &gNoStopTag;
            pClient->stopTag.found = false;
            pClient->delimiterRequired = false;
            // Now actually call the wake-up callback which may recurse
            // back into here
            if (pClient->pWakeUp->pHandler((uAtClientHandle_t) pClient,
                                           pClient->pWakeUp->pParam) != 0) {
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            }
            // At this point all of the calls back into here
            // performed as part of the wake-up process will have
            // been completed; there may have been calls from other
            // tasks but they will have been blocked on the normal
            // mutex before reaching here.
            // We can now set the wakeUpTask back to NULL and all
            // blocking will be on the normal mutex again
            pClient->pWakeUp->wakeUpTask = NULL;
            // Put all the saved things back
            pClient->scope = savedScope;
            pClient->stopTag = savedStopTag;
            pClient->delimiterRequired = savedDelimiterRequired;
            pClient->deviceError = savedDeviceError;
            // Set the adjusted lock time, allowing for potential
            // wrap in uPortGetTickTimeMs()
            wakeUpDurationMs = uPortGetTickTimeMs() - wakeUpDurationMs;
            if (wakeUpDurationMs > 0) {
                pClient->lockTimeMs = savedLockTimeMs + wakeUpDurationMs;
            } else {
                pClient->lockTimeMs = uPortGetTickTimeMs();
            }
            // We are no longer in the wake-up handler
            uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);
        }

        if (pClient->error == U_ERROR_COMMON_SUCCESS) {
            if (pClient->pInterceptTx != NULL) {
                if (pData < pDataStart + length) {
                    // Call the intercept function
                    pDataToWrite = pClient->pInterceptTx((uAtClientHandle_t) pClient,
                                                         &pData, &lengthToWrite,
                                                         pClient->pInterceptTxContext);
                } else {
                    // andFlush must be true: call the intercept
                    // function again with NULL to flush it out
                    pDataToWrite = pClient->pInterceptTx((uAtClientHandle_t) pClient,
                                                         NULL, &lengthToWrite,
                                                         pClient->pInterceptTxContext);
                    andFlush = false;
                }
            } else {
                // If there is no intercept function then move pData
                // on, plus clear andFlush, to indicate that we're done
                pData = pDataStart + length;
                andFlush = false;
            }
            if ((pDataToWrite == NULL) && (lengthToWrite > 0)) {
                setError(pClient, U_ERROR_COMMON_UNKNOWN);
            }
            while ((lengthToWrite > 0) &&
                   (pDataToWrite != NULL) &&
                   (pClient->error == U_ERROR_COMMON_SUCCESS)) {
                // Send the data
                switch (pClient->streamType) {
                    case U_AT_CLIENT_STREAM_TYPE_UART:
                        thisLengthWritten = uPortUartWrite(pClient->streamHandle,
                                                           pDataToWrite, lengthToWrite);
                        break;
                    // Write handled in intercept
                    case U_AT_CLIENT_STREAM_TYPE_EDM:
                        break;
                    default:
                        break;
                }
                if (thisLengthWritten > 0) {
                    pDataToWrite += thisLengthWritten;
                    lengthToWrite -= thisLengthWritten;
                    pClient->lastTxTimeMs = uPortGetTickTimeMs();
                } else {
                    setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                }
            }
        }
    }

    // If there is an intercept function it may be that
    // the length written is longer or shorter than
    // passed in so it is not easily possible to printAt()
    // exactly what was written, we can only check
    // if *everything* was written
    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        printAt(pClient, pDataStart, length);
    } else {
        length = 0;
    }

    return length;
}

// Do common checks before sending parameters
// and also deal with the need for a delimiter.
static bool writeCheckAndDelimit(uAtClientInstance_t *pClient)
{
    bool isOk = false;

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // No errors, that's good
        if (pClient->delimiterRequired) {
            // Write a delimiter
            if (write(pClient, &(pClient->delimiter), 1, false) == 1) {
                isOk = true;
            }
        } else {
            // A delimiter wasn't required because
            // we were at the start of an AT command
            // but it will be in future
            pClient->delimiterRequired = true;
            isOk = true;
        }
    }

    return isOk;
}

// Check if a URC handler is already in the list.
static bool findUrcHandler(const uAtClientInstance_t *pClient,
                           const char *pPrefix)
{
    uAtClientUrc_t *pUrc = pClient->pUrcList;
    bool found = false;

    while ((pUrc != NULL) && !found) {
        if (strcmp(pPrefix, pUrc->pPrefix) == 0) {
            found = true;
        }
        pUrc = pUrc->pNext;
    }

    return found;
}

// Try to lock the stream: this does NOT clear errors.
// Returns the stream mutex that was locked or NULL.
static uPortMutexHandle_t tryLock(uAtClientInstance_t *pClient)
{
    uPortMutexHandle_t streamMutex;

    streamMutex = streamTryLock(pClient, 0);
    if (streamMutex != NULL) {
        pClient->lockTimeMs = uPortGetTickTimeMs();
        if (pClient->pActivityPin != NULL) {
            // If an activity pin is set then switch it on
            while (uPortGetTickTimeMs() - pClient->pActivityPin->lastToggleTime <
                   pClient->pActivityPin->hysteresisMs) {
                uPortTaskBlock(U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS);
            }
            if (uPortGpioSet(pClient->pActivityPin->pin,
                             (int32_t) pClient->pActivityPin->highIsOn) == 0) {
                pClient->pActivityPin->lastToggleTime = uPortGetTickTimeMs();
                uPortTaskBlock(pClient->pActivityPin->readyMs);
            }
        }
    }

    return streamMutex;
}

// Unlock the stream without kicking off
// any further data reception.  This is used
// directly in taskUrc to avoid recursion.
static void unlockNoDataCheck(uAtClientInstance_t *pClient,
                              uPortMutexHandle_t streamMutex)
{
    if ((pClient->pWakeUp != NULL) &&
        ((uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) != 0) ||
         // This just to unlock the mutex if the try actually succeeded
         (uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex) != 0))) {
        // If we're in a wake-up handler then restore any
        // saved timeout value from there, if there is one,
        // but don't do any unlocking as that will happen
        // when we unwind out of the wake-up handler.
        if (pClient->pWakeUp->atTimeoutSavedMs >= 0) {
            pClient->atTimeoutMs = pClient->pWakeUp->atTimeoutSavedMs;
            pClient->pWakeUp->atTimeoutSavedMs = -1;
        }
    } else {
        // Not in a wake-up handler so just restore the
        // usual saved timeout if there was one
        if (pClient->atTimeoutSavedMs >= 0) {
            pClient->atTimeoutMs = pClient->atTimeoutSavedMs;
            pClient->atTimeoutSavedMs = -1;
        }

        if (pClient->pActivityPin != NULL) {
            // If an activity pin is set then switch it off
            while (uPortGetTickTimeMs() - pClient->pActivityPin->lastToggleTime <
                   pClient->pActivityPin->hysteresisMs) {
                uPortTaskBlock(U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS);
            }
            if (uPortGpioSet(pClient->pActivityPin->pin,
                             (int32_t) !pClient->pActivityPin->highIsOn) == 0) {
                pClient->pActivityPin->lastToggleTime = uPortGetTickTimeMs();
            }
        }
    }

    // Now unlock the stream
    uPortMutexUnlock(streamMutex);
}

// Convert a string which should contain
// something like "7587387289371387" (and
// be null-terminated) into a uint64_t
// Any leading crap is ignored and conversion
// stops when a non-numeric character is reached.
static uint64_t stringToUint64(const char *pBuffer)
{
    uint64_t uint64 = 0;

    while (*pBuffer >= '0' && *pBuffer <= '9') {
        uint64 = (uint64 * 10) + (*pBuffer++ - '0');
    }

    return uint64;
}

// Convert a uint64_t into a string,
// returning the length of string that
// would be required even if bufLen were
// too small (i.e. just like snprintf() would).
static int32_t uint64ToString(char *pBuffer, size_t length,
                              uint64_t uint64)
{
    int32_t sizeOrError = -1;
    uint64_t x;

    // Max value of a uint64_t is
    // 18,446,744,073,709,551,616,
    // so maximum divisor is
    // 10,000,000,00,000,000,000.
    uint64_t divisor = 10000000000000000000ULL;

    if (length > 0) {
        sizeOrError = 0;
        // Cut the divisor down to size
        while (uint64 < divisor) {
            divisor /= 10;
        }
        if (divisor == 0) {
            divisor = 1;
        }

        // Reduce length by 1 to allow for the terminator
        length--;
        // Now write the numerals
        while (divisor > 0) {
            x = uint64 / divisor;
            if (length > 0) {
                *pBuffer = (char) (x + '0');
            }
            uint64 -= x * divisor;
            sizeOrError++;
            length--;
            pBuffer++;
            divisor /= 10;
        }
        // Add the terminator
        *pBuffer = '\0';
    }

    return sizeOrError;
}

// Get the amount of stuff in the receive buffer for the URC
// (and so check processAsync() also).
static int32_t getReceiveSizeForUrc(const uAtClientInstance_t *pClient)
{
    int32_t receiveSize = 0;

    if (processAsync(pClient->magicNumber)) {
        switch (pClient->streamType) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                receiveSize = uPortUartGetReceiveSize(pClient->streamHandle);
                break;
            case U_AT_CLIENT_STREAM_TYPE_EDM:
                receiveSize = uShortRangeEdmStreamAtGetReceiveSize(pClient->streamHandle);
                break;
            default:
                break;
        }
    }

    return receiveSize;
}

// Callback to find URC's from AT responses, triggered through
// something being received from the AT server.
static void urcCallback(int32_t streamHandle, uint32_t eventBitmask,
                        void *pParameters)
{
    uAtClientInstance_t *pClient;
    uAtClientReceiveBuffer_t *pReceiveBuffer;
    uPortMutexHandle_t streamMutex;
    int32_t sizeOrError;

    pClient = (uAtClientInstance_t *) pParameters;

    if ((pClient != NULL) &&
        (pClient->streamHandle == streamHandle) &&
        (eventBitmask & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) &&
        (uPortMutexTryLock(pClient->urcPermittedMutex, 0) == 0)) {

        // Potential URC data is available.  However,
        // the main thread may already have taken the lock
        // and be processing it, in which case just return.
        streamMutex = tryLock(pClient);
        if (streamMutex != NULL) {
            // Loop until no received characters left to process
            pReceiveBuffer = pClient->pReceiveBuffer;
            while (((sizeOrError = getReceiveSizeForUrc(pClient)) > 0) ||
                   (pReceiveBuffer->readIndex < pReceiveBuffer->length)) {
#if !U_CFG_OS_CLIB_LEAKS
                // Don't do this if CLIB is leaky on this platform since it's
                // the printf() that leaks
                if (pClient->debugOn) {
                    uPortLog("U_AT_CLIENT_%d-%d: possible URC data readable %d,"
                             " already buffered %u.\n", pClient->streamType,
                             pClient->streamHandle, sizeOrError,
                             pReceiveBuffer->length - pReceiveBuffer->readIndex);
                }
#endif
                pClient->scope = U_AT_CLIENT_SCOPE_NONE;
                for (size_t x = 0; x < U_AT_CLIENT_URC_DATA_LOOP_GUARD; x++) {
                    // Search through the URCs
                    if (bufferMatchOneUrc(pClient)) {
                        // If there's a bufferMatch, see if more data is available
                        sizeOrError = getReceiveSizeForUrc(pClient);
                        if ((sizeOrError <= 0) &&
                            (pReceiveBuffer->readIndex >=
                             pReceiveBuffer->dataBufferSize)) {
                            // We have no more data to process, leave this loop
                            break;
                        }
                        // If no bufferMatch was found, look for CR/LF
                    } else if (pMemStr(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                       pReceiveBuffer->readIndex,
                                       pReceiveBuffer->length,
                                       U_AT_CLIENT_CRLF, U_AT_CLIENT_CRLF_LENGTH_BYTES) != NULL) {
                        // Consume everything up to the CR/LF
                        consumeToString(pClient, U_AT_CLIENT_CRLF);
                    } else {
                        // If no bufferMatch was found and there's no CR/LF to
                        // consume up to, bring in more data and we'll check
                        // it again
                        if (processAsync(pClient->magicNumber) && bufferFill(pClient, true)) {
                            // Start the cycle again as if we'd just done
                            // uAtClientLock()
                            pClient->lockTimeMs = uPortGetTickTimeMs();
                        } else {
                            // There is no more data: clear anything that
                            // could not be handled and leave this loop
                            bufferReset(pClient, false);
                            break;
                        }
                    }
                }
#if !U_CFG_OS_CLIB_LEAKS
                // Don't do this if CLIB is leaky on this platform since it's
                // the printf() that leaks
                if (pClient->debugOn) {
                    uPortLog("U_AT_CLIENT_%d-%d: URC checking done.\n",
                             pClient->streamType, pClient->streamHandle);
                }
#endif
            }

            // Just unlock the stream without
            // checking for more data, which would try
            // to queue stuff on this task and I'm not
            // sure that's safe
            unlockNoDataCheck(pClient, streamMutex);
        }

        uPortMutexUnlock(pClient->urcPermittedMutex);
    }
}

// Callback for the event queue.
static void eventQueueCallback(void *pParameters, size_t paramLength)
{
    uAtClientCallback_t *pCb = (uAtClientCallback_t *) pParameters;

    (void) paramLength;

    if ((pCb != NULL) && (pCb->pFunction != NULL) &&
        processAsync(pCb->atClientMagicNumber)) {
        pCb->pFunction(pCb->atHandle, pCb->pParam);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: DETAILED DEBUG ONLY
 * These functions are for detailed debug only, purely for internal
 * development purposes and therefore not exposed in the header file.
 * -------------------------------------------------------------- */

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
// Switch detailed debug on.
void uAtClientDetailedDebugOn()
{
    gDebugOn = true;
}

// Switch detailed debug off.
void uAtClientDetailedDebugOff()
{
    gDebugOn = false;
}

// Print the detailed debug (done anyway on AT client deinit).
void uAtClientDetailedDebugPrint()
{
    // Print out the detailed debug log.
    printLogDebug(gDebug, gDebugIndex);
    gDebugIndex = 0;
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INITIALISATION AND CONFIGURATION
 * -------------------------------------------------------------- */

// Initialise the AT client infrastructure.
int32_t uAtClientInit()
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        // Create an event queue for callbacks
        errorCodeOrHandle = uPortEventQueueOpen(eventQueueCallback,
                                                "atCallbacks",
                                                sizeof(uAtClientCallback_t),
                                                U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES,
                                                U_AT_CLIENT_CALLBACK_TASK_PRIORITY,
                                                U_AT_CLIENT_CALLBACK_QUEUE_LENGTH);
        if (errorCodeOrHandle >= 0) {
            gEventQueueHandle = errorCodeOrHandle;
            // Create the mutex that protects gEventQueueHandle
            errorCodeOrHandle = uPortMutexCreate(&gMutexEventQueue);
            if (errorCodeOrHandle == 0) {
                // Create the mutex that protects the linked list
                errorCodeOrHandle = uPortMutexCreate(&gMutex);
                if (errorCodeOrHandle != 0) {
                    // Failed, release the callbacks event queue again
                    // and its mutex
                    uPortEventQueueClose(gEventQueueHandle);
                    uPortMutexDelete(gMutexEventQueue);
                }
            } else {
                // Failed, release the callbacks event queue again
                uPortEventQueueClose(gEventQueueHandle);
            }
        }
    }

    return errorCodeOrHandle;
}

// Deinitialise all AT clients and the infrastructure.
void uAtClientDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Remove all the AT handlers
        while (gpAtClientList != NULL) {
            removeClient(gpAtClientList);
        }

        U_PORT_MUTEX_LOCK(gMutexEventQueue);
        // Release the callbacks event queue
        uPortEventQueueClose(gEventQueueHandle);

        // Delete the mutexes
        U_PORT_MUTEX_UNLOCK(gMutexEventQueue);
        uPortMutexDelete(gMutexEventQueue);
        gMutexEventQueue = NULL;
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;

#ifdef U_CFG_AT_CLIENT_DETAILED_DEBUG
        // Print out the detailed debug log.
        printLogDebug(gDebug, gDebugIndex);
        gDebugIndex = 0;
#endif
    }
}

// Add an AT client.
uAtClientHandle_t uAtClientAdd(int32_t streamHandle,
                               uAtClientStream_t streamType,
                               void *pReceiveBuffer,
                               size_t receiveBufferSize)
{
    uAtClientInstance_t *pClient = NULL;
    bool receiveBufferIsMalloced = false;
    int32_t errorCode = -1;

    U_PORT_MUTEX_LOCK(gMutex);

    // Check parameters
    if ((receiveBufferSize > U_AT_CLIENT_BUFFER_OVERHEAD_BYTES) &&
        (streamType < U_AT_CLIENT_STREAM_TYPE_MAX)) {
        // See if there's already an AT client for this stream and
        // also check that we have room for another entry in the
        // magic number array
        pClient = pGetAtClientInstance(streamHandle, streamType);
        if ((pClient == NULL) &&
            (numAtClients() < sizeof(gAtClientMagicNumberProcessAsync) /
             sizeof(gAtClientMagicNumberProcessAsync[0]))) {
            // Nope, create one
            pClient = (uAtClientInstance_t *) pUPortMalloc(sizeof(uAtClientInstance_t));
            if (pClient != NULL) {
                memset(pClient, 0, sizeof(*pClient));
                pClient->pReceiveBuffer = (uAtClientReceiveBuffer_t *) pReceiveBuffer;
                // Make sure we have a receive buffer
                if (pClient->pReceiveBuffer == NULL) {
                    receiveBufferIsMalloced = true;
                    pClient->pReceiveBuffer = (uAtClientReceiveBuffer_t *) pUPortMalloc(receiveBufferSize);
                }
                if (pClient->pReceiveBuffer != NULL) {
                    pClient->pReceiveBuffer->isMalloced = (int32_t) receiveBufferIsMalloced;
                    // Create the mutexes
                    if ((uPortMutexCreate(&(pClient->mutex)) == 0) &&
                        (uPortMutexCreate(&(pClient->streamMutex)) == 0) &&
                        (uPortMutexCreate(&(pClient->urcPermittedMutex)) == 0)) {
                        // Set all the non-zero initial values before we set
                        // the event handlers which might call us
                        pClient->streamHandle = streamHandle;
                        pClient->streamType = streamType;
                        pClient->atTimeoutMs = U_AT_CLIENT_DEFAULT_TIMEOUT_MS;
                        pClient->atTimeoutSavedMs = -1;
                        pClient->delimiter = U_AT_CLIENT_DEFAULT_DELIMITER;
                        mutexStackInit(&(pClient->lockedStreamMutexStack));
                        pClient->delayMs = U_AT_CLIENT_DEFAULT_DELAY_MS;
                        clearError(pClient);
                        // This will also set stopTag
                        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
                        pClient->lastTxTimeMs = -1;
                        pClient->urcMaxStringLength = U_AT_CLIENT_INITIAL_URC_LENGTH;
                        pClient->maxRespLength = U_AT_CLIENT_MAX_LENGTH_INFORMATION_RESPONSE_PREFIX;
                        // Set up the buffer and its protection markers
                        pClient->pReceiveBuffer->dataBufferSize = receiveBufferSize -
                                                                  U_AT_CLIENT_BUFFER_OVERHEAD_BYTES;
                        bufferReset(pClient, true);
                        memcpy(pClient->pReceiveBuffer->mk0, U_AT_CLIENT_MARKER,
                               U_AT_CLIENT_MARKER_SIZE);
                        memcpy(U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer) +
                               pClient->pReceiveBuffer->dataBufferSize,
                               U_AT_CLIENT_MARKER, U_AT_CLIENT_MARKER_SIZE);
                        // Now add an event handler for characters
                        // received on the stream
                        switch (streamType) {
                            case U_AT_CLIENT_STREAM_TYPE_UART:
                                errorCode = uPortUartEventCallbackSet(streamHandle,
                                                                      U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                                      urcCallback, pClient,
                                                                      U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                                                      U_AT_CLIENT_URC_TASK_PRIORITY);
                                break;
                            case U_AT_CLIENT_STREAM_TYPE_EDM:
                                errorCode = uShortRangeEdmStreamAtCallbackSet(streamHandle, urcCallback, pClient);
                                break;
                            default:
                                // streamType is checked on entry
                                break;
                        }
                        if (errorCode == 0) {
                            // Add the instance to the list
                            addAtClientInstance(pClient);
                        }
                    }
                }

                if (errorCode != 0) {
                    // Clean up on failure
                    if (pClient->urcPermittedMutex != NULL) {
                        uPortMutexDelete(pClient->urcPermittedMutex);
                    }
                    if (pClient->streamMutex != NULL) {
                        uPortMutexDelete(pClient->streamMutex);
                    }
                    if (pClient->mutex != NULL) {
                        uPortMutexDelete(pClient->mutex);
                    }
                    if (receiveBufferIsMalloced) {
                        uPortFree(pClient->pReceiveBuffer);
                    }
                    uPortFree(pClient);
                    pClient = NULL;
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(gMutex);

    return (uAtClientHandle_t) pClient;
}

// Tell an AT client to throw away asynchronous events.
void uAtClientIgnoreAsync(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(gMutex);

    if (pClient == NULL) {
        pClient = gpAtClientList;
        while (pClient != NULL) {
            ignoreAsync(pClient);
            pClient = pClient->pNext;
        }
    } else {
        ignoreAsync(pClient);
    }

    U_PORT_MUTEX_UNLOCK(gMutex);
}

// Remove an AT client.
void uAtClientRemove(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    if (pClient != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        removeClient(pClient);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Return whether general debug is on or not.
//lint -e{818} suppress "could be declared as pointing to const": it is!
bool uAtClientDebugGet(const uAtClientHandle_t atHandle)
{
    return ((uAtClientInstance_t *) atHandle)->debugOn;
}

// Set general debug on or off.
void uAtClientDebugSet(uAtClientHandle_t atHandle, bool onNotOff)
{
    // Keep Lint happy
    if (atHandle != NULL) {
        ((uAtClientInstance_t *) atHandle)->debugOn = onNotOff;
    }
}

// Return whether printing of AT commands is on or not.
//lint -e{818} suppress "could be declared as pointing to const": it is!
bool uAtClientPrintAtGet(const uAtClientHandle_t atHandle)
{
    return ((uAtClientInstance_t *) atHandle)->printAtOn;
}

// Set whether printing of AT commands is on or off.
void uAtClientPrintAtSet(uAtClientHandle_t atHandle, bool onNotOff)
{
    // Keep Lint happy
    if (atHandle != NULL) {
        ((uAtClientInstance_t *) atHandle)->printAtOn = onNotOff;
    }
}

// Return the current AT timeout.
//lint -e{818} suppress "could be declared as pointing to const": it is!
int32_t uAtClientTimeoutGet(const uAtClientHandle_t atHandle)
{
    return ((uAtClientInstance_t *) atHandle)->atTimeoutMs;
}

// Set the AT timeout.
void uAtClientTimeoutSet(uAtClientHandle_t atHandle, int32_t timeoutMs)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uPortMutexHandle_t streamMutex;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    // Try, without blocking, to lock this AT client's stream mutex
    streamMutex = streamTryLock(pClient, 0);
    if (streamMutex != NULL) {
        // We were able to lock the stream mutex, we're obviously
        // not currently in a lock, so set the timeout
        // forever and unlock the mutex again
        pClient->atTimeoutMs = timeoutMs;
        uPortMutexUnlock(streamMutex);
    } else {
        // We were not able to lock the stream mutex so we must
        // be in a lock.  In this case save the current
        // timeout before changing it so that we can put
        // it back once the stream mutex is unlocked
        if ((pClient->pWakeUp != NULL) &&
            ((uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) != 0) ||
             // This just to unlock the mutex if the try actually succeeded
             (uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex) != 0))) {
            // If we're in a wake-up handler stash the saved timeout
            // in the wake-up structure so that we can restore
            // it when we unwind back out without overwriting one
            // that might be saved in the client context
            if (pClient->pWakeUp->atTimeoutSavedMs < 0) {
                pClient->pWakeUp->atTimeoutSavedMs = pClient->atTimeoutMs;
            }
        } else {
            // Not in a wake-up handler so just save the timeout
            // in the client context
            if (pClient->atTimeoutSavedMs < 0) {
                pClient->atTimeoutSavedMs = pClient->atTimeoutMs;
            }
        }
        pClient->atTimeoutMs = timeoutMs;
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Set a callback to be called on consecutive AT timeouts.
void uAtClientTimeoutCallbackSet(uAtClientHandle_t atHandle,
                                 void (*pCallback) (uAtClientHandle_t,
                                                    int32_t *))
{
    // Keep Lint happy
    if (atHandle != NULL) {
        ((uAtClientInstance_t *) atHandle)->pConsecutiveTimeoutsCallback = pCallback;
    }
}

// Get the delimiter.
//lint -e{818} suppress "could be declared as pointing to const": it is!
char uAtClientDelimiterGet(const uAtClientHandle_t atHandle)
{
    return ((uAtClientInstance_t *) atHandle)->delimiter;
}

// Set the delimiter.
void uAtClientDelimiterSet(uAtClientHandle_t atHandle,
                           char delimiter)
{
    // Keep Lint happy
    if (atHandle != NULL) {
        ((uAtClientInstance_t *) atHandle)->delimiter = delimiter;
    }
}

// Get the delay between AT commands.
//lint -e{818} suppress "could be declared as pointing to const": it is!
int32_t uAtClientDelayGet(const uAtClientHandle_t atHandle)
{
    return ((uAtClientInstance_t *) atHandle)->delayMs;
}

// Set the delay between AT commands.
void uAtClientDelaySet(uAtClientHandle_t atHandle,
                       int32_t delayMs)
{
    // Keep Lint happy
    if (atHandle != NULL) {
        ((uAtClientInstance_t *) atHandle)->delayMs = delayMs;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEND AN AT COMMAND
 * -------------------------------------------------------------- */

// Lock the stream.
void uAtClientLock(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uPortMutexHandle_t streamMutex;

    // IMPORTANT: this can't lock pClient->mutex as it
    // needs to wait on the stream mutex and if it locked
    // pClient->mutex that would prevent uAtClientUnlock()
    // from working.
    if ((pClient != NULL) && (pClient->streamMutex != NULL)) {
        streamMutex = streamLock(pClient);
        mutexStackPush(&(pClient->lockedStreamMutexStack), streamMutex);
        if (pClient->pActivityPin != NULL) {
            while (uPortGetTickTimeMs() - pClient->pActivityPin->lastToggleTime <
                   pClient->pActivityPin->hysteresisMs) {
                uPortTaskBlock(U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS);
            }
            // If an activity pin is set then switch it on
            if (uPortGpioSet(pClient->pActivityPin->pin,
                             (int32_t) pClient->pActivityPin->highIsOn) == 0) {
                pClient->pActivityPin->lastToggleTime = uPortGetTickTimeMs();
                uPortTaskBlock(pClient->pActivityPin->readyMs);
            }
        }
        clearError(pClient);
        pClient->lockTimeMs = uPortGetTickTimeMs();
    }
}

// Unlock the stream and kick off a receive
// if there is some data lounging around.
int32_t uAtClientUnlock(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t sizeBytes;
    uPortMutexHandle_t streamMutex;
    int32_t sendErrorCode;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    streamMutex = mutexStackPop(&(pClient->lockedStreamMutexStack));
    if (streamMutex != NULL) {
        unlockNoDataCheck(pClient, streamMutex);

        switch (pClient->streamType) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                sizeBytes = uPortUartGetReceiveSize(pClient->streamHandle);
                if ((sizeBytes > 0) ||
                    (pClient->pReceiveBuffer->readIndex < pClient->pReceiveBuffer->length)) {
                    // Note: we use the "try" version of the UART event
                    // send function here, otherwise if the UART event queue
                    // is full we may get stuck since (a) this function has
                    // the AT client API locked and (b) the URC callback may
                    // be running a URC handler which could also be calling
                    // into the AT client API to read the elements of the URC;
                    // there is no danger here since, if there are already
                    // events in the UART queue, the URC callback will certainly
                    // be run anyway.
                    sendErrorCode = uPortUartEventTrySend(pClient->streamHandle,
                                                          U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                          0);
                    if ((sendErrorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) ||
                        (sendErrorCode == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED)) {
                        uPortUartEventSend(pClient->streamHandle,
                                           U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
                    }
                }
                break;
            case U_AT_CLIENT_STREAM_TYPE_EDM:
                sizeBytes = uShortRangeEdmStreamAtGetReceiveSize(pClient->streamHandle);
                if ((sizeBytes > 0) ||
                    (pClient->pReceiveBuffer->readIndex < pClient->pReceiveBuffer->length)) {
                    uShortRangeEdmStreamAtEventSend(pClient->streamHandle,
                                                    U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
                }
                break;
            default:
                break;
        }

        U_ASSERT(U_AT_CLIENT_GUARD_CHECK(pClient->pReceiveBuffer));
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return (int32_t) pClient->error;
}

// Start an AT command sequence.
void uAtClientCommandStart(uAtClientHandle_t atHandle,
                           const char *pCommand)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // Wait for delay period if required, constructed this way
        // to be safe if uPortGetTickTimeMs() wraps
        if (pClient->delayMs > 0) {
            while (uPortGetTickTimeMs() - pClient->lastResponseStopMs < pClient->delayMs) {
                uPortTaskBlock(10);
            }
        }

        // Send the command, no delimiter at first
        pClient->delimiterRequired = false;
        // Note: allow pCommand to be NULL here only
        // because that is useful during testing
        if (pCommand != NULL) {
            write(pClient, pCommand, strlen(pCommand), false);
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Write an integer parameter.
void uAtClientWriteInt(uAtClientHandle_t atHandle,
                       int32_t param)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char numberString[12];
    int32_t length;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (writeCheckAndDelimit(pClient)) {
        // Write the integer parameter
        length = snprintf(numberString, sizeof(numberString),
                          "%d", (int) param);
        if ((length > 0) && (length < (int32_t) sizeof(numberString))) {
            // write() will set device error if there's a problem
            write(pClient, numberString, length, false);
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Write a uint64_t parameter.
void uAtClientWriteUint64(uAtClientHandle_t atHandle,
                          uint64_t param)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char numberString[24];
    int32_t length;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (writeCheckAndDelimit(pClient)) {
        // Write the uint64_t parameter
        length = uint64ToString(numberString, sizeof(numberString),
                                param);
        if ((length > 0) && (length < (int32_t) sizeof(numberString))) {
            // write() will set device error if there's a problem
            write(pClient, numberString, length, false);
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Write a string parameter.
void uAtClientWriteString(uAtClientHandle_t atHandle,
                          const char *pParam,
                          bool useQuotations)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (writeCheckAndDelimit(pClient)) {
        // Write opening quotes if required
        if (useQuotations) {
            write(pClient, "\"", 1, false);
        }
        write(pClient, pParam, strlen(pParam), false);
        // Write closing quotes if required
        if (useQuotations) {
            write(pClient, "\"", 1, false);
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Write a sequence of bytes.
size_t uAtClientWriteBytes(uAtClientHandle_t atHandle,
                           const char *pData,
                           size_t lengthBytes,
                           bool standalone)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    size_t writeLength = 0;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    // Do write check and delimit if required, else
    // just check for errors
    if ((standalone || writeCheckAndDelimit(pClient)) &&
        (pClient->error == U_ERROR_COMMON_SUCCESS)) {
        // write() will set device error if there's a problem
        // If this is a standalone write, do a flush also
        writeLength = write(pClient, pData, lengthBytes, standalone);
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return writeLength;
}

void uAtClientWritePartialString(uAtClientHandle_t atHandle,
                                 bool isFirst,
                                 const char *pParam)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (!isFirst || writeCheckAndDelimit(pClient)) {
        write(pClient, pParam, strlen(pParam), false);
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Stop the outgoing part of an AT command sequence.
void uAtClientCommandStop(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // Finish by writing the AT command delimiter
        // write() will set device error if there's a problem
        write(pClient, U_AT_CLIENT_COMMAND_DELIMITER,
              U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES,
              true);
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Stop the outgoing part and deal with a simple response also.
void uAtClientCommandStopReadResponse(uAtClientHandle_t atHandle)
{
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, NULL);
    uAtClientResponseStop(atHandle);
}

// Start the response part.
int32_t uAtClientResponseStart(uAtClientHandle_t atHandle,
                               const char *pPrefix)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    bool prefixMatched = false;
    int32_t returnCode = (int32_t) pClient->error;

    // IMPORTANT: this can't lock pClient->mutex as it
    // checks for URCs and may end up calling a URC
    // handler which will also need the lock.

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // Stop any previous information response
        if (pClient->scope == U_AT_CLIENT_SCOPE_INFORMATION) {
            informationResponseStop(pClient);
        }
        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);

        // Bring as much data into the buffer as possible
        // but without blocking
        bufferRewind(pClient);
        bufferFill(pClient, false);

        // Now do the response processing
        setScope(pClient, U_AT_CLIENT_SCOPE_RESPONSE);
        prefixMatched = processResponse(pClient, pPrefix, true);

        // If the prefix matched we're in
        // the information response
        if (prefixMatched) {
            setScope(pClient, U_AT_CLIENT_SCOPE_INFORMATION);
            returnCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else {
            returnCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        }
    }
    return returnCode;
}

// Read an integer parameter.
int32_t uAtClientReadInt(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t integerRead;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    integerRead = readInt(pClient);

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return integerRead;
}

// Read a uint64_t parameter.
int32_t uAtClientReadUint64(uAtClientHandle_t atHandle,
                            uint64_t *pUint64)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char buffer[32]; // Enough for an integer
    int32_t returnValue = -1;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
        !pClient->stopTag.found &&
        (readString(pClient, buffer,
                    sizeof(buffer), false) > 0)) {
        // Would use sscanf() here but we cannot
        // rely on there being 64 bit sscanf() support
        // in the underlying library, hence
        // we do our own thing
        *pUint64 = stringToUint64(buffer);
        returnValue = 0;
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return returnValue;
}

// Read a string parameter.
int32_t uAtClientReadString(uAtClientHandle_t atHandle,
                            char *pString,
                            size_t lengthBytes,
                            bool ignoreStopTag)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t lengthRead;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    lengthRead = readString(pClient, pString, lengthBytes, ignoreStopTag);

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return lengthRead;
}

// Read bytes.
int32_t uAtClientReadBytes(uAtClientHandle_t atHandle,
                           char *pBuffer,
                           size_t lengthBytes,
                           bool standalone)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientTag_t *pStopTag = &(pClient->stopTag);
    int32_t lengthRead = 0;
    int32_t matchPos = 0;
    int32_t c;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    while ((lengthRead < ((int32_t) lengthBytes + matchPos)) &&
           (pClient->error == U_ERROR_COMMON_SUCCESS) &&
           !pStopTag->found) {
        c = bufferReadChar(pClient);
        if (c == -1) {
            // Error
            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
        } else {
            if (pStopTag->pTagDef->length > 0) {
                // It could be a stop tag
                if (c == *(pStopTag->pTagDef->pString + matchPos)) {
                    matchPos++;
                } else {
                    // If it wasn't a stop tag, reset
                    // the match position and check again
                    // in case it is the start of a new stop tag
                    matchPos = 0;
                    if (c == *(pStopTag->pTagDef->pString)) {
                        matchPos++;
                    }
                }
                if (matchPos == (int32_t) pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                    // Remove tag from string if it was matched
                    lengthRead -= (int32_t) pStopTag->pTagDef->length - 1;
                }
            } else {
                // Not anything
                matchPos = 0;
            }
            if (!pStopTag->found) {
                if (pBuffer != NULL) {
                    // Add the byte to the buffer
                    *(pBuffer + lengthRead) = (char) c;
                }
                lengthRead++;
            }
        }
    }

    if (!standalone) {
        // While this function ignores delimiters in the "wanted"
        // length, if it is not a standalone sequence
        // clear up any rubbish by consuming to delimiter or
        // stop tag
        c = -1;
        while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
               (c != pClient->delimiter) &&
               !pStopTag->found) {
            c = bufferReadChar(pClient);
            if (c == -1) {
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            } else if (pStopTag->pTagDef->length > 0) {
                // It could be a stop tag
                if (c == *(pStopTag->pTagDef->pString + matchPos)) {
                    matchPos++;
                } else {
                    // If it wasn't a stop tag, reset
                    // the match position and check again
                    // in case it is the start of a new stop tag
                    matchPos = 0;
                    if (c == *(pStopTag->pTagDef->pString)) {
                        matchPos++;
                    }
                }
                if (matchPos == (int32_t) pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                }
            }
        }
    }

    if (pClient->error != U_ERROR_COMMON_SUCCESS) {
        lengthRead = -1;
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return lengthRead;
}

// Stop the response part of an AT sequence.
void uAtClientResponseStop(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->scope == U_AT_CLIENT_SCOPE_INFORMATION) {
        informationResponseStop(pClient);
    }

    // Consume up to the response stop tag
    if (consumeToStopTag(pClient)) {
        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
    }

    pClient->lastResponseStopMs = uPortGetTickTimeMs();

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Switch off stop tag detection.
void uAtClientIgnoreStopTag(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Switch stop tag detection back on.
void uAtClientRestoreStopTag(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        setScope(pClient, U_AT_CLIENT_SCOPE_RESPONSE);
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Skip the given number of parameters.
void uAtClientSkipParameters(uAtClientHandle_t atHandle,
                             size_t count)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientTag_t *pStopTag = &(pClient->stopTag);
    bool inQuotes = false;
    size_t matchPos = 0;
    int32_t c;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    for (size_t x = 0; (x < count) && !pStopTag->found &&
         (pClient->error == U_ERROR_COMMON_SUCCESS); x++) {
        c = -1;
        while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
               (c != pClient->delimiter) &&
               !pStopTag->found) {
            c = bufferReadChar(pClient);
            if (c == -1) {
                // Error
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            } else if (!inQuotes && (c == pClient->delimiter)) {
                // Reached delimiter
            } else if (c == '\"') {
                // Switch into or out of quotes
                matchPos = 0;
                inQuotes = !inQuotes;
            } else if (!inQuotes &&
                       (pStopTag->pTagDef->length > 0)) {
                // It could be a stop tag
                if (c == *(pStopTag->pTagDef->pString + matchPos)) {
                    matchPos++;
                } else {
                    // If it wasn't a stop tag, reset
                    // the match position and check again
                    // in case it is the start of a new stop tag
                    matchPos = 0;
                    if (c == *(pStopTag->pTagDef->pString)) {
                        matchPos++;
                    }
                }
                if (matchPos == pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                }
            } else {
                matchPos = 0;
            }
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Skip the given number of bytes.
void uAtClientSkipBytes(uAtClientHandle_t atHandle,
                        size_t lengthBytes)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t c;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (!pClient->stopTag.found) {
        for (size_t x = 0; (x < lengthBytes) &&
             (pClient->error == U_ERROR_COMMON_SUCCESS);
             x++) {
            c = bufferReadChar(pClient);
            if (c == -1) {
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            }
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Wait for a single character to arrive.
int32_t uAtClientWaitCharacter(uAtClientHandle_t atHandle,
                               char character)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    int32_t stopTimeMs;
    bool urcFound;

    // IMPORTANT: this can't lock pClient->mutex as it
    // checks for URCs and hence may end up calling a
    // URC handler which itself will need to be able
    // to perform a lock.

    // Can't allow CR or LF since we remove them from the
    // stream as part of looking for URCs
    if ((character != 0x0d) && (character != 0x0a)) {
        errorCode = U_ERROR_COMMON_NOT_FOUND;
        if (!pClient->stopTag.found) {
            // While there is a timeout inside the call to bufferFill()
            // below, it might be that the length in the buffer never
            // gets to zero (in which case we won't call bufferFill())
            // and hence, for safety, we run our own AT timeout guard
            // on the loop as well
            stopTimeMs = uPortGetTickTimeMs() + pClient->atTimeoutMs;
            if (stopTimeMs < 0) {
                // Protect against wrapping
                stopTimeMs = pClient->atTimeoutMs;
            }
            while ((errorCode != U_ERROR_COMMON_SUCCESS) &&
                   (pClient->error == U_ERROR_COMMON_SUCCESS)) {
                // Continue to look for URCs, you never
                // know when they might turn up
                do {
                    // Need to remove any CR/LF's at the start
                    while (bufferMatch(pClient, U_AT_CLIENT_CRLF,
                                       U_AT_CLIENT_CRLF_LENGTH_BYTES)) {}
                    urcFound = bufferMatchOneUrc(pClient);
                } while (urcFound);

                // Check for a device error landing in the buffer
                deviceErrorInBuffer(pClient);
                // Now we can check for our wanted character, removing
                // at least one character now that we know that what is
                // in there is not a URC.  Of course this relies upon
                // the module sending URCs in coherent lines, not
                // stuttering them out with gaps such that we receive just
                // part of a URC prefix, but the alternative is to not
                // remove irrelevant characters (e.g. from URCs that
                // we have set no capture for) in our search for the
                // wanted character, which would be a larger problem
                if (consumeOneCharacter(pClient, character, true)) {
                    // Got it: the character will be removed from the buffer
                    // and all is good
                    errorCode = U_ERROR_COMMON_SUCCESS;
                } else {
                    // Remove the processed stuff from the buffer
                    bufferRewind(pClient);
                    if (pReceiveBuffer->length == 0) {
                        // If there's nothing left, try to get more stuff
                        if (!bufferFill(pClient, true)) {
                            // If we don't get any data within
                            // the timeout, set an error to
                            // indicate the need for recovery
                            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                            consecutiveTimeout(pClient);
                        } else {
                            pClient->numConsecutiveAtTimeouts = 0;
                        }
                    } else {
                        if (uPortGetTickTimeMs() > stopTimeMs) {
                            // If we're stuck, set an error
                            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                            consecutiveTimeout(pClient);
                        }
                    }
                }
            }
        }
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: HANDLE UNSOLICITED RESPONSES
 * -------------------------------------------------------------- */

// Set a handler for a URC.
int32_t uAtClientSetUrcHandler(uAtClientHandle_t atHandle,
                               const char *pPrefix,
                               void (*pHandler) (uAtClientHandle_t,
                                                 void *),
                               void *pHandlerParam)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientUrc_t *pUrc = NULL;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    size_t prefixLength;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if ((pPrefix != NULL) && (pHandler != NULL)) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        if (!findUrcHandler(pClient, pPrefix)) {
            pUrc = (uAtClientUrc_t *) pUPortMalloc(sizeof(uAtClientUrc_t));
            if (pUrc != NULL) {
                prefixLength = strlen(pPrefix);
                if (prefixLength > pClient->urcMaxStringLength) {
                    pClient->urcMaxStringLength = prefixLength;
                    if (pClient->urcMaxStringLength > pClient->maxRespLength) {
                        pClient->maxRespLength = pClient->urcMaxStringLength;
                    }
                }
                pUrc->pPrefix = pPrefix;
                pUrc->prefixLength = prefixLength;
                pUrc->pHandler = pHandler;
                pUrc->pHandlerParam = pHandlerParam;

                errorCode = U_ERROR_COMMON_SUCCESS;
            }
        } else {
            errorCode = U_ERROR_COMMON_SUCCESS;
            if (pClient->debugOn) {
                uPortLog("U_AT_CLIENT_%d-%d: URC already added with prefix"
                         " \"%s\".\n", pClient->streamType,
                         pClient->streamHandle, pPrefix);
            }
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    if (pUrc != NULL) {
        // Only insert the URC in the list outside the pClient mutex lock,
        // since we need to prevent a URC happening while we do so and we
        // can't do that within the locks as a URC callback might have
        // locked  pClient->mutex
        U_PORT_MUTEX_LOCK(pClient->urcPermittedMutex);

        pUrc->pNext = pClient->pUrcList;
        pClient->pUrcList = pUrc;

        U_PORT_MUTEX_UNLOCK(pClient->urcPermittedMutex);
    }

    return (int32_t) errorCode;
}

// Remove a URC handler.
void uAtClientRemoveUrcHandler(uAtClientHandle_t atHandle,
                               const char *pPrefix)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientUrc_t *pCurrent = pClient->pUrcList;
    uAtClientUrc_t *pPrev = NULL;

    // IMPORTANT: this can't lock pClient->mutex as it
    // needs to be able to acquire urcPermittedMutex
    // under which a URC handler might have already
    // locked pClient->mutex

    while (pCurrent != NULL) {
        if (strcmp(pPrefix, pCurrent->pPrefix) == 0) {

            // Stop any URCs occurring while we modify the list
            U_PORT_MUTEX_LOCK(pClient->urcPermittedMutex);

            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                pClient->pUrcList = pCurrent->pNext;
            }

            U_PORT_MUTEX_UNLOCK(pClient->urcPermittedMutex);

            uPortFree(pCurrent);
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
}

// Get the stack high watermark for the URC task.
int32_t uAtClientUrcHandlerStackMinFree(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t stackMinFree = -1;

    switch (pClient->streamType) {
        case U_AT_CLIENT_STREAM_TYPE_UART:
            stackMinFree = uPortUartEventStackMinFree(pClient->streamHandle);
            break;
        case U_AT_CLIENT_STREAM_TYPE_EDM:
            stackMinFree = uShortRangeEdmStreamAtEventStackMinFree(pClient->streamHandle);
            break;
        default:
            break;
    }

    return stackMinFree;
}

// Make a callback resulting from a URC.
int32_t uAtClientCallback(uAtClientHandle_t atHandle,
                          void (*pCallback) (uAtClientHandle_t, void *),
                          void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientCallback_t cb;

    U_PORT_MUTEX_LOCK(gMutexEventQueue);

    if (pCallback != NULL) {
        cb.pFunction = pCallback;
        cb.atHandle = atHandle;
        cb.pParam = pCallbackParam;
        cb.atClientMagicNumber = ((uAtClientInstance_t *) atHandle)->magicNumber;
        errorCode = uPortEventQueueSend(gEventQueueHandle, &cb, sizeof(cb));
    }

    U_PORT_MUTEX_UNLOCK(gMutexEventQueue);

    return errorCode;
}

// Get the stack high watermark for the AT callback task.
int32_t uAtClientCallbackStackMinFree()
{
    int32_t sizeOrErrorCode;

    U_PORT_MUTEX_LOCK(gMutexEventQueue);

    sizeOrErrorCode = uPortEventQueueStackMinFree(gEventQueueHandle);

    U_PORT_MUTEX_UNLOCK(gMutexEventQueue);

    return sizeOrErrorCode;
}

// Handle a URC "in-line".
int32_t uAtClientUrcDirect(uAtClientHandle_t atHandle,
                           const char *pPrefix,
                           void (*pHandler) (uAtClientHandle_t,
                                             void *),
                           void *pHandlerParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    size_t strlenPrefix;
    bool prefixFound = false;

    // IMPORTANT: this can't lock pClient->mutex as it
    // checks for URCs asynchronously (as well as directly)

    if ((pPrefix != NULL) && (pHandler != NULL)) {
        errorCode = (int32_t) pClient->error;
        if (pClient->error == U_ERROR_COMMON_SUCCESS) {
            strlenPrefix = strlen(pPrefix);

            // Clear out any previous scope
            setScope(pClient, U_AT_CLIENT_SCOPE_NONE);

            // Bring all the available data into the buffer
            bufferRewind(pClient);
            bufferFill(pClient, false);

            // Set us to information response mode, i.e. a
            // line with a CR/LF on the end
            setScope(pClient, U_AT_CLIENT_SCOPE_INFORMATION);

            // Look for the URC prefix
            while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
                   (!pClient->stopTag.found) && !prefixFound) {
                // Remove the CR/LF's that should be at the start
                while (bufferMatch(pClient, U_AT_CLIENT_CRLF,
                                   U_AT_CLIENT_CRLF_LENGTH_BYTES)) {}
                prefixFound = bufferMatch(pClient, pPrefix, strlenPrefix);
                // If no prefix was found, check for a URC; yes,
                // another URC might arrive while we're waiting for
                // _this_ URC. If we don't find a URC either then
                // try to bring in more stuff, blocking until done
                if (!prefixFound && !bufferMatchOneUrc(pClient) &&
                    !bufferFill(pClient, true)) {
                    // nuffin: set an error to get us out of here
                    setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
                }
            }

            if (prefixFound) {
                // Found it, call the handler
                pHandler(pClient, pHandlerParam);
                // Consume up to the CR/LF stop tag
                if (consumeToStopTag(pClient)) {
                    setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
                }
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Flush the receive buffer
void uAtClientFlush(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->debugOn) {
        uPortLog("U_AT_CLIENT_%d-%d: flush.\n", pClient->streamType,
                 pClient->streamHandle);
    }

    bufferReset(pClient, true);
    while (bufferFill(pClient, false)) {
        bufferReset(pClient, true);
    }

    // For security
    memset(U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer), 0,
           pClient->pReceiveBuffer->dataBufferSize);

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Clear the error status to none.
void uAtClientClearError(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    clearError(pClient);

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Get the error status
int32_t uAtClientErrorGet(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uErrorCode_t error;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    error = pClient->error;

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return (int32_t) error;
}

// Get the device error status (i.e. from CMS ERROR or
// CME ERROR).
void uAtClientDeviceErrorGet(uAtClientHandle_t atHandle,
                             uAtClientDeviceError_t *pDeviceError)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pDeviceError != NULL) {
        memcpy(pDeviceError, &(pClient->deviceError),
               sizeof(*pDeviceError));
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Get the handle and type of the underlying stream
int32_t uAtClientStreamGet(uAtClientHandle_t atHandle,
                           uAtClientStream_t *pStreamType)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    *pStreamType = pClient->streamType;

    return pClient->streamHandle;
}

// Add a transmit intercept function.
void uAtClientStreamInterceptTx(uAtClientHandle_t atHandle,
                                const char *(*pCallback) (uAtClientHandle_t,
                                                          const char **,
                                                          size_t *,
                                                          void *),
                                void *pContext)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    pClient->pInterceptTx = pCallback;
    pClient->pInterceptTxContext = pContext;

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Add a receive intercept function.
void uAtClientStreamInterceptRx(uAtClientHandle_t atHandle,
                                char *(*pCallback) (uAtClientHandle_t,
                                                    char **,
                                                    size_t *,
                                                    void *),
                                void *pContext)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    // Must reset the buffer before doing this
    // as there are indexes in there that keep
    // track of where the intercept function is at
    bufferReset(pClient, true);

    pClient->pInterceptRx = pCallback;
    pClient->pInterceptRxContext = pContext;

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);
}

// Set a wake-up handler function.
int32_t uAtClientSetWakeUpHandler(uAtClientHandle_t atHandle,
                                  int32_t (*pHandler) (uAtClientHandle_t,
                                                       void *),
                                  void *pHandlerParam,
                                  int32_t inactivityTimeoutMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uPortTaskHandle_t dummy;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    // Make sure that the uPortTaskGetHandle(), uPortEnterCritical()
    // and uPortExitCritical() APIs are supported because
    // the wake-up process requires them.
    if ((uPortTaskGetHandle(&dummy) == 0) && (uPortEnterCritical() == 0)) {
        uPortExitCritical();
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        if (pHandler == NULL) {
            // Switching the wake-up handler off
            if (pClient->pWakeUp != NULL) {
                // Mustn't be in the wake-up handler
                U_ASSERT(uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) == 0);
                // Delete all the mutexes
                uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);
                uPortMutexDelete(pClient->pWakeUp->inWakeUpHandlerMutex);
                U_PORT_MUTEX_LOCK(pClient->pWakeUp->streamMutex);
                U_PORT_MUTEX_UNLOCK(pClient->pWakeUp->streamMutex);
                uPortMutexDelete(pClient->pWakeUp->streamMutex);
                U_PORT_MUTEX_LOCK(pClient->pWakeUp->mutex);
                U_PORT_MUTEX_UNLOCK(pClient->pWakeUp->mutex);
                uPortMutexDelete(pClient->pWakeUp->mutex);
                uPortFree(pClient->pWakeUp);
                pClient->pWakeUp = NULL;
            }
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else {
            if (pClient->pWakeUp == NULL) {
                pClient->pWakeUp = (uAtClientWakeUp_t *) pUPortMalloc(sizeof(*(pClient->pWakeUp)));
                if (pClient->pWakeUp != NULL) {
                    memset(pClient->pWakeUp, 0, sizeof(*pClient->pWakeUp));
                    if (uPortMutexCreate(&(pClient->pWakeUp->inWakeUpHandlerMutex)) == 0) {
                        if (uPortMutexCreate(&(pClient->pWakeUp->mutex)) == 0) {
                            if (uPortMutexCreate(&(pClient->pWakeUp->streamMutex)) == 0) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            }
                        }
                    }
                    if (errorCode != 0) {
                        // Clean up if we couldn't create a mutex
                        if (pClient->pWakeUp->inWakeUpHandlerMutex != NULL) {
                            uPortMutexDelete(pClient->pWakeUp->inWakeUpHandlerMutex);
                        }
                        if (pClient->pWakeUp->mutex != NULL) {
                            uPortMutexDelete(pClient->pWakeUp->mutex);
                        }
                        if (pClient->pWakeUp->streamMutex != NULL) {
                            uPortMutexDelete(pClient->pWakeUp->streamMutex);
                        }
                        uPortFree(pClient->pWakeUp);
                        pClient->pWakeUp = NULL;
                    }
                }
            } else {
                // re-use the existing wake-up context, just
                // mustn't be in the wake-up handler
                U_ASSERT(uPortMutexTryLock(pClient->pWakeUp->inWakeUpHandlerMutex, 0) == 0);
                uPortMutexUnlock(pClient->pWakeUp->inWakeUpHandlerMutex);
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
            if (pClient->pWakeUp != NULL) {
                pClient->pWakeUp->pHandler = pHandler;
                pClient->pWakeUp->pParam = pHandlerParam;
                pClient->pWakeUp->inactivityTimeoutMs = inactivityTimeoutMs;
                pClient->pWakeUp->atTimeoutSavedMs = -1;
                pClient->pWakeUp->wakeUpTask = NULL;
            }
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return errorCode;
}

// Return true if a wake-up handler is set.
//lint -esym(818, atHandle) Suppress could be declared
// as pointing to const. it is!
bool uAtClientWakeUpHandlerIsSet(const uAtClientHandle_t atHandle)
{
    return ((const uAtClientInstance_t *) atHandle)->pWakeUp != NULL;
}

// Set an "activity" pin.
int32_t uAtClientSetActivityPin(uAtClientHandle_t atHandle,
                                int32_t pin, int32_t readyMs,
                                int32_t hysteresisMs, bool highIsOn)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pin < 0) {
        uPortFree(pClient->pActivityPin);
        pClient->pActivityPin = NULL;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    } else {
        if (pClient->pActivityPin == NULL) {
            pClient->pActivityPin = (uAtClientActivityPin_t *) pUPortMalloc(sizeof(*(pClient->pActivityPin)));
        }
        if (pClient->pActivityPin != NULL) {
            pClient->pActivityPin->pin = pin;
            pClient->pActivityPin->readyMs = readyMs;
            pClient->pActivityPin->highIsOn = highIsOn;
            pClient->pActivityPin->lastToggleTime = uPortGetTickTimeMs();
            pClient->pActivityPin->hysteresisMs = hysteresisMs;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return errorCode;
}

// Return the activity pin.
//lint -esym(818, atHandle) Suppress could be declared
// as pointing to const. it is!
int32_t uAtClientGetActivityPin(const uAtClientHandle_t atHandle)
{
    int32_t activityPin = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    const uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_AT_CLIENT_LOCK_CLIENT_MUTEX(pClient);

    if (pClient->pActivityPin != NULL) {
        activityPin = pClient->pActivityPin->pin;
    }

    U_AT_CLIENT_UNLOCK_CLIENT_MUTEX(pClient);

    return activityPin;
}
// End of file
