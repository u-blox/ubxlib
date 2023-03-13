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
 * @brief Stubs to allow the HTTP Client API to be compiled without Wi-Fi;
 * if you call a Wi-Fi API function from the source code here you must
 * also include a weak stub for it which will return
 * #U_ERROR_COMMON_NOT_SUPPORTED when Wi-Fi is not included in the build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h" // U_WEAK
#include "u_error_common.h"
#include "u_device.h"
#include "u_wifi_http.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uWifiHttpOpen(uDeviceHandle_t wifiHandle, const char *pServerName,
                             const char *pUserName, const char *pPassword,
                             int32_t timeoutSeconds, uWifiHttpCallback_t *pCallback,
                             void *pCallbackParam)
{
    (void) wifiHandle;
    (void) pServerName;
    (void) pUserName;
    (void) pPassword;
    (void) timeoutSeconds;
    (void) pCallback;
    (void) pCallbackParam;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK void uWifiHttpClose(uDeviceHandle_t wifiHandle, int32_t httpHandle)
{
    (void) wifiHandle;
    (void) httpHandle;
}

U_WEAK int32_t uWifiHttpRequest(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                                uWifiHttpRequest_t requestType, const char *pPath,
                                const char *pContent, const char *pContentType)
{
    (void) wifiHandle;
    (void) httpHandle;
    (void) requestType;
    (void) pPath;
    (void) pContent;
    (void) pContentType;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiHttpRequestEx(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                                  uWifiHttpRequest_t requestType, const char *pPath,
                                  const char *pData, size_t contentLength, const char *pContentType)
{
    (void) wifiHandle;
    (void) httpHandle;
    (void) requestType;
    (void) pPath;
    (void) pData;
    (void) contentLength;
    (void) pContentType;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

U_WEAK int32_t uWifiHttpGetLastErrorCode(uDeviceHandle_t wifiHandle,
                                         int32_t httpHandle)
{
    (void) wifiHandle;
    (void) httpHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
