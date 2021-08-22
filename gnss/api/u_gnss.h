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

#ifndef _U_GNSS_H_
#define _U_GNSS_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the general GNSS APIs.
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

/** Error codes specific to GNSS.
 */
typedef enum {
    U_GNSS_ERROR_FORCE_32_BIT = 0x7FFFFFFF,    /**< Force this enum to be 32 bit as it can be
                                                    used as a size also. */
    U_GNSS_ERROR_TRANSPORT = U_ERROR_GNSS_MAX, /**< -1024 if U_ERROR_BASE is 0. */
    U_GNSS_ERROR_NACK = U_ERROR_GNSS_MAX - 1,  /**< -1025 if U_ERROR_BASE is 0. */
} uGnssErrorCode_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the GNSS driver.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uGnssInit();

/** Shut-down the GNSS driver.  All GNSS instances will be removed
 * internally with calls to uGnssRemove().
 */
void uGnssDeinit();

/** Add a GNSS instance.
 *
 * @param moduleType         the GNSS module type.
 * @param transportType      the type of transport that has been set up
 *                           to talk with the GNSS module.
 * @param transportHandle    the handle of the transport to use to
 *                           talk with the GNSS module.  This must
 *                           already have been created by the caller.
 * @param pinGnssEnablePower the pin of the MCU that enables power to the
 *                           GNSS module; use -1 if there is no such connection.
 * @param leavePowerAlone    set this to true if initialisation should
 *                           not modify the state of pinGnssEnablePower, else
 *                           pinGnssEnablePower will be set to its "off" state.
 * @return                   on success the handle of the GNSS instance,
 *                           else negative error code.
 */
int32_t uGnssAdd(uGnssModuleType_t moduleType,
                 uGnssTransportType_t transportType,
                 const uGnssTransportHandle_t transportHandle,
                 int32_t pinGnssEnablePower,
                 bool leavePowerAlone);

/** Remove a GNSS instance.  It is up to the caller to ensure
 * that the GNSS module for the given instance has been powered down etc.;
 * all this function does is remove the logical instance.
 *
 * @param gnssHandle  the handle of the GNSS instance to remove.
 */
void uGnssRemove(int32_t gnssHandle);

/** Get the type and handle of the transport used by the given
 * GNSS instance.
 *
 * @param gnssHandle        the handle of the GNSS instance.
 * @param pTransportType    a place to put the transport type,
 *                          may be NULL.
 * @param pTransportHandle  a place to put the transport handle,
 *                          may be NULL.
 * @return                  zero on success, else negative error code.
 */
int32_t uGnssGetTransportHandle(int32_t gnssHandle,
                                uGnssTransportType_t *pTransportType,
                                uGnssTransportHandle_t *pTransportHandle);

/** If the transport type is AT, i.e. the GNSS chip is being
 * accessed through an intermediate (e.g. cellular) module, then
 * that module may also be responsible for powering the GNSS
 * chip up and down. If that is the case then this function should
 * be called to set the pin of the module which enables power to
 * the GNSS chip.  For instance, for a cellular module, GPIO2 is
 * cellular module pin 23 and hence 23 would be used here.  If
 * no power-enable functionality is required then specify -1
 * (which is the default).
 * Note that this function is distinct and separate from the
 * uCellLocSetPinGnssPwr() over in the cellular API: if you are
 * using that API then you should call that function.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param pin         the cellular module pin to use.
 */
void uGnssSetAtPinPwr(int32_t gnssHandle, int32_t pin);

/** If the transport type is AT, i.e. the GNSS chip is being
 * accessed through an intermediate (e.g. cellular) module, then
 * the module may be connected to the GNSS chip's data ready pin.
 * If that is the case then this function should be called to set
 * the module pin that is used for GNSS data ready.  For instance,
 * for a cellular module, GPIO3 is cellular module pin 24 and
 * hence 24 would be used here.  If no Data Ready signalling is
 * required then specify -1 (which is the default).
 * Note that this function is distinct and separate from the
 * uCellLocSetPinGnssDataReady() over in the cellular API: if
 * you are using that API then you should call that function.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param pin         the cellular module pin to use.
 */
void uGnssSetAtPinDataReady(int32_t gnssHandle, int32_t pin);

/** Get the maximum time to wait for a response from the
 * GNSS chip for general API calls; does not apply to the
 * positioning calls, where U_GNSS_POS_TIMEOUT_SECONDS and
 * the pKeepGoingCallback are used.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the timeout in milliseconds.
 */
int32_t uGnssGetTimeout(int32_t gnssHandle);

/** Set the timeout for getting a response from the GNSS chip.
 * If this is not called the timeout will be
 * U_GNSS_DEFAULT_TIMEOUT_MS. Does not apply to the positioning
 * calls, where U_GNSS_POS_TIMEOUT_SECONDS and the
 * pKeepGoingCallback are used.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param timeoutMs   the timeout in milliseconds.
 */
void uGnssSetTimeout(int32_t gnssHandle, int32_t timeoutMs);

/** Get whether printing of ubx commands and responses
 * is on or off.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             true if printing ubx commands and
 *                     responses is on, else false.
 */
bool uGnssGetUbxMessagePrint(int32_t gnssHandle);

/** Switch printing of ubx commands and response on or off.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param onNotOff     set to true to cause ubx commands
 *                     and responses to be printed, false to
 *                     switch printing off.
 */
void uGnssSetUbxMessagePrint(int32_t gnssHandle, bool onNotOff);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_H_

// End of file
