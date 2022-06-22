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

#ifndef _U_ERROR_COMMON_H_
#define _U_ERROR_COMMON_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup error Error codes
 *  @{
 */

/** @file
 *@brief This header file defines the error codes for ubxlib.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Base error code: you may override this with a NEGATIVE number
 * if you wish.
 */
#ifndef U_ERROR_BASE
# define U_ERROR_BASE 0
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes.
 */
typedef enum {
    U_ERROR_COMMON_FORCE_INT32 = 0x7FFFFFFF, /* Force this enum to be 32 bit
                                              * as it can be used as a size
                                              * also. */
    U_ERROR_COMMON_SUCCESS = 0,
    U_ERROR_COMMON_UNKNOWN = U_ERROR_BASE - 1,
    U_ERROR_COMMON_BSD_ERROR = U_ERROR_BASE - 1,
    U_ERROR_COMMON_NOT_INITIALISED = U_ERROR_BASE - 2,
    U_ERROR_COMMON_NOT_IMPLEMENTED = U_ERROR_BASE - 3,
    U_ERROR_COMMON_NOT_SUPPORTED = U_ERROR_BASE - 4,
    U_ERROR_COMMON_INVALID_PARAMETER = U_ERROR_BASE - 5,
    U_ERROR_COMMON_NO_MEMORY = U_ERROR_BASE - 6,
    U_ERROR_COMMON_NOT_RESPONDING = U_ERROR_BASE - 7,
    U_ERROR_COMMON_PLATFORM = U_ERROR_BASE - 8,
    U_ERROR_COMMON_TIMEOUT = U_ERROR_BASE - 9,
    U_ERROR_COMMON_DEVICE_ERROR = U_ERROR_BASE - 10,
    U_ERROR_COMMON_NOT_FOUND = U_ERROR_BASE - 11,
    U_ERROR_COMMON_INVALID_ADDRESS = U_ERROR_BASE - 12,
    U_ERROR_COMMON_TEMPORARY_FAILURE = U_ERROR_BASE - 13,
    U_ERROR_COMMON_AUTHENTICATION_FAILURE = U_ERROR_BASE - 14,
    U_ERROR_COMMON_MIN = U_ERROR_BASE - 255,
    U_ERROR_CELL_MAX = U_ERROR_BASE - 256,
    U_ERROR_CELL_MIN = U_ERROR_BASE - 511,
    U_ERROR_BLE_MAX = U_ERROR_BASE - 512,
    U_ERROR_BLE_MIN = U_ERROR_BASE - 1023,
    U_ERROR_GNSS_MAX = U_ERROR_BASE - 1024,
    U_ERROR_GNSS_MIN = U_ERROR_BASE - 2047,
    U_ERROR_WIFI_MAX = U_ERROR_BASE - 2048,
    U_ERROR_WIFI_MIN = U_ERROR_BASE - 4095,
    U_ERROR_SHORT_RANGE_MAX = U_ERROR_BASE - 4096,
    U_ERROR_SHORT_RANGE_MIN = U_ERROR_BASE - 8191,
} uErrorCode_t;

/** @}*/

#endif // _ERROR_COMMON_H_

// End of file
