/*
 * Copyright 2024 u-blox
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

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the API for a WiFi captive
 * portal intended to be used for a providing WiFi credentials
 * via a standard hot-spot login functionality.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_WIFI_CAPTIVE_PORTAL_DNS_TASK_STACK_SIZE_BYTES
/** The stack size of the task that runs a local DNS (created and
 * destroyed by uWifiCaptivePortal()).
 */
# define U_WIFI_CAPTIVE_PORTAL_DNS_TASK_STACK_SIZE_BYTES 2304
#endif

#ifndef U_WIFI_CAPTIVE_PORTAL_DNS_TASK_PRIORITY
/** The priority of the task that runs a local DNS (created and
 * destroyed by uWifiCaptivePortal()).
 */
# define U_WIFI_CAPTIVE_PORTAL_DNS_TASK_PRIORITY (U_CFG_OS_APP_TASK_PRIORITY + 1)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Captive portal callback.
 *
 * @param deviceHandle the handle of the network device instance.
 * @return             true if the captive portal should keep going, false
 *                     to cause the captive portal to exit.
 */
typedef bool (*uWifiCaptivePortalKeepGoingCallback_t)(uDeviceHandle_t deviceHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create the captive port and wait a user to select an available
 * SSID network and enter the corresponding password. Once that has been
 * done the credentials are stored in the WiFi module and it will be
 * restarted to connect to the selected network. The process involves
 * starting a DNS server and a web server to handle the captive portal.
 *
 * This function is NOT threadsafe: there can be only one.
 *
 * Note: this function, internally, calls uNetworkInterfaceUp() and so,
 * if it returns successfully, it is up to you to call
 * uNetworkInterfaceDown() on deviceHandle when done.
 *
 * @param deviceHandle  the handle of the network device instance.
 * @param[in] pSsid     the name of the captive portal (its SSID)
 *                      Can be set to NULL in which case no access
 *                      point will be started, just the web and dns
 *                      server. This is mainly intended for testing.
 * @param[in] pPassword optional password for the portal. Can be set
 *                      to NULL for an open access point.
 * @param cb            callback that may be used to control when
 *                      the captive portal exits; NULL to continue
 *                      until a user selection has occurred (there
 *                      is no timeout).
 * @return              possible negative errors occurring during
 *                      the setup of the portal or when connecting
 *                      using the entered credentials.
 *                      Set to 0 on success.
 */
int32_t uWifiCaptivePortal(uDeviceHandle_t deviceHandle,
                           const char *pSsid,
                           const char *pPassword,
                           uWifiCaptivePortalKeepGoingCallback_t cb);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_CAPTIVE_PORTAL_H_

// End of file