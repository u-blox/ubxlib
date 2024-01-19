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

#ifndef _U_GNSS_SHARED_H_
#define _U_GNSS_SHARED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few functions that are not public
 * but are shared with the rest of ubxlib.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Update the AT handles of any GNSS devices that were using pAtOld
 * to be pAtNew; useful if the AT handle is changed dynamically,
 * for example when CMUX is invoked with a cellular module via which
 * a GNSS device is connected.
 *
 * @param[in] pAtOld  the old AT handle.
 * @param[in] pAtNew  the new AT handle.
 */
void uGnssUpdateAtHandle(void *pAtOld, void *pAtNew);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_SHARED_H_

// End of file
