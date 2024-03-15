/*
 * Copyright 2019-2024 u-blox
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
 * @brief Implementation of the CellTime API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MIN
#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdio.h"     // snprintf()
#include "stdbool.h"
#include "string.h"    // memcpy(), strstr()
#include "ctype.h"     // isdigit()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* snprintf(), must be included
                                              before the other port files. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_uart.h"
#include "u_port_ppp.h"

#include "u_time.h"

#include "u_at_client.h"

#include "u_device_shared.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_info.h"
#include "u_cell_cfg.h"
#include "u_cell_time.h"
#include "u_cell_time_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** All the parameters for the event callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t,  uCellTimeEvent_t *, void *);
    void *pCallbackParameter;
    uCellTimeEvent_t event;
    int32_t cellIdPhysicalFromCellSync;
} uCellTimeEventData_t;

/** All the parameters for the time callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    void (*pCallback) (uDeviceHandle_t,  uCellTime_t *, void *);
    void *pCallbackParameter;
    uCellTime_t time;
} uCellTimeTimeData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Map the result code received in a +UUTIMECELLSELECT URC to one
 * of our error codes.
 */
static const int32_t gSyncResultToErrorCode[] = {
    (int32_t) U_ERROR_COMMON_CANCELLED,      // 0: synchronisation disabled, cell released
    (int32_t) U_ERROR_COMMON_SUCCESS,        // 1: synchronization enabled and successful, camped on the requested cell, TA is available
    (int32_t) U_ERROR_COMMON_NOT_FOUND,      // 2: synchronization enabled and unsuccessful, the requested cell was not found
    (int32_t) U_CELL_ERROR_CONNECTED,        // 3: cellular functionality not switched off, the synchronization cannot be enabled or disabled
    (int32_t) U_ERROR_COMMON_SUCCESS,        // 4: RACH failure: synchronization enabled and successful, camped on the requested cell but TA is not available
    (int32_t) U_ERROR_COMMON_UNKNOWN         // 5: generic error (e.g. release configuration failure)
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set a cellular module GPIO pin to a given function.
static int32_t gpioConfig(uAtClientHandle_t atHandle, int32_t gpioId,
                          int32_t function)
{
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UGPIOC=");
    uAtClientWriteInt(atHandle, gpioId);
    uAtClientWriteInt(atHandle, function);
    uAtClientCommandStopReadResponse(atHandle);
    return uAtClientUnlock(atHandle);
}

// Convert a string of the form "0123456789.0123456789", representing
// a number with N fractional digits, into a number times 1,000,000,000
static int64_t numberX1e9(const char *pNumber, size_t fractionalDigits)
{
    int64_t x1e9 = 0;
    int32_t x = 0;
    int64_t y;
    int32_t z = 9;

    // Find out how many digits there are before the first
    // non-digit thing (which might be a decimal point).
    while (isdigit((int32_t) *(pNumber + x))) { // *NOPAD* stop AStyle making * look like a multiply
        x++;
    }

    // Now read those digits and accumulate them into x1e9
    while (x > 0) {
        y = *pNumber - '0';
        for (int32_t z = 1; z < x; z++) {
            y *= 10;
        }
        x1e9 += y;
        x--;
        pNumber++;
    }

    x1e9 *= 1000000000;

    if (*pNumber == '.') {
        // If we're now at a decimal point, skip over it and
        // deal with the fractional part of up to fractionalDigits
        pNumber++;
        x = fractionalDigits;
        while (isdigit((int32_t) *pNumber) && (x > 0)) { // *NOPAD*
            y = *pNumber - '0';
            for (int32_t w = 1; w < z; w++) {
                y *= 10;
            }
            x1e9 += y;
            x--;
            z--;
            pNumber++;
        }
    }

    return x1e9;
}

// Callback via which the user's event callback is called.
// This must be called through the uAtClientCallback() mechanism
// in order to prevent customer code blocking the AT client.
static void eventCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellTimeEventData_t *pEventData = (uCellTimeEventData_t *) pParameter;

    if (pEventData != NULL) {
        if (pEventData->pCallback != NULL) {
            if (pEventData->event.source == U_CELL_TIME_SOURCE_CELL) {
                // Need to populate the cellIdPhysical field, first try using
                // AT+CELLINFO, which goes as follows:
                // +UCELLINFO: <mode>,<type>,<MCC>,<MNC>,<CI>,<PhysCellID>,<TAC>,<RSRP>,<RSRQ>,<LTE_rrc>,<TA_abs>,<TA_state>,<dl_data_rate>,<dl_rx_rate>,<ul_data_rate>,<ul_tx_rate>
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UCELLINFO?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UCELLINFO:");
                // Skip <mode>, <type>, <MCC>, <MNC> and <CI>
                uAtClientSkipParameters(atHandle, 5);
                // Read <PhysCellID>
                pEventData->event.cellIdPhysical = uAtClientReadInt(atHandle);
                if (pEventData->event.cellIdPhysical == 0xFFFF) {
                    // The physical cell ID is not known, use one we might
                    // have saved from forcing sync
                    pEventData->event.cellIdPhysical = pEventData->cellIdPhysicalFromCellSync;
                }
                uAtClientResponseStop(atHandle);
                uAtClientUnlock(atHandle);
            }
            pEventData->pCallback(pEventData->cellHandle,
                                  &(pEventData->event),
                                  pEventData->pCallbackParameter);
        }
        uPortFree(pEventData);
    }
}

// URC handler for +UUTIMEIND.
static void UUTIMEIND_urc(uAtClientHandle_t atHandle, void *pParam)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParam;
    uCellTimePrivateContext_t *pCellTimeContext;
    uCellTimeCellSyncPrivateContext_t *pCellTimeCellSyncContext;
    uCellTimeEventData_t *pEventData;
    uCellTimeEvent_t event = {0};
    int32_t x;

    if (pInstance != NULL) {
        pCellTimeContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
        if (pCellTimeContext != NULL) {
            event.cellIdPhysical = -1; // This is populated later
            // Format is +UUTIMEIND: <status>,<time_info>,<abs_time>,<result>[,<offset_ns>,<offset_s>]
            // Read it all into a local structure
            event.mode = uAtClientReadInt(atHandle);
            event.source = uAtClientReadInt(atHandle);
            x = uAtClientReadInt(atHandle);
            if (x == 0) {
                event.cellTime = true;
            }
            event.result = uAtClientReadInt(atHandle);
            if ((event.result == U_CELL_TIME_RESULT_UTC_ALIGNMENT) ||
                (event.result == U_CELL_TIME_RESULT_OFFSET_DETECTED)) {
                event.offsetNanoseconds = uAtClientReadInt(atHandle);
                if (event.offsetNanoseconds >= 0) {
                    x = uAtClientReadInt(atHandle);
                    if (x > 0) {
                        event.offsetNanoseconds += ((int64_t) x) * 1000000000;
                    }
                }
            }
            if ((event.source != U_CELL_TIME_SOURCE_INIT) &&
                ((event.result == U_CELL_TIME_RESULT_SUCCESS) ||
                 (event.result == U_CELL_TIME_RESULT_UTC_ALIGNMENT) ||
                 (event.result == U_CELL_TIME_RESULT_OFFSET_DETECTED))) {
                // If we not initialisting and the result is not an error case,
                // we are synchronised
                event.synchronised = true;
            }
            if ((event.mode >= 0) && (event.source >= 0) && (event.result >= 0) &&
                (pCellTimeContext->pCallbackEvent != NULL)) {
                // Put the data for the callback into a struct to our
                // local callback via the AT client's callback mechanism
                // to decouple it from any URC handler.
                // Note: it is up to eventCallback() to free the allocated memory.
                pEventData = (uCellTimeEventData_t *) pUPortMalloc(sizeof(uCellTimeEventData_t));
                if (pEventData != NULL) {
                    memcpy(&(pEventData->event), &event, sizeof(pEventData->event));
                    // We can't always get the physical cell ID of the cell
                    // we are syncrhonised to by asking the module so pass on the
                    // physical cell ID that we might have forced synchronisation
                    // to for it to use in that case
                    pEventData->cellIdPhysicalFromCellSync = -1;
                    pCellTimeCellSyncContext = (uCellTimeCellSyncPrivateContext_t *)
                                               pInstance->pCellTimeCellSyncContext;
                    if (pCellTimeCellSyncContext != NULL) {
                        pEventData->cellIdPhysicalFromCellSync = pCellTimeCellSyncContext->cellIdPhysical;
                    }
                    pEventData->cellHandle = pInstance->cellHandle;
                    pEventData->pCallback = pCellTimeContext->pCallbackEvent;
                    pEventData->pCallbackParameter = pCellTimeContext->pCallbackEventParam;
                    if (uAtClientCallback(atHandle, eventCallback, pEventData) != 0) {
                        // Clean up on error
                        uPortFree(pEventData);
                    }
                }
            }
        }
    }
}

// Callback via which the user's time callback is called.
// This must be called through the uAtClientCallback() mechanism
// in order to prevent customer code blocking the AT client.
static void timeCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellTimeTimeData_t *pTimeData = (uCellTimeTimeData_t *) pParameter;

    (void) atHandle;

    if (pTimeData != NULL) {
        if (pTimeData->pCallback != NULL) {
            pTimeData->pCallback(pTimeData->cellHandle,
                                 &(pTimeData->time),
                                 pTimeData->pCallbackParameter);
        }
        uPortFree(pTimeData);
    }
}

// URC handler for +UUTIME.
static void UUTIME_urc(uAtClientHandle_t atHandle, void *pParam)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParam;
    uCellTimePrivateContext_t *pContext;
    uCellTimeTimeData_t *pTimeData;
    uCellTime_t time = {0};
    char buffer[25]; // Enough room for "012345678.012345678" ir 22/08/2020" or "11:22:33", plus a terminator
    int32_t length;
    size_t offset;
    int32_t numParameters = 0;
    int32_t months;
    int32_t x;
    int64_t timeSeconds = 0;

    if (pInstance != NULL) {
        pContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
        if (pContext != NULL) {
            // Format is +UUTIME: <date>,<time>,<milliseconds>,<accuracy>,<source>
            // where <date>,<time> are of the form 22/08/2020,11:22:33 and both
            // <milliseconds> and <accuracy> are floating point with up to nine
            // decimal places
            length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (length > 0) {
                offset = 0;
                // Day (1 to 31)
                buffer[offset + 2] = 0;
                timeSeconds += (int64_t) (strtol(&(buffer[offset]), NULL, 10) - 1) * 3600 * 24;
                // Months converted to months since January
                offset = 3;
                buffer[offset + 2] = 0;
                // Month (1 to 12), so take away 1 to make it zero-based
                months = strtol(&(buffer[offset]), NULL, 10) - 1;
                // Four digit year converted to years since 1970
                offset = 6;
                buffer[offset + 4] = 0;
                x = strtol(&(buffer[offset]), NULL, 10) - 1970;
                months += x * 12;
                // Work out the number of seconds due to the year/month count
                timeSeconds += uTimeMonthsToSecondsUtc(months);
                numParameters++;
            }
            length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (length > 0) {
                // Hours since midnight
                offset = 0;
                buffer[offset + 2] = 0;
                timeSeconds += (int64_t) strtol(&(buffer[offset]), NULL, 10) * 3600;
                // Minutes after the hour
                offset = 3;
                buffer[offset + 2] = 0;
                timeSeconds += (int64_t) strtol(&(buffer[offset]), NULL, 10) * 60;
                // Seconds after the hour
                offset = 6;
                buffer[offset + 2] = 0;
                timeSeconds += (int64_t) strtol(&(buffer[offset]), NULL, 10);
                numParameters++;
            }
            time.timeNanoseconds = timeSeconds * 1000000000;
            // Time, milliseconds portion
            length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (length > 0) {
                time.timeNanoseconds += (numberX1e9(buffer, 6) / 1000);
                numParameters++;
            }
            // Accuracy, nanoseconds
            length = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (length > 0) {
                time.accuracyNanoseconds = numberX1e9(buffer, 9);
                numParameters++;
            }
            // Source
            x = uAtClientReadInt(atHandle);
            if (x >= 0) {
                numParameters++;
                if (x == 0) {
                    time.cellTime = true;
                    // In this case we report the relative time
                    time.timeNanoseconds -= U_CELL_TIME_CONVERT_TO_UNIX_SECONDS * 1000000000;
                }
            }
            if ((numParameters == 5) && (pContext->pCallbackTime != NULL)) {
                // Put the data for the callback into a struct to our
                // local callback via the AT client's callback mechanism
                // to decouple it from any URC handler.
                // Note: it is up to timeCallback() to free the allocated memory.
                pTimeData = (uCellTimeTimeData_t *) pUPortMalloc(sizeof(uCellTimeTimeData_t));
                if (pTimeData != NULL) {
                    memcpy(&(pTimeData->time), &time, sizeof(pTimeData->time));
                    pTimeData->cellHandle = pInstance->cellHandle;
                    pTimeData->pCallback = pContext->pCallbackTime;
                    pTimeData->pCallbackParameter = pContext->pCallbackTimeParam;
                    if (uAtClientCallback(atHandle, timeCallback, pTimeData) != 0) {
                        // Clean up on error
                        uPortFree(pTimeData);
                    }
                }
            }
        }
    }
}

// URC handler for +UUTIMECELLSELECT.
static void UUTIMECELLSELECT_urc(uAtClientHandle_t atHandle, void *pParam)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParam;
    uCellTimeCellSyncPrivateContext_t *pContext;
    int32_t x;

    if (pInstance != NULL) {
        pContext = (uCellTimeCellSyncPrivateContext_t *) pInstance->pCellTimeCellSyncContext;
        if (pContext != NULL) {
            // Format is +UUTIMECELLSELECT: <result>,[<TA>]
            x = uAtClientReadInt(atHandle);
            if ((x >= 0) && (x < sizeof(gSyncResultToErrorCode) / sizeof(gSyncResultToErrorCode[0]))) {
                pContext->errorCode = gSyncResultToErrorCode[x];
            }
            if (x == 1) {
                // Should have a TA
                x = uAtClientReadInt(atHandle);
                if (x >= 0) {
                    pContext->timingAdvance = x;
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: CELLTIME
 * -------------------------------------------------------------- */

// Enable CellTime.
int32_t uCellTimeEnable(uDeviceHandle_t cellHandle,
                        uCellTimeMode_t mode, bool cellTimeOnly,
                        int64_t offsetNanoseconds,
                        void (*pCallback) (uDeviceHandle_t,
                                           uCellTimeEvent_t *,
                                           void *),
                        void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellTimePrivateContext_t *pContext;
    void (*pTimeCallback) (uDeviceHandle_t, uCellTime_t *, void *);
    void *pTimeCallbackParameter;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) &&
            ((mode == U_CELL_TIME_MODE_PULSE) || (mode == U_CELL_TIME_MODE_ONE_SHOT) ||
             (mode == U_CELL_TIME_MODE_EXT_INT_TIMESTAMP))) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
                if (pContext == NULL) {
                    // Get a context if we don't already have one; this
                    // will be free'd only when the cellular instance is closed
                    // to ensure thread-safety
                    pContext = (uCellTimePrivateContext_t *) pUPortMalloc(sizeof(uCellTimePrivateContext_t));
                    memset(pContext, 0, sizeof(*pContext));
                }
                if (pContext != NULL) {
                    pInstance->pCellTimeContext = pContext;
                    // When resetting an existing context, don't resetting
                    // the time callback as that may have been called
                    // before us
                    pTimeCallback = pContext->pCallbackTime;
                    pTimeCallbackParameter = pContext->pCallbackTimeParam;
                    memset(pContext, 0, sizeof(*pContext));
                    pContext->pCallbackTime = pTimeCallback;
                    pContext->pCallbackTimeParam = pTimeCallbackParameter;
                    pContext->pCallbackEvent = pCallback;
                    pContext->pCallbackEventParam = pCallbackParameter;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    // If required by the mode, configure the module's GPIOs
                    atHandle = pInstance->atHandle;
                    if ((mode == U_CELL_TIME_MODE_PULSE) || (mode == U_CELL_TIME_MODE_ONE_SHOT)) {
                        // GPIO ID 19 ("GPIO6") needs to have special function
                        // "Time pulse output" (22)
                        errorCode = gpioConfig(atHandle, 19, 22);
                    } else if (mode == U_CELL_TIME_MODE_EXT_INT_TIMESTAMP) {
                        // GPIO ID 33 ("EXT_INT") needs to have special function
                        // "Time stamp of external interrupt" (23)
                        errorCode = gpioConfig(atHandle, 33, 23);
                    }
                    if ((errorCode == 0) && !cellTimeOnly && !uCellPrivateGnssInsideCell(pInstance)) {
                        // If we may use GNSS and the GNSS chip is external
                        // to the cellular module then the pins that provide
                        // timing need to be configured
                        // GPIO ID 46 ("SDIO_CMD"), special function
                        // "External GNSS time pulse input" (28)
                        errorCode = gpioConfig(atHandle, 46, 28);
                        if (errorCode == 0) {
                            // GPIO ID 25 ("GPIO4"), special function
                            // "External GNSS time stamp of external interrupt" (29)
                            errorCode = gpioConfig(atHandle, 25, 29);
                        }
                    }
                    if (errorCode == 0) {
                        // Set the offset
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UTIMECFG=");
                        uAtClientWriteInt(atHandle, (int32_t) (offsetNanoseconds % 1000000000));
                        uAtClientWriteInt(atHandle, (int32_t) (offsetNanoseconds / 1000000000));
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                    }
                    if ((errorCode == 0) && (pContext->pCallbackEvent != NULL)) {
                        // Enable the +UUTIMEIND URC
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UTIMEIND=");
                        uAtClientWriteInt(atHandle, 1);
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if (errorCode == 0) {
                            // Attach the +UUTIMEIND URC handler
                            errorCode = uAtClientSetUrcHandler(atHandle, "+UUTIMEIND:",
                                                               UUTIMEIND_urc, pInstance);
                        }
                    }
                    if (errorCode == 0) {
                        // Now, finally, set the CellTime mode
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UTIME=");
                        uAtClientWriteInt(atHandle, mode);
                        uAtClientWriteInt(atHandle, cellTimeOnly ? 2 : 1);
#ifndef U_CELL_CFG_SARA_R5_00B
                        if (mode == U_CELL_TIME_MODE_PULSE) {
                            uAtClientWriteInt(atHandle, U_CELL_TIME_PULSE_PERIOD_SECONDS);
                            uAtClientWriteInt(atHandle, U_CELL_TIME_PULSE_WIDTH_MILLISECONDS);
                        }
#endif
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if (errorCode != 0) {
                            // Clean up on error but leave the context to avoid race
                            // conditions; it will be cleaned-up when the cellular
                            // instance is closed
                            if (pContext->pCallbackEvent != NULL) {
                                uAtClientRemoveUrcHandler(atHandle, "+UUTIMEIND:");
                                pContext->pCallbackEvent = NULL;
                            }
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Disable CellTime.
int32_t uCellTimeDisable(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellTimePrivateContext_t *pContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                atHandle = pInstance->atHandle;
                pContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
                if (pContext != NULL) {
                    if (pContext->pCallbackEvent != NULL) {
                        uAtClientRemoveUrcHandler(atHandle, "+UUTIMEIND:");
                        pContext->pCallbackEvent = NULL;
                    }
                    if (pContext->pCallbackTime != NULL) {
                        uAtClientRemoveUrcHandler(atHandle, "+UUTIME:");
                        pContext->pCallbackTime = NULL;
                    }
                }
                errorCode = (int32_t) U_CELL_ERROR_AT;
                // This sometimes doesn't receive a response on the
                // first occasion, so allow a few tries
                for (size_t x = 0; (errorCode < 0) && (x < 3); x++) {
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UTIME=");
                    uAtClientWriteInt(atHandle, 0);
                    uAtClientCommandStopReadResponse(atHandle);
                    errorCode = uAtClientUnlock(atHandle);
                    if (errorCode < 0) {
                        uPortTaskBlock(1000);
                    }
                }
                // Leave the context to avoid race conditions:
                // it will be cleaned-up when the cellular
                // instance is closed
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Set a callback for when time has been received, +UUTIME URC.
int32_t uCellTimeSetCallback(uDeviceHandle_t cellHandle,
                             void (*pCallback) (uDeviceHandle_t,
                                                uCellTime_t *,
                                                void *),
                             void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellTimePrivateContext_t *pContext;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            pContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
            if ((pContext == NULL) && (pCallback == NULL)) {
                // Nothing to do
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
                if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    if (pContext == NULL) {
                        // This may be called before uCellTimeEnable() so need
                        // to obtain a context if we don't yet have one
                        pContext = (uCellTimePrivateContext_t *) pUPortMalloc(sizeof(uCellTimePrivateContext_t));
                        memset(pContext, 0, sizeof(*pContext));
                    }
                    if (pContext != NULL) {
                        pInstance->pCellTimeContext = pContext;
                        pContext->pCallbackTime = pCallback;
                        pContext->pCallbackTimeParam = pCallbackParameter;
                        if (pContext->pCallbackTime != NULL) {
                            // Attach the +UUTIME URC handler
                            errorCode = uAtClientSetUrcHandler(pInstance->atHandle, "+UUTIME:",
                                                               UUTIME_urc, pInstance);
                        } else {
                            uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUTIME:");
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Force synchronisation to a specific cell of a specific MNO.
int32_t uCellTimeSyncCellEnable(uDeviceHandle_t cellHandle,
                                uCellNetCellInfo_t *pCell,
                                int32_t *pTimingAdvance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellTimeCellSyncPrivateContext_t *pContext;
    uAtClientHandle_t atHandle;
    char buffer[7]; // Enough room for MCC/MNC plus a null terminator
    int32_t startTimeMs;

    if (gUCellPrivateMutex != NULL) {

        // Since this function requires the normal radio
        // operation of the module to be disabled, take any
        // PPP connection down first (since we can't do so
        // while the cellular API mutex is locked)
        uPortPppDisconnect(cellHandle);

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if ((pInstance != NULL) && (pCell != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pContext = (uCellTimeCellSyncPrivateContext_t *) pInstance->pCellTimeCellSyncContext;
                if (pContext == NULL) {
                    // Get a context if we don't already have one; this
                    // will be free'd only when the cellular instance is closed
                    // to ensure thread-safety
                    pContext = (uCellTimeCellSyncPrivateContext_t *) pUPortMalloc(sizeof(
                                                                                      uCellTimeCellSyncPrivateContext_t));
                }
                if (pContext != NULL) {
                    memset(pContext, 0, sizeof(*pContext));
                    pInstance->pCellTimeCellSyncContext = pContext;
                    // Make sure the radio is off (for normal things) for this
                    uCellPrivateCFunMode(pInstance, 0);
                    atHandle = pInstance->atHandle;
                    errorCode = uAtClientSetUrcHandler(atHandle, "+UUTIMECELLSELECT:",
                                                       UUTIMECELLSELECT_urc, pInstance);
                    if (errorCode == 0) {
                        uAtClientLock(atHandle);
                        pContext->errorCode = INT_MIN;
                        pContext->timingAdvance = -1;
                        pContext->cellIdPhysical = -1;
                        uAtClientCommandStart(atHandle, "AT+UTIMECELLSELECT=");
                        uAtClientWriteInt(atHandle, U_CELL_TIME_SYNC_MODE);
                        snprintf(buffer, sizeof(buffer), "%03d%03d", (int) pCell->mcc, (int) pCell->mnc);
                        uAtClientWriteString(atHandle, buffer, true);
                        uAtClientWriteInt(atHandle, pCell->earfcnDownlink);
                        uAtClientWriteInt(atHandle, pCell->cellIdPhysical);
                        if ((pTimingAdvance != NULL) && (*pTimingAdvance >= 0)) {
                            uAtClientWriteInt(atHandle, *pTimingAdvance);
                        }
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if (errorCode == 0) {
                            // Wait for the URC for the outcome
                            startTimeMs = uPortGetTickTimeMs();
                            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                            while ((pContext->errorCode == INT_MIN) &&
                                   (uPortGetTickTimeMs() - startTimeMs < (U_CELL_TIME_SYNC_TIME_SECONDS * 1000))) {
                                uPortTaskBlock(1000);
                            }
                            if (pContext->errorCode != INT_MIN) {
                                errorCode = pContext->errorCode;
                                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                                    // Record the physical cell ID as we can't read
                                    // it back from the module
                                    pContext->cellIdPhysical = pCell->cellIdPhysical;
                                }
                                if ((pTimingAdvance != NULL) && (pContext->timingAdvance >= 0)) {
                                    *pTimingAdvance = pContext->timingAdvance;
                                }
                            }
                        }
                        uAtClientRemoveUrcHandler(atHandle, "+UUTIMECELLSELECT:");
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// Disable synchronisation to a specific cell.
int32_t uCellTimeSyncCellDisable(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uCellTimeCellSyncPrivateContext_t *pContext;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) {
                pContext = (uCellTimeCellSyncPrivateContext_t *) pInstance->pCellTimeCellSyncContext;
                if (pContext != NULL) {
                    atHandle = pInstance->atHandle;
                    errorCode = uAtClientSetUrcHandler(atHandle, "+UUTIMECELLSELECT:",
                                                       UUTIMECELLSELECT_urc, pInstance);
                    if (errorCode == 0) {
                        uAtClientLock(atHandle);
                        pContext->errorCode = INT_MIN;
                        uAtClientCommandStart(atHandle, "AT+UTIMECELLSELECT=");
                        uAtClientWriteInt(atHandle, 0);
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if (errorCode == 0) {
                            // Have to wait for the URC for the outcome
                            startTimeMs = uPortGetTickTimeMs();
                            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                            while ((pContext->errorCode != (int32_t) U_ERROR_COMMON_CANCELLED) &&
                                   (uPortGetTickTimeMs() - startTimeMs < (U_CELL_TIME_SYNC_TIME_SECONDS * 1000))) {
                                uPortTaskBlock(1000);
                            }
                            if (pContext->errorCode != INT_MIN) {
                                errorCode = pContext->errorCode;
                                if (errorCode == (int32_t) U_ERROR_COMMON_CANCELLED) {
                                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                                    pContext->timingAdvance = -1;
                                    pContext->cellIdPhysical = -1;
                                }
                            }
                        }
                        uAtClientRemoveUrcHandler(atHandle, "+UUTIMECELLSELECT:");
                        // Leave the context to avoid race conditions:
                        // it will be cleaned-up when the cellular
                        // instance is closed
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: ALIASES OF THE TIME-RELATED FUNCTIONS OF CFG/INFO
 * -------------------------------------------------------------- */

// Alias of uCellInfoGetTimeUtc().
int64_t uCellTimeGetUtc(uDeviceHandle_t cellHandle)
{
    return uCellInfoGetTimeUtc(cellHandle);
}

// Alias of uCellInfoGetTimeUtcStr().
int32_t uCellTimeGetUtcStr(uDeviceHandle_t cellHandle,
                           char *pStr, size_t size)
{
    return uCellInfoGetTimeUtcStr(cellHandle, pStr, size);
}

// Alias of uCellInfoGetTime().
int64_t uCellTimeGet(uDeviceHandle_t cellHandle, int32_t *pTimeZoneSeconds)
{
    return uCellInfoGetTime(cellHandle, pTimeZoneSeconds);
}

// Alias of uCellCfgSetTime().
int64_t uCellTimeSet(uDeviceHandle_t cellHandle, int64_t timeLocal,
                     int32_t timeZoneSeconds)
{
    return uCellCfgSetTime(cellHandle, timeLocal, timeZoneSeconds);
}

// End of file
