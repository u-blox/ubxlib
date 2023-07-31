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

#ifndef _U_DNS_SERVER_H_
#define _U_DNS_SERVER_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __dns __DNS
 *  @{
 */

/** @file
 * @brief This header file defines the API for a DNS server
 * intended to be used for a captive portal, i.e. all lookup
 * requests will return the same ip address.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** DNS server callback.
 *
 * @param deviceHandle the handle of the network device instance.
 * @return             true if the DNS server should keep going, false
 *                     to cause the DNS server to exit.
 */
typedef bool (*uDnsKeepGoingCallback_t)(uDeviceHandle_t deviceHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create a DNS server on the supplied device. All requests
 * will then be directed to the specified ipv4 address.
 * The server is intended to run in a separate process thread.
 *
 * @param deviceHandle the handle of the network device instance.
 * @param[in] pIpAddr  the address for all lookups
 * @param cb           callback that may be used to control when
 *                     the DNS server exits; NULL to continue forever.
 * @return             possible negative errors occurring during
 *                     the creation of the server.
 */
int32_t uDnsServer(uDeviceHandle_t deviceHandle,
                   const char *pIpAddr,
                   uDnsKeepGoingCallback_t cb);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_DNS_SERVER_H_

// End of file