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
 * @brief Stubs to allow the sockets API to be compiled without cellular;
 * if you call a cellular API function from the source code here you must
 * also include a weak stub for it which will return
 * minus #U_SOCK_ENOSYS when cellular is not included in the build.
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
#include "u_cell_sock.h"

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

U_WEAK int32_t uCellSockInit()
{
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockInitInstance(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
    return -U_SOCK_ENOSYS;
}

U_WEAK void uCellSockDeinit()
{
}

U_WEAK int32_t uCellSockCreate(uDeviceHandle_t cellHandle,
                               uSockType_t type,
                               uSockProtocol_t protocol)
{
    (void) cellHandle;
    (void) type;
    (void) protocol;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockConnect(uDeviceHandle_t cellHandle,
                                int32_t sockHandle,
                                const uSockAddress_t *pRemoteAddress)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockClose(uDeviceHandle_t cellHandle,
                              int32_t sockHandle,
                              void (*pCallback) (uDeviceHandle_t,
                                                 int32_t))
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pCallback;
    return -U_SOCK_ENOSYS;
}

U_WEAK void uCellSockCleanup(uDeviceHandle_t cellHandle)
{
    (void) cellHandle;
}

U_WEAK void uCellSockBlockingSet(uDeviceHandle_t cellHandle,
                                 int32_t sockHandle,
                                 bool isBlocking)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) isBlocking;
}

U_WEAK int32_t uCellSockOptionSet(uDeviceHandle_t cellHandle,
                                  int32_t sockHandle,
                                  int32_t level,
                                  uint32_t option,
                                  const void *pOptionValue,
                                  size_t optionValueLength)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) level;
    (void) option;
    (void) pOptionValue;
    (void) optionValueLength;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockOptionGet(uDeviceHandle_t cellHandle,
                                  int32_t sockHandle,
                                  int32_t level,
                                  uint32_t option,
                                  void *pOptionValue,
                                  size_t *pOptionValueLength)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) level;
    (void) option;
    (void) pOptionValue;
    (void) pOptionValueLength;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockSecure(uDeviceHandle_t cellHandle,
                               int32_t sockHandle,
                               int32_t profileId)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) profileId;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockSetNextLocalPort(uDeviceHandle_t cellHandle,
                                         int32_t port)
{
    (void) cellHandle;
    (void) port;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockSendTo(uDeviceHandle_t cellHandle,
                               int32_t sockHandle,
                               const uSockAddress_t *pRemoteAddress,
                               const void *pData, size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockReceiveFrom(uDeviceHandle_t cellHandle,
                                    int32_t sockHandle,
                                    uSockAddress_t *pRemoteAddress,
                                    void *pData, size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pRemoteAddress;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockWrite(uDeviceHandle_t cellHandle,
                              int32_t sockHandle,
                              const void *pData, size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockRead(uDeviceHandle_t cellHandle,
                             int32_t sockHandle,
                             void *pData, size_t dataSizeBytes)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pData;
    (void) dataSizeBytes;
    return -U_SOCK_ENOSYS;
}

U_WEAK void uCellSockRegisterCallbackData(uDeviceHandle_t cellHandle,
                                          int32_t sockHandle,
                                          void (*pCallback) (uDeviceHandle_t,
                                                             int32_t))
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pCallback;
}

U_WEAK void uCellSockRegisterCallbackClosed(uDeviceHandle_t cellHandle,
                                            int32_t sockHandle,
                                            void (*pCallback) (uDeviceHandle_t,
                                                               int32_t))
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pCallback;
}

U_WEAK int32_t uCellSockGetHostByName(uDeviceHandle_t cellHandle,
                                      const char *pHostName,
                                      uSockIpAddress_t *pHostIpAddress)
{
    (void) cellHandle;
    (void) pHostName;
    (void) pHostIpAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockGetLocalAddress(uDeviceHandle_t cellHandle,
                                        int32_t sockHandle,
                                        uSockAddress_t *pLocalAddress)
{
    (void) cellHandle;
    (void) sockHandle;
    (void) pLocalAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockBind(uDeviceHandle_t devHandle,
                             int32_t sockHandle,
                             const uSockAddress_t *pLocalAddress)
{
    (void)devHandle;
    (void)sockHandle;
    (void)pLocalAddress;
    return -U_SOCK_ENOSYS;
}

U_WEAK int32_t uCellSockListen(uDeviceHandle_t devHandle,
                               int32_t sockHandle,
                               size_t backlog)
{
    (void)devHandle;
    (void)sockHandle;
    (void)backlog;
    return U_SOCK_ENONE;
}

U_WEAK int32_t uCellSockAccept(uDeviceHandle_t devHandle,
                               int32_t sockHandle,
                               uSockAddress_t *pRemoteAddress)
{
    (void)devHandle;
    (void)sockHandle;
    (void)pRemoteAddress;
    return U_SOCK_ENONE;
}

// End of file
