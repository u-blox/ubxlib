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

#ifndef _U_CELL_CFG_H_
#define _U_CELL_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that configure a cellular
 * module. These functions are thread-safe with the proviso that a
 * cellular instance should not be accessed before it has been added
 * or after it has been removed.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The North American bands for cat-M1, band mask bits 1 to 64.
 */
#define U_CELL_CFG_BAND_MASK_1_NORTH_AMERICA_CATM1_DEFAULT 0x000000400B0F189FLL

/** The North American bands for cat-M1, band mask bits 65 to 128.
 */
#define U_CELL_CFG_BAND_MASK_2_NORTH_AMERICA_CATM1_DEFAULT 0LL

/** Bands 8 and 20, suitable for NB1 in Europe, band mask bits 1 to 64.
 */
#define U_CELL_CFG_BAND_MASK_1_EUROPE_NB1_DEFAULT 0x0000000000080080LL

/** NB1 in Europe, band mask bits 65 to 128.
 */
#define U_CELL_CFG_BAND_MASK_2_EUROPE_NB1_DEFAULT 0LL

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Set the bands to be used by the cellular module.
 * The module must be powered on for this to work but must NOT be
 * connected to the cellular network (e.g. by calling
 * uCellNetDisconnect() to be sure) and the module must be
 * re-booted afterwards (with a call to uCellPwrReboot()) for it to
 * take effect.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param rat         the RAT to set the band mask for.
 * @param bandMask1   the first band mask where bit 0 is band 1
 *                    and bit 63 is band 64.
 * @param bandMask2   the second band mask where bit 0 is band 65
 *                    and bit 63 is band 128.
 * @return            zero on success or negative error code
 *                    on failure.
 */
int32_t uCellCfgSetBandMask(uDeviceHandle_t cellHandle,
                            uCellNetRat_t rat,
                            uint64_t bandMask1,
                            uint64_t bandMask2);

/** Get the bands being used by the cellular module.
 * The module must be powered on for this to work.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param rat             the radio access technology to obtain the
 *                        band mask for.
 * @param[out] pBandMask1 pointer to a place to store band mask 1,
 *                        where bit 0 is band 1 and bit 63 is band 64,
 *                        cannot be NULL.
 * @param[out] pBandMask2 pointer to a place to store band mask 2,
 *                        where bit 0 is band 65 and bit 63 is
 *                        band 128, cannot be NULL.
 * @return                zero on succese else negative error code.
 */
int32_t uCellCfgGetBandMask(uDeviceHandle_t cellHandle,
                            uCellNetRat_t rat,
                            uint64_t *pBandMask1,
                            uint64_t *pBandMask2);

/** Set the sole radio access technology to be used by the
 * cellular module.  The module is set to use this radio
 * access technology alone and no other; use
 * uCellCfgSetRankRat() if you want to use more than one
 * radio access technology.
 * The module must be powered on for this to work but must
 * NOT be connected to the cellular network (e.g. by calling
 * uCellNetDisconnect() to be sure) and the module must be
 * re-booted afterwards (with a call to uCellPwrReboot()) for
 * the change to take effect.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param rat         the radio access technology to use.
 * @return            zero on success or negative error code
 *                    on failure.
 */
int32_t uCellCfgSetRat(uDeviceHandle_t cellHandle,
                       uCellNetRat_t rat);

/** Set the radio access technology to be used at the
 * given rank.  By using different ranks the module can
 * be made to support more than one radio access technology
 * at the same time but bare in mind that this can extend
 * the network search and registration time.  Rank 0 is the
 * highest priority, then rank 1, etc.  The module must
 * be powered on for this to work but must NOT be connected
 * to the cellular network (e.g. by calling
 * uCellNetDisconnect() to be sure) and the module must be
 * re-booted afterwards (with a call to uCellPwrReboot())
 * for it to take effect.  The permitted RAT combinations
 * are module dependent.  Setting the same RAT at two
 * different ranks will result in that RAT only being set
 * in the higher (i.e. lower-numbered) of the two ranks.
 * A rank may be set to #U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
 * in order to eliminate the RAT at that rank but note that
 * having no RATs will generate an error and that the RATs
 * of lower rank will be shuffled-up so that there are no
 * gaps.  In other words, with RATs at ranks 0 = a and
 * 1 = b setting the RAT at rank 0 to
 * #U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED will result in 0 = b.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param rat         the radio access technology to use.
 * @param rank        the rank at which to use the radio access
 *                    technology, where 0 is the highest and the
 *                    lowest is module dependent.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellCfgSetRatRank(uDeviceHandle_t cellHandle,
                           uCellNetRat_t rat,
                           int32_t rank);

/** Get the radio access technology that is being used by
 * the cellular module at the given rank.  Rank 0 will always
 * return a known radio access technology at all times while
 * higher-numbered (i.e. lower priority) ranks may return
 * #U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED.  As soon as
 * #U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED is returned at a given
 * rank all greater ranks can be assumed to be
 * #U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param rank        the rank to check, where 0 is the highest
 *                    and the lowest is implementation dependent.
 * @return            the radio access technology being used at
 *                    that rank.
 */
uCellNetRat_t uCellCfgGetRat(uDeviceHandle_t cellHandle,
                             int32_t rank);

/** Get the rank at which the given radio access technology
 * is being used by the cellular module.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param rat         the radio access technology to find.
 * @return            the rank or negative error code if the
 *                    radio access technology is not found in
 *                    the ranked radio access technologies.
 */
int32_t uCellCfgGetRatRank(uDeviceHandle_t cellHandle,
                           uCellNetRat_t rat);

/** Set the MNO profile use by the cellular module.  The module must
 * be powered on for this to work but must NOT be connected to the
 * cellular network (e.g. by calling uCellNetDisconnect() to be sure)
 * and the module must be re-booted afterwards (with a call to
 * uCellPwrReboot()) for the new MNO profile setting to take effect.
 * Note: not all modules support MNO profile, an error will be
 * returned where this is the case.
 * IMPORTANT: the MNO profile is a kind of super-configuration,
 * which can change many things: the RAT, the bands, the APN,
 * etc.  So if you set an MNO profile you may wish to check what
 * it has done, in case you disagree with any of it.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param mnoProfile  the MNO profile.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellCfgSetMnoProfile(uDeviceHandle_t cellHandle,
                              int32_t mnoProfile);

/** Get the MNO profile used by the cellular module.
 * Note: not all modules support MNO profile, an error will be
 * returned where this is the case.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the MNO profile used by the module or negative
 *                    error code on failure.
 */
int32_t uCellCfgGetMnoProfile(uDeviceHandle_t cellHandle);

/** Configures the cellular module's serial interface. The configuration
 * affects how an available (physical or logical) serial interface is
 * used, e.g the meaning of data flowing over it. Possible usages are:
 *
 *  - modem interface (AT command),
 *  - trace interface (diagnostic log),
 *  - raw interface (e.g GPS/GNSS).
 *
 * The module must be re-booted afterwards (with a call to uCellPwrReboot())
 * for it to take effect.
 * Note: to find the serial interface variants available for your module, see the
 * serial interface configuration section (AT+USIO) of AT manual.
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param requestedVariant  the serial interface variant to set, e.g 0 - 255
 * @return                  zero on success or negative error code on failure.
 */
int32_t uCellCfgSetSerialInterface(uDeviceHandle_t cellHandle, int32_t requestedVariant);

/** Get the cellular module's active serial interface configuration.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            active variant of serial interface or negative code on failure.
 */
int32_t uCellCfgGetActiveSerialInterface(uDeviceHandle_t cellHandle);

/** Some cellular modules support an "AT+UDCONF" command which
 * allows details of specific features to be configured inside the
 * module, thereafter stored as a non-volatile setting and so
 * only used once.  This allows a UDCONF command to be sent to the
 * module with up to three integer parameters.  A reboot is usually
 * required afterwards to write the setting to non-volatile memory.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param param1      the first parameter, a positive integer.
 * @param param2      the second parameter, a positive integer.
 * @param param3      the optional third parameter, a positive integer
 *                    or negative to indicate that it is not present.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellCfgSetUdconf(uDeviceHandle_t cellHandle, int32_t param1,
                          int32_t param2,  int32_t param3);

/** Get the given "AT+UDCONF" setting.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param param1      the first parameter, a positive integer.
 * @param param2      the optional second parameter, a positive integer.
 *                    or negative to indicate that it is not present.
 * @return            the positive integer setting value or negative
 *                    error code on failure.
 */
int32_t uCellCfgGetUdconf(uDeviceHandle_t cellHandle, int32_t param1,
                          int32_t param2);

/** Return the cellular module's file system and or non-volatile
 * storage to factory defaults. The module must be re-booted afterwards
 * (with a call to uCellPwrReboot()) for it to take effect.
 * Note: not all restore types are supported by all modules, an error
 * will be returned in case of an invalid restore type.  Check the AT
 * command manual for your module for further information.
 *
 *  @param cellHandle    the handle of the cellular instance.
 *  @param fsRestoreType the file system factory restore type. Valid options
 *                       are 0, 1 and 2.
 *                       0: no factory restore.
 *                       1: Check datasheet, if this option is
 *                          supported by your module.
 *                       2: all files stored in FS deleted.
 * @param nvmRestoreType the file system factory restore type. Valid options
 *                       are 0, 1 and 2.
 *                       0: no factory restore.
 *                       1: NVM flash sectors erased.
 *                       2: Check datasheet, if this option is
 *                          supported by your module.
 * @return               zero on success or negative error code on
 *                       failure.
 */
int32_t uCellCfgFactoryReset(uDeviceHandle_t cellHandle, int32_t fsRestoreType,
                             int32_t nvmRestoreType);

/** Set a greeting message, which will be emitted by the module
 * at boot.  Note that when a module is set to auto-baud (the
 * default setting for SARA-R5 and SARA-U201) the greeting message
 * will only be emitted after the module has been sent the first
 * AT command (since the module does not know what baud rate to
 * use when sending the greeting message otherwise).  In order
 * for the greeting message to be sent as soon as the module has
 * booted the baud-rate used by the module must be fixed, e.g.
 * with a call to uCellCfgSetAutoBaudOff() in the case of SARA-R5
 * and SARA-U201.
 *
 * @param cellHandle   the handle of the cellular instance.
 * @param[in] pStr     the null-terminated greeting message; use NULL
 *                     to remove an existing greeting message.
 * @return             zero on success or negative error code on
 *                     failure.
 */
int32_t uCellCfgSetGreeting(uDeviceHandle_t cellHandle, const char *pStr);

/** Get the current greeting message.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a place to put the greeting message.  Room
 *                    should be allowed for a null terminator, which
 *                    will be added to terminate the string.  This
 *                    pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a null terminator.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (i.e. as
 *                    strlen() would return), on failure negative
 *                    error code.  If there is no greeting message
 *                    zero will be returned.
 */
int32_t uCellCfgGetGreeting(uDeviceHandle_t cellHandle, char *pStr, size_t size);

/** Switch off auto-bauding in the cellular module.  This will fix
 * the baud rate of the cellular module to the current baud rate,
 * storing the change in non-volatile memory in the cellular module.
 * It is useful where a module supports auto-bauding (e.g. SARA-U201
 * and SARA-R5) and yet you wish the module to emit a greeting message
 * the moment it boots, see uCellCfgSetGreeting() for details.  For
 * the setting to persist the module must be powered off with a call
 * to uCellPwrOff() (rather than a reboot or a hard power off).
 *
 * IMPORTANT: once this function has returned successfully, to change
 * the baud rate you must first call uCellCfgSetAutoBaudOn(), power the
 * cellular module off, remove the AT client/close this MCU's UART,
 * open the MCU's UART/add an AT client with the new baud rate and add
 * the cellular module once more.  You may then call this function again
 * to fix the new baud rate in the cellular module if you wish.
 *
 * @param cellHandle   the handle of the cellular instance.
 * @return             zero on success or negative error code on
 *                     failure.
 */
int32_t uCellCfgSetAutoBaudOff(uDeviceHandle_t cellHandle);

/** Switch auto-bauding on in the cellular module.  Auto-bauding
 * is not supported by all modules (e.g. the SARA-R4 series do not
 * support auto-bauding, they simply default to 115200); if
 * auto-bauding is supported by a module then it will be the default
 * and there is usually no need to call this function.  For the
 * auto-baud setting to persist the module must be powered off with
 * a call to uCellPwrOff() (rather than a reboot or a hard power off).
 *
 * @param cellHandle   the handle of the cellular instance.
 * @return             zero on success or negative error code on
 *                     failure.
 */
int32_t uCellCfgSetAutoBaudOn(uDeviceHandle_t cellHandle);

/** Determine whether auto-bauding is on in the cellular module.
 *
 * @param cellHandle   the handle of the cellular instance.
 * @return             true if auto-bauding is on, else false.
 */
bool uCellCfgAutoBaudIsOn(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_CFG_H_

// End of file
