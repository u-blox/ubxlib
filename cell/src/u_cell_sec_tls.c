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
 * @brief Implementation of the TLS security API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "ctype.h"     // isxdigit(), isprint()
#include "stdio.h"     // snprintf()
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_hex_bin_convert.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"         // Order is
#include "u_cell_net.h"          // important here
#include "u_cell_private.h"      // don't change it

#include "u_cell_sec_tls.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_SEC_PROFILES_MAX_NUM
/** The maximum number of security profile IDs that can be supported
 * at once.
 */
# define U_CELL_SEC_PROFILES_MAX_NUM 5
#endif

#ifndef U_CELL_SEC_CIPHERS_BUFFER_LENGTH_BYTES
/** The maximum length of a ciphers string, as pointed to by
 * pString in uCellSecTlsCipherList_t.
 */
# define U_CELL_SEC_CIPHERS_BUFFER_LENGTH_BYTES 1024
#endif

/** The number of characters in a valid IANA cipher
 * identifier string.
  */
#define U_CELL_SEC_IANA_STRING_NUM_CHARS 4

// Do some cross checking
#if U_CELL_SEC_TLS_PSK_ID_MAX_LENGTH_BYTES < U_CELL_SEC_TLS_PSK_MAX_LENGTH_BYTES
# error U_CELL_SEC_TLS_PSK_ID_MAX_LENGTH_BYTES is less than U_CELL_SEC_TLS_PSK_MAX_LENGTH_BYTES
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold an IANA number and a legacy u-blox cipher suite
 * number.
 */
typedef struct {
    uint16_t iana;
    uint8_t legacy;
} uCellSecTlsIanaToLegacy_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The last error code.
 */
static int32_t gLastErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

/** The array of contexts.
 */
static uCellSecTlsContext_t *gpContextList[U_CELL_SEC_PROFILES_MAX_NUM] = {0};

/** Array of IANA to u-blox legacy cipher suite numbers.
 */
static const uCellSecTlsIanaToLegacy_t gIanaToLegacyCipher[] = {
    {0x002f, 1},   // TLS_RSA_WITH_AES_128_CBC_SHA
    {0x003c, 2},   // TLS_RSA_WITH_AES_128_CBC_SHA256
    {0x0035, 3},   // TLS_RSA_WITH_AES_256_CBC_SHA
    {0x003d, 4},   // TLS_RSA_WITH_AES_256_CBC_SHA256
    {0x000a, 5},   // TLS_RSA_WITH_3DES_EDE_CBC_SHA
    {0x008c, 6},   // TLS_PSK_WITH_AES_128_CBC_SHA
    {0x008d, 7},   // TLS_PSK_WITH_AES_256_CBC_SHA
    {0x008b, 8},   // TLS_PSK_WITH_3DES_EDE_CBC_SHA
    {0x0094, 9},   // TLS_RSA_PSK_WITH_AES_128_CBC_SHA
    {0x0095, 10},  // TLS_RSA_PSK_WITH_AES_256_CBC_SHA
    {0x0093, 11},  // TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA
    {0x00ae, 12},  // TLS_PSK_WITH_AES_128_CBC_SHA256
    {0x00af, 13},  // TLS_PSK_WITH_AES_256_CBC_SHA384
    {0x00b6, 14},  // TLS_RSA_PSK_WITH_AES_128_CBC_SHA256
    {0x00b7, 15},  // TLS_RSA_PSK_WITH_AES_256_CBC_SHA384
    {0x00a8, 16},  // TLS_PSK_WITH_AES_128_GCM_SHA256
    {0x00a9, 17},  // TLS_PSK_WITH_AES_256_GCM_SHA384
    {0x00ac, 18},  // TLS_RSA_PSK_WITH_AES_128_GCM_SHA256
    {0x00ad, 19},  // TLS_RSA_PSK_WITH_AES_256_GCM_SHA384
    {0xc008, 20},  // TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
    {0xc009, 21},  // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
    {0xc00a, 22},  // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
    {0xc012, 23},  // TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
    {0xc013, 24},  // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
    {0xc014, 25},  // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
    {0xc023, 26},  // TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
    {0xc024, 27},  // TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
    {0xc027, 28},  // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
    {0xc028, 29},  // TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
    {0xc02b, 30},  // TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
    {0xc02c, 31},  // TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
    {0xc02f, 32},  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
    {0xc030, 33}   // TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get a new context.
static uCellSecTlsContext_t *pNewContext()
{
    uCellSecTlsContext_t *pContext = NULL;

    // Find an unused entry in the list
    for (size_t x = 0; (x < sizeof(gpContextList) / sizeof(gpContextList[0])) &&
         (pContext == NULL); x++) {
        if (gpContextList[x] == NULL) {
            // Allocate memory for the entry
            gpContextList[x] = (uCellSecTlsContext_t *) pUPortMalloc(sizeof(uCellSecTlsContext_t));
            if (gpContextList[x] != NULL) {
                // Initialise the entry
                pContext = gpContextList[x];
                pContext->cellHandle = NULL;
                pContext->cipherList.pString = NULL;
                pContext->cipherList.index = 0;
                pContext->profileId = (uint8_t) x;
            }
        }
    }

    return pContext;
}

// Free a security context.
static void freeContext(const uCellSecTlsContext_t *pContext)
{
    uint8_t profileId = pContext->profileId;

    if (profileId < sizeof(gpContextList) / sizeof(gpContextList[0])) {
        // Free the context
        uPortFree(gpContextList[profileId]);
        // Mark the entry in the list as free
        gpContextList[profileId] = NULL;
    }
}

// Set a string parameter using AT+USECPRF.
static int32_t setString(const uCellSecTlsContext_t *pContext,
                         const char *pString,
                         int32_t opCode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to set the string thing
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                // Profile ID
                uAtClientWriteInt(atHandle, pContext->profileId);
                // The operation
                uAtClientWriteInt(atHandle, opCode);
                // The string thing
                uAtClientWriteString(atHandle, pString, true);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get a string parameter using AT+USECPRF.
static int32_t getString(const uCellSecTlsContext_t *pContext,
                         char *pString, size_t size,
                         int32_t opCode)
{
    int32_t errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t readSize = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to get the string thing
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                // Profile ID
                uAtClientWriteInt(atHandle, pContext->profileId);
                // The operation
                uAtClientWriteInt(atHandle, opCode);
                uAtClientCommandStop(atHandle);
                // Grab the string thing
                // The response is +USECPRF: 0,x,<string>
                uAtClientResponseStart(atHandle, "+USECPRF:");
                // Skip the first two parameters
                uAtClientSkipParameters(atHandle, 2);
                readSize = uAtClientReadString(atHandle, pString,
                                               size, false);
                uAtClientResponseStop(atHandle);
                errorCodeOrSize = uAtClientUnlock(atHandle);
                if ((errorCodeOrSize == 0) && (readSize > 0)) {
                    errorCodeOrSize = readSize;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrSize;
}

// Set a binary sequence using AT+USECPRF.
static int32_t setSequence(const uCellSecTlsContext_t *pContext,
                           const char *pBinary, size_t size,
                           int32_t opCode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const uCellPrivateModule_t *pModule;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    char *pString = NULL;
    size_t y;
    bool isHex = false;
    bool good = true;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                pModule = pUCellPrivateGetModule(pContext->cellHandle);
                if (U_CELL_PRIVATE_HAS(pModule,
                                       U_CELL_PRIVATE_FEATURE_SECURITY_TLS_PSK_AS_HEX)) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    // If the module supports encoding the PSK as
                    // hex then do that since then it can include
                    // zeroes
                    isHex = true;
                    pString = (char *) pUPortMalloc(size * 2 + 1);
                    if (pString != NULL) {
                        // Encode as hex
                        y = uBinToHex(pBinary, size, pString);
                        // Add a terminator
                        *(pString + y) = 0;
                    }
                } else {
                    // Check that what we've been given is
                    // a printable ASCII string
                    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
                    for (size_t x = 0; (x < size) && good; x++) {
                        good = isprint((int32_t) *(pBinary + x)) != 0; // *NOPAD*
                    }
                    if (good) {
                        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        pString = (char *) pUPortMalloc(size + 1);
                        if (pString != NULL) {
                            // Just copy
                            memcpy(pString, pBinary, size);
                            // Add a terminator
                            *(pString + size) = 0;
                        }
                    }
                }
                if (pString != NULL) {
                    atHandle = pInstance->atHandle;
                    // Talk to the cellular module to set the thing
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USECPRF=");
                    // Profile ID
                    uAtClientWriteInt(atHandle, pContext->profileId);
                    // The operation
                    uAtClientWriteInt(atHandle, opCode);
                    // The string
                    uAtClientWriteString(atHandle, pString, true);
                    if (isHex) {
                        // The string type
                        uAtClientWriteInt(atHandle, 1);
                    }
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);

                    // Free memory
                    uPortFree(pString);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Given a u-blox legacy cipher suite number, return the IANA
// number or negative error code.
static int32_t getLegacy(int32_t iana)
{
    int32_t errorCodeOrLegacy = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    for (size_t x = 0; (x < sizeof(gIanaToLegacyCipher) /
                        sizeof(gIanaToLegacyCipher[0])) &&
         (errorCodeOrLegacy < 0); x++) {
        if (iana == gIanaToLegacyCipher[x].iana) {
            errorCodeOrLegacy = gIanaToLegacyCipher[x].legacy;
        }
    }

    return errorCodeOrLegacy;
}

// Add or remove a cipher suite to the set in use.
static int32_t cipherSuiteSet(const uCellSecTlsContext_t *pContext,
                              int32_t ianaNumber, bool addNotRemove)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const uCellPrivateModule_t *pModule;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t y = 100;
    char buffer[3]; // Enough room for, e.g. "C0" and a null terminator

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                pModule = pUCellPrivateGetModule(pContext->cellHandle);
                if (!U_CELL_PRIVATE_HAS(pModule,
                                        U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)) {
                    // When using legacy numbering only a single
                    // cipher suite can be selected and removing
                    // it is done by setting to zero
                    y = 0;
                    if (addNotRemove) {
                        y = getLegacy(ianaNumber);
                    }
                } else {
                    if (!U_CELL_PRIVATE_HAS(pModule,
                                            U_CELL_PRIVATE_FEATURE_SECURITY_TLS_CIPHER_LIST)) {
                        // If we have IANA numbering but not in list form
                        // we can use the IANA numbers given directly but
                        // we still use zero to remove and the value of
                        // y becomes 99
                        y = 0;
                        if (addNotRemove) {
                            y = 99;
                        }
                    }
                }
                if (y >= 0) {
                    atHandle = pInstance->atHandle;
                    // Talk to the cellular module to add the
                    // cipher suite
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USECPRF=");
                    // Profile ID
                    uAtClientWriteInt(atHandle, pContext->profileId);
                    // The cipher suite operation
                    uAtClientWriteInt(atHandle, 2);
                    // Legacy number or IANA format indicator (100 or 99)
                    uAtClientWriteInt(atHandle, y);
                    if (y >= 99) {
                        // The next parameter is the upper-byte of the
                        // IANA number as a two-character string
                        snprintf(buffer, sizeof(buffer), "%02x", (int) ((((uint32_t) ianaNumber) >> 8) & 0xFF));
                        uAtClientWriteString(atHandle, buffer, true);
                        // Then the lower-byte of the
                        // IANA number as a two-character string
                        snprintf(buffer, sizeof(buffer), "%02x", (int) (ianaNumber & 0xFF));
                        uAtClientWriteString(atHandle, buffer, true);
                        if (y == 100) {
                            // We have a list
                            if (addNotRemove) {
                                // "Add" operation
                                uAtClientWriteInt(atHandle, 0);
                            } else {
                                // "Remove" operation
                                uAtClientWriteInt(atHandle, 1);
                            }
                        }
                    }
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Free cipher suite listing memory.
// The mutex must be locked before this is called.
static void cipherListFree(uCellSecTlsCipherList_t *pList)
{
    uPortFree(pList->pString);
    pList->pString = NULL;
    pList->index = 0;
}

// Get the next entry from pList and move the index on.
// The mutex must be locked before this is called.
static int32_t cipherListGetRemove(uCellSecTlsCipherList_t *pList)
{
    int32_t errorCodeOrIana = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    char *pTmp;
    bool goodNumber = true;
    bool lastEntry = false;

    if (pList->pString != NULL) {
        // The index is at the next entry to fetch
        pTmp = pList->pString + pList->index;
        // The next U_CELL_SEC_IANA_STRING_NUM_CHARS characters must be hex digits
        for (size_t x = 0; (x < U_CELL_SEC_IANA_STRING_NUM_CHARS) && goodNumber; x++) {
            goodNumber = isxdigit((int32_t) * pTmp) != 0;
            pTmp++;
        }
        if (goodNumber) {
            // Next must either be a ';' delimiter or a null terminator
            if (*pTmp == 0) {
                lastEntry = true;
            }
            if (lastEntry || (*pTmp == ';')) {
                // Make it a terminator so that we can use strtol()
                *pTmp = 0;
                // We have U_CELL_SEC_IANA_STRING_NUM_CHARS characters
                // with a null terminator on the end, convert them from hex
                errorCodeOrIana = strtol(pList->pString + pList->index,
                                         NULL, 16);
                pList->index += U_CELL_SEC_IANA_STRING_NUM_CHARS;
                if (lastEntry) {
                    // If that was the last entry, free the list
                    cipherListFree(pList);
                } else {
                    // Move over the terminator to the start of the
                    // next entry
                    pList->index++;
                }
            } else {
                // Unexpected: best clear the list
                cipherListFree(pList);
            }
        } else {
            // Unexpected: best clear the list
            cipherListFree(pList);
        }
    }

    return errorCodeOrIana;
}

// Set root of trust PSK generation using AT+USECPRF.
static int32_t setGeneratePsk(const uCellSecTlsContext_t *pContext,
                              bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const uCellPrivateModule_t *pModule;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pModule = pUCellPrivateGetModule(pContext->cellHandle);
            if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
                if (pInstance != NULL) {
                    atHandle = pInstance->atHandle;
                    // Talk to the cellular module to set the PSK
                    // generation mode
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USECPRF=");
                    uAtClientWriteInt(atHandle, pContext->profileId);
                    // RoT PSK generation operation
                    uAtClientWriteInt(atHandle, 11);
                    if (onNotOff) {
                        uAtClientWriteInt(atHandle, 1);
                    } else {
                        uAtClientWriteInt(atHandle, 0);
                    }
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                }
            } else {
                if (onNotOff) {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: ADD/REMOVE A TLS SECURITY CONTEXT
 * -------------------------------------------------------------- */

// Add a cellular TLS security context with default settings.
uCellSecTlsContext_t *pUCellSecSecTlsAdd(uDeviceHandle_t cellHandle)
{
    uCellSecTlsContext_t *pContext = NULL;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            gLastErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pContext = pNewContext();
            if (pContext != NULL) {
                pContext->cellHandle = cellHandle;
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to initialise the context
                // to defaults
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                uAtClientWriteInt(atHandle, pContext->profileId);
                uAtClientCommandStopReadResponse(atHandle);
                gLastErrorCode = uAtClientUnlock(atHandle);
                if (gLastErrorCode != 0) {
                    // If initialisation failed, free the
                    // context again
                    freeContext(pContext);
                    pContext = NULL;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return pContext;
}

// Remove a cellular TLS security context.
void uCellSecTlsRemove(uCellSecTlsContext_t *pContext)
{
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (pContext != NULL) {
            cipherListFree(&(pContext->cipherList));
            freeContext(pContext);
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

// Get the last error that occurred in this API and reset it.
int32_t uCellSecTlsResetLastError()
{
    int32_t errorCode = gLastErrorCode;
    gLastErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURE CERTIFICATES/SECRETS
 * -------------------------------------------------------------- */

// Set the name of the root CA X.509 certificate to use.
int32_t uCellSecTlsRootCaCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                            const char *pName)
{
    // Operation 3 is the root CA X.509 cert name operation
    gLastErrorCode = setString(pContext, pName, 3);
    return gLastErrorCode;
}

// Get the name of the root CA X.509 certificate in use.
int32_t uCellSecTlsRootCaCertificateNameGet(const uCellSecTlsContext_t *pContext,
                                            char *pName,
                                            size_t size)
{
    // Operation 3 is the root CA X.509 cert name operation
    gLastErrorCode = getString(pContext, pName, size, 3);
    return gLastErrorCode;
}

// Set the name of the client X.509 certificate to use.
int32_t uCellSecTlsClientCertificateNameSet(const uCellSecTlsContext_t *pContext,
                                            const char *pName)
{
    // Operation 5 is the client X.509 cert name operation
    gLastErrorCode = setString(pContext, pName, 5);
    return gLastErrorCode;
}

// Get the name of the client X.509 certificate in use.
int32_t uCellSecTlsClientCertificateNameGet(const uCellSecTlsContext_t *pContext,
                                            char *pName,
                                            size_t size)
{
    // Operation 5 is the client X.509 cert name operation
    gLastErrorCode = getString(pContext, pName, size, 5);
    return gLastErrorCode;
}

// Set the name of the client private key and associated password.
int32_t uCellSecTlsClientPrivateKeyNameSet(const uCellSecTlsContext_t *pContext,
                                           const char *pName,
                                           const char *pPassword)
{
    // Operation 6 is the private key name operation
    gLastErrorCode = setString(pContext, pName, 6);
    if ((gLastErrorCode == 0) && (pPassword != NULL)) {
        // Operation 7 is the private key password operation
        gLastErrorCode = setString(pContext, pPassword, 7);
    }
    return gLastErrorCode;
}

// Get the name of the client private key in use.
int32_t uCellSecTlsClientPrivateKeyNameGet(const uCellSecTlsContext_t *pContext,
                                           char *pName,
                                           size_t size)
{
    // Operation 6 is the private key name operation
    gLastErrorCode = getString(pContext, pName, size, 6);
    return gLastErrorCode;
}

// Set the pre-shared key and pre-shared key identity to use.
int32_t uCellSecTlsClientPskSet(const uCellSecTlsContext_t *pContext,
                                const char *pPsk, size_t pskLengthBytes,
                                const char *pPskId, size_t pskIdLengthBytes,
                                bool generate)
{
    gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    if (pContext != NULL) {
        if (generate) {
            if ((pPsk == NULL) && (pskLengthBytes == 0) &&
                (pPskId == NULL) && (pskIdLengthBytes == 0)) {
                gLastErrorCode = setGeneratePsk(pContext, true);
            }
        } else {
            if ((pPsk != NULL) && (pskLengthBytes > 0) &&
                (pskLengthBytes <= U_CELL_SEC_TLS_PSK_MAX_LENGTH_BYTES) &&
                (pPskId != NULL) && (pskIdLengthBytes > 0) &&
                (pskIdLengthBytes <= U_CELL_SEC_TLS_PSK_ID_MAX_LENGTH_BYTES)) {
                gLastErrorCode = setGeneratePsk(pContext, false);
                if (gLastErrorCode == 0) {
                    // Operation 8 is the PSK operation
                    gLastErrorCode = setSequence(pContext, pPsk,
                                                 pskLengthBytes, 8);
                    if (gLastErrorCode == 0) {
                        // Operation 9 is the PSK ID operation
                        gLastErrorCode = setSequence(pContext, pPskId,
                                                     pskIdLengthBytes, 9);
                    }
                }
            }
        }
    }

    return gLastErrorCode;
}

// Use the device public X.509 certificate from security sealing as
// the client certificate.
int32_t uCellSecTlsUseDeviceCertificateSet(const uCellSecTlsContext_t *pContext,
                                           bool includeCaCertificates)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t parameter = 2; // Default is not to include CA certificates

    if (includeCaCertificates) {
        parameter = 1;
    }

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                uAtClientWriteInt(atHandle, pContext->profileId);
                uAtClientWriteInt(atHandle, 14);
                uAtClientWriteInt(atHandle, parameter);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get whether the device public X.509 certificate from security sealing
// is being used as the client certificate.
bool uCellSecTlsIsUsingDeviceCertificate(const uCellSecTlsContext_t *pContext,
                                         bool *pIncludeCaCertificates)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x;
    bool isUsingDeviceCertificate = false;

    if (pIncludeCaCertificates != NULL) {
        *pIncludeCaCertificates = false;
    }

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                uAtClientWriteInt(atHandle, pContext->profileId);
                uAtClientWriteInt(atHandle, 14);
                uAtClientCommandStop(atHandle);
                // The response is +USECPRF: 0,14,x
                uAtClientResponseStart(atHandle, "+USECPRF:");
                // Skip the first parameter, which is just 14
                // coming back at us
                uAtClientSkipParameters(atHandle, 1);
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (x > 0)) {
                    isUsingDeviceCertificate = true;
                    if ((pIncludeCaCertificates != NULL) && (x == 1)) {
                        *pIncludeCaCertificates = true;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return isUsingDeviceCertificate;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CONFIGURE CIPHER SUITE
 * -------------------------------------------------------------- */

// Add a cipher suite to the set in use.
int32_t uCellSecTlsCipherSuiteAdd(const uCellSecTlsContext_t *pContext,
                                  int32_t ianaNumber)
{
    gLastErrorCode = cipherSuiteSet(pContext, ianaNumber, true);
    return gLastErrorCode;
}

// Remove a cipher suite from the set in use.
int32_t uCellSecTlsCipherSuiteRemove(const uCellSecTlsContext_t *pContext,
                                     int32_t ianaNumber)
{
    gLastErrorCode = cipherSuiteSet(pContext, ianaNumber, false);
    return gLastErrorCode;
}

// Get the first cipher suite in use.
int32_t uCellSecTlsCipherSuiteListFirst(uCellSecTlsContext_t *pContext)
{
    const uCellPrivateModule_t *pModule;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellSecTlsCipherList_t *pCipherList;
    int32_t readSize = 0;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                pModule = pUCellPrivateGetModule(pContext->cellHandle);
                if (U_CELL_PRIVATE_HAS(pModule,
                                       U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)) {
                    gLastErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pCipherList = &(pContext->cipherList);
                    // Free any previous cipher list
                    cipherListFree(pCipherList);
                    // Allocate space for the list
                    pCipherList->pString = (char *) pUPortMalloc(U_CELL_SEC_CIPHERS_BUFFER_LENGTH_BYTES);
                    if (pCipherList->pString != NULL) {
                        atHandle = pInstance->atHandle;
                        // Talk to the cellular module to get the
                        // cipher list
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+USECPRF=");
                        // Profile ID
                        uAtClientWriteInt(atHandle, pContext->profileId);
                        // The cipher suite operation
                        uAtClientWriteInt(atHandle, 2);
                        uAtClientCommandStop(atHandle);
                        // If a list is supported the response is
                        // +USECPRF: 0,2,100,"C02A;C02C...", else it
                        // is +USECPRF: 0,2,99,"C02A"
                        uAtClientResponseStart(atHandle, "+USECPRF:");
                        // Skip the first three parameters
                        uAtClientSkipParameters(atHandle, 3);
                        readSize = uAtClientReadString(atHandle, pCipherList->pString,
                                                       U_CELL_SEC_CIPHERS_BUFFER_LENGTH_BYTES,
                                                       false);
                        uAtClientResponseStop(atHandle);
                        gLastErrorCode = uAtClientUnlock(atHandle);
                        if (gLastErrorCode == 0) {
                            gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                            if (readSize >= U_CELL_SEC_IANA_STRING_NUM_CHARS) {
                                // Go get the first value
                                gLastErrorCode = cipherListGetRemove(pCipherList);
                            }
                        }

                        // Free memory if there's been an error
                        if (gLastErrorCode < 0) {
                            cipherListFree(pCipherList);
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Get the subsequent cipher suite in use.
int32_t uCellSecTlsCipherSuiteListNext(uCellSecTlsContext_t *pContext)
{
    const uCellPrivateModule_t *pModule;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            pModule = pUCellPrivateGetModule(pContext->cellHandle);
            if (U_CELL_PRIVATE_HAS(pModule,
                                   U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)) {
                gLastErrorCode = cipherListGetRemove(&(pContext->cipherList));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Free cipher suite listing memory.
void uCellSecTlsCipherSuiteListLast(uCellSecTlsContext_t *pContext)
{
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        if (pContext != NULL) {
            cipherListFree(&(pContext->cipherList));
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC SETTINGS
 * -------------------------------------------------------------- */

// Set the minimum [D]TLS version to use.
int32_t uCellSecTlsVersionSet(const uCellSecTlsContext_t *pContext,
                              int32_t tlsVersionMin)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x = 0;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pContext != NULL) && (tlsVersionMin >= 0) && (tlsVersionMin <= 12)) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                // Convert to module version number
                switch (tlsVersionMin) {
                    case 0:
                        // Nothing to do
                        break;
                    case 10:
                        x = 1;
                        break;
                    case 11:
                        x = 2;
                        break;
                    case 12:
                        x = 3;
                        break;
                    default:
                        break;
                }
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to set the minimum
                // TLS version
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                // Profile ID
                uAtClientWriteInt(atHandle, pContext->profileId);
                // Min TLS version operation
                uAtClientWriteInt(atHandle, 1);
                // The version
                uAtClientWriteInt(atHandle, x);
                uAtClientCommandStopReadResponse(atHandle);
                gLastErrorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Get the minimum [D]TLS version in use.
int32_t uCellSecTlsVersionGet(const uCellSecTlsContext_t *pContext)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pContext != NULL) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to get the minimum
                // TLS version
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                // Profile ID
                uAtClientWriteInt(atHandle, pContext->profileId);
                // Min TLS version operation
                uAtClientWriteInt(atHandle, 1);
                uAtClientCommandStop(atHandle);
                // The response is +USECPRF: 0,1,<version>
                uAtClientResponseStart(atHandle, "+USECPRF:");
                // Skip the first two parameters
                uAtClientSkipParameters(atHandle, 2);
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                gLastErrorCode = uAtClientUnlock(atHandle);
                if (gLastErrorCode == 0) {
                    gLastErrorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    // Convert
                    switch (x) {
                        case 0:
                            gLastErrorCode = 0;
                            break;
                        case 1:
                            gLastErrorCode = 10;
                            break;
                        case 2:
                            gLastErrorCode = 11;
                            break;
                        case 3:
                            gLastErrorCode = 12;
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Set the type of certificate checking to perform.
int32_t uCellSecTlsCertificateCheckSet(const uCellSecTlsContext_t *pContext,
                                       uCellSecTlsCertficateCheck_t check,
                                       const char *pUrl)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pContext != NULL) &&
            ((check < U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL) || (pUrl != NULL)) &&
            (check < U_CELL_SEC_TLS_CERTIFICATE_CHECK_MAX_NUM)) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                gLastErrorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (check >= U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL) {
                    // Write the URL first
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USECPRF=");
                    // Profile ID
                    uAtClientWriteInt(atHandle, pContext->profileId);
                    // Expected server host name operation
                    uAtClientWriteInt(atHandle, 4);
                    // The URL
                    uAtClientWriteString(atHandle, pUrl, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    gLastErrorCode = uAtClientUnlock(atHandle);
                }
                if (gLastErrorCode == 0) {
                    // Set the certificate checking level
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+USECPRF=");
                    // Profile ID
                    uAtClientWriteInt(atHandle, pContext->profileId);
                    // Certificate check operation
                    uAtClientWriteInt(atHandle, 0);
                    // The check level: can be used directly
                    uAtClientWriteInt(atHandle, (int32_t) check);
                    uAtClientCommandStopReadResponse(atHandle);
                    gLastErrorCode = uAtClientUnlock(atHandle);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Get the type of certificate checking being performed.
int32_t uCellSecTlsCertificateCheckGet(const uCellSecTlsContext_t *pContext,
                                       char *pUrl, size_t size)
{
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x;
    int32_t readSize = 0;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pContext != NULL) && ((pUrl == NULL) || (size > 0))) {
            pInstance = pUCellPrivateGetInstance(pContext->cellHandle);
            if (pInstance != NULL) {
                atHandle = pInstance->atHandle;
                // Talk to the cellular module to get the certificate
                // checking level
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECPRF=");
                // Profile ID
                uAtClientWriteInt(atHandle, pContext->profileId);
                // Certificate check operation
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStop(atHandle);
                // The response is +USECPRF: 0,1,<check>
                uAtClientResponseStart(atHandle, "+USECPRF:");
                // Skip the first two parameters
                uAtClientSkipParameters(atHandle, 2);
                // The check level
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                gLastErrorCode = uAtClientUnlock(atHandle);
                if ((gLastErrorCode == 0) &&
                    (x >= (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL) &&
                    (x < (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_MAX_NUM)) {
                    // Get the URL if requested
                    if (pUrl != NULL) {
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+USECPRF=");
                        // Profile ID
                        uAtClientWriteInt(atHandle, pContext->profileId);
                        // Expected server name operation
                        uAtClientWriteInt(atHandle, 4);
                        uAtClientCommandStop(atHandle);
                        // The response is +USECPRF: 0,1,<server_name>
                        uAtClientResponseStart(atHandle, "+USECPRF:");
                        // Skip the first two parameters
                        uAtClientSkipParameters(atHandle, 2);
                        readSize = uAtClientReadString(atHandle, pUrl,
                                                       size, false);
                        uAtClientResponseStop(atHandle);
                        gLastErrorCode = uAtClientUnlock(atHandle);
                    }
                }
                if ((gLastErrorCode == 0) && (readSize >= 0)) {
                    gLastErrorCode = x;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return gLastErrorCode;
}

// Set the optional Server Name Indication.
int32_t uCellSecTlsSniSet(const uCellSecTlsContext_t *pContext,
                          const char *pSni)
{
    const uCellPrivateModule_t *pModule;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    if ((pContext != NULL) && (pSni != NULL)) {
        gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pModule = pUCellPrivateGetModule(pContext->cellHandle);
        if (U_CELL_PRIVATE_HAS(pModule,
                               U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)) {
            // Operation 10 is the SNI operation
            gLastErrorCode = setString(pContext, pSni, 10);
        }
    }

    return gLastErrorCode;
}

// Get the optional Server Name Indication string.
int32_t uCellSecTlsSniGet(const uCellSecTlsContext_t *pContext,
                          char *pSni, size_t size)
{
    const uCellPrivateModule_t *pModule;

    gLastErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    if (pContext != NULL) {
        gLastErrorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pModule = pUCellPrivateGetModule(pContext->cellHandle);
        if (U_CELL_PRIVATE_HAS(pModule,
                               U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)) {
            // Operation 10 is the SNI operation
            gLastErrorCode = getString(pContext, pSni, size, 10);
        }
    }

    return gLastErrorCode;
}

// End of file
