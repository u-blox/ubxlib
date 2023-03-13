/*
 * Copyright 2019-2023 u-blox
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
 * @brief Stubs to allow the AT client to compile without short-range.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_short_range_pbuf.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

//lint -esym(593, pParam) Suppress pParam not being freed here
U_WEAK int32_t uShortRangeEdmStreamAtCallbackSet(int32_t handle,
                                                 uEdmAtEventCallback_t pFunction,
                                                 void *pParam)
{
    (void) handle;
    (void) pFunction;
    (void) pParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uShortRangeEdmStreamAtCallbackRemove(int32_t handle)
{
    (void) handle;
}

U_WEAK void uShortRangeEdmStreamIpEventCallbackRemove(int32_t handle)
{
    (void) handle;
}

U_WEAK int32_t uShortRangeEdmStreamAtRead(int32_t handle, void *pBuffer,
                                          size_t sizeBytes)
{
    (void) handle;
    (void) pBuffer;
    (void) sizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uShortRangeEdmStreamAtEventSend(int32_t handle, uint32_t eventBitMap)
{
    (void) handle;
    (void) eventBitMap;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK bool uShortRangeEdmStreamAtEventIsCallback(int32_t handle)
{
    (void) handle;
    return false;
}

U_WEAK int32_t uShortRangeEdmStreamAtEventStackMinFree(int32_t handle)
{
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uShortRangeEdmStreamAtGetReceiveSize(int32_t handle)
{
    (void) handle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
