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
 * @brief Assert failure function.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.

#include "u_cfg_sw.h"
#include "u_compiler.h" // for U_WEAK

#include "u_port_debug.h"

#include "u_assert.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The assert failed hook.
 */
static upAssertFailed_t *gpAssertFailed = NULL;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Register an assertFailed() function.
void uAssertHookSet(upAssertFailed_t *pAssertFailed)
{
    gpAssertFailed = pAssertFailed;
}

// The default assertFailed() function.
U_WEAK void uAssertFailed(const char *pFileStr, int32_t line)
{
    if (gpAssertFailed != NULL) {
        gpAssertFailed(pFileStr, line);
    } else {
        uPortLog("*** ASSERT FAILURE at %s:%d ***\n", pFileStr, line);
        // Enter infinite loop
        for (;;) {}
    }
}

// End of file
