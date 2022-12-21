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
 * @brief Implementation of the info API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "errno.h"
#include "limits.h"    // INT_MAX
#include "stdlib.h"    // strol(), atoi(), strol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen()
#include "time.h"      // struct tm

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" // strtok_r() and, in some cases, isblank()
#include "u_port_clib_mktime64.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_info.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert RSRP in 3GPP TS 36.133 format to dBm.
// Returns 0 if the number is not known.
// 0: -141 dBm or less,
// 1..96: from -140 dBm to -45 dBm with 1 dBm steps,
// 97: -44 dBm or greater,
// 255: not known or not detectable.
static int32_t rsrpToDbm(int32_t rsrp)
{
    int32_t rsrpDbm = 0;

    if ((rsrp >= 0) && (rsrp <= 97)) {
        rsrpDbm = rsrp - (97 + 44);
        if (rsrpDbm < -141) {
            rsrpDbm = -141;
        }
    }

    return rsrpDbm;
}

// Convert RSRQ in 3GPP TS 36.133 format to dB.
// Returns 0x7FFFFFFF if the number is not known.
// -30: less than -34 dB
// -29..46: from -34 dB to 2.5 dB with 0.5 dB steps
//          where 0 is -19.5 dB
// 255: not known or not detectable.
static int32_t rsrqToDb(int32_t rsrq)
{
    int32_t rsrqDb = 0x7FFFFFFF;

    if ((rsrq >= -30) && (rsrq <= 46)) {
        rsrqDb = (rsrq - 39) / 2;
        if (rsrqDb < -34) {
            rsrqDb = -34;
        }
    }

    return rsrqDb;
}

// Convert the UTRAN RSSI number in 3GPP TS 25.133 format to dBm.
// Returns 0x7FFFFFFF if the number is not known.
// 0:     less than -100 dBm
// 1..75: from -100 to -25 dBm with 1 dBm steps
// 76:    -25 dBm or greater
// 255:   not known or not detectable
static int32_t rssiUtranToDbm(int32_t rssi)
{
    int32_t rssiDbm = 0x7FFFFFFF;

    if ((rssi >= 0) && (rssi <= 76)) {
        rssiDbm = (rssi - 100);
        if (rssiDbm < -25) {
            rssiDbm = -25;
        }
    }

    return rssiDbm;
}


// Get an ID string from the cellular module.
static int32_t getString(uAtClientHandle_t atHandle,
                         const char *pCmd, char *pBuffer,
                         size_t bufferSize)
{
    int32_t errorCodeOrSize;
    int32_t bytesRead;
    char delimiter;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, pCmd);
    uAtClientCommandStop(atHandle);
    // Don't want characters in the string being interpreted
    // as delimiters
    delimiter = uAtClientDelimiterGet(atHandle);
    uAtClientDelimiterSet(atHandle, '\x00');
    uAtClientResponseStart(atHandle, NULL);
    bytesRead = uAtClientReadString(atHandle, pBuffer,
                                    bufferSize, false);
    uAtClientResponseStop(atHandle);
    // Restore the delimiter
    uAtClientDelimiterSet(atHandle, delimiter);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((bytesRead >= 0) && (errorCodeOrSize == 0)) {
        uPortLog("U_CELL_INFO: ID string, length %d character(s),"
                 " returned by %s is \"%s\".\n",
                 bytesRead, pCmd, pBuffer);
        errorCodeOrSize = bytesRead;
    } else {
        errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
        uPortLog("U_CELL_INFO: unable to read ID string using"
                 " %s.\n", pCmd);
    }

    return errorCodeOrSize;
}

// Fill in the radio parameters the AT+CSQ way
static int32_t getRadioParamsCsq(uAtClientHandle_t atHandle,
                                 uCellPrivateRadioParameters_t *pRadioParameters)
{
    int32_t errorCode;
    int32_t x;
    int32_t y;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CSQ");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CSQ:");
    x = uAtClientReadInt(atHandle);
    y = uAtClientReadInt(atHandle);
    if (y == 99) {
        y = -1;
    }
    uAtClientResponseStop(atHandle);
    errorCode = uAtClientUnlock(atHandle);

    if (errorCode == 0) {
        if ((x >= 0) && (x <= 31)) {
            pRadioParameters->rssiDbm =  -(113 - (x * 2));
        }
        pRadioParameters->rxQual = y;
    }

    return errorCode;
}

// Fill in the radio parameters the AT+UCGED=2 way, SARA-R5 flavour
static int32_t getRadioParamsUcged2SaraR5(uAtClientHandle_t atHandle,
                                          uCellPrivateRadioParameters_t *pRadioParameters)
{
    int32_t x;

    // +UCGED: 2
    // <rat>,<svc>,<MCC>,<MNC>
    // <earfcn>,<Lband>,<ul_BW>,<dl_BW>,<tac>,<LcellId>,<PCID>,<mTmsi>,<mmeGrId>,<mmeCode>, <rsrp>,<rsrq>,<Lsinr>,<Lrrc>,<RI>,<CQI>,<avg_rsrp>,<totalPuschPwr>,<avgPucchPwr>,<drx>, <l2w>,<volte_mode>[,<meas_gap>,<tti_bundling>]
    // e.g.
    // 6,4,001,01
    // 2525,5,50,50,e8fe,1a2d001,1,d60814d1,8001,01,28,31,13.75,3,1,10,28,-50,-6,0,255,255,0
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UCGED?");
    uAtClientCommandStop(atHandle);
    // The line with just "+UCGED: 2" on it
    uAtClientResponseStart(atHandle, "+UCGED:");
    uAtClientSkipParameters(atHandle, 1);
    // Don't want anything from the next line
    uAtClientResponseStart(atHandle, NULL);
    uAtClientSkipParameters(atHandle, 4);
    // Now the line of interest
    uAtClientResponseStart(atHandle, NULL);
    // EARFCN is the first integer
    pRadioParameters->earfcn = uAtClientReadInt(atHandle);
    // Skip <Lband>, <ul_BW>, <dl_BW>, <tac> and <LcellId>
    uAtClientSkipParameters(atHandle, 5);
    // Read <PCID>
    pRadioParameters->cellId = uAtClientReadInt(atHandle);
    // Skip <mTmsi>, <mmeGrId> and <mmeCode>
    uAtClientSkipParameters(atHandle, 3);
    // RSRP is element 11, coded as specified in TS 36.133
    pRadioParameters->rsrpDbm = rsrpToDbm(uAtClientReadInt(atHandle));
    // RSRQ is element 12, coded as specified in TS 36.133.
    x = uAtClientReadInt(atHandle);
    if (uAtClientErrorGet(atHandle) == 0) {
        // Note that this can be a negative integer, hence
        // we check for errors here so as not to mix up
        // what might be a negative error code with a
        // negative return value.
        pRadioParameters->rsrqDb = rsrqToDb(x);
    }
    uAtClientResponseStop(atHandle);

    return uAtClientUnlock(atHandle);
}

// Fill in the radio parameters the AT+UCGED=2 way, SARA-R422 flavour
static int32_t getRadioParamsUcged2SaraR422(uAtClientHandle_t atHandle,
                                            uCellPrivateRadioParameters_t *pRadioParameters)
{
    int32_t x;
    int32_t y;

    // +UCGED: 2
    // <rat>,<MCC>,<MNC>
    // <EARFCN>,<Lband>,<ul_BW>,<dl_BW>,<TAC>,<P-CID>,<RSRP_value>,<RSRQ_value>,<NBMsinr>,<esm_cause>,<emm_state>,<tx_pwr>,<drx_cycle_len>,<tmsi>
    // e.g.
    // 6,310,410
    // 5110,12,10,10,830e,162,-86,-14,131,-1,3,255,128,"FB306E02"
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UCGED?");
    uAtClientCommandStop(atHandle);
    // The line with just "+UCGED: 2" on it
    uAtClientResponseStart(atHandle, "+UCGED:");
    uAtClientSkipParameters(atHandle, 1);
    // Don't want anything from the next line
    uAtClientResponseStart(atHandle, NULL);
    uAtClientSkipParameters(atHandle, 3);
    // Now the line of interest
    uAtClientResponseStart(atHandle, NULL);
    // EARFCN is the first integer
    pRadioParameters->earfcn = uAtClientReadInt(atHandle);
    // Skip <Lband>, <ul_BW>, <dl_BW> and <TAC>
    uAtClientSkipParameters(atHandle, 4);
    // Read <P-CID>
    pRadioParameters->cellId = uAtClientReadInt(atHandle);
    // RSRP is element 7, as a plain-old dBm value
    x = uAtClientReadInt(atHandle);
    // RSRQ is element 8, as a plain-old dB value.
    y = uAtClientReadInt(atHandle);
    if (uAtClientErrorGet(atHandle) == 0) {
        // Note that these last two are usually negative
        // integers, hence we check for errors here so as
        // not to mix up what might be a negative error
        // code with a negative return value.
        pRadioParameters->rsrpDbm = x;
        pRadioParameters->rsrqDb = y;
    }
    uAtClientResponseStop(atHandle);

    return uAtClientUnlock(atHandle);
}

// Fill in the radio parameters the AT+UCGED=2 way, LARA-R6 flavour
static int32_t getRadioParamsUcged2LaraR6(uAtClientHandle_t atHandle,
                                          uCellPrivateRadioParameters_t *pRadioParameters)
{
    int32_t rat;
    int32_t skipParameters = 2;
    int32_t x;
    int32_t y;

    // The formats are RAT dependent as follows:
    //
    // 2G:
    //
    // +UCGED: 2
    // 2,<MCC>,<MNC>
    // <arfcn>,<band1900>,<GcellId>,<BSIC>,<Glac>,<Grac>,<RxLev>,<t_adv>,<C1>,<C2>,<NMO>,<channel_type>
    // (lines may follow with neighbour cell information in them, which we will ignore)
    // e.g.
    // 2,222,1
    // 1009,0,5265,11,d5bd,00,36,-1,30,30,1,1
    //
    // 3G:
    //
    // +UCGED: 2
    // 3,<svc>,<MCC>,<MNC>
    // <uarfcn>,<Wband>,<WcellId>,<Wlac>,<Wrac>,<scrambling_code>,<Wrrc>,<rssi>,<ecn0_lev>,<Wspeech_mode>
    // e.g.
    // 3,4,001,01
    // 4400,5,0000000,0000,80,9,4,62,42,255
    //
    // LTE:
    //
    // +UCGED: 2
    // 4,<svc>,<MCC>,<MNC>
    // <EARFCN>,<Lband>,<ul_BW>,<dl_BW>,<TAC>,<LcellId>,<P-CID>,<mTmsi>,<mmeGrId>,<mmeCode>,<RSRP>,<RSRQ>... etc.
    // e.g.
    // 4,0,001,01
    // 2525,5,25,50,2b67,69f6bc7,111,00000000,ffff,ff,67,19,0.00,255,255,255,67,11,255,0,255,255,0,0
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UCGED?");
    uAtClientCommandStop(atHandle);
    // The line with just "+UCGED: 2" on it
    uAtClientResponseStart(atHandle, "+UCGED:");
    uAtClientSkipParameters(atHandle, 1);
    // Read the RAT from the next line and skip the rest
    uAtClientResponseStart(atHandle, NULL);
    rat = uAtClientReadInt(atHandle);
    if (rat > 2) {
        skipParameters = 3;
    }
    uAtClientSkipParameters(atHandle, skipParameters);
    // Now the main line of interest
    uAtClientResponseStart(atHandle, NULL);
    switch (rat) {
        case 2:
            // ARFCN is the first integer
            pRadioParameters->earfcn = uAtClientReadInt(atHandle);
            // Skip <band1900>
            uAtClientSkipParameters(atHandle, 1);
            // Read <GcellId>
            pRadioParameters->cellId = uAtClientReadInt(atHandle);
            // Ignore the rest; rssiDbm will have come in via CSQ
            break;
        case 3:
            // UARFCN is the first integer
            pRadioParameters->earfcn = uAtClientReadInt(atHandle);
            // Skip <Wband>
            uAtClientSkipParameters(atHandle, 1);
            // Read <WcellId>
            pRadioParameters->cellId = uAtClientReadInt(atHandle);
            // Skip <Wlac>, <Wrac>, <scrambling_code> and <Wrrc>
            uAtClientSkipParameters(atHandle, 4);
            // Read <rssi> and convert it to dBm
            pRadioParameters->rssiDbm = rssiUtranToDbm(uAtClientReadInt(atHandle));
            // Ignore the rest
            break;
        case 4:
            // EARFCN is the first integer
            pRadioParameters->earfcn = uAtClientReadInt(atHandle);
            // Skip <Lband>, <ul_BW>, <dl_BW>, <TAC> and <LcellId>
            uAtClientSkipParameters(atHandle, 5);
            // Read <P-CID>
            pRadioParameters->cellId = uAtClientReadInt(atHandle);
            // Skip <mTmsi>, <mmeGrId> and <mmeCode>
            uAtClientSkipParameters(atHandle, 3);
            // RSRP is element 11, as a plain-old dBm value
            x = uAtClientReadInt(atHandle);
            // RSRQ is element 12, as a plain-old dB value.
            y = uAtClientReadInt(atHandle);
            if (uAtClientErrorGet(atHandle) == 0) {
                // Note that these last two are usually negative
                // integers, hence we check for errors here so as
                // not to mix up what might be a negative error
                // code with a negative return value.
                pRadioParameters->rsrpDbm = x;
                pRadioParameters->rsrqDb = y;
            }
            break;
        default:
            break;
    }
    uAtClientResponseStop(atHandle);

    return uAtClientUnlock(atHandle);
}

// Turn a string such as "-104.20", i.e. a signed
// decimal floating point number, into an int32_t.
static int32_t strToInt32(const char *pString)
{
    char *pEnd = NULL;
    int32_t value;

    value = strtol(pString, &pEnd, 10);
    if (pEnd == pString) {
        value = 0;
    } else if ((pEnd != NULL) && (strlen(pEnd) > 1) && (*pEnd == '.') &&
               (*(pEnd + 1) >= '5') && (*(pEnd + 1) <= '9')) {
        if (value >= 0) {
            value++;
        } else {
            value--;
        }
    }

    return value;
}

// Fill in the radio parameters the AT+UCGED=5 way
static int32_t getRadioParamsUcged5(uAtClientHandle_t atHandle,
                                    uCellPrivateRadioParameters_t *pRadioParameters)
{
    char buffer[16];

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UCGED?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+RSRP:");
    pRadioParameters->cellId = uAtClientReadInt(atHandle);
    pRadioParameters->earfcn = uAtClientReadInt(atHandle);
    if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
        pRadioParameters->rsrpDbm = strToInt32(buffer);
    }
    uAtClientResponseStart(atHandle, "+RSRQ:");
    // Skip past cell ID and EARFCN since they will be the same
    uAtClientSkipParameters(atHandle, 2);
    if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
        pRadioParameters->rsrqDb = strToInt32(buffer);
    }
    uAtClientResponseStop(atHandle);
    return uAtClientUnlock(atHandle);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Refresh the RF status values;
int32_t uCellInfoRefreshRadioParameters(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPrivateRadioParameters_t *pRadioParameters;
    uAtClientHandle_t atHandle;
    uCellNetRat_t rat;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
            atHandle = pInstance->atHandle;
            pRadioParameters = &(pInstance->radioParameters);
            uCellPrivateClearRadioParameters(pRadioParameters);
            if (uCellPrivateIsRegistered(pInstance)) {
                // The mechanisms to get the radio information
                // are different between EUTRAN and GERAN but
                // AT+CSQ works in all cases though it sometimes
                // doesn't return a reading.  Collect what we can
                // with it
                errorCode = getRadioParamsCsq(atHandle, pRadioParameters);
                // Note that AT+UCGED is used next rather than AT+CESQ
                // as, in my experience, it is more reliable in
                // reporting answers.
                // Allow a little sleepy-byes here, don't want to overtask
                // the module if this is being called repeatedly
                uPortTaskBlock(500);
                if (U_CELL_PRIVATE_HAS(pInstance->pModule, U_CELL_PRIVATE_FEATURE_UCGED5)) {
                    // SARA-R4 (except 422) only supports UCGED=5, and it only
                    // supports it in EUTRAN mode
                    rat = uCellPrivateGetActiveRat(pInstance);
                    if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
                        errorCode = getRadioParamsUcged5(atHandle, pRadioParameters);
                    } else {
                        // Can't use AT+UCGED, that's all we can get
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    // The AT+UCGED=2 formats are module-specific
                    switch (pInstance->pModule->moduleType) {
                        case U_CELL_MODULE_TYPE_SARA_R5:
                            errorCode = getRadioParamsUcged2SaraR5(atHandle, pRadioParameters);
                            break;
                        case U_CELL_MODULE_TYPE_SARA_R422:
                            errorCode = getRadioParamsUcged2SaraR422(atHandle, pRadioParameters);
                            break;
                        case U_CELL_MODULE_TYPE_LARA_R6:
                            errorCode = getRadioParamsUcged2LaraR6(atHandle, pRadioParameters);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (errorCode == 0) {
                uPortLog("U_CELL_INFO: radio parameters refreshed:\n");
                uPortLog("             RSSI:    %d dBm\n", pRadioParameters->rssiDbm);
                uPortLog("             RSRP:    %d dBm\n", pRadioParameters->rsrpDbm);
                uPortLog("             RSRQ:    %d dB\n", pRadioParameters->rsrqDb);
                uPortLog("             RxQual:  %d\n", pRadioParameters->rxQual);
                uPortLog("             cell ID: %d\n", pRadioParameters->cellId);
                uPortLog("             EARFCN:  %d\n", pRadioParameters->earfcn);
            } else {
                uPortLog("U_CELL_INFO: unable to refresh radio parameters.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the RSSI.
int32_t uCellInfoGetRssiDbm(uDeviceHandle_t cellHandle)
{
    // Zero is the error code here as negative values are valid
    int32_t errorCodeOrValue = 0;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.rssiDbm;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the RSRP.
int32_t uCellInfoGetRsrpDbm(uDeviceHandle_t cellHandle)
{
    // Zero is the error code here as negative values are valid
    int32_t errorCodeOrValue = 0;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.rsrpDbm;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the RSRQ.
int32_t uCellInfoGetRsrqDb(uDeviceHandle_t cellHandle)
{
    // 0x7FFFFFFF is the error code here as negative and small
    // positive values are valid
    int32_t errorCodeOrValue = 0x7FFFFFFF;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.rsrqDb;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the RxQual.
int32_t uCellInfoGetRxQual(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.rxQual;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the SNR.
// The fixed-pointed calculation below was created and confirmed
// by @mazgch with the following test code:
//
// #include <stdio.h>
// #include <stdlib.h>
// #include <math.h>
// #include <time.h>
//
// int snrOld(int rssiDbm, int rsrpDbm) {
//     double rssi = pow(10.0, ((double) rssiDbm) / 10);
//     double rsrp = pow(10.0, ((double) rsrpDbm) / 10);
//     double snrDb = 10 * log10(rsrp / (rssi - rsrp));
//     return (int) round(snrDb);
// }
//
//  // Calculate the SNR from DBm values of Received Signal Strength Indicator (RSSI)
//  //   @note rssiDbm should be greather than rsrpDbm otherwise this function fails and returns -2147483648
//  //   @param rssiDbm Received Signal Strength Indicator (RSSI) in dBm
//  //   @param rsrpDbm Reference Signal Received Power (RSRP) in dBm
//  //   @returns the calculated SNR rounded to nearest integer
//
// int snrNew(int rssiDbm, int rsrpDbm) {
//     const char snrLut[] = {6, 2, 0, -2, -3, -5, -6, -7, -8, -10};
//     int ix = rssiDbm - rsrpDbm - 1;
//     return  (ix < 0) ? -2147483648 :
//             (ix < sizeof(snrLut)) ? snrLut[ix] : (- ix - 1);
// }
//
// int main()
// {
//     srand (time(NULL));
//     int from = -10;
//     int to = -130;
//
//     // test a random number from the range above
//     int32_t rssiDbm = to + rand() % (from    - to);
//     int32_t rsrpDbm = to + rand() % (rssiDbm - to);
//     int snr = snrOld(rssiDbm, rsrpDbm);
//     printf("%d %d -> %i = %i", rssiDbm, rsrpDbm, snrOld(rssiDbm, rsrpDbm), snrNew(rssiDbm, rsrpDbm));
//
//     // test the full range and make sure all the return values of the two functions are the same
//     for (int i = from; i >= to; i --) {
//         for (int r = from; r >= to; r --) {
//             int s = snrOld(i, r);
//             int x = snrNew(i, r);
//             if (x != s)
//                 printf("ERROR rssiDbm %i rsrpDbm %i oldSnr %i newSnr %i\n", i, r, s, x);
//         }
//     }
//
//     return 0;
// }
int32_t uCellInfoGetSnrDb(uDeviceHandle_t cellHandle, int32_t *pSnrDb)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellPrivateRadioParameters_t *pRadioParameters;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pSnrDb != NULL)) {
            pRadioParameters = &(pInstance->radioParameters);
            errorCode = (int32_t) U_CELL_ERROR_VALUE_OUT_OF_RANGE;
            // SNR = RSRP / (RSSI - RSRP).
            if ((pRadioParameters->rssiDbm != 0) &&
                (pRadioParameters->rssiDbm <= pRadioParameters->rsrpDbm)) {
                *pSnrDb = INT_MAX;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else if ((pRadioParameters->rssiDbm != 0) && (pRadioParameters->rsrpDbm != 0)) {
                int32_t ix = pRadioParameters->rssiDbm - (pRadioParameters->rsrpDbm + 1);
                if (ix >= 0) {
                    const signed char snrLut[] = {6, 2, 0, -2, -3, -5, -6, -7, -8, -10};
                    *pSnrDb = (ix < (int32_t) sizeof(snrLut)) ? snrLut[ix] : (- ix - 1);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the cell ID.
int32_t uCellInfoGetCellId(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.cellId;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the EARFCN.
int32_t uCellInfoGetEarfcn(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrValue = pInstance->radioParameters.earfcn;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the IMEI of the cellular module.
int32_t uCellInfoGetImei(uDeviceHandle_t cellHandle,
                         char *pImei)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pImei != NULL)) {
            errorCode = uCellPrivateGetImei(pInstance, pImei);
            if (errorCode == 0) {
                uPortLog("U_CELL_INFO: IMEI is %.*s.\n",
                         U_CELL_INFO_IMEI_SIZE, pImei);
            } else {
                uPortLog("U_CELL_INFO: unable to read IMEI.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the IMSI of the SIM in the cellular module.
int32_t uCellInfoGetImsi(uDeviceHandle_t cellHandle,
                         char *pImsi)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pImsi != NULL)) {
            errorCode = uCellPrivateGetImsi(pInstance, pImsi);
            if (errorCode == 0) {
                uPortLog("U_CELL_INFO: IMSI is %.*s.\n",
                         U_CELL_INFO_IMSI_SIZE, pImsi);
            } else {
                uPortLog("U_CELL_INFO: unable to read IMSI.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the ICCID string of the SIM in the cellular module.
int32_t uCellInfoGetIccidStr(uDeviceHandle_t cellHandle,
                             char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CCID");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+CCID:");
            bytesRead = uAtClientReadString(atHandle, pStr, size, false);
            uAtClientResponseStop(atHandle);
            errorCodeOrSize = uAtClientUnlock(atHandle);
            if ((bytesRead >= 0) && (errorCodeOrSize == 0)) {
                errorCodeOrSize = bytesRead;
                uPortLog("U_CELL_INFO: ICCID is %s.\n", pStr);
            } else {
                errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
                uPortLog("U_CELL_INFO: unable to read ICCID.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the manufacturer ID string from the cellular module.
int32_t uCellInfoGetManufacturerStr(uDeviceHandle_t cellHandle,
                                    char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            errorCodeOrSize = getString(pInstance->atHandle, "AT+CGMI",
                                        pStr, size);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the model identification string from the cellular module.
int32_t uCellInfoGetModelStr(uDeviceHandle_t cellHandle,
                             char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            errorCodeOrSize = getString(pInstance->atHandle, "AT+CGMM",
                                        pStr, size);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the firmware version string from the cellular module.
int32_t uCellInfoGetFirmwareVersionStr(uDeviceHandle_t cellHandle,
                                       char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            // Use ATI9 instead of AT+CGMR as it contains more information
            errorCodeOrSize = getString(pInstance->atHandle, "ATI9",
                                        pStr, size);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the UTC time according to cellular.
int64_t uCellInfoGetTimeUtc(uDeviceHandle_t cellHandle)
{
    int64_t errorCodeOrValue = (int64_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int64_t timeUtc;
    char buffer[32];
    struct tm timeInfo;
    int32_t bytesRead;
    size_t offset = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int64_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CCLK?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+CCLK:");
            bytesRead = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
            uAtClientResponseStop(atHandle);
            errorCodeOrValue = uAtClientUnlock(atHandle);
            if ((bytesRead >= 17) && (errorCodeOrValue == 0)) {
                errorCodeOrValue = (int64_t) U_ERROR_COMMON_UNKNOWN;
                uPortLog("U_CELL_INFO: time is %s.\n", buffer);
                // The format of the returned string is
                // "yy/MM/dd,hh:mm:ss+TZ" but the +TZ may be omitted

                // Two-digit year converted to years since 1900
                offset = 0;
                buffer[offset + 2] = 0;
                timeInfo.tm_year = atoi(&(buffer[offset])) + 2000 - 1900;
                // Months converted to months since January
                offset = 3;
                buffer[offset + 2] = 0;
                timeInfo.tm_mon = atoi(&(buffer[offset])) - 1;
                // Day of month
                offset = 6;
                buffer[offset + 2] = 0;
                timeInfo.tm_mday = atoi(&(buffer[offset]));
                // Hours since midnight
                offset = 9;
                buffer[offset + 2] = 0;
                timeInfo.tm_hour = atoi(&(buffer[offset]));
                // Minutes after the hour
                offset = 12;
                buffer[offset + 2] = 0;
                timeInfo.tm_min = atoi(&(buffer[offset]));
                // Seconds after the hour
                offset = 15;
                buffer[offset + 2] = 0;
                timeInfo.tm_sec = atoi(&(buffer[offset]));
                // Get the time in seconds from this
                timeUtc = mktime64(&timeInfo);
                if ((timeUtc >= 0) && (bytesRead >= 20)) {
                    // There's a timezone, expressed in 15 minute intervals,
                    // subtract it to get UTC
                    offset = 17;
                    buffer[offset + 3] = 0;
                    timeUtc -= ((int64_t) atoi(&(buffer[offset]))) * 15 * 60;
                }

                if (timeUtc >= 0) {
                    errorCodeOrValue = timeUtc;
                    uPortLog("U_CELL_INFO: UTC time is %d.\n", (int32_t) errorCodeOrValue);
                } else {
                    uPortLog("U_CELL_INFO: unable to calculate UTC time.\n");
                }
            } else {
                errorCodeOrValue = (int64_t) U_CELL_ERROR_AT;
                uPortLog("U_CELL_INFO: unable to read time with AT+CCLK.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

/* Get the UTC time string according to cellular */
int32_t uCellInfoGetTimeUtcStr(uDeviceHandle_t cellHandle, char *pStr, size_t size)
{
    int32_t sizeOrErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;
    const size_t minBufferSize = 32;
    const int32_t timeStrMinLen = 17;

    if (pStr == NULL || size < minBufferSize) {
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (gUCellPrivateMutex != NULL && sizeOrErrorCode == 0) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        sizeOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CCLK?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+CCLK:");
            bytesRead = uAtClientReadString(atHandle, pStr,
                                            size, false);

            uAtClientResponseStop(atHandle);
            sizeOrErrorCode = uAtClientUnlock(atHandle);
            if ((bytesRead >= timeStrMinLen) && (sizeOrErrorCode == 0)) {
                sizeOrErrorCode = bytesRead;
                uPortLog("U_CELL_INFO: time is %s.\n", pStr);
            } else {
                sizeOrErrorCode = (int32_t) U_CELL_ERROR_AT;
                uPortLog("U_CELL_INFO: unable to read time with AT+CCLK.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool uCellInfoIsRtsFlowControlEnabled(uDeviceHandle_t cellHandle)
{
    bool isEnabled = false;
    uCellPrivateInstance_t *pInstance;
    int32_t atStreamHandle;
    uAtClientStream_t atStreamType;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atStreamHandle = uAtClientStreamGet(pInstance->atHandle, &atStreamType);
            if (atStreamType == U_AT_CLIENT_STREAM_TYPE_UART) {
                isEnabled = uPortUartIsRtsFlowControlEnabled(atStreamHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isEnabled;
}

// Determine if CTS flow control is enabled.
bool uCellInfoIsCtsFlowControlEnabled(uDeviceHandle_t cellHandle)
{
    bool isEnabled = false;
    uCellPrivateInstance_t *pInstance;
    int32_t atStreamHandle;
    uAtClientStream_t atStreamType;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atStreamHandle = uAtClientStreamGet(pInstance->atHandle, &atStreamType);
            if (atStreamType == U_AT_CLIENT_STREAM_TYPE_UART) {
                isEnabled = uPortUartIsCtsFlowControlEnabled(atStreamHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isEnabled;
}

// End of file
