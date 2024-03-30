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

#ifndef _U_DEVICE_SERIAL_WRAPPED_H_
#define _U_DEVICE_SERIAL_WRAPPED_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup device Device
 *  @{
 */

/** @file
 * @brief Function to wrap a real serial device with a virtual serial
 * interface.
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

/** Create a serial device under which lies a physical UART.
 * Thereafter the physical UART uPortUart calls will be mapped to
 * the device serial calls, allowing a physical UART port to be used
 * in the same way as a virtual one.
 *
 * When you have finished with the wrapped serial device it should be
 * deleted with a call to uDeviceSerialDelete() in the usual way.
 *
 * @param[in] pCfgUart  a pointer to the UART configuration; cannot
 *                      be NULL.
 * @return              on success a pointer to the serial device, else
 *                      NULL.
 */
uDeviceSerial_t *pDeviceSerialCreateWrappedUart(const uDeviceCfgUart_t *pCfgUart);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_DEVICE_SERIAL_WRAPPED_H_

// End of file
