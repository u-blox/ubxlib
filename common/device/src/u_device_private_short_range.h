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

#ifndef _U_DEVICE_PRIVATE_SHORT_RANGE_H_
#define _U_DEVICE_PRIVATE_SHORT_RANGE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/* This header file defines the short-range-specific part of the
 * device API. These functions perform NO error checking
 * and are NOT thread-safe; they should only be called from
 * within the device API which sorts all that out.
 */

/** @file
 * @brief Functions associated with a short-range device, one
 * supporting either BLE or Wifi or both.
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

/** Initialise short-range.  If short-range is already initialised
 * this function will return without doing anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uDevicePrivateShortRangeInit(void);

/** Deinitialise short-range.  If short-range is already deinitialised
 * this function will return without doing anything.
 */
void uDevicePrivateShortRangeDeinit(void);

/** Add a short-range device, powering it up and making it available
 * for configuration and to support a network interface.  Note that
 * this is ONLY for use where the short-range device is external
 * to the MCU; where the short-range "device" is actually on-board
 * the MCU, use uDevicePrivateShortRangeOpenCpuAdd().
 *
 * @param[in] pDevCfg        a pointer to the device configuration
 *                           structure, one that should have been
 *                           populated for short-range; cannot be NULL.
 * @param[out] pDeviceHandle a pointer to a place to put the device
 *                           handle, cannot be NULL.
 * @return                   zero on success else negative error code.
 */
int32_t uDevicePrivateShortRangeAdd(const uDeviceCfg_t *pDevCfg,
                                    uDeviceHandle_t *pDeviceHandle);

/** Add a short-range device, making it available for configuration
 * and to support a network interface.  Note that this is ONLY for
 * use where the short-range "device" is on-board the MCU; where
 * the short-range device is external to the MCU, use
 * uDevicePrivateShortRangeAdd() instead.
 *
 * @param[in] pDevCfg        a pointer to the device configuration
 *                           structure, one that should have been
 *                           populated for short-range; cannot be NULL.
 * @param[out] pDeviceHandle a pointer to a place to put the device
 *                           handle, cannot be NULL.
 * @return                   zero on success else negative error code.
 */
int32_t uDevicePrivateShortRangeOpenCpuAdd(const uDeviceCfg_t *pDevCfg,
                                           uDeviceHandle_t *pDeviceHandle);

/** Remove a short-range device.  Note that this is ONLY for use
 * where the short-range device is external to the MCU; where the
 * short-range "device" is actually on-board the MCU, use
 * uDevicePrivateShortRangeOpenCpuRemove() instead.
 *
 * @param devHandle the handle of the device.
 * @return          zero on success else negative error code.
 */
int32_t uDevicePrivateShortRangeRemove(uDeviceHandle_t devHandle);

/** Remove a short-range device.  Note that this is ONLY for use
 * where the short-range "device" is on-board the MCU; where the
 * short-range device is external to the MCU, use
 * uDevicePrivateShortRangeRemove() instead.
 *
 * @param devHandle the handle of the device.
 * @return          zero on success else negative error code.
 */
int32_t uDevicePrivateShortRangeOpenCpuRemove(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_PRIVATE_SHORT_RANGE_H_

// End of file
