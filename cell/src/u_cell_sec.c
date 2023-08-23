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
 * @brief Implementation of the u-blox security API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // for INT_MAX
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_crypto.h"

#include "u_hex_bin_convert.h"

#include "u_at_client.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Order is important
#include "u_cell_private.h" // here don't change it
#include "u_cell_info.h"    // For the IMEI

#include "u_cell_sec.h"

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

// Read a certificate/key/authority generated/used during sealing.
static int32_t ztpGet(uDeviceHandle_t cellHandle, int32_t type,
                      char *pData, size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_SECURITY_ZTP)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECDEVCERT=");
                uAtClientWriteInt(atHandle, type);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USECDEVCERT:");
                // Skip the type that is sent back to us
                uAtClientSkipParameters(atHandle, 1);
                // Read the string that follows
                if (pData == NULL) {
                    // If the data is to be thrown away, make
                    // sure all of it is thrown away
                    dataSizeBytes = INT_MAX;
                }
                x = uAtClientReadString(atHandle, pData,
                                        // Cast in two stages to keep Lint happy
                                        (size_t)  (unsigned) dataSizeBytes,
                                        false);
                uAtClientResponseStop(atHandle);
                errorCodeOrSize = uAtClientUnlock(atHandle);
                if ((errorCodeOrSize == 0) && (x > 0)) {
                    errorCodeOrSize = x + 1; // +1 to include the terminator in the count
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uCellSecPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

// Get whether a cellular module supports u-blox security services.
bool uCellSecIsSupported(uDeviceHandle_t cellHandle)
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
bool uCellSecIsBootstrapped(uDeviceHandle_t cellHandle)
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
int32_t uCellSecGetSerialNumber(uDeviceHandle_t cellHandle,
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
int32_t uCellSecGetRootOfTrustUid(uDeviceHandle_t cellHandle,
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
                            errorCodeOrSize = (int32_t) uHexToBin(buffer,
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
 * PUBLIC FUNCTIONS: SEAL
 * -------------------------------------------------------------- */

// Request security sealing of a cellular module.
int32_t uCellSecSealSet(uDeviceHandle_t cellHandle,
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
bool uCellSecIsSealed(uDeviceHandle_t cellHandle)
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
 * FUNCTIONS: ZERO TOUCH PROVISIONING
 * -------------------------------------------------------------- */

// Read the device public certificate generated during seaing.
int32_t uCellSecZtpGetDeviceCertificate(uDeviceHandle_t cellHandle,
                                        char *pData,
                                        size_t dataSizeBytes)
{
    return ztpGet(cellHandle, 1, pData, dataSizeBytes);
}

// Read the device private key generated during sealing.
int32_t uCellSecZtpGetPrivateKey(uDeviceHandle_t cellHandle,
                                 char *pData,
                                 size_t dataSizeBytes)
{
    return ztpGet(cellHandle, 0, pData, dataSizeBytes);
}

// Read the certificate authorities used during sealing.
int32_t uCellSecZtpGetCertificateAuthorities(uDeviceHandle_t cellHandle,
                                             char *pData,
                                             size_t dataSizeBytes)
{
    return ztpGet(cellHandle, 2, pData, dataSizeBytes);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: PRE-SHARED KEY GENERATION
 * -------------------------------------------------------------- */

// Generate a PSK and accompanying PSK ID.
int32_t uCellSecPskGenerate(uDeviceHandle_t cellHandle,
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
                    sizeOutPskId = (int32_t) uHexToBin(buffer,
                                                       sizeOutPskId,
                                                       pPskId);
                }
                // Read the PSK
                sizeOutPsk = uAtClientReadString(atHandle, buffer,
                                                 sizeof(buffer),
                                                 false);
                if (sizeOutPsk > 0) {
                    sizeOutPsk = (int32_t) uHexToBin(buffer,
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
int32_t uCellSecHeartbeatTrigger(uDeviceHandle_t cellHandle)
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
