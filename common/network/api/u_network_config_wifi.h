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

#ifndef _U_NETWORK_CONFIG_WIFI_H_
#define _U_NETWORK_CONFIG_WIFI_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup network
 *  @{
 */

/** @file
 * @brief This header file defines the configuration structure for the
 * network API for Wifi.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Note: try, wherever possible, to use only basic types in this
 * structure (i.e. int32_t, const char, bool, etc.) since otherwise
 * your Wifi headers will have to be brought in to all files that
 * need this config type, so into all network examples, etc.
 * irrespective of whether Wifi is used there.
 */

typedef enum {
    U_WIFI_MODE_STA = 0, /**< Wifi station */
    U_WIFI_MODE_AP,      /**< Wifi access point */
    U_WIFI_MODE_STA_AP,  /**< Station and access point */
    U_WIFI_MODE_NONE     /**< Inactive */
} uNetworkWifiMode_t;

/** The network configuration for Wifi station and access point.
 */
typedef struct {
    uNetworkCfgVersion_t version; /**< version of this network
                                     configuration; allow your
                                     compiler to initialise this
                                     to zero unless otherwise
                                     specified below. */
    uNetworkType_t type;          /**< for error checking purposes. */
    const char *pSsid;            /**< the access point SSID for a station to connect to
                                     Can be set to NULL to enforce use of previously
                                     saved credentials. */
    int32_t authentication;       /**< authentication mode for the station. Values are:
                                     1: Open (No authentication)
                                     2: WPA/WPA2/WPA3-PSK
                                     6: WPA2/WPA3-PSK
                                     7: WPA3-PSK */
    const char *pPassPhrase;      /**< WPA/WPA2/WPA3 passphrase - should be NULL for open */
    const char *pHostName;        /**< the network host name of the Wifi device.
                                     When NULL a default name combined of the module
                                     type and mac address will be used. */
    uNetworkWifiMode_t mode;      /**< mode in which the Wifi module should be started. */
    const char *pApSssid;         /**< SSID for the access point when applicable. */
    int32_t apAuthentication;     /**< access point authentication mode. */
    const char *pApPassPhrase;    /**< access point WPA/WPA2/WPA3 passphrase.
                                       Should be NULL for open. */
    const char *pApIpAddress;     /**< ip address of the access point. */
    /* This is the end of version 0 of this
       structure: should any fields be added to
       this structure in future they must be
       added AFTER this point and instructions
       must be given against each one as to how
       to set the version field if any of the
       new fields are populated. For example, if
       int32_t magic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field
       of this structure must be set to 1 or higher". */
} uNetworkCfgWifi_t;

/** @}*/

#endif // _U_NETWORK_CONFIG_WIFI_H_

// End of file
