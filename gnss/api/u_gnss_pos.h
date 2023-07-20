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

#ifndef _U_GNSS_POS_H_
#define _U_GNSS_POS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the GNSS APIs to read position.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POS_TIMEOUT_SECONDS
/** The timeout for position establishment in seconds.
 */
# define U_GNSS_POS_TIMEOUT_SECONDS 240
#endif

/** The default streamed position period in milliseconds.
 */
#define U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS 1000

/** The recommended minimum number of satellites required to
 * be visible and meet the criteria when calling uGnssPosGetRrlp()
 * for the Cloud Locate service.
 */
#define U_GNSS_RRLP_SVS_THRESHOLD_RECOMMENDED 5

/** The recommended threshold to use for carrier to noise
 * ratio when calling uGnssPosGetRrlp() for the Cloud Locate service.
 */
#define U_GNSS_RRLP_C_NO_THRESHOLD_RECOMMENDED 30

/** The recommended limit to use for multipath index when calling
 * uGnssPosGetRrlp() for the Cloud Locate service.
 */
#define U_GNSS_RRLP_MULTIPATH_INDEX_LIMIT_RECOMMENDED 1

/** The recommended limit to use for the pseudorange RMS error
 * index when calling uGnssPosGetRrlp() for the Cloud Locate service.
 */
#define U_GNSS_RRLP_PSEUDORANGE_RMS_ERROR_INDEX_LIMIT_RECOMMENDED 3

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
void uGnssPosPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the current position, one-shot, returning on success or when
 * pKeepGoingCallback returns false; this will work with any
 * transport type.
 *
 * @param gnssHandle                       the handle of the GNSS instance
 *                                         to use.
 * @param[out] pLatitudeX1e7               a place to put latitude (in ten
 *                                         millionths of a degree); may
 *                                         be NULL.
 * @param[out] pLongitudeX1e7              a place to put longitude (in ten
 *                                         millionths of a degree); may be
 *                                         NULL.
 * @param[out] pAltitudeMillimetres        a place to put the altitude (in
 *                                         millimetres); may be NULL.
 * @param[out] pRadiusMillimetres          a place to put the radius of
 *                                         position (in millimetres); may
 *                                         be NULL.  If the radius is
 *                                         unknown -1 will be returned.
 * @param[out] pSpeedMillimetresPerSecond  a place to put the speed (in
 *                                         millimetres per second); may be
 *                                         NULL.  If the speed is unknown
 *                                         -1 will be returned.
 * @param[out] pSvs                        a place to store the number of
 *                                         space vehicles used in the
 *                                         solution; may be NULL. If the
 *                                         number of space vehicles is
 *                                         unknown or irrelevant -1 will
 *                                         be returned.
 * @param[out] pTimeUtc                    a place to put the UTC time;
 *                                         may be NULL. If the time is
 *                                         unknown -1 will be returned.
 *                                         Note that this is the time of
 *                                         the fix and, by the time the
 *                                         fix is returned, it may not
 *                                         represent the *current* time.
 *                                         Note that this value may be
 *                                         populated even if the return
 *                                         value of the function is not
 *                                         success, since time may be
 *                                         available even if a position
 *                                         fix is not.
 * @param[in] pKeepGoingCallback           a callback function that governs
 *                                         how long position-fixing is
 *                                         allowed to take. This function
 *                                         is called while waiting for
 *                                         position establishment to complete;
 *                                         position establishment will only
 *                                         continue while it returns true.
 *                                         This allows the caller to terminate
 *                                         the locating process at their
 *                                         convenience. This function may
 *                                         also be used to feed any watchdog
 *                                         timer that might be running. May
 *                                         be NULL, in which case position
 *                                         establishment will stop when
 *                                         #U_GNSS_POS_TIMEOUT_SECONDS have
 *                                         elapsed.  The single int32_t
 *                                         parameter is the GNSS handle.
 * @return                                 zero on success or negative error
 *                                         code on failure.
 */
int32_t uGnssPosGet(uDeviceHandle_t gnssHandle,
                    int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                    int32_t *pAltitudeMillimetres,
                    int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond,
                    int32_t *pSvs, int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** A non-blocking version of uGnssPosGet(), so this is still a
 * one-shot operation but the answer arrives via a callback; this will work
 * with any transport, however see uGnssPosGetStreamedStart() if you want
 * streamed position.
 *
 * Should you wish to cancel a request, or start a new request without
 * waiting for the answer to the previous request, then you must call
 * uGnssPosGetStop() first (otherwise #U_ERROR_COMMON_NO_MEMORY will
 * be returned).  uGnssPosGetStart() creates a mutex for thread-safety
 * which remains in memory until the GNSS API is deinitialised; should
 * you wish to free the memory occupied by the mutex then calling
 * uGnssPosGetStop() will also do that.
 *
 * @param gnssHandle     the handle of the GNSS instance to use.
 * @param[in] pCallback  a callback that will be called when a fix has been
 *                       obtained.  The parameters to the callback are as
 *                       described in uGnssPosGet() except that they are
 *                       not pointers.  The position fix is only valid
 *                       if the second int32_t, errorCode, is zero but
 *                       a timeUtc value may still be included even
 *                       if a position fix has failed (timeUtc will be
 *                       set to -1 if the UTC time is not valid).
 *                       Note: don't call back into this API from your
 *                       pCallback, it could lead to recursion.
 * @return               zero on success or negative error code on
 *                       failure.
 */
int32_t uGnssPosGetStart(uDeviceHandle_t gnssHandle,
                         void (*pCallback) (uDeviceHandle_t gnssHandle,
                                            int32_t errorCode,
                                            int32_t latitudeX1e7,
                                            int32_t longitudeX1e7,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int64_t timeUtc));

/** Cancel a uGnssPosGetStart(); after this function has returned the
 * callback passed to uGnssPosGetStart() will not be called until
 * another uGnssPosGetStart() is begun.  The start function also creates
 * a mutex for thread safety which will remain in the system even after
 * pCallback has been called; this will free the memory it occupies.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
void uGnssPosGetStop(uDeviceHandle_t gnssHandle);

/** Get position readings streamed constantly to a callback; this will
 * only work with one of the streamed transports (for instance UART, I2C,
 * SPI or Virtual Serial), it will NOT work with AT-command-based transport
 * (#U_GNSS_TRANSPORT_AT).  uGnssPosGetStart() allocates some storage
 * which remains in memory until the GNSS API is deinitialised; should
 * you wish to free that memory then calling uGnssPosGetStreamedStop()
 * will also do that.
 *
 * Note: under the hood the UBX protocol is used to establish position.
 * By default, NMEA position is _also_ output from the GNSS chip: to
 * reduce the load on the system you may want to, at least temporarily,
 * disable NMEA output using uGnssCfgSetProtocolOut(); if you use
 * uLocationGetContinuousStart() instead of uGnssPosGetStreamedStart()
 * it will do this for you.
 *
 * Note: this uses one of the #U_GNSS_MSG_RECEIVER_MAX_NUM message
 * handles from the uGnssMsg API.
 *
 * To cancel streamed position, call uGnssPosGetStreamedStop().
 *
 * @param gnssHandle       the handle of the GNSS instance to use.
 * @param rateMs           the desired time between position fixes in
 *                         milliseconds. The rate specified here will
 *                         be applied to the measurement interval and
 *                         the navigation count (i.e. the number of measurements
 *                         required to make a navigation solution) will
 *                         be 1.  If you want to use a navigation count
 *                         greater 1 one you may set that by calling
 *                         uGnssCfgSetRate() before this function and
 *                         then setting rateMs here to -1, which will leave
 *                         the rate settings unchanged.
 * @param[in] pCallback    a callback that will be called when fixes are
 *                         obtained.  The parameters to the callback are as
 *                         described in uGnssPosGetStart().
 *                         Note: don't call back into this API from your
 *                         pCallback, it could lead to recursion.
 *                         IMPORTANT: you should check the value of
 *                         errorCode before treating rhe parameters:
 *                         a value of zero means that a position fix
 *                         has been achieved but a value of
 *                         #U_ERROR_COMMON_TIMEOUT may be used to
 *                         indicate that a message has arrived from the
 *                         GNSS device giving no position fix or a
 *                         time-only fix.  Where no fix is achieved the
 *                         variables will be populated with out of range
 *                         values (i.e. INT_MIN or -1 as appopriate).
 * @return                 zero on success or negative error code on
 *                         failure.
 */
int32_t uGnssPosGetStreamedStart(uDeviceHandle_t gnssHandle,
                                 int32_t rateMs,
                                 void (*pCallback) (uDeviceHandle_t gnssHandle,
                                                    int32_t errorCode,
                                                    int32_t latitudeX1e7,
                                                    int32_t longitudeX1e7,
                                                    int32_t altitudeMillimetres,
                                                    int32_t radiusMillimetres,
                                                    int32_t speedMillimetresPerSecond,
                                                    int32_t svs,
                                                    int64_t timeUtc));

/** Cancel a uGnssPosGetStreamedStart(); after this function has returned
 * the callback passed to uGnssPosGetStreamedStart() will not be called
 * until another uGnssPosGetStreamedStart() is begun.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
void uGnssPosGetStreamedStop(uDeviceHandle_t gnssHandle);

/** Set the mode for uGnssPosGetRrlp(); M10 modules or later only.
 * If this is not called U_GNSS_RRLP_MODE_MEASX will apply.  Setting
 * modes #U_GNSS_RRLP_MODE_MEAS50, #U_GNSS_RRLP_MODE_MEAS20,
 * #U_GNSS_RRLP_MODE_MEASC12 / #U_GNSS_RRLP_MODE_MEASD12 etc. reduces
 * the volume of data required with some loss of accuracy (e.g.
 * 10 metres for UBX-RXM-MEASX/UBX-RXM-MEAS50 versus 20-30 metres
 * for UBX-RXM-MEAS20, 30-40 metres for UBX-RXM-MEASC12/UBX-RXM-MEASD12
 * etc.).
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param mode        the RRLP mode.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPosSetRrlpMode(uDeviceHandle_t gnssHandle, uGnssRrlpMode_t mode);

/** Get the mode for uGnssPosGetRrlp().
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the RRLP mode, else negative error code.
 */
int32_t uGnssPosGetRrlpMode(uDeviceHandle_t gnssHandle);

/** Get the binary RRLP information directly from the GNSS chip.  By default
 * this will obtain the RRLP information using the UBX-RXM-MEASX command of
 * the UBX protocol; if you have an M10 device or later you may use
 * uGnssPosSetRrlpMode() to select instead the UBX-RXM-MEAS50, UBX-RXM-MEAS20
 * or UBX-RXM-MEASC12/UBX-RXM-MEASD12 commands for reduced payload sizes
 * versus position accuracy (10 metres for UBX-RXM-MEASX/UBX-RXM-MEAS50,
 * 20-30 metres for UBX-RXM-MEAS20, 30-40 metres for UBX-RXM-MEASC12/
 * UBX-RXM-MEASD12 etc.).
 *
 * Using RRLP information is more efficient, both in terms of power and time,
 * than asking for position: the RRLP information may be sent to the u-blox
 * Cloud Locate service where the exact position of the device can
 * be determined and made available, e.g. for trackers.
 *
 * @param gnssHandle                    the handle of the GNSS instance to use.
 * @param pBuffer                       a place to store the binary RRLP
 *                                      information; cannot be NULL.  The
 *                                      storage required is the UBX protocol
 *                                      overhead (8 bytes) plus an amount that
 *                                      depends upon the RRLP mode.  For
 *                                      #U_GNSS_RRLP_MODE_MEASX this is
 *                                      44 + 24 * [number of satellites]
 *                                      so for, e.g. 32 satellites, 820 bytes
 *                                      would be sufficient.  For the other
 *                                      modes the number of bytes is indicated
 *                                      by the mode, so for #U_GNSS_RRLP_MODE_MEAS50
 *                                      8 + 50 bytes would be required, for
 *                                      #U_GNSS_RRLP_MODE_MEAS20 8 + 20 bytes and
 *                                      for #U_GNSS_RRLP_MODE_MEASC12 /
 *                                      #U_GNSS_RRLP_MODE_MEASD12 8 + 12 bytes.
 *                                      If you intend to send the RRLP information
 *                                      to the Cloud Locate service then, for
 *                                      #U_GNSS_RRLP_MODE_MEASX you MUST include
 *                                      the six bytes of message header of the UBX
 *                                      protocol, i.e. 0xB5 62 02 14 AA BB (where
 *                                      0xAA BB is the [little-endian coded] 16-bit
 *                                      length of the RRLP data that follows) AND
 *                                      the two CRC bytes on the end, as the Cloud
 *                                      Locate service expects these and checks
 *                                      them.  However, when using the other formats
 *                                      the header and CRC bytes must NOT be sent to
 *                                      the Cloud Locate service, hence you should
 *                                      offset your read from pBuffer by six
 *                                      bytes to skip the header and take eight from
 *                                      the length to also remove the two CRC bytes.
 * @param sizeBytes                     the number of bytes of storage at pBuffer.
 * @param svsThreshold                  the minimum number of satellites that must
 *                                      be visible to return the RRLP information;
 *                                      specify -1 for "don't care"; the recommended
 *                                      value to use for the Cloud Locate service is 5.
 *                                      Ignored if the RRLP mode is not
 *                                      #U_GNSS_RRLP_MODE_MEASX, since in those cases
 *                                      the thresholding is performed in the GNSS module.
 * @param cNoThreshold                  the minimum carrier to noise value that must
 *                                      be met to return the RRLP information, range
 *                                      0 to 63; specify -1 for "don't care".  The
 *                                      ideal value to use for the Cloud Locate service
 *                                      is 35 but that requires clear sky and a good
 *                                      antenna, hence the recommended value is 30;
 *                                      lower threshold values may work, just less
 *                                      reliably. Ignored if the RRLP mode is not
 *                                      #U_GNSS_RRLP_MODE_MEASX, since in those cases
 *                                      the thresholding is performed in the GNSS module.
 * @param multipathIndexLimit           the maximum multipath index that must be
 *                                      met to return the RRLP information, 1 = low,
 *                                      2 = medium, 3 = high; specify -1 for "don't
 *                                      care".  The recommended value for the Cloud
 *                                      Locate service is 1. Ignored if the RRLP mode
 *                                      is not #U_GNSS_RRLP_MODE_MEASX, since in those
 *                                      cases the thresholding is performed in the GNSS
 *                                      module.
 * @param pseudorangeRmsErrorIndexLimit the maximum pseudorange RMS error index that
 *                                      must be met to return the RRLP information;
 *                                      specify -1 for "don't care".  The recommended
 *                                      value for the Cloud Locate service is 3.
 *                                      Ignored if the RRLP mode is not
 *                                      #U_GNSS_RRLP_MODE_MEASX, since in those cases
 *                                      the thresholding is performed in the GNSS module.
 * @param[in] pKeepGoingCallback        a callback function that governs the wait. This
 *                                      This function is called while waiting for RRLP
 *                                      data that meets the criteria; the API will
 *                                      only continue to wait while the callback function
 *                                      returns true.  This allows the caller to terminate
 *                                      the process at their convenience. The function may
 *                                      also be used to feed any watchdog timer that
 *                                      might be running. May be NULL, in which case
 *                                      this function will stop when
 *                                      #U_GNSS_POS_TIMEOUT_SECONDS have elapsed.  The
 *                                      single int32_t parameter is the GNSS handle.
 * @return                              on success the number of bytes returned, else
 *                                      negative error code.
 */
int32_t uGnssPosGetRrlp(uDeviceHandle_t gnssHandle, char *pBuffer, size_t sizeBytes,
                        int32_t svsThreshold, int32_t cNoThreshold,
                        int32_t multipathIndexLimit,
                        int32_t pseudorangeRmsErrorIndexLimit,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t));

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_POS_H_

// End of file
