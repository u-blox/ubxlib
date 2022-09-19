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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Logging strings for the RAM logging client.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.

/** The log events as strings; must be kept in line with the uLogRamEvent_t
 * enum in u_log_ram_enum.h. By convention, a "*" prefix means that an
 * error has occurred, making errors easier to spot.
 */
const char *gULogRamString[] = {
    // Log points required by this code, do not change
    "  EMPTY",
    "  START",
    "  START_AGAIN",
    "  STOP",
    "* ENTRIES_OVERWRITTEN",
    "  TIME_WRAP",
    // Generic user log points, do not change
    "  USER_0",
    "  USER_1",
    "  USER_2",
    "  USER_3",
    "  USER_4",
    "  USER_5",
    "  USER_6",
    "  USER_7",
    "  USER_8",
    "  USER_9",
    // Specific log points defined by the user
#include "u_log_ram_string_user.h"
};

/** Size of the gULogRamString array.
 */
const size_t gULogRamNumStrings = sizeof(gULogRamString) / sizeof(gULogRamString[0]);

// End of file
