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

#ifndef _U_SHORT_RANGE_CFG_H_
#define _U_SHORT_RANGE_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"
#include "u_at_client.h"
#include "u_short_range_module_type.h"

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to the SHORT_RANGE API.
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

/* FUNCTIONS
 * -------------------------------------------------------------- */

/** Return the shortrange module's file system and or non-volatile
 * storage to factory defaults. The module will re-booted afterwards
 * for it to take effect.
 *
 * @param devHandle      the handle of the shortrange instance.
 * @return               zero on success or negative error code on
 *                       failure.
 */

int32_t uShortRangeCfgFactoryReset(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_CFG_H_

// End of file