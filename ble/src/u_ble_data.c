/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the data API for ble.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"

#include "u_at_client.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    uShortRangePrivateInstance_t *pInstance;
    int32_t connHandle;
    int32_t type;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    int32_t dataChannel;
    int32_t mtu;
    void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *);
    void *pCallbackParameter;
} uBleDataSpsConnection_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLC_urc(uAtClientHandle_t atHandle,
                         void *pParameter)
{
    // We only need to read out to clean up for the at client, all data we need
    // will arrive in later events.
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];

    (void)uAtClientReadInt(atHandle); // Connection handle
    (void)uAtClientReadInt(atHandle); // Type (always 0 meaning GATT)
    (void)uAtClientReadString(atHandle, address, U_SHORT_RANGE_BT_ADDRESS_SIZE, false);
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUBTACLD_urc(uAtClientHandle_t atHandle,
                         void *pParameter)
{
    // We only need to read out to clean up for the at client, all data we need
    // will arrive in later events.
    (void)uAtClientReadInt(atHandle); // Connection handle
}

static void spsEventCallback(uAtClientHandle_t atHandle,
                             void *pParameter)
{
    uBleDataSpsConnection_t *pStatus = (uBleDataSpsConnection_t *) pParameter;

    (void) atHandle;

    if (pStatus != NULL) {
        if (pStatus->pCallback != NULL) {
            pStatus->pCallback(pStatus->connHandle, pStatus->address, pStatus->type,
                               pStatus->dataChannel, pStatus->mtu, pStatus->pCallbackParameter);
        }
        if (pStatus->pInstance != NULL) {
            pStatus->pInstance->pPendingSpsConnectionEvent = NULL;
        }

        free(pStatus);
    }
}

//lint -e{818} suppress "address could be declared as pointing to const":
// need to follow function signature
static void btEdmConnectionCallback(int32_t streamHandle, uint32_t type,
                                    uint32_t channel, bool ble, int32_t mtu,
                                    char *address, void *pParam)
{
    (void) ble;
    (void) streamHandle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParam;
    // Type 0 == connected
    if (pInstance != NULL && pInstance->atHandle != NULL) {
        uBleDataSpsConnection_t *pStatus = (uBleDataSpsConnection_t *)
                                           pInstance->pPendingSpsConnectionEvent;
        bool send = false;

        if (pStatus == NULL) {
            //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
            pStatus = (uBleDataSpsConnection_t *) malloc(sizeof(*pStatus));
        } else {
            send = true;
        }

        if (pStatus != NULL) {
            pStatus->pInstance = pInstance;
            memcpy(pStatus->address, address, U_SHORT_RANGE_BT_ADDRESS_SIZE);
            pStatus->type = (int32_t) type;
            pStatus->dataChannel = (int32_t) channel;
            pStatus->mtu = mtu;
            pStatus->pCallback = pInstance->pSpsConnectionCallback;
            pStatus->pCallbackParameter = pInstance->pSpsConnectionCallbackParameter;
        }
        if (send) {
            uAtClientCallback(pInstance->atHandle, spsEventCallback, pStatus);
        } else {
            pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
        }
    }
}

static void atConnectionEvent(int32_t connHandle, int32_t type, void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    bool send = false;

    if (pInstance->pSpsConnectionCallback != NULL) {
        uBleDataSpsConnection_t *pStatus = (uBleDataSpsConnection_t *)
                                           pInstance->pPendingSpsConnectionEvent;

        if (pStatus == NULL) {
            //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
            pStatus = (uBleDataSpsConnection_t *) malloc(sizeof(*pStatus));
        } else {
            send = true;
        }
        if (pStatus != NULL) {
            pStatus->connHandle = connHandle;
            // AT (this) event info: connHandle, type, profile, address, mtu
            // EDM event info: type, profile, address, mtu, channel
            // use connHandle from here, the rest from the EDM event

            if (send) {
                uAtClientCallback(pInstance->atHandle, spsEventCallback, pStatus);
            } else {
                pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
            }
        }
    }
}

static void dataCallback(int32_t handle, int32_t channel, int32_t length,
                         char *pData, void *pParameters)
{
    (void)handle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameters;

    if ((pInstance != NULL) && (pInstance->pBtDataCallback)) {
        pInstance->pBtDataCallback(channel, length, pData, pInstance->pBtDataCallbackParameter);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uBleDataSetCallbackConnectionStatus(int32_t bleHandle,
                                            void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *),
                                            void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(bleHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            bool cleanUp = false;
            if (pCallback != NULL && pInstance->pSpsConnectionCallback == NULL) {
                pInstance->pSpsConnectionCallback = pCallback;
                pInstance->pSpsConnectionCallbackParameter = pCallbackParameter;

                errorCode = uAtClientSetUrcHandler(pInstance->atHandle, "+UUBTACLC:",
                                                   UUBTACLC_urc, pInstance);

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = uAtClientSetUrcHandler(pInstance->atHandle, "+UUBTACLD:",
                                                       UUBTACLD_urc, pInstance);
                }

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = uShortRangeConnectionStatusCallback(bleHandle, U_SHORT_RANGE_CONNECTTION_TYPE_BT,
                                                                    atConnectionEvent, (void *) pInstance);
                }

                if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                    errorCode = uShortRangeEdmStreamBtEventCallbackSet(pInstance->streamHandle, btEdmConnectionCallback,
                                                                       pInstance, 1536, U_CFG_OS_PRIORITY_MAX - 5);
                }

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    cleanUp = true;
                }

            } else if (pCallback == NULL && pInstance->pSpsConnectionCallback != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                cleanUp = true;
            }

            if (cleanUp) {
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUBTACLC:");
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUBTACLD:");
                uShortRangeConnectionStatusCallback(bleHandle, U_SHORT_RANGE_CONNECTTION_TYPE_BT, NULL, NULL);
                uShortRangeEdmStreamBtEventCallbackSet(pInstance->streamHandle, NULL, NULL, 0, 0);
                pInstance->pSpsConnectionCallback = NULL;
                pInstance->pSpsConnectionCallbackParameter = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uBleDataConnectSps(int32_t bleHandle, const char *pAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(bleHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                char url[20];
                memset(url, 0, 20);
                char start[] = "sps://";
                memcpy(url, start, 6);
                memcpy((url + 6), pAddress, 13);
                atHandle = pInstance->atHandle;
                uPortLog("U_BLE_DATA: Sending AT+UDCP\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UDCP=");
                uAtClientWriteString(atHandle, (char *)&url[0], false);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UDCP:");
                uAtClientReadInt(atHandle); // conn handle
                uAtClientResponseStop(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uBleDataDisconnect(int32_t bleHandle, int32_t connHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(bleHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            uAtClientHandle_t atHandle = pInstance->atHandle;
            uPortLog("U_SHORT_RANGE: Sending disconnect\n");

            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UDCPC=");
            uAtClientWriteInt(atHandle, connHandle);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uBleDataSend(int32_t bleHandle, int32_t channel, const char *pData, int32_t length)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(bleHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = uShortRangeEdmStreamWrite(pInstance->streamHandle, channel, pData, length);
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uBleDataSetCallbackData(int32_t bleHandle,
                                void (*pCallback) (int32_t, size_t, char *, void *),
                                void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(bleHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (pInstance->pBtDataCallback == NULL && pCallback != NULL) {
                pInstance->pBtDataCallback = pCallback;
                pInstance->pBtDataCallbackParameter = pCallbackParameter;

                errorCode = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle, 0, dataCallback,
                                                                     pInstance, U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                                                     U_CFG_OS_PRIORITY_MAX - 5);
            } else if (pInstance->pBtDataCallback != NULL && pCallback == NULL) {
                pInstance->pBtDataCallback = NULL;
                pInstance->pBtDataCallbackParameter = NULL;

                errorCode = uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle, 0, NULL, NULL, 0, 0);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

// End of file
