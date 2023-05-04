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
 * @brief Stubs to allow the sockets API to be compiled without Wi-Fi;
 * if you call a Wi-Fi API function from the source code here you must
 * also include a weak stub for it which will return
 * minus #U_SOCK_ENOSYS when Wi-Fi is not included in the build.
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
#include "u_sock_errno.h"
#include "u_sock.h"
#include "u_wifi_sock.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uWifiSockInit(void)
{
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockInitInstance(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockDeinitInstance(uDeviceHandle_t devHandle)
{
    (void) devHandle;
    return -U_SOCK_ENOSYS;
}

U_WEAK void uWifiSockDeinit()
{
}

U_WEAK int32_t uWifiSockCreate(uDeviceHandle_t devHandle,
                               uSockType_t type,
                               uSockProtocol_t protocol)
{
    (void) devHandle;
    (void) type;
    (void) protocol;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockConnect(uDeviceHandle_t devHandle,
                                int32_t sockHandle,
                                const uSockAddress_t *pRemoteAddress)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockClose(uDeviceHandle_t devHandle,
                              int32_t sockHandle,
                              uWifiSockCallback_t pCallback)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pCallback;
    return -U_SOCK_ENOSYS;
}

U_WEAK void uWifiSockCleanup(uDeviceHandle_t devHandle)
{
    (void) devHandle;
}

U_WEAK void uWifiSockBlockingSet(uDeviceHandle_t devHandle,
                                 int32_t sockHandle,
                                 bool isBlocking)
{
    (void) devHandle;
    (void) sockHandle;
    (void) isBlocking;
}

U_WEAK int32_t uWifiSockOptionSet(uDeviceHandle_t devHandle,
                                  int32_t sockHandle,
                                  int32_t level,
                                  uint32_t option,
                                  const void *pOptionValue,
                                  size_t optionValueLength)
{
    (void) devHandle;
    (void) sockHandle;
    (void) level;
    (void) option;
    (void) pOptionValue;
    (void) optionValueLength;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockOptionGet(uDeviceHandle_t devHandle,
                                  int32_t sockHandle,
                                  int32_t level,
                                  uint32_t option,
                                  void *pOptionValue,
                                  size_t *pOptionValueLength)
{
    (void) devHandle;
    (void) sockHandle;
    (void) level;
    (void) option;
    (void) pOptionValue;
    (void) pOptionValueLength;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockSetNextLocalPort(uDeviceHandle_t devHandle, int32_t port)
{
    (void) devHandle;
    (void) port;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockWrite(uDeviceHandle_t devHandle,
                              int32_t sockHandle,
                              const void *pData, size_t dataSizeBytes)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockRead(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             void *pData, size_t dataSizeBytes)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockSendTo(uDeviceHandle_t devHandle,
                               int32_t sockHandle,
                               const uSockAddress_t *pRemoteAddress,
                               const void *pData,
                               size_t dataSizeBytes)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockReceiveFrom(uDeviceHandle_t devHandle,
                                    int32_t sockHandle,
                                    uSockAddress_t *pRemoteAddress,
                                    void *pData, size_t dataSizeBytes)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockRegisterCallbackData(uDeviceHandle_t devHandle,
                                             int32_t sockHandle,
                                             uWifiSockCallback_t pCallback)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pCallback;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockRegisterCallbackClosed(uDeviceHandle_t devHandle,
                                               int32_t sockHandle,
                                               uWifiSockCallback_t pCallback)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pCallback;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockGetHostByName(uDeviceHandle_t devHandle,
                                      const char *pHostName,
                                      uSockIpAddress_t *pHostIpAddress)
{
    (void) devHandle;
    (void) pHostName;
    (void) pHostIpAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockGetLocalAddress(uDeviceHandle_t devHandle,
                                        int32_t sockHandle,
                                        uSockAddress_t *pLocalAddress)
{
    (void) devHandle;
    (void) sockHandle;
    (void) pLocalAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockBind(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             const uSockAddress_t *pLocalAddress)
{
    (void)devHandle;
    (void)sockHandle;
    (void)pLocalAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uWifiSockListen(uDeviceHandle_t devHandle,
                               int32_t sockHandle,
                               size_t backlog)
{
    (void)devHandle;
    (void)sockHandle;
    (void)backlog;
    return U_SOCK_ENONE;
}

U_WEAK int32_t uWifiSockAccept(uDeviceHandle_t devHandle,
                               int32_t sockHandle,
                               uSockAddress_t *pRemoteAddress)
{
    (void)devHandle;
    (void)sockHandle;
    (void)pRemoteAddress;
    return U_SOCK_ENONE;
}

// End of file
