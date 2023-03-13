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

#ifndef _U_INTERFACE_H_
#define _U_INTERFACE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files.
 *
 * This header file may be dragged into the top-level u_device.h
 * file and so it is PARTICULARLY important that it is not
 * depending on anything other than stdint.h types. */

/** \addtogroup __utils
 *  @{
 */

/** @file
 * @brief This header file defines functions that help with
 * generic "interface" types, containing sets of function
 * pointers that can be created and destroyed at run-time.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The default interface version number.
 */
#define U_INTERFACE_VERSION_DEFAULT 0

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The vector table.
 */
typedef void *uInterfaceTable_t;

/** The initialisation function of an interface.
 *
 * @param pInterfaceTable  a pointer to the vector table.
 * @param pInitParam       user parameter, passed through by
 *                         pUInterfaceCreate().
 */
typedef void (*uInterfaceInit_t)(uInterfaceTable_t pInterfaceTable,
                                 void *pInitParam);

/** The deinitialisation function of an interface.
 *
 * @param pInterfaceTable  a pointer to the vector table.
 */
typedef void (*uInterfaceDeinit_t)(uInterfaceTable_t pInterfaceTable);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Create an interface, along with context storage if required.
 *
 * @param sizeVectorTableBytes  the size of the vector table required, in bytes.
 * @param sizeContextBytes      the size of context required; may be zero.
 * @param version               a version number for the interface; use
 *                              #U_INTERFACE_VERSION_DEFAULT if you don't care.
 * @param pInterfaceInit        the initialisation function for the interface;
 *                              this will be called by pUInterfaceCreate() once
 *                              the interface has been created.  It may be used,
 *                              for example, to populate the vector table and/or
 *                              initialise the context; may be NULL if no
 *                              initialisation is required.
 *                              IMPORTANT: for forwards-compatibility it is
 *                              highly recommended that an initialisation
 *                              function is provided which populates all of
 *                              the entries in the vector table with default
 *                              implementations that return
 *                              #U_ERROR_COMMON_NOT_IMPLEMENTED or similar;
 *                              without this, should you add new functions to
 *                              an existing interface type without notice to
 *                              the implementers of that interface, any user of
 *                              the interface may end up calling NULL pointers.
 * @param pInitParam            parameter that will be passed to pInterfaceInit;
 *                              may be NULL, ignored if pInterfaceInit is NULL.
 * @param pInterfaceDeinit      the deinitialisation function for the interface;
 *                              this will be stored and passed to
 *                              uInterfaceDelete() so that it can be called when
 *                              the interface is deleted.
 *                              May be NULL if no deinitialisation is required.
 * @return                      on success a pointer to the vector table, else NULL.
 */
uInterfaceTable_t *pUInterfaceCreate(size_t sizeVectorTableBytes,
                                     size_t sizeContextBytes,
                                     int32_t version,
                                     uInterfaceInit_t pInterfaceInit,
                                     void *pInitParam,
                                     uInterfaceDeinit_t pInterfaceDeinit);

/** Get the context pointer of an interface.
 *
 * @param pInterfaceTable  a pointer to the interface table that was returned by
 *                         pUInterfaceCreate().
 * @return                 a pointer to the context for the interface; NULL if
 *                         zero bytes of context were requested in the call to
 *                         pUInterfaceCreate().
 */
void *pUInterfaceContext(uInterfaceTable_t pInterfaceTable);

/** Get the interface version.
 *
 * @param pInterfaceTable  a pointer to the interface table that was returned by
 *                         pUInterfaceCreate().
 * @return                 the interface version, as passed to pUInterfaceCreate().
 */
int32_t uInterfaceVersion(uInterfaceTable_t pInterfaceTable);

/** Delete an interface, calling the pInterfaceDeinit function that was
 * passed to pUInterfaceCreate() in the process.
 *
 * @param pInterfaceTable  a pointer to the interface table that was returned by
 *                         pUInterfaceCreate().
 */
void uInterfaceDelete(uInterfaceTable_t pInterfaceTable);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_INTERFACE_H_

// End of file
