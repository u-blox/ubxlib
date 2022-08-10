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

/** @file
 * @brief General functions private to the device layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_i2c.h"

#include "u_device.h"
#include "u_device_shared.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_DEVICE_PRIVATE_I2C_MAX_NUM
/** The number of I2C HW blocks that this code knows about.
 */
# ifdef U_PORT_I2C_MAX_NUM
#  define U_DEVICE_PRIVATE_I2C_MAX_NUM U_PORT_I2C_MAX_NUM
# else
#  define U_DEVICE_PRIVATE_I2C_MAX_NUM 4
# endif
#endif

#ifndef U_DEVICE_PRIVATE_DEVICE_I2C_MAX_NUM
/** The maximum number of devices that can be using an I2C transport
 * at any one time.
 */
# define U_DEVICE_PRIVATE_DEVICE_I2C_MAX_NUM 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold an I2C HW block and a openCount of how many times it
 * has been opened.
 */
typedef struct {
    int32_t i2c;
    int32_t i2cHandle;
    size_t openCount;
} uDevicePrivateI2c_t;

/** Type to hold a device and what I2C HW block it has open.
 */
typedef struct {
    uDeviceHandle_t devHandle;
    uDevicePrivateI2c_t *pI2c;
} uDevicePrivateDeviceI2c_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Storage to track how many times a given I2C HW block has
 * been opened.
 */
static uDevicePrivateI2c_t gI2c[U_DEVICE_PRIVATE_I2C_MAX_NUM];

/** Storage to track which devices are using which I2C HW blocks.
 */
static uDevicePrivateDeviceI2c_t gDeviceI2c[U_DEVICE_PRIVATE_DEVICE_I2C_MAX_NUM];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find an I2C HW block in the list of I2C HW blocks; returns NULL if
// not found, use -1 to find the first unused entry.
static uDevicePrivateI2c_t *pFindI2c(int32_t i2c)
{
    uDevicePrivateI2c_t *pI2c = NULL;

    for (size_t x = 0; (x < sizeof(gI2c) / sizeof(gI2c[0])) && (pI2c == NULL); x++) {
        if (gI2c[x].i2c == i2c) {
            pI2c = &(gI2c[x]);
        }
    }

    return pI2c;
}

// Find a device in the list of devices that are using an I2C transport;
// returns NULL if not found, use NULL to find the first unused entry.
static uDevicePrivateDeviceI2c_t *pFindDeviceI2c(uDeviceHandle_t devHandle)
{
    uDevicePrivateDeviceI2c_t *pDeviceI2c = NULL;

    for (size_t x = 0; (x < sizeof(gDeviceI2c) / sizeof(gDeviceI2c[0])) &&
         (pDeviceI2c == NULL); x++) {
        if (gDeviceI2c[x].devHandle == devHandle) {
            pDeviceI2c = &(gDeviceI2c[x]);
        }
    }

    return pDeviceI2c;
}

// Clear a device I2C entry.
static void clearDeviceI2cEntry(uDevicePrivateDeviceI2c_t *pDeviceI2c)
{
    if (pDeviceI2c != NULL) {
        pDeviceI2c->devHandle = NULL;
        pDeviceI2c->pI2c = NULL;
    }
}

// Clear an I2C entry.
static void clearI2cEntry(uDevicePrivateI2c_t *pI2c)
{
    if (pI2c != NULL) {
        pI2c->i2c = -1;
        pI2c->i2cHandle = -1;
        pI2c->openCount = 0;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Open an I2C port.
int32_t uDevicePrivateI2cOpen(const uDeviceCfgI2c_t *pCfgI2c)
{
    int32_t errorCodeOrI2cHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDevicePrivateI2c_t *pI2c;
    int32_t x;

    if ((pCfgI2c != NULL) && (pCfgI2c->i2c >= 0)) {
        pI2c = pFindI2c(pCfgI2c->i2c);
        if (pI2c == NULL) {
            // The I2C HW block is not in the list so we need
            // to either open it or adopt it for our use
            if (pCfgI2c->alreadyOpen) {
                errorCodeOrI2cHandle = uPortI2cAdopt(pCfgI2c->i2c, true);
            } else {
                errorCodeOrI2cHandle = uPortI2cOpen(pCfgI2c->i2c, pCfgI2c->pinSda,
                                                    pCfgI2c->pinScl, true);
                // If we're opening rather than adopting, so can
                // touch the HW, then also configure the clock if the
                // user has set a clock frequency
                if ((errorCodeOrI2cHandle >= 0) && (pCfgI2c->clockHertz > 0)) {
                    x = uPortI2cSetClock(errorCodeOrI2cHandle, pCfgI2c->clockHertz);
                    if (x < 0) {
                        // Clean up on error
                        uPortI2cClose(errorCodeOrI2cHandle);
                        errorCodeOrI2cHandle = x;
                    }
                }
            }
            if (errorCodeOrI2cHandle >= 0) {
                // Find a free entry in the list and put the I2C HW
                // block and handle there, setting the openCount to 1
                pI2c = pFindI2c(-1);
                if (pI2c != NULL) {
                    pI2c->i2c = pCfgI2c->i2c;
                    pI2c->i2cHandle = errorCodeOrI2cHandle;
                    pI2c->openCount = 1;
                } else {
                    // No room, clean up; don't need to worry about whether
                    // it is adopted etc. the port API handles that
                    uPortI2cClose(errorCodeOrI2cHandle);
                    errorCodeOrI2cHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                }
            }
        } else {
            // The I2C HW block is in the list, already open,
            // just increment openCount and return the already
            // opened handle
            pI2c->openCount++;
            errorCodeOrI2cHandle = pI2c->i2cHandle;
        }
    }

    return errorCodeOrI2cHandle;
}

// Track the I2C configuration used by a given handle.
int32_t uDevicePrivateI2cIsUsedBy(uDeviceHandle_t devHandle,
                                  const uDeviceCfgI2c_t *pCfgI2c)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDevicePrivateI2c_t *pI2c;
    uDevicePrivateDeviceI2c_t *pDeviceI2c;

    if ((pCfgI2c != NULL) && (pCfgI2c->i2c >= 0)) {
        pI2c = pFindI2c(pCfgI2c->i2c);
        if (pI2c != NULL) {
            // The I2C HW block is in the list
            pDeviceI2c = pFindDeviceI2c(devHandle);
            if (pDeviceI2c == NULL) {
                // The device is not in the list, so find
                // a free entry to put it in
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pDeviceI2c = pFindDeviceI2c(NULL);
                if (pDeviceI2c != NULL) {
                    // Done
                    pDeviceI2c->devHandle = devHandle;
                    pDeviceI2c->pI2c = pI2c;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    return errorCode;
}

// Close an I2C port based on device handle.
void uDevicePrivateI2cCloseDevHandle(uDeviceHandle_t devHandle)
{
    uDevicePrivateDeviceI2c_t *pDeviceI2c;

    if (devHandle != NULL) {
        // Find the device in the list
        pDeviceI2c = pFindDeviceI2c(devHandle);
        if ((pDeviceI2c != NULL) && (pDeviceI2c->pI2c != NULL)) {
            // Found it, decrement the openCount for this I2C HW block
            if (pDeviceI2c->pI2c->openCount > 0) {
                pDeviceI2c->pI2c->openCount--;
            }
            if (pDeviceI2c->pI2c->openCount == 0) {
                // If no-one is using the port, close it; no need
                // to worry about whether it is adopted etc. the port
                // API handles that.
                uPortI2cClose(pDeviceI2c->pI2c->i2cHandle);
                // Remove the linkage to any devices in the list
                for (size_t x = 0; x < sizeof(gDeviceI2c) / sizeof(gDeviceI2c[0]); x++) {
                    if (gDeviceI2c[x].pI2c == pDeviceI2c->pI2c) {
                        clearDeviceI2cEntry(&gDeviceI2c[x]);
                    }
                }
                // Delete the entry in the I2C HW list
                clearI2cEntry(pDeviceI2c->pI2c);
            } else {
                // NULL this device I2C entry
                clearDeviceI2cEntry(pDeviceI2c);
            }
        }
    }
}

// Close an I2C port based on I2C configuration.
void uDevicePrivateI2cCloseCfgI2c(const uDeviceCfgI2c_t *pCfgI2c)
{
    uDevicePrivateI2c_t *pI2c;

    if ((pCfgI2c != NULL) && (pCfgI2c->i2c >= 0)) {
        pI2c = pFindI2c(pCfgI2c->i2c);
        if (pI2c != NULL) {
            // The I2C HW block is in the list, decrement the openCount
            if (pI2c->openCount > 0) {
                pI2c->openCount--;
            }
            if (pI2c->openCount == 0) {
                // If no-one is using the port, close it; no need
                // to worry about whether it is adopted etc. the port
                // API handles that.
                uPortI2cClose(pI2c->i2cHandle);
                // Remove the linkage to any devices in the list
                for (size_t x = 0; x < sizeof(gDeviceI2c) / sizeof(gDeviceI2c[0]); x++) {
                    if (gDeviceI2c[x].pI2c == pI2c) {
                        clearDeviceI2cEntry(&gDeviceI2c[x]);
                    }
                }
                // Delete the entry in the I2C HW list
                clearI2cEntry(pI2c);
            }
        }
    }
}

// Reset stuff in the device internals.
void uDevicePrivateInit()
{
    for (size_t x = 0; x < sizeof(gDeviceI2c) / sizeof(gDeviceI2c[0]); x++) {
        clearDeviceI2cEntry(&gDeviceI2c[x]);
    }
    for (size_t x = 0; x < sizeof(gI2c) / sizeof(gI2c[0]); x++) {
        clearI2cEntry(&gI2c[x]);
    }
}

// End of file
