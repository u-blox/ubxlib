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

#ifndef _U_CELL_PPP_H_
#define _U_CELL_PPP_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a small number of public functions
 * to do with the operation of PPP. USUALLY YOU NEED NONE OF THESE:
 * the cellular PPP interface functions are deliberately kept within
 * the cellular source code as they are called automagically when a
 * cellular connection is brought up or taken down, the application
 * does not need to know about them.
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

/** Provide a serial device that will be used when a PPP connection
 * is opened.  YOU DO NOT NORMALLY NEED TO USE THIS FUNCTION; the
 * serial port on which the existing AT interface is running will
 * normally be used for PPP, via the CMUX protocol.
 *
 * This function is useful if your AT interface is actually the
 * USB interface of the cellular module, which does not support
 * the CMUX protocol; with this function you can open a virtual
 * serial device on another USB end-point and that will be used
 * for PPP.  This is done automatically for you if you use
 * uNetworkInterfaceUp() and give it the details of the end-point
 * (i.e. the uNetwork code will call uCellPppDevice() for you).
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param[in] pDeviceSerial  the serial device to use for the PPP
 *                           connection, NULL to remove a previously
 *                           added serial device.
 * @return                   zero on success, else negative error
 *                           code.
 */
int32_t uCellPppDevice(uDeviceHandle_t cellHandle,
                       uDeviceSerial_t *pDeviceSerial);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_PPP_H_

// End of file
