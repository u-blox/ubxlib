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
 * @brief Implementation of named pipes on the Linux platform.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

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
    bool creator;
} uPortNamePipe_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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
            const char *prefix = "/tmp/";
            size_t len = strlen(prefix) + strlen(pName) + 1;
            pPipe->name = (char *)pUPortMalloc(len);
            if (pPipe->name != NULL) {
                errorCode = U_ERROR_COMMON_SUCCESS;
                pPipe->creator = server;
                snprintf(pPipe->name, len, "%s%s", prefix, pName);
                if (server) {
                    errno = 0;
                    mkfifo(pPipe->name, 0666);
                    if (!((errno == 0) || (errno == EEXIST))) {
                        errorCode = U_ERROR_COMMON_PLATFORM;
                    }
                    pPipe->creator = errno == 0;
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
    int32_t result;
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    int fd = open(pPipe->name, O_WRONLY);
    if (fd >= 0) {
        result = write(fd, pStr, strlen(pStr) + 1);
        if (result > 0) {
            result = 0;
        }
        close(fd);
    } else {
        result = fd;
    }
    return result;
}

int32_t uPortNamedPipeReadStr(uPortNamePipeHandle_t pipeHandle, char *pStr, size_t maxLength)
{
    if ((pipeHandle == NULL) || (pStr == NULL) || (maxLength < 1)) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    int32_t result;
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    int fd = open(pPipe->name, O_RDONLY);
    if (fd >= 0) {
        memset(pStr, 0, maxLength);
        result = read(fd, pStr, maxLength - 1);
        close(fd);
    } else {
        result = fd;
    }
    return result;
}

int32_t uPortNamedPipeDelete(uPortNamePipeHandle_t pipeHandle)
{
    if (pipeHandle == NULL) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    uPortNamePipe_t *pPipe = (uPortNamePipe_t *)pipeHandle;
    if (pPipe->creator) {
        unlink(pPipe->name);
    }
    uPortFree(pPipe->name);
    uPortFree(pPipe);
    return 0;
}
// End of file
