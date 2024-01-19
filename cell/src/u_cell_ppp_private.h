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

#ifndef _U_CELL_PPP_PRIVATE_H_
#define _U_CELL_PPP_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines functions that are private to
 * cellular and associated with PPP.
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

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Remove the PPP context for the given cellular instance.  If PPP
 * is active it will be disabled first.  Note that this may cause the
 * atHandle in pInstance to change, so if you have a local copy of it
 * you will need to refresh it once this function returns.
 *
 * This should be called _before_ uCellMuxPrivateRemoveContext(),
 * since it cleans up CMUX stuff of its own.
 *
 * Note: gUCellPrivateMutex should be locked before this is called.
 *
 * @param[in] pInstance a pointer to the cellular instance.
 */
void uCellPppPrivateRemoveContext(uCellPrivateInstance_t *pInstance);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_PPP_PRIVATE_H_

// End of file
