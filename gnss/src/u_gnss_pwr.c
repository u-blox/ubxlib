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
 * @brief Implementation of the power API for GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"  // Required by u_gnss_private.h
#include "u_port_gpio.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_pwr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_PWR_IS_ALIVE_TIMEOUT_MS
/** The timeout to use for an "is alive" check in milliseconds.
 */
# define U_GNSS_PWR_IS_ALIVE_TIMEOUT_MS 2500
#endif

#ifndef U_GNSS_PWR_AIDING_TYPES
/** The aiding types to request when switching-on a GNSS
 * chip (all of them).
 */
#define U_GNSS_PWR_AIDING_TYPES 15
#endif

#ifndef U_GNSS_PWR_SYSTEM_TYPES
/** The system types to request when switching-on a GNSS
 * chip (all of them).
 */
#define U_GNSS_PWR_SYSTEM_TYPES 0x7f
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Power a GNSS chip on.
int32_t uGnssPwrOn(uDeviceHandle_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    // Message buffer for the 120-byte UBX-MON-MSGPP message
    char message[120] = {0};
    uint64_t y;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pinGnssEnablePower >= 0) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (uPortGpioSet(pInstance->pinGnssEnablePower,
                                 pInstance->pinGnssEnablePowerOnState) == 0) {
                    // Wait a moment for the device to power up.
                    uPortTaskBlock(U_GNSS_POWER_UP_TIME_SECONDS * 1000);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }

            if (errorCode == 0) {
                if (pInstance->transportType == U_GNSS_TRANSPORT_AT) {
                    atHandle = (uAtClientHandle_t) pInstance->transportHandle.pAt;
                    // Switch on an indication which is useful when debugging
                    // aiding modes
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UGIND=");
                    uAtClientWriteInt(atHandle, 1);
                    uAtClientCommandStopReadResponse(atHandle);
                    uAtClientUnlock(atHandle);
                    // On some modules, e.g. SARA-R5, an attempt to change
                    // the pin that controls the GNSS chip power will return
                    // an error if the GNSS chip is already powered and also
                    // an attempt to _turn_ the GNSS chip on will return an
                    // error if the cellular module is currently talking to the
                    // GNSS chip.  Hence we check if the GNSS chip is already
                    // on here.
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UGPS?");
                    // Response is +UGPS: <mode>[,<aid_mode>[,<GNSS_systems>]]
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UGPS:");
                    y = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    uAtClientUnlock(atHandle);
                    if (y != 1) {
                        // If the first parameter is not 1, try to
                        // configure the cellular module's GPIO pins and
                        // switch GNSS on
                        if (!uGnssPrivateIsInsideCell(pInstance)) {
                            // First, if the GNSS module is not inside
                            // the cellular module, configure the GPIOs
                            if (pInstance->atModulePinPwr >= 0) {
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UGPIOC=");
                                uAtClientWriteInt(atHandle, pInstance->atModulePinPwr);
                                // 3 is external GNSS supply enable mode
                                uAtClientWriteInt(atHandle, 3);
                                uAtClientCommandStopReadResponse(atHandle);
                                errorCode = uAtClientUnlock(atHandle);
                            }
                            if ((errorCode == 0) && (pInstance->atModulePinDataReady >= 0)) {
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UGPIOC=");
                                uAtClientWriteInt(atHandle, pInstance->atModulePinDataReady);
                                // 4 is external GNSS data ready mode
                                uAtClientWriteInt(atHandle, 4);
                                uAtClientCommandStopReadResponse(atHandle);
                                errorCode = uAtClientUnlock(atHandle);
                            }
                        }
                        if (errorCode == 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                            for (size_t x = 0; (errorCode < 0) &&
                                 (x < U_GNSS_AT_POWER_ON_RETRIES + 1); x++) {
                                // Now ask the cellular module to switch GNSS on
                                uPortTaskBlock(U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS);
                                uAtClientLock(atHandle);
                                uAtClientTimeoutSet(atHandle, U_GNSS_AT_POWER_UP_TIME_SECONDS * 1000);
                                uAtClientCommandStart(atHandle, "AT+UGPS=");
                                uAtClientWriteInt(atHandle, 1);
                                // If you change the aiding types and
                                // GNSS system types below you may wish
                                // to change them in u_cell_loc.c also.
                                // All aiding types allowed
                                uAtClientWriteInt(atHandle, U_GNSS_PWR_AIDING_TYPES);
                                // All GNSS system types enabled
                                uAtClientWriteInt(atHandle, U_GNSS_PWR_SYSTEM_TYPES);
                                uAtClientCommandStopReadResponse(atHandle);
                                errorCode = uAtClientUnlock(atHandle);
                                if (errorCode < 0) {
                                    uPortTaskBlock(U_GNSS_AT_POWER_ON_RETRY_INTERVAL_SECONDS * 1000);
                                }
                            }
                        }
                    }
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                    // Make sure GNSS is on with UBX-CFG-RST
                    // The message is not acknowledged, so must use
                    // uGnssPrivateSendOnlyCheckStreamUbxMessage()
                    message[2] = 0x09; // Controlled GNSS hot start
                    if (uGnssPrivateSendOnlyCheckStreamUbxMessage(pInstance,
                                                                  0x06, 0x04,
                                                                  message, 4) > 0) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        if (pInstance->pModule->moduleType == U_GNSS_MODULE_TYPE_M8) {
                            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                            // From the M8 receiver description, a HW reset is also
                            // required at this point if Galileo is enabled,
                            // so find out if it is by polling UBX-MON-GNSS
                            if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                  0x0a, 0x28,
                                                                  NULL, 0,
                                                                  message, 8) == 8) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                // Byte 3 is the enabled flags and bit 3 of that is Galileo
                                if (message[3] & 0x08) {
                                    // Setting the message to all zeroes effects a HW reset
                                    memset(message, 0, sizeof(message));
                                    // Nothing we can do here to check that the message
                                    // has been accepted as the reset removes all evidence
                                    errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                                  0x06, 0x04,
                                                                                  message, 4,
                                                                                  NULL, 0);
                                    if (errorCode == 0) {
                                        // Wait for the reset to complete
                                        uPortTaskBlock(U_GNSS_RESET_TIME_SECONDS * 1000);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if ((errorCode == 0) &&
                ((pInstance->transportType == U_GNSS_TRANSPORT_UBX_UART) ||
                 (pInstance->transportType == U_GNSS_TRANSPORT_UBX_I2C))) {
                errorCode = uGnssPrivateSetProtocolOut(pInstance, U_GNSS_PROTOCOL_NMEA, false);
            }

            if ((errorCode < 0) && (pInstance->pinGnssEnablePower >= 0)) {
                // If we were unable to send all the relevant commands and
                // there is a power enable then switch it off again so that
                // we're not left in a strange state
                uPortGpioSet(pInstance->pinGnssEnablePower,
                             (int32_t) !pInstance->pinGnssEnablePowerOnState);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Check that a GNSS chip is powered on.
bool uGnssPwrIsAlive(uDeviceHandle_t gnssHandle)
{
    bool isAlive = false;
    uGnssPrivateInstance_t *pInstance;
    int32_t timeoutMs;
    // Message buffer for a UBX-CFG-ANT response
    // (antenna settings), chosen just because it
    // is nice and short
    char message[4] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Set a short timeout for this
            timeoutMs = pInstance->timeoutMs;
            pInstance->timeoutMs = U_GNSS_PWR_IS_ALIVE_TIMEOUT_MS;
            // UBX-CFG-ANT (0x06 0x13)
            if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                  0x06, 0x13,
                                                  NULL, 0, message,
                                                  sizeof(message)) == sizeof(message)) {
                // Don't care what the answer is; if we get
                // one then the GNSS chip is alive
                isAlive = true;
            }
            pInstance->timeoutMs = timeoutMs;
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return isAlive;
}

// Power a GNSS chip off.
int32_t uGnssPwrOff(uDeviceHandle_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    // The body of a UBX-CFG-RST message
    char message[4] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (pInstance->transportType == U_GNSS_TRANSPORT_AT) {
                // For the AT interface, need to ask the cellular module
                // to power the GNSS module down
                atHandle = (uAtClientHandle_t) pInstance->transportHandle.pAt;
                uPortTaskBlock(U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS);
                uAtClientLock(atHandle);
                // Can take a little while if the cellular module is
                // busy talking to the GNSS module at the time
                uAtClientTimeoutSet(atHandle, U_GNSS_AT_POWER_DOWN_TIME_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+UGPS=");
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // Make sure GNSS is off with UBX-CFG-RST
                // This message is not acknowledged, so we use
                // uGnssPrivateSendOnlyCheckStreamUbxMessage()
                message[2] = 0x08; // Controlled GNSS stop
                if (uGnssPrivateSendOnlyCheckStreamUbxMessage(pInstance,
                                                              0x06, 0x04,
                                                              message, 4) > 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }

            if (pInstance->pinGnssEnablePower >= 0) {
                // Let this overwrite any other errors
                errorCode = uPortGpioSet(pInstance->pinGnssEnablePower,
                                         (int32_t) !pInstance->pinGnssEnablePowerOnState);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Power a GNSS chip off and put it into back-up mode.
int32_t uGnssPwrOffBackup(uDeviceHandle_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // The body of a UBX-RXM-PMREQ message
    char message[16] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (pInstance->transportType != U_GNSS_TRANSPORT_AT) {
                // Put the GNSS chip into backup mode with UBX-RXM-PMREQ
                // This message is not acknowledged and fiddling with the
                // GNSS chip after this will wake it up again, so we just
                // use uGnssPrivateSendReceiveUbxMessage() with an
                // empty response buffer
                message[8] = 0x02; // Backup
                //lint -save -e569 Suppress loss of information: OK on all our compilers
                message[12] = 0xe4; // Wake-up on all sources
                //lint -restore
                errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                              0x02, 0x41,
                                                              message,
                                                              sizeof(message),
                                                              NULL, 0);
                if ((errorCode == 0) && (pInstance->pinGnssEnablePower >= 0)) {
                    errorCode = uPortGpioSet(pInstance->pinGnssEnablePower,
                                             (int32_t) !pInstance->pinGnssEnablePowerOnState);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// End of file
