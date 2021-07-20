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

#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_network_private_short_range.h"

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
    int32_t shoHandle; /**< The handle returned by uShortRangeAdd(). */
    int32_t uart; /**< The UART HW block to use. */
    int32_t module; /**< The module type that is connected,
                         see uShortRangeModuleType_t in u_short_range.h. */
    int32_t uartHandle; /**< The handle returned by uPortUartOpen(). */
    int32_t edmStreamHandle; /**< The handle returned by uShortRangeEdmStreamOpen(). */
    uAtClientHandle_t atClientHandle; /**< The handle returned by uAtClientAdd(). */
    int32_t refCounter;  /**< Reference counter used for Add/Remove. */
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
    pInstance->shoHandle = -1;
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
static uNetworkPrivateShoInstance_t *pFindHandle(int32_t shoHandle)
{
    uNetworkPrivateShoInstance_t *pIter = NULL;

    for (size_t x = 0; (x < sizeof(gInstance) / sizeof(gInstance[0])) &&
         (pIter == NULL); x++) {
        if (gInstance[x].shoHandle == shoHandle) {
            pIter = &gInstance[x];
        }
    }

    return pIter;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API for short range.
int32_t uNetworkInitShortRange()
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

// Deinitialise the short range network API.
void uNetworkDeinitShortRange()
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

// Add a short range network instance.
int32_t uNetworkAddShortRange(const uShortRangeConfig_t *pConfiguration)
{
    int32_t errorCode;
    uNetworkPrivateShoInstance_t *pInstance;

    // The short range module might already be initialized
    pInstance = pFindUart(pConfiguration->uart);
    if (pInstance) {
        // Module already initialized so we just return the handle
        if ((int32_t) pInstance->module != pConfiguration->module) {
            return (int32_t) U_SHORT_RANGE_ERROR_WRONG_TYPE;
        } else {
            pInstance->refCounter++;
            return pInstance->shoHandle;
        }
    }

    // No previous connection to this module so we need to init

    // First a free instance
    pInstance = pFindHandle(-1);
    if (pInstance == NULL) {
        return (int32_t)U_ERROR_COMMON_NO_MEMORY;
    }

    // Open a UART with the recommended buffer length
    // and default baud rate.
    errorCode = uPortUartOpen(pConfiguration->uart,
                              U_SHORT_RANGE_UART_BAUD_RATE, NULL,
                              U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES,
                              pConfiguration->pinTxd,
                              pConfiguration->pinRxd,
                              pConfiguration->pinCts,
                              pConfiguration->pinRts);
    pInstance->uartHandle = errorCode;

    if (errorCode >= 0) {
        errorCode = uShortRangeEdmStreamOpen(pInstance->uartHandle);
        pInstance->edmStreamHandle = errorCode;
    }

    if (errorCode >= 0) {
        // Add an AT client on the UART with the recommended
        // default buffer size.
        pInstance->atClientHandle = uAtClientAdd(pInstance->edmStreamHandle,
                                                 U_AT_CLIENT_STREAM_TYPE_EDM,
                                                 NULL,
                                                 U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
        if (!pInstance->atClientHandle) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_AT;
        }
    }

    if (errorCode >= 0) {
        uShortRangeEdmStreamSetAtHandle(pInstance->edmStreamHandle, pInstance->atClientHandle);
        errorCode = uShortRangeAdd((uShortRangeModuleType_t) pConfiguration->module,
                                   pInstance->atClientHandle);
        pInstance->shoHandle = errorCode;
    }

    if (errorCode >= 0) {
        uShortRangeModuleType_t shortRangeModule = uShortRangeDetectModule(pInstance->shoHandle);

        if (shortRangeModule == U_SHORT_RANGE_MODULE_TYPE_INVALID) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_NOT_DETECTED;
        } else if ((int32_t) shortRangeModule != pConfiguration->module) {
            errorCode = (int32_t) U_SHORT_RANGE_ERROR_WRONG_TYPE;
        } else {
            // Everything is now setup
            pInstance->module = (int32_t) shortRangeModule;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
        pInstance->refCounter = 1;
        pInstance->uart = pConfiguration->uart;
        errorCode = pInstance->shoHandle;
    } else {
        // Something went wrong - cleanup needed
        if (pInstance->shoHandle >= 0) {
            uShortRangeRemove(pInstance->shoHandle);
        }
        if (pInstance->atClientHandle != NULL) {
            uAtClientRemove(pInstance->atClientHandle);
        }
        if (pInstance->edmStreamHandle >= 0) {
            uShortRangeEdmStreamClose(pInstance->edmStreamHandle);
        }
        if (pInstance->uartHandle >= 0) {
            uPortUartClose(pInstance->uartHandle);
        }
        clearInstance(pInstance);
    }

    return errorCode;
}

// Remove a short range network instance.
int32_t uNetworkRemoveShortRange(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uNetworkPrivateShoInstance_t *pInstance;

    // Find the instance in the list
    pInstance = pFindHandle(handle);
    if ((pInstance != NULL) && (--pInstance->refCounter <= 0)) {
        uShortRangeRemove(pInstance->shoHandle);
        uShortRangeEdmStreamClose(pInstance->edmStreamHandle);
        uAtClientRemove(pInstance->atClientHandle);
        uPortUartClose(pInstance->uartHandle);
        clearInstance(pInstance);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

uAtClientHandle_t uNetworkGetAtClientShortRange(int32_t handle)
{
    uNetworkPrivateShoInstance_t *pInstance;

    pInstance = pFindHandle(handle);
    if (pInstance) {
        return pInstance->atClientHandle;
    }

    return NULL;
}

// End of file
