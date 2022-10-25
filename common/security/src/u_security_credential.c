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
 * @brief Implementation of the comon u-blox credential API;
 * this API is thread-safe.  Since the AT interface for the storage
 * of security credentials, the AT+USECMNG command, is common
 * across all u-blox modules this implementation uses that AT
 * command directly.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen(), strtol()
#include "time.h"      // struct tm
#include "ctype.h"     // isprint(), isblank()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" // isblank() in some cases
#include "u_port_clib_mktime64.h"
#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_ble_module_type.h"
#include "u_wifi_module_type.h"

#include "u_short_range.h"

#include "u_security_credential.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The length of the credential type string (e.g. "CA").
 */
#define U_SECURITY_CREDENTIAL_TYPE_LENGTH_BYTES 2

/** The length of the "expiry date" field returned by
 * AT+USECMNG when listing credentials: format is YYYY/MM/DD HH:MM:SS
 */
#define U_SECURITY_CREDENTIAL_EXPIRATION_DATE_LENGTH_BYTES 19

// Do some cross-checking
#if U_SECURITY_CREDENTIAL_TYPE_LENGTH_BYTES > U_SECURITY_CREDENTIAL_EXPIRATION_DATE_LENGTH_BYTES
#error U_SECURITY_CREDENTIAL_TYPE_LENGTH_BYTES  is greater than U_SECURITY_CREDENTIAL_EXPIRATION_DATE_LENGTH_BYTES, check code below
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Container for a security credential so that it can be used
 * in a linked list.
 */
typedef struct uSecurityCredentialContainer_t {
    uSecurityCredential_t credential;
    struct uSecurityCredentialContainer_t *pNext;
} uSecurityCredentialContainer_t;

/** struct to help converting credential type strings into the
 * credential type enum.
 */
typedef struct {
    const char *pStr;
    uSecurityCredentialType_t type;
} uSecuritCredentialTypeStr_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list of security credential containers,
 * used when reading the list of stored credentials.
 */
static uSecurityCredentialContainer_t *gpCredentialList = NULL;

/** Table of credential type string to credential type values.
 */
static const uSecuritCredentialTypeStr_t gTypeStr[] = {
    {"CA", U_SECURITY_CREDENTIAL_ROOT_CA_X509},
    {"CC", U_SECURITY_CREDENTIAL_CLIENT_X509},
    {"PK", U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE},
    {"SC", U_SECURITY_CREDENTIAL_SERVER_X509},
    {"VC", U_SECURITY_CREDENTIAL_SIGNATURE_VERIFICATION_X509},
    {"PU", U_SECURITY_CREDENTIAL_SIGNATURE_VERIFICATION_KEY_PUBLIC}
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the AT handle used by the given network.
static int32_t getAtClient(uDeviceHandle_t devHandle,
                           uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    int32_t devType = uDeviceGetDeviceType(devHandle);
    if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
        errorCode = uShortRangeAtClientHandleGet(devHandle, pAtHandle);
    } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
        errorCode = uCellAtClientHandleGet(devHandle, pAtHandle);
    } else if (devType < 0) {
        errorCode = devType;
    }

    return errorCode;
}

// Convert a pair of ASCII characters representing a hex number into
// a number
static bool hexToBin(const char *pHex, char *pBin)
{
    bool success = true;
    char y[2];

    y[0] = *pHex - '0';
    pHex++;
    y[1] = *pHex - '0';
    for (size_t x = 0; (x < sizeof(y)) && success; x++) {
        if (y[x] > 9) {
            // Must be A to F or a to f
            y[x] -= 'A' - '0';
            y[x] += 10;
        }
        if (y[x] > 15) {
            // Must be a to f
            y[x] -= 'a' - 'A';
        }
        // Cast here to shut-up a warning under ESP-IDF
        // which appears to have chars as unsigned and
        // hence thinks the first condition is always true
        success = ((signed char) y[x] >= 0) && (y[x] <= 15);
    }

    if (success) {
        *pBin = (char) (((y[0] & 0x0f) << 4) | y[1]);
    }

    return success;
}

// Convert an MD5 hash as an ASCII hex string into a binary
// sequence.
static bool convertHash(const char *pHex, char *pBin)
{
    bool success = true;

    for (size_t x = 0; (x < U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES) && success; x++) {
        success = hexToBin(pHex, pBin);
        pHex += 2;
        pBin++;
    }

    return success;
}

// Convert a credential type string into an enum value.
static uSecurityCredentialType_t convertType(const char *pStr)
{
    uSecurityCredentialType_t type = U_SECURITY_CREDENTIAL_NONE;
    const uSecuritCredentialTypeStr_t *pTypeStr = gTypeStr;

    for (size_t x = 0; (x < sizeof(gTypeStr) / sizeof(gTypeStr[0])) &&
         (type == U_SECURITY_CREDENTIAL_NONE); x++) {
        if (strcmp(pStr, pTypeStr->pStr) == 0) {
            type = pTypeStr->type;
        }
        pTypeStr++;
    }

    return type;
}

// Clear the credential list
static void credentialListClear()
{
    uSecurityCredentialContainer_t *pTmp;

    while (gpCredentialList != NULL) {
        pTmp = gpCredentialList->pNext;
        uPortFree(gpCredentialList);
        gpCredentialList = pTmp;
    }
}

// Add an entry to the end of the linked list
// and count how many are in it once added.
static size_t credentialListAddCount(uSecurityCredentialContainer_t *pAdd)
{
    size_t count = 0;
    uSecurityCredentialContainer_t **ppTmp = &gpCredentialList;

    while (*ppTmp != NULL) {
        ppTmp = &((*ppTmp)->pNext);
        count++;
    }

    if (pAdd != NULL) {
        *ppTmp = pAdd;
        count++;
    }

    return count;
}

// Get an entry from the start of the linked list and remove
// it from the list, returning the number left
static int32_t credentialListGetRemove(uSecurityCredential_t *pCredential)
{
    int32_t errorOrCount = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uSecurityCredentialContainer_t *pTmp = gpCredentialList;

    if (pTmp != NULL) {
        if (pCredential != NULL) {
            memcpy(pCredential, &(pTmp->credential), sizeof(*pCredential));
        }
        pTmp = gpCredentialList->pNext;
        uPortFree(gpCredentialList);
        gpCredentialList = pTmp;
        errorOrCount = 0;
        while (pTmp != NULL) {
            pTmp = pTmp->pNext;
            errorOrCount++;
        }
    }

    return errorOrCount;
}

// Parse a string of the form YYYY/MM/DD HH:MM:SS to make
// a UTC timestamp.  Note: this modifies the incoming string.
static int64_t parseTimestampString(char *pStr)
{
    int64_t utc = 0;
    struct tm timeStruct = {0};

    // Do sanity checks
    if ((strlen(pStr) == 19) && (*(pStr + 4) == '/') &&
        (*(pStr + 7) == '/') && (*(pStr + 10) == ' ') &&
        (*(pStr + 13) == ':') && (*(pStr + 16) == ':')) {

        // Populate the time structure
        timeStruct.tm_sec = strtol(pStr + 17, NULL, 10);
        *(pStr + 16) = 0;
        timeStruct.tm_min = strtol(pStr + 14, NULL, 10);
        *(pStr + 13) = 0;
        timeStruct.tm_hour = strtol(pStr + 11, NULL, 10);
        *(pStr + 10) = 0;
        timeStruct.tm_mday = strtol(pStr + 8, NULL, 10);
        *(pStr + 7) = 0;
        timeStruct.tm_mon = strtol(pStr + 5, NULL, 10) - 1;
        *(pStr + 4) = 0;
        timeStruct.tm_year = strtol(pStr, NULL, 10) - 1900;

        utc = mktime64(&timeStruct);
    }

    return utc;
}

// Strip whitespace from a null-terminated string, in-place
static size_t stripWhitespace(char *pString, size_t stringLength)
{
    size_t newLength = 0;
    char *pFrom = pString;

    for (size_t x = 0; x < stringLength; x++) {
        if (isprint((int32_t) (uint8_t) *pFrom) && !isblank((int32_t) (uint8_t) *pFrom)) {
            *(pString + newLength) = *pFrom;
            newLength++;
        }
        pFrom++;
    }

    if (stringLength > 0) {
        // Add a terminator
        *(pString + newLength) = 0;
    }

    return newLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Store the given X.509 certificate or security key.
int32_t uSecurityCredentialStore(uDeviceHandle_t devHandle,
                                 uSecurityCredentialType_t type,
                                 const char *pName,
                                 const char *pContents,
                                 size_t size,
                                 const char *pPassword,
                                 char *pMd5)
{
    uAtClientHandle_t atHandle;
    int32_t errorCode = getAtClient(devHandle, &atHandle);
    char hashHexRead[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1]; // +1 for terminator
    int32_t hashHexReadSize;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        //lint -e(568) Suppress warning that type can't be negative:
        // it can be if people are careless so better to check it
        if ((((int32_t) type >= 0) && (type < U_SECURITY_CREDENTIAL_MAX_NUM)) &&
            ((pName != NULL) && (strlen(pName) <= U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES)) &&
            ((size > 0) || (pContents == NULL)) &&
            (size <= U_SECURITY_CREDENTIAL_MAX_LENGTH_BYTES) &&
            ((pPassword == NULL) ||
             ((pContents != NULL) &&
              (type == U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE) &&
              (strlen(pPassword) <= U_SECURITY_CREDENTIAL_PASSWORD_MAX_LENGTH_BYTES)))) {
            if (pContents != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                // Do the USECMNG thang with the AT interface
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+USECMNG=");
                // Write credential operation
                uAtClientWriteInt(atHandle, 0);
                // Type
                uAtClientWriteInt(atHandle, (int32_t) type);
                // Name
                uAtClientWriteString(atHandle, pName, true);
                // Number of bytes to follow
                uAtClientWriteInt(atHandle, (int32_t) size);
                if (pPassword) {
                    // Password, if present
                    uAtClientWriteString(atHandle, pPassword, true);
                }
                uAtClientCommandStop(atHandle);
                // Wait for the prompt
                if (uAtClientWaitCharacter(atHandle, '>') == 0) {
                    // Allow plenty of time for this to complete
                    uAtClientTimeoutSet(atHandle, 10000);
                    // Wait for it...
                    uPortTaskBlock(50);
                    // Write the contents
                    uAtClientWriteBytes(atHandle, pContents, size, true);
                    // Grab the response
                    uAtClientResponseStart(atHandle, "+USECMNG:");
                    // Skip the first three parameters
                    uAtClientSkipParameters(atHandle, 3);
                    // Grab the MD5 hash, which is a quoted hex string
                    hashHexReadSize = uAtClientReadString(atHandle, hashHexRead,
                                                          sizeof(hashHexRead),
                                                          false);
                    uAtClientResponseStop(atHandle);
                    if ((uAtClientUnlock(atHandle) == 0) &&
                        (hashHexReadSize == sizeof(hashHexRead) - 1)) {
                        // Convert the hash into a binary sequence and write
                        // it to pMd5
                        if (pMd5 != NULL) {
                            if (convertHash(hashHexRead, pMd5)) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            }
                        } else {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                } else {
                    // Best to tidy whatever might have arrived instead
                    // of the prompt before exitting
                    uAtClientResponseStop(atHandle);
                    uAtClientUnlock(atHandle);
                }
            } else {
                // Nothing to do
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    return errorCode;
}

// Read the MD5 hash of a stored X.509 certificate
// or security key.
int32_t uSecurityCredentialGetHash(uDeviceHandle_t devHandle,
                                   uSecurityCredentialType_t type,
                                   const char *pName,
                                   char *pMd5)
{
    uAtClientHandle_t atHandle;
    int32_t errorCode = getAtClient(devHandle, &atHandle);
    char hashHexRead[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES * 2 + 1]; // +1 for terminator
    int32_t hashHexReadSize;

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if (((pName != NULL) && (strlen(pName) <= U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES)) &&
            (pMd5 != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            // Do the USECMNG thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+USECMNG=");
            // Read hash operation
            uAtClientWriteInt(atHandle, 4);
            // Type
            uAtClientWriteInt(atHandle, (int32_t) type);
            // Name
            uAtClientWriteString(atHandle, pName, true);
            uAtClientCommandStop(atHandle);
            // Grab the response
            uAtClientResponseStart(atHandle, "+USECMNG:");
            // Skip the first three parameters
            uAtClientSkipParameters(atHandle, 3);
            // Grab the MD5 hash, which is a quoted hex string
            hashHexReadSize = uAtClientReadString(atHandle, hashHexRead,
                                                  sizeof(hashHexRead), false);
            uAtClientResponseStop(atHandle);
            if ((uAtClientUnlock(atHandle) == 0) &&
                (hashHexReadSize == sizeof(hashHexRead) - 1)) {
                // Convert the hash into a binary sequence and write
                // it to pMd5
                if (convertHash(hashHexRead, pMd5)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    return errorCode;
}

// Get the description of the first X.509 certificate or security key.
int32_t uSecurityCredentialListFirst(uDeviceHandle_t devHandle,
                                     uSecurityCredential_t *pCredential)
{
    bool keepGoing = true;
    uAtClientHandle_t atHandle;
    int32_t errorCodeOrSize = getAtClient(devHandle, &atHandle);
    uSecurityCredentialContainer_t *pContainer;
    char buffer[U_SECURITY_CREDENTIAL_EXPIRATION_DATE_LENGTH_BYTES + 1];
    int32_t bytesRead;
    size_t count = 0;

    if (errorCodeOrSize == 0) {
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if (pCredential != NULL) {
            errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
            // Do the USECMNG thang with the AT interface
            uAtClientLock(atHandle);
            // Make sure the credential list is clear
            credentialListClear();
            uAtClientCommandStart(atHandle, "AT+USECMNG=");
            // List credentials operation
            uAtClientWriteInt(atHandle, 3);
            uAtClientCommandStop(atHandle);
            // Will get back a set of single lines:
            // "CA","AddTrustCA","AddTrust External CA Root","2020/05/30"
            // ...where the last two are only present for root and client
            // certificates.  There is no prefix to the line
            // so need to check everything carefully to avoid confusing
            // a line with a URC
            while (keepGoing) {
                errorCodeOrSize = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                keepGoing = false;
                pContainer = (uSecurityCredentialContainer_t *) pUPortMalloc(sizeof(*pContainer));
                if (pContainer != NULL) {
                    pContainer->pNext = NULL;
                    errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                    uAtClientResponseStart(atHandle, NULL);
                    // First parameter should be the credential type
                    bytesRead = uAtClientReadString(atHandle, buffer,
                                                    sizeof(buffer), false);
                    if (bytesRead > 0) {
                        // Some modules (SARA_R410M_02B) add spurious whitespace
                        // at the start of the list: get rid of it
                        // Cast twice to keep Lint happy
                        bytesRead = (int32_t) (signed) stripWhitespace(buffer, (size_t) (unsigned) bytesRead);
                    }
                    if (bytesRead == U_SECURITY_CREDENTIAL_TYPE_LENGTH_BYTES) {
                        errorCodeOrSize = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                        // Convert to one of our enums
                        pContainer->credential.type = convertType(buffer);
                        if (pContainer->credential.type != U_SECURITY_CREDENTIAL_NONE) {
                            // Next is the name
                            bytesRead = uAtClientReadString(atHandle, pContainer->credential.name,
                                                            sizeof(pContainer->credential.name),
                                                            false);
                            if (bytesRead > 0) {
                                pContainer->credential.subject[0] = 0;
                                pContainer->credential.expirationUtc = 0;
                                if ((pContainer->credential.type == U_SECURITY_CREDENTIAL_ROOT_CA_X509) ||
                                    (pContainer->credential.type == U_SECURITY_CREDENTIAL_CLIENT_X509)) {
                                    // For these credential types we *might* have the subject
                                    // and expiry date fields
                                    bytesRead = uAtClientReadString(atHandle, pContainer->credential.subject,
                                                                    sizeof(pContainer->credential.subject),
                                                                    false);
                                    if (bytesRead > 0) {
                                        bytesRead = uAtClientReadString(atHandle, buffer,
                                                                        sizeof(buffer), false);
                                        if (bytesRead == U_SECURITY_CREDENTIAL_EXPIRATION_DATE_LENGTH_BYTES) {
                                            // Parse the expiration date to make a UTC timestamp
                                            pContainer->credential.expirationUtc = parseTimestampString(buffer);
                                            errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                                            keepGoing = true;
                                        }
                                    } else {
                                        // Some modules don't support these fields so
                                        // this is OK
                                        errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                                        keepGoing = true;
                                    }
                                } else {
                                    errorCodeOrSize = (int32_t) U_ERROR_COMMON_SUCCESS;
                                    keepGoing = true;
                                }
                            }
                        }
                    }
                }

                if (keepGoing) {
                    // Add the container to the end of the list
                    count = credentialListAddCount(pContainer);
                } else {
                    // Nothing there, free it
                    uPortFree(pContainer);
                }
                // Now that we've got one, set the timeout short for
                // the rest so that we don't wait around for
                // ages at the end of the list
                uAtClientTimeoutSet(atHandle, 1000);
            }
            uAtClientResponseStop(atHandle);

            // Do the following parts inside the AT lock,
            // providing protection for the linked-list.
            if (errorCodeOrSize == (int32_t) U_ERROR_COMMON_NO_MEMORY) {
                // If we ran out of memory, clear the whole list,
                // don't want to report partial information
                credentialListClear();
            } else {
                if (count > 0) {
                    // Set the return value, copy out the first item in the list
                    // and remove it.
                    errorCodeOrSize = (int32_t) count;
                    credentialListGetRemove(pCredential);
                } else {
                    errorCodeOrSize = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                }
            }

            uAtClientUnlock(atHandle);
        }
    }

    return errorCodeOrSize;
}

// Return subsequent descriptions of credentials in the list.
int32_t uSecurityCredentialListNext(uDeviceHandle_t devHandle,
                                    uSecurityCredential_t *pCredential)
{
    uAtClientHandle_t atHandle;
    int32_t errorCodeOrSize = getAtClient(devHandle, &atHandle);

    if (errorCodeOrSize == 0) {
        errorCodeOrSize = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        if (pCredential != NULL) {
            uAtClientLock(atHandle);
            // While this doesn't use the AT interface we can use
            // the mutex to protect the linked list.
            errorCodeOrSize = credentialListGetRemove(pCredential);
            uAtClientUnlock(atHandle);
        }
    }

    return errorCodeOrSize;
}

// Free memory from credential listing.
void uSecurityCredentialListLast(uDeviceHandle_t devHandle)
{
    uAtClientHandle_t atHandle;

    if (getAtClient(devHandle, &atHandle) == 0) {
        uAtClientLock(atHandle);
        // While this doesn't use the AT interface we can use
        // the mutex to protect the linked list.
        credentialListClear();
        uAtClientUnlock(atHandle);
    }
}

// Remove the given X.509 certificate or security key from storage.
int32_t uSecurityCredentialRemove(uDeviceHandle_t devHandle,
                                  uSecurityCredentialType_t type,
                                  const char *pName)
{
    uAtClientHandle_t atHandle;
    int32_t errorCode = getAtClient(devHandle, &atHandle);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // Check parameters
        //lint -e(568) Suppress warning that type can't be negative:
        // it can be if people are careless so better to check it
        if ((((int32_t) type >= 0) && (type < U_SECURITY_CREDENTIAL_MAX_NUM)) &&
            ((pName != NULL) && (strlen(pName) <= U_SECURITY_CREDENTIAL_NAME_MAX_LENGTH_BYTES))) {
            // Do the USECMNG thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+USECMNG=");
            // Remove item operation
            uAtClientWriteInt(atHandle, 2);
            // Type
            uAtClientWriteInt(atHandle, (int32_t) type);
            // Name
            uAtClientWriteString(atHandle, pName, true);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

// End of file
