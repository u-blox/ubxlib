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
 * @brief Implementation of the ring buffer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "string.h"
#include "stdlib.h"    // malloc() and free()
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

void uRingBufferCreate(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size)
{
    memset(pRingBuffer, 0x00, sizeof (uRingBuffer_t));
    pRingBuffer->pBuffer = pLinearBuffer;
    pRingBuffer->size = size;
    uPortMutexCreate(&(pRingBuffer->mutex));
}

void uRingBufferDelete(uRingBuffer_t *pRingBuffer)
{
    if ((pRingBuffer != NULL) && (pRingBuffer->mutex != NULL)) {
        uPortMutexDelete(pRingBuffer->mutex);
        pRingBuffer->pBuffer = NULL;
        pRingBuffer->mutex = NULL;
    }
}

bool uRingBufferAdd(uRingBuffer_t *pRingBuffer, const char *pData, size_t length)
{
    bool dataFitsInBuffer = true;

    if (pRingBuffer->pBuffer != NULL) {
        U_PORT_MUTEX_LOCK(pRingBuffer->mutex);
        if (pRingBuffer->dataSize + length > pRingBuffer->size) {
            dataFitsInBuffer = false;
        }

        if (dataFitsInBuffer) {
            pRingBuffer->dataSize += length;
            while (length-- > 0) {
                pRingBuffer->pBuffer[pRingBuffer->dataIndex++] = *pData++;
                if (pRingBuffer->dataIndex == pRingBuffer->size) {
                    pRingBuffer->dataIndex = 0;
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(pRingBuffer->mutex);
    }

    return dataFitsInBuffer;
}

size_t uRingBufferRead(uRingBuffer_t *pRingBuffer, char *pData, size_t length)
{
    size_t readIndex;
    size_t bytesRead = 0;

    if (pRingBuffer->pBuffer != NULL) {
        U_PORT_MUTEX_LOCK(pRingBuffer->mutex);
        if (pRingBuffer->dataSize < length) {
            length = pRingBuffer->dataSize;
        }

        if (pRingBuffer->dataIndex >= pRingBuffer->dataSize) {
            readIndex = pRingBuffer->dataIndex - pRingBuffer->dataSize;
        } else {
            readIndex = pRingBuffer->dataIndex + pRingBuffer->size - pRingBuffer->dataSize;
        }

        pRingBuffer->dataSize -= length;
        while (bytesRead < length) {
            *pData++ = pRingBuffer->pBuffer[readIndex++];
            bytesRead++;
            if (readIndex == pRingBuffer->size) {
                readIndex = 0;
            }
        }
        U_PORT_MUTEX_UNLOCK(pRingBuffer->mutex);
    }

    return bytesRead;
}

size_t uRingBufferDataSize(const uRingBuffer_t *pRingBuffer)
{
    return (pRingBuffer->dataSize);
}

size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer)
{
    return (pRingBuffer->size - pRingBuffer->dataSize);
}

void uRingBufferReset(uRingBuffer_t *pRingBuffer)
{
    pRingBuffer->dataIndex = 0;
    pRingBuffer->dataSize = 0;
}

// End of file
