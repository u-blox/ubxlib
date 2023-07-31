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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Stubs to allow the HTTP Client API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when cellular is not included in the build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_at_client.h"
#include "u_device.h"
#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_http.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uCellFileWrite(uDeviceHandle_t cellHandle,
                              const char *pFileName,
                              const char *pData,
                              size_t dataSize)
{
    (void) cellHandle;
    (void) pFileName;
    (void) pData;
    (void) dataSize;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellFileBlockRead(uDeviceHandle_t cellHandle,
                                  const char *pFileName,
                                  char *pData,
                                  size_t offset,
                                  size_t dataSize)
{
    (void) cellHandle;
    (void) pFileName;
    (void) pData;
    (void) offset;
    (void) dataSize;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellFileDelete(uDeviceHandle_t cellHandle,
                               const char *pFileName)
{
    (void) cellHandle;
    (void) pFileName;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellFileListFirst_r(uDeviceHandle_t cellHandle,
                                    char *pFileName, void **ppRentrant)
{
    (void) cellHandle;
    (void) pFileName;
    (void) ppRentrant;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellFileListNext_r(char *pFileName, void **ppRentrant)
{
    (void) pFileName;
    (void) ppRentrant;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uCellFileListLast_r(void **ppRentrant)
{
    (void) ppRentrant;
}

U_WEAK int32_t uCellAtClientHandleGet(uDeviceHandle_t cellHandle,
                                      uAtClientHandle_t *pAtHandle)
{
    (void) cellHandle;
    (void) pAtHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellHttpOpen(uDeviceHandle_t cellHandle, const char *pServerName,
                             const char *pUserName, const char *pPassword,
                             int32_t timeoutSeconds, uCellHttpCallback_t *pCallback,
                             void *pCallbackParam)
{
    (void) cellHandle;
    (void) pServerName;
    (void) pUserName;
    (void) pPassword;
    (void) timeoutSeconds;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uCellHttpClose(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    (void) cellHandle;
    (void) httpHandle;
}

U_WEAK int32_t uCellHttpSetSecurityOn(uDeviceHandle_t cellHandle, int32_t httpHandle,
                                      int32_t securityProfileId)
{
    (void) cellHandle;
    (void) httpHandle;
    (void) securityProfileId;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellHttpSetSecurityOff(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    (void) cellHandle;
    (void) httpHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellHttpRequest(uDeviceHandle_t cellHandle, int32_t httpHandle,
                                uCellHttpRequest_t requestType,
                                const char *pPath, const char *pFileNameResponse,
                                const char *pStrPost, const char *pContentTypePost)
{
    (void) cellHandle;
    (void) httpHandle;
    (void) requestType;
    (void) pPath;
    (void) pFileNameResponse;
    (void) pStrPost;
    (void) pContentTypePost;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellHttpRequestFile(uDeviceHandle_t cellHandle, int32_t httpHandle,
                                    uCellHttpRequest_t requestType,
                                    const char *pPath, const char *pFileNameResponse,
                                    const char *pFileNamePutPost,
                                    const char *pContentTypePutPost)
{
    (void) cellHandle;
    (void) httpHandle;
    (void) requestType;
    (void) pPath;
    (void) pFileNameResponse;
    (void) pFileNamePutPost;
    (void) pContentTypePutPost;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uCellHttpGetLastErrorCode(uDeviceHandle_t cellHandle, int32_t httpHandle)
{
    (void) cellHandle;
    (void) httpHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// This used only in the test code, but still needs a U_WEAK
// version for that case.
U_WEAK int32_t uCellPwrReboot(uDeviceHandle_t cellHandle,
                              bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    (void) cellHandle;
    (void) pKeepGoingCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
