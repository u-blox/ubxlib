/*
 * Copyright 2023 u-blox
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
 * @brief Implementation of the DNS server intended e.g. for a captive portal
 */

#ifdef U_CFG_OVERRIDE
#include "u_cfg_override.h"  // For a customer's configuration override
#endif

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "stdio.h"
#include "limits.h"

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_os.h"
#include "u_cfg_sw.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_cfg.h"

#include "u_network.h"
#include "u_sock_errno.h"
#include "u_sock.h"

#include "u_dns_server.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// DNS response time-to-live in seconds
#ifndef U_DNS_TTL
# define U_DNS_TTL 600
#endif

#define DNS_QR_QUERY      0
#define DNS_QR_RESPONSE   1
#define DNS_OPCODE_QUERY  0
#define DNS_DEFAULT_TTL  60

#define DNS_NO_ERROR      0
#define DNS_FORM_ERROR    1
#define DNS_NOTIMPL_ERROR 4

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// DNS request/response header
typedef struct {
    uint16_t ID;               // identification number
    unsigned char RD : 1;      // recursion desired
    unsigned char TC : 1;      // truncated message
    unsigned char AA : 1;      // authoritive answer
    unsigned char OPCode : 4;  // message_type
    unsigned char QR : 1;      // query/response flag
    unsigned char RCode : 4;   // response code
    unsigned char Z : 3;       // its z! reserved
    unsigned char RA : 1;      // recursion available
    uint16_t QDCount;          // number of question entries
    uint16_t ANCount;          // number of answer entries
    uint16_t NSCount;          // number of authority entries
    uint16_t ARCount;          // number of resource entries
} uDnsHeader_t;


/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* Local implementation of endian conversion routines in order to
   avoid possible compiler or os differences. */

static bool isLe()
{
    int32_t x = 1;
    return (*((char *)(&x)) == 1);
}

static uint16_t ntohs(uint16_t x)
{
    uint16_t retVal = x;
    if (isLe()) {
        retVal = (x & 0xFF00) >> 8;
        retVal += (x & 0x00FF) << 8;
    }
    return retVal;
}

static uint32_t htonl(uint32_t x)
{
    uint32_t retVal = x;
    if (isLe()) {
        retVal = (x & 0xFF000000) >> 24;
        retVal += (x & 0x00FF0000) >> 8;
        retVal += (x & 0x0000FF00) << 8;
        retVal += (x & 0x000000FF) << 24;
    }
    return retVal;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uDnsServer(uDeviceHandle_t deviceHandle,
                   const char *pIpAddr,
                   uDnsKeepGoingCallback_t cb)
{
    uSockAddress_t lookupAddr;
    uSockStringToAddress(pIpAddr, &lookupAddr);
    // Address in network endian format
    uint32_t netAddr = htonl(lookupAddr.ipAddress.address.ipv4);
    uint8_t netAddrLen = sizeof(netAddr);
    // Time to live for the response in seconds
    uint32_t ttl = htonl(U_DNS_TTL);

    int32_t sock = uSockCreate(deviceHandle,
                               U_SOCK_TYPE_DGRAM,
                               U_SOCK_PROTOCOL_UDP);
    if (sock < 0) {
        uPortLog("U_DNS: Failed to create DNS server socket: %d\n", sock);
        return sock;
    }
    uSockBlockingSet(sock, false);
    uSockAddress_t remoteAddr;
    uint8_t buff[255];
    remoteAddr.ipAddress.address.ipv4 = 0;
    remoteAddr.port = 53;
    uSockBind(sock, &remoteAddr);
    memset(buff, 0, sizeof(buff));
    uPortLog("U_DNS: server started\n");
    while (true) {
        int32_t errOrCnt = uSockReceiveFrom(sock,
                                            &remoteAddr,
                                            buff,
                                            sizeof(buff));
        if (errOrCnt > (int32_t)sizeof(uDnsHeader_t)) {
            // Incoming request
            uDnsHeader_t *pHeader = (uDnsHeader_t *)buff;
            bool valid =
                pHeader->QR == DNS_QR_QUERY &&
                pHeader->OPCode == DNS_OPCODE_QUERY &&
                ntohs(pHeader->QDCount) == 1 &&
                pHeader->ANCount == 0 &&
                pHeader->NSCount == 0 &&
                pHeader->ARCount == 0;
            if (valid) {
                // Create a response which maps any requested name
                // to the address specified by pIpAddr
                pHeader->QR = DNS_QR_RESPONSE;
                pHeader->ANCount = pHeader->QDCount;
                uint8_t *pData = buff + sizeof(uDnsHeader_t);
                // Need to parse the query name even if it is ignored
                // in order to get the correct position for the response ip.
                // Also print the name for debugging purposes.
                char name[100];
                memset(name, 0, sizeof(name));
                int32_t remain = errOrCnt - sizeof(uDnsHeader_t);
                while (remain > 0 && *pData != 0) {
                    uint8_t len = *pData + 1;
                    if (len > remain) {
                        valid = false;
                        break;
                    }
                    if (sizeof(name) - strlen(name) > len) {
                        strncat(name, (char *)(pData + 1), len - 1);
                        strcat(name, ".");
                    }
                    remain -= len;
                    pData += len;
                }
                if (strlen(name)) {
                    name[strlen(name) - 1] = 0;
                }
                if (valid) {
                    uPortLog("U_DNS lookup: %s\n", name);
                }

                // Make sure we have space for the address as well in the buffer
                valid = valid && (((pData - buff) + 21) < sizeof(buff));

                if (valid) {
                    // Skip remaining
                    pData += 5;
                    // Answer name is a pointer
                    *(pData++) = 0xC0;
                    // Pointer is to the name at offset
                    *(pData++) = 0x0C;
                    // Answer is type A query (host address)
                    *(pData++) = 0;
                    *(pData++) = 1;
                    // Answer is class IN (Internet address)
                    *(pData++) = 0;
                    *(pData++) = 1;
                    // TTL
                    memcpy(pData, (uint8_t *)&ttl, sizeof(ttl));
                    pData += sizeof(ttl);
                    // The fixed lookup address
                    *(pData++) = 0;
                    *(pData++) = netAddrLen;
                    memcpy(pData, (uint8_t *)&netAddr, netAddrLen);
                    pData += netAddrLen;
                    uSockSendTo(sock, &remoteAddr, buff, pData - buff);
                }
            }
            if (!valid) {
                // Send an error response
                int32_t dnsError =
                    pHeader->OPCode != DNS_OPCODE_QUERY ?
                    DNS_NOTIMPL_ERROR :
                    DNS_FORM_ERROR;
                pHeader->QR = DNS_QR_RESPONSE;
                pHeader->RCode = (unsigned char)dnsError;
                pHeader->QDCount = 0;
                pHeader->ANCount = 0;
                pHeader->NSCount = 0;
                pHeader->ARCount = 0;
                uPortLog("U_DNS: Unhandled request: %d\n", dnsError);
                uSockSendTo(sock, &remoteAddr, pHeader, sizeof(uDnsHeader_t));
            }
        }
        uPortTaskBlock(100);
        if (cb != NULL && !cb(deviceHandle)) {
            break;
        }
    }
    return uSockClose(sock);
}
