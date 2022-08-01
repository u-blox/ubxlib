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

#ifndef _U_CELL_MODULE_TYPES_H_
#define _U_CELL_MODULE_TYPES_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

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
 * tables in u_cell_private.c and u_cell_sock_test.c.
 */
//lint -estring(788, uCellModuleType_t::U_CELL_MODULE_TYPE_MAX_NUM)
// Suppress not used within defaulted switch
typedef enum {
    U_CELL_MODULE_TYPE_SARA_U201 = 0,
    U_CELL_MODULE_TYPE_SARA_R410M_02B = 1, /**<  the difference between the
                                                 R410M-x2B module flavours
                                                 (the "x") is the band support,
                                                 which is not "known" by this
                                                 driver, hence specifying
                                                 #U_CELL_MODULE_TYPE_SARA_R410M_02B
                                                 should work for all SARA-R410M-x2B
                                                 module varieties.
                                                 IMPORTANT: the "2" absolutely IS
                                                 significant though; SARA-R410M-x2B
                                                 modules, for instance, have an utterly
                                                 different set of MQTT AT commands
                                                 to the SARA-R410M-x3B modules. */
    U_CELL_MODULE_TYPE_SARA_R412M_02B = 2,
    U_CELL_MODULE_TYPE_SARA_R412M_03B = 3, /**< this module designation never made it
                                                to mass production and hence is no
                                                longer tested. */
    U_CELL_MODULE_TYPE_SARA_R5 = 4,
    U_CELL_MODULE_TYPE_SARA_R410M_03B = 5, /**< as for the R410M-x2B modules, the
                                                difference between the R410M-x3B
                                                module flavours (the "x") is band
                                                support, which is not "known" by
                                                this driver, hence specifying
                                                #U_CELL_MODULE_TYPE_SARA_R410M_03B
                                                should work for all SARA-R410M-x3B
                                                module varieties. Again, do NOT
                                                get a SARA-R4 x3B module and a
                                                SARA_R4 x2B module mixed up though,
                                                the "3" is significant. */
    U_CELL_MODULE_TYPE_SARA_R422 = 6,
    U_CELL_MODULE_TYPE_LARA_R6 = 7,
    U_CELL_MODULE_TYPE_MAX_NUM
} uCellModuleType_t;

/** @}*/

#endif // _U_CELL_MODULE_TYPES_H_

// End of file
