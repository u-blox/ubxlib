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
 * @brief Implementation of the file system API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strncpy(), strtok_r(), strtol()

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_at_client.h"
#include "u_cell_module_type.h"
#include "u_cell_net.h"
#include "u_cell_file.h"
#include "u_cell_private.h"

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list of files container,
 * used when reading the list of stored files on file system.
 */
static uCellPrivateFileListContainer_t *gpFileList = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the tagged area of the file system that future calls will use.
int32_t uCellFileSetTag(uDeviceHandle_t cellHandle, const char *pTag)
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
const char *pUCellFileGetTag(uDeviceHandle_t cellHandle)
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
int32_t uCellFileWrite(uDeviceHandle_t cellHandle,
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
int32_t uCellFileRead(uDeviceHandle_t cellHandle,
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
int32_t uCellFileBlockRead(uDeviceHandle_t cellHandle,
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
int32_t uCellFileSize(uDeviceHandle_t cellHandle,
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
int32_t uCellFileDelete(uDeviceHandle_t cellHandle,
                        const char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = uCellPrivateFileDelete(pInstance, pFileName);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the name of the first file stored on file system.
int32_t uCellFileListFirst(uDeviceHandle_t cellHandle,
                           char *pFileName)
{
    return uCellFileListFirst_r(cellHandle, pFileName, (void **) &gpFileList);
}

// Return subsequent file name in the list.
int32_t uCellFileListNext(uDeviceHandle_t cellHandle,
                          char *pFileName)
{
    (void) cellHandle;
    return uCellFileListNext_r(pFileName, (void **) &gpFileList);
}

// Free memory from list.
void uCellFileListLast(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    uCellFileListLast_r((void **) &gpFileList);
}

// Get the name of the first file stored on file system, re-entrant version.
int32_t uCellFileListFirst_r(uDeviceHandle_t cellHandle,
                             char *pFileName, void **ppRentrant)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (ppRentrant != NULL)) {
            *ppRentrant = NULL;
            errorCode = uCellPrivateFileListFirst(pInstance,
                                                  (uCellPrivateFileListContainer_t **) ppRentrant,
                                                  pFileName);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Return subsequent file name in the list, re-entrant version.
int32_t uCellFileListNext_r(char *pFileName, void **ppRentrant)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gUCellPrivateMutex != NULL) {

        // Though this doesn't use the instance pointer we can
        // use the cellular API mutex to protect the linked list
        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (ppRentrant != NULL) {
            errorCode = uCellPrivateFileListNext((uCellPrivateFileListContainer_t **) ppRentrant,
                                                 pFileName);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Free memory from list, re-entrant version.
void uCellFileListLast_r(void **ppRentrant)
{
    if (gUCellPrivateMutex != NULL) {

        // Though this doesn't use the instance pointer we can
        // use the cellular API mutex to protect the linked list
        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (ppRentrant != NULL) {
            uCellPrivateFileListLast((uCellPrivateFileListContainer_t **) ppRentrant);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

// End of file
