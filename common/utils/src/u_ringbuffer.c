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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the ring buffer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "string.h"
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"    // snprintf()

#include "u_cfg_sw.h"
#include "u_compiler.h" // For U_INLINE

#include "u_error_common.h"
#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_ringbuffer.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of the debug prints.
 */
#define U_RINGBUFFER_PREFIX "U_RINGBUFFER: "

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Parsing context.
 */
typedef struct {
    uRingBuffer_t *pRingBuffer;
    const char *pSource;
    size_t bytesAvailable;
    size_t bytesParsed;
    size_t bytesDiscard;
} uRingBufferParseContext_t;

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static U_INLINE size_t ptrDiff(const char *pRead, char *pWrite,
                               size_t bufferSize)
{
    size_t size = 0;

    if (pWrite >= pRead) {
        size = pWrite - pRead;
    } else {
        size = bufferSize - (pRead - pWrite);
    }

    return size;
}

static U_INLINE const char *pPtrInc(const char *pData, const char *pBuffer,
                                    size_t bufferSize)
{
    pData++;
    if (pData >= pBuffer + bufferSize) {
        pData = pBuffer;
    }

    return pData;
}

static U_INLINE const char *pPtrOffset(const char *pData, size_t offset,
                                       const char *pBuffer, size_t bufferSize)
{
    pData += offset;
    if (pData >= pBuffer + bufferSize) {
        pData -= bufferSize;
    }

    return pData;
}

// The ring buffer's mutex should be locked before this is called
static void bufferReset(uRingBuffer_t *pRingBuffer)
{
    U_ASSERT(pRingBuffer->maxNumReadPointers > 0);
    U_ASSERT(pRingBuffer->pDataRead != NULL);
    U_ASSERT(pRingBuffer->pBuffer != NULL);

    for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
        if (pRingBuffer->pDataRead[x] != NULL) {
            pRingBuffer->pDataRead[x] = pRingBuffer->pBuffer;
        }
    }
    pRingBuffer->pDataWrite = pRingBuffer->pBuffer;
    // The default handle-less read pointer can always be set
    pRingBuffer->pDataRead[0] = pRingBuffer->pDataWrite;
}

static int32_t createCommon(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size)
{
    pRingBuffer->pBuffer = pLinearBuffer;
    pRingBuffer->size = size;
    bufferReset(pRingBuffer);
    return uPortMutexCreate((uPortMutexHandle_t *) &pRingBuffer->mutex);
}

// The ring buffer's mutex should be locked before this is called
static size_t read(uRingBuffer_t *pRingBuffer, int32_t handle, char *pData,
                   size_t length, size_t offset, bool destructive)
{
    size_t bytesRead = 0;
    size_t available;
    const char *pSource;

    if ((handle >= 0) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
        (pRingBuffer->pDataRead[handle] != NULL)) {

        pSource = pPtrOffset(pRingBuffer->pDataRead[handle], offset,
                             pRingBuffer->pBuffer, pRingBuffer->size);
        available = ptrDiff(pSource, pRingBuffer->pDataWrite, pRingBuffer->size);
        if (length > available) {
            length = available;
        }

        while (bytesRead < length) {
            if (pData != NULL) {
                *pData = *pSource;
                pData++;
            }
            pSource = pPtrInc(pSource, pRingBuffer->pBuffer, pRingBuffer->size);
            bytesRead++;
        }
        if (destructive) {
            pRingBuffer->pDataRead[handle] = pSource;
        }
    }

    return bytesRead;
}

// The ring buffer's mutex should be locked before this is called
static bool add(uRingBuffer_t *pRingBuffer, const char *pData,
                size_t length, bool destructive)
{
    bool dataFitsInBuffer = true;
    size_t lost;
    size_t used;

    if (length >= pRingBuffer->size) {
        dataFitsInBuffer = false;
    } else {
        for (size_t x = 0; (x < pRingBuffer->maxNumReadPointers) &&
             (dataFitsInBuffer || destructive); x++) {
            if (pRingBuffer->pDataRead[x] != NULL) {
                used = ptrDiff(pRingBuffer->pDataRead[x], pRingBuffer->pDataWrite, pRingBuffer->size);
                used++; // Account for the fact that we can't have the pointers overlap
                if (used + length > pRingBuffer->size) {
                    // If we're on the "normal" read pointer (0) and it can't be used (because
                    // of the readHandleRequired flag) OR we are being destructive (so a
                    // forced add) and this data read pointer is not locked, then we
                    // throw away enough data to make it fit.
                    if (((x == 0) && pRingBuffer->readHandleRequired) ||
                        (destructive && ((x == 0) || (pRingBuffer->dataReadLockBitmap & (1ULL << (x - 1))) == 0))) {
                        lost = read(pRingBuffer, x, NULL, used + length - pRingBuffer->size, 0, true);
                        if (x == 0) {
                            pRingBuffer->statReadLossNormalBytes += lost;
                        } else {
                            pRingBuffer->statReadLossBytes[x] += lost;
                        }
                    } else {
                        dataFitsInBuffer = false;
                    }
                }
            }
        }
    }

    if (dataFitsInBuffer) {
        while (length > 0) {
            *(pRingBuffer->pDataWrite) = *pData;
            pRingBuffer->pDataWrite = (char *) pPtrInc(pRingBuffer->pDataWrite, pRingBuffer->pBuffer,
                                                       pRingBuffer->size);
            length--;
            pData++;
        }
    } else {
        pRingBuffer->statAddLossBytes += length;
    }

    return dataFitsInBuffer;
}

// This function does the ring buffer mutex locking itself.
static size_t lock(uRingBuffer_t *pRingBuffer, int32_t handle, bool lockNotUnlock)
{
    size_t dataSize = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers)) {
            if (lockNotUnlock) {
                pRingBuffer->dataReadLockBitmap |= 1ULL << (handle - 1);
                dataSize = ptrDiff(pRingBuffer->pDataRead[handle], pRingBuffer->pDataWrite, pRingBuffer->size);
            } else {
                pRingBuffer->dataReadLockBitmap &= ~(1ULL << (handle - 1));
            }
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataSize;
}

// This function does the ring buffer mutex locking itself.
static size_t availableSize(const uRingBuffer_t *pRingBuffer, bool max)
{
    size_t size = 0;
    size_t y = 0;
    bool foundADataReadPointer = false;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        size = pRingBuffer->size;
        for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
            // If a read handle is required we ignore the data behind
            // the "normal" read pointer as it's not possible to get
            // at it
            if ((pRingBuffer->pDataRead[x] != NULL) &&
                ((x > 0) || !pRingBuffer->readHandleRequired)) {
                // If we're doing max then we only take into account
                // locked data buffer pointers and we ignore 0 since
                // it is not lockable
                if (!max || ((x > 0) && (pRingBuffer->dataReadLockBitmap & (1ULL << (x - 1))))) {
                    y = pRingBuffer->size - ptrDiff(pRingBuffer->pDataRead[x], pRingBuffer->pDataWrite,
                                                    pRingBuffer->size);
                    if (y < size) {
                        size = y;
                    }
                    foundADataReadPointer = true;
                }
            }
        }

        if (!max && !foundADataReadPointer) {
            // If we didn't find a single data read pointer,
            // and we're not doing max, report what is in the
            // buffer anyway
            size = pRingBuffer->size - ptrDiff(pRingBuffer->pBuffer, pRingBuffer->pDataWrite,
                                               pRingBuffer->size);
        }
        if (size > 0) {
            //  Must keep one to prevent pointer wrap
            size--;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return size;
}

// Hex print for debug purposes.
static void printHex(const char *pBuffer, size_t size)
{
    uPortLog(U_RINGBUFFER_PREFIX "buffer contents 0x%08x %6d byte(s):", pBuffer, size);
    for (size_t x = 0; x < size; x++) {
        uPortLog(" %02x", *pBuffer);
        pBuffer++;
    }
    uPortLog("\n");
}

// Print a buffer pointer for debug purposes.
static void printPointer(const char *pTitle, const char *pBuffer, size_t bufferSize,
                         const char *pStart, size_t size, const char *pTwoCharStr)
{
    const char *pTmp = pBuffer;
    size_t startFillCount = 0;

    if (pStart + size > pBuffer + bufferSize) {
        startFillCount = pStart + size - (pBuffer + bufferSize);
    }

    uPortLog(U_RINGBUFFER_PREFIX "%15s 0x%08x %6d byte(s):", pTitle, pStart, size);
    size -= startFillCount;
    while (startFillCount > 0) {
        uPortLog(" %.2s", pTwoCharStr);
        pTmp++;
        startFillCount--;
    }
    while (pTmp < pStart) {
        uPortLog("   ");
        pTmp++;
    }
    while (size > 0) {
        uPortLog(" %.2s", pTwoCharStr);
        size--;
    }
    uPortLog("\n");
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: DEBUG
 * -------------------------------------------------------------- */

// Dump the status of a ring buffer.
void uRingBufferDump(uRingBuffer_t *pRingBuffer)
{
    size_t freeMin = pRingBuffer->size;
    size_t y;
    char buffer1[3];
    char buffer2[16];

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((pRingBuffer != NULL) && (pRingBuffer->mutex != NULL)) {
            printHex(pRingBuffer->pBuffer, pRingBuffer->size);
        }
        for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
            if (pRingBuffer->pDataRead[x] != NULL) {
                snprintf(buffer1, sizeof(buffer1), "%02d", x);
                snprintf(buffer2, sizeof(buffer2), "read handle %s", buffer1);
                y = ptrDiff(pRingBuffer->pDataRead[x], pRingBuffer->pDataWrite,
                            pRingBuffer->size);
                printPointer(buffer2, pRingBuffer->pBuffer, pRingBuffer->size,
                             pRingBuffer->pDataRead[x], y, buffer1);
                y = pRingBuffer->size - y;
                if (y < freeMin) {
                    freeMin = y;
                }
            }
        }

        freeMin--;  // Account for there being no overlap
        printPointer("free", pRingBuffer->pBuffer, pRingBuffer->size,
                     pRingBuffer->pDataWrite, freeMin, "www");

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: BASIC
 * -------------------------------------------------------------- */

int32_t uRingBufferCreate(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size)
{
    memset(pRingBuffer, 0x00, sizeof(uRingBuffer_t));
    // No handlers in this case, just use the storage
    // at pDataReadNormal
    pRingBuffer->pDataRead = &(pRingBuffer->pDataReadNormal);
    pRingBuffer->maxNumReadPointers = 1;
    pRingBuffer->isMalloced = false;
    return createCommon(pRingBuffer, pLinearBuffer, size);
}

void uRingBufferDelete(uRingBuffer_t *pRingBuffer)
{
    if ((pRingBuffer != NULL) && (pRingBuffer->mutex != NULL)) {
        if (pRingBuffer->isMalloced) {
            uPortFree(pRingBuffer->pDataRead);
            pRingBuffer->pDataRead = NULL;
            uPortFree(pRingBuffer->statReadLossBytes);
            pRingBuffer->statReadLossBytes = NULL;
        }
        pRingBuffer->maxNumReadPointers = 0;
        uPortMutexDelete((uPortMutexHandle_t) pRingBuffer->mutex);
        pRingBuffer->mutex = NULL;
        pRingBuffer->pBuffer = NULL;
    }
}

bool uRingBufferAdd(uRingBuffer_t *pRingBuffer, const char *pData, size_t length)
{
    bool dataFitsInBuffer = false;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        dataFitsInBuffer = add(pRingBuffer, pData, length, false);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataFitsInBuffer;
}

bool uRingBufferForceAdd(uRingBuffer_t *pRingBuffer, const char *pData, size_t length)
{
    bool dataFitsInBuffer = false;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        dataFitsInBuffer = add(pRingBuffer, pData, length, true);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataFitsInBuffer;
}

size_t uRingBufferRead(uRingBuffer_t *pRingBuffer, char *pData, size_t length)
{
    size_t bytesRead = 0;

    if ((pRingBuffer->pBuffer != NULL) && !pRingBuffer->readHandleRequired) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesRead = read(pRingBuffer, 0, pData, length, 0, true);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesRead;
}

size_t uRingBufferPeek(uRingBuffer_t *pRingBuffer, char *pData, size_t length,
                       size_t offset)
{
    size_t bytesRead = 0;

    if ((pRingBuffer->pBuffer != NULL) && !pRingBuffer->readHandleRequired) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesRead = read(pRingBuffer, 0, pData, length, offset, false);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesRead;
}

size_t uRingBufferDataSize(const uRingBuffer_t *pRingBuffer)
{
    size_t dataSize = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if (!pRingBuffer->readHandleRequired) {
            // Only report if the non-handled read can be used
            dataSize = ptrDiff(pRingBuffer->pDataRead[0], pRingBuffer->pDataWrite, pRingBuffer->size);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataSize;
}

size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer)
{
    return availableSize(pRingBuffer, false);
}

void uRingBufferFlush(uRingBuffer_t *pRingBuffer)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        pRingBuffer->pDataRead[0] = pRingBuffer->pDataWrite;

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

void uRingBufferReset(uRingBuffer_t *pRingBuffer)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bufferReset(pRingBuffer);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

size_t uRingBufferStatReadLoss(uRingBuffer_t *pRingBuffer)
{
    size_t bytesLost = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesLost = pRingBuffer->statReadLossNormalBytes;

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesLost;
}

size_t uRingBufferStatAddLoss(uRingBuffer_t *pRingBuffer)
{
    size_t bytesLost = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesLost = pRingBuffer->statAddLossBytes;

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesLost;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MULTIPLE READERS
 * -------------------------------------------------------------- */

int32_t uRingBufferCreateWithReadHandle(uRingBuffer_t *pRingBuffer, char *pLinearBuffer,
                                        size_t size,  size_t maxNumReadHandles)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;

    memset(pRingBuffer, 0x00, sizeof(uRingBuffer_t));
    maxNumReadHandles++; // Add one more for the non-handled read
    pRingBuffer->pDataRead = (const char **) pUPortMalloc((maxNumReadHandles) * sizeof(const char *));
    pRingBuffer->statReadLossBytes = (size_t *) pUPortMalloc((maxNumReadHandles) * sizeof(size_t));
    if ((pRingBuffer->pDataRead != NULL) && (pRingBuffer->statReadLossBytes != NULL) &&
        (maxNumReadHandles < (sizeof(pRingBuffer->dataReadLockBitmap) * 8))) {
        pRingBuffer->isMalloced = true;
        pRingBuffer->maxNumReadPointers = maxNumReadHandles;
        for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
            pRingBuffer->pDataRead[x] = NULL;
            pRingBuffer->statReadLossBytes[x] = 0;
        }
        errorCode = createCommon(pRingBuffer, pLinearBuffer, size);
    }
    if (errorCode != 0) {
        uPortFree(pRingBuffer->pDataRead);
        pRingBuffer->pDataRead = NULL;
        uPortFree(pRingBuffer->statReadLossBytes);
        pRingBuffer->statReadLossBytes = NULL;
        pRingBuffer->maxNumReadPointers = 0;
    }

    return errorCode;
}

void uRingBufferSetReadRequiresHandle(uRingBuffer_t *pRingBuffer, bool onNotOff)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if (pRingBuffer->readHandleRequired && !onNotOff) {
            // If the setting was on and we're switching
            // it off, set the non-handled read
            // pointer so that it gets sensible data
            pRingBuffer->pDataRead[0] = pRingBuffer->pDataWrite;
        }
        pRingBuffer->readHandleRequired = onNotOff;

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

}

bool uRingBufferGetReadRequiresHandle(uRingBuffer_t *pRingBuffer)
{
    return pRingBuffer->readHandleRequired;
}

int32_t uRingBufferTakeReadHandle(uRingBuffer_t *pRingBuffer)
{
    int32_t readHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        // Leave out the zeroth entry, which is reserved for
        // un-handled reads
        for (size_t x = 1; (x < pRingBuffer->maxNumReadPointers) &&
             (readHandle < 0); x++) {
            if (pRingBuffer->pDataRead[x] == NULL) {
                pRingBuffer->pDataRead[x] = pRingBuffer->pDataWrite;
                pRingBuffer->statReadLossBytes[x] = 0;
                readHandle = x;
            }
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return readHandle;
}

void uRingBufferGiveReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers)) {
            pRingBuffer->pDataRead[handle] = NULL;
            pRingBuffer->dataReadLockBitmap &= ~(1ULL << (handle - 1));
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

// Lock a read handle.
size_t uRingBufferLockReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle)
{
    return lock(pRingBuffer, handle, true);
}

// Unlock a read handle.
void uRingBufferUnlockReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle)
{
    lock(pRingBuffer, handle, false);
}

// Determine whether a read handle is locked or not.
bool uRingBufferReadHandleIsLocked(uRingBuffer_t *pRingBuffer, int32_t handle)
{
    bool isLocked = false;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
            (pRingBuffer->dataReadLockBitmap & (1ULL << (handle - 1)))) {
            isLocked = true;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return isLocked;
}

size_t uRingBufferReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                             char *pData, size_t length)
{
    size_t bytesRead = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesRead = read(pRingBuffer, handle, pData, length, 0, true);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesRead;
}

size_t uRingBufferPeekHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                             char *pData, size_t length, size_t offset)
{
    size_t bytesRead = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesRead = read(pRingBuffer, handle, pData, length, offset, false);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesRead;
}

size_t uRingBufferDataSizeHandle(const uRingBuffer_t *pRingBuffer, int32_t handle)
{
    size_t dataSize = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
            (pRingBuffer->pDataRead[handle] != NULL)) {
            dataSize = ptrDiff(pRingBuffer->pDataRead[handle], pRingBuffer->pDataWrite, pRingBuffer->size);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataSize;
}

size_t uRingBufferAvailableSizeMax(const uRingBuffer_t *pRingBuffer)
{
    return availableSize(pRingBuffer, true);
}

void uRingBufferFlushHandle(uRingBuffer_t *pRingBuffer, int32_t handle)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
            (pRingBuffer->pDataRead[handle] != NULL)) {
            pRingBuffer->pDataRead[handle] = pRingBuffer->pDataWrite;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

size_t uRingBufferStatReadLossHandle(uRingBuffer_t *pRingBuffer,
                                     int32_t handle)
{
    size_t bytesLost = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 1) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
            (pRingBuffer->pDataRead[handle] != NULL)) {
            bytesLost = pRingBuffer->statReadLossBytes[handle];
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return bytesLost;
}

/* ----------------------------------------------------------------
 * FUNCTIONS: PARSER
 * -------------------------------------------------------------- */

size_t uRingBufferParseHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                              U_RING_BUFFER_PARSER_f *pParserList, void *pUserParam)
{
    size_t errorCodeOrLength = U_ERROR_COMMON_INVALID_PARAMETER;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        if ((handle >= 0) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
            (pRingBuffer->pDataRead[handle] != NULL)) {
            const char *pOffset = pPtrOffset(pRingBuffer->pDataRead[handle], 0, pRingBuffer->pBuffer,
                                             pRingBuffer->size);
            size_t bytesAvailable = ptrDiff(pOffset, pRingBuffer->pDataWrite, pRingBuffer->size);
            size_t bytesDiscard  = 0;
            errorCodeOrLength = U_ERROR_COMMON_TIMEOUT;
            while (bytesAvailable) {
                U_RING_BUFFER_PARSER_f *pParser = pParserList;
                // find the right protocol
                errorCodeOrLength = U_ERROR_COMMON_NOT_FOUND;
                while (*pParser) {
                    uRingBufferParseContext_t ctx = {
                        .pRingBuffer    = pRingBuffer,
                        .pSource        = pOffset,
                        .bytesAvailable = bytesAvailable,
                        .bytesParsed    = 0,
                        .bytesDiscard   = bytesDiscard
                    };
                    errorCodeOrLength = (*pParser)(&ctx, pUserParam);
                    pParser ++;
                    if (errorCodeOrLength == U_ERROR_COMMON_SUCCESS) {
                        errorCodeOrLength = ctx.bytesParsed;
                    }
                    if (errorCodeOrLength != U_ERROR_COMMON_NOT_FOUND) {
                        break;
                    }
                }
                if (errorCodeOrLength != U_ERROR_COMMON_NOT_FOUND) {
                    break;
                }
                pOffset = pPtrInc(pOffset, pRingBuffer->pBuffer, pRingBuffer->size);
                bytesDiscard ++;
                bytesAvailable --;
            }
            if (bytesDiscard > 0) {
                errorCodeOrLength = bytesDiscard;
            }
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return errorCodeOrLength;
}


bool uRingBufferGetByteUnprotected(uParseHandle_t parseHandle, void *p)
{
    uRingBufferParseContext_t *pCtx = (uRingBufferParseContext_t *)parseHandle;
    if (1 > pCtx->bytesAvailable) {
        return false;
    }
    char *pDest = (char *)p;
    *pDest = *pCtx->pSource;
    pCtx->pSource = pPtrInc(pCtx->pSource, pCtx->pRingBuffer->pBuffer, pCtx->pRingBuffer->size);
    pCtx->bytesParsed ++;
    pCtx->bytesAvailable --;
    return true;
}

size_t uRingBufferBytesAvailableUnprotected(uParseHandle_t parseHandle)
{
    uRingBufferParseContext_t *pCtx = (uRingBufferParseContext_t *)parseHandle;
    return pCtx->bytesAvailable;
}

size_t uRingBufferBytesDiscardUnprotected(uParseHandle_t parseHandle)
{
    uRingBufferParseContext_t *pCtx = (uRingBufferParseContext_t *)parseHandle;
    return pCtx->bytesDiscard;
}

// End of file
