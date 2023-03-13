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

#ifndef _U_BLE_EXTMOD_PRIVATE_H_
#define _U_BLE_EXTMOD_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines some common private function
 * for BLE external module implementations.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the current BLE role for the connected module
 *
 * @param atHandle    the handle of the AT client to use.
 * @return            a uBleCfgRole_t value, on failure negative error code.
 */
int32_t uBlePrivateGetRole(const uAtClientHandle_t atHandle);

#ifdef __cplusplus
}
#endif

#endif  // _U_BLE_EXTMOD_PRIVATE_H_

// End of file
