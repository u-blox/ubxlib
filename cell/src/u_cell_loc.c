/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_cell_loc.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURATION
 * -------------------------------------------------------------- */

// Get the desired location accuracy.
int32_t uCellLocDesiredAccuracyGet(int32_t cellHandle)
{
    (void) cellHandle;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Set the desired location accuracy.
void uCellLocDesiredAccuracySet(int32_t cellHandle,
                                int32_t accuracyMillimetres)
{
    (void) cellHandle;
    (void) accuracyMillimetres;
    // TODO
}

// Get the desired location fix time-out.
int32_t uCellLocDesiredFixTimeoutGet(int32_t cellHandle)
{
    (void) cellHandle;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Set the desired location fix time-out.
void uCellLocDesiredFixTimeoutSet(int32_t cellHandle,
                                  int32_t fixTimeoutSeconds)
{
    (void) cellHandle;
    (void) fixTimeoutSeconds;
    // TODO
}

// Get whether GNSS is employed in the location fix or not.
bool uCellLocGnssEnableGet(int32_t cellHandle)
{
    (void) cellHandle;

    // TODO

    return false;
}

// Set whether a GNSS chip is used or not.
void uCellLocGnssEnableSet(int32_t cellHandle, bool onNotOff)
{
    (void) cellHandle;
    (void) onNotOff;
    // TODO
}

// Set the module pin connected to GNSSEN.
int32_t uCellLocPinGnssPwrSet(int32_t cellHandle, int32_t pin)
{
    (void) cellHandle;
    (void) pin;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Set the module pin connected to Data Ready of the GNSS chip.
int32_t uCellLocPinGnssDataReadySet(int32_t cellHandle, int32_t pin)
{
    (void) cellHandle;
    (void) pin;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Configure the Cell Locate server parameters.
int32_t uCellLocServerCfg(int32_t cellHandle,
                          const char *pAuthenticationTokenStr,
                          const char *pPrimaryServerStr,
                          const char *pSecondaryServerStr)
{
    (void) cellHandle;
    (void) pAuthenticationTokenStr;
    (void) pPrimaryServerStr;
    (void) pSecondaryServerStr;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Check whether a GNSS chip is present.
bool uCellLocIsGnssPresent(int32_t cellHandle)
{
    (void) cellHandle;

    // TODO

    return false;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: LOCATION ESTABLISHMENT
 * -------------------------------------------------------------- */

// Add information on a Wifi access point.
int32_t uCellLocWifiAddAp(int32_t cellHandle,
                          const uCellLocWifiAp_t *pInfo)
{
    (void) cellHandle;
    (void) pInfo;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

// Delete all Wifi access point information.
void uCellLocWifiClearAllAps(int32_t cellHandle)
{
    (void) cellHandle;
    // TODO
}

// Get the current location.
int32_t uCellLocGet(int32_t cellHandle,
                    int32_t *pLatitudeX1e6, int32_t *pLongitudeX1e6,
                    int32_t *pAltitudeMillimetres, int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond, int32_t pSvs,
                    int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (int32_t))
{
    (void) cellHandle;
    (void) pLatitudeX1e6;
    (void) pLongitudeX1e6;
    (void) pAltitudeMillimetres;
    (void) pRadiusMillimetres;
    (void) pSpeedMillimetresPerSecond;
    (void) pSvs;
    (void) pTimeUtc;
    (void) pKeepGoingCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

/** Get the current location, non-blocking version.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pCallback   a callback that will be called when a fix has been
 *                    obtained.  The first parameter to the callback is
 *                    the cellular handle, the remaining parameters are
 *                    as described in uCellLocGet() except that they are
 *                    not pointers.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellLocGetStart(int32_t cellHandle,
                         void (*pCallback) (int32_t cellHandle,
                                            int32_t latitudeX1e6,
                                            int32_t longitudeX1e6,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int64_t timeUtc))
{
    (void) cellHandle;
    (void) pCallback;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

/** Get the last status of a location fix attempt.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            on success the location status, taken
 *                    from uLocationStatus_t (see common
 *                    location API), else negative error code.
 */
int32_t uCellLocStatusGet(int32_t cellHandle)
{
    (void) cellHandle;

    // TODO

    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

/** Cancel a uCellLocGetStart(); after calling this function the
 * callback passed to uCellLocGetStart() will not be called until
 * another uCellLocGetStart() is begun.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellLocGetStop(int32_t cellHandle)
{
    (void) cellHandle;
    // TODO
}

// End of file
