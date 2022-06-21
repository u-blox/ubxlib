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

#ifndef _U_DEVICE_SHARED_CELL_H_
#define _U_DEVICE_SHARED_CELL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This file define types that are specific to cellular and
 * are known to the device layer but need to be visible to the
 * network layer; the data here is passed around in the pContext
 * pointer of a device instance.
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

/** The things we need to remember per cellular device.
 */
typedef struct {
    int32_t uart;
    uAtClientHandle_t at;
    int64_t stopTimeMs;
    int32_t pinPwrOn;
} uDeviceCellContext_t;

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_SHARED_CELL_H_

// End of file
