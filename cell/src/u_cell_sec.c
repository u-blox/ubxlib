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

#include "u_port_heap.h"
#include "u_port_os.h"
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

/** The length of the encrypted C2C confirmation tag,
 * used in V2 C2C key pairing.
 */
#define U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES (U_CELL_SEC_C2C_IV_LENGTH_BYTES +               \
                                                                U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES + \
                                                                U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES +          \
                                                                U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES)

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

// Check that U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES is big enough to hold
// the hex version of array of U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES.
#if U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2 != U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES
# error U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES not the same size as the ASCII hex version of U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES.
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

// Enrypt a C2C confirmation tag, consisting
// of U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES
// but hex encode (so twice that long).
// pC2cConfirmationTagHex must point to the hex-coded
// C2C confirmation tag, length
// U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2.
// pTeSecret is the fixed length TE secret, length
// U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES.
// pKey is the fixed length encryption key, length
// U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES.
// pHMacKey is the fixed length HMAC key, length
// U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES.
// pOutputBuffer must point to storage of length
// U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES.
// Note that this is actually just the "body" part
// of the V2 C2C frame encoding, see encode() over in
// u_cell_sec_cec.c.
static size_t encryptC2cConfirmationTag(const char *pC2cConfirmationTagHex,
                                        const char *pTeSecret,
                                        const char *pKey,
                                        const char *pHMacKey,
                                        char *pOutputBuffer)
{
    size_t length = 0;
    char ivOrMac[U_PORT_CRYPTO_SHA256_OUTPUT_LENGTH_BYTES];
    // *INDENT-OFF* (otherwise AStyle makes a mess of this)
    char c2cConfirmationTagPadded[U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES +
                                  U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES];
    // *INDENT-ON*

    // Get an IV into a local variable
    memcpy(ivOrMac, pUCellSecC2cGetIv(), U_CELL_SEC_C2C_IV_LENGTH_BYTES);

    // We want to end up with this:
    //
    //  ----------------------------------------------------------------
    // |    IV    | Encrypted padded C2C confirmation  |  truncated MAC |
    // | 16 bytes |           tag (binary)             |     16 bytes   |
    //  ----------------------------------------------------------------
    //
    // Write IV into its position in the output.
    // Then the encryption function can be pointed at the
    // local copy and will overwrite it
    memcpy(pOutputBuffer, ivOrMac, U_CELL_SEC_C2C_IV_LENGTH_BYTES);
    length += U_CELL_SEC_C2C_IV_LENGTH_BYTES;

    // Copy the hex into the padding buffer as binary
    uHexToBin(pC2cConfirmationTagHex, U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2,
              c2cConfirmationTagPadded);

    // Need to deal with padding.  Counter-intuitively, though
    // the binary confirmation tag will be 16 bytes long,
    // that is actually the worst case for padding with the
    // RFC 5652 algorithm: it gains a whole 16 bytes of padding.
    for (size_t x = 0; x < U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES; x++) {
        c2cConfirmationTagPadded[U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES + x] =
            (char) U_CELL_SEC_C2C_MAX_PAD_LENGTH_BYTES;
    }

    // Encrypt the padded binary C2C confirmation tag into the
    // output buffer after the IV using the encryption key and the IV
    if (uPortCryptoAes128CbcEncrypt(pKey,
                                    U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES,
                                    ivOrMac, c2cConfirmationTagPadded,
                                    sizeof(c2cConfirmationTagPadded),
                                    pOutputBuffer + U_CELL_SEC_C2C_IV_LENGTH_BYTES) == 0) {
        length += sizeof(c2cConfirmationTagPadded);
        // Next we need to create a HMAC tag across the
        // IV, the encrypted text and the TE Secret.
        // The simplest way to do this is to copy
        // the TE Secret into the output buffer, perform
        // the calculation (putting the result into the
        // local variable ivOrMac) and then we overwrite
        // where it is in the buffer with the truncated MAC
        // (which is at least as big, as checked with
        // a #error above)
        memcpy(pOutputBuffer + length, pTeSecret, U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES);
        // NOLINTNEXTLINE(readability-suspicious-call-argument)
        if (uPortCryptoHmacSha256(pHMacKey,
                                  U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES,
                                  pOutputBuffer,
                                  length + U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES,
                                  ivOrMac) == 0) {
            // Now copy the first 16 bytes of the
            // generated HMAC tag into the output,
            // overwriting the TE Secret
            memcpy(pOutputBuffer + length, ivOrMac,
                   U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES);
            // Account for its length
            length += U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES;
        }
    }

    return length;
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
 * PUBLIC FUNCTIONS: CHIP TO CHIP SECURITY
 * -------------------------------------------------------------- */

// Pair a cellular module's AT interface for chip to chip security.
int32_t uCellSecC2cPair(uDeviceHandle_t cellHandle,
                        const char *pTESecret,
                        char *pKey, char *pHMac)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t x = -1;
    int32_t y = -1;
    int32_t z = -1;
    char buffer[U_CELL_SEC_HEX_BUFFER_LENGTH_BYTES + 1]; // +1 for terminator
    char *pEncryptedC2cConfirmationTag;
    char *pEncryptedC2cConfirmationTagHex;

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
                uBinToHex(pTESecret, sizeof(buffer) / 2, buffer);
                // Add terminator since the AT write needs a string
                *(buffer + sizeof(buffer) - 1) = 0;
                uAtClientWriteString(atHandle, buffer, true);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USECC2C:");
                // Must get back a zero and then another zero indicating
                // success
                // NOLINTBEGIN(misc-redundant-expression)
                if ((uAtClientReadInt(atHandle) == 0) &&
                    (uAtClientReadInt(atHandle) == 0)) {
                    // NOLINTEND(misc-redundant-expression)
                    // Success: read the key
                    x = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
                    if (x == sizeof(buffer) - 1) {
                        x = (int32_t) uHexToBin(buffer,
                                                sizeof(buffer) - 1,
                                                pKey);
                    }
                    // Try to read the HMAC key, which will
                    // only be present if the module implements
                    // the V2 chip to chip scheme
                    y = uAtClientReadString(atHandle, buffer,
                                            sizeof(buffer), false);
                    if (y == sizeof(buffer) - 1) {
                        y = (int32_t) uHexToBin(buffer,
                                                sizeof(buffer) - 1,
                                                pHMac);
                        // If the HMAC key is present, there must
                        // also be a chip to chip confirmation tag
                        z = uAtClientReadString(atHandle, buffer,
                                                sizeof(buffer), false);
                        // We don't need to convert this to binary,
                        // just need the hex
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
                // so do both the HMAC key and the C2C confirmation tag
                if ((uAtClientUnlock(atHandle) == 0) &&
                    (x == U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES) &&
                    ((z < 0) ||
                     ((y == U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES) &&
                      // * 2 since we're only using the hex here
                      (z == U_SECURITY_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2)))) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }

                if ((errorCode == 0) && (y > 0) && (z > 0)) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    // For V2 encryption there is another step: the C2C
                    // confirmation tag has to be encrypted in exactly
                    // the same way as we would encrypt a C2C frame, using
                    // the secrets, but without the surrounding framing
                    // and then sent back to the module, hex coded, to
                    // confirm that we have received all of the above.

                    // Get memory to put it in
                    // *INDENT-OFF* (otherwise AStyle makes a mess of this)
                    pEncryptedC2cConfirmationTag = (char *) pUPortMalloc(U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES);
                    if (pEncryptedC2cConfirmationTag != NULL) {
                        // ...and memory to put the hex-coded version in, +1 for terminator
                        pEncryptedC2cConfirmationTagHex = (char *) pUPortMalloc((U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2) + 1);
                        // *INDENT-ON*
                        if (pEncryptedC2cConfirmationTagHex != NULL) {
                            errorCode = (int32_t) U_ERROR_COMMON_AUTHENTICATION_FAILURE;
                            // Encrypt the buffer, which should contain the
                            // hex-coded C2C confirmation tag, with all the
                            // other bits and pieces
                            // NOLINTNEXTLINE(readability-suspicious-call-argument)
                            x = (int32_t) encryptC2cConfirmationTag(buffer, pTESecret,
                                                                    pKey, pHMac,
                                                                    pEncryptedC2cConfirmationTag);
                            if (x == U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES) {
                                // Now send the TE secret and this to the module
                                uAtClientLock(atHandle);
                                uAtClientTimeoutSet(atHandle,
                                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                                uAtClientCommandStart(atHandle, "AT+USECC2C=");
                                uAtClientWriteInt(atHandle, 4);
                                uBinToHex(pTESecret, U_SECURITY_C2C_TE_SECRET_LENGTH_BYTES, buffer);
                                // Add a terminator since the AT write needs a string
                                buffer[sizeof(buffer) - 1] = 0;
                                uAtClientWriteString(atHandle, buffer, true);
                                uBinToHex(pEncryptedC2cConfirmationTag,
                                          U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES,
                                          pEncryptedC2cConfirmationTagHex);
                                // Add a terminator since the AT write needs a string
                                *(pEncryptedC2cConfirmationTagHex +
                                  (U_CELL_SEC_ENCRYPTED_C2C_CONFIRMATION_TAG_LENGTH_BYTES * 2)) = 0;
                                uAtClientWriteString(atHandle, pEncryptedC2cConfirmationTagHex, true);
                                uAtClientCommandStopReadResponse(atHandle);
                                // Should get OK back
                                if (uAtClientUnlock(atHandle) == 0) {
                                    // NOW we're good
                                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                }
                            }
                            // Free the hex buffer
                            uPortFree(pEncryptedC2cConfirmationTagHex);
                        }
                        // Free the binary buffer
                        uPortFree(pEncryptedC2cConfirmationTag);
                    }
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
int32_t uCellSecC2cOpen(uDeviceHandle_t cellHandle,
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
                    uBinToHex(pTESecret, sizeof(buffer) / 2, buffer);
                    // Add terminator since the AT write needs a string
                    *(buffer + sizeof(buffer) - 1) = 0;
                    uAtClientWriteString(atHandle, buffer, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode == 0) {
                        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                        // If that was successful, set up
                        // the chip to chip security context
                        pInstance->pSecurityC2cContext = pUPortMalloc(sizeof(uCellSecC2cContext_t));
                        if (pInstance->pSecurityC2cContext != NULL) {
                            pContext = (uCellSecC2cContext_t *) pInstance->pSecurityC2cContext;
                            memset(pContext, 0, sizeof(uCellSecC2cContext_t));
                            pContext->pTx = (uCellSecC2cContextTx_t *) pUPortMalloc(sizeof(uCellSecC2cContextTx_t));
                            if (pContext->pTx != NULL) {
                                memset(pContext->pTx, 0, sizeof(uCellSecC2cContextTx_t));
                                pContext->pRx = (uCellSecC2cContextRx_t *) pUPortMalloc(sizeof(uCellSecC2cContextRx_t));
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
int32_t uCellSecC2cClose(uDeviceHandle_t cellHandle)
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
 * PUBLIC FUNCTIONS: END TO END ENCRYPTION
 * -------------------------------------------------------------- */

// Set the E2E encryption version to be used.
int32_t uCellSecE2eSetVersion(uDeviceHandle_t cellHandle, int32_t version)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (version > 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECOPCMD=");
                uAtClientWriteString(atHandle, "e2e_enc", true);
                uAtClientWriteInt(atHandle, version - 1);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Get the E2E encryption version.
int32_t uCellSecE2eGetVersion(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrVersion = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    int32_t version;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCodeOrVersion = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCodeOrVersion = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientTimeoutSet(atHandle,
                                    U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS * 1000);
                uAtClientCommandStart(atHandle, "AT+USECOPCMD=");
                uAtClientWriteString(atHandle, "e2e_enc", true);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+USECOPCMD:");
                // Skip the first parameter, which is just "e2e_enc"
                // being sent back to us
                uAtClientSkipParameters(atHandle, 1);
                version = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                errorCodeOrVersion = uAtClientUnlock(atHandle);
                if (errorCodeOrVersion == 0) {
                    errorCodeOrVersion = version + 1;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCodeOrVersion;
}

// Ask a cellular module to encrypt a block of data.
int32_t uCellSecE2eEncrypt(uDeviceHandle_t cellHandle,
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
                            // Read the length of the response
                            sizeOutBytes = uAtClientReadInt(atHandle);
                            if (sizeOutBytes > 0) {
                                // Don't stop for anything!
                                uAtClientIgnoreStopTag(atHandle);
                                // Get the leading quote mark out of the way
                                uAtClientReadBytes(atHandle, NULL, 1, true);
                                // Now read out all the actual data
                                uAtClientReadBytes(atHandle, (char *) pDataOut,
                                                   sizeOutBytes, true);
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
