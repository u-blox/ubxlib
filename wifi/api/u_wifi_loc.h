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

#ifndef _U_WIFI_LOC_H_
#define _U_WIFI_LOC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_location.h"

/** \addtogroup _wifi
 *  @{
 */

/** @file
 * @brief This header file defines the location APIs for Wi-Fi;
 * these APIs require that the module is running uConnectExpress
 * version 5 or higher.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_WIFI_LOC_REQUEST_TIMEOUT_SECONDS
/** How long to wait for Wi-Fi to perform the location request.
 */
# define U_WIFI_LOC_REQUEST_TIMEOUT_SECONDS 10
#endif

#ifndef U_WIFI_LOC_ANSWER_TIMEOUT_SECONDS
/** How long to wait for a location fix to be returned in seconds.
 */
# define U_WIFI_LOC_ANSWER_TIMEOUT_SECONDS 30
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Callback for asynchronous Wi-Fi location.
 *
 * @param wifiHandle the handle of the Wi-Fi instance.
 * @param errorCode  the outcome of the location request: zero on
 *                   success, negative error code if there has
 *                   been a local error or, if the cloud service
 *                   has returned an HTTP status code which does NOT
 *                   indicate success (e.g. some return 206
 *                   to indicate "location not found") then that
 *                   will be returned.
 * @param pLocation  the location; may be NULL on error.  The
 *                   callback must take a copy of the contents of
 *                   pLocation as they will not be valid after
 *                   the callback has returned.
 */
typedef void (uWifiLocCallback_t) (uDeviceHandle_t wifiHandle,
                                   int32_t errorCode,
                                   const uLocation_t *pLocation);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get location (blocking) from the likes of Google, Skyhook and
 * Here; requires uConnectExpress version 5 and up.  The module must
 * have already been connected to an access point for this function
 * to work.
 *
 * When this function is first called it will allocate some memory
 * for thread-safety which is never free'd; should you need that
 * memory back then call uWifiLocFree().
 *
 * Note: internally the module uses one of the two available HTTP
 * sessions to do the location request.
 *
 * @param wifiHandle          the handle of the Wi-Fi instance to
 *                            be used.
 * @param type                the type of location to perform.
 * @param[in] pApiKey         the null-terminated API key for the
 *                            location service; cannot be NULL, must
 *                            be a true constant, i.e. no copy is
 *                            taken.
 * @param accessPointsFilter  the number of access points that
 *                            must be visible for location to
 *                            be requested, range 5 to 16.
 * @param rssiDbmFilter       ignore access points with received
 *                            signal strength less than this,
 *                            range -100 dBm to 0 dBm.
 * @param[out] pLocation      a pointer to a location structure
 *                            that will be populated with the
 *                            location if successful.
 * @param pKeepGoingCallback  a callback function that governs
 *                            how long location establishment
 *                            is allowed to take.  This function
 *                            is called while waiting for the
 *                            answer from the cloud service (so
 *                            after the scan phase); location
 *                            establishment will only continue
 *                            while it returns true.
 *                            This allows the caller to terminate
 *                            the locating process at their
 *                            convenience.  This function may
 *                            also be used to feed any watchdog
 *                            timer that might be running.  May
 *                            be NULL, in which case location
 *                            establishment will stop when
 *                            up to #U_WIFI_LOC_REQUEST_TIMEOUT_SECONDS +
 *                            #U_WIFI_LOC_ANSWER_TIMEOUT_SECONDS have
 *                            elapsed.  The single int32_t
 *                            parameter is the device handle.
 * @return                    zero on success; if there is a local
 *                            error then a negative error code
 *                            will be returned, else if the cloud
 *                            service returns an HTTP status code that
 *                            does not indicate success (e.g. 206 is
 *                            used by some cloud services to indicate
 *                            "unable to determine location") then
 *                            that will be returned.
 */
int32_t uWifiLocGet(uDeviceHandle_t wifiHandle,
                    uLocationType_t type, const char *pApiKey,
                    int32_t accessPointsFilter,
                    int32_t rssiDbmFilter,
                    uLocation_t *pLocation,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Get the current location, non-blocking version.  Requires
 * uConnectExpress version 5 and up.  The module must have already
 * been connected to an access point for this function to work.
 * Call uWifiLocGetStop() to stop the location request and free
 * memory.  You MUST complete a Wi-Fi location request that has
 * started, either by the callback being called or by calling
 * uWifiLocGetStop(), before calling this function again or
 * before the device is deinitialised.  This is a one-shot location:
 * once the callback has been called it will not be called again
 * unless you call this function again.  For a "continuous" position
 * fix, see the uLocation API.
 *
 * When this function is first called it will allocate some memory
 * for thread-safety which is never free'd; should you need that
 * memory back, provided you are ABSOLUTELY SURE that no Wi-Fi
 * location request is on-going (e.g. you have called
 * uWifiLocGetStop()) then call uWifiLocFree().
 *
 * Note: internally the module uses one of the two available HTTP
 * sessions to do the location request.
 *
 * @param wifiHandle          the handle of the Wi-Fi instance to
 *                            be used.
 * @param type                the type of location to perform.
 * @param[in] pApiKey         the null-terminated API key for the
 *                            location service; cannot be NULL, must
 *                            be a true constant, i.e. no copy is
 *                            taken.
 * @param accessPointsFilter  the number of access points that
 *                            must be visible for location to
 *                            be requested, range 5 to 16.
 * @param rssiDbmFilter       ignore access points with received
 *                            signal strength less than this,
 *                            range -100 dBm to 0 dBm.
 * @param[in] pCallback       a callback that will be called when a
 *                            fix has been obtained.  The position
 *                            fix is only valid if the errorCode
 *                            parameter to the callback is zero.
 *                            The callback MUST take a copy
 *                            of the contents of the pLocation
 *                            parameter, if it is non-NULL, before
 *                            the callback returns.
 * @return                    zero on success or negative error code
 *                            on failure.
 */
int32_t uWifiLocGetStart(uDeviceHandle_t wifiHandle,
                         uLocationType_t type, const char *pApiKey,
                         int32_t accessPointsFilter,
                         int32_t rssiDbmFilter,
                         uWifiLocCallback_t *pCallback);

/** Cancel a uWifiLocGetStart(); after calling this function the
 * callback passed to uWifiLocGetStart() will not be called until
 * another uWifiLocGetStart() is begun.  Note that this causes the
 * code here to stop waiting for any answer coming back from the
 * cloud service but the service may still send such an answer and,
 * since there is no reference count in it, if uWifiLocGetStart() is
 * called again quickly it may pick up the first answer (and then
 * the subsequent answer will be ignored, etc.).
 *
 * @param wifiHandle  the handle of the Wi-Fi instance to be used.
 */
void uWifiLocGetStop(uDeviceHandle_t wifiHandle);

/** When uWifiLocGet() or uWifiLocGetStart() are first called they
 * will allocate some memory (for thread-safety) that is never free'd.
 * If you need that memory back and you are ABSOLUTELY SURE that no
 * location request is running (e.g. you have called uWifiLocGetStop()
 * and waited a while to be really really sure) then you may free
 * that memory by calling this function.
 *
 * @param wifiHandle  the handle of the Wi-Fi instance to be used.
 */
void uWifiLocFree(uDeviceHandle_t wifiHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_WIFI_LOC_H_

// End of file
