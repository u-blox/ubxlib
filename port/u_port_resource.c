/*
 * Copyright 2019-2023 u-blox
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
 * @brief Default implementation of the resource counting functions,
 * uPortOsResourceAllocCount(), uPortUartResourceAllocCount(), etc.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stdint.h"      // int32_t etc.
#include "u_compiler.h"  // U_WEAK

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifndef _MSC_VER
/* Note: *&!ing MSVC will simply NOT do weak-linkage, or even a
 * hacky equivalent, well enough to ignore these implementations in
 * favour of the ones under platform/windows: seem to be able to
 * do it for one (uPortGetTimezoneOffsetSeconds()) but not for more
 * than one.  Hence just ifndef'ing these out for MSVC.
 */
U_WEAK int32_t uPortOsResourceAllocCount()
{
    return 0;
}

U_WEAK int32_t uPortUartResourceAllocCount()
{
    return 0;
}
#endif

U_WEAK int32_t uPortI2cResourceAllocCount()
{
    return 0;
}

U_WEAK int32_t uPortSpiResourceAllocCount()
{
    return 0;
}

// End of file
