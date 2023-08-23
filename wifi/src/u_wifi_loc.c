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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the location APIs for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strtok_r()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For U_CFG_OS_YIELD_MS and U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_location.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_wifi_module_type.h"
#include "u_wifi_loc.h"
#include "u_wifi_loc_private.h"
#include "u_wifi_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Context that can be hung off the Wi-Fi instance.
 */
typedef struct {
    int32_t errorCode;
    uLocation_t *pLocation;
    uWifiLocCallback_t *pCallback;
} uWifiLocContext_t;

/** Context that is used to push data to a callback for async operation.
 * This contains the location result body, NOT a pointer to it, for
 * thread-safety.
 */
typedef struct {
    uDeviceHandle_t wifiHandle;
    int32_t errorCode;
    uLocation_t location;
    uWifiLocCallback_t *pCallback;
} uWifiLocCallbackContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

static int32_t gULocationTypeToUConnectType[] = {
    -1, // U_LOCATION_TYPE_NONE
        -1, // U_LOCATION_TYPE_GNSS
        0, // U_LOCATION_TYPE_CLOUD_CELL_LOCATE
        1, // U_LOCATION_TYPE_CLOUD_GOOGLE
        2, // U_LOCATION_TYPE_CLOUD_SKYHOOK
        3, // U_LOCATION_TYPE_CLOUD_HERE
        -1 // U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE
    };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Multiply a number by 10 ^ thePower, returning an int32_t inside
// an int64_t.
static int64_t timesTenToThePower(int64_t number, int32_t thePower)
{
    for (int32_t x = 0; x < thePower; x++) {
        number *= 10;
    }

    if (number > INT_MAX) {
        number = INT_MAX;
    }

    return number;
}

// Parse a number from a string; the number is assumed to be
// well formed, so 1234 or 0.1234 or -0.1234, and must fit into
// an int32_t when multiplied by 10 ^ tenToThePower
static int32_t parseNumber(char *pStr, int32_t tenToThePower)
{
    int32_t number = 0;
    int64_t tmp = 0;
    int32_t sign = 1;
    bool fractional = false;

    if (pStr != NULL) {
        // Strip leading characters
        while ((*pStr != 0) &&
               !((*pStr >= '0') && (*pStr <= '9')) &&
               (*pStr != '-') && (*pStr != '+')) {
            pStr++;
        }
        // Handle a sign
        if (*pStr == '-') {
            sign = -1;
            pStr++;
        }
        while ((pStr != NULL) && (*pStr != 0)) {
            if ((*pStr >= '0') && (*pStr <= '9')) {
                // Add a decimal digit, but only if the
                // result fits into an int32_t
                tmp = number;
                tmp *= 10;
                tmp += *pStr - '0';
                if (fractional) {
                    tenToThePower--;
                }
                if (timesTenToThePower(tmp, tenToThePower) <= INT_MAX) {
                    number = (int32_t) tmp;
                    pStr++;
                    if (tenToThePower <= 0) {
                        break;
                    }
                } else {
                    // Too big, stop here
                    if (fractional) {
                        // Put the tenToThePower back
                        // because we never added it
                        tenToThePower++;
                    } else {
                        // Limit
                        number = INT_MAX;
                    }
                    break;
                }
            } else if (*pStr == '.') {
                // Skip the decimal point
                fractional = true;
                pStr++;
            } else {
                // Stop
                break;
            }
        }

        // Round up if there's room
        if ((pStr != NULL) && (*pStr >= '0') && (*pStr <= '9') &&
            (number < INT_MAX) && (*pStr - '0' >= 5)) {
            number++;
        }

        // Do the remainder of the scaling, if we haven't
        // limited already
        number = (int32_t) timesTenToThePower(number, tenToThePower);
    }

    return number * sign;
}

// Parse the location returned by a service from a string.
// This is destructive (i.e. it uses strtok()).
static int32_t parseBuffer(uLocation_t *pLocation, char *pStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
    size_t count = 0;
    char *pTmp;
    char *pSave;

    pLocation->latitudeX1e7 = 0;
    pLocation->longitudeX1e7 = 0;
    pLocation->altitudeMillimetres = INT_MIN;
    pLocation->radiusMillimetres = -1;
    pLocation->speedMillimetresPerSecond = INT_MIN;
    pLocation->svs = -1;
    pLocation->timeUtc = -1;

    // Fortunately, the strings returned by Google, Skyhook
    // and Here all use the same JSON keys (some differences
    // in bracketing but the keys are the same), so we can have
    // one parser to rule them all
    // Google:  {  "location": {    "lat": 52.2226116,    "lng": -0.0744764  },  "accuracy": 20}
    // Skyhook: {"location":{"lat":52.222533,"lng":-0.074445},"accuracy":34.0}
    // Here:    {"location":{"lat":52.22296709,"lng":-0.07337817,"accuracy":152}}
    pTmp = strtok_r(pStr, ":", &pSave);
    //  Make sure there is a "location"
    if ((pTmp != NULL) && (strstr(pTmp, "location") != NULL)) {
        pTmp = strtok_r(NULL, ":", &pSave);
    }
    // Make sure there is a "lat" next
    if ((pTmp != NULL) && (strstr(pTmp, "lat") != NULL)) {
        pTmp = strtok_r(NULL, ":", &pSave);
        count++;
    }
    // Now we should have the latitude value
    pLocation->latitudeX1e7 = parseNumber(pTmp, 7);
    // Make sure there is a "lng" next
    if (strstr(pTmp, "lng") != NULL) {
        pTmp = strtok_r(NULL, ":", &pSave);
        count++;
    }
    // Now we should have the longitude value
    pLocation->longitudeX1e7 = parseNumber(pTmp, 7);
    // There is no altitude value, all that is left is
    // the accuracy, which is provided in metres
    if (strstr(pTmp, "accuracy") != NULL) {
        pTmp = strtok_r(NULL, ":", &pSave);
        count++;
    }
    if (pTmp != NULL) {
        pLocation->radiusMillimetres = parseNumber(pTmp, 3);
    }

    if ((count >= 3) && (pLocation->radiusMillimetres >= 0)) {
        // If we got three things and we actually got a radius
        // then we're good
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Handler that is called via uAtClientCallback() from the
// UUDHTTP URC handler below, and ultimately calls the user callback.
static void UUDHTTP_urc_callback(uAtClientHandle_t atHandle, void *pParam)
{
    uWifiLocCallbackContext_t *pCallbackContext = (uWifiLocCallbackContext_t *) pParam;
    uLocation_t *pLocation = NULL;

    (void) atHandle;

    if (pCallbackContext != NULL) {
        if (pCallbackContext->errorCode == 0) {
            pLocation = &(pCallbackContext->location);
        }
        if (pCallbackContext->pCallback != NULL) {
            pCallbackContext->pCallback(pCallbackContext->wifiHandle,
                                        pCallbackContext->errorCode,
                                        pLocation);
        }

        // Free the callback context
        uPortFree(pCallbackContext);
    }
}

// Begin the process of getting a location fix.
// Note: this will allocate and return a pointer to a uWifiLocContext_t
// which the caller is responsible for freeing.
static volatile uWifiLocContext_t *pBeginLocationAlloc(uShortRangePrivateInstance_t *pInstance,
                                                       uLocationType_t type, const char *pApiKey,
                                                       int32_t accessPointsFilter,
                                                       int32_t rssiDbmFilter,
                                                       uLocation_t *pLocation)
{
    uAtClientHandle_t atHandle;
    volatile uWifiLocContext_t *pContext = NULL;

    if (pInstance != NULL) {
        pContext = pInstance->pLocContext;
        if (pContext == NULL) {
            pContext = (volatile uWifiLocContext_t *) pUPortMalloc(sizeof(uWifiLocContext_t));
        }
    }
    if (pContext != NULL) {
        pContext->errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
        pContext->pLocation = pLocation;
        pContext->pCallback = NULL;
        if (pLocation != NULL) {
            pLocation->latitudeX1e7 = INT_MIN;
            pLocation->longitudeX1e7 = INT_MIN;
            pLocation->altitudeMillimetres = INT_MIN;
            pLocation->radiusMillimetres = -1;
            pLocation->speedMillimetresPerSecond = INT_MIN;
            pLocation->svs = -1;
            pLocation->timeUtc = -1;
            pLocation->type = type;
        }
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        // This needs a little longer to respond with OK
        uAtClientTimeoutSet(atHandle, U_WIFI_LOC_REQUEST_TIMEOUT_SECONDS * 1000);
        uAtClientCommandStart(atHandle, "AT+ULOCWIFIPOS=");
        uAtClientWriteInt(atHandle, accessPointsFilter);
        uAtClientWriteInt(atHandle, rssiDbmFilter);
        uAtClientWriteInt(atHandle, gULocationTypeToUConnectType[type]);
        uAtClientWriteString(atHandle, pApiKey, true);
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) != 0) {
            // Free memory on error
            pInstance->pLocContext = NULL;
            uPortFree((void *) pContext);
            pContext = NULL;
        }
    }

    return pContext;
}

// Ensure that we have a location mutex for the instance.
static int32_t ensureMutex(uShortRangePrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (pInstance->locMutex == NULL) {
        errorCode = uPortMutexCreate(&(pInstance->locMutex));
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uWifiLocPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO WIFI LOC
 * -------------------------------------------------------------- */

// Process a URC containing a LOC response.
void uWifiLocPrivateUrc(uAtClientHandle_t atHandle, void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    uWifiLocContext_t *pContext = (uWifiLocContext_t *) pInstance->pLocContext;
    uWifiLocCallbackContext_t *pCallbackContext;
    int32_t bytesReadOrErrorCode;
    char *pBuffer = NULL;

    // Note that we use trylock here as we really don't want a URC
    // handler to be blocked by anything else
    if ((pInstance->locMutex != NULL) && (uPortMutexTryLock(pInstance->locMutex, 0) == 0)) {
        if (pContext != NULL) {
            // Note: the first parameter of the URC, the HTTP handle, must
            // already have been read from the stream, next is
            // the status code
            pContext->errorCode = uAtClientReadInt(atHandle);
            if (pContext->errorCode >= 0) {
                // Read the number of bytes of the contents field
                bytesReadOrErrorCode = uAtClientReadInt(atHandle);
                if (bytesReadOrErrorCode >= 0) {
                    // Next is the content type, which we don't need, so skip it
                    uAtClientSkipParameters(atHandle, 1);
                    // Have an HTTP status code, check if it is good
                    if (pContext->errorCode == 200) {
                        if (bytesReadOrErrorCode >= 0) {
                            // Allocate memory to read the contents into,
                            // +1 for terminator.
                            pBuffer = pUPortMalloc(bytesReadOrErrorCode + 1);
                            // Now read the contents into pBuffer (if pBuffer is
                            // NULL that's OK, this will just throw the contents away)
                            // The contents are NOT in quotes and may contain commas
                            // (the standard AT interface delimiter) and CR/LF etc,
                            // hence we do a binary read ignoring any stop tags
                            uAtClientIgnoreStopTag(atHandle);
                            bytesReadOrErrorCode = uAtClientReadBytes(atHandle, pBuffer,
                                                                      bytesReadOrErrorCode, true);
                            // Note: don't restore stop tag here, since we're not in
                            // a usual response, we're in a URC; as this is the last
                            // part of the URC the generic AT Client URC handling will
                            // do the right thing
                            if (bytesReadOrErrorCode >= 0) {
                                pContext->errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                                if (pBuffer != NULL) {
                                    // Add a terminator so that pBuffer can be treated as a string
                                    *(pBuffer + bytesReadOrErrorCode) = 0;
                                    pContext->errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                    if (pContext->pLocation != NULL) {
                                        // We should now have the location response as
                                        // a string, parse it
                                        pContext->errorCode = parseBuffer(pContext->pLocation,
                                                                          pBuffer);
                                    }
                                }
                            } else {
                                pContext->errorCode = bytesReadOrErrorCode;
                            }
                            // Free memory
                            uPortFree(pBuffer);
                        } else {
                            pContext->errorCode = bytesReadOrErrorCode;
                        }
                    } else {
                        if (bytesReadOrErrorCode > 0) {
                            // For any other error code that has contents, read it
                            // into NULL to get it out of the way
                            uAtClientIgnoreStopTag(atHandle);
                            uAtClientReadBytes(atHandle, pBuffer, bytesReadOrErrorCode, true);
                        }
                    }
                }
            }

            // If we got an HTTP error code (which may be success or may not)
            // and there is a callback then we should call it.
            if (pContext->pCallback != NULL) {
                // If there was a callback, this was an asynchronous
                // location request.  In order to get this out of
                // the URC queue, and also to keep this thread-safe
                // versus uWifiLocGetStop(), push it to the AT client
                // callback queue with a copy of all the things it needs
                // to know about, taken under the protection of the
                // location mutex.  This copy will be free'd by
                // UUDHTTP_urc_callback() just above.
                pCallbackContext = (uWifiLocCallbackContext_t *) pUPortMalloc(sizeof(uWifiLocCallbackContext_t));
                if (pCallbackContext != NULL) {
                    memset(pCallbackContext, 0, sizeof(*pCallbackContext));
                    pCallbackContext->wifiHandle = pInstance->devHandle;
                    pCallbackContext->errorCode = pContext->errorCode;
                    if (pContext->pLocation != NULL) {
                        pCallbackContext->location = *pContext->pLocation;
                    }
                    pCallbackContext->pCallback = pContext->pCallback;
                    if (uAtClientCallback(atHandle, UUDHTTP_urc_callback, pCallbackContext) != 0) {
                        uPortFree(pCallbackContext);
                    }
                }
                // For the callback case we will have mallocated the context,
                // so need to free it
                uPortFree(pContext->pLocation);
                pContext->pLocation = NULL;
                pContext->pCallback = NULL;
            }
        }

        // UNLOCK the location mutex
        uPortMutexUnlock(pInstance->locMutex);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uWifiLocGet(uDeviceHandle_t wifiHandle,
                    uLocationType_t type, const char *pApiKey,
                    int32_t accessPointsFilter,
                    int32_t rssiDbmFilter,
                    uLocation_t *pLocation,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    volatile uWifiLocContext_t *pContext;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        // Not checking accessPointsFilter and rssiDbmFilter
        // ranges here, as the module will do that, but we can at
        // least check that the RSSI filter is zero or less.
        if ((pInstance != NULL) && (pApiKey != NULL) && (rssiDbmFilter <= 0) &&
            (type >= 0) &&
            (type < sizeof(gULocationTypeToUConnectType) / sizeof(gULocationTypeToUConnectType[0]))) {
            // Make sure we have a mutex (needed to protect the context
            // for async operations but we use it here as a busy-check also)
            errorCode = ensureMutex(pInstance);
            if (errorCode == 0) {
                // Can only fiddle with memory if we have the location mutex
                errorCode = (int32_t) U_ERROR_COMMON_BUSY;
                if (uPortMutexTryLock(pInstance->locMutex, 0) == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pContext = pBeginLocationAlloc(pInstance, type, pApiKey,
                                                   accessPointsFilter, rssiDbmFilter,
                                                   pLocation);
                    if (pContext != NULL) {
                        pInstance->pLocContext = (volatile void *) pContext;
                        // UNLOCK the location mutex to let the URC handler run
                        uPortMutexUnlock(pInstance->locMutex);
                        // Hook in the URC handler and wait
                        atHandle = pInstance->atHandle;
                        errorCode = uAtClientSetUrcHandler(atHandle, "+UUDHTTP:",
                                                           uWifiPrivateUudhttpUrc,
                                                           pInstance);
                        if (errorCode == 0) {
                            startTimeMs = uPortGetTickTimeMs();
                            while ((pContext->errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                                   (((pKeepGoingCallback == NULL) &&
                                     ((uPortGetTickTimeMs() - startTimeMs) < U_WIFI_LOC_ANSWER_TIMEOUT_SECONDS * 1000)) ||
                                    ((pKeepGoingCallback != NULL) && pKeepGoingCallback(wifiHandle)))) {
                                uPortTaskBlock(250);
                            }
                            errorCode = pContext->errorCode;
                        }
                        pInstance->pLocContext = NULL;
                        // Free memory
                        uPortFree((void *) pContext);
                    } else {
                        // UNLOCK the location mutex on error
                        uPortMutexUnlock(pInstance->locMutex);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

// Get the current location, non-blocking version.
int32_t uWifiLocGetStart(uDeviceHandle_t wifiHandle,
                         uLocationType_t type, const char *pApiKey,
                         int32_t accessPointsFilter,
                         int32_t rssiDbmFilter,
                         uWifiLocCallback_t *pCallback)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    volatile uWifiLocContext_t *pContext;
    uLocation_t *pLocation;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        // Not checking accessPointsFilter and rssiDbmFilter
        // ranges here, as the module will do that, but we can at
        // least check that the RSSI filter is zero or less.
        if ((pInstance != NULL) && (pCallback != NULL) &&
            (pApiKey != NULL) && (rssiDbmFilter <= 0) &&
            (type >= 0) &&
            (type < sizeof(gULocationTypeToUConnectType) / sizeof(gULocationTypeToUConnectType[0]))) {
            // Make sure we have a mutex
            errorCode = ensureMutex(pInstance);
            if (errorCode == 0) {
                // Can only fiddle with memory if we have the location mutex
                errorCode = (int32_t) U_ERROR_COMMON_BUSY;
                if (uPortMutexTryLock(pInstance->locMutex, 0) == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pLocation = (uLocation_t *) pUPortMalloc(sizeof(uLocation_t));
                    if (pLocation != NULL) {
                        pContext = pBeginLocationAlloc(pInstance, type, pApiKey,
                                                       accessPointsFilter, rssiDbmFilter,
                                                       pLocation);
                        if (pContext != NULL) {
                            pContext->pCallback = pCallback;
                            pInstance->pLocContext = (volatile void *) pContext;
                            // Hook in the URC handler and return
                            atHandle = pInstance->atHandle;
                            errorCode = uAtClientSetUrcHandler(atHandle, "+UUDHTTP:",
                                                               uWifiPrivateUudhttpUrc,
                                                               pInstance);
                            if (errorCode != 0) {
                                // Free memory on error
                                pInstance->pLocContext = NULL;
                                uPortFree((void *) pContext);
                                uPortFree(pLocation);
                            }
                        } else {
                            // Free memory on error
                            uPortFree(pLocation);
                        }
                    }

                    // UNLOCK the location  mutex; it will be locked again by the URC handler
                    uPortMutexUnlock(pInstance->locMutex);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

// Cancel a uWifiLocGetStart().
void uWifiLocGetStop(uDeviceHandle_t wifiHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    volatile uWifiLocContext_t *pContext;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        // Can only fiddle with memory if we have the location mutex
        // If this fails then we must be in the mutex, in which
        // case uWifiLocFree() will do the work when called later
        if ((pInstance != NULL) && (pInstance->locMutex != NULL) &&
            (uPortMutexTryLock(pInstance->locMutex, 0) == 0)) {
            pContext = pInstance->pLocContext;
            if ((pContext != NULL) && (pContext->pCallback != NULL)) {
                // Must be in asynchronous mode: free the location
                // buffer we allocated
                uPortFree(pContext->pLocation);
            }
            // NULL and free the location context
            pInstance->pLocContext = NULL;
            uPortFree((void *) pContext);

            // UNLOCK the location mutex but don't free it
            // as we need it for thread-safety
            uPortMutexUnlock(pInstance->locMutex);
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
}

// Free the mutex that is protecting the data passed around by uWifiLoc.
void uWifiLocFree(uDeviceHandle_t wifiHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    volatile uWifiLocContext_t *pContext;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(wifiHandle);
        if ((pInstance != NULL) && (pInstance->locMutex != NULL)) {

            U_PORT_MUTEX_LOCK(pInstance->locMutex);

            pContext = pInstance->pLocContext;
            if ((pContext != NULL) && (pContext->pCallback != NULL)) {
                uPortFree(pContext->pLocation);
            }
            // NULL and free the location context
            pInstance->pLocContext = NULL;
            uPortFree((void *) pContext);

            U_PORT_MUTEX_UNLOCK(pInstance->locMutex);
            uPortMutexDelete(pInstance->locMutex);
            pInstance->locMutex = NULL;
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }
}

// End of file
