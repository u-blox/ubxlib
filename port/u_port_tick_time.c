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

/** @file
 * @brief Implemenation of common wrap-safe tick-timer expiry functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_compiler.h"  // U_INLINE

#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_port_debug.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Perform a tick time-out check, relative to start time, taking
// into account wrap.
U_INLINE bool uPortTickTimeExpired(int32_t startTimeMs,
                                   int32_t timeoutMs)
{
    return (uPortGetTickTimeMs() - startTimeMs > timeoutMs);
}

// Perform a tick timeout check taking into account wrap.
U_INLINE bool uPortTickTimeBeyondStop(int32_t stopTimeMs)
{
    return (uPortGetTickTimeMs() - stopTimeMs > 0);
}

// End of file
