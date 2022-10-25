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
                                         uRingBufferRead()/uRingBufferPeek()
                                         can use when read handles are not
                                         involved, i.e. the ring buffer
                                         was created with
                                         uRingBufferCreate(). */
    const char **pDataRead;         /**< the data read pointers, named
                                         this way so that we naturally treat
                                         it as an array of type const char *
                                         in the code.
                                         In the uRingBufferCreate() case
                                         this will point at pDataReadNormal;
                                         in the uRingBufferCreateWithReadHandle()
                                         case it will be malloc'ed and
                                         pDataReadNormal will not be used,
                                         instead the zeroth entry of the
                                         allocated array will hold the
                                         "normal" read pointer. */
    size_t maxNumReadPointers;      /**< will always be at least 1 for the
                                         "normal" read case. */
    uint64_t dataReadLockBitmap;
    bool isMalloced;                /**< true if pDataRead was allocated. */
    char *pDataWrite;
    size_t size;
    void *mutex;                    /**< mutex for the ring buffer to ensure
                                         thread-safety, brought in as void *,
                                         and not "p"'ed, in order not to drag
                                         OS headers into everywhere. */
    bool readHandleRequired;        /**< true to ONLY allow uRingBufferReadHandle()/
                                         uRingBufferPeekHandle(); uRingBufferRead()/
                                         uRingBufferPeek() will return nothing. */
    size_t statReadLossNormalBytes; /**< storage for the bytes lost as a
                                         result of forced add pushing
                                         data out of the "normal" read
                                         pointer. */
    size_t *statReadLossBytes;      /**< storage for the bytes lost as a
                                         result of forced add pushing data
                                         out of any of the read pointers,
                                         named this way so that we naturally
                                         treat it as an array of type size_t
                                         in the code; the zeroth entry
                                         of this storage is unused. */
    size_t statAddLossBytes;        /**< storage for the number of bytes lost
                                         as a result of add or forced add
                                         being unable to write into the
                                         ring buffer. */
} uRingBuffer_t;

typedef void *uParseHandle_t; //!< Parser handle.

/** Parser function prototype, used with uRingBufferParseHandle().
 *
 * @param parseHandle     the parser handle used to access the ring buffer.
 * @param[in] pUserParam  a user parameter, passed in via uRingBufferParseHandle().
 * @return                #U_ERROR_COMMON_TIMEOUT if more data is needed to
 *                        conclude, #U_ERROR_COMMON_NOT_FOUND if nothing found or
 *                        #U_ERROR_COMMON_SUCCESS if sucessful.
 */
typedef int32_t (*U_RING_BUFFER_PARSER_f)(uParseHandle_t parseHandle, void *pUserParam);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: DEBUG
 * -------------------------------------------------------------- */

/** Dump the status of a ring buffer as a debug print.  Note that this
 * dumps the entire buffer contents with the read and write extents
 * visually laid out; it can be quite intensive print-wise.
 *
 * @param[in] pRingBuffer   a pointer to a ring buffer, cannot be NULL.
 */
void uRingBufferDump(uRingBuffer_t *pRingBuffer);

/* ----------------------------------------------------------------
 * FUNCTIONS: BASIC
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

/** Add data to a ring buffer, moving any [non-locked: see
 * uRingBufferLockReadHandle()] read pointer(s) on to make room if required
 * (so losing data from the ring buffer is OK).
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param[in] pData         pointer to the data.
 * @param length            the length of the data.
 * @return                  true if the data was added, false if either the
 *                          data to be added is greater in length than
 *                          the ring buffer or there is one or more locked
 *                          data pointers that prevent room being created
 *                          by moving them on.
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

/** Like uRingBufferRead() but doesn't move the read pointer on,
 * take a peek; see also uRingBufferPeekHandle() if you have multiple
 * consumers of data from the ring buffer.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param[out] pData        where to put the data.
 * @param length            the maximum amount of data to be peeked.
 * @param offset            the offset from the read pointer at which to
 *                          begin the peek.
 * @return                  the number of bytes peeked.
 */
size_t uRingBufferPeek(uRingBuffer_t *pRingBuffer, char *pData, size_t length,
                       size_t offset);

/** Get the amount of data available in a ring buffer; see also
 * uRingBufferDataSizeHandle(). If uRingBufferSetReadRequiresHandle()
 * is true then this will return zero.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes available for reading.
 */
size_t uRingBufferDataSize(const uRingBuffer_t *pRingBuffer);

/** Get the free space available in a ring buffer, that is what uRingBufferAdd()
 * would be able to store; see also uRingBufferAvailableSizeMax() for
 * what uRingBufferForcedAdd() would be able to store.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes available for storing.
 */
size_t uRingBufferAvailableSize(const uRingBuffer_t *pRingBuffer);

/** Flush the data of uRingBufferRead(); does not affect the data of
 * any uRingBufferTakeReadHandle(), for that see uRingBufferFlushHandle().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 */
void uRingBufferFlush(uRingBuffer_t *pRingBuffer);

/** Reset a ring buffer.  This resets the data only; any read handles
 * returned by uRingBufferTakeReadHandle() will remain valid, the
 * stats on the buffer will remain as they are.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 */
void uRingBufferReset(uRingBuffer_t *pRingBuffer);

/** Get the number of bytes lost due to uRingBufferForcedAdd() pushing
 * data out from under uRingBufferRead().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes lost.
 */
size_t uRingBufferStatReadLoss(uRingBuffer_t *pRingBuffer);

/** Get the number of bytes lost due to uRingBufferAdd() or
 * uRingBufferForcedAdd() being unable to write data into the
 * ring buffer.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the number of bytes lost.
 */
size_t uRingBufferStatAddLoss(uRingBuffer_t *pRingBuffer);

/* ----------------------------------------------------------------
 * FUNCTIONS: MULTIPLE READERS
 * -------------------------------------------------------------- */

/** Create a new ring buffer from a linear buffer that allows
 * multiple read handles. allowing the "handle" API functions
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
 *                          should be allowed; up to 64 are permitted.
 * @return                  zero on success else negative error code.
 */
int32_t uRingBufferCreateWithReadHandle(uRingBuffer_t *pRingBuffer,
                                        char *pLinearBuffer, size_t size,
                                        size_t maxNumReadHandles);

/** Set whether a ring buffer accepts uRingBufferRead()/uRingBufferPeek()
 * or requires the "handle" form, uRingBufferReadHandle()/
 * uRingBufferPeekHandle(), to be used.  Only useful if the ring buffer
 * was created by calling uRingBufferCreateWithReadHandle() rather than
 * uRingBufferCreate().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param onNotOff        true to require use of uRingBufferReadHandle()/
 *                        uRingBufferPeekHandle(), uRingBufferRead()/
 *                        uRingBufferPeek() will return nothing; false
 *                        [the default] allows both.
 */
void uRingBufferSetReadRequiresHandle(uRingBuffer_t *pRingBuffer,
                                      bool onNotOff);

/** Get whether a ring buffer accepts uRingBufferRead()/uRingBufferPeek()
 * or requires the "handle" form, uRingBufferReadHandle()/
 * uRingBufferPeekHandle(), to be used.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return    true if use of uRingBufferReadHandle()/uRingBufferPeekHandle()
 *            is required, else false (so either uRingBufferReadHandle()/
 *            uRingBufferPeekHandle() or uRingBufferRead()/uRingBufferPeek()
 *            can be used).
 */
bool uRingBufferGetReadRequiresHandle(uRingBuffer_t *pRingBuffer);

/** Register with the ring buffer as a reader.  Use this in conjunction
 * with uRingBufferReadHandle()/uRingBufferPeekHandle() (instead of
 * uRingBufferRead()/uRingBufferPeek()) if there is going to be more than
 * one consumer of the data in the ring buffer. It allows the ring buffer
 * code to remember whose read pointer to move on.  To use this function
 * the ring buffer must have been created by calling
 * uRingBufferCreateWithReadHandle() rather than uRingBufferCreate().
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

/** Lock a read handle; if this is called uRingBufferForceAdd() will not
 * be able to move this read pointer, data cannot be lost from it.  Use
 * this while peeking about before deciding to read the data; try not
 * to lock a read handle for long or you may miss incoming data through
 * forced-adds failing.  The function returns the number of bytes of
 * data waiting at the read handle, which is something you often
 * want to know at about this time.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          the handle to lock.
 * @return                the number of bytes available for reading from
 *                        this handle.
 */
size_t uRingBufferLockReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle);

/** Unlock a read handle that was locked with uRingBufferLockReadHandle().
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param handle            the handle to unlock.
 */
void uRingBufferUnlockReadHandle(uRingBuffer_t *pRingBuffer, int32_t handle);

/** Determine whether a read handle is locked or not.
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param handle            the read handle.
 * @return                  true if the read handle is locked, else false.
 */
bool uRingBufferReadHandleIsLocked(uRingBuffer_t *pRingBuffer, int32_t handle);

/** Like uRingBufferRead() except for use by an entity that has previously
 * obtained a read handle by calling uRingBufferTakeReadHandle(); this
 * mechanism should be employed if there is to be more than one consumer
 * of data from the ring buffer.  To use this function the ring buffer must
 * have been created by calling uRingBufferCreateWithReadHandle() rather
 * than uRingBufferCreate().
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

/** Like uRingBufferReadHandle() but doesn't move the read pointer on,
 * take a peek.  To use this function the ring buffer must have
 * been created by calling uRingBufferCreateWithReadHandle() rather
 * than uRingBufferCreate().
 *
 * @param[in] pRingBuffer   a pointer to the ring buffer, cannot be NULL.
 * @param handle            a read handle, as originally  returned by
 *                          uRingBufferTakeReadHandle().
 * @param[out] pData        where to put the data.
 * @param length            the maximum amount of data to peek.
 * @param offset            the offset from the read pointer at which to
 *                          begin the peek.
 * @return                  the number of bytes peeked.
 */
size_t uRingBufferPeekHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                             char *pData, size_t length, size_t offset);

/** Like uRingBufferDataSize() except for use by an entity that has
 * previously obtained a read handle by calling uRingBufferTakeReadHandle();
 * this mechanism should be employed if there is to be more than one consumer
 * of data from the ring buffer.  To use this function the ring buffer must
 * have been created by calling uRingBufferCreateWithReadHandle() rather
 * than uRingBufferCreate().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          a read handle, as originally returned by
 *                        uRingBufferTakeReadHandle().
 * @return                the number of bytes available for reading.
 */
size_t uRingBufferDataSizeHandle(const uRingBuffer_t *pRingBuffer, int32_t handle);

/** Like uRingBufferAvailableSize() but ignores any read handles that
 * are unlocked, so the amount of buffer space available to
 * uRingBufferForceAdd().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @return                the maximum number of bytes available for
 *                        storing by uRingBufferForceAdd().
 */
size_t uRingBufferAvailableSizeMax(const uRingBuffer_t *pRingBuffer);

/** Flush the data out of the given read handle; does not affect
 * the data of uRingBufferRead(), for that see uRingBufferFlush().
 * This will work even if the read handle is locked.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          a read handle, as originally returned by
 *                        uRingBufferTakeReadHandle().
 */
void uRingBufferFlushHandle(uRingBuffer_t *pRingBuffer, int32_t handle);

/** Get the number of bytes lost due to uRingBufferForcedAdd()() pushing
 * data out from under the given uRingBufferReadHandle().
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          a read handle, as originally returned by
 *                        uRingBufferTakeReadHandle().
 * @return                the number of bytes lost from the given
 *                        read handle.
 */
size_t uRingBufferStatReadLossHandle(uRingBuffer_t *pRingBuffer,
                                     int32_t handle);

/* ----------------------------------------------------------------
 * FUNCTIONS: PARSER
 * -------------------------------------------------------------- */

/** Run a set of parsers over the contents of the ring buffer.
 *
 * @param[in] pRingBuffer a pointer to the ring buffer, cannot be NULL.
 * @param handle          a read handle, as originally returned by
 *                        uRingBufferTakeReadHandle().
 * @param[in] pParserList a pointer to a list of parsers, terminated by
 *                        a NULL pointer.
 * @param[in] pUserParam  a user parameter to pass to each parser in the list.
 * @return                the number of bytes lost from the given
 *                        read handle.
 */
size_t uRingBufferParseHandle(uRingBuffer_t *pRingBuffer, int32_t handle,
                              U_RING_BUFFER_PARSER_f *pParserList, void *pUserParam);

/** Get a byte from the ring buffer while in a parser function.
 *
 * IMPORTANT: unlike all of the other ring-buffer functions, this function
 * is NOT thread-safe, it is ONLY intended to be used from within a
 * U_RING_BUFFER_PARSER_f function that will be called by uRingBufferParseHandle()
 * (which adds thread-safety).
 *
 * @param parseHandle     the parser handle used to access the ring buffer.
 * @param[out] p          pointer to store the byte or char.
 * @return                false if no more data, true if get of byte sucessful.
 */
bool uRingBufferGetByteUnprotected(uParseHandle_t parseHandle, void *p);

/** Number of bytes in the ring buffer while in a parser function.
 *
 * IMPORTANT: unlike all of the other ring-buffer functions, this function
 * is NOT thread-safe, it is ONLY intended to be used from within a
 * U_RING_BUFFER_PARSER_f function that will be called by uRingBufferParseHandle()
 * (which adds thread-safety).
 *
 * @param parseHandle     the parser handle used to access the ring buffer.
 * @return                number of bytes available in the ring buffer.
 */
size_t uRingBufferBytesAvailableUnprotected(uParseHandle_t parseHandle);

/** Number of bytes in the ring buffer discarded while in a parser function.
 *
 * IMPORTANT: unlike all of the other ring-buffer functions, this function
 * is NOT thread-safe, it is ONLY intended to be used from within a
 * U_RING_BUFFER_PARSER_f function that will be called by uRingBufferParseHandle()
 * (which adds thread-safety).
 *
 * @param parseHandle     the parser handle used to access the ring buffer.
 * @return                number of bytes discarded in the ring buffer.
 */
size_t uRingBufferBytesDiscardUnprotected(uParseHandle_t parseHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_RINGBUFFER_H_

// End of file
