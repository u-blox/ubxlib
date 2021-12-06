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

/** Type to hold the static data of a named pipe.
 */
typedef struct {
    size_t itemSizeBytes;
    size_t maxNumItems;
    HANDLE writeHandle;
    HANDLE readHandle;
} uPortPrivatePipe_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the private bits of the porting layer.
 *
 * @return: zero on success else negative error code.
 */
int32_t uPortPrivateInit(void);

/** Deinitialise the private bits of the porting layer.
 */
void uPortPrivateDeinit(void);

/** Add a pipe to the list of pipes.
 *
 * @return  on success the handle for the pipe, else
 *          negative error code; the handle can be used
 *          as x in a pipe string "\\.\pipe\x".
 */
int32_t uPortPrivatePipeAdd(void);

/** Set the static data of a pipe.
 *
 * @param pipeHandle         the handle of the pipe.
 * @param itemSizeBytes      the size of each item sent down
 *                           the pipe.
 * @param maxNumItems        the number of items that can be
 *                           put in the pipe.
 * @param writeHandle        the write handle of the pipe.
 * @param readHandle         the read handle of the pipe.
 * @return                   zero on success else negative error
 *                           code.
 */
int32_t uPortPrivatePipeSet(int32_t pipeHandle,
                            size_t itemSizeBytes,
                            size_t maxNumItems,
                            HANDLE writeHandle,
                            HANDLE readHandle);

/** Return a copy of a pipe entry from the list.
 *
 * @param pipeHandle  the handle of the pipe.
 * @param pPipeCopy   a place to put the copy of the pipe entry.
 * @return            zero on success else negative error code.
 */
int32_t uPortPrivatePipeGetCopy(int32_t pipeHandle,
                                uPortPrivatePipe_t *pPipeCopy);

/** Remove a pipe from the list of pipes.
 *
 * @param pipeHandle  the handle of the pipe.
 */
void uPortPrivatePipeRemove(int32_t pipeHandle);

/** For convenience the task priorities are kept in a 0 to 15 range,
 * however withint the Windows thread API the priorities are -2 to
 * +2: this function converts the 0 to 15 values into the Windows
 * native values.
 *
 * @param priority  the 0 to 15 priority value range.
 * @return          the value in -2 to +2 Windows value range.
 */
int32_t uPortPrivateTaskPriorityConvert(int32_t priority);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
