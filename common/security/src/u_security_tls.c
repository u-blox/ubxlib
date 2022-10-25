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
 * @brief Implementation of the comon u-blox TLS API; this API is
 * thread-safe.  It calls into the underlying cellular or
 * short_range security APIs to perform the actual security
 * configuration.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen()

#include "u_error_common.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_device_shared.h"

#include "u_security_credential.h"
#include "u_security_tls.h"

#include "u_cell_sec_tls.h"
#include "u_short_range_sec_tls.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex for re-entrancy protection.
 */
static uPortMutexHandle_t gMutex = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise TLS security.
static int32_t init()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

// Check that a given security configuration contains no errors.
static bool checkConfig(const uSecurityTlsSettings_t *pSettings)
{
    bool isGood = false;

    if (((pSettings->tlsVersionMin >= U_SECURITY_TLS_VERSION_ANY) &&
         (pSettings->tlsVersionMin < U_SECURITY_TLS_VERSION_MAX_NUM)) &&
        ((pSettings->pRootCaCertificateName == NULL) ||
         (strlen(pSettings->pRootCaCertificateName) <=
          U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES)) &&
        ((pSettings->pClientCertificateName == NULL) ||
         (strlen(pSettings->pClientCertificateName) <=
          U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES)) &&
        ((pSettings->pClientPrivateKeyName == NULL) ||
         (strlen(pSettings->pClientPrivateKeyName) <=
          U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES)) &&
        (pSettings->cipherSuites.num < U_SECURITY_TLS_MAX_NUM_CIPHER_SUITES) &&
        (((pSettings->psk.pBin == NULL) && (pSettings->pskId.pBin == NULL) &&
          (pSettings->psk.size == 0) && (pSettings->pskId.size == 0)) ||
         (((pSettings->psk.size <= U_SECURITY_TLS_PSK_MAX_LENGTH_BYTES)) &&
          (pSettings->pskId.size <= U_SECURITY_TLS_PSK_ID_MAX_LENGTH_BYTES))) &&
        //lint -e(568) Suppress warning that this can't be negative:
        // it can be if people are careless so better to check it
        ((int32_t) pSettings->certificateCheck >= 0) &&
        (pSettings->certificateCheck < U_SECURITY_TLS_CERTIFICATE_CHECK_MAX_NUM) &&
        ((pSettings->pExpectedServerUrl == NULL) ||
         (strlen(pSettings->pExpectedServerUrl) <=
          U_SECURITY_TLS_EXPECTED_SERVER_URL_MAX_LENGTH_BYTES)) &&
        ((pSettings->pSni == NULL) || (strlen(pSettings->pSni) <=
                                       U_SECURITY_TLS_SNI_MAX_LENGTH_BYTES)) &&
        !pSettings->enableSessionResumption) {
        isGood = true;
    }

    return isGood;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set up a TLS security context.
uSecurityTlsContext_t *pUSecurityTlsAdd(uDeviceHandle_t devHandle,
                                        const uSecurityTlsSettings_t *pSettings)
{
    int32_t errorCode = init();
    uSecurityTlsContext_t *pContext = (uSecurityTlsContext_t *) pUPortMalloc(sizeof(*pContext));
    void *pNetworkSpecific = NULL;
    const char *pRootCaCertificateName = NULL;
    const char *pClientCertificateName = NULL;
    const char *pClientPrivateKeyName = NULL;
    bool certificateCheckOn = false;
    uSecurityTlsVersion_t tlsVersionMin = U_SECURITY_TLS_VERSION_ANY;

    if ((errorCode == 0) && (pContext != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        memset(pContext, 0, sizeof(*pContext));

        U_PORT_MUTEX_LOCK(gMutex);

        if ((pSettings == NULL) || checkConfig(pSettings)) {
            int32_t devType = uDeviceGetDeviceType(devHandle);
            errorCode = (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
            if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                // Short range TLS security context setup
                if (pSettings != NULL) {
                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                    // Only CA checking (not the URL and date versions)
                    // are supported for short range
                    if (pSettings->certificateCheck <= U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        pRootCaCertificateName = pSettings->pRootCaCertificateName;
                        pClientCertificateName = pSettings->pClientCertificateName;
                        pClientPrivateKeyName = pSettings->pClientPrivateKeyName;
                        certificateCheckOn = pSettings->certificateCheck == U_SECURITY_TLS_CERTIFICATE_CHECK_ROOT_CA;
                        tlsVersionMin = pSettings->tlsVersionMin;
                        if (tlsVersionMin < U_SECURITY_TLS_VERSION_1_0) {
                            tlsVersionMin = U_SECURITY_TLS_VERSION_1_0;
                        }
                    }
                }
                if (errorCode == 0) {
                    pNetworkSpecific = (void *) pUShortRangeSecTlsAdd(tlsVersionMin,
                                                                      pRootCaCertificateName,
                                                                      pClientCertificateName,
                                                                      pClientPrivateKeyName,
                                                                      certificateCheckOn);
                    if (pNetworkSpecific == NULL) {
                        errorCode = uShortRangeSecTlsResetLastError();
                    }
                }
            } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                // Allocate a cellular security context with
                // default settings
                pNetworkSpecific = (void *) pUCellSecSecTlsAdd(devHandle);
                if (pNetworkSpecific == NULL) {
                    errorCode = uCellSecTlsResetLastError();
                } else {
                    if (pSettings != NULL) {
                        // Looks like some specific settings have been
                        // requested: set them
                        if (pSettings->tlsVersionMin != U_SECURITY_TLS_VERSION_ANY) {
                            // Set the TLS version (encoding is the
                            // same in cellular)
                            errorCode = uCellSecTlsVersionSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                              (int32_t) pSettings->tlsVersionMin);
                        }
                        if ((errorCode == 0) &&
                            (pSettings->pRootCaCertificateName != NULL)) {
                            // Set the root CA certificate name
                            errorCode = uCellSecTlsRootCaCertificateNameSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                            pSettings->pRootCaCertificateName);
                        }
                        if ((errorCode == 0) &&
                            (pSettings->pClientCertificateName != NULL)) {
                            // Set the client certificate name
                            errorCode = uCellSecTlsClientCertificateNameSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                            pSettings->pClientCertificateName);
                        }
                        if ((errorCode == 0) &&
                            (pSettings->pClientPrivateKeyName != NULL)) {
                            // Set the client private key name
                            errorCode = uCellSecTlsClientPrivateKeyNameSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                           pSettings->pClientPrivateKeyName,
                                                                           pSettings->pClientPrivateKeyPassword);
                        }
                        if ((errorCode == 0) &&
                            (pSettings->cipherSuites.num > 0)) {
                            // Set the cipher suites
                            for (size_t x = 0; (x < pSettings->cipherSuites.num) &&
                                 (errorCode == 0); x++) {
                                errorCode = uCellSecTlsCipherSuiteAdd((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                      (int32_t) pSettings->cipherSuites.suite[x]);
                            }
                        }
                        if ((errorCode == 0) &&
                            (((pSettings->psk.pBin != NULL) && (pSettings->psk.size > 0) &&
                              (pSettings->pskId.pBin != NULL) && (pSettings->pskId.size > 0)) ||
                             pSettings->pskGeneratedByRoT)) {
                            // Set the pre-shared key and accompanying ID
                            errorCode = uCellSecTlsClientPskSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                pSettings->psk.pBin, pSettings->psk.size,
                                                                pSettings->pskId.pBin, pSettings->pskId.size,
                                                                pSettings->pskGeneratedByRoT);
                        }
                        if (errorCode == 0) {
                            // Set the certificate checking
                            errorCode = uCellSecTlsCertificateCheckSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                       (uCellSecTlsCertficateCheck_t) pSettings->certificateCheck,
                                                                       pSettings->pExpectedServerUrl);
                        }
                        if ((errorCode == 0) && (pSettings->pSni != NULL)) {
                            // Set the Server Name Indication string
                            errorCode = uCellSecTlsSniSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                          pSettings->pSni);
                        }
                        if ((errorCode == 0) && (pSettings->useDeviceCertificate)) {
                            // Set that the device certificate from security sealing
                            // should be used as the client certificate
                            errorCode = uCellSecTlsUseDeviceCertificateSet((uCellSecTlsContext_t *) pNetworkSpecific,
                                                                           pSettings->includeCaCertificates);
                        }
                    }
                }
            } else if (devType < 0) {
                errorCode = devType;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    // Finally, set the values in the returned context
    if (pContext != NULL) {
        pContext->errorCode = errorCode;
        pContext->devHandle = devHandle;
        pContext->pNetworkSpecific = pNetworkSpecific;
    }

    return pContext;
}

// Free the given TLS security context.
void uSecurityTlsRemove(uSecurityTlsContext_t *pContext)
{
    if ((pContext != NULL) && (init() == 0)) {

        U_PORT_MUTEX_LOCK(gMutex);

        int32_t devType = uDeviceGetDeviceType(pContext->devHandle);
        if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
            uShortRangeSecTlsRemove((uShortRangeSecTlsContext_t *) pContext->pNetworkSpecific);
        } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
            uCellSecTlsRemove((uCellSecTlsContext_t *) pContext->pNetworkSpecific);
        }
        uPortFree(pContext);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Clean-up memory from TLS security contexts.
void uSecurityTlsCleanUp()
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// End of file
