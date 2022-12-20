/*
 * Copyright 2019-2022 u-blox
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

#ifndef _U_WIFI_TEST_CFG_H_
#define _U_WIFI_TEST_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines configuration values for wifi
 & API testing.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */


#ifndef U_WIFI_TEST_CFG_SSID
/** The SSID to connect to for testing WiFi
 * Must be WITHOUT quotes.
 */
# define U_WIFI_TEST_CFG_SSID ubx
#endif

#ifndef U_WIFI_TEST_CFG_AUTHENTICATION
/** The authentication mode to use for testing WiFi
 */
# define U_WIFI_TEST_CFG_AUTHENTICATION 2 // WPA/WPA2/WPA3
#endif

#ifndef U_WIFI_TEST_CFG_WPA2_PASSPHRASE
/** The WPA2 passphrase to use during WiFi testing
 */
# define U_WIFI_TEST_CFG_WPA2_PASSPHRASE ReplaceThis
#endif

#endif // _U_WIFI_TEST_CFG_H_

// End of file
