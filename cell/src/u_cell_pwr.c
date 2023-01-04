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
 * @brief Implementation of the power (both on/off and power saving)
 * API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_pwr.h"
#include "u_cell_pwr_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The number of times to poke the module to confirm that
 * she's powered-on.
 */
#define U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON 10

/** The number of time to try a configuration AT command by default.
 */
#define U_CELL_PWR_CONFIGURATION_COMMAND_TRIES 3

/** The UART power saving duration in GSM frames, needed for the
 * UART power saving AT command.
 */
#define U_CELL_PWR_UART_POWER_SAVING_GSM_FRAMES ((U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS * 1000000) / 4615)

/** Convert a decoded EUTRAN paging window value into seconds for the given RAT.
 */
#define U_CELL_PWR_PAGING_WINDOW_DECODED_EUTRAN_TO_SECONDS(value, rat) (((rat) == U_CELL_NET_RAT_NB1) ? \
                                                                        (((value) + 1) * 256 / 100) :   \
                                                                        (((value) + 1) * 128 / 100))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The UART power-saving modes: note that these numbers are defined
 * by the AT interface and should NOT be changed.
 */
//lint -esym(749, uCellPwrPsvMode_t::U_CELL_PWR_PSV_MODE_RTS) Suppress not referenced
//lint -esym(749, uCellPwrPsvMode_t::U_CELL_PWR_PSV_MODE_DTR) Suppress not referenced
typedef enum {
    U_CELL_PWR_PSV_MODE_DISABLED = 0,    /**< No UART power saving. */
    U_CELL_PWR_PSV_MODE_DATA = 1,        /**< Module wakes up on TXD line activity, SARA-U201/SARA-R5 version. */
    U_CELL_PWR_PSV_MODE_RTS = 2,         /**< Module wakes up on RTS line being asserted (not used in this code). */
    U_CELL_PWR_PSV_MODE_DTR = 3,         /**< Module wakes up on DTR line being asserted. */
    U_CELL_PWR_PSV_MODE_DATA_SARA_R4 = 4 /**< Module wakes up on TXD line activity, SARA-R4 version. */
} uCellPwrPsvMode_t;

/** All the parameters for a wake-up-from-deep sleep callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t, void *);
    void *pCallbackParam;
} uCellPwrDeepSleepWakeUpCallback_t;

/** All the parameters for an E-DRX URC callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t, uCellNetRat_t, bool, int32_t, int32_t, int32_t, void *);
    uCellNetRat_t rat;
    bool onNotOff;
    int32_t eDrxSecondsRequested;
    int32_t eDrxSecondsAssigned;
    int32_t pagingWindowSecondsAssigned;
    void *pCallbackParam;
} uCellPwrEDrxCallback_t;

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
// SARA-R5xxx-01B remembers whether sockets are in hex mode or
// not so reset that here in order that all modules behave the
// same way
                                              "AT+UDCONF=1,0",
                                              "ATI9",      // Firmware version
                                              "AT&C1",     // DCD circuit (109) changes with the carrier
                                              "AT&D0"      // Ignore changes to DTR
                                             };

/** Array to convert the RAT emited by AT+CEDRXS to one of our RATs.
 */
static const uCellNetRat_t gCedrxsRatToCellRat[] = {U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,
                                                    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,
                                                    U_CELL_NET_RAT_GSM_GPRS_EGPRS,  // 2 is GPRS
                                                    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,
                                                    U_CELL_NET_RAT_CATM1, // 4 is LTE and is also CATM1
                                                    U_CELL_NET_RAT_NB1    // 5 is NB1
                                                   };

/** Array to convert one of our RATs to the RAT emited by AT+CEDRXS.
 */
static const int32_t gCellRatToCedrxsRat[] = { -1, // U_CELL_NET_RAT_DUMMY
                                               -1, // U_CELL_NET_RAT_NKNOWN_OR_NOT_USED
                                               2,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS
                                               -1, // U_CELL_NET_RAT_GSM_COMPACT
                                               -1, // U_CELL_NET_RAT_UTRAN
                                               -1, // U_CELL_NET_RAT_EGPRS
                                               -1, // U_CELL_NET_RAT_HSDPA
                                               -1, // U_CELL_NET_RAT_HSDPA_HSUPA
                                               4,  // U_CELL_NET_RAT_LTE
                                               -1, // U_CELL_NET_RAT_EC_GSM
                                               4,  // U_CELL_NET_RAT_CATM1
                                               5   // U_CELL_NET_RAT_NB1
                                               };

/** Array to convert E-DRX values for Cat-M1 in seconds into the number
 * value of 24.008 table 10.5.5.34 (the index of the entry in the array
 * is the number value).
 */
static const int32_t gEdrxCatM1SecondsToNumber[] = {5, 10, 20, 41, 61, 82, 102, 122, 143, 164, 328, 655, 1310, 2621};

/** Array to convert E-DRX values for NB1 in seconds into the number
 * value of 24.008 table 10.5.5.32 (the index of the entry in the array
 * is the number value).  Note that some values are missing, denoted
 * with entries of -1, and some just default to 20 seconds.
 */
static const int32_t gEdrxNb1SecondsToNumber[] = {-1, -1, 20, 41, 20, 82, 20, 20, 20, 164, 328, 655, 1310, 2621, 5243, 10486};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: 3GPP POWER SAVING
 * -------------------------------------------------------------- */

// Convert an unsigned integer into binary.
static void uintToBinaryString(uint32_t num, char *pStr, int32_t strSize,
                               int32_t bitCount)
{
    int32_t tmp = 0;
    int32_t pos = 0;

    for (int32_t x = 31; x >= 0; x--) {
        tmp = (int32_t) (num >> x);
        if (x < bitCount) {
            if (pos < strSize) {
                if (tmp & 1) {
                    *(pStr + pos) = 1 + '0';
                } else {
                    *(pStr + pos) = 0 + '0';
                }
            }
            pos++;
        }
    }
}

// Convert a string representing a binary value into an unsigned integer.
static uint32_t binaryStringToUint(const char *pStr)
{
    size_t strSize = strlen(pStr);
    uint32_t value = 0;

    for (size_t x = 0; x < strSize; x++) {
        value += ((unsigned) (int32_t) *(pStr + (strSize - x) - 1) - '0') << x; // *NOPAD*
    }

    return value;
}

// Set the power saving parameters using AT+CPSMS.
static int32_t setPowerSavingMode(const uCellPrivateInstance_t *pInstance,
                                  bool onNotOff,
                                  int32_t activeTimeSeconds,
                                  int32_t periodicWakeupSeconds)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    char ptEncoded[8 + 1] = {0}; // timer value encoded as 3GPP IE
    char atEncoded[8 + 1] = {0}; // timer value encoded as 3GPP IE
    uint32_t value;
    int32_t t;

    if ((activeTimeSeconds >= 0) && (periodicWakeupSeconds >= 0)) {
        // PSM string encoding code borrowed from AT_CellularPower.cpp
        // Table 10.5.163a/3GPP TS 24.008: GPRS Timer 3 information element
        // Bits 5 to 1 represent the binary coded timer value.
        // Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
        // 8 7 6
        // 0 0 0 value is incremented in multiples of 10 minutes
        // 0 0 1 value is incremented in multiples of 1 hour
        // 0 1 0 value is incremented in multiples of 10 hours
        // 0 1 1 value is incremented in multiples of 2 seconds
        // 1 0 0 value is incremented in multiples of 30 seconds
        // 1 0 1 value is incremented in multiples of 1 minute
        // 1 1 0 value is incremented in multiples of 320 hours (NOTE 1)
        // 1 1 1 value indicates that the timer is deactivated (NOTE 2).
        if (periodicWakeupSeconds <= 2 * 0x1f) { // multiples of 2 seconds
            value = periodicWakeupSeconds / 2;
            strncpy(ptEncoded, "01100000", sizeof(ptEncoded));
        } else {
            if (periodicWakeupSeconds <= 30 * 0x1f) { // multiples of 30 seconds
                value = periodicWakeupSeconds / 30;
                strncpy(ptEncoded, "10000000", sizeof(ptEncoded));
            } else {
                if (periodicWakeupSeconds <= 60 * 0x1f) { // multiples of 1 minute
                    value = periodicWakeupSeconds / 60;
                    strncpy(ptEncoded, "10100000", sizeof(ptEncoded));
                } else {
                    if (periodicWakeupSeconds <= 10 * 60 * 0x1f) { // multiples of 10 minutes
                        value = periodicWakeupSeconds / (10 * 60);
                        strncpy(ptEncoded, "00000000", sizeof(ptEncoded));
                    } else {
                        if (periodicWakeupSeconds <= 60 * 60 * 0x1f) { // multiples of 1 hour
                            value = periodicWakeupSeconds / (60 * 60);
                            strncpy(ptEncoded, "00100000", sizeof(ptEncoded));
                        } else {
                            if (periodicWakeupSeconds <= 10 * 60 * 60 * 0x1f) { // multiples of 10 hours
                                value = periodicWakeupSeconds / (10 * 60 * 60);
                                strncpy(ptEncoded, "01000000", sizeof(ptEncoded));
                            } else { // multiples of 320 hours
                                t = periodicWakeupSeconds / (320 * 60 * 60);
                                if (t > 0x1f) {
                                    t = 0x1f;
                                }
                                value = t;
                                strncpy(ptEncoded, "11000000", sizeof(ptEncoded));
                            }
                        }
                    }
                }
            }
        }

        uintToBinaryString(value, &ptEncoded[3], sizeof(ptEncoded) - 3, 5);
        ptEncoded[8] = '\0';

        // Table 10.5.172/3GPP TS 24.008: GPRS Timer information element
        // Bits 5 to 1 represent the binary coded timer value.
        // Bits 6 to 8 defines the timer value unit for the GPRS timer as follows:
        // 8 7 6
        // 0 0 0  value is incremented in multiples of 2 seconds
        // 0 0 1  value is incremented in multiples of 1 minute
        // 0 1 0  value is incremented in multiples of decihours
        // 1 1 1  value indicates that the timer is deactivated.
        // Other values shall be interpreted as multiples of 1 minute in this
        // version of the protocol.
        if (activeTimeSeconds <= 2 * 0x1f) { // multiples of 2 seconds
            value = activeTimeSeconds / 2;
            strncpy(atEncoded, "00000000", sizeof(atEncoded));
        } else {
            if (activeTimeSeconds <= 60 * 0x1f) { // multiples of 1 minute
                value = (1 << 5) | (activeTimeSeconds / 60);
                strncpy(atEncoded, "00100000", sizeof(atEncoded));
            } else { // multiples of decihours
                t = activeTimeSeconds / (6 * 60);
                if (t > 0x1f) {
                    t = 0x1f;
                }
                value = t;
                strncpy(atEncoded, "01000000", sizeof(atEncoded));
            }
        }

        uintToBinaryString(value, &atEncoded[3], sizeof(atEncoded) - 3, 5);
        atEncoded[8] = '\0';
    }

    value = 0;
    if (onNotOff) {
        value = 1;
    }

    uAtClientLock(atHandle);
    // Can need a little longer for this
    uAtClientTimeoutSet(atHandle, 10000);
    uAtClientCommandStart(atHandle, "AT+CPSMS=");
    // Write the on/off flag
    uAtClientWriteInt(atHandle, (int32_t) value);
    if ((activeTimeSeconds >= 0) && (periodicWakeupSeconds >= 0)) {
        // Skip unused GPRS parameters
        uAtClientWriteString(atHandle, "", false);
        uAtClientWriteString(atHandle, "", false);
        // Write wanted parameters
        uAtClientWriteString(atHandle, ptEncoded, true);
        uAtClientWriteString(atHandle, atEncoded, true);
    }
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    if (errorCode == 0) {
        uPortLog("U_CELL_PWR: requested PSM %s, requested TAU time %d second(s),"
                 " requested active time %d second(s).\n",
                 onNotOff ? "on" : "off", periodicWakeupSeconds,
                 activeTimeSeconds);
        // Note: the URC for deep sleep is switched on at power-on
        if (pInstance->pSleepContext != NULL) {
            // Assume that the network has agreed: this
            // will be updated when the 3GPP power saving
            // state is read and when we get a +CEREG
            pInstance->pSleepContext->powerSaving3gppAgreed = onNotOff;
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: E-DRX
 * -------------------------------------------------------------- */

// Create a sleep context.
static int32_t createSleepContext(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uCellPrivateSleep_t *pContext;

    pInstance->pSleepContext = (uCellPrivateSleep_t *) pUPortMalloc(sizeof(uCellPrivateSleep_t));
    if (pInstance->pSleepContext != NULL) {
        pContext = pInstance->pSleepContext;
        memset(pContext, 0, sizeof(*pContext));
        // Set the CEREG items up to an impossible set (can't be on if
        // activeTimeSeconds is -1) so that when some genuine ones
        // arrive we will notice the difference.
        pContext->powerSaving3gppOnNotOffCereg = true;
        pContext->activeTimeSecondsCereg = -1;
        pContext->periodicWakeupSecondsCereg = -1;
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Return the 24.008 table 10.5.5.32 value for a given E-DRX value.
static int32_t edrxSecondsToNumber(int32_t seconds, uCellNetRat_t rat)
{
    int32_t number = -1;
    size_t y = 0;
    const int32_t *pTmp = NULL;

    switch (rat) {
        case U_CELL_NET_RAT_GSM_GPRS_EGPRS:
            number = (seconds * 100 * 1300 / 306) / 100;
            break;
        case U_CELL_NET_RAT_CATM1:
            pTmp = gEdrxCatM1SecondsToNumber;
            y = sizeof(gEdrxCatM1SecondsToNumber) / sizeof(gEdrxCatM1SecondsToNumber[0]);
            break;
        case U_CELL_NET_RAT_NB1:
            pTmp = gEdrxNb1SecondsToNumber;
            y = sizeof(gEdrxNb1SecondsToNumber) / sizeof(gEdrxNb1SecondsToNumber[0]);
            break;
        default:
            break;
    }
    if ((pTmp != NULL) && (y > 0)) {
        // For Cat-M1/NB1 need to look up the values up
        // in a table as it is not a simple mapping
        for (int32_t x = 0; (number < 0) && (x < (int32_t) y); x++) {
            if ((*(pTmp + x) >= 0) && (seconds <= *(pTmp + x))) {
                number = x;
            }
        }
        // If we couldn't find one, use the largest
        if (number < 0) {
            number = *(pTmp + y - 1);
        }
    }

    return number;
}

// Return the value in seconds for a given 24.008 table 10.5.5.32 E-DRX number.
static int32_t edrxNumberToSeconds(int32_t number, uCellNetRat_t rat)
{
    int32_t seconds = -1;

    switch (rat) {
        case U_CELL_NET_RAT_GSM_GPRS_EGPRS:
            seconds = (number * 100 * 306 / 1300) / 100;
            break;
        case U_CELL_NET_RAT_CATM1:
            if ((number >= 0) && (number < (int32_t) (sizeof(gEdrxCatM1SecondsToNumber) /
                                                      sizeof(gEdrxCatM1SecondsToNumber[0])))) {
                seconds = gEdrxCatM1SecondsToNumber[number];
            }
            break;
        case U_CELL_NET_RAT_NB1:
            if ((number >= 0) && (number < (int32_t) (sizeof(gEdrxNb1SecondsToNumber) /
                                                      sizeof(gEdrxNb1SecondsToNumber[0])))) {
                seconds = gEdrxNb1SecondsToNumber[number];
            }
            break;
        default:
            break;
    }

    return seconds;
}

// Read CEDRXS or CEDRXRDP.
int32_t readCedrxsOrCedrxrdp(const uCellPrivateInstance_t *pInstance, bool rdpNotS,
                             uCellNetRat_t rat,
                             bool *pOnNotOffRequested, int32_t *pEDrxSecondsRequested,
                             int32_t *pPagingWindowSecondsRequested, bool *pOnNotOffAssigned,
                             int32_t *pEDrxSecondsAssigned, int32_t *pPagingWindowSecondsAssigned)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool keepGoing = true;
    char encoded[4 + 1]; // String representing four binary digits
    int32_t firstInt = -1;
    int32_t bytesRead;
    int32_t value;
    int32_t eDrxSecondsRequested = -1;
    int32_t pagingWindowSecondsRequested = -1;
    int32_t eDrxSecondsAssigned = -1;
    int32_t pagingWindowSecondsAssigned = -1;
    const char *pAtCommand = "AT+CEDRXS?";
    const char *pAtResponse = "+CEDRXS:";

    if (rdpNotS) {
        pAtCommand = "AT+CEDRXRDP";
        pAtResponse = "+CEDRXRDP:";
    }

    // CEDRXS and CEDRXP are very similar in format but not _quite_ the same.
    //
    // On SARA-R4 CEDRXS goes like this: a multi-line response giving the
    // requested values for E-DRX and, optionally, paging window, where
    // the lack of a line for a given RAT indicates that E-DRX is off, e.g.
    //
    // +CEDRXS: 2,"0111","0001"
    // +CEDRXS: 4,"0111","0001"
    //
    //...means that E-DRX for NBIoT (RAT 5) is off but it is on for GPRS
    // (RAT 2) and Cat-M1 (RAT 4).
    //
    // On SARA-R5, however, the +CEDRXS line is still present even if E-DRX
    // is *off* for that RAT.
    //
    // CEDRXP, on the other hand, gives both the requested E-DRX value
    // and the assigned E-DRX and assigned paging window values (in that
    // order) and looks/ something like this on both SARA-R4 and SARA-R5:
    //
    // +CEDRXRDP: 2,"0111","0001","0001"
    // +CEDRXRDP: 4,"0111","0001","0001"
    //
    // ...but in this case the first digit can also be 0 to indicate that E-DRX
    // is disabled by the network.  So to get the _requested_ E-DRX value on
    // both SARA-R4 and SARA-R5 reliably use CEDRXRDP, to get the requested
    // paging window value, where supported, use CEDRXS and to get the
    // assigned values for both use CEDRXRDP.

    if (((int32_t) rat >= 0) &&
        // Cast in two stages to keep Lint happy
        ((size_t) (int32_t) rat < (sizeof(gCellRatToCedrxsRat) /
                                   sizeof(gCellRatToCedrxsRat[0])))) {
        errorCode = (int32_t) U_CELL_ERROR_AT;
        uAtClientLock(atHandle);
        // Set a short time-out so that we can
        // detect the end of the response quickly
        uAtClientTimeoutSet(atHandle, pInstance->pModule->responseMaxWaitMs);
        uAtClientCommandStart(atHandle, pAtCommand);
        uAtClientCommandStop(atHandle);
        while (keepGoing) {
            if (uAtClientResponseStart(atHandle, pAtResponse) == 0) {
                // Read the RAT or, if CEDRXRDP, what might be 0 for
                // "disabled by the network"
                value = uAtClientReadInt(atHandle);
                if ((value >= 0) &&
                    (value < (int32_t) (sizeof(gCedrxsRatToCellRat) / sizeof(gCedrxsRatToCellRat[0])))) {
                    if ((rat == gCedrxsRatToCellRat[value]) || (rdpNotS && (value == 0))) {
                        // If we're doing CEDRXRDP and the first integer is 0 then that means
                        // E-DRX is off but the values that follow may still be populated (e.g.
                        // if the network has refused a perfectly valid requested E-DRX setting)
                        if (rdpNotS && (value == 0) && (firstInt < 0)) {
                            firstInt = value;
                        }
                        // The first 4-bit binary thing is always the encoded requested E-DRX value
                        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
                        if (bytesRead == 4) {
                            // Convert the encoded value to a number
                            value = (int32_t) binaryStringToUint(encoded);
                            eDrxSecondsRequested = edrxNumberToSeconds(value, rat);
                        }
                        if (rdpNotS) {
                            // If we're reading CEDRXRDP then the next 4-bit binary thing
                            // is the assigned E-DRX value
                            bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
                            if (bytesRead == 4) {
                                // Convert the encoded value to a number
                                value = (int32_t) binaryStringToUint(encoded);
                                eDrxSecondsAssigned = edrxNumberToSeconds(value, rat);
                            }
                            // ...and the thing that follows that is the assigned paging
                            // window value, if present
                            bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
                            if (bytesRead == 4) {
                                // Convert the encoded value to a number
                                value = (int32_t) binaryStringToUint(encoded);
                                if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
                                    pagingWindowSecondsAssigned = U_CELL_PWR_PAGING_WINDOW_DECODED_EUTRAN_TO_SECONDS(value,
                                                                                                                     rat);
                                } else {
                                    pagingWindowSecondsAssigned = value;
                                }
                            }
                        } else {
                            // If we're doing CEDRXS then the only thing that can
                            // follow is the optional requested paging window value
                            bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
                            if (bytesRead == 4) {
                                // Convert the encoded value to a number
                                value = (int32_t) binaryStringToUint(encoded);
                                if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
                                    pagingWindowSecondsRequested = U_CELL_PWR_PAGING_WINDOW_DECODED_EUTRAN_TO_SECONDS(value,
                                                                                                                      rat);
                                } else {
                                    pagingWindowSecondsRequested = value;
                                }
                            }
                        }
                    }
                } else {
                    // Some platforms (e.g. SARAR-R41x) return "+CEDRXS:"
                    // followed by no digits whatsoever to indicate that
                    // E-DRX is off
                    if (!rdpNotS && (value < 0)) {
                        firstInt = 0;
                    }
                    keepGoing = false;
                }
            } else {
                keepGoing = false;
            }
            uAtClientClearError(atHandle);
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        if (eDrxSecondsRequested >= 0) {
            // Having decoded a requested E-DRX value constitues success
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pEDrxSecondsRequested != NULL) {
                *pEDrxSecondsRequested = eDrxSecondsRequested;
            }
        } else if (firstInt == 0) {
            // If the first integer is zero, or is absent then that
            // means we're successful and the requested E-DRX state
            // was "off"
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pOnNotOffRequested != NULL) {
                *pOnNotOffRequested = false;
            }
        }
        // Now fill everything else in
        if ((pagingWindowSecondsRequested >= 0) && (pPagingWindowSecondsRequested != NULL)) {
            *pPagingWindowSecondsRequested = pagingWindowSecondsRequested;
        }
        if ((eDrxSecondsAssigned >= 0) && (pEDrxSecondsAssigned != NULL)) {
            *pEDrxSecondsAssigned = eDrxSecondsAssigned;
        }
        if ((pagingWindowSecondsAssigned >= 0) && (pPagingWindowSecondsAssigned != NULL)) {
            *pPagingWindowSecondsAssigned = pagingWindowSecondsAssigned;
        }
        if (pOnNotOffRequested != NULL) {
            if (eDrxSecondsRequested >= 0) {
                *pOnNotOffRequested = true;
            } else {
                *pOnNotOffRequested = false;
            }
        }
        if (pOnNotOffAssigned != NULL) {
            if (eDrxSecondsAssigned >= 0) {
                *pOnNotOffAssigned = true;
            } else {
                *pOnNotOffAssigned = false;
            }
        }
    }

    return errorCode;
}

// Callback via which the user's E-DRX parameter update callback is called.
// This is called through the uAtClientCallback() mechanism in order
// to prevent the AT client URC from blocking.
static void eDrxCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellPwrEDrxCallback_t *pCallback = (uCellPwrEDrxCallback_t *) pParameter;

    (void) atHandle;

    if (pCallback != NULL) {
        pCallback->pCallback(pCallback->cellHandle,
                             pCallback->rat,
                             pCallback->onNotOff,
                             pCallback->eDrxSecondsRequested,
                             pCallback->eDrxSecondsAssigned,
                             pCallback->pagingWindowSecondsAssigned,
                             pCallback->pCallbackParam);
        uPortFree(pCallback);
    }
}

// URC for when the E-DRX parameters change.
static void CEDRXP_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    int32_t value;
    uCellNetRat_t rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    char encoded[4 + 1]; // String representing four binary digits
    int32_t bytesRead;
    int32_t eDrxSecondsRequested = -1;
    int32_t eDrxSecondsAssigned = -1;
    int32_t pagingWindowSecondsAssigned = -1;
    uCellPwrEDrxCallback_t *pCallback;

    // +CEDRXP: 4,"0001","0001","0011

    // Read the RAT, and this really is just the RAT, it is not also used
    // to indicate "off" by being 0 or anything like that
    value = uAtClientReadInt(atHandle);
    if ((value >= 0) &&
        (value < (int32_t) (sizeof(gCedrxsRatToCellRat) / sizeof(gCedrxsRatToCellRat[0])))) {
        rat = gCedrxsRatToCellRat[value];
        // The first 4-bit binary string is the encoded requested E-DRX value
        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
        if (bytesRead == 4) {
            // Convert the encoded value to a number
            value = (int32_t) binaryStringToUint(encoded);
            eDrxSecondsRequested = edrxNumberToSeconds(value, rat);
        }
        // The second 4-bit binary string is the assigned E-DRX value
        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
        if (bytesRead == 4) {
            // Convert the encoded value to a number
            value = (int32_t) binaryStringToUint(encoded);
            eDrxSecondsAssigned = edrxNumberToSeconds(value, rat);
        }
        // The last 4-bit binary string is the assigned paging window value
        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
        if (bytesRead == 4) {
            // Convert the encoded value to a number
            value = (int32_t) binaryStringToUint(encoded);
            if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
                pagingWindowSecondsAssigned = U_CELL_PWR_PAGING_WINDOW_DECODED_EUTRAN_TO_SECONDS(value,
                                                                                                 rat);
            } else {
                pagingWindowSecondsAssigned = value;
            }
        }
    }

    if ((pInstance->pSleepContext != NULL) &&
        (pInstance->pSleepContext->pEDrxCallback != NULL)) {
        // Put all the data in a struct and pass a pointer to it to our
        // local callback via the AT client's callback mechanism to decouple
        // it from whatever might have called us.
        // Note: eDrxCallback will free the allocated memory.
        pCallback = (uCellPwrEDrxCallback_t *) pUPortMalloc(sizeof(*pCallback));
        if (pCallback != NULL) {
            pCallback->cellHandle = pInstance->cellHandle;
            pCallback->pCallback = pInstance->pSleepContext->pEDrxCallback;
            pCallback->rat = rat;
            pCallback->onNotOff = (eDrxSecondsAssigned >= 0);
            pCallback->eDrxSecondsRequested = eDrxSecondsRequested;
            pCallback->eDrxSecondsAssigned = eDrxSecondsAssigned;
            pCallback->pagingWindowSecondsAssigned = pagingWindowSecondsAssigned;
            pCallback->pCallbackParam = pInstance->pSleepContext->pEDrxCallbackParam;
            uAtClientCallback(pInstance->atHandle, eDrxCallback, pCallback);
        }
    }
}

// Switch the E-DRX URC on for all RATs where E-DRX is enabled.
static int32_t setEDrxUrc(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t cedrxsRat[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    int32_t value = 0;
    int32_t bytesRead;
    char encoded[4 + 1]; // String representing four binary digits

    for (size_t x = 0; x < sizeof(cedrxsRat) / sizeof(cedrxsRat[0]); x++) {
        cedrxsRat[x] = -1;
    }

    // Read the currently requested E-DRX values
    uAtClientLock(atHandle);
    // Set a short time-out so that we can
    // detect the end of the response quickly
    uAtClientTimeoutSet(atHandle, pInstance->pModule->responseMaxWaitMs);
    uAtClientCommandStart(atHandle, "AT+CEDRXS?");
    uAtClientCommandStop(atHandle);
    for (size_t x = 0; (value >= 0) && (x < sizeof(cedrxsRat) / sizeof(cedrxsRat[0])); x++) {
        uAtClientResponseStart(atHandle, "+CEDRXS:");
        // Read the RAT
        value = uAtClientReadInt(atHandle);
        if ((value >= 0) &&
            (value < (int32_t) (sizeof(gCedrxsRatToCellRat) / sizeof(gCedrxsRatToCellRat[0])))) {
            // Got a valid RAT
            cedrxsRat[x] = value;
            // Read the requested E-DRX value for this RAT
            bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
            if (bytesRead == 4) {
                // Convert the encoded value to seconds
                if (edrxNumberToSeconds((int32_t) binaryStringToUint(encoded),
                                        gCedrxsRatToCellRat[cedrxsRat[x]]) < 0) {
                    // If it doesn't convert, remove the RAT from the list
                    cedrxsRat[x] = -1;
                }
            } else {
                // Not enough characters in the string - remove the RAT from the list
                cedrxsRat[x] = -1;
            }
        }
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    for (size_t x = 0; (x < sizeof(cedrxsRat) / sizeof(cedrxsRat[0])) && (errorCode == 0); x++) {
        if (cedrxsRat[x] >= 0) {
            // For all the RATs that support E-DRX, write the
            // command back again requesting that the URC is
            // emitted; the other settings are remembered by the
            // module and so don't need to be included
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CEDRXS=");
            // 2 means on and with the URC
            uAtClientWriteInt(atHandle, 2);
            // Write the RAT
            uAtClientWriteInt(atHandle, cedrxsRat[x]);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: DEEP SLEEP
 * -------------------------------------------------------------- */

// Callback via which the user's deep sleep wake-up callback is called.
// This is called through the uAtClientCallback() mechanism in order
// to prevent the AT client URC from blocking.
static void deepSleepWakeUpCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellPwrDeepSleepWakeUpCallback_t *pCallback = (uCellPwrDeepSleepWakeUpCallback_t *) pParameter;

    (void) atHandle;

    if (pCallback != NULL) {
        pCallback->pCallback(pCallback->cellHandle, pCallback->pCallbackParam);
        uPortFree(pCallback);
    }
}

// URC for the module's protocol stack entering/leaving deactivated
// mode; not that this doesn't _necessarily_ mean that the module is about
// to enter deep sleep, or woken up from deep sleep in fact.
static void UUPSMR_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    int32_t x;

    x = uAtClientReadInt(atHandle);
    // 0 means waking up, but not necessarily waking up from deep sleep,
    //   any old waking up, so we can't infer anything from that,
    // 1 means that the protocol stack has gone to sleep, which we note
    //   as a state but can't actually use for anything since the module
    //   is likely still responsive to AT commands,
    // 2 means sleep is blocked.
    if (x == 1) {
        pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_PROTOCOL_STACK_ASLEEP;
    }
    pInstance->deepSleepBlockedBy = -1;
    if (x == 2) {
        pInstance->deepSleepBlockedBy = uAtClientReadInt(atHandle);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: POWERING UP/DOWN
 * -------------------------------------------------------------- */

// Check that the cellular module is alive.
static int32_t moduleIsAlive(uCellPrivateInstance_t *pInstance,
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
    if (!pInstance->inWakeUpCallback &&
        (uCellPrivateWakeUpCallback(atHandle, pInstance) == 0)) {
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
                               const char *pAtString,
                               int32_t configurationTries)
{
    bool success = false;
    for (size_t x = configurationTries; (x > 0) && !success; x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, pAtString);
        uAtClientCommandStopReadResponse(atHandle);
        success = (uAtClientUnlock(atHandle) == 0);
    }

    return success;
}

// Configure the cellular module.
static int32_t moduleConfigure(uCellPrivateInstance_t *pInstance,
                               bool andRadioOff, bool returningFromSleep)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_NOT_CONFIGURED;
    bool success = true;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t atStreamHandle;
    uCellPwrPsvMode_t uartPowerSavingMode = U_CELL_PWR_PSV_MODE_DISABLED; // Assume no UART power saving
    uAtClientStream_t atStreamType;
    char buffer[20]; // Enough room for AT+UPSV=2,1300

    // First send all the commands that everyone gets
    for (size_t x = 0;
         (x < sizeof(gpConfigCommand) / sizeof(gpConfigCommand[0])) &&
         success; x++) {
        success = moduleConfigureOne(atHandle, gpConfigCommand[x],
                                     U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
    }

    if (success &&
        (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) ||
         (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6))) {
        // SARA-R4 and LARA-R6 only: switch on the right UCGED mode
        // (SARA-R5 and SARA-U201 have a single mode and require no setting)
        if (U_CELL_PRIVATE_HAS(pInstance->pModule, U_CELL_PRIVATE_FEATURE_UCGED5)) {
            success = moduleConfigureOne(atHandle, "AT+UCGED=5",
                                         U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
        } else {
            success = moduleConfigureOne(atHandle, "AT+UCGED=2",
                                         U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
        }
    }

    atStreamHandle = uAtClientStreamGet(atHandle, &atStreamType);
    if (success && (atStreamType == U_AT_CLIENT_STREAM_TYPE_UART)) {
        // Get the UART stream handle and set the flow
        // control and power saving mode correctly for it
        // TODO: check if AT&K3 requires both directions
        // of flow control to be on or just one of them
        if (uPortUartIsRtsFlowControlEnabled(atStreamHandle) &&
            uPortUartIsCtsFlowControlEnabled(atStreamHandle)) {
            success = moduleConfigureOne(atHandle, "AT&K3",
                                         U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
            if (uAtClientWakeUpHandlerIsSet(atHandle)) {
                // The RTS/CTS handshaking lines are being used
                // for flow control by the UART HW.  This complicates
                // matters for power saving as, at least on SARA-R5
                // (where power saving is a valued feature), the CTS
                // line floats high during sleep, preventing the
                // "wake-up" character being sent to the module to get
                // it out of sleep.

                // Check if this platform supports UPSV power saving
                // at all and if it supports suspension of CTS on a
                // temporary basis
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING) &&
                    (uPortUartCtsSuspend(atStreamHandle) == 0)) {
                    // It does: resume CTS and we can use the wake-up on
                    // TX line feature for power saving
                    uPortUartCtsResume(atStreamHandle);
                    uartPowerSavingMode = U_CELL_PWR_PSV_MODE_DATA;
                }
            }
        } else {
            success = moduleConfigureOne(atHandle, "AT&K0",
                                         U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
            // RTS/CTS handshaking is not used by the UART HW, we
            // can use the wake-up on TX line feature without any
            // complications
            if (uAtClientWakeUpHandlerIsSet(atHandle) &&
                U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)) {
                uartPowerSavingMode = U_CELL_PWR_PSV_MODE_DATA;
            }
        }
    }

    if (success && uAtClientWakeUpHandlerIsSet(atHandle) &&
        (pInstance->pinDtrPowerSaving >= 0) &&
        U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)) {
        // Irrespective of all the above, we permit the user to define
        // and connect this MCU to the module's DTR pin which,
        // on SARA-R5 and SARA-U201, can be used to get out of sleep.
        // This will already have been set by the user calling
        // uCellPwrSetDtrPowerSavingPin().
        uartPowerSavingMode = U_CELL_PWR_PSV_MODE_DTR;
    }

    if (uAtClientWakeUpHandlerIsSet(atHandle) &&
        U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
        // SARA-R4 doesn't support modes 1, 2 or 3 but
        // does support the functionality of mode 1
        // though numbered as mode 4 and without the
        // timeout parameter (the timeout is fixed at
        // 6 seconds) *and* this works even if the flow
        // control lines are connected to a sleeping
        // module: it would appear the module incoming
        // flow control line (CTS) is held low ("on") even
        // while the module is asleep in the SARA-R4 case.
        uartPowerSavingMode = U_CELL_PWR_PSV_MODE_DATA_SARA_R4;
    }

    if (success) {
        // Assemble the UART power saving mode AT command
        if (uartPowerSavingMode == U_CELL_PWR_PSV_MODE_DATA) {
            snprintf(buffer, sizeof(buffer), "AT+UPSV=%d,%d",
                     (int) uartPowerSavingMode,
                     U_CELL_PWR_UART_POWER_SAVING_GSM_FRAMES);
        } else {
            snprintf(buffer, sizeof(buffer), "AT+UPSV=%d", (int) uartPowerSavingMode);
            if (!returningFromSleep &&
                (uartPowerSavingMode == U_CELL_PWR_PSV_MODE_DISABLED) &&
                uAtClientWakeUpHandlerIsSet(atHandle)) {
                // Remove the wake-up handler if it turns out that power
                // saving cannot be supported but leave well alone if
                // we're actually just returning from sleep, this will
                // have already been set up
                uAtClientSetWakeUpHandler(atHandle, NULL, NULL, 0);
            }
        }
        // Use the UART power saving mode AT command to set the mode
        // in the module
        if (!moduleConfigureOne(atHandle, buffer, 1) &&
            uAtClientWakeUpHandlerIsSet(atHandle) &&
            !returningFromSleep) {
            // If AT+UPSV returns error and we're not already returning
            // from sleep then power saving cannot be supported; this is
            // true when the UART interface is actually a virtual UART
            // interface being used from an application that is on-board
            // the module; remove the wake-up handler in this case
            uAtClientSetWakeUpHandler(atHandle, NULL, NULL, 0);
            uPortLog("U_CELL_PWR: power saving not supported.\n");
        }
        // Now tell the AT Client that it should control the
        // DTR pin, if relevant
        if (!returningFromSleep && (uartPowerSavingMode == U_CELL_PWR_PSV_MODE_DTR)) {
            uAtClientSetActivityPin(atHandle, pInstance->pinDtrPowerSaving,
                                    U_CELL_PWR_UART_POWER_SAVING_DTR_READY_MS,
                                    U_CELL_PWR_UART_POWER_SAVING_DTR_HYSTERESIS_MS,
                                    U_CELL_PRIVATE_DTR_POWER_SAVING_PIN_ON_STATE(pInstance->pinStates) == 1 ? true : false);
        }
    }

    if (success) {
        // Switch on the URC for deep sleep if the platform has it
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_DEEP_SLEEP_URC)) {
            success = moduleConfigureOne(atHandle, "AT+UPSMR=1",
                                         U_CELL_PWR_CONFIGURATION_COMMAND_TRIES);
            if (success && !returningFromSleep) {
                // Add the URC handler if it wasn't there before
                uAtClientSetUrcHandler(pInstance->atHandle, "+UUPSMR:",
                                       UUPSMR_urc, pInstance);
            }
        }
        // Update the sleep parameters; note that we ask for the
        // requested 3GPP power saving state here, rather than the
        // assigned, since it might not be assigned by the network
        // at this point but can come along later
        uCellPwrPrivateGet3gppPowerSaving(pInstance, false, NULL, NULL, NULL);
        uCellPrivateSetDeepSleepState(pInstance);
        if (success &&
            U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
            // For SARA-R4, whether the E-DRX URC is on or not does not
            // survive a restart, so need to set it up again here
            success = (setEDrxUrc(pInstance) == 0);
        }
    }

    if (success) {
        // Retrieve and store the current MNO profile
        pInstance->mnoProfile = -1;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UMNOPROF?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UMNOPROF:");
        pInstance->mnoProfile = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        if (andRadioOff) {
            // Switch the radio off until commanded to connect
            // Wait for flip time to expire
            while (uPortGetTickTimeMs() - pInstance->lastCfunFlipTimeMs <
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
                            bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool moduleIsOff = false;
    int32_t startTimeMs = uPortGetTickTimeMs();

    while (!moduleIsOff &&
           (uPortGetTickTimeMs() - startTimeMs < pInstance->pModule->powerDownWaitSeconds * 1000) &&
           ((pKeepGoingCallback == NULL) || pKeepGoingCallback(pInstance->cellHandle))) {
        if (pInstance->pinVInt >= 0) {
            // If we have a VInt pin then wait until that
            // goes to the off state
            moduleIsOff = (uPortGpioGet(pInstance->pinVInt) ==
                           (int32_t) !U_CELL_PRIVATE_VINT_PIN_ON_STATE(pInstance->pinStates));
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
                        bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uPortLog("U_CELL_PWR: powering off with AT command.\n");
    // Sleep is no longer available
    pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
    if (uAtClientWakeUpHandlerIsSet(atHandle)) {
        // Switch off UART power saving first, as it seems to
        // affect the power off process.
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSV=0");
        uAtClientCommandStopReadResponse(atHandle);
        uAtClientUnlock(atHandle);
    }
    // Send the power off command and then pull the power
    uAtClientLock(atHandle);
    // Clear the dynamic parameters
    uCellPrivateClearDynamicParameters(pInstance);
    uAtClientTimeoutSet(atHandle,
                        U_CELL_PRIVATE_CPWROFF_WAIT_TIME_SECONDS * 1000);
    uAtClientCommandStart(atHandle, "AT+CPWROFF");
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    // Wait for the module to power down
    waitForPowerOff(pInstance, pKeepGoingCallback);
    // Now switch off power if possible
    if (pInstance->pinEnablePower >= 0) {
        uPortGpioSet(pInstance->pinEnablePower,
                     (int32_t) !U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
    }
    if (pInstance->pinPwrOn >= 0) {
        uPortGpioSet(pInstance->pinPwrOn,
                     (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
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
                          bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    if (pInstance->pinPwrOn >= 0) {
        // Sleep is no longer available
        pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
        // Power off the module by pulling the PWR_ON pin
        // low for the correct number of milliseconds
        uPortGpioSet(pInstance->pinPwrOn,
                     U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
        uPortTaskBlock(pInstance->pModule->powerOffPullMs);
        uPortGpioSet(pInstance->pinPwrOn,
                     (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
        // Wait for the module to power down
        waitForPowerOff(pInstance, pKeepGoingCallback);
        // Now switch off power if possible
        if (pInstance->pinEnablePower > 0) {
            uPortGpioSet(pInstance->pinEnablePower,
                         (int32_t) !U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
        }
        // Remove any security context as these disappear
        // at power off
        uCellPrivateC2cRemoveContext(pInstance);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO CELLULAR
 * -------------------------------------------------------------- */

// The power on function, separated out here so that it can also be
// used by the sleep code to get us out of 3GPP sleep.
// IMPORTANT: nothing called from here should rely on callbacks
// sent via the uAtClientCallback() mechanism or URCs; these will
// be held back during the time that the module is being woken from
// deep sleep, which would lead to a lock-up if that's what this
// function was called to do.
int32_t uCellPwrPrivateOn(uCellPrivateInstance_t *pInstance,
                          bool (*pKeepGoingCallback) (uDeviceHandle_t),
                          bool allowPrinting)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    int32_t platformError = 0;
    int32_t enablePowerAtStart = 1;
    bool asleepAtStart = (pInstance->deepSleepState == U_CELL_PRIVATE_DEEP_SLEEP_STATE_ASLEEP);
    uDeviceHandle_t cellHandle = pInstance->cellHandle;
    uCellPrivateSleep_t *pSleepContext = pInstance->pSleepContext;
    uCellPwrDeepSleepWakeUpCallback_t *pCallback;

    // We're powering on: set the sleep state to unknown, when
    // we configure the module we will set the sleep state up
    // correctly once more
    pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNKNOWN;
    pInstance->deepSleepBlockedBy = -1;

    if (pInstance->pinEnablePower >= 0) {
        enablePowerAtStart = uPortGpioGet(pInstance->pinEnablePower);
    }
    // For some modules the power-on pulse on PWR_ON and the
    // power-off pulse on PWR_ON are the same duration,
    // in effect a toggle.  To avoid accidentally powering
    // the module off, check if it is already on.
    // Note: doing this even if there is an enable power
    // pin for safety sake
    // Note: also doing this even if we were asleep because the module
    // might be asleep as far as the protocol stack is concerned but
    // not yet actually powered down.
    if (((pInstance->pinVInt >= 0) &&
         (uPortGpioGet(pInstance->pinVInt) == U_CELL_PRIVATE_VINT_PIN_ON_STATE(pInstance->pinStates))) ||
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
                                    !uCellPrivateIsRegistered(pInstance),
                                    asleepAtStart);
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
        if (allowPrinting) {
            uPortLog("U_CELL_PWR: powering on.\n");
        }
        // First, switch on the volts
        if (!asleepAtStart && (pInstance->pinEnablePower >= 0)) {
            platformError = uPortGpioSet(pInstance->pinEnablePower,
                                         U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
        }
        if (platformError == 0) {
            // Wait for things to settle
            uPortTaskBlock(100);

            if (pInstance->pinPwrOn >= 0) {
                // Power the module on by holding the PWR_ON pin in
                // the relevant state for the correct number of milliseconds
                platformError = uPortGpioSet(pInstance->pinPwrOn,
                                             U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                if (platformError == 0) {
                    uPortTaskBlock(pInstance->pModule->powerOnPullMs);
                    // Not bothering with checking return code here
                    // as it would have barfed on the last one if
                    // it were going to
                    uPortGpioSet(pInstance->pinPwrOn,
                                 (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                } else {
                    if (allowPrinting) {
                        uPortLog("U_CELL_PWR: uPortGpioSet() for PWR_ON"
                                 " pin %d returned error code %d.\n",
                                 pInstance->pinPwrOn, platformError);
                    }
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
                // Configure the module, only putting into radio-off
                // mode if we weren't already registered at the start
                // (e.g. we might have been in 3GPP sleep, which retains
                // the registration status)
                errorCode = moduleConfigure(pInstance,
                                            !uCellPrivateIsRegistered(pInstance),
                                            asleepAtStart);
                if (errorCode != 0) {
                    // If the module fails configuration, power it
                    // off and try again
                    quickPowerOff(pInstance, pKeepGoingCallback);
                }
            }
        } else {
            if (allowPrinting) {
                uPortLog("U_CELL_PWR: uPortGpioSet() for enable power"
                         " pin %d returned error code%d.\n",
                         pInstance->pinEnablePower, platformError);
            }
        }
    }

    // If we weren't just sleeping and were off at the start and
    // power-on was unsuccessful then go back to that state
    if (!asleepAtStart && (errorCode != 0) && (enablePowerAtStart == 0)) {
        quickPowerOff(pInstance, pKeepGoingCallback);
    }

    // If we were successful, were asleep at the start and there is
    // a wake-up callback then call it
    if (asleepAtStart && (errorCode == 0) && (pSleepContext != NULL) &&
        (pSleepContext->pWakeUpCallback != NULL)) {
        // Put all the data in a struct and pass a pointer to it to our
        // local callback via the AT client's callback mechanism to decouple
        // it from whatever might have called us.
        // Note: deepSleepWakeUpCallback will free the allocated memory.
        pCallback = (uCellPwrDeepSleepWakeUpCallback_t *) pUPortMalloc(sizeof(*pCallback));
        if (pCallback != NULL) {
            pCallback->cellHandle = pInstance->cellHandle;
            pCallback->pCallback = pSleepContext->pWakeUpCallback;
            pCallback->pCallbackParam = pSleepContext->pWakeUpCallbackParam;
            uAtClientCallback(pInstance->atHandle, deepSleepWakeUpCallback, pCallback);
        }
    }

    return errorCode;
}

// Decode an active time (T3324) string representing the binary value
// of a GPRS Timer 2 IE into seconds.
int32_t uCellPwrPrivateActiveTimeStrToSeconds(const char *pStr, int32_t *pSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t value = -1;
    int32_t multiplier;

    if (strlen(pStr) == 8) {
        // Decode the active time
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        value = *(pStr + 7) - '0';
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 6) - '0') << 1); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 5) - '0') << 2); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 4) - '0') << 3); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 3) - '0') << 4); // *NOPAD*

        multiplier = *(pStr + 2) - '0';
        multiplier += (int32_t) (((unsigned) (int32_t) *(pStr + 1) - '0') << 1); // *NOPAD*
        multiplier += (int32_t) (((unsigned) (int32_t) *pStr - '0') << 2); // *NOPAD*

        switch (multiplier) {
            case 0:
                // 2 seconds
                value = value * 2;
                break;
            case 1:
                // 1 minute
                value = value * 60;
                break;
            case 2:
                // decihours (i.e. 6 minutes)
                value = value * 6 * 60;
                break;
            case 7:
                // Deactivated
                value = -1;
                break;
            default:
                value = -1;
                errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                break;
        }
    }

    if (pSeconds != NULL) {
        *pSeconds = value;
    }

    return errorCode;
}

// Decode a periodic wake-up time (T3412) string representing the binary
// value of a GPRS Timer 3 IE into seconds.
int32_t uCellPwrPrivatePeriodicWakeupStrToSeconds(const char *pStr, bool t3412Ext,
                                                  int32_t *pSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t value = -1;
    int32_t multiplier;

    if (strlen(pStr) == 8) {
        // Decode the TAU period
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        value = *(pStr + 7) - '0';
        // Cast in two stages to keep Lint happy
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 6) - '0') << 1); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 5) - '0') << 2); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 4) - '0') << 3); // *NOPAD*
        value += (int32_t) (((unsigned) (int32_t) *(pStr + 3) - '0') << 4); // *NOPAD*

        multiplier = *(pStr + 2) - '0';
        multiplier += (int32_t) (((unsigned) (int32_t) *(pStr + 1) - '0') << 1); // *NOPAD*
        multiplier += (int32_t) (((unsigned) (int32_t) (*pStr - '0')) << 2);

        if (t3412Ext) {
            switch (multiplier) {
                case 0:
                    // 10 minutes
                    value = value * 10 * 60;
                    break;
                case 1:
                    // 1 hour
                    value = value * 60 * 60;
                    break;
                case 2:
                    // 10 hours
                    value = value * 10 * 60 * 60;
                    break;
                case 3:
                    // 2 seconds
                    value = value * 2;
                    break;
                case 4:
                    // 30 seconds
                    value = value * 30;
                    break;
                case 5:
                    // 1 minute
                    value = value * 60;
                    break;
                case 6:
                    // 320 hours
                    value = value * 320 * 60 * 60;
                    break;
                case 7:
                    // Deactivated
                    value = -1;
                    break;
                default:
                    value = -1;
                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                    break;
            }
        } else {
            switch (multiplier) {
                case 0:
                    // 2 minutes
                    value = value * 2;
                    break;
                case 1:
                    // 1 minute
                    value = value * 60;
                    break;
                case 2:
                    // decihours (i.e. 6 minutes)
                    value = value * 6 * 60;
                    break;
                case 7:
                    // Deactivated
                    value = -1;
                    break;
                default:
                    value = -1;
                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                    break;
            }
        }
    }

    if (pSeconds != NULL) {
        *pSeconds = value;
    }

    return errorCode;
}

// Get the 3GPP power saving settings.
int32_t uCellPwrPrivateGet3gppPowerSaving(uCellPrivateInstance_t *pInstance,
                                          bool assignedNotRequested,
                                          bool *pOnNotOff,
                                          int32_t *pActiveTimeSeconds,
                                          int32_t *pPeriodicWakeupSeconds)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    char ptEncoded[8 + 1] = {0}; // Timer value encoded as 3GPP IE
    char atEncoded[8 + 1] = {0}; // Timer value encoded as 3GPP IE
    int32_t ptLength;
    int32_t atLength;
    int32_t value;
    bool t3412Ext = true;  // Some SARA-R4 modules do not send this parameter, default is T3412_ext
    bool badValueRead = false;
    bool onNotOff = false;;
    int32_t periodicWakeupSeconds = -1;
    int32_t activeTimeSeconds = -1;
    const char *pAtCommandStr = "AT+CPSMS?";
    const char *pAtResponseStr = "+CPSMS:";

    if (assignedNotRequested) {
        pAtCommandStr = "AT+UCPSMS?";
        pAtResponseStr = "+UCPSMS:";
    }

    // +UCPSMS: 1,,,"01000011","01000011",0
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, pAtCommandStr);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, pAtResponseStr);
    value = uAtClientReadInt(atHandle);
    if (value >= 0) {
        onNotOff = (value == 1);
        if (pOnNotOff != NULL) {
            *pOnNotOff = onNotOff;
        }
        if (pInstance->pSleepContext == NULL) {
            // If the 3GPP power saving state is either requested or
            // assigned to be on then make sure we have a sleep context
            // to capture this
            createSleepContext(pInstance);
        }
        if ((pInstance->pSleepContext != NULL) && assignedNotRequested) {
            pInstance->pSleepContext->powerSaving3gppAgreed = onNotOff;
        }
    } else {
        badValueRead = true;
    }
    // Skip over the unused GPRS parameters
    uAtClientSkipParameters(atHandle, 2);
    ptLength = uAtClientReadString(atHandle, ptEncoded, sizeof(ptEncoded), false);
    // This may be absent
    atLength = uAtClientReadString(atHandle, atEncoded, sizeof(atEncoded), false);
    // This may be present if ptEncoded is
    value = uAtClientReadInt(atHandle);
    if (value >= 0) {
        t3412Ext = (value == 1);
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    if (!badValueRead) {
        if (ptLength > 0) {
            errorCode = uCellPwrPrivatePeriodicWakeupStrToSeconds(ptEncoded, t3412Ext,
                                                                  &periodicWakeupSeconds);
        }
        if ((errorCode == 0) && (atLength > 0)) {
            errorCode = uCellPwrPrivateActiveTimeStrToSeconds(atEncoded, &activeTimeSeconds);
        }
        if (pPeriodicWakeupSeconds != NULL) {
            *pPeriodicWakeupSeconds = periodicWakeupSeconds;
        }
        if (pActiveTimeSeconds != NULL) {
            *pActiveTimeSeconds = activeTimeSeconds;
        }
    }

    return errorCode;
}

// Get the E-DRX settings for the given RAT.
int32_t uCellPwrPrivateGetEDrx(const uCellPrivateInstance_t *pInstance,
                               bool assignedNotRequested,
                               uCellNetRat_t rat,
                               bool *pOnNotOff,
                               int32_t *pEDrxSeconds,
                               int32_t *pPagingWindowSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    bool onNotOff = false;
    int32_t eDrxSeconds = -1;
    int32_t pagingWindowSeconds = -1;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_EDRX)) {
        if (assignedNotRequested) {
            errorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
            if (uCellPrivateIsRegistered(pInstance)) {
                // Read the assigned E-DRX value (and hence whether E-DRX
                // is on or off) and the assigned paginging window value
                // using CEDRXRDP
                errorCode = readCedrxsOrCedrxrdp(pInstance, true, rat,
                                                 NULL, NULL, NULL,
                                                 &onNotOff, &eDrxSeconds, &pagingWindowSeconds);
            }
        } else {
            // First read the requested E-DRX value, and hence
            // whether E-DRX is on or off, using CEDRXRDP
            errorCode = readCedrxsOrCedrxrdp(pInstance, true, rat,
                                             &onNotOff, &eDrxSeconds, NULL,
                                             NULL, NULL, NULL);
            if ((errorCode == 0) && onNotOff) {
                // If that worked, try to read the requested
                // paging window value using CEDRXS
                errorCode = readCedrxsOrCedrxrdp(pInstance, false, rat,
                                                 NULL, NULL, &pagingWindowSeconds,
                                                 NULL, NULL, NULL);
            }
        }
        if (errorCode == 0) {
            if (pOnNotOff != NULL) {
                *pOnNotOff = onNotOff;
            }
            if (pEDrxSeconds != NULL) {
                *pEDrxSeconds = eDrxSeconds;
            }
            if (pPagingWindowSeconds != NULL) {
                *pPagingWindowSeconds = pagingWindowSeconds;
            }
            uPortLog("U_CELL_PWR: for RAT %d %s E-DRX is %s, %d second(s),"
                     " paging window %d second(s).\n", (int32_t) rat,
                     assignedNotRequested ? "assigned" : "requested",
                     onNotOff ? "on" : "off", eDrxSeconds, pagingWindowSeconds);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Determine if the cellular module has power.
bool uCellPwrIsPowered(uDeviceHandle_t cellHandle)
{
    bool isPowered = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isPowered = true;
            if (pInstance->pinEnablePower >= 0) {
                isPowered = (uPortGpioGet(pInstance->pinEnablePower) ==
                             U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);

    }

    return isPowered;
}

// Determine if the module is responsive.
bool uCellPwrIsAlive(uDeviceHandle_t cellHandle)
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
int32_t uCellPwrOn(uDeviceHandle_t cellHandle, const char *pSimPinCode,
                   bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_CELL_ERROR_PIN_ENTRY_NOT_SUPPORTED;
            if (pSimPinCode == NULL) {
                errorCode = uCellPwrPrivateOn(pInstance, pKeepGoingCallback, true);
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
int32_t uCellPwrOff(uDeviceHandle_t cellHandle,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
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
int32_t uCellPwrOffHard(uDeviceHandle_t cellHandle, bool trulyHard,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t))
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
                             (int32_t) !U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
                // Remove any security context as these disappear
                // at power off
                uCellPrivateC2cRemoveContext(pInstance);
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                if (pInstance->pinPwrOn >= 0) {
                    if (uAtClientWakeUpHandlerIsSet(atHandle)) {
                        // Switch off UART power saving first, as it seems to
                        // affect the power off process, no error checking,
                        // we're going down anyway
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UPSV=0");
                        uAtClientCommandStopReadResponse(atHandle);
                        uAtClientUnlock(atHandle);
                    }
                    uPortLog("U_CELL_PWR: powering off using the PWR_ON pin.\n");
                    uPortGpioSet(pInstance->pinPwrOn,
                                 U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                    // Power off the module by pulling the PWR_ON pin
                    // to the relevant state for the correct number of
                    // milliseconds
                    uPortTaskBlock(pInstance->pModule->powerOffPullMs);
                    uPortGpioSet(pInstance->pinPwrOn,
                                 (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                    // Clear the dynamic parameters
                    uCellPrivateClearDynamicParameters(pInstance);
                    // Wait for the module to power down
                    waitForPowerOff(pInstance, pKeepGoingCallback);
                    // Now switch off power if possible
                    if (pInstance->pinEnablePower > 0) {
                        uPortGpioSet(pInstance->pinEnablePower,
                                     (int32_t) !U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
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
bool uCellPwrRebootIsRequired(uDeviceHandle_t cellHandle)
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
int32_t uCellPwrReboot(uDeviceHandle_t cellHandle,
                       bool (*pKeepGoingCallback) (uDeviceHandle_t))
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
            while (uPortGetTickTimeMs() - pInstance->lastCfunFlipTimeMs <
                   (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
                uPortTaskBlock(1000);
            }
            // Sleep is no longer available
            pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
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
                // Remove any security context as these disappear at reboot
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
                        // boot in its development version: flush it away
                        uAtClientFlush(atHandle);
                    }
                    // Wait for the module to return to life and configure it
                    errorCode = moduleIsAlive(pInstance,
                                              U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON);
                    if (errorCode == 0) {
                        // Sleep is no longer available
                        pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
                        // Configure the module
                        errorCode = moduleConfigure(pInstance, true, false);
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
                            uPortGpioSet(pInstance->pinPwrOn,
                                         U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                            uPortTaskBlock(pInstance->pModule->powerOffPullMs);
                            uPortGpioSet(pInstance->pinPwrOn,
                                         (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                            // Wait for the module to power down
                            waitForPowerOff(pInstance, pKeepGoingCallback);
                            // Now switch off power if possible
                            if (pInstance->pinEnablePower > 0) {
                                uPortGpioSet(pInstance->pinEnablePower,
                                             (int32_t) !U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
                                // Wait for things to settle
                                uPortTaskBlock(5000);
                            }
                        }
                        // Now power back on again
                        if (pInstance->pinEnablePower >= 0) {
                            uPortGpioSet(pInstance->pinEnablePower,
                                         U_CELL_PRIVATE_ENABLE_POWER_PIN_ON_STATE(pInstance->pinStates));
                            // Wait for things to settle
                            uPortTaskBlock(100);
                        }
                        if (pInstance->pinPwrOn >= 0) {
                            uPortGpioSet(pInstance->pinPwrOn,
                                         U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
                            uPortTaskBlock(pInstance->pModule->powerOnPullMs);
                            uPortGpioSet(pInstance->pinPwrOn,
                                         (int32_t) !U_CELL_PRIVATE_PWR_ON_PIN_TOGGLE_TO_STATE(pInstance->pinStates));
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
int32_t uCellPwrResetHard(uDeviceHandle_t cellHandle, int32_t pinReset)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t platformError;
    uPortGpioConfig_t gpioConfig;
    int64_t startTime;
    int32_t resetHoldMilliseconds;
    int32_t pinResetToggleToState = (pinReset & U_CELL_PIN_INVERTED) ?
                                    !U_CELL_RESET_PIN_TOGGLE_TO_STATE : U_CELL_RESET_PIN_TOGGLE_TO_STATE;
    uPortGpioDriveMode_t pinResetDriveMode;

#ifdef U_CELL_RESET_PIN_DRIVE_MODE
    // User override
    pinResetDriveMode = U_CELL_RESET_PIN_DRIVE_MODE;
#else
    // The drive mode is normally open drain so that we
    // can pull RESET_N low and then let it float
    // afterwards since it is pulled-up by the cellular
    // module
    pinResetDriveMode = U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN;
    if (pinResetToggleToState == 1) {
        // If RESET_N is toggling to 1 then there's an
        // inverter between us and the MCU which only needs
        // normal drive mode.
        pinResetDriveMode = U_PORT_GPIO_DRIVE_MODE_NORMAL;
    }
#endif

    pinReset &= ~U_CELL_PIN_INVERTED;

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
            // Sleep is no longer available
            pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
            // Set the RESET pin to the "reset" state
            platformError = uPortGpioSet(pinReset, pinResetToggleToState);
            if (platformError == 0) {
                // Configure the GPIO to go to this state
                U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                gpioConfig.pin = pinReset;
                gpioConfig.driveMode = pinResetDriveMode;
                gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
                platformError = uPortGpioConfig(&gpioConfig);
                if (platformError == 0) {
                    // Remove any security context as these disappear at reboot
                    uCellPrivateC2cRemoveContext(pInstance);
                    // We have rebooted
                    pInstance->rebootIsRequired = false;
                    startTime = uPortGetTickTimeMs();
                    while (uPortGetTickTimeMs() - startTime < resetHoldMilliseconds) {
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
                        // boot in its development version: flush it away
                        uAtClientFlush(pInstance->atHandle);
                    }
                    // Wait for the module to return to life and configure it
                    pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
                    errorCode = moduleIsAlive(pInstance,
                                              U_CELL_PWR_IS_ALIVE_ATTEMPTS_POWER_ON);
                    if (errorCode == 0) {
                        pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNKNOWN;
                        // Configure the module
                        errorCode = moduleConfigure(pInstance, true, false);
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

// Set the DTR power-saving pin.
int32_t uCellPwrSetDtrPowerSavingPin(uDeviceHandle_t cellHandle, int32_t pin)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uPortGpioConfig_t gpioConfig;
    int32_t pinOnState = (pin & U_CELL_PIN_INVERTED) ? !U_CELL_DTR_PIN_ON_STATE :
                         U_CELL_DTR_PIN_ON_STATE;

    pin &= ~U_CELL_PIN_INVERTED;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL) && (pin >= 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING)) {
                // Set the pin state so that we can use it elsewhere
                if (pinOnState != 0) {
                    pInstance->pinStates |= 1 << U_CELL_PRIVATE_DTR_POWER_SAVING_PIN_BIT_ON_STATE;
                }
                // Set the DTR pin as an output, asserted to prevent sleep
                // initially.  Note that the mode of sleep that uses the DTR
                // pin is a literal switch: DTR must be asserted while this
                // MCU communicates with the module; URCs are always active.
                uPortGpioSet(pin, pinOnState);
                U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
                gpioConfig.pin = pin;
                gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
                errorCode = uPortGpioConfig(&gpioConfig);
                if (errorCode == 0) {
                    pInstance->pinDtrPowerSaving = pin;
                    uPortLog("U_CELL_PWR: pin %d (0x%02x), connected to module DTR"
                             " pin, is being used to control power saving,"
                             " where %d means \"DTR on\" (and hence power"
                             " saving not allowed).\n",
                             pin, pin, pinOnState);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);

    }

    return errorCode;
}

// Get the DTR power-saving pin.
int32_t uCellPwrGetDtrPowerSavingPin(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrPin = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrPin = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrPin = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            if (pInstance->pinDtrPowerSaving >= 0) {
                errorCodeOrPin = pInstance->pinDtrPowerSaving;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrPin;
}

// Set the requested 3GPP power saving parameters.
int32_t  uCellPwrSetRequested3gppPowerSaving(uDeviceHandle_t cellHandle,
                                             uCellNetRat_t rat,
                                             bool onNotOff,
                                             int32_t activeTimeSeconds,
                                             int32_t periodicWakeupSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t value;
    bool justMalloced = false;
    bool onNotOffPrevious = false;
    int32_t activeTimeSecondsPrevious = -1;
    int32_t periodicWakeupSecondsPrevious = -1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL) &&
            (!onNotOff ||
             (activeTimeSeconds >= U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            atHandle = pInstance->atHandle;
            // Must support the feature then, to switch 3GPP
            // power saving on, the AT wake-up callback
            // must be in place (this will be there for UPSV
            // power saving anyway) must be on an EUTRAN RAT
            // for 3GPP sleep, must have a PWR_ON pin (or we
            // could never wake up again) and must also have
            // VInt connected (so that we can tell when we're
            // in deep sleep)
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING) &&
                (!onNotOff || (uAtClientWakeUpHandlerIsSet(atHandle) &&
                               U_CELL_PRIVATE_RAT_IS_EUTRAN(rat) && (pInstance->pinPwrOn >= 0) &&
                               (pInstance->pinVInt >= 0)))) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                // Before we start...
                if (onNotOff &&
                    U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                    // For SARA-R4, the default value of psm_ver will
                    // cause the module to enter 3GPP sleep even
                    // without the network's agreement.  This is not
                    // a good idea, so here we set the first three bits
                    // of psm_ver to binary "100" to stop that
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UPSMVER?");
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UPSMVER:");
                    // Just need the first integer
                    value = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if ((errorCode == 0) && (value >= 0) && ((value & 0x07) != 0x04)) {
                        value = (value & ~0x07) | 0x04;
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UPSMVER=");
                        uAtClientWriteInt(atHandle, value);
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                    }
                }
                if ((errorCode == 0) && onNotOff && (pInstance->pSleepContext == NULL)) {
                    errorCode = createSleepContext(pInstance);
                    if (errorCode == 0) {
                        justMalloced = true;
                    }
                }
                if ((errorCode == 0) &&
                    (!onNotOff || (pInstance->pSleepContext != NULL))) {
                    uCellPwrPrivateGet3gppPowerSaving(pInstance, false, &onNotOffPrevious,
                                                      &activeTimeSecondsPrevious,
                                                      &periodicWakeupSecondsPrevious);
                    errorCode = setPowerSavingMode(pInstance, onNotOff,
                                                   activeTimeSeconds,
                                                   periodicWakeupSeconds);
                    if (errorCode == 0) {
                        if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) &&
                            ((onNotOff != onNotOffPrevious) ||
                             (activeTimeSeconds != activeTimeSecondsPrevious) ||
                             (periodicWakeupSeconds != periodicWakeupSecondsPrevious))) {
                            pInstance->rebootIsRequired = true;
                        }
                    } else {
                        if (justMalloced) {
                            // Clean up on failure
                            uPortFree(pInstance->pSleepContext);
                            pInstance->pSleepContext = NULL;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the requested 3GPP power saving parameters.
int32_t uCellPwrGetRequested3gppPowerSaving(uDeviceHandle_t cellHandle,
                                            bool *pOnNotOff,
                                            int32_t *pActiveTimeSeconds,
                                            int32_t *pPeriodicWakeupSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    bool onNotOff = false;
    int32_t activeTimeSeconds = -1;
    int32_t periodicWakeupSeconds = -1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
                errorCode = uCellPwrPrivateGet3gppPowerSaving(pInstance, false, &onNotOff,
                                                              &activeTimeSeconds,
                                                              &periodicWakeupSeconds);
                if (errorCode == 0) {
                    if (pOnNotOff != NULL ) {
                        *pOnNotOff = onNotOff;
                    }
                    if (pActiveTimeSeconds != NULL) {
                        *pActiveTimeSeconds = activeTimeSeconds;
                    }
                    if (pPeriodicWakeupSeconds != NULL) {
                        *pPeriodicWakeupSeconds = periodicWakeupSeconds;
                    }
                    uPortLog("U_CELL_PWR: requested PSM is %s, active time"
                             " %d second(s), periodic wake-up %d second(s).\n",
                             onNotOff ? "on" : "off", activeTimeSeconds,
                             periodicWakeupSeconds);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the 3GPP power saving parameters as agreed with the network.
int32_t uCellPwrGet3gppPowerSaving(uDeviceHandle_t cellHandle,
                                   bool *pOnNotOff,
                                   int32_t *pActiveTimeSeconds,
                                   int32_t *pPeriodicWakeupSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t periodicWakeupSeconds = -1;
    int32_t activeTimeSeconds = -1;
    bool onNotOff = false;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
                errorCode = uCellPwrPrivateGet3gppPowerSaving(pInstance, true, &onNotOff,
                                                              &activeTimeSeconds,
                                                              &periodicWakeupSeconds);
                if (errorCode == 0) {
                    if (pOnNotOff != NULL ) {
                        *pOnNotOff = onNotOff;
                    }
                    if (pPeriodicWakeupSeconds != NULL) {
                        *pPeriodicWakeupSeconds = periodicWakeupSeconds;
                    }
                    if (pActiveTimeSeconds != NULL) {
                        *pActiveTimeSeconds = activeTimeSeconds;
                    }
                    uPortLog("U_CELL_PWR: PSM is %s, active time %d second(s),"
                             " periodic wake-up %d second(s).\n",
                             onNotOff ? "on" : "off", activeTimeSeconds,
                             periodicWakeupSeconds);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set a callback which will be called when the 3GPP power saving
// parameters are indicated by the network.
int32_t uCellPwrSet3gppPowerSavingCallback(uDeviceHandle_t cellHandle,
                                           void (*pCallback) (uDeviceHandle_t cellHandle,
                                                              bool onNotOff,
                                                              int32_t activeTimeSeconds,
                                                              int32_t periodicWakeupSeconds,
                                                              void *pCallbackParam),
                                           void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pInstance->pSleepContext == NULL) {
                    errorCode = createSleepContext(pInstance);
                }
                if (pInstance->pSleepContext != NULL) {
                    pInstance->pSleepContext->p3gppPowerSavingCallback = pCallback;
                    pInstance->pSleepContext->p3gppPowerSavingCallbackParam = pCallbackParam;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the current state of 3GPP power saving.
uCellPwr3gppPowerSavingState_t uCellPwrGet3gppPowerSavingState(uDeviceHandle_t cellHandle,
                                                               int32_t *pApplication)
{
    uCellPwr3gppPowerSavingState_t powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_UNKNOWN;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
                powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_AVAILABLE;
                if (pInstance->pSleepContext != NULL) {
                    if (pInstance->pSleepContext->powerSaving3gppAgreed) {
                        powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_AGREED_BY_NETWORK;
                        if (pInstance->pSleepContext->powerSaving3gppOnNotOffCereg) {
                            if (pInstance->deepSleepBlockedBy >= 0) {
                                powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_BLOCKED_BY_MODULE;
                                if (pApplication != NULL) {
                                    *pApplication = pInstance->deepSleepBlockedBy;
                                }
                            } else {
                                if ((pInstance->deepSleepState == U_CELL_PRIVATE_DEEP_SLEEP_STATE_PROTOCOL_STACK_ASLEEP) ||
                                    (pInstance->deepSleepState == U_CELL_PRIVATE_DEEP_SLEEP_STATE_ASLEEP)) {
                                    powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_ACTIVE;
                                    if (uCellPrivateIsDeepSleepActive(pInstance)) {
                                        powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_ACTIVE_DEEP_SLEEP_ACTIVE;
                                    }
                                }
                            }
                        } else {
                            powerSavingState3gpp = U_CELL_PWR_3GPP_POWER_SAVING_STATE_BLOCKED_BY_NETWORK;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return powerSavingState3gpp;
}

// Set the requested E-DRX parameters.
int32_t uCellPwrSetRequestedEDrx(uDeviceHandle_t cellHandle,
                                 uCellNetRat_t rat,
                                 bool onNotOff,
                                 int32_t eDrxSeconds,
                                 int32_t pagingWindowSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char encoded[4 + 1]; // String representing four binary digits
    int32_t value;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL) &&
            // Cast in two stages to keep Lint happy
            ((int32_t) rat >= 0) && ((size_t) (int32_t) rat < (sizeof(gCellRatToCedrxsRat) /
                                                               sizeof(gCellRatToCedrxsRat[0])))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            atHandle = pInstance->atHandle;
            // Must support the feature, then to switch E-DRX on
            // the AT wake-up callback must be in place (that
            // will be there for UPSV power saving anyway)
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_EDRX) &&
                (!onNotOff || uAtClientWakeUpHandlerIsSet(atHandle))) {
                // SARA-R4 won't let E-DRX be configured when it is connected
                errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
                if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) ||
                    !uCellPrivateIsRegistered(pInstance)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    // Before we start...
                    if (onNotOff) {
                        // If bit 3 of the UPSMVER command is set then full
                        // 3GPP sleep may be entered in some E-DRX circumstances,
                        // thus losing all of the module-based IP/MQTT
                        // context information.
                        // This is not a good idea, so switch off that flag here
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UPSMVER?");
                        uAtClientCommandStop(atHandle);
                        uAtClientResponseStart(atHandle, "+UPSMVER:");
                        // Just need the first integer
                        value = uAtClientReadInt(atHandle);
                        uAtClientResponseStop(atHandle);
                        // Note: don't set errorCode here as SARA-R5xx-00B
                        // doesn't support AT+UPSMVER
                        if ((uAtClientUnlock(atHandle) == 0) && (value >= 0) && ((value & 0x08) != 0)) {
                            // If bit 3 is 1, set it to 0
                            value &= ~0x08;
                            uAtClientLock(atHandle);
                            uAtClientCommandStart(atHandle, "AT+UPSMVER=");
                            uAtClientWriteInt(atHandle, value);
                            uAtClientCommandStopReadResponse(atHandle);
                            errorCode = uAtClientUnlock(atHandle);
                        }
                    }
                    if (errorCode == 0) {
                        // NOTE: E-DRX doesn't need the sleep context unless the E-DRX
                        // callback is set, hence one is not checked for or created here
                        // +CEDRXS: 1,,"0111","0001"
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+CEDRXS=");
                        value = 0; // 0 means off
                        if (onNotOff) {
                            value = 2; // 2 means on and with the URC
                        }
                        uAtClientWriteInt(atHandle, value);
                        // Write the RAT
                        uAtClientWriteInt(atHandle, (int32_t) gCellRatToCedrxsRat[(int32_t) rat]);
                        if (onNotOff) {
                            value = edrxSecondsToNumber(eDrxSeconds, rat);
                            uintToBinaryString(value, encoded, sizeof(encoded), 4);
                            encoded[4] = 0;
                            // Write the E-DRX value
                            uAtClientWriteString(atHandle, encoded, true);
                            // Write the paging window value, if supported
                            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING_PAGING_WINDOW_SET)) {
                                if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
                                    if (rat == U_CELL_NET_RAT_NB1) {
                                        value = pagingWindowSeconds * 100 / 256;
                                    } else {
                                        value = pagingWindowSeconds * 100 / 128;
                                    }
                                } else {
                                    value = pagingWindowSeconds;
                                }
                                uintToBinaryString(value, encoded, sizeof(encoded), 4);
                                encoded[4] = 0;
                                // Write the paging window value
                                uAtClientWriteString(atHandle, encoded, true);
                            }
                        }
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if ((errorCode == 0) &&
                            U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                            pInstance->rebootIsRequired = true;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the requested E-DRX parameters.
int32_t uCellPwrGetRequestedEDrx(uDeviceHandle_t cellHandle,
                                 uCellNetRat_t rat,
                                 bool *pOnNotOff,
                                 int32_t *pEDrxSeconds,
                                 int32_t *pPagingWindowSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = uCellPwrPrivateGetEDrx(pInstance, false, rat,
                                               pOnNotOff, pEDrxSeconds,
                                               pPagingWindowSeconds);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the current E-DRX parameters as agreed with the network.
int32_t uCellPwrGetEDrx(uDeviceHandle_t cellHandle,
                        uCellNetRat_t rat,
                        bool *pOnNotOff,
                        int32_t *pEDrxSeconds,
                        int32_t *pPagingWindowSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = uCellPwrPrivateGetEDrx(pInstance, true, rat,
                                               pOnNotOff, pEDrxSeconds,
                                               pPagingWindowSeconds);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set a callback which will be called when the EDRX parameters change.
int32_t uCellPwrSetEDrxCallback(uDeviceHandle_t cellHandle,
                                void (*pCallback) (uDeviceHandle_t cellHandle,
                                                   uCellNetRat_t rat,
                                                   bool onNotOff,
                                                   int32_t eDrxSecondsRequested,
                                                   int32_t eDrxSecondsAssigned,
                                                   int32_t pagingWindowSecondsAssigned,
                                                   void *pCallbackParam),
                                void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_EDRX)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pInstance->pSleepContext == NULL) {
                    errorCode = createSleepContext(pInstance);
                }
                if (pInstance->pSleepContext != NULL) {
                    pInstance->pSleepContext->pEDrxCallback = pCallback;
                    pInstance->pSleepContext->pEDrxCallbackParam = pCallbackParam;
                    if (pCallback != NULL) {
                        uAtClientSetUrcHandler(pInstance->atHandle, "+CEDRXP:",
                                               CEDRXP_urc, pInstance);
                    } else {
                        uAtClientRemoveUrcHandler(pInstance->atHandle, "+CEDRXP:");
                    }
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set callback for wake-up from deep sleep.
int32_t uCellPwrSetDeepSleepWakeUpCallback(uDeviceHandle_t cellHandle,
                                           void (*pCallback) (uDeviceHandle_t cellHandle,
                                                              void *pCallbackParam),
                                           void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // Must have a PWR_ON pin (in order to wake up from
            // sleep). Must also have VInt connected.
            if ((pInstance->pinPwrOn >= 0) && (pInstance->pinVInt >= 0)) {
                if (pInstance->pSleepContext == NULL) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pInstance->pSleepContext = (uCellPrivateSleep_t *) pUPortMalloc(sizeof(uCellPrivateSleep_t));
                    if (pInstance->pSleepContext != NULL) {
                        memset(pInstance->pSleepContext, 0,
                               sizeof(*(pInstance->pSleepContext)));
                    }
                }
                if (pInstance->pSleepContext != NULL) {
                    pInstance->pSleepContext->pWakeUpCallback = pCallback;
                    pInstance->pSleepContext->pWakeUpCallbackParam = pCallbackParam;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get whether deep sleep is currently active or not.
int32_t uCellPwrGetDeepSleepActive(uDeviceHandle_t cellHandle, bool *pSleepActive)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING) &&
                (pInstance->pinVInt >= 0)) {
                if (pSleepActive != NULL) {
                    *pSleepActive = uCellPrivateIsDeepSleepActive(pInstance);
                }
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Wake the module from deep sleep.
int32_t uCellPwrWakeUpFromDeepSleep(uDeviceHandle_t cellHandle,
                                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    return uCellPwrOn(cellHandle, NULL, pKeepGoingCallback);
}

// Disable 32 kHz sleep.
int32_t uCellPwrDisableUartSleep(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPrivateUartSleepCache_t *pUartSleepCache;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            pUartSleepCache = &(pInstance->uartSleepCache);
            // If a wake-up handler has been set then the module supports
            // UART sleep, if it has not then it doesn't and we can say so
            atHandle = pInstance->atHandle;
            // If a sleep handler is not set then sleep is already
            // disabled, so that's fine
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (uAtClientWakeUpHandlerIsSet(atHandle)) {
                // Read and stash the current UART sleep parameters
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UPSV?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UPSV:");
                pUartSleepCache->mode = uAtClientReadInt(atHandle);
                if (pUartSleepCache->mode == 1) {
                    // Mode 1 has a time attached
                    pUartSleepCache->sleepTime = uAtClientReadInt(atHandle);
                }
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
                if (errorCode == 0) {
                    // Now switch off sleep and remove the handler,
                    // so that everyone knows sleep is gone
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UPSV=");
                    uAtClientWriteInt(atHandle, 0);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        uAtClientSetWakeUpHandler(atHandle, NULL, NULL, 0);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Enable 32 kHz sleep.
int32_t uCellPwrEnableUartSleep(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPrivateUartSleepCache_t *pUartSleepCache;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            pUartSleepCache = &(pInstance->uartSleepCache);
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            atHandle = pInstance->atHandle;
            if (uAtClientWakeUpHandlerIsSet(atHandle)) {
                // If the sleep handler is set the sleep is already
                // enabled, there is nothing to do
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                // If no sleep handler is set then either sleep
                // is not supported or it has been disabled:
                // if it has been disabled then the cache
                // will contain the previous mode so check it
                if (pUartSleepCache->mode > 0) {
                    // There is a cached mode, put it back again
#ifndef U_CFG_CELL_DISABLE_UART_POWER_SAVING
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UPSV=");
                    uAtClientWriteInt(atHandle, pUartSleepCache->mode);
                    if (pUartSleepCache->mode == 1) {
                        // Mode 1 has a time
                        uAtClientWriteInt(atHandle, pUartSleepCache->sleepTime);
                    }
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        // Empty the cache so that we know sleep
                        // has been re-enabled
                        pUartSleepCache->mode = 0;
                        pUartSleepCache->sleepTime = 0;
                        uAtClientSetWakeUpHandler(atHandle, uCellPrivateWakeUpCallback, pInstance,
                                                  (U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS * 1000) -
                                                  U_CELL_POWER_SAVING_UART_WAKEUP_MARGIN_MILLISECONDS);
                    } else {
                        // Return a clearer error code than "AT error"
                        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                    }
#endif
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}


// Determine whether UART, AKA 32 kHz, sleep is enabled or not.
bool uCellPwrUartSleepIsEnabled(uDeviceHandle_t cellHandle)
{
    bool isEnabled = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pInstance->pModule != NULL)) {
            isEnabled = uAtClientWakeUpHandlerIsSet(pInstance->atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isEnabled;
}


// End of file
