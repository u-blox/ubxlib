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

#ifndef _U_BLE_PRIVATE_H_
#define _U_BLE_PRIVATE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to the cellular API.
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
} ringBuffer_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */
/** Initialize data part of BLE
 */
void uBleDataPrivateInit(void);

/** De-Initialize data part of BLE
 */
void uBleDataPrivateDeinit(void);

/** Create new ring buffer from linear buffer
 *
 * @param pRingBuffer   Pointer to ring buffer
 * @param pLinearBuffer Pointer to linear buffer
 * @param size          Size of linear buffer in bytes
 */
void ringBufferCreate(ringBuffer_t *pRingBuffer, char *pLinearBuffer, size_t size);

/** Delete ring buffer
 *
 * @param pRingBuffer   Pointer to ring buffer
 */
void ringBufferDelete(ringBuffer_t *pRingBuffer);

/** Add data to ringbuffer
 *
 * @param pRingBuffer   Handle to ringbuffer
 * @param pData        Pointer to data
 * @param length       Length of data
 *
 * @return             True if data was added
 *                     False if data was not added, which will be
 *                     the case if there is not room enough
 */
bool ringBufferAdd(ringBuffer_t *pRingBuffer, const char *pData, size_t length);

/** Read data from ringbuffer
 *
 * @param pRingBuffer   Handle to ringbuffer
 * @param pData        Pointer where to put data
 * @param length       Maximum length of data
 *
 * @return             Number of bytes read
 */
size_t ringBufferRead(ringBuffer_t *pRingBuffer, char *pData, size_t length);

/** Amount of data available
 *
 * @param pRingBuffer   Handle to ringbuffer
 *
 * @return             Number of bytes available for reading
 */
size_t ringBufferDataSize(const ringBuffer_t *pRingBuffer);

/** Free space available
 *
 * @param pRingBuffer   Handle to ringbuffer
 *
 * @return             Number of bytes available for storing
 */
size_t ringBufferAvailableSize(const ringBuffer_t *pRingBuffer);

/** Reset ring buffer
 *
 * @param pRingBuffer   Handle to ringbuffer
 */
void ringBufferReset(ringBuffer_t *pRingBuffer);


/** Translate MAC address in byte array to string
 *
 * @param pAddrIn  pointer to byte array
 * @param addrType Public, Random or Unknown
 * @param msbLast  Last byte in array should be leftmost byte in string
 * @param pAddrOut Output string
 */
void addrArrayToString(const uint8_t *pAddrIn, uPortBtLeAddressType_t addrType, bool msbLast,
                       char *pAddrOut);

#ifdef __cplusplus
}
#endif

#endif // _U_BLE_PRIVATE_H_

// End of file
