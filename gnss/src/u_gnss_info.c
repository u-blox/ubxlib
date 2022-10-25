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

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
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

/** The amount of space required to store the body of a
 * UBX-MON-COMMS message (see uGnssInfoGetCommunicationStats())
 * with max port numbers.
 */
#define U_GNSS_INFO_MESSAGE_BODY_LENGTH_UBX_MON_COMMS (8 + (40 * U_GNSS_PORT_MAX_NUM))

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

        errorCodeOrTime = (int64_t) U_ERROR_COMMON_INVALID_PARAMETER;
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

// Get the communication stats as seen by the GNSS chip.
int32_t uGnssInfoGetCommunicationStats(uDeviceHandle_t gnssHandle,
                                       int32_t port,
                                       uGnssCommunicationStats_t *pStats)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    char *pMessage;
    int32_t messageLength;
    int32_t numPorts = -1;
    int32_t protocolId;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            // TODO: fix this properly with versioned UBX messaging later
            if (pInstance->pModule->moduleType >= U_GNSS_MODULE_TYPE_M9) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Message big enough to store UBX-MON-COMMS with max port numbers
                pMessage = (char *) pUPortMalloc(U_GNSS_INFO_MESSAGE_BODY_LENGTH_UBX_MON_COMMS);
                if (pMessage != NULL) {
                    if (port < 0) {
                        port = (int32_t) pInstance->portNumber;
                    }
                    // Note: the if() condition below is present to allow future
                    // values, or new and interesting values, to be passed transparently
                    // to this function.
                    if (port < U_GNSS_PORT_MAX_NUM) {
                        // The encoding of the port number in this message is _different_
                        // to that in UBX-CFG-PORT - here it is, adopting the form used
                        // in the system integration manuals, which is AFTER endian
                        // conversion:
                        //
                        // 0 ==> 0x0000 I2C
                        // 1 ==> 0x0100 UART1
                        // 2 ==> 0x0201 UART2
                        // 3 ==> 0x0300 USB
                        // 4 ==> 0x0400 SPI
                        //
                        // This is because there are additional UARTs internal to the
                        // GNSS device which need to be accounted for.  The ones listed
                        // above are those that may be connected to a host MCU, but note
                        // that others (e.g. 0x0101) may appear in the output of
                        // UBX-MON-COMMS, which we will ignore.
                        port = ((uint32_t) port) << 8;
                        if (port == (((uint32_t) U_GNSS_PORT_UART2) << 8)) {
                            port++;
                        }
                    }
                    // Poll with the message class and ID of the UBX-MON-COMMS command
                    errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                                  0x0a, 0x36,
                                                                  NULL, 0, pMessage,
                                                                  U_GNSS_INFO_MESSAGE_BODY_LENGTH_UBX_MON_COMMS);
                    if (errorCode >= 0) {
                        messageLength = errorCode;
                        if ((messageLength >= 2) && (*pMessage == 0)) {
                            // Have a message in a version we understand;
                            // get the number of ports reported in it
                            numPorts = *(pMessage + 1);
                        }
                        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                        if ((numPorts > 0) && (messageLength >= 8 + (numPorts * 40))) {
                            // The message has some ports in it and is of the correct
                            // length for that number of ports; run through the
                            // message in blocks of 40 bytes, the length of the
                            // report for one port being 40 bytes, after the initial
                            // 8 bytes, to find the report for our port number
                            for (int32_t offset = 0; (8 + offset < messageLength) &&
                                 (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS); offset += 40) {
                                // No endian conversion here as port is already endian converted
                                if (*(uint16_t *) (pMessage + 8 + offset) == port) {
                                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                    if (pStats != NULL) {
                                        pStats->txPendingBytes = uUbxProtocolUint16Decode(pMessage + 10 + offset);
                                        pStats->txBytes = uUbxProtocolUint32Decode(pMessage + 12 + offset);
                                        pStats->txPercentageUsage = *(pMessage + 16 + offset);
                                        pStats->txPeakPercentageUsage = *(pMessage + 17 + offset);
                                        pStats->rxPendingBytes = uUbxProtocolUint16Decode(pMessage + 18 + offset);
                                        pStats->rxBytes = uUbxProtocolUint32Decode(pMessage + 20 + offset);
                                        pStats->rxPercentageUsage = *(pMessage + 24 + offset);
                                        pStats->rxPeakPercentageUsage = *(pMessage + 25 + offset);
                                        pStats->rxOverrunErrors = uUbxProtocolUint16Decode(pMessage + 26 + offset);
                                        // The number of messages parsed is in the array which follows
                                        // based on the array of protocol IDs way back at the start
                                        // of the message in byte 4
                                        for (size_t x = 0; x < sizeof(pStats->rxNumMessages) / sizeof(pStats->rxNumMessages[0]); x++) {
                                            pStats->rxNumMessages[x] = -1;
                                        }
                                        for (size_t x = 0; x < 4; x++) {
                                            protocolId = *(pMessage + 4 + x);
                                            if ((protocolId >= 0) &&
                                                (protocolId < sizeof(pStats->rxNumMessages) / sizeof(pStats->rxNumMessages[0]))) {
                                                pStats->rxNumMessages[protocolId] = uUbxProtocolUint16Decode(pMessage + 28 + offset + (x * 2));
                                            }
                                        }
                                        pStats->rxSkippedBytes = uUbxProtocolUint32Decode(pMessage + 44 + offset);
                                    }
                                }
                            }
                        }
                    }

                    // Free memory
                    uPortFree(pMessage);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}


// End of file
