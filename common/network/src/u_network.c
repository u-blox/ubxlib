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
 * @brief Implementation of the common portion of the network API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // malloc(), free()

#include "u_error_common.h"

#include "u_port_os.h"

#include "u_network_handle.h"
#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"
#include "u_network_private_ble.h"
#include "u_network_private_cell.h"
#include "u_network_private_wifi.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of a network, intended to be used in a linked list.
 */
typedef struct uNetwork_t {
    int32_t handle;
    const void *pConfiguration;
    struct uNetwork_t *pNext;
} uNetwork_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the list.  Also used
 * for a non-NULL check that we're initialised.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Root of the nework instance list.
 */
static uNetwork_t *gpNetworkListHead = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Retrieve a network instance from the list
// Note: this does not lock the mutex, you have to do that.
static uNetwork_t *pGetNetwork(int32_t handle)
{
    uNetwork_t *pNetwork = gpNetworkListHead;

    while ((pNetwork != NULL) &&
           (pNetwork->handle != handle)) {
        pNetwork = pNetwork->pNext;
    }

    return pNetwork;
}

// Add a network to the list, mallocing as necessary.
// Note: this does not lock the mutex, you have to do that.
static uNetwork_t *pAddInstance()
{
    uNetwork_t *pTmp = gpNetworkListHead;
    uNetwork_t *pNetwork;

    // Malloc memory for the entry
    pNetwork = (uNetwork_t *) malloc(sizeof(uNetwork_t));
    if (pNetwork != NULL) {
        // Add it to the list
        gpNetworkListHead = pNetwork;
        pNetwork->pNext = pTmp;
    }

    return pNetwork;
}

// Remove a network instance from the list, freeing memory.
// Note: this does not lock the mutex, you have to do that.
static void removeInstance(const uNetwork_t *pNetwork)
{
    uNetwork_t **ppThis = &gpNetworkListHead;
    //lint -esym(438, pPrevious) Suppress last value not used
    uNetwork_t *pPrevious = NULL;

    // Find the entry in the list
    while ((*ppThis != NULL) && (*ppThis != pNetwork)) {
        pPrevious = *ppThis;
        ppThis = &((*ppThis)->pNext);
    }

    if (*ppThis != NULL) {
        // Unlink it from the list and free it
        if (pPrevious != NULL) {
            pPrevious = (*ppThis)->pNext;
        }
        free(*ppThis);
        *ppThis = NULL;
    }
}

// Remove a network (does not free the instance,
// call removeInstance() for that).
// Note: this does not lock the mutex, you have to do that.
static int32_t remove(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    if (U_NETWORK_HANDLE_IS_BLE(handle)) {
        errorCode = uNetworkRemoveBle(handle);
    } else if (U_NETWORK_HANDLE_IS_CELL(handle)) {
        errorCode = uNetworkRemoveCell(handle);
    } else if (U_NETWORK_HANDLE_IS_WIFI(handle)) {
        errorCode = uNetworkRemoveWifi(handle);
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the network API.
int32_t uNetworkInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {

            U_PORT_MUTEX_LOCK(gMutex);

            // Call the init functions in the
            // underlying network layers
            errorCode = uNetworkInitBle();
            if (errorCode == 0) {
                errorCode = uNetworkInitCell();
                if (errorCode == 0) {
                    errorCode = uNetworkInitWifi();
                    if (errorCode != 0) {
                        uNetworkDeinitCell();
                        uNetworkDeinitBle();
                    }
                } else {
                    uNetworkDeinitBle();
                }
            }

            U_PORT_MUTEX_UNLOCK(gMutex);
            // If the any of the underlying
            // layers fail then free our
            // mutex again and mark it as NULL
            if (errorCode != 0) {
                uPortMutexDelete(gMutex);
                gMutex = NULL;
            }
        }
    }

    return errorCode;
}

// Deinitialise the network API.
void uNetworkDeinit()
{
    uNetwork_t **ppNetwork = &gpNetworkListHead;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Remove all the instances,
        // ignoring errors 'cos there's
        // nothing we can do
        while (*ppNetwork != NULL) {
            remove((*ppNetwork)->handle);
            removeInstance(*ppNetwork);
        }

        // Call the deinit functions in the
        // underlying network layers
        uNetworkDeinitBle();
        uNetworkDeinitCell();
        uNetworkDeinitWifi();

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Add a network instance.
int32_t uNetworkAdd(uNetworkType_t type,
                    const void *pConfiguration)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Create storage for an instance
        pNetwork = pAddInstance();
        if (pNetwork != NULL) {
            errorCodeOrHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            if (pConfiguration != NULL) {
                switch (type) {
                    case U_NETWORK_TYPE_BLE:
                        // First parameter in the config must indicate
                        // that it is for BLE
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_BLE) {
                            errorCodeOrHandle = uNetworkAddBle((const uNetworkConfigurationBle_t *) pConfiguration);
                        }
                        break;
                    case U_NETWORK_TYPE_CELL:
                        // First parameter in the config must indicate
                        // that it is for cellular
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_CELL) {
                            errorCodeOrHandle = uNetworkAddCell((const uNetworkConfigurationCell_t *) pConfiguration);
                        }
                        break;
                    case U_NETWORK_TYPE_WIFI:
                        // First parameter in the config must indicate
                        // that it is for Wifi
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_WIFI) {
                            errorCodeOrHandle = uNetworkAddWifi((const uNetworkConfigurationWifi_t *) pConfiguration);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (errorCodeOrHandle >= 0) {
                // All good, remember it
                pNetwork->handle = errorCodeOrHandle;
                pNetwork->pConfiguration = pConfiguration;
            } else {
                // Failed, free the instance again
                removeInstance(pNetwork);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrHandle;
}

// Remove a network instance.
int32_t uNetworkRemove(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(handle);
        if (pNetwork != NULL) {
            errorCode = remove(pNetwork->handle);
            if (errorCode == 0) {
                removeInstance(pNetwork);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Bring up the given network instance.
int32_t uNetworkUp(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;
    const void *pConfiguration;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(handle);
        if (pNetwork != NULL) {
            handle = pNetwork->handle;
            pConfiguration = pNetwork->pConfiguration;
            if (U_NETWORK_HANDLE_IS_BLE(handle)) {
                errorCode = uNetworkUpBle(handle,
                                          (const uNetworkConfigurationBle_t *) pConfiguration);
            } else if (U_NETWORK_HANDLE_IS_CELL(handle)) {
                errorCode = uNetworkUpCell(handle,
                                           (const uNetworkConfigurationCell_t *) pConfiguration);
            } else if (U_NETWORK_HANDLE_IS_WIFI(handle)) {
                errorCode = uNetworkUpWifi(handle,
                                           (const uNetworkConfigurationWifi_t *) pConfiguration);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Take down the given network instance.
int32_t uNetworkDown(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;
    const void *pConfiguration;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(handle);
        if (pNetwork != NULL) {
            handle = pNetwork->handle;
            pConfiguration = pNetwork->pConfiguration;
            if (U_NETWORK_HANDLE_IS_BLE(handle)) {
                errorCode = uNetworkDownBle(handle,
                                            (const uNetworkConfigurationBle_t *) pConfiguration);
            } else if (U_NETWORK_HANDLE_IS_CELL(handle)) {
                errorCode = uNetworkDownCell(handle,
                                             (const uNetworkConfigurationCell_t *) pConfiguration);
            } else if (U_NETWORK_HANDLE_IS_WIFI(handle)) {
                errorCode = uNetworkDownWifi(handle,
                                             (const uNetworkConfigurationWifi_t *) pConfiguration);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// End of file
