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
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the current position, returning on success or when
 * pKeepGoingCallback returns false.
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
 * one-shot operation but the answer arrives via a callback.  Should
 * you wish to cancel a request, or start a new request without waiting
 * for the answer to the previous request, then you must call
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
 * callback passed to uGnssPosGetStart() will not be called until another
 * uGnssPosGetStart() is begun.  uGnssPosGetStart() also creates a mutex
 * for thread safety which will remain in the system even after
 * pCallback has been called; this will free the memory it occupies.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
void uGnssPosGetStop(uDeviceHandle_t gnssHandle);

/** Get the binary RRLP information directly from the GNSS chip,
 * as returned by the UBX-RXM-MEASX command of the UBX protocol.  This
 * is more efficient, both in terms of power and time, than asking
 * for position: the RRLP information may be sent to the u-blox
 * Cloud Locate service where the exact position of the device can
 * be determined and made available, e.g. for trackers.
 *
 * @param gnssHandle                    the handle of the GNSS instance to use.
 * @param pBuffer                       a place to store the binary RRLP
 *                                      information; cannot be NULL.  The
 *                                      storage required for each RRLP data set
 *                                      is: 8 + 44 + 24 * [number of satellites]
 *                                      so for, e.g. 32 satellites, 820 bytes
 *                                      would be sufficient.  Note that what
 *                                      is written to pBuffer includes the six
 *                                      bytes of message header of the UBX
 *                                      protocol, i.e. 0xB5 62 02 14 AA BB, where
 *                                      0xAA BB is the [little-endian coded] 16-bit
 *                                      length of the RRLP data that follows;
 *                                      the two CRC bytes from the UBX protocol are
 *                                      ALSO written to pBuffer because Cloud Locate
 *                                      not only expects them it requires them AND it
 *                                      checks them.
 * @param sizeBytes                     the number of bytes of storage at pBuffer.
 * @param svsThreshold                  the minimum number of satellites that must
 *                                      be visible to return the RRLP information;
 *                                      specify -1 for "don't care"; the recommended
 *                                      value to use for the Cloud Locate service is 5.
 * @param cNoThreshold                  the minimum carrier to noise value that must
 *                                      be met to return the RRLP information, range
 *                                      0 to 63; specify -1 for "don't care".  The
 *                                      ideal value to use for the Cloud Locate service
 *                                      is 35 but that requires clear sky and a good
 *                                      antenna, hence the recommended value is 30;
 *                                      lower threshold values may work, just less
 *                                      reliably.
 * @param multipathIndexLimit           the maximum multipath index that must be
 *                                      met to return the RRLP information, 1 = low,
 *                                      2 = medium, 3 = high; specify -1 for "don't
 *                                      care".  The recommended value for the Cloud
 *                                      Locate service is 1.
 * @param pseudorangeRmsErrorIndexLimit the maximum pseudorange RMS error index that
 *                                      must be met to return the RRLP information;
 *                                      specify -1 for "don't care".  The recommended
 *                                      value for the Cloud Locate service is 3.
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
