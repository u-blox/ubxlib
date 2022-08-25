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
 * @brief Implementation of the API to read general information from
 * a GNSS chip; for position information please see the u_gnss_pos.h
 * API instead.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc()/free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"  // Required by u_gnss_private.h
#include "u_port_debug.h"

#include "u_time.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_info.h"

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the version string from the GNSS chip.
int32_t uGnssInfoGetFirmwareVersionStr(uDeviceHandle_t gnssHandle,
                                       char *pStr, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Poll with the message class and ID of the UBX-MON-VER
            // message and pass the message body directly back
            errorCodeOrLength = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                  0x0a, 0x04,
                                                                  NULL, 0,
                                                                  pStr, size);
            // Add a terminator
            if ((errorCodeOrLength > 0) && (size > 0)) {
                if (errorCodeOrLength == size) {
                    errorCodeOrLength--;
                }
                *(pStr + errorCodeOrLength) = 0;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

int32_t uGnssInfoGetVersions(uDeviceHandle_t gnssHandle,
                             uGnssVersionType_t *pVer)
{
    int32_t errorCodeOrLength = U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrLength = U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (NULL != pVer)) {
            // Poll with the message class and ID of the UBX-MON-VER
            // message and pass the message body directly back
            struct {
                char sw[30];
                char hw[10];
                char ext[10][30];
            } message;
            errorCodeOrLength = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                  0x0a, 0x04,
                                                                  NULL, 0,
                                                                  (char *)&message, sizeof(message));
            // Add a terminator
            if (errorCodeOrLength > sizeof(message.sw) + sizeof(message.hw)) {
                memset(pVer, 0, sizeof(*pVer));
                strncpy(pVer->ver, message.sw, sizeof(pVer->ver));
                strncpy(pVer->hw, message.hw, sizeof(pVer->hw));
                size_t n = (errorCodeOrLength - sizeof(message.sw) + sizeof(message.hw)) / sizeof(message.ext[0]);
                for (size_t i = 0; i < n; i++) {
                    if (0 == strncmp(message.ext[i], "ROM BASE ", 9)) {
                        strncpy(pVer->rom, message.ext[i] + 9, sizeof(pVer->rom));
                    } else if (0 == strncmp(message.ext[i], "FWVER=", 6)) {
                        strncpy(pVer->fw, message.ext[i] + 6, sizeof(pVer->fw));
                    } else if (0 == strncmp(message.ext[i], "PROTVER=", 8)) {
                        strncpy(pVer->prot, message.ext[i] + 8, sizeof(pVer->prot));
                    } else if (0 == strncmp(message.ext[i], "MOD=", 4)) {
                        strncpy(pVer->mod, message.ext[i] + 4, sizeof(pVer->mod));
                    }
                }
                errorCodeOrLength = U_ERROR_COMMON_SUCCESS;
            } else if (errorCodeOrLength >= 0) {
                errorCodeOrLength = U_ERROR_COMMON_NOT_RESPONDING;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}


// Get the chip ID from the GNSS chip.
int32_t uGnssInfoGetIdStr(uDeviceHandle_t gnssHandle,
                          char *pStr, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Enough room for the body of the UBX-SEC-UNIQID message
    char message[9];

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Poll with the message class and ID of the UBX-SEC-UNIQID command
            errorCodeOrLength = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                  0x27, 0x03,
                                                                  NULL, 0, message,
                                                                  sizeof(message));
            if (errorCodeOrLength >= (int32_t) sizeof(message)) {
                // The first byte of the first uint32_t should indicate version 1
                if ((uUbxProtocolUint32Decode(message) & 0xFF) == 1) {
                    // The remaining bytes are the chip ID
                    errorCodeOrLength -= 4;
                    // Copy them in and add a terminator
                    if (size > 0) {
                        size--;
                        if (errorCodeOrLength > (int32_t) size) {
                            errorCodeOrLength = (int32_t) size;
                        }
                        if (pStr != NULL) {
                            memcpy(pStr, message + 4, errorCodeOrLength);
                            *(pStr + errorCodeOrLength) = 0;
                        }
                    } else {
                        errorCodeOrLength = 0;
                    }
                } else {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

// Get the UTC time according to GNSS.
int64_t uGnssInfoGetTimeUtc(uDeviceHandle_t gnssHandle)
{
    int64_t errorCodeOrTime = (int64_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Enough room for the body of the UBX-NAV-TIMEUTC message
    char message[20];
    int32_t months;
    int32_t year;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Poll with the message class and ID of the UBX-NAV-TIMEUTC command
            errorCodeOrTime = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                0x01, 0x21,
                                                                NULL, 0, message,
                                                                sizeof(message));
            if (errorCodeOrTime >= (int64_t) sizeof(message)) {
                // Check the validity flag
                errorCodeOrTime = (int64_t) U_ERROR_COMMON_UNKNOWN;
                if (message[19] & 0x04) {
                    errorCodeOrTime = 0;
                    // Year is 1999-2099, so need to adjust to get year since 1970
                    year = (uUbxProtocolUint16Decode(message + 12) - 1999) + 29;
                    // Month (1 to 12), so take away 1 to make it zero-based
                    months = message[14] - 1;
                    months += year * 12;
                    // Work out the number of seconds due to the year/month count
                    errorCodeOrTime += uTimeMonthsToSecondsUtc(months);
                    // Day (1 to 31)
                    errorCodeOrTime += ((int32_t) message[15] - 1) * 3600 * 24;
                    // Hour (0 to 23)
                    errorCodeOrTime += ((int32_t) message[16]) * 3600;
                    // Minute (0 to 59)
                    errorCodeOrTime += ((int32_t) message[17]) * 60;
                    // Second (0 to 60)
                    errorCodeOrTime += message[18];

                    uPortLog("U_GNSS_POS: UTC time is %d.\n", (int32_t) errorCodeOrTime);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrTime;
}

// End of file
