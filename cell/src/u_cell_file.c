/*
 * Copyright 2019-2024 u-blox
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
#include "u_port_debug.h"
#include "u_port_heap.h"
#include "u_timeout.h"
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

// Open a file, LEXI-R10 style, returning handle or error code on failure.
static int32_t openFileLexiR10(uCellPrivateInstance_t *pInstance, const char *pFileName)
{

    int32_t fileHandle = 0;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    char *pBuffer = NULL;
    int32_t errorCode = U_ERROR_COMMON_NO_MEMORY;

    pBuffer = (char *) pUPortMalloc(pInstance->pModule->cellFileNameMaxLength + 1);
    if (pBuffer != NULL) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+FOPEN?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+FOPEN: ");
        // file name
        uAtClientReadString(atHandle, pBuffer, sizeof(pBuffer), false);
        // read the file handle
        fileHandle = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if ((errorCode == 0) && (fileHandle > 0) && (strcmp(pBuffer, pFileName) == 0)) {
            // File is already open
            errorCode = fileHandle;
        } else {
            // Open the file
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+FOPEN=");
            // Write file name
            uAtClientWriteString(atHandle, pFileName, true);
            // Mode to open the file:
            uAtClientWriteInt(atHandle, 0);
            uAtClientCommandStop(atHandle);
            // Wait for the file handle to be received in response.
            uAtClientResponseStart(atHandle, "+FOPEN: ");
            fileHandle = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                errorCode = fileHandle;
            }
        }
        uPortFree(pBuffer);
    }
    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uCellFilePrivateLink()
{
    //dummy
}

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
    int32_t bytesWritten = 0;
    int32_t fileHandle = -1;
    int32_t timeout = 10000;
    int32_t offset = 0;
    int32_t thisSize = 0;
    int32_t chunkLength = 254;
    size_t totalBytesWritten = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= pInstance->pModule->cellFileNameMaxLength)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LEXI_R10) {
                fileHandle = openFileLexiR10(pInstance, pFileName);
                if (fileHandle > 0 ) {
                    // If the file already exists then we need to append data at the end.
                    // So check its length in bytes and seek to that position.

                    // Get the file size
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+ULSTFILE=2,");
                    uAtClientWriteString(atHandle, pFileName, true);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+ULSTFILE: ");
                    offset = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    // Jump to the end of the file
                    uAtClientCommandStart(atHandle, "AT+FSEEK=");
                    uAtClientWriteInt(atHandle, fileHandle);
                    uAtClientWriteInt(atHandle, offset);
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientUnlock(atHandle);

                    // Data is encoded as hex, so two characters per byte
                    dataSize = dataSize * 2;
                    while ((dataSize > 0) && (thisSize >= 0) &&
                           (bytesWritten >= 0)) {
                        thisSize = dataSize;
                        if (thisSize > (int32_t) chunkLength) {
                            thisSize = (int32_t) chunkLength;
                        }
                        if (thisSize > 0) {
                            uAtClientLock(atHandle);
                            uAtClientCommandStart(atHandle, "AT+FWRITEHEX=");
                            uAtClientWriteInt(atHandle, fileHandle);
                            uAtClientWriteInt(atHandle, thisSize);
                            uAtClientCommandStop(atHandle);
                            // Wait for "CONNECT" to come.
                            uAtClientResponseStart(atHandle, "CONNECT");
                            // Deliberately no uAtClientResponseStop() here.
                            uAtClientCommandStart(atHandle, "");
                            uAtClientWriteHexData(atHandle, (const uint8_t *) pData,
                                                  (uint8_t)(thisSize / 2)); // length in bytes
                            uAtClientCommandStop(atHandle);
                            uAtClientResponseStart(atHandle, "+FWRITE: ");
                            bytesWritten = uAtClientReadInt(atHandle);
                            // Skip the total file size
                            uAtClientSkipParameters(atHandle, 1);
                            uAtClientResponseStop(atHandle);
                            if (bytesWritten > 0) {
                                pData += bytesWritten;
                                dataSize -= bytesWritten * 2; // dataSize is in hex characters
                                totalBytesWritten += bytesWritten;
                            }
                            errorCode = uAtClientUnlock(atHandle);
                        }
                    }
                    if (errorCode == 0) {
                        // Return the total bytes written
                        errorCode = (int32_t) totalBytesWritten;
                    }
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+FCLOSE=");
                    uAtClientWriteInt(atHandle, fileHandle);
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientUnlock(atHandle);
                } else {
                    // Opening the file failed
                    errorCode = fileHandle;
                }
            } else {
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
                    uAtClientTimeoutSet(atHandle, timeout);
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
    char buffer[20];
    int32_t fileHandle = -1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= pInstance->pModule->cellFileNameMaxLength)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;

            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LEXI_R10) {
                // Get the file size
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+ULSTFILE=2,");
                uAtClientWriteString(atHandle, pFileName, true);
                uAtClientCommandStop(atHandle);
                // Wait for the file size to be received in response.
                uAtClientResponseStart(atHandle, "+ULSTFILE: ");
                indicatedReadSize = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if (uAtClientUnlock(atHandle) == 0) {
                    if (indicatedReadSize > (int32_t)dataSize) {
                        // If the requested data length is less than
                        // the file size. Read the requested data bytes.
                        indicatedReadSize = dataSize;
                    }
                    // Open the file
                    fileHandle = openFileLexiR10(pInstance, pFileName);
                    if (fileHandle > 0) {
                        // Read the file
                        uAtClientLock(atHandle);
                        // Use the file handle to read from the file
                        uAtClientCommandStart(atHandle, "AT+FREAD=");
                        uAtClientWriteInt(atHandle, fileHandle);
                        uAtClientWriteInt(atHandle, indicatedReadSize);
                        uAtClientCommandStop(atHandle);
                        // Wait for "CONNECT <size>" to come and skip it.
                        snprintf(buffer, sizeof(buffer), "CONNECT %d\r\n", (int) indicatedReadSize);
                        uAtClientResponseStart(atHandle, buffer);
                        // Don't stop for anything!
                        uAtClientIgnoreStopTag(atHandle);
                        readSize = uAtClientReadBytes(atHandle, pData,
                                                      (size_t)  (unsigned) indicatedReadSize,
                                                      true);
                        // Make sure to wait for the stop tag before
                        // we finish
                        uAtClientRestoreStopTag(atHandle);
                        uAtClientResponseStop(atHandle);
                        if (uAtClientUnlock(atHandle) == 0) {
                            errorCode = readSize;
                        }
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+FCLOSE=");
                        uAtClientWriteInt(atHandle, fileHandle);
                        uAtClientCommandStopReadResponse(atHandle);
                        uAtClientUnlock(atHandle);
                    } else {
                        // Opening the file failed
                        errorCode = fileHandle;
                    }
                }
            } else {
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
    int32_t fileHandle = 0;
    char buffer[20];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if ((pInstance != NULL) && (pData !=  NULL) && (pFileName != NULL) &&
            (strlen(pFileName) <= pInstance->pModule->cellFileNameMaxLength)) {

            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Use of tags is not supported by any of the modules
            // we support for block reads
            if (pInstance->pFileSystemTag == NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LEXI_R10) {
                    // Get the file size
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+ULSTFILE=2,");
                    uAtClientWriteString(atHandle, pFileName, true);
                    uAtClientCommandStop(atHandle);
                    // Wait for the file size to be received in response.
                    uAtClientResponseStart(atHandle, "+ULSTFILE: ");
                    indicatedReadSize = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    uAtClientUnlock(atHandle);
                    if (indicatedReadSize >= (int32_t)(dataSize + offset)) {
                        // Within the file range, we are all fine.
                        indicatedReadSize = dataSize;
                    } else {
                        // The requested data size is large than the
                        // file size.
                        if (indicatedReadSize >= (int32_t) offset) {
                            indicatedReadSize -= offset;
                        } else {
                            // error condition.
                            uPortLog("U_CELL_FILE: incompatible file read request sizes.");
                            uPortLog("Offset (%d) is larger than file size (%d).\n", offset, indicatedReadSize);
                        }
                    }
                    // Open the file
                    fileHandle = openFileLexiR10(pInstance, pFileName);
                    if (fileHandle > 0) {
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+FSEEK=");
                        uAtClientWriteInt(atHandle, fileHandle);
                        uAtClientWriteInt(atHandle, offset);
                        uAtClientCommandStopReadResponse(atHandle);
                        uAtClientUnlock(atHandle);

                        // Read the file
                        uAtClientLock(atHandle);
                        // Use the file handle to read from the file
                        uAtClientCommandStart(atHandle, "AT+FREAD=");
                        uAtClientWriteInt(atHandle, fileHandle);
                        uAtClientWriteInt(atHandle, indicatedReadSize);
                        uAtClientCommandStop(atHandle);
                        // Wait for "CONNECT <size>" to come and skip it.
                        snprintf(buffer, sizeof(buffer), "CONNECT %d\r\n", (int) indicatedReadSize);
                        uAtClientResponseStart(atHandle, buffer);
                        // Don't stop for anything!
                        uAtClientIgnoreStopTag(atHandle);
                        readSize = uAtClientReadBytes(atHandle, pData,
                                                      (size_t)  (unsigned) indicatedReadSize,
                                                      true);
                        // Make sure to wait for the stop tag before
                        // we finish
                        uAtClientRestoreStopTag(atHandle);
                        uAtClientResponseStop(atHandle);
                        if (uAtClientUnlock(atHandle) == 0) {
                            errorCode = readSize;
                        }
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+FCLOSE=");
                        uAtClientWriteInt(atHandle, (int32_t) fileHandle);
                        uAtClientCommandStopReadResponse(atHandle);
                        uAtClientUnlock(atHandle);
                    } else {
                        // Opening the file failed
                        errorCode = fileHandle;
                    }
                } else {
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
