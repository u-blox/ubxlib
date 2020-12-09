/*
 * Copyright 2020 u-blox Ltd
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
 * @brief Implementation of functions that are private to cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "ctype.h"     // isdigit()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"     // Required by u_cell_private.h
#include "u_port_crypto.h"

#include "u_at_client.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_sec_c2c.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES THAT ARE SHARED THROUGHOUT THE CELLULAR IMPLEMENTATION
 * -------------------------------------------------------------- */

/** Root for the linked list of instances.
 */
uCellPrivateInstance_t *gpUCellPrivateInstanceList = NULL;

/** Mutex to protect the linked list.
 */
uPortMutexHandle_t gUCellPrivateMutex = NULL;

/** The characteristics of the modules supported by this driver,
 * compiled into the driver.
 */
const uCellPrivateModule_t gUCellPrivateModuleList[] = {
    {
        U_CELL_MODULE_TYPE_SARA_U201, 1 /* Pwr On pull ms */, 1500 /* Pwr off pull ms */,
        5 /* Boot wait */, 5 /* Min awake */, 5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        50 /* Cmd wait ms */, 2000 /* Resp max wait ms */, 0 /* radioOffCfun */, 2 /* Simultaneous RATs */,
        ((1UL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1UL << (int32_t) U_CELL_NET_RAT_UTRAN)) /* RATs */,
        (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION) /* features */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R410M_02B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 2 /* Simultaneous RATs */,
        ((1UL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1UL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1UL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE) |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)) /* features */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R412M_02B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        5 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 10 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 3 /* Simultaneous RATs */,
        ((1UL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1UL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1UL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1UL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE) |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON) |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)) /* features */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R412M_03B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 2000 /* Resp max wait ms */, 4 /* radioOffCfun */, 3 /* Simultaneous RATs */,
        ((1UL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1UL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1UL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1UL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE) |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)) /* features */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R5, 1500 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        5 /* Boot wait */, 10 /* Min awake */, 20 /* Pwr down wait */, 15 /* Reboot wait */, 10 /* AT timeout */,
        20 /* Cmd wait ms */, 5000 /* Resp max wait ms */, 0 /* radioOffCfun */, 1 /* Simultaneous RATs */,
        (1UL << (int32_t) U_CELL_NET_RAT_CATM1) /* RATs */,
        ((1UL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)   |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)         |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST) |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_C2C)  |
         (1UL << (int32_t) U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)) /* features */
    }
};

/** Number of items in the gUCellPrivateModuleList array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUCellPrivateModuleListSize = sizeof(gUCellPrivateModuleList) /
                                           sizeof(gUCellPrivateModuleList[0]);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** For binary/hex conversion.
 */
static const char gHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
                           };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO CELLULAR
 * -------------------------------------------------------------- */

// Return true if the given buffer contains only numeric characters
// (i.e. 0 to 9)
bool uCellPrivateIsNumeric(const char *pBuffer, size_t bufferSize)
{
    bool numeric = true;

    for (size_t x = 0; (x < bufferSize) && numeric; x++) {
        numeric = (isdigit((int32_t) * (pBuffer + x)) != 0);
    }

    return numeric;
}

// Find a cellular instance in the list by instance handle.
uCellPrivateInstance_t *pUCellPrivateGetInstance(int32_t handle)
{
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->handle != handle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Set the radio parameters back to defaults.
void uCellPrivateClearRadioParameters(uCellPrivateRadioParameters_t *pParameters)
{
    pParameters->rssiDbm = 0;
    pParameters->rsrpDbm = 0;
    pParameters->rsrqDb = 0;
    pParameters->cellId = -1;
    pParameters->earfcn = -1;
}

// Clear the dynamic parameters of an instance,
// so the network status, the active RAT and
// the
void uCellPrivateClearDynamicParameters(uCellPrivateInstance_t *pInstance)
{
    for (size_t x = 0;
         x < sizeof(pInstance->networkStatus) / sizeof(pInstance->networkStatus[0]);
         x++) {
        pInstance->networkStatus[x] = U_CELL_NET_STATUS_UNKNOWN;
    }
    for (size_t x = 0;
         x < sizeof(pInstance->rat) / sizeof(pInstance->rat[0]);
         x++) {
        pInstance->rat[x] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }
    uCellPrivateClearRadioParameters(&(pInstance->radioParameters));
}

// Ensure that a module is powered up.
int32_t  uCellPrivateCFunOne(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCodeOrMode;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CFUN?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CFUN:");
    errorCodeOrMode = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    // Set powered-up mode if it wasn't already
    if (errorCodeOrMode != 1) {
        // Wait for flip time to expire
        while (uPortGetTickTimeMs() < pInstance->lastCfunFlipTimeMs +
               (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
            uPortTaskBlock(1000);
        }
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CFUN=1");
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
            // And don't do anything for a second,
            // as the module might not be quite ready yet
            uPortTaskBlock(1000);
        }
    }

    return errorCodeOrMode;
}

// Do the opposite of uCellPrivateCFunOne(), put the mode back.
void uCellPrivateCFunMode(uCellPrivateInstance_t *pInstance,
                          int32_t mode)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Wait for flip time to expire
    while (uPortGetTickTimeMs() < pInstance->lastCfunFlipTimeMs +
           (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
        uPortTaskBlock(1000);
    }
    uAtClientLock(atHandle);
    if (mode != 1) {
        // If we're doing anything other than powering up,
        // i.e. AT+CFUN=0 or AT+CFUN=4, this can take
        // longer than your average response time
        uAtClientTimeoutSet(atHandle,
                            U_CELL_PRIVATE_AT_CFUN_OFF_RESPONSE_TIME_SECONDS * 1000);
    }
    uAtClientCommandStart(atHandle, "AT+CFUN=");
    uAtClientWriteInt(atHandle, mode);
    uAtClientCommandStopReadResponse(atHandle);
    if (uAtClientUnlock(atHandle) == 0) {
        pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
    }
}

// Get the IMSI of the SIM.
int32_t uCellPrivateGetImsi(const uCellPrivateInstance_t *pInstance,
                            char *pImsi)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    // Try this ten times: unfortunately
    // the module can spit out a URC just when
    // we're expecting the IMSI and, since there
    // is no prefix on the response, we have
    // no way of telling the difference.  Hence
    // check the length and that length being
    // made up entirely of numerals
    for (size_t x = 10; (x > 0) && (errorCode != 0); x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CIMI");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, NULL);
        bytesRead = uAtClientReadBytes(atHandle, pImsi,
                                       15, false);
        uAtClientResponseStop(atHandle);
        if ((uAtClientUnlock(atHandle) == 0) &&
            (bytesRead == 15) &&
            uCellPrivateIsNumeric(pImsi, 15)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Get whether the given instance is registered with the network.
// Needs to be in the packet switched domain, circuit switched is
// no use for this API.
bool uCellPrivateIsRegistered(const uCellPrivateInstance_t *pInstance)
{
    return U_CELL_PRIVATE_STATUS_MEANS_REGISTERED(pInstance->networkStatus[U_CELL_NET_REG_DOMAIN_PS]);
}

// Get the active RAT.
// Uses the packet switched domain, circuit switched is no use
// for this API.
uCellNetRat_t uCellPrivateGetActiveRat(const uCellPrivateInstance_t *pInstance)
{
    // The active RAT is the RAT for the packet switched
    // domain, the circuit switched domain is not relevant
    // to this API
    return pInstance->rat[U_CELL_NET_REG_DOMAIN_PS];
}

// Get the operator name.
int32_t uCellPrivateGetOperatorStr(const uCellPrivateInstance_t *pInstance,
                                   char *pStr, size_t size)
{
    int32_t errorCodeOrSize;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    uAtClientLock(atHandle);
    // First set long alphanumeric format
    uAtClientCommandStart(atHandle, "AT+COPS=3,0");
    uAtClientCommandStopReadResponse(atHandle);
    // Then read the operator name
    uAtClientCommandStart(atHandle, "AT+COPS?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+COPS:");
    // Skip past <mode> and <format>
    uAtClientSkipParameters(atHandle, 2);
    // Read the operator name
    bytesRead = uAtClientReadString(atHandle, pStr, size, false);
    uAtClientResponseStop(atHandle);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((errorCodeOrSize == 0) && (bytesRead >= 0)) {
        errorCodeOrSize = bytesRead;
    }

    return errorCodeOrSize;
}

// Free network scan results.
void uCellPrivateScanFree(uCellPrivateNet_t **ppScanResults)
{
    uCellPrivateNet_t *pTmp;

    while (*ppScanResults != NULL) {
        pTmp = (*ppScanResults)->pNext;
        free(*ppScanResults);
        *ppScanResults = pTmp;
    }

    *ppScanResults = NULL;
}

// Get the module characteristics for a given instance.
const uCellPrivateModule_t *pUCellPrivateGetModule(int32_t handle)
{
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;
    const uCellPrivateModule_t *pModule = NULL;

    while ((pInstance != NULL) && (pInstance->handle != handle)) {
        pInstance = pInstance->pNext;
    }

    if (pInstance != NULL) {
        pModule = pInstance->pModule;
    }

    return pModule;
}

// Convert a buffer into the ASCII hex equivalent.
size_t uCellPrivateBinToHex(const char *pBin, size_t binLength,
                            char *pHex)
{
    for (size_t x = 0; x < binLength; x++) {
        *pHex = gHex[((unsigned char) * pBin) >> 4];
        pHex++;
        *pHex = gHex[*pBin & 0x0f];
        pHex++;
        pBin++;
    }

    return binLength * 2;
}

// Convert a buffer of ASCII hex into the binary equivalent.
size_t uCellPrivateHexToBin(const char *pHex, size_t hexLength,
                            char *pBin)
{
    bool success = true;
    size_t length;
    char z[2];

    for (length = 0; (length < hexLength / 2) && success; length++) {
        z[0] = *pHex - '0';
        pHex++;
        z[1] = *pHex - '0';
        pHex++;
        for (size_t y = 0; (y < sizeof(z)) && success; y++) {
            if (z[y] > 9) {
                // Must be A to F or a to f
                z[y] -= 'A' - '0';
                z[y] += 10;
            }
            if (z[y] > 15) {
                // Must be a to f
                z[y] -= 'a' - 'A';
            }
            // Cast here to shut-up a warning under ESP-IDF
            // which appears to have chars as unsigned and
            // hence thinks the first condition is always true
            success = ((signed char) z[y] >= 0) && (z[y] <= 15);
        }
        if (success) {
            *pBin = (char) (((z[0] & 0x0f) << 4) | z[1]);
            pBin++;
        }
    }

    return length;
}

// Remove the chip to chip security context for the given instance.
void uCellPrivateC2cRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellSecC2cContext_t *pContext = (uCellSecC2cContext_t *) pInstance->pSecurityC2cContext;

    if (pContext != NULL) {
        if (pContext->pTx != NULL) {
            uAtClientStreamInterceptTx(pInstance->atHandle,
                                       NULL, NULL);
            // For safety
            memset(pContext->pTx, 0, sizeof(*(pContext->pTx)));
            free(pContext->pTx);
        }
        if (pContext->pRx != NULL) {
            uAtClientStreamInterceptRx(pInstance->atHandle,
                                       NULL, NULL);
            // For safety
            memset(pContext->pRx, 0, sizeof(*(pContext->pRx)));
            free(pContext->pRx);
        }
        // For safety
        memset(pContext, 0, sizeof(*pContext));
        free(pContext);
        pInstance->pSecurityC2cContext = NULL;
    }
}

// End of file
