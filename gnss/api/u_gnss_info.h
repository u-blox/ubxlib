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

#ifndef _U_GNSS_INFO_H_
#define _U_GNSS_INFO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines functions that read general
 * information from a GNSS chip; for position information please
 * see the u_gnss_pos.h API instead.
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

typedef struct {
    char ver[31];   //!< Sofware Version
    char hw[11];    //!< Hardware / Chip Version
    char rom[22];   //!< Underlying ROM Version
    char fw[25];    //!< Firmware Version
    char prot[23];  //!< Protocol Version
    char mod[27];   //<! Module  Variant
} uGnssVersionType_t; //!< return structs with different information

/** Array of communications as seen by the GNSS chip.
 */
typedef struct {
    size_t txPendingBytes;        /**< number of bytes pending in the GNSS chip's
                                       transmitter buffer. */
    size_t txBytes;               /**< number of bytes ever sent by the GNSS chip. */
    size_t txPercentageUsage;     /**< maximum percentage usage of the GNSS chip's
                                       transmit buffer during the last sysmon period. */
    size_t txPeakPercentageUsage; /**< maximum percentage usage of the GNSS chip's
                                       transmit buffer, the high water-mark. */
    size_t rxPendingBytes;        /**< number of bytes pending in the GNSS chip's
                                       receive buffer. */
    size_t rxBytes;               /**< number of bytes ever received by the GNSS chip. */
    size_t rxPercentageUsage;     /**< maximum percentage usage of the GNSS chip's
                                       receive buffer during the last sysmon period. */
    size_t rxPeakPercentageUsage; /**< maximum percentage usage of the GNSS chip's
                                       receive buffer, the high water-mark. */
    size_t rxOverrunErrors;       /**< the number of 100 ms timeslots with receive
                                       overrun errors. */
    int32_t rxNumMessages[U_GNSS_PROTOCOL_MAX_NUM]; /**< the number of messages
                                                         received by the GNSS
                                                         chip for each protocol
                                                         type, indexed by
                                                         uGnssProtocol_t; any
                                                         that are not reported
                                                         will contain -1. */
    size_t rxSkippedBytes;        /**< the number of receive bytes skipped. */
} uGnssCommunicationStats_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the version string from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the firmware version string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a null terminator. Must be
 *                    greater than zero.
 * @return            on success, the number of characters copied
 *                    into pStr NOT including the null terminator
 *                    (as strlen() would return), else negative
 *                    error code.  Note that the string itself may
 *                    contain multiple lines separated by [more than
 *                    one] null terminator, depending on what the
 *                    GNSS device chooses to return.
 */
int32_t uGnssInfoGetFirmwareVersionStr(uDeviceHandle_t gnssHandle,
                                       char *pStr, size_t size);

/** Get the various information from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param[out] pVer   a pointer to structure where information is copied.
 *                    This pointer cannot be NULL.
 * @return            on sucesss 0, else negative error code.
 */
int32_t uGnssInfoGetVersions(uDeviceHandle_t gnssHandle,
                             uGnssVersionType_t *pVer);

/** Get the chip ID from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the chip ID string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a null terminator. Must be
 *                    greater than zero.
 * @return            on success, the number of characters copied
 *                    into pStr NOT including the null terminator
 *                    (as strlen() would return), else negative
 *                    error code.  Note that the string itself may
 *                    contain nulls, depending on what the GNSS
 *                    device chooses to return
 */
int32_t uGnssInfoGetIdStr(uDeviceHandle_t gnssHandle,
                          char *pStr, size_t size);

/** Get the UTC time according to GNSS.
 *
 * Note: in order to obtain UTC time the GNSS chip has to download
 * the leap seconds information from the satellite, which is only
 * transmitted relatively infrequently (every 12.5 minutes), so you
 * must have requested position from GNSS at least once, with the
 * GNSS receiver left on for sufficiently long to pick up the
 * leap second information, for this to work.  That or, if you
 * are connecting via an intermediate [cellular] module, you may
 * call uCellLocSetServer() to enter your CellLocate token
 * (obtained from the Location Services section of your Thingstream
 * portal, https://portal.thingstream.io/app/location-services)
 * and connect to cellular for the relevant information to be
 * downloaded from a u-blox server instead.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the Unix UTC time, else negative
 *                    error code.
 */
int64_t uGnssInfoGetTimeUtc(uDeviceHandle_t gnssHandle);

/** Get the communication stats as seen by the GNSS chip; supported
 * only on M9 modules and beyond.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param port        the GNSS chip's port number, selected
 *                    from #uGnssPort_t; this is for the [rare]
 *                    scenario that you are connected to the
 *                    GNSS chip on two different ports at the
 *                    same time (e.g. I2C and USB) and want to
 *                    read the communicatins stats of the I2C
 *                    port from the USB port, for instance.
 *                    To just read the communication stats for
 *                    the current port set this to -1.
 *                    Note: this parameter is _deliberately_ not
 *                    range checked so that new port numbers adopted
 *                    on future GNSS devices may be passed
 *                    transparently through to the GNSS device.
 * @param[out] pStats a pointer to a place to put the stats.
 * @return            zero on success, else negative error code.
 */
int32_t uGnssInfoGetCommunicationStats(uDeviceHandle_t gnssHandle,
                                       int32_t port,
                                       uGnssCommunicationStats_t *pStats);



#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_INFO_H_

// End of file
