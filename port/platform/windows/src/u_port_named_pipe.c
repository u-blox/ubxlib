/*
 * Copyright 2024 u-blox
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

/** @file
 * @brief Implementation of named pipes on the Windows platform.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <windows.h>

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_named_pipe.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    char *name;
    HANDLE hPipe;
    bool creator;
} uPortNamePipe_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Create the named pipe on the server side.
static bool createThePipe(uPortNamePipe_t *pPipe)
{
    pPipe->hPipe = CreateNamedPipe(pPipe->name,
                                   PIPE_ACCESS_DUPLEX,       // Pipe open mode
                                   PIPE_TYPE_MESSAGE |       // Pipe message type pipe
                                   PIPE_READMODE_MESSAGE |   // Pipe message-read mode
                                   PIPE_WAIT,                // Pipe blocking mode
                                   PIPE_UNLIMITED_INSTANCES, // Max. instances
                                   0,   // no outbound buffer
                                   0,   // no inbound buffer
                                   0,   // use default wait time
                                   NULL // use default security attributes
                                  );
    return pPipe->hPipe != INVALID_HANDLE_VALUE;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uPortNamedPipeCreate(uPortNamePipeHandle_t *pPipeHandle, const char *pName, bool server)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    if ((pPipeHandle != NULL) && (pName != NULL)) {
        errorCode = U_ERROR_COMMON_NO_MEMORY;
        uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pUPortMalloc(sizeof(uPortNamePipe_t));
        if (pPipe != NULL) {
            const char *prefix = "\\\\.\\pipe\\";
            size_t len = strlen(prefix) + strlen(pName) + 1;
            pPipe->name = (char *)pUPortMalloc(len);
            if (pPipe->name != NULL) {
                errorCode = U_ERROR_COMMON_SUCCESS;
                pPipe->creator = server;
                snprintf(pPipe->name, len, "%s%s", prefix, pName);
                if (server) {
                    createThePipe(pPipe);
                } else {
                    // Wait for pipe to become available.
                    do {
                        pPipe->hPipe = CreateFile(pPipe->name,
                                                  GENERIC_READ | GENERIC_WRITE,
                                                  0,             // No sharing
                                                  NULL,          // Use default security attributes
                                                  OPEN_EXISTING,
                                                  0,             // No attributes
                                                  NULL           // No template
                                                 );
                        if (pPipe->hPipe != INVALID_HANDLE_VALUE) {
                            break;
                        }
                        uPortTaskBlock(1000);
                    } while (GetLastError() == ERROR_FILE_NOT_FOUND);
                }
                if (pPipe->hPipe == INVALID_HANDLE_VALUE) {
                    errorCode = U_ERROR_COMMON_PLATFORM;
                }
            }
        }
        if (errorCode == U_ERROR_COMMON_SUCCESS) {
            *pPipeHandle = pPipe;
        } else {
            if (pPipe != NULL) {
                if (pPipe->name != NULL) {
                    uPortFree(pPipe->name);
                }
                uPortFree(pPipe);
            }
        }
    }
    return (int32_t)errorCode;
}

int32_t uPortNamedPipeWriteStr(uPortNamePipeHandle_t pipeHandle, const char *pStr)
{
    if ((pipeHandle == NULL) || (pStr == NULL)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    int32_t errorOrLength = (int32_t)U_ERROR_COMMON_SUCCESS;
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    int32_t bytesWritten;
    if (WriteFile(pPipe->hPipe, pStr, strlen(pStr) + 1, &bytesWritten, NULL)) {
        errorOrLength = 0;
    } else {
        errorOrLength = (int32_t)U_ERROR_COMMON_PLATFORM;
    }
    return errorOrLength;
}

int32_t uPortNamedPipeReadStr(uPortNamePipeHandle_t pipeHandle, char *pStr, size_t maxLength)
{
    if ((pipeHandle == NULL) || (pStr == NULL) || (maxLength < 1)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    int32_t errorOrLength = (int32_t)U_ERROR_COMMON_SUCCESS;
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    if (pPipe->creator) {
        // Wait for connection if the pipe exists, otherwise recreate it.
        while (!ConnectNamedPipe(pPipe->hPipe, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_PIPE_CONNECTED) {
                break;
            } else if (err == ERROR_NO_DATA) {
                CloseHandle(pPipe->hPipe);
                if (!createThePipe(pPipe)) {
                    return (int32_t)U_ERROR_COMMON_PLATFORM;
                }
            } else {
                return (int32_t)U_ERROR_COMMON_PLATFORM;
            }
        }
    }
    memset(pStr, 0, maxLength);
    int32_t bytesRead;
    if (ReadFile(pPipe->hPipe, pStr, maxLength - 1, &bytesRead, NULL)) {
        errorOrLength = bytesRead;
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE) {
            // The pipe has been closed, just return empty string.
            errorOrLength = 0;
        } else {
            errorOrLength = U_ERROR_COMMON_PLATFORM;
        }
    }
    return errorOrLength;
}

int32_t uPortNamedPipeDelete(uPortNamePipeHandle_t pipeHandle)
{
    if (pipeHandle == NULL) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    CloseHandle(pPipe->hPipe);
    uPortFree(pPipe->name);
    uPortFree(pPipe);
    return 0;
}
// End of file
