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

#ifndef _U_DEVICE_PRIVATE_H_
#define _U_DEVICE_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief General functions private to the device layer.
 * To ensure thread-safety the device API must be locked with
 * a call to uDeviceLock() before any of these functions are
 * called.
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

/** Open an I2C port; if the alreadyOpen flag is set in
 * pCfgI2c then the device is adopted instead of being
 * opened.  The device API must be locked with a call to
 * uDeviceLock() before this is called.
 *
 * @param pCfgI2c   the I2C port configuration data.
 * @return          on success the I2C handle else negative error code.
 */
int32_t uDevicePrivateI2cOpen(const uDeviceCfgI2c_t *pCfgI2c);

/** Log that the given I2C configuration is used by the given
 * device handle.  The I2C port must have been opened first with
 * uDevicePrivateI2cOpen().  This should be called once a
 * device that is going to use an I2C port has been successfully
 * created, so that this code can keep track of who is using which
 * I2C ports and not close them prematurely.  The device API must
 * be locked with a call to uDeviceLock() before this is called.
 *
 * @param devHandle the handle of the device.
 * @param pCfgI2c   the I2C port configuration data.
 * @return          zero on success else negative error code.
 */
int32_t uDevicePrivateI2cIsUsedBy(uDeviceHandle_t devHandle,
                                  const uDeviceCfgI2c_t *pCfgI2c);

/** Close an I2C port based on the device handle; the port is only
 * actually closed if no-one is still using it based on a count of
 * the number of times it has been opened.  The device API must
 * be locked with a call to uDeviceLock() before this is called.
 *
 * @param devHandle the handle of the device using the I2C port.
 */
void uDevicePrivateI2cCloseDevHandle(uDeviceHandle_t devHandle);

/** Close an I2C port based on the I2C configuration; this may
 * be used to clean-up if an I2C port is opened but was never
 * associated with a device.  The port is only actually closed
 * if no-one is still using it based on a count of the number
 * of times it has been opened.  The device API must be locked
 * with a call to uDeviceLock() before this is called.
 *
 * @param pCfgI2c   the I2C port configuration data.
 */
void uDevicePrivateI2cCloseCfgI2c(const uDeviceCfgI2c_t *pCfgI2c);

/** Initialise stuff in the device internals, should be called by
 * the device layer initialisation function.
 */
void uDevicePrivateInit();

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_PRIVATE_H_

// End of file
