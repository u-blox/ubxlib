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

#ifndef _U_WIFI_CAPTIVE_PORTAL_H_
#define _U_WIFI_CAPTIVE_PORTAL_H_

/** @file
 * @brief This header file defines the API for a WiFi captive
 * portal intended to be used for a providing WiFi credentials
 * via a standard hot-spot login functionality.
 */

/** Captive portal exit callback.
 * @param deviceHandle the handle of the network device instance.
 * @return             when returning true the captive portal server
 *                     will stop and exit.
 */
typedef bool (*uWifiCaptivePortalExitCallback_t)(uDeviceHandle_t devHandle);

/** Create the captive port and wait a user to select an available
 * SSID network and enter the corresponding password. Once that has
 * done the credentials are stored in the WiFi module and it will
 * restarted to connect to the selected network. The process involves
 * starting a DNS server and a web server to handle the captive portal.
 *
 * @param deviceHandle  the handle of the network device instance.
 * @param[in] pSsid     the name of the captive portal (its SSID)
 *                      Can be set to NULL in which case no access
 *                      point will be started, just the web and dns
 *                      server. This is mainly intended for testing.
 * @param[in] pPassword optional password for the portal. Can be set
 *                      to NULL for an open access point.
 * @param cb            optional exit callback. Can be set to NULL.
 * @return              possible negative errors occurring during
 *                      the setup of the portal or when connecting
 *                      using the entered credentials.
 *                      Set to 0 on success.
 */
int32_t uWifiCaptivePortal(uDeviceHandle_t deviceHandle,
                           const char *pSsid,
                           const char *pPassword,
                           uWifiCaptivePortalExitCallback_t cb);

#endif