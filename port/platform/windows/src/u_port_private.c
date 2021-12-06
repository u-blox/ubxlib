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

/** @file
 * @brief Stuff private to the Windows porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stdlib.h"    // For malloc()/free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "windows.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The lowest value that pipe handle can have; we avoid 0 since
 * there may be checks for NULL-ness floating around that it
 * would be prudent to avoid.
 */
#define U_PORT_PRIVATE_PIPE_HANDLE_MIN 1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold uPortPrivatePipe_t in a linked list.
 */
typedef struct uPortPrivatePipeList_t {
    int32_t pipeHandle;
    uPortPrivatePipe_t contents;
    struct uPortPrivatePipeList_t *pNext;
} uPortPrivatePipeList_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the linked list of named pipe handles used when
 * implementing queues in the OS functions.
 */
static uPortMutexHandle_t gMutexPipeList = NULL;

/** A hook for the linked list of pipe handles.
 */
static uPortPrivatePipeList_t *gpPipeListRoot = NULL;

/** The next pipe handle to use.
 * Design note: we use an index here to maintain uniquness
 * of each pipe, being afraid that just using the value of
 * the pointer, which may of course be reused when a pipe
 * is opened and closed, could result in odd stuff
 * happening underneath us in Windows.
 */
static int32_t gNextPipeHandle = U_PORT_PRIVATE_PIPE_HANDLE_MIN;

/** Convert a local task priority value into a Windows one.
 */
static const int32_t localToWinPriority[] = {-2,  // 0
                                             -2,  // 1
                                             -2,  // 2
                                             -2,  // 3
                                             -1,  // 4
                                             -1,  // 5
                                             -1,  // 6
                                             -1,  // 7
                                             0,  // 8
                                             0,  // 9
                                             0,  // 10
                                             0,  // 11
                                             1,  // 12
                                             1,  // 13
                                             1,  // 14
                                             1
                                             }; // 15

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a pipe entry in the list by handle (i.e. index) and return
// a handle to its contents.
// gMutexPipeList should be locked before this is called.
static uPortPrivatePipe_t *pPipeListEntryFind(int32_t pipeHandle)
{
    uPortPrivatePipeList_t *pTmp = gpPipeListRoot;
    uPortPrivatePipe_t *pContents = NULL;

    while ((pTmp != NULL) && (pContents == NULL)) {
        if (pTmp->pipeHandle == pipeHandle) {
            pContents = &(pTmp->contents);
        } else {
            pTmp = pTmp->pNext;
        }
    }

    return pContents;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT
 * -------------------------------------------------------------- */

// Initialise the private bits of the porting layer.
int32_t uPortPrivateInit(void)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexPipeList == NULL) {
        errorCode = uPortMutexCreate(&gMutexPipeList);
    }

    return errorCode;
}

// Deinitialise the private bits of the porting layer.
void uPortPrivateDeinit(void)
{
    if (gMutexPipeList != NULL) {
        U_PORT_MUTEX_LOCK(gMutexPipeList);
        U_PORT_MUTEX_UNLOCK(gMutexPipeList);
        uPortMutexDelete(gMutexPipeList);
        gMutexPipeList = NULL;
    }
}

// Add a pipe to the list, returning its handle.
int32_t uPortPrivatePipeAdd(void)
{
    int32_t errorCodeOrIndex = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t x;
    bool success = true;
    uPortPrivatePipeList_t *pNew;
    uPortPrivatePipe_t *pContents;

    if (gMutexPipeList != NULL) {
        errorCodeOrIndex = (int32_t) U_ERROR_COMMON_NO_MEMORY;

        U_PORT_MUTEX_LOCK(gMutexPipeList);

        // Find a unique handle
        x = gNextPipeHandle;
        while ((pPipeListEntryFind(gNextPipeHandle) != NULL) && success) {
            gNextPipeHandle++;
            if (gNextPipeHandle < U_PORT_PRIVATE_PIPE_HANDLE_MIN) {
                gNextPipeHandle = U_PORT_PRIVATE_PIPE_HANDLE_MIN;
            }
            if (gNextPipeHandle == x) {
                // Wrapped
                success = false;
            }
        }

        if (success) {
            // Allocate memory and add the pipe to the linked list
            pNew = (uPortPrivatePipeList_t *) malloc(sizeof(uPortPrivatePipeList_t));
            if (pNew != NULL) {
                pNew->pipeHandle = gNextPipeHandle;
                pContents = &(pNew->contents);
                pContents->itemSizeBytes = 0;
                pContents->maxNumItems = 0;
                pContents->writeHandle = INVALID_HANDLE_VALUE;
                pContents->readHandle = INVALID_HANDLE_VALUE;
                pNew->pNext = gpPipeListRoot;
                gpPipeListRoot = pNew;
                errorCodeOrIndex = gNextPipeHandle;
                gNextPipeHandle++;
                if (gNextPipeHandle < U_PORT_PRIVATE_PIPE_HANDLE_MIN) {
                    gNextPipeHandle = U_PORT_PRIVATE_PIPE_HANDLE_MIN;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexPipeList);
    }

    return errorCodeOrIndex;
}

// Set the static data of a pipe in the list.
int32_t uPortPrivatePipeSet(int32_t pipeHandle,
                            size_t itemSizeBytes,
                            size_t maxNumItems,
                            HANDLE writeHandle,
                            HANDLE readHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivatePipe_t *pContents;

    if (gMutexPipeList != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((writeHandle != INVALID_HANDLE_VALUE) &&
            (readHandle != INVALID_HANDLE_VALUE)) {

            U_PORT_MUTEX_LOCK(gMutexPipeList);

            pContents = pPipeListEntryFind(pipeHandle);
            if (pContents != NULL) {
                pContents->itemSizeBytes = itemSizeBytes;
                pContents->maxNumItems = maxNumItems;
                pContents->writeHandle = writeHandle;
                pContents->readHandle = readHandle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }

            U_PORT_MUTEX_UNLOCK(gMutexPipeList);
        }
    }

    return errorCode;
}

// Return a copy of a pipe entry from the list.
int32_t uPortPrivatePipeGetCopy(int32_t pipeHandle,
                                uPortPrivatePipe_t *pPipeCopy)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivatePipe_t *pTmp;

    if (gMutexPipeList != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pPipeCopy != NULL) {

            U_PORT_MUTEX_LOCK(gMutexPipeList);

            pTmp = pPipeListEntryFind(pipeHandle);
            if (pTmp != NULL) {
                *pPipeCopy = *pTmp;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }

            U_PORT_MUTEX_UNLOCK(gMutexPipeList);
        }
    }

    return errorCode;
}

// Remove a pipe from the list of pipes.
void uPortPrivatePipeRemove(int32_t pipeHandle)
{
    uPortPrivatePipeList_t *pTmp = gpPipeListRoot;
    uPortPrivatePipeList_t *pPrevious = NULL;

    if (gMutexPipeList != NULL) {

        U_PORT_MUTEX_LOCK(gMutexPipeList);

        while (pTmp != NULL) {
            if (pTmp->pipeHandle == pipeHandle) {
                if (pPrevious == NULL) {
                    // At head
                    gpPipeListRoot = pTmp->pNext;
                } else {
                    pPrevious->pNext = pTmp->pNext;
                }
                free(pTmp);
                // Force exit
                pTmp = NULL;
            } else {
                pPrevious = pTmp;
                pTmp = pTmp->pNext;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexPipeList);
    }
}

// Convert to Windows thread priority.
int32_t uPortPrivateTaskPriorityConvert(int32_t priority)
{
    if (priority < 0) {
        priority = 0;
    }
    if (priority > sizeof(localToWinPriority) - 1) {
        priority = sizeof(localToWinPriority) - 1;
    }

    return localToWinPriority[priority];
}

// End of file
