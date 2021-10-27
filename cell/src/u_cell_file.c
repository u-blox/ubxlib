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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the file system API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stdlib.h"    // malloc(), free(), strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strcpy(), strtok_r(), strtol()

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_at_client.h"
#include "u_cell_module_type.h"
#include "u_cell_net.h"
#include "u_cell_private.h"
#include "u_cell_file.h"

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list of files container,
 * used when reading the list of stored files on file system.
 */
static uCellFileListContainer_t *gpFileList = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add an entry to the end of the linked list
// and count how many are in it once added.
static size_t uCellFilelListAddCount(uCellFileListContainer_t *pAdd)
{
    size_t count = 0;
    uCellFileListContainer_t **ppTmp = &gpFileList;

    while (*ppTmp != NULL) {
        ppTmp = &((*ppTmp)->pNext);
        count++;
    }

    if (pAdd != NULL) {
        *ppTmp = pAdd;
        count++;
    }

    return count;
}

// Get an entry from the start of the linked list and remove
// it from the list, returning the number left
static int32_t uCellFileListGetRemove(char *pFile)
{
    int32_t errorOrCount = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uCellFileListContainer_t *pTmp = gpFileList;

    if (pTmp != NULL) {
        if (pFile != NULL) {
            strcpy(pFile, pTmp->fileName);
        }
        pTmp = gpFileList->pNext;
        free(gpFileList);
        gpFileList = pTmp;
        errorOrCount = 0;
        while (pTmp != NULL) {
            pTmp = pTmp->pNext;
            errorOrCount++;
        }
    }

    return errorOrCount;
}

// Clear the file list
static void uCellFileListClear()
{
    uCellFileListContainer_t *pTmp;

    while (gpFileList != NULL) {
        pTmp = gpFileList->pNext;
        free(gpFileList);
        gpFileList = pTmp;
    }
    gpFileList = NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the tagged area of the file system that future calls will use.
int32_t uCellFileSetTag(int32_t cellHandle, const char *pTag)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
                pInstance->pFileSystemTag = pTag;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the file system tag that is currently in use.
const char *pUCellFileGetTag(int32_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    const char *pFileSystemTag = NULL;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance !=  NULL) {
            pFileSystemTag = pInstance->pFileSystemTag;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return pFileSystemTag;
}

// Write data into the file.
int32_t uCellFileWrite(int32_t cellHandle,
                       const char *pFileName,
                       const char *pData,
                       size_t dataSize)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    size_t bytesWritten = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the UDWNFILE thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDWNFILE=");
            // Write file name
            uAtClientWriteString(atHandle, pFileName, true);
            // Write size of data to be written into the file
            uAtClientWriteInt(atHandle, (int32_t) dataSize);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            // Wait for the prompt
            if (uAtClientWaitCharacter(atHandle, '>') == 0) {
                // Allow plenty of time for this to complete
                uAtClientTimeoutSet(atHandle, 10000);
                uPortTaskBlock(50);
                bytesWritten = uAtClientWriteBytes(atHandle, (const char *) pData,
                                                   dataSize, true);
                // Restore at client timeout to default
                uAtClientTimeoutSet(atHandle, U_AT_CLIENT_DEFAULT_TIMEOUT_MS);
                // Grab the response
                uAtClientCommandStopReadResponse(atHandle);
                if (uAtClientUnlock(atHandle) == 0) {
                    errorCode = (int32_t) bytesWritten;
                }
            } else {
                // Best to tidy whatever might have arrived instead
                // of the prompt before exiting
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Read data from file.
int32_t uCellFileRead(int32_t cellHandle,
                      const char *pFileName,
                      char *pData,
                      size_t dataSize)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t readSize = 0;
    int32_t indicatedReadSize = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the URDFILE thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+URDFILE=");
            // Write file name
            uAtClientWriteString(atHandle, pFileName, true);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            // Grab the response
            if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                // SARA-R4 only puts \n before the
                // response, not \r\n as it should
                uAtClientResponseStart(atHandle, "\n+URDFILE:");
            } else {
                uAtClientResponseStart(atHandle, "+URDFILE:");
            }
            // Skip the file name
            uAtClientSkipParameters(atHandle, 1);
            // Read the size
            indicatedReadSize = uAtClientReadInt(atHandle);
            readSize = indicatedReadSize;
            if (readSize > (int32_t) dataSize) {
                readSize = (int32_t) dataSize;
            }
            // Don't stop for anything!
            uAtClientIgnoreStopTag(atHandle);
            // Get the leading quote mark out of the way
            uAtClientReadBytes(atHandle, NULL, 1, true);
            // Now read out all the actual data,
            // first the bit we want
            readSize = uAtClientReadBytes(atHandle, pData,
                                          // Cast in two stages to keep Lint happy
                                          (size_t)  (unsigned) readSize,
                                          true);
            if (indicatedReadSize > readSize) {
                //...and then the rest poured away to NULL
                uAtClientReadBytes(atHandle, NULL,
                                   // Cast in two stages to keep Lint happy
                                   (size_t) (unsigned) (indicatedReadSize - readSize),
                                   true);
            }
            // Make sure to wait for the stop tag before
            // we finish
            uAtClientRestoreStopTag(atHandle);
            uAtClientResponseStop(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                errorCode = readSize;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Read block of data from file.
int32_t uCellFileBlockRead(int32_t cellHandle,
                           const char *pFileName,
                           char *pData,
                           size_t offset,
                           size_t dataSize)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t readSize = 0;
    int32_t indicatedReadSize = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Use of tags is not supported by any of the modules
            // we support for block reads
            if (pInstance->pFileSystemTag == NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                // Do the URDBLOCK thang with the AT interface
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+URDBLOCK=");
                // Write file name
                uAtClientWriteString(atHandle, pFileName, true);
                // Write offset in bytes from the beginning of the file
                uAtClientWriteInt(atHandle, (int32_t) offset);
                // Write size of data to be read from file
                uAtClientWriteInt(atHandle, (int32_t) dataSize);
                uAtClientCommandStop(atHandle);
                // Grab the response
                if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                    // SARA-R4 only puts \n before the
                    // response, not \r\n as it should
                    uAtClientResponseStart(atHandle, "\n+URDBLOCK:");
                } else {
                    uAtClientResponseStart(atHandle, "+URDBLOCK:");
                }
                // Skip the file name
                uAtClientSkipParameters(atHandle, 1);
                // Read the size
                indicatedReadSize = uAtClientReadInt(atHandle);
                readSize = indicatedReadSize;
                if (readSize > (int32_t) dataSize) {
                    readSize = (int32_t) dataSize;
                }
                // Don't stop for anything!
                uAtClientIgnoreStopTag(atHandle);
                // Get the leading quote mark out of the way
                uAtClientReadBytes(atHandle, NULL, 1, true);
                // Now read out all the actual data,
                // first the bit we want
                readSize = uAtClientReadBytes(atHandle, pData,
                                              // Cast in two stages to keep Lint happy
                                              (size_t) (unsigned) readSize,
                                              true);
                if (indicatedReadSize > readSize) {
                    //...and then the rest poured away to NULL
                    uAtClientReadBytes(atHandle, NULL,
                                       // Cast in two stages to keep Lint happy
                                       (size_t) (unsigned) (indicatedReadSize - readSize),
                                       true);
                }
                // Make sure to wait for the stop tag before
                // we finish
                uAtClientRestoreStopTag(atHandle);
                uAtClientResponseStop(atHandle);
                if (uAtClientUnlock(atHandle) == 0) {
                    errorCode = readSize;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Read file size.
int32_t uCellFileSize(int32_t cellHandle,
                      const char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t size = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the ULSTFILE thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+ULSTFILE=");
            // Write get file size op_code
            uAtClientWriteInt(atHandle, 2);
            // Write file name
            uAtClientWriteString(atHandle, pFileName, true);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            // Grab the response
            uAtClientResponseStart(atHandle, "+ULSTFILE:");
            // Read file size
            size = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                errorCode = size;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Delete file on file system.
int32_t uCellFileDelete(int32_t cellHandle,
                        const char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the UDELFILE thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDELFILE=");
            // Write file name
            uAtClientWriteString(atHandle, pFileName, true);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            // Grab the response
            uAtClientCommandStopReadResponse(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the name of the first file stored on file system.
int32_t uCellFileListFirst(int32_t cellHandle,
                           char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellFileListContainer_t *pFileContainer;
    bool keepGoing = true;
    int32_t bytesRead = 0;
    size_t count = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pFileName != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the ULSTFILE thang with the AT interface
            uAtClientLock(atHandle);
            // Make sure the credential list is clear
            uCellFileListClear();
            uAtClientCommandStart(atHandle, "AT+ULSTFILE=");
            // List files operation
            uAtClientWriteInt(atHandle, 0);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+ULSTFILE:");
            while (keepGoing) {
                keepGoing = false;
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pFileContainer = (uCellFileListContainer_t *) malloc(sizeof(*pFileContainer));
                if (pFileContainer != NULL) {
                    pFileContainer->pNext = NULL;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    // Read file name
                    bytesRead = uAtClientReadString(atHandle, pFileContainer->fileName,
                                                    sizeof(pFileContainer->fileName), false);
                }
                if (bytesRead > 0) {
                    bytesRead = 0;
                    keepGoing = true;
                    // Add the container to the end of the list
                    count = uCellFilelListAddCount(pFileContainer);
                } else {
                    // Nothing there, free it
                    free(pFileContainer);
                }
            }
            uAtClientResponseStop(atHandle);

            // Do the following parts inside the AT lock,
            // providing protection for the linked-list.
            if (errorCode == (int32_t) U_ERROR_COMMON_NO_MEMORY) {
                // If we ran out of memory, clear the whole list,
                // don't want to report partial information
                uCellFileListClear();
            } else {
                if (count > 0) {
                    // Set the return value, copy out the first item in the list
                    // and remove it.
                    errorCode = (int32_t) count;
                    uCellFileListGetRemove(pFileName);
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                }
            }
            uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Return subsequent file name in the list.
int32_t uCellFileListNext(int32_t cellHandle,
                          char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pFileName != NULL)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            // While this doesn't use the AT interface we can use
            // the mutex to protect the linked list.
            errorCode = uCellFileListGetRemove(pFileName);
            uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Free memory from list.
void uCellFileListLast(int32_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        // Check parameters
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            // While this doesn't use the AT interface we can use
            // the mutex to protect the linked list.
            uCellFileListClear();
            uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}
