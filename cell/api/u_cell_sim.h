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

#ifndef _U_CELL_SIM_H_
#define _U_CELL_SIM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox cellular API
 * for SIM access.  Note that this is DELIBERATELY a minimal API:
 * the kinds of applications in which ubxlib is used don't
 * generally use SIM PINs (a PIN offers no security since it
 * has to be stored in this MCU and passed over a UART interface
 * at every module power-on), don't need phone-book access, etc.
 * Only APIs that have been requested are included here: if
 * you need more, please let us know, or if there is enough
 * interest we might create a generic +CSIM/+CRSM send/receive
 * API etc.
 *
 * These functions are thread-safe unless otherwise specified
 * in the function description.
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

/** Delete the forbidden PLMN list on the SIM.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           zero on success or negative error code on failure.
 */
int32_t uCellSimFplmnListDelete(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_SIM_H_

// End of file
