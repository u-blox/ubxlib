/*
 * Copyright 2022 u-blox
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
 * @brief This is a temporary solution to allow multiple short range
 * network types to use the same UART. This will in the future be replaced
 * with a public transport protocol API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range_pbuf.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_network.h"
#include "u_network_private_short_range.h"

// TODO: I guess the contents of this whole file evaporates, as it
// ends up in uDevice?

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_NETWORK_PRIVATE_SHO_MAX_NUM
# define U_NETWORK_PRIVATE_SHO_MAX_NUM 1
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    uDeviceHandle_t wifiHandle; /**< The handle returned by uShortRangeAdd(). */
    uDeviceHandle_t bleHandle; /**< The handle returned by uShortRangeAdd(). */
    int32_t uart; /**< The UART HW block to use. */
    int32_t module; /**< The module type that is connected,
                         see uShortRangeModuleType_t in u_short_range.h. */
    int32_t uartHandle; /**< The handle returned by uPortUartOpen(). */
    int32_t edmStreamHandle; /**< The handle returned by uShortRangeEdmStreamOpen(). */
    uAtClientHandle_t atClientHandle; /**< The handle returned by uAtClientAdd(). */
} uNetworkPrivateShoInstance_t;


/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to keep track of the instances.
 */
static uNetworkPrivateShoInstance_t gInstance[U_NETWORK_PRIVATE_SHO_MAX_NUM];

static int32_t gRefCounter = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void clearInstance(uNetworkPrivateShoInstance_t *pInstance)
{
    pInstance->uart = -1;
    pInstance->module = -1;
    pInstance->uartHandle = -1;
    pInstance->atClientHandle = NULL;
    pInstance->edmStreamHandle = -1;
    pInstance->wifiHandle = NULL;
    pInstance->bleHandle = NULL;
}

// Find instance by uart - return NULL if not found.
static uNetworkPrivateShoInstance_t *pFindUart(int32_t uart)
{
    uNetworkPrivateShoInstance_t *pIter = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pIter == NULL); x++) {
        if (gInstance[x].uart == uart) {
            pIter = &gInstance[x];
        }
    }

    return pIter;
}

// Find instance by handle - return NULL if not found.
static uNetworkPrivateShoInstance_t *pFindHandle(uDeviceHandle_t devHandle)
{
    uNetworkPrivateShoInstance_t *pIter = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pIter == NULL); x++) {
        if (devHandle == NULL) {
            // devHandle==NULL is used to find a free handle.
            // In this case both ble and wifi handle must be NULL.
            if ((gInstance[x].wifiHandle == NULL) && (gInstance[x].bleHandle == NULL)) {
                pIter = &gInstance[x];
            }
        } else {
            // Check if either ble or wifi handle matches
            if (gInstance[x].wifiHandle == devHandle) {
                pIter = &gInstance[x];
            } else if (gInstance[x].bleHandle == devHandle) {
                pIter = &gInstance[x];
            }
        }
    }

    return pIter;
}

static void uNetworkConfigToShortRangeUartConfig(const uShortRangeConfig_t *pConfiguration,
                                                 uShortRangeUartConfig_t *pUartConfig)
{
    pUartConfig->uartPort = pConfiguration->uart;
    pUartConfig->baudRate = U_SHORT_RANGE_UART_BAUD_RATE;
    pUartConfig->pinTx = pConfiguration->pinTxd;
    pUartConfig->pinRx = pConfiguration->pinRxd;
    pUartConfig->pinCts = pConfiguration->pinCts;
    pUartConfig->pinRts = pConfiguration->pinRts;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// TODO: does this get removed, as I guess the functionality ends up
// in uDevice?
// Initialise the network API for short range.
int32_t uNetworkInitShortRange(void)
{
    // We will only do the real initialization on first call to
    // uNetworkInitShortRange().
    int32_t errCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    if (gRefCounter++ == 0) {
        for (size_t x = 0; x < sizeof(gInstance) / sizeof(gInstance[0]); x++) {
            clearInstance(&gInstance[x]);
        }

        errCode = uShortRangeEdmStreamInit();
        if (errCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            errCode = uAtClientInit();
        }
    }

    return errCode;
}

// TODO: does this get removed, as I guess the functionality ends up
// in uDevice?
// Deinitialise the short range network API.
void uNetworkDeinitShortRange(void)
{
    // We use the ref counter to decide if its time to do the real
    // deinit. As an example: if uNetworkInitShortRange() has been
    // called 2 times then the real deinit will happen on the second
    // call to uNetworkDeinitShortRange()
    if ((gRefCounter > 0) && (--gRefCounter == 0)) {
        uAtClientDeinit();
        uShortRangeEdmStreamDeinit();
    }
}

// TODO: WILL BE REMOVED: functionality will be in
// uDevicePrivateShortRangeAdd() in u_device_private_short_range.c.
int32_t uNetworkAddShortRange(uNetworkType_t netType,
                              const uShortRangeConfig_t *pConfiguration,
                              uDeviceHandle_t *pDevHandle)
{
    int32_t errorCode;
    uNetworkPrivateShoInstance_t *pInstance;
    uShortRangeUartConfig_t uartConfig;

    if ((netType != U_NETWORK_TYPE_BLE) && (netType != U_NETWORK_TYPE_WIFI)) {
        return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    }

    // The short range module might already be initialized
    pInstance = pFindUart(pConfiguration->uart);
    if (pInstance) {
        // Module already initialized so we just return the handle
        if ((int32_t) pInstance->module != pConfiguration->module) {
            return (int32_t) U_SHORT_RANGE_ERROR_WRONG_TYPE;
        } else {
            // When we get here we have an existing pInstance
            // In this instance either bleHandle or wifiHandle (or both)
            // is already set. So we need to:
            // 1. Check that the network type that the user want to setup is already used
            // 2. Create a copy of the existing uDeviceHandle_t where we only change the
            //    network type the user specified one.
            // NOTE: This is only a temporary solution until the Network API have been
            //       modified to allow multiple interfaces for the same uDevice.
            uDeviceInstance_t **ppTargetHandle;
            uDeviceInstance_t **ppExistingHandle;

            if (netType == U_NETWORK_TYPE_BLE) {
                ppTargetHandle = (uDeviceInstance_t **) &(pInstance->bleHandle);    // *NOPAD*
                ppExistingHandle = (uDeviceInstance_t **) &(pInstance->wifiHandle); // *NOPAD*
            } else {
                ppTargetHandle = (uDeviceInstance_t **) &(pInstance->wifiHandle);  // *NOPAD*
                ppExistingHandle = (uDeviceInstance_t **) &(pInstance->bleHandle); // *NOPAD*
            }
            if (*ppTargetHandle != NULL) {
                // User have already initialized network layer for the specified network type
                return (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            }
            // We now need to create a new uDeviceHandle_t with correct network type
            *ppTargetHandle = pUDeviceCreateInstance(U_DEVICE_TYPE_SHORT_RANGE);
            if (*ppTargetHandle == NULL) {
                return (int32_t) U_ERROR_COMMON_NO_MEMORY;
            }
            (*ppTargetHandle)->pContext = (*ppExistingHandle)->pContext;
            (*ppTargetHandle)->netType = (int32_t) netType;
            *pDevHandle = (uDeviceHandle_t)(*ppTargetHandle);
            return (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    // No previous connection to this module so we need to init

    // First a free instance
    pInstance = pFindHandle(NULL);
    if (pInstance == NULL) {
        return (int32_t) U_ERROR_COMMON_NO_MEMORY;
    }
    clearInstance(pInstance);

    uNetworkConfigToShortRangeUartConfig(pConfiguration, &uartConfig);
    // Open UART, EDM stream and initialize the module
    errorCode = uShortRangeOpenUart((uShortRangeModuleType_t) pConfiguration->module,
                                    &uartConfig, true, pDevHandle);

    if (errorCode >= 0) {
        U_DEVICE_INSTANCE(*pDevHandle)->netType = (int32_t) netType;
        if (netType == U_NETWORK_TYPE_BLE) {
            pInstance->bleHandle = *pDevHandle;
        } else {
            pInstance->wifiHandle = *pDevHandle;
        }
        pInstance->uartHandle = uShortRangeGetUartHandle(*pDevHandle);
        pInstance->edmStreamHandle = uShortRangeGetEdmStreamHandle(*pDevHandle);
        uShortRangeAtClientHandleGet(*pDevHandle, &pInstance->atClientHandle);

        pInstance->module = pConfiguration->module;
        pInstance->uart = pConfiguration->uart;
    } else {
        clearInstance(pInstance);
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality will be in
// uDevicePrivateShortRangeRemove() in u_device_private_short_range.c.
int32_t uNetworkRemoveShortRange(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateShoInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pFindHandle(devHandle);
    if (pInstance != NULL) {
        if (pInstance->bleHandle == devHandle) {
            pInstance->bleHandle = NULL;
        } else {
            pInstance->wifiHandle = NULL;
        }
        // Only if both wifi and ble has been removed we close the short range device
        if ((pInstance->bleHandle == NULL) && (pInstance->wifiHandle == NULL)) {
            uShortRangeClose(devHandle);
            clearInstance(pInstance);
        } else {
            // Don't forget to free the duplicated device handle
            uDeviceDestroyInstance(U_DEVICE_INSTANCE(devHandle));
        }
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// TODO: does this get removed, as I guess the functionality ends up
// in uDevice, since the AT interface is part of the device?
uAtClientHandle_t uNetworkGetAtClientShortRange(uDeviceHandle_t devHandle)
{
    uNetworkPrivateShoInstance_t *pInstance;

    pInstance = pFindHandle(devHandle);
    if (pInstance) {
        return pInstance->atClientHandle;
    }

    return NULL;
}

// End of file
