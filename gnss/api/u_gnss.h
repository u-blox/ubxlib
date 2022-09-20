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

#ifndef _U_GNSS_H_
#define _U_GNSS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _GNSS _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the general GNSS APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_I2C_ADDRESS
/** The usual I2C address for a u-blox GNSS device.
 */
# define U_GNSS_I2C_ADDRESS 0x42
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to GNSS.
 */
typedef enum {
    U_GNSS_ERROR_FORCE_32_BIT = 0x7FFFFFFF,    /**< Force this enum to be 32 bit as it can be
                                                    used as a size also. */
    U_GNSS_ERROR_TRANSPORT = U_ERROR_GNSS_MAX, /**< -1024 if #U_ERROR_BASE is 0. */
    U_GNSS_ERROR_NACK = U_ERROR_GNSS_MAX - 1,  /**< -1025 if #U_ERROR_BASE is 0. */
    U_GNSS_ERROR_CRC = U_ERROR_GNSS_MAX - 2,   /**< -1026 if #U_ERROR_BASE is 0. */
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
 *                           GNSS module; use -1 if there is no such
 *                           connection.  If there is an inverter between
 *                           the pin of this MCU and whatever is switching
 *                           the power, so that 0 indicates "on" rather
 *                           than 1, then the value of pinGnssEnablePower
 *                           should be ORed with #U_GNSS_PIN_INVERTED (defined
 *                           in u_gnss_type.h).
 * @param leavePowerAlone    set this to true if initialisation should
 *                           not modify the state of pinGnssEnablePower, else
 *                           pinGnssEnablePower will be set to its "off" state.
 * @param[out] pGnssHandle   a pointer to the output handle. Will only be set on success.
 * @return                   zero on success or negative error code on failure.
 */
int32_t uGnssAdd(uGnssModuleType_t moduleType,
                 uGnssTransportType_t transportType,
                 const uGnssTransportHandle_t transportHandle,
                 int32_t pinGnssEnablePower,
                 bool leavePowerAlone,
                 uDeviceHandle_t *pGnssHandle);

/** Set the I2C address at which the GNSS device can be expected to
 * be found.  If not called the default #U_GNSS_I2C_ADDRESS is assumed.
 * Note that this does not _configure_ the I2C address inside the GNSS
 * device, that must have already been set by other means.  Obviously this
 * only makes a difference if the transport type is I2C.
 *
 * @param gnssHandle  the handle of the GNSS.
 * @param i2cAddress  the I2C address of the GNSS device.
 * @return            zero on success else negative error code.
 */
int32_t uGnssSetI2cAddress(uDeviceHandle_t gnssHandle, int32_t i2cAddress);

/** Get the I2C address which this code is using to talk to a GNSS
 * device.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the I2C address being used for the GNSS
 *                    device, else negative error code.
 */
int32_t uGnssGetI2cAddress(uDeviceHandle_t gnssHandle);

/** Remove a GNSS instance.  It is up to the caller to ensure
 * that the GNSS module for the given instance has been powered down etc.;
 * all this function does is remove the logical instance.
 *
 * @param gnssHandle  the handle of the GNSS instance to remove.
 */
void uGnssRemove(uDeviceHandle_t gnssHandle);

/** Get the type and handle of the transport used by the given
 * GNSS instance.
 *
 * @param gnssHandle            the handle of the GNSS instance.
 * @param[out] pTransportType   a place to put the transport type,
 *                              may be NULL.
 * @param[out] pTransportHandle a place to put the transport handle,
 *                              may be NULL.
 * @return                      zero on success, else negative error code.
 */
int32_t uGnssGetTransportHandle(uDeviceHandle_t gnssHandle,
                                uGnssTransportType_t *pTransportType,
                                uGnssTransportHandle_t *pTransportHandle);

/** If the transport type is AT, so the GNSS chip is being
 * accessed through an intermediate (for example cellular) module, then
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
void uGnssSetAtPinPwr(uDeviceHandle_t gnssHandle, int32_t pin);

/** If the transport type is AT, so the GNSS chip is being accessed
 * through an intermediate (for example cellular) module, then
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
void uGnssSetAtPinDataReady(uDeviceHandle_t gnssHandle, int32_t pin);

/** Get the maximum time to wait for a response from the
 * GNSS chip for general API calls; does not apply to the
 * positioning calls, where #U_GNSS_POS_TIMEOUT_SECONDS and
 * the pKeepGoingCallback are used.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the timeout in milliseconds.
 */
int32_t uGnssGetTimeout(uDeviceHandle_t gnssHandle);

/** Set the timeout for getting a response from the GNSS chip.
 * If this is not called the timeout will be
 * #U_GNSS_DEFAULT_TIMEOUT_MS. Does not apply to the positioning
 * calls, where #U_GNSS_POS_TIMEOUT_SECONDS and the
 * pKeepGoingCallback are used.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param timeoutMs   the timeout in milliseconds.
 */
void uGnssSetTimeout(uDeviceHandle_t gnssHandle, int32_t timeoutMs);

/** Get whether printing of UBX commands and responses
 * is on or off.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             true if printing UBX commands and
 *                     responses is on, else false.
 */
bool uGnssGetUbxMessagePrint(uDeviceHandle_t gnssHandle);

/** Switch printing of UBX commands and response on or off.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param onNotOff     set to true to cause UBX commands
 *                     and responses to be printed, false to
 *                     switch printing off.
 */
void uGnssSetUbxMessagePrint(uDeviceHandle_t gnssHandle, bool onNotOff);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_H_

// End of file
