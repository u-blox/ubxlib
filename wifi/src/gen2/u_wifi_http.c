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
 * @brief Implementation of the u-blox HTTP client API for Wi-Fi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // strlen()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_APP_TASK_PRIORITY and U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_event_queue.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"         // Order is important

#include "u_wifi_http.h"
#include "u_wifi_http_private.h"
#include "u_wifi_private.h"

#include "u_http_client.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_hex_bin_convert.h"

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
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiHttpPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO HTTP WIFI
 * -------------------------------------------------------------- */

// Process a URC containing an HTTP response.
bool uWifiHttpPrivateUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    (void)atHandle;
    (void)pParameter;
    return false;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// ##### NOT supported in uCx yet! #####

// Open a Wi-Fi HTTP client instance.
int32_t uWifiHttpOpen(uDeviceHandle_t wifiHandle, const char *pServerName,
                      const char *pUserName, const char *pPassword,
                      int32_t timeoutSeconds, uWifiHttpCallback_t *pCallback,
                      void *pCallbackParam)
{
    (void)wifiHandle;
    (void)pServerName;
    (void)pUserName;
    (void)pPassword;
    (void)timeoutSeconds;
    (void)pCallback;
    (void)pCallbackParam;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// Shut-down the given Wi-Fi HTTP client instance.
void uWifiHttpClose(uDeviceHandle_t wifiHandle, int32_t httpHandle)
{
}

// Perform an HTTP request. Primary used for GET and DELETE
int32_t uWifiHttpRequest(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                         uWifiHttpRequest_t requestType, const char *pPath,
                         const char *pContent, const char *pContentType)
{
    (void)wifiHandle;
    (void)httpHandle;
    (void)requestType;
    (void)pPath;
    (void)pContent;
    (void)pContentType;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// Perform an extended HTTP request. Primary for POST,PUT,PATCH,OPTIONS and GET_BINARY
int32_t uWifiHttpRequestEx(uDeviceHandle_t wifiHandle, int32_t httpHandle,
                           uWifiHttpRequest_t requestType, const char *pPath,
                           const char *pData, size_t contentLength, const char *pContentType)
{
    (void)wifiHandle;
    (void)httpHandle;
    (void)requestType;
    (void)pPath;
    (void)pData;
    (void)contentLength;
    (void)pContentType;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_NOT_IMPLEMENTED;

    return errorCode;
}

// End of file
