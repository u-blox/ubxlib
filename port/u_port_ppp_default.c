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

/** @file
 * @brief Default implementations of uPortPppAttach(), uPortPppConnect(),
 * uPortPppDisconnect() and uPortPppDetach() which simply return
 * #U_ERROR_COMMON_NOT_SUPPORTED.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h"  // U_WEAK

#include "u_error_common.h"

#include "u_port_ppp.h"

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

void uPortPppDefaultPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Attach a PPP interface to the bottom of the IP stack of a platform.
U_WEAK int32_t uPortPppAttach(void *pDevHandle,
                              uPortPppConnectCallback_t *pConnectCallback,
                              uPortPppDisconnectCallback_t *pDisconnectCallback,
                              uPortPppTransmitCallback_t *pTransmitCallback)
{
    (void) pDevHandle;
    (void) pConnectCallback;
    (void) pDisconnectCallback;
    (void) pTransmitCallback;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Bring a previously attached PPP interface up.
U_WEAK int32_t uPortPppConnect(void *pDevHandle,
                               uSockIpAddress_t *pIpAddress,
                               uSockIpAddress_t *pDnsIpAddressPrimary,
                               uSockIpAddress_t *pDnsIpAddressSecondary,
                               const char *pUsername,
                               const char *pPassword,
                               uPortPppAuthenticationMode_t authenticationMode)
{
    (void) pDevHandle;
    (void) pIpAddress;
    (void) pDnsIpAddressPrimary;
    (void) pDnsIpAddressSecondary;
    (void) pUsername;
    (void) pPassword;
    (void) authenticationMode;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Reconnect a PPP interface after it was lost.
U_WEAK int32_t uPortPppReconnect(void *pDevHandle,
                                 uSockIpAddress_t *pIpAddress)
{
    (void) pDevHandle;
    (void) pIpAddress;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Take a previously attached PPP interface down.
U_WEAK int32_t uPortPppDisconnect(void *pDevHandle)
{
    (void) pDevHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Detach a PPP interface from the bottom of a platform's IP stack.
U_WEAK int32_t uPortPppDetach(void *pDevHandle)
{
    (void) pDevHandle;
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// End of file
