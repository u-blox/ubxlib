/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the power (both on/off and power
 * saving) API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_pwr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The number of times to poke the module to confirm that
 * she's powered-on.
 */
#define U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON 10

/** The UART power saving duration in GSM frames, needed for the
 * UART power saving AT command.
 */
#define U_CELL_PWR_UART_POWER_SAVING_GSM_FRAMES ((U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS * 1000000) / 4615)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table of AT commands to send to all cellular module types
 * during configuration.
 */
static const char *const gpConfigCommand[] = {"ATE0",      // Echo off
#ifdef U_CFG_CELL_ENABLE_NUMERIC_ERROR
// With this compilation flag defined numeric errors will be
// returned and so uAtClientDeviceErrorGet() will be able
// to return a non-zero value for deviceError.code.
// IMPORTANT: this switch is simply for customer convenience,
// no ubxlib code should set it or depend on the value
// of deviceError.code.
                                              "AT+CMEE=1", // Extended errors on, numeric format
#else
// The normal case: errors are reported by the module as
// verbose text, most useful when debugging normally with
// AT interface prints shown, uAtClientPrintAtSet() set
// to true.
                                              "AT+CMEE=2", // Extended errors on, verbose/text format
#endif
#ifdef U_CFG_1V8_SIM_WORKAROUND
// This can be used to tell a SARA-R422 module that a 1.8V
// SIM which does NOT include 1.8V in its answer-to-reset
// really is a good 1.8V SIM.
                                              "AT+UDCONF=92,1,1",
#endif
                                              "ATI9",      // Firmware version
                                              "AT&C1",     // DCD circuit (109) changes with the carrier
                                              "AT&D0"      // Ignore changes to DTR
                                             };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Check that the cellular module is alive.
static int32_t moduleIsAlive(const uCellPrivateInstance_t *pInstance,
                             int32_t attempts)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_RESPONDING;
    uAtClientDeviceError_t deviceError;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool isAlive = false;

    // It may be that we have been called when an AT client
    // has just been instantiated (so it has no knowledge of
    // previous transmit events against which to measure an
    // inactivity time-out) and yet the module is already
    // powered-on but is in UART power saving mode; call the
    // wake-up call-back here to handle that case
    if (uCellPrivateUartWakeUpCallback(atHandle, NULL) == 0) {
        // If it responds at this point then it must be alive,
        // job done
        isAlive = true;
    } else {
        // See if the cellular module is responding at the AT interface
        // by poking it with "AT" up to "attempts" times.
        // The response can be "OK" or it can also be "CMS/CMS ERROR"
        // if the modem happened to be awake and in the middle
        // of something from a previous command.
        for (int32_t x = 0; !isAlive && (x < attempts); x++) {
            uAtClientLock(atHandle);
            uAtClientTimeoutSet(atHandle,
                                pInstance->pModule->responseMaxWaitMs);
            uAtClientCommandStart(atHandle, "AT");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            isAlive = (uAtClientUnlock(atHandle) == 0) ||
                      (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
        }
    }

    if (isAlive) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Configure one item in the cellular module.
static bool moduleConfigureOne(uAtClientHandle_t atHandle,
                               const char *pAtString)
{
    bool success = false;
    // I have seen modules return "ERROR" to some AT
    // commands during startup so try a few times
    for (size_t x = 3; (x > 0) && !success; x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, pAtString);
        uAtClientCommandStopReadResponse(atHandle);
        success = (uAtClientUnlock(atHandle) == 0);
    }

    return success;
}

// Configure the cellular module.
static int32_t moduleConfigure(uCellPrivateInstance_t *pInstance,
                               bool andRadioOff)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_NOT_CONFIGURED;
    bool success = true;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t atStreamHandle;
    int32_t uartPowerSavingMode = 0; // Assume no UART power saving
    uAtClientStream_t atStreamType;
    char buffer[20]; // Enough room for AT+UPSV=2,1300

    // First send all the commands that everyone gets
    for (size_t x = 0;
         (x < sizeof(gpConfigCommand) / sizeof(gpConfigCommand[0])) &&
         success; x++) {
        success = moduleConfigureOne(atHandle, gpConfigCommand[x]);
    }

    if (success &&
        U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
        // SARA-R4 only: switch on the right UCGED mode
        // (SARA-R5 and SARA-U201 have a single mode and require no setting)
        if (U_CELL_PRIVATE_HAS(pInstance->pModule, U_CELL_PRIVATE_FEATURE_UCGED5)) {
            success = moduleConfigureOne(atHandle, "AT+UCGED=5");
        } else {
            success = moduleConfigureOne(atHandle, "AT+UCGED=2");
        }
    }

    if (success &&
        U_CELL_PRIVATE_MODULE_HAS_3GPP_POWER_SAVING(pInstance->pModule->moduleType)) {
        // (TODO) switch off power saving until it is integrated into this API
        success = moduleConfigureOne(atHandle, "AT+CPSMS=0");
    }

    atStreamHandle = uAtClientStreamGet(atHandle, &atStreamType);
    if (success && (atStreamType == U_AT_CLIENT_STREAM_TYPE_UART)) {
        // Get the UART stream handle and set the flow
        // control and power saving mode correctly for it
        // TODO: check if AT&K3 requires both directions
        // of flow control to be on or just one of them
        if (uPortUartIsRtsFlowControlEnabled(atStreamHandle) &&
            uPortUartIsCtsFlowControlEnabled(atStreamHandle)) {
            success = moduleConfigureOne(atHandle, "AT&K3");
            // The RTS/CTS handshaking lines are being used
            // for flow control by the UART HW so we can't use
            // them for power control purposes.  Also,
            // we can't use the wake-up on TX line feature
            // since, when the module has gone to sleep,
            // the module incoming flow control line (CTS) will
            // float to "off" and this MCU's UART HW will not be
            // able to send anything (but see exception for SARA-R4
            // below).
        } else {
            success = moduleConfigureOne(atHandle, "AT&K0");
            // RTS/CTS handshaking is not used by the UART HW, we
            // can use the wake-up on TX line feature.
            // TODO: we _could_, on non-SARA-R4 modules (see below),
            // use the RTS line as the flow control signaller but this
            // is complicated as we would need to add an API to tell
            // this cellular handler about the line and then the
            // wake-up handler would need to take control of the
            // RTS line as a GPIO and set it to 0 ("on") sufficiently
            // far in advance of [and during] AT communication with
            // the module. And, of course, the application would have
            // to NOT try to use UART flow control as the RTS pin is
            // no longer available to the UART HW: no UART HW would
            // set the RTS pin to 0 ("on") sufficiently far in advance
            // of the actual transmission of data for the module to
            // be able to wake up and receive that TXD data.
            if (uAtClientWakeUpHandlerIsSet(atHandle)) {
                uartPowerSavingMode = 1;
            }
        }

        if (uAtClientWakeUpHandlerIsSet(atHandle) &&
            U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
            // SARA-R4 doesn't support modes 1 or 2 but
            // does support the functionality of mode 1
            // though numbered as mode 4 and without the
            // timeout parameter (the timeout is fixed at
            // 6 seconds) *and* this works even if the flow
            // control lines are connected to a sleeping
            // module: it would appear the module incoming
            // flow control line (CTS) is held low ("on") even
            // while the module is asleep in the SARA-R4 case.
            uartPowerSavingMode = 4;
        }
    }

    if (success) {
        // Set the UART power saving mode
        if (uartPowerSavingMode == 1) {
            snprintf(buffer, sizeof(buffer), "AT+UPSV=%d,%d",
                     (int) uartPowerSavingMode,
                     U_CELL_PWR_UART_POWER_SAVING_GSM_FRAMES);
        } else {
            snprintf(buffer, sizeof(buffer), "AT+UPSV=%d", (int) uartPowerSavingMode);
            if ((uartPowerSavingMode == 0) && uAtClientWakeUpHandlerIsSet(atHandle)) {
                // Remove the wake-up handler if it turns out that power
                // saving cannot be supported
                uAtClientSetWakeUpHandler(atHandle, NULL, NULL, 0);
            }
        }
        success = moduleConfigureOne(atHandle, buffer);
    }

    if (success) {
        if (andRadioOff) {
            // switch the radio off until commanded to connect
            // Wait for flip time to expire
            while (uPortGetTickTimeMs() < pInstance->lastCfunFlipTimeMs +
                   (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
                uPortTaskBlock(1000);
            }
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CFUN=");
            uAtClientWriteInt(atHandle,
                              pInstance->pModule->radioOffCfun);
            uAtClientCommandStopReadResponse(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        } else {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Wait for power off to complete
static void waitForPowerOff(uCellPrivateInstance_t *pInstance,
                            bool (*pKeepGoingCallback) (int32_t))
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool moduleIsOff = false;
    int64_t endTimeMs = uPortGetTickTimeMs() +
                        (((int64_t) pInstance->pModule->powerDownWaitSeconds) * 1000);

    while (!moduleIsOff && (uPortGetTickTimeMs() < endTimeMs) &&
           ((pKeepGoingCallback == NULL) || pKeepGoingCallback(pInstance->handle))) {
        if (pInstance->pinVInt >= 0) {
            // If we have a VInt pin then wait until that
            // goes to the off state
            moduleIsOff = (uPortGpioGet(pInstance->pinVInt) == (int32_t) !U_CELL_VINT_PIN_ON_STATE);
        } else {
            // Wait for the module to stop responding at the AT interface
            // by poking it with "AT"
            uAtClientLock(atHandle);
            uAtClientTimeoutSet(atHandle,
                                pInstance->pModule->responseMaxWaitMs);
            uAtClientCommandStart(atHandle, "AT");
            uAtClientCommandStopReadResponse(atHandle);
            moduleIsOff = (uAtClientUnlock(atHandle) != 0);
        }
        // Relax a bit
        uPortTaskBlock(1000);
    }

    // We have rebooted
    if (moduleIsOff) {
        pInstance->rebootIsRequired = false;
    }
}

// Power the cellular module off.
// Note: gUCellPrivateMutex must be locked before this is called
static int32_t powerOff(uCellPrivateInstance_t *pInstance,
                        bool (*pKeepGoingCallback) (int32_t))
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uPortLog("U_CELL_PWR: powering off with AT command.\n");
    // Send the power off command and then pull the power
    // No error checking, we're going dowwwwwn...
    uAtClientLock(atHandle);
    // Clear the dynamic parameters
    uCellPrivateClearDynamicParameters(pInstance);
    uAtClientTimeoutSet(atHandle,
                        U_CELL_PRIVATE_CPWROFF_WAIT_TIME_SECONDS * 1000);
    // Switch off UART power saving first, as it seems to
    // affect the power off process
    uAtClientCommandStart(atHandle, "AT+UPSV=0");
    uAtClientCommandStopReadResponse(atHandle);
    uAtClientCommandStart(atHandle, "AT+CPWROFF");
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    // Wait for the module to power down
    waitForPowerOff(pInstance, pKeepGoingCallback);
    // Now switch off power if possible
    if (pInstance->pinEnablePower >= 0) {
        uPortGpioSet(pInstance->pinEnablePower,
                     (int32_t) !U_CELL_ENABLE_POWER_PIN_ON_STATE);
    }
    if (pInstance->pinPwrOn >= 0) {
        uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
    }

    // Remove any security context as these disappear
    // at power off
    uCellPrivateC2cRemoveContext(pInstance);

    return errorCode;
}

// Do a quick power off, used for recovery situations only.
// IMPORTANT: this won't work if a SIM PIN needs
// to be entered at a power cycle
// Note: gUCellPrivateMutex must be locked before this is called
static void quickPowerOff(uCellPrivateInstance_t *pInstance,
                          bool (*pKeepGoingCallback) (int32_t))
{
    if (pInstance->pinPwrOn >= 0) {
        // Power off the module by pulling the PWR_ON pin
        // low for the correct number of milliseconds
        uPortGpioSet(pInstance->pinPwrOn, U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
        uPortTaskBlock(pInstance->pModule->powerOffPullMs);
        uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
        // Wait for the module to power down
        waitForPowerOff(pInstance, pKeepGoingCallback);
        // Now switch off power if possible
        if (pInstance->pinEnablePower > 0) {
            uPortGpioSet(pInstance->pinEnablePower,
                         (int32_t) !U_CELL_ENABLE_POWER_PIN_ON_STATE);
        }
        // Remove any security context as these disappear
        // at power off
        uCellPrivateC2cRemoveContext(pInstance);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Determine if the cellular module has power.
bool uCellPwrIsPowered(int32_t cellHandle)
{
    bool isPowered = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isPowered = true;
            if (pInstance->pinEnablePower >= 0) {
                isPowered = (uPortGpioGet(pInstance->pinEnablePower) == U_CELL_ENABLE_POWER_PIN_ON_STATE);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);

    }

    return isPowered;
}

// Determine if the module is responsive.
bool uCellPwrIsAlive(int32_t cellHandle)
{
    bool isAlive = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isAlive = (moduleIsAlive(pInstance, 1) == 0);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isAlive;
}

// Power the cellular module on.
int32_t uCellPwrOn(int32_t cellHandle, const char *pPin,
                   bool (*pKeepGoingCallback) (int32_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t platformError = 0;
    int32_t enablePowerAtStart = 1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_CELL_ERROR_PIN_ENTRY_NOT_SUPPORTED;
            if (pInstance->pinEnablePower >= 0) {
                enablePowerAtStart = uPortGpioGet(pInstance->pinEnablePower);
            }
            if (pPin == NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // For some modules the power-on pulse on PWR_ON and the
                // power-off pulse on PWR_ON are the same duration,
                // in effect a toggle.  To avoid accidentally powering
                // the module off, check if it is already on.
                // Note: doing this even if there is an enable power
                // pin for safety sake
                if (((pInstance->pinVInt >= 0) &&
                     (uPortGpioGet(pInstance->pinVInt) == U_CELL_VINT_PIN_ON_STATE)) ||
                    ((pInstance->pinVInt < 0) &&
                     (moduleIsAlive(pInstance, 1) == 0))) {
                    uPortLog("U_CELL_PWR: powering on, module is already on.\n");
                    // Configure the module.  Since it was already
                    // powered on we might have been called from
                    // a state where everything was already fine
                    // and dandy so only switch the radio off at
                    // the end of configuration if we are not
                    // already registered
                    errorCode = moduleConfigure(pInstance,
                                                !uCellPrivateIsRegistered(pInstance));
                    if (errorCode != 0) {
                        // I have seen situations where the module responds
                        // initially and then fails configuration.  If that is
                        // the case then make sure it's definitely off before
                        // we go any further
                        quickPowerOff(pInstance, pKeepGoingCallback);
                    }
                }
                // Two goes at this, 'cos I've seen some module types
                // fail during initial configuration.
                for (size_t x = 2; (x > 0) && (errorCode != 0) && (platformError == 0) &&
                     ((pKeepGoingCallback == NULL) || pKeepGoingCallback(cellHandle)); x--) {
                    uPortLog("U_CELL_PWR: powering on.\n");
                    // First, switch on the volts
                    if (pInstance->pinEnablePower >= 0) {
                        platformError = uPortGpioSet(pInstance->pinEnablePower,
                                                     (int32_t) U_CELL_ENABLE_POWER_PIN_ON_STATE);
                    }
                    if (platformError == 0) {
                        // Wait for things to settle
                        uPortTaskBlock(100);

                        if (pInstance->pinPwrOn >= 0) {
                            // Power the module on by holding the PWR_ON pin in
                            // the relevant state for the correct number of milliseconds
                            platformError = uPortGpioSet(pInstance->pinPwrOn,
                                                         U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                            if (platformError == 0) {
                                uPortTaskBlock(pInstance->pModule->powerOnPullMs);
                                // Not bothering with checking return code here
                                // as it would have barfed on the last one if
                                // it were going to
                                uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                                uPortTaskBlock(pInstance->pModule->bootWaitSeconds * 1000);
                                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                                    // SARA-R5 chucks out a load of stuff after
                                    // boot at the moment: flush it away
                                    // TODO: do we still need this?
                                    uAtClientFlush(pInstance->atHandle);
                                }
                            } else {
                                uPortLog("U_CELL_PWR: uPortGpioSet() for PWR_ON"
                                         " pin %d returned error code %d.\n",
                                         pInstance->pinPwrOn, platformError);
                            }
                        }
                        // Cellular module should be up, see if it's there
                        // and, if so, configure it
                        for (size_t y = U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON;
                             (y > 0) && (errorCode != 0) &&
                             ((pKeepGoingCallback == NULL) || pKeepGoingCallback(cellHandle));
                             y--) {
                            errorCode = moduleIsAlive(pInstance, 1);
                        }
                        if (errorCode == 0) {
                            // Configure the module
                            errorCode = moduleConfigure(pInstance, true);
                            if (errorCode != 0) {
                                // If the module fails configuration, power it
                                // off and try again
                                quickPowerOff(pInstance, pKeepGoingCallback);
                            }
                        }
                    } else {
                        uPortLog("U_CELL_PWR: uPortGpioSet() for enable power"
                                 " pin %d returned error code%d.\n",
                                 pInstance->pinEnablePower, platformError);
                    }
                }

                // If we were off at the start and power-on was
                // unsuccessful then go back to that state
                if ((errorCode != 0) && (enablePowerAtStart == 0)) {
                    quickPowerOff(pInstance, pKeepGoingCallback);
                }
            } else {
                uPortLog("U_CELL_PWR: a SIM PIN has been set but PIN entry is"
                         " not supported I'm afraid.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Power the cellular module off.
int32_t uCellPwrOff(int32_t cellHandle,
                    bool (*pKeepGoingCallback) (int32_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = powerOff(pInstance, pKeepGoingCallback);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);

    }

    return errorCode;
}

// Remove power to the cellular module using HW lines.
int32_t uCellPwrOffHard(int32_t cellHandle, bool trulyHard,
                        bool (*pKeepGoingCallback) (int32_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            errorCode = (int32_t) U_CELL_ERROR_NOT_CONFIGURED;
            // If we have control of power and the user
            // wants a truly hard power off then just do it.
            if (trulyHard && (pInstance->pinEnablePower > 0)) {
                uPortLog("U_CELL_PWR: powering off by pulling the power.\n");
                uPortGpioSet(pInstance->pinEnablePower,
                             (int32_t) !U_CELL_ENABLE_POWER_PIN_ON_STATE);
                // Remove any security context as these disappear
                // at power off
                uCellPrivateC2cRemoveContext(pInstance);
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                if (pInstance->pinPwrOn >= 0) {
                    // Switch off UART power saving first, as it seems to
                    // affect the power off process, no error checking,
                    // we're going down anyway
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UPSV=0");
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientUnlock(atHandle);
                    uPortLog("U_CELL_PWR: powering off using the PWR_ON pin.\n");
                    uPortGpioSet(pInstance->pinPwrOn, U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                    // Power off the module by pulling the PWR_ON pin
                    // to the relevant state for the correct number of
                    // milliseconds
                    uPortTaskBlock(pInstance->pModule->powerOffPullMs);
                    uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                    // Clear the dynamic parameters
                    uCellPrivateClearDynamicParameters(pInstance);
                    // Wait for the module to power down
                    waitForPowerOff(pInstance, pKeepGoingCallback);
                    // Now switch off power if possible
                    if (pInstance->pinEnablePower > 0) {
                        uPortGpioSet(pInstance->pinEnablePower,
                                     (int32_t) !U_CELL_ENABLE_POWER_PIN_ON_STATE);
                    }
                    // Remove any security context as these disappear
                    // at power off
                    uCellPrivateC2cRemoveContext(pInstance);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Determine if the cellular module needs to be
// rebooted.
bool uCellPwrRebootIsRequired(int32_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    bool rebootIsRequired = false;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            rebootIsRequired = pInstance->rebootIsRequired;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return rebootIsRequired;
}


// Re-boot the cellular module.
int32_t uCellPwrReboot(int32_t cellHandle,
                       bool (*pKeepGoingCallback) (int32_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    bool success = false;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uPortLog("U_CELL_PWR: rebooting.\n");
            // Wait for flip time to expire
            while (uPortGetTickTimeMs() < pInstance->lastCfunFlipTimeMs +
                   (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
                uPortTaskBlock(1000);
            }
            uAtClientLock(atHandle);
            uAtClientTimeoutSet(atHandle,
                                U_CELL_PRIVATE_AT_CFUN_OFF_RESPONSE_TIME_SECONDS * 1000);
            // Clear the dynamic parameters
            uCellPrivateClearDynamicParameters(pInstance);
            uAtClientCommandStart(atHandle, "AT+CFUN=");
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                // SARA-R5 doesn't support 15 (which doesn't reset the SIM)
                uAtClientWriteInt(atHandle, 16);
            } else {
                uAtClientWriteInt(atHandle, 15);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                // Remove any security context as these disappear
                // at reboot
                uCellPrivateC2cRemoveContext(pInstance);
                // We have rebooted
                pInstance->rebootIsRequired = false;
                // Wait for the module to boot
                uPortTaskBlock(pInstance->pModule->rebootCommandWaitSeconds * 1000);
                // Two goes at this with a power-off inbetween,
                // 'cos I've seen some modules
                // fail during initial configuration.
                // IMPORTANT: this won't work if a SIM PIN needs
                // to be entered at a power cycle
                for (size_t x = 2; (x > 0) && (!success) &&
                     ((pKeepGoingCallback == NULL) || pKeepGoingCallback(cellHandle)); x--) {
                    if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                        // SARA-R5 chucks out a load of stuff after
                        // boot at the moment: flush it away
                        // TODO: do we still need this?
                        uAtClientFlush(atHandle);
                    }
                    // Wait for the module to return to life
                    // and configure it
                    errorCode = moduleIsAlive(pInstance,
                                              U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON);
                    if (errorCode == 0) {
                        // Configure the module
                        errorCode = moduleConfigure(pInstance, true);
                    }
                    if (errorCode == 0) {
                        success = true;
                    } else {
                        // If the module has failed to come up or
                        // configure after the reboot, power it
                        // off and on again to recover, if we can
                        // Note: ignore return values here as, if
                        // there were going to be any GPIO configuration
                        // errors, they would have already occurred
                        // during power on
                        if (pInstance->pinPwrOn >= 0) {
                            // Power off the module by pulling the PWR_ON pin
                            // to the relevant state for the correct number of
                            // milliseconds
                            uPortGpioSet(pInstance->pinPwrOn, U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                            uPortTaskBlock(pInstance->pModule->powerOffPullMs);
                            uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                            // Wait for the module to power down
                            waitForPowerOff(pInstance, pKeepGoingCallback);
                            // Now switch off power if possible
                            if (pInstance->pinEnablePower > 0) {
                                uPortGpioSet(pInstance->pinEnablePower,
                                             (int32_t) !U_CELL_ENABLE_POWER_PIN_ON_STATE);
                                // Wait for things to settle
                                uPortTaskBlock(5000);
                            }
                        }
                        // Now power back on again
                        if (pInstance->pinEnablePower >= 0) {
                            uPortGpioSet(pInstance->pinEnablePower,
                                         (int32_t) U_CELL_ENABLE_POWER_PIN_ON_STATE);
                            // Wait for things to settle
                            uPortTaskBlock(100);
                        }
                        if (pInstance->pinPwrOn >= 0) {
                            uPortGpioSet(pInstance->pinPwrOn, U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                            uPortTaskBlock(pInstance->pModule->powerOnPullMs);
                            uPortGpioSet(pInstance->pinPwrOn, (int32_t) !U_CELL_PWR_ON_PIN_TOGGLE_TO_STATE);
                            uPortTaskBlock(pInstance->pModule->bootWaitSeconds * 1000);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Perform a hard reset of the cellular module.
int32_t uCellPwrResetHard(int32_t cellHandle, int32_t pinReset)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t platformError;
    uPortGpioConfig_t gpioConfig;
    int64_t startTime;
    int32_t resetHoldMilliseconds;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pinReset >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            resetHoldMilliseconds = pInstance->pModule->resetHoldMilliseconds;
            uPortLog("U_CELL_PWR: performing hard reset, this will take"
                     " at least %d milliseconds...\n", resetHoldMilliseconds +
                     (pInstance->pModule->rebootCommandWaitSeconds * 1000));
            // Set the RESET pin to the "reset" state
            platformError = uPortGpioSet(pinReset,
                                         (int32_t) U_CELL_RESET_PIN_TOGGLE_TO_STATE);
            if (platformError == 0) {
                // Configure the GPIO to go to this state
                U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                gpioConfig.pin = pinReset;
                gpioConfig.driveMode = U_CELL_RESET_PIN_DRIVE_MODE;
                gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
                platformError = uPortGpioConfig(&gpioConfig);
                if (platformError == 0) {
                    // Remove any security context as these disappear
                    // at reboot
                    uCellPrivateC2cRemoveContext(pInstance);
                    // We have rebooted
                    pInstance->rebootIsRequired = false;
                    startTime = uPortGetTickTimeMs();
                    while (uPortGetTickTimeMs() < startTime + resetHoldMilliseconds) {
                        uPortTaskBlock(100);
                    }
                    // Set the pin back to the "non RESET" state
                    // Note: not checking for errors here, it would have
                    // barfed above if there were a problem and there's
                    // nothing we can do about it anyway
                    uPortGpioSet(pinReset, (int32_t) !U_CELL_RESET_PIN_TOGGLE_TO_STATE);
                    // Wait for the module to boot
                    uPortTaskBlock(pInstance->pModule->rebootCommandWaitSeconds * 1000);
                    if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                        // SARA-R5 chucks out a load of stuff after
                        // boot at the moment: flush it away
                        // TODO: do we still need this?
                        uAtClientFlush(pInstance->atHandle);
                    }
                    // Wait for the module to return to life
                    // and configure it
                    pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
                    errorCode = moduleIsAlive(pInstance,
                                              U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON);
                    if (errorCode == 0) {
                        // Configure the module
                        errorCode = moduleConfigure(pInstance, true);
                    }
                } else {
                    uPortLog("U_CELL_PWR: uPortGpioConfig() for RESET pin %d"
                             " (0x%02x) returned error code %d.\n",
                             pinReset, pinReset, platformError);
                }
            } else {
                uPortLog("U_CELL_PWR: uPortGpioSet() for RESET pin %d (0x%02x)"
                         " returned error code %d.\n",
                         pinReset, pinReset, platformError);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
