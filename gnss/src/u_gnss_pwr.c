/*
 * Copyright 2019-2023 u-blox
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

#include "limits.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_gpio.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_cell_module_type.h"
#include "u_cell.h"              // For uCellAtClientHandleGet()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"
#include "u_gnss_pwr.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_private.h"

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
# define U_GNSS_PWR_AIDING_TYPES 15
#endif

#ifndef U_GNSS_PWR_SYSTEM_TYPES
/** The system types to request when switching-on a GNSS
 * chip (all of them).
 */
# define U_GNSS_PWR_SYSTEM_TYPES 0x7f
#endif

/** The number of entries in gFlagToKeyId.
 */
#define U_GNSS_PWR_FLAG_TO_KEY_ID_NUM_ENTRIES 8

/** A mask for all the valid power-saving flag bits.
 */
#define U_GNSS_PWR_FLAG_MASK ((1UL << U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE) |   \
                              (1UL << U_GNSS_PWR_FLAG_EXTINT_PIN_1_NOT_0) |                          \
                              (1UL << U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE) |                          \
                              (1UL << U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE) |                        \
                              (1UL << U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE) |                    \
                              (1UL << U_GNSS_PWR_FLAG_LIMIT_PEAK_CURRENT_ENABLE) |                   \
                              (1UL << U_GNSS_PWR_FLAG_WAIT_FOR_TIME_FIX_ENABLE) |                    \
                              (1UL << U_GNSS_PWR_FLAG_RTC_WAKE_ENABLE) |                             \
                              (1UL << U_GNSS_PWR_FLAG_EPHEMERIS_WAKE_ENABLE) |                       \
                              (1UL << U_GNSS_PWR_FLAG_ACQUISITION_RETRY_IMMEDIATELY_ENABLE))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a power-saving flag and the corresponding
 * key ID for the CFG-VAL interface.
 */
typedef struct {
    uGnssPwrFlag_t flag;
    uint32_t keyId;
} uGnssPwrFlagToKeyId_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Table of power-saving flags versus CFG-VAL key IDs.
 * If you add or remove an entry in this table, update
 * #U_GNSS_PWR_FLAG_TO_KEY_ID_NUM_ENTRIES to match.
 */
static const uGnssPwrFlagToKeyId_t gFlagToKeyId[] = {
    {U_GNSS_PWR_FLAG_EXTINT_PIN_1_NOT_0, U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTSEL_E1}, // Though this is an enum, values are only 0 or 1
    {U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTWAKE_L},
    {U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L},
    {U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L},
    {U_GNSS_PWR_FLAG_LIMIT_PEAK_CURRENT_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_LIMITPEAKCURR_L},
    {U_GNSS_PWR_FLAG_WAIT_FOR_TIME_FIX_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_WAITTIMEFIX_L},
    {U_GNSS_PWR_FLAG_EPHEMERIS_WAKE_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_UPDATEEPH_L},
    {U_GNSS_PWR_FLAG_ACQUISITION_RETRY_IMMEDIATELY_ENABLE, U_GNSS_CFG_VAL_KEY_ID_PM_DONOTENTEROFF_L}
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Do AT command stuff necessary to power on via an intermediate module.
// gUGnssPrivateMutex must be locked before this is called.
static int32_t atPowerOn(uGnssPrivateInstance_t *pInstance,
                         uAtClientHandle_t atHandle,
                         bool ugindOnNotOff)
{
    int32_t errorCode;
    uint64_t y;

    // On a best effort basis, switch on or off an indication
    // which is useful when debugging aiding modes, but gets
    // in the way when we're talking to the GNSS chip directly
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UGIND=");
    uAtClientWriteInt(atHandle, ugindOnNotOff);
    uAtClientCommandStopReadResponse(atHandle);
    uAtClientUnlock(atHandle);
    // Then do the real powering-oning stuff for AT.
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
    errorCode = uAtClientUnlock(atHandle);
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

    return errorCode;
}

// Do AT command stuff necessary to power off via an intermediate module.
static int32_t atPowerOff(uAtClientHandle_t atHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;

    uPortTaskBlock(U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS);
    // Give this two tries as sometimes, if the GNSS chip is
    // busy, the command can receive no reponse
    for (size_t x = 0; (x < 2) && (errorCode < 0); x++) {
        uAtClientLock(atHandle);
        // Can take a little while if the cellular module is
        // busy talking to the GNSS module at the time
        uAtClientTimeoutSet(atHandle, U_GNSS_AT_POWER_DOWN_TIME_SECONDS * 1000);
        uAtClientCommandStart(atHandle, "AT+UGPS=");
        uAtClientWriteInt(atHandle, 0);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode < 0) {
            // If we got no response, abort the command
            // before trying again
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, " ");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

// Get UBX-CFG-PM2.
// gUGnssPrivateMutex must be locked before this is called.
static int32_t getUbxCfgPm2(uGnssPrivateInstance_t *pInstance,
                            int32_t *pMaxStartupStateDurSeconds,
                            int32_t *pUpdatePeriodMs,
                            int32_t *pSearchPeriodMs,
                            int32_t *pGridOffsetMs,
                            int32_t *pOnTimeSeconds,
                            int32_t *pMinAcqTimeSeconds,
                            int32_t *pExtInactivityMs,
                            uint32_t *pFlags)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    // Enough room for the body of the V2 UBX-CFG-PM2 message
    char message[48];

    // Poll with the message class and ID of the UBX-CFG-PM2 message,
    // expecting to get back 44 bytes for the version 1 message or
    // the full 48 for the version 2 message
    errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance, 0x06, 0x3B,
                                                  NULL, 0,
                                                  message, sizeof(message));
    if (errorCode >= 44) {
        if (pMaxStartupStateDurSeconds != NULL) {
            // maxStartupStateDurSeconds is one byte at offset 2
            *pMaxStartupStateDurSeconds = message[2];
        }
        if (pFlags != NULL) {
            // Flags is a uint32 at offset 4
            *pFlags = uUbxProtocolUint32Decode(message + 4);
        }
        if (pUpdatePeriodMs != NULL) {
            // updatePeriodMs is an int32 at offset 8
            *pUpdatePeriodMs = (int32_t) uUbxProtocolUint32Decode(message + 8);
        }
        if (pSearchPeriodMs != NULL) {
            // searchPeriodMs is an int32 at offset 12
            *pSearchPeriodMs = (int32_t) uUbxProtocolUint32Decode(message + 12);
        }
        if (pGridOffsetMs != NULL) {
            // gridOffsetMs is an int32 at offset 16
            *pGridOffsetMs = (int32_t) uUbxProtocolUint32Decode(message + 16);
        }
        if (pOnTimeSeconds != NULL) {
            // onTimeSeconds is an int16 at offset 20
            *pOnTimeSeconds = (int32_t) uUbxProtocolUint16Decode(message + 20);
        }
        if (pMinAcqTimeSeconds != NULL) {
            // minAcqTimeSeconds is an int16 at offset 22
            *pMinAcqTimeSeconds = (int32_t) uUbxProtocolUint16Decode(message + 22);
        }
        if (pExtInactivityMs != NULL) {
            *pExtInactivityMs = -1;
            if (errorCode >= 48) {
                // extInactivityMs is an int32 at offset 44
                *pExtInactivityMs = (int32_t) uUbxProtocolUint32Decode(message + 44);
            }
        }
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    } else {
        if (errorCode >= 0) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        }
    }

    return errorCode;
}

// Set UBX-CFG-PM2.
// gUGnssPrivateMutex must be locked before this is called.
static int32_t setUbxCfgPm2(uGnssPrivateInstance_t *pInstance,
                            int32_t maxStartupStateDurSeconds,
                            int32_t updatePeriodMs,
                            int32_t searchPeriodMs,
                            int32_t gridOffsetMs,
                            int32_t onTimeSeconds,
                            int32_t minAcqTimeSeconds,
                            int32_t extInactivityMs,
                            uint32_t flagsSet,
                            uint32_t flagsClear)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    // Enough room for the body of the V2 UBX-CFG-PM2 message
    char message[48];
    uint32_t x;

    // Poll with the message class and ID of the UBX-CFG-PM2 message,
    // expecting to get back 44 bytes for the version 1 message or
    // the full 48 for the version 2 message
    errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance, 0x06, 0x3B,
                                                  NULL, 0,
                                                  message, sizeof(message));
    if (errorCode >= 44) {
        if ((extInactivityMs < 0) || (errorCode >= 48)) {
            if (maxStartupStateDurSeconds >= 0) {
                // maxStartupStateDurSeconds is one byte at offset 2
                message[2] = (char) maxStartupStateDurSeconds;
            }
            // Flags is a uint32 at offset 4
            x = *((uint32_t *) (message + 4));
            x |= uUbxProtocolUint32Encode(flagsSet);
            x &= ~uUbxProtocolUint32Encode(flagsClear);
            *((uint32_t *) (message + 4)) = x;
            if (updatePeriodMs >= 0) {
                // updatePeriodMs is an int32 at offset 8
                *((uint32_t *) (message + 8)) = uUbxProtocolUint32Encode(updatePeriodMs);
            }
            if (searchPeriodMs >= 0) {
                // searchPeriodMs is an int32 at offset 12
                *((uint32_t *) (message + 12)) = uUbxProtocolUint32Encode(searchPeriodMs);
            }
            if (gridOffsetMs >= 0) {
                // gridOffsetMs is an int32 at offset 16
                *((uint32_t *) (message + 16)) = uUbxProtocolUint32Encode(gridOffsetMs);
            }
            if (onTimeSeconds >= 0) {
                // onTimeSeconds is an int16 at offset 20
                *((uint32_t *) (message + 20)) = uUbxProtocolUint16Encode(onTimeSeconds);
            }
            if (minAcqTimeSeconds >= 0) {
                // minAcqTimeSeconds is an int16 at offset 22
                *((uint32_t *) (message + 22)) = uUbxProtocolUint16Encode(minAcqTimeSeconds);
            }
            if (extInactivityMs >= 0) {
                // extInactivityMs is an int32 at offset 44
                *((uint32_t *) (message + 44)) = uUbxProtocolUint32Encode(extInactivityMs);
            }
            // Send back the modified UBX-CFG-PM2 message
            errorCode = uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x3B,
                                                   message, errorCode);
        } else {
            // Can't set extInactivityMs if the GNSS device only has a version 1 message
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        }
    } else {
        if (errorCode >= 0) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        }
    }

    return errorCode;
}

// Populate an array with the CFG-VAL flags for the given bit-map.
static size_t cfgValFlags(uGnssCfgVal_t *pCfgVal, size_t numEntries, uint32_t bitMap,
                          bool setNotClear)
{
    size_t entriesPopulated = 0;

    for (size_t x = 0; (x < sizeof(gFlagToKeyId) / sizeof(gFlagToKeyId[0])) &&
         (entriesPopulated < numEntries); x++) {
        if ((1UL << gFlagToKeyId[x].flag) & bitMap) {
            (pCfgVal + entriesPopulated)->keyId = gFlagToKeyId[x].keyId;
            (pCfgVal + entriesPopulated)->value = setNotClear;
            entriesPopulated++;
        }
    }

    return entriesPopulated;
}

// Set or clear the given power-saving flags.
// Note: deliberately don't apply a bit-mask to the flags here for forwards-compatibility.
static int32_t setOrClearFlags(uDeviceHandle_t gnssHandle, uint32_t bitMap, bool setNotClear)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal[U_GNSS_PWR_FLAG_TO_KEY_ID_NUM_ENTRIES];
    size_t x;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Some flags are only available in the UBX-CFG-PM2 message, in
            // which case that, else use the CFG-VAL mechanism
            if (bitMap & (U_GNSS_PWR_FLAG_RTC_WAKE_ENABLE |
                          U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE)) {
                if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                       U_GNSS_PRIVATE_FEATURE_OLD_CFG_API)) {
                    if (setNotClear) {
                        errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1, -1, bitMap, 0);
                    } else {
                        errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1, -1, 0, bitMap);
                    }
                }
            } else {
                if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                       U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                    // Use the CFG-VAL interface
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    x = cfgValFlags(cfgVal, sizeof(cfgVal) / sizeof(cfgVal[0]), bitMap, setNotClear);
                    if (x > 0) {
                        errorCode = uGnssCfgPrivateValSetList(pInstance, cfgVal, x,
                                                              U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                              U_GNSS_CFG_LAYERS_SET);
                    }
                } else {
                    // Old style
                    if (setNotClear) {
                        errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1, -1, bitMap, 0);
                    } else {
                        errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1, -1, 0, bitMap);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

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
                    errorCode = atPowerOn(pInstance, atHandle, true);
                } else {
                    atHandle = uGnssPrivateGetIntermediateAtHandle(pInstance);
                    if (atHandle != NULL) {
                        // If there is an intermediate module (to which
                        // we are connected by a virtual serial port) then
                        // we need to power the GNSS device on by AT commands
                        // and not do the other stuff, which is already being
                        // done by a software entity on that intermediate
                        // module.
                        errorCode = atPowerOn(pInstance, atHandle, false);
                    } else {
                        if (pInstance->portNumber != 3) {
                            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                            // Make sure GNSS is on with UBX-CFG-RST but ONLY
                            // if the GNSS chip is not on a USB interface (if
                            // it is on a USB interface then resetting it
                            // resets the USB interface also and we will lose
                            // connection).
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
                }
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
    char buffer[1];

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Set a short timeout for this
            timeoutMs = pInstance->timeoutMs;
            pInstance->timeoutMs = U_GNSS_PWR_IS_ALIVE_TIMEOUT_MS;
            // UBX-MON-VER (0x0a 0x04) is the only thing that all
            // GNSS modules are guaranteed to respond to; capture just
            // one token byte of the response, throwing the rest away:
            // provided an answer comes back we're good
            if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                  0x0a, 0x04,
                                                  NULL, 0,
                                                  buffer, sizeof(buffer)) > 0) {
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
                errorCode = atPowerOff(atHandle);
            } else {
                atHandle = uGnssPrivateGetIntermediateAtHandle(pInstance);
                if (atHandle != NULL) {
                    // If there is an intermediate module (to which
                    // we are connected by a virtual serial port) then
                    // we need to power off using AT commands
                    errorCode = atPowerOff(atHandle);
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                    // Make sure GNSS is off with UBX-CFG-RST
                    // This message is not acknowledged, so we use
                    // uGnssPrivateSendOnlyCheckStreamUbxMessage()
                    message[2] = 0x08; // Controlled GNSS stop
                    if (uGnssPrivateSendOnlyCheckStreamUbxMessage(pInstance,
                                                                  0x06, 0x04,
                                                                  message,
                                                                  sizeof(message)) > 0) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
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

// Set the power-saving mode.
int32_t uGnssPwrSetMode(uDeviceHandle_t gnssHandle, uGnssPwrSavingMode_t mode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_PM_OPERATEMODE_E1,
                            .value = mode
                           };
    uint32_t flagsSet = 0;
    uint32_t flagsClear;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                // Use the CFG-VAL interface, setting the mode in
                // BBRAM as well as RAM in case it is on/off power saving,
                // which loses the contents of RAM
                errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                      U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                      U_GNSS_CFG_LAYERS_SET);
            } else {
                // Old style: the mode is in the flags field, bits 17 and 18,
                // where 0 == on/off power-saving and 1 == cyclic tracking
                // power-saving, there is no "none" setting
                flagsClear = 3UL << 17;
                if (mode == U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING) {
                    flagsSet = 1UL << 17;
                    flagsClear = 1UL << 18;
                }
                errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1, -1,
                                         flagsSet, flagsClear);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the power-saving mode.
int32_t uGnssPwrGetMode(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrMode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t keyId = U_GNSS_CFG_VAL_KEY_ID_PM_OPERATEMODE_E1;
    uGnssCfgVal_t *pCfgVal = NULL;
    uint32_t flags;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrMode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                // Use the CFG-VAL interface
                errorCodeOrMode = uGnssCfgPrivateValGetListAlloc(pInstance, &keyId, 1, &pCfgVal,
                                                                 U_GNSS_CFG_VAL_LAYER_RAM);
                if ((errorCodeOrMode >= 0) && (pCfgVal != NULL)) {
                    errorCodeOrMode = (int32_t) pCfgVal->value;
                    uPortFree(pCfgVal);
                }
            } else {
                // Old style
                errorCodeOrMode = getUbxCfgPm2(pInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &flags);
                if (errorCodeOrMode == 0) {
                    errorCodeOrMode = (int32_t) U_GNSS_PWR_SAVING_MODE_ON_OFF;
                    if (((flags >> 17) & 0x03) == 0x01) {
                        errorCodeOrMode = (int32_t) U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrMode;
}

// Set one or more of the power-saving flags.
int32_t uGnssPwrSetFlag(uDeviceHandle_t gnssHandle, uint32_t setBitMap)
{
    return setOrClearFlags(gnssHandle, setBitMap, true);
}

// Clear one or more power-saving flags.
int32_t uGnssPwrClearFlag(uDeviceHandle_t gnssHandle, uint32_t clearBitMap)
{
    return setOrClearFlags(gnssHandle, clearBitMap, false);
}

// Get the current values of all of the power-saving flags.
int32_t uGnssPwrGetFlag(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrFlags = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t *pCfgVal = NULL;
    uint32_t flags = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrFlags = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Since some flags are only available in the UBX-CFG-PM2 message,
            // we use that if the GNSS chip supports it, else we use the
            // CFG-VAL mechanism
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_OLD_CFG_API)) {
                errorCodeOrFlags = getUbxCfgPm2(pInstance, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &flags);
                if (errorCodeOrFlags == 0) {
                    // Need to get rid of the reserved bits which may be set
                    errorCodeOrFlags = (int32_t) (flags & U_GNSS_PWR_FLAG_MASK);
                }
            } else {
                // Use the CFG-VAL interface.  It would be better to do this with
                // a single call to uGnssCfgPrivateValGetListAlloc(), however not
                // all modules support all flags and if an unsupported flag is
                // included in the list the entire thing is NACKed, hence we do it
                // the long way
                errorCodeOrFlags = 0;
                for (size_t x = 0; (x < sizeof(gFlagToKeyId) / sizeof(gFlagToKeyId[0])) &&
                     ((errorCodeOrFlags >= 0) || (errorCodeOrFlags == (int32_t) U_GNSS_ERROR_NACK)); x++) {
                    errorCodeOrFlags = uGnssCfgPrivateValGetListAlloc(pInstance,
                                                                      &(gFlagToKeyId[x].keyId), 1,
                                                                      &pCfgVal,
                                                                      U_GNSS_CFG_VAL_LAYER_RAM);
                    if ((errorCodeOrFlags >= 0) && (pCfgVal != NULL)) {
                        // Translate the value into the bit-map
                        if (pCfgVal->value) {
                            flags |= 1UL << gFlagToKeyId[x].flag;
                        }
                        uPortFree(pCfgVal);
                    }
                }
                if ((errorCodeOrFlags >= 0) || (errorCodeOrFlags == (int32_t) U_GNSS_ERROR_NACK)) {
                    errorCodeOrFlags = (int32_t) flags;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrFlags;
}

// Set the various timings for the GNSS device.
int32_t uGnssPwrSetTiming(uDeviceHandle_t gnssHandle,
                          int32_t acquisitionPeriodSeconds,
                          int32_t acquisitionRetryPeriodSeconds,
                          int32_t onTimeSeconds,
                          int32_t maxAcquisitionTimeSeconds,
                          int32_t minAcquisitionTimeSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal[5];
    size_t x = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (onTimeSeconds <= UINT16_MAX) &&
            (maxAcquisitionTimeSeconds <= UINT8_MAX) &&
            ((U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                 U_GNSS_PRIVATE_FEATURE_OLD_CFG_API) &&
              (minAcquisitionTimeSeconds <= UINT16_MAX)) ||
             (minAcquisitionTimeSeconds <= UINT8_MAX))) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_OLD_CFG_API)) {
                // Use the old style if we can as the minimum acquisition
                // time has a larger range
                errorCode = setUbxCfgPm2(pInstance,
                                         maxAcquisitionTimeSeconds,
                                         acquisitionPeriodSeconds * 1000,
                                         acquisitionRetryPeriodSeconds * 1000,
                                         -1,
                                         onTimeSeconds,
                                         minAcquisitionTimeSeconds,
                                         -1,
                                         0, 0);
            } else {
                // Use the CFG-VAL interface
                if (acquisitionPeriodSeconds >= 0) {
                    cfgVal[x].keyId = U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4;
                    cfgVal[x].value = acquisitionPeriodSeconds;
                    x++;
                }
                if (acquisitionRetryPeriodSeconds >= 0) {
                    cfgVal[x].keyId = U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4;
                    cfgVal[x].value = acquisitionRetryPeriodSeconds;
                    x++;
                }
                if (onTimeSeconds >= 0) {
                    cfgVal[x].keyId = U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2;
                    cfgVal[x].value = onTimeSeconds;
                    x++;
                }
                if (maxAcquisitionTimeSeconds >= 0) {
                    cfgVal[x].keyId = U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1;
                    cfgVal[x].value = maxAcquisitionTimeSeconds;
                    x++;
                }
                if (minAcquisitionTimeSeconds >= 0) {
                    cfgVal[x].keyId = U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1;
                    cfgVal[x].value = minAcquisitionTimeSeconds;
                    x++;
                }
                errorCode = uGnssCfgPrivateValSetList(pInstance, cfgVal, x,
                                                      U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                      U_GNSS_CFG_LAYERS_SET);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the various timings for the GNSS device.
int32_t uGnssPwrGetTiming(uDeviceHandle_t gnssHandle,
                          int32_t *pAcquisitionPeriodSeconds,
                          int32_t *pAcquisitionRetryPeriodSeconds,
                          int32_t *pOnTimeSeconds,
                          int32_t *pMaxAcquisitionTimeSeconds,
                          int32_t *pMinAcquisitionTimeSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t keyIds[5];
    uGnssCfgVal_t *pCfgVal = NULL;
    size_t y = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                // Use the CFG-VAL interface
                if (pAcquisitionPeriodSeconds != NULL) {
                    *pAcquisitionPeriodSeconds = -1;
                    keyIds[y] = U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4;
                    y++;
                }
                if (pAcquisitionRetryPeriodSeconds != NULL) {
                    *pAcquisitionRetryPeriodSeconds = -1;
                    keyIds[y] = U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4;
                    y++;
                }
                if (pOnTimeSeconds != NULL) {
                    *pOnTimeSeconds = -1;
                    keyIds[y] = U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2;
                    y++;
                }
                if (pMaxAcquisitionTimeSeconds != NULL) {
                    *pMaxAcquisitionTimeSeconds = -1;
                    keyIds[y] = U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1;
                    y++;
                }
                if (pMinAcquisitionTimeSeconds != NULL) {
                    *pMinAcquisitionTimeSeconds = -1;
                    keyIds[y] = U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1;
                    y++;
                }
                errorCode = uGnssCfgPrivateValGetListAlloc(pInstance, keyIds, y,
                                                           &pCfgVal,
                                                           U_GNSS_CFG_VAL_LAYER_RAM);
                if ((errorCode >= 0) && (pCfgVal != NULL)) {
                    for (int32_t x = 0; x < errorCode; x++) {
                        switch ((pCfgVal + x)->keyId) {
                            case U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4:
                                if (pAcquisitionPeriodSeconds != NULL) {
                                    *pAcquisitionPeriodSeconds = (int32_t) (pCfgVal + x)->value;
                                }
                                break;
                            case U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4:
                                if (pAcquisitionRetryPeriodSeconds != NULL) {
                                    *pAcquisitionRetryPeriodSeconds = (int32_t) (pCfgVal + x)->value;
                                }
                                break;
                            case U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2:
                                if (pOnTimeSeconds != NULL) {
                                    *pOnTimeSeconds = (int32_t) (pCfgVal + x)->value;
                                }
                                break;
                            case  U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1:
                                if (pMaxAcquisitionTimeSeconds != NULL) {
                                    *pMaxAcquisitionTimeSeconds = (int32_t) (pCfgVal + x)->value;
                                }
                                break;
                            case U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1:
                                if (pMinAcquisitionTimeSeconds != NULL) {
                                    *pMinAcquisitionTimeSeconds = (int32_t) (pCfgVal + x)->value;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    uPortFree(pCfgVal);
                }
            } else {
                // Old style
                errorCode = getUbxCfgPm2(pInstance,
                                         pMaxAcquisitionTimeSeconds,
                                         pAcquisitionPeriodSeconds,
                                         pAcquisitionRetryPeriodSeconds,
                                         NULL,
                                         pOnTimeSeconds,
                                         pMinAcquisitionTimeSeconds,
                                         NULL, NULL);
                if (errorCode >= 0) {
                    // The values for acquisition period and acquisition
                    // retry period returned by UBX-CFG-PM2 are in milliseconds
                    // so need to convert
                    if (pAcquisitionPeriodSeconds != NULL) {
                        *pAcquisitionPeriodSeconds /= 1000;
                    }
                    if (pAcquisitionRetryPeriodSeconds != NULL) {
                        *pAcquisitionRetryPeriodSeconds /= 1000;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Set the offset of the acquisition and acquisition retry periods.
int32_t uGnssPwrSetTimingOffset(uDeviceHandle_t gnssHandle, int32_t offsetSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_PM_GRIDOFFSET_U4,
                            .value = offsetSeconds
                           };

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                      U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                      U_GNSS_CFG_LAYERS_SET);
            } else {
                errorCode = setUbxCfgPm2(pInstance, -1, -1, -1,
                                         offsetSeconds * 1000, -1, -1, -1, 0, 0);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the offset of the acquisition and acquisition retry periods.
int32_t uGnssPwrGetTimingOffset(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrOffset = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t keyId = U_GNSS_CFG_VAL_KEY_ID_PM_GRIDOFFSET_U4;
    uGnssCfgVal_t *pCfgVal = NULL;
    int32_t y = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrOffset = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrOffset = uGnssCfgPrivateValGetListAlloc(pInstance, &keyId, 1,
                                                                   &pCfgVal,
                                                                   U_GNSS_CFG_VAL_LAYER_RAM);
                if ((errorCodeOrOffset >= 0) && (pCfgVal != NULL)) {
                    errorCodeOrOffset = (int32_t) pCfgVal->value;
                    uPortFree(pCfgVal);
                }
            } else {
                errorCodeOrOffset = getUbxCfgPm2(pInstance, NULL, NULL, NULL,
                                                 &y, NULL, NULL, NULL, NULL);
                if (errorCodeOrOffset == 0) {
                    errorCodeOrOffset = y / 1000;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrOffset;
}

// Set the inactivity timeout of the EXTINT pin.
int32_t uGnssPwrSetExtintInactivityTimeout(uDeviceHandle_t gnssHandle,
                                           int32_t timeoutMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVITY_U4,
                            .value = timeoutMs
                           };

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                      U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                      U_GNSS_CFG_LAYERS_SET);
            } else {
                errorCode = setUbxCfgPm2(pInstance, -1, -1, -1, -1, -1, -1,
                                         timeoutMs, 0, 0);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the inactivity timeout used with the EXTINT pin.
int32_t uGnssPwrGetExtintInactivityTimeout(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t keyId = U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVITY_U4;
    uGnssCfgVal_t *pCfgVal = NULL;
    int32_t y = 0;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrTimeout = uGnssCfgPrivateValGetListAlloc(pInstance, &keyId, 1,
                                                                    &pCfgVal,
                                                                    U_GNSS_CFG_VAL_LAYER_RAM);
                if ((errorCodeOrTimeout >= 0) && (pCfgVal != NULL)) {
                    errorCodeOrTimeout = (int32_t) pCfgVal->value;
                    uPortFree(pCfgVal);
                }
            } else {
                errorCodeOrTimeout = getUbxCfgPm2(pInstance, NULL, NULL, NULL, NULL, NULL, NULL,
                                                  &y, NULL);
                if (errorCodeOrTimeout == 0) {
                    errorCodeOrTimeout = y;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrTimeout;
}

// End of file
