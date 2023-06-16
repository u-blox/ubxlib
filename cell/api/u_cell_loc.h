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

#ifndef _U_CELL_LOC_H_
#define _U_CELL_LOC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the API into the Cell Locate
 * and the Assist Now services.  The Cell Locate service is
 * used to establish location anywhere (either using cell
 * towers or using a GNSS chip that is inside or connected-via the
 * cellular module), while the AssistNow service is used to
 * reduce the time to first fix for a GNSS chip that is inside
 * or is connected-via the cellular module.
 *
 * These functions are thread-safe with the following exceptions:
 *
 * - uCellLocCleanUp() should not be called while location
 *   establishment is running.
 * - a cellular instance should not be deinitialised while location
 *   establishment is running.
 *
 * To use the Cell Locate or Assist Now services you will need to
 * obtain an authentication token from the Location Services
 * section of your Thingstream portal
 * (https://portal.thingstream.io/app/location-services) and
 * call uCellLocSetServer() to supply that authentication
 * token to the cellular module.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_LOC_TIMEOUT_SECONDS
/** The timeout for location establishment in seconds.
 */
# define U_CELL_LOC_TIMEOUT_SECONDS 240
#endif

#ifndef U_CELL_LOC_DESIRED_ACCURACY_DEFAULT_MILLIMETRES
/** The default desired location accuracy in metres.
 */
# define U_CELL_LOC_DESIRED_ACCURACY_DEFAULT_MILLIMETRES (10 * 1000)
#endif

#ifndef U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS
/** The default desired location fix time-out in seconds.
 */
# define U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS 60
#endif

#ifndef U_CELL_LOC_GNSS_ENABLE_DEFAULT
/** The default as to whether GNSS is enabled or not.
 */
# define U_CELL_LOC_GNSS_ENABLE_DEFAULT true
#endif

#ifndef U_CELL_LOC_BUFFER_LENGTH_BYTES
/** The length of buffer to use for a Wifi tag string.
 * The maximum AT command-line length is usually 1024
 * characters so the biggest buffer that can be sent is
 * "AT+ULOCEXT=\r\n" characters less than that.
 */
# define U_CELL_LOC_BUFFER_LENGTH_BYTES 1011
#endif

#ifndef U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=1.  If you
 * change this and you also use the GNSS API then you might
 * want to change the value of #U_GNSS_AT_POWER_UP_TIME_SECONDS
 * also.
 */
# define U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS 30
#endif

#ifndef U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=0.  If you
 * change this and you also use the GNSS API then you might
 * want to change the value of #U_GNSS_AT_POWER_DOWN_TIME_SECONDS
 * also.
 */
# define U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS 30
#endif

#ifndef U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS
/** Some intermediate modules (for example SARA-R4) can be touchy
 * about a power-up or power-down request occurring close
 * on the heels of a previous GNSS-related command  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * #U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS also.
 */
# define U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS 500
#endif

#ifndef U_CELL_LOC_MODULE_HAS_CELL_LOCATE
/** Seems a strange define this but some modules, specifically
 * the SARA-R4xx-x2B-00, SARA-R4xx-x2B-01 and SARA-R4xx-x2B-02
 * modules, don't support the sensor type "cell locate" (sensor
 * type 2) on the AT+ULOC AT command, they only respond to AT+ULOC
 * if a GNSS chip is attached to the cellular module.  Should
 * you wish to use the Cell Locate API with this module type then
 * you should define #U_CELL_LOC_MODULE_HAS_CELL_LOCATE to be 0
 * (and of course make sure you have a GNSS chip attached to the
 * cellular module and don't disable GNSS in this API).
 */
# define U_CELL_LOC_MODULE_HAS_CELL_LOCATE 1
#endif

#ifndef U_CELL_LOC_GNSS_AIDING_TYPES
/** The aiding types to request when switching-on a GNSS
 * chip attached to a cellular module (all of them).
 */
# define U_CELL_LOC_GNSS_AIDING_TYPES 15
#endif

#ifndef U_CELL_LOC_GNSS_SYSTEM_TYPES
/** The system types to request when switching-on a GNSS
 * chip attached to a cellular module (all of them).
 */
# define U_CELL_LOC_GNSS_SYSTEM_TYPES 0x7f
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS:  WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

/** Workaround for Espressif linker missing out files that
 * only contain functions which also have weak alternatives
 * (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
 *
 * You can ignore this function.
 */
void uCellLocPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Using this API will allocate memory for a context, which will
 * be cleaned up when uCellDeinit() is called.  If you want to
 * free that memory before uCellDeinit() is called then call this
 * function.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellLocCleanUp(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURATION
 * -------------------------------------------------------------- */

/** Set the cellular module pin which enables power to the
 * GNSS chip.  This is the pin number of the cellular module so,
 * for instance, GPIO2 is cellular module pin 23 and hence 23 would
 * be used here.  If this function is not called then no
 * power-enable functionality is assumed.
 * Note that this function is distinct and separate from the
 * uGnssSetAtPinPwr() over in the GNSS API: if you are
 * using that API then you should call that function.
 * The cellular module must be powered-on for this to work.
 * If the cellular module is powered off this setting will be
 * forgotten.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pin         the pin to use.
 * @return            zero on success or negative error code.
 */
int32_t uCellLocSetPinGnssPwr(uDeviceHandle_t cellHandle, int32_t pin);

/** Set the cellular module pin which is connected to the Data
 * Ready pin of the GNSS chip.  This is the pin number of the
 * cellular module so, for instance, GPIO3 is cellular module
 * pin 24 and hence 24 would be used here.  If this function
 * is not called then no Data Ready functionality is assumed.
 * Note that this function is distinct and separate from the
 * uGnssSetAtPinDataReady() over in the GNSS API: if you are
 * using that API then you should call that function.
 * The cellular module must be powered-on for this to work.
 * If the cellular module is powered off this setting will be
 * forgotten.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pin         the pin to use.
 * @return            zero on success or negative error code.
 */
int32_t uCellLocSetPinGnssDataReady(uDeviceHandle_t cellHandle, int32_t pin);

/** Configure the Cell Locate/Assist Now server parameters, in particular
 * the authentication token that is required to use the Cell Locate
 * or Assist Now services.  This may be obtained from the Location Services
 * section of your Thingstream portal
 * (https://portal.thingstream.io/app/location-services).
 * The cellular module must be powered-on for this to work.
 * If the cellular module is powered off this setting will be
 * forgotten.
 *
 * @param cellHandle                  the handle of the cellular instance.
 * @param[in] pAuthenticationTokenStr a pointer to the null-terminated
 *                                    authentication token for the Cell
 *                                    Locate/Assist Now server. May be NULL,
 *                                    in which case pPrimaryServerStr and
 *                                    pSecondaryServerStr are ignored.
 * @param[in] pPrimaryServerStr       a pointer to the null-terminated
 *                                    primary server string, for example
 *                                    "celllive1.services.u-blox.com".
 *                                    May be NULL, in which case the default
 *                                    is used.
 * @param[in] pSecondaryServerStr     a pointer to the null-terminated
 *                                    secondary server string, for example
 *                                    "celllive2.services.u-blox.com".
 *                                    May be NULL, in which case the default
 *                                    is used.
 * @return                            zero on success or negative error code.
 */
int32_t uCellLocSetServer(uDeviceHandle_t cellHandle,
                          const char *pAuthenticationTokenStr,
                          const char *pPrimaryServerStr,
                          const char *pSecondaryServerStr);

/** Set the GNSS systems that a GNSS chip inside or connected-via a
 * cellular module will employ.  Not all GNSS chips support all system types.
 * If this is not called #U_CELL_LOC_GNSS_SYSTEM_TYPES will be used.
 *
 * @param cellHandle            the handle of the cellular instance.
 * @param gnssSystemTypesBitMap a bit-map of the GNSS systems that should be used,
 *                              chosen from #uGnssSystem_t (see u_gnss_type.h),
 *                              where each system is represented by its bit-position
 *                              (for example set bit 0 to one for GPS).  Not all
 *                              systems are supported by all modules.
 * @return                      zero on success or negative error code.
 */
int32_t uCellLocSetSystem(uDeviceHandle_t cellHandle, uint32_t gnssSystemTypesBitMap);

/** Get the GNSS systems that a GNSS chip inside or connected-via a
 * cellular module will employ.
 *
 * @param cellHandle                  the handle of the cellular instance.
 * @param[out] pGnssSystemTypesBitMap a pointer to a place to put the bit-map of the
 *                                    GNSS systems, see #uGnssSystem_t (u_gnss_type.h),
 *                                    where each system is represented by its
 *                                    bit-position (for example bit 0 represents GPS).
 * @return                            zero on success or negative error code.
 */
int32_t uCellLocGetSystem(uDeviceHandle_t cellHandle, uint32_t *pGnssSystemTypesBitMap);

/** Check whether a GNSS chip is present or not.  Note that this may
 * fail if the cellular module controls power to the GNSS chip and
 * the correct cellular module GPIO pin for that has not been set
 * (by calling uCellLocSetPinGnssPwr()).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if a GNSS chip is present, else false.
 */
bool uCellLocIsGnssPresent(uDeviceHandle_t cellHandle);

/** Check whether there is a GNSS chip on-board the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if there is a GNSS chip inside the cellular
 *                    module, else false.
 */
bool uCellLocGnssInsideCell(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURATION OF CELL LOCATE
 * -------------------------------------------------------------- */

/** Set the desired location accuracy.  If this is not called
 * then the default #U_CELL_LOC_DESIRED_ACCURACY_DEFAULT_MILLIMETRES
 * is used.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param accuracyMillimetres the desired accuracy in millimetres.
 */
void uCellLocSetDesiredAccuracy(uDeviceHandle_t cellHandle,
                                int32_t accuracyMillimetres);

/** Get the desired location accuracy.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the desired accuracy in millimetres.
 */
int32_t uCellLocGetDesiredAccuracy(uDeviceHandle_t cellHandle);

/** Set the desired location fix time-out.  If this is not called
 * then the default #U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS
 * is used.
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param fixTimeoutSeconds the desired fix timeout in seconds.
 */
void uCellLocSetDesiredFixTimeout(uDeviceHandle_t cellHandle,
                                  int32_t fixTimeoutSeconds);

/** Get the desired location fix time-out.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the desired timeout in seconds.
 */
int32_t uCellLocGetDesiredFixTimeout(uDeviceHandle_t cellHandle);

/** Set whether a GNSS chip attached to the cellular module
 * should be used in the location fix or not.  If this is not
 * called then the default #U_CELL_LOC_GNSS_ENABLE_DEFAULT
 * is used.  Call this with false if you have a GNSS chip
 * attached via the cellular module but you intend to use
 * the GNSS API to manage it directly rather than letting
 * Cell Locate use it via this API.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param onNotOff    true if GNSS should be used, else false.
 */
void uCellLocSetGnssEnable(uDeviceHandle_t cellHandle, bool onNotOff);

/** Get whether GNSS is employed in the location fix or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if GNSS is used else false.
 */
bool uCellLocGetGnssEnable(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: CONFIGURATION OF ASSIST NOW
 * -------------------------------------------------------------- */

/** Set the data types used by AssistNow Online to reduce the time to
 * first fix when a GNSS chip that is inside or is connected-via a
 * cellular module is first powered-up.  For AssistNow Online to work
 * a valid authentication token must have been supplied with
 * uCellLocSetServer() and the cellular module must have been
 * connected to the network (e.g. by calling uCellNetConnect()) before
 * the GNSS chip is powered-up.
 *
 * If dataTypeBitMap is zero then AssistNow Online will not be used,
 * though note that, if the GNSS chip is on when this function is called,
 * it will be power-cycled for the switch-off to take effect.
 *
 * If this is not called AssistNow Online will be used (provided
 * a valid token has been provided via uCellLocSetServer()) and all
 * data types will be requested.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param dataTypeBitMap  a bit-map of the data types that are to be
 *                        requested, chosen from #uGnssMgaDataType_t
 *                        (see u_gnss_mga.h), where each data type is
 *                        represented by its bit position; for example
 *                        set bit 0 to one for ephemeris data.  NOTE:
 *                        this is NOT the same as the bit-map given
 *                        for the AT+UGSRV command in the AT manual:
 *                        instead here it is made common with the GNSS
 *                        one, look at #uGnssMgaDataType_t.
 * @return                zero on success else negative error code.
 */
int32_t uCellLocSetAssistNowOnline(uDeviceHandle_t cellHandle,
                                   uint32_t dataTypeBitMap);

/** Get which data types from the AssistNow Online service are being
 * used to speed-up the time to first fix of a GNSS chip that is
 * inside or connected-via a cellular module.
 *
 * If *pDataTypeBitMap is zero then AssistNow Online is not being used.
 *
 * @param cellHandle            the handle of the cellular instance.
 * @param[out] pDataTypeBitMap  a pointer to a place to store the bit-map
 *                              of data types from the AssistNow Online
 *                              service that are being requested, see
 *                              #uGnssMgaDataType_t (u_gnss_mga.h), where
 *                              each data type is represented by its bit
 *                              position; for example bit 0 represents
 *                              ephemeris data.  NOTE: this is NOT the same
 *                              as the bit-map given for the AT+UGSRV command
 *                              in the AT manual: instead here it is made
 *                              common with the GNSS one, look at
 *                              #uGnssMgaDataType_t.
 * @return                      zero on success else negative error code.
 */
int32_t uCellLocGetAssistNowOnline(uDeviceHandle_t cellHandle,
                                   uint32_t *pDataTypeBitMap);

/** Configure AssistNow Offline, used by the cellular module to
 * reduce the time to first fix when a GNSS chip that is inside or
 * is connected-via a cellular module is first powered-up.  AssistNow
 * Offline is useful if the cellular module is not going to be
 * connected to the network on a regular basis at the time when the
 * GNSS chip is being first powered-up.  For AssistNow Offline to
 * work a valid authentication token must have been supplied using
 * uCellLocSetServer().
 *
 * If either of the parameters gnssSystemTypesBitMap or periodDays is zero
 * then AssistNow Offline will not be used.
 *
 * If the GNSS chip is on when this function is called, it will be
 * power-cycled for the change to take effect.
 *
 * @param cellHandle            the handle of the cellular instance.
 * @param gnssSystemTypesBitMap a bit-map of the GNSS systems that should
 *                              be requested, chosen from #uGnssSystem_t
 *                              (see u_gnss_type.h), where each system is
 *                              represented by its bit-position (for
 *                              example set bit 0 to one for GPS).  Not
 *                              all systems are supported (see the latest
 *                              u-blox AssistNow service description for
 *                              which are supported).  Use zero to switch
 *                              off AssistNow Offline, ignored if periodDays
 *                              is zero.
 * @param periodDays            the number of days for which data is required;
 *                              note that the size of the response returned
 *                              by the server may increase by between 5 and
 *                              10 kbytes per day requested. Use zero to
 *                              switch off AssistNow Offline; ignored if
 *                              gnssSystemTypesBitMap is zero.  Note that,
 *                              depending on the GNSS device and the
 *                              cellular module in use, the period may be
 *                              rounded up into a whole number of weeks.
 * @param daysBetweenItems      the number of days between items: 1 for
 *                              every day, 2 for one every two days or 3 for
 *                              one every 3 days; ignored if either of
 *                              gnssSystemTypesBitMap or periodDays is zero.
 * @return                      zero on success else negative error code.
 */
int32_t uCellLocSetAssistNowOffline(uDeviceHandle_t cellHandle,
                                    uint32_t gnssSystemTypesBitMap,
                                    int32_t periodDays,
                                    int32_t daysBetweenItems);

/** Get the configuration of AssistNow Offline used by the cellular
 * module to reduce the time to first fix when a GNSS chip that is
 * inside or connected-via a cellular module is first powered-up.
 *
 * AssistNow Offline is not being used if *pSystemBitMap or *pPeriodDays
 * is zero.
 *
 * @param cellHandle                   the handle of the cellular instance.
 * @param[out] pGnssSystemTypesBitMap  a pointer to a place to put the bit-map
 *                                     of GNSS systems that are being used,
 *                                     see #uGnssSystem_t (in u_gnss_type.h),
 *                                     where each system is represented by
 *                                     its bit-position (for example bit 0
 *                                     represents GPS); if all bits are zero
 *                                     then AssistNow Offline is not being
 *                                     used.  May be NULL.
 * @param[out] pPeriodDays             a pointer to a place to put the number
 *                                     of days for which AssistNow Offline data
 *                                     is requested; zero means AssistNow Offline
 *                                     is not being used.  May be NULL.
 * @param[out] pDaysBetweenItems       a pointer to a place to put the number of
 *                                     days between items; may be NULL.
 * @return                             zero on success else negative error code.
 */
int32_t uCellLocGetAssistNowOffline(uDeviceHandle_t cellHandle,
                                    uint32_t *pGnssSystemTypesBitMap,
                                    int32_t *pPeriodDays,
                                    int32_t *pDaysBetweenItems);

/** Set whether AssistNow Autonomous, for a GNSS chip inside or
 * connected-via a cellular module, is on or off; if this
 * is not called AssistNow Autonomous will be on.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param onNotOff    true if AssistNow Autonomous should be on,
 *                    false if it is to be off.
 * @return            zero on success else negative error code.
 */
int32_t uCellLocSetAssistNowAutonomous(uDeviceHandle_t cellHandle,
                                       bool onNotOff);

/** Get whether AssistNow Autonomous, where a GNSS chip that is
 * inside or connected-via a cellular module can figure out future
 * satellite movements and use this to reduce the time to first fix,
 * is on or off.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if AssistNow Autonomous operation is
 *                    on, else false.
 */
bool uCellLocAssistNowAutonomousIsOn(uDeviceHandle_t cellHandle);

/** Set whether the GNSS assistance database of a GNSS chip that is
 * inside or connected-via a cellular module is automatically saved by
 * the cellular module before power-off and restored again after power-on,
 * to reduce the time to first fix.  This is equivalent to calling
 * uGnssMgaGetDatabase() and uGnssMgaSetDatabase() for a GNSS chip
 * directly connected to this MCU but is performed automatically, as
 * required, by the cellular module.  If this is not called AssistNow
 * database saving will be on.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param onNotOff    true to enable automatic saving of the assistance
 *                    database, false to disable it.
 * @return            true if GNSS assistance database saving is on,
 *                    else false.
 */
int32_t uCellLocSetAssistNowDatabaseSave(uDeviceHandle_t cellHandle,
                                         bool onNotOff);

/** Check whether the GNSS assistance database of a GNSS chip that is
 * inside or connected-via a cellular module is automatically saved by
 * the cellular module before power-off and restored again after power-on,
 * to reduce the time to first fix.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if GNSS assistance database saving is on,
 *                    else false.
 */
bool uCellLocAssistNowDatabaseSaveIsOn(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: LOCATION ESTABLISHMENT
 * -------------------------------------------------------------- */

/** Get the current location, returning on success or if
 * pKeepGoingCallback returns false.  This will ONLY work if
 * the cellular module is currently registered on a network
 * (e.g. as a result of uCellNetConnect() or uCellNetRegister()
 * being called).
 *
 * IMPORTANT: if Cell Locate is unable to establish a location
 * it may still return a valid time and a location of all zeros
 * but with a very large radius (e.g. 200 km), hence it is always
 * wise to check the radius.
 *
 * @param cellHandle                       the handle of the cellular instance.
 * @param[out] pLatitudeX1e7               a place to put latitude (in ten
 *                                         millionths of a degree); may be NULL.
 * @param[out] pLongitudeX1e7              a place to put longitude (in ten millionths
 *                                         of a degree); may be NULL.
 * @param[out] pAltitudeMillimetres        a place to put the altitude (in
 *                                         millimetres); may be NULL.
 * @param[out] pRadiusMillimetres          a place to put the radius of position
 *                                         (in millimetres); may be NULL.  Radius may
 *                                         be absent even when a location is
 *                                         established; should this be the case this
 *                                         variable will point to INT_MIN.
 * @param[out] pSpeedMillimetresPerSecond  a place to put the speed (in
 *                                         millimetres per second); may be
 *                                         NULL.  This field is only populated if
 *                                         there is a GNSS chip attached to the cellular
 *                                         module which is used in the Cell Locate
 *                                         location establishment process, otherwise
  *                                        zero will be returned.
 * @param[out] pSvs                        a place to store the number of
 *                                         space vehicles used in the
 *                                         solution; may be NULL. This field is only
 *                                         populated if there is a GNSS chip attached
 *                                         to the cellular module which is used in the
 *                                         Cell Locate location establishment process,
 *                                         otherwise zero will be returned.
 * @param[out] pTimeUtc                    a place to put the UTC time; may be NULL.
 * @param[in] pKeepGoingCallback           a callback function that governs how
 *                                         long a location establishment may continue
 *                                         for. This function is called once a second
 *                                         while waiting for a location fix; the
 *                                         location establishment attempt will only
 *                                         continue while it returns true.  This allows
 *                                         the caller to terminate the establishment
 *                                         attempt at their convenience. This function
 *                                         may also be used to feed any watchdog timer
 *                                         that may be running.  The single int32_t
 *                                         parameter is the cell handle. May be NULL,
 *                                         in which case the location establishment
 *                                         attempt will time-out after
 *                                         #U_CELL_LOC_TIMEOUT_SECONDS seconds.
 * @return                                 zero on success or negative error code on
 *                                         failure.
 */
int32_t uCellLocGet(uDeviceHandle_t cellHandle,
                    int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                    int32_t *pAltitudeMillimetres, int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond,
                    int32_t *pSvs, int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Get the current location, non-blocking version.  This will ONLY
 * work if the cellular module is currently registered on a network
 * (e.g. as a result of uCellNetConnect() or uCellNetRegister() being
 * called). The location establishment attempt will time-out after
 * #U_CELL_LOC_TIMEOUT_SECONDS.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @param[in] pCallback  a callback that will be called when a fix has
 *                       been obtained.  The position fix is only valid
 *                       if the second int32_t, errorCode, is zero.
 *                       The first parameter to the callback is the
 *                       cellular handle, the parameters after errorCode
 *                       are as described in uCellLocGet() except that
 *                       they are not pointers.
 * @return               zero on success or negative error code on
 *                       failure.
 */
int32_t uCellLocGetStart(uDeviceHandle_t cellHandle,
                         void (*pCallback) (uDeviceHandle_t cellHandle,
                                            int32_t errorCode,
                                            int32_t latitudeX1e7,
                                            int32_t longitudeX1e7,
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
int32_t uCellLocGetStatus(uDeviceHandle_t cellHandle);

/** Cancel a uCellLocGetStart(); after calling this function the
 * callback passed to uCellLocGetStart() will not be called until
 * another uCellLocGetStart() is begun.  Note that this causes the
 * code here to stop waiting for any answer coming back from the
 * cellular module but the module may still send such an answer and,
 * since there is no reference count in it, if uCellLocGetStart() is
 * called again quickly it may pick up the first answer (and then
 * the subsequent answer will be ignored, etc.).
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellLocGetStop(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_LOC_H_

// End of file
