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

#ifndef _U_GNSS_INFO_H_
#define _U_GNSS_INFO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

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

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the version string from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param pStr        a pointer to size bytes of storage into which
 *                    the firmware version string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a null terminator. Must be
 *                    greater than zero.
 * @return            on success, the number of characters copied
 *                    into pStr NOT including the null terminator
 *                    (i.e. as strlen() would return), else negative
 *                    error code.  Note that the string itself may
 *                    contain multiple lines separated by [more than
 *                    one] null terminator, depending on what the
 *                    GNSS device chooses to return.
 */
int32_t uGnssInfoGetFirmwareVersionStr(int32_t gnssHandle,
                                       char *pStr, size_t size);

/** Get the chip ID from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param pStr        a pointer to size bytes of storage into which
 *                    the chip ID string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr,
 *                    including room for a null terminator. Must be
 *                    greater than zero.
 * @return            on success, the number of characters copied
 *                    into pStr NOT including the null terminator
 *                    (i.e. as strlen() would return), else negative
 *                    error code.  Note that the string itself may
 *                    contain nulls, depending on what the GNSS
 *                    device chooses to return
 */
int32_t uGnssInfoGetIdStr(int32_t gnssHandle,
                          char *pStr, size_t size);

/** Get the UTC time according to GNSS.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the Unix UTC time, else negative
 *                    error code.
 */
int64_t uGnssInfoGetTimeUtc(int32_t gnssHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_INFO_H_

// End of file
