/*
 * Copyright 2020 u-blox
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

#ifndef _U_GNSS_PWR_H_
#define _U_GNSS_PWR_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines the GNSS APIs to control the power
 * state of a GNSS chip.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POWER_UP_TIME_SECONDS
/** How long to wait for a GNSS chip to be available after it is
 * powered up.  If you change this and you also use the cell locate
 * API then you might want to change the value of
 * U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_POWER_UP_TIME_SECONDS 2
#endif

#ifndef U_GNSS_AT_POWER_UP_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=1.  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_UP_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_DOWN_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=0.  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_DOWN_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS
/** Some intermediate modules (e.g. SARA-R4) can be touchy
 * about a power-up or power-down request occurring close
 * on the heels of a previous GNSS-related command  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * U_CELL_LOC_GNSS_POWER_CHANGE_MILLISECONDS also.
 */
# define U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS 500
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Power a GNSS chip on.  If the transport type for the given GNSS
 * instance is U_GNSS_TRANSPORT_UBX_AT then you must have powered
 * the associated cellular module up (e.g. with a call to uNetworkAdd()
 * or uCellPwrOn()) before calling this function.  Also powering up
 * a GNSS module which is attached via a cellular module will "claim"
 * the GNSS module for this GNSS interface and so if you use the cellLoc
 * API at the same time you MUST either call uGnssPwrOff() first or
 * you must disable GNSS for Cell Locate (either by setting disableGnss
 * to true in the pLocationAssist structure when calling the location API
 * or by calling uCellLocSetGnssEnable() with false) otherwise cellLoc
 * location establishment will fail.
 *
 * @param gnssHandle  the handle of the GNSS instance to power on.
 */
int32_t uGnssPwrOn(int32_t gnssHandle);

/** Check that a GNSS chip is responsive.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
bool uGnssPwrIsAlive(int32_t gnssHandle);

/** Power a GNSS chip off
 *
 * @param gnssHandle  the handle of the GNSS instance to power off.
 */
int32_t uGnssPwrOff(int32_t gnssHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_PWR_H_

// End of file
