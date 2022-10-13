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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the "general" API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"

#include "u_at_client.h"
#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a cellular instance in the list by AT handle.
// gUCellPrivateMutex should be locked before this is called.
//lint -e{818} suppress "could be declared as pointing to const":
// atHandle is anonymous
static uCellPrivateInstance_t *pGetCellInstanceAtHandle(uAtClientHandle_t atHandle)
{
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->atHandle != atHandle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Add a cellular instance to the list.
// gUCellPrivateMutex should be locked before this is called.
// Note: doesn't copy it, just adds it.
static void addCellInstance(uCellPrivateInstance_t *pInstance)
{
    pInstance->pNext = gpUCellPrivateInstanceList;
    gpUCellPrivateInstanceList = pInstance;
}

// Remove a cell instance from the list.
// gUCellPrivateMutex should be locked before this is called.
// Note: doesn't free it, the caller must do that.
static void removeCellInstance(const uCellPrivateInstance_t *pInstance)
{
    uCellPrivateInstance_t *pCurrent;
    uCellPrivateInstance_t *pPrev = NULL;

    pCurrent = gpUCellPrivateInstanceList;
    while (pCurrent != NULL) {
        if (pInstance == pCurrent) {
            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                gpUCellPrivateInstanceList = pCurrent->pNext;
            }
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }

    uDeviceDestroyInstance(U_DEVICE_INSTANCE(pInstance->cellHandle));
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the cellular driver.
int32_t uCellInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gUCellPrivateMutex == NULL) {
        // Create the mutex that protects the linked list
        errorCode = uPortMutexCreate(&gUCellPrivateMutex);
    }

    return errorCode;
}

// Shut-down the cellular driver.
void uCellDeinit()
{
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        // Remove all cell instances
        while (gpUCellPrivateInstanceList != NULL) {
            pInstance = gpUCellPrivateInstanceList;
            removeCellInstance(pInstance);
            // Tell the AT client to ignore any asynchronous events from now on
            uAtClientIgnoreAsync(pInstance->atHandle);
            // Free the wake-up callback
            uAtClientSetWakeUpHandler(pInstance->atHandle, NULL, NULL, 0);
            // Free any scan results
            uCellPrivateScanFree(&(pInstance->pScanResults));
            // Free any chip to chip security context
            uCellPrivateC2cRemoveContext(pInstance);
            // Free any location context and associated URC
            uCellPrivateLocRemoveContext(pInstance);
            // Free any sleep context
            uCellPrivateSleepRemoveContext(pInstance);
            // Free any FOTA context
            free(pInstance->pFotaContext);
            // Free any HTTP context
            uCellPrivateHttpRemoveContext(pInstance);
            free(pInstance);
        }

        // Unlock the mutex so that we can delete it
        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
        uPortMutexDelete(gUCellPrivateMutex);
        gUCellPrivateMutex = NULL;
    }
}

// Add a cellular instance.
int32_t uCellAdd(uCellModuleType_t moduleType,
                 uAtClientHandle_t atHandle,
                 int32_t pinEnablePower, int32_t pinPwrOn,
                 int32_t pinVInt, bool leavePowerAlone,
                 uDeviceHandle_t *pCellHandle)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t platformError = 0;
    uCellPrivateInstance_t *pInstance = NULL;
    uPortGpioConfig_t gpioConfig;
    int32_t enablePowerAtStart;
    int32_t pinEnablePowerOnState = (pinEnablePower & U_CELL_PIN_INVERTED) ?
                                    !U_CELL_ENABLE_POWER_PIN_ON_STATE : U_CELL_ENABLE_POWER_PIN_ON_STATE;
    int32_t pinPwrOnPinToggleToState = (pinPwrOn & U_CELL_PIN_INVERTED) ?
                                       !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE : U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE;
    int32_t pinVIntOnState = (pinVInt & U_CELL_PIN_INVERTED) ? !U_CELL_VINT_PIN_ON_STATE :
                             U_CELL_VINT_PIN_ON_STATE;
    uPortGpioDriveMode_t pinPwrOnDriveMode;

    pinEnablePower &= ~U_CELL_PIN_INVERTED;
    pinPwrOn &= ~U_CELL_PIN_INVERTED;
    pinVInt &= ~U_CELL_PIN_INVERTED;

#ifdef U_CELL_PWR_ON_PIN_DRIVE_MODE
    // User override
    pinPwrOnDriveMode = U_CELL_PWR_ON_PIN_DRIVE_MODE;
#else
    // The drive mode is normally open drain so that we
    // can pull PWR_ON low and then let it float
    // afterwards since it is pulled-up by the cellular
    // module
    pinPwrOnDriveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
    if (pinPwrOnPinToggleToState == 1) {
        // If PWR_ON is toggling to 1 then there's an
        // inverter between us and the MCU which only needs
        // normal drive mode.
        pinPwrOnDriveMode = U_PORT_GPIO_DRIVE_MODE_NORMAL;
    }
#endif

    if (gUCellPrivateMutex != NULL) {
        uDeviceInstance_t *pDevInstance;

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        pDevInstance = pUDeviceCreateInstance(U_DEVICE_TYPE_CELL);
        if (pDevInstance != NULL) {

            U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

            // Check parameters
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            if (((size_t) moduleType < gUCellPrivateModuleListSize) &&
                (atHandle != NULL) &&
                (pGetCellInstanceAtHandle(atHandle) == NULL)) {
                handleOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Allocate memory for the instance
                pInstance = (uCellPrivateInstance_t *) malloc(sizeof(uCellPrivateInstance_t));
                if (pInstance != NULL) {
                    handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                    // Fill the values in
                    memset(pInstance, 0, sizeof(*pInstance));
                    // Set the pin states so that we can use them elsewhere
                    if (pinEnablePowerOnState != 0) {
                        pInstance->pinStates |= 1 << U_CELL_PRIVATE_ENABLE_POWER_PIN_BIT_ON_STATE;
                    }
                    if (pinPwrOnPinToggleToState != 0) {
                        pInstance->pinStates |= 1 << U_CELL_PRIVATE_PWR_ON_PIN_BIT_TOGGLE_TO_STATE;
                    }
                    if (pinVIntOnState != 0) {
                        pInstance->pinStates |= 1 << U_CELL_PRIVATE_VINT_PIN_BIT_ON_STATE;
                    }
                    pInstance->cellHandle = (uDeviceHandle_t)pDevInstance;
                    pInstance->atHandle = atHandle;
                    pInstance->pinEnablePower = pinEnablePower;
                    pInstance->pinPwrOn = pinPwrOn;
                    pInstance->pinVInt = pinVInt;
                    pInstance->pinDtrPowerSaving = -1;
                    for (size_t x = 0;
                         x < sizeof(pInstance->networkStatus) / sizeof(pInstance->networkStatus[0]);
                         x++) {
                        pInstance->networkStatus[x] = U_CELL_NET_STATUS_UNKNOWN;
                    }
                    uCellPrivateClearRadioParameters(&(pInstance->radioParameters));
                    pInstance->pModule = &(gUCellPrivateModuleList[moduleType]);
                    pInstance->sockNextLocalPort = -1;

                    // Now set up the pins
                    uPortLog("U_CELL: initialising with enable power pin ");
                    if (pinEnablePower >= 0) {
                        uPortLog("%d (0x%02x) (where %d is on), ", pinEnablePower,
                                 pinEnablePower, pinEnablePowerOnState);
                    } else {
                        uPortLog("not connected, ");
                    }
                    uPortLog("PWR_ON pin ", pinPwrOn, pinPwrOn);
                    if (pinPwrOn >= 0) {
                        uPortLog("%d (0x%02x) (and is toggled from %d to %d)",
                                 pinPwrOn, pinPwrOn,
                                 (int32_t) !pinPwrOnPinToggleToState, pinPwrOnPinToggleToState);
                    } else {
                        uPortLog("not connected");
                    }
                    if (leavePowerAlone) {
                        uPortLog(", leaving the level of both those pins alone");
                    }
                    uPortLog(" and VInt pin ");
                    if (pinVInt >= 0) {
                        uPortLog("%d (0x%02x) (and is %d when module is on).\n",
                                 pinVInt, pinVInt, pinVIntOnState);
                    } else {
                        uPortLog("not connected.\n");
                    }
                    // Sort PWR_ON pin if there is one
                    if (pinPwrOn >= 0) {
                        if (!leavePowerAlone) {
                            // Set PWR_ON to its steady state so that we can pull it
                            // the other way
                            platformError = uPortGpioSet(pinPwrOn,
                                                         (int32_t) !pinPwrOnPinToggleToState);
                        }
                        if (platformError == 0) {
                            U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                            gpioConfig.pin = pinPwrOn;
                            if (pinPwrOnPinToggleToState == 0) {
                                // The u-blox C030-R412M board requires a pull-up here.
                                gpioConfig.pullMode = U_PORT_GPIO_PULL_MODE_PULL_UP;
                            }
                            gpioConfig.driveMode = pinPwrOnDriveMode;
                            gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
                            platformError = uPortGpioConfig(&gpioConfig);
                            if (platformError != 0) {
                                uPortLog("U_CELL: uPortGpioConfig() for PWR_ON pin %d"
                                         " (0x%02x) returned error code %d.\n",
                                         pinPwrOn, pinPwrOn, platformError);
                            }
                        } else {
                            uPortLog("U_CELL: uPortGpioSet() for PWR_ON pin %d (0x%02x)"
                                     " returned error code %d.\n",
                                     pinPwrOn, pinPwrOn, platformError);
                        }
                    }
                    // Sort the enable power pin, if there is one
                    if ((platformError == 0) && (pinEnablePower >= 0)) {
                        U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                        gpioConfig.pin = pinEnablePower;
                        gpioConfig.pullMode = U_PORT_GPIO_PULL_MODE_NONE;
                        // Input/output so we can read it as well
                        gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT_OUTPUT;
                        platformError = uPortGpioConfig(&gpioConfig);
                        if (platformError == 0) {
                            enablePowerAtStart = uPortGpioGet(pinEnablePower);
                            if (!leavePowerAlone) {
                                // Make sure the default is off.
                                enablePowerAtStart = !pinEnablePowerOnState;
                            }
                            platformError = uPortGpioSet(pinEnablePower, enablePowerAtStart);
                            if (platformError != 0) {
                                uPortLog("U_CELL: uPortGpioSet() for enable power pin %d"
                                         " (0x%02x) returned error code %d.\n",
                                         pinEnablePower, pinEnablePower, platformError);
                            }
                        } else {
                            uPortLog("U_CELL: uPortGpioConfig() for enable power pin %d"
                                     " (0x%02x) returned error code %d.\n",
                                     pinEnablePower, pinEnablePower, platformError);
                        }
                    }
                    // Finally, sort the VINT pin if there is one
                    if ((platformError == 0) && (pinVInt >= 0)) {
                        // Set pin that monitors VINT as input
                        U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                        gpioConfig.pin = pinVInt;
                        gpioConfig.direction = U_PORT_GPIO_DIRECTION_INPUT;
                        platformError = uPortGpioConfig(&gpioConfig);
                        if (platformError != 0) {
                            uPortLog("U_CELL: uPortGpioConfig() for VInt pin %d"
                                     " (0x%02x) returned error code %d.\n",
                                     pinVInt, pinVInt, platformError);
                        }
                    }
                    // With that done, set up the AT client for this module
                    if (platformError == 0) {
                        uAtClientTimeoutSet(atHandle,
                                            pInstance->pModule->atTimeoutSeconds * 1000);
                        uAtClientDelaySet(atHandle,
                                          pInstance->pModule->commandDelayMs);
#ifndef U_CFG_CELL_DISABLE_UART_POWER_SAVING
                        // Here we set the power-saving wake-up handler but note
                        // that this might be _removed_ during the power-on
                        // process if it turns out that the configuration of
                        // flow control lines is such that such power saving
                        // cannot be supported
                        uAtClientSetWakeUpHandler(atHandle, uCellPrivateWakeUpCallback, pInstance,
                                                  (U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS * 1000) -
                                                  U_CELL_POWER_SAVING_UART_WAKEUP_MARGIN_MILLISECONDS);
#endif
                        // ...and finally add it to the list
                        addCellInstance(pInstance);
                        handleOrErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        *pCellHandle = pInstance->cellHandle;
                    } else {
                        // If we hit a platform error, free memory again
                        free(pInstance);
                    }
                }
            }
            if (handleOrErrorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                // Don't forget to deallocate device instance on failure
                uDeviceDestroyInstance(pDevInstance);
            }

            U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
        }
    }

    return (int32_t) handleOrErrorCode;
}

// Remove a cellular instance.
void uCellRemove(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            removeCellInstance(pInstance);
            // Tell the AT client to ignore any asynchronous events from now on
            uAtClientIgnoreAsync(pInstance->atHandle);
            // Free the wake-up callback
            uAtClientSetWakeUpHandler(pInstance->atHandle, NULL, NULL, 0);
            // Free any scan results
            uCellPrivateScanFree(&(pInstance->pScanResults));
            // Free any chip to chip security context
            uCellPrivateC2cRemoveContext(pInstance);
            // Free any location context and associated URC
            uCellPrivateLocRemoveContext(pInstance);
            // Free any sleep context
            uCellPrivateSleepRemoveContext(pInstance);
            free(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

// Get the handle of the AT client.
int32_t uCellAtClientHandleGet(uDeviceHandle_t cellHandle,
                               uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pAtHandle != NULL)) {
            *pAtHandle = pInstance->atHandle;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
