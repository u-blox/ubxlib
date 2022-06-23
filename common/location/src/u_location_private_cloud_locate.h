/*
 * Copyright 2019-2022 u-blox Ltd
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

#ifndef _U_LOCATION_PRIVATE_CLOUD_LOCATE_H_
#define _U_LOCATION_PRIVATE_CLOUD_LOCATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions that do not form part,
 * of the location API but are used internally to provide the Cloud
 * Locate API.
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

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Run Cloud Locate.
 *
 * @param devHandle                     the handle of the thing
 *                                      providing the MQTT
 *                                      connection (for example the
 *                                      cellular or Wi-Fi module).
 * @param gnssDevHandle                 the handle of the device
 *                                      that will provide the RRLP
 *                                      data for Cloud Locate; set
 *                                      this to the same value as
 *                                      devHandle if, for instance,
 *                                      the GNSS module is inside the
 *                                      cellular module that you are
 *                                      using for the MQTT connection.
 * @param pMqttClientContext            the context of an MQTT client
 *                                      that can be used to communicate
 *                                      with the Cloud Locate service;
 *                                      must already have been logged-in
 *                                      to the Cloud Locate service.
 * @param svsThreshold                  the minimum number of satellites
 *                                      that must be visible and meet
 *                                      the criteria for C/No threshold,
 *                                      multipath index and psuedorange
 *                                      RMS error for an RRLP data
 *                                      block to be considered usable;
 *                                      use -1 for "don't care".
 * @param cNoThreshold                  the minimum carrier to noise
 *                                      value that must be met for any
 *                                      single satellite's RRLP information
 *                                      to be considered usable, range
 *                                      0 to 63; specify -1 for
 *                                      "don't care".  The ideal value
 *                                       to use is 35 but that requires
 *                                      clear sky and a good antenna,
 *                                      hence the recommended value is
 *                                      30; lower threshold values may
 *                                      work, just less reliably.
 * @param multipathIndexLimit           the maximum multipath index that
 *                                      must be met for any single
 *                                      satellite's RRLP information to
 *                                      be considered valid, 1 = low,
 *                                      2 = medium, 3 = high; specify
 *                                      -1 for "don't care".  The
 *                                      recommended value is 1.
 * @param pseudorangeRmsErrorIndexLimit the maximum pseudorange RMS error
 *                                      index that must be met for any
 *                                      single satellite's RRLP information
 *                                      to be considered valid, specify -1
 *                                      for "don't care".  The recommended
 *                                      value is 3.
 * @param pClientIdStr                  the Thingstream device ID, obtained
 *                                      from the Thingstream portal, for
 *                                      this device; must be provided if
 *                                      pLocation is not NULL, must be
 *                                      null-terminated.
 * @param pLocation                     a place to put the location once
 *                                      established, may be NULL if this
 *                                      device does not require the
 *                                      location, if it is sufficient
 *                                      for it to be known in the cloud.
 * @param pKeepGoingCallback            a callback function that governs
 *                                      how long location establishment
 *                                      is allowed to take.  This function
 *                                      is called while waiting for
 *                                      location establishment to complete;
 *                                      location establishment will only
 *                                      continue while it returns true.
 *                                      This allows the caller to terminate
 *                                      the locating process at their
 *                                      convenience.  This function may
 *                                      also be used to feed any watchdog
 *                                      timer that might be running.  May
 *                                      be NULL, in which case location
 *                                      establishment will stop when
 *                                      #U_LOCATION_TIMEOUT_SECONDS have
 *                                      elapsed.  The single int32_t
 *                                      parameter is the network handle.
 */
int32_t uLocationPrivateCloudLocate(uDeviceHandle_t devHandle,
                                    uDeviceHandle_t gnssDevHandle,
                                    uMqttClientContext_t *pMqttClientContext,
                                    int32_t svsThreshold,
                                    int32_t cNoThreshold,
                                    int32_t multipathIndexLimit,
                                    int32_t pseudorangeRmsErrorIndexLimit,
                                    const char *pClientIdStr,
                                    uLocation_t *pLocation,
                                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

#ifdef __cplusplus
}
#endif

#endif // _U_LOCATION_PRIVATE_CLOUD_LOCATE_H_

// End of file
