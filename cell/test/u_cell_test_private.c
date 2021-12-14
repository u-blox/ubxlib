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
 * @brief Common stuff used in testing of the cellular API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_cfg.h"
#include "u_cell_info.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifndef U_CELL_NET_TEST_RAT
/** When we do testing by default we set a single RAT to make
 * things simple and quick.  The RAT to use is fixed based on what
 * the module supports: if the module supports CAT-M1 then use
 * CAT-M1 as it will be connected to our Nutaq network box for testing.
 * Else if it supports NB1 then use NB1 for the same reason.  Else
 * LTE, else UTRAN, else GSM.  This can be overridden with the
 * #define U_CELL_NET_TEST_RAT.
 */
static const uCellNetRat_t gNetworkOrder[] = {U_CELL_NET_RAT_CATM1,
                                              U_CELL_NET_RAT_NB1,
                                              U_CELL_NET_RAT_LTE,
                                              U_CELL_NET_RAT_UTRAN,
                                              U_CELL_NET_RAT_GSM_GPRS_EGPRS
                                             };
#endif

/** Descriptions for each RAT.
 */
static const char *const pRatStr[] = {"unknown or not used",
                                      "GSM/GPRS/EGPRS",
                                      "GSM Compact",
                                      "UTRAN",
                                      "EGPRS",
                                      "HSDPA",
                                      "HSUPA",
                                      "HSDPA/HSUPA",
                                      "LTE",
                                      "EC GSM",
                                      "CAT-M1",
                                      "NB1"
                                     };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set the given context.
static void contextSet(int32_t cellHandle,
                       int32_t contextId,
                       const char *pApn)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    bool present = false;
    bool changeIt = false;
    int32_t y = 0;
    char buffer[32];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientTimeoutSet(atHandle,
                                pInstance->pModule->responseMaxWaitMs);
            uAtClientCommandStart(atHandle, "AT+CGDCONT?");
            uAtClientCommandStop(atHandle);
            for (size_t x = 0; (x < U_CELL_NET_MAX_NUM_CONTEXTS) &&
                 !present && (y >= 0); x++) {
                uAtClientResponseStart(atHandle, "+CGDCONT:");
                // Check if this is our context ID
                y = uAtClientReadInt(atHandle);
                present = (y == contextId);
                // Skip the IP field
                uAtClientSkipParameters(atHandle, 1);
                // Read the APN field
                y = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
                if (pApn != NULL) {
                    changeIt = (strcmp(buffer, pApn) != 0);
                } else {
                    changeIt = (y > 0);
                }
            }
            uAtClientResponseStop(atHandle);
            // Don't check for errors here as we will likely
            // have a timeout through waiting for an +CGDCONT that
            // didn't come.
            uAtClientUnlock(atHandle);
            if (changeIt) {
                // Change it and read it back for diagnostics
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+CGDCONT=");
                uAtClientWriteInt(atHandle, contextId);
                uAtClientWriteString(atHandle, "IP", true);
                if (pApn != NULL) {
                    uAtClientWriteString(atHandle, pApn, true);
                }
                uAtClientCommandStopReadResponse(atHandle);
                uAtClientCommandStart(atHandle, "AT+CGDCONT?");
                uAtClientCommandStopReadResponse(atHandle);
                uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// The standard preamble for a cell test.
int32_t uCellTestPrivatePreamble(uCellModuleType_t moduleType,
                                 uCellTestPrivate_t *pParameters,
                                 bool powerOn)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t mnoProfile;
    uCellNetRat_t rat;
    uCellNetRat_t primaryRat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    bool setRat = false;
    uint64_t bandMask1;
    uint64_t bandMask2;
    bool rebootRequired = false;
    char imsi[U_CELL_INFO_IMSI_SIZE];

    // Set some defaults
    pParameters->uartHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->cellHandle = -1;

    uPortLog("U_CELL_TEST_PRIVATE: test preamble start.\n");

    // Initialise the porting layer
    if (uPortInit() == 0) {
        uPortLog("U_CELL_TEST_PRIVATE: opening UART %d...\n",
                 U_CFG_APP_CELL_UART);
        // Open a UART with the standard parameters
        pParameters->uartHandle = uPortUartOpen(U_CFG_APP_CELL_UART,
                                                U_CELL_UART_BAUD_RATE, NULL,
                                                U_CELL_UART_BUFFER_LENGTH_BYTES,
                                                U_CFG_APP_PIN_CELL_TXD,
                                                U_CFG_APP_PIN_CELL_RXD,
                                                U_CFG_APP_PIN_CELL_CTS,
                                                U_CFG_APP_PIN_CELL_RTS);
    }

    if (pParameters->uartHandle >= 0) {
        if (uAtClientInit() == 0) {
            uPortLog("U_CELL_TEST_PRIVATE: adding an AT client on UART %d...\n",
                     U_CFG_APP_CELL_UART);
            pParameters->atClientHandle = uAtClientAdd(pParameters->uartHandle,
                                                       U_AT_CLIENT_STREAM_TYPE_UART,
                                                       NULL,
                                                       U_CELL_AT_BUFFER_LENGTH_BYTES);
        }
    }

    if (pParameters->atClientHandle != NULL) {
        // So that we can see what we're doing
        uAtClientPrintAtSet(pParameters->atClientHandle, true);
        uAtClientDebugSet(pParameters->atClientHandle, true);
        if (uCellInit() == 0) {
            uPortLog("U_CELL_TEST_PRIVATE: adding a cellular instance on"
                     " the AT client...\n");
            pParameters->cellHandle = uCellAdd(moduleType,
                                               pParameters->atClientHandle,
                                               U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                               U_CFG_APP_PIN_CELL_PWR_ON,
                                               U_CFG_APP_PIN_CELL_VINT, false);
        }
    }

    if (pParameters->cellHandle >= 0) {
        cellHandle = pParameters->cellHandle;
        if (powerOn) {
            // Power up
            uPortLog("U_CELL_TEST_PRIVATE: powering on...\n");
            errorCode = uCellPwrOn(cellHandle, U_CELL_TEST_CFG_SIM_PIN, NULL);
            if (errorCode == 0) {
                // Note: if this is a SARA-R422 module, which supports only
                // 1.8V SIMs, the SIM cards we happen to use in the ubxlib test farm
                // send an ATR which indicates they do NOT support 1.8V operation,
                // even though they do, and this will cause power-on to fail since
                // "+CME ERROR: SIM not inserted" is spat out by the module from
                // quite early on, in response to even non-SIM related AT commands
                // (e.g. AT&C1).
                // This is fixed with an AT+UDCONF=92,1,1 command which
                // can be sent with uCellCfgSetUdconf() however unfortunately we
                // can't send it here since even power on will have failed because
                // of the CME ERRORs: you will need to just hack "AT+UDCONF=92,1,1"
                // into the gpConfigCommand[] list in u_cell_pwr.c, just after "ATI9",
                // and then make sure you reboot afterwards to write the setting to
                // non-volatile memory.  Once this is done the hack can be removed.

                // Give the module time to read its SIM before we continue
                // or it might refuse to answer some commands (e.g. AT+URAT?)
                errorCode = uCellInfoGetImsi(cellHandle, imsi);
                if (errorCode == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                    // Set a greeting message so that we can see spot if the module
                    // has rebooted underneath us
                    uCellCfgSetGreeting(cellHandle, U_CELL_PRIVATE_GREETING_STR);
                    pModule = pUCellPrivateGetModule(cellHandle);
                    if (pModule != NULL) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        if (U_CELL_PRIVATE_HAS(pModule,
                                               U_CELL_PRIVATE_FEATURE_MNO_PROFILE)) {
                            // Ensure that the MNO profile, where supported, is set
                            // to the one we want
                            mnoProfile = uCellCfgGetMnoProfile(cellHandle);
                            if ((mnoProfile >= 0) &&
                                (mnoProfile != U_CELL_TEST_CFG_MNO_PROFILE)) {
                                errorCode = uCellCfgSetMnoProfile(cellHandle,
                                                                  U_CELL_TEST_CFG_MNO_PROFILE);
                                // SARA-R412M-02B modules with SW version M0.10.0 fresh out of
                                // the box are set to MNO profile 0 which stops any configuration
                                // being performed (setting the RAT won't work, for instance) so
                                // re-boot immediately here just in case
                                if (errorCode == 0) {
                                    errorCode = uCellPwrReboot(cellHandle, NULL);
                                }
                            }
                        }
                        if (errorCode == 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
                            // Ensure that the sole RAT set is the one
                            // we want for testing this module
                            for (size_t x = 0; x < pModule->maxNumSimultaneousRats; x++) {
                                rat = uCellCfgGetRat(cellHandle, (int32_t) x);
                                if (rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
                                    if (x == 0) {
                                        primaryRat = rat;
                                        // This is the *only* RAT we want: is it
                                        // set the way we want it?
#ifdef U_CELL_NET_TEST_RAT
                                        if (primaryRat != U_CELL_NET_TEST_RAT) {
                                            primaryRat = U_CELL_NET_TEST_RAT;
                                            setRat = true;
                                        }
#else
                                        for (size_t y = 0; !setRat &&
                                             (y < sizeof(gNetworkOrder) / sizeof(gNetworkOrder[0])) &&
                                             (primaryRat != gNetworkOrder[y]); y++) {
                                            if ((pModule->supportedRatsBitmap & (1U << (int32_t) gNetworkOrder[y])) &&
                                                (primaryRat != gNetworkOrder[y])) {
                                                primaryRat = gNetworkOrder[y];
                                                setRat = true;
                                            }
                                        }
#endif
                                    } else {
                                        if (!setRat) {
                                            // We haven't already decided to set a sole RAT,
                                            // make sure that there are no alternative RATs
                                            if (rat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) {
                                                // If more than a single RAT is set then we
                                                // must set the sole RAT to get rid of them
                                                setRat = true;
                                            }
                                        }
                                    }
                                } else {
                                    // No point in looping if uCellCfgGetRat()
                                    // has returned nothing
                                    break;
                                }
                            }
                            if (setRat && (primaryRat > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED)) {
                                errorCode = uCellCfgSetRat(cellHandle, primaryRat);
                                rebootRequired = true;
                            } else {
                                // If we haven't set or read a sole RAT then
                                // the module doesn't support it, just carry on
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            }
                        }

                        // If we're on cat-M1 or NB1, set the band-mask
                        // correctly for the Nutaq network box we use for testing
                        if ((errorCode == 0) &&
                            ((primaryRat == U_CELL_NET_RAT_CATM1) ||
                             (primaryRat == U_CELL_NET_RAT_NB1))) {
                            errorCode = uCellCfgGetBandMask(cellHandle, primaryRat,
                                                            &bandMask1, &bandMask2);
                            if (errorCode == 0) {
                                // bandMaskx must be exactly U_CELL_TEST_CFG_BANDMASKx
                                // unless they are both set to zero (an invalid value
                                // which we interpret as "leave alone")
                                //lint -e{774, 587, 845} Suppress always evaluates to True
                                if (((U_CELL_TEST_CFG_BANDMASK1 != 0) ||
                                     (U_CELL_TEST_CFG_BANDMASK2 != 0)) &&
                                    ((bandMask1 != U_CELL_TEST_CFG_BANDMASK1) ||
                                     (bandMask2 != U_CELL_TEST_CFG_BANDMASK2))) {
                                    // Set the band masks
                                    errorCode = uCellCfgSetBandMask(cellHandle, primaryRat,
                                                                    U_CELL_TEST_CFG_BANDMASK1,
                                                                    U_CELL_TEST_CFG_BANDMASK2);
                                    rebootRequired = true;
                                }
                            }
                            if (errorCode == 0) {
                                // On LTE, if the APN is wrong we will be
                                // denied service, so set the AT+CGDCONT
                                // entry correctly
                                contextSet(cellHandle, U_CELL_NET_CONTEXT_ID,
                                           U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_EUTRAN_APN));
                            }
                        }

                        // Re-boot if we've made a change
                        if ((errorCode == 0) && rebootRequired) {
                            errorCode = uCellPwrReboot(cellHandle, NULL);
                        }
                    }
                }

                if (errorCode == 0) {
                    uPortLog("U_CELL_TEST_PRIVATE: test preamble end.\n");
                }
            }
        } else {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// The standard postamble for a cell test.
void uCellTestPrivatePostamble(uCellTestPrivate_t *pParameters,
                               bool powerOff)
{
#if U_CFG_APP_PIN_CELL_PWR_ON >= 0
    if (powerOff && (pParameters->cellHandle >= 0)) {
        uCellPwrOff(pParameters->cellHandle, NULL);
    }
#endif

    uPortLog("U_CELL_TEST_PRIVATE: deinitialising cellular API...\n");
    // Let uCellDeinit() remove the cell handle
    uCellDeinit();

    uPortLog("U_CELL_TEST_PRIVATE: removing AT client...\n");
    uAtClientRemove(pParameters->atClientHandle);
    uAtClientDeinit();

    uPortUartClose(pParameters->uartHandle);
    pParameters->uartHandle = -1;

    uPortDeinit();
}

// The standard clean-up for a cell test.
void uCellTestPrivateCleanup(uCellTestPrivate_t *pParameters)
{
    uCellDeinit();
    uAtClientDeinit();
    if (pParameters->uartHandle >= 0) {
        uPortUartClose(pParameters->uartHandle);
    }
    pParameters->uartHandle = -1;
}

// Return a string describing a RAT.
const char *pUCellTestPrivateRatStr(uCellNetRat_t rat)
{
    const char *pStr = "UNKNOWN";

    // Lots of casting to keep Lint happy
    if (((int32_t) rat >= 0) && ((size_t) (int32_t) rat < sizeof(pRatStr) / sizeof (pRatStr[0]))) {
        pStr = pRatStr[rat];
    }

    return pStr;
}

// Return the sole RAT that the uCellTestPrivatePreamble() ensures
// will be set before a test begins.
uCellNetRat_t uCellTestPrivateInitRatGet(uint32_t supportedRatsBitmap)
{
#ifdef U_CELL_NET_TEST_RAT
    return U_CELL_NET_TEST_RAT;
#else
    uCellNetRat_t rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    for (size_t x = 0; (rat == U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) &&
         (x < sizeof(gNetworkOrder) / sizeof(gNetworkOrder[0])); x++) {
        if (supportedRatsBitmap & (1U << (int32_t) gNetworkOrder[x])) {
            rat = gNetworkOrder[x];
        }
    }

    return rat;
#endif
}

// End of file
