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

/** Structure that defines a ring buffer; note that the contents
 * of this structure are internal, subject to change, please use
 * the access functions of this API to get to them, rather than
 * reading-from or writing-to them directly; this also ensures
 * thread-safety.
 */

typedef struct {
    char *pBuffer;
    const char *pDataReadNormal;    /**< storage for the default
                                         "normal" read pointer, which
                                         uRingBufferRead() can use
                                         when read handles are not
                                         involved, i.e. the ring buffer
                                         was created with
                                         uRingBufferCreate(). */
    const char **dataRead;          /**< the data read pointers, named
                                         this way so that we naturally treat
                                         it as an array of type const char *
                                         in the code.
                                         In the uRingBufferCreate() case
                                         this will point at pDataReadNormal;
                                         in the uRingBufferCreateWithReadHandle()
                                         case it will be malloc'ed and
                                         pDataReadNormal will not be used,
                                         instead the zeroth entry of the
                                         malloc()ed array will hold the
                                         "normal" read pointer. */
    size_t maxNumReadPointers;      /**< will always be at least 1 for the
                                         "normal" read case. */
    bool isMalloced;                /**< true if dataRead was malloc()ed. */
    char *pDataWrite;
    size_t size;
    void *mutex;                    /**< mutex for the ring buffer to ensure
                                         thread-safety, brought in as void *,
                                         and not "p"'ed, in order not to drag
                                         OS headers into everywhere. */
    bool readHandleRequired;        /**< true to ONLY allow uRingBufferReadHandle();
                                         uRingBufferRead() will return nothing. */
} uRingBuffer_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a new ring buffer from a linear buffer.  If you wish to
 * use the  "read handle" type API functions here you must instead
 * create the ring buffer using uRingBufferCreateWithReadHandle().
 *
 * @param[in] pRingBuffer   a pointer to a ring buffer, cannot be NULL.
 * @param[in] pLinearBuffer a pointer to the linear buffer.
 * @param size              the size of the linear buffer in bytes; the
 *                          ring buffer will be of maximum size this
 *                          number minus one as one byte is used to
 *                          prevent pointer-wrap.
 * @return                  zero on success else negative error code.
 */
int32_t uRingBufferCreate(uRingBuffer_t *pRingBuffer, char *pLinearBuffer,
                          size_t size);

/** Delete a ring buffer.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 */
void uRingBufferDelete(uRingBuffer_t *pRingBuffer);

/** Add data to a ring buffer.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param[in] pData         pointer to the data.
 * @param length            the length of the data.
 * @return                  true if the data was added, false if the data
 *                          was not added, which will be the case if there
 *                          is not room enough.
 */
bool uRingBufferAdd(uRingBuffer_t *pRingBuffer, const char *pData,
                    size_t length);

/** Add data to a ring buffer, moving the read pointer(s) on to make
 * room if required (i.e. losing data from the ring buffer is OK).
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param[in] pData         pointer to the data.
 * @param length            the length of the data.
 * @return                  true if the data was added, false if the
 *                          data to be added is greater in length
 *                          than the ring buffer.
 */
bool uRingBufferForceAdd(uRingBuffer_t *pRingBuffer, const char *pData,
                         size_t length);

/** Read data from a ring buffer; see also uRingBufferReadHandle()
 * if you want to have multiple consumers of data from the ring buffer.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param[out] pData        where to put the data; may be NULL to throw the
 *                          data away.
 * @param length            the maximum amount of data to read.
 * @return                  the number of bytes read.
 */
size_t uRingBufferRead(uRingBuffer_t *pRingBuffer, char *pData, size_t length);

/** Get the amount of data available in a ring buffer; see also
 * uRingBufferDataSizeHandle(). If uRingBufferSetReadRequiresHandle()
 * is true then this will return zero.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes available for reading.
 */
size_t uRingBufferDataSize(const uRingBuffer_t *pRingBuffer);

/** Get the free space available in a ring buffer.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes available for storing.
 */
size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer);

/** Reset a ring buffer.  This resets the data pointers only;
 * any read handles returned by uRingBufferTakeReadHandle() will
 * remain valid.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 */
void uRingBufferReset(uRingBuffer_t *pRingBuffer);

/** Create a new ring buffer from a linear buffer that allows
 * multiple read handles, i.e. allowing the "handle" API functions
 * here to be used.  If you don't need/want this overhead, i.e. you
 * only have one consumer of data from the buffer that will call
 * uRingBufferRead(), then you should create your ring buffer using
 * uRingBufferCreate() instead.
 *
 * @param[in] pRingBuffer   a pointer to a ring buffer, cannot be NULL.
 * @param[in] pLinearBuffer a pointer to the linear buffer.
 * @param size              the size of the linear buffer in bytes; the
 *                          ring buffer will be of maximum size this
 *                          number minus one as one byte is used to
 *                          prevent pointer-wrap.
 * @param maxNumReadHandles the maximum number of read handles that
 *                          should be allowed.
 * @return                  zero on success else negative error code.
 */
int32_t uRingBufferCreateWithReadHandle(uRingBuffer_t *pRingBuffer,
                                        char *pLinearBuffer, size_t size,
                                        size_t maxNumReadHandles);

/** Set whether a ring buffer accepts uRingBufferRead() or requires
 * the "handle" form, uRingBufferReadHandle(), to be used.  Only useful
 * if the ring buffer was created by calling
 * uRingBufferCreateWithReadHandle(), rather than uRingBufferCreate().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param onNotOff        true to require use of uRingBufferReadHandle(),
 *                        uRingBufferRead() will return nothing;
 *                        false [the default] allows both.
 */
void uRingBufferSetReadRequiresHandle(uRingBuffer_t *pRingBuffer,
                                      bool onNotOff);

/** Get whether a ring buffer accepts uRingBufferRead() or requires
 * the "handle" form, uRingBufferReadHandle(), to be used.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return    true if use of uRingBufferReadHandle() is required, else
 *            false (i.e. either uRingBufferReadHandle() or
 *            uRingBufferRead() can be used).
 */
bool uRingBufferGetReadRequiresHandle(uRingBuffer_t *pRingBuffer);

/** Register with the ring buffer as a reader.  Use this in conjunction
 * with uRingBufferReadHandle() (instead of uRingBufferRead()) if there is
 * going to be more than one consumer of the data in the ring buffer; it
 * allows the ring buffer code to remember whose read pointer to move on.
 * To use this function the ring buffer must have been created by calling
 * uRingBufferCreateWithReadHandle() (rather than uRingBufferCreate()).
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @return                  a registration handle else negative error code.
 */
int32_t uRingBufferTakeReadHandle(uRingBuffer_t *pRingBuffer);

/** Give back a read handle.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param handle            the handle to give back, as originally returned
 *                          by uRingBufferTakeReadHandle().
 */
void uRingBufferGiveReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle);

/** Like uRingBufferRead() except for use by an entity that has previously
 * obtained a read handle by calling uRingBufferTakeReadHandle(); this
 * mechanism should be employed if there is to be more than one consumer
 * of data from the ring buffer.  To use this function the ring buffer must
 * have been created by calling uRingBufferCreateWithReadHandle() (rather
 * than uRingBufferCreate()).
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param handle            a read handle, as originally  returned by
 *                          uRingBufferTakeReadHandle().
 * @param[out] pData        where to put the data; may be NULL to throw the
 *                          data away.
 * @param length            the maximum amount of data to read.
 * @return                  the number of bytes read.
 */
size_t uRingBufferReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                             char *pData, size_t length);

/** Like uRingBufferDataSize() except for use by an entity that has
 * previously obtained a read handle by calling uRingBufferTakeReadHandle();
 * this mechanism should be employed if there is to be more than one consumer
 * of data from the ring buffer.  To use this function the ring buffer must
 * have been created by calling uRingBufferCreateWithReadHandle() (rather
 * than uRingBufferCreate()).
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          a read handle, as originally returned by
 *                        uRingBufferTakeReadHandle().
 * @return                the number of bytes available for reading.
 */
size_t uRingBufferDataSizeHandle(const uRingBuffer_t *pRingBuffer, int32_t handle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_RINGBUFFER_H_

// End of file
