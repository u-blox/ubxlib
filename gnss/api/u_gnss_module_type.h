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

/*  IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT
 *
 * NOTE TO MAINTAINERS: if you change this enum you will need to
 * change u-blox,ubxlib-device-gnss.yaml AND
 * u-blox,ubxlib-network-gnss.yaml over in
 * /port/platform/zephyr/dts/bindings to match and you will also
 * need to update the table in the Zephyr u_port_board_cfg.c file
 * that maps string to enum.
 */
/** The possible types of GNSS module.
 * Note: if you add a new module type here, check the
 * U_GNSS_PRIVATE_MODULE_xxx macros in u_gnss_private.h
 * to see if they need updating and also update the
 * tables in u_gnss_private.c and u_gnss_pwr.c.
 */
//lint -estring(788, uGnssModuleType_t::U_GNSS_MODULE_TYPE_MAX_NUM)
// Suppress not used within defaulted switch
typedef enum {
    U_GNSS_MODULE_TYPE_M8  = 0,
    U_GNSS_MODULE_TYPE_M9  = 1,
    U_GNSS_MODULE_TYPE_M10 = 2,
    // Add any new module types here, before U_GNSS_MODULE_TYPE_ANY, assigning
    // them to specific values.
    // IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT: see note above.
    U_GNSS_MODULE_TYPE_ANY, /**< when this module type is used the code will
                                 interrogate the module and chose the correct
                                 module type by itself; should this fail, for
                                 example because you are using a module type which
                                 is sufficiently close to a supported module type
                                 to work but the ID string it returns is too
                                 different to be detected, then you should chose
                                 the specific module type you want instead. */
    U_GNSS_MODULE_TYPE_MAX_NUM
} uGnssModuleType_t;

/** @}*/

#endif // _U_GNSS_MODULE_TYPES_H_

// End of file
