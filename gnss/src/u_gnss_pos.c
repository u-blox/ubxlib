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
 * @brief Implementation of the GNSS APIs to read position.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_time.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_private.h"
#include "u_gnss_msg.h"
#include "u_gnss_msg_private.h"
#include "u_gnss_pos.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POS_CALLBACK_TASK_STACK_SIZE_BYTES
/** The stack size for the position establishment task.  The limiting
 * factor is ESP-IDF, and in particular on Arduino, which seems to
 * require the most stack, and if power saving may be on then
 * additional stack will be used by the AT client.
 */
# define U_GNSS_POS_CALLBACK_TASK_STACK_SIZE_BYTES (1024 * 5)
#endif

#ifndef U_GNSS_POS_CALLBACK_TASK_PRIORITY
/** The task priority for the position establishment task.
 */
# define U_GNSS_POS_CALLBACK_TASK_PRIORITY (U_CFG_OS_PRIORITY_MIN + 2)
#endif

#ifndef U_GNSS_POS_CALLBACK_TASK_STACK_DELAY_SECONDS
/** The delay between position attempts in the asynchronous task.
 */
# define U_GNSS_POS_CALLBACK_TASK_STACK_DELAY_SECONDS 5
#endif

#ifndef U_GNSS_POS_RRLP_HEADER_SIZE_BYTES
/** The number of bytes of UBX protocol header that
 * will be added to the front of the raw RRLP binary data.
 */
#define U_GNSS_POS_RRLP_HEADER_SIZE_BYTES (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES - 2)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Parameters to pass to the asynchronous position establishment task.
 */
typedef struct {
    uDeviceHandle_t gnssHandle;
    uGnssPrivateInstance_t *pInstance;
    void (*pCallback) (uDeviceHandle_t gnssHandle,
                       int32_t errorCode,
                       int32_t latitudeX1e7,
                       int32_t longitudeX1e7,
                       int32_t altitudeMillimetres,
                       int32_t radiusMillimetres,
                       int32_t speedMillimetresPerSecond,
                       int32_t svs,
                       int64_t timeUtc);
} uGnssPosGetTaskParameters_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Table to convert U_GNSS_RRLP_MODE_MEASxxx into a message class
 * of UBX-RXM-MEASxxx.
 */
int32_t gRrlpModeToUbxRxmMessageClass[] = {
    0x14, // UBX-RXM-MEASX
    0x86, // UBX_RXM_MEAS50
    0x84, // UBX_RXM_MEAS20
    0x82, // UBX_RXM_MEASC12
    0x80  // UBX_RXM_MEASD12
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Decode the contents of a UBX-NAV-PVT message.  pMessage must
// be a pointer to the 92 byte body of a UBX-NAV-PVT message.
static int32_t posDecode(char *pMessage,
                         int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                         int32_t *pAltitudeMillimetres,
                         int32_t *pRadiusMillimetres,
                         int32_t *pSpeedMillimetresPerSecond,
                         int32_t *pSvs, int64_t *pTimeUtc, bool printIt)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
    int32_t months;
    int32_t year;
    int32_t y;
    int64_t t = -1;

    if ((*(pMessage + 11) & 0x03) == 0x03) {
        // Time and date are valid; we don't indicate
        // success based on this but we report it anyway
        // if it is valid
        t = 0;
        // Year is 1999-2099, so need to adjust to get year since 1970
        year = ((int32_t) uUbxProtocolUint16Decode(pMessage + 4) - 1999) + 29;
        // Month (1 to 12), so take away 1 to make it zero-based
        months = *(pMessage + 6) - 1;
        months += year * 12;
        // Work out the number of seconds due to the year/month count
        t += uTimeMonthsToSecondsUtc(months);
        // Day (1 to 31)
        t += ((int32_t) * (pMessage + 7) - 1) * 3600 * 24;
        // Hour (0 to 23)
        t += ((int32_t) * (pMessage + 8)) * 3600;
        // Minute (0 to 59)
        t += ((int32_t) * (pMessage + 9)) * 60;
        // Second (0 to 60)
        t += *(pMessage + 10);
        if (printIt) {
            uPortLog("U_GNSS_POS: UTC time = %d.\n", (int32_t) t);
        }
    }
    if (pTimeUtc != NULL) {
        *pTimeUtc = t;
    }
    // From here onwards Lint complains about accesses
    // into message[] and it doesn't seem to be possible
    // to suppress those warnings with -esym(690, message)
    // or even -e(690), hence do it the blunt way
    //lint -save -e690
    if ((t >= 0) && (*(pMessage + 21) & 0x01)) {
        if (printIt) {
            uPortLog("U_GNSS_POS: %dD fix achieved.\n", *(pMessage + 20));
        }
        y = (int32_t) * (pMessage + 23);
        if (printIt) {
            uPortLog("U_GNSS_POS: satellite(s) = %d.\n", y);
        }
        if (pSvs != NULL) {
            *pSvs = y;
        }
        y = (int32_t) uUbxProtocolUint32Decode(pMessage + 24);
        if (printIt) {
            uPortLog("U_GNSS_POS: longitude = %d (degrees * 10^7).\n", y);
        }
        if (pLongitudeX1e7 != NULL) {
            *pLongitudeX1e7 = y;
        }
        y = (int32_t) uUbxProtocolUint32Decode(pMessage + 28);
        if (printIt) {
            uPortLog("U_GNSS_POS: latitude = %d (degrees * 10^7).\n", y);
        }
        if (pLatitudeX1e7 != NULL) {
            *pLatitudeX1e7 = y;
        }
        y = INT_MIN;
        if (*(pMessage + 20) == 0x03) {
            y = (int32_t) uUbxProtocolUint32Decode(pMessage + 36);
            if (printIt) {
                uPortLog("U_GNSS_POS: altitude = %d (mm).\n", y);
            }
        }
        if (pAltitudeMillimetres != NULL) {
            *pAltitudeMillimetres = y;
        }
        y = (int32_t) uUbxProtocolUint32Decode(pMessage + 40);
        if (printIt) {
            uPortLog("U_GNSS_POS: radius = %d (mm).\n", y);
        }
        if (pRadiusMillimetres != NULL) {
            *pRadiusMillimetres = y;
        }
        y = (int32_t) uUbxProtocolUint32Decode(pMessage + 60);
        if (printIt) {
            uPortLog("U_GNSS_POS: speed = %d (mm/s).\n", y);
        }
        if (pSpeedMillimetresPerSecond != NULL) {
            *pSpeedMillimetresPerSecond = y;
        }
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        //lint -restore
    }

    return errorCode;
}

// Establish position.
static int32_t posGet(uGnssPrivateInstance_t *pInstance,
                      int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                      int32_t *pAltitudeMillimetres,
                      int32_t *pRadiusMillimetres,
                      int32_t *pSpeedMillimetresPerSecond,
                      int32_t *pSvs, int64_t *pTimeUtc, bool printIt)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
    // Enough room for the body of the UBX-NAV-PVT message
    char message[92] = {0};

    errorCode = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                  0x01, 0x07, NULL, 0,
                                                  message, sizeof(message));
    if (errorCode == sizeof(message)) {
        // Got the correct message body length, process it
        errorCode = posDecode(message,
                              pLatitudeX1e7, pLongitudeX1e7,
                              pAltitudeMillimetres,
                              pRadiusMillimetres,
                              pSpeedMillimetresPerSecond,
                              pSvs, pTimeUtc, printIt);
    } else {
        if (errorCode >= 0) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        }
    }

    return errorCode;
}

// Establish position as a task.
// IMPORTANT: this does NOT lock gUGnssPrivateMutex and hence it
// is important that it is stopped before a pInstance is released.
static void posGetTask(void *pParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
    uGnssPosGetTaskParameters_t taskParameters;
    int64_t startTime;
    int32_t latitudeX1e7 = INT_MIN;
    int32_t longitudeX1e7 = INT_MIN;
    int32_t altitudeMillimetres = INT_MIN;
    int32_t radiusMillimetres = -1;
    int32_t speedMillimetresPerSecond = INT_MIN;
    int32_t svs = -1;
    int64_t timeUtc = -1;

    // Copy the parameter into our local variable and free it
    memcpy(&taskParameters, pParameter, sizeof(taskParameters));
    uPortFree(pParameter);

    // Lock the mutex to indicate that we're running
    U_PORT_MUTEX_LOCK(taskParameters.pInstance->posMutex);

    startTime = uPortGetTickTimeMs();
    taskParameters.pInstance->posTaskFlags |= U_GNSS_POS_TASK_FLAG_HAS_RUN;

    while ((taskParameters.pInstance->posTaskFlags & U_GNSS_POS_TASK_FLAG_KEEP_GOING) &&
           (errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
           ((uPortGetTickTimeMs() - startTime) / 1000 < U_GNSS_POS_TIMEOUT_SECONDS)) {
        // Call posGet() to do the work
        errorCode = posGet(taskParameters.pInstance,
                           &latitudeX1e7,
                           &longitudeX1e7,
                           &altitudeMillimetres,
                           &radiusMillimetres,
                           &speedMillimetresPerSecond,
                           &svs,
                           &timeUtc, false);
        if (errorCode != 0) {
            uPortTaskBlock(U_GNSS_POS_CALLBACK_TASK_STACK_DELAY_SECONDS * 1000);
        }
    }

    // Call the callback
    taskParameters.pCallback(taskParameters.gnssHandle, errorCode, latitudeX1e7,
                             longitudeX1e7, altitudeMillimetres, radiusMillimetres,
                             speedMillimetresPerSecond, svs, timeUtc);

    U_PORT_MUTEX_UNLOCK(taskParameters.pInstance->posMutex);

    // Delete ourselves
    uPortTaskDelete(NULL);
}

// Callback that should receive a UBX-NAV-PVT message.
static void messageCallback(uDeviceHandle_t gnssHandle,
                            const uGnssMessageId_t *pMessageId,
                            int32_t errorCodeOrLength,
                            void *pCallbackParam)
{
    uGnssPrivateInstance_t *pInstance = (uGnssPrivateInstance_t *) pCallbackParam;
    char message[92 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES] = {0};
    int32_t latitudeX1e7 = INT_MIN;
    int32_t longitudeX1e7 = INT_MIN;
    int32_t altitudeMillimetres = INT_MIN;
    int32_t radiusMillimetres = -1;
    int32_t speedMillimetresPerSecond = INT_MIN;
    int32_t svs = -1;
    int64_t timeUtc = -1;

    (void) pMessageId;

    if (errorCodeOrLength > 0) {
        if (errorCodeOrLength > sizeof(message)) {
            errorCodeOrLength = sizeof(message);
        }
        // Get the while UBX-NAV-PVT message
        uGnssMsgReceiveCallbackRead(gnssHandle,
                                    message,
                                    errorCodeOrLength);
        // Decode the body
        errorCodeOrLength = posDecode(message + U_UBX_PROTOCOL_HEADER_LENGTH_BYTES,
                                      &latitudeX1e7, &longitudeX1e7,
                                      &altitudeMillimetres,
                                      &radiusMillimetres,
                                      &speedMillimetresPerSecond,
                                      &svs, &timeUtc, false);
        // Call the callback
        // Note: there can be two handles involved here, e.g. if
        // GNSS is inside a cellular device, hence we make sure
        // we pass back the one that came in
        pInstance->pStreamedPosition->pCallback(pInstance->pStreamedPosition->gnssHandle,
                                                errorCodeOrLength,
                                                latitudeX1e7,
                                                longitudeX1e7,
                                                altitudeMillimetres,
                                                radiusMillimetres,
                                                speedMillimetresPerSecond,
                                                svs,
                                                timeUtc);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uGnssPosPrivateLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the current position.
int32_t uGnssPosGet(uDeviceHandle_t gnssHandle,
                    int32_t *pLatitudeX1e7, int32_t *pLongitudeX1e7,
                    int32_t *pAltitudeMillimetres,
                    int32_t *pRadiusMillimetres,
                    int32_t *pSpeedMillimetresPerSecond,
                    int32_t *pSvs, int64_t *pTimeUtc,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
    uint8_t message[4]; // Room for the body of a UBX-CFG-ANT message
#endif
    int64_t startTime;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
            if ((pInstance->transportType == U_GNSS_TRANSPORT_AT) ||
                (uGnssPrivateGetIntermediateAtHandle(pInstance) != NULL)) {
                // Temporary change: on prototype versions of the
                // SARA-R510M8S module (production week (printed on the
                // module label, upper right) earlier than 20/27)
                // the LNA in the GNSS chip is not automatically switched
                // on by the firmware in the cellular module, so we need
                // to switch it on ourselves by sending UBX-CFG-ANT
                // with contents 02000f039
                message[0] = 0x02;
                message[1] = 0;
                message[2] = 0xf0;
                message[3] = 0x39;
                uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x13,
                                           (const char *) message, 4);
            }
#endif
            startTime = uPortGetTickTimeMs();
            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
            while ((errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                   (((pKeepGoingCallback == NULL) &&
                     (uPortGetTickTimeMs() - startTime) / 1000 < U_GNSS_POS_TIMEOUT_SECONDS) ||
                    ((pKeepGoingCallback != NULL) && pKeepGoingCallback(gnssHandle)))) {
                // Call posGet() to do the work
                errorCode = posGet(pInstance,
                                   pLatitudeX1e7,
                                   pLongitudeX1e7,
                                   pAltitudeMillimetres,
                                   pRadiusMillimetres,
                                   pSpeedMillimetresPerSecond,
                                   pSvs, pTimeUtc, true);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the current position, non-blocking version.
int32_t uGnssPosGetStart(uDeviceHandle_t gnssHandle,
                         void (*pCallback) (uDeviceHandle_t gnssHandle,
                                            int32_t errorCode,
                                            int32_t latitudeX1e7,
                                            int32_t longitudeX1e7,
                                            int32_t altitudeMillimetres,
                                            int32_t radiusMillimetres,
                                            int32_t speedMillimetresPerSecond,
                                            int32_t svs,
                                            int64_t timeUtc))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPosGetTaskParameters_t *pParameters;
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
    uint8_t message[4]; // Room for the body of a UBX-CFG-ANT message
#endif

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pCallback != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if (pInstance->posTaskFlags == 0) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                // Create a mutex to allow us to monitor whether the
                // task is running, if there isn't already one
                // sitting around from a previous run
                if (pInstance->posMutex == NULL) {
                    errorCode = uPortMutexCreate(&(pInstance->posMutex));
                }
                if (errorCode == 0) {
                    // Malloc memory to copy the parameters into:
                    // this memory will be free'd by the task
                    // once it has started
                    pParameters = (uGnssPosGetTaskParameters_t *) pUPortMalloc(sizeof(*pParameters));
                    if (pParameters != NULL) {
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
                        if ((pInstance->transportType == U_GNSS_TRANSPORT_AT) ||
                            (uGnssPrivateGetIntermediateAtHandle(pInstance) != NULL)) {
                            // Temporary change: on prototype versions of the
                            // SARA-R510M8S module (production week (printed on the
                            // module label, upper right) earlier than 20/27)
                            // the LNA in the GNSS chip is not automatically switched
                            // on by the firmware in the cellular module, so we need
                            // to switch it on ourselves by sending UBX-CFG-ANT
                            // with contents 02000f039
                            message[0] = 0x02;
                            message[1] = 0;
                            message[2] = 0xf0;
                            message[3] = 0x39;
                            uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x13,
                                                       (const char *) message, 4);
                        }
#endif
                        // Fill in the callback and start a task
                        // that will establish position (or not)
                        // and call it
                        memset(pParameters, 0, sizeof(*pParameters));
                        pInstance->posTaskFlags |= U_GNSS_POS_TASK_FLAG_KEEP_GOING;
                        pParameters->gnssHandle = gnssHandle;
                        pParameters->pInstance = pInstance;
                        pParameters->pCallback = pCallback;
                        errorCode = uPortTaskCreate(posGetTask,
                                                    "gnssPosCallback",
                                                    U_GNSS_POS_CALLBACK_TASK_STACK_SIZE_BYTES,
                                                    (void *) pParameters,
                                                    U_GNSS_POS_CALLBACK_TASK_PRIORITY,
                                                    &(pInstance->posTask));
                        if (errorCode >= 0) {
                            while (!(pInstance->posTaskFlags & U_GNSS_POS_TASK_FLAG_HAS_RUN)) {
                                // Make sure the task has run before we
                                // exit so that stopping it works properly
                                uPortTaskBlock(U_CFG_OS_YIELD_MS);
                            }
                        } else {
                            // If we couldn't create the task, clean the memory
                            // for the parameters and the mutex and re-zero
                            // posTaskFlags.
                            uPortFree(pParameters);
                            uPortMutexDelete(pInstance->posMutex);
                            pInstance->posMutex = NULL;
                            pInstance->posTaskFlags = 0;
                        }
                    } else {
                        // If we couldn't get memory for the parameters,
                        // clean up the mutex
                        uPortMutexDelete(pInstance->posMutex);
                        pInstance->posMutex = NULL;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Cancel a uGnssPosGetStart().
void uGnssPosGetStop(uDeviceHandle_t gnssHandle)
{
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            uGnssPrivateCleanUpPosTask(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
}

// Get position readings constantly streamed to a callback.
int32_t uGnssPosGetStreamedStart(uDeviceHandle_t gnssHandle,
                                 int32_t rateMs,
                                 void (*pCallback) (uDeviceHandle_t gnssHandle,
                                                    int32_t errorCode,
                                                    int32_t latitudeX1e7,
                                                    int32_t longitudeX1e7,
                                                    int32_t altitudeMillimetres,
                                                    int32_t radiusMillimetres,
                                                    int32_t speedMillimetresPerSecond,
                                                    int32_t svs,
                                                    int64_t timeUtc))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateStreamedPosition_t *pStreamedPosition;
    int32_t measurementPeriodMs;
    int32_t navigationCount;
    int32_t messageRate = -1;
    uGnssPrivateMessageId_t ubxNavPvtMessageId =  {.type = U_GNSS_PROTOCOL_UBX,
                                                   .id.ubx = 0x0107
                                                  };
    uint32_t keyId = U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1;
    uGnssCfgVal_t *pCfgVal = NULL;
    uGnssCfgVal_t cfgVal = {.keyId = U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1,
                            .value = 1
                           };
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
    uint8_t message[4]; // Room for the body of a UBX-CFG-ANT message
#endif

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pCallback != NULL) && (rateMs != 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (uGnssPrivateGetStreamType(pInstance->transportType) >= 0) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                if (pInstance->pStreamedPosition == NULL) {
                    // Malloc memory to copy the parameters into:
                    // this memory will be free'd when
                    // uGnssPosGetStreamedStop() is called
                    pStreamedPosition = (uGnssPrivateStreamedPosition_t *) pUPortMalloc(sizeof(*pStreamedPosition));
                    if (pStreamedPosition != NULL) {
                        pInstance->pStreamedPosition = pStreamedPosition;
                        memset(pStreamedPosition, 0, sizeof(*pStreamedPosition));
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        // Put defaults in place so that we know
                        // to change things back only if necessary
                        pStreamedPosition->measurementPeriodMs = -1;
                        pStreamedPosition->navigationCount = -1;
                        pStreamedPosition->messageRate = -1;
                        pStreamedPosition->asyncHandle = -1;
                        pStreamedPosition->pCallback = pCallback;
                        if (rateMs >= 0) {
                            // Get the existing measurement/navigation rate
                            // and, if it is not rateMs, set it to rateMs
                            if (uGnssPrivateGetRate(pInstance,
                                                    &measurementPeriodMs,
                                                    &navigationCount,
                                                    NULL) != rateMs) {
                                // Set the measurement rate, with a navigation count of 1
                                // and leaving the time system unchanged
                                errorCode = uGnssPrivateSetRate(pInstance, rateMs, 1,
                                                                U_GNSS_TIME_SYSTEM_NONE);
                                if (errorCode == 0) {
                                    pStreamedPosition->measurementPeriodMs = measurementPeriodMs;
                                    pStreamedPosition->navigationCount = navigationCount;
                                }
                            }
                        }
                        if (errorCode == 0) {
                            // Make sure that the UBX-NAV-PVT message
                            // is enabled at once per measurement
                            if (U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                                   U_GNSS_PRIVATE_FEATURE_OLD_CFG_API)) {
                                messageRate = uGnssPrivateGetMsgRate(pInstance,
                                                                     &ubxNavPvtMessageId);
                                if (messageRate != 1) {
                                    errorCode = uGnssPrivateSetMsgRate(pInstance,
                                                                       &ubxNavPvtMessageId, 1);
                                    if (errorCode == 0) {
                                        pStreamedPosition->messageRate = messageRate;
                                    }
                                }
                            } else {
                                if (uGnssCfgPrivateValGetListAlloc(pInstance,
                                                                   &keyId, 1,
                                                                   &pCfgVal,
                                                                   U_GNSS_CFG_VAL_LAYER_RAM) == 1) {
                                    messageRate = (int32_t) pCfgVal->value;
                                    uPortFree(pCfgVal);
                                }
                                if (messageRate != (int32_t) cfgVal.value) {
                                    errorCode = uGnssCfgPrivateValSetList(pInstance, &cfgVal, 1,
                                                                          U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                                          U_GNSS_CFG_LAYERS_SET);
                                    if (errorCode == 0) {
                                        pStreamedPosition->messageRate = messageRate;
                                    }
                                }
                            }
                        }
                        if (errorCode == 0) {
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
                            if (uGnssPrivateGetIntermediateAtHandle(pInstance) != NULL) {
                                // Temporary change: on prototype versions of the
                                // SARA-R510M8S module (production week (printed on the
                                // module label, upper right) earlier than 20/27)
                                // the LNA in the GNSS chip is not automatically switched
                                // on by the firmware in the cellular module, so we need
                                // to switch it on ourselves by sending UBX-CFG-ANT
                                // with contents 02000f039
                                message[0] = 0x02;
                                message[1] = 0;
                                message[2] = 0xf0;
                                message[3] = 0x39;
                                uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x13,
                                                           (const char *) message, 4);
                            }
#endif
                            // Start a message received for the UBX-NAV-PVT message,
                            // which will ultimately call pCallback
                            errorCode = uGnssMsgPrivateReceiveStart(pInstance,
                                                                    &ubxNavPvtMessageId,
                                                                    messageCallback,
                                                                    pInstance);
                            if (errorCode >= 0) {
                                // And we're off
                                pStreamedPosition->gnssHandle = gnssHandle;
                                pStreamedPosition->asyncHandle = errorCode;
                            } else {
                                // If we couldn't create the asynchronous
                                // message receiver, clean up
                                uGnssPrivateCleanUpStreamedPos(pInstance);
                            }
                        } else {
                            // If we couldn't set the rate, clean up
                            uGnssPrivateCleanUpStreamedPos(pInstance);
                        }
                    }
                }
            }
        }
    }

    U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);

    return errorCode;
}

// Cancel a uGnssPosGetStreamedStart().
void uGnssPosGetStreamedStop(uDeviceHandle_t gnssHandle)
{
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            uGnssPrivateCleanUpStreamedPos(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
}

// Set the mode for uGnssPosGetRrlp().
int32_t uGnssPosSetRrlpMode(uDeviceHandle_t gnssHandle, uGnssRrlpMode_t mode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) &&
            (mode < sizeof(gRrlpModeToUbxRxmMessageClass) / sizeof(gRrlpModeToUbxRxmMessageClass[0]))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if ((mode == U_GNSS_RRLP_MODE_MEASX) ||
                U_GNSS_PRIVATE_HAS(pInstance->pModule,
                                   U_GNSS_PRIVATE_FEATURE_RXM_MEAS_50_20_C12_D12)) {
                pInstance->rrlpMode = mode;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the mode for uGnssPosGetRrlp().
int32_t uGnssPosGetRrlpMode(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrRrlpMode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrRrlpMode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCodeOrRrlpMode = (int32_t) pInstance->rrlpMode;
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrRrlpMode;
}

// Get RRLP information from the GNSS chip.
int32_t uGnssPosGetRrlp(uDeviceHandle_t gnssHandle, char *pBuffer,
                        size_t sizeBytes, int32_t svsThreshold,
                        int32_t cNoThreshold,
                        int32_t multipathIndexLimit,
                        int32_t pseudorangeRmsErrorIndexLimit,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    int32_t messageClass;
    int64_t startTime;
    int32_t svs;
    int32_t numBytes;
    int32_t z;
    int32_t numMeetingCriteria;
    bool goodSatellite;
    int32_t ca = 0;
    int32_t cb = 0;
    // Access the buffer as a uint8_t to avoid maths funnies with
    // chars being signed or unsigned
    uint8_t *pBufferUint8 = (uint8_t *) pBuffer;
#ifdef U_CFG_SARA_R5_M8_WORKAROUND
    uint8_t message[4]; // Room for the body of a UBX-CFG-ANT message
#endif

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pBufferUint8 != NULL) &&
            (sizeBytes >= U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES)) {

#ifdef U_CFG_SARA_R5_M8_WORKAROUND
            if ((pInstance->transportType == U_GNSS_TRANSPORT_AT) ||
                (uGnssPrivateGetIntermediateAtHandle(pInstance) != NULL)) {
                // Temporary change: on prototype versions of the
                // SARA-R510M8S module (production week (printed on the
                // module label, upper right) earlier than 20/27)
                // the LNA in the GNSS chip is not automatically switched
                // on by the firmware in the cellular module, so we need
                // to switch it on ourselves by sending UBX-CFG-ANT
                // with contents 02000f039
                message[0] = 0x02;
                message[1] = 0;
                message[2] = 0xf0;
                message[3] = 0x39;
                uGnssPrivateSendUbxMessage(pInstance, 0x06, 0x13,
                                           (const char *) message, 4);
            }
#endif
            messageClass = gRrlpModeToUbxRxmMessageClass[pInstance->rrlpMode];
            startTime = uPortGetTickTimeMs();
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
            while ((errorCodeOrLength == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                   (((pKeepGoingCallback == NULL) &&
                     (uPortGetTickTimeMs() - startTime) / 1000 < U_GNSS_POS_TIMEOUT_SECONDS) ||
                    ((pKeepGoingCallback != NULL) && pKeepGoingCallback(gnssHandle)))) {

                numBytes = uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                             0x02, messageClass, NULL, 0,
                                                             pBuffer + U_GNSS_POS_RRLP_HEADER_SIZE_BYTES,
                                                             sizeBytes - U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
                if ((numBytes > 0) && (pInstance->rrlpMode == U_GNSS_RRLP_MODE_MEASX)) {
                    // Got something; when using MEASX we need to check if it is good enough.
                    // 34 since that's the furthest we need to read to check on the number of satellites
                    if ((((svsThreshold >= 0) || (cNoThreshold >= 0) ||
                          (multipathIndexLimit >= 0) || (pseudorangeRmsErrorIndexLimit >= 0)) && (numBytes >= 34))) {
                        // The number of satellites is at offset 34
                        svs = *(pBufferUint8 + U_GNSS_POS_RRLP_HEADER_SIZE_BYTES + 34);
                        uPortLog("U_GNSS_POS: RRLP information for %d satellite(s).\n", svs);
                        if ((svsThreshold >= 0) && (svs < svsThreshold)) {
                            // Not enough satellites in the first place
                            numBytes = -1;
                        }
                        if ((numBytes > 0) &&
                            ((cNoThreshold >= 0) || (multipathIndexLimit >= 0) || (pseudorangeRmsErrorIndexLimit >= 0))) {
                            numMeetingCriteria = svs;
                            // 65 since that's the furthest we need to check on the criteria
                            for (int8_t x = 0; (x < svs) && (numBytes >= 65 + (x * 24)); x++) {
                                goodSatellite = true;
                                // Carrier to noise ratio is at offset 46 + (x * 24)
                                if (cNoThreshold >= 0) {
                                    z = *(pBufferUint8 + U_GNSS_POS_RRLP_HEADER_SIZE_BYTES + 46 + (x * 24));
                                    uPortLog("U_GNSS_POS: RRLP CNo for satellite %d is %d.\n", x + 1, z);
                                    if (z < cNoThreshold) {
                                        goodSatellite = false;
                                    }
                                }
                                // Multipath index is at offset 47 + (x * 24)
                                if (goodSatellite && (multipathIndexLimit >= 0)) {
                                    z = *(pBufferUint8 + U_GNSS_POS_RRLP_HEADER_SIZE_BYTES + 47 + (x * 24));
                                    uPortLog("U_GNSS_POS: RRLP multipath for satellite %d is %d.\n", x + 1, z);
                                    if (z > multipathIndexLimit) {
                                        goodSatellite = false;
                                    }
                                }
                                // Pseudorange RMS error index is at offset 65 + (x * 24)
                                if (goodSatellite && (pseudorangeRmsErrorIndexLimit >= 0)) {
                                    z = *(pBufferUint8 + U_GNSS_POS_RRLP_HEADER_SIZE_BYTES + 65 + (x * 24));
                                    uPortLog("U_GNSS_POS: pseudorange RMS error index for satellite %d is %d.\n",
                                             x + 1, z);
                                    if (z > pseudorangeRmsErrorIndexLimit) {
                                        goodSatellite = false;
                                    }
                                }
                                if (!goodSatellite) {
                                    numMeetingCriteria--;
                                    uPortLog("U_GNSS_POS: only up to %d satellite(s) meet the criteria.\n",
                                             numMeetingCriteria);
                                    if (numMeetingCriteria < svsThreshold) {
                                        // Force exit
                                        numBytes = -1;
                                    }
                                }
                            }
                        }
                    }
                }

                if (numBytes > 0) {
                    // Got a good measurement!
                    // Since the Cloud Locate service expects the
                    // UBX protocol header information we need to
                    // re-construct that on the front of the message
                    *pBufferUint8 = 0xb5;
                    *(pBufferUint8 + 1) = 0x62;
                    *(pBufferUint8 + 2) = 0x02;
                    *(pBufferUint8 + 3) = messageClass;
                    // Little-endian length of the body
                    *(pBufferUint8 + 4) = (uint8_t) numBytes;
                    *(pBufferUint8 + 5) = (uint8_t) ((uint32_t) numBytes >> 8);
                    // Cloud Locate also needs the two-byte CRC which
                    // is across the class, ID, length and body so
                    // reconstruct that here
                    pBufferUint8 += 2;
                    for (int32_t x = 0; x < numBytes + 4; x++) {
                        ca += *pBufferUint8;
                        cb += ca;
                        pBufferUint8++;
                    }
                    // Write in the CRC
                    *pBufferUint8++ = (uint8_t) (ca & 0xff);
                    *pBufferUint8 = (uint8_t) (cb & 0xff);

                    errorCodeOrLength = numBytes + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

// End of file
