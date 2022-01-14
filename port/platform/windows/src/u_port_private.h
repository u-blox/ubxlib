/*
 * Copyright 2020 u-blox
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

#ifndef _U_PORT_PRIVATE_H_
#define _U_PORT_PRIVATE_H_

/** @file
 * @brief Stuff private to the Windows porting layer.
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

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Initialise the private bits of the porting layer.
 *
 * @return: zero on success else negative error code.
 */
int32_t uPortPrivateInit(void);

/** Deinitialise the private bits of the porting layer.
 */
void uPortPrivateDeinit(void);

/** For convenience the task priorities are kept in a 0 to 15 range,
 * however within the Windows thread API the priorities are -2 to
 * +2: this function converts the 0 to 15 values into the Windows
 * native values.
 *
 * @param priority  the 0 to 15 priority value range.
 * @return          the value in -2 to +2 Windows value range.
 */
int32_t uPortPrivateTaskPriorityConvert(int32_t priority);

/* ----------------------------------------------------------------
 * FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

/** Add a queue to the list of queues.
 *
 * @param itemSizeBytes the size of a queue item.
 * @param maxNumItems   the maximum number of items the queue can
 *                      accommodate.
 * @return              on success the handle for the queue, else
 *                      negative error code.
 */
int32_t uPortPrivateQueueAdd(size_t itemSizeBytes, size_t maxNumItems);

/** Write a block of data to the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData   the block of data to write, of size itemSizeBytes,
 *                as was used in the uPortPrivateQueueAdd() call that
 *                created the queue; cannot be NULL.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueWrite(int32_t handle, const char *pData);

/** Read a block of data from the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData   storage for the data, of size itemSizeBytes, as was
 *                used in the uPortPrivateQueueAdd() call that
 *                created the queue; may be NULL in which case the
 *                block is thrown away.
 * @param waitMs  the time to wait for data to become available;
 *                specify -1 for blocking.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueRead(int32_t handle, char *pData, int32_t waitMs);

/** Peek the given queue.
 *
 * @param handle  the handle of the queue.
 * @param pData  storage for the data, of size itemSizeBytes, as was
 *                used in the uPortPrivateQueueAdd() call that
 *                created the queue; cannot be NULL.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueuePeek(int32_t handle, char *pData);

/** Get the number of free spaces in the given queue.
 *
 * @param handle  the handle of the queue.
 * @return        on success the number of free spaces, else negative
 *                error code.
 */
int32_t uPortPrivateQueueGetFree(int32_t handle);

/** Remove a queue from the list of queues.
 *
 * @param handle  the handle of the queue.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateQueueRemove(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
