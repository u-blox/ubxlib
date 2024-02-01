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
 * @brief Implementation of the cfg API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdio.h"     // snprintf()
#include "stdbool.h"
#include "string.h"    // memset()
#include "time.h"      // time_t and struct tm

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* gmtime_r(), snprintf(), must be included
                                              before the other port files. */
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table to convert uCellNetRat_t to the value used in
 * CONFIGURING the module, SARA_U201 form
 */
static const int8_t gCellRatToModuleRatU201[] = {
    -1,  // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    0,   // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1,  // U_CELL_NET_RAT_GSM_COMPACT
    2,   // U_CELL_NET_RAT_UTRAN: 3G
    -1,  // U_CELL_NET_RAT_EGPRS
    -1,  // U_CELL_NET_RAT_HSDPA
    -1,  // U_CELL_NET_RAT_HSUPA
    -1,  // U_CELL_NET_RAT_HSDPA_HSUPA
    -1,  // U_CELL_NET_RAT_LTE
    -1,  // U_CELL_NET_RAT_EC_GSM
    -1,  // U_CELL_NET_RAT_CATM1
    -1,  // U_CELL_NET_RAT_NB1
    -1,  // U_CELL_NET_RAT_GSM_UMTS
    -1,  // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1,  // U_CELL_NET_RAT_GSM_LTE
    -1   // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * CONFIGURING the module, SARA-R4/R5 form.
 */
static const int8_t gCellRatToModuleRatR4R5[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    9,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    -1, // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    -1, // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    7,  // U_CELL_NET_RAT_CATM1
    8,  // U_CELL_NET_RAT_NB1
    -1, // U_CELL_NET_RAT_GSM_UMTS
    -1, // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1, // U_CELL_NET_RAT_GSM_LTE
    -1  // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * CONFIGURING the module, LARA-R6 form.
 */
static const int8_t gCellRatToModuleRatR6[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    0,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    2,  // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    3,  // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    -1, // U_CELL_NET_RAT_CATM1
    -1, // U_CELL_NET_RAT_NB1
    -1, // U_CELL_NET_RAT_GSM_UMTS
    -1, // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1, // U_CELL_NET_RAT_GSM_LTE
    -1  // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * CONFIGURING the module, LENA-R8 form.
 */
static const int8_t gCellRatToModuleRatR8[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    0,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    2,  // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    3,  // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    -1, // U_CELL_NET_RAT_CATM1
    -1, // U_CELL_NET_RAT_NB1
    1,  // U_CELL_NET_RAT_GSM_UMTS
    4,  // U_CELL_NET_RAT_GSM_UMTS_LTE
    5,  // U_CELL_NET_RAT_GSM_LTE
    6   // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * setting the bandmask, SARA-R4/R5 form.
 */
static const int8_t gCellRatToModuleRatBandMaskR4R5[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    -1,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    -1, // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    -1, // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    0,  // U_CELL_NET_RAT_CATM1
    1,  // U_CELL_NET_RAT_NB1
    -1, // U_CELL_NET_RAT_GSM_UMTS
    -1, // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1, // U_CELL_NET_RAT_GSM_LTE
    -1  // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * setting the bandmask, LARA-R6 form.
 */
static const int8_t gCellRatToModuleRatBandMaskR6[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    2,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    2,  // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    3,  // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    -1, // U_CELL_NET_RAT_CATM1
    -1, // U_CELL_NET_RAT_NB1
    -1, // U_CELL_NET_RAT_GSM_UMTS
    -1, // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1, // U_CELL_NET_RAT_GSM_LTE
    -1  // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert uCellNetRat_t to the value used in
 * setting the bandmask, LENA-R8 form.
 */
static const int8_t gCellRatToModuleRatBandMaskR8[] = {
    -1, // Dummy value for U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
    0,  // U_CELL_NET_RAT_GSM_GPRS_EGPRS: 2G
    -1, // U_CELL_NET_RAT_GSM_COMPACT
    2,  // U_CELL_NET_RAT_UTRAN: 3G
    -1, // U_CELL_NET_RAT_EGPRS
    -1, // U_CELL_NET_RAT_HSDPA
    -1, // U_CELL_NET_RAT_HSUPA
    -1, // U_CELL_NET_RAT_HSDPA_HSUPA
    3,  // U_CELL_NET_RAT_LTE
    -1, // U_CELL_NET_RAT_EC_GSM
    -1, // U_CELL_NET_RAT_CATM1
    -1, // U_CELL_NET_RAT_NB1
    -1, // U_CELL_NET_RAT_GSM_UMTS
    -1, // U_CELL_NET_RAT_GSM_UMTS_LTE
    -1, // U_CELL_NET_RAT_GSM_LTE
    -1  // U_CELL_NET_RAT_UMTS_LTE
};

/** Table to convert the RAT values used in the
 * module while reading the bandmask to uCellNetRat_t,
 * R4/R5 version.
 */
static const uCellNetRat_t gModuleRatBandMaskToCellRatR4R5[] = {
    U_CELL_NET_RAT_CATM1, // 0: Cat-M1
    U_CELL_NET_RAT_NB1    // 1: NB1
};

/** Table to convert the RAT values used in the
 * module while reading the bandmask to uCellNetRat_t,
 * R6 version.
 */
static const uCellNetRat_t gModuleRatBandMaskToCellRatR6[] = {
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 0: Cat-M1
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 1: NB1
    U_CELL_NET_RAT_GSM_GPRS_EGPRS,      // 2: 2G (also applied to UTRAN)
    U_CELL_NET_RAT_LTE                  // 3: LTE
};

/** All the parameters for the greeting callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t, void *);
    void *pCallbackParameter;
} uCellCfgGreeting_t;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: RAT CONVERSION
 * -------------------------------------------------------------- */

// Convert our RAT to module RAT, usual case.
int8_t cellRatToModuleRat(uCellModuleType_t moduleType, uCellNetRat_t rat)
{
    int8_t moduleRat = -1;

    switch (moduleType) {
        case U_CELL_MODULE_TYPE_SARA_U201:
            moduleRat = gCellRatToModuleRatU201[(int32_t) rat];
            break;
        case U_CELL_MODULE_TYPE_LARA_R6:
            moduleRat = gCellRatToModuleRatR6[(int32_t) rat];
            break;
        case U_CELL_MODULE_TYPE_LENA_R8:
            moduleRat = gCellRatToModuleRatR8[(int32_t) rat];
            break;
        default:
            moduleRat = gCellRatToModuleRatR4R5[(int32_t) rat];
            break;
    }

    return moduleRat;
}

// Convert our RAT to module RAT, bandmask case.
int8_t cellRatToModuleRatBandMask(uCellModuleType_t moduleType, uCellNetRat_t rat)
{
    int8_t moduleRat = -1;

    switch (moduleType) {
        case U_CELL_MODULE_TYPE_LARA_R6:
            moduleRat = gCellRatToModuleRatBandMaskR6[(int32_t) rat];
            break;
        case U_CELL_MODULE_TYPE_LENA_R8:
            moduleRat = gCellRatToModuleRatBandMaskR8[(int32_t) rat];
            break;
        default:
            moduleRat = gCellRatToModuleRatBandMaskR4R5[(int32_t) rat];
            break;
    }

    return moduleRat;
}

// Convert the module RAT for the bandmask case to our RAT.
uCellNetRat_t moduleRatBandMaskToCellRat(uCellModuleType_t moduleType, int32_t rat)
{
    uCellNetRat_t cellRat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    // Need to do boundary checking here as the array sizes are different
    if (rat >= 0) {
        switch (moduleType) {
            case U_CELL_MODULE_TYPE_LARA_R6:
                if (rat < sizeof(gModuleRatBandMaskToCellRatR6) / sizeof(gModuleRatBandMaskToCellRatR6[0])) {
                    cellRat = gModuleRatBandMaskToCellRatR6[rat];
                }
                break;
            // Don't need LENA-R8 as the form of the bandmask AT commands are different
            default:
                if (rat < sizeof(gModuleRatBandMaskToCellRatR4R5) / sizeof(gModuleRatBandMaskToCellRatR4R5[0])) {
                    cellRat = gModuleRatBandMaskToCellRatR4R5[rat];
                }
                break;
        }
    }

    return cellRat;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SARA-U2 RAT SETTING/GETTING BEHAVIOUR
 * -------------------------------------------------------------- */

// Set the given COPS if it's not already the given one, returning
// the one it was, if you see what I mean.
static int32_t setCops(uAtClientHandle_t atHandle, int32_t cops)
{
    int32_t x;
    int32_t errorCodeOrCops;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+COPS?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+COPS:");
    x = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    errorCodeOrCops = uAtClientUnlock(atHandle);
    if (errorCodeOrCops == 0) {
        errorCodeOrCops = x;
        if (errorCodeOrCops != cops) {
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+COPS=");
            uAtClientWriteInt(atHandle, cops);
            uAtClientCommandStopReadResponse(atHandle);
            x = uAtClientUnlock(atHandle);
            if (x < 0) {
                errorCodeOrCops = x;
            }
        }
    }

    return errorCodeOrCops;
}

// Get the radio access technology that is being used by
// the cellular module at the given rank, SARA-U2 style.
// Note: gUCellPrivateMutex should be locked before this is called.
static uCellNetRat_t getRatSaraU2(uCellPrivateInstance_t *pInstance,
                                  int32_t rank)
{
    int32_t errorOrRat = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t modes[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    int32_t cFunMode;

    // For SARA-U2, need to be in AT+CFUN=1 to get the RAT
    cFunMode = uCellPrivateCFunOne(pInstance);

    // In the SARA-U2 case the first "RAT" represents the operating
    // mode and the second the preferred RAT in that operating mode
    // if the first was dual mode, so here I call them "modes" rather
    // than RATs.

    // Assume there are no operating modes
    for (size_t x = 0; x < sizeof(modes) / sizeof(modes[0]); x++) {
        modes[x] = -1;
    }
    // Get the operating modes from the module
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+URAT:");
    // Read up to N integers representing the modes
    for (size_t x = 0; x < pInstance->pModule->maxNumSimultaneousRats; x++) {
        modes[x] = uAtClientReadInt(atHandle);
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    // Don't check error here as there may be fewer integers than we
    // tried to read
    if ((modes[0] == 0) || (modes[0] == 2)) {
        errorOrRat = (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
        // If the first mode is 0 (2G mode) or 2 (3G mode) then we are in
        // single mode operation and that's that.
        if (rank == 0) {
            // If we were being asked for the RAT at rank 0, this is it
            // as there is no other rank
            errorOrRat = (int32_t) uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[0]);
        }
        uPortLog("U_CELL_CFG: RAT is %d (in module terms %d).\n",
                 errorOrRat, modes[0]);
    } else if ((modes[0] == 1) && (modes[1] >= 0)) {
        errorOrRat = (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
        // If the first mode is 1, dual mode, then there MUST be a second
        // number and that indicates the preference
        if (rank == 0) {
            // If we were being asked for the RAT at rank 0, this is it
            errorOrRat = (int32_t) uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[1]);
        } else if (rank == 1) {
            // If we were being asked for the RAT at rank 1, it is
            // the OTHER one, the non-preferred RAT, that we must report
            if (modes[1] ==
                cellRatToModuleRat(pInstance->pModule->moduleType, U_CELL_NET_RAT_GSM_GPRS_EGPRS)) {
                errorOrRat = (int32_t)U_CELL_NET_RAT_UTRAN;
            } else if (modes[1] == cellRatToModuleRat(pInstance->pModule->moduleType, U_CELL_NET_RAT_UTRAN)) {
                errorOrRat = (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS;
            }
        }
        uPortLog("U_CELL_CFG: RAT is %d (in module terms %d).\n",
                 errorOrRat, modes[1]);
    }

    // Put the AT+CFUN back if it was not already 1
    if ((cFunMode >= 0) && (cFunMode != 1)) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return (uCellNetRat_t) errorOrRat;
}

// Get the rank at which the given RAT is being used, SARA-U2 style.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t getRatRankSaraU2(uCellPrivateInstance_t *pInstance,
                                uCellNetRat_t rat)
{
    int32_t errorCodeOrRank = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t modes[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    int32_t cFunMode;

    // For SARA-U2, need to be in AT+CFUN=1 to get the RAT
    cFunMode = uCellPrivateCFunOne(pInstance);
    // Not checking error here, what follows will fail if this
    // fails anyway

    // In the SARA-U2 case the first "RAT" represents the operating
    // mode and the second the preferred RAT in that operating mode
    // if the first was dual mode, so here I call them "modes" rather
    // than RATs.

    // Assume there are no operating modes
    for (size_t x = 0; x < sizeof(modes) / sizeof(modes[0]); x++) {
        modes[x] = -1;
    }
    // Get the operating modes from the module
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+URAT:");
    // Read up to N integers representing the modes
    for (size_t x = 0; x < pInstance->pModule->maxNumSimultaneousRats; x++) {
        modes[x] = uAtClientReadInt(atHandle);
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    // Don't check error here as there may be fewer integers than we
    // tried to read
    if ((modes[0] == 0) || (modes[0] == 2)) {
        errorCodeOrRank = (int32_t) U_CELL_ERROR_NOT_FOUND;
        // If the first mode is 0 (2G mode) or 2 (3G mode) then we are in
        // single mode operation and so can check for the indicated
        // RAT here
        if (rat == uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[0])) {
            errorCodeOrRank = 0;
        }
    } else if ((modes[0] == 1) && (modes[1] >= 0)) {
        errorCodeOrRank = (int32_t) U_CELL_ERROR_NOT_FOUND;
        // If the first mode is 1, dual mode, then there MUST be a second
        // number which indicates the preference
        // If the RAT being asked for is 2G or 3G then if it in this
        // second number it is at rank 0, else it must by implication
        // be at rank 1
        if ((rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) || (rat == U_CELL_NET_RAT_UTRAN)) {
            errorCodeOrRank = 1;
            if (rat == uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[1])) {
                errorCodeOrRank = 0;
            }
        }
    }

    // Put the AT+CFUN mode back if it was not already 1
    if ((cFunMode >= 0) && (cFunMode != 1)) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return errorCodeOrRank;
}

// Set RAT SARA-U2 stylee.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t setRatSaraU2(uCellPrivateInstance_t *pInstance,
                            uCellNetRat_t rat)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t cFunMode;
    int32_t cops;

    // For SARA-U2, need to be in AT+CFUN=1 and AT+COPS=2 to set the RAT
    cFunMode = uCellPrivateCFunOne(pInstance);
    cops = setCops(atHandle, 2);

    uPortLog("U_CELL_CFG: setting sole RAT to %d (in module terms %d).\n",
             rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat));
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT=");
    uAtClientWriteInt(atHandle,
                      cellRatToModuleRat(pInstance->pModule->moduleType, rat));
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);

    // Put AT+COPS back
    if (cops >= 0) {
        // Put COPS back
        setCops(atHandle, cops);
    }
    // Put the AT+CFUN mode back if it was not already 1
    if ((cFunMode >= 0) && (cFunMode != 1)) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return errorCode;
}

// Set RAT rank SARA-U2 stylee.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t setRatRankSaraU2(uCellPrivateInstance_t *pInstance,
                                uCellNetRat_t rat, int32_t rank)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    bool validOperation = false;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t modes[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    int32_t cFunMode;
    int32_t cops;

    // In the SARA-U2 case the first "RAT" represents the operating
    // mode and the second the preferred RAT in that operating mode
    // if the first was dual mode, so here I call them "modes" rather
    // than RATs.

    // Assume there are no operating modes
    for (size_t x = 0; x < sizeof(modes) / sizeof(modes[0]); x++) {
        modes[x] = -1;
    }
    // For SARA-U2, need to be in AT+CFUN=1 to get the RAT
    cFunMode = uCellPrivateCFunOne(pInstance);
    cops = setCops(atHandle, 2);

    // Get the existing operating modes
    // Not checking error here, what follows will fail if this
    // fails anyway
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+URAT:");
    // Read up to N integers representing the modes
    for (size_t x = 0; x < pInstance->pModule->maxNumSimultaneousRats; x++) {
        modes[x] = uAtClientReadInt(atHandle);
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    // Don't check error here as there may be fewer integers than we
    // tried to read

    if (rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
        // If we are setting rather than removing the
        // RAT at a given rank...
        if ((modes[0] >= 0) && (modes[1] >= 0)) {
            // ...and we already have dual mode...
            if (rank == 0) {
                // ...and we are setting the first rank,
                // then set the preference in the second number
                modes[1] = cellRatToModuleRat(pInstance->pModule->moduleType, rat);
                validOperation = true;
            } else if (rank == 1) {
                // ...otherwise if we are setting the second
                // rank then we want to set the OPPOSITE of
                // the desired RAT in the second number.
                // In other words, to put 2G at rank 1, we
                // need to set 3G as our preferred RAT.
                if (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) {
                    modes[1] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                  U_CELL_NET_RAT_UTRAN);
                    validOperation = true;
                } else if (rat == U_CELL_NET_RAT_UTRAN) {
                    modes[1] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                  U_CELL_NET_RAT_GSM_GPRS_EGPRS);
                    validOperation = true;
                }
            }
        } else if ((modes[0] >= 0) && (modes[1] < 0)) {
            // ...and we are in single mode...
            if (rank == 0) {
                // ...then if we are setting rank 0 just set it
                modes[0] = cellRatToModuleRat(pInstance->pModule->moduleType, rat);
                validOperation = true;
            } else if (rank == 1) {
                // ...or if we're setting rank 1, then if it
                // is different from the existing RAT...
                if (rat != uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[0])) {
                    // ...then switch to dual mode and, as above, set
                    // the opposite of the desired RAT in the second
                    // number.
                    if (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) {
                        modes[0] = 1;
                        modes[1] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                      U_CELL_NET_RAT_UTRAN);
                        validOperation = true;
                    } else if (rat == U_CELL_NET_RAT_UTRAN) {
                        modes[0] = 1;
                        modes[1] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                      U_CELL_NET_RAT_GSM_GPRS_EGPRS);
                        validOperation = true;
                    }
                } else {
                    // ...else leave things as they are
                    validOperation = true;
                }
            }
        }
    } else {
        // If we are removing the RAT at a given rank...
        if ((modes[0] >= 0) && (modes[1] >= 0)) {
            // ...then we must be in dual mode
            // (anything else is invalid or pointless)...
            if (rank == 0) {
                // If are removing the top-most rank
                // then we set the single mode to be
                // the opposite of the currently
                // preferred RAT
                if (uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[1]) ==
                    U_CELL_NET_RAT_GSM_GPRS_EGPRS) {
                    modes[0] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                  U_CELL_NET_RAT_UTRAN);
                    modes[1] = -1;
                    validOperation = true;
                } else if (uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType,
                                                          modes[1]) == U_CELL_NET_RAT_UTRAN) {
                    modes[0] = cellRatToModuleRat(pInstance->pModule->moduleType,
                                                  U_CELL_NET_RAT_GSM_GPRS_EGPRS);
                    modes[1] = -1;
                    validOperation = true;
                }
            } else if (rank == 1) {
                // If are removing the second rank
                // then we set the single mode to be
                // the currently preferred RAT
                modes[0] = modes[1];
                modes[1] = -1;
                validOperation = true;
            }
        }
    }

    if (validOperation) {
        // Send the AT command
        uPortLog("U_CELL_CFG: setting RATs:\n");
        for (size_t x = 0; (x < sizeof(modes) / sizeof(modes[0])); x++) {
            if (modes[x] >= 0) {
                uPortLog("  rank[%d]: %d (in module terms %d).\n", x,
                         uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, modes[x]),
                         modes[x]);
            } else {
                uPortLog("  rank[%d]: %d (in module terms %d).\n", x,
                         U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, -1);
            }
        }
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+URAT=");
        for (size_t x = 0; (x < sizeof(modes) / sizeof(modes[0])); x++) {
            if (modes[x] >= 0) {
                uAtClientWriteInt(atHandle, modes[x]);
            }
        }
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    } else {
        uPortLog("U_CELL_CFG: setting RAT %d (in module terms %d) at rank %d"
                 " is not a valid thing to do.\n", rat,
                 cellRatToModuleRat(pInstance->pModule->moduleType, rat), rank);
    }

    // Put AT+COPS back
    if (cops >= 0) {
        // Put COPS back
        setCops(atHandle, cops);
    }
    // Put the AT+CFUN mode back if it was not already 1
    if ((cFunMode >= 0) && (cFunMode != 1)) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: SARA-R4/R5/R6 RAT SETTING/GETTING BEHAVIOUR
 * -------------------------------------------------------------- */

// Get the radio access technology that is being used by
// the cellular module at the given rank, SARA-R4/R5/R6 style.
// Note: gUCellPrivateMutex should be locked before this is called.
static uCellNetRat_t getRatSaraRx(const uCellPrivateInstance_t *pInstance,
                                  int32_t rank)
{
    int32_t errorOrRat = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t rat;
    uCellNetRat_t rats[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];

    // Assume there are no RATs
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        rats[x] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }
    // Get the RAT from the module
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+URAT:");
    // Read up to N integers representing the RATs
    for (size_t x = 0; x < pInstance->pModule->maxNumSimultaneousRats; x++) {
        rat = uAtClientReadInt(atHandle);
        rats[x] = uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, rat);
    }
    uAtClientResponseStop(atHandle);
    if (uAtClientUnlock(atHandle) == 0) {
        errorOrRat = (int32_t) rats[rank];
    }
    uPortLog("U_CELL_CFG: RATs are:\n");
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        uPortLog("  rank[%d]: %d (in module terms %d).\n",
                 x, rats[x], cellRatToModuleRat(pInstance->pModule->moduleType, rats[x]));
    }

    return (uCellNetRat_t) errorOrRat;
}

// Get the rank at which the given RAT is being used, SARA-R4/R5/R6 style.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t getRatRankSaraRx(const uCellPrivateInstance_t *pInstance,
                                uCellNetRat_t rat)
{
    int32_t errorCodeOrRank = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t y;

    // Get the RATs from the module
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+URAT:");
    // Read up to N integers representing the RATs
    for (size_t x = 0; (errorCodeOrRank < 0) &&
         (x < pInstance->pModule->maxNumSimultaneousRats); x++) {
        y = uAtClientReadInt(atHandle);
        if (rat == uCellPrivateModuleRatToCellRat(pInstance->pModule->moduleType, y)) {
            errorCodeOrRank = (int32_t) x;
        }
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    return errorCodeOrRank;
}

// Set RAT SARA-R4/R5/R6 stylee.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t setRatSaraRx(uCellPrivateInstance_t *pInstance,
                            uCellNetRat_t rat)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t cFunMode = -1;

    if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        // For SARA-R5 the module has to be in state AT+CFUN=0
        cFunMode = uCellPrivateCFunGet(pInstance);
        if (cFunMode != 0) {
            uCellPrivateCFunMode(pInstance, 0);
        }
    }

    // Do the mode change
    uPortLog("U_CELL_CFG: setting sole RAT to %d (in module terms %d).\n",
             rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat));
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT=");
    uAtClientWriteInt(atHandle, cellRatToModuleRat(pInstance->pModule->moduleType, rat));
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);

    // Put the AT+CFUN mode back to what it was if necessary
    if (cFunMode > 0) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return errorCode;
}

// Set RAT rank SARA-R4/R5/R6 stylee.
// Note: gUCellPrivateMutex should be locked before this is called.
static int32_t setRatRankSaraRx(uCellPrivateInstance_t *pInstance,
                                uCellNetRat_t rat, int32_t rank)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t rats[U_CELL_PRIVATE_MAX_NUM_SIMULTANEOUS_RATS];
    int32_t cFunMode = -1;

    // Assume there are no RATs
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        rats[x] = (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }

    // Get the existing RATs
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        rats[x] = (int32_t) getRatSaraRx(pInstance, (int32_t) x);
        if (rats[x] == (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
            break;
        }
    }
    // Overwrite the one we want to set
    rats[rank] = (int32_t) rat;

    uPortLog("U_CELL_CFG: setting the RAT at rank %d to"
             " %d (in module terms %d).\n",
             rank, rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat));
    // Remove duplicates
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        for (size_t y = x + 1; y < sizeof(rats) / sizeof(rats[0]); y++) {
            if ((rats[x] > (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
                (rats[x] == rats[y])) {
                rats[y] = (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
            }
        }
    }

    if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
        // For SARA-R5 the module has to be in state AT+CFUN=0
        cFunMode = uCellPrivateCFunGet(pInstance);
        if (cFunMode != 0) {
            uCellPrivateCFunMode(pInstance, 0);
        }
    }

    // Send the AT command
    uPortLog("U_CELL_CFG: RATs (removing duplicates) become:\n");
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        uPortLog("  rank[%d]: %d (in module terms %d).\n",
                 x, rats[x], cellRatToModuleRat(pInstance->pModule->moduleType,
                                                (uCellNetRat_t) rats[x]));
    }
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+URAT=");
    for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
        if (rats[x] != (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
            uAtClientWriteInt(atHandle,
                              cellRatToModuleRat(pInstance->pModule->moduleType,
                                                 (uCellNetRat_t) rats[x]));
        }
    }
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);

    // Put the AT+CFUN mode back to what it was if necessary
    if (cFunMode > 0) {
        uCellPrivateCFunMode(pInstance, cFunMode);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: GREETING MESSAGE RELATED
 * -------------------------------------------------------------- */

// Set, or unset, a greeting message.
static int32_t setGreeting(uAtClientHandle_t atHandle, const char *pStr)
{
    int32_t mode = 0;

    if (pStr != NULL) {
        mode = 1;
    }

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CSGT=");
    uAtClientWriteInt(atHandle, mode);
    if (pStr != NULL) {
        uAtClientWriteString(atHandle, pStr, true);
    }
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Get the current greeting message; a null
// terminator will be included.
static int32_t getGreeting(uAtClientHandle_t atHandle, char *pStr, size_t size)
{
    int32_t errorCodeOrSize;
    int32_t mode;
    int32_t bytesRead;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CSGT?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CSGT:");
    bytesRead = uAtClientReadString(atHandle, pStr, size, false);
    mode = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if (errorCodeOrSize == 0) {
        if (mode == 0) {
            bytesRead = 0;
            *pStr = 0;
        }
        errorCodeOrSize = bytesRead;
    }

    return errorCodeOrSize;
}

// Callback via which the user's greeting callback is called.
// This must be called through the uAtClientCallback() mechanism
// in order to prevent customer code blocking the AT client.
static void greetingCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellCfgGreeting_t *pGreeting = (uCellCfgGreeting_t *) pParameter;

    (void) atHandle;

    if (pGreeting != NULL) {
        if (pGreeting->pCallback != NULL) {
            pGreeting->pCallback(pGreeting->cellHandle,
                                 pGreeting->pCallbackParameter);
        }
        uPortFree(pGreeting);
    }
}

// URC handler for when the greeting message has been detected.
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void GREETING_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    const uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    uCellCfgGreeting_t *pGreeting;

    (void) atHandle;

    if (pInstance->pGreetingCallback != NULL) {
        // Put the data for the callback into a struct to our
        // local callback via the AT client's callback mechanism
        // to decouple it from any URC handler.
        // Note: it is up to greetingCallback() to free the
        // allocated memory.
        pGreeting = (uCellCfgGreeting_t *) pUPortMalloc(sizeof(*pGreeting));
        if (pGreeting != NULL) {
            pGreeting->cellHandle = pInstance->cellHandle;
            pGreeting->pCallback = pInstance->pGreetingCallback;
            pGreeting->pCallbackParameter = pInstance->pGreetingCallbackParameter;
            if (uAtClientCallback(atHandle, greetingCallback, pGreeting) != 0) {
                // Clean up on error
                uPortFree(pGreeting);
            }
        }
    }
}

// Add a greeting URC, handling SARA-R41x oddness if necessary.
static int32_t addGreetingUrc(uCellPrivateInstance_t *pInstance,
                              const char *pStr)
{
    int32_t errorCode;
    // +1 for terminator, +2 for the SARA-R41X workaround
    char buffer[U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES + 1 + 2];

    if (U_CELL_PRIVATE_MODULE_IS_SARA_R41X(pInstance->pModule->moduleType)) {
        // This is necessary since SARA-R41X modules add an odd set of
        // control characters before the greeting string: usually this is
        // a null and then 0x0a (LF) 0x0d (CR), rather than the usual CR/LF.
        // The null is obliterating a CR, which can sometimes appears, so
        // the greeting string can be prefixed with 00 0a 0d or 0d 0a 0d;
        // the AT client will remove the null itself, and will also strip
        // any CR/LF (0d 0a), so we need URC handlers for 0a 0d <URC> and 0d <URC>.
        // Shuffle everything in the buffer up by two
        strncpy(buffer + 2, pStr, sizeof(buffer) - 2);
        // Add LF/CR at the start
        buffer[0] = 0x0a;
        buffer[1] = 0x0d;
        errorCode = uAtClientSetUrcHandler(pInstance->atHandle, buffer,
                                           GREETING_urc, pInstance);
        if (errorCode == 0) {
            // And the same for just CR
            errorCode = uAtClientSetUrcHandler(pInstance->atHandle, buffer + 1,
                                               GREETING_urc, pInstance);
        }
    } else {
        errorCode = uAtClientSetUrcHandler(pInstance->atHandle, pStr,
                                           GREETING_urc, pInstance);
    }

    return errorCode;
}

// Remove a greeting URC, handling SARA-R41x oddness if necessary.
static void removeGreetingUrc(uCellPrivateInstance_t *pInstance,
                              const char *pStr)
{
    // +1 for terminator, +2 for the SARA-R41X workaround
    char buffer[U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES + 1 + 2];

    if (U_CELL_PRIVATE_MODULE_IS_SARA_R41X(pInstance->pModule->moduleType)) {
        // Same reasoning as for addGreetingUrc()
        strncpy(buffer + 2, pStr, sizeof(buffer) - 2);
        // Add LF/CR at the start
        buffer[0] = 0x0a;
        buffer[1] = 0x0d;
        uAtClientRemoveUrcHandler(pInstance->atHandle, buffer);
        // And the same for just CR
        uAtClientRemoveUrcHandler(pInstance->atHandle, buffer + 1);
    } else {
        uAtClientRemoveUrcHandler(pInstance->atHandle, pStr);
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: GENERAL
 * -------------------------------------------------------------- */

// Set the baud rate in the cellular module and store it in NVM.
static int32_t setAndStoreBaudRate(const uCellPrivateInstance_t *pInstance,
                                   int32_t baudRate)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Set the baud rate
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+IPR=");
    uAtClientWriteInt(atHandle, baudRate);
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    if ((errorCode == 0) &&
        U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_AT_PROFILES)) {
        // Make sure it is stored in an NVM profile,
        // where supported
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT&W");
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the bands to be used by the cellular module: building the bandmask itself.
int32_t uCellCfgSetBands(uDeviceHandle_t cellHandle,
                         uCellNetRat_t rat,
                         size_t numBands,
                         uint8_t *pBands)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uint64_t bandMask1 = 0;
    uint64_t bandMask2 = 0;
    bool isValid = true;

    if (pBands != NULL) {
        for (size_t i = 0; i < numBands; i++) {
            if (pBands[i] > 0) { //Valid band
                if (pBands[i] <= 64) { //Populate Bandmask 1
                    //subtracting 1 is due to the fact that band 1 maps on bit 0 and band 64 to bit 63
                    bandMask1 |= 1ULL << ((pBands[i]) - 1);

                } else if ((pBands[i] > 64) && (pBands[i] <= 128)) { //Populate bandmask 2
                    //subtracting 1 is due to the fact that band 1 maps on bit 0
                    bandMask2 |= 1ULL << ((pBands[i] - (sizeof(uint64_t) * 8)) - 1);
                } else {
                    uPortLog("U_CELL_CFG: invalid band: %d at location %d in the array.\n", pBands[i], i);
                    isValid = false;
                }
            }
        }
    } else {
        isValid = false;
    }
    if (isValid) {
        errorCode = uCellCfgSetBandMask(cellHandle, rat, bandMask1, bandMask2);
    }

    return errorCode;
}

// Set the bands to be used by the cellular module.
int32_t uCellCfgSetBandMask(uDeviceHandle_t cellHandle,
                            uCellNetRat_t rat,
                            uint64_t bandMask1,
                            uint64_t bandMask2)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t bandNumber;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            ((rat == U_CELL_NET_RAT_CATM1) || (rat == U_CELL_NET_RAT_NB1) ||
             (rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) ||
             (rat == U_CELL_NET_RAT_UTRAN)) &&
            (pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) rat))) {
            errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
            if (!uCellPrivateIsRegistered(pInstance)) {
                atHandle = pInstance->atHandle;
                uPortLog("U_CELL_CFG: setting band mask for RAT %d (in module"
                         " terms %d) to 0x%08x%08x %08x%08x.\n",
                         rat, cellRatToModuleRatBandMask(pInstance->pModule->moduleType, rat),
                         (uint32_t) (bandMask2 >> 32), (uint32_t) bandMask2,
                         (uint32_t) (bandMask1 >> 32), (uint32_t) bandMask1);
                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LENA_R8) {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                    if (rat == U_CELL_NET_RAT_LTE) {
                        // For LTE, LENA-R8 uses the AT+SETLOCK command
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+SETLOCK=");
                        if ((bandMask1 == 0) && (bandMask2 == 0)) {
                            // For LENA-R8 an empty band mask means set all of them
                            uAtClientWriteInt(atHandle, 0);
                            uAtClientWriteInt(atHandle, 0); // Has to be present
                        } else {
                            // Configuring a selection of bands that is not all
                            uAtClientWriteInt(atHandle, 1);
                            uAtClientWriteInt(atHandle, 0); // Has to be present
                            for (size_t x = 0; x < (sizeof(uint64_t) * 2 * 8); x++) {
                                bandNumber = -1;
                                if (x < (sizeof(uint64_t) * 8)) {
                                    if ((1ULL << x) & bandMask1) {
                                        bandNumber = x + 1; // +1 because bit position 0 is band 1
                                    }
                                } else {
                                    if ((1ULL << (x - (sizeof(uint64_t) * 8))) & bandMask2) {
                                        bandNumber = x + 1;
                                    }
                                }
                                if (bandNumber >= 0) {
                                    uAtClientWriteInt(atHandle, bandNumber);
                                }
                            }
                        }
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                    } else if (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) {
                        // For GSM, LENA-R8 uses the AT+SETBAND command
                        bandNumber = -1;
                        // Lots of -1's below because bit position 0 is band 1
                        if ((bandMask1 & (1ULL << (3 - 1)) /* DCS1800 */) && (bandMask1 & (1ULL << (8 - 1)) /* 900 */)) {
                            // Don't care about PCS1900 or 850: you get them for free if
                            // you ask for both 900 and DCS1800
                            bandNumber = 0;
                        } else if (bandMask1 & (1ULL << (8 - 1)) /* 900 */) {
                            bandNumber = 1;
                        } else if (bandMask1 & (1ULL << (3 - 1)) /* DCS1800 */) {
                            bandNumber = 2;
                        }
                        if (bandNumber >= 0) {
                            uAtClientLock(atHandle);
                            uAtClientCommandStart(atHandle, "AT+SETBAND=");
                            uAtClientWriteInt(atHandle, bandNumber);
                            uAtClientCommandStopReadResponse(atHandle);
                            errorCode = uAtClientUnlock(atHandle);
                        }
                    }
                } else {
                    // Everything else uses the AT+UBANDMASK command
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UBANDMASK=");
                    uAtClientWriteInt(atHandle, cellRatToModuleRatBandMask(pInstance->pModule->moduleType, rat));
                    uAtClientWriteUint64(atHandle, bandMask1);
                    uAtClientWriteUint64(atHandle, bandMask2);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                }
                if (errorCode == 0) {
                    pInstance->rebootIsRequired = true;
                }
            } else {
                uPortLog("U_CELL_CFG: unable to set band mask as we are"
                         " connected to the network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the bands being used by the cellular module.
int32_t uCellCfgGetBandMask(uDeviceHandle_t cellHandle,
                            uCellNetRat_t rat,
                            uint64_t *pBandMask1,
                            uint64_t *pBandMask2)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uint64_t i[6];
    uint64_t masks[2][2];
    int32_t rats[2];
    bool success = true;
    int32_t count = 0;
    int32_t bandNumber;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            ((rat == U_CELL_NET_RAT_CATM1) || (rat == U_CELL_NET_RAT_NB1) ||
             (rat == U_CELL_NET_RAT_LTE) || (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) ||
             (rat == U_CELL_NET_RAT_UTRAN)) &&
            (pBandMask1 != NULL) && (pBandMask2 != NULL) &&
            (pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) rat))) {
            errorCode = (int32_t) U_CELL_ERROR_AT;
            // Initialise locals
            for (size_t x = 0; x < sizeof(i) / sizeof(i[0]); x++) {
                i[x] = (uint64_t) -1;
            }
            memset(masks, 0, sizeof(masks));
            for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
                rats[x] = -1;
            }

            atHandle = pInstance->atHandle;
            uPortLog("U_CELL_CFG: getting band mask for RAT %d (in module terms %d).\n",
                     rat, cellRatToModuleRatBandMask(pInstance->pModule->moduleType, rat));
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LENA_R8) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (rat == U_CELL_NET_RAT_LTE) {
                    // For LTE, LENA-R8 uses the AT+SETLOCK command
                    rats[0] = rat;
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+SETLOCK=");
                    uAtClientWriteInt(atHandle, 2); // Read
                    uAtClientWriteInt(atHandle, 0); // Has to be present
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+SETLOCK:");
                    // The first parameter is the number of parameters to follow,
                    // where 0 means "all of the bands are enabled"
                    count = uAtClientReadInt(atHandle);
                    if (count > sizeof(i) / sizeof(i[0])) {
                        count = sizeof(i) / sizeof(i[0]);
                    }
                    for (int32_t x = 0; x < count; x++) {
                        bandNumber = uAtClientReadInt(atHandle);
                        i[x] = bandNumber;
                    }
                    uAtClientResponseStop(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if ((errorCode == 0) && (count >= 0)) {
                        masks[0][0] = 0;
                        masks[0][1] = 0;
                        if (count > 0) {
                            for (int32_t x = 0; x < count; x++) {
                                if (i[x] <= sizeof(uint64_t) * 8) {
                                    masks[0][0] |= 1ULL << (i[x] - 1); // -1 'cos bit position 0 is band 1
                                } else {
                                    masks[0][1] |= 1ULL << (i[x] - 1 - ( sizeof(uint64_t) * 8));
                                }
                            }
                        }
                    }
                } else if (rat == U_CELL_NET_RAT_GSM_GPRS_EGPRS) {
                    // For GSM, LENA-R8 uses the AT+SETBAND command
                    rats[0] = rat;
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+SETBAND?");
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+SETBAND:");
                    bandNumber = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if ((errorCode == 0) && (bandNumber >= 0)) {
                        masks[0][0] = 0;
                        masks[0][1] = 0;
                        switch (bandNumber) {
                            case 0:
                                // PCS1900, DCS1800, 850 and 900,
                                // -1's because bit position 0 is band 1
                                masks[0][0] = (1ULL << (2 - 1)) | (1ULL << (3 - 1)) | (1ULL << (5 - 1)) | (1ULL << (8 - 1));
                                break;
                            case 1:
                                // 900
                                masks[0][0] = 1ULL << (8 - 1);
                                break;
                            case 2:
                                // DCS1800
                                masks[0][0] = 1ULL << (3 - 1);
                                break;
                            default:
                                break;
                        }
                    }
                }
            } else {
                // Everything else uses the AT+UBANDMASK command
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBANDMASK?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UBANDMASK:");
                // The AT response here can be any one of the following:
                //    0        1             2             3           4                 5
                // <rat_a>,<bandmask_a0>
                // <rat_a>,<bandmask_a0>,<bandmask_a1>
                // <rat_a>,<bandmask_a0>,<rat_b>,      <bandmask_b0>
                // <rat_a>,<bandmask_a0>,<bandmask_a1>,<rat_b>,      <bandmask_b0>
                // <rat_a>,<bandmask_a0>,<rat_b>,      <bandmask_b0>,<bandmask_b1>                  <-- ASSUMED THIS CANNOT HAPPEN!!!
                // <rat_a>,<bandmask_a0>,<bandmask_a1>,<rat_b>,      <bandmask_b0>,  <bandmask_b1>
                //
                // Since each entry is just a decimal number, how to tell which format
                // is being used?
                //
                // Here's my algorithm:
                // i.   Read i0 and i1, <rat_a> and <bandmask_a0>.
                // ii.  Attempt to read i2: if is present it could be
                //      <bandmask_a1> or <rat_b>, if not FINISH.
                // iii. Attempt to read i3: if it is present then it is
                //      either <bandmask_b0> or <rat_b>, if it
                //      is not present then the i2 was <bandmask_a1> FINISH.
                // iv.  Attempt to read i4 : if it is present then i2
                //      was <bandmask_a1>, i3 was <rat_b> and i4 is
                //      <bandmask_b0>, if it is not present then i2 was
                //      <rat_b> and i3 was <bandmask_b0> FINISH.
                // v.   Attempt to read i5: if it is present then it is
                //      <bandmask_b1>.

                // Read all the numbers in
                for (size_t x = 0; (x < sizeof(i) / sizeof(i[0])) && success; x++) {
                    success = (uAtClientReadUint64(atHandle, &(i[x])) == 0);
                    if (success) {
                        count++;
                    }
                }
                uAtClientResponseStop(atHandle);
                uAtClientUnlock(atHandle);

                // Point i, nice and simple, <rat_a> and <bandmask_a0>.
                if (count >= 2) {
                    rats[0] = (int32_t) i[0];
                    masks[0][0] = i[1];
                }
                if (count >= 3) {
                    // Point ii, the "present" part.
                    if (count >= 4) {
                        // Point iii, the "present" part.
                        if (count >= 5) {
                            // Point iv, the "present" part, <bandmask_a1>,
                            // <rat_b> and <bandmask_b1>.
                            masks[0][1] = i[2];
                            rats[1] = (int32_t) i[3];
                            masks[1][0] = i[4];
                            if (count >= 6) {
                                // Point v, <bandmask_b1>.
                                masks[1][1] = i[5];
                            }
                        } else {
                            // Point iv, the "not present" part, <rat_b>
                            // and <bandmask_b0>.
                            rats[1] = (int32_t) i[2];
                            masks[1][0] = i[3];
                        }
                    } else {
                        // Point iii, the "not present" part, <bandmask_a1>.
                        masks[0][1] = i[2];
                    }
                } else {
                    // Point ii, the "not present" part, FINISH.
                }

                // Convert the RAT numbering to keep things simple on the brain
                for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
                    rats[x] = (int32_t) moduleRatBandMaskToCellRat(pInstance->pModule->moduleType, rats[x]);
                }
            }

            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6) {
                // LARA-R6 uses the same band-mask number for both 2G and 3G, which
                // will have been converted to our 2G RAT number by
                // moduleRatBandMaskToCellRat() so, if the user has asked for
                // U_CELL_NET_RAT_UTRAN then switch it
                if (rat == U_CELL_NET_RAT_UTRAN) {
                    rat = U_CELL_NET_RAT_GSM_GPRS_EGPRS;
                }
            }

            // Fill in the answers
            for (size_t x = 0; x < sizeof(rats) / sizeof(rats[0]); x++) {
                if (rats[x] == (int32_t) rat) {
                    *pBandMask1 = masks[x][0];
                    *pBandMask2 = masks[x][1];
                    uPortLog("U_CELL_CFG: band mask for RAT %d (in module terms %d)"
                             " is 0x%08x%08x %08x%08x.\n",
                             rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat),
                             (uint32_t) (*pBandMask2 >> 32), (uint32_t) (*pBandMask2),
                             (uint32_t) (*pBandMask1 >> 32), (uint32_t) (*pBandMask1));
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set the sole radio access technology to be used by the
// cellular module.
int32_t uCellCfgSetRat(uDeviceHandle_t cellHandle,
                       uCellNetRat_t rat)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            (rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
            (rat < U_CELL_NET_RAT_MAX_NUM)) {
            errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
            if (!uCellPrivateIsRegistered(pInstance)) {
                // The behaviour of URAT is significantly
                // different between SARA-U2 versus
                // SARA-R4/R5 so do them in separate
                // functions
                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_U201) {
                    errorCode = setRatSaraU2(pInstance, rat);
                } else {
                    // Do the mode change
                    errorCode = setRatSaraRx(pInstance, rat);
                }
                if (errorCode == 0) {
                    pInstance->rebootIsRequired = true;
                }
            } else {
                uPortLog("U_CELL_CFG: unable to set RAT as we are"
                         " connected to the network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set the radio access technology to be used at the
// given rank.
int32_t uCellCfgSetRatRank(uDeviceHandle_t cellHandle,
                           uCellNetRat_t rat, int32_t rank)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            /* U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED is allowed here */
            (rat >= U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
            (rat < U_CELL_NET_RAT_MAX_NUM) &&
            (rank >= 0) &&
            (rank < (int32_t) pInstance->pModule->maxNumSimultaneousRats)) {
            errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
            if (!uCellPrivateIsRegistered(pInstance)) {
                // The behaviour of URAT is significantly
                // different between SARA-U2 versus
                // SARA-R4/R5 so do them in separate
                // functions
                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_U201) {
                    errorCode = setRatRankSaraU2(pInstance, rat, rank);
                } else {
                    errorCode = setRatRankSaraRx(pInstance, rat, rank);
                }
                if (errorCode == 0) {
                    pInstance->rebootIsRequired = true;
                }
            } else {
                uPortLog("U_CELL_CFG: unable to set RAT as we are"
                         " connected to the network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the radio access technology that is being used by
// the cellular module at the given rank.
uCellNetRat_t uCellCfgGetRat(uDeviceHandle_t cellHandle,
                             int32_t rank)
{
    int32_t errorCodeOrRat = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrRat = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (rank >= 0) &&
            (rank < (int32_t) pInstance->pModule->maxNumSimultaneousRats)) {
            // The behaviour of URAT is significantly
            // different between SARA-U2 versus
            // SARA-R4/R5 so do them in separate
            // functions
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_U201) {
                errorCodeOrRat = (int32_t) getRatSaraU2(pInstance, rank);
            } else {
                errorCodeOrRat = (int32_t) getRatSaraRx(pInstance, rank);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return (uCellNetRat_t) errorCodeOrRat;
}

// Get the rank at which the given radio access technology
// is being used by the cellular module.
int32_t uCellCfgGetRatRank(uDeviceHandle_t cellHandle,
                           uCellNetRat_t rat)
{
    int32_t errorCodeOrRank = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrRank = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            (rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
            (rat < U_CELL_NET_RAT_MAX_NUM)) {
            // The behaviour of URAT is significantly
            // different between SARA-U2 versus
            // SARA-R4/R5 so do them in separate
            // functions
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_U201) {
                errorCodeOrRank = (int32_t) getRatRankSaraU2(pInstance, rat);
            } else {
                errorCodeOrRank = (int32_t) getRatRankSaraRx(pInstance, rat);
            }

            if (errorCodeOrRank >= 0) {
                uPortLog("U_CELL_CFG: rank of RAT %d (in module terms"
                         " %d) is %d.\n", rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat),
                         errorCodeOrRank);
            } else {
                uPortLog("U_CELL_CFG: RAT %d (in module terms %d) "
                         " is not ranked.\n", rat, cellRatToModuleRat(pInstance->pModule->moduleType, rat));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrRank;
}

// Set the MNO profile use by the cellular module.
int32_t uCellCfgSetMnoProfile(uDeviceHandle_t cellHandle,
                              int32_t mnoProfile)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (mnoProfile >= 0)) {
            errorCode = (int32_t) U_CELL_ERROR_CONNECTED;
            if (!uCellPrivateIsRegistered(pInstance)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UMNOPROF=");
                uAtClientWriteInt(atHandle, mnoProfile);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
                if (errorCode == 0) {
                    pInstance->rebootIsRequired = true;
                    uPortLog("U_CELL_CFG: MNO profile set to %d.\n",
                             mnoProfile);
                } else {
                    uPortLog("U_CELL_CFG: unable to set MNO profile"
                             " to %d.\n", mnoProfile);
                }
            } else {
                uPortLog("U_CELL_CFG: unable to set MNO Profile as we are"
                         " connected to the network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the MNO profile used by the cellular module.
int32_t uCellCfgGetMnoProfile(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrMnoProfile = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t mnoProfile;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrMnoProfile = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMNOPROF?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UMNOPROF:");
            mnoProfile = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCodeOrMnoProfile = uAtClientUnlock(atHandle);
            if ((errorCodeOrMnoProfile == 0) && (mnoProfile >= 0)) {
                uPortLog("U_CELL_CFG: MNO profile is %d.\n", mnoProfile);
                errorCodeOrMnoProfile = mnoProfile;
            } else {
                uPortLog("U_CELL_CFG: unable to read MNO profile.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrMnoProfile;
}

// Configure serial interface
int32_t uCellCfgSetSerialInterface(uDeviceHandle_t cellHandle, int32_t requestedVariant)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            // Lock mutex before using AT client.
            uAtClientLock(atHandle);
            // Send AT command.
            uAtClientCommandStart(atHandle, "AT+USIO=");
            // Write serial interface request variant
            uAtClientWriteInt(atHandle, requestedVariant);
            // Wait for response
            uAtClientCommandStopReadResponse(atHandle);
            // Unlock mutex after using AT client.
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                pInstance->rebootIsRequired = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the serial interface active configuration
int32_t uCellCfgGetActiveSerialInterface(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrActiveVariant = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrActiveVariant = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrActiveVariant = uCellPrivateGetActiveSerialInterface(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrActiveVariant;
}

// Set "AT+UDCONF".
int32_t uCellCfgSetUdconf(uDeviceHandle_t cellHandle, int32_t param1,
                          int32_t param2,  int32_t param3)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (param1 >= 0) && (param2 >= 0)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDCONF=");
            uAtClientWriteInt(atHandle, param1);
            uAtClientWriteInt(atHandle, param2);
            if (param3 >= 0) {
                uAtClientWriteInt(atHandle, param3);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                pInstance->rebootIsRequired = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get "AT+UDCONF".
int32_t uCellCfgGetUdconf(uDeviceHandle_t cellHandle, int32_t param1,
                          int32_t param2)
{
    int32_t errorCodeOrUdconf = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    size_t skip = 1;
    int32_t udconf;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrUdconf = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (param1 >= 0)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDCONF=");
            uAtClientWriteInt(atHandle, param1);
            if (param2 >= 0) {
                uAtClientWriteInt(atHandle, param2);
                // If we're writing a second parameter it
                // will be echoed back at us so we need to
                // skip it there
                skip++;
            }
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UDCONF:");
            // Skip the first and potentially second integers in
            // the response
            uAtClientSkipParameters(atHandle, skip);
            udconf = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCodeOrUdconf = uAtClientUnlock(atHandle);
            if ((errorCodeOrUdconf == 0) && (udconf >= 0)) {
                errorCodeOrUdconf = udconf;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrUdconf;
}

// Perform a factory reset: note that this function is not
// tested, so if you make changes please be sure to get them
// right!
int32_t uCellCfgFactoryReset(uDeviceHandle_t cellHandle, int32_t fsRestoreType,
                             int32_t nvmRestoreType)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            // Lock mutex before using AT client.
            uAtClientLock(atHandle);
            // Send AT command.
            uAtClientCommandStart(atHandle, "AT+UFACTORY=");
            // Write file system restore type.
            uAtClientWriteInt(atHandle, fsRestoreType);
            // Write NVM restore type.
            uAtClientWriteInt(atHandle, nvmRestoreType);
            // Terminate the entire AT command sequence by looking for the
            // `OK` or `ERROR` response.
            uAtClientCommandStopReadResponse(atHandle);
            // Unlock mutex after using AT client.
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                pInstance->rebootIsRequired = true;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set a greeting message.
int32_t uCellCfgSetGreeting(uDeviceHandle_t cellHandle, const char *pStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    size_t size;
    // +1 for terminator
    char buffer[U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES + 1];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            if (pInstance->pGreetingCallback != NULL) {
                // If there's already a greeting callback, remove
                // it 'cos this is the "non-callback" form
                size = getGreeting(atHandle, buffer, sizeof(buffer));
                if (size > 0) {
                    removeGreetingUrc(pInstance, buffer);
                }
                pInstance->pGreetingCallback = NULL;
                pInstance->pGreetingCallbackParameter = NULL;
            }
            // Now actually set the greeting
            errorCode = setGreeting(atHandle, pStr);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set a greeting message with a callback.
int32_t uCellCfgSetGreetingCallback(uDeviceHandle_t cellHandle,
                                    const char *pStr,
                                    void (*pCallback) (uDeviceHandle_t, void *),
                                    void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    size_t size;
    // +1 for terminator
    char buffer[U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES + 1];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (((pStr != NULL) && (strlen(pStr) <= U_CELL_CFG_GREETING_CALLBACK_MAX_LEN_BYTES)) ||
            (pCallback == NULL)) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                // Remove any existing callback
                size = getGreeting(atHandle, buffer, sizeof(buffer));
                if (size > 0) {
                    removeGreetingUrc(pInstance, buffer);
                }
                // Set the new greeting
                errorCode = setGreeting(atHandle, pStr);
                if (errorCode == 0) {
                    if (pCallback != NULL) {
                        errorCode = addGreetingUrc(pInstance, pStr);
                        if (errorCode != 0) {
                            // Clean up on error
                            setGreeting(atHandle, NULL);
                        }
                    }
                    if (errorCode == 0) {
                        pInstance->pGreetingCallback = pCallback;
                        pInstance->pGreetingCallbackParameter = pCallbackParam;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the current greeting message.
int32_t uCellCfgGetGreeting(uDeviceHandle_t cellHandle, char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pStr != NULL)) {
            errorCodeOrSize = getGreeting(pInstance->atHandle, pStr, size);
            if (errorCodeOrSize > 0) {
                uPortLog("U_CELL_CFG: greeting message is \"%s\".\n", pStr);
                // Note: LENA-R8 returns ERROR to AT+CGST? if the greeting
                // message is switched off, hence the extra condition below
            } else if ((errorCodeOrSize == 0) ||
                       ((errorCodeOrSize < 0) &&
                        (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LENA_R8))) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                *pStr = 0;
                uPortLog("U_CELL_CFG: no greeting message is set.\n");
            } else {
                uPortLog("U_CELL_CFG: unable to read greeting message.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Switch off auto-bauding in the cellular module.
int32_t uCellCfgSetAutoBaudOff(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t baudRate;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)) {
                errorCode = (int32_t) U_CELL_ERROR_AT;
                atHandle = pInstance->atHandle;
                // Get the current baud rate
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+IPR?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+IPR:");
                baudRate = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (baudRate > 0)) {
                    // Fix the baud rate to this value
                    errorCode = setAndStoreBaudRate(pInstance, baudRate);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Switch auto-bauding on in the cellular module.
int32_t uCellCfgSetAutoBaudOn(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)) {
                // Switch auto-bauding on
                errorCode = setAndStoreBaudRate(pInstance, 0);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Check if auto-bauding is on in the cellular module.
bool uCellCfgAutoBaudIsOn(uDeviceHandle_t cellHandle)
{
    bool autoBaudOn = false;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x;
    char buffer[16]; // enough room for "+IPR:115200"
    char *pStr;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) &&
            U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)) {
            atHandle = pInstance->atHandle;
            // Whether autobauding is on or off is a stored
            // value in the AT&V set.  This contains multiple
            // values, all we're interested in is the first set,
            // the ACTIVE PROFILE, and whether there is an entry
            // "+IPR:0" in it
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT&V");
            uAtClientCommandStop(atHandle);
            // The AT&V output appears on discrete lines
            // "ACTIVE PROFILE:" is on a line of its own
            uAtClientResponseStart(atHandle, "ACTIVE PROFILE:");
            // The next line has the S value settings etc.
            // e.g. &C1, &D0, &K0, &S1, E0, Q0, V1, S2:043, S3:013, S4:010, S5:008,
            uAtClientResponseStart(atHandle, NULL);
            // Then the next line includes the +IPR setting, e.g.
            // +ICF:3,1, +IFC:0,0, +IPR:0,
            uAtClientResponseStart(atHandle, NULL);
            while (((x = uAtClientReadString(atHandle, buffer,
                                             sizeof(buffer), false)) > 0) &&
                   !autoBaudOn) {
                // Remove any leading spaces from buffer (this AT
                // command differs from all the others as it seems
                // to have them)
                pStr = buffer;
                for (int32_t y = 0; y < x; y++) {
                    if (*pStr == ' ') {
                        pStr++;
                    }
                }
                autoBaudOn = (strcmp(pStr, "+IPR:0") == 0);
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return autoBaudOn;
}

// Set "AT+UGPRF".
int32_t uCellCfgSetGnssProfile(uDeviceHandle_t cellHandle, int32_t profileBitMap,
                               const char *pServerName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = uCellPrivateSetGnssProfile(pInstance, profileBitMap, pServerName);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get "AT+UGPRF".
int32_t uCellCfgGetGnssProfile(uDeviceHandle_t cellHandle, char *pServerName,
                               size_t sizeBytes)
{
    int32_t errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrBitMap = uCellPrivateGetGnssProfile(pInstance, pServerName, sizeBytes);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrBitMap;
}

// Set the time in the cellular module.
int64_t uCellCfgSetTime(uDeviceHandle_t cellHandle, int64_t timeLocal,
                        int32_t timeZoneSeconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    struct tm tmStruct;
    char buffer[32];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        // The format is "yy/MM/dd,hh:mm:ss+TZ" where +TZ is
        // in quarter hours.  First get the time in a struct
        if ((pInstance != NULL) && gmtime_r((const time_t *) &timeLocal, &tmStruct) != NULL) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            int32_t ignored = snprintf(buffer, sizeof(buffer), "%02d/%02d/%02d,%02d:%02d:%02d%c%02d",
                                       tmStruct.tm_year % 100, tmStruct.tm_mon + 1, tmStruct.tm_mday,
                                       tmStruct.tm_hour, tmStruct.tm_min, tmStruct.tm_sec,
                                       timeZoneSeconds >= 0 ? '+' : '-', // Do this cos %+02d doesn't fill with zeroes as it should
                                       timeZoneSeconds >= 0 ? (int) timeZoneSeconds / (15 * 60) : (int) - timeZoneSeconds / (15 * 60));
            // This to stop GCC 12.3.0 complaining that variables printed into buffer are being truncated
            (void) ignored;
#pragma GCC diagnostic pop
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CCLK=");
            uAtClientWriteString(atHandle, buffer, true);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if (errorCode == 0) {
                uPortLog("U_CELL_CFG: time set to %s.\n", buffer);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
