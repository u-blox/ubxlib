/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_CELL_LOC_H_
#define _U_CELL_LOC_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the API into the Cell Locate
 * service.  These functions are thread-safe with the proviso that
 * a cellular instance should not be accessed before it has been added
 * or after it has been removed.
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

/** The possible Wifi authentication mechanisms.
 */
typedef enum {
    U_CELL_LOC_WIFI_WIFI_AUTH_NONE = 0,
    U_CELL_LOC_WIFI_WIFI_AUTH_WEP = 0x01,
    U_CELL_LOC_WIFI_WIFI_AUTH_PSK = 0x02,
    U_CELL_LOC_WIFI_WIFI_AUTH_EAP = 0x04,
    U_CELL_LOC_WIFI_WIFI_AUTH_WPA = 0x08,
    U_CELL_LOC_WIFI_WIFI_AUTH_WPA2 = 0x10
} uCellLocWifiAuth_t;

/** The possible Wifi cipher mechanisms.
 */
typedef enum {
    U_CELL_LOC_WIFI_CIPHER_NONE = 0,
    U_CELL_LOC_WIFI_CIPHER_WEP64 = 0x01,
    U_CELL_LOC_WIFI_CIPHER_WEP128 = 0x02,
    U_CELL_LOC_WIFI_CIPHER_TKIP = 0x04,
    U_CELL_LOC_WIFI_CIPHER_AES_CCMP = 0x08
} uCellLocWifiCipher_t;

/** Structure to hold the information on a single Wifi
 * access point.
 */
typedef struct {
    char ssid[33];      //!< the SSID of the access point as a NULL
    //! terminated string.
    uint8_t bssid[6];   //!< MAC address of the access point (binary
    //! values, not characters/text).
    int32_t rssiDbm;    //!< the RSSI of the access point in dBm.
    int32_t channel;    //!< the Wifi channel used by the network.
    bool isAdhoc;       //!< true if this is an adhoc AP, else false.
    uint8_t authBitmap; //!< bitmap of the supported authentication
    //! types, see uCellLocWifiAuth_t.
    uint8_t unicastCipherBitmap; //!< bitmap of the supported unicast
    //! ciphers, see uCellLocWifiCipher_t.
    uint8_t groupCipherBitmap;   //!< bitmap of the supported group
    //! ciphers see uCellLocWifiCipher_t.
} uCellLocWifiAp_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURATION
 * -------------------------------------------------------------- */

/** Get the desired location accuracy.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the desired accuracy in millimetres.
 */
int32_t uCellLocDesiredAccuracyGet(int32_t cellHandle);

/** Set the desired location accuracy.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param accuracyMillimetres the desired accuracy in millimetres.
 */
void uCellLocDesiredAccuracySet(int32_t cellHandle,
                                int32_t accuracyMillimetres);

/** Get the desired location fix time-out.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the desrired timeout in seconds.
 */
int32_t uCellLocDesiredFixTimeoutGet(int32_t cellHandle);

/** Set the desired location fix time-out.
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param fixTimeoutSeconds the desired fix timeout in seconds.
 */
void uCellLocDesiredFixTimeoutSet(int32_t cellHandle,
                                  int32_t fixTimeoutSeconds);

/** Get whether GNSS is employed in the location fix or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if GNSS is used else false.
 */
bool uCellLocGnssEnableGet(int32_t cellHandle);

/** Set whether a GNSS chip attached to the cellular module
 * should be used in the location fix or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param onNotOff    true if GNSS should be used, else false.
 */
void uCellLocGnssEnableSet(int32_t cellHandle, bool onNotOff);

/** Set the cellular module pin which is connected to the
 * GNSSEN pin of the GNSS chip.  This is the pin number of the
 * cellular module, so for instance GPIO2 is cellular module
 * pin 23 and hence 23 would be used here.  If no power control
 * functionality is required then specify -1 (which is the default).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pin         the pin to use.
 * @return            zero on success else negative error code.
 */
int32_t uCellLocPinGnssPwrSet(int32_t cellHandle, int32_t pin);

/** Set the cellular module pin which is connected to the Data
 * Ready pin of the GNSS chip.  This is the pin number of the
 * cellular module, so for instance GPIO3 is cellular module
 * pin 24 and hence 24 would be used here.  If no Data Ready
 * signalling is required then specify -1 (which is the default).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pin         the pin to use.
 * @return            zero on success else negative error code.
 */
int32_t uCellLocPinGnssDataReadySet(int32_t cellHandle, int32_t pin);

/** Configure the Cell Locate server parameters.
 *
 * @param cellHandle              the handle of the cellular instance.
 * @param pAuthenticationTokenStr a pointer to the null-terminated
 *                                authentication token for the Cell
 *                                Locate server. May be NULL, in which
 *                                case pPrimaryServerStr and
 *                                pSecondaryServerStr are ignored.
 * @param pPrimaryServerStr       a pointer to the null-terminated
 *                                primary server string, e.g.
 *                                "celllive1.services.u-blox.com".
 *                                May be NULL, in which case the default
 *                                is used.
 * @param pSecondaryServerStr     a pointer to the null-terminated
 *                                secondary server string, e.g.
 *                                "celllive2.services.u-blox.com".
 *                                May be NULL, in which case the default
 *                                is used.
 * @return                        zero on success, negative error code on
 *                                failure.
 */
int32_t uCellLocServerCfg(int32_t cellHandle,
                          const char *pAuthenticationTokenStr,
                          const char *pPrimaryServerStr,
                          const char *pSecondaryServerStr);

/** Check whether a GNSS chip is present or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if a GNSS chip is present, else false.
 */
bool uCellLocIsGnssPresent(int32_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: LOCATION ESTABLISHMENT
 * -------------------------------------------------------------- */

/** Add information on a Wifi access point which may be used to
 * improve the location fix when uCellLocGet() is called.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pInfo       a pointer to the Wifi access point information,
 *                    which will be copied into this component and
 *                    hence may be destroyed when this function
 *                    returns; cannot be NULL.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellLocWifiAddAp(int32_t cellHandle,
                          const uCellLocWifiAp_t *pInfo);

/** Delete any Wifi access point information which was previously
 * in use.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellLocWifiClearAllAps(int32_t cellHandle);

/** Get the current location, returning on success or until
 * pKeepGoingCallback returns false.
 *
 * @param cellHandle                  the handle of the cellular instance.
 * @param pLatitudeX1e6               a place to put latitude (in
 *                                    millionths of a degree); may be NULL.
 * @param pLongitudeX1e6              a place to put longitude (in millionths
 *                                    of a degree); may be NULL.
 * @param pAltitudeMillimetres        a place to put the altitude (in
 *                                    millimetres); may be NULL.
 * @param pRadiusMillimetres          a place to put the radius of position
 *                                    (in millimetres); may be NULL.  If the
 *                                    radius is unknown -1 will be returned.
 * @param pSpeedMillimetresPerSecond  a place to put the speed (in
 *                                    millimetres per second); may be
 *                                    NULL.  If the speed is unknown
 *                                    -1 will be returned.
 * @param pSvs                        a place to store the number of
 *                                    space vehicles used in the
 *                                    solution; may be NULL. If the
 *                                    number of space vehicles is
 *                                    unknown or irrelevant -1 will
 *                                    be returned.
 * @param pTimeUtc                    a place to put the UTC time; may be NULL.
 *                                    If the time is unknown -1 will be
 *                                    returned. Note that this is the time of
 *                                    the fix and, by the time the fix is
 *                                    returned, it may not represent the
 *                                    *current* time.
 * @param pKeepGoingCallback          a callback function that governs how
 *                                    long a location establishment may continue
 *                                    for. This function is called once a second
 *                                    while waiting for a location fix; the
 *                                    location establishment attempt will only
 *                                    continue while it returns true.  This allows
 *                                    the caller to terminate the establishment
 *                                    attempt at their convenience. This function
 *                                    may also be used to feed any watchdog timer
 *                                    that may be running.  The single int32_t
 *                                    parameter is the cell handle. May be NULL,
 *                                    in which case the location establishment
 *                                    attempt will eventually time out on failure.
 * @return                            zero on success or negative error code on
 *                                    failure.
 */
int32_t uCellLocGet(int32_t cellHandle,
                    int32_t *pLatitudeX1e6, int32_t *pLongitudeX1e6,
                    int32_t *pAltitudeMillimetres, int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond, int32_t pSvs,
                    int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (int32_t));

/** Get the current location, non-blocking version.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pCallback   a callback that will be called when a fix has been
 *                    obtained.  The first parameter to the callback is
 *                    the cellular handle, the remaining parameters are
 *                    as described in uCellLocGet() except that they are
 *                    not pointers.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellLocGetStart(int32_t cellHandle,
                         void (*pCallback) (int32_t cellHandle,
                                            int32_t latitudeX1e6,
                                            int32_t longitudeX1e6,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int64_t timeUtc));

/** Get the last status of a location fix attempt.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            on success the location status, taken
 *                    from uLocationStatus_t (see common
 *                    location API), else negative error code.
 */
int32_t uCellLocStatusGet(int32_t cellHandle);

/** Cancel a uCellLocGetStart(); after calling this function the
 * callback passed to uCellLocGetStart() will not be called until
 * another uCellLocGetStart() is begun.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellLocGetStop(int32_t cellHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_LOC_H_

// End of file
