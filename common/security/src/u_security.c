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
 * @brief Implementation of the u-blox security API.
 *
 * This implementation expects to call on underlying APIs
 * for the functions listed below, where "Xxx" could be Ble
 * or Cell or Wifi
 *
 * In all cases the value of handle will be taken from the
 * appropriate range in u_network_handle.h. An error from
 * BLE/Wifi/cell must be indicated by a returning a
 * negative error value; zero means success and a positive
 * number may be used to indicate a length. See the function
 * definitions in u_security.h for the meanings of the
 * parameters and return values; parameters will be error
 * checked before these functions are called.
 *
 * Get whether a module supports u-blox security services or
 * not (mandatory):
 *
 * bool uXxxSecIsSupported(int32_t handle);
 *
 * Get whether a module is bootstrapped with u-blox security
 * services or not (mandatory):
 *
 * bool uXxxSecIsBootstrapped(int32_t handle);
 *
 * Get the module serial number string (optional)
 *
 * int32_t uXxxSecGetSerialNumber(int32_t handle,
 *                                char *pSerialNumber);
 *
 * Get the root of trust UID from the module (mandatory):
 *
 * int32_t uXxxSecGetRootOfTrustUid(int32_t handle,
 *                                  char *pRootOfTrustUid);
 *
 * Pair with a module for chip to chip security (optional):
 *
 * int32_t uXxxSecC2cPair(int32_t handle, const char * pTESecret,
 *                        char * pKey, char * pHMac);
 *
 * Open a chip to chip secure session (mandatory if
 * uXxxSecC2cPair() is implemented):
 *
 * int32_t uXxxSecC2cOpen(int32_t handle,
 *                        const char *pTESecret,
 *                        const char *pKey,
 *                        const char *pHMac);
 *
 * Close a chip to chip secure session (mandatory if
 * uXxxSecC2cPair() is implemented):
 *
 * int32_t uXxxSecC2cClose(int32_t handle);
 *
 * Security seal a module (mandatory):
 *
 * int32_t uXxxSecSealSet(int32_t handle,
 *                        const char *pDeviceProfileUid,
 *                        const char *pDeviceSerialNumberStr,
 *                        bool (*pKeepGoingCallback) (void));
 *
 * Get whether the module is security sealed or not (mandatory):
 *
 * bool uXxxSecIsSealed(int32_t handle);
 *
 * Read the device public certificate generated during sealing (optional):
 *
 * int32_t uXxxSecZtpGetDeviceCertificate(int32_t handle,
 *                                        char *pData,
 *                                        size_t dataSizeBytes);
 *
 * Read the device private key generated during sealing (optional):
 *
 * int32_t uXxxSecZtpGetPrivateKey(int32_t handle,
 *                                 char *pData,
 *                                 size_t dataSizeBytes);
 *
 * Read the certificate authorities used during sealing (optional):
 *
 * int32_t uXxxSecZtpGetCertificateAuthorities(int32_t handle,
 *                                             char *pData,
 *                                             size_t dataSizeBytes);
 *
 * Perform end to end encryption on a block of data (optional):
 *
 * int32_t uXxxSecE2eEncrypt(int32_t handle,
 *                           const void *pDataIn,
 *                           void *pDataOut,
 *                           size_t dataSizeBytes);
 *
 * Set the end to end encryption version in use (optional):
 *
 * int32_t uXxxSecE2eSetVersion(int32_t handle,
 *                              int32_t version);
 *
 * Get the end to end encryption version in use (mandatory
 * if uXxxSecE2eSetVersion() is supported):
 *
 * int32_t uXxxSecE2eGetVersion(int32_t handle);
 *
 * Trigger a security heartbeat (optional):
 *
 * int32_t uXxxSecHeartbeatTrigger(int32_t handle);
 *
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_cell_sec.h"

#include "u_security.h"
#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

// Get whether a module supports u-blox security services or not.
bool uSecurityIsSupported(uDeviceHandle_t devHandle)
{
    bool isSupported = false;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        isSupported = uCellSecIsSupported(devHandle);
    } else if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
        // Not implemented yet
        isSupported = false;
    }

    return isSupported;
}

// Get the security bootstrap status of a module.
bool uSecurityIsBootstrapped(uDeviceHandle_t devHandle)
{
    bool isBootstrapped = false;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        isBootstrapped = uCellSecIsBootstrapped(devHandle);
    }

    return isBootstrapped;
}

// Get the module serial number string.
int32_t uSecurityGetSerialNumber(uDeviceHandle_t devHandle,
                                 char *pSerialNumber)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (pSerialNumber != NULL) {
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
        if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
            errorCodeOrSize = uCellSecGetSerialNumber(devHandle,
                                                      pSerialNumber);
        } else if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_SHORT_RANGE)) {
            errorCodeOrSize = uShortRangeGetSerialNumber(devHandle, pSerialNumber);
        }
    }

    return errorCodeOrSize;
}

// Get the root of trust UID from the module.
int32_t uSecurityGetRootOfTrustUid(uDeviceHandle_t devHandle,
                                   char *pRootOfTrustUid)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
    char buffer[U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES];

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCodeOrSize = uCellSecGetRootOfTrustUid(devHandle,
                                                    buffer);
        if ((errorCodeOrSize > 0) && (pRootOfTrustUid != NULL)) {
            memcpy(pRootOfTrustUid, buffer, sizeof(buffer));
        }
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CHIP TO CHIP SECURITY
 * -------------------------------------------------------------- */

// Pair a module's AT interface for chip to chip security.
int32_t uSecurityC2cPair(uDeviceHandle_t devHandle,
                         const char *pTESecret,
                         char *pKey, char *pHMac)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pTESecret != NULL) && (pKey != NULL) && (pHMac != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
        if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellSecC2cPair(devHandle,
                                        pTESecret, pKey, pHMac);
        }
    }

    return errorCode;
}

// Open a secure AT session.
int32_t uSecurityC2cOpen(uDeviceHandle_t devHandle,
                         const char *pTESecret,
                         const char *pKey,
                         const char *pHMac)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pTESecret != NULL) && (pKey != NULL) && (pHMac != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
        if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellSecC2cOpen(devHandle,
                                        pTESecret, pKey, pHMac);
        }
    }

    return errorCode;
}

// Close a secure AT session.
int32_t uSecurityC2cClose(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCode = uCellSecC2cClose(devHandle);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEAL
 * -------------------------------------------------------------- */

// Request security sealing of a module.
int32_t uSecuritySealSet(uDeviceHandle_t devHandle,
                         const char *pDeviceProfileUid,
                         const char *pDeviceSerialNumberStr,
                         bool (*pKeepGoingCallback) (void))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pDeviceProfileUid != NULL) &&
        (pDeviceSerialNumberStr != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
        if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
            errorCode = uCellSecSealSet(devHandle,
                                        pDeviceProfileUid,
                                        pDeviceSerialNumberStr,
                                        pKeepGoingCallback);
        }
    }

    return errorCode;
}

// Get the security seal status of a module.
bool uSecurityIsSealed(uDeviceHandle_t devHandle)
{
    bool isSealed = false;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        isSealed = uCellSecIsSealed(devHandle);
    }

    return isSealed;
}

/* ----------------------------------------------------------------
 * FUNCTIONS: ZERO TOUCH PROVISIONING
 * -------------------------------------------------------------- */

// Read the device public certificate generated during sealing.
int32_t uSecurityZtpGetDeviceCertificate(uDeviceHandle_t devHandle,
                                         char *pData,
                                         size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCodeOrSize = uCellSecZtpGetDeviceCertificate(devHandle,
                                                          pData,
                                                          dataSizeBytes);
    }

    return errorCodeOrSize;
}

// Read the device private key generated during sealing.
int32_t uSecurityZtpGetPrivateKey(uDeviceHandle_t devHandle,
                                  char *pData,
                                  size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCodeOrSize = uCellSecZtpGetPrivateKey(devHandle,
                                                   pData,
                                                   dataSizeBytes);
    }

    return errorCodeOrSize;
}

// Read the certificate authorities used during sealing.
int32_t uSecurityZtpGetCertificateAuthorities(uDeviceHandle_t devHandle,
                                              char *pData,
                                              size_t dataSizeBytes)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCodeOrSize = uCellSecZtpGetCertificateAuthorities(devHandle,
                                                               pData,
                                                               dataSizeBytes);
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: END TO END ENCRYPTION
 * -------------------------------------------------------------- */

// Set the E2E encryption version to be used.
int32_t uSecurityE2eSetVersion(uDeviceHandle_t devHandle, int32_t version)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCode = uCellSecE2eSetVersion(devHandle,
                                          version);
    }

    return errorCode;
}

// Get the E2E encryption version.
int32_t uSecurityE2eGetVersion(uDeviceHandle_t devHandle)
{
    int32_t errorCodeOrVersion = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCodeOrVersion = uCellSecE2eGetVersion(devHandle);
    }

    return errorCodeOrVersion;
}

// Ask a module to encrypt a block of data.
int32_t uSecurityE2eEncrypt(uDeviceHandle_t devHandle,
                            const void *pDataIn,
                            void *pDataOut, size_t dataSizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (pDataIn != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pDataOut != NULL) && (dataSizeBytes > 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
            if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
                errorCode = uCellSecE2eEncrypt(devHandle,
                                               pDataIn, pDataOut,
                                               dataSizeBytes);
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: PRE-SHARED KEY GENERATION
 * -------------------------------------------------------------- */

// Generate a PSK and accompanying PSK ID.
int32_t uSecurityPskGenerate(uDeviceHandle_t devHandle,
                             size_t pskSizeBytes, char *pPsk,
                             char *pPskId)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if ((pPsk != NULL) && (pPskId != NULL) &&
        ((pskSizeBytes == 16) || (pskSizeBytes == 32))) {
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
        if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
            errorCodeOrSize = uCellSecPskGenerate(devHandle,
                                                  pskSizeBytes, pPsk,
                                                  pPskId);
        }
    }

    return errorCodeOrSize;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Trigger a security heartbeat.
int32_t uSecurityHeartbeatTrigger(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;

    if (U_DEVICE_IS_TYPE(devHandle, U_DEVICE_TYPE_CELL)) {
        errorCode = uCellSecHeartbeatTrigger(devHandle);
    }

    return errorCode;
}

// End of file
