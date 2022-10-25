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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the Cell Locate API.
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
#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_time.h"

#include "u_at_client.h"

#include "u_location.h"

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

#ifndef U_CELL_LOC_GNSS_AIDING_TYPES
/** The aiding types to request when switching-on a GNSS
 * chip attached to a cellular module (all of them).
 */
#define U_CELL_LOC_GNSS_AIDING_TYPES 15
#endif

#ifndef U_CELL_LOC_GNSS_SYSTEM_TYPES
/** The system types to request when switching-on a GNSS
 * chip attached to a cellular module (all of them).
 */
#define U_CELL_LOC_GNSS_SYSTEM_TYPES 0x7f
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
 * allows asynchronous operation: we only all
 */
typedef enum {
    U_CELL_LOC_FIX_DATA_STORAGE_TYPE_BLOCK,
    U_CELL_LOC_FIX_DATA_STORAGE_TYPE_CALLBACK
} uCellLocFixDataStorageType_t;

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
} uCellLocUrc_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

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
                pUrcStorage->pContext = pContext;
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

// Set the module pin that enables power to the GNSS chip.
int32_t uCellLocSetPinGnssPwr(uDeviceHandle_t cellHandle, int32_t pin)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // There is a bit of conundrum here: on some modules
        // (e.g. SARA-R5) an error will be returned if the
        // module pin that controls power to the GNSS chip
        // is configured when the GNSS chip is already powered,
        // hence we need to check that first.
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UGPS?");
        // Response is +UGPS: <mode>[,<aid_mode>[,<GNSS_systems>]]
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UGPS:");
        x = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (x != 1) {
            // If the GNSS chip is not already on, do the thing
            // 3 is external GNSS supply enable mode
            errorCode = setModulePin(atHandle, pin, 3);
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
    uAtClientHandle_t atHandle;
    int32_t x;
    bool gnssOn = false;
    const char *pEmpty = "";

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        atHandle = pInstance->atHandle;
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pAuthenticationTokenStr != NULL) {
            // It is not permitted to send AT+UGSRV if there is a
            // GNSS chip  attached to the cellular module and
            // the GNSS chip is on, so find that out and switch
            // it off while we do this.
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPS?");
            // Response is +UGPS: <mode>[,<aid_mode>[,<GNSS_systems>]]
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UGPS:");
            x = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
            if (x == 1) {
                // The GNSS chip is on: remember that and switch
                // it off
                gnssOn = true;
                uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGPS=");
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStopReadResponse(atHandle);
                uAtClientUnlock(atHandle);
            }
            // Now set the server strings
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
            uAtClientWriteString(atHandle, pAuthenticationTokenStr, true);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (gnssOn) {
                // Switch the GNSS chip back on again
                uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGPS=");
                uAtClientWriteInt(atHandle, 1);
                // The aiding types allowed
                uAtClientWriteInt(atHandle, U_CELL_LOC_GNSS_AIDING_TYPES);
                // The GNSS system types enabled
                uAtClientWriteInt(atHandle, U_CELL_LOC_GNSS_SYSTEM_TYPES);
                uAtClientCommandStopReadResponse(atHandle);
                uAtClientUnlock(atHandle);
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return errorCode;
}

// Check whether a GNSS chip is present.
bool uCellLocIsGnssPresent(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    int32_t x = 0;

    U_CELL_LOC_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode);

    if ((errorCode == 0) && (pInstance != NULL)) {
        atHandle = pInstance->atHandle;
        // Ask if the GNSS module is powered up
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UGPS?");
        // Response is +UGPS: <mode>[,<aid_mode>[,<GNSS_systems>]]
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UGPS:");
        x = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if ((errorCode < 0) || (x != 1)) {
            x = 1;
            // If the first parameter is not 1, try to switch GNSS on
            uAtClientLock(atHandle);
            uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
            uAtClientTimeoutSet(atHandle,
                                U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS * 1000);
            uAtClientCommandStart(atHandle, "AT+UGPS=");
            uAtClientWriteInt(atHandle, x);
            // In case something goes wrong with the power-off
            // procedure which follows, set all the parameters
            // as wide as possible as that's what the GNSS API
            // would ask for when powering a GNSS chip on.
            // The aiding types allowed
            uAtClientWriteInt(atHandle, U_CELL_LOC_GNSS_AIDING_TYPES);
            // The GNSS system types enabled
            uAtClientWriteInt(atHandle, U_CELL_LOC_GNSS_SYSTEM_TYPES);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                // Power it off again
                uAtClientLock(atHandle);
                uPortTaskBlock(U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+UGPS=");
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStopReadResponse(atHandle);
                uAtClientUnlock(atHandle);
            } else {
                x = 0;
            }
        }
    }

    U_CELL_LOC_EXIT_FUNCTION();

    return (x != 0);
}

// Check whether there is a GNSS chip on-board the cellular module.
bool uCellLocGnssInsideCell(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    bool isInside = false;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;
    char buffer[64]; // Enough for the ATI response

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            // Simplest way to check is to send ATI and see if
            // it includes an "M8"
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "ATI");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, NULL);
            bytesRead = uAtClientReadBytes(atHandle, buffer,
                                           sizeof(buffer) - 1, false);
            uAtClientResponseStop(atHandle);
            if ((uAtClientUnlock(atHandle) == 0) && (bytesRead > 0)) {
                // Add a terminator
                buffer[bytesRead] = 0;
                if (strstr(buffer, "M8") != NULL) {
                    isInside = true;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isInside;
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
    volatile uCellLocFixDataStorageBlock_t fixDataStorageBlock;
    int64_t startTime;

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
                startTime = uPortGetTickTimeMs();
                while ((fixDataStorageBlock.errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                       (((pKeepGoingCallback == NULL) &&
                         (uPortGetTickTimeMs() - startTime) / 1000 < U_CELL_LOC_TIMEOUT_SECONDS) ||
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
