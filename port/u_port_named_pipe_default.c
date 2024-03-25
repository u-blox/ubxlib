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
 * @brief Default (empty) Implementation of named pipes.
 */

#include "stddef.h" // NULL, size_t etc.
#include "stdint.h" // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_port_named_pipe.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uPortNamedPipeCreate(uPortNamePipeHandle_t *pPipeHandle, const char *pName,
                                    bool server)
{
    (void)pPipeHandle;
    (void)pName;
    (void)server;
    return (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uPortNamedPipeWriteStr(uPortNamePipeHandle_t pipeHandle, const char *pStr)
{
    (void)pipeHandle;
    (void)pStr;
    return (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uPortNamedPipeReadStr(uPortNamePipeHandle_t pipeHandle, char *pStr, size_t maxLength)
{
    (void)pipeHandle;
    (void)pStr;
    (void)maxLength;
    return (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uPortNamedPipeDelete(uPortNamePipeHandle_t pipeHandle)
{
    (void)pipeHandle;
    return (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
}
// End of file
