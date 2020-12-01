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
 * @brief Implementation of the "general" API for short range modules.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc() and free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h" // Required by u_at_client.h

#include "u_error_common.h"

#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"
#include "u_short_range_at_commands.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_SHORT_RANGE_UUDPC_TYPE_BT 1

#ifndef U_SHORT_RANGE_UART_READ_BUFFER
# define U_SHORT_RANGE_UART_READ_BUFFER 1000
#endif

#define U_SHORT_RANGE_BT_ADDRESS_SIZE 14

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    int32_t connHandle;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    void (*pCallback) (int32_t, char *, void *);
    void *pCallbackParameter;
} uShortRangeBtConnection_t;

typedef struct {
    uShortRangePrivateInstance_t *pInstance;
    int32_t connHandle;
    int32_t type;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    int32_t dataChannel;
    int32_t mtu;
    void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *);
    void *pCallbackParameter;
} uShortRangeSpsConnection_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** The next instance handle to use.
 */
static int32_t gNextInstanceHandle = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Find a short range instance in the list by AT handle.
// gUShortRangePrivateMutex should be locked before this is called.
//lint -e{818} suppress "could be declared as pointing to const": atHandle is anonymous
static uShortRangePrivateInstance_t *pGetShortRangeInstanceAtHandle(uAtClientHandle_t atHandle)
{
    uShortRangePrivateInstance_t *pInstance = gpUShortRangePrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->atHandle != atHandle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Add a short range instance to the list.
// gUShortRangePrivateMutex should be locked before this is called.
// Note: doesn't copy it, just adds it.
static void addShortRangeInstance(uShortRangePrivateInstance_t *pInstance)
{
    pInstance->pNext = gpUShortRangePrivateInstanceList;
    gpUShortRangePrivateInstanceList = pInstance;
}

// Remove a short range instance from the list.
// gUShortRangePrivateMutex should be locked before this is called.
// Note: doesn't free it, the caller must do that.
static void removeShortRangeInstance(const uShortRangePrivateInstance_t *pInstance)
{
    uShortRangePrivateInstance_t *pCurrent;
    uShortRangePrivateInstance_t *pPrev = NULL;

    pCurrent = gpUShortRangePrivateInstanceList;
    while (pCurrent != NULL) {
        if (pInstance == pCurrent) {
            if (pPrev != NULL) {
                pPrev->pNext = pCurrent->pNext;
            } else {
                gpUShortRangePrivateInstanceList = pCurrent->pNext;
            }
            pCurrent = NULL;
        } else {
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
}

//lint -e{818} suppress "could be declared as pointing to const": it is!
static void restarted(const uAtClientHandle_t atHandle,
                      void *pParameter)
{
    (void)atHandle;
    (void)pParameter;
    uPortLog("U_SHORT_RANGE: module restart detected\n");
}

static void dataCallback(int32_t streamHandle, uint32_t eventBitmask,
                         void *pParameters)
{
    (void)streamHandle;
    int32_t sizeOrError;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameters;

    size_t read = 0;

    if (eventBitmask == U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED) {
        do {
            sizeOrError = uPortUartRead(pInstance->streamHandle,
                                        pInstance->pBuffer + read,
                                        U_SHORT_RANGE_UART_READ_BUFFER - read);
            if (sizeOrError > 0) {
                read += sizeOrError;
            }
        } while ((sizeOrError > 0) && (read <= U_SHORT_RANGE_UART_READ_BUFFER));

        if (pInstance->pDataCallback) {
            pInstance->pDataCallback(-1, read, pInstance->pBuffer, pInstance->pDataCallbackParameter);
        }
    }
}

static void edmDataCallback(int32_t handle, int32_t channel, int32_t length,
                            char *pData, void *pParameters)
{
    (void)handle;
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameters;

    if (pInstance != NULL && pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
        if (pInstance->pDataCallback) {
            pInstance->pDataCallback(channel, length, pData, pInstance->pDataCallbackParameter);
        }
    }
}

static void connectionStatusCallback(uAtClientHandle_t atHandle,
                                     void *pParameter)
{
    uShortRangeBtConnection_t *pStatus = (uShortRangeBtConnection_t *) pParameter;

    (void) atHandle;

    if (pStatus != NULL) {
        if (pStatus->pCallback != NULL) {
            pStatus->pCallback(pStatus->connHandle, (char *)pStatus->address,
                               pStatus->pCallbackParameter);
        }
        free(pStatus);
    }
}

static void spsEventCallback(uAtClientHandle_t atHandle,
                             void *pParameter)
{
    uShortRangeSpsConnection_t *pStatus = (uShortRangeSpsConnection_t *) pParameter;

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
    if (pInstance != NULL && pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM &&
        pInstance->atHandle != NULL) {
        uShortRangeSpsConnection_t *pStatus = (uShortRangeSpsConnection_t *)
                                              pInstance->pPendingSpsConnectionEvent;
        bool send = false;

        if (pStatus == NULL) {
            //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
            pStatus = (uShortRangeSpsConnection_t *) malloc(sizeof(*pStatus));
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

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UBTACLC_urc(uAtClientHandle_t atHandle,
                        void *pParameter)
{
    const uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
    int32_t connHandle;
    uShortRangeBtConnection_t *pStatus;

    connHandle = uAtClientReadInt(atHandle);
    //type is not used but needs to be read out
    (void)uAtClientReadInt(atHandle);
    (void)uAtClientReadString(atHandle, address, U_SHORT_RANGE_BT_ADDRESS_SIZE, false);

    if (pInstance->pConnectionStatusCallback != NULL) {
        //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
        pStatus = (uShortRangeBtConnection_t *) malloc(sizeof(*pStatus));
        if (pStatus != NULL) {
            pStatus->connHandle = connHandle;
            memcpy(pStatus->address, address, U_SHORT_RANGE_BT_ADDRESS_SIZE);
            pStatus->pCallback = pInstance->pConnectionStatusCallback;
            pStatus->pCallbackParameter = pInstance->pConnectionStatusCallbackParameter;
        }
        uAtClientCallback(atHandle, connectionStatusCallback, pStatus);
    }
}

//+UUDPC:<peer_handle>,<type>,<profile>,<address>,<frame_size>
//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUDPC_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    int32_t connHandle;
    int32_t type;

    connHandle = uAtClientReadInt(atHandle);
    type = uAtClientReadInt(atHandle);

    if (type == (int32_t)U_SHORT_RANGE_UUDPC_TYPE_BT) {
        char address[U_SHORT_RANGE_BT_ADDRESS_SIZE];
        (void)uAtClientReadInt(atHandle);
        (void)uAtClientReadString(atHandle, address, U_SHORT_RANGE_BT_ADDRESS_SIZE, false);
        int32_t mtu = uAtClientReadInt(atHandle);
        bool send = false;

        if (pInstance->pSpsConnectionCallback != NULL) {
            uShortRangeSpsConnection_t *pStatus = (uShortRangeSpsConnection_t *)
                                                  pInstance->pPendingSpsConnectionEvent;

            if (pStatus == NULL) {
                //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
                pStatus = (uShortRangeSpsConnection_t *) malloc(sizeof(*pStatus));
            } else {
                send = true;
            }
            if (pStatus != NULL) {
                pStatus->connHandle = connHandle;
                // AT (this) event info: connHandle, type, profile, address, mtu
                // EDM event info: type, profile, address, mtu, channel
                // use connHandle from here, the rest from the EDM event
                if (pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM) {
                    pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
                } else {
                    memcpy(pStatus->address, address, U_SHORT_RANGE_BT_ADDRESS_SIZE);
                    pStatus->dataChannel = -1; // Only EDM
                    pStatus->mtu = mtu;
                    pStatus->type = 0; //Connected
                    pStatus->pCallback = pInstance->pSpsConnectionCallback;
                    pStatus->pCallbackParameter = pInstance->pSpsConnectionCallbackParameter;
                    pStatus->pInstance = pInstance;
                    send = true;
                }
                if (send) {
                    uAtClientCallback(atHandle, spsEventCallback, pStatus);
                }
            }
        }
    }
}

//lint -esym(818, pParameter) Suppress pParameter could be const, need to
// follow prototype
static void UUDPD_urc(uAtClientHandle_t atHandle,
                      void *pParameter)
{
    uShortRangePrivateInstance_t *pInstance = (uShortRangePrivateInstance_t *) pParameter;
    int32_t connHandle;

    connHandle = uAtClientReadInt(atHandle);

    bool send = false;

    if (pInstance->pSpsConnectionCallback != NULL) {
        uShortRangeSpsConnection_t *pStatus = (uShortRangeSpsConnection_t *)
                                              pInstance->pPendingSpsConnectionEvent;

        if (pStatus == NULL) {
            //lint -esym(429, pStatus) Suppress pStatus not being free()ed here
            pStatus = (uShortRangeSpsConnection_t *) malloc(sizeof(*pStatus));
        } else {
            send = true;
        }
        if (pStatus != NULL) {
            pStatus->connHandle = connHandle;
            // AT (this) event info: connHandle
            // EDM event info: channel
            if (pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM) {
                pInstance->pPendingSpsConnectionEvent = (void *) pStatus;
            } else {
                pStatus->dataChannel = -1; // Only EDM
                pStatus->mtu = -1;
                pStatus->type = 1;
                pStatus->pCallback = pInstance->pSpsConnectionCallback;
                pStatus->pCallbackParameter = pInstance->pSpsConnectionCallbackParameter;
                pStatus->pInstance = pInstance;
                send = true;
            }
            if (send) {
                uAtClientCallback(atHandle, spsEventCallback, pStatus);
            }
        }
    }
}

static void exitDataMode(const uShortRangePrivateInstance_t *pInstance)
{
    const char escSeq[3] = {'+', '+', '+'};

    uPortTaskBlock(1100);
    if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
        if (uShortRangeEdmStreamAtWrite(pInstance->streamHandle, escSeq, 3) != 3) {
            uPortTaskBlock(1100);
            uShortRangeEdmStreamAtWrite(pInstance->streamHandle, escSeq, 3);
        }
    } else if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
        if (uPortUartWrite(pInstance->streamHandle, escSeq, 3) != 3) {
            uPortTaskBlock(1100);
            uPortUartWrite(pInstance->streamHandle, escSeq, 3);
        }
    }

    uPortTaskBlock(1100);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uShortRangeInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gUShortRangePrivateMutex == NULL) {
        // Create the mutex that protects the linked list
        errorCode = uPortMutexCreate(&gUShortRangePrivateMutex);
    }

    return errorCode;
}

void uShortRangeDeinit()
{
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        // Remove all short range instances
        while (gpUShortRangePrivateInstanceList != NULL) {
            pInstance = gpUShortRangePrivateInstanceList;
            removeShortRangeInstance(pInstance);
            free(pInstance);
        }

        // Unlock the mutex so that we can delete it
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        uPortMutexDelete(gUShortRangePrivateMutex);
        gUShortRangePrivateMutex = NULL;
    }
}

int32_t uShortRangeAdd(uShortRangeModuleType_t moduleType,
                       uAtClientHandle_t atHandle)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance = NULL;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        // Check parameters
        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (((size_t) moduleType < gUShortRangePrivateModuleListSize) &&
            (atHandle != NULL) &&
            (pGetShortRangeInstanceAtHandle(atHandle) == NULL)) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Allocate memory for the instance
            pInstance = (uShortRangePrivateInstance_t *) malloc(sizeof(uShortRangePrivateInstance_t));
            if (pInstance != NULL) {
                int32_t streamHandle;
                uAtClientStream_t streamType;
                // Fill the values in
                memset(pInstance, 0, sizeof(*pInstance));
                pInstance->handle = gNextInstanceHandle;
                gNextInstanceHandle++;
                if (gNextInstanceHandle < 0) {
                    gNextInstanceHandle = 0;
                }

                pInstance->atHandle = atHandle;
                pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;
                pInstance->startTimeMs = 500;

                streamHandle = uAtClientStreamGet(atHandle, &streamType);
                pInstance->streamHandle = streamHandle;
                pInstance->streamType = streamType;

                pInstance->pModule = &(gUShortRangePrivateModuleList[moduleType]);
                pInstance->pNext = NULL;

                uAtClientTimeoutSet(atHandle, pInstance->pModule->atTimeoutSeconds * 1000);
                uAtClientDelaySet(atHandle, pInstance->pModule->commandDelayMs);
                // ...and finally add it to the list
                addShortRangeInstance(pInstance);
                handleOrErrorCode = pInstance->handle;

                uAtClientSetUrcHandler(atHandle, "+STARTUP",
                                       restarted, pInstance);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return handleOrErrorCode;
}

void uShortRangeRemove(int32_t shortRangeHandle)
{
    uShortRangePrivateInstance_t *pInstance;

    if (shortRangeHandle != -1 && gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        if (pInstance != NULL) {
            removeShortRangeInstance(pInstance);
            uAtClientRemoveUrcHandler(pInstance->atHandle, "+STARTUP");
            free(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);

    }
}

int32_t uShortRangeConfigure(int32_t shortRangeHandle, const uShortRangeBleCfg_t *pShortRangeBleCfg)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pShortRangeBleCfg != NULL) {

            U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

            pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
            if (pInstance != NULL) {
                errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
                if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                    pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    bool restartNeeded = false;
                    atHandle = pInstance->atHandle;

                    int32_t role = getBleRole(atHandle);
                    if (role != (int32_t) pShortRangeBleCfg->role) {
                        errorCode = setBleRole(atHandle, (int32_t) pShortRangeBleCfg->role);
                        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                            restartNeeded = true;
                        }
                    }

                    if (pShortRangeBleCfg->spsServer) {
                        if (getServers(atHandle, U_SHORT_RANGE_SERVER_SPS) < 0) {
                            errorCode = setServer(atHandle, U_SHORT_RANGE_SERVER_SPS);
                            if (errorCode >= 0) {
                                restartNeeded = true;
                            }
                        }
                    }


                    if (errorCode >= 0 && restartNeeded) {
                        restart(atHandle, true);
                    }
                }

            }

            U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
        }
    }

    return errorCode;
}

int32_t uShortRangeSetDataCallback(int32_t shortRangeHandle,
                                   void (*pCallback) (int32_t, size_t, char *, void *),
                                   void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL && pCallback != NULL) {
            pInstance->pDataCallback = pCallback;
            pInstance->pDataCallbackParameter = pCallbackParameter;

            if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                uShortRangeEdmStreamDataEventCallbackSet(pInstance->streamHandle, edmDataCallback,
                                                         pInstance, 1536, U_CFG_OS_PRIORITY_MAX - 5);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

uShortRangeModuleType_t uShortRangeDetectModule(int32_t shortRangeHandle)
{
    uShortRangePrivateInstance_t *pInstance;
    uShortRangeModuleType_t module = U_SHORT_RANGE_MODULE_TYPE_INVALID;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        if (pInstance != NULL) {
            int32_t errorCode;
            // If we think we are in data mode, leave it
            if (pInstance->mode == U_SHORT_RANGE_MODE_DATA) {
                exitDataMode(pInstance);
                pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;
            }

            // Sometimes the UART (both in and outgoing) needs to be cleaned
            // from garbage or stray characters so test twice;
            // Use ATE0 as we want the echo off anyway and it is a OK/ERROR only
            // response command.
            errorCode = setEchoOff(pInstance->atHandle, 2);

            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS &&
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                const char atCommand[] = "\r\nATO2\r\n";
                uShortRangeEdmStreamAtWrite(pInstance->streamHandle, atCommand, sizeof(atCommand));
                uPortTaskBlock(60);
                errorCode = setEchoOff(pInstance->atHandle, 1);

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    exitDataMode(pInstance);
                    // This is edm handler, not uart, need that somehow
                    uShortRangeEdmStreamAtWrite(pInstance->streamHandle, atCommand, sizeof(atCommand));
                    uPortTaskBlock(60);
                }

                errorCode = setEchoOff(pInstance->atHandle, 1);
            }

            // We want to be in command mode but the module might be in EDM mode, the
            // only way to exit is to restart the module. This will send the binary packet that
            // executes "AT+CPWROFF" on the module as a raw write on the UART. Response is
            // ignored and if it works it will result in a startup URC (also ignored here, but might
            // trigger something in user code).
            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS &&
                pInstance->mode == U_SHORT_RANGE_MODE_COMMAND) {
                const unsigned char atCommand[] = { 0xAA, 0x00, 0x0D, 0x00, 0x44, 0x41, 0x54,
                                                    0x2B, 0x43, 0x50, 0x57, 0x52, 0x4F, 0x46,
                                                    0x46, 0x0D, 0x55
                                                  };
                uPortUartWrite(pInstance->streamHandle, atCommand, sizeof(atCommand));

                // Worst case restart time
                uPortTaskBlock(500);
                errorCode = setEchoOff(pInstance->atHandle, 1);

                // We want to be in command mode but the module might start up in data mode.
                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    exitDataMode(pInstance);
                    errorCode = setEchoOff(pInstance->atHandle, 1);
                }
            }

            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                module = getModule(pInstance->atHandle);
            }
        }
        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return module;
}

int32_t uShortRangeAttention(int32_t shortRangeHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                uAtClientHandle_t atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Sending AT\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeCheckBleRole(int32_t shortRangeHandle, uShortRangeBleRole_t *pRole)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL && pRole != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                int32_t response;
                atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Sending AT+UBTLE?\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UBTLE?");
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UBTLE:");
                response = uAtClientReadInt(atHandle);
                if (response >= 0) {
                    *pRole = (uShortRangeBleRole_t)response;
                }
                uAtClientResponseStop(atHandle);

                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeData(int32_t shortRangeHandle, int32_t channel,
                        const char *pData, int32_t length)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
    if (pInstance) {
        errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
        if (pInstance->mode == U_SHORT_RANGE_MODE_DATA) {
            errorCode = uPortUartWrite(pInstance->streamHandle, pData, length);
        } else if (pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
            errorCode = uShortRangeEdmStreamWrite(pInstance->streamHandle, channel, pData, length);
        }
    }
    return errorCode;
}

int32_t uShortRangeDataMode(int32_t shortRangeHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND) {
                atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Goto Data mode\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "ATO1");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);

                if (errorCode == (int32_t)U_ERROR_COMMON_SUCCESS) {
                    uAtClientRemove(pInstance->atHandle);
                    pInstance->pBuffer = (char *)malloc(U_SHORT_RANGE_UART_READ_BUFFER);
                    errorCode = uPortUartEventCallbackSet(pInstance->streamHandle,
                                                          U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                          dataCallback, (void *) pInstance,
                                                          U_AT_CLIENT_URC_TASK_STACK_SIZE_BYTES,
                                                          U_AT_CLIENT_URC_TASK_PRIORITY);
                    if (errorCode !=  (int32_t)U_ERROR_COMMON_SUCCESS) {
                        free(pInstance->pBuffer);
                    } else {
                        pInstance->mode = U_SHORT_RANGE_MODE_DATA;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeCommandMode(int32_t shortRangeHandle, uAtClientHandle_t *pAtHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            const char escSeq[3] = {'+', '+', '+'};

            uPortTaskBlock(1100);
            if (uPortUartWrite(pInstance->streamHandle, escSeq, 3) == 3) {
                uPortTaskBlock(1100);

                uPortUartEventCallbackRemove(pInstance->streamHandle);

                pInstance->atHandle = uAtClientAdd(pInstance->streamHandle, U_AT_CLIENT_STREAM_TYPE_UART,
                                                   NULL, U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
                *pAtHandle = pInstance->atHandle;
                atHandle = pInstance->atHandle;
                pInstance->mode = U_SHORT_RANGE_MODE_COMMAND;

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT");
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeSetEdm(int32_t shortRangeHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pInstance->mode = U_SHORT_RANGE_MODE_EDM;
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeSpsConnectionStatusCallback(int32_t shortRangeHandle,
                                               void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *),
                                               void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (pCallback != NULL) {
                pInstance->pSpsConnectionCallback = pCallback;
                pInstance->pSpsConnectionCallbackParameter = pCallbackParameter;
                uAtClientSetUrcHandler(pInstance->atHandle, "+UUDPC:",
                                       UUDPC_urc, pInstance);
                uAtClientSetUrcHandler(pInstance->atHandle, "+UUDPD:",
                                       UUDPD_urc, pInstance);
                if (pInstance->streamType == U_AT_CLIENT_STREAM_TYPE_EDM) {
                    uShortRangeEdmStreamBtEventCallbackSet(pInstance->streamHandle, btEdmConnectionCallback,
                                                           pInstance, 1536, U_CFG_OS_PRIORITY_MAX - 5);
                }

            } else {
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUDPC:");
                pInstance->pConnectionStatusCallback = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeBtConnectionStatusCallback(int32_t shortRangeHandle,
                                              void (*pCallback) (int32_t, char *, void *),
                                              void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            if (pCallback != NULL) {
                pInstance->pConnectionStatusCallback = pCallback;
                pInstance->pConnectionStatusCallbackParameter = pCallbackParameter;
                uAtClientSetUrcHandler(pInstance->atHandle, "+UUBTACLC:",
                                       UBTACLC_urc, pInstance);
            } else {
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUBTACLC:");
                pInstance->pConnectionStatusCallback = NULL;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

int32_t uShortRangeConnectSps(int32_t shortRangeHandle, const char *pAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
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
                uPortLog("U_SHORT_RANGE: Sending AT+UDCP\n");

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

int32_t uShortRangeDisconnect(int32_t shortRangeHandle, int32_t connHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uShortRangePrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;

    if (gUShortRangePrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUShortRangePrivateMutex);

        pInstance = pUShortRangePrivateGetInstance(shortRangeHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_INVALID_MODE;
            if (pInstance->mode == U_SHORT_RANGE_MODE_COMMAND ||
                pInstance->mode == U_SHORT_RANGE_MODE_EDM) {
                atHandle = pInstance->atHandle;
                uPortLog("U_SHORT_RANGE: Sending disconnect\n");

                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UDCPC=");
                uAtClientWriteInt(atHandle, connHandle);
                uAtClientCommandStopReadResponse(atHandle);
                errorCode = uAtClientUnlock(atHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUShortRangePrivateMutex);
    }

    return errorCode;
}

// End of file
