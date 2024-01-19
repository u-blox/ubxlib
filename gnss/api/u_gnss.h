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

#ifndef U_GNSS_RETRY_ON_NO_RESPONSE_DELAY_MS
/** How long to wait between retries of a message exchange
 * with a GNSS device if there is no response, in milliseconds.
 * 500 ms should be long enough for the device to wake up if
 * it was asleep.
 */
# define U_GNSS_RETRY_ON_NO_RESPONSE_DELAY_MS 500
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
 *                           to talk with the GNSS module; if you are using
 *                           #U_GNSS_TRANSPORT_VIRTUAL_SERIAL, see also
 *                           uGnssSetIntermediate().
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

/** If you have called uGnssAdd() with the transport type
 * #U_GNSS_TRANSPORT_VIRTUAL_SERIAL because the GNSS chip is inside or
 * connected via an intermediate (for example cellular) module then you
 * should call this function to let the GNSS instance know that there
 * is such an intermediate device.  This is required because some procedures,
 * e.g. powering the GNSS device on or off, need to be done differently
 * when there is an intermediate module.  You do NOT need to call this
 * function (it will return an error) if you are using
 * #U_GNSS_TRANSPORT_AT, as the code will already know that there is an
 * intermediate module in that case.  Likewise, if you are using
 * #U_GNSS_TRANSPORT_VIRTUAL_SERIAL for another reason and no
 * intermediate module is involved, you do not need to call this function.
 *
 * @param gnssHandle          the handle of the GNSS instance.
 * @param intermediateHandle  the handle of the intermediate (e.g. cellular)
 *                            instance.
 * @return                    zero on success else negative error code.
 */
int32_t uGnssSetIntermediate(uDeviceHandle_t gnssHandle,
                             uDeviceHandle_t intermediateHandle);

/** Get the handle of the intermediate device set using
 * uGnssSetIntermediate().
 *
 * @param gnssHandle           the handle of the GNSS instance.
 * @param pIntermediateHandle  a place to put the handle of the
 *                             intermediate device; cannot be NULL.
 * @return                     zero on success else negative error code.
 */
int32_t uGnssGetIntermediate(uDeviceHandle_t gnssHandle,
                             uDeviceHandle_t *pIntermediateHandle);

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
 * Note: where the transport is over AT (i.e. the case where AT+UGUBX
 * messages are being used to talk to a GNSS chip that is inside or
 * connected via a GNSS chip, e.g. if U_NETWORK_GNSS_CFG_CELL_USE_AT_ONLY
 * is defined, or CMUX is not supported, not the normal case) it is
 * possible for the AT handle to change underneath, so an AT handle
 * returned by this function will be locked and therefore unusable.
 * This will occur if a PPP session is opened to the cellular device.
 * Should a PPP session be opened this function should be called again
 * to obtain the correct AT handle.
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

/** When using an SPI interface the only way to tell if the
 * byte-stream received from the GNSS chip contains useful data or not
 * is to check for one or more 0xFF fill bytes; of course, since 0xFF can
 * legimitately occur in the stream it must be more than one fill
 * byte, but how many?  Use this function to get the current setting.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the number of 0xFF bytes which constitute fill.
 */
int32_t uGnssGetSpiFillThreshold(uDeviceHandle_t gnssHandle);

/** Set the number of 0xFF bytes which, if received from the GNSS
 * chip in a row when using an SPI transport, constitute fill rather
 * than useful data.  If this is not called #U_GNSS_DEFAULT_SPI_FILL_THRESHOLD
 * will apply.  It is not advisable to set the threshold to zero, meaning
 * no thresholding, since that will result in message reads always
 * continuing for the maximum time (since there will always be "valid"
 * [but 0xFF] data to read).  Setting the threshold to a small value
 * is equally inadvisable, since it may result in valid data (i.e.
 * consecutive genuine 0xFF bytes contained in a message body) being
 * discarded as fill.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param count       the number of 0xFF bytes which constitute fill,
 *                    can be no more than #U_GNSS_SPI_FILL_THRESHOLD_MAX.
 * @return            zero on success else negative error code.
 */
int32_t uGnssSetSpiFillThreshold(uDeviceHandle_t gnssHandle, int32_t count);

/** Get whether printing of UBX commands and responses is on or off.
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

/** If the GNSS device does not respond to a message because
 * it is inactive due to power-saving (see uGnssPwrSetMode())
 * then retry sending the message this many times, with a gap
 * of #U_GNSS_RETRY_ON_NO_RESPONSE_DELAY_MS.  If this is not
 * called no retries are attempted.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param retries     the number of retries.
 */
void uGnssSetRetries(uDeviceHandle_t gnssHandle, int32_t retries);

/** Get the number of retries when there is no response from
 * the GNSS device to a message.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the number of retries, else negative
 *                    error code.
 */
int32_t uGnssGetRetries(uDeviceHandle_t gnssHandle);

/** Get the internal port number that we are using inside the
 * GNSS device; this is dictated by the physical transport that
 * is in use (NOT necessarily the #uGnssTransportType_t as, for
 * instance, UART interfaces may be delivered as USB and Virtual
 * Serial ports may be absolutely anything).  It may be useful to
 * know this port number if you are using the uGnssCfgValXxx()
 * functions to set or get a value which is dependent upon it
 * (e.g. the one of the U_GNSS_CFG_VAL_KEY_ID_MSGOUT_XXX key IDs).
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the port number, else negative
 *                    error code.
 */
int32_t uGnssGetPortNumber(uDeviceHandle_t gnssHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_H_

// End of file
