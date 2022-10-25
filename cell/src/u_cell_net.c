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
 * @brief Implementation of the network API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MAX
#include "stdlib.h"    // atoi()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), memcmp(), strlen()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_info.h"
#include "u_cell_apn_db.h"
#include "u_cell_mno_db.h"

#include "u_cell_pwr_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The length of temporary buffer to use when reading network scan
 * results, sufficient to store N of:
 *
 * (stat,long_name,short_name,numeric[,AcT])
 */
#define U_CELL_NET_SCAN_LENGTH_BYTES (128 * 10)

/** The type of CEREG to request; 4 to get the 3GPP sleep parameters
 * also.
 * IMPORTANT: if this value ever needs to change, because of the
 * similarity between the response to this AT command and the URC,
 * it needs to be considered _very_ carefully, need to be sure that
 * the dodge in CXREG_urc() and registerNetwork() still works.
*/
#define U_CELL_NET_CEREG_TYPE 4

/** The type of CREG/CGREG to request.
 * IMPORTANT: if this value ever needs to change, because of the
 * similarity between the response to this AT command and the URC,
 * it needs to be considered _very_ carefully, need to be sure that
 * the dodge in CXREG_urc() and registerNetwork() still works.
*/
#define U_CELL_NET_CREG_OR_CGREG_TYPE 2

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to accommodate the types of registration query.
 */
typedef struct {
    uCellNetRegDomain_t domain;
    const char *pSetStr;
    const char *pQueryStr;
    const char *pResponseStr;
    int32_t type;
    uint32_t supportedRatsBitmap;
} uCellNetRegTypes_t;

/** All the parameters for the registration status callback.
 */
typedef struct {
    uCellNetRegDomain_t domain;
    uCellNetStatus_t networkStatus;
    void (*pCallback) (uCellNetRegDomain_t, uCellNetStatus_t, void *);
    void *pCallbackParameter;
} uCellNetRegistationStatus_t;

/** All the parameters for the base station connection status callback.
 */
typedef struct {
    bool isConnected;
    void (*pCallback) (bool, void *);
    void *pCallbackParameter;
} uCellNetConnectionStatus_t;

/** All the parameters for 3GPP power saving parameters callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t, bool, int32_t, int32_t, void *);
    bool onNotOff;
    int32_t activeTimeSeconds;
    int32_t periodicWakeupSeconds;
    void *pCallbackParam;
} uCellNet3gppPowerSavingCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Table to convert the RAT value returned by an AT+COPS
 * or AT+CxREG command into a uCellNetRat_t value.
 */
static const uCellNetRat_t g3gppRatToCellRat[] = {
    U_CELL_NET_RAT_GSM_GPRS_EGPRS, // 0: 2G
    U_CELL_NET_RAT_GSM_COMPACT,    // 1: GSM compact
    U_CELL_NET_RAT_UTRAN,          // 2: UTRAN
    U_CELL_NET_RAT_EGPRS,          // 3: EDGE
    U_CELL_NET_RAT_HSDPA,          // 4: UTRAN with HSDPA
    U_CELL_NET_RAT_HSUPA,          // 5: UTRAN with HSUPA
    U_CELL_NET_RAT_HSDPA_HSUPA,    // 6: UTRAN with HSDPA and HSUPA
    U_CELL_NET_RAT_LTE,            // 7: LTE, which includes cat-M1
    U_CELL_NET_RAT_EC_GSM,         // 8: EC-GSM
    U_CELL_NET_RAT_NB1             // 9: E-UTRAN (NB-S1 mode)
};

/** Table to convert the status values returned by an AT+CxREG
 * command into a uCellNetStatus_t value.
 */
static const uCellNetStatus_t g3gppStatusToCellStatus[] = {
    U_CELL_NET_STATUS_NOT_REGISTERED,              // +CEREG: 0
    U_CELL_NET_STATUS_REGISTERED_HOME,             // +CEREG: 1
    U_CELL_NET_STATUS_SEARCHING,                   // +CEREG: 2
    U_CELL_NET_STATUS_REGISTRATION_DENIED,         // +CEREG: 3
    U_CELL_NET_STATUS_OUT_OF_COVERAGE,             // +CEREG: 4
    U_CELL_NET_STATUS_REGISTERED_ROAMING,          // +CEREG: 5
    U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_HOME,    // +CEREG: 6
    U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_ROAMING, // +CEREG: 7
    U_CELL_NET_STATUS_EMERGENCY_ONLY,              // +CEREG: 8
    U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME,     // +CEREG: 9
    U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING   // +CEREG: 10
};

/** The possible registration query strings.
 */
static const uCellNetRegTypes_t gRegTypes[] = {
    {U_CELL_NET_REG_DOMAIN_CS, "AT+CREG=", "AT+CREG?", "+CREG:", U_CELL_NET_CREG_OR_CGREG_TYPE, INT_MAX /* All RATs */},
    {U_CELL_NET_REG_DOMAIN_PS, "AT+CGREG=", "AT+CGREG?", "+CGREG:", U_CELL_NET_CREG_OR_CGREG_TYPE, INT_MAX /* All RATs */},
    {
        U_CELL_NET_REG_DOMAIN_PS, "AT+CEREG=", "AT+CEREG?", "+CEREG:", U_CELL_NET_CEREG_TYPE,
        (1UL << (int32_t) U_CELL_NET_RAT_LTE) | (1UL << (int32_t) U_CELL_NET_RAT_CATM1) | (1UL << (int32_t) U_CELL_NET_RAT_NB1)
    },
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: FORWARD DECLARATIONS
 * -------------------------------------------------------------- */

static int32_t activateContext(const uCellPrivateInstance_t *pInstance,
                               int32_t contextId, int32_t profileId);

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: URC AND RELATED FUNCTIONS
 * -------------------------------------------------------------- */

// Callback via which the user's registration status callback
// is called.  This must be called through the uAtClientCallback()
// mechanism in order to prevent customer code blocking the AT
// client.
static void registrationStatusCallback(uAtClientHandle_t atHandle,
                                       void *pParameter)
{
    uCellNetRegistationStatus_t *pStatus = (uCellNetRegistationStatus_t *) pParameter;

    (void) atHandle;

    if (pStatus != NULL) {
        if (pStatus->pCallback != NULL) {
            pStatus->pCallback(pStatus->domain, pStatus->networkStatus,
                               pStatus->pCallbackParameter);
        }
        uPortFree(pStatus);
    }
}

// Callback via which the user's 3GPP power saving parameters callback
// is called.  This must be called through the uAtClientCallback()
// mechanism in order to prevent customer code blocking the AT
// client.
static void powerSaving3gppCallback(uAtClientHandle_t atHandle,
                                    void *pParameter)
{
    uCellNet3gppPowerSavingCallback_t *pCallback = (uCellNet3gppPowerSavingCallback_t *) pParameter;

    (void) atHandle;

    if (pCallback != NULL) {
        if (pCallback->pCallback != NULL) {
            pCallback->pCallback(pCallback->cellHandle, pCallback->onNotOff,
                                 pCallback->activeTimeSeconds,
                                 pCallback->periodicWakeupSeconds,
                                 pCallback->pCallbackParam);
        }
        uPortFree(pCallback);
    }
}

// Callback that will be called if we need to reactivate a context
// on regaining service after some sort of network outage.
static void activateContextCallback(uAtClientHandle_t atHandle,
                                    void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;

    (void) atHandle;

    activateContext(pInstance, U_CELL_NET_CONTEXT_ID, U_CELL_NET_PROFILE_ID);
}

// Set the current network status.
// Deliberately using VERY short debug strings as this
// might be called from a URC.
static void setNetworkStatus(uCellPrivateInstance_t *pInstance,
                             uCellNetStatus_t status, int32_t rat,
                             uCellNetRegDomain_t domain,
                             bool fromUrc)
{
    uCellNetRegistationStatus_t *pStatus;
    bool printAllowed = true;
#if U_CFG_OS_CLIB_LEAKS
    // If we're in a URC and the C library leaks memory
    // when printf() is called from a dynamically
    // allocated task (which a URC is), then don't
    // print stuff
    if (fromUrc) {
        printAllowed = false;
    }
#else
    (void) fromUrc;
#endif

    switch (status) {
        case U_CELL_NET_STATUS_NOT_REGISTERED:
            // Not (yet) registered (+CxREG: 0)
            if (printAllowed) {
                uPortLog("%d: NReg\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_HOME:
            // Registered on the home network (+CxREG: 1)
            if (printAllowed) {
                uPortLog("%d: RegH\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_SEARCHING:
            // Searching for a network (+CxREG: 2)
            if (printAllowed) {
                uPortLog("%d: Search\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTRATION_DENIED:
            // Registeration denied (+CxREG: 3)
            if (printAllowed) {
                uPortLog("%d: Deny\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_OUT_OF_COVERAGE:
            // Out of coverage (+CxREG: 4)
            if (printAllowed) {
                uPortLog("%d: OoC\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_ROAMING:
            // Registered on a roaming network (+CxREG: 5)
            if (printAllowed) {
                uPortLog("%d: RegR\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_HOME:
            // Registered for SMS only on the home network
            // (+CxREG: 6)
            if (printAllowed) {
                uPortLog("%d: RegS\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_ROAMING:
            // Registered for SMS only on a roaming network
            // (+CxREG: 7)
            if (printAllowed) {
                uPortLog("%d: RegS\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_EMERGENCY_ONLY:
            // Registered for emergency service only (+CxREG: 8)
            if (printAllowed) {
                uPortLog("%d: RegE\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME:
            // Registered on the home network, CFSB not preferred
            // (+CxREG: 9)
            if (printAllowed) {
                uPortLog("%d: RegNC\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING:
            // Registered on a roaming network, CFSB not preferred
            // (+CxREG: 10)
            if (printAllowed) {
                uPortLog("%d: RegNC\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_TEMPORARY_NETWORK_BARRING:
            // Temporary barring
            if (printAllowed) {
                uPortLog("%d: NRegB\n", rat);
            }
            break;
        case U_CELL_NET_STATUS_UNKNOWN:
        default:
            // Unknown registration status
            if (printAllowed) {
                uPortLog("%d: Unk %d\n", rat, status);
            }
            break;
    }

    pInstance->networkStatus[domain] = status;

    pInstance->rat[domain] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    if (U_CELL_NET_STATUS_MEANS_REGISTERED(status) &&
        (rat >= 0) &&
        (rat < (int32_t) (sizeof(g3gppRatToCellRat) /
                          sizeof(g3gppRatToCellRat[0])))) {
        pInstance->rat[domain] = (uCellNetRat_t) g3gppRatToCellRat[rat];
        if ((pInstance->rat[domain] == U_CELL_NET_RAT_LTE) &&
            !(pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_LTE)) &&
            (pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_CATM1))) {
            // The RAT on the end of the network status indication doesn't
            // differentiate between LTE and Cat-M1 so, if the device doesn't
            // support LTE but does support Cat-M1, switch it
            pInstance->rat[domain] = U_CELL_NET_RAT_CATM1;
        }
        if (pInstance->profileState == U_CELL_PRIVATE_PROFILE_STATE_REQUIRES_REACTIVATION) {
            // This flag will be set if we had been knocked out
            // of our PDP context by a network outage and need
            // to get it back again; make sure to get this in the
            // queue before any user registratioon status callback
            // so that everything is sorted for them
            if (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                                    U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                // Use the AT client's callback mechanism to do the operation
                // out of the URC task
                uAtClientCallback(pInstance->atHandle,
                                  activateContextCallback, pInstance);
            }
            pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_UP;
        }
    }

    // Set the sleep state based on this new RAT state
    uCellPrivateSetDeepSleepState(pInstance);

    if (pInstance->pRegistrationStatusCallback != NULL) {
        // If the user has a callback for this, put all the
        // data in a struct and pass a pointer to it to our
        // local callback via the AT client's callback mechanism
        // to decouple it from any URC handler.
        // Note: it is up to registrationStatusCallback() to free the
        // allocated memory
        pStatus = (uCellNetRegistationStatus_t *) pUPortMalloc(sizeof(*pStatus));
        if (pStatus != NULL) {
            pStatus->domain = domain;
            pStatus->networkStatus = status;
            pStatus->pCallback = pInstance->pRegistrationStatusCallback;
            pStatus->pCallbackParameter = pInstance->pRegistrationStatusCallbackParameter;
            uAtClientCallback(pInstance->atHandle,
                              registrationStatusCallback, pStatus);
        }
    }
}

// Registration on a network (AT+CREG/CGREG/CEREG).
// Note: there are cases where the RAT value is not signalled as part
// of the AT response: e.g. LARA-R6 can just send:
// +CEREG: 4,5,,,,,,,"00000000","01100000"
// ...in response to an AT+CEREG? query. For these cases assumed3gppRat
// must be provided so that this function can do something useful.
static inline uCellNetStatus_t CXREG_urc(uCellPrivateInstance_t *pInstance,
                                         uCellNetRegDomain_t domain,
                                         int32_t assumed3gppRat)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t status3gpp;
    uCellNetStatus_t status = U_CELL_NET_STATUS_UNKNOWN;
    int32_t secondInt;
    int32_t rat = -1;
    int32_t skippedParameters = 1;
    bool responseToCommandNotUrc = false;

    // As described in registerNetwork(), it is possible
    // for this URC handler to capture the response to
    // an AT+CxREG? command instead of the URC, so
    // do some dodging here to avoid it.
    // The first integer might either by the mode we set, <n>,
    // sent back to us or it might be the <status> value of the
    // URC.  The dodge to distinguish the two is based on the
    // fact that our values for <n> match status values that mean
    // "not registered", so we can do this:
    // (a) if the first integer matches the <n>/mode
    //     parameter from the AT+CxREG=<n>,... command, then either
    //     i)  this is a response to a AT+CxREG command and
    //         the status etc. parameters follow, or,
    //     ii) this is a URC with a value indicating we are not
    //         registered and hence will not be followed
    //         by any further parameters,
    // (b) if the first integer does not match <n> then this
    //     is a URC and the first integer is the <status> value.

    // Assume case (b) at the outset
    status3gpp = uAtClientReadInt(atHandle);
    secondInt = uAtClientReadInt(atHandle);
    if ((status3gpp == U_CELL_NET_CREG_OR_CGREG_TYPE) ||
        (status3gpp == U_CELL_NET_CEREG_TYPE)) {
        // case (a.i) or (a.ii)
        if (secondInt < 0) {
            // case (a.ii)
            uAtClientClearError(atHandle);
        } else {
            // case (a.i)
            status3gpp = secondInt;
            responseToCommandNotUrc = true;
        }
    }
    if ((status3gpp >= 0) &&
        (status3gpp < (int32_t) (sizeof(g3gppStatusToCellStatus) /
                                 sizeof(g3gppStatusToCellStatus[0])))) {
        status = g3gppStatusToCellStatus[status3gpp];
    }
    if (U_CELL_NET_STATUS_MEANS_REGISTERED(status)) {
        // Note: this used to be simple but a combination of 3GPP power saving
        // and SARA-R4xx-02B/LARA-R6 has made it complex.  After having dealt
        // with the first two integers of the URC, there is a parameter that has
        // to be skipped before the RAT can be read.  However, in the specific case
        // of CEREG type 4 (so not for CREG or CGREG) and on SARA-R4xx-02B in
        // all cases and on LARA-R6 JUST in the "response to AT+CEREG" case (the
        // URC is different), an additional parameter is inserted (not added on
        // the end, inserted)/ which has to be skipped before the RAT can be read.
        if ((gRegTypes[2 /* CEREG */].type == 4) &&
            (((pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R410M_02B) ||
              (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R412M_02B)) ||
             ((pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6) &&
              responseToCommandNotUrc))) {
            skippedParameters++;
        }
        // Skip <ci> (<lac> already absorbed by the
        // read of secondInt above) and potentially
        // <rac_or_mme>
        uAtClientSkipParameters(atHandle, skippedParameters);
        // Read the RAT that we're on
        rat = uAtClientReadInt(atHandle);
        // Use the assumed 3GPP RAT if no RAT is included
        if (rat < 0) {
            rat = assumed3gppRat;
        }
    }
    setNetworkStatus(pInstance, status, rat, domain, true);

    return status;
}

// Registration on a network in the circuit switched
// domain (AT+CREG).
static void CREG_urc(uAtClientHandle_t atHandle,
                     void *pParameter)
{
    (void) atHandle;
    // Doesn't really matter what the assumed3gppRat parameter
    // is here, it is only used in the LTE/Cat-M1 case
    CXREG_urc((uCellPrivateInstance_t *) pParameter,
              U_CELL_NET_REG_DOMAIN_CS, -1);
}

// Registration on a network in the packet-switched
// domain (AT+CGREG).
static void CGREG_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    (void) atHandle;
    // Doesn't really matter what the assumed3gppRat parameter
    // is here, it is only used in the LTE/Cat-M1 case
    CXREG_urc((uCellPrivateInstance_t *) pParameter,
              U_CELL_NET_REG_DOMAIN_PS, -1);
}

// Registration on an EUTRAN (LTE) network (AT+CEREG)
// in the packet-switched domain.
static void CEREG_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uCellNetStatus_t status;
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    uCellPrivateSleep_t *pSleepContext = pInstance->pSleepContext;
    uCellNet3gppPowerSavingCallback_t *pCallback;
    char encoded[8 + 1] = {0}; // Timer value encoded as 3GPP IE
    int32_t bytesRead;
    bool onNotOff;
    int32_t activeTimeSeconds = -1;
    int32_t periodicWakeupSeconds = -1;
    int32_t assumed3gppRat = 7; // LTE

    if (!(pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_LTE)) &&
        (pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_CATM1))) {
        // Assumed RAT has to be Cat-M1 if we don't support LTE
        assumed3gppRat = 8; // Cat-M1
    }

    status = CXREG_urc(pInstance, U_CELL_NET_REG_DOMAIN_PS, assumed3gppRat);
    if (U_CELL_NET_STATUS_MEANS_REGISTERED(status) &&
        (pSleepContext != NULL)) {
        // If we have a sleep context, try to read the
        // parameters from the end of +CEREG also
        // CXREG_urc() will have read up to and including
        // the parameter indicating the active RAT, next
        // skip the <cause_type> and <reject_cause> parameters
        uAtClientSkipParameters(atHandle, 2);
        // Now read the active time, T3324, as a string, and decode it
        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
        if (bytesRead > 0) {
            uCellPwrPrivateActiveTimeStrToSeconds(encoded, &activeTimeSeconds);
        }
        // Read the periodic wake-up time, T3412 ext, as a string,
        // and decode it
        bytesRead = uAtClientReadString(atHandle, encoded, sizeof(encoded), false);
        if (bytesRead > 0) {
            uCellPwrPrivatePeriodicWakeupStrToSeconds(encoded, true,
                                                      &periodicWakeupSeconds);
        }
        onNotOff = (activeTimeSeconds >= 0);
        // Update the 3GPP power saving status in the sleep context
        pSleepContext->powerSaving3gppAgreed = onNotOff;
        // Inform the user if there is a callback and the parameters have changed
        if ((pSleepContext->p3gppPowerSavingCallback != NULL) &&
            //lint -e(731) Suppress use of Boolean argument in comparison
            ((pSleepContext->powerSaving3gppOnNotOffCereg != onNotOff) ||
             (pSleepContext->activeTimeSecondsCereg != activeTimeSeconds) ||
             (pSleepContext->periodicWakeupSecondsCereg != periodicWakeupSeconds))) {
            // Put all the data in a struct and pass a pointer to it to our
            // local callback via the AT client's callback mechanism to decouple
            // it from whatever might have called us.
            // Note: powerSaving3gppCallback will free the allocated memory.
            pCallback = (uCellNet3gppPowerSavingCallback_t *) pUPortMalloc(sizeof(*pCallback));
            if (pCallback != NULL) {
                pCallback->cellHandle = pInstance->cellHandle;
                pCallback->pCallback = pSleepContext->p3gppPowerSavingCallback;
                pCallback->onNotOff = onNotOff;
                pCallback->activeTimeSeconds = activeTimeSeconds;
                pCallback->periodicWakeupSeconds = periodicWakeupSeconds;
                pCallback->pCallbackParam = pSleepContext->p3gppPowerSavingCallbackParam;
                uAtClientCallback(pInstance->atHandle, powerSaving3gppCallback, pCallback);
                // Set the stored parameters to the ones we just received
                pSleepContext->powerSaving3gppOnNotOffCereg = onNotOff;
                pSleepContext->activeTimeSecondsCereg = activeTimeSeconds;
                pSleepContext->periodicWakeupSecondsCereg = periodicWakeupSeconds;
            }
        }
    }
}

// Callback via which the user's base station connection
// status callback is called.  This must be called through
// the uAtClientCallback() mechanism in order to prevent
// customer code blocking the AT client.
static void connectionStatusCallback(uAtClientHandle_t atHandle,
                                     void *pParameter)
{
    uCellNetConnectionStatus_t *pStatus = (uCellNetConnectionStatus_t *) pParameter;

    (void) atHandle;

    if (pStatus != NULL) {
        if (pStatus->pCallback != NULL) {
            pStatus->pCallback(pStatus->isConnected,
                               pStatus->pCallbackParameter);
        }
        uPortFree(pStatus);
    }
}

// Base station connection URC.
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void CSCON_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    const uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    bool isConnected;
    uCellNetConnectionStatus_t *pStatus;

    (void) atHandle;

    // Read the status
    isConnected = (uAtClientReadInt(atHandle) == 1);

    if (pInstance->pConnectionStatusCallback != NULL) {
        // If the user has a callback for this, put all the
        // data in a struct and pass a pointer to it to our
        // local callback via the AT client's callback mechanism
        // to decouple it from any URC handler.
        // Note: it is up to connectionStatusCallback() to free the
        // allocate memory.
        pStatus = (uCellNetConnectionStatus_t *) pUPortMalloc(sizeof(*pStatus));
        if (pStatus != NULL) {
            pStatus->isConnected = isConnected;
            pStatus->pCallback = pInstance->pConnectionStatusCallback;
            pStatus->pCallbackParameter = pInstance->pConnectionStatusCallbackParameter;
            uAtClientCallback(atHandle, connectionStatusCallback, pStatus);
        }
    }
}

// Detect deactivation of an internal profile, which will occur if we
// fall out of service.
static void UUPSDD_urc(uAtClientHandle_t atHandle,
                       void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;

    // Skip the parameter; we don't care since we only ever
    // activate a single internal profile
    uAtClientSkipParameters(atHandle, 1);

    if (pInstance->profileState == U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_UP) {
        // Set the state so that, should we re-register with the network,
        // we will reactivate the internal profile
        pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_REQUIRES_REACTIVATION;
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: REGISTRATION RELATED
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process.
static bool keepGoingLocalCb(const uCellPrivateInstance_t *pInstance)
{
    bool keepGoing = true;

    if (pInstance->pKeepGoingCallback != NULL) {
        keepGoing = pInstance->pKeepGoingCallback(pInstance->cellHandle);
    } else {
        if ((pInstance->startTimeMs > 0) &&
            (uPortGetTickTimeMs() - pInstance->startTimeMs >
             (U_CELL_NET_CONNECT_TIMEOUT_SECONDS * 1000))) {
            keepGoing = false;
        }
    }

    return keepGoing;
}

// Turn the radio off: this done in a function of
// its own so that it can be more subtly controlled.
static int32_t radioOff(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Try three times to do this, would like to
    // get it right but sometimes modules fight back
    pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_DOWN;
    for (size_t x = 3; (x > 0) && (errorCode < 0); x--) {
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
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode < 0) {
            // If we got no response, abort the command and
            // check the status
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, " ");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientUnlock(atHandle);
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CFUN?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+CFUN:");
            if (uAtClientReadInt(atHandle) == pInstance->pModule->radioOffCfun) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }
    }

    if (errorCode == 0) {
        pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
    }

    return errorCode;
}

// Perform an abort of an AT command.
static void abortCommand(const uCellPrivateInstance_t *pInstance)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientDeviceError_t deviceError;
    bool success = false;

    // Abort is done by sending anything, we use
    // here just a space, after an AT command has
    // been sent and before the response comes
    // back.  It is, however, possible for
    // an abort to be ignored so we test for that
    // and try a few times
    for (size_t x = 3; (x > 0) && !success; x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, " ");
        uAtClientCommandStopReadResponse(atHandle);
        uAtClientDeviceErrorGet(atHandle, &deviceError);
        // Check that a device error has been signalled,
        // we've not simply timed out
        success = (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
        uAtClientUnlock(atHandle);
    }
}

// Prepare for connection with the network.
static int32_t prepareConnect(uCellPrivateInstance_t *pInstance)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    char imsi[U_CELL_INFO_IMSI_SIZE];
    size_t numRegTypes = sizeof(gRegTypes) / sizeof(gRegTypes[0]);

    uPortLog("U_CELL_NET: preparing to register/connect...\n");

    // Register the URC handlers
    uAtClientSetUrcHandler(atHandle, "+CREG:", CREG_urc, pInstance);
    uAtClientSetUrcHandler(atHandle, "+CGREG:", CGREG_urc, pInstance);
    uAtClientSetUrcHandler(atHandle, "+CEREG:", CEREG_urc, pInstance);
    uAtClientSetUrcHandler(atHandle, "+UUPSDD:", UUPSDD_urc, pInstance);

    // Switch on the unsolicited result codes for registration
    // and also ask for the additional parameters <lac>, <ci> and
    // <AcTStatus> to follow.
    if (!U_CELL_PRIVATE_SUPPORTED_RATS_LTE(pInstance->pModule->supportedRatsBitmap)) {
        // LTE not supported so one less type of registration URC
        numRegTypes--;
    }
    for (size_t x = 0; (x < numRegTypes) && (errorCode == 0); x++) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, gRegTypes[x].pSetStr);
        uAtClientWriteInt(atHandle, gRegTypes[x].type);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }
    if (errorCode == 0) {
        // We're not going to get anywhere unless a SIM
        // is inserted and this might take a while to be
        // read if we've just powered up so wait here for
        // it to be ready
        errorCode = uCellPrivateGetImsi(pInstance, imsi);
    }

    return errorCode;
}

// Set automatic network selection mode.
static int32_t setAutomaticMode(const uCellPrivateInstance_t *pInstance)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientDeviceError_t deviceError;
    int32_t errorCode;
    int32_t x;

    uPortLog("U_CELL_NET: setting automatic network"
             " selection mode...\n");

    deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR;
    // See if we are already in automatic mode
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+COPS?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+COPS:");
    x = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    if ((errorCode == 0) && (x != 0)) {
        // If we aren't, set it.  Set the
        // timeout to a second so that we can spin
        // around a loop.  Check also for an
        // ERROR response: if we get this the module
        // has actually accepted the command,
        // despite what it says
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, 1000);
        uAtClientCommandStart(atHandle, "AT+COPS=0");
        uAtClientCommandStop(atHandle);
        x = -1;
        while ((x != 0) && keepGoingLocalCb(pInstance) &&
               (deviceError.type ==
                U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR)) {
            uAtClientResponseStart(atHandle, NULL);
            x = uAtClientErrorGet(atHandle);
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            uAtClientClearError(atHandle);
            uPortTaskBlock(1000);
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        if ((x != 0) && (deviceError.type ==
                         U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR)) {
            // If we never got an answer, abort the
            // command and check the status
            abortCommand(pInstance);
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+COPS?");
            uAtClientCommandStop(atHandle);
            if (uAtClientReadInt(atHandle) == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

// Store a network scan result and return the
// number stored
static int32_t storeNextScanItem(uCellPrivateInstance_t *pInstance,
                                 char *pBuffer)
{
    int32_t count = 0;
    bool success = false;
    uCellPrivateNet_t *pNet;
    uCellPrivateNet_t **ppTmp;
    int32_t copsRat;
    size_t x;
    char *pSaved;
    char *pStr;

    // Should have:
    // (<stat>,<long_name>,<short_name>,<numeric>[,<AcT>]
    // However, there can be  gunk on the end of the AT+COPS=?
    // response string, for instance the "test" response:
    // ,(0-6),(0-2)
    // ...may appear there, so check for errors;
    // the <stat> and <numeric> fields must be present, the
    // rest could be absent or zero length strings
    // Malloc() memory to store this item
    pNet = (uCellPrivateNet_t *) pUPortMalloc(sizeof(*pNet));
    if (pNet != NULL) {
        // Check that "(<stat>" is there and throw it away
        pStr = strtok_r(pBuffer, ",", &pSaved);
        success = ((pStr != NULL) && (*pStr == '('));
        if (success) {
            success = false;
            // Grab <long_name> and put it in name
            pStr = strtok_r(NULL, ",", &pSaved);
            if (pStr != NULL) {
                x = strlen(pStr);
                pNet->name[0] = '\0';
                if (x > 1) {
                    // > 1 since "" is the minimum we can have
                    snprintf(pNet->name, sizeof(pNet->name), "%.*s",
                             x - 2, pStr + 1);
                    success = true;
                }
            }
        }
        if (success) {
            // Check if <short_name> is there but
            // don't store it
            pStr = strtok_r(NULL, ",", &pSaved);
            success = ((pStr != NULL) && (strlen(pStr) > 1));
        }
        if (success) {
            success = false;
            // Grab <numeric> and pluck the MCC/MNC from it
            pStr = strtok_r(NULL, ",", &pSaved);
            pNet->mcc = 0;
            pNet->mnc = 0;
            // +2 for the quotes at each end
            if ((pStr != NULL) && (strlen(pStr) >= 5 + 2)) {
                // +1 for the initial quotation mark
                pNet->mnc = atoi(pStr + 3 + 1);
                *(pStr + 3 + 1) = 0;
                pNet->mcc = atoi(pStr + 1);
                success = true;
            }
        }
        if (success) {
            // See if <AcT> is there
            pNet->rat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
            pStr = strtok_r(NULL, ",", &pSaved);
            if (pStr != NULL) {
                // If it is convert it into a RAT value
                copsRat = atoi(pStr);
                if ((copsRat >= 0) &&
                    (copsRat < (int32_t) (sizeof(g3gppRatToCellRat) /
                                          sizeof(g3gppRatToCellRat[0])))) {
                    pNet->rat = g3gppRatToCellRat[copsRat];
                    if ((pNet->rat == U_CELL_NET_RAT_LTE) &&
                        !(pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_LTE)) &&
                        (pInstance->pModule->supportedRatsBitmap & (1UL << (int32_t) U_CELL_NET_RAT_CATM1))) {
                        // The RAT on the end of the network status indication doesn't
                        // differentiate between LTE and Cat-M1 so, if the device doesn't
                        // support LTE but does support Cat-M1, switch it
                        pNet->rat = U_CELL_NET_RAT_CATM1;
                    }
                }
            }
        }
        pNet->pNext = NULL;
    }

    // Count the number of things already
    // in the list
    ppTmp = &(pInstance->pScanResults);
    while (*ppTmp != NULL) {
        ppTmp = &((*ppTmp)->pNext);
        count++;
    }

    if (success) {
        // Add the new entry to the end of the list
        *ppTmp = pNet;
        count++;
    } else {
        // Found gunk, just free the memory
        uPortFree(pNet);
    }

    return count;
}

// Return the next network scan result, freeing
// it from the list.
static int32_t readNextScanItem(uCellPrivateInstance_t *pInstance,
                                char *pMccMnc, char *pName,
                                size_t nameSize, uCellNetRat_t *pRat)
{
    int32_t errorCodeOrNumber = (int32_t) U_CELL_ERROR_NOT_FOUND;
    uCellPrivateNet_t *pNet = pInstance->pScanResults;
    uCellPrivateNet_t *pTmp;

    if (pNet != NULL) {
        if (pMccMnc != NULL) {
            snprintf(pMccMnc, U_CELL_NET_MCC_MNC_LENGTH_BYTES,
                     "%03d%02d", (int) pNet->mcc, (int) pNet->mnc);
        }
        if (pName != NULL) {
            snprintf(pName, nameSize, "%s", pNet->name);
        }
        if (pRat != NULL) {
            *pRat = pNet->rat;
        }
        // Now remove this entry from the list
        pTmp = pNet->pNext;
        uPortFree(pNet);
        pInstance->pScanResults = pTmp;

        // Count what's left
        errorCodeOrNumber = 0;
        while (pTmp != NULL) {
            pTmp = pTmp->pNext;
            errorCodeOrNumber++;
        }
    }

    return errorCodeOrNumber;
}

// Register with the cellular network
static int32_t registerNetwork(uCellPrivateInstance_t *pInstance,
                               const char *pMccMnc)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    bool keepGoing = true;
    bool deviceErrorDetected = false;
    int32_t regType;
    int32_t firstInt;
    int32_t status3gpp;
    uCellNetStatus_t status;
    int32_t skippedParameters = 2;
    int32_t rat = (int32_t) U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    bool gotUrc;
    size_t errorCount = 0;

    // Come out of airplane mode and try to register
    // Wait for flip time to expire first though
    while (uPortGetTickTimeMs() - pInstance->lastCfunFlipTimeMs <
           (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
        uPortTaskBlock(1000);
    }
    // Reset the current registration status
    for (size_t x = 0; x < sizeof(pInstance->networkStatus) /
         sizeof(pInstance->networkStatus[0]); x++) {
        pInstance->networkStatus[x] = U_CELL_NET_STATUS_UNKNOWN;
    }
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CFUN=1");
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    if ((errorCode == 0) && (pMccMnc != NULL)) {
        pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
        // A network was given, so automatic
        // mode is not enough.  In manual mode
        // the AT command does not return until
        // registration has been done so set the
        // timeout to a second so that we can spin
        // around a loop
        uPortLog("U_CELL_NET: registering on %s...\n", pMccMnc);
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle, 1000);
        uAtClientCommandStart(atHandle, "AT+COPS=");
        // Manual mode
        uAtClientWriteInt(atHandle, 1);
        // Numeric format
        uAtClientWriteInt(atHandle, 2);
        // The network
        uAtClientWriteString(atHandle, pMccMnc, true);
        uAtClientCommandStop(atHandle);
        // Loop until either we give up or we get a response
        while (keepGoing && keepGoingLocalCb(pInstance) && !deviceErrorDetected) {
            uAtClientResponseStart(atHandle, NULL);
            keepGoing = (uAtClientErrorGet(atHandle) < 0);
            // keepGoing will be false if we were successful
            // (uAtClientErrorGet() returned 0, which is success),
            // however it will ALSO be false if the module returned
            // ERROR or "+CME ERROR: no network service",
            // or "+CME ERROR: operation not allowed",
            // so we need to check for device errors specifically
            // and leave if one landed.
            uAtClientDeviceError_t deviceError;
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            deviceErrorDetected = (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
            uAtClientClearError(atHandle);
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        if (keepGoing && !deviceErrorDetected) {
            // Get here if there was a local abort (keepGoing
            // was till true, we were still waiting for a response)
            // and the module did not return ERROR/CME ERROR, i.e.
            // we timed out waiting for an answer: need to
            // abort the command for the module to start
            // listening to us again
            abortCommand(pInstance);
        }
        // Let the registration outcome be decided
        // by the code block below, driven by the URCs
        keepGoing = true;
    }

    if (errorCode == 0) {
        // Wait for registration to succeed
        errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
        regType = 0;
        while (keepGoing && keepGoingLocalCb(pInstance) &&
               !uCellPrivateIsRegistered(pInstance)) {
            // Prod the modem anyway, we've nout much else to do
            // We use each of the AT+CxREG? query types,
            // one at a time.
            if (gRegTypes[regType].supportedRatsBitmap &
                pInstance->pModule->supportedRatsBitmap) {
                status = U_CELL_NET_STATUS_UNKNOWN;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    pInstance->pModule->responseMaxWaitMs);
                uAtClientCommandStart(atHandle, gRegTypes[regType].pQueryStr);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, gRegTypes[regType].pResponseStr);
                // It is possible for the module to spit-out
                // a "+CxREG: y" URC while we're waiting for
                // the "+CxREG: x,y" response from the AT+CxREG
                // command. So the first integer might either by the mode
                // we set, <n>, being sent back to us or it might be the
                // <status> value of the URC.  The dodge to distinguish the
                // two is based on the fact that our values for <n> match status
                // values that mean "not registered", so we can do this:
                // (a) if the first integer matches the <n>/mode
                //     parameter from the AT+CxREG=<n>,... command, then either
                //     i)  this is the response we were expecting and
                //         the status etc. parameters follow, or,
                //     ii) this is a URC with a value indicating we are not
                //         registered and hence will not be followed
                //         by any further parameters,
                // (b) if the first integer does not match <n> then this
                //     is a URC and the first integer is the <status> value.

                gotUrc = false;
                firstInt = uAtClientReadInt(atHandle);
                status3gpp = uAtClientReadInt(atHandle);
                if ((firstInt == U_CELL_NET_CREG_OR_CGREG_TYPE) ||
                    (firstInt == U_CELL_NET_CEREG_TYPE)) {
                    // case (a.i) or (a.ii)
                    if (status3gpp < 0) {
                        // case (a.ii)
                        gotUrc = true;
                        status3gpp = firstInt;
                        uAtClientClearError(atHandle);
                    }
                } else {
                    // case (b), it's the URC
                    gotUrc = true;
                    status3gpp = firstInt;
                }
                if (gotUrc) {
                    // Read the actual response, which should follow
                    uAtClientResponseStart(atHandle,
                                           gRegTypes[regType].pResponseStr);
                    uAtClientReadInt(atHandle);
                    status3gpp = uAtClientReadInt(atHandle);
                }
                if ((status3gpp >= 0) &&
                    (status3gpp < (int32_t) (sizeof(g3gppStatusToCellStatus) /
                                             sizeof(g3gppStatusToCellStatus[0])))) {
                    status = g3gppStatusToCellStatus[status3gpp];
                }
                if (U_CELL_NET_STATUS_MEANS_REGISTERED(status)) {
                    // Skip <lac>, <ci>
                    if ((regType == 2 /* CEREG */) && (gRegTypes[regType].type == 4) &&
                        (((pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R410M_02B) ||
                          (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R412M_02B)) ||
                         ((pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6) &&
                          !gotUrc))) {
                        // SARA-R41x-02B modules, and LARA-R6 modules but only in the
                        // non-URC case, sneak an extra <rac_or_mme> parameter in when
                        // U_CELL_NET_CEREG_TYPE is 4 so we need to skip an additional
                        // parameter
                        skippedParameters++;
                    }
                    uAtClientSkipParameters(atHandle, skippedParameters);
                    // Read the RAT that we're on
                    rat = uAtClientReadInt(atHandle);
                    if ((rat < 0) && (regType == 2 /* CEREG */)) {
                        // LARA-R6 sometime misses out the RAT in the +CEREG
                        // response; we need something...
                        rat = 7; // LTE
                    }
                }
                // Set the status
                setNetworkStatus(pInstance, status, rat,
                                 gRegTypes[regType].domain,
                                 false);
                uAtClientResponseStop(atHandle);
                if (uAtClientUnlock(atHandle) != 0) {
                    // We're prodding the module pretty often
                    // while it is busy, it is possible for
                    // the responses to fall outside of the
                    // nominal responseMaxWaitMs, so
                    // allow a few errors before we give up
                    errorCount++;
                    if (errorCount > 10) {
                        keepGoing = false;
                    }
                } else {
                    uPortTaskBlock(300);
                }
            }
            // Next AT+CxREG? type
            regType++;
            if (regType >= (int32_t) (sizeof(gRegTypes) / sizeof(gRegTypes[0]))) {
                regType = 0;
            }
        }
    }

    if (uCellPrivateIsRegistered(pInstance)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Make sure we are attached to the cellular network.
static int32_t attachNetwork(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_ATTACH_FAILURE;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Wait for AT+CGATT to return 1
    for (size_t x = 10; (x > 0) && (errorCode != 0) &&
         keepGoingLocalCb(pInstance); x--) {
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle,
                            pInstance->pModule->responseMaxWaitMs);
        uAtClientCommandStart(atHandle, "AT+CGATT?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+CGATT:");
        if (uAtClientReadInt(atHandle) == 1) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);
        if (errorCode != 0) {
            uPortTaskBlock(1000);
        }
    }

    return errorCode;
}

// Disconnect from the network.
static int32_t disconnectNetwork(uCellPrivateInstance_t *pInstance,
                                 bool (pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t status3gpp;

    errorCode = radioOff(pInstance);
    if (errorCode == 0) {
        for (int32_t count = 10;
             (count > 0) && uCellPrivateIsRegistered(pInstance) &&
             ((pKeepGoingCallback == NULL) || pKeepGoingCallback(pInstance->cellHandle));
             count--) {
            for (size_t x = 0; (x < sizeof(gRegTypes) / sizeof(gRegTypes[0])) &&
                 ((pKeepGoingCallback == NULL) || pKeepGoingCallback(pInstance->cellHandle)); x++) {
                if (gRegTypes[x].supportedRatsBitmap &
                    pInstance->pModule->supportedRatsBitmap) {
                    // Prod the modem to see if it is done
                    // Use each of the AT+CxREG? query types,
                    // one at a time.
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle,
                                        pInstance->pModule->responseMaxWaitMs);
                    uAtClientCommandStart(atHandle, gRegTypes[x].pQueryStr);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, gRegTypes[x].pResponseStr);
                    // No need to worry about the URC getting
                    // in the way here, we'll just catch it
                    // next time around
                    // Ignore the first parameter
                    uAtClientReadInt(atHandle);
                    // Read the status
                    status3gpp = uAtClientReadInt(atHandle);
                    if ((status3gpp >= 0) &&
                        (status3gpp < (int32_t) (sizeof(g3gppStatusToCellStatus) /
                                                 sizeof(g3gppStatusToCellStatus[0])))) {
                        setNetworkStatus(pInstance,
                                         g3gppStatusToCellStatus[status3gpp],
                                         -1, gRegTypes[x].domain, false);
                    }
                    uAtClientResponseStop(atHandle);
                    uAtClientUnlock(atHandle);
                }
                uPortTaskBlock(300);
            }
            // There is a corner case that has occurred
            // on SARA-R412M-02B when operating on an NB1 network
            // (262 01 1nce.net) with a roaming SIM where
            // the +CEREG URC indicates that we are registered
            // even though all other indications are that we are
            // not registered.  Hence we also query the attach status
            // here and allow that to override all the others.
            uAtClientLock(atHandle);
            uAtClientTimeoutSet(atHandle,
                                pInstance->pModule->responseMaxWaitMs);
            uAtClientCommandStart(atHandle, "AT+CGATT?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+CGATT:");
            if (uAtClientReadInt(atHandle) == 0) {
                setNetworkStatus(pInstance,
                                 U_CELL_NET_STATUS_NOT_REGISTERED,
                                 -1, U_CELL_NET_REG_DOMAIN_PS, false);
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: CONTEXT ACTIVATION RELATED
 * -------------------------------------------------------------- */

// Define a PDP context
static int32_t defineContext(const uCellPrivateInstance_t *pInstance,
                             int32_t contextId, const char *pApn)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Set up context definition
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CGDCONT=");
    uAtClientWriteInt(atHandle, contextId);
    // Note that "IP" equates to IPV4 but it in no
    // way limits what the network will actually give
    // us back
    uAtClientWriteString(atHandle, "IP", true);
    if (pApn != NULL) {
        uAtClientWriteString(atHandle, pApn, true);
    } else {
        uAtClientWriteString(atHandle, "", true);
    }
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Set the authentication mode, use this if
// a username and password are given when
// in non-AT+UPSD mode.
static int32_t setAuthenticationMode(const uCellPrivateInstance_t *pInstance,
                                     int32_t contextId,
                                     const char *pUsername,
                                     const char *pPassword)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UAUTHREQ=");
    uAtClientWriteInt(atHandle, contextId);
    uAtClientWriteInt(atHandle, 3); // Automatic choice of authentication type
    if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) &&
        (pInstance->pModule->moduleType != U_CELL_MODULE_TYPE_LARA_R6)) {
        uAtClientWriteString(atHandle, pUsername, true);
        uAtClientWriteString(atHandle, pPassword, true);
    } else {
        // For SARA-R4 and LARA-R6 modules the parameters are reversed
        uAtClientWriteString(atHandle, pPassword, true);
        uAtClientWriteString(atHandle, pUsername, true);
    }
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Get the APN currently in use 3GPP commands, required
// for SARA-R4/R5/R6 and TOBY modules.
static int32_t getApnStr(const uCellPrivateInstance_t *pInstance,
                         char *pStr, size_t size)
{
    int32_t errorCodeOrSize;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    uAtClientLock(atHandle);
    // Use the AT+CGCONTRDP rather than AT+CGDCONT
    // as you can tell +CGCONTRDP which context
    // you want to know about
    uAtClientCommandStart(atHandle, "AT+CGCONTRDP=");
    uAtClientWriteInt(atHandle, U_CELL_NET_CONTEXT_ID);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CGCONTRDP:");
    // Skip echo of context ID and <bearer_id>
    uAtClientSkipParameters(atHandle, 2);
    // Read the APN
    bytesRead = uAtClientReadString(atHandle, pStr, size, false);
    // Skip <local_addr_and_subnet_mask> as it may contain
    // characters that could confuse uAtClientResponseStop()
    uAtClientSkipParameters(atHandle, 1);
    uAtClientResponseStop(atHandle);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((errorCodeOrSize == 0) && (bytesRead > 0)) {
        errorCodeOrSize = bytesRead;
    } else {
        errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
    }

    return errorCodeOrSize;
}

// Get the current APN using AT+UPSD commands, required
// for SARA-G3 and SARA-U2 modules.
static int32_t getApnStrUpsd(const uCellPrivateInstance_t *pInstance,
                             char *pStr, size_t size)
{
    int32_t errorCodeOrSize;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UPSD=");
    uAtClientWriteInt(atHandle, U_CELL_NET_PROFILE_ID);
    uAtClientWriteInt(atHandle, 1);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UPSD:");
    // Skip the echo of the profile ID and command
    uAtClientSkipParameters(atHandle, 2);
    // Read the APN
    bytesRead = uAtClientReadString(atHandle, pStr, size, false);
    uAtClientResponseStop(atHandle);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((errorCodeOrSize == 0) && (bytesRead > 0)) {
        errorCodeOrSize = bytesRead;
    } else {
        errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
    }

    return errorCodeOrSize;
}

// Activate context using 3GPP commands, required
// for SARA-R4/R5/R6 and TOBY modules.
static int32_t activateContext(const uCellPrivateInstance_t *pInstance,
                               int32_t contextId, int32_t profileId)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_CONTEXT_ACTIVATION_FAILURE;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientDeviceError_t deviceError;
    bool activated = false;
    bool ours;

    deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR;
    for (size_t x = 5; (x > 0) && keepGoingLocalCb(pInstance) &&
         (errorCode != 0) &&
         ((deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) ||
          (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_ERROR)); x--) {
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle,
                            pInstance->pModule->responseMaxWaitMs);
        uAtClientCommandStart(atHandle, "AT+CGACT?");
        uAtClientCommandStop(atHandle);
        ours = false;
        for (size_t y = 0; (y < U_CELL_NET_MAX_NUM_CONTEXTS) &&
             !ours; y++) {
            uAtClientResponseStart(atHandle, "+CGACT:");
            // Check if this is our context ID
            if (uAtClientReadInt(atHandle) == contextId) {
                ours = true;
                // If it is, 1 means activated
                activated = (uAtClientReadInt(atHandle) == 1);
            }
        }
        uAtClientResponseStop(atHandle);
        // Don't check for errors here as we will likely
        // have a timeout through waiting for a +CGACT that
        // didn't come.
        uAtClientUnlock(atHandle);
        if (activated) {
            errorCode = uCellPrivateActivateProfile(pInstance, contextId,
                                                    profileId, 5, keepGoingLocalCb);
        } else {
            uPortTaskBlock(2000);
            // Help it on its way.
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+CGACT=");
            uAtClientWriteInt(atHandle, 1);
            uAtClientWriteInt(atHandle, contextId);
            uAtClientCommandStopReadResponse(atHandle);
            // If we get back ERROR then the module wasn't
            // ready, if we get back CMS/CME error then
            // likely the network has actively rejected us,
            // e.g. due to an invalid APN
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

// Activate context using AT+UPSD commands, required
// for SARA-G3 and SARA-U2 modules.
static int32_t activateContextUpsd(const uCellPrivateInstance_t *pInstance,
                                   int32_t profileId, const char *pApn,
                                   const char *pUsername, const char *pPassword)
{
    int32_t errorCode;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientDeviceError_t deviceError;
    int32_t startTimeMs;
    bool activated = false;

    // SARA-U2 pattern: everything is done through AT+UPSD
    // Set up the APN
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UPSD=");
    uAtClientWriteInt(atHandle, profileId);
    uAtClientWriteInt(atHandle, 1);
    if ((pApn != NULL) && !uCellMnoDbProfileHas(pInstance,
                                                U_CELL_MNO_DB_FEATURE_NO_CGDCONT)) {
        uAtClientWriteString(atHandle, pApn, true);
    } else {
        uAtClientWriteString(atHandle, "", true);
    }
    uAtClientCommandStopReadResponse(atHandle);
    errorCode = uAtClientUnlock(atHandle);
    if (errorCode == 0) {
        // Set up the user name
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSD=");
        uAtClientWriteInt(atHandle, profileId);
        uAtClientWriteInt(atHandle, 2);
        if ((pUsername != NULL)  && !uCellMnoDbProfileHas(pInstance,
                                                          U_CELL_MNO_DB_FEATURE_NO_CGDCONT)) {
            uAtClientWriteString(atHandle, pUsername, true);
        } else {
            uAtClientWriteString(atHandle, "", true);
        }
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }
    if (errorCode == 0) {
        // Set up the password
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSD=");
        uAtClientWriteInt(atHandle, profileId);
        uAtClientWriteInt(atHandle, 3);
        if ((pPassword != NULL) && !uCellMnoDbProfileHas(pInstance,
                                                         U_CELL_MNO_DB_FEATURE_NO_CGDCONT)) {
            uAtClientWriteString(atHandle, pPassword, true);
        } else {
            uAtClientWriteString(atHandle, "", true);
        }
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }
    if (errorCode == 0) {
        // Set up dynamic IP address assignment
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSD=");
        uAtClientWriteInt(atHandle, profileId);
        uAtClientWriteInt(atHandle, 7);
        uAtClientWriteString(atHandle, "0.0.0.0", true);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }
    if (errorCode == 0) {
        // Automatic authentication protocol selection
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSD=");
        uAtClientWriteInt(atHandle, profileId);
        uAtClientWriteInt(atHandle, 6);
        uAtClientWriteInt(atHandle, 3);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }

    if (errorCode == 0) {
        // Wait for activation
        // We get back an OK if it succeeded,
        // else we get an ERROR or the AT client
        // will timeout.
        uAtClientLock(atHandle);
        // Set timeout to 1 second and we can spin around
        // the loop
        startTimeMs = uPortGetTickTimeMs();
        uAtClientTimeoutSet(atHandle, 1000);
        uAtClientCommandStart(atHandle, "AT+UPSDA=");
        uAtClientWriteInt(atHandle, profileId);
        uAtClientWriteInt(atHandle, 3);
        uAtClientCommandStop(atHandle);
        // Wait for something to come back
        deviceError.type = U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR;
        while (!activated && keepGoingLocalCb(pInstance) &&
               (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) &&
               (uPortGetTickTimeMs() - startTimeMs <
                (U_CELL_NET_UPSD_CONTEXT_ACTIVATION_TIME_SECONDS * 1000))) {
            uAtClientClearError(atHandle);
            uAtClientResponseStart(atHandle, NULL);
            activated = (uAtClientErrorGet(atHandle) == 0);
            if (!activated) {
                uAtClientDeviceErrorGet(atHandle, &deviceError);
                uPortTaskBlock(1000);
            }
        }
        uAtClientResponseStop(atHandle);
        uAtClientUnlock(atHandle);

        if (!activated &&
            (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR)) {
            // If we never got an answer, abort the
            // UPSDA command first.
            abortCommand(pInstance);
        }
    }

    if (!activated) {
        errorCode = (int32_t) U_CELL_ERROR_CONTEXT_ACTIVATION_FAILURE;
    }

    return errorCode;
}

// Check if a context is active using 3GPP commands, required
// for SARA-R4/R5/R6 and TOBY modules.
static bool isActive(const uCellPrivateInstance_t *pInstance,
                     int32_t contextId)
{
    bool ours = false;
    bool active = false;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t y = 0;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CGACT?");
    uAtClientCommandStop(atHandle);
    for (size_t x = 0; (x < U_CELL_NET_MAX_NUM_CONTEXTS) &&
         (y >= 0) && !ours; x++) {
        uAtClientResponseStart(atHandle, "+CGACT:");
        // Check if this is our context ID
        y = uAtClientReadInt(atHandle);
        if (y == contextId) {
            ours = true;
            // If it is, 1 means activated
            // (if it is negative we will exit)
            active = (uAtClientReadInt(atHandle) == 1);
        }
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    return active;
}

// Check if a context is active using AT+UPSD commands,
// required for SARA-G3 and SARA-U2 modules.
static bool isActiveUpsd(const uCellPrivateInstance_t *pInstance,
                         int32_t profileId)
{
    bool active = false;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UPSND=");
    uAtClientWriteInt(atHandle, profileId);
    uAtClientWriteInt(atHandle, 8);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UPSND:");
    if (uAtClientReadInt(atHandle) == profileId) {
        // Skip the echo of the command
        uAtClientSkipParameters(atHandle, 1);
        active = (uAtClientReadInt(atHandle) == 1);
    }
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);

    return active;
}

// Deactivate context using 3GPP commands, required
// for SARA-R4/R5/R6 and TOBY modules.
static int32_t deactivate(uCellPrivateInstance_t *pInstance,
                          int32_t contextId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    if (isActive(pInstance, contextId)) {
        pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_DOWN;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CGACT=");
        uAtClientWriteInt(atHandle, 0);
        uAtClientWriteInt(atHandle, U_CELL_NET_CONTEXT_ID);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }

    return errorCode;
}

// Deactivate context using AT+UPSD commands, required
// for SARA-G3 and SARA-U2 modules.
static  int32_t deactivateUpsd(uCellPrivateInstance_t *pInstance,
                               int32_t profileId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    if (isActiveUpsd(pInstance, profileId)) {
        pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_DOWN;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSDA=");
        uAtClientWriteInt(atHandle, U_CELL_NET_PROFILE_ID);
        uAtClientWriteInt(atHandle, 4);
        uAtClientCommandStopReadResponse(atHandle);
        errorCode = uAtClientUnlock(atHandle);
    }

    return errorCode;
}

// When given a new APN, check if we have an existing compatible
// PDP context and, if we don't, do something about it
// NOTE: returns 0 (success) if the current context is adequate, else error.
static int32_t handleExistingContext(uCellPrivateInstance_t *pInstance,
                                     const char *pApn,
                                     bool (pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
    bool hasContext = false;
    bool hasApn;
    char *pBuffer;
    uCellNetRat_t rat;

    // Check if we already have a PDP context.
    // Be very sure about this: it is possible for a context
    // to have _not_ _quite_ yet activated from a previous
    // API call and if we think it hasn't activated when it
    // has that will cause confusion here.
    // So if we get a negative answer, try a few times to
    // make sure it really is negative
    for (size_t x = 3; (x > 0) && !hasContext; x--) {
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
            hasContext = isActiveUpsd(pInstance, U_CELL_NET_PROFILE_ID);
        } else {
            hasContext = isActive(pInstance, U_CELL_NET_CONTEXT_ID);
        }
        if (!hasContext) {
            uPortTaskBlock(500);
        }
    }
    if (hasContext) {
        // Check if we already have the right APN
        pBuffer = (char *) pUPortMalloc(U_CELL_NET_MAX_APN_LENGTH_BYTES);
        if (pBuffer != NULL) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                hasApn = (getApnStrUpsd(pInstance, pBuffer,
                                        U_CELL_NET_MAX_APN_LENGTH_BYTES) > 0);
            } else {
                hasApn = (getApnStr(pInstance, pBuffer,
                                    U_CELL_NET_MAX_APN_LENGTH_BYTES) > 0);
            }
            if (hasApn) {
                if (pApn != NULL) {
                    // If we were given an APN check if it's the same
                    if (strcmp(pApn, pBuffer) == 0) {
                        // All good
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    // Have an active context and
                    // no APN was specified so we're good
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }

            // Free memory
            uPortFree(pBuffer);
        }

        if (errorCode != 0) {
            // If we have an inadequate PDP context,
            // need to do something about it
            rat = uCellPrivateGetActiveRat(pInstance);
            if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat) ||
                U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                // If we're on EUTRAN or we're on SARA-R4,
                // can't/don't go to the "no PDP context" state.
                // Deregistration will sort it
                disconnectNetwork(pInstance, pKeepGoingCallback);
            } else {
                // Otherwise, just deactivate the existing context
                // Ignore error codes here: whatever called this
                // function will fail anyway if this fails
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                    deactivateUpsd(pInstance, U_CELL_NET_PROFILE_ID);
                } else {
                    deactivate(pInstance, U_CELL_NET_CONTEXT_ID);
                }
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Read DNS addresses SARA-R4/R5/R6 style.
static int32_t getDnsStr(const uCellPrivateInstance_t *pInstance,
                         bool v6, char *pStrDns1, char *pStrDns2)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    char *pBuffer;
    int32_t bytesRead1[2] = {0};
    int32_t bytesRead2[2] = {0};

    // Malloc() memory for this rather than put it on
    // the stack as we read both IPV4 and IPV6 addresses
    // if available
    pBuffer = (char *) pUPortMalloc(U_CELL_NET_IP_ADDRESS_SIZE * 4);
    if (pBuffer != NULL) {
        errorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
        if (pStrDns1 != NULL) {
            *pStrDns1 = '\0';
        }
        if (pStrDns2 != NULL) {
            *pStrDns2 = '\0';
        }
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CGCONTRDP=");
        uAtClientWriteInt(atHandle, U_CELL_NET_CONTEXT_ID);
        uAtClientCommandStop(atHandle);
        // Two rows may be returned, the first
        // containing the IPV4 values and the second
        // containing the IPV6 values.
        for (size_t x = 0; (x < 2) && (errorCode < 0); x++) {
            if (x == 1) {
                // Set a short timeout for the second time
                // around as there may not be a second line
                uAtClientTimeoutSet(atHandle,
                                    pInstance->pModule->responseMaxWaitMs);
            }
            uAtClientResponseStart(atHandle, "+CGCONTRDP:");
            // Skip the echo of the context ID, <bearer_id>, <APN>,
            // <local_addr_and_subnet_mask> and <gw_addr>
            uAtClientSkipParameters(atHandle, 5);
            // Read the primary DNS address
            bytesRead1[x] = uAtClientReadString(atHandle,
                                                pBuffer + (U_CELL_NET_IP_ADDRESS_SIZE * x * 2),
                                                U_CELL_NET_IP_ADDRESS_SIZE,
                                                false);
            if (bytesRead1[x] > 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (v6) {
                    if (x == 1) {
                        if (pStrDns1 != NULL) {
                            strncpy(pStrDns1,
                                    pBuffer + (U_CELL_NET_IP_ADDRESS_SIZE * 2),
                                    U_CELL_NET_IP_ADDRESS_SIZE);
                        }
                    }
                } else {
                    if (x == 0) {
                        if (pStrDns1 != NULL) {
                            strncpy(pStrDns1, pBuffer, U_CELL_NET_IP_ADDRESS_SIZE);
                        }
                    }
                }
            }
            // Read the secondary DNS address
            bytesRead2[x] = uAtClientReadString(atHandle,
                                                pBuffer + (U_CELL_NET_IP_ADDRESS_SIZE * ((x * 2) + 1)),
                                                U_CELL_NET_IP_ADDRESS_SIZE,
                                                false);
            if (bytesRead2[x] > 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (v6) {
                    if (x == 1) {
                        if (pStrDns2 != NULL) {
                            strncpy(pStrDns2,
                                    pBuffer + (U_CELL_NET_IP_ADDRESS_SIZE * 3),
                                    U_CELL_NET_IP_ADDRESS_SIZE);
                        }
                    }
                } else {
                    if (x == 0) {
                        if (pStrDns2 != NULL) {
                            strncpy(pStrDns2,
                                    pBuffer + U_CELL_NET_IP_ADDRESS_SIZE,
                                    U_CELL_NET_IP_ADDRESS_SIZE);
                        }
                    }
                }
            }
        }
        uAtClientResponseStop(atHandle);
        // Can't check the return code here as we may have an error
        // 'cos there was only one row above
        uAtClientUnlock(atHandle);
        // Print what we got out for debug purposes
        if (errorCode == 0) {
            if (bytesRead1[0] > 0) {
                uPortLog("U_CELL_NET: primary DNS address:   \"%.*s\".\n",
                         bytesRead1[0], pBuffer);
            }
            if (bytesRead1[1] > 0) {
                uPortLog("U_CELL_NET:                        \"%.*s\".\n",
                         bytesRead1[1], pBuffer +
                         (U_CELL_NET_IP_ADDRESS_SIZE * 2));
            }
            if (bytesRead2[0] > 0) {
                uPortLog("U_CELL_NET: secondary DNS address: \"%.*s\".\n",
                         bytesRead2[0], pBuffer +
                         (U_CELL_NET_IP_ADDRESS_SIZE * 1));
            }
            if (bytesRead2[1] > 0) {
                uPortLog("U_CELL_NET:                        \"%.*s\".\n",
                         bytesRead2[1], pBuffer +
                         (U_CELL_NET_IP_ADDRESS_SIZE * 3));
            }
        } else {
            uPortLog("U_CELL_NET: unable to read DNS addresses.\n");
        }

        // Free memory
        uPortFree(pBuffer);
    }

    return errorCode;
}

// Read DNS addresses using AT+UPSND commands,
// required for SARA-U2 and SARA-G3 modules.
// Note: can't chose IPV6 or IPV4 in this
// case; you get what you're given.
static int32_t getDnsStrUpsd(const uCellPrivateInstance_t *pInstance,
                             char *pStrDns1, char *pStrDns2)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    char *pBuffer;
    int32_t bytesRead[2] = {0};

    // Malloc() memory for this as there are two possibly
    // IPV6 addresses
    pBuffer = (char *) pUPortMalloc(U_CELL_NET_IP_ADDRESS_SIZE * 2);
    if (pBuffer != NULL) {
        errorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
        if (pStrDns1 != NULL) {
            *pStrDns1 = '\0';
        }
        if (pStrDns2 != NULL) {
            *pStrDns2 = '\0';
        }
        for (size_t x = 0; (x < 2) && (errorCode < 0); x++) {
            // SARA-U2 uses AT+UPSND
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UPSND=");
            uAtClientWriteInt(atHandle, U_CELL_NET_PROFILE_ID);
            uAtClientWriteInt(atHandle, 1 + (int32_t) x);
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UPSND:");
            // Skip the echo of the profile ID and command
            uAtClientSkipParameters(atHandle, 2);
            // Read the DNS address.
            bytesRead[x] = uAtClientReadString(atHandle, pBuffer +
                                               (U_CELL_NET_IP_ADDRESS_SIZE * x),
                                               U_CELL_NET_IP_ADDRESS_SIZE,
                                               false);
            uAtClientResponseStop(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
        if (errorCode == 0) {
            if (bytesRead[0] >= 0) {
                if (pStrDns1 != NULL) {
                    strncpy(pStrDns1, pBuffer,
                            U_CELL_NET_IP_ADDRESS_SIZE);
                }
            }
            if (bytesRead[1] >= 0) {
                if (pStrDns2 != NULL) {
                    strncpy(pStrDns2,
                            pBuffer + U_CELL_NET_IP_ADDRESS_SIZE,
                            U_CELL_NET_IP_ADDRESS_SIZE);
                }
            }
            // Print what we got out for debug purposes
            if (bytesRead[0] > 0) {
                uPortLog("U_CELL_NET: primary DNS address: \"%.*s\".\n",
                         bytesRead[0], pBuffer);
            }
            if (bytesRead[1] > 0) {
                uPortLog("U_CELL_NET: secondary DNS address: \"%.*s\".\n",
                         bytesRead[1], pBuffer + U_CELL_NET_IP_ADDRESS_SIZE);
            }
        } else {
            uPortLog("U_CELL_NET: unable to read DNS addresses.\n");
        }

        // Free memory
        uPortFree(pBuffer);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Register with the cellular network and activate a PDP context.
int32_t uCellNetConnect(uDeviceHandle_t cellHandle,
                        const char *pMccMnc,
                        const char *pApn, const char *pUsername,
                        const char *pPassword,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    char buffer[15];  // At least 15 characters for the IMSI
    const char *pApnConfig = NULL;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            ((pUsername == NULL) || (pPassword != NULL))) {

            errorCode = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
            if (uCellPrivateIsRegistered(pInstance)) {
                // First deal with any existing context,
                // which might turn out to be good enough
                errorCode = handleExistingContext(pInstance, pApn,
                                                  pKeepGoingCallback);
            }

            if (errorCode != 0) {
                // Nope, no free ride, do some work
                errorCode = prepareConnect(pInstance);
                if (errorCode == 0) {
                    if ((pApn == NULL) &&
                        !uCellMnoDbProfileHas(pInstance,
                                              U_CELL_MNO_DB_FEATURE_NO_CGDCONT) &&
                        (uCellPrivateGetImsi(pInstance, buffer) == 0)) {
                        // Set up the APN look-up since none is specified
                        pApnConfig = pApnGetConfig(buffer);
                    }
                    pInstance->pKeepGoingCallback = pKeepGoingCallback;
                    pInstance->startTimeMs = uPortGetTickTimeMs();
                    // Now try to connect, potentially multiple times
                    do {
                        if (pApnConfig != NULL) {
                            pApn = _APN_GET(pApnConfig);
                            pUsername = _APN_GET(pApnConfig);
                            pPassword = _APN_GET(pApnConfig);
                            uPortLog("U_CELL_NET: APN from database is"
                                     " \"%s\".\n", pApn);
                        } else {
                            if (pApn != NULL) {
                                if (uCellMnoDbProfileHas(pInstance,
                                                         U_CELL_MNO_DB_FEATURE_IGNORE_APN)) {
                                    uPortLog("U_CELL_NET: ** WARNING ** user-specified APN"
                                             " \"%s\" will be IGNORED as the current MNO"
                                             " profile (%d) does not permit user APNs.\n",
                                             pApn, pInstance->mnoProfile);
                                    pApn = NULL;
                                } else if (uCellMnoDbProfileHas(pInstance,
                                                                U_CELL_MNO_DB_FEATURE_NO_CGDCONT)) {
                                    // An APN has been specified but the MNO profile doesn't
                                    // permit one to be set through AT+CGDCONT (or the AT+UPSD
                                    // equivalent) so flag an error
                                    uPortLog("U_CELL_NET: APN \"%s\" was specified but the"
                                             " current MNO profile (%d) does not permit an"
                                             " APN to be set.\n", pInstance->mnoProfile, pApn);
                                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                                } else {
                                    uPortLog("U_CELL_NET: user-specified APN is"
                                             " \"%s\".\n", pApn);
                                }
                            } else {
                                uPortLog("U_CELL_NET: default APN will be"
                                         " used by network.\n");
                            }
                        }
                        if ((errorCode == 0) &&
                            !U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION) &&
                            !uCellMnoDbProfileHas(pInstance,
                                                  U_CELL_MNO_DB_FEATURE_NO_CGDCONT)) {
                            // If we're not using AT+UPSD-based
                            // context activation, set the context using
                            // AT+CGDCONT and the authentication mode
                            errorCode = defineContext(pInstance,
                                                      U_CELL_NET_CONTEXT_ID,
                                                      pApn);
                            if ((errorCode == 0) && (pUsername != NULL) &&
                                (pPassword != NULL)) {
                                // Set the authentication mode
                                errorCode = setAuthenticationMode(pInstance,
                                                                  U_CELL_NET_CONTEXT_ID,
                                                                  pUsername,
                                                                  pPassword);
                            }
                        }
                        if (errorCode == 0) {
                            if (pMccMnc == NULL) {
                                // If no MCC/MNC is given, make sure we are
                                // in automatic network selection mode
                                // Don't check error code here as some
                                // modules can return an error as we still
                                // have the radio off (but they still obey)
                                setAutomaticMode(pInstance);
                            }
                            // Register
                            errorCode = registerNetwork(pInstance, pMccMnc);
                            if (errorCode == 0) {
                                // Print the network name for debug purposes
                                if (uCellPrivateGetOperatorStr(pInstance,
                                                               buffer,
                                                               sizeof(buffer)) == 0) {
                                    uPortLog("U_CELL_NET: registered on %s.\n", buffer);
                                    // This to prevent warnings if uPortLog is compiled-out
                                    (void) buffer;
                                }
                            } else {
                                uPortLog("U_CELL_NET: unable to register with"
                                         " the network");
                                if (pApn != NULL) {
                                    uPortLog(", is APN \"%s\" correct and is an"
                                             " antenna connected?\n", pApn);
                                } else {
                                    uPortLog(", does an APN need to be specified"
                                             " and is an antenna connected?\n");
                                }
                            }
                        }
                        if (errorCode == 0) {
                            // This step _shouldn't_ be necessary.  However,
                            // for reasons I don't understand, SARA-R4 can be
                            // registered but not attached (i.e. AT+CGATT
                            // returns 0) on both RATs (unh?).  Phil Ware, who
                            // knows about these things, always goes through
                            // (a) register, (b) wait for AT+CGATT to return 1
                            // and then (c) check that a context is active
                            // with AT+CGACT or using AT+UPSD (even for EUTRAN).
                            // Since this sequence works for both RANs, it is
                            // best to be consistent.
                            errorCode = attachNetwork(pInstance);
                        }
                        if (errorCode == 0) {
                            // Activate the context
                            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                   U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                                errorCode = activateContextUpsd(pInstance,
                                                                U_CELL_NET_PROFILE_ID,
                                                                pApn, pUsername,
                                                                pPassword);
                            } else {
                                errorCode = activateContext(pInstance,
                                                            U_CELL_NET_CONTEXT_ID,
                                                            U_CELL_NET_PROFILE_ID);
                            }
                            if (errorCode != 0) {
                                uPortLog("U_CELL_NET: unable to activate a PDP context");
                                if (pApn != NULL) {
                                    uPortLog(", is APN \"%s\" correct?\n", pApn);
                                } else {
                                    uPortLog(" (no APN specified/[or allowed]).\n");
                                }
                            }
                        }
                        // Exit if there are no errors or if the APN
                        // was user-specified (pApnConfig == NULL) or
                        // we're out of APN database options or the
                        // user callback has returned false
                    } while ((errorCode != 0) && (pApnConfig != NULL) &&
                             (*pApnConfig != '\0') && keepGoingLocalCb(pInstance));

                    if (errorCode == 0) {
                        // Remember the MCC/MNC in case we need to deactivate
                        // and reactivate context later and that causes
                        // de/re-registration.
                        memset(pInstance->mccMnc, 0, sizeof(pInstance->mccMnc));
                        if (pMccMnc != NULL) {
                            memcpy(pInstance->mccMnc, pMccMnc, sizeof(pInstance->mccMnc));
                        }
                        pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_UP;
                        pInstance->connectedAtMs = uPortGetTickTimeMs();
                        uPortLog("U_CELL_NET: connected after %d second(s).\n",
                                 (int32_t) ((uPortGetTickTimeMs() -
                                             pInstance->startTimeMs) / 1000));
                    } else {
                        // Switch radio off after failure
                        radioOff(pInstance);
                        uPortLog("U_CELL_NET: connection attempt stopped after"
                                 " %d second(s).\n",
                                 (int32_t) ((uPortGetTickTimeMs() -
                                             pInstance->startTimeMs) / 1000));
                    }

                    // Take away the callback again
                    pInstance->pKeepGoingCallback = NULL;
                    pInstance->startTimeMs = 0;

                }
            } else {
                uPortLog("U_CELL_NET: already connected.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Register with the cellular network.
int32_t uCellNetRegister(uDeviceHandle_t cellHandle,
                         const char *pMccMnc,
                         bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    char buffer[15];

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {

            errorCode = prepareConnect(pInstance);
            if (errorCode == 0) {
                pInstance->pKeepGoingCallback = pKeepGoingCallback;
                pInstance->startTimeMs = uPortGetTickTimeMs();
                if (pMccMnc == NULL) {
                    // If no MCC/MNC is given, make sure we are in
                    // automatic network selection mode
                    // Don't check error code here as some
                    // modules can return an error as we still
                    // have the radio off (but they still obey)
                    setAutomaticMode(pInstance);
                }
                // Register
                errorCode = registerNetwork(pInstance, pMccMnc);
                if (errorCode == 0) {
                    if (uCellPrivateGetOperatorStr(pInstance,
                                                   buffer,
                                                   sizeof(buffer)) == 0) {
                        uPortLog("U_CELL_NET: registered on %s.\n", buffer);
                    }
                } else {
                    uPortLog("U_CELL_NET: unable to register with the network.\n");
                }
                if (errorCode == 0) {
                    // This step _shouldn't_ be necessary.  However,
                    // for reasons I don't understand, SARA-R4 can
                    // be registered but not attached.
                    errorCode = attachNetwork(pInstance);
                }

                if (errorCode == 0) {
                    // Remember the MCC/MNC in case we need to deactivate
                    // and reactivate context later and that causes
                    // de/re-registration.
                    memset(pInstance->mccMnc, 0, sizeof(pInstance->mccMnc));
                    if (pMccMnc != NULL) {
                        memcpy(pInstance->mccMnc, pMccMnc, sizeof(pInstance->mccMnc));
                    }
                    uPortLog("U_CELL_NET: registered after %d second(s).\n",
                             (int32_t) ((uPortGetTickTimeMs() -
                                         pInstance->startTimeMs) / 1000));
                } else {
                    // Switch radio off after failure
                    radioOff(pInstance);
                    uPortLog("U_CELL_NET: registration attempt stopped after"
                             " %d second(s).\n",
                             (int32_t) ((uPortGetTickTimeMs() -
                                         pInstance->startTimeMs) / 1000));
                }

                // Take away the callback again
                pInstance->pKeepGoingCallback = NULL;
                pInstance->startTimeMs = 0;

            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Activate the PDP context.
int32_t uCellNetActivate(uDeviceHandle_t cellHandle,
                         const char *pApn, const char *pUsername,
                         const char *pPassword,
                         bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    const char *pMccMnc = NULL;
    char imsi[15];
    const char *pApnConfig = NULL;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            ((pUsername == NULL) || (pPassword != NULL))) {

            errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
            if (uCellPrivateIsRegistered(pInstance)) {
                // First deal with any existing context,
                // which might turn out to be good enough
                errorCode = handleExistingContext(pInstance, pApn,
                                                  pKeepGoingCallback);
                if (errorCode != 0) {
                    // No, get to work
                    pInstance->pKeepGoingCallback = pKeepGoingCallback;
                    pInstance->startTimeMs = uPortGetTickTimeMs();
                    if ((pApn == NULL) &&
                        (uCellPrivateGetImsi(pInstance, imsi) == 0)) {
                        // Set up the APN look-up since none is specified
                        pApnConfig = pApnGetConfig(imsi);
                    }
                    // Now try to activate the context, potentially multiple times
                    do {
                        if (pApnConfig != NULL) {
                            pApn = _APN_GET(pApnConfig);
                            pUsername = _APN_GET(pApnConfig);
                            pPassword = _APN_GET(pApnConfig);
                            uPortLog("U_CELL_NET: APN from database is \"%s\".\n",
                                     pApn);
                        } else {
                            if (pApn != NULL) {
                                uPortLog("U_CELL_NET: user-specified APN is"
                                         " \"%s\".\n", pApn);
                            } else {
                                uPortLog("U_CELL_NET: default APN will be used"
                                         " by network.\n");
                            }
                        }
                        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                               U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                            // Activate context AT+UPSD-wise
                            errorCode = activateContextUpsd(pInstance,
                                                            U_CELL_NET_PROFILE_ID,
                                                            pApn, pUsername,
                                                            pPassword);
                        } else {
                            // Set the context using AT+CGDCONT
                            errorCode = defineContext(pInstance,
                                                      U_CELL_NET_CONTEXT_ID,
                                                      pApn);
                            if ((errorCode == 0) && (pUsername != NULL) &&
                                (pPassword != NULL)) {
                                // Set the authentication mode
                                errorCode = setAuthenticationMode(pInstance,
                                                                  U_CELL_NET_CONTEXT_ID,
                                                                  pUsername,
                                                                  pPassword);
                            }
                            if (errorCode == 0) {
                                if (!uCellPrivateIsRegistered(pInstance)) {
                                    // The process of handling an existing context
                                    // has ended up de-registering us from
                                    // the network, so register again
                                    if (strlen(pInstance->mccMnc) > 0) {
                                        pMccMnc = pInstance->mccMnc;
                                    }
                                    errorCode = registerNetwork(pInstance, pMccMnc);
                                    if (errorCode == 0) {
                                        // This step _shouldn't_ be necessary.  However,
                                        // for reasons I don't understand, SARA-R4 can
                                        // be registered but not attached.
                                        errorCode = attachNetwork(pInstance);
                                    }
                                    if (errorCode != 0) {
                                        // Switch radio off after failure
                                        radioOff(pInstance);
                                    }
                                }
                                // Activate context
                                errorCode = activateContext(pInstance,
                                                            U_CELL_NET_CONTEXT_ID,
                                                            U_CELL_NET_PROFILE_ID);
                            }
                        }
                        // Exit if there are no errors or if the APN
                        // was user-specified (pApnConfig == NULL) or
                        // we're out of APN database options
                    } while ((errorCode != 0) && (pApnConfig != NULL) &&
                             (*pApnConfig != '\0') && keepGoingLocalCb(pInstance));

                    // Take away the callback again
                    pInstance->pKeepGoingCallback = NULL;
                    pInstance->startTimeMs = 0;
                }

                if (errorCode == 0) {
                    pInstance->profileState = U_CELL_PRIVATE_PROFILE_STATE_SHOULD_BE_UP;
                    pInstance->connectedAtMs = uPortGetTickTimeMs();
                    if (pApn != NULL) {
                        uPortLog("U_CELL_NET: activated on APN \"%s\".\n", pApn);
                    } else {
                        uPortLog("U_CELL_NET: activated.\n");
                    }
                } else {
                    uPortLog("U_CELL_NET: unable to activate a PDP context");
                    if (pApn != NULL) {
                        uPortLog(", is APN \"%s\" correct?\n", pApn);
                    } else {
                        uPortLog(" (no APN specified).\n");
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Deactivate the PDP context.
int32_t uCellNetDeactivate(uDeviceHandle_t cellHandle,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellNetRat_t rat;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (uCellPrivateIsRegistered(pInstance)) {
                rat = uCellPrivateGetActiveRat(pInstance);
                if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat) ||
                    U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                    // Can't not have a PDP context, deregister entirely
                    errorCode = disconnectNetwork(pInstance, pKeepGoingCallback);
                } else {
                    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                           U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                        // SARA-U2 style, for an internal context,
                        // with AT+UPSDA
                        errorCode = deactivateUpsd(pInstance, U_CELL_NET_PROFILE_ID);
                    } else {
                        // SARA-R4/R5/R6 style, with AT+CGACT
                        errorCode = deactivate(pInstance, U_CELL_NET_CONTEXT_ID);
                    }
                }
                if (errorCode != 0) {
                    uPortLog("U_CELL_NET: unable to deactivate context.\n");
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Disconnect from the network.
int32_t uCellNetDisconnect(uDeviceHandle_t cellHandle,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t status3gpp;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            // See if we are already disconnected
            uAtClientLock(atHandle);
            // Clear out the old RF readings
            uCellPrivateClearRadioParameters(&(pInstance->radioParameters));
            uAtClientCommandStart(atHandle, "AT+COPS?");
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+COPS:");
            status3gpp = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            errorCode = uAtClientUnlock(atHandle);
            if ((errorCode == 0) && (status3gpp != 2)) {
                errorCode = disconnectNetwork(pInstance, pKeepGoingCallback);
            }
            if (!uCellPrivateIsRegistered(pInstance)) {
                uAtClientRemoveUrcHandler(atHandle, "+CREG:");
                uAtClientRemoveUrcHandler(atHandle, "+CGREG:");
                uAtClientRemoveUrcHandler(atHandle, "+CEREG:");
                uAtClientRemoveUrcHandler(atHandle, "+UUPSDD:");
                uPortLog("U_CELL_NET: disconnected.\n");
            } else {
                uPortLog("U_CELL_NET: unable to disconnect.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Initiate a network scan and return the first result.
int32_t uCellNetScanGetFirst(uDeviceHandle_t cellHandle,
                             char *pName, size_t nameSize,
                             char *pMccMnc, uCellNetRat_t *pRat,
                             bool (*pKeepGoingCallback) (uDeviceHandle_t cellHandle))
{
    int32_t errorCodeOrNumber = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char *pBuffer;
    int32_t bytesRead;
    int32_t mode;
    int64_t innerStartTimeMs;
    uAtClientDeviceError_t deviceError;
    bool gotAnswer = false;
    char *pSaved;
    char *pStr;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrNumber = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) &&
            ((pName == NULL) || (nameSize > 0))) {
            atHandle = pInstance->atHandle;
            // Free any previous scan results
            uCellPrivateScanFree(&(pInstance->pScanResults));
            errorCodeOrNumber = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Allocate some temporary storage
            pBuffer = (char *) pUPortMalloc(U_CELL_NET_SCAN_LENGTH_BYTES);
            if (pBuffer != NULL) {
                errorCodeOrNumber = (int32_t) U_CELL_ERROR_TEMPORARY_FAILURE;
                // Ensure that we're powered up.
                mode = uCellPrivateCFunOne(pInstance);
                // Start a scan
                // Do this three times: if the module
                // is busy doing its own search when we ask it
                // to do a network search, as it might be if
                // we've just come out of airplane mode,
                // it will ignore us and simply return the
                // "test" response to the AT+COPS=? command,
                // i.e.: +COPS: ,,(0-6),(0-2)
                // If we get the "test" response instead
                // readBytes will be 12 whereas for the
                // intended response of:
                // (<stat>,<long_name>,<short_name>,<numeric>[,<AcT>])
                // it will be at longer than that hence we set
                // a threshold for readBytes of > 12 characters.
                pInstance->startTimeMs = uPortGetTickTimeMs();
                for (size_t x = U_CELL_NET_SCAN_RETRIES + 1;
                     (x > 0) && (errorCodeOrNumber <= 0) &&
                     ((pKeepGoingCallback == NULL) || (pKeepGoingCallback(cellHandle)));
                     x--) {
                    uAtClientLock(atHandle);
                    // Set the timeout to a second so that we
                    // can spin around the loop
                    gotAnswer = false;
                    uAtClientTimeoutSet(atHandle, 1000);
                    uAtClientCommandStart(atHandle, "AT+COPS=?");
                    uAtClientCommandStop(atHandle);
                    // Will get back "+COPS:" then a single line consisting of
                    // comma delimited list of
                    // (<stat>,<long_name>,<short_name>,<numeric>[,<AcT>])
                    // ...plus some other stuff on the end.
                    // Sit in a loop waiting for a response
                    // of some form to arrive
                    bytesRead = -1;
                    innerStartTimeMs = uPortGetTickTimeMs();
                    while ((bytesRead <= 0) &&
                           (uPortGetTickTimeMs() - innerStartTimeMs <
                            (U_CELL_NET_SCAN_TIME_SECONDS * 1000)) &&
                           ((pKeepGoingCallback == NULL) || (pKeepGoingCallback(cellHandle)))) {
                        uAtClientResponseStart(atHandle, "+COPS:");
                        // We use uAtClientReadBytes() here because the
                        // thing we're reading contains quotation marks
                        // but we do actually want to end up with a string,
                        // so leave room to add a terminator
                        bytesRead = uAtClientReadBytes(atHandle, pBuffer,
                                                       U_CELL_NET_SCAN_LENGTH_BYTES - 1,
                                                       false);
                        if (bytesRead >= 0) {
                            // Add a terminator
                            *(pBuffer + bytesRead) = 0;
                        }
                        // Check if an error has been returned by the module,
                        // e.g. +CME ERROR: Temporary Failure, and if
                        // so exit the while() loop and try AT+COPS=? again.
                        uAtClientDeviceErrorGet(atHandle, &deviceError);
                        if (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR) {
                            // Purely to exit the while() loop and cause us to
                            // try gain in the outer for() loop
                            bytesRead = 1;
                        }
                        uAtClientClearError(atHandle);
                        uPortTaskBlock(1000);
                    }
                    if (bytesRead > 0) {
                        // Got _something_ back, but it may still be the
                        // "test" response or a device error
                        gotAnswer = true;
                    }
                    if (bytesRead > 12) {
                        // Got a real answer: process it in
                        // chunks delimited by ")"
                        for (pStr = strtok_r(pBuffer, ")", &pSaved);
                             pStr != NULL;
                             pStr = strtok_r(NULL, ")", &pSaved)) {
                            errorCodeOrNumber = storeNextScanItem(pInstance, pStr);
                        }
                    }
                    uAtClientResponseStop(atHandle);
                    uAtClientUnlock(atHandle);
                    if (!gotAnswer) {
                        // If we never got an answer, abort the
                        // command first.
                        abortCommand(pInstance);
                    }
                }

                // Free memory
                uPortFree(pBuffer);

                // Put the mode back if it was not already 1
                if ((mode >= 0) && (mode != 1)) {
                    uCellPrivateCFunMode(pInstance, mode);
                }
                if (gotAnswer) {
                    // Return the first thing from what we stored
                    readNextScanItem(pInstance, pMccMnc, pName,
                                     nameSize, pRat);
                } else {
                    errorCodeOrNumber = (int32_t) U_ERROR_COMMON_TIMEOUT;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrNumber;
}

// Return subsequent results from a network scan.
int32_t uCellNetScanGetNext(uDeviceHandle_t cellHandle,
                            char *pName, size_t nameSize,
                            char *pMccMnc, uCellNetRat_t *pRat)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = readNextScanItem(pInstance, pMccMnc, pName,
                                         nameSize, pRat);
            if (errorCode == 0) {
                // Must have read the lot, free the scan results
                uCellPrivateScanFree(&(pInstance->pScanResults));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Clear up memory from a network scan.
void uCellNetScanGetLast(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            // Free scan results
            uCellPrivateScanFree(&(pInstance->pScanResults));
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

// Enable or disable the registration status call-back.
int32_t uCellNetSetRegistrationStatusCallback(uDeviceHandle_t cellHandle,
                                              void (*pCallback) (uCellNetRegDomain_t,
                                                                 uCellNetStatus_t,
                                                                 void *),
                                              void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            pInstance->pRegistrationStatusCallback = pCallback;
            pInstance->pRegistrationStatusCallbackParameter = pCallbackParameter;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Enable or disable the basestation connection call-back.
int32_t uCellNetSetBaseStationConnectionStatusCallback(uDeviceHandle_t cellHandle,
                                                       void (*pCallback) (bool,
                                                                          void *),
                                                       void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t value = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            atHandle = pInstance->atHandle;
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_CSCON)) {
                if (pCallback != NULL) {
                    pInstance->pConnectionStatusCallback = pCallback;
                    pInstance->pConnectionStatusCallbackParameter = pCallbackParameter;
                    uAtClientSetUrcHandler(pInstance->atHandle, "+CSCON:",
                                           CSCON_urc, pInstance);
                    value = 1;
                } else {
                    uAtClientRemoveUrcHandler(pInstance->atHandle, "+CSCON:");
                    pInstance->pConnectionStatusCallback = NULL;
                }
                // Switch the URC on or off
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+CSCON=");
                uAtClientWriteInt(atHandle, value);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the current network registration status.
uCellNetStatus_t uCellNetGetNetworkStatus(uDeviceHandle_t cellHandle,
                                          uCellNetRegDomain_t domain)
{
    int32_t errorCodeOrStatus = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrStatus = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (domain < U_CELL_NET_REG_DOMAIN_MAX_NUM)) {
            errorCodeOrStatus = (int32_t) pInstance->networkStatus[domain];
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return (uCellNetStatus_t) errorCodeOrStatus;
}

// Get a value whether the module is registered on the network.
bool uCellNetIsRegistered(uDeviceHandle_t cellHandle)
{
    bool isRegistered = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            isRegistered = uCellPrivateIsRegistered(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isRegistered;
}

// Return the RAT that is currently in use.
uCellNetRat_t uCellNetGetActiveRat(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrRat = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrRat = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrRat = (int32_t) uCellPrivateGetActiveRat(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return (uCellNetRat_t) errorCodeOrRat;
}

// Get the operator name.
int32_t uCellNetGetOperatorStr(uDeviceHandle_t cellHandle,
                               char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            errorCodeOrSize = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
            if (uCellPrivateIsRegistered(pInstance)) {
                errorCodeOrSize = uCellPrivateGetOperatorStr(pInstance,
                                                             pStr, size);
                if (errorCodeOrSize >= 0) {
                    uPortLog("U_CELL_NET: operator is \"%s\".\n", pStr);
                } else {
                    uPortLog("U_CELL_NET: unable to read operator name.\n");
                }
            } else {
                uPortLog("U_CELL_NET: unable to read operator name, not"
                         " registered with a network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Get the MCC/MNC of the network.
int32_t uCellNetGetMccMnc(uDeviceHandle_t cellHandle,
                          int32_t *pMcc, int32_t *pMnc)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char buffer[U_CELL_NET_MCC_MNC_LENGTH_BYTES];
    int32_t bytesRead;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pMcc != NULL) && (pMnc != NULL)) {
            errorCode = (int32_t) U_CELL_ERROR_NOT_REGISTERED;
            if (uCellPrivateIsRegistered(pInstance)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                // First set numeric format
                uAtClientCommandStart(atHandle, "AT+COPS=3,2");
                uAtClientCommandStopReadResponse(atHandle);
                // Then read the network name
                uAtClientCommandStart(atHandle, "AT+COPS?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+COPS:");
                // Skip past <mode> and <format>
                uAtClientSkipParameters(atHandle, 2);
                // Read the operator name, which will be
                // as MCC/MNC
                bytesRead = uAtClientReadString(atHandle, buffer,
                                                sizeof(buffer), false);
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
                if ((errorCode == 0) && (bytesRead >= 5)) {
                    // Should now have a string something like "255255"
                    // The first three digits are the MCC, the next two or
                    // three the MNC
                    *pMnc = atoi(&(buffer[3]));
                    buffer[3] = 0;
                    *pMcc = atoi(buffer);
                    uPortLog("U_CELL_NET: MCC/MNC is %u/%u.\n",
                             (uint32_t) *pMcc, (uint32_t) *pMnc);
                } else {
                    errorCode = (int32_t) U_CELL_ERROR_AT;
                    uPortLog("U_CELL_NET: unable to read MCC/MNC.\n");
                }
            } else {
                uPortLog("U_CELL_NET: unable to read MCC/MNC, not"
                         " registered with a network.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Return the IP address of the currently active connection.
int32_t uCellNetGetIpAddressStr(uDeviceHandle_t cellHandle,
                                char *pStr)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    bool active;
    char *pBuffer = NULL;
    int32_t bytesRead = -1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrSize = (int32_t) U_CELL_ERROR_NOT_CONNECTED;
            atHandle = pInstance->atHandle;
            // First check if the context is active
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                active = isActiveUpsd(pInstance, U_CELL_NET_PROFILE_ID);
            } else {
                active = isActive(pInstance, U_CELL_NET_CONTEXT_ID);
            }
            if (active) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Malloc() memory for this rather than put it on
                // the stack as IPV6 addresses can be quite big
                pBuffer = (char *) pUPortMalloc(U_CELL_NET_IP_ADDRESS_SIZE);
                if (pBuffer != NULL) {
                    // Try this a few times: I have seen
                    // "AT+CGPADDR= 1," returned on rare occasions
                    for (size_t x = 3; (x > 0) && (errorCodeOrSize <= 0); x--) {
                        *pBuffer = '\0'; // In case we read zero bytes successfully
                        uAtClientLock(atHandle);
                        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                               U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                            uAtClientCommandStart(atHandle, "AT+UPSND=");
                            uAtClientWriteInt(atHandle, U_CELL_NET_PROFILE_ID);
                            uAtClientWriteInt(atHandle, 0);
                            uAtClientCommandStop(atHandle);
                            uAtClientResponseStart(atHandle, "+UPSND:");
                            // Skip the echo of the profile ID and command
                            uAtClientSkipParameters(atHandle, 2);
                            // Read the IP address.
                            bytesRead = uAtClientReadString(atHandle, pBuffer,
                                                            U_CELL_NET_IP_ADDRESS_SIZE,
                                                            false);
                        } else {
                            uAtClientCommandStart(atHandle, "AT+CGPADDR=");
                            uAtClientWriteInt(atHandle, U_CELL_NET_CONTEXT_ID);
                            uAtClientCommandStop(atHandle);
                            uAtClientResponseStart(atHandle, "+CGPADDR:");
                            // Skip the context ID
                            uAtClientSkipParameters(atHandle, 1);
                            // Read the IP address.
                            bytesRead = uAtClientReadString(atHandle, pBuffer,
                                                            U_CELL_NET_IP_ADDRESS_SIZE,
                                                            false);
                        }
                        uAtClientResponseStop(atHandle);
                        errorCodeOrSize = uAtClientUnlock(atHandle);
                        if ((errorCodeOrSize == 0) && (bytesRead > 0)) {
                            errorCodeOrSize = bytesRead;
                            if (pStr != NULL) {
                                strncpy(pStr, pBuffer, U_CELL_NET_IP_ADDRESS_SIZE);
                            }
                            uPortLog("U_CELL_NET: IP address \"%.*s\".\n",
                                     bytesRead, pBuffer);
                        } else {
                            errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
                            uPortLog("U_CELL_NET: unable to read IP address.\n");
                            uPortTaskBlock(1000);
                        }
                    }

                    // Free memory
                    uPortFree(pBuffer);
                }
            } else {
                uPortLog("U_CELL_NET: not connected, unable to read IP address.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Return the DNS addresses.
int32_t uCellNetGetDnsStr(uDeviceHandle_t cellHandle,
                          bool v6, char *pStrDns1, char *pStrDns2)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                // Can't ask for V6 in this case,
                // we get what we're given
                errorCode = getDnsStrUpsd(pInstance, pStrDns1,
                                          pStrDns2);
            } else {
                errorCode = getDnsStr(pInstance, v6, pStrDns1,
                                      pStrDns2);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the APN currently in use.
int32_t uCellNetGetApnStr(uDeviceHandle_t cellHandle,
                          char *pStr, size_t size)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pStr != NULL) && (size > 0)) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION)) {
                errorCodeOrSize = getApnStrUpsd(pInstance, pStr, size);
            } else {
                errorCodeOrSize = getApnStr(pInstance, pStr, size);
            }
            if (errorCodeOrSize >= 0) {
                uPortLog("U_CELL_NET: APN is \"%.*s\".\n",
                         errorCodeOrSize, pStr);
            } else {
                uPortLog("U_CELL_NET: unable to read APN.\n");
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: DATA COUNTERS
 * -------------------------------------------------------------- */

// Get the current value of the transmit data counter.
int32_t uCellNetGetDataCounterTx(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    bool ours = false;
    int32_t bytesSent = 0;
    int32_t y = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
                errorCodeOrCount = (int32_t) U_CELL_ERROR_AT;
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGCNTRD");
                uAtClientCommandStop(atHandle);
                for (size_t x = 0; (x < U_CELL_NET_MAX_NUM_CONTEXTS) &&
                     (y >= 0) && !ours; x++) {
                    uAtClientResponseStart(atHandle, "+UGCNTRD:");
                    // Check if this is our context ID
                    y = uAtClientReadInt(atHandle);
                    if (y == U_CELL_NET_CONTEXT_ID) {
                        ours = true;
                        // If it is, the next byte is the sent
                        // count for this session
                        bytesSent = uAtClientReadInt(atHandle);
                    }
                }
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && ours &&
                    (bytesSent >= 0)) {
                    errorCodeOrCount = bytesSent;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrCount;
}

// Get the current value of the receive data counter.
int32_t uCellNetGetDataCounterRx(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    bool ours = false;
    int32_t bytesReceived = 0;
    int32_t y = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
                errorCodeOrCount = (int32_t) U_CELL_ERROR_AT;
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGCNTRD");
                uAtClientCommandStop(atHandle);
                for (size_t x = 0; (x < U_CELL_NET_MAX_NUM_CONTEXTS) &&
                     (y >= 0) && !ours; x++) {
                    uAtClientResponseStart(atHandle, "+UGCNTRD:");
                    // Check if this is our context ID
                    y = uAtClientReadInt(atHandle);
                    if (y == U_CELL_NET_CONTEXT_ID) {
                        ours = true;
                        // Skip the transmitted byte count
                        uAtClientSkipParameters(atHandle, 1);
                        // Get the received count for this session
                        bytesReceived = uAtClientReadInt(atHandle);
                    }
                }
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && ours &&
                    (bytesReceived >= 0)) {
                    errorCodeOrCount = bytesReceived;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrCount;
}

// Reset the transmit and receive data counters.
int32_t uCellNetResetDataCounters(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UGCNTSET=");
                uAtClientWriteInt(atHandle, U_CELL_NET_CONTEXT_ID);
                uAtClientWriteInt(atHandle, 0);
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
