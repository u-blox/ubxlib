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

#ifndef _U_CELL_PWR_PRIVATE_H_
#define _U_CELL_PWR_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few power functions that are needed
 * an in internal form inside the cellular API.  These few functions
 * are made available this way in order to avoid dragging the whole of
 * the pwr part of the cellular API into u_cell_private.c.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Power the cellular module on or wakeit from deep sleep.  If this
 * function returns success then the cellular module is ready to
 * receive configuration commands and register with the cellular network.
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param pInstance          a pointer to the instance.
 * @param pKeepGoingCallback power on usually takes between 5 and
 *                           15 seconds but it is possible for it
 *                           to take longer.  If this callback
 *                           function is non-NULL then it will
 *                           be called during the power-on
 *                           process and may be used to feed a
 *                           watchdog timer.  The callback
 *                           function should return true to
 *                           allow the power-on process to
 *                           be completed normally.  If the
 *                           callback function returns false
 *                           then the power-on process will
 *                           be abandoned.  Even when
 *                           this callback returns false
 *                           this function may still take some
 *                           10's of seconds to return in order
 *                           to ensure that the module is in a
 *                           cleanly powered (or not) state.
 *                           If this function is forced to return
 *                           it is advisable to call
 *                           uCellPwrIsAlive() to confirm
 *                           the final state of the module. The
 *                           single int32_t parameter is the
 *                           cell handle.
 * @param allowPrinting      if true then the function may print
 *                           out informative things, else it
 *                           will not.
 * @return                   zero on success or negative error
 *                           code on failure.
 */
int32_t uCellPwrPrivateOn(uCellPrivateInstance_t *pInstance,
                          bool (*pKeepGoingCallback) (uDeviceHandle_t),
                          bool allowPrinting);

/** Decode a string representing the binary value of a 3GPP power
 * saving active time (T3324) as a GPRS Timer 2 IE into seconds.
 *
 * @param pStr      the string, for example "00011001".
 * @param pSeconds  a place to put the value in seconds; may be NULL.
 * @return          zero on success else negative error code.
 */
int32_t uCellPwrPrivateActiveTimeStrToSeconds(const char *pStr,
                                              int32_t *pSeconds);

/** Decode a string representing the binary value of a 3GPP power
 * saving periodic wake-up time (T3412 or T3412 ext) as a GPRS Timer
 * 3 IE into seconds.
 *
 * @param pStr      the string, for example "00011001".
 * @param t3412Ext  true if the encoding is ext format, else false.
 * @param pSeconds  a place to put the value in seconds; may be NULL.
 * @return          zero on success else negative error code.
 */
int32_t uCellPwrPrivatePeriodicWakeupStrToSeconds(const char *pStr,
                                                  bool t3412Ext,
                                                  int32_t *pSeconds);

/** Get the 3GPP power saving settings.
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param pInstance              a pointer to the cellular instance.
 * @param assignedNotRequested   if true then get the values assigned
 *                               by the network, else get the values
 *                               that we requested from the network.
 * @param pOnNotOff              a place to put whether 3GPP
 *                               power saving is on or off;
 *                               may be NULL.
 * @param pActiveTimeSeconds     a place to put the period of
 *                               inactivity after which the module
 *                               should go to 3GPP power saving
 *                               mode; may be NULL.
 * @param pPeriodicWakeupSeconds a place to put the period at
 *                               which the module wishes to
 *                               wake-up to inform the cellular
 *                               network that it is still
 *                               connected; may be NULL.
 * @return                       zero on successful wake-up, else
 *                               negative error.
 */
int32_t uCellPwrPrivateGet3gppPowerSaving(uCellPrivateInstance_t *pInstance,
                                          bool assignedNotRequested,
                                          bool *pOnNotOff,
                                          int32_t *pActiveTimeSeconds,
                                          int32_t *pPeriodicWakeupSeconds);

/** Get the E-DRX settings for the given RAT.
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param pInstance              a pointer to the cellular instance.
 * @param assignedNotRequested   true to get the assigned parameters,
 *                               false to get the parameters that will
 *                               be requested.
 * @param rat                    the radio access technology
 *                               for example #U_CELL_NET_RAT_CATM1 or
 *                               #U_CELL_NET_RAT_NB1 or the
 *                               return value of
 *                               uCellNetGetActiveRat() if
 *                               registered with the network.
 * @param pOnNotOff              a place to put whether E-DRX
 *                               is on or off, may be NULL.
 * @param pEDrxSeconds           a place to put the E-DRX value
 *                               in seconds; may be NULL.
 * @param pPagingWindowSeconds   a place to put the paging window
 *                               vaue in seconds; may be NULL.
 * @return                       zero on success or negative error
 *                               code on failure.
 * @return                       zero on successful wake-up, else
 *                               negative error.
 */
int32_t uCellPwrPrivateGetEDrx(const uCellPrivateInstance_t *pInstance,
                               bool assignedNotRequested,
                               uCellNetRat_t rat,
                               bool *pOnNotOff,
                               int32_t *pEDrxSeconds,
                               int32_t *pPagingWindowSeconds);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_PWR_PRIVATE_H_

// End of file
