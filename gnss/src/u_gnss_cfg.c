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
 * @brief Implementation of the configuration API for GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"  // Required by u_gnss_private.h

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_cfg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the contents of UBX-CFG-NAV5.
// pBuffer must point to a buffer of length 36 bytes.
static int32_t uGnssCfgGetUbxCfgNav5(uDeviceHandle_t gnssHandle,
                                     char *pBuffer)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            // Poll with the message class and ID of the
            // UBX-CFG-NAV5 message
            if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                  0x06, 0x24,
                                                  NULL, 0,
                                                  pBuffer, 36) == 36) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Set the contents of UBX-CFG-NAV5.
static int32_t uGnssCfgSetUbxCfgNav5(uDeviceHandle_t gnssHandle,
                                     uint16_t mask,
                                     const char *pBuffer,
                                     size_t size,
                                     size_t offset)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Set the mask bytes at the start of the message
            *((uint16_t *) message) = uUbxProtocolUint16Encode(mask);
            // Copy in the contents, which must have already
            // been correctly encoded
            memcpy(message + offset, pBuffer, size);
            // Send the UBX-CFG-NAV5 message
            errorCode = uGnssPrivateSendUbxMessage(pInstance,
                                                   0x06, 0x24,
                                                   message,
                                                   sizeof(message));
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the dynamic platform model from the GNSS chip.
int32_t uGnssCfgGetDynamic(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrDynamic;
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36];

    errorCodeOrDynamic = uGnssCfgGetUbxCfgNav5(gnssHandle, message);
    if (errorCodeOrDynamic == 0) {
        // The dynamic platform model is at offset 2
        errorCodeOrDynamic = message[2];
    }

    return errorCodeOrDynamic;
}

// Set the dynamic platform model of the GNSS chip.
int32_t uGnssCfgSetDynamic(uDeviceHandle_t gnssHandle, uGnssDynamic_t dynamic)
{
    return uGnssCfgSetUbxCfgNav5(gnssHandle,
                                 0x01, /* Mask for dynamic model */
                                 (char *) &dynamic,
                                 1, 2 /* One byte at offset 2 */);
}

// Get the fix mode from the GNSS chip.
int32_t uGnssCfgGetFixMode(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrFixMode;
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36];

    errorCodeOrFixMode = uGnssCfgGetUbxCfgNav5(gnssHandle, message);
    if (errorCodeOrFixMode == 0) {
        // The fix mode is at offset 3
        errorCodeOrFixMode = message[3];
    }

    return errorCodeOrFixMode;
}

// Set the fix mode of the GNSS chip.
int32_t uGnssCfgSetFixMode(uDeviceHandle_t gnssHandle, uGnssFixMode_t fixMode)
{
    return uGnssCfgSetUbxCfgNav5(gnssHandle,
                                 0x04, /* Mask for fix mode */
                                 (char *) &fixMode,
                                 1, 3 /* One byte at offset 3 */);
}

// Get the UTC standard from the GNSS chip.
int32_t uGnssCfgGetUtcStandard(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrUtcStandard;
    // Enough room for the body of the UBX-CFG-NAV5 message
    char message[36];

    errorCodeOrUtcStandard = uGnssCfgGetUbxCfgNav5(gnssHandle, message);
    if (errorCodeOrUtcStandard == 0) {
        // The UTC standard is at offset 30
        errorCodeOrUtcStandard = message[30];
    }

    return errorCodeOrUtcStandard;
}

// Set the UTC standard of the GNSS chip.
int32_t uGnssCfgSetUtcStandard(uDeviceHandle_t gnssHandle,
                               uGnssUtcStandard_t utcStandard)
{
    return uGnssCfgSetUbxCfgNav5(gnssHandle,
                                 0x0400, /* Mask for UTC standard */
                                 (char *) &utcStandard,
                                 1, 30 /* One byte at offset 30 */);
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

// End of file
