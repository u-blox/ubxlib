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

#ifndef _U_GNSS_MODULE_TYPES_H_
#define _U_GNSS_MODULE_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the module types for the
 * GNSS API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible types of GNSS module.
 * Note: if you add a new module type here, check the
 * U_GNSS_PRIVATE_MODULE_xxx macros in u_gnss_private.h
 * to see if they need updating and also update the
 * tables in u_gnss_private.c.
 */
//lint -estring(788, uGnssModuleType_t::U_GNSS_MODULE_TYPE_MAX_NUM)
// Suppress not used within defaulted switch
typedef enum {
    U_GNSS_MODULE_TYPE_M8 = 0,
    U_GNSS_MODULE_TYPE_M9 = 1,
    U_GNSS_MODULE_TYPE_MAX_NUM
} uGnssModuleType_t;

/** @}*/

#endif // _U_GNSS_MODULE_TYPES_H_

// End of file
