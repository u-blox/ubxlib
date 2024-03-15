/*
 * Copyright 2019-2024 u-blox
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

#ifndef _U_LOCATION_SHARED_H_
#define _U_LOCATION_SHARED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions that do not form part,
 * of the location API but are shared internally for use with the
 * network API, for example so that it can clear-up asynchronous
 * location requests when a network is taken down.
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

/** The FIFOs to push/pop to/from for asynchronous requests.
 */
typedef enum {
    U_LOCATION_SHARED_FIFO_NONE,
    U_LOCATION_SHARED_FIFO_GNSS,
    U_LOCATION_SHARED_FIFO_CELL_LOCATE,
    U_LOCATION_SHARED_FIFO_WIFI
} uLocationSharedFifo_t;

/** Structure of the things we need to remember for Wifi location,
 * required since that needs to be freshly configured each time.
 */
typedef struct {
    const char *pApiKey;
    int32_t accessPointsFilter;
    int32_t rssiDbmFilter;
} uLocationSharedWifiSettings_t;

/** FIFO entry to keep track of asynchronous location requests.
 */
typedef struct uLocationSharedFifoEntry_t {
    uDeviceHandle_t devHandle;
    int32_t desiredRateMs;
    uLocationType_t type;
    const uLocationSharedWifiSettings_t *pWifiSettings;
    void (*pCallback) (uDeviceHandle_t devHandle,
                       int32_t errorCode,
                       const uLocation_t *pLocation);
    struct uLocationSharedFifoEntry_t *pNext;
} uLocationSharedFifoEntry_t;

/* ----------------------------------------------------------------
 * SHARED VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the FIFO.
 */
extern uPortMutexHandle_t gULocationMutex;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the internally shared location API: should
 * be called by the network API when it initialises itself.
 * gULocationMutex should NOT be locked before this
 * is called (since this creates the mutex).
 *
 * @return  zero on success or negative error code.
 */
int32_t uLocationSharedInit();

/** De-initialise the internally shared location API: should
 * be called by the network API when it de-initialises itself.
 * gULocationMutex should NOT be locked before this
 * is called (since this deletes the mutex).
 */
void uLocationSharedDeinit();

/** Add a new location request to the FIFO.
 *
 * IMPORTANT: gULocationMutex should be locked before this
 * is called.
 *
 * @param devHandle         the handle of the device making the request.
 * @param fifo              the FIFO (GNSS, CellLocate or Wifi).
 * @param type              the request type.
 * @param desiredRateMs     the desired location rate, for continuous
 *                          measurements only; use 0 for one-shot.
 *                          pushed when the current one has been called.
 * @param[in] pWifiSettings WiFi has to be set up for each attempt and
 *                          hence the settings required can be kept
 *                          here; use NULL if the FIFO type is not
 *                          #U_LOCATION_SHARED_FIFO_WIFI.
 * @param[in] pCallback     the callback associated with the request.
 * @return                  zero on success or negative error code.
 */
int32_t uLocationSharedRequestPush(uDeviceHandle_t devHandle,
                                   uLocationSharedFifo_t fifo,
                                   uLocationType_t type,
                                   int32_t desiredRateMs,
                                   const uLocationSharedWifiSettings_t *pWifiSettings,
                                   void (*pCallback) (uDeviceHandle_t devHandle,
                                                      int32_t errorCode,
                                                      const uLocation_t *pLocation));

/** Pop the oldest location request of the given FIFO.
 *
 * IMPORTANT: gULocationMutex should be locked before this
 * is called.
 *
 * @param fifo the FIFO to pop from.
 * @return     the entry pointer: it is removed from the list and
 *             hence it is up to the calling task to free the pointer
 *             when done; NULL is returned if the FIFO is empty.
 */
uLocationSharedFifoEntry_t *pULocationSharedRequestPop(uLocationSharedFifo_t fifo);

/** Free a FIFO entry (e.g. as returned by
 * pULocationSharedRequestPop()) when done.
 *
 * IMPORTANT: gULocationMutex should be locked before this
 * is called.
 *
 * @param[in] pFifoEntry  the FIFO entry to free.
 */
void uLocationSharedFifoEntryFree(uLocationSharedFifoEntry_t *pFifoEntry);

#ifdef __cplusplus
}
#endif

#endif // _U_LOCATION_SHARED_H_

// End of file
