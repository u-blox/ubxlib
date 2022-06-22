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

#ifndef _U_RINGBUFFER_H_
#define _U_RINGBUFFER_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __utils
 *  @{
 */

/** @file
 * @brief Ring buffer wrapper API for linear buffer.
 * All functions except uRingBufferCreate() and uRingBufferDelete()
 * are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    char *pBuffer;
    size_t dataIndex;
    size_t dataSize;
    size_t size;
    uPortMutexHandle_t mutex;
} uRingBuffer_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create new ring buffer from linear buffer.
 *
 * @param pRingBuffer   pointer to ring buffer.
 * @param pLinearBuffer pointer to linear buffer.
 * @param size          size of linear buffer in bytes.
 */
void uRingBufferCreate(uRingBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size);

/** Delete ring buffer.
 *
 * @param pRingBuffer   pointer to ring buffer.
 */
void uRingBufferDelete(uRingBuffer_t *pRingBuffer);

/** Add data to ringbuffer.
 *
 * @param pRingBuffer   handle to ringbuffer.
 * @param pData         pointer to data.
 * @param length        length of data.
 * @return              true if data was added
 *                      false if data was not added, which will be
 *                      the case if there is not room enough.
 */
bool uRingBufferAdd(uRingBuffer_t *pRingBuffer, const char *pData, size_t length);

/** Read data from ringbuffer.
 *
 * @param pRingBuffer   handle to ringbuffer.
 * @param pData         pointer where to put data.
 * @param length        maximum length of data.
 * @return              number of bytes read.
 */
size_t uRingBufferRead(uRingBuffer_t *pRingBuffer, char *pData, size_t length);

/** Amount of data available.
 *
 * @param pRingBuffer   handle to ringbuffer.
 * @return              number of bytes available for reading.
 */
size_t uRingBufferDataSize(const uRingBuffer_t *pRingBuffer);

/** Free space available.
 *
 * @param pRingBuffer   handle to ringbuffer.
 * @return              number of bytes available for storing.
 */
size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer);

/** Reset ring buffer.
 *
 * @param pRingBuffer   handle to ringbuffer.
 */
void uRingBufferReset(uRingBuffer_t *pRingBuffer);


#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_RINGBUFFER_H_

// End of file
