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
 * @brief Implementation of the u-blox security API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_crypto.h"

#include "u_at_client.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell_net.h"     // Order is important
#include "u_cell_private.h" // here don't change it
#include "u_cell_info.h"    // For the IMEI

#include "u_cell_sec.h"
#include "u_cell_sec_c2c.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Size of the buffer to store hex versions of the various keys.
 */
#define U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES 32

#ifndef U_CELL_SEC_USECDEVINFO_RETRY
/** Number of times to retry AT+USECDEVINFO? since a module may
 * not respond if it's freshly booted.
 */
# define U_CELL_SEC_USECDEVINFO_RETRY 3
#endif

#ifndef U_CELL_SEC_USECDEVINFO_DELAY_SECONDS
/** Wait between retries of AT+USECDEVINFO?.
 */
# define U_CELL_SEC_USECDEVINFO_DELAY_SECONDS 5
#endif

// Check that U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES is big enough
// to hold the IMEI as a string
#if U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES < (U_CELL_INFO_IMEI_SIZE + 1)
# error U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES must be at least as big as U_CELL_INFO_IMEI_SIZE plus room for a null terminator.
#endif

// Check that U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES is big enough to hold
// the hex version of array of U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES.
#if U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES * 2 != U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES
# error U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES not the same size as the ASCII hex version of U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES.
#endif

// Check that U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES is big enough to hold
// the hex version of array of U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES.
#if U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES * 2 != U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES
# error U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES not the same size as the ASCII hex version of U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES.
#endif

// Check that U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES is big enough to hold
// the hex version of array of U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES.
#if U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES * 2 != U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES
# error U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES not the same size as the ASCII hex version of U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES.
#endif

// Check that U_SECURITY_PSK_MAX_LENGTH_BYTES is at least as big as U_SECURITY_PSK_ID_MAX_LENGTH_BYTES
#if U_SECURITY_PSK_MAX_LENGTH_BYTES < U_SECURITY_PSK_ID_MAX_LENGTH_BYTES
# error U_SECURITY_PSK_MAX_LENGTH_BYTES is smaller than U_SECURITY_PSK_ID_MAX_LENGTH_BYTES.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the security seal status of a cellular module.
static bool moduleIsSealed(const uCellPrivateInstance_t *pInstance)
{
    bool isSealed = false;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t moduleIsRegistered;
    int32_t deviceIsRegistered;
    int32_t deviceIsActivated = -1;

    // Try this a few times in case we've just booted
    for (size_t x = 0; (x < U_CELL_SEC_USECDEVINFO_RETRY) &&
         (deviceIsActivated < 0); x++) {
        // Sealed is when AT+USECDEVINFO
        // returns 1,1,1
        uAtClientLock(atHandle);
        uAtClientTimeoutSet(atHandle,
                            U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
        uAtClientCommandStart(atHandle, "AT+USECDEVINFO?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+USECDEVINFO:");
        moduleIsRegistered = uAtClientReadInt(atHandle);
        deviceIsRegistered = uAtClientReadInt(atHandle);
        deviceIsActivated = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            isSealed = (moduleIsRegistered == 1) &&
                       (deviceIsRegistered == 1) &&
                       (deviceIsActivated == 1);
        } else {
            // Wait between tries
            uPortTaskBlock(U_CELL_SEC_USECDEVINFO_DELAY_SECONDS * 1000);
        }
    }

    return isSealed;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

// Get whether a cellular module supports u-blox security services.
bool uCellSecIsSupported(int32_t cellHandle)
{
    bool isSupported = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            // No need to contact the module, this is something
            // we know in advance for a given module type
            isSupported = U_CELL_PRIVATE_HAS(pInstance->pModule,
                                             U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isSupported;
}

// Get the security bootstrap status of a cellular module.
bool uCellSecIsBootstrapped(int32_t cellHandle)
{
    bool isBootstrapped = false;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t moduleIsRegistered;
    int32_t deviceIsActivated = -1;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                // Bootstrapped is when AT+USECDEVINFO
                // returns 1,x,1
                atHandle = pInstance->atHandle;
                // Try this a few times in case we've just booted
                for (size_t x = 0; (x < U_CELL_SEC_USECDEVINFO_RETRY) &&
                     (deviceIsActivated < 0); x++) {
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle,
                                        U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                    uAtClientCommandStart(atHandle, "AT+USECDEVINFO?");
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+USECDEVINFO:");
                    moduleIsRegistered = uAtClientReadInt(atHandle);
                    // Skip device registration field, that's only
                    // relevant to sealing
                    uAtClientSkipParameters(atHandle, 1);
                    deviceIsActivated = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    if (uAtClientUnlock(atHandle) == 0) {
                        isBootstrapped = (moduleIsRegistered == 1) &&
                                         (deviceIsActivated == 1);
                    } else {
                        // Wait between tries
                        uPortTaskBlock(U_CELL_SEC_USECDEVINFO_DELAY_SECONDS * 1000);
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isBootstrapped;
}

// Get the cellular module's serial number (IMEI) as a string.
int32_t uCellSecGetSerialNumber(int32_t cellHandle,
                                char *pSerialNumber)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Don't lock mutex, uCellInfoGetImei() does that
    if (pSerialNumber != NULL) {
        errorCodeOrSize = uCellInfoGetImei(cellHandle, pSerialNumber);
        if (errorCodeOrSize == 0) {
            // Add terminator and set the return length
            // to what strlen() would return
            *(pSerialNumber + U_CELL_INFO_IMEI_SIZE) = 0;
            errorCodeOrSize = U_CELL_INFO_IMEI_SIZE;
        }
    }

    return errorCodeOrSize;
}

// Get the root of trust UID from the cellular module.
int32_t uCellSecGetRootOfTrustUid(int32_t cellHandle,
                                  char *pRootOfTrustUid)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t sizeOutBytes;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char buffer[(U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES * 2) + 1]; // * 2 for hex,  +1 for terminator

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (pRootOfTrustUid != NULL) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
            errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            if (pInstance != NULL) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                    // Try a few times to get the root of trust UID,
                    // can take a little while if the module has just booted
                    errorCodeOrSize = (int32_t) U_ERROR_COMMON_TEMPORARY_FAILURE;
                    for (size_t x = 3; (x > 0) && (errorCodeOrSize < 0); x--) {
                        atHandle = pInstance->atHandle;
                        uAtClientLock(atHandle);
                        uAtClientTimeoutSet(atHandle,
                                            U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                        uAtClientCommandStart(atHandle, "AT+USECROTUID");
                        uAtClientCommandStop(atHandle);
                        uAtClientResponseStart(atHandle, "+USECROTUID:");
                        sizeOutBytes = uAtClientReadString(atHandle, buffer,
                                                           sizeof(buffer),
                                                           false);
                        uAtClientResponseStop(atHandle);
                        if ((uAtClientUnlock(atHandle) == 0) &&
                            (sizeOutBytes == sizeof(buffer) - 1)) {
                            errorCodeOrSize = (int32_t) uCellPrivateHexToBin(buffer,
                                                                             sizeOutBytes,
                                                                             pRootOfTrustUid);
                        } else {
                            uPortTaskBlock(5000);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CHIP TO CHIP SECURITY
 * -------------------------------------------------------------- */

// Pair a cellular module's AT interface for chip to chip security.
int32_t uCellSecC2cPair(int32_t cellHandle,
                        const char *pTESecret,
                        char *pKey, char *pHMac)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x = -1;
    int32_t y = -1;
    char buffer[U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES + 1]; // +1 for terminator

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pTESecret != NULL) &&
            (pKey != NULL) && (pHMac != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_SECURITY_C2C)) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECC2C=");
                uAtClientWriteInt(atHandle, 0);
                uCellPrivateBinToHex(pTESecret, sizeof(buffer) / 2,
                                     buffer);
                // Add terminator since the AT write needs a string
                *(buffer + sizeof(buffer) - 1) = 0;
                uAtClientWriteString(atHandle, buffer, true);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USECC2C:");
                // Must get back a zero and then another zero indicating
                // success
                if ((uAtClientReadInt(atHandle) == 0) &&
                    (uAtClientReadInt(atHandle) == 0)) {
                    // Success: read the key
                    x = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
                    if (x == sizeof(buffer) - 1) {
                        x = (int32_t) uCellPrivateHexToBin(buffer,
                                                           sizeof(buffer) - 1,
                                                           pKey);
                    }
                    // Try to read the HMAC key, which will
                    // only be present if the module implements
                    // the V2 chip to chip scheme
                    y = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
                    if (y == sizeof(buffer) - 1) {
                        y = (int32_t) uCellPrivateHexToBin(buffer,
                                                           sizeof(buffer) - 1,
                                                           pHMac);
                    } else {
                        // Zero the HMAC key field so that we know it is
                        // empty, then we know to use the V1 scheme.
                        memset(pHMac, 0,
                               U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES);
                        uAtClientClearError(atHandle);
                    }
                }
                uAtClientResponseStop(atHandle);
                // Key has to be the right length and, if present,
                // so does the HMAC key
                if ((uAtClientUnlock(atHandle) == 0) &&
                    (x == U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES) &&
                    ((y <= 0) ||
                     (y == U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES))) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }

                // For safety, don't want keys sitting around in RAM
                uAtClientFlush(atHandle);
                memset(buffer, 0, sizeof(buffer));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Open a secure AT session.
int32_t uCellSecC2cOpen(int32_t cellHandle,
                        const char *pTESecret,
                        const char *pKey,
                        const char *pHMacKey)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char buffer[U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES + 1]; // +1 for terminator
    uCellSecC2cContext_t *pContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pTESecret != NULL) &&
            (pKey != NULL) && (pHMacKey != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_SECURITY_C2C)) {
                if (pInstance->pSecurityC2cContext == NULL) {
                    atHandle = pInstance->atHandle;
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle,
                                        U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                    uAtClientCommandStart(atHandle, "AT+USECC2C=");
                    uAtClientWriteInt(atHandle, 1);
                    uCellPrivateBinToHex(pTESecret, sizeof(buffer) / 2,
                                         buffer);
                    // Add terminator since the AT write needs a string
                    *(buffer + sizeof(buffer) - 1) = 0;
                    uAtClientWriteString(atHandle, buffer, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        // If that was successful, set up
                        // the chip to chip security context
                        pInstance->pSecurityC2cContext = malloc(sizeof(uCellSecC2cContext_t));
                        if (pInstance->pSecurityC2cContext != NULL) {
                            pContext = (uCellSecC2cContext_t *) pInstance->pSecurityC2cContext;
                            memset(pContext, 0, sizeof(uCellSecC2cContext_t));
                            pContext->pTx = (uCellSecC2cContextTx_t *) malloc(sizeof(uCellSecC2cContextTx_t));
                            if (pContext->pTx != NULL) {
                                memset(pContext->pTx, 0, sizeof(uCellSecC2cContextTx_t));
                                pContext->pRx = (uCellSecC2cContextRx_t *) malloc(sizeof(uCellSecC2cContextRx_t));
                                if (pContext->pRx != NULL) {
                                    memset(pContext->pRx, 0, sizeof(uCellSecC2cContextRx_t));
                                    // Copy the values we've been given into
                                    // the context
                                    memcpy(pContext->teSecret, pTESecret,
                                           sizeof(pContext->teSecret));
                                    memcpy(pContext->key, pKey,
                                           sizeof(pContext->key));
                                    memcpy(pContext->hmacKey, pHMacKey,
                                           sizeof(pContext->hmacKey));
                                    pContext->pTx->txInLimit = U_CELL_SEC_C2C_USER_MAX_TX_LENGTH_BYTES;
                                    // If the pHmacTag has anything other than zero
                                    // in it this must be a V2 implementation
                                    for (size_t x = sizeof(pContext->hmacKey);
                                         (x > 0) && !pContext->isV2; x--) {
                                        pContext->isV2 = (pContext->hmacKey[x] != 0);
                                    }
                                    // Hook the intercept functions into the AT handler
                                    uAtClientStreamInterceptTx(atHandle, pUCellSecC2cInterceptTx,
                                                               (void *) pContext);
                                    uAtClientStreamInterceptRx(atHandle, pUCellSecC2cInterceptRx,
                                                               (void *) pContext);
                                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                }
                            }
                        }
                    }
                    // For safety, don't want keys sitting around in RAM
                    uAtClientFlush(atHandle);
                    memset(buffer, 0, sizeof(buffer));
                } else {
                    // Nothing to do
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Close a secure AT session.
int32_t uCellSecC2cClose(int32_t cellHandle)
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
                                   U_CELL_PRIVATE_FEATURE_SECURITY_C2C)) {
                if (pInstance->pSecurityC2cContext != NULL) {
                    atHandle = pInstance->atHandle;
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle,
                                        U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                    uAtClientCommandStart(atHandle, "AT+USECC2C=");
                    uAtClientWriteInt(atHandle, 2);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        // If that was successful, remove
                        // the security context
                        uCellPrivateC2cRemoveContext(pInstance);
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    // Nothing to do
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEAL
 * -------------------------------------------------------------- */

// Request security sealing of a cellular module.
int32_t uCellSecSealSet(int32_t cellHandle,
                        const char *pDeviceProfileUid,
                        const char *pDeviceSerialNumberStr,
                        bool (*pKeepGoingCallback) (void))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pDeviceProfileUid != NULL) &&
            (pDeviceSerialNumberStr != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECDEVINFO=");
                uAtClientWriteString(atHandle, pDeviceProfileUid, true);
                uAtClientWriteString(atHandle, pDeviceSerialNumberStr, true);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
                if (errorCode == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    while ((errorCode != 0) &&
                           ((pKeepGoingCallback == NULL) ||
                            pKeepGoingCallback())) {
                        if (moduleIsSealed(pInstance)) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        } else {
                            uPortTaskBlock(1000);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the security seal status of a celluar module.
bool uCellSecIsSealed(int32_t cellHandle)
{
    bool isSealed = false;
    uCellPrivateInstance_t *pInstance;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                isSealed = moduleIsSealed(pInstance);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isSealed;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: END TO END ENCRYPTION
 * -------------------------------------------------------------- */

// Ask a cellular module to encrypt a block of data.
int32_t uCellSecE2eEncrypt(int32_t cellHandle,
                           const void *pDataIn,
                           void *pDataOut,
                           size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t sizeOutBytes;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pDataIn != NULL) {
            pInstance = pUCellPrivateGetInstance(cellHandle);
            if (pInstance != NULL) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                    if ((pDataOut == NULL) && (dataSizeBytes == 0)) {
                        // Nothing to do
                        errorCodeOrSize = 0;
                    } else {
                        atHandle = pInstance->atHandle;
                        uAtClientLock(atHandle);
                        uAtClientTimeoutSet(atHandle,
                                            U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                        uAtClientCommandStart(atHandle, "AT+USECE2EDATAENC=");
                        uAtClientWriteInt(atHandle, (int32_t) dataSizeBytes);
                        uAtClientCommandStop(atHandle);
                        // Wait for the prompt
                        if (uAtClientWaitCharacter(atHandle, '>') == 0) {
                            // Wait for it...
                            uPortTaskBlock(50);
                            // Go!
                            uAtClientWriteBytes(atHandle, (const char *) pDataIn,
                                                dataSizeBytes, true);
                            // Grab the response
                            uAtClientResponseStart(atHandle, "+USECE2EDATAENC:");
                            // dataSizeBytes is now that of the encrypted response
                            dataSizeBytes += U_SECURITY_E2E_HEADER_LENGTH_BYTES;
                            // Read the length of the response
                            sizeOutBytes = uAtClientReadInt(atHandle);
                            if (sizeOutBytes > 0) {
                                if (sizeOutBytes < (int32_t) dataSizeBytes) {
                                    dataSizeBytes = sizeOutBytes;
                                }
                                // Don't stop for anything!
                                uAtClientIgnoreStopTag(atHandle);
                                // Get the leading quote mark out of the way
                                uAtClientReadBytes(atHandle, NULL, 1, true);
                                // Now read out all the actual data
                                uAtClientReadBytes(atHandle, (char *) pDataOut,
                                                   dataSizeBytes, true);
                                if (sizeOutBytes > (int32_t) dataSizeBytes) {
                                    //...and any extra poured away to NULL
                                    uAtClientReadBytes(atHandle, NULL,
                                                       sizeOutBytes -
                                                       dataSizeBytes, true);
                                    sizeOutBytes = (int32_t) dataSizeBytes;
                                }
                            }
                            // Make sure to wait for the top tag before
                            // we finish
                            uAtClientRestoreStopTag(atHandle);
                            uAtClientResponseStop(atHandle);
                            errorCodeOrSize = uAtClientUnlock(atHandle);
                            if (errorCodeOrSize == 0) {
                                // All good
                                errorCodeOrSize = sizeOutBytes;
                            }
                        } else {
                            errorCodeOrSize = uAtClientUnlock(atHandle);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: PRE-SHARED KEY GENERATION
 * -------------------------------------------------------------- */

// Generate a PSK and accompanying PSK ID.
int32_t uCellSecPskGenerate(int32_t cellHandle,
                            size_t pskSizeBytes, char *pPsk,
                            char *pPskId)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t sizeOutPsk;
    int32_t sizeOutPskId;
    char buffer[(U_SECURITY_PSK_MAX_LENGTH_BYTES * 2) + 1]; // * 2 for hex,  +1 for terminator

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pInstance != NULL) && (pPsk != NULL) && (pPskId != NULL) &&
            ((pskSizeBytes == 16) || (pskSizeBytes == 32))) {
            errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECPSK=");
                uAtClientWriteInt(atHandle, (int32_t) pskSizeBytes);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USECPSK:");
                // Read the PSK ID
                sizeOutPskId = uAtClientReadString(atHandle, buffer,
                                                   sizeof(buffer),
                                                   false);
                if ((sizeOutPskId > 0) &&
                    (sizeOutPskId <= U_SECURITY_PSK_ID_MAX_LENGTH_BYTES * 2)) {
                    sizeOutPskId = (int32_t) uCellPrivateHexToBin(buffer,
                                                                  sizeOutPskId,
                                                                  pPskId);
                }
                // Read the PSK
                sizeOutPsk = uAtClientReadString(atHandle, buffer,
                                                 sizeof(buffer),
                                                 false);
                if (sizeOutPsk > 0) {
                    sizeOutPsk = (int32_t) uCellPrivateHexToBin(buffer,
                                                                sizeOutPsk,
                                                                pPsk);
                }
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) &&
                    (sizeOutPsk == (int32_t) pskSizeBytes)) {
                    errorCodeOrSize = sizeOutPskId;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Trigger a security heartbeat.
int32_t uCellSecHeartbeatTrigger(int32_t cellHandle)
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
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECCONN");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
