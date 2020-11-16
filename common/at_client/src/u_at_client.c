/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), strcmp(), strcspn(), strspm()
#include "ctype.h"     // isprint()
#include "math.h"      // pow()
#include "assert.h"
#include "stdio.h"     // For snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"
#include "u_port_clib_platform_specific.h"
#include "u_short_range_edm_stream.h"

#include "u_at_client.h"

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

// Do some cross-checking
#if (U_AT_CLIENT_CALLBACK_TASK_PRIORITY >= U_AT_CLIENT_URC_TASK_PRIORITY)
# error U_AT_CLIENT_CALLBACK_TASK_PRIORITY must be less than U_AT_CLIENT_URC_TASK_PRIORITY
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
    size_t length;     /** The number of characters in the buffer. */
    size_t readIndex;  /** The read position in the buffer. */
    char mk0[U_AT_CLIENT_MARKER_SIZE]; /** Opening marker. */
} uAtClientReceiveBuffer_t;

/** A struct defining a callback plus its optional parameter.
 */
typedef struct {
    void (*pFunction) (uAtClientHandle_t, void *);
    uAtClientHandle_t atHandle;
    void *pParam;
} uAtClientCallback_t;

/** Definition of an AT client instance.
 */
typedef struct uAtClientInstance_t {
    int32_t streamHandle; /** The stream handle to use. */
    uAtClientStream_t streamType; /** The type of API that streamHandle applies to. */
    uPortMutexHandle_t mutex; /** Mutex for threadsafeness. */
    uPortMutexHandle_t streamMutex; /** Mutex for the data stream. */
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
    int64_t lastResponseStopMs; /** The time the last response ended in milliseconds. */
    int64_t lockTimeMs; /** The time when the stream was locked. */
    size_t urcMaxStringLength; /** The longest URC string to monitor for. */
    size_t maxRespLength; /** The max length of OK, (CME) (CMS) ERROR and URCs. */
    bool delimiterRequired; /** Is a delimiter to be inserted before the next parameter or not. */
    const char *(*pInterceptTx) (uAtClientHandle_t,
                                 const char *,
                                 size_t *,
                                 void *); /** Function that intercepts Tx data before it
                                              is given to the stream. */
    void *pInterceptTxContext; /** Context pointer that will be passed to pInterceptTx
                                   as its fourth parameter. */
    char *(*pInterceptRx) (uAtClientHandle_t,
                           char *, size_t *,
                           void *); /** Function that intercepts received data before it is
                                        processed by the AT client. */
    void *pInterceptRxContext; /** Context pointer that will be passed to pInterceptRx
                                   as its fourth parameter. */
    struct uAtClientInstance_t *pNext;
} uAtClientInstance_t;

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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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

// Add an AT client instance to the list.
// gMutex should be locked before this is called.
// Note: doesn't copy it, just adds it.
static void addAtClientInstance(uAtClientInstance_t *pClient)
{
    pClient->pNext = gpAtClientList;
    gpAtClientList = pClient;
}

// Remove an AT client instance from the list.
// gMutex should be locked before this is called.
// Note: doesn't free it, the caller must do that.
static void removeAtClientInstance(const uAtClientInstance_t *pClient)
{
    uAtClientInstance_t *pCurrent;
    uAtClientInstance_t *pPrev = NULL;

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

    // Lock the stream first to avoid pulling
    // the rug out from under a URC
    U_PORT_MUTEX_LOCK(pClient->streamMutex);

    U_PORT_MUTEX_LOCK(pClient->mutex);

    // Remove it from the list
    removeAtClientInstance(pClient);

    // Free any URC handlers it had.
    while (pClient->pUrcList != NULL) {
        pUrc = pClient->pUrcList;
        pClient->pUrcList = pUrc->pNext;
        free(pUrc);
    }

    // Remove the URC event handler
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

    // Delete the stream mutex
    U_PORT_MUTEX_UNLOCK(pClient->streamMutex);
    uPortMutexDelete(pClient->streamMutex);

    // Free the receive buffer if it was malloc()ed.
    if (pClient->pReceiveBuffer->isMalloced) {
        free(pClient->pReceiveBuffer);
    }

    // Unlock its main mutex so that we can delete it
    U_PORT_MUTEX_UNLOCK(pClient->mutex);
    uPortMutexDelete(pClient->mutex);

    // And finally free the client context.
    free(pClient);
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
                if (c == '\r') {
                    // Convert \r\n into \n
                    uPortLog("%c", '\n');
                } else if (c == '\n') {
                    // Do nothing
                } else {
                    // Print the hex
                    uPortLog("[%02x]", c);
                }
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

    pClient->numConsecutiveAtTimeouts++;
    if (pClient->pConsecutiveTimeoutsCallback != NULL) {
        // pConsecutiveTimeoutsCallback second parameter
        // is an int32_t pointer but of course the generic
        // callback function is a void pointer so
        // need to cast here
        cb.pFunction = (void (*) (uAtClientHandle_t, void *)) pClient->pConsecutiveTimeoutsCallback;
        cb.atHandle = (uAtClientHandle_t) pClient;
        cb.pParam = &(pClient->numConsecutiveAtTimeouts);
        uPortEventQueueSend(gEventQueueHandle, &cb, sizeof(cb));
    }
}

// Calculate the remaining time for polling based on the start
// time and the AT timeout. Returns the time remaining for
// polling in milliseconds.
static int32_t pollTimeRemaining(int32_t atTimeoutMs,
                                 int64_t lockTimeMs)
{
    int64_t timeRemainingMs;
    int64_t now = uPortGetTickTimeMs();

    if (atTimeoutMs >= 0) {
        if (now > lockTimeMs + atTimeoutMs) {
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
static void bufferReset(const uAtClientInstance_t *pClient)
{
    pClient->pReceiveBuffer->readIndex = 0;
    pClient->pReceiveBuffer->length = 0;
}

// Set the read position to 0 and move the buffer's
// unread content to the beginning.
static void bufferRewind(const uAtClientInstance_t *pClient)
{
    uAtClientReceiveBuffer_t *pBuffer = pClient->pReceiveBuffer;

    if ((pBuffer->readIndex > 0) &&
        (pBuffer->length >= pBuffer->readIndex)) {
        pBuffer->length -= pBuffer->readIndex;
        // Move what has not been read to the
        // beginning of the buffer
        memmove(((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t),
                ((char *) pBuffer) + sizeof(uAtClientReceiveBuffer_t) + pBuffer->readIndex,
                pBuffer->length);
        pBuffer->readIndex = 0;
    }
}

// This is where data comes into the AT client.
// Read from the stream into the receive buffer.
// Returns true on a successful read or false on timeout.
static bool bufferFill(uAtClientInstance_t *pClient,
                       bool blocking)
{
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    int32_t atTimeoutMs = -1;
    int32_t readLength = -1;
    size_t actualLength = 0;
    bool eventIsCallback = false;
    char *pDataRead;

    // If we're blocking, set the timeout value.
    if (blocking) {
        atTimeoutMs = pClient->atTimeoutMs;
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
        if (eventIsCallback) {
            // Short timeout if we're in a URC callback
            atTimeoutMs = U_AT_CLIENT_URC_TIMEOUT_MS;
        }
    }

    // Reset buffer if it's become full
    if (pReceiveBuffer->length == pReceiveBuffer->dataBufferSize) {
        if (pClient->debugOn) {
            uPortLog("U_AT_CLIENT_%d-%d: !!! overflow.\n",
                     pClient->streamType, pClient->streamHandle);
        }
        printAt(pClient, U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer),
                pReceiveBuffer->length);
        bufferReset(pClient);
    }

    // Do the read
    do {
        switch (pClient->streamType) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                readLength = uPortUartRead(pClient->streamHandle,
                                           U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                           pReceiveBuffer->length,
                                           pReceiveBuffer->dataBufferSize -
                                           pReceiveBuffer->length);
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
        // Block for a little while in case the data coming
        // in is stuttering; we want a good load or we'll just
        // be looping on partially obtained strings to no useful
        // effect.
        uPortTaskBlock(U_AT_CLIENT_STREAM_READ_RETRY_DELAY_MS);
    } while ((readLength == 0) &&
             (pollTimeRemaining(atTimeoutMs, pClient->lockTimeMs) > 0));

    if (readLength > 0) {
        // Call the intercept function if there is one
        if (pClient->pInterceptRx != NULL) {
            // Cast in two stages to keep Lint happy
            actualLength = (size_t) (unsigned) readLength;
            pDataRead = pClient->pInterceptRx((uAtClientHandle_t) pClient,
                                              U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                              pReceiveBuffer->length, &actualLength,
                                              pClient->pInterceptRxContext);
            readLength = (int32_t) actualLength;
            // pDataRead may be further forward in the buffer
            // so add the difference to the read index so that we
            // effectively drop it
            pReceiveBuffer->readIndex += pDataRead - (U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                                                      pReceiveBuffer->length);
        }

        printAt(pClient, U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                pReceiveBuffer->length + pReceiveBuffer->readIndex,
                readLength);
        pReceiveBuffer->length += readLength;
    } else {
        // If there was no data and there is an intercept function,
        // call it with a NULL data pointer so that it can reset
        // itself
        if (pClient->pInterceptRx != NULL) {
            pClient->pInterceptRx((uAtClientHandle_t) pClient,
                                  NULL, &actualLength,
                                  pClient->pInterceptRxContext);
        }
    }

    assert(U_AT_CLIENT_GUARD_CHECK(pReceiveBuffer));

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

    if (pReceiveBuffer->readIndex < pReceiveBuffer->length) {
        // Read from the buffer
        character = *(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
                      pReceiveBuffer->readIndex);
        pReceiveBuffer->readIndex++;
    } else {
        // Everything has been read, try to bring more in
        bufferReset(pClient);
        if (bufferFill(pClient, true)) {
            // Read something, all good
            character = *(U_AT_CLIENT_DATA_BUFFER_PTR(pReceiveBuffer) +
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

// Look for pString in the current receive buffer,
// without bringing more data into it, and if the string
// is there consume it.
static bool bufferMatch(const uAtClientInstance_t *pClient,
                        const char *pString, size_t length)
{
    uAtClientReceiveBuffer_t *pReceiveBuffer = pClient->pReceiveBuffer;
    bool found = false;

    bufferRewind(pClient);

    if ((pReceiveBuffer->length - pReceiveBuffer->readIndex) >= length) {
        if (pString && (memcmp(U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer) +
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
                                char character)
{
    int32_t readCharacter = bufferReadChar(pClient);

    if (readCharacter >= 0) {
        if (((char) readCharacter) != character) {
            // If we read something other than character,
            // reset the buffer index to "put it back"
            pClient->pReceiveBuffer->readIndex--;
        }
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
                // information response prefix
                consumeOneCharacter(pClient, ' ');
                pStopTag->pTagDef = &gInformationStopTag;
                break;
            case U_AT_CLIENT_SCOPE_NONE:
                pStopTag->pTagDef = &gNoStopTag;
                break;
            default:
                //lint -e506 Suppress constant value Boolean
                assert(false);
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
            }
        } else if (index != 0) {
            index = 0;
            if (character == *pString) {
                index++;
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
            bufferReset(pClient);
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
    int64_t now;
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
                if (pUrc->pHandler) {
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
                (pStopTag->pTagDef->length > 0) &&
                (c == *(pStopTag->pTagDef->pString + matchPos))) {
                // It could be a stop tag
                matchPos++;
                if (matchPos == pStopTag->pTagDef->length) {
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
            } else if ((pStopTag->pTagDef->length > 0) &&
                       (c == *(pStopTag->pTagDef->pString + matchPos))) {
                matchPos++;
                if (matchPos == pStopTag->pTagDef->length) {
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
                            // If there is no prefix, no CR/LF after some stuff
                            // and no point in running bufferMatch for OK,
                            // ERROR or a URC again (since maxRespLength has
                            // already been checked in the buffer), return
                            // so that the caller can do something
                            if (!pPrefix &&
                                ((pClient->pReceiveBuffer->length -
                                  pClient->pReceiveBuffer->readIndex) >=
                                 pClient->maxRespLength)) {
                                processingDone = true;
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
    }

    return prefixMatched;
}

// Write data to the stream.
static size_t write(uAtClientInstance_t *pClient,
                    const char *pData, size_t length,
                    bool andFlush)
{
    int32_t thisLengthWritten = 0;
    size_t lengthToWrite = length;
    const char *pDataToWrite = pData;

    // Call the intercept function if there is one
    if (pClient->pInterceptTx != NULL) {
        pDataToWrite = pClient->pInterceptTx((uAtClientHandle_t) pClient,
                                             pData, &lengthToWrite,
                                             pClient->pInterceptTxContext);
        // If we are to flush, call the intercept function
        // again with NULL
        if (andFlush) {
            pDataToWrite = pClient->pInterceptTx((uAtClientHandle_t) pClient,
                                                 NULL, &lengthToWrite,
                                                 pClient->pInterceptTxContext);
        }
    }
    while ((lengthToWrite > 0) &&
           (pClient->error == U_ERROR_COMMON_SUCCESS)) {
        switch (pClient->streamType) {
            case U_AT_CLIENT_STREAM_TYPE_UART:
                thisLengthWritten = uPortUartWrite(pClient->streamHandle,
                                                   pDataToWrite, lengthToWrite);
                break;
            //Write handled in intercept
            case U_AT_CLIENT_STREAM_TYPE_EDM:
                break;
            default:
                break;
        }
        if (thisLengthWritten > 0) {
            pDataToWrite += thisLengthWritten;
            lengthToWrite -= thisLengthWritten;
        } else {
            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
        }
    }

    // If there is an intercept function it may be that
    // the length written is longer or shorter than
    // passed in so it's not easily possible to printAt()
    // exactly what was written, we can only check
    // if *everything* was written
    if (lengthToWrite == 0) {
        printAt(pClient, pData, length);
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
// Returns zero on success.
static int32_t tryLock(uAtClientInstance_t *pClient)
{
    int32_t outcome;

    outcome = uPortMutexTryLock(pClient->streamMutex, 0);
    if (outcome == 0) {
        pClient->lockTimeMs = uPortGetTickTimeMs();
    }

    return outcome;
}

// Unlock the stream without kicking off
// any further data reception.  This is used
// directly in taskUrc to avoid recursion.
static void unlockNoDataCheck(uAtClientInstance_t *pClient)
{
    // If there is a saved timeout value, restore it
    // before unlocking the mutex
    if (pClient->atTimeoutSavedMs >= 0) {
        pClient->atTimeoutMs = pClient->atTimeoutSavedMs;
        pClient->atTimeoutSavedMs = -1;
    }
    uPortMutexUnlock(pClient->streamMutex);
}

// Convert a string which should contain
// something like "7587387289371387" (and
// be null-terminated) into a uint64_t
// Any leading crap is ignored and conversion
// stops when a non-numeric character is reached.
static uint64_t stringToUint64(const char *pBuffer)
{
    uint64_t uint64 = 0;
    int32_t length;
    const char *pNumerals = "0123456789";
    uint64_t multiplier;

    // Skip things that aren't numerals
    pBuffer += strcspn(pBuffer, pNumerals);
    // Determine the length of the numerals part
    //lint -e{712} Suppress loss of precision
    // (strspn() returns an int64_t with the compiler we use for Linting)
    length = strspn(pBuffer, pNumerals);
    while (length > 0) {
        multiplier = (uint64_t) pow(10, (length - 1));
        uint64 += (*pBuffer - '0') * multiplier;
        length--;
        pBuffer++;
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

// Get the amount of stuff in the receive buffer for a stream.
static int32_t getReceiveSize(const uAtClientInstance_t *pClient)
{
    int32_t receiveSize = -1;

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

    return receiveSize;
}

// Callback to find URC's from AT responses, triggered through
// something being received from the AT server.
static void urcCallback(int32_t streamHandle, uint32_t eventBitmask,
                        void *pParameters)
{
    uAtClientInstance_t *pClient;
    uAtClientReceiveBuffer_t *pReceiveBuffer;
    int32_t sizeOrError;

    pClient = (uAtClientInstance_t *) pParameters;

    if ((pClient != NULL) &&
        (pClient->streamHandle == streamHandle) &&
        (eventBitmask & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
        // Potential URC data is available.  However,
        // the main thread may already have taken the lock
        // and be processing it, in which case just return.
        if (tryLock(pClient) == 0) {
            // Loop until no received characters left to process
            pReceiveBuffer = pClient->pReceiveBuffer;
            while (((sizeOrError = getReceiveSize(pClient)) > 0) ||
                   (pReceiveBuffer->readIndex < pReceiveBuffer->length)) {
                if (pClient->debugOn) {
                    uPortLog("U_AT_CLIENT_%d-%d: possible URC data readable %d,"
                             " already buffered %u.\n", pClient->streamType,
                             pClient->streamHandle, sizeOrError,
                             pReceiveBuffer->length - pReceiveBuffer->readIndex);
                }
                pClient->scope = U_AT_CLIENT_SCOPE_NONE;
                for (size_t x = 0; x < U_AT_CLIENT_URC_DATA_LOOP_GUARD; x++) {
                    // Search through the URCs
                    if (bufferMatchOneUrc(pClient)) {
                        // If there's a bufferMatch, see if more data is available
                        sizeOrError = getReceiveSize(pClient);
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
                        if (bufferFill(pClient, true)) {
                            // Start the cycle again as if we'd just done
                            // uAtClientLock()
                            pClient->lockTimeMs = uPortGetTickTimeMs();
                        } else {
                            // There is no more data: clear anything that
                            // could not be handled and leave this loop
                            bufferReset(pClient);
                            break;
                        }
                    }
                }
                if (pClient->debugOn) {
                    uPortLog("U_AT_CLIENT_%d-%d: URC checking done.\n",
                             pClient->streamType, pClient->streamHandle);
                }
            }

            // Just unlock the stream without
            // checking for more data, which would try
            // to queue stuff on this task and I'm not
            // sure that's safe
            unlockNoDataCheck(pClient);
        }
    }
}

// Callback for the event queue.
static void eventQueueCallback(void *pParameters, size_t paramLength)
{
    uAtClientCallback_t *pCb = (uAtClientCallback_t *) pParameters;

    (void) paramLength;

    if ((pCb != NULL) && (pCb->pFunction != NULL)) {
        pCb->pFunction(pCb->atHandle, pCb->pParam);
    }
}

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
            // Create the mutex that protects the linked list
            errorCodeOrHandle = uPortMutexCreate(&gMutex);
            if (errorCodeOrHandle != 0) {
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

        // Release the callbacks event queue
        uPortEventQueueClose(gEventQueueHandle);

        // Delete the mutex
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
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
        // See if there's already an AT client for this stream
        pClient = pGetAtClientInstance(streamHandle, streamType);
        if (pClient == NULL) {
            // Nope, create one
            pClient = (uAtClientInstance_t *) malloc(sizeof(uAtClientInstance_t));
            if (pClient != NULL) {
                pClient->pReceiveBuffer = (uAtClientReceiveBuffer_t *) pReceiveBuffer;
                // Make sure we have a receive buffer
                if (pClient->pReceiveBuffer == NULL) {
                    receiveBufferIsMalloced = true;
                    pClient->pReceiveBuffer = (uAtClientReceiveBuffer_t *) malloc(receiveBufferSize);
                }
                if (pClient->pReceiveBuffer != NULL) {
                    pClient->pReceiveBuffer->isMalloced = (int32_t) receiveBufferIsMalloced;
                    // Create the client's mutex
                    if (uPortMutexCreate(&(pClient->mutex)) == 0) {
                        // Create the client's stream mutex
                        if (uPortMutexCreate(&(pClient->streamMutex)) == 0) {
                            // Finally, add an event handler for characters
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
                                    errorCode = uShortRangeEdmStreamAtCallbackSet(streamHandle, urcCallback, pClient,
                                                                                  U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                                                                  U_AT_CLIENT_URC_TASK_PRIORITY);
                                    break;
                                default:
                                    // streamType is checked on entry
                                    break;
                            }
                            if (errorCode == 0) {
                                // Got all "failable" things, now
                                // set all the initial values
                                pClient->streamHandle = streamHandle;
                                pClient->streamType = streamType;
                                pClient->debugOn = false;
                                pClient->printAtOn = false;
                                pClient->atTimeoutMs = U_AT_CLIENT_DEFAULT_TIMEOUT_MS;
                                pClient->atTimeoutSavedMs = -1;
                                pClient->numConsecutiveAtTimeouts = 0;
                                pClient->pConsecutiveTimeoutsCallback = NULL;
                                pClient->delimiter = U_AT_CLIENT_DEFAULT_DELIMITER;
                                pClient->delayMs = U_AT_CLIENT_DEFAULT_DELAY_MS;
                                clearError(pClient);
                                // This will also set stopTag
                                setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
                                pClient->pUrcList = NULL;
                                pClient->lastResponseStopMs = 0;
                                pClient->lockTimeMs = 0;
                                pClient->urcMaxStringLength = U_AT_CLIENT_INITIAL_URC_LENGTH;
                                pClient->maxRespLength = U_AT_CLIENT_MAX_LENGTH_INFORMATION_RESPONSE_PREFIX;
                                pClient->delimiterRequired = false;
                                pClient->pInterceptTx = NULL;
                                pClient->pInterceptRx = NULL;
                                pClient->pNext = NULL;
                                // Finally set up the buffer and its protection markers
                                pClient->pReceiveBuffer->dataBufferSize = receiveBufferSize -
                                                                          U_AT_CLIENT_BUFFER_OVERHEAD_BYTES;
                                bufferReset(pClient);
                                memcpy(pClient->pReceiveBuffer->mk0, U_AT_CLIENT_MARKER,
                                       U_AT_CLIENT_MARKER_SIZE);
                                memcpy(U_AT_CLIENT_DATA_BUFFER_PTR(pClient->pReceiveBuffer) +
                                       pClient->pReceiveBuffer->dataBufferSize,
                                       U_AT_CLIENT_MARKER, U_AT_CLIENT_MARKER_SIZE);
                                // Add the instance to the list
                                addAtClientInstance(pClient);
                            } else {
                                // Couldn't create the stream event callback, delete
                                // the stream and general mutexes, the buffer
                                // (if necessary) and the client.
                                uPortMutexDelete(pClient->streamMutex);
                                uPortMutexDelete(pClient->mutex);
                                if (receiveBufferIsMalloced) {
                                    free(pClient->pReceiveBuffer);
                                }
                                free(pClient);
                                pClient = NULL;
                            }
                        } else {
                            // Couldn't create the stream mutex, delete
                            // the general mutex, the buffer (if necessary)
                            // and the client.
                            uPortMutexDelete(pClient->mutex);
                            if (receiveBufferIsMalloced) {
                                free(pClient->pReceiveBuffer);
                            }
                            free(pClient);
                            pClient = NULL;
                        }
                    } else {
                        // Couldn't create the mutex, delete the
                        // buffer (if necessary) and the client.
                        if (receiveBufferIsMalloced) {
                            free(pClient->pReceiveBuffer);
                        }
                        free(pClient);
                        pClient = NULL;
                    }
                } else {
                    // Don't have a buffer, delete the client.
                    free(pClient);
                    pClient = NULL;
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(gMutex);

    return (uAtClientHandle_t) pClient;
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

    U_PORT_MUTEX_LOCK(pClient->mutex);

    // Try, without blocking, to lock this AT client's
    // stream mutex
    if (uPortMutexTryLock(pClient->streamMutex, 0) == 0) {
        // We were able to lock the stream mutex, we're obviously
        // not currently in a lock, so set the timeout
        // forever and unlock the mutex again
        ((uAtClientInstance_t *) atHandle)->atTimeoutMs = timeoutMs;
        uPortMutexUnlock(pClient->streamMutex);
    } else {
        // We were not able to lock the stream mutex so we must
        // be in a lock.  In this case save the current
        // timeout before changing it so that we can put
        // it back once the stream mutex is unlocked
        ((uAtClientInstance_t *) atHandle)->atTimeoutSavedMs =
            ((uAtClientInstance_t *) atHandle)->atTimeoutMs;
        ((uAtClientInstance_t *) atHandle)->atTimeoutMs = timeoutMs;
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
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

    // IMPORTANT: this can't lock pClient->mutex as it
    // needs to wait on the stream mutex and if it locked
    // pClient->mutex that would prevent uAtClientUnlock()
    // from working.
    if ((pClient != NULL) && pClient->streamMutex != NULL) {
        uPortMutexLock(pClient->streamMutex);
        clearError(pClient);
        pClient->lockTimeMs = uPortGetTickTimeMs();
    }
}

// Unlock the stream and kick off a receive
// if there is some data lounging around.
int32_t uAtClientUnlock(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uErrorCode_t error;
    int32_t sizeBytes;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    unlockNoDataCheck(pClient);

    switch (pClient->streamType) {
        case U_AT_CLIENT_STREAM_TYPE_UART:
            sizeBytes = uPortUartGetReceiveSize(pClient->streamHandle);
            if ((sizeBytes > 0) ||
                (pClient->pReceiveBuffer->readIndex < pClient->pReceiveBuffer->length)) {
                uPortUartEventSend(pClient->streamHandle,
                                   U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
            }
            break;
        case U_AT_CLIENT_STREAM_TYPE_EDM:
            sizeBytes = uShortRangeEdmStreamAtGetReceiveSize(pClient->streamHandle);
            if ((sizeBytes > 0) ||
                (pClient->pReceiveBuffer->readIndex < pClient->pReceiveBuffer->length)) {
                uPortUartEventSend(pClient->streamHandle,
                                   U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED);
            }
            break;
        default:
            break;
    }

    error = pClient->error;
    assert(U_AT_CLIENT_GUARD_CHECK(pClient->pReceiveBuffer));

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return (int32_t) error;
}

// Start an AT command sequence.
void uAtClientCommandStart(uAtClientHandle_t atHandle,
                           const char *pCommand)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int64_t delayMs;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // Wait for delay period if required
        if (pClient->delayMs > 0) {
            delayMs = pClient->lastResponseStopMs + pClient->delayMs -
                      uPortGetTickTimeMs();
            if (delayMs > 0) {
                uPortTaskBlock((int32_t) delayMs);
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

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Write an integer parameter.
void uAtClientWriteInt(uAtClientHandle_t atHandle,
                       int32_t param)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char numberString[12];
    int32_t length;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (writeCheckAndDelimit(pClient)) {
        // Write the integer parameter
        length = snprintf(numberString, sizeof(numberString),
                          "%d", (int) param);
        if ((length > 0) && (length < (int32_t) sizeof(numberString))) {
            // write() will set device error if there's a problem
            write(pClient, numberString, length, false);
        }
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Write a uint64_t parameter.
void uAtClientWriteUint64(uAtClientHandle_t atHandle,
                          uint64_t param)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char numberString[24];
    int32_t length;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (writeCheckAndDelimit(pClient)) {
        // Write the uint64_t parameter
        length = uint64ToString(numberString, sizeof(numberString),
                                param);
        if ((length > 0) && (length < (int32_t) sizeof(numberString))) {
            // write() will set device error if there's a problem
            write(pClient, numberString, length, false);
        }
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Write a string parameter.
void uAtClientWriteString(uAtClientHandle_t atHandle,
                          const char *pParam,
                          bool useQuotations)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

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

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Write a sequence of bytes.
size_t uAtClientWriteBytes(uAtClientHandle_t atHandle,
                           const char *pData,
                           size_t lengthBytes,
                           bool standalone)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    size_t writeLength = 0;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    // Do write check and delimit if required, else
    // just check for errors
    if ((standalone || writeCheckAndDelimit(pClient)) &&
        (pClient->error == U_ERROR_COMMON_SUCCESS)) {
        // write() will set device error if there's a problem
        // If this is a standalone write, do a flush also
        writeLength = write(pClient, pData, lengthBytes, standalone);
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return writeLength;
}

// Stop the outgoing part of an AT command sequence.
void uAtClientCommandStop(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        // Finish by writing the AT command delimiter
        // write() will set device error if there's a problem
        write(pClient, U_AT_CLIENT_COMMAND_DELIMITER,
              U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES,
              true);
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Stop the outgoing part and deal with a simple response also.
void uAtClientCommandStopReadResponse(uAtClientHandle_t atHandle)
{
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, NULL);
    uAtClientResponseStop(atHandle);
}

// Start the response part.
void uAtClientResponseStart(uAtClientHandle_t atHandle,
                            const char *pPrefix)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    bool prefixMatched;

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
        }
    }
}

// Read an integer parameter.
int32_t uAtClientReadInt(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t integerRead;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    integerRead = readInt(pClient);

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return integerRead;
}

// Read a uint64_t parameter.
int32_t uAtClientReadUint64(uAtClientHandle_t atHandle,
                            uint64_t *pUint64)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    char buffer[32]; // Enough for an integer
    int32_t returnValue = -1;

    U_PORT_MUTEX_LOCK(pClient->mutex);

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

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

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

    U_PORT_MUTEX_LOCK(pClient->mutex);

    lengthRead = readString(pClient, pString, lengthBytes, ignoreStopTag);

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

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

    U_PORT_MUTEX_LOCK(pClient->mutex);

    while ((lengthRead < ((int32_t) lengthBytes + matchPos)) &&
           (pClient->error == U_ERROR_COMMON_SUCCESS) &&
           !pStopTag->found) {
        c = bufferReadChar(pClient);
        if (c == -1) {
            // Error
            setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
        } else {
            if ((pStopTag->pTagDef->length > 0) &&
                (c == *(pStopTag->pTagDef->pString + matchPos))) {
                // It could be a stop tag
                matchPos++;
                if (matchPos == pStopTag->pTagDef->length) {
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
            } else if ((pStopTag->pTagDef->length > 0) &&
                       (c == *(pStopTag->pTagDef->pString + matchPos))) {
                matchPos++;
                if (matchPos == pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                }
            }
        }
    }

    if (pClient->error != U_ERROR_COMMON_SUCCESS) {
        lengthRead = -1;
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return lengthRead;
}

// Stop the response part of an AT sequence.
void uAtClientResponseStop(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pClient->scope == U_AT_CLIENT_SCOPE_INFORMATION) {
        informationResponseStop(pClient);
    }

    // Consume up to the response stop tag
    if (consumeToStopTag(pClient)) {
        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
    }

    pClient->lastResponseStopMs = uPortGetTickTimeMs();

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Switch off stop tag detection.
void uAtClientIgnoreStopTag(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pClient->error == U_ERROR_COMMON_SUCCESS) {
        setScope(pClient, U_AT_CLIENT_SCOPE_NONE);
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
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

    U_PORT_MUTEX_LOCK(pClient->mutex);

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
                       (pStopTag->pTagDef->length > 0) &&
                       (c == *(pStopTag->pTagDef->pString + matchPos))) {
                // It could be a stop tag
                matchPos++;
                if (matchPos == pStopTag->pTagDef->length) {
                    pStopTag->found = true;
                }
            } else {
                matchPos = 0;
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Skip the given number of bytes.
void uAtClientSkipBytes(uAtClientHandle_t atHandle,
                        size_t lengthBytes)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    int32_t c;

    U_PORT_MUTEX_LOCK(pClient->mutex);

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

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Wait for a single character to arrive.
int32_t uAtClientWaitCharacter(uAtClientHandle_t atHandle,
                               char character)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uErrorCode_t errorCode = U_ERROR_COMMON_UNKNOWN;
    int32_t c = -1;

    // IMPORTANT: this can't lock pClient->mutex as it
    // checks for URCs and hence may end up calling a
    // URC handler which itself will need to be able
    // to perform a lock.

    if (!pClient->stopTag.found) {
        while ((pClient->error == U_ERROR_COMMON_SUCCESS) &&
               (c != character)) {
            c = bufferReadChar(pClient);
            if (c == -1) {
                setError(pClient, U_ERROR_COMMON_DEVICE_ERROR);
            }
            // Continue to look for URCs,
            // you never know when the sneaky
            // buggers might turn up.
            bufferMatchOneUrc(pClient);
            // Check for a device error landing
            // in the buffer
            deviceErrorInBuffer(pClient);
        }

        if (c == character) {
            errorCode = U_ERROR_COMMON_SUCCESS;
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
    uAtClientUrc_t *pUrc;
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    size_t prefixLength;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if ((pPrefix != NULL) && (pHandler != NULL)) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        if (!findUrcHandler(pClient, pPrefix)) {
            pUrc = (uAtClientUrc_t *) malloc(sizeof(uAtClientUrc_t));
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
                pUrc->pNext = pClient->pUrcList;
                pClient->pUrcList = pUrc;
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

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return (int32_t) errorCode;
}

// Remove a URC handler.
void uAtClientRemoveUrcHandler(uAtClientHandle_t atHandle,
                               const char *pPrefix)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uAtClientUrc_t *pCurrent = pClient->pUrcList;
    uAtClientUrc_t *pPrev = NULL;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    while (pCurrent != NULL) {
        if (strcmp(pPrefix, pCurrent->pPrefix) == 0) {
            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                pClient->pUrcList = pCurrent->pNext;
            }
            free(pCurrent);
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
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
            stackMinFree = uPortShortRangeEdmStremAtEventStackMinFree(pClient->streamHandle);
            break;
        default:
            break;
    }

    return stackMinFree;
}

// Make a callback resulting from a URC.
//lint -esym(593, pCallbackParam) Suppress pCallbackParam not being
// free()ed here: if Lint spots that pCallbackParam was malloc()ed
// by the caller it can complain that it was not free()ed here.
int32_t uAtClientCallback(uAtClientHandle_t atHandle,
                          void (*pCallback) (uAtClientHandle_t, void *),
                          void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientCallback_t cb;

    U_PORT_MUTEX_LOCK(gMutex);

    if (pCallback != NULL) {
        cb.pFunction = pCallback;
        cb.atHandle = atHandle;
        cb.pParam = pCallbackParam;
        errorCode = uPortEventQueueSend(gEventQueueHandle, &cb, sizeof(cb));
    }

    U_PORT_MUTEX_UNLOCK(gMutex);

    return errorCode;
}

// Get the stack high watermark for the AT callback task
int32_t uAtClientCallbackStackMinFree()
{
    int32_t sizeOrErrorCode;

    U_PORT_MUTEX_LOCK(gMutex);

    sizeOrErrorCode = uPortEventQueueStackMinFree(gEventQueueHandle);

    U_PORT_MUTEX_UNLOCK(gMutex);

    return sizeOrErrorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Flush the receive buffer
void uAtClientFlush(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pClient->debugOn) {
        uPortLog("U_AT_CLIENT_%d-%d: flush.\n", pClient->streamType,
                 pClient->streamHandle);
    }

    bufferReset(pClient);
    while (bufferFill(pClient, false)) {
        bufferReset(pClient);
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Clear the error status to none.
void uAtClientClearError(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    clearError(pClient);

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Get the error status
int32_t uAtClientErrorGet(uAtClientHandle_t atHandle)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;
    uErrorCode_t error;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    error = pClient->error;

    U_PORT_MUTEX_UNLOCK(pClient->mutex);

    return (int32_t) error;
}

// Get the device error status (i.e. from CMS ERROR or
// CME ERROR).
void uAtClientDeviceErrorGet(uAtClientHandle_t atHandle,
                             uAtClientDeviceError_t *pDeviceError)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    if (pDeviceError != NULL) {
        memcpy(pDeviceError, &(pClient->deviceError),
               sizeof(*pDeviceError));
    }

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
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
                                                          const char *,
                                                          size_t *,
                                                          void *),
                                void *pContext)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    pClient->pInterceptTx = pCallback;
    pClient->pInterceptTxContext = pContext;

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// Add a receive intercept function.
void uAtClientStreamInterceptRx(uAtClientHandle_t atHandle,
                                char *(*pCallback) (uAtClientHandle_t,
                                                    char *,
                                                    size_t *,
                                                    void *),
                                void *pContext)
{
    uAtClientInstance_t *pClient = (uAtClientInstance_t *) atHandle;

    U_PORT_MUTEX_LOCK(pClient->mutex);

    pClient->pInterceptRx = pCallback;
    pClient->pInterceptRxContext = pContext;

    U_PORT_MUTEX_UNLOCK(pClient->mutex);
}

// End of file
