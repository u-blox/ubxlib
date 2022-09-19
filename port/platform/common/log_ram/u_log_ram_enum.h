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

#ifndef _LOG_RAM_ENUM_H_
#define _LOG_RAM_ENUM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This file contains the fixed log points for the logging
 * utility: see u_log_ram_enum_user.h for the user log-points.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Increment this variable if you make any changes to the enum below.
 */
#define U_LOG_RAM_VERSION 0

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible events for the RAM log if you add an item here,
 * don't forget to add it to gULogRamEventString (in
 * u_log_ram_string.c) also.
 */
typedef enum {
    // Log points required by this code, do not change
    U_LOG_RAM_EVENT_NONE = 0,
    U_LOG_RAM_EVENT_START,
    U_LOG_RAM_EVENT_START_AGAIN,
    U_LOG_RAM_EVENT_STOP,
    U_LOG_RAM_EVENT_ENTRIES_OVERWRITTEN,
    U_LOG_RAM_EVENT_TIME_WRAP,
    // Generic log points
    U_LOG_RAM_EVENT_USER_0,
    U_LOG_RAM_EVENT_USER_1,
    U_LOG_RAM_EVENT_USER_2,
    U_LOG_RAM_EVENT_USER_3,
    U_LOG_RAM_EVENT_USER_4,
    U_LOG_RAM_EVENT_USER_5,
    U_LOG_RAM_EVENT_USER_6,
    U_LOG_RAM_EVENT_USER_7,
    U_LOG_RAM_EVENT_USER_8,
    U_LOG_RAM_EVENT_USER_9,
    // Add your own named log points in u_log_ram_enum_user.h
#include "u_log_ram_enum_user.h"
} uLogRamEvent_t;

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _LOG_RAM_ENUM_H_

// End of file
