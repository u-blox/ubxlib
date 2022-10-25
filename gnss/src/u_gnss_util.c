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
 * @brief This source file contains the implementation of the utility
 * functions of the GNSS API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_os_platform_specific.h"
#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"  // Required by u_gnss_private.h
#include "u_port_debug.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_hex_bin_convert.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"
#include "u_gnss_util.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_UTIL_TRANSPARENT_RECEIVE_DELAY_MS
/** The amount of time to wait between lines received from the GNSS
 * chip to ensure we don't lose any of a transparent message.
 */
# define U_GNSS_UTIL_TRANSPARENT_RECEIVE_DELAY_MS 500
#endif

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

// Transparently send a command to the GNSS chip.
int32_t uGnssUtilUbxTransparentSendReceive(uDeviceHandle_t gnssHandle,
                                           const char *pCommand,
                                           size_t commandLengthBytes,
                                           char *pResponse,
                                           size_t maxResponseLengthBytes)
{
    int32_t errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    int32_t streamType;
    int32_t streamHandle = -1;
    int32_t startTimeMs;
    int32_t x = 0;
    int32_t bytesRead = 0;
    char *pBuffer;
    size_t bufferLengthBytes;
    char *pTmp = NULL;
    uAtClientHandle_t atHandle;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) &&
            (((pCommand == NULL) && (commandLengthBytes == 0)) ||
             (commandLengthBytes > 0)) &&
            (((pResponse == NULL) && (maxResponseLengthBytes == 0)) ||
             (maxResponseLengthBytes > 0))) {

            errorCodeOrResponseLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
            streamType = uGnssPrivateGetStreamType(pInstance->transportType);

            U_PORT_MUTEX_LOCK(pInstance->transportMutex);

            switch (streamType) {
                case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                    streamHandle = pInstance->transportHandle.uart;
                    break;
                case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                    streamHandle = pInstance->transportHandle.i2c;
                    break;
                default:
                    break;
            }

            if (streamHandle >= 0) {
                // Streaming transport
                switch (streamType) {
                    case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                        errorCodeOrResponseLength = uPortUartWrite(streamHandle,
                                                                   pCommand,
                                                                   commandLengthBytes);
                        break;
                    case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                        errorCodeOrResponseLength = uPortI2cControllerSend(streamHandle, pInstance->i2cAddress,
                                                                           pCommand, commandLengthBytes, false);
                        if (errorCodeOrResponseLength == 0) {
                            errorCodeOrResponseLength = commandLengthBytes;
                        }
                        break;
                    default:
                        break;
                }
                if (errorCodeOrResponseLength == commandLengthBytes) {
                    if (pInstance->printUbxMessages) {
                        uPortLog("U_GNSS: sent command");
                        uGnssPrivatePrintBuffer(pCommand, commandLengthBytes);
                        uPortLog(".\n");
                    }
                    errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_SUCCESS;
                    if (pResponse != NULL) {
                        errorCodeOrResponseLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
                        startTimeMs = uPortGetTickTimeMs();
                        // Wait for something to start coming back
                        while ((bytesRead < (int32_t) maxResponseLengthBytes) &&
                               ((x = uGnssPrivateStreamGetReceiveSize(streamHandle, streamType,
                                                                      pInstance->i2cAddress)) <= 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < pInstance->timeoutMs)) {
                            // Relax a little
                            uPortTaskBlock(U_GNSS_UTIL_TRANSPARENT_RECEIVE_DELAY_MS);
                        }
                        if (x > 0) {
                            // Got something; continue receiving until nothing arrives for
                            // U_GNSS_UTIL_TRANSPARENT_RECEIVE_DELAY_MS
                            while ((bytesRead < (int32_t) maxResponseLengthBytes) &&
                                   ((x = uGnssPrivateStreamGetReceiveSize(streamHandle, streamType,
                                                                          pInstance->i2cAddress)) > 0) &&
                                   (uPortGetTickTimeMs() - startTimeMs < pInstance->timeoutMs)) {
                                if (x > 0) {
                                    if (x > ((int32_t) maxResponseLengthBytes) - bytesRead) {
                                        x = maxResponseLengthBytes - bytesRead;
                                    }
                                    // Read the response into pResponse
                                    switch (streamType) {
                                        case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                                            x = uPortUartRead(streamHandle, pResponse + bytesRead, x);
                                            break;
                                        case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                                            x = uPortI2cControllerSendReceive(streamHandle, pInstance->i2cAddress,
                                                                              NULL, 0, pResponse + bytesRead, x);
                                            break;
                                        default:
                                            break;
                                    }
                                    if (x > 0) {
                                        bytesRead += x;
                                    }
                                } else {
                                    // Relax a little
                                    uPortTaskBlock(U_GNSS_UTIL_TRANSPARENT_RECEIVE_DELAY_MS);
                                }
                            }
                            if (bytesRead > 0) {
                                errorCodeOrResponseLength = bytesRead;
                            }
                        }
                        if (pInstance->printUbxMessages &&
                            (errorCodeOrResponseLength >= 0)) {
                            uPortLog("U_GNSS: received response");
                            uGnssPrivatePrintBuffer(pResponse, errorCodeOrResponseLength);
                            uPortLog(".\n");
                        }
                    }
                }
            } else {
                // AT transport
                errorCodeOrResponseLength = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pTmp = NULL;
                atHandle = pInstance->transportHandle.pAt;
                // Need a buffer to hex encode the message into
                // and receive the hex-encoded response into
                bufferLengthBytes = (commandLengthBytes * 2) + 1; // +1 for terminator
                if (bufferLengthBytes < (maxResponseLengthBytes * 2) + 1) {
                    bufferLengthBytes = (maxResponseLengthBytes * 2) + 1;
                }
                pBuffer = (char *) pUPortMalloc(bufferLengthBytes);
                if (pBuffer != NULL) {
                    errorCodeOrResponseLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
                    if (pCommand != NULL) {
                        x = (int32_t) uBinToHex(pCommand, commandLengthBytes, pBuffer);
                    }
                    // Add terminator
                    *(pBuffer + x) = 0;
                    uAtClientLock(atHandle);
                    uAtClientTimeoutSet(atHandle, pInstance->timeoutMs);
                    // Send the command
                    uAtClientCommandStart(atHandle, "AT+UGUBX=");
                    uAtClientWriteString(atHandle, pBuffer, true);
                    uAtClientCommandStop(atHandle);
                    // Read the hex-coded response back into pTmp,
                    // which may be NULL to throw the response away
                    uAtClientResponseStart(atHandle, "+UGUBX:");
                    if (pResponse != NULL) {
                        pTmp = pBuffer;
                    }
                    bytesRead = uAtClientReadString(atHandle, pTmp,
                                                    bufferLengthBytes, false);
                    uAtClientResponseStop(atHandle);
                    if ((uAtClientUnlock(atHandle) == 0) && (bytesRead >= 0)) {
                        if (bytesRead > (int32_t) maxResponseLengthBytes) {
                            bytesRead = (int32_t) maxResponseLengthBytes;
                        }
                        errorCodeOrResponseLength = 0;
                        if (pTmp != NULL) {
                            // Decode the hex into pResponse
                            errorCodeOrResponseLength = (int32_t) uHexToBin(pTmp, bytesRead, pResponse);
                            if (pInstance->printUbxMessages) {
                                uPortLog("U_GNSS: received response");
                                uGnssPrivatePrintBuffer(pResponse, errorCodeOrResponseLength);
                                uPortLog(".\n");
                            }
                        }
                    }
                    // Free memory
                    uPortFree(pBuffer);
                }
            }

            U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrResponseLength;
}

// End of file
