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

#ifndef _U_CELL_INFO_H_
#define _U_CELL_INFO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that obtain general
 * information from a cellular module (IMEI, etc.).
 * These functions are thread-safe with the proviso that a cellular
 * instance should not be accessed before it has been added or after
 * it has been removed.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The number of digits in an IMSI.
 */
#define U_CELL_INFO_IMSI_SIZE 15

/** The number of digits in an IMEI.
 */
#define U_CELL_INFO_IMEI_SIZE 15

/** The number of digits required to store an ICCID.  Note
 * that 19 digit ICCIDs also exist.  This size includes room
 * for a null terminator.
 */
#define U_CELL_INFO_ICCID_BUFFER_SIZE 21

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Refresh the RF status values.  Call this to refresh
 * RSSI, RSRP, RSRQ, Cell ID, EARFCN, etc.  This way all of the
 * values read are synchronised to a given point in time.  The
 * radio parameters stored by this function are cleared on
 * disconnect and reboot.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success, negative error code on
 *                    failure.
 */
int32_t uCellInfoRefreshRadioParameters(uDeviceHandle_t cellHandle);

/** Get the RSSI that pertained after the last call to
 * uCellInfoRefreshRadioParameters().  Note that RSSI may not
 * be available unless the module has successfully registered
 * with the cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the RSSI in dBm, or zero if no RSSI
 *                    measurement is currently available.
 *                    Note that RSSI values are NEGATIVE.
 */
int32_t uCellInfoGetRssiDbm(uDeviceHandle_t cellHandle);

/** Get the RSRP that pertained after the last call to
 * uCellInfoRefreshRadioParameters().  Note that RSRP may not
 * be available unless the module has successfully registered
 * with the cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the RSRP in dBm, or zero if no RSRP
 *                    measurement is currently available.
 *                    Note that RSRP values are NEGATIVE.
 */
int32_t uCellInfoGetRsrpDbm(uDeviceHandle_t cellHandle);

/** Get the RSRQ that pertained after the last call to
 * uCellInfoRefreshRadioParameters().  Note that RSRQ may not be
 * available unless the module has successfully registered with the
 * cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the RSRQ in dB, or 0x7FFFFFFF if no RSRQ
 *                    measurement is currently available.
 *                    Note that RSRQ values are usually
 *                    negative but small positive values are
 *                    also possible.
 */
int32_t uCellInfoGetRsrqDb(uDeviceHandle_t cellHandle);

/** Get the RxQual that pertained after the last call to
 * uCellInfoRefreshRadioParameters().  This is a number
 * from 0 to 7.  The number means different things for
 * different RATs, see the u-blox AT command manual or
 * 3GPP specification 27.007 for detailed translation
 * tables.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the RxQual, 0 to 7, or negative if no
 *                    RxQual is available.
 */
int32_t uCellInfoGetRxQual(uDeviceHandle_t cellHandle);

/** Get the SNR that pertained after the last call to
 * uCellInfoRefreshRadioParameters(). Note that the format of
 * this call is different to that of uCellInfoGetRssiDbm(),
 * uCellInfoGetRsrpDbm() and uCellInfoGetRsrqDb() in that a
 * pointer must be passed in to obtain the result.  This is
 * because negative, positive and zero values for SNR are valid.
 * SNR is RSRP / (RSSI - RSRP) and so if RSSI and RSRP are the
 * same a maximal integer value will be returned.
 * SNR may not be available unless the module has successfully
 * registered with the cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pSnrDb a place to put the SNR measurement.  Must
 *                    not be NULL.
 * @return            zero on success, negative error code on
 *                    failure.
 */
int32_t uCellInfoGetSnrDb(uDeviceHandle_t cellHandle,
                          int32_t *pSnrDb);

/** Get the cell ID that pertained after the last call to
 * uCellInfoRefreshRadioParameters().
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the cell ID, or negative error code on
 *                    failure.
 */
int32_t uCellInfoGetCellId(uDeviceHandle_t cellHandle);

/** Get the EARFCN that pertained after the last call to
 * uCellInfoRefreshRadioParameters().
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the EARFCN, or -1 if the module is not
 *                    registered with the cellular network.
 */
int32_t uCellInfoGetEarfcn(uDeviceHandle_t cellHandle);

/** Get the IMEI of the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pImei  a pointer to #U_CELL_INFO_IMEI_SIZE bytes
 *                    of storage into which the IMEI will be
 *                    copied; no terminator is added as the
 *                    IMEI is of fixed length. This pointer
 *                    cannot be NULL.
 * @return            zero on success, negative error code on
 *                    failure.
 */
int32_t uCellInfoGetImei(uDeviceHandle_t cellHandle,
                         char *pImei);

/** Get the IMSI of the SIM in the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pImsi  a pointer to #U_CELL_INFO_IMSI_SIZE bytes
 *                    of storage into which the IMSI will be
 *                    copied; no terminator is added as the IMSI
 *                    is of fixed length. This pointer cannot be
 *                    NULL.
 * @return            zero on success, negative error code on
 *                    failure.
 */
int32_t uCellInfoGetImsi(uDeviceHandle_t cellHandle,
                         char *pImsi);

/** Get the ICCID string of the SIM in the cellular module.  Note
 * that, while the ICCID is all numeric digits, like the IMEI and
 * the IMSI, the length of the ICCID can vary between 19 and 20
 * digits; it is treated as a string here because of that variable
 * length.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the ICCID string will be copied.  Room
 *                    should be allowed for a null terminator, which
 *                    will be added to terminate the string.  This
 *                    pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a terminator.  Allocating
 *                    #U_CELL_INFO_ICCID_BUFFER_SIZE bytes of
 *                    storage is safe.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellInfoGetIccidStr(uDeviceHandle_t cellHandle,
                             char *pStr, size_t size);

/** Get the manufacturer identification string from the cellular
 * module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the manufacturer string will be copied.  Room
 *                    should be allowed for a null terminator, which
 *                    will be added to terminate the string.  This
 *                    pointer cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater
 *                    than zero.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellInfoGetManufacturerStr(uDeviceHandle_t cellHandle,
                                    char *pStr, size_t size);

/** Get the model identification string from the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the model string will be copied.  Room should
 *                    be allowed for a null terminator, which will be
 *                    added to terminate the string.  This pointer
 *                    cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater
 *                    than zero.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellInfoGetModelStr(uDeviceHandle_t cellHandle,
                             char *pStr, size_t size);

/** Get the firmware version string from the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the firmware version string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater
 *                    than zero.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellInfoGetFirmwareVersionStr(uDeviceHandle_t cellHandle,
                                       char *pStr, size_t size);

/** Get the UTC time according to cellular.  This feature requires
 * a connection to have been activated and support for this feature
 * is optional in the cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            on success the Unix UTC time, else negative
 *                    error code.
 */
int64_t uCellInfoGetTimeUtc(uDeviceHandle_t cellHandle);

/** Get the UTC time string according to cellular. This feature requires
 * a connection to have been activated and support for this feature
 * is optional in the cellular network.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the UTC time string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater or equal
 *                    to 32 bytes.
 * @return            on success, the number of characters copied into
 *                    pStr NOT include the terminator (as strlen() would
 *                    return), on failure negative error code.
 */
int32_t uCellInfoGetTimeUtcStr(uDeviceHandle_t cellHandle,
                               char *pStr, size_t size);

/** Determine if RTS flow control, the signal from the
 * cellular module to this software that the module is
 * ready to receive data, is enabled.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           true if RTS flow control is enabled,
 *                   else false.
 */
bool uCellInfoIsRtsFlowControlEnabled(uDeviceHandle_t cellHandle);

/** Determine if CTS flow control, the signal from this
 * software to the cellular module that this sofware is
 * ready to accept data, is enabled.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           true if CTS flow control is enabled,
 *                   else false.
 */
bool uCellInfoIsCtsFlowControlEnabled(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_INFO_H_

// End of file
