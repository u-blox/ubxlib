/*
 * Copyright 2019-2024 u-blox
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
#include "ctype.h"     // isdigit()

#include "u_cfg_sw.h"
#include "u_compiler.h" // U_DEPRECATED

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" // strtok_r() and, in some cases, isblank()
#include "u_port_clib_mktime64.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_device_shared.h"

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

// Convert the UTRAN RSSI number in 3GPP TS 25.133 format to dBm.
// Returns 0x7FFFFFFF if the number is not known.
//
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

// Convert the UTRAN ecnoLev number to dB.
// 0:     less than -24 dB
// 1..48: -24 dB to 0 dB in 0.5 dB steps
// 49:    less than 0 dB
// Returns 0x7FFFFFFF if the number is not known.
static int32_t ecnoLevToDb(int32_t ecnoLev)
{
    int32_t ecnoDb = 0x7FFFFFFF;

    if ((ecnoLev >= 0) && (ecnoLev <= 49)) {
        ecnoDb = - (int32_t) (((uint32_t) (ecnoLev - 49)) >> 2);
    }

    return ecnoDb;
}

// Get SINR as an integer from a decimal (e.g -13.75) in a string,
// or 0x7FFFFFFF if not known
static int32_t getSinr(const char *pStr, int32_t divisor)
{
    int32_t sinrDb = 0x7FFFFFFF;
    int32_t x;
    char *pTmp;

    x = strtol(pStr, &pTmp, 10);
    // 255 means "not present/known"
    if (x != 255) {
        sinrDb = x;
        if (*pTmp == '.') {
            // Round based on mantissa
            pTmp++;
            if (isdigit((int32_t) *pTmp) && (*pTmp >= 0x35)) {
                if (x >= 0) {
                    sinrDb++;
                } else {
                    sinrDb--;
                }
            }
        }
        sinrDb += divisor / 2; // This to round to the nearest integer
        sinrDb /= divisor;
    }

    return sinrDb;
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
    char buffer[10]; // More than enough room for an SNIR reading, e.g. 13.75,
    // with a terminator, and enough for an 8-digit cell ID

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
    // Skip <Lband>, <ul_BW>, <dl_BW> and <tac>
    uAtClientSkipParameters(atHandle, 4);
    // Read <LcellId>
    if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
        pRadioParameters->cellIdLogical = strtol(buffer, NULL, 16);
    }
    // Read <PCID>
    pRadioParameters->cellIdPhysical = uAtClientReadInt(atHandle);
    // Skip <mTmsi>, <mmeGrId> and <mmeCode>
    uAtClientSkipParameters(atHandle, 3);
    // RSRP is element 11, coded as specified in TS 36.133
    pRadioParameters->rsrpDbm = uCellPrivateRsrpToDbm(uAtClientReadInt(atHandle));
    // RSRQ is element 12, coded as specified in TS 36.133.
    x = uAtClientReadInt(atHandle);
    if (uAtClientErrorGet(atHandle) == 0) {
        // Note that this can be a negative integer, hence
        // we check for errors here so as not to mix up
        // what might be a negative error code with a
        // negative return value.
        pRadioParameters->rsrqDb = uCellPrivateRsrqToDb(x);
    }
    // SINR is element 13, directly in dB, a decimal number
    // with a mantissa, 255 if unknown.
    x = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
    if (x > 0) {
        pRadioParameters->snrDb = getSinr(buffer, 1);
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
    char buffer[U_CELL_PRIVATE_CELL_ID_LOGICAL_SIZE + 1]; // +1 for terminator

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UCGED?");
    uAtClientCommandStop(atHandle);
    // The line with just "+UCGED: 2" on it
    uAtClientResponseStart(atHandle, "+UCGED:");
    uAtClientSkipParameters(atHandle, 1);

    // UCGED has two flavours for SARA-R422, one for GSM and the other for Cat-M1/NB1
    // Read the next line to get the RAT, which is always the first parameter
    uAtClientResponseStart(atHandle, NULL);
    x = uAtClientReadInt(atHandle);
    if (x == 2) {
        // GSM:
        // 2,<svc>,<MCC>,<MNC>
        // <ARFCN>,<band1900>,<GcellId>,<BSIC>,<Glac>,<Grac>,<rxlev>,<grr>,<t_adv>,<Gspeech_mode>
        // e.g.
        // 2,4,001,01
        // 810,1,0000,01,0000,80,63,255,255,255

        // Don't want anything from the rest of the first line
        uAtClientSkipParameters(atHandle, 3);
        // Now the line of interest
        uAtClientResponseStart(atHandle, NULL);
        // ARFCN is the first integer
        pRadioParameters->earfcn = uAtClientReadInt(atHandle);
        // Skip <band1900>
        uAtClientSkipParameters(atHandle, 1);
        // Read <GcellId>
        if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
            pRadioParameters->cellIdLogical = strtol(buffer, NULL, 16);
        }
        // RSSI is in the rxlev parameter element 7
        // If we don't already have it (from doing AT+CSQ),
        // get it from here
        if (pRadioParameters->rssiDbm == 0) {
            // Skip <BSIC>, <Glac>, <Grac>
            uAtClientSkipParameters(atHandle, 3);
            x = uAtClientReadInt(atHandle);
            if ((x >= 0) && (x <= 63)) {
                pRadioParameters->rssiDbm =  -(110 - x);
            }
            if (pRadioParameters->rssiDbm > -48) {
                pRadioParameters->rssiDbm = -48;
            }
        }
    } else {
        // Cat-M1/NB1:
        // <rat>,<MCC>,<MNC>
        // <EARFCN>,<Lband>,<ul_BW>,<dl_BW>,<TAC>,<P-CID>,<RSRP_value>,<RSRQ_value>,<NBMsinr>,<esm_cause>,<emm_state>,<tx_pwr>,<drx_cycle_len>,<tmsi>
        // e.g.
        // 6,310,410
        // 5110,12,10,10,830e,162,-86,-14,131,-1,3,255,128,"FB306E02"

        // Don't want anything from the rest of the first line
        uAtClientSkipParameters(atHandle, 2);
        // Now the line of interest
        uAtClientResponseStart(atHandle, NULL);
        // EARFCN is the first integer
        pRadioParameters->earfcn = uAtClientReadInt(atHandle);
        // Skip <Lband>, <ul_BW>, <dl_BW> and <TAC>
        uAtClientSkipParameters(atHandle, 4);
        // Read <P-CID>
        pRadioParameters->cellIdPhysical = uAtClientReadInt(atHandle);
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
        // SINR is element 9, encoded in 1/5ths of a dB where
        // 0 is -20 dB and the maximum is 250 (30 dB)
        x = uAtClientReadInt(atHandle);
        if (x >= 0) {
            pRadioParameters->snrDb = (x - (20 * 5)) / 5;
        }
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
    char buffer[10]; // More than enough room for an SNIR reading, e.g. 13.75,
    // with a terminator, or an 8-digit logical cell ID
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
    // <EARFCN>,<Lband>,<ul_BW>,<dl_BW>,<TAC>,<LcellId>,<P-CID>,<mTmsi>,<mmeGrId>,<mmeCode>,<RSRP>,<RSRQ>,<Lsinr>... etc.
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
            if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
                pRadioParameters->cellIdLogical = strtol(buffer, NULL, 16);
            }
            // Ignore the rest; rssiDbm will have come in via CSQ
            break;
        case 3:
            // UARFCN is the first integer
            pRadioParameters->earfcn = uAtClientReadInt(atHandle);
            // Skip <Wband>
            uAtClientSkipParameters(atHandle, 1);
            // Read <WcellId>
            if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
                pRadioParameters->cellIdLogical = strtol(buffer, NULL, 16);
            }
            // Skip <Wlac>, <Wrac>, <scrambling_code> and <Wrrc>
            uAtClientSkipParameters(atHandle, 4);
            // Read <rssi> and convert it to dBm
            pRadioParameters->rssiDbm = rssiUtranToDbm(uAtClientReadInt(atHandle));
            // Read <ecn0_lev> and convert it to dB
            pRadioParameters->snrDb = ecnoLevToDb(uAtClientReadInt(atHandle));
            // Ignore the rest
            break;
        case 4:
            // EARFCN is the first integer
            pRadioParameters->earfcn = uAtClientReadInt(atHandle);
            // Skip <Lband>, <ul_BW>, <dl_BW> and <TAC>
            uAtClientSkipParameters(atHandle, 4);
            // Read <LcellId>
            if (uAtClientReadString(atHandle, buffer, sizeof(buffer), false) > 0) {
                y = strtol(buffer, NULL, 16);
                // LARA-R6 has been seen to return a logical cell ID
                // of 0, even when obviously registered (because +CEREG
                // shows a proper hex value), therefore only update
                // the logical cell ID here if we have something real
                if (y > 0) {
                    pRadioParameters->cellIdLogical = y;
                }
            }
            // Read <PCID>
            pRadioParameters->cellIdPhysical = uAtClientReadInt(atHandle);
            // Skip <mTmsi>, <mmeGrId> and <mmeCode>
            uAtClientSkipParameters(atHandle, 3);
            // In the LARA-R6 00B FW RSRP (element 11) and RSRQ (element 12)
            // are plain-old dBm values, while in the LARA-R6 01B FW they are
            // both 3GPP coded values.  Since RSRP is negative in plain-old
            // form and positive in 3GPP form we can, thankfully, tell the
            // difference
            x = uAtClientReadInt(atHandle);
            if (x >= 0) {
                // RSRP is coded as specified in TS 36.133
                pRadioParameters->rsrpDbm = uCellPrivateRsrpToDbm(x);
                // RSRQ is coded as specified in TS 36.133.
                x = uAtClientReadInt(atHandle);
                if (uAtClientErrorGet(atHandle) == 0) {
                    // Note that this can be a negative integer, hence
                    // we check for errors here so as not to mix up
                    // what might be a negative error code with a
                    // negative return value.
                    pRadioParameters->rsrqDb = uCellPrivateRsrqToDb(x);
                }
            } else {
                // RSRP and RSRQ are plain-old dB values.
                y = uAtClientReadInt(atHandle);
                if (uAtClientErrorGet(atHandle) == 0) {
                    // Note that these last two are usually negative
                    // integers, hence we check for errors here so as
                    // not to mix up what might be a negative error
                    // code with a negative return value.
                    pRadioParameters->rsrpDbm = x;
                    pRadioParameters->rsrqDb = y;
                }
            }
            // SINR is element 13, directly in tenths of a dB, a
            // decimal number with a mantissa, 255 if unknown.
            x = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (x > 0) {
                pRadioParameters->snrDb = getSinr(buffer, 10);
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
    pRadioParameters->cellIdPhysical = uAtClientReadInt(atHandle);
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

// Get the time and time-zone offset.
static int64_t getTimeAndTimeZone(uAtClientHandle_t atHandle,
                                  int32_t *pTimeZoneSeconds)
{
    int64_t errorCodeOrValue;
    int64_t timeValue;
    int32_t timeZoneSeconds = INT_MIN;
    char timezoneSign = 0;
    char buffer[32];
    struct tm timeInfo;
    int32_t bytesRead;
    size_t offset = 0;

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
        // ...but, if there is timezone information,
        // save it before we obliterate the sign
        if (bytesRead >= 20) {
            timezoneSign = buffer[17];
        }
        offset = 15;
        buffer[offset + 2] = 0;
        timeInfo.tm_sec = atoi(&(buffer[offset]));
        // Get the time in seconds from this
        timeValue = mktime64(&timeInfo);
        offset = 17;
        if ((timeValue >= 0) && (bytesRead >= 20) &&
            ((timezoneSign == '+') || (timezoneSign == '-'))) {
            // There's a timezone, expressed in 15 minute intervals,
            // put the timezone sign back so that atoi() can handle it
            buffer[offset] = timezoneSign;
            buffer[offset + 3] = 0;
            timeZoneSeconds = atoi(&(buffer[offset])) * 15 * 60;
        }

        if (timeValue >= 0) {
            errorCodeOrValue = timeValue;
            uPortLog("U_CELL_INFO: local time is %d", (int32_t) errorCodeOrValue);
            if (timeZoneSeconds > INT_MIN) {
                uPortLog(", timezone offset %d seconds, hence UTC time is %d.\n", timeZoneSeconds,
                         (int32_t) (errorCodeOrValue - timeZoneSeconds));
                if (pTimeZoneSeconds != NULL) {
                    *pTimeZoneSeconds = timeZoneSeconds;
                }
            } else {
                uPortLog(".\n");
            }
        } else {
            uPortLog("U_CELL_INFO: unable to calculate time.\n");
        }
    } else {
        errorCodeOrValue = (int64_t) U_CELL_ERROR_AT;
        uPortLog("U_CELL_INFO: unable to read time with AT+CCLK.\n");
    }

    return errorCodeOrValue;
}

// Get the cell ID.
int32_t getCellId(uDeviceHandle_t cellHandle, bool logicalNotPhysical)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (logicalNotPhysical) {
                errorCodeOrValue = pInstance->radioParameters.cellIdLogical;
            } else {
                errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
                    errorCodeOrValue = pInstance->radioParameters.cellIdPhysical;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
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
            uCellPrivateClearRadioParameters(pRadioParameters, true);
            if (uCellPrivateIsRegistered(pInstance)) {
                // The mechanisms to get the radio information
                // are different between EUTRAN and GERAN but
                // AT+CSQ works in all cases though it sometimes
                // doesn't return a reading.  Collect what we can
                // with it
                errorCode = getRadioParamsCsq(atHandle, pRadioParameters);
                // Note that none of the mechanisms below are supported by
                // LENA-R8: if you can't get it with AT+CSQ then you can't get it
                if (U_CELL_PRIVATE_HAS(pInstance->pModule, U_CELL_PRIVATE_FEATURE_UCGED)) {
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
            }

            if (errorCode == 0) {
                uPortLog("U_CELL_INFO: radio parameters refreshed:\n");
                uPortLog("             RSSI:             %d dBm\n", pRadioParameters->rssiDbm);
                uPortLog("             RSRP:             %d dBm\n", pRadioParameters->rsrpDbm);
                uPortLog("             RSRQ:             %d dB\n", pRadioParameters->rsrqDb);
                uPortLog("             RxQual:           %d\n", pRadioParameters->rxQual);
                uPortLog("             logical cell ID:  0x%08x\n", pRadioParameters->cellIdLogical);
                uPortLog("             physical cell ID: %d\n", pRadioParameters->cellIdPhysical);
                uPortLog("             EARFCN:           %d\n", pRadioParameters->earfcn);
                if (pRadioParameters->snrDb != 0x7FFFFFFF) {
                    uPortLog("             SNR:              %d\n", pRadioParameters->snrDb);
                }
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
        if ((pInstance != NULL) &&
            (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8)) {
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
        if ((pInstance != NULL) &&
            (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8)) {
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
    uCellNetRat_t rat;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pSnrDb != NULL)) {
            pRadioParameters = &(pInstance->radioParameters);
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
                rat = uCellPrivateGetActiveRat(pInstance);
                if ((rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) ||
                    (rat == U_CELL_NET_RAT_EGPRS)) {
                    // Don't have SNR in 2G, just calculate it from RSSI and RSRP
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
                } else {
                    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                           U_CELL_PRIVATE_FEATURE_SNR_REPORTED)) {
                        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                        if (pRadioParameters->snrDb != 0x7FFFFFFF) {
                            // If we have a stored SNIR value that we've been
                            // able to read directly out of the module, then
                            // report that
                            *pSnrDb = pRadioParameters->snrDb;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the cell ID.
U_DEPRECATED int32_t uCellInfoGetCellId(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrValue = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if ((pInstance->radioParameters.cellIdPhysical >= 0) &&
                (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8)) {
                errorCodeOrValue = pInstance->radioParameters.cellIdPhysical;
            } else {
                errorCodeOrValue = pInstance->radioParameters.cellIdLogical;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrValue;
}

// Get the logical cell ID.
int32_t uCellInfoGetCellIdLogical(uDeviceHandle_t cellHandle)
{
    return getCellId(cellHandle, true);
}

// Get the physical cell ID.
int32_t uCellInfoGetCellIdPhysical(uDeviceHandle_t cellHandle)
{
    return getCellId(cellHandle, false);
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
            errorCodeOrValue = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LENA_R8) {
                errorCodeOrValue = pInstance->radioParameters.earfcn;
            }
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
            errorCodeOrSize = uCellPrivateGetIdStr(pInstance->atHandle, "AT+CGMI",
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
            errorCodeOrSize = uCellPrivateGetIdStr(pInstance->atHandle, "AT+CGMM",
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
            errorCodeOrSize = uCellPrivateGetIdStr(pInstance->atHandle, "ATI9",
                                                   pStr, size);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the UTC time according to cellular.
int64_t uCellInfoGetTimeUtc(uDeviceHandle_t cellHandle)
{
    int64_t errorCodeOrUtcTime = (int64_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t timeZoneSeconds = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrUtcTime = (int64_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrUtcTime = getTimeAndTimeZone(pInstance->atHandle,
                                                    &timeZoneSeconds);
            if (errorCodeOrUtcTime >= 0) {
                errorCodeOrUtcTime -= timeZoneSeconds;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrUtcTime;
}

// Get the UTC time string according to cellular.
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

// Get the local time according to cellular.
int64_t uCellInfoGetTime(uDeviceHandle_t cellHandle, int32_t *pTimeZoneSeconds)
{
    int64_t errorCodeOrTime = (int64_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    int32_t timeZoneSeconds = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrTime = (int64_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrTime = getTimeAndTimeZone(pInstance->atHandle,
                                                 &timeZoneSeconds);
            if ((errorCodeOrTime >= 0) && (pTimeZoneSeconds != NULL)) {
                *pTimeZoneSeconds = timeZoneSeconds;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrTime;
}

// Determine if RTS flow control is enabled.
bool uCellInfoIsRtsFlowControlEnabled(uDeviceHandle_t cellHandle)
{
    bool isEnabled = false;
    uCellPrivateInstance_t *pInstance;
    uAtClientStreamHandle_t stream = U_AT_CLIENT_STREAM_HANDLE_DEFAULTS;
    uDeviceSerial_t *pDeviceSerial;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            uAtClientStreamGetExt(pInstance->atHandle, &stream);
            switch (stream.type) {
                case U_AT_CLIENT_STREAM_TYPE_UART:
                    isEnabled = uPortUartIsRtsFlowControlEnabled(stream.handle.int32);
                    break;
                case U_AT_CLIENT_STREAM_TYPE_VIRTUAL_SERIAL:
                    pDeviceSerial = stream.handle.pDeviceSerial;
                    isEnabled = pDeviceSerial->isRtsFlowControlEnabled(pDeviceSerial);
                    break;
                default:
                    break;
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
    uAtClientStreamHandle_t stream = U_AT_CLIENT_STREAM_HANDLE_DEFAULTS;
    uDeviceSerial_t *pDeviceSerial;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            uAtClientStreamGetExt(pInstance->atHandle, &stream);
            switch (stream.type) {
                case U_AT_CLIENT_STREAM_TYPE_UART:
                    isEnabled = uPortUartIsCtsFlowControlEnabled(stream.handle.int32);
                    break;
                case U_AT_CLIENT_STREAM_TYPE_VIRTUAL_SERIAL:
                    pDeviceSerial = stream.handle.pDeviceSerial;
                    isEnabled = pDeviceSerial->isCtsFlowControlEnabled(pDeviceSerial);
                    break;
                default:
                    break;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isEnabled;
}

// End of file
