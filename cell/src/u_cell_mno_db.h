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

#ifndef _U_CELL_MNO_DB_H_
#define _U_CELL_MNO_DB_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines an internal API which allows
 * the ubxlib code to identify special behaviours, ones implied
 * by the currently-set MNO profile.  For instance, the Verizon
 * Wireless (VZW) profile, number 3, requires that the AT+CGDCONT
 * command should not be accepted; a cellulat module set to
 * MNO profile 3 will return "operation not allowed" if the
 * command is sent.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Features of an MNO profile that require different
 * run-time behaviours in this implementation.
 */
typedef enum {
    U_CELL_MNO_DB_FEATURE_NO_CGDCONT, /**< VZW (profile 3) needs this. */
    U_CELL_MNO_DB_FEATURE_IGNORE_APN  /**< set this if
                                           #U_CELL_MNO_DB_FEATURE_NO_CGDCONT
                                           is set in order to continue without
                                           error if the user tries to set
                                           an APN. */
} uCellMnoDbFeature_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Determine if the current MNO profile has the given feature.
 *
 * @param[in] pInstance a pointer to the cellular instance.
 * @param feature       the feature to check.
 * @return              true if the feature is present for the current
 *                      MNO profile, else false.
 */
bool uCellMnoDbProfileHas(const uCellPrivateInstance_t *pInstance,
                          uCellMnoDbFeature_t feature);

/** @}*/

#endif // _U_CELL_MNO_DB_H_

// End of file
