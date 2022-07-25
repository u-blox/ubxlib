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
 * service.  These functions are thread-safe with the following
 * exceptions:
 *
 * - uCellLocCleanUp() should not be called while location
 *   establishment is running.
 * - a cellular instance should not be deinitialised while location
 *   establishment is running.
 *
 * To use the Cell Locate service you will need to obtain an
 * authentication token from the Location Services
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

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

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
 * @return            the desrired timeout in seconds.
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

/** Configure the Cell Locate server parameters, in particular
 * authentication token that is required to use the Cell Locate
 * service.  This may be obtained from the Location Services
 * section of your Thingstream portal
 * (https://portal.thingstream.io/app/location-services).
 * The cellular module must be powered-on for this to work.
 * If the cellular module is powered off this setting will be
 * forgotten.
 *
 * @param cellHandle                  the handle of the cellular instance.
 * @param[in] pAuthenticationTokenStr a pointer to the null-terminated
 *                                    authentication token for the Cell
 *                                    Locate server. May be NULL, in which
 *                                    case pPrimaryServerStr and
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
 * FUNCTIONS: LOCATION ESTABLISHMENT
 * -------------------------------------------------------------- */

/** Get the current location, returning on success or if
 * pKeepGoingCallback returns false.  This will ONLY work if
 * the cellular module is currently registered on a network
 * (e.g. as a result of uCellNetConnect() or uCellNetRegister()
 * being called).
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
 * the subsequent answer one will be ignored, etc.).
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
