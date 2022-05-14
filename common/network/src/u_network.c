/*
 * Copyright 2020 u-blox
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

#include "u_device_internal.h"

#include "u_port_os.h"

#include "u_location.h"
#include "u_location_shared.h"

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"
#include "u_network_config_gnss.h"
#include "u_network_private_ble.h"
#include "u_network_private_cell.h"
#include "u_network_private_wifi.h"
#include "u_network_private_gnss.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of a network, intended to be used in a linked list.
 */
typedef struct uNetwork_t {
    uDeviceHandle_t devHandle;
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
static uNetwork_t *pGetNetwork(uDeviceHandle_t devHandle)
{
    uNetwork_t *pNetwork = gpNetworkListHead;

    while ((pNetwork != NULL) &&
           (pNetwork->devHandle != devHandle)) {
        pNetwork = pNetwork->pNext;
    }

    return pNetwork;
}

static uNetworkType_t getNetworkType(uDeviceHandle_t devHandle)
{
    uDeviceInstance_t *pInstance;
    uNetworkType_t type = U_NETWORK_TYPE_NONE;

    if (uDeviceGetInstance(devHandle, &pInstance) == (int32_t )U_ERROR_COMMON_SUCCESS) {
        type = (uNetworkType_t) pInstance->netType;
    }

    return type;
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
    uNetwork_t **ppPrevious = &gpNetworkListHead;
    uNetwork_t *pTmp;

    // Find the entry in the list
    while ((*ppThis != NULL) && (*ppThis != pNetwork)) {
        ppPrevious = ppThis;
        ppThis = &((*ppThis)->pNext);
    }

    if (*ppThis != NULL) {
        // Unlink it from the list and free it
        pTmp = (*ppThis)->pNext;
        free(*ppThis);
        *ppPrevious = pTmp;
    }
}

// Remove a network (does not free the instance,
// call removeInstance() for that).
// Note: this does not lock the mutex, you have to do that.
static int32_t remove(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    uNetworkType_t netType = getNetworkType(devHandle);
    switch (netType) {
        case U_NETWORK_TYPE_BLE:
            errorCode = uNetworkRemoveBle(devHandle);
            break;
        case U_NETWORK_TYPE_CELL:
            errorCode = uNetworkRemoveCell(devHandle);
            break;
        case U_NETWORK_TYPE_GNSS:
            errorCode = uNetworkRemoveGnss(devHandle);
            break;
        case U_NETWORK_TYPE_WIFI:
            errorCode = uNetworkRemoveWifi(devHandle);
            break;
        default:
            break;
    }

    return errorCode;
}

static int32_t uNetworkInterfaceChangeState(uDeviceHandle_t devHandle, uNetworkType_t netType,
                                            bool up)
{
    int32_t returnCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceInstance_t *pInstance;

    // TODO: this doesn't lock a mutex, which is fine in the sense that,
    // when we're done, it won't have a linked-list to worry about,
    // however there is then nothing to stop two tasks trying to bring
    // the network interface up or down at the same time, which might
    // prove interesting.  If we intend to remove the mutex then we
    // should state that these functions are not thread-safe up at the
    // start of u_network.h (see what we do for other files).
    // Also, I've modified a few variable names below so that the variable
    // name follows the pattern of the type name, just for consistency.

    if (uDeviceGetInstance(devHandle, &pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS) {
        switch (uDeviceGetDeviceType(devHandle)) {
            case U_DEVICE_TYPE_CELL: {
                static uNetworkConfigurationCell_t cfgCell;
                //lint -e(1773) Suppress complaints about passing the pointer as non-volatile
                uDeviceNetworkCfgCell_t *pDevCfgCell = (uDeviceNetworkCfgCell_t *)
                                                       pInstance->pNetworkCfg[U_NETWORK_TYPE_CELL];
                if (pDevCfgCell) {
                    cfgCell.moduleType = (int32_t) U_NETWORK_TYPE_CELL;
                    cfgCell.pApn = pDevCfgCell->pApn;
                    cfgCell.pPin = pDevCfgCell->pPin;
                    cfgCell.timeoutSeconds = pDevCfgCell->timeoutSeconds;
                    returnCode = up ? uNetworkUpCell(devHandle, &cfgCell) : uNetworkDownCell(devHandle, &cfgCell);
                }
            }
            break;
            case U_DEVICE_TYPE_GNSS:
                returnCode = up ? uNetworkUpGnss(devHandle, NULL) : uNetworkDownGnss(devHandle, NULL);
                break;
            case U_DEVICE_TYPE_SHORT_RANGE: {
                if (netType == U_NETWORK_TYPE_WIFI) {
                    //lint -e(1773) Suppress complaints about passing the pointer as non-volatile
                    uDeviceNetworkCfgWifi_t *pDevCfgWifi = (uDeviceNetworkCfgWifi_t *)
                                                           pInstance->pNetworkCfg[U_NETWORK_TYPE_WIFI];
                    returnCode = uNetworkChangeStateWifi(devHandle, pDevCfgWifi, up);
                } else if (netType == U_NETWORK_TYPE_BLE) {
                    static uNetworkConfigurationBle_t cfgBle;
                    //lint -e(1773) Suppress complaints about passing the pointer as non-volatile
                    uDeviceNetworkCfgBle_t *pDevCfgBle = (uDeviceNetworkCfgBle_t *)
                                                         pInstance->pNetworkCfg[U_NETWORK_TYPE_BLE];
                    cfgBle.type = U_NETWORK_TYPE_BLE;
                    cfgBle.module = pInstance->module;
                    cfgBle.role = pDevCfgBle->role;
                    cfgBle.spsServer = pDevCfgBle->spsServer;
                    returnCode = up ? uNetworkUpBle(devHandle, &cfgBle) : uNetworkDownBle(devHandle, &cfgBle);
                }
            }
            break;
            case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
                // Not implemented yet
                break;
            default:
                break;
        }
    }
    return returnCode;
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
            if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED)) {
                errorCode = uNetworkInitCell();
                if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED)) {
                    errorCode = uNetworkInitWifi();
                    if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED)) {
                        errorCode = uNetworkInitGnss();
                        if (!((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED))) {
                            uNetworkDeinitWifi();
                            uNetworkDeinitCell();
                            uNetworkDeinitBle();
                        }
                    } else {
                        uNetworkDeinitCell();
                        uNetworkDeinitBle();
                    }
                } else {
                    uNetworkDeinitBle();
                }
            }

            if ((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED)) {
                // Initialise the internally shared location API
                uLocationSharedInit();
            }

            U_PORT_MUTEX_UNLOCK(gMutex);
            // If the any of the underlying
            // layers fail then free our
            // mutex again and mark it as NULL
            if (!((errorCode == 0) || (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED))) {
                uPortMutexDelete(gMutex);
                gMutex = NULL;
            }
        }
    }

    if (errorCode == (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED) {
        errorCode = 0;
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
            remove((*ppNetwork)->devHandle);
            removeInstance(*ppNetwork);
        }

        // De-initialise the internally shared location API
        uLocationSharedDeinit();

        // Call the deinit functions in the
        // underlying network layers
        uNetworkDeinitGnss();
        uNetworkDeinitWifi();
        uNetworkDeinitCell();
        uNetworkDeinitBle();

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// TODO: WILL BE REMOVED: functionality will be in uDeviceOpen().
int32_t uNetworkAdd(uNetworkType_t type,
                    const void *pConfiguration,
                    uDeviceHandle_t *pDevHandle)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;

    *pDevHandle = NULL;

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
                            errorCodeOrHandle = uNetworkAddBle((const uNetworkConfigurationBle_t *) pConfiguration, pDevHandle);
                        }
                        break;
                    case U_NETWORK_TYPE_CELL:
                        // First parameter in the config must indicate
                        // that it is for cellular
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_CELL) {
                            errorCodeOrHandle = uNetworkAddCell((const uNetworkConfigurationCell_t *) pConfiguration,
                                                                pDevHandle);
                        }
                        break;
                    case U_NETWORK_TYPE_WIFI:
                        // First parameter in the config must indicate
                        // that it is for Wifi
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_WIFI) {
                            errorCodeOrHandle = uNetworkAddWifi((const uNetworkConfigurationWifi_t *) pConfiguration,
                                                                pDevHandle);
                        }
                        break;
                    case U_NETWORK_TYPE_GNSS:
                        // First parameter in the config must indicate
                        // that it is for GNSS
                        if (*((const uNetworkType_t *) pConfiguration) == U_NETWORK_TYPE_GNSS) {
                            errorCodeOrHandle = uNetworkAddGnss((const uNetworkConfigurationGnss_t *) pConfiguration,
                                                                pDevHandle);
                        }
                        break;
                    default:
                        break;
                }
            }

            if (errorCodeOrHandle >= 0) {
                // All good, remember it
                pNetwork->devHandle = *pDevHandle;
                // TODO: Right now we set the network type in the device handle.
                //       This should be removed when we have changed the Network API
                //       to handle devices with multiple interfaces.
                U_DEVICE_INSTANCE(pNetwork->devHandle)->netType = (int32_t) type;
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

// TODO: WILL BE REMOVED: functionality will be in uDeviceClose().
int32_t uNetworkRemove(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(devHandle);
        if (pNetwork != NULL) {
            errorCode = remove(pNetwork->devHandle);
            if (errorCode == 0) {
                removeInstance(pNetwork);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality will be in uNetworkInterfaceUp().
int32_t uNetworkUp(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;
    const void *pConfiguration;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(devHandle);
        if (pNetwork != NULL) {
            uNetworkType_t netType = getNetworkType(devHandle);
            pConfiguration = pNetwork->pConfiguration;

            switch (netType) {
                case U_NETWORK_TYPE_BLE:
                    errorCode = uNetworkUpBle(devHandle,
                                              (const uNetworkConfigurationBle_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_CELL:
                    errorCode = uNetworkUpCell(devHandle,
                                               (const uNetworkConfigurationCell_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_GNSS:
                    errorCode = uNetworkUpGnss(devHandle,
                                               (const uNetworkConfigurationGnss_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_WIFI:
                    errorCode = uNetworkUpWifi(devHandle,
                                               (const uNetworkConfigurationWifi_t *) pConfiguration);
                    break;
                default:
                    break;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// TODO: WILL BE REMOVED: functionality will be in uNetworkInterfaceDown().
int32_t uNetworkDown(uDeviceHandle_t devHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uNetwork_t *pNetwork;
    const void *pConfiguration;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pNetwork = pGetNetwork(devHandle);
        if (pNetwork != NULL) {
            uNetworkType_t netType = getNetworkType(devHandle);
            pConfiguration = pNetwork->pConfiguration;

            switch (netType) {
                case U_NETWORK_TYPE_BLE:
                    errorCode = uNetworkDownBle(devHandle,
                                                (const uNetworkConfigurationBle_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_CELL:
                    errorCode = uNetworkDownCell(devHandle,
                                                 (const uNetworkConfigurationCell_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_GNSS:
                    errorCode = uNetworkDownGnss(devHandle,
                                                 (const uNetworkConfigurationGnss_t *) pConfiguration);
                    break;
                case U_NETWORK_TYPE_WIFI:
                    errorCode = uNetworkDownWifi(devHandle,
                                                 (const uNetworkConfigurationWifi_t *) pConfiguration);
                    break;
                default:
                    break;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

int32_t uNetworkInterfaceUp(uDeviceHandle_t devHandle, uNetworkType_t netType,
                            const void *pConfiguration)
{
    int32_t returnCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceInstance_t *pInstance;
    if (uDeviceGetInstance(devHandle, &pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS &&
        netType >= U_NETWORK_TYPE_NONE &&
        netType < U_NETWORK_TYPE_MAX_NUM) {
        if (!pConfiguration) {
            // Use possible last set configuration
            pConfiguration = pInstance->pNetworkCfg[netType];
        }
        // GNSS doesn't have any network configuration
        if (pInstance->deviceType == U_DEVICE_TYPE_GNSS || pConfiguration) {
            pInstance->pNetworkCfg[netType] = pConfiguration;
            returnCode = uNetworkInterfaceChangeState(devHandle, netType, true);
        }
    }
    return returnCode;
}

int32_t uNetworkInterfaceDown(uDeviceHandle_t devHandle, uNetworkType_t netType)
{
    return uNetworkInterfaceChangeState(devHandle, netType, false);
}

// End of file
