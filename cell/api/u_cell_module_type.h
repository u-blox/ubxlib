/*
 * Copyright 2020 u-blox Ltd
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

#ifndef _U_CELL_MODULE_TYPES_H_
#define _U_CELL_MODULE_TYPES_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the module types for the
 * cellular API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible types of cellular module.
 * Note: if you add a new module type here, check the
 * U_CELL_PRIVATE_MODULE_xxx macros in u_cell_private.h
 * to see if they need updating and also update the
 * tables in u_cell_cfg.c and u_cell_private.c.
 * Note: order is important as these are used to index
 * into a statically defined array in u_cell_cfg.c.
 */
//lint -estring(788, uCellModuleType_t::U_CELL_MODULE_TYPE_MAX_NUM)
// Suppress not used within defaulted switch
typedef enum {
    U_CELL_MODULE_TYPE_SARA_U201 = 0,
    U_CELL_MODULE_TYPE_SARA_R410M_02B = 1, /**<  The difference between the
                                                 R410M module flavours is
                                                 the band support, which is
                                                 not "known" by this driver,
                                                 hence specifying
                                                 U_CELL_MODULE_TYPE_SARA_R410M_02B
                                                 should work for all SARA-R410M
                                                 module varieties. */
    U_CELL_MODULE_TYPE_SARA_R412M_02B = 2,
    U_CELL_MODULE_TYPE_SARA_R412M_03B = 3,
    U_CELL_MODULE_TYPE_SARA_R5 = 4,
    U_CELL_MODULE_TYPE_MAX_NUM
} uCellModuleType_t;

#endif // _U_CELL_MODULE_TYPES_H_

// End of file
