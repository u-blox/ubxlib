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
#include "stdlib.h"    // malloc() and free()
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // For U_INLINE

#include "u_error_common.h"
#include "u_assert.h"

#include "u_port_os.h"

#include "u_ringbuffer.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static U_INLINE size_t ptrDiff(const char *pRead, char *pWrite, size_t bufferSize)
{
    size_t size = 0;

    if (pWrite >= pRead) {
        size = pWrite - pRead;
    } else {
        size = bufferSize - (pRead - pWrite);
    }

    return size;
}

static U_INLINE const char *pPtrInc(const char *pData, const char *pBuffer, size_t bufferSize)
{
    pData++;
    if (pData >= pBuffer + bufferSize) {
        pData = pBuffer;
    }

    return pData;
}

// The ring buffer's mutex should be locked before this is called
static void bufferReset(uRingBuffer_t *pRingBuffer)
{
    U_ASSERT(pRingBuffer->maxNumReadPointers > 0);
    U_ASSERT(pRingBuffer->dataRead != NULL);
    U_ASSERT(pRingBuffer->pBuffer != NULL);

    for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
        if (pRingBuffer->dataRead[x] != NULL) {
            pRingBuffer->dataRead[x] = pRingBuffer->pBuffer;
        }
    }
    pRingBuffer->pDataWrite = pRingBuffer->pBuffer;
    // The default handle-less read pointer can always be set
    pRingBuffer->dataRead[0] = pRingBuffer->pDataWrite;
}

static int32_t createCommon(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size)
{
    pRingBuffer->pBuffer = pLinearBuffer;
    pRingBuffer->size = size;
    bufferReset(pRingBuffer);
    return uPortMutexCreate((uPortMutexHandle_t *) &pRingBuffer->mutex);
}

// The ring buffer's mutex should be locked before this is called
static size_t read(uRingBuffer_t *pRingBuffer, int32_t handle, char *pData, size_t length)
{
    size_t bytesRead = 0;
    size_t available;

    if ((handle >= 0) && (handle < (int32_t) pRingBuffer->maxNumReadPointers) &&
        (pRingBuffer->dataRead[handle] != NULL)) {

        available = ptrDiff(pRingBuffer->dataRead[handle], pRingBuffer->pDataWrite, pRingBuffer->size);
        if (length > available) {
            length = available;
        }

        while (bytesRead < length) {
            if (pData != NULL) {
                *pData = *pRingBuffer->dataRead[handle];
            }
            pRingBuffer->dataRead[handle] = pPtrInc(pRingBuffer->dataRead[handle],
                                                    pRingBuffer->pBuffer, pRingBuffer->size);
            pData++;
            bytesRead++;
        }
    }

    return bytesRead;
}

// The ring buffer's mutex should be locked before this is called
static bool add(uRingBuffer_t *pRingBuffer, const char *pData,
                size_t length, bool destructive)
{
    bool dataFitsInBuffer = true;
    size_t used;

    if (length >= pRingBuffer->size) {
        dataFitsInBuffer = false;
    } else {
        for (size_t x = 0; (x < pRingBuffer->maxNumReadPointers) &&
             (dataFitsInBuffer || destructive); x++) {
            if (pRingBuffer->dataRead[x] != NULL) {
                used = ptrDiff(pRingBuffer->dataRead[x], pRingBuffer->pDataWrite, pRingBuffer->size);
                used++; // Account for the fact that we can't have the pointers overlap
                if (used + length > pRingBuffer->size) {
                    // If a read handle is required we must always be destructive
                    // to the data behind the "normal" read pointer or we
                    // will get stuck
                    if (destructive ||
                        ((x == 0) && pRingBuffer->readHandleRequired)) {
                        // Throw away enough to make it fit
                        read(pRingBuffer, x, NULL, used + length - pRingBuffer->size);
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
    }

    return dataFitsInBuffer;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uRingBufferCreate(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size)
{
    memset(pRingBuffer, 0x00, sizeof(uRingBuffer_t));
    // No handlers in this case, just use the storage
    // at pDataReadNormal
    pRingBuffer->dataRead = &(pRingBuffer->pDataReadNormal);
    pRingBuffer->maxNumReadPointers = 1;
    pRingBuffer->isMalloced = false;
    return createCommon(pRingBuffer, pLinearBuffer, size);
}

void uRingBufferDelete(uRingBuffer_t *pRingBuffer)
{
    if ((pRingBuffer != NULL) && (pRingBuffer->mutex != NULL)) {
        if (pRingBuffer->isMalloced) {
            free(pRingBuffer->dataRead);
            pRingBuffer->dataRead = NULL;
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

        bytesRead = read(pRingBuffer, 0, pData, length);

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
            dataSize = ptrDiff(pRingBuffer->dataRead[0], pRingBuffer->pDataWrite, pRingBuffer->size);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataSize;
}

size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer)
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
            if ((pRingBuffer->dataRead[x] != NULL) &&
                ((x > 0) || !pRingBuffer->readHandleRequired)) {
                y = pRingBuffer->size - ptrDiff(pRingBuffer->dataRead[x], pRingBuffer->pDataWrite,
                                                pRingBuffer->size);
                if (y < size) {
                    size = y;
                }
                foundADataReadPointer = true;
            }
        }

        if (!foundADataReadPointer) {
            // If we didn't find a single data read pointer,
            // report what is in the buffer anyway
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

void uRingBufferReset(uRingBuffer_t *pRingBuffer)
{
    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bufferReset(pRingBuffer);

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

int32_t uRingBufferCreateWithReadHandle(uRingBuffer_t *pRingBuffer, char *pLinearBuffer,
                                        size_t size,  size_t maxNumReadHandles)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;

    memset(pRingBuffer, 0x00, sizeof(uRingBuffer_t));
    maxNumReadHandles++; // Add one more for the non-handled read
    pRingBuffer->dataRead = (const char **) malloc((maxNumReadHandles) * sizeof(const char *));
    if (pRingBuffer->dataRead != NULL) {
        pRingBuffer->isMalloced = true;
        pRingBuffer->maxNumReadPointers = maxNumReadHandles;
        for (size_t x = 0; x < pRingBuffer->maxNumReadPointers; x++) {
            pRingBuffer->dataRead[x] = NULL;
        }
        errorCode = createCommon(pRingBuffer, pLinearBuffer, size);
        if (errorCode != 0) {
            free(pRingBuffer->dataRead);
            pRingBuffer->dataRead = NULL;
            pRingBuffer->maxNumReadPointers = 0;
        }
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
            pRingBuffer->dataRead[0] = pRingBuffer->pDataWrite;
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
            if (pRingBuffer->dataRead[x] == NULL) {
                pRingBuffer->dataRead[x] = pRingBuffer->pDataWrite;
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
            pRingBuffer->dataRead[handle] = NULL;
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }
}

size_t uRingBufferReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                             char *pData, size_t length)
{
    size_t bytesRead = 0;

    if (pRingBuffer->pBuffer != NULL) {

        U_PORT_MUTEX_LOCK((uPortMutexHandle_t) pRingBuffer->mutex);

        bytesRead = read(pRingBuffer, handle, pData, length);

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
            (pRingBuffer->dataRead[handle] != NULL)) {
            dataSize = ptrDiff(pRingBuffer->dataRead[handle], pRingBuffer->pDataWrite, pRingBuffer->size);
        }

        U_PORT_MUTEX_UNLOCK((uPortMutexHandle_t) pRingBuffer->mutex);
    }

    return dataSize;
}

// End of file
