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
 * @brief Implementation of the configuration API for GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_compiler.h" // U_INLINE
#include "u_error_common.h"
#include "u_assert.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_private.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of values that can be stored in a VALXXX
 * message.
 */
#define U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES 64

#ifndef U_GNSS_CFG_MAX_NUM_VAL_GET_SEGMENTS
/** The maximum number of a VALGET message segments, each containing
 * U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES, that we can handle.
 */
# define U_GNSS_CFG_MAX_NUM_VAL_GET_SEGMENTS 50
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold the body of multiple VALGET messages received
 * from the GNSS module.
 */
typedef struct {
    char *pBody;
    size_t size;
    size_t itemCount;
} uGnssCfgValGetMessageBody_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: OLDE WORLDE
 * -------------------------------------------------------------- */

// Get a single byte value from a UBX-CFG-NAV5 message.
// Note: gUGnssPrivateMutex must be locked before this is called.
static int32_t getUbxCfgNav5(uGnssPrivateInstance_t *pInstance,
                             size_t offset)
{
    int32_t errorCodeOrValue = (int32_t) U_ERROR_COMMON_PLATFORM;
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36];

    // Poll with the message class and ID of the
    // UBX-CFG-NAV5 message
    if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                          0x06, 0x24,
                                          NULL, 0,
                                          message, sizeof(message)) == sizeof(message)) {
        errorCodeOrValue = message[offset];
    }

    return errorCodeOrValue;
}

// Set a single byte value with a UBX-CFG-NAV5 message.
// Note: gUGnssPrivateMutex must be locked before this is called.
static int32_t setUbxCfgNav5(uGnssPrivateInstance_t *pInstance,
                             uint16_t mask, size_t offset, uint8_t value)
{
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36] = {0};

    // Set the mask at the start of the message
    *((uint16_t *) message) = uUbxProtocolUint16Encode(mask);
    // Copy in the byte we want to change at the given offset
    message[offset] = (char) value;

    // Send the UBX-CFG-NAV5 message
    return uGnssPrivateSendUbxMessage(pInstance,
                                      0x06, 0x24,
                                      message,
                                      sizeof(message));
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: VALGET/VALSET/VALDEL
 * -------------------------------------------------------------- */

// Encode a layer enum into the value for a VALGET message.
static int32_t encodeLayerForGet(uGnssCfgValLayer_t layer)
{
    int32_t encodedLayer = -1;

    switch (layer) {
        case U_GNSS_CFG_VAL_LAYER_RAM:
            encodedLayer = 0;
            break;
        case U_GNSS_CFG_VAL_LAYER_BBRAM:
            encodedLayer = 1;
            break;
        case U_GNSS_CFG_VAL_LAYER_FLASH:
            encodedLayer = 2;
            break;
        case U_GNSS_CFG_VAL_LAYER_DEFAULT:
            encodedLayer = 7;
            break;
        default:
            break;
    }

    return encodedLayer;
}

// Get the size in bytes of an item, given the storage size from the key ID.
static U_INLINE size_t getStorageSizeBytes(uGnssCfgValKeySize_t storageSize)
{
    size_t size = 0;

    switch (storageSize) {
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BIT:
        //lint -fallthrough
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BYTE:
            size = 1;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_TWO_BYTES:
            size = 2;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_FOUR_BYTES:
            size = 4;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES:
            size = 8;
            break;
        default:
            break;
    }

    return size;
}

// Pack a value from a configuration item into a buffer.
static U_INLINE void packValue(char *pBuffer, const uint64_t *pValue, size_t storageSizeBytes)
{
    switch (storageSizeBytes) {
        case 1:
            *(uint8_t *) pBuffer = *((const uint8_t *) pValue);
            break;
        case 2:
            *(uint16_t *) pBuffer = uUbxProtocolUint16Encode(*(const uint16_t *) pValue);
            break;
        case 4:
            *(uint32_t *) pBuffer =  uUbxProtocolUint32Encode(*(const uint32_t *) pValue);
            break;
        case 8:
            *(uint64_t *) pBuffer =  uUbxProtocolUint64Encode(*(const uint64_t *) pValue);
            break;
        default:
            break;
    }
}

// Pack a pCfgItem for a UBX-CFG-VALSET message into the given buffer.
static void packMessage(const uGnssCfgVal_t *pCfgItem, size_t numValues,
                        char *pBuffer, size_t size)
{
    size_t storageSizeBytes;

    for (size_t x = 0; x < numValues; x++) {
        // Store the key ID
        U_ASSERT(size >= sizeof(pCfgItem->keyId));
        *((uint32_t *) pBuffer) = uUbxProtocolUint32Encode(pCfgItem->keyId);
        size -= sizeof(pCfgItem->keyId);
        pBuffer += sizeof(pCfgItem->keyId);
        // Add the value
        storageSizeBytes = getStorageSizeBytes(U_GNSS_CFG_VAL_KEY_GET_SIZE(pCfgItem->keyId));
        U_ASSERT (size >= storageSizeBytes);
        packValue(pBuffer, &(pCfgItem->value), storageSizeBytes);
        size -= storageSizeBytes;
        pBuffer += storageSizeBytes;
        pCfgItem++;
    }
}

// Unpack a configuration item, returning the size of the configuration
// item and the value in pValue; pValue may be NULL to just get the size.
static size_t unpackItem(const char *pCfgData, size_t size, uint64_t *pValue)
{
    size_t itemSize = 0;
    size_t x;
    uint32_t keyId;

    if (size >= sizeof(keyId)) {
        keyId = uUbxProtocolUint32Decode(pCfgData);
        size -= sizeof(keyId);
        pCfgData += sizeof(keyId);
        x = getStorageSizeBytes(U_GNSS_CFG_VAL_KEY_GET_SIZE(keyId));
        if (size >= x) {
            itemSize = x + sizeof(keyId);
        }
    }

    if ((pValue != NULL) && (itemSize > 0)) {
        switch (itemSize - sizeof(keyId)) {
            case 1:
                *pValue = *(const uint8_t *) pCfgData;
                break;
            case 2:
                *pValue = uUbxProtocolUint16Decode(pCfgData);
                break;
            case 4:
                *pValue = uUbxProtocolUint32Decode(pCfgData);
                break;
            case 8:
                *pValue = uUbxProtocolUint64Decode(pCfgData);
                break;
            default:
                break;
        }
    }

    return itemSize;
}

// Unpack a set of UBX-CFG-VALGET responses into *pList, allocating memory
// to do so, and return the number of items unpacked.
static int32_t unpackMessageAlloc(uGnssCfgValGetMessageBody_t *pMessageList,
                                  size_t messageCount,
                                  uGnssCfgVal_t **pList)
{
    int32_t errorCodeOrCount = 0;
    size_t count = 0;
    uGnssCfgValGetMessageBody_t *pMessage;
    int32_t y;
    uGnssCfgVal_t *pCfgItem;
    size_t size;
    const char *pCfgData;

    // Note that this does no error checking since the message
    // this came in will already have been CRC checked

    // First, run through the configuration data in each
    // message to compute the amount of storage required
    pMessage = pMessageList;
    for (size_t x = 0; x < messageCount; x++, pMessage++) {
        pCfgData = pMessage->pBody;
        size =  pMessage->size;
        // Check the version and size of the message
        if ((*pCfgData == 0x01) && (size > 4)) {
            // Skip to the configuration data and parse it
            // to determine the number of items
            pCfgData += 4;
            size -= 4;
            do {
                y = unpackItem(pCfgData, size, NULL);
                if (y > 0) {
                    pMessage->itemCount++;
                    count++;
                }
                pCfgData += y;
                size -= y;
            } while (y > 0);
        }
    }

    if (count > 0) {
        errorCodeOrCount = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        *pList = (uGnssCfgVal_t *) pUPortMalloc(count * sizeof(uGnssCfgVal_t));
        if (*pList != NULL) {
            errorCodeOrCount = (int32_t) count;
            // Now we can populate the array from the messages
            pMessage = pMessageList;
            pCfgItem = *pList;
            for (size_t x = 0; x < messageCount; x++, pMessage++) {
                // Skip to the configuration data
                pCfgData = pMessage->pBody + 4;
                size =  pMessage->size - 4;
                for (size_t z = 0; z < pMessage->itemCount; z++, pCfgItem++) {
                    // Get the key ID
                    pCfgItem->keyId = uUbxProtocolUint32Decode(pCfgData);
                    // Unpack the value
                    y = unpackItem(pCfgData, size, &(pCfgItem->value));
                    pCfgData += y;
                    size -= y;
                }
            }
        }
    }

    return errorCodeOrCount;
}

// Get the current value of a single E1-type or L-type
// configuration item using UBX-CFG-VALGET, used by the likes
// of uGnssCfgGetDynamic(), uGnssCfgGetFixMode(), uGnssCfgGetUtcStandard()
// and uGnssCfgSetAntennaActive().
// Note: gUGnssPrivateMutex must be locked before this is called.
static int32_t valGetByte(uGnssPrivateInstance_t *pInstance,
                          uint32_t keyId)
{
    int32_t errorCodeOrByteValue;
    // Message buffer for the UBX-CFG-VALGET message body:
    // four bytes of header and four bytes for the key ID
    char messageOut[4 + 4] = {0};
    char *pMessageIn = NULL;
    char *pMessage;
    size_t messageInSizeBytes;
    uint32_t y;

    // The 4-byte message header is all zeroes: version 0,
    // 0 for the RAM layer, position 0, so all we have to
    // do is copy in the key Id
    *((uint32_t *) &(messageOut[4])) = uUbxProtocolUint32Encode(keyId); // *NOPAD*
    // Send it off and wait for the response
    errorCodeOrByteValue = uGnssPrivateSendReceiveUbxMessageAlloc(pInstance,
                                                                  0x06, 0x8b,
                                                                  messageOut,
                                                                  sizeof(messageOut),
                                                                  &pMessageIn);
    // 4 below since there must be at least four bytes of header
    if ((errorCodeOrByteValue >= 4) && (pMessageIn != NULL)) {
        messageInSizeBytes = (size_t) errorCodeOrByteValue;
        errorCodeOrByteValue = (int32_t) U_ERROR_COMMON_PLATFORM;
        pMessage = pMessageIn + 4;
        // After a four byte header, which we can ignore,
        // find in the received message our key ID and the E1/L value
        while ((errorCodeOrByteValue < 0) &&
               (messageInSizeBytes - (pMessage - pMessageIn) >= 4 + 1)) {
            y = uUbxProtocolUint32Decode(pMessage);
            pMessage += sizeof(uint32_t);
            if (y == keyId) {
                errorCodeOrByteValue = *pMessage;
            }
            pMessage += getStorageSizeBytes(keyId >> 24);
        }
    }

    // Free memory from uGnssPrivateSendReceiveUbxMessageAlloc()
    uPortFree(pMessageIn);

    return errorCodeOrByteValue;
}

// Set the current value of a single E1-type or L-type
// configuration item using UBX-CFG-VALSET, used by the
// likes of uGnssCfgSetDynamic(), uGnssCfgSetFixMode(),
// uGnssCfgSetUtcStandard() and uGnssCfgSetAntennaActive().
// Note: gUGnssPrivateMutex must be locked before this is called.
static int32_t valSetByte(uGnssPrivateInstance_t *pInstance,
                          uint32_t keyId, uint8_t value)
{
    // Message buffer for the UBX-CFG-VALSET message body:
    // four bytes of header, four bytes for the key ID and
    // one byte for the value
    char message[4 + 4 + 1];

    // Assemble the message
    message[0] = 0; // version
    message[1] = U_GNSS_CFG_VAL_LAYER_RAM;
    message[2] = 0; // reserved
    message[3] = 0;
    // Add the key ID and value
    *((uint32_t *) &(message[4])) = uUbxProtocolUint32Encode(keyId); // *NOPAD*
    message[8] = (char) value;

    // Send the message off
    return uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x8a,
                                      message, sizeof(message));
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO GNSS
 * -------------------------------------------------------------- */

// Get a list of configuration items using VALGET.
int32_t uGnssCfgPrivateValGetListAlloc(uGnssPrivateInstance_t *pInstance,
                                       const uint32_t *pKeyIdList,
                                       size_t numKeyIds,
                                       uGnssCfgVal_t **pList,
                                       uGnssCfgValLayer_t layer)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t encodedLayer = encodeLayerForGet(layer);
    char *pMessageOut = NULL;
    size_t messageOutSize = 4 + (4 * numKeyIds);
    uGnssCfgValGetMessageBody_t messageIn[U_GNSS_CFG_MAX_NUM_VAL_GET_SEGMENTS] = {0};
    size_t messageInCount = 0;

    if ((pInstance != NULL) && (pKeyIdList != NULL) && (numKeyIds > 0) &&
        (pList != NULL) && (encodedLayer >= 0)) {
        errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
            errorCodeOrCount = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Get memory for the body of the UBX-CFG-VALGET message
            pMessageOut = (char *) pUPortMalloc(messageOutSize);
            if (pMessageOut != NULL) {
                // Assemble the message
                *pMessageOut       = 0; // Version
                *(pMessageOut + 1) = (char) encodedLayer;
                // Position is added in the loop below
                for (size_t x = 0; x < numKeyIds; x++) {
                    *((uint32_t *) (pMessageOut + 4 + (x << 2))) = uUbxProtocolUint32Encode(*pKeyIdList);
                    pKeyIdList++;
                }
                do {
                    // Slip in the current position
                    *((uint16_t *) (pMessageOut + 2)) = uUbxProtocolUint16Encode((uint16_t) (messageInCount *
                                                                                             U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES));
                    // Send it off and wait for the response
                    errorCodeOrCount = uGnssPrivateSendReceiveUbxMessageAlloc(pInstance,
                                                                              0x06, 0x8b,
                                                                              pMessageOut,
                                                                              messageOutSize,
                                                                              &(messageIn[messageInCount].pBody));
                    if (errorCodeOrCount >= 0) {
                        messageIn[messageInCount].size = errorCodeOrCount;
                        messageInCount++;
                    }
                    // Repeat until less than 64 responses are returned or we
                    // run out of message buffers
                } while ((messageInCount < sizeof(messageIn) / sizeof (messageIn[0])) &&
                         (errorCodeOrCount >= U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES));

                // Now process all of the messages into an array; note that even if
                // we got an error part way through we still return what we received
                // because we get a NACK to indicate "done", which would appear as
                // an error code
                if (messageInCount > 0) {
                    errorCodeOrCount = unpackMessageAlloc(messageIn, messageInCount, pList);
                    // Free the memory that was allocated by the send/receive calls
                    for (size_t x = 0; x < messageInCount; x++) {
                        uPortFree(messageIn[x].pBody);
                    }
                }

                // Free the memory that was used for the outgoing message
                uPortFree(pMessageOut);
            }
        }
    }

    return errorCodeOrCount;
}

// Set a list of configuration items using VALSET.
int32_t uGnssCfgPrivateValSetList(uGnssPrivateInstance_t *pInstance,
                                  const uGnssCfgVal_t *pList,
                                  size_t numValues,
                                  uGnssCfgValTransaction_t transaction,
                                  int32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char *pMessage = NULL;
    size_t messageSize = 4 + (4 * numValues);

    if ((pInstance != NULL) &&
        ((pList != NULL) || (numValues == 0)) &&
        (numValues <= U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES) &&
        ((numValues == 0) ||
         ((layers > 0) && ((layers & ~U_GNSS_CFG_VAL_LAYER_DEFAULT) == 0)))) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Work out how much memory we need for the message;
            // we already have the overhead and the amount per key ID,
            // need to add the amount per value
            for (size_t x = 0; x < numValues; x++) {
                messageSize += getStorageSizeBytes(U_GNSS_CFG_VAL_KEY_GET_SIZE((pList + x)->keyId));
            }
            // Get memory for the body of the UBX-CFG-VALSET message
            pMessage = (char *) pUPortMalloc(messageSize);
            if (pMessage != NULL) {
                // Assemble the message
                *pMessage       = 0x01; // Version
                *(pMessage + 1) = layers;
                *(pMessage + 2) = transaction;
                *(pMessage + 3) = 0; // Reserved
                // Add the values
                packMessage(pList, numValues, pMessage + 4, messageSize - 4);
                // Send them all off
                errorCode = uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x8a,
                                                       pMessage, messageSize);
                // Free memory
                uPortFree(pMessage);
            }
        }
    }

    return errorCode;
}

// Delete a list of configuration items using VALDEL.
int32_t uGnssCfgPrivateValDelList(uGnssPrivateInstance_t *pInstance,
                                  const uint32_t *pKeyIdList,
                                  size_t numKeyIds,
                                  uGnssCfgValTransaction_t transaction,
                                  uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char *pMessage = NULL;
    int32_t messageSize = 4 + (4 * numKeyIds);
    uint32_t *pUintBuffer;

    if ((pInstance != NULL) &&
        ((pKeyIdList != NULL) || (numKeyIds == 0)) &&
        (numKeyIds <= U_GNSS_CFG_VAL_MSG_MAX_NUM_VALUES) &&
        ((numKeyIds == 0) ||
         ((layers > 0) &&
          ((layers & ~(U_GNSS_CFG_VAL_LAYER_BBRAM | U_GNSS_CFG_VAL_LAYER_FLASH)) == 0)))) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Get memory for the body of the UBX-CFG-VALDEL message
            pMessage = pUPortMalloc(messageSize);
            if (pMessage != NULL) {
                // Assemble the message
                *pMessage       = 0x01; // Version
                *(pMessage + 1) = layers;
                *(pMessage + 2) = transaction;
                *(pMessage + 3) = 0; // Reserved
                // Add the key IDs
                pUintBuffer = (uint32_t *) (pMessage + 4);
                for (size_t x = 0; x < numKeyIds; x++) {
                    *pUintBuffer = uUbxProtocolUint32Encode(*pKeyIdList);
                    pKeyIdList++;
                    pUintBuffer++;
                }
                // Send them all off
                errorCode = uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x8c,
                                                       pMessage, messageSize);
                // Free memory
                uPortFree(pMessage);
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SPECIFIC CONFIGURATION FUNCTIONS
 * -------------------------------------------------------------- */

// Get the rate at which position is obtained.
int32_t uGnssCfgGetRate(uDeviceHandle_t gnssHandle,
                        int32_t *pMeasurementPeriodMs,
                        int32_t *pNavigationCount,
                        uGnssTimeSystem_t *pTimeSystem)
{
    int32_t errorCodeOrRate = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrRate = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrRate = uGnssPrivateGetRate(pInstance,
                                                  pMeasurementPeriodMs,
                                                  pNavigationCount,
                                                  pTimeSystem);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrRate;
}

// Set the rate at which position is obtained.
int32_t uGnssCfgSetRate(uDeviceHandle_t gnssHandle,
                        int32_t measurementPeriodMs,
                        int32_t navigationCount,
                        uGnssTimeSystem_t timeSystem)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGnssPrivateSetRate(pInstance, measurementPeriodMs,
                                            navigationCount, timeSystem);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the rate at which a message ID is emitted.
int32_t uGnssCfgGetMsgRate(uDeviceHandle_t gnssHandle,
                           uGnssMessageId_t *pMessageId)
{
    int32_t errorCodeOrMsgRate = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMessageId_t privateMessageId;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrMsgRate = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pMessageId != NULL) &&
            (uGnssPrivateMessageIdToPrivate(pMessageId, &privateMessageId) == 0)) {
            errorCodeOrMsgRate = uGnssPrivateGetMsgRate(pInstance, &privateMessageId);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrMsgRate;
}

// Set the rate at which a given message ID is emitted.
int32_t uGnssCfgSetMsgRate(uDeviceHandle_t gnssHandle,
                           uGnssMessageId_t *pMessageId,
                           int32_t rate)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMessageId_t privateMessageId;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pMessageId != NULL) &&
            (uGnssPrivateMessageIdToPrivate(pMessageId, &privateMessageId) == 0)) {
            errorCode = uGnssPrivateSetMsgRate(pInstance, &privateMessageId, rate);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the dynamic platform model from the GNSS chip.
int32_t uGnssCfgGetDynamic(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrDynamic = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrDynamic = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrDynamic = valGetByte(pInstance,
                                                U_GNSS_CFG_VAL_KEY_ID_NAVSPG_DYNMODEL_E1);
            } else {
                // The dynamic platform model is at offset 2
                errorCodeOrDynamic = getUbxCfgNav5(pInstance, 2);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrDynamic;
}

// Set the dynamic platform model of the GNSS chip.
int32_t uGnssCfgSetDynamic(uDeviceHandle_t gnssHandle, uGnssDynamic_t dynamic)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = valSetByte(pInstance,
                                       U_GNSS_CFG_VAL_KEY_ID_NAVSPG_DYNMODEL_E1,
                                       (uint8_t) dynamic);
            } else {
                // Set the dynamic model with the right mask and offset
                errorCode = setUbxCfgNav5(pInstance, 0x01, 2, (uint8_t) dynamic);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the fix mode from the GNSS chip.
int32_t uGnssCfgGetFixMode(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrFixMode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrFixMode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrFixMode = valGetByte(pInstance,
                                                U_GNSS_CFG_VAL_KEY_ID_NAVSPG_FIXMODE_E1);
            } else {
                // The fix mode is at offset 3
                errorCodeOrFixMode = getUbxCfgNav5(pInstance, 3);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrFixMode;
}

// Set the fix mode of the GNSS chip.
int32_t uGnssCfgSetFixMode(uDeviceHandle_t gnssHandle, uGnssFixMode_t fixMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = valSetByte(pInstance,
                                       U_GNSS_CFG_VAL_KEY_ID_NAVSPG_FIXMODE_E1,
                                       (uint8_t) fixMode);
            } else {
                // Set the fix mode with the right mask and offset
                errorCode = setUbxCfgNav5(pInstance, 0x04, 3, (uint8_t) fixMode);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the UTC standard from the GNSS chip.
int32_t uGnssCfgGetUtcStandard(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrUtcStandard = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrUtcStandard = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrUtcStandard = valGetByte(pInstance,
                                                    U_GNSS_CFG_VAL_KEY_ID_NAVSPG_UTCSTANDARD_E1);
            } else {
                // The UTC standard is at offset 30
                errorCodeOrUtcStandard = getUbxCfgNav5(pInstance, 30);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrUtcStandard;
}

// Set the UTC standard of the GNSS chip.
int32_t uGnssCfgSetUtcStandard(uDeviceHandle_t gnssHandle,
                               uGnssUtcStandard_t utcStandard)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = valSetByte(pInstance,
                                       U_GNSS_CFG_VAL_KEY_ID_NAVSPG_UTCSTANDARD_E1,
                                       (uint8_t) utcStandard);
            } else {
                // Set the UTC standard with the right mask and offset
                errorCode = setUbxCfgNav5(pInstance, 0x0400, 30, (uint8_t) utcStandard);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the protocol types output by the GNSS chip.
int32_t uGnssCfgGetProtocolOut(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrBitMap = uGnssPrivateGetProtocolOut(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrBitMap;
}

// Set the protocol type output by the GNSS chip.
int32_t uGnssCfgSetProtocolOut(uDeviceHandle_t gnssHandle,
                               uGnssProtocol_t protocol,
                               bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGnssPrivateSetProtocolOut(pInstance,
                                                   protocol,
                                                   onNotOff);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get whether the antenna has active power or not.
int32_t uGnssCfgGetAntennaActive(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrActive = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrActive = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCodeOrActive = valGetByte(pInstance,
                                               U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_VOLTCTRL_L);
            } else {
                // Get the antenna active bit (svcs) with UBX-CFG-ANT
                char message[4] = {0};
                uint16_t value;

                errorCodeOrActive = (int32_t) U_ERROR_COMMON_PLATFORM;
                // Poll with the message class and ID of UBX-CFG-ANT
                if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                      0x06, 0x13,
                                                      NULL, 0,
                                                      message,
                                                      sizeof(message)) == sizeof(message)) {
                    // svcs is bit 0 of the first two bytes
                    value = uUbxProtocolUint16Decode(message);
                    errorCodeOrActive = ((value & 0x0001) != 0);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrActive;
}

// Set whether the antenna has active power or not.
int32_t uGnssCfgSetAntennaActive(uDeviceHandle_t gnssHandle, bool active)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            if (U_GNSS_PRIVATE_HAS(pInstance->pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                errorCode = valSetByte(pInstance,
                                       U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_VOLTCTRL_L,
                                       (uint8_t) active);
            } else {
                // Set the antenna active bit (svcs) with UBX-CFG-ANT
                char message[4] = {0};
                if (active) {
                    // svcs is bit 0 of the first two bytes
                    *((uint16_t *) message) = uUbxProtocolUint16Encode(0x0001);
                }
                // Send the UBX-CFG-ANT message
                errorCode = uGnssPrivateSendUbxMessage(pInstance,
                                                       0x06, 0x13,
                                                       message,
                                                       sizeof(message));
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: GENERIC CONFIGURATION USING VALGET/VALSET/VALDEL
 * -------------------------------------------------------------- */

// Get the value of a single configuration item.
int32_t uGnssCfgValGet(uDeviceHandle_t gnssHandle, uint32_t keyId,
                       void *pValue, size_t size,
                       uGnssCfgValLayer_t layer)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t *pList = NULL;
    size_t storageSizeBytes;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) &&
            (U_GNSS_CFG_VAL_KEY_GET_GROUP_ID(keyId) != U_GNSS_CFG_VAL_KEY_GROUP_ID_ALL) &&
            (U_GNSS_CFG_VAL_KEY_GET_ITEM_ID(keyId) != U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL) &&
            ((pValue != NULL) || (size == 0))) {
            errorCodeOrCount = uGnssCfgPrivateValGetListAlloc(pInstance,
                                                              &keyId, 1,
                                                              &pList,
                                                              layer);
            if (errorCodeOrCount > 0) {
                errorCodeOrCount = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                storageSizeBytes = getStorageSizeBytes(U_GNSS_CFG_VAL_KEY_GET_SIZE(keyId));
                if ((pValue == NULL) || (size >= storageSizeBytes)) {
                    errorCodeOrCount = (int32_t) U_ERROR_COMMON_SUCCESS;
                    if (pValue != NULL) {
                        memcpy(pValue, &(pList->value), storageSizeBytes);
                    }
                }
            }
            // Free memory
            uPortFree(pList);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrCount;
}

// Get the value of a configuration item.
int32_t uGnssCfgValGetAlloc(uDeviceHandle_t gnssHandle, uint32_t keyId,
                            uGnssCfgVal_t **pList,
                            uGnssCfgValLayer_t layer)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrCount = uGnssCfgPrivateValGetListAlloc(pInstance,
                                                              &keyId, 1,
                                                              pList,
                                                              layer);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrCount;
}

// Get the value of a list of configuration items.
int32_t uGnssCfgValGetListAlloc(uDeviceHandle_t gnssHandle,
                                const uint32_t *pKeyIdList, size_t numKeyIds,
                                uGnssCfgVal_t **pList,
                                uGnssCfgValLayer_t layer)
{
    int32_t errorCodeOrCount = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrCount = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrCount = uGnssCfgPrivateValGetListAlloc(pInstance,
                                                              pKeyIdList,
                                                              numKeyIds,
                                                              pList,
                                                              layer);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrCount;
}

// Set the value of a configuration item.
int32_t uGnssCfgValSet(uDeviceHandle_t gnssHandle,
                       uint32_t keyId, uint64_t value,
                       uGnssCfgValTransaction_t transaction,
                       uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssCfgVal_t val;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            val.keyId = keyId;
            val.value = value;
            errorCode = uGnssCfgPrivateValSetList(pInstance,
                                                  &val, 1,
                                                  transaction,
                                                  layers);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Set the value of several configuration items at once.
int32_t uGnssCfgValSetList(uDeviceHandle_t gnssHandle,
                           const uGnssCfgVal_t *pList, size_t numValues,
                           uGnssCfgValTransaction_t transaction,
                           uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGnssCfgPrivateValSetList(pInstance,
                                                  pList,
                                                  numValues,
                                                  transaction,
                                                  layers);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Delete a configuration item.
int32_t uGnssCfgValDel(uDeviceHandle_t gnssHandle, uint32_t keyId,
                       uGnssCfgValTransaction_t transaction,
                       uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGnssCfgPrivateValDelList(pInstance,
                                                  &keyId, 1,
                                                  transaction,
                                                  layers);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Delete several configuration items at once.
int32_t uGnssCfgValDelList(uDeviceHandle_t gnssHandle,
                           const uint32_t *pKeyIdList, size_t numKeyIds,
                           uGnssCfgValTransaction_t transaction,
                           uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = uGnssCfgPrivateValDelList(pInstance,
                                                  pKeyIdList,
                                                  numKeyIds,
                                                  transaction,
                                                  layers);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// As uGnssCfgValDelList() but takes an array of type uGnssCfgVal_t.
int32_t uGnssCfgValDelListX(uDeviceHandle_t gnssHandle,
                            const uGnssCfgVal_t *pList, size_t numValues,
                            uGnssCfgValTransaction_t transaction,
                            uint32_t layers)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t *pKeyIdList = NULL;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if ((numValues > 0) && (pList != NULL)) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pKeyIdList = pUPortMalloc(sizeof(uint32_t) * numValues);
                if (pKeyIdList != NULL) {
                    for (size_t x = 0; x < numValues; x++) {
                        *(pKeyIdList + x) = pList->keyId;
                        pList++;
                    }
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }

            if (errorCode == 0) {
                errorCode = uGnssCfgPrivateValDelList(pInstance,
                                                      pKeyIdList,
                                                      numValues,
                                                      transaction,
                                                      layers);
                uPortFree(pKeyIdList);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// End of file
