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

/** @file
 * @brief This header file defines the API for a DNS server
 * intended to be used for a captive portal, i.e. all lookup
 * requests will return the same ip address.
 */

/** DNS server callback.
 * @param deviceHandle the handle of the network device instance.
 * @return             when returning true the DNS server will stop and exit.
 */
typedef bool (*uDnsExitCallback_t)(uDeviceHandle_t devHandle);

/** Create a DNS server on the supplied device. All requests
 * will then be directed to the specified ipv4 address.
 * The server is intended to run in a separate process thread.
 *
 * @param deviceHandle the handle of the network device instance.
 * @param[in] pIpAddr  the address for all lookups
 * @param cb           exit callback Can be set to NULL
 * @return             possible negative errors occurring during
 *                     the creation of the server.
 */
int32_t uDnsServer(uDeviceHandle_t deviceHandle,
                   const char *pIpAddr,
                   uDnsExitCallback_t cb);

#endif