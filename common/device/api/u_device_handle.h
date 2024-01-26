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

#ifndef _U_DEVICE_HANDLE_H_
#define _U_DEVICE_HANDLE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup device Device
 *  @{
 */

/** @file
 * @brief Definition of uDeviceHandle_t, pulled out into a
 * separate header file so that u_port_board_cfg.h can get at
 * it without dragging the rest of the uDevice definitions into
 * the port API.
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

/** The u-blox device handle; this is intended to be anonymous,
 * the contents should never be referenced by the application.
 */
typedef void *uDeviceHandle_t;

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_DEVICE_HANDLE_H_

// End of file
