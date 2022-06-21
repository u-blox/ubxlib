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

#ifndef _U_DEVICE_PRIVATE_GNSS_H_
#define _U_DEVICE_PRIVATE_GNSS_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/* This header file defines the GNSS-specific part of the
 * device API. These functions perform NO error checking
 * and are NOT thread-safe; they should only be called from
 * within the device API which sorts all that out.
 */

/** @file
 * @brief Functions associated with a GNSS device.
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

/** Initialise GNSS.  If GNSS is already initialised this function
 * will return without doing anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uDevicePrivateGnssInit(void);

/** Deinitialise GNSS.  If GNSS is already deinitialised this
 * function will return without doing anything.
 */
void uDevicePrivateGnssDeinit(void);

/** Add a GNSS device, powering it up and making it available
 * for configuration and to support receiving satellites.
 *
 * @param[in] pDevCfg        a pointer to the device configuration
 *                           structure, one that should have been
 *                           populated for GNSS; cannot be NULL.
 * @param[out] pDeviceHandle a pointer to a place to put the device
 *                           handle, cannot be NULL.
 * @return                   zero on success else negative error
 *                           code.
 */
int32_t uDevicePrivateGnssAdd(const uDeviceCfg_t *pDevCfg,
                              uDeviceHandle_t *pDeviceHandle);

/** Remove a GNSS device.
 *
 * @param devHandle the handle of the device.
 * @param powerOff  if true then also power the device off.
 * @return          zero on success else negative error code.
 */
int32_t uDevicePrivateGnssRemove(uDeviceHandle_t devHandle,
                                 bool powerOff);

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_PRIVATE_GNSS_H_

// End of file
