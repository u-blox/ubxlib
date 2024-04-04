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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the Cell Locate API and the Assist Now API
 * for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // LONG_MIN, INT_MIN
#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "string.h"    // strstr()
#include "stdbool.h"
#include "ctype.h"     // isdigit()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* integer stdio, must be
                                              included before the
                                              other port files if any
                                              print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_timeout.h"

#include "u_time.h"

#include "u_at_client.h"

#include "u_location.h"

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_shared.h"

#include "u_gnss_mga.h" // uGnssMgaDataType_t

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"
#include "u_cell_net.h"     // Order is important
#include "u_cell_private.h" // here don't change it

#include "u_cell_loc.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_LOC_ENTRY_FUNCTION(cellHandle, ppInstance, pErrorCode) \
                                  { entryFunction(cellHandle, \
                                                  ppInstance, \
                                                  pErrorCode)

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_LOC_EXIT_FUNCTION() } exitFunction()

#ifndef U_CELL_LOC_MIN_UTC_TIME
/** If cell locate is unable to establish a location it will
 * return one with an invalid timestamp (e.g. some time in 2015).
 * This is a minimum value to check against (21 July 2021 13:40:36).
 */
# define U_CELL_LOC_MIN_UTC_TIME 1626874836
#endif

#ifndef U_CELL_LOC_AUTHENTICATION_TOKEN_STR_MAX_LEN_BYTES
/** The maximum length of a CellLocate/AssistNow server
 * authentication token NOT INCLUDING the null terminator.
 */
# define U_CELL_LOC_AUTHENTICATION_TOKEN_STR_MAX_LEN_BYTES 64
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The type of fix data storage.
 * "block" type data storage is associated with the type
 * uCellLocFixDataStorageBlock_t and is used locally within
 * this API: we create a data block (e.g. on the stack), let position
 * be established (monitoring the errorCode field) and then read
 * the result out and release the data block.
 * "callback" type data storage is associated with pCallback and
 * allows asynchronous operation.
 */
typedef enum {
    U_CELL_LOC_FIX_DATA_STORAGE_TYPE_BLOCK,
    U_CELL_LOC_FIX_DATA_STORAGE_TYPE_CALLBACK
} uCellLocFixDataStorageType_t;

/** The aid_mode values in AT+UGPS.
 */
typedef enum {
    U_CELL_LOC_AID_MODE_AUTOMATIC_LOCAL = 1,
    U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE = 2,
    U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE = 4,
    U_CELL_LOC_AID_MODE_ASSIST_NOW_AUTONOMOUS = 8
} uCellLocAidMode_t;

/** Structure in which to store a position fix.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    int32_t errorCode;
    int32_t latitudeX1e7;
    int32_t longitudeX1e7;
    int32_t altitudeMillimetres;
    int32_t radiusMillimetres;
    int32_t speedMillimetresPerSecond;
    int32_t svs;
    int64_t timeUtc;
} uCellLocFixDataStorageBlock_t;

/** Union of the possible fix data storage types.
 */
typedef union {
    volatile uCellLocFixDataStorageBlock_t *pBlock;
    void (*pCallback) (uDeviceHandle_t cellHandle,
                       int32_t errorCode,
                       int32_t latitudeX1e7,
                       int32_t longitudeX1e7,
                       int32_t altitudeMillimetres,
                       int32_t radiusMillimetres,
                       int32_t speedMillimetresPerSecond,
                       int32_t svs,
                       int64_t timeUtc);
} uCellLocFixDataStorageUnion_t;

/** Structure to bring together the #uCellLocFixDataStorageUnion_t
 * union the enum indicating what type of storage it is.
 */
typedef struct {
    uCellLocFixDataStorageType_t type;
    uCellLocFixDataStorageUnion_t store;
} uCellLocFixDataStorage_t;

/** Structure for the URC to use as storage.
 */
typedef struct {
    uCellPrivateLocContext_t *pContext;
    uCellLocFixDataStorageBlock_t fixDataStorageBlock;
    uGeofenceContext_t *pFenceContext;
} uCellLocUrc_t;

/** Structure to hold a #uGeofenceDynamicStatus_t, plus the
 * associated device handle.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    uGeofenceDynamicStatus_t lastStatus;
} uCellLocGeofenceDynamicStatus_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A cache of #uGeofenceDynamicStatus_t.
 *
 * Note: we go to great lengths here to make a copy of things
 * that get passed to asynchronous calls in order to not be
 * caught out by instances being disassembled underneath us,
 * etc., i.e. to ensure thread-safety.  This presents a problem
 * for the geofence case since the #uGeofenceDynamicStatus_t part of
 * the geofence context structure needs to be read and then
 * _updated_ by uGeofenceContextTest(), which obviously
 * won't work if you have a copy.  Hence what we do here is
 * keep a cache of up to #U_CELL_LOC_GEOFENCE_NUM_CACHED
 * #uGeofenceDynamicStatus_t for UULOC_urc_callback() to use and
 * update.
 */
static uCellLocGeofenceDynamicStatus_t gFenceDynamicsStatus[U_CELL_LOC_GEOFENCE_NUM_CACHED] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: URC RELATED
 * -------------------------------------------------------------- */

// Used by the URC; convert a number of the form "xx.yyyy"
// with a possible sign on the front, into an int32_t multiplied
// by 10 million (i.e. the lat/long format as an integer).
static int32_t numberToX1e7(const char *pNumber)
{
    uint32_t x1e7 = 0;
    int32_t x = 0;
    uint32_t y;
    bool isNegative = false;

    // Deal with the sign
    if (*pNumber == '-') {
        isNegative = true;
        pNumber++;
    } else if (*pNumber == '+') {
        pNumber++;
    }

    // Find out how many digits there are before the first
    // non-digit thing (which might be a decimal point).
    while (isdigit((int32_t) *(pNumber + x))) { // *NOPAD* stop AStyle making * look like a multiply
        x++;
    }

    // Now read those digits and accumulate them into x1e7
    while (x > 0) {
        y = *pNumber - '0';
        for (int32_t z = 1; z < x; z++) {
            y *= 10;
        }
        x1e7 += y;
        x--;
        pNumber++;
    }

    // Do the x1e7 bit
    x1e7 *= 10000000;

    if (*pNumber == '.') {
        // If we're now at a decimal point, skip over it and
        // deal with the fractional part of up to 7 digits
        pNumber++;
        x = 7;
        while (isdigit((int32_t) *pNumber) && (x > 0)) { // *NOPAD*
            y = *pNumber - '0';
            for (int32_t z = 1; z < x; z++) {
                y *= 10;
            }
            x1e7 += y;
            x--;
            pNumber++;
        }
    }

    return isNegative ? -(int32_t) x1e7 : (int32_t) x1e7;
}

// Get the stored fence dynamic status for the given cellular handle.
static uGeofenceDynamicStatus_t *pGetFenceDynamicStatus(uDeviceHandle_t cellHandle)
{
    uGeofenceDynamicStatus_t *pStatus = NULL;

    for (size_t x = 0; (x < sizeof(gFenceDynamicsStatus) / sizeof(gFenceDynamicsStatus[0])) &&
         (pStatus == NULL); x++) {
        if (cellHandle == gFenceDynamicsStatus[x].cellHandle) {
            pStatus = &(gFenceDynamicsStatus[x].lastStatus);
        }
    }

    return pStatus;
}

// Set the stored fence dynamic status for the given cellular handle.
static bool setFenceDynamicStatus(uDeviceHandle_t cellHandle,
                                  const uGeofenceDynamicStatus_t *pStatus)
{
    uCellLocGeofenceDynamicStatus_t *pTmp = NULL;

    if (cellHandle != NULL) {
        for (size_t x = 0; (x < sizeof(gFenceDynamicsStatus) / sizeof(gFenceDynamicsStatus[0])) &&
             (pTmp == NULL); x++) {
            if (cellHandle == gFenceDynamicsStatus[x].cellHandle) {
                pTmp = &(gFenceDynamicsStatus[x]);
            }
        }
        // If we did not find the entry, get an empty one so that
        // we can add it
        for (size_t x = 0; (x < sizeof(gFenceDynamicsStatus) / sizeof(gFenceDynamicsStatus[0])) &&
             (pTmp == NULL); x++) {
            if (gFenceDynamicsStatus[x].cellHandle == 0) {
                pTmp = &(gFenceDynamicsStatus[x]);
            }
        }
        if (pTmp != NULL) {
            // Update/add the entry
            if (pStatus != NULL) {
                pTmp->cellHandle = cellHandle;
                pTmp->lastStatus = *pStatus;
            } else {
                memset(pTmp, 0, sizeof(*pTmp));
            }
        }
    } else {
        // Reset the lot
        memset(&gFenceDynamicsStatus, 0, sizeof(gFenceDynamicsStatus));
    }

    return (cellHandle == NULL) || (pTmp != NULL);
}

// Handler that is called via uAtClientCallback()
// from the UULOC or UULOCIND URCs (the latter in case
// it indicates a fatal error) and ultimately either calls
// the user callback or dumps the data into a data block it
// was given for processing within this API. In
// BOTH cases it free's pContext->pFixDataStorage.
static void UULOC_urc_callback(uAtClientHandle_t atHandle, void *pParam)
{
    uCellLocUrc_t *pUrcStorage = (uCellLocUrc_t *) pParam;
    uCellPrivateLocContext_t *pContext;
    uCellLocFixDataStorage_t *pFixDataStorage;
    uCellLocFixDataStorageBlock_t *pFixDataStorageBlock;
    uGeofenceDynamicStatus_t *pFenceDynamicsStatus;

    (void) atHandle;

    if (pUrcStorage != NULL) {
        pContext = pUrcStorage->pContext;
        if (pContext != NULL) {

            // Lock the data storage mutex while we use it
            U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);

            pFixDataStorage = (uCellLocFixDataStorage_t *) pContext->pFixDataStorage;
            if (pFixDataStorage != NULL) {
                pFixDataStorageBlock = &(pUrcStorage->fixDataStorageBlock);
                switch (pFixDataStorage->type) {
                    case U_CELL_LOC_FIX_DATA_STORAGE_TYPE_BLOCK:
                        if (pFixDataStorage->store.pBlock != NULL) {
                            // Copy the data into the block;
                            *pFixDataStorage->store.pBlock = *pFixDataStorageBlock;
                        }
                        break;
                    case U_CELL_LOC_FIX_DATA_STORAGE_TYPE_CALLBACK:
                        if (pFixDataStorage->store.pCallback != NULL) {
                            pFixDataStorage->store.pCallback(pFixDataStorageBlock->cellHandle,
                                                             pFixDataStorageBlock->errorCode,
                                                             pFixDataStorageBlock->latitudeX1e7,
                                                             pFixDataStorageBlock->longitudeX1e7,
                                                             pFixDataStorageBlock->altitudeMillimetres,
                                                             pFixDataStorageBlock->radiusMillimetres,
                                                             pFixDataStorageBlock->speedMillimetresPerSecond,
                                                             pFixDataStorageBlock->svs,
                                                             pFixDataStorageBlock->timeUtc);
                        }
                        break;
                    default:
                        break;
                }

                if ((pUrcStorage->pFenceContext != NULL) && (pFixDataStorageBlock->errorCode == 0)) {
                    // Check out geofencing for this location, using the
                    // cached fence dynamic status, rather than the one we were
                    // passed, as it is solely this function that is keeping
                    // them up to date
                    pFenceDynamicsStatus = pGetFenceDynamicStatus(pFixDataStorageBlock->cellHandle);
                    if (pFenceDynamicsStatus != NULL) {
                        pUrcStorage->pFenceContext->dynamic.lastStatus = *pFenceDynamicsStatus;
                    }
                    uGeofenceContextTest(pFixDataStorageBlock->cellHandle,
                                         pUrcStorage->pFenceContext,
                                         U_GEOFENCE_TEST_TYPE_NONE, false,
                                         ((int64_t) pFixDataStorageBlock->latitudeX1e7) * 100,
                                         ((int64_t) pFixDataStorageBlock->longitudeX1e7) * 100,
                                         pFixDataStorageBlock->altitudeMillimetres,
                                         pFixDataStorageBlock->radiusMillimetres, -1);
                    // Update our cache with the outcome
                    setFenceDynamicStatus(pFixDataStorageBlock->cellHandle,
                                          &(pUrcStorage->pFenceContext->dynamic.lastStatus));
                    uPortFree(pUrcStorage->pFenceContext);
                }

                // Having called the callback we must free
                // the data storage; the block is unaffected,
                // that's the responsibility of whoever called us
                uPortFree(pContext->pFixDataStorage);
                pContext->pFixDataStorage = NULL;
            }

            U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);
        }

        // Free the URC storage
        uPortFree(pUrcStorage);
    }
}

// Callback for getting a fix from the +UULOC URC.
static void UULOC_urc(uAtClientHandle_t atHandle, void *pParam)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParam;
    uCellPrivateLocContext_t *pContext;
    int32_t length;
    size_t offset;
    int32_t numParameters = 0;
    int32_t months;
    int32_t year;
    int64_t timeUtc = LONG_MIN;
    int32_t latitudeX1e7 = INT_MIN;
    int32_t longitudeX1e7 = INT_MIN;
    int32_t altitudeMillimetres = INT_MIN;
    int32_t radiusMillimetres = INT_MIN;
    int32_t speedMillimetresPerSecond = 0;
    int32_t svs;
    char buffer[15]; // Enough room for "-180.0000000" plus a terminator
    uCellLocUrc_t *pUrcStorage;
    uCellLocFixDataStorageBlock_t *pFixDataStorageBlock;

    // Format is:
    // +UULOC: <date>,<time>,<lat>, <long>,<alt>,<uncertainty>
    // Date is of the form 07/11/2019
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        offset = 0;
        timeUtc = 0;
        // Day (1 to 31)
        buffer[offset + 2] = 0;
        timeUtc += (int64_t) (strtol(&(buffer[offset]), NULL, 10) - 1) * 3600 * 24;
        // Months converted to months since January
        offset = 3;
        buffer[offset + 2] = 0;
        // Month (1 to 12), so take away 1 to make it zero-based
        months = strtol(&(buffer[offset]), NULL, 10) - 1;
        // Four digit year converted to years since 1970
        offset = 6;
        buffer[offset + 4] = 0;
        year = strtol(&(buffer[offset]), NULL, 10) - 1970;
        months += year * 12;
        // Work out the number of seconds due to the year/month count
        timeUtc += uTimeMonthsToSecondsUtc(months);
        numParameters++;
    }
    // Time is of the form 10:48:43.000
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        // Hours since midnight
        offset = 0;
        buffer[offset + 2] = 0;
        timeUtc += (int64_t) strtol(&(buffer[offset]), NULL, 10) * 3600;
        // Minutes after the hour
        offset = 3;
        buffer[offset + 2] = 0;
        timeUtc += (int64_t) strtol(&(buffer[offset]), NULL, 10) * 60;
        // Seconds after the hour
        offset = 6;
        buffer[offset + 2] = 0;
        timeUtc += (int64_t) strtol(&(buffer[offset]), NULL, 10);
        numParameters++;
    }

    // latitude
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        latitudeX1e7 = numberToX1e7(buffer);
        numParameters++;
    }
    // longitude
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        longitudeX1e7 = numberToX1e7(buffer);
        numParameters++;
    }
    // altitude
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        altitudeMillimetres = strtol(buffer, NULL, 10) * 1000;
        numParameters++;
    }
    // radius
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        radiusMillimetres = strtol(buffer, NULL, 10) * 1000;
        numParameters++;
    }
    // speed
    length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (length > 0) {
        speedMillimetresPerSecond = strtol(buffer, NULL, 10) * 1000;
        numParameters++;
    }
    // skip <direction>,<vertical_acc>,<sensor_used>
    uAtClientSkipParameters(atHandle, 3);
    // number of space vehicles used
    svs = uAtClientReadInt(atHandle);
    if (svs >= 0) {
        numParameters++;
    }

    if ((numParameters >= 8) && (pInstance != NULL)) {
        pContext = pInstance->pLocContext;
        if (pContext != NULL) {
            // pUPortMalloc memory in which to pass the location data
            // to a callback, where we can safely lock the
            // data storage mutex
            // Note: the callback will free the memory allocated here
            //lint -esym(593, pUrcStorage) Suppress pUrcStorage not freed,
            // the callback does that
            pUrcStorage = (uCellLocUrc_t *) pUPortMalloc(sizeof(*pUrcStorage));
            if (pUrcStorage != NULL) {
                memset(pUrcStorage, 0, sizeof(*pUrcStorage));
                pUrcStorage->pContext = pContext;
                if (pInstance->pFenceContext != NULL) {
                    pUrcStorage->pFenceContext = (uGeofenceContext_t *) pUPortMalloc(sizeof(uGeofenceContext_t));
                    if (pUrcStorage->pFenceContext != NULL) {
                        *pUrcStorage->pFenceContext = *(uGeofenceContext_t *) pInstance->pFenceContext;
                    }
                }
                pFixDataStorageBlock = &(pUrcStorage->fixDataStorageBlock);
                pFixDataStorageBlock->latitudeX1e7 = latitudeX1e7;
                pFixDataStorageBlock->longitudeX1e7 = longitudeX1e7;
                pFixDataStorageBlock->altitudeMillimetres = altitudeMillimetres;
                pFixDataStorageBlock->radiusMillimetres = radiusMillimetres;
                pFixDataStorageBlock->speedMillimetresPerSecond = speedMillimetresPerSecond;
                pFixDataStorageBlock->svs = svs;
                pFixDataStorageBlock->timeUtc = timeUtc;
                pFixDataStorageBlock->cellHandle = pInstance->cellHandle;
                pFixDataStorageBlock->errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                if (timeUtc > U_CELL_LOC_MIN_UTC_TIME) {
                    pFixDataStorageBlock->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
                if (uAtClientCallback(atHandle, UULOC_urc_callback, pUrcStorage) != 0) {
                    uPortFree(pUrcStorage->pFenceContext);
                    uPortFree(pUrcStorage);
                }
            }
        }
    }
}

// Callback for getting fix status from the +UULOCIND URC.
// Note: we're meant to always get a +UULOC response so we
// don't need to do anything as a result of a +UULOCIND.
static void UULOCIND_urc(uAtClientHandle_t atHandle, void *pParam)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParam;
    uCellPrivateLocContext_t *pContext;
    int32_t step;
    int32_t result;

    if (pInstance != NULL) {
        pContext = pInstance->pLocContext;
        if (pContext != NULL) {
            step = uAtClientReadInt(atHandle);
            result = uAtClientReadInt(atHandle);

            if (uAtClientErrorGet(atHandle) == 0) {
                switch (step) {
                    case 0: // Network scan start
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_CELLULAR_SCAN_START;
                        break;
                    case 1: // Network scan end
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_CELLULAR_SCAN_END;
                        break;
                    case 2: // Requesting data from server
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_REQUESTING_DATA_FROM_SERVER;
                        break;
                    case 3: // Receiving data from the server
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_RECEIVING_DATA_FROM_SERVER;
                        break;
                    case 4: // Sending feedback to the server
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_SENDING_FEEDBACK_TO_SERVER;
                        break;
                    default:
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN;
                        break;
                }
                // The result integer gives a sub-status that is only relevant to
                // the server-comms-related statuses and, if more then 0, often represents
                // a fatal error.  While it may be related to any one of them the root
                // cause is likely to be the same for each and so it is simpler for
                // everyone just to report the detailed status without tying to figure out
                // which direction the problem is in.
                if (result > 0) {
                    pContext->fixStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN_COMMS_ERROR;
                    if (result <= 11) {
                        pContext->fixStatus = (int32_t) U_LOCATION_STATUS_FATAL_ERROR_HERE_AND_BEYOND + result - 1;
                    }
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: THE REST
 * -------------------------------------------------------------- */

// Ensure that there is a location context.
// gUCellPrivateMutex should be locked before this is called.
static int32_t ensureContext(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uCellPrivateLocContext_t *pContext;

    if (pInstance->pLocContext == NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // This is free'd by uCellDeinit() and uCellLocCleanUp()
        pContext = (uCellPrivateLocContext_t *) pUPortMalloc(sizeof(*pContext));
        if (pContext != NULL) {
            errorCode = uPortMutexCreate(&(pContext->fixDataStorageMutex));
            if (errorCode == 0) {
                pContext->desiredAccuracyMillimetres = U_CELL_LOC_DESIRED_ACCURACY_DEFAULT_MILLIMETRES;
                pContext->desiredFixTimeoutSeconds = U_CELL_LOC_DESIRED_FIX_TIMEOUT_DEFAULT_SECONDS;
                pContext->gnssEnable = U_CELL_LOC_GNSS_ENABLE_DEFAULT;
                pContext->fixStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN;
                uAtClientSetUrcHandler(pInstance->atHandle,
                                       "+UULOCIND:", UULOCIND_urc,
                                       pInstance);
                pContext->pFixDataStorage = NULL;
                pInstance->pLocContext = pContext;
            } else {
                // Free context on failure to create a fixDataStorageMutex
                uPortFree(pContext);
            }
        }
    }

    return errorCode;
}

// Check all the basics and lock the mutex, MUST be called
// at the start of every API function; use the helper macro
// U_CELL_LOC_ENTRY_FUNCTION to be sure of this, rather than
// calling this function directly.
static void entryFunction(uDeviceHandle_t cellHandle,
                          uCellPrivateInstance_t **ppInstance,
                          int32_t *pErrorCode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    if (gUCellPrivateMutex != NULL) {

        uPortMutexLock(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = ensureContext(pInstance);
        }
    }

    if (ppInstance != NULL) {
        *ppInstance = pInstance;
    }
    if (pErrorCode != NULL) {
        *pErrorCode = errorCode;
    }
}

// MUST be called at the end of every API function to unlock
// the cellular mutex; use the helper macro
// U_CELL_LOC_EXIT_FUNCTION to be sure of this, rather than
// calling this function directly.
static void exitFunction()
{
    if (gUCellPrivateMutex != NULL) {
        uPortMutexUnlock(gUCellPrivateMutex);
    }
}

// Set the pin of the module that is used for the
// given function.
static int32_t setModulePin(uAtClientHandle_t atHandle,
                            int32_t modulePin,
                            int32_t moduleFunction)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UGPIOC=");
    uAtClientWriteInt(atHandle, modulePin);
    uAtClientWriteInt(atHandle, moduleFunction);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Begin the process of getting a location fix.
static int32_t beginLocationFix(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;;
    uCellPrivateLocContext_t *pContext = pInstance->pLocContext;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t sensorType = U_CELL_LOC_MODULE_HAS_CELL_LOCATE << 1; // See below

    uPortLog("U_CELL_LOC: getting location.\n");

    // Request progress indications
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+ULOCIND=");
    uAtClientWriteInt(atHandle, 1);
    uAtClientCommandStopReadResponse(atHandle);
    // Don't care about the error here,
    // let's get on with it...
    uAtClientUnlock(atHandle);

    // Sometimes location requests are bounced by the
    // cellular module if it is busy talking to the
    // GNSS module so try this a few times
    for (size_t x = 0; (x < 6) && (errorCode < 0) ; x++) {
        // Send the location request
        uAtClientLock(atHandle);
        // Can take a little while
        uAtClientTimeoutSet(atHandle, 5000);
        // Note on the sensor type (second parameter)
        // Every bit is a sensor:
        // bit 0: GNSS
        // bit 1: Cell Locate
        // We will have 10 if U_CELL_LOC_MODULE_HAS_CELL_LOCATE
        // is at its default value of 1, just need to OR
        // in bit 0 to add GNSS
        if (pContext->gnssEnable) {
            sensorType |= 0x01;
        }
        uAtClientCommandStart(atHandle, "AT+ULOC=");
        uAtClientWriteInt(atHandle, 2); // Single shot position
        uAtClientWriteInt(atHandle, sensorType);
        uAtClientWriteInt(atHandle, 1); // Response includes speed and svs if available
        uAtClientWriteInt(atHandle, pContext->desiredFixTimeoutSeconds);
        uAtClientWriteInt(atHandle, pContext->desiredAccuracyMillimetres / 1000);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode < 0) {
            // Wait before re-trying
            uPortTaskBlock(10000);
        }
    }

    return errorCode;
}

// Get AT+UGPS.
//                   *** BE CAREFUL ***
//  The cellular module will only populate *pAidMode and
// *pGnssSystemBitMap if it is powered on.
static int32_t getUgps(const uCellPrivateInstance_t *pInstance,
                       bool *pOnNotOff, uint32_t *pAidMode,
                       uint32_t *pGnssSystemBitMap)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool gnssIsOn;
    int32_t x;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UGPS?");
    // Response is +UGPS: <mode>[,<aid_mode>[,<GNSS_systems>]]
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UGPS:");
    x = uAtClientReadInt(atHandle);
    if (pOnNotOff != NULL) {
        *pOnNotOff = (x == 1);
    }
    // Track whether GNSS is on or not as LENA-R8 still reports
    // the following two parameters (as zeroes) if GNSS is
    // off when it really means "I can't tell, GNSS is off"
    gnssIsOn = (x == 1);
    x = uAtClientReadInt(atHandle);
    if ((pAidMode != NULL) && (x >= 0) && gnssIsOn) {
        *pAidMode = (uint32_t) x;
    }
    x = uAtClientReadInt(atHandle);
    if ((pGnssSystemBitMap != NULL) && (x >= 0) && gnssIsOn) {
        *pGnssSystemBitMap = (uint32_t) x;
    }
    uAtClientResponseStop(atHandle);

    return uAtClientUnlock(atHandle);
}

// Set AT+UGPS.
static int32_t setUgps(const uCellPrivateInstance_t *pInstance,
                       bool onNotOff, uint32_t *pAidMode,
                       uint32_t *pGnssSystemBitMap)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t atTimeoutMs = U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS * 1000;

    if (onNotOff) {
        atTimeoutMs = U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS * 1000;
    }

    uAtClientLock(atHandle);
    uAtClientTimeoutSet(atHandle, atTimeoutMs);
    uAtClientCommandStart(atHandle, "AT+UGPS=");
    uAtClientWriteInt(atHandle, onNotOff);
    if (pAidMode != NULL) {
        uAtClientWriteInt(atHandle, *(int32_t *) pAidMode);
    }
    if (pGnssSystemBitMap != NULL) {
        uAtClientWriteInt(atHandle, *(int32_t *) pGnssSystemBitMap);
    }
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Send AT+UGSRV, reading out the existing parameters as necessary
// to make the command work.  To leave a parameter alone, set it to
// NULL or -1.  The aid_mode is tagged on the end in case this
// function needs to power the GNSS device off and on again,
// potentially with a new aid_mode.
static int32_t setUgsrv(const uCellPrivateInstance_t *pInstance,
                        const char *pAuthenticationTokenStr,
                        const char *pPrimaryServerStr,
                        const char *pSecondaryServerStr,
                        int32_t periodDays,
                        int32_t daysBetweenItems,
                        int32_t systemBitMap,
                        int32_t mode,
                        int32_t dataTypeBitMap,
                        int32_t aidMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uint32_t currentAidMode = pInstance->gnssAidMode;
    uint32_t gnssSystemTypesBitMap = pInstance->gnssSystemTypesBitMap;
    bool gnssOn = false;
    int32_t x;
    // +1 for terminator
    char authenticationTokenStr[U_CELL_LOC_AUTHENTICATION_TOKEN_STR_MAX_LEN_BYTES + 1];
    const char *pEmpty = "";

    if ((pAuthenticationTokenStr == NULL) ||
        (strlen(pAuthenticationTokenStr) <= U_CELL_LOC_AUTHENTICATION_TOKEN_STR_MAX_LEN_BYTES)) {
        // This AT command allows all parameters to be left empty, in which
        // case defaults will be used, EXCEPT the authentication string,
        // which is unfortunate, so we have to read it out every time in order
        // to change the other parameters

        // If there is a GNSS chip attached to the cellular module and
        // the GNSS chip is on, switch it off
        if ((getUgps(pInstance, &gnssOn, &currentAidMode, &gnssSystemTypesBitMap) == 0) && gnssOn) {
            // The GNSS chip is on: remember that and switch it off
            uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPS=");
            uAtClientWriteInt(atHandle, 0);
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientUnlock(atHandle);
            if (aidMode >= 0) {
                currentAidMode = (uint32_t) aidMode;
            }
        }
        // Get the current setting
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UGSRV?");
        uAtClientCommandStop(atHandle);
        // Response is +UGSRV: <mga_primary_server>,<mga_secondary_server>,<auth_token>,<days>,<period>,<resolution>,<GNSS_types>,<mode>,<datatype>
        uAtClientResponseStart(atHandle, "+UGSRV:");
        // Skip the first two parameters
        uAtClientSkipParameters(atHandle, 2);
        // Read the authentication token
        x = uAtClientReadString(atHandle, authenticationTokenStr, sizeof(authenticationTokenStr), false);
        // Don't care about the rest
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode == 0) {
            if (x <= 0) {
                // AT+UGSRV won't allow anything to be written if the authentication token
                // has not yet been set; to work around this just put "not set" in there
                strncpy(authenticationTokenStr, "not set", sizeof(authenticationTokenStr));
            }
            // Now we can write the command back
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGSRV=");
            if (pPrimaryServerStr != NULL) {
                uAtClientWriteString(atHandle, pPrimaryServerStr, true);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (pSecondaryServerStr != NULL) {
                uAtClientWriteString(atHandle, pSecondaryServerStr, true);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (pAuthenticationTokenStr != NULL) {
                uAtClientWriteString(atHandle, pAuthenticationTokenStr, true);
            } else {
                uAtClientWriteString(atHandle, authenticationTokenStr, true);
            }
            // The coding of the "days" field applied by cellular modules
            // is not actually the way the AssistNow Offline service
            // uses the field (any more, at least): the cellular module
            // fixes the days to certain values up to 14 but that is only
            // for M7 modules: for M8 and above the AssistNow Offline
            // service allows any value up to 35. However, the cellular
            // module checks that this field obeys the M7 rules, so
            // the best option is to leave it blank (since we don't support
            // M7 modules in any case) and use the coarser "period" field
            // instead, rounded-up.
            uAtClientWriteString(atHandle, pEmpty, false);
            if (periodDays >= 0) {
                x = periodDays / 7;
                if (x * 7 != periodDays) {
                    x++;
                }
                uAtClientWriteInt(atHandle, x);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (daysBetweenItems >= 0) {
                uAtClientWriteInt(atHandle, daysBetweenItems);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (systemBitMap >= 0) {
                uAtClientWriteInt(atHandle, systemBitMap);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (mode >= 0) {
                uAtClientWriteInt(atHandle, mode);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            if (dataTypeBitMap >= 0) {
                uAtClientWriteInt(atHandle, dataTypeBitMap);
            } else {
                // Skip the parameter
                uAtClientWriteString(atHandle, pEmpty, false);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        if (gnssOn) {
            // Switch the GNSS chip back on again
            uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
            setUgps(pInstance, gnssOn, &currentAidMode, &gnssSystemTypesBitMap);
        }
    }

    return errorCode;
}

// Convert a GNSS-API style AssistNow Online data type bit-map into
// a cellular one.
static int32_t gnssDataTypeBitMapToCellular(int32_t gnssDataTypeBitMap)
{
    int32_t cellDataTypeBitMap = 0;

    // The GNSS data type bit-map is:
    // U_GNSS_MGA_DATA_TYPE_EPHEMERIS = 1ULL << 0,
    // U_GNSS_MGA_DATA_TYPE_ALMANAC = 1ULL << 1,
    // U_GNSS_MGA_DATA_TYPE_AUX = 1ULL << 2,
    // U_GNSS_MGA_DATA_TYPE_POS = 1ULL << 3
    // ...while the cellular one is:
    // time = 0, position = 1, ephemeris = 2, almanac = 4, auxiliary = 8, filtered ephemeris = 16

    // Time is therefore always set (no bits set == time),
    // and we always set filtered ephemeris as that saves
    // data if the cellular module can use the currently
    // registered network as a location.
    if (gnssDataTypeBitMap & (1ULL << U_GNSS_MGA_DATA_TYPE_EPHEMERIS)) {
        cellDataTypeBitMap |= 2 | 16;
    }
    if (gnssDataTypeBitMap & (1ULL << U_GNSS_MGA_DATA_TYPE_ALMANAC)) {
        cellDataTypeBitMap |= 4;
    }
    if (gnssDataTypeBitMap & (1ULL << U_GNSS_MGA_DATA_TYPE_AUX)) {
        cellDataTypeBitMap |= 8;
    }
    if (gnssDataTypeBitMap & (1ULL << U_GNSS_MGA_DATA_TYPE_POS)) {
        cellDataTypeBitMap |= 1;
    }

    return cellDataTypeBitMap;
}

// Convert a cellular AssistNow Online data type bit-map into a GNSS one.
static int32_t cellDataTypeBitMapToGnss(int32_t cellDataTypeBitMap)
{
    int32_t gnssDataTypeBitMap = 0;

    if (cellDataTypeBitMap & 1) { // Position
        gnssDataTypeBitMap |= (1ULL << U_GNSS_MGA_DATA_TYPE_POS);
    }
    if (cellDataTypeBitMap & 2) { // Ephemeris
        gnssDataTypeBitMap |= (1ULL << U_GNSS_MGA_DATA_TYPE_EPHEMERIS);
    }
    if (cellDataTypeBitMap & 4) { // Almanac
        gnssDataTypeBitMap |= (1ULL << U_GNSS_MGA_DATA_TYPE_ALMANAC);
    }
    if (cellDataTypeBitMap & 8) { // Auxiliary
        gnssDataTypeBitMap |= (1ULL << U_GNSS_MGA_DATA_TYPE_AUX);
    }
    // Can ignore 16 since 2 will always be set if 16 is set anyway

    return gnssDataTypeBitMap;
}

// Set a single bit in the aid mode field of AT+UGPS (if it needs setting).
static int32_t setAidModeBit(uDeviceHandle_t cellHandle, bool onNotOff,
                             uCellLocAidMode_t aidMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uint32_t currentAidMode = 0;
    bool gnssOn = false;
    bool writeIt = false;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // Set the current aid_mode in case GNSS is off
        currentAidMode = pInstance->gnssAidMode;
        // Get the current aid mode
        errorCode = getUgps(pInstance, &gnssOn, &currentAidMode, NULL);
        if (errorCode == 0) {
            if (onNotOff) {
                if ((currentAidMode & (uint32_t) aidMode) == 0) {
                    currentAidMode |= (uint32_t) aidMode;
                    writeIt = true;
                }
            } else {
                if ((currentAidMode & (uint32_t) aidMode) > 0) {
                    currentAidMode &= ~(uint32_t) aidMode;
                    writeIt = true;
                }
            }
            if (writeIt) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (gnssOn) {
                    // The AT interface only supports setting aid_mode
                    // if the GNSS chip is on, otherwise we just have
                    // to remember it for when we do switch the GNSS
                    // chip on
                    errorCode = setUgps(pInstance, gnssOn, &currentAidMode, NULL);
                }
                if (errorCode == 0) {
                    pInstance->gnssAidMode &= ~((uint32_t) aidMode);
                    if ((currentAidMode & (uint32_t) aidMode) > 0) {
                        pInstance->gnssAidMode |= (uint32_t) aidMode;
                    }
                }
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get a single bit in the aid mode field of AT+UGPS.
static bool getAidModeBit(uDeviceHandle_t cellHandle, uCellLocAidMode_t aidMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uint32_t currentAidMode = 0;
    bool gnssOn = false;
    bool onNotOff = false;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // In case the GNSS device is off, set the outcome
        // based on what we would apply when switching it on
        onNotOff = ((pInstance->gnssAidMode & (uint32_t) aidMode) > 0);
        // Get the requested aid mode bit from the device if we can
        if ((getUgps(pInstance, &gnssOn, &currentAidMode, NULL) == 0) && gnssOn) {
            onNotOff = ((currentAidMode & (uint32_t) aidMode) > 0);
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return onNotOff;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uCellLocPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Free memory used by this API.
void uCellLocCleanUp(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        uCellPrivateLocRemoveContext(pInstance);
    }

    U_CELL_LOC_EXIT_FUNCTION();
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURATION
 * -------------------------------------------------------------- */

// Set the module pin that enables power to the GNSS chip.
int32_t uCellLocSetPinGnssPwr(uDeviceHandle_t cellHandle, int32_t pin)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    bool gnssOn = false;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // There is a bit of conundrum here: on some modules
        // (e.g. SARA-R5) an error will be returned if the
        // module pin that controls power to the GNSS chip
        // is configured when the GNSS chip is already powered,
        // hence we need to check that first.
        getUgps(pInstance, &gnssOn, NULL, NULL);
        if (!gnssOn) {
            // If the GNSS chip is not already on, do the thing
            // 3 is external GNSS supply enable mode
            errorCode = setModulePin(pInstance->atHandle, pin, 3);
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Set the module pin connected to Data Ready of the GNSS chip.
int32_t uCellLocSetPinGnssDataReady(uDeviceHandle_t cellHandle, int32_t pin)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // 4 is external GNSS data ready mode
        errorCode = setModulePin(pInstance->atHandle, pin, 4);
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Configure the Cell Locate server parameters.
int32_t uCellLocSetServer(uDeviceHandle_t cellHandle,
                          const char *pAuthenticationTokenStr,
                          const char *pPrimaryServerStr,
                          const char *pSecondaryServerStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pAuthenticationTokenStr != NULL) {
            errorCode = setUgsrv(pInstance, pAuthenticationTokenStr,
                                 pPrimaryServerStr, pSecondaryServerStr,
                                 -1, -1, -1, -1, -1, -1);
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Set the GNSS systems that a GNSS chip should use.
int32_t uCellLocSetSystem(uDeviceHandle_t cellHandle, uint32_t gnssSystemTypesBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uint32_t aidMode;
    uint32_t currentGnssSystemTypesBitMap = 0;
    bool gnssOn = false;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // In case GNSS is off, set the aid mode and bit-map to what we would use
        // if we were to switch it on
        aidMode = pInstance->gnssAidMode;
        currentGnssSystemTypesBitMap = pInstance->gnssSystemTypesBitMap;
        errorCode = getUgps(pInstance, &gnssOn, &aidMode, &currentGnssSystemTypesBitMap);
        if ((errorCode == 0) && gnssOn && (gnssSystemTypesBitMap != currentGnssSystemTypesBitMap)) {
            errorCode = setUgps(pInstance, gnssOn, &aidMode, &gnssSystemTypesBitMap);
        }
        if (errorCode == 0) {
            pInstance->gnssSystemTypesBitMap = gnssSystemTypesBitMap;
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get the GNSS systems that a GNSS chip is using.
int32_t uCellLocGetSystem(uDeviceHandle_t cellHandle, uint32_t *pGnssSystemTypesBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        if (pGnssSystemTypesBitMap != NULL) {
            // In case GNSS is off, set the bit-map to what it would be
            // if GNSS were powered-up
            *pGnssSystemTypesBitMap = pInstance->gnssSystemTypesBitMap;
        }
        errorCode = getUgps(pInstance, NULL, NULL, pGnssSystemTypesBitMap);
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Check whether a GNSS chip is present.
bool uCellLocIsGnssPresent(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    bool gnssPresent = false;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // Ask if the GNSS module is powered up
        errorCode = getUgps(pInstance, &gnssPresent, NULL, NULL);
        if ((errorCode < 0) || !gnssPresent) {
            // If not, try to switch GNSS on
            // In case something has gone wrong, set all the parameters
            // to their required values here, rather than the ones we
            // read (which should, in any case, be the same).
            gnssPresent = true;
            errorCode = setUgps(pInstance, gnssPresent,
                                &(pInstance->gnssAidMode),
                                &(pInstance->gnssSystemTypesBitMap));
            if (errorCode == 0) {
                // Power it off again
                gnssPresent = false;
                uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
                setUgps(pInstance, gnssPresent, NULL, NULL);
                gnssPresent = true;
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return gnssPresent;
}

// Check whether there is a GNSS chip on-board the cellular module.
bool uCellLocGnssInsideCell(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    bool isInside = false;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isInside = uCellPrivateGnssInsideCell(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isInside;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURATION OF CELL LOCATE
 * -------------------------------------------------------------- */

// Set the desired location accuracy.
void uCellLocSetDesiredAccuracy(uDeviceHandle_t cellHandle,
                                int32_t accuracyMillimetres)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        pInstance->pLocContext->desiredAccuracyMillimetres = accuracyMillimetres;
    }

    U_CELL_LOC_EXIT_FUNCTION();
}

// Get the desired location accuracy.
int32_t uCellLocGetDesiredAccuracy(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrAccuracy = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrAccuracy);

    if ((errorCodeOrAccuracy == 0) && (pInstance != NULL)) {
        errorCodeOrAccuracy = pInstance->pLocContext->desiredAccuracyMillimetres;
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCodeOrAccuracy;
}

// Set the desired location fix time-out.
void uCellLocSetDesiredFixTimeout(uDeviceHandle_t cellHandle,
                                  int32_t fixTimeoutSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        pInstance->pLocContext->desiredFixTimeoutSeconds = fixTimeoutSeconds;
    }

    U_CELL_LOC_EXIT_FUNCTION();
}

// Get the desired location fix time-out.
int32_t uCellLocGetDesiredFixTimeout(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrFixTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrFixTimeout);

    if ((errorCodeOrFixTimeout == 0) && (pInstance != NULL)) {
        errorCodeOrFixTimeout = pInstance->pLocContext->desiredFixTimeoutSeconds;
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCodeOrFixTimeout;
}

// Set whether a GNSS chip is used or not.
void uCellLocSetGnssEnable(uDeviceHandle_t cellHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        pInstance->pLocContext->gnssEnable = onNotOff;
    }

    U_CELL_LOC_EXIT_FUNCTION();
}

// Get whether GNSS is employed in the location fix or not.
bool uCellLocGetGnssEnable(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrGnssEnable = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrGnssEnable);

    if ((errorCodeOrGnssEnable == 0) && (pInstance != NULL)) {
        errorCodeOrGnssEnable = pInstance->pLocContext->gnssEnable;
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return (errorCodeOrGnssEnable != (int32_t) false);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURATION OF ASSIST NOW
 * -------------------------------------------------------------- */

// Set the data types used by AssistNow Online.
int32_t uCellLocSetAssistNowOnline(uDeviceHandle_t cellHandle,
                                   uint32_t dataTypeBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    uint32_t aidMode = 0;
    int32_t currentDataTypeBitMap;
    bool writeIt = false;
    bool gnssOn = false;
    int32_t dataTypeBitMapSigned = (int32_t) dataTypeBitMap;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL) && (dataTypeBitMapSigned >= 0)) {
        // In case GNSS is off, set the aid mode to what we would use when switching it on
        aidMode = pInstance->gnssAidMode;
        errorCode = getUgps(pInstance, &gnssOn, &aidMode, NULL);
        if (errorCode == 0) {
            dataTypeBitMapSigned = gnssDataTypeBitMapToCellular(dataTypeBitMapSigned);
            atHandle = pInstance->atHandle;
            // Get the current setting (in order to avoid power-cycling
            // the GNSS chip unnecessarily if the setting is already correct)
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGSRV?");
            uAtClientCommandStop(atHandle);
            // Response is +UGSRV: <mga_primary_server>,<mga_secondary_server>,<auth_token>,<days>,<period>,<resolution>,<GNSS_types>,<mode>,<datatype>
            uAtClientResponseStart(atHandle, "+UGSRV:");
            // Skip the first eight parameters
            uAtClientSkipParameters(atHandle, 8);
            currentDataTypeBitMap = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if ((errorCode == 0) && (currentDataTypeBitMap >= 0)) {
                if (dataTypeBitMapSigned == 0) {
                    if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE) > 0) {
                        // AssistNow Online is on but the caller wants it to be off
                        aidMode &= ~((uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE);
                        dataTypeBitMapSigned = -1;
                        writeIt = true;
                    }
                } else {
                    if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE) == 0) {
                        // The caller wants AssistNow Online to be on but it is currently off
                        aidMode |= (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE;
                        writeIt = true;
                    } else if (currentDataTypeBitMap != dataTypeBitMapSigned) {
                        // The caller is changing data types, so we need to write that
                        writeIt = true;
                    }
                }
                if (writeIt) {
                    if (dataTypeBitMapSigned >= 0) {
                        errorCode = setUgsrv(pInstance, NULL, NULL, NULL,
                                             -1, -1, -1, -1, (uint32_t) dataTypeBitMapSigned, aidMode);
                    } else {
                        // Just an aid_mode change to switch AssistNow Online off
                        if (gnssOn) {
                            errorCode = setUgps(pInstance, gnssOn, &aidMode, NULL);
                        }
                    }
                    if (errorCode == 0) {
                        pInstance->gnssAidMode &= ~((uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE);
                        if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE) > 0) {
                            pInstance->gnssAidMode |= (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE;
                        }
                    }
                }
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get which data types of the AssistNow Online service are being used.
int32_t uCellLocGetAssistNowOnline(uDeviceHandle_t cellHandle,
                                   uint32_t *pDataTypeBitMap)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    uint32_t aidMode = 0;
    int32_t dataTypeBitMap = 0;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // In case GNSS is off, set the aid mode to what we would use when switching it on
        aidMode = pInstance->gnssAidMode;
        errorCode = getUgps(pInstance, NULL, &aidMode, NULL);
        if (errorCode == 0) {
            if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_ONLINE) > 0) {
                atHandle = pInstance->atHandle;
                // Get the current setting
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGSRV?");
                uAtClientCommandStop(atHandle);
                // Response is +UGSRV: <mga_primary_server>,<mga_secondary_server>,<auth_token>,<days>,<period>,<resolution>,<GNSS_types>,<mode>,<datatype>
                uAtClientResponseStart(atHandle, "+UGSRV:");
                // Skip the first eight parameters
                uAtClientSkipParameters(atHandle, 8);
                dataTypeBitMap = uAtClientReadInt(atHandle);
                // Don't care about the rest
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
            if ((errorCode == 0) && (dataTypeBitMap >= 0)) {
                if (pDataTypeBitMap != NULL) {
                    *pDataTypeBitMap = (uint32_t) cellDataTypeBitMapToGnss(dataTypeBitMap);
                }
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Configure AssistNow Offline.
int32_t uCellLocSetAssistNowOffline(uDeviceHandle_t cellHandle,
                                    uint32_t gnssSystemTypesBitMap,
                                    int32_t periodDays,
                                    int32_t daysBetweenItems)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    uint32_t aidMode = 0;
    int32_t currentGnssSystemTypesBitMap = 0;
    int32_t currentPeriodDays = 0;
    int32_t currentDaysBetweenItems = 0;
    bool writeIt = false;
    bool gnssOn = false;
    int32_t gnssSystemTypesBitMapSigned = (int32_t) gnssSystemTypesBitMap;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL) && (periodDays >= 0) &&
        ((periodDays == 0) || (gnssSystemTypesBitMapSigned == 0) || (daysBetweenItems >= 1))) {
        // In case GNSS is off, set the aid mode to what we would use when switching it on
        aidMode = pInstance->gnssAidMode;
        errorCode = getUgps(pInstance, &gnssOn, &aidMode, NULL);
        if (errorCode == 0) {
            atHandle = pInstance->atHandle;
            // Get the current setting (in order to avoid power-cycling
            // the GNSS chip unnecessarily if the setting is already correct)
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGSRV?");
            uAtClientCommandStop(atHandle);
            // Response is +UGSRV: <mga_primary_server>,<mga_secondary_server>,<auth_token>,<days>,<period>,<resolution>,<GNSS_types>,<mode>,<datatype>
            uAtClientResponseStart(atHandle, "+UGSRV:");
            // Skip the first four parameters to get to the period (we ignore the
            // days parameter since the cellular module treats that as M7 only)
            uAtClientSkipParameters(atHandle, 4);
            currentPeriodDays = uAtClientReadInt(atHandle) * 7;
            currentDaysBetweenItems = uAtClientReadInt(atHandle);
            currentGnssSystemTypesBitMap = uAtClientReadInt(atHandle);
            // Don't care about the rest
            uAtClientResponseStop(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if ((errorCode == 0) && (currentPeriodDays >= 0) &&
                (currentDaysBetweenItems >= 0) && (currentGnssSystemTypesBitMap >= 0)) {
                if ((periodDays == 0) || (gnssSystemTypesBitMapSigned == 0)) {
                    if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE) > 0) {
                        // AssistNow Offline is on but the caller wants it to be off
                        aidMode &= ~((uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE);
                        gnssSystemTypesBitMapSigned = -1;
                        periodDays = -1;
                        daysBetweenItems = -1;
                        writeIt = true;
                    }
                } else {
                    if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE) == 0) {
                        // The caller wants AssistNow Offline to be on but it is currently off
                        aidMode |= (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE;
                        writeIt = true;
                    } else if ((currentPeriodDays != periodDays) ||
                               (currentDaysBetweenItems != daysBetweenItems) ||
                               (currentGnssSystemTypesBitMap != gnssSystemTypesBitMapSigned)) {
                        // The caller is changing parameters, so we need to write that
                        writeIt = true;
                    }
                }
                if (writeIt) {
                    if (gnssSystemTypesBitMapSigned >= 0) {
                        errorCode = setUgsrv(pInstance, NULL, NULL, NULL,
                                             periodDays, daysBetweenItems,
                                             (uint32_t) gnssSystemTypesBitMapSigned,
                                             -1, -1, aidMode);
                    } else {
                        if (gnssOn) {
                            // Just an aid_mode change to switch AssistNow Offline off
                            errorCode = setUgps(pInstance, gnssOn, &aidMode, NULL);
                        }
                    }
                    if (errorCode == 0) {
                        pInstance->gnssAidMode &= ~((uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE);
                        if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE) > 0) {
                            pInstance->gnssAidMode |= (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE;
                        }
                    }
                }
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get the AssistNow Offline configuration.
int32_t uCellLocGetAssistNowOffline(uDeviceHandle_t cellHandle,
                                    uint32_t *pGnssSystemTypesBitMap,
                                    int32_t *pPeriodDays,
                                    int32_t *pDaysBetweenItems)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    uint32_t aidMode = 0;
    int32_t gnssSystemTypesBitMap = 0;
    int32_t periodDays = 0;
    int32_t daysBetweenItems = 0;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // In case GNSS is off, set the aid mode to what we would use when switching it on
        aidMode = pInstance->gnssAidMode;
        errorCode = getUgps(pInstance, NULL, &aidMode, NULL);
        if (errorCode == 0) {
            if ((aidMode & (uint32_t) U_CELL_LOC_AID_MODE_ASSIST_NOW_OFFLINE) > 0) {
                atHandle = pInstance->atHandle;
                // Get the current setting (in order to avoid power-cycling
                // the GNSS chip unnecessarily if the setting is already correct)
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGSRV?");
                uAtClientCommandStop(atHandle);
                // Response is +UGSRV: <mga_primary_server>,<mga_secondary_server>,<auth_token>,<days>,<period>,<resolution>,<GNSS_types>,<mode>,<datatype>
                uAtClientResponseStart(atHandle, "+UGSRV:");
                // Skip the first four parameters to get to the period (we ignore the
                // days parameter since the cellular module treats that as M7 only)
                uAtClientSkipParameters(atHandle, 4);
                periodDays = uAtClientReadInt(atHandle) * 7;
                daysBetweenItems = uAtClientReadInt(atHandle);
                gnssSystemTypesBitMap = uAtClientReadInt(atHandle);
                // Don't care about the rest
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
            if ((errorCode == 0) && (periodDays >= 0) && (daysBetweenItems >= 0) &&
                (gnssSystemTypesBitMap >= 0)) {
                if (pPeriodDays != NULL) {
                    *pPeriodDays = periodDays;
                }
                if (pDaysBetweenItems != NULL) {
                    *pDaysBetweenItems = daysBetweenItems;
                }
                if (pGnssSystemTypesBitMap != NULL) {
                    *pGnssSystemTypesBitMap = (uint32_t) gnssSystemTypesBitMap;
                }
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Set whether AssistNow Autonomous is on or off.
int32_t uCellLocSetAssistNowAutonomous(uDeviceHandle_t cellHandle,
                                       bool onNotOff)
{
    return setAidModeBit(cellHandle, onNotOff, U_CELL_LOC_AID_MODE_ASSIST_NOW_AUTONOMOUS);
}

// Get whether AssistNow Autonomous is on or off.
bool uCellLocAssistNowAutonomousIsOn(uDeviceHandle_t cellHandle)
{
    return getAidModeBit(cellHandle, U_CELL_LOC_AID_MODE_ASSIST_NOW_AUTONOMOUS);
}

// Set whether the GNSS assistance database is saved or not.
int32_t uCellLocSetAssistNowDatabaseSave(uDeviceHandle_t cellHandle, bool onNotOff)
{
    return setAidModeBit(cellHandle, onNotOff, U_CELL_LOC_AID_MODE_AUTOMATIC_LOCAL);
}

// Get whether the GNSS assistance database is saved or not.
bool uCellLocAssistNowDatabaseSaveIsOn(uDeviceHandle_t cellHandle)
{
    return getAidModeBit(cellHandle, U_CELL_LOC_AID_MODE_AUTOMATIC_LOCAL);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: LOCATION ESTABLISHMENT
 * -------------------------------------------------------------- */

// Get the current location, blocking version.
int32_t uCellLocGet(uDeviceHandle_t cellHandle,
                    int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                    int32_t *pAltitudeMillimetres, int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond,
                    int32_t *pSvs,
                    int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uCellPrivateLocContext_t *pContext;
    uCellLocFixDataStorage_t *pFixDataStorage;
    volatile uCellLocFixDataStorageBlock_t fixDataStorageBlock = {0};
    uTimeoutStart_t timeoutStart;

    fixDataStorageBlock.errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
        if (uCellPrivateIsRegistered(pInstance)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pContext = pInstance->pLocContext;

            // Lock the fix storage mutex while we fiddle with it
            U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);

            if (pContext->pFixDataStorage == NULL) {
                // Allocate the data storage. This will be freed by the
                // local callback that is called from the URC handler
                // once it's got an answer and copied it into our
                // data block
                pFixDataStorage = (uCellLocFixDataStorage_t *) pUPortMalloc(sizeof(*pFixDataStorage));
                if (pFixDataStorage != NULL) {
                    // Attach our block to it
                    pFixDataStorage->type = U_CELL_LOC_FIX_DATA_STORAGE_TYPE_BLOCK;
                    pFixDataStorage->store.pBlock = &fixDataStorageBlock;
                    pContext->pFixDataStorage = (void *) pFixDataStorage;
                    // Register a URC handler and give it the instance,
                    // which has our data storage attached to it
                    uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
                    uAtClientSetUrcHandler(pInstance->atHandle,
                                           "+UULOC:", UULOC_urc,
                                           pInstance);
                    // Start the location fix
                    pContext->fixStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN;
                    errorCode = beginLocationFix(pInstance);
                    if (errorCode != 0) {
                        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
                    }
                }
            }

            U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);

            if (errorCode == 0) {
                uPortLog("U_CELL_LOC: waiting for the answer...\n");
                // Wait for the callback called by the URC to set
                // errorCode inside our block to success
                timeoutStart = uTimeoutStart();
                while ((fixDataStorageBlock.errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                       (((pKeepGoingCallback == NULL) &&
                         !uTimeoutExpiredSeconds(timeoutStart, U_CELL_LOC_TIMEOUT_SECONDS)) ||
                        ((pKeepGoingCallback != NULL) && pKeepGoingCallback(cellHandle)))) {
                    // Relax a little
                    uPortTaskBlock(1000);
                }
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
                errorCode = fixDataStorageBlock.errorCode;
                if (errorCode == 0) {
                    if (pLatitudeX1e7 != NULL) {
                        *pLatitudeX1e7 = fixDataStorageBlock.latitudeX1e7;
                    }
                    if (pLongitudeX1e7 != NULL) {
                        *pLongitudeX1e7 = fixDataStorageBlock.longitudeX1e7;
                    }
                    if (pAltitudeMillimetres != NULL) {
                        *pAltitudeMillimetres = fixDataStorageBlock.altitudeMillimetres;
                    }
                    if (pRadiusMillimetres != NULL) {
                        *pRadiusMillimetres = fixDataStorageBlock.radiusMillimetres;
                    }
                    if (pSpeedMillimetresPerSecond != NULL) {
                        *pSpeedMillimetresPerSecond = fixDataStorageBlock.speedMillimetresPerSecond;
                    }
                    if (pSvs != NULL) {
                        *pSvs = fixDataStorageBlock.svs;
                    }
                    if (pTimeUtc != NULL) {
                        *pTimeUtc = fixDataStorageBlock.timeUtc;
                    }
                }
            }

            // Free memory, locking the mutex while we do so
            U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);

            // In case the callback hasn't freed the fix
            // data storage memory
            if (pContext->pFixDataStorage != NULL) {
                uPortFree(pContext->pFixDataStorage);
                pContext->pFixDataStorage = NULL;
            }

            U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get the current location, non-blocking version.
int32_t uCellLocGetStart(uDeviceHandle_t cellHandle,
                         void (*pCallback) (uDeviceHandle_t cellHandle,
                                            int32_t errorCode,
                                            int32_t latitudeX1e7,
                                            int32_t longitudeX1e7,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int64_t timeUtc))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uCellPrivateLocContext_t *pContext;
    uCellLocFixDataStorage_t *pFixDataStorage;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
        if (uCellPrivateIsRegistered(pInstance)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pContext = pInstance->pLocContext;

            U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);

            if (pContext->pFixDataStorage == NULL) {
                // Allocate the data storage and copy pCallback in
                // The data storage will be freed by the local callback
                // that is called from the URC handler after it has done
                // pCallback
                pFixDataStorage = (uCellLocFixDataStorage_t *) pUPortMalloc(sizeof(*pFixDataStorage));
                if (pFixDataStorage != NULL) {
                    pFixDataStorage->type = U_CELL_LOC_FIX_DATA_STORAGE_TYPE_CALLBACK;
                    pFixDataStorage->store.pCallback = pCallback;
                    pContext->pFixDataStorage = (void *) pFixDataStorage;
                    // Start the location fix
                    pContext->fixStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN;
                    // Register a URC handler and give it the instance,
                    // which has our data storage attached to it
                    uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
                    uAtClientSetUrcHandler(pInstance->atHandle, "+UULOC:",
                                           UULOC_urc, pInstance);
                    errorCode = beginLocationFix(pInstance);
                    if (errorCode != 0) {
                        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
                    }
                }
            }

            U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Get the last status of a location fix attempt.
int32_t uCellLocGetStatus(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrStatus = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrStatus);

    if ((errorCodeOrStatus == 0) && (pInstance != NULL)) {
        errorCodeOrStatus = pInstance->pLocContext->fixStatus;
        switch (errorCodeOrStatus) {
            case 0:
                uPortLog("U_CELL_LOC: last status unknown.\n");
                break;
            case 1:
                uPortLog("U_CELL_LOC: last status cellular scan start.\n");
                break;
            case 2:
                uPortLog("U_CELL_LOC: last status cellular scan end.\n");
                break;
            case 3:
                uPortLog("U_CELL_LOC: last status requesting data from server.\n");
                break;
            case 4:
                uPortLog("U_CELL_LOC: last status receiving data from server.\n");
                break;
            case 5:
                uPortLog("U_CELL_LOC: last status sending feedback to server.\n");
                break;
            case 6:
                uPortLog("U_CELL_LOC: last status wrong URL.\n");
                break;
            case 7:
                uPortLog("U_CELL_LOC: last status HTTP error.\n");
                break;
            case 8:
                uPortLog("U_CELL_LOC: last status create socket error.\n");
                break;
            case 9:
                uPortLog("U_CELL_LOC: last status close socket error.\n");
                break;
            case 10:
                uPortLog("U_CELL_LOC: last status write to socket error.\n");
                break;
            case 11:
                uPortLog("U_CELL_LOC: last status read from socket error.\n");
                break;
            case 12:
                uPortLog("U_CELL_LOC: last status connection or DNS error.\n");
                break;
            case 13:
                uPortLog("U_CELL_LOC: last status bad authentication token.\n");
                break;
            case 14:
                uPortLog("U_CELL_LOC: last status generic error.\n");
                break;
            case 15:
                uPortLog("U_CELL_LOC: last status user terminated.\n");
                break;
            case 16:
                uPortLog("U_CELL_LOC: last status no data from server.\n");
                break;
            case 17:
                uPortLog("U_CELL_LOC: last status unknown comms error (%d).\n",
                         (errorCodeOrStatus - 7) + 1);
                break;
            default:
                uPortLog("U_CELL_LOC: last status value unknown (%d).\n",
                         errorCodeOrStatus);
                break;
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCodeOrStatus;
}

// Cancel a uCellLocGetStart().
void uCellLocGetStop(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uCellPrivateLocContext_t *pContext;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {

        pContext = pInstance->pLocContext;

        // Lock the fix data storage mutex while we fiddle
        U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);

        if (pContext->pFixDataStorage != NULL) {
            uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
            uPortFree(pContext->pFixDataStorage);
            pContext->pFixDataStorage = NULL;
        }

        U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);
    }

    U_CELL_LOC_EXIT_FUNCTION();
}

// End of file
