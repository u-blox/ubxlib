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

#ifndef _U_GNSS_PWR_H_
#define _U_GNSS_PWR_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

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
 * #U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_POWER_UP_TIME_SECONDS 2
#endif

#ifndef U_GNSS_RESET_TIME_SECONDS
/** How long to wait for a GNSS chip to be available after it has
 * been asked to reset.
 */
# define U_GNSS_RESET_TIME_SECONDS 5
#endif

#ifndef U_GNSS_AT_POWER_UP_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=1.  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_UP_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_DOWN_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=0.  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_DOWN_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS
/** Some intermediate modules (for example SARA-R4) can be touchy
 * about a power-up or power-down request occurring close
 * on the heels of a previous GNSS-related command  If you
 * change this and you also use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS also.
 */
# define U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS 500
#endif

#ifndef U_GNSS_AT_POWER_ON_RETRIES
/** When GNSS is connected via an intermediat module that
 * intermediate module can sometimes already be talking to
 * the GNSS module when we ask it to power the GNSS module
 * on, resulting in the error response "+CME ERROR: Invalid
 * operation with LOC running / GPS Busy".  In order to
 * avoid that we retry a few times in case of error.
 */
# define U_GNSS_AT_POWER_ON_RETRIES 2
#endif

#ifndef U_GNSS_AT_POWER_ON_RETRY_INTERVAL_SECONDS
/** How long to wait between power-on retries; only
 * relevant if #U_GNSS_AT_POWER_ON_RETRIES is greater than
 * zero.
 */
# define U_GNSS_AT_POWER_ON_RETRY_INTERVAL_SECONDS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Power a GNSS chip on.  If the transport type for the given GNSS
 * instance is #U_GNSS_TRANSPORT_AT then you must have powered
 * the associated [cellular] module up (e.g. with a call to uDeviceOpen()
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
int32_t uGnssPwrOn(uDeviceHandle_t gnssHandle);

/** Check that a GNSS chip is responsive.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
bool uGnssPwrIsAlive(uDeviceHandle_t gnssHandle);

/** Power a GNSS chip off
 *
 * @param gnssHandle  the handle of the GNSS instance to power off.
 */
int32_t uGnssPwrOff(uDeviceHandle_t gnssHandle);

/** Power a GNSS chip off and put it into back-up mode.  All of the
 * possible HW wake-up lines (UART RXD, SPI CS, EXTINT 0 and 1) will
 * wake the module up from this state but note that none of these
 * lines are I2C and hence, if you call this function when talking to
 * a GNSS module via I2C, the ONLY WAY back again is to have wired
 * the GNSS module's RESET_N line to this MCU and to toggle it low and
 * high again to wake the GNSS module up again. Or you can power-cycle
 * the GNSS chip of course.
 *
 * IMPORTANT: this function will return an error if the GNSS chip
 * is connected via an intermediate [e.g. cellular] module; this is
 * because the module will be communicating with the GNSS chip over I2C.
 *
 * @param gnssHandle  the handle of the GNSS instance to power on.
 */
int32_t uGnssPwrOffBackup(uDeviceHandle_t gnssHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_PWR_H_

// End of file
